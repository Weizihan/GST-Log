#pragma once
#include <memory>
#include "LogFormat.h"

namespace GST {
namespace LOG {

struct LogExtraConfig {
    virtual ~LogExtraConfig() = default;
};

struct LogConfig {

    std::string _logger_name = "";

    std::string _log_type = "file";

    LOG_LEVEL _log_level = LOG_LEVEL::LEVEL_FATAL;

    std::string _log_format;

    std::string _log_target = "log";
    //IP or IO_output
    std::string _log_encoding = "UTF-8";

    std::string _log_file_name = "log";

    bool _trunback_type = 1;


    int _log_max_size;
    
    //AsyncLog
    int _buffer_size = 64 * 1024;
    
    //ConsoleLog
    bool _is_colorful_log;

    virtual ~LogConfig() = default;

};
}
}