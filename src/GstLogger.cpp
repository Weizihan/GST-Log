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
        if (_Logger_ptrs.empty()) {
            return;
        }

        if (index < 0 || static_cast<size_t>(index) >= _Logger_ptrs.size()) {
            loggers.reserve(_Logger_ptrs.size());
            for (auto& ptr : _Logger_ptrs) {
                loggers.push_back(ptr.get());
            }
        } else {
            loggers.push_back(_Logger_ptrs.at(index).get());
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
    std::unique_ptr<Logger> logger_ptr;

    if (config._logger_name == "Async") {
        logger_ptr = std::make_unique<AsyncFileLogger>();
    } else if (config._logger_name == "File") {
        logger_ptr = std::make_unique<FileLogger>();
    } else if (config._logger_name == "Console") {
        logger_ptr = std::make_unique<ConsoleLogger>();
    } else {
        return false;
    }

    if (!logger_ptr || !logger_ptr->init(config)) {
        return false;
    }

    std::lock_guard<std::mutex> lock(_loggers_mutex);
    for (const auto& existing : _Logger_ptrs) {
        if (existing && existing->get_name() == config._logger_name) {
            return false;
        }
    }
    _Logger_ptrs.emplace_back(std::move(logger_ptr));
    _is_start = true;
    return true;
}

// 无参数调用， 使用默认的配置
bool GstLogger::init() {
    std::lock_guard<std::mutex> lock(_loggers_mutex);
    if(_is_start) {
        return false;
    }
    std::unique_ptr<Logger> default_logger_ptr = std::make_unique<ConsoleLogger>();
    LogConfig default_log;
    default_log._logger_name = "log";
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
