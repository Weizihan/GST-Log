#include "GstLogger.h"

#include <iostream>
#include <fstream>
#include <cstdarg>

#include "logger/AsyncFileLogger.h"
#include "logger/FileLogger.h"
#include "logger/ConsoleLogger.h"
#include "logger/LogConfig.h"
#include "logger/LogFormat.h"
#include "../third_party/json/include/nlohmann/json.hpp"


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
    //printf("%d %s %d %s\n", index, file, line, func);
    va_list args;
    va_start(args, format);
    char buffer[256] = {0};
    int size = std::vsnprintf(buffer, 256, format.c_str(), args);
    //std::cout << "size:" << size << std::endl;
    if(size < 0) {
        return;
    }

    buffer[sizeof(buffer) - 1] = '\0';
    //printf("buffer:%s\n", buffer);
    va_end(args);
    std::string msg = std::string(buffer);
    //std::cout << "msg" << msg << std::endl;
    if (index == -1 || index > _Logger_ptrs.size()) {
        for(const auto& logger_ptr : _Logger_ptrs) {
            logger_ptr->log(level, msg, file, line, func);
        }
    } else {
        _Logger_ptrs.at(index)->log(level, msg, file, line, func);
    }
}

void GstLogger::log(const char* name, LOG_LEVEL level, const char* file,
                    int line, const char* func, const std::string& format, ...) {
    int index = GstLogger::get_Instance()->get_logger_index_by_name(name);
    //printf("%d %s %d %s\n", index, file, line, func);
    va_list args;
    va_start(args, format);
    char buffer[256] = {0};
    int size = std::vsnprintf(buffer, 256, format.c_str(), args);
    //std::cout << "size:" << size << std::endl;
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

int GstLogger::get_logger_index_by_name(const char* name) {
    for(int i = 0; i < _Logger_ptrs.size() ; i++) {
        if(!strcmp(name, _Logger_ptrs.at(i)->get_name())) {
            return i;
        }
    }
    return -1;
}

bool GstLogger::init(const std::string& conf_path) {
    if (!read_json_conf(conf_path)) {
        std::cerr << "read_json_conf failed" << std::endl;
        return false;
    }
    std::cout << "read_json_conf succeed" << std::endl;
    return true;
}

bool GstLogger::read_json_conf(const std::string& json_conf) {
    std::ifstream conf_stream(json_conf);
    if (!conf_stream.is_open()) {
        std::cerr << "Failed to open " << json_conf << ": " << std::strerror(errno) << ", using default log_config" << std::endl;
        return false;
    }
    nlohmann::json json_data =  nlohmann::json::parse(conf_stream);

    for(const auto& config_name : json_data["LOG_CONFIGS"]) {
        const auto& log_config_data  = json_data.at(config_name.get<std::string>());
        auto config_ptr = std::make_unique<LogConfig>();
        if (log_config_data.contains("LOG_TYPE")) {
            log_config_data.at("LOG_TYPE").get_to(config_ptr->_log_type);
        }
        
        if (log_config_data.contains("LOG_LEVEL")) {
            std::string level_str;
            log_config_data.at("LOG_LEVEL").get_to(level_str);
            try {
                config_ptr->_log_level = string_to_log_level(level_str);
            } catch (const std::invalid_argument& e) {
                std::cerr << e.what() << std::endl;
                return false;
            }
        }

        if (log_config_data.contains("LOG_FORMAT")) {
            log_config_data.at("LOG_FORMAT").get_to(config_ptr->_log_format);
        }

        if (log_config_data.contains("LOG_TARGET")) {
            log_config_data.at("LOG_TARGET").get_to(config_ptr->_log_target);
        }

        if (log_config_data.contains("LOG_ENCODING")) {
            log_config_data.at("LOG_ENCODING").get_to(config_ptr->_log_encoding);
        }

        if (log_config_data.contains("LOG_MAX_SIZE")) {
            log_config_data.at("LOG_MAX_SIZE").get_to(config_ptr->_log_max_size);
        }

        if (log_config_data.contains("BUFFER_SIZE")) {
            log_config_data.at("BUFFER_SIZE").get_to(config_ptr->_buffer_size);
        }

        if (log_config_data.contains("IS_COLORFUL_LOG")) {
            log_config_data.at("IS_COLORFUL_LOG").get_to(config_ptr->_is_colorful_log);
        }


        if (config_ptr->_log_type == "Async") {
            auto logger_ptr = std::make_unique<AsyncFileLogger>();
            _Logger_ptrs.emplace_back(std::move(logger_ptr));
        } else if (config_ptr->_log_type == "File") {
            auto logger_ptr = std::make_unique<FileLogger>();
            _Logger_ptrs.emplace_back(std::move(logger_ptr));
        } else if (config_ptr->_log_type == "Console") {
            auto logger_ptr = std::make_unique<ConsoleLogger>();
            _Logger_ptrs.emplace_back(std::move(logger_ptr));
        } 
        
        for (auto &logger_ptr : _Logger_ptrs) {
            if(logger_ptr->init(*config_ptr)) {
            }else{
                return false;
            }
        }
    }
    return true;
}