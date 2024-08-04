#pragma once

#include <string>
#include <memory>
#include <vector>

#include "logger/Logger.h"
#include "LogConfig.h"

namespace GST{
namespace LOG{
    
class GstLogger {
public:
    static std::shared_ptr<GST::LOG::GstLogger> get_Instance() {
        if(_instances == nullptr){
            _instances = std::shared_ptr<GstLogger>(new GstLogger(), Deleter());
        }
        return _instances;
    }

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
    ~GstLogger() = default;
    GstLogger(const GstLogger&) = delete;
    GstLogger(const GstLogger&&) = delete;
    GstLogger& operator=(const GstLogger&) = delete;
    class Deleter {
    public:
        void operator()(GstLogger* p) {
            delete p;
        }
    };

    int get_logger_index_by_name(const std::string& name);
    std::vector<std::unique_ptr<GST::LOG::Logger>> _Logger_ptrs;
    static std::shared_ptr<GST::LOG::GstLogger> _instances;
    bool _is_start;

};

}
}
