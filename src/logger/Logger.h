#pragma once
#include <atomic>
#include <memory>
#include <string>
#include <vector>
#include "LogConfig.h"
#include "Marco.h"

namespace GST {
namespace LOG {
using buffer = std::string;
using bufferptr = std::unique_ptr<buffer>;
using buffervecptr = std::unique_ptr<std::vector<buffer>>;

class Logger {
public:
    virtual ~Logger();
    virtual bool init(const LogConfig& config) = 0;
    virtual bool write_log(const buffer& log) = 0;
    virtual bool flush() = 0;
    virtual bool shutdown() = 0;

    virtual void log(LOG_LEVEL level, std::string& message, const char* file,
                     int line, const char* func) final;

    bool try_log(LOG_LEVEL level, const std::string& message, const char* file,
                 int line, const char* func);

    bool enabled(LOG_LEVEL level) const;

    const std::string& get_name() const;

    const std::string& get_target() const;

protected:
    virtual void decorate(LOG_LEVEL level, std::string& message);

    bool configure(const LogConfig& config);

    void start();

    std::string _name;
    LOG_LEVEL _level{LOG_LEVEL::LEVEL_INFO};
    LogFormat _format;
    std::string _target;
    bool _initialized{false};
    std::atomic<bool> _begin{false};
};
}
}
