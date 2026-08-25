#pragma once

#include <chrono>
#include <string>
#include <memory>
#include <vector>
#include <mutex>

#include "logger/Logger.h"
#include "LogConfig.h"

namespace GST{
namespace LOG{
    
class GstLogger {
public:
    ~GstLogger() = default;

    static std::shared_ptr<GST::LOG::GstLogger> get_Instance();

    bool init();
    bool init(const LogConfig& config);

    // Waits until every log accepted before the synchronization boundary has
    // been submitted to the operating system. Logs may continue concurrently.
    // This is not an fsync-style power-loss durability guarantee.
    bool flush(std::chrono::milliseconds timeout);

    // Stops every configured backend from accepting new logs and drains all
    // logs accepted before that boundary. On timeout, shutdown can be retried;
    // stopped backends remain stopped and timed-out backends remain Stopping.
    bool shutdown(std::chrono::milliseconds timeout);

    void log(int index, GST::LOG::LOG_LEVEL level, const char* file,
                    int line, const char* func, const std::string& format, ...);
    void log(const std::string& name, GST::LOG::LOG_LEVEL level, const char* file,
                    int line, const char* func, const std::string& format, ...);
private:
    GstLogger();
    GstLogger(const GstLogger&) = delete;
    GstLogger(const GstLogger&&) = delete;
    GstLogger& operator=(const GstLogger&) = delete;
    int get_logger_index_by_name(const std::string& name);
    std::vector<std::unique_ptr<GST::LOG::Logger>> _Logger_ptrs;
    mutable std::mutex _loggers_mutex;
    bool _is_start;
    bool _shutdown_started{false};

};

}
}
