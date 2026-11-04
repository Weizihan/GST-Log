#pragma once
#include <string>
#include <string_view>
#include "Marco.h"

namespace GST {
namespace LOG {

/*
    格式化占位符：
    %P：线程 ID
    %L：日志级别
    %T：时间
    %F：函数名
    %I：行号
    %S：日志消息
    %R：源文件名
*/
class LogFormat {
public:
    LogFormat();
    LogFormat(const std::string& format);

    bool format(LOG_LEVEL level, std::string& log, const char* file,
                int line, const char* func);
private:
    std::string get_filepath(std::string_view file_path);
    std::string get_curtime();
    std::string get_loglevelstr(GST::LOG::LOG_LEVEL level);
    unsigned long int get_thread_id();

    std::string _pattern;
};

}
}
