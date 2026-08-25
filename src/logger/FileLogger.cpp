#include "FileLogger.h"

#include <chrono>
#include <cstdio>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace GST {
namespace LOG {

static std::string current_date_str() {
    auto now = std::chrono::system_clock::now();
    auto tt  = std::chrono::system_clock::to_time_t(now);
    struct tm tm_info{};
    localtime_r(&tt, &tm_info);
    char buf[9];
    strftime(buf, sizeof(buf), "%Y%m%d", &tm_info);
    return buf;
}

static std::string current_datetime_str() {
    auto now = std::chrono::system_clock::now();
    auto tt  = std::chrono::system_clock::to_time_t(now);
    struct tm tm_info{};
    localtime_r(&tt, &tm_info);
    char buf[16];
    strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", &tm_info);
    return buf;
}

static bool next_rotation_path(
    const std::filesystem::path& log_path,
    std::filesystem::path& result) {
    const std::string timestamp = current_datetime_str();
    const std::string stem = log_path.stem().string() + "_" + timestamp;
    const std::string extension = log_path.extension().string();
    std::filesystem::path candidate =
        log_path.parent_path() / (stem + extension);
    std::error_code ec;
    for (std::size_t sequence = 1;; ++sequence) {
        const bool candidate_exists = std::filesystem::exists(candidate, ec);
        if (ec) {
            return false;
        }
        if (!candidate_exists) {
            result = std::move(candidate);
            return true;
        }
        candidate = log_path.parent_path() /
            (stem + "_" + std::to_string(sequence) + extension);
    }
}

FileLogger::~FileLogger() {
    std::lock_guard<std::mutex> shutdown_lock(_shutdown_mutex);
    _begin = false;
    std::lock_guard<std::timed_mutex> lock(_mutex);
    _accepting = false;
    if (_log_stream.is_open()) {
        _log_stream.flush();
        _log_stream.close();
    }
    _shutdown_complete = true;
}

bool FileLogger::init(const LogConfig& config) {
    _log_path = config._log_target;
    std::error_code ec;
    const auto parent = _log_path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, ec);
    }

    _log_stream.open(_log_path, std::ios::out | std::ios::trunc | std::ios::binary);
    if(!_log_stream.is_open()) {
        std::cerr << "open file path failed: " << _log_path << std::endl;
        return false;
    }
    _level  = config._log_level;
    _format = LogFormat(config._log_format);
    _name   = config._logger_name;

    if (config._trunback_type) {
        _trunc_type = TRUNC_TYPE_SYS_TIME;
    } else if (config._log_max_size > 0) {
        _trunc_type      = TRUNC_TYPE_FILE_SZIE;
        _trunc_threshold = config._log_max_size;
    } else {
        _trunc_type = TRUNC_TYPE_NONE;
    }

    _current_date = current_date_str();
    _accepting = true;
    _shutdown_complete = false;
    _begin = true;
    return true;
}

bool FileLogger::trunc_log() {
    std::lock_guard<std::timed_mutex> lock(_mutex);
    try {
        const bool success = trunc_log_unlocked();
        if (!success) {
            report_degraded_unlocked();
        }
        return success;
    } catch (...) {
        report_degraded_unlocked();
        return false;
    }
}

bool FileLogger::trunc_log_unlocked() {
    // 文件被外部删除时重新创建
    std::error_code ec;
    const bool log_exists = std::filesystem::exists(_log_path, ec);
    if (ec) {
        return false;
    }

    if (!log_exists || !_log_stream.is_open() || !_log_stream.good()) {
        _log_stream.close();
        _log_stream.clear();
        const auto parent = _log_path.parent_path();
        if (!parent.empty()) {
            std::filesystem::create_directories(parent, ec);
            if (ec) {
                return false;
            }
        }
        _log_stream.open(_log_path, std::ios::out | std::ios::app | std::ios::binary);
        return _log_stream.is_open() && _log_stream.good();
    }

    switch (_trunc_type) {
        case TRUNC_TYPE_NONE:
            return true;

        case TRUNC_TYPE_FILE_SZIE: {
            auto sz = std::filesystem::file_size(_log_path, ec);
            if (ec) {
                return false;
            }
            if (sz >= static_cast<uintmax_t>(_trunc_threshold)) {
                return rotate_file();
            }
            return true;
        }

        case TRUNC_TYPE_SYS_TIME: {
            std::string today = current_date_str();
            if (today != _current_date) {
                _current_date = today;
                return rotate_file();
            }
            return true;
        }
    }
    return true;
}

// 关闭当前文件，重命名为带时间戳的备份，再开新文件
bool FileLogger::rotate_file() {
    std::filesystem::path backup;
    if (!next_rotation_path(_log_path, backup)) {
        return false;
    }

    _log_stream.flush();
    _log_stream.close();

    std::error_code ec;
    std::filesystem::rename(_log_path, backup, ec);
    bool backup_created = !ec;
    // rename 失败（如跨设备）时降级为复制+删除
    if (ec) {
        std::filesystem::copy_file(_log_path, backup,
            std::filesystem::copy_options::overwrite_existing, ec);
        if (!ec) {
            backup_created = true;
            std::filesystem::remove(_log_path, ec);
        }
    }

    if (!backup_created) {
        // 备份失败时只能继续追加原文件。此处绝不能以 trunc 方式重开，
        // 否则一次轮转错误会同时破坏现有日志和当前待写日志。
        _log_stream.clear();
        _log_stream.open(_log_path, std::ios::out | std::ios::app | std::ios::binary);
        if (!_log_stream.is_open()) {
            std::cerr << "rotate_file: backup failed and original file could not be reopened: "
                      << _log_path << std::endl;
            return false;
        }
        std::cerr << "rotate_file: backup failed; continuing with original file: "
                  << _log_path << std::endl;
        return true;
    }

    _log_stream.clear();
    _log_stream.open(_log_path, std::ios::out | std::ios::trunc | std::ios::binary);
    return _log_stream.is_open();
}

bool FileLogger::write_log(const buffer& log) {
    std::lock_guard<std::timed_mutex> lock(_mutex);
    if (!_accepting) {
        return false;
    }
    try {
        const bool success = write_log_unlocked(log);
        if (success) {
            report_recovered_unlocked();
        } else {
            report_degraded_unlocked();
        }
        return success;
    } catch (...) {
        report_degraded_unlocked();
        return false;
    }
}

bool FileLogger::write_log_unlocked(const buffer& log) {
    _log_stream.write(log.data(), static_cast<std::streamsize>(log.size()));
    if(!_log_stream.good()) {
        return false;
    }
    _log_stream.flush();
    return _log_stream.good();
}

void FileLogger::log(LOG_LEVEL level, std::string& log, const char* file,
                     int line, const char* func) {
    if (!_begin || level < _level) {
        return;
    }

    std::lock_guard<std::timed_mutex> lock(_mutex);
    if (!_accepting) {
        return;
    }
    try {
        if (!_format.format(level, log, file, line, func)) {
            return;
        }
        if (!trunc_log_unlocked() || !write_log_unlocked(log)) {
            report_degraded_unlocked();
            return;
        }
        report_recovered_unlocked();
    } catch (...) {
        report_degraded_unlocked();
    }
}

bool FileLogger::flush(std::chrono::milliseconds timeout) {
    const auto non_negative_timeout =
        timeout < std::chrono::milliseconds::zero()
            ? std::chrono::milliseconds::zero()
            : timeout;
    std::unique_lock<std::timed_mutex> lock(_mutex, std::defer_lock);
    if (!lock.try_lock_until(
            std::chrono::steady_clock::now() + non_negative_timeout)) {
        return false;
    }
    if (_shutdown_complete) {
        return true;
    }
    if (!_log_stream.good() && !trunc_log_unlocked()) {
        report_degraded_unlocked();
        return false;
    }
    _log_stream.flush();
    const bool success = _log_stream.good();
    if (success) {
        report_recovered_unlocked();
    } else {
        report_degraded_unlocked();
    }
    return success;
}

bool FileLogger::shutdown(std::chrono::milliseconds timeout) {
    std::lock_guard<std::mutex> shutdown_lock(_shutdown_mutex);
    const auto non_negative_timeout =
        timeout < std::chrono::milliseconds::zero()
            ? std::chrono::milliseconds::zero()
            : timeout;
    const auto deadline = std::chrono::steady_clock::now() + non_negative_timeout;
    _begin = false;
    _accepting = false;

    std::unique_lock<std::timed_mutex> lock(_mutex, std::defer_lock);
    if (!lock.try_lock_until(deadline)) {
        return false;
    }
    if (_shutdown_complete) {
        return true;
    }
    if (!_log_stream.good() && !trunc_log_unlocked()) {
        report_degraded_unlocked();
        return false;
    }
    _log_stream.flush();
    if (!_log_stream.good()) {
        report_degraded_unlocked();
        return false;
    }
    _log_stream.close();
    _shutdown_complete = true;
    return true;
}

void FileLogger::report_degraded_unlocked() noexcept {
    if (!_degraded) {
        std::fputs("GST_log: file logger degraded; retrying on next log\n", stderr);
    }
    _degraded = true;
}

void FileLogger::report_recovered_unlocked() noexcept {
    if (_degraded) {
        std::fputs("GST_log: file logger recovered\n", stderr);
    }
    _degraded = false;
}


}
}
