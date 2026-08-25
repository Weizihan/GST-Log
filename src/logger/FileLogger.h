#pragma once

#include <filesystem>
#include <fstream>
#include <mutex>

#include "Logger.h"
namespace GST{
namespace LOG{

class FileLogger : public Logger
{
public:
    FileLogger() = default;
    ~FileLogger() override;

    bool write_log(const buffer& log) override;
    bool trunc_log() override;
    bool flush(std::chrono::milliseconds timeout) override;
    bool shutdown(std::chrono::milliseconds timeout) override;
    void log(LOG_LEVEL level, std::string& log, const char* file,
             int line, const char* func) override;

    bool init(const LogConfig& config) override;

private:
    bool write_log_unlocked(const buffer& log);
    bool trunc_log_unlocked();
    bool rotate_file();
    void report_degraded_unlocked() noexcept;
    void report_recovered_unlocked() noexcept;

    std::filesystem::path _log_path;
    std::ofstream _log_stream;
    std::string _current_date;  // for TRUNC_TYPE_SYS_TIME
    std::timed_mutex _mutex;
    std::mutex _shutdown_mutex;
    std::atomic<bool> _accepting{false};
    bool _shutdown_complete{false};
    bool _degraded{false};
};

}
}
