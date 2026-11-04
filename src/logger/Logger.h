#pragma once
#include <atomic>
#include <cstdio>
#include <exception>
#include <memory>
#include <string>
#include <vector>
#include "LogConfig.h"
#include "Marco.h"

namespace GST {
namespace LOG {
using buffer = std::string;
using bufferptr = std::unique_ptr<buffer>;
using buffervecptr = std::unique_ptr<std::vector<buffer>>;

class Logger {
public:
    virtual ~Logger() = default;
    virtual bool init(const LogConfig& config) = 0;
    virtual bool write_log(const buffer& log) = 0;
    virtual bool flush() = 0;
    virtual bool shutdown() = 0;

    virtual void log(LOG_LEVEL level, std::string& message, const char* file,
                     int line, const char* func) final {
        if (enabled(level) && !try_log(level, message, file, line, func)) {
            std::fprintf(stderr, "GST_log: logger '%s' rejected a message\n", _name.c_str());
        }
    }

    bool try_log(LOG_LEVEL level, const std::string& message, const char* file,
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

    bool enabled(LOG_LEVEL level) const {
        return _begin && level >= _level && level <= LOG_LEVEL::LEVEL_FATAL;
    }

    inline const std::string& get_name() const {
        return _name;
    }

    inline const std::string& get_target() const {
        return _target;
    }

protected:
    virtual void decorate(LOG_LEVEL, std::string&) {}

    bool configure(const LogConfig& config) {
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

    void start() { _initialized = true; _begin = true; }

    std::string _name;
    LOG_LEVEL _level{LOG_LEVEL::LEVEL_INFO};
    LogFormat _format;
    std::string _target;
    bool _initialized{false};
    std::atomic<bool> _begin{false};
};
}
}
