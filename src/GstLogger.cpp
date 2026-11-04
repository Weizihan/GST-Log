#include "GstLogger.h"
#include "logger/AsyncFileLogger.h"
#include "logger/FileLogger.h"
#include "logger/ConsoleLogger.h"
#include "logger/LogFile.h"
#include <algorithm>
#include <cctype>
#include <cstdio>

using namespace GST::LOG;

GstLogger::GstLogger() {
}

GstLogger::~GstLogger() {
    shutdown();
}

std::shared_ptr<GstLogger> GstLogger::get_Instance() {
    static std::shared_ptr<GstLogger> instance(new GstLogger());
    return instance;
}

bool GstLogger::init(const LogConfig& config) {
    std::string type = config._log_type;
    std::transform(type.begin(), type.end(), type.begin(), [](unsigned char c) {
        return std::tolower(c);
    });
    std::unique_ptr<Logger> logger;
    if (type == "file") {
        logger = std::make_unique<FileLogger>();
    }
    else if (type == "async") {
        logger = std::make_unique<AsyncFileLogger>();
    }
    else if (type == "console") {
        logger = std::make_unique<ConsoleLogger>();
    }
    else {
        std::fputs("GST_log: unknown logger type\n", stderr);
        return false;
    }
    // 配置属于启动阶段操作，必须在产生任何文件系统副作用之前检查实例身份。
    std::lock_guard<std::mutex> lock(_loggers_mutex);
    if (_shutdown_started || config._logger_name.empty()) {
        std::fputs("GST_log: logger is not initialized\n", stderr);
        return false;
    }
    for (const auto& existing : _Logger_ptrs) {
        if (existing->get_name() == config._logger_name) {
            std::fputs("GST_log: duplicate logger name\n", stderr);
            return false;
        }
    }
    try {
        if (type != "console") {
            std::filesystem::path path;
            if (!detail::resolve_log_path(config, path)) {
                return false;
            }
            for (const auto& existing : _Logger_ptrs) {
                if (existing->get_target() == path.string()) {
                    std::fputs("GST_log: one backend must own each log path\n", stderr);
                    return false;
                }
            }
        }
        if (!logger->init(config)) {
            return false;
        }
        _Logger_ptrs.push_back(std::move(logger));
        return true;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "GST_log: initialization failed: %s\n", error.what());
        return false;
    }
}

bool GstLogger::init() {
    {
        std::lock_guard<std::mutex> lock(_loggers_mutex);
        if (!_Logger_ptrs.empty() || _shutdown_started) {
            return false;
        }
    }
    LogConfig config;
    config._logger_name = "default";
    config._log_type = "console";
    config._log_level = LOG_LEVEL::LEVEL_DEBUG;
    config._is_colorful_log = true;
    return init(config);
}

std::vector<Logger*> GstLogger::select(int index, const std::string* name) {
    std::vector<Logger*> result;
    std::lock_guard<std::mutex> lock(_loggers_mutex);
    if (_shutdown_started) {
        return result;
    }
    if (_Logger_ptrs.empty()) {
        return result;
    }
    if (name) {
        for (auto& logger : _Logger_ptrs) {
            if (logger->get_name() == *name) {
                result.push_back(logger.get());
                return result;
            }
        }
        std::fputs("GST_log: unknown logger name\n", stderr);
    } else if (index == -1) {
        for (auto& logger : _Logger_ptrs) {
            result.push_back(logger.get());
        }
    } else if (index >= 0 && static_cast<std::size_t>(index) < _Logger_ptrs.size()) {
        result.push_back(_Logger_ptrs[static_cast<std::size_t>(index)].get());
    } else {
        std::fputs("GST_log: invalid logger index\n", stderr);
    }
    return result;
}

bool GstLogger::dispatch(const std::vector<Logger*>& loggers, LOG_LEVEL level,
                         const char* file, int line, const char* func,
                         const char* format, va_list args, bool diagnose) {
    if (!format || loggers.empty()) {
        return false;
    }
    bool eligible = false;
    for (auto* logger : loggers) {
        eligible = eligible || logger->enabled(level);
    }
    if (!eligible) {
        return false;
    }
    try {
        va_list copy;
        va_copy(copy, args);
        const int size = std::vsnprintf(nullptr, 0, format, copy);
        va_end(copy);
        if (size < 0) {
            std::fputs("GST_log: message formatting failed\n", stderr);
            return false;
        }
        std::string message(static_cast<std::size_t>(size) + 1, '\0');
        const int written = std::vsnprintf(message.data(), message.size(), format, args);
        if (written != size) {
            std::fputs("GST_log: message formatting changed between passes\n", stderr);
            return false;
        }
        message.resize(static_cast<std::size_t>(size));
        bool success = true;
        bool accepted = false;
        for (auto* logger : loggers) {
            if (!logger->enabled(level)) {
                continue;
            }
            const bool result = logger->try_log(level, message, file, line, func);
            accepted = accepted || result;
            success = success && result;
            if (!result && diagnose) {
                std::fprintf(stderr, "GST_log: logger '%s' rejected a message\n", logger->get_name().c_str());
            }
        }
        return accepted && success;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "GST_log: message failed: %s\n", error.what());
        return false;
    }
}

void GstLogger::log(int index, LOG_LEVEL level, const char* file, int line,
                    const char* func, const char* format, ...) {
    const auto loggers = select(index, nullptr);
    va_list args;
    va_start(args, format);
    dispatch(loggers, level, file, line, func, format, args, true);
    va_end(args);
}

void GstLogger::log(const std::string& name, LOG_LEVEL level, const char* file, int line,
                    const char* func, const char* format, ...) {
    const auto loggers = select(0, &name);
    va_list args;
    va_start(args, format);
    dispatch(loggers, level, file, line, func, format, args, true);
    va_end(args);
}

bool GstLogger::try_log(int index, LOG_LEVEL level, const char* file, int line,
                        const char* func, const char* format, ...) {
    const auto loggers = select(index, nullptr);
    va_list args;
    va_start(args, format);
    const bool result = dispatch(loggers, level, file, line, func, format, args, false);
    va_end(args);
    return result;
}

bool GstLogger::try_log(const std::string& name, LOG_LEVEL level, const char* file, int line,
                        const char* func, const char* format, ...) {
    const auto loggers = select(0, &name);
    va_list args;
    va_start(args, format);
    const bool result = dispatch(loggers, level, file, line, func, format, args, false);
    va_end(args);
    return result;
}

bool GstLogger::flush() {
    std::vector<Logger*> loggers;
    {
        std::lock_guard<std::mutex> lock(_loggers_mutex);
        for (auto& logger : _Logger_ptrs) {
            loggers.push_back(logger.get());
        }
    }
    bool success = true;
    for (auto* logger : loggers) {
        success = logger->flush() && success;
    }
    return success;
}

bool GstLogger::shutdown() {
    std::lock_guard<std::mutex> shutdown_lock(_shutdown_mutex);
    std::vector<Logger*> loggers;
    {
        std::lock_guard<std::mutex> lock(_loggers_mutex);
        _shutdown_started = true;
        for (auto& logger : _Logger_ptrs) {
            loggers.push_back(logger.get());
        }
    }
    bool success = true;
    for (auto* logger : loggers) {
        success = logger->shutdown() && success;
    }
    return success;
}
