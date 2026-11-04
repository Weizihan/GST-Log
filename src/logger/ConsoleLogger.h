#pragma once
#include <mutex>
#include "Logger.h"
namespace GST {
namespace LOG {
class ConsoleLogger : public Logger {
public:
    ~ConsoleLogger() override;
    bool init(const LogConfig& config) override;
    bool write_log(const buffer& log) override;
    bool flush() override;
    bool shutdown() override;
private:
    void decorate(LOG_LEVEL level, std::string& message) override;
    std::mutex _mutex;
    bool _is_colorful_log{false};
    bool _had_error{false};
};
}
}
