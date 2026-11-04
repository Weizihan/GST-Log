#include "Logger.h"
#include <cstdio>
#include <exception>

namespace GST {
namespace LOG {

Logger::~Logger() {
}

void Logger::log(LOG_LEVEL level, std::string& message, const char* file,
                 int line, const char* func) {
    if (enabled(level) && !try_log(level, message, file, line, func)) {
        std::fprintf(stderr, "GST_log: logger '%s' rejected a message\n", _name.c_str());
    }
}

bool Logger::try_log(LOG_LEVEL level, const std::string& message, const char* file,
                     int line, const char* func) {
    if (!enabled(level)) {
        return false;
    }
    try {
        std::string formatted = message;
        if (!_format.format(level, formatted, file, line, func)) {
            return false;
        }
        decorate(level, formatted);
        return write_log(formatted);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "GST_log: logger '%s': %s\n", _name.c_str(), error.what());
        return false;
    }
}

bool Logger::enabled(LOG_LEVEL level) const {
    return _begin && level >= _level && level <= LOG_LEVEL::LEVEL_FATAL;
}

const std::string& Logger::get_name() const {
    return _name;
}

const std::string& Logger::get_target() const {
    return _target;
}

void Logger::decorate(LOG_LEVEL, std::string&) {
}

bool Logger::configure(const LogConfig& config) {
    if (_initialized || config._logger_name.empty() ||
        config._log_level < LOG_LEVEL::LEVEL_DEBUG || config._log_level > LOG_LEVEL::LEVEL_FATAL ||
        config._log_max_size < 0 || config._rotation_interval.count() < 0 ||
        (config._natural_rotation != NaturalRotation::Legacy &&
         config._natural_rotation != NaturalRotation::None &&
         config._natural_rotation != NaturalRotation::Hourly &&
         config._natural_rotation != NaturalRotation::Daily) ||
        (config._log_encoding != "UTF-8" && config._log_encoding != "utf-8")) {
        std::fputs("GST_log: invalid configuration (only UTF-8 byte output is supported)\n", stderr);
        return false;
    }
    _name = config._logger_name;
    _level = config._log_level;
    _format = LogFormat(config._log_format);
    return true;
}

void Logger::start() {
    _initialized = true;
    _begin = true;
}

}
}
