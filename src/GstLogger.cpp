#include "GstLogger.h"

#include <iostream>
#include <fstream>
#include <cstdarg>
#include <memory>
#include <vector>
#include <cstdio>

#include "logger/AsyncFileLogger.h"
#include "logger/FileLogger.h"
#include "logger/ConsoleLogger.h"
#include "LogFormat.h"


using namespace GST::LOG;

namespace {

void report_uninitialized_log(const std::string& format, va_list args) noexcept {
    ::flockfile(stderr);
    std::fputs("GST_log error: logger is not initialized: ", stderr);
    if (std::vfprintf(stderr, format.c_str(), args) < 0) {
        std::fputs("<message formatting failed>", stderr);
    }
    std::fputc('\n', stderr);
    ::funlockfile(stderr);
}

std::chrono::milliseconds remaining_timeout(
    std::chrono::steady_clock::time_point deadline) {
    const auto now = std::chrono::steady_clock::now();
    if (now >= deadline) {
        return std::chrono::milliseconds::zero();
    }
    return std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now);
}

} // anonymous namespace

GstLogger::GstLogger() {
    _is_start = false;
}

std::shared_ptr<GST::LOG::GstLogger> GstLogger::get_Instance() {
    static std::shared_ptr<GstLogger> instance(new GstLogger());
    return instance;
}

LOG_LEVEL string_to_log_level(const std::string& level) {
    if (level == "INFO") return LOG_LEVEL::LEVEL_INFO;
    if (level == "WARN") return LOG_LEVEL::LEVEL_WARN;
    if (level == "ERROR") return LOG_LEVEL::LEVEL_ERROR;
    if (level == "FATAL") return LOG_LEVEL::LEVEL_FATAL;
    if (level == "DEBUG") return LOG_LEVEL::LEVEL_DEBUG;
    throw std::invalid_argument("Invalid log level: " + level);
}

void GstLogger::log(int index, LOG_LEVEL level, const char* file,
                    int line, const char* func, const std::string& format, ...) {
    std::vector<Logger*> loggers;
    {
        std::lock_guard<std::mutex> lock(_loggers_mutex);
        if (_shutdown_started) {
            return;
        }
        if (_Logger_ptrs.empty()) {
            va_list args;
            va_start(args, format);
            report_uninitialized_log(format, args);
            va_end(args);
            return;
        }

        if (index == -1) {
            loggers.reserve(_Logger_ptrs.size());
            for (auto& ptr : _Logger_ptrs) {
                loggers.push_back(ptr.get());
            }
        } else if (index >= 0 && static_cast<size_t>(index) < _Logger_ptrs.size()) {
            loggers.push_back(_Logger_ptrs.at(index).get());
        } else {
            std::fputs("GST_log error: invalid logger index\n", stderr);
            return;
        }
    }

    va_list args;
    va_start(args, format);
    va_list args_copy;
    va_copy(args_copy, args);
    int size = std::vsnprintf(nullptr, 0, format.c_str(), args_copy);
    va_end(args_copy);
    if(size < 0) {
        va_end(args);
        return;
    }
    std::string msg;
    msg.resize(static_cast<size_t>(size));
    std::vsnprintf(msg.data(), static_cast<size_t>(size) + 1, format.c_str(), args);
    va_end(args);

    for (auto* logger_ptr : loggers) {
        if (logger_ptr != nullptr) {
            std::string msg_copy = msg;
            logger_ptr->log(level, msg_copy, file, line, func);
        }
    }
}

void GstLogger::log(const std::string& name, LOG_LEVEL level, const char* file,
                    int line, const char* func, const std::string& format, ...) {
    {
        std::lock_guard<std::mutex> lock(_loggers_mutex);
        if (_shutdown_started) {
            return;
        }
        if (_Logger_ptrs.empty()) {
            va_list args;
            va_start(args, format);
            report_uninitialized_log(format, args);
            va_end(args);
            return;
        }
    }

    int index = get_logger_index_by_name(name);
    if (index < 0) {
        return;
    }
    va_list args;
    va_start(args, format);
    va_list args_copy;
    va_copy(args_copy, args);
    int size = std::vsnprintf(nullptr, 0, format.c_str(), args_copy);
    va_end(args_copy);
    if(size < 0) {
        va_end(args);
        return;
    }
    std::string msg;
    msg.resize(static_cast<size_t>(size));
    std::vsnprintf(msg.data(), static_cast<size_t>(size) + 1, format.c_str(), args);
    va_end(args);
    log(index, level, file, line, func, "%s", msg.c_str());
}

int GstLogger::get_logger_index_by_name(const std::string& name) {
    std::lock_guard<std::mutex> lock(_loggers_mutex);
    for(int i = 0; i < static_cast<int>(_Logger_ptrs.size()) ; i++) {
        if(name == _Logger_ptrs.at(i)->get_name()) {
            return i;
        }
    }
    return -1;
}

bool GstLogger::init(const LogConfig& config) {
    if (config._logger_name.empty()) {
        return false;
    }

    std::unique_ptr<Logger> logger_ptr;

    switch (config._logger_type) {
    case LoggerType::AsyncFile:
        logger_ptr = std::make_unique<AsyncFileLogger>();
        break;
    case LoggerType::File:
        logger_ptr = std::make_unique<FileLogger>();
        break;
    case LoggerType::Console:
        logger_ptr = std::make_unique<ConsoleLogger>();
        break;
    }

    std::lock_guard<std::mutex> lock(_loggers_mutex);
    if (_shutdown_started) {
        return false;
    }
    for (const auto& existing : _Logger_ptrs) {
        if (existing && existing->get_name() == config._logger_name) {
            return false;
        }
    }

    // 同名检查和后端初始化保持在同一配置临界区内，避免并发 init
    // 在最终发现重名之前打开或截断目标文件。
    if (!logger_ptr || !logger_ptr->init(config)) {
        return false;
    }

    _Logger_ptrs.emplace_back(std::move(logger_ptr));
    _is_start = true;
    return true;
}

// 无参数调用， 使用默认的配置
bool GstLogger::init() {
    std::lock_guard<std::mutex> lock(_loggers_mutex);
    if(_is_start || _shutdown_started) {
        return false;
    }
    std::unique_ptr<Logger> default_logger_ptr = std::make_unique<ConsoleLogger>();
    LogConfig default_log;
    default_log._logger_name = "default";
    default_log._logger_type = LoggerType::Console;
    default_log._log_level = LOG_LEVEL::LEVEL_DEBUG;
    default_log._log_format = "[%L][%T%R/%F%I]:%S";
    default_log._log_target = "";
    default_log._log_encoding = "UTF-8";
    default_log._log_file_name = "log.txt";
    default_log._trunback_type = true;
    default_log._log_max_size = 1024 * 1024;
    default_log._buffer_size = 64 * 1024;
    default_log._is_colorful_log = true;
    default_logger_ptr->init(default_log);
    _Logger_ptrs.emplace_back(std::move(default_logger_ptr));
    _is_start = true;

    return true;
}

bool GstLogger::flush(std::chrono::milliseconds timeout) {
    std::vector<Logger*> loggers;
    {
        std::lock_guard<std::mutex> lock(_loggers_mutex);
        if (_Logger_ptrs.empty()) {
            return false;
        }
        loggers.reserve(_Logger_ptrs.size());
        for (auto& logger : _Logger_ptrs) {
            loggers.push_back(logger.get());
        }
    }

    const auto non_negative_timeout =
        timeout < std::chrono::milliseconds::zero()
            ? std::chrono::milliseconds::zero()
            : timeout;
    const auto deadline = std::chrono::steady_clock::now() + non_negative_timeout;
    bool success = true;
    for (auto* logger : loggers) {
        if (logger != nullptr && !logger->flush(remaining_timeout(deadline))) {
            success = false;
        }
    }
    return success;
}

bool GstLogger::shutdown(std::chrono::milliseconds timeout) {
    std::vector<Logger*> loggers;
    {
        std::lock_guard<std::mutex> lock(_loggers_mutex);
        _shutdown_started = true;
        loggers.reserve(_Logger_ptrs.size());
        for (auto& logger : _Logger_ptrs) {
            loggers.push_back(logger.get());
        }
    }

    const auto non_negative_timeout =
        timeout < std::chrono::milliseconds::zero()
            ? std::chrono::milliseconds::zero()
            : timeout;
    const auto deadline = std::chrono::steady_clock::now() + non_negative_timeout;
    bool success = true;
    for (auto* logger : loggers) {
        if (logger != nullptr && !logger->shutdown(remaining_timeout(deadline))) {
            success = false;
        }
    }
    if (success) {
        std::lock_guard<std::mutex> lock(_loggers_mutex);
        _is_start = false;
    }
    return success;
}
