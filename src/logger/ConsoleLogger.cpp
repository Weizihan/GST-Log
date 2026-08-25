#include "ConsoleLogger.h"

#include <cstdio>

namespace GST {
namespace LOG {

ConsoleLogger::~ConsoleLogger() {
    std::lock_guard<std::mutex> shutdown_lock(_shutdown_mutex);
    _begin = false;
    std::lock_guard<std::timed_mutex> lock(_mutex);
    _accepting = false;
    std::fflush(stdout);
    _shutdown_complete = true;
}

bool ConsoleLogger::init(const GST::LOG::LogConfig& config) {
    _level = config._log_level;
    _format = LogFormat(config._log_format);
    _name = config._logger_name;

    _is_colorful_log = config._is_colorful_log;
    _accepting = true;
    _shutdown_complete = false;
    _begin = true;
    return true;
}

bool ConsoleLogger::write_log(const buffer& log) {
    std::lock_guard<std::timed_mutex> lock(_mutex);
    if (!_accepting) {
        return false;
    }
    return write_log_unlocked(log);
}

bool ConsoleLogger::write_log_unlocked(const buffer& log) {
    printf("%s", log.c_str());
    return true;
}

void ConsoleLogger::log(LOG_LEVEL level, std::string& log, const char* file,
                int line, const char* func) {
    if (!_begin || level < _level) {
        return;
    }
    std::lock_guard<std::timed_mutex> lock(_mutex);
    if (!_accepting) {
        return;
    }
    if (!_format.format(level, log, file, line, func)) {
        return;
    }
    if (_is_colorful_log) {
        level_to_color(level, log);
    }
    write_log_unlocked(log);
}

bool ConsoleLogger::flush(std::chrono::milliseconds timeout) {
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
    return std::fflush(stdout) == 0;
}

bool ConsoleLogger::shutdown(std::chrono::milliseconds timeout) {
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
    if (std::fflush(stdout) != 0) {
        return false;
    }
    _shutdown_complete = true;
    return true;
}

void ConsoleLogger::level_to_color(LOG_LEVEL level, std::string& log_msg) {
        std::string color_code;
        switch (level) {
            case GST::LOG::LOG_LEVEL::LEVEL_DEBUG:
                color_code = "\033[33m";
                break;
            case GST::LOG::LOG_LEVEL::LEVEL_ERROR:
                color_code = "\033[31m";
                break;
            case GST::LOG::LOG_LEVEL::LEVEL_FATAL:
                color_code = "\033[1;31m";
                break;
            case GST::LOG::LOG_LEVEL::LEVEL_INFO:
                color_code = "\033[34m";
                break;
            case GST::LOG::LOG_LEVEL::LEVEL_WARN:
                color_code = "\033[35m";
                break;
            default:
                color_code = "\033[0m";
                break;
        }
        log_msg = color_code + log_msg + "\033[0m";


}
}
}
