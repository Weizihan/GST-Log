#pragma once
#include <mutex>
#include "Logger.h"
#include "LogFile.h"
namespace GST {
namespace LOG {
class FileLogger : public Logger {
public:
    ~FileLogger() override;
    bool init(const LogConfig& config) override;
    bool write_log(const buffer& log) override;
    bool flush() override;
    bool shutdown() override;
private:
    std::mutex _mutex;
    detail::LogFile _file;
};
}
}
