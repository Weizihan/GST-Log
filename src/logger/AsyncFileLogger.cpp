#include "AsyncFileLogger.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <iomanip>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "LogFormat.h"

namespace GST {
//logger

namespace LOG {

AsyncFileLogger::AsyncFileLogger(){
}

bool AsyncFileLogger::init(const GST::LOG::LogConfig& config) {

    _fd_file = open(config._log_target.c_str(),  O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if(_fd_file < 0) {
        std::cerr << "open file file path fialed " << std::endl;
        return false;
    }
    
    _current_buffer = std::make_unique<buffer>();
    _next_buffer = std::make_unique<buffer>();

    _buffers = std::make_unique<std::vector<buffer>>();
    _format = LogFormat(config._log_format);
    _level = config._log_level;
    _current_buffer->reserve(config._buffer_size * 64);
    _next_buffer->reserve(config._buffer_size * 64);
    _write_worker = std::thread(&AsyncFileLogger::thread_func, this);
    _begin = true;
    return true;
}

//专门用来管理双缓冲的线程
void AsyncFileLogger::thread_func(){
    int wrtie_size = 0;
    buffervecptr tmp_buffers = std::make_unique<std::vector<buffer>>();;
    tmp_buffers->reserve(1024*64);
    while (_begin)
    {
        {
            std::unique_lock<std::mutex> _lock(_buffer_mutex);
            if(_current_buffer->empty()){
                _cv.wait_for(_lock,std::chrono::seconds(4));//固定时间交换
            }
            tmp_buffers.swap(_buffers);
        }

        //write buffer to file
        buffer write_to_file;
        for(const auto& it : *tmp_buffers) {
            if(!it.empty()) {
                write_to_file.append(it);
            }
        }

        int ret = write(_fd_file, write_to_file.c_str(), write_to_file.size());
        if(ret == -1) {
            std::cerr << "write into file failed" << std::endl;
        }
        std::vector<buffer>().swap(*tmp_buffers);
    }

    //写入所有的数据
    if(!_current_buffer->empty()) {
        int ret = write(_fd_file, _current_buffer->c_str(), _current_buffer->size());
        if(ret == -1) {
            std::cerr << "write into file failed" << std::endl;
        }
    }

}

bool AsyncFileLogger::file_trunc() {
    return false;
}

bool AsyncFileLogger::write_log(const buffer& log) {
    std::unique_lock<std::mutex> lock(_buffer_mutex);

    // no enough buffer to wirte
    if(_current_buffer->capacity() - _current_buffer->size() < log.size() ) {
        _buffers->emplace_back(std::move(*_current_buffer));
        _current_buffer = std::move(_next_buffer);
        _next_buffer = std::make_unique<buffer>();
        _next_buffer.get()->reserve(64*1024);
    }
    _current_buffer.get()->append(log);
    _cv.notify_one();

    return true;

}


}//namespace LOG
}//namespace GST