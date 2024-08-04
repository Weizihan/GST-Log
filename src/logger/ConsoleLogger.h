#pragma once
#include "Logger.h"

namespace GST {
namespace LOG {

class ConsoleLogger: public Logger {
public:
    ConsoleLogger() = default;
    ~ConsoleLogger() override {
        _begin = false;
    } ;

    bool init(const GST::LOG::LogConfig& config) override; 

    bool write_log(const buffer& log) override;

    virtual void log(LOG_LEVEL level, std::string& log, const char* file,
                int line, const char* func) override;

private:
    bool _is_colorful_log; 

    void level_to_color(LOG_LEVEL level, std::string& log_msg);
};

}
}