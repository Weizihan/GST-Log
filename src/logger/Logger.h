#pragma once
#include <string>
#include <vector>
#include <memory>
#include <iostream>

#include "LogConfig.h"
#include "Marco.h"

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
        if(level > _level) {
            return;
        }
        if(_format.format(level, log, file, line, func)) {
            write_log(log);
        }
        return;
    }

    inline const std::string& get_name() const { return _name;};

protected:
    // logger的名称
    std::string _name;
    LOG_LEVEL _level;
    LogFormat _format;
    std::string _target;
    std::string _encoding;
    
    // 日志截断相关参数
    LOG_TRUNC_TYPE _trunc_type;
    int _trunc_threshold;

    bool _begin = false;
};

}
}