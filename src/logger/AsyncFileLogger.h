#pragma once

#include <thread>
#include <condition_variable>

#include "Logger.h"
namespace GST {

namespace LOG {

class AsyncFileLogger : public Logger{
public:
    AsyncFileLogger();
    ~AsyncFileLogger() override{
        _begin = false;
        _cv.notify_all();
        if (_write_worker.joinable()) {
            _write_worker.join();
        }
    };
    bool init(const LogConfig& config) override;

    bool write_log(const buffer& log) override;
private:
    void thread_func();

    bool file_trunc();
    
    int _fd_file;

    std::thread _write_worker;
    std::condition_variable _cv;
    std::mutex _buffer_mutex;
    std::mutex _trunc_mutex;
    buffervecptr _buffers;
    bufferptr _current_buffer;
    bufferptr _next_buffer;
};

}
}