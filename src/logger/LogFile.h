#pragma once
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <string>
#include "LogConfig.h"

// 内部时钟接缝由测试在链接时替换，无需运行期回调。
extern "C" std::time_t gst_log_wall_time();
extern "C" std::chrono::steady_clock::time_point gst_log_steady_time();

namespace GST {
namespace LOG {
namespace detail {
bool resolve_log_path(const LogConfig& config, std::filesystem::path& path);

// 每个文件只归一个后端所有，调用方负责串行访问。
class LogFile {
public:
    ~LogFile();
    LogFile();
    LogFile(const LogFile&) = delete;
    LogFile& operator=(const LogFile&) = delete;
    bool open(const LogConfig& config);
    bool write(const std::string& message);
    bool flush() const;

    bool close();
    const std::filesystem::path& path() const;

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
