#include "FileLogger.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
namespace GST {
namespace LOG {

bool FileLogger::init(const LogConfig& config) {
    std::cout << "init" << std::endl;
    _fd_file = open(config._log_target.c_str(),  O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if(_fd_file < 0) {
        std::cerr << "open file file path fialed " << std::endl;
        return false;
    }
    _level = config._log_level;
    _format = LogFormat(config._log_format);
    _name = config._logger_name;
    _begin = true;
    return true;
}

bool FileLogger::write_log(const buffer& log) {
    int ret = write(_fd_file,log.c_str(),log.size());
    if(ret != 0) {
        return false;
    }
    return true;
}


}
}