#include "LogFile.h"
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace GST {
namespace LOG {
namespace detail {

bool resolve_log_path(const LogConfig& config, std::filesystem::path& path) {
    if (config._log_target.empty() || config._log_target.find('\0') != std::string::npos ||
        config._log_file_name.empty() || config._log_file_name.find('\0') != std::string::npos) {
        std::fputs("GST_log: empty path/name or embedded NUL in file configuration\n", stderr);
        return false;
    }
    std::error_code ec;
    path = config._log_target;
    const bool exists = std::filesystem::exists(path, ec);
    if (ec) {
        std::fprintf(stderr, "GST_log: inspect path '%s': %s\n", path.c_str(), ec.message().c_str());
        return false;
    }
    const bool directory = (exists && std::filesystem::is_directory(path, ec)) ||
        config._log_target.back() == '/' || (!exists && !path.has_extension());
    if (ec) {
        std::fprintf(stderr, "GST_log: inspect path type: %s\n", ec.message().c_str());
        return false;
    }
    if (directory) {
        const std::filesystem::path name(config._log_file_name);
        if (name.has_parent_path() || name == "." || name == "..") {
            std::fputs("GST_log: file name must be a single path component\n", stderr);
            return false;
        }
        path /= name;
    }
    path = std::filesystem::absolute(path, ec);
    if (!ec) {
        path = std::filesystem::weakly_canonical(path, ec);
    }
    if (ec) {
        std::fprintf(stderr, "GST_log: resolve path: %s\n", ec.message().c_str());
        return false;
    }
    return true;
}

LogFile::LogFile() {
}

LogFile::~LogFile() {
    close();
}

bool LogFile::flush() const {
    // write(2) 只保证字节已提交给操作系统。
    return !_had_error;
}

const std::filesystem::path& LogFile::path() const {
    return _path;
}

bool LogFile::error(const char* operation, int code) {
    _had_error = true;
    std::fprintf(stderr, "GST_log: %s '%s': %s\n", operation, _path.c_str(), std::strerror(code));
    return false;
}

std::string LogFile::period(std::time_t time) const {
    if (_natural == NaturalRotation::None) {
        return {};
    }
    std::tm local{};
    localtime_r(&time, &local);
    char text[32]{};
    std::strftime(text, sizeof(text), _natural == NaturalRotation::Hourly ? "%Y%m%d%H" : "%Y%m%d", &local);
    return text;
}

bool LogFile::open(const LogConfig& config) {
    _had_error = false;
    if (!resolve_log_path(config, _path)) {
        return false;
    }
    _max_size = static_cast<std::uintmax_t>(config._log_max_size);
    _natural = config._natural_rotation == NaturalRotation::Legacy
        ? (config._trunback_type ? NaturalRotation::Daily : NaturalRotation::None)
        : config._natural_rotation;
    _interval = config._rotation_interval;
    return reopen();
}

bool LogFile::reopen() {
    std::error_code ec;
    std::filesystem::create_directories(_path.parent_path(), ec);
    if (ec) {
        return error("create directories for", ec.value());
    }
    const int fresh = ::open(_path.c_str(), O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
    if (fresh < 0) {
        return error("open", errno);
    }
    struct stat info{};
    if (::fstat(fresh, &info) != 0) {
        const int code = errno;
        ::close(fresh);
        return error("stat", code);
    }
    const int previous = _fd;
    _fd = fresh;
    _period = period(info.st_size > 0 ? info.st_mtime : gst_log_wall_time());
    _interval_start = gst_log_steady_time();
    if (previous >= 0 && ::close(previous) != 0) {
        return error("close replaced file", errno);
    }
    return true;
}

bool LogFile::rotate(std::time_t now) {
    std::tm local{};
    localtime_r(&now, &local);
    char timestamp[32]{};
    std::strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", &local);
    const std::string stem = _path.stem().string() + "_" + timestamp;
    const std::string extension = _path.extension().string();
    auto backup = _path.parent_path() / (stem + extension);
    std::error_code ec;
    for (std::size_t sequence = 1; ; ++sequence) {
        const auto status = std::filesystem::symlink_status(backup, ec);
        if (ec && ec != std::errc::no_such_file_or_directory) {
            return error("inspect archive", ec.value());
        }
        if (!std::filesystem::exists(status)) {
            break;
        }
        backup = _path.parent_path() / (stem + "_" + std::to_string(sequence) + extension);
    }
    if (::rename(_path.c_str(), backup.c_str()) != 0) {
        return error("archive", errno);
    }
    // 在归档安全完成且新文件成功打开之前保留旧文件描述符。
    const int fresh = ::open(_path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_APPEND | O_CLOEXEC, 0644);
    if (fresh < 0) {
        return error("create active file", errno);
    }
    const int old = _fd;
    _fd = fresh;
    _period = period(now);
    _interval_start = gst_log_steady_time();
    if (old >= 0 && ::close(old) != 0) {
        return error("close archive", errno);
    }
    return true;
}

bool LogFile::write(const std::string& message) {
    struct stat path_info{}, file_info{};
    const int result = ::stat(_path.c_str(), &path_info);
    if (result != 0 && errno != ENOENT) {
        return error("stat path", errno);
    }
    if (result != 0 || _fd < 0 || ::fstat(_fd, &file_info) != 0 ||
        path_info.st_dev != file_info.st_dev || path_info.st_ino != file_info.st_ino) {
        if (!reopen()) {
            return false;
        }
    }
    if (::fstat(_fd, &file_info) != 0) {
        return error("stat file", errno);
    }
    const auto now = gst_log_wall_time();
    const bool size_due = _max_size > 0 && static_cast<std::uintmax_t>(file_info.st_size) >= _max_size;
    const bool time_due = _natural != NaturalRotation::None && period(now) != _period;
    const bool interval_due = _interval.count() > 0 && gst_log_steady_time() - _interval_start >= _interval;
    if (file_info.st_size > 0 && (size_due || time_due || interval_due) && !rotate(now)) {
        return false;
    }
    std::size_t offset = 0;
    while (offset < message.size()) {
        const auto count = ::write(_fd, message.data() + offset, message.size() - offset);
        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count <= 0) {
            const int code = count == 0 ? EIO : errno;
            ::close(_fd);
            _fd = -1;
            return error("write", code);
        }
        offset += static_cast<std::size_t>(count);
    }
    return true;
}

bool LogFile::close() {
    if (_fd >= 0) {
        const int descriptor = _fd;
        _fd = -1;
        if (::close(descriptor) != 0) {
            return error("close", errno);
        }
    }
    return !_had_error;
}
}
}
}
