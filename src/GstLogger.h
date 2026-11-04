#pragma once
#include <cstdarg>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <type_traits>
#include "logger/Logger.h"
#include "LogConfig.h"

namespace GST {
namespace LOG {
class GstLogger {
public:
    ~GstLogger();
    static std::shared_ptr<GstLogger> get_Instance();
    bool init();
    bool init(const LogConfig& config);
    bool flush();
    bool shutdown();

    void log(int index, LOG_LEVEL level, const char* file, int line,
             const char* func, const char* format, ...);
    void log(const std::string& name, LOG_LEVEL level, const char* file, int line,
             const char* func, const char* format, ...);
    bool try_log(int index, LOG_LEVEL level, const char* file, int line,
                 const char* func, const char* format, ...);
    bool try_log(const std::string& name, LOG_LEVEL level, const char* file, int line,
                 const char* func, const char* format, ...);

    // 保留 std::string 格式参数调用，同时避免对引用参数使用 va_start。
    template<class Route, class Format, class... Args,
             std::enable_if_t<std::is_same<std::decay_t<Format>, std::string>::value, int> = 0>
    void log(const Route& route, LOG_LEVEL level, const char* file, int line,
             const char* func, Format&& format, Args... args) {
        log(route, level, file, line, func, format.c_str(), args...);
    }

    template<class Route, class Format, class... Args,
             std::enable_if_t<std::is_same<std::decay_t<Format>, std::string>::value, int> = 0>
    bool try_log(const Route& route, LOG_LEVEL level, const char* file, int line,
                 const char* func, Format&& format, Args... args) {
        return try_log(route, level, file, line, func, format.c_str(), args...);
    }

private:
    GstLogger();
    GstLogger(const GstLogger&) = delete;
    GstLogger& operator=(const GstLogger&) = delete;
    std::vector<Logger*> select(int index, const std::string* name);
    bool dispatch(const std::vector<Logger*>& loggers, LOG_LEVEL level,
                  const char* file, int line, const char* func,
                  const char* format, va_list args, bool diagnose);
    std::vector<std::unique_ptr<Logger>> _Logger_ptrs;
    std::mutex _loggers_mutex;
    std::mutex _shutdown_mutex;
    bool _shutdown_started{false};
};
}
}
