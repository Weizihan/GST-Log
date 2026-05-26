#pragma once

#include <thread>
#include <condition_variable>
#include <filesystem>
#include <fstream>

#include "Logger.h"
namespace GST {

namespace LOG {

class AsyncFileLogger : public Logger{
public:
    AsyncFileLogger();
    ~AsyncFileLogger() override;
    bool init(const LogConfig& config) override;

    bool write_log(const buffer& log) override;
    bool trunc_log() override;
private:
    void thread_func();
    bool file_trunc();   // 在 worker 线程内执行实际的轮转检查
    bool rotate_file();  // 轮转：rename + 重开文件

    std::filesystem::path _log_path;
    std::ofstream _log_stream;
    std::thread _write_worker;
    std::condition_variable _cv;
    std::mutex _buffer_mutex;
    std::mutex _trunc_mutex;
    buffervecptr _buffers;
    bufferptr _current_buffer;
    bufferptr _next_buffer;
    std::string _current_date;  // for TRUNC_TYPE_SYS_TIME
};

}
}