#pragma once
#include <string>
#include "Marco.h"

namespace GST{
namespace LOG{

/*
    format parrten
    %P  :   thread_id
    %L  ：  level
    %T  ：  time
    %F  :   func
    %I  :   line
    %S  :   log
    %R  :   file
*/
class LogFormat{
public:
    LogFormat() = default;
    LogFormat(const std::string& format):_pattern(format){};
    bool format(LOG_LEVEL level, std::string& log, const char* file,
                int line, const char* func);
private:
    std::string get_filepath(std::string&& file_path);
    std::string get_curtime();
    std::string get_loglevelstr(GST::LOG::LOG_LEVEL level);
    unsigned long int get_thread_id();

    std::string _pattern;
    LOG_LEVEL _level;
};

}
}