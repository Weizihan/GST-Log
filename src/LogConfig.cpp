#include "LogConfig.h"

#include <ctime>

GST::LOG::LogExtraConfig::~LogExtraConfig() {
}

GST::LOG::LogConfig::~LogConfig() {
}

extern "C" std::time_t gst_log_wall_time() {
    return std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
}

extern "C" std::chrono::steady_clock::time_point gst_log_steady_time() {
    return std::chrono::steady_clock::now();
}
