#include "ConsoleLogger.h"

#include <cstdio>

#include "LogConfig.h"

namespace GST {
namespace LOG {
ConsoleLogger::~ConsoleLogger() {
    shutdown();
}

bool ConsoleLogger::init(const LogConfig& config) {
    std::lock_guard<std::mutex> lock(_mutex);
    if (!configure(config)) {
        return false;
    }
    _is_colorful_log = config._is_colorful_log;
    start();
    return true;
}

bool ConsoleLogger::write_log(const buffer& log) {
    std::lock_guard<std::mutex> lock(_mutex);
    if (!_begin) {
        return false;
    }
    if (std::fwrite(log.data(), 1, log.size(), stdout) != log.size()) {
        _had_error = true;
        std::fputs("GST_log: console write failed\n", stderr);
        return false;
    }
    return true;
}

bool ConsoleLogger::flush() {
    std::lock_guard<std::mutex> lock(_mutex);
    if (std::fflush(stdout) != 0) {
        _had_error = true;
        std::fputs("GST_log: console flush failed\n", stderr);
    }
    return !_had_error;
}

bool ConsoleLogger::shutdown() {
    std::lock_guard<std::mutex> lock(_mutex);
    _begin = false;
    if (std::fflush(stdout) != 0) {
        _had_error = true;
    }
    return !_had_error;
}

void ConsoleLogger::decorate(LOG_LEVEL level, std::string& message) {
    if (!_is_colorful_log) {
        return;
    }
    const char* color = "\033[0m";
    switch (level) {
    case LOG_LEVEL::LEVEL_DEBUG:
        color = "\033[33m";
        break;
    case LOG_LEVEL::LEVEL_INFO:
        color = "\033[34m";
        break;
    case LOG_LEVEL::LEVEL_WARN:
        color = "\033[35m";
        break;
    case LOG_LEVEL::LEVEL_ERROR:
        color = "\033[31m";
        break;
    case LOG_LEVEL::LEVEL_FATAL:
        color = "\033[1;31m";
        break;
    }
    // 即使启用彩色输出，也始终让换行符位于末尾。
    message.insert(0, color);
    message.insert(message.size() - 1, "\033[0m");
}
}
}
