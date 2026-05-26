#include "AsyncFileLogger.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <filesystem>

#include "LogFormat.h"

namespace GST {
//logger

namespace LOG {

namespace {

std::string async_current_date_str() {
    auto now = std::chrono::system_clock::now();
    auto tt  = std::chrono::system_clock::to_time_t(now);
    struct tm tm_info{};
    localtime_r(&tt, &tm_info);
    char buf[9];
    strftime(buf, sizeof(buf), "%Y%m%d", &tm_info);
    return buf;
}

std::string async_current_datetime_str() {
    auto now = std::chrono::system_clock::now();
    auto tt  = std::chrono::system_clock::to_time_t(now);
    struct tm tm_info{};
    localtime_r(&tt, &tm_info);
    char buf[16];
    strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", &tm_info);
    return buf;
}

} // anonymous namespace

AsyncFileLogger::AsyncFileLogger(){
}

AsyncFileLogger::~AsyncFileLogger() {
    _begin = false;
    _cv.notify_all();
    if (_write_worker.joinable()) {
        _write_worker.join();
    }
    if (_log_stream.is_open()) {
        _log_stream.flush();
        _log_stream.close();
    }
}

bool AsyncFileLogger::init(const GST::LOG::LogConfig& config) {
    _log_path = config._log_target;
    std::error_code ec;
    const auto parent = _log_path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, ec);
    }

    _log_stream.open(_log_path, std::ios::out | std::ios::trunc | std::ios::binary);
    if(!_log_stream.is_open()) {
        std::cerr << "open file path failed: " << _log_path << std::endl;
        return false;
    }
    
    _current_buffer = std::make_unique<buffer>();
    _next_buffer = std::make_unique<buffer>();

    _buffers = std::make_unique<std::vector<buffer>>();
    _format = LogFormat(config._log_format);
    _level  = config._log_level;
    _name   = config._logger_name;
    _current_buffer->reserve(config._buffer_size);
    _next_buffer->reserve(config._buffer_size);

    if (config._trunback_type) {
        _trunc_type = TRUNC_TYPE_SYS_TIME;
    } else if (config._log_max_size > 0) {
        _trunc_type      = TRUNC_TYPE_FILE_SZIE;
        _trunc_threshold = config._log_max_size;
    } else {
        _trunc_type = TRUNC_TYPE_NONE;
    }
    _current_date = async_current_date_str();

    _begin = true;
    _write_worker = std::thread(&AsyncFileLogger::thread_func, this);
    return true;
}

//专门用来管理双缓冲的线程
void AsyncFileLogger::thread_func(){
    buffervecptr tmp_buffers = std::make_unique<std::vector<buffer>>();
    tmp_buffers->reserve(64);
    while (_begin) {
        {
            std::unique_lock<std::mutex> lock(_buffer_mutex);
            if (_current_buffer->empty()) {
                _cv.wait_for(lock, std::chrono::seconds(1), [this] {
                    return !_begin || !_current_buffer->empty();
                });
            }
            if (!_current_buffer->empty()) {
                _buffers->emplace_back(std::move(*_current_buffer));
                if (_next_buffer == nullptr) {
                    _next_buffer = std::make_unique<buffer>();
                    _next_buffer->reserve(64 * 1024);
                }
                _current_buffer = std::move(_next_buffer);
                _next_buffer = std::make_unique<buffer>();
                _next_buffer->reserve(64 * 1024);
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

        if (write_to_file.empty()) {
            continue;
        }

        _log_stream.write(write_to_file.data(), static_cast<std::streamsize>(write_to_file.size()));
        if(!_log_stream.good()) {
            std::cerr << "write into file failed" << std::endl;
        }
        _log_stream.flush();
        std::vector<buffer>().swap(*tmp_buffers);
    }

    //写入所有剩余的数据
    {
        std::unique_lock<std::mutex> lock(_buffer_mutex);
        if (!_current_buffer->empty()) {
            _buffers->emplace_back(std::move(*_current_buffer));
            _current_buffer = std::make_unique<buffer>();
        }
        tmp_buffers.swap(_buffers);
    }
    for (const auto& it : *tmp_buffers) {
        if (!it.empty()) {
            _log_stream.write(it.data(), static_cast<std::streamsize>(it.size()));
        }
    }
    if (!_log_stream.good()) {
        std::cerr << "write into file failed" << std::endl;
    }
    _log_stream.flush();
}

// 由调用线程调用，实际轮转在 worker 线程内完成，此处为 no-op
bool AsyncFileLogger::trunc_log() {
    return true;
}

// 在 worker 线程内执行，无需额外加锁
bool AsyncFileLogger::file_trunc() {
    // 文件被外部删除时重新创建
    if (!std::filesystem::exists(_log_path)) {
        _log_stream.close();
        std::error_code ec;
        const auto parent = _log_path.parent_path();
        if (!parent.empty()) {
            std::filesystem::create_directories(parent, ec);
        }
        _log_stream.open(_log_path, std::ios::out | std::ios::trunc | std::ios::binary);
        return _log_stream.is_open();
    }

    switch (_trunc_type) {
    case TRUNC_TYPE_NONE:
        return true;

    case TRUNC_TYPE_FILE_SZIE: {
        std::error_code ec;
        auto sz = std::filesystem::file_size(_log_path, ec);
        if (!ec && sz >= static_cast<uintmax_t>(_trunc_threshold)) {
            return rotate_file();
        }
        return true;
    }

    case TRUNC_TYPE_SYS_TIME: {
        std::string today = async_current_date_str();
        if (today != _current_date) {
            _current_date = today;
            return rotate_file();
        }
        return true;
    }
    }
    return true;
}

bool AsyncFileLogger::rotate_file() {
    _log_stream.flush();
    _log_stream.close();

    std::filesystem::path backup =
        _log_path.parent_path() /
        (_log_path.stem().string() + "_" + async_current_datetime_str() +
         _log_path.extension().string());

    std::error_code ec;
    std::filesystem::rename(_log_path, backup, ec);
    if (ec) {
        std::filesystem::copy_file(_log_path, backup,
            std::filesystem::copy_options::overwrite_existing, ec);
        if (!ec) {
            std::filesystem::remove(_log_path, ec);
        }
    }

    _log_stream.open(_log_path, std::ios::out | std::ios::trunc | std::ios::binary);
    if (!_log_stream.is_open()) {
        std::cerr << "rotate_file: failed to open new log file: " << _log_path << std::endl;
        return false;
    }
    return true;
}

bool AsyncFileLogger::write_log(const buffer& log) {
    std::unique_lock<std::mutex> lock(_buffer_mutex);

    // no enough buffer to wirte
    if(_current_buffer->capacity() - _current_buffer->size() < log.size() ) {
        _buffers->emplace_back(std::move(*_current_buffer));
        if (_next_buffer == nullptr) {
            _next_buffer = std::make_unique<buffer>();
            _next_buffer->reserve(64 * 1024);
        }
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