#include "FileLogger.h"
namespace GST {
namespace LOG {
FileLogger::~FileLogger() {
    shutdown();
}

bool FileLogger::init(const LogConfig& config) {
    std::lock_guard<std::mutex> lock(_mutex);
    if (!configure(config) || !_file.open(config)) {
        return false;
    }
    _target = _file.path().string();
    start();
    return true;
}

bool FileLogger::write_log(const buffer& log) {
    std::lock_guard<std::mutex> lock(_mutex);
    return _begin && _file.write(log);
}

bool FileLogger::flush() {
    std::lock_guard<std::mutex> lock(_mutex);
    return _file.flush();
}

bool FileLogger::shutdown() {
    std::lock_guard<std::mutex> lock(_mutex);
    _begin = false;
    const bool closed = _file.close();
    return !_initialized || closed;
}
}
}
