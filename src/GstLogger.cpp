#include "GstLogger.h"

#include <iostream>
#include <fstream>
#include <cstdarg>
#include <string.h>
#include <memory>

#include "logger/AsyncFileLogger.h"
#include "logger/FileLogger.h"
#include "logger/ConsoleLogger.h"
#include "LogFormat.h"


using namespace GST::LOG;

std::shared_ptr<GST::LOG::GstLogger> GstLogger::_instances = nullptr;

GstLogger::GstLogger() {
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
    va_list args;
    va_start(args, format);
    char buffer[256] = {0};
    int size = std::vsnprintf(buffer, 256, format.c_str(), args);
    if(size < 0) {
        return;
    }

    buffer[sizeof(buffer) - 1] = '\0';
    va_end(args);
    std::string msg = std::string(buffer);
    if (index == -1 || index > _Logger_ptrs.size()) {
        for(const auto& logger_ptr : _Logger_ptrs) {
            logger_ptr->log(level, msg, file, line, func);
        }
    } else {
        _Logger_ptrs.at(index)->log(level, msg, file, line, func);
    }
}

void GstLogger::log(const std::string& name, LOG_LEVEL level, const char* file,
                    int line, const char* func, const std::string& format, ...) {
    int index = GstLogger::get_Instance()->get_logger_index_by_name(name);
    va_list args;
    va_start(args, format);
    char buffer[256] = {0};
    int size = std::vsnprintf(buffer, 256, format.c_str(), args);
    if(size < 0) {
        return;
    }

    buffer[sizeof(buffer) - 1] = '\0';
    va_end(args);
    std::string msg = std::string(buffer);
    if (index == -1 || index > _Logger_ptrs.size()) {
        for(const auto& logger_ptr : _Logger_ptrs) {
            logger_ptr->log(level, msg, file, line, func);
        }
    } else {
        _Logger_ptrs.at(index)->log(level, msg, file, line, func);
    }
}

int GstLogger::get_logger_index_by_name(const std::string& name) {
    for(int i = 0; i < _Logger_ptrs.size() ; i++) {
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

    return logger_ptr ? logger_ptr->init(config) : false;
}

// 无参数调用， 使用默认的配置
bool GstLogger::init() {
    if(_is_start) {
        return false;
    }
    std::unique_ptr<Logger> default_logger_ptr = std::make_unique<ConsoleLogger>();
    LogConfig default_log;
    default_log._logger_name = "log";
    default_log._log_level = LOG_LEVEL::LEVEL_FATAL;
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

    return true;
}
