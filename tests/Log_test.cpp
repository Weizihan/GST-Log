#include "GstLog.h"
#include "src/logger/FileLogger.h"
#include "src/logger/AsyncFileLogger.h"
#include "src/logger/ConsoleLogger.h"
#include <atomic>
#include <algorithm>
#include <cerrno>
#include <condition_variable>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <mutex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace GST::LOG;
namespace fs = std::filesystem;
#define CHECK(expression) do { if (!(expression)) { throw std::runtime_error(std::string(__FILE__) + ":" + std::to_string(__LINE__) + ": " #expression); } } while (false)

static std::atomic<std::time_t> wall{0};
static std::atomic<std::int64_t> steady{100};
extern "C" std::time_t __wrap_gst_log_wall_time() { return wall.load(); }

extern "C" std::chrono::steady_clock::time_point __wrap_gst_log_steady_time() {
    return std::chrono::steady_clock::time_point(std::chrono::seconds(steady.load()));
}

extern "C" ssize_t __real_write(int, const void*, size_t);
extern "C" int __real_rename(const char*, const char*);
extern "C" int __real_open(const char*, int, ...);
static std::atomic<bool> fail_active_open{false};
extern "C" int __wrap_open(const char* path, int flags, ...) {
    int mode = 0;
    if (flags & O_CREAT) {
        va_list args; va_start(args, flags); mode = va_arg(args, int); va_end(args);
    }
    if ((flags & O_EXCL) && fail_active_open.exchange(false)) {
        errno = ENOSPC;
        return -1;
    }
    return __real_open(path, flags, mode);
}

static std::atomic<bool> fail_rename{false};
extern "C" int __wrap_rename(const char* from, const char* to) {
    if (fail_rename) {
        errno = EACCES;
        return -1;
    }
    return __real_rename(from, to);
}

enum class WriteMode { Normal, Partial, FailOnce, Block };
static std::mutex io_mutex;
static std::condition_variable io_cv;
static std::string io_path;
static WriteMode io_mode = WriteMode::Normal;
static int io_stage = 0;
static bool io_entered = false, io_released = false;
extern "C" ssize_t __wrap_write(int fd, const void* data, size_t size) {
    char link[64], path[4096];
    std::snprintf(link, sizeof(link), "/proc/self/fd/%d", fd);
    const auto n = ::readlink(link, path, sizeof(path) - 1);
    if (n >= 0) {
        path[n] = '\0';
        std::unique_lock<std::mutex> lock(io_mutex);
        if (io_path == path) {
            if (io_mode == WriteMode::Block) {
                io_entered = true;
                io_cv.notify_all();
                io_cv.wait(lock, [] { return io_released; });
            } else if (io_mode == WriteMode::FailOnce) {
                io_mode = WriteMode::Normal;
                errno = ENOSPC;
                return -1;
            } else if (io_mode == WriteMode::Partial) {
                if (io_stage++ == 0) {
                    errno = EINTR;
                    return -1;
                }
                if (io_stage == 2) {
                    return __real_write(fd, data, std::min(size, size_t(3)));
                }
                io_mode = WriteMode::Normal;
            }
        }
    }
    return __real_write(fd, data, size);
}

static void set_io(const fs::path& path, WriteMode mode) {
    std::lock_guard<std::mutex> lock(io_mutex);
    io_path = path.string(); io_mode = mode; io_stage = 0;
    io_entered = false; io_released = false;
}

static void wait_io() {
    std::unique_lock<std::mutex> lock(io_mutex);
    CHECK(io_cv.wait_for(lock, std::chrono::seconds(5), [] { return io_entered; }));
}

static void release_io() {
    std::lock_guard<std::mutex> lock(io_mutex);
    io_released = true; io_mode = WriteMode::Normal; io_cv.notify_all();
}

struct Temp {
    fs::path root;
    Temp() { char text[] = "/tmp/gst-log-test.XXXXXX"; auto p = ::mkdtemp(text); CHECK(p); root = p; }

    ~Temp() { std::error_code ec; fs::remove_all(root, ec); }
};
static std::time_t when(int hour, int minute = 0, int day = 2) {
    std::tm value{}; value.tm_year = 124; value.tm_mon = 0; value.tm_mday = day;
    value.tm_hour = hour; value.tm_min = minute; value.tm_isdst = -1;
    return std::mktime(&value);
}

static void timestamp(const fs::path& path, std::time_t time) {
    timespec times[2]{{time, 0}, {time, 0}};
    CHECK(::utimensat(AT_FDCWD, path.c_str(), times, 0) == 0);
}

static void seed(const fs::path& path, const std::string& data) {
    std::ofstream file(path, std::ios::binary); CHECK(file); file.write(data.data(), data.size()); CHECK(file);
}

static std::string read(const fs::path& path) {
    std::ifstream file(path, std::ios::binary); CHECK(file);
    return {std::istreambuf_iterator<char>(file), {}};
}

static LogConfig config(const fs::path& path, std::string name = "test") {
    LogConfig c; c._logger_name = std::move(name); c._log_type = "file";
    c._log_target = path.string(); c._log_format = "%S";
    c._log_level = LOG_LEVEL::LEVEL_INFO; c._natural_rotation = NaturalRotation::None;
    return c;
}

static std::unique_ptr<Logger> backend(bool async) {
    if (async) {
        return std::make_unique<AsyncFileLogger>();
    }
    return std::make_unique<FileLogger>();
}

static bool put(Logger& logger, const std::string& message, LOG_LEVEL level = LOG_LEVEL::LEVEL_INFO) {
    return logger.try_log(level, message, "/source/test.cpp", 42, "function");
}

static size_t files(const fs::path& dir) {
    return std::distance(fs::directory_iterator(dir), fs::directory_iterator());
}

static std::multiset<std::string> all_lines(const fs::path& dir) {
    std::multiset<std::string> result;
    for (const auto& file : fs::directory_iterator(dir)) {
        std::istringstream input(read(file.path())); std::string line;
        while (std::getline(input, line)) result.insert(line);
    }
    return result;
}

struct Capture {
    int saved;
    FILE* target;
    explicit Capture(const fs::path& path) {
        CHECK(std::fflush(stdout) == 0);
        saved = ::dup(STDOUT_FILENO); CHECK(saved >= 0);
        target = std::fopen(path.c_str(), "wb"); CHECK(target);
        CHECK(::dup2(::fileno(target), STDOUT_FILENO) >= 0);
        std::clearerr(stdout);
    }

    ~Capture() {
        std::fflush(stdout); ::dup2(saved, STDOUT_FILENO); ::close(saved);
        std::fclose(target); std::clearerr(stdout);
    }
};
static void format_test() {
    LogFormat format("[%L]|%S|%%|%Q|%R:%I:%F"); std::string message = "hello";
    CHECK(format.format(LOG_LEVEL::LEVEL_INFO, message, "/a/test.cpp", 42, "fn"));
    CHECK(message == "[INFO]|hello|%|%Q|test.cpp:42:fn\n");
    LogFormat plain("%S"); message = "line\n";
    CHECK(plain.format(LOG_LEVEL::LEVEL_INFO, message, "", 0, "")); CHECK(message == "line\n");
    message.clear(); CHECK(plain.format(LOG_LEVEL::LEVEL_INFO, message, nullptr, 0, nullptr)); CHECK(message == "\n");
    Temp temp; const auto path = temp.root / "console.log";
    {
        Capture capture(path); ConsoleLogger logger; auto c = config(path);
        c._log_format = "[%L] %S"; c._is_colorful_log = true;
        CHECK(logger.init(c)); CHECK(!put(logger, "filtered", LOG_LEVEL::LEVEL_DEBUG));
        CHECK(put(logger, std::string("A\0B", 3))); CHECK(logger.flush()); CHECK(logger.shutdown());
        CHECK(!put(logger, "closed"));
    }
    CHECK(read(path) == std::string("\033[34m[INFO] ") + std::string("A\0B", 3) + "\033[0m\n");
}

static void registry_test() {
    Temp temp; auto logger = get_Instance();
    auto c = config(temp.root / "one.log", "one"); c._log_format = "one:%S"; CHECK(logger->init(c));
    c = config(temp.root / "two.log", "two"); c._log_format = "two:%S"; CHECK(logger->init(c));
    c = config(temp.root / "async1.log", "async1"); c._log_type = "async"; CHECK(logger->init(c));
    c = config(temp.root / "async2.log", "async2"); c._log_type = "async"; CHECK(logger->init(c));
    INFO("broadcast=%d", 7);
    INFO_NAME("two", "named");
    CHECK(logger->try_log("one", LOG_LEVEL::LEVEL_INFO, "a", 1, "f", "%c%c%c", 'A', 0, 'B'));
    const std::string pattern = "string=%d";
    CHECK(logger->try_log(0, LOG_LEVEL::LEVEL_INFO, "a", 1, "f", pattern, 9));
    CHECK(!logger->try_log(123, LOG_LEVEL::LEVEL_INFO, "a", 1, "f", "bad"));
    CHECK(!logger->try_log(-2, LOG_LEVEL::LEVEL_INFO, "a", 1, "f", "bad"));
    CHECK(!logger->try_log("missing", LOG_LEVEL::LEVEL_INFO, "a", 1, "f", "bad"));
    CHECK(!logger->try_log(0, LOG_LEVEL::LEVEL_DEBUG, "a", 1, "f", "filtered"));
    CHECK(!logger->try_log(0, LOG_LEVEL::LEVEL_INFO, "a", 1, "f", nullptr));
    c = config(temp.root / "rejected.log", "one"); CHECK(!logger->init(c)); CHECK(!fs::exists(c._log_target));
    c = config(temp.root / "one.log", "alias"); CHECK(!logger->init(c));
    c = config(temp.root / "invalid.log", "invalid"); c._log_type = "unknown"; CHECK(!logger->init(c)); CHECK(!fs::exists(c._log_target));
    c._log_type.clear(); CHECK(!logger->init(c)); CHECK(!fs::exists(c._log_target));
    CHECK(logger->flush());
    std::string expected = "one:broadcast=7\n"; expected += std::string("one:A\0B\n", 8); expected += "one:string=9\n";
    CHECK(read(temp.root / "one.log") == expected);
    CHECK(read(temp.root / "two.log") == "two:broadcast=7\ntwo:named\n");
    CHECK(read(temp.root / "async1.log") == "broadcast=7\n"); CHECK(read(temp.root / "async2.log") == "broadcast=7\n");
    for (int length : {0, 1, 15, 16, 255, 4096, 65536}) {
        std::string text(length, 'Q'); CHECK(logger->try_log("two", LOG_LEVEL::LEVEL_INFO, "a", 1, "f", "%s", text.c_str()));
    }
    CHECK(logger->shutdown()); CHECK(logger->shutdown()); CHECK(logger->flush());
    CHECK(!logger->try_log(0, LOG_LEVEL::LEVEL_INFO, "a", 1, "f", "closed")); CHECK(!logger->init(config(temp.root / "late.log", "late")));
}

static void compatibility_test() {
    Temp temp; auto logger = get_Instance();
    CHECK(!logger->try_log(-1, LOG_LEVEL::LEVEL_INFO, "a", 1, "f", "uninitialized"));
    {
        Capture capture(temp.root / "console.log");
        CHECK(logger->init()); CHECK(!logger->init());
        CHECK(LogConfig{}._log_type == "file");
        auto c = config(temp.root / "file.log", "Async");
        c._log_type = LogConfig{}._log_type; c._buffer_size = -1; CHECK(logger->init(c));
        c = config(temp.root / "async.log", "File"); c._log_type = "async"; CHECK(logger->init(c));
        c = config(temp.root / "named-console.log", "Console");
        c._log_type = LogConfig{}._log_type; CHECK(logger->init(c));
        INFO("compatibility"); INFO_INDEX(1, "index"); INFO_NAME("File", "name");
        logger->log("Async", LOG_LEVEL::LEVEL_INFO, "a", 1, "f", std::string("string=%d"), 5);
        CHECK(logger->flush()); CHECK(logger->shutdown());
    }
    CHECK(read(temp.root / "file.log") == "compatibility\nindex\nstring=5\n");
    CHECK(read(temp.root / "async.log") == "compatibility\nname\n");
    CHECK(read(temp.root / "named-console.log") == "compatibility\n");
    CHECK(read(temp.root / "console.log").find("compatibility") != std::string::npos);
}

static void paths_test() {
    Temp temp;
    {
        FileLogger logger; auto c = config(temp.root / "absolute"); c._log_file_name = "app.log";
        CHECK(logger.init(c)); CHECK(put(logger, "absolute")); CHECK(logger.shutdown()); CHECK(read(temp.root / "absolute/app.log") == "absolute\n");
    }
    const auto previous = fs::current_path(); fs::current_path(temp.root);
    {
        AsyncFileLogger logger; auto c = config("./relative/deep/"); c._log_file_name = "app.log";
        CHECK(logger.init(c)); CHECK(put(logger, "relative")); CHECK(logger.shutdown());
    }
    fs::current_path(previous); CHECK(read(temp.root / "relative/deep/app.log") == "relative\n");
    const auto path = temp.root / "existing.log"; seed(path, "history\n");
    for (bool async : {false, true}) {
        auto logger = backend(async);
        CHECK(logger->init(config(path)));
        CHECK(read(path).find("history\n") == 0);
        CHECK(put(*logger, "append"));
        CHECK(logger->shutdown());
    }
    CHECK(read(path) == "history\nappend\nappend\n");
    seed(temp.root / "parent.log", "parent");
    for (bool async : {false, true}) {
        auto logger = backend(async); CHECK(!logger->init(config(temp.root / "parent.log/child.log")));
        CHECK(logger->shutdown());
        CHECK(logger->init(config(temp.root / (async ? "retry-async.log" : "retry-sync.log"))));
        CHECK(put(*logger, "retry")); CHECK(logger->flush()); CHECK(logger->shutdown());
    }
    auto invalid = config(temp.root / "must-not-exist/deep.log"); invalid._log_encoding = "GBK";
    { FileLogger logger; CHECK(!logger.init(invalid)); } CHECK(!fs::exists(temp.root / "must-not-exist"));
    invalid._log_encoding = "UTF-8"; invalid._log_max_size = -1;
    { FileLogger logger; CHECK(!logger.init(invalid)); } CHECK(!fs::exists(temp.root / "must-not-exist"));
    invalid._log_max_size = 0; invalid._buffer_size = -1;
    { AsyncFileLogger logger; CHECK(!logger.init(invalid)); } CHECK(!fs::exists(temp.root / "must-not-exist"));
    invalid._buffer_size = 64; invalid._max_pending_logs = 0;
    { AsyncFileLogger logger; CHECK(!logger.init(invalid)); } CHECK(!fs::exists(temp.root / "must-not-exist"));
    invalid._max_pending_logs = 1; invalid._max_pending_bytes = 0;
    { AsyncFileLogger logger; CHECK(!logger.init(invalid)); } CHECK(!fs::exists(temp.root / "must-not-exist"));
    invalid = config(temp.root / "must-not-exist/"); invalid._log_file_name = "../escape.log";
    { FileLogger logger; CHECK(!logger.init(invalid)); } CHECK(!fs::exists(temp.root / "must-not-exist"));
    const auto denied = temp.root / "denied"; fs::create_directory(denied);
    if (::geteuid() != 0) {
        fs::permissions(denied, fs::perms::owner_read | fs::perms::owner_exec);
        for (bool async : {false, true}) {
            auto logger = backend(async);
            CHECK(!logger->init(config(denied / "no.log")));
        }
        fs::permissions(denied, fs::perms::owner_all);
    }
}

static void rotation_test(bool async) {
    Temp temp;
    auto scenario = [&](const std::string& name) { auto path = temp.root / name; fs::create_directory(path); return path; };
    {
        const auto dir = scenario("hourly"); auto c = config(dir / "active.log"); c._natural_rotation = NaturalRotation::Hourly;
        wall = when(4, 59); auto logger = backend(async); CHECK(logger->init(c)); CHECK(put(*logger, "before")); CHECK(logger->flush());
        wall = when(5); CHECK(put(*logger, "after")); CHECK(logger->flush()); CHECK(files(dir) == 2); CHECK(read(c._log_target) == "after\n");
        wall = when(8, 0, 5); CHECK(put(*logger, "idle")); CHECK(logger->flush()); CHECK(files(dir) == 3); CHECK(logger->shutdown());
        CHECK(all_lines(dir) == std::multiset<std::string>({"before", "after", "idle"}));
    }
    {
        const auto dir = scenario("daily"); auto c = config(dir / "active.log"); c._natural_rotation = NaturalRotation::Daily;
        wall = when(23, 59); auto logger = backend(async); CHECK(logger->init(c)); CHECK(put(*logger, "day1")); CHECK(logger->flush());
        wall = when(0, 0, 3); CHECK(put(*logger, "day2")); CHECK(logger->flush()); CHECK(files(dir) == 2);
        wall = when(1, 0, 3); CHECK(put(*logger, "same-day")); CHECK(logger->shutdown()); CHECK(files(dir) == 2);
    }
    {
        const auto dir = scenario("restart"); auto c = config(dir / "active.log"); c._natural_rotation = NaturalRotation::Hourly;
        seed(c._log_target, "history\n"); timestamp(c._log_target, when(4, 5)); wall = when(4, 59);
        { auto logger = backend(async); CHECK(logger->init(c)); CHECK(files(dir) == 1); CHECK(put(*logger, "same-period")); CHECK(logger->shutdown()); }
        CHECK(files(dir) == 1); timestamp(c._log_target, when(4, 59)); wall = when(5);
        { auto logger = backend(async); CHECK(logger->init(c)); CHECK(files(dir) == 1); CHECK(put(*logger, "new-period")); CHECK(logger->shutdown()); }
        CHECK(files(dir) == 2); CHECK(read(c._log_target) == "new-period\n");
        CHECK(all_lines(dir) == std::multiset<std::string>({"history", "same-period", "new-period"}));
    }
    {
        const auto dir = scenario("collision"); auto c = config(dir / "active.log"); c._log_max_size = 1; wall = when(6);
        auto logger = backend(async); CHECK(logger->init(c));
        for (const char* text : {"a", "b", "c", "d"}) {
            CHECK(put(*logger, text));
            CHECK(logger->flush());
        }
        CHECK(logger->shutdown()); CHECK(files(dir) == 4); CHECK(all_lines(dir) == std::multiset<std::string>({"a", "b", "c", "d"}));
    }
    {
        const auto dir = scenario("size-restart"); auto c = config(dir / "active.log"); c._log_max_size = 3;
        seed(c._log_target, "history\n"); timestamp(c._log_target, wall);
        auto logger = backend(async); CHECK(logger->init(c)); CHECK(files(dir) == 1); CHECK(put(*logger, "next")); CHECK(logger->shutdown()); CHECK(files(dir) == 2);
    }
    {
        const auto dir = scenario("combined"); auto c = config(dir / "active.log"); c._natural_rotation = NaturalRotation::Hourly; c._log_max_size = 2;
        wall = when(4, 59); auto logger = backend(async); CHECK(logger->init(c)); CHECK(put(*logger, "a")); CHECK(logger->flush());
        wall = when(5); CHECK(put(*logger, "b")); CHECK(logger->flush()); CHECK(files(dir) == 2);
        CHECK(put(*logger, "c")); CHECK(logger->shutdown()); CHECK(files(dir) == 3);
    }
    {
        const auto dir = scenario("interval"); auto c = config(dir / "active.log"); c._rotation_interval = std::chrono::seconds(60);
        steady = 100; auto logger = backend(async); CHECK(logger->init(c)); CHECK(put(*logger, "a")); CHECK(logger->flush());
        wall = when(20); CHECK(put(*logger, "b")); CHECK(logger->flush()); CHECK(files(dir) == 1);
        steady = 160; CHECK(put(*logger, "c")); CHECK(logger->shutdown()); CHECK(files(dir) == 2);
    }
    {
        const auto dir = scenario("archive-failure"); auto c = config(dir / "active.log");
        c._natural_rotation = NaturalRotation::Hourly; wall = when(4, 59);
        auto logger = backend(async); CHECK(logger->init(c)); CHECK(put(*logger, "safe")); CHECK(logger->flush()); wall = when(5); fail_rename = true;
        const bool accepted = put(*logger, "rejected"); CHECK(accepted == async); CHECK(!logger->flush());
        CHECK(read(c._log_target) == "safe\n"); CHECK(files(dir) == 1);
        fail_rename = false; CHECK(put(*logger, "recovered")); CHECK(!logger->shutdown());
        CHECK(files(dir) == 2); CHECK(read(c._log_target) == "recovered\n");
    }
    {
        const auto dir = scenario("active-open-failure"); auto c = config(dir / "active.log"); c._log_max_size = 1;
        auto logger = backend(async); CHECK(logger->init(c)); CHECK(put(*logger, "safe")); CHECK(logger->flush());
        fail_active_open = true;
        CHECK(put(*logger, "not-written") == async); CHECK(!logger->flush());
        CHECK(!fs::exists(c._log_target)); CHECK(files(dir) == 1); CHECK(all_lines(dir) == std::multiset<std::string>({"safe"}));
        CHECK(put(*logger, "recovered")); CHECK(!logger->shutdown());
        CHECK(files(dir) == 2); CHECK(read(c._log_target) == "recovered\n");
    }
}

static void async_test() {
    Temp temp; const auto path = temp.root / "queue.log";
    AsyncFileLogger logger; auto c = config(path); c._buffer_size = 8; c._max_pending_logs = 2; c._max_pending_bytes = 32;
    CHECK(logger.init(c)); set_io(path, WriteMode::Block); CHECK(put(logger, "first")); wait_io();
    CHECK(put(logger, "second")); CHECK(!put(logger, "full"));
    CHECK(!put(logger, std::string(33, 'X')));
    std::atomic<bool> control_started{false}; bool shutdown_result = false;
    std::thread closer([&] { control_started = true; shutdown_result = logger.shutdown(); });
    while (!control_started) std::this_thread::yield();
    release_io(); closer.join(); CHECK(shutdown_result);
    CHECK(logger.flush()); CHECK(logger.shutdown()); CHECK(!put(logger, "closed")); CHECK(read(path) == "first\nsecond\n");
    const auto byte_path = temp.root / "bytes.log";
    AsyncFileLogger bytes; c = config(byte_path); c._buffer_size = 8; c._max_pending_logs = 100; c._max_pending_bytes = 8;
    CHECK(bytes.init(c)); set_io(byte_path, WriteMode::Block); CHECK(put(bytes, "1234567")); wait_io(); CHECK(!put(bytes, "x")); release_io(); CHECK(bytes.shutdown());
    const auto flush_path = temp.root / "flush.log";
    AsyncFileLogger flushing; CHECK(flushing.init(config(flush_path))); set_io(flush_path, WriteMode::Block); CHECK(put(flushing, "boundary")); wait_io();
    bool flush_result = false; std::thread waiter([&] { flush_result = flushing.flush(); });
    CHECK(put(flushing, "later")); release_io(); waiter.join(); CHECK(flush_result); CHECK(flushing.shutdown()); CHECK(read(flush_path) == "boundary\nlater\n");
}

static void errors_test() {
    Temp temp;
    for (bool async : {false, true}) {
        const auto path = temp.root / (async ? "async.log" : "sync.log"); auto logger = backend(async); CHECK(logger->init(config(path)));
        set_io(path, WriteMode::Partial); CHECK(put(*logger, "partial-and-eintr")); CHECK(logger->flush()); CHECK(read(path) == "partial-and-eintr\n");
        set_io(path, WriteMode::FailOnce); const bool accepted = put(*logger, "lost"); CHECK(accepted == async); CHECK(!logger->flush());
        CHECK(put(*logger, "recovered")); CHECK(!logger->flush()); CHECK(!logger->shutdown()); CHECK(read(path) == "partial-and-eintr\nrecovered\n");
        const auto deleted = temp.root / (async ? "deleted_async.log" : "deleted_sync.log"); auto recreated = backend(async); CHECK(recreated->init(config(deleted)));
        CHECK(put(*recreated, "old")); CHECK(recreated->flush()); CHECK(fs::remove(deleted)); CHECK(put(*recreated, "new")); CHECK(recreated->shutdown()); CHECK(read(deleted) == "new\n");
        auto full = backend(async); CHECK(full->init(config("/dev/full"))); const bool queued = put(*full, "disk-full"); CHECK(queued == async); CHECK(!full->flush()); CHECK(!full->shutdown());
    }
    {
        Capture capture("/dev/full"); ConsoleLogger console; CHECK(console.init(config("unused")));
        const bool accepted = put(console, "console-full");
        const bool flushed = console.flush(); CHECK(!accepted || !flushed); CHECK(!flushed); CHECK(!console.shutdown());
    }
}

static void concurrent_test() {
    Temp temp;
    for (bool async : {false, true}) {
        const auto path = temp.root / (async ? "async.log" : "sync.log"); auto logger = backend(async); CHECK(logger->init(config(path)));
        std::mutex mutex; std::condition_variable cv; int ready = 0; bool go = false; std::atomic<bool> accepted{true}; std::vector<std::thread> threads;
        for (int t = 0; t < 4; ++t) {
            threads.emplace_back([&, t] {
                { std::unique_lock<std::mutex> lock(mutex); ++ready; cv.notify_all(); cv.wait(lock, [&] { return go; }); }
                for (int i = 0; i < 500; ++i) {
                    if (!put(*logger, std::to_string(t) + ":" + std::to_string(i))) {
                        accepted = false;
                    }
                }
            });
        }
        { std::unique_lock<std::mutex> lock(mutex); cv.wait(lock, [&] { return ready == 4; }); go = true; cv.notify_all(); }
        for (auto& thread : threads) {
            thread.join();
        }
        CHECK(accepted); CHECK(logger->flush()); CHECK(logger->shutdown());
        int seen[4]{}; int count = 0; std::istringstream input(read(path)); std::string line;
        while (std::getline(input, line)) { int t = -1, i = -1; char extra; CHECK(std::sscanf(line.c_str(), "%d:%d%c", &t, &i, &extra) == 2); CHECK(t >= 0 && t < 4); CHECK(i == seen[t]++); ++count; }
        CHECK(count == 2000);
        for (int value : seen) {
            CHECK(value == 500);
        }
    }
}

int main(int argc, char** argv) {
    ::setenv("TZ", "UTC", 1); ::tzset(); wall = when(4, 59);
    try {
        CHECK(argc == 2); const std::string name = argv[1];
        if (name == "format") {
            format_test();
        }
        else if (name == "registry") {
            registry_test();
        }
        else if (name == "compatibility") {
            compatibility_test();
        }
        else if (name == "paths") {
            paths_test();
        }
        else if (name == "rotation_sync") {
            rotation_test(false);
        }
        else if (name == "rotation_async") {
            rotation_test(true);
        }
        else if (name == "async_lifecycle") {
            async_test();
        }
        else if (name == "errors") {
            errors_test();
        }
        else if (name == "concurrent") {
            concurrent_test();
        }
        else {
            throw std::runtime_error("unknown test");
        }
        std::cout << name << ": passed\n"; return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; return 1;
    }
}
