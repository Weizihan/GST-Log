#pragma once

#include "Logger.h"
namespace GST{
namespace LOG{

class FileLogger : public Logger
{
public:
    FileLogger() = default;
    ~FileLogger() override {};

    bool write_log(const buffer& log);

    bool init(const LogConfig& config) override;

private:
    
    int _fd_file;
};

}
}