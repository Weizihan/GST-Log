#pragma once
#include <string>
#include <vector>
#include <memory>

#include "LogConfig.h"

#include <type_traits>

#include <iostream>

namespace GST{
namespace LOG{

using buffer = std::string;
using bufferptr = std::unique_ptr<buffer>;
using buffervecptr = std::unique_ptr<std::vector<buffer>>;
using std::vector;


class Logger {
public:
    Logger(){};
    
    virtual ~Logger() = default;
    
    virtual bool init(const GST::LOG::LogConfig& config) = 0; 

    virtual bool write_log(const buffer& log) = 0;
    
    virtual void log(LOG_LEVEL level, std::string& log, const char* file,
                int line, const char* func) {
        if(!_begin) {
            return;
        }
        if(level > _log_level) {
            return;
        }
        if(_log_format.format(level, log, file, line, func)) {
            write_log(log);
        }
        return;
    }

    inline const char* get_name() const { return _logger_name.c_str();};

    inline bool format_log(LOG_LEVEL level, std::string& log, const char* file,
                int line, const char* func) { 
                    return _log_format.format(level, log, file, line, func);
                };

protected:
    std::string _logger_name;
    std::string _log_type;
    LOG_LEVEL _log_level;
    LogFormat _log_format;
    std::string _log_target;
    std::string _log_encoding;
    bool _begin = false;
    
};

}
}