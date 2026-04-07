#pragma once

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

    void log(int index, GST::LOG::LOG_LEVEL level, const char* file,
                    int line, const char* func, const std::string& format, ...);
    void log(const std::string& name, GST::LOG::LOG_LEVEL level, const char* file,
                    int line, const char* func, const std::string& format, ...);
    bool add_logger();
    bool delete_logger();
private:
    GstLogger();
    GstLogger(const GstLogger&) = delete;
    GstLogger(const GstLogger&&) = delete;
    GstLogger& operator=(const GstLogger&) = delete;
    int get_logger_index_by_name(const std::string& name);
    std::vector<std::unique_ptr<GST::LOG::Logger>> _Logger_ptrs;
    mutable std::mutex _loggers_mutex;
    bool _is_start;

};

}
}
