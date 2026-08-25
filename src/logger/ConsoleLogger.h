#pragma once
#include <mutex>

#include "Logger.h"

namespace GST {
namespace LOG {

class ConsoleLogger: public Logger {
public:
    ConsoleLogger() = default;
    ~ConsoleLogger() override;

    bool init(const GST::LOG::LogConfig& config) override; 

    bool write_log(const buffer& log) override;
    bool trunc_log() override { return true; }
    bool flush(std::chrono::milliseconds timeout) override;
    bool shutdown(std::chrono::milliseconds timeout) override;

    virtual void log(LOG_LEVEL level, std::string& log, const char* file,
                int line, const char* func) override;

private:
    bool write_log_unlocked(const buffer& log);
    std::timed_mutex _mutex;
    std::mutex _shutdown_mutex;
    std::atomic<bool> _accepting{false};
    bool _shutdown_complete{false};
    bool _is_colorful_log; 

    void level_to_color(LOG_LEVEL level, std::string& log_msg);
};

}
}
