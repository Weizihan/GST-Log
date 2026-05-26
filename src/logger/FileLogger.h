#pragma once

#include <filesystem>
#include <fstream>

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

    bool init(const LogConfig& config) override;

private:
    bool rotate_file();

    std::filesystem::path _log_path;
    std::ofstream _log_stream;
    std::string _current_date;  // for TRUNC_TYPE_SYS_TIME
};

}
}