#pragma once
#include <memory>
#include <chrono>
#include <cstddef>
#include "LogFormat.h"

namespace GST {
namespace LOG {

struct LogExtraConfig {
    virtual ~LogExtraConfig() = default;
};

enum class NaturalRotation { Legacy, None, Hourly, Daily };

struct LogConfig {

    std::string _logger_name = "";

    // Backend type is independent of the user-defined instance name.
    std::string _log_type = "file";

    LOG_LEVEL _log_level = LOG_LEVEL::LEVEL_FATAL;

    std::string _log_format = "[%T][%L] %S";

    std::string _log_target = "log";
    //IP or IO_output
    std::string _log_encoding = "UTF-8";

    std::string _log_file_name = "log";

    bool _trunback_type = true;
    NaturalRotation _natural_rotation = NaturalRotation::Legacy;
    std::chrono::seconds _rotation_interval{0};


    int _log_max_size = 0;
    
    //AsyncLog
    int _buffer_size = 64 * 1024;
    std::size_t _max_pending_logs = 8192;
    std::size_t _max_pending_bytes = 8 * 1024 * 1024;
    
    //ConsoleLog
    bool _is_colorful_log = false;

    virtual ~LogConfig() = default;

};
}
}
