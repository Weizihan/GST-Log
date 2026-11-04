#include "src/logger/AsyncFileLogger.h"
#include <condition_variable>
#include <atomic>
#include <cstdlib>
#include <dlfcn.h>
#include <fstream>
#include <iostream>
#include <iterator>
#include <mutex>
#include <pthread.h>
#include <stdexcept>
#include <unistd.h>

// 阻塞实际 worker 入口，直到 shutdown 或析构流程执行到 pthread_join。
// 这样可以稳定复现线程交错，无需在生产代码中增加回调或休眠循环。
static std::atomic<bool> armed{false};
static std::mutex mutex;
static std::condition_variable cv;
static bool entered = false, released = false;
static pthread_t worker{};
struct Start {
    void* (*function)(void*);
    void* argument;
};
static void* entry(void* argument) {
    std::unique_ptr<Start> start(static_cast<Start*>(argument));
    {
        std::unique_lock<std::mutex> lock(mutex);
        entered = true;
        cv.notify_all();
        cv.wait(lock, [] {
            return released;
        });
    }
    return start->function(start->argument);
}

extern "C" int pthread_create(pthread_t* thread, const pthread_attr_t* attr,
                               void* (*function)(void*), void* argument) {
    static auto real_create = reinterpret_cast<int (*)(pthread_t*, const pthread_attr_t*, void* (*)(void*), void*)>(dlsym(RTLD_NEXT, "pthread_create"));
    if (armed.exchange(false)) {
        auto start = new Start{function, argument};
        const int result = real_create(thread, attr, entry, start);
        if (result != 0) {
            delete start;
        }
        else {
            worker = *thread;
        }
        return result;
    }
    return real_create(thread, attr, function, argument);
}

extern "C" int pthread_join(pthread_t thread, void** result) {
    static auto real_join = reinterpret_cast<int (*)(pthread_t, void**)>(dlsym(RTLD_NEXT, "pthread_join"));
    if (pthread_equal(thread, worker)) {
        std::lock_guard<std::mutex> lock(mutex);
        released = true;
        cv.notify_all();
    }
    return real_join(thread, result);
}

int main() {
    char text[] = "/tmp/gst-log-early.XXXXXX";
    const auto dir = ::mkdtemp(text);
    if (!dir) {
        return 1;
    }
    const std::filesystem::path root(dir);
    bool success = true;
    for (bool explicit_shutdown : {false, true}) {
        entered = false;
        released = false;
        armed = true;
        auto path = root / (explicit_shutdown ? "shutdown.log" : "destructor.log");
        std::string expected;
        {
            GST::LOG::AsyncFileLogger logger;
            GST::LOG::LogConfig config;
            config._logger_name = "early";
            config._log_target = path.string();
            config._natural_rotation = GST::LOG::NaturalRotation::None;
            config._buffer_size = 32;
            if (!logger.init(config)) {
                return 2;
            }
            {
                std::unique_lock<std::mutex> lock(mutex);
                if (!cv.wait_for(lock, std::chrono::seconds(5), [] {
                        return entered;
                    })) {
                    std::_Exit(3);
                }
            }
            for (int i = 0; i < 50; ++i) {
                const auto line = "record-" + std::to_string(i) + "\n";
                if (!logger.write_log(line)) {
                    std::_Exit(4);
                }
                expected += line;
            }
            if (explicit_shutdown && !logger.shutdown()) {
                success = false;
            }
        }
        std::ifstream file(path, std::ios::binary);
        std::string actual{std::istreambuf_iterator<char>(file), {}};
        success = success && actual == expected;
    }
    std::filesystem::remove_all(root);
    std::cout << "worker entry delayed until join: " << (success ? "passed" : "failed") << '\n';
    return success ? 0 : 1;
}
