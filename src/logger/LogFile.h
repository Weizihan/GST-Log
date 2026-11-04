#pragma once
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <string>
#include "LogConfig.h"

// Internal clock seams; tests replace them at link time, without runtime callbacks.
extern "C" std::time_t gst_log_wall_time();
extern "C" std::chrono::steady_clock::time_point gst_log_steady_time();

namespace GST {
namespace LOG {
namespace detail {
bool resolve_log_path(const LogConfig& config, std::filesystem::path& path);

// One backend owns this file. The caller serializes access.
class LogFile {
public:
    ~LogFile();
    LogFile() = default;
    LogFile(const LogFile&) = delete;
    LogFile& operator=(const LogFile&) = delete;
    bool open(const LogConfig& config);
    bool write(const std::string& message);
    bool flush() const { return !_had_error; } // write(2) submits bytes to the OS.

    bool close();
    const std::filesystem::path& path() const { return _path; }

private:
    bool reopen();
    bool rotate(std::time_t now);
    std::string period(std::time_t time) const;
    bool error(const char* operation, int code);
    int _fd{-1};
    std::filesystem::path _path;
    std::uintmax_t _max_size{0};
    NaturalRotation _natural{NaturalRotation::None};
    std::string _period;
    std::chrono::seconds _interval{0};
    std::chrono::steady_clock::time_point _interval_start{};
    bool _had_error{false};
};
}
}
}
