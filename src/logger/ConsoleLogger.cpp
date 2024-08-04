#include "ConsoleLogger.h"
namespace GST {
namespace LOG {

bool ConsoleLogger::init(const GST::LOG::LogConfig& config) {
    _level = config._log_level;
    _format = LogFormat(config._log_format);
    _name = config._logger_name;

    _is_colorful_log = config._is_colorful_log;
    _begin = true;
    return true;
}

bool ConsoleLogger::write_log(const buffer& log) {
    printf("%s", log.c_str());
    return true;
}

void ConsoleLogger::log(LOG_LEVEL level, std::string& log, const char* file,
                int line, const char* func) {
        if(!_begin) {
            return;
        }
        if(level > _level) {
            return;
        }
        if(_format.format(level, log, file, line, func)) {
            if(_is_colorful_log) {
                level_to_color(level, log);
            }
            write_log(log);
        }
        return;
}

void ConsoleLogger::level_to_color(LOG_LEVEL level, std::string& log_msg) {
        std::string color_code;
        switch (level) {
            case GST::LOG::LOG_LEVEL::LEVEL_DEBUG:
                color_code = "\033[33m";
                break;
            case GST::LOG::LOG_LEVEL::LEVEL_ERROR:
                color_code = "\033[31m";
                break;
            case GST::LOG::LOG_LEVEL::LEVEL_FATAL:
                color_code = "\033[1;31m";
                break;
            case GST::LOG::LOG_LEVEL::LEVEL_INFO:
                color_code = "\033[34m";
                break;
            case GST::LOG::LOG_LEVEL::LEVEL_WARN:
                color_code = "\033[35m";
                break;
            default:
                color_code = "\033[0m";
                break;
        }
        log_msg = color_code + log_msg + "\033[0m";


}
}
}