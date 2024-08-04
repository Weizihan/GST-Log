#include "ConsoleLogger.h"

namespace GST {
namespace LOG {

bool ConsoleLogger::init(const GST::LOG::LogConfig& config) {
    _log_level = config._log_level;
    _log_format = LogFormat(config._log_format);
    _logger_name = config._logger_name;
    _log_type = config._log_type;

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
        if(level > _log_level) {
            return;
        }
        if(_log_format.format(level, log, file, line, func)) {
            if(_is_colorful_log) {
                level_to_color(level, log);
            }
            write_log(log);
        }
        return;
}

void ConsoleLogger::level_to_color(LOG_LEVEL level, std::string& log) {
        std::string color_code;
        switch (level) {
            case GST::LOG::LOG_LEVEL::LEVEL_DEBUG:
                color_code = "\033[33m"; // 黄色
                break;
            case GST::LOG::LOG_LEVEL::LEVEL_ERROR:
                color_code = "\033[31m"; // 红色
                break;
            case GST::LOG::LOG_LEVEL::LEVEL_FATAL:
                color_code = "\033[1;31m"; // 粗体红色
                break;
            case GST::LOG::LOG_LEVEL::LEVEL_INFO:
                color_code = "\033[34m"; // 蓝色
                break;
            case GST::LOG::LOG_LEVEL::LEVEL_WARN:
                color_code = "\033[35m"; // 紫色
                break;
            default:
                color_code = "\033[0m"; // 默认颜色（无颜色）
                break;
        }
        log = color_code + log + "\033[0m"; // 恢复默认颜色
    }


}
}