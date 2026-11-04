#include "AsyncFileLogger.h"
namespace GST {
namespace LOG {
AsyncFileLogger::~AsyncFileLogger() { shutdown(); }

bool AsyncFileLogger::init(const LogConfig& config) {
    std::lock_guard<std::mutex> shutdown_lock(_shutdown_mutex);
    if (config._buffer_size <= 0 || config._max_pending_logs == 0 ||
        config._max_pending_bytes == 0 ||
        static_cast<std::size_t>(config._buffer_size) > config._max_pending_bytes) {
        std::fputs("GST_log: invalid async buffer/queue limits\n", stderr);
        return false;
    }
    if (!configure(config)) {
        return false;
    }
    _buffer_size = static_cast<std::size_t>(config._buffer_size);
    _max_pending_logs = config._max_pending_logs;
    _max_pending_bytes = config._max_pending_bytes;
    _current_buffer.reserve(_buffer_size);
    if (!_file.open(config)) {
        return false;
    }
    _target = _file.path().string();
    {
        std::lock_guard<std::mutex> lock(_buffer_mutex);
        _worker_stopped = false;
        _begin = true; // Publish the running state before the worker can execute.
    }
    try {
        _write_worker = std::thread(&AsyncFileLogger::thread_func, this);
    } catch (const std::exception& error) {
        std::lock_guard<std::mutex> lock(_buffer_mutex);
        _begin = false;
        _worker_stopped = true;
        _file.close();
        std::fprintf(stderr, "GST_log: start worker: %s\n", error.what());
        return false;
    }
    _initialized = true;
    return true;
}

void AsyncFileLogger::publish_current_locked() {
    if (_current_records == 0) {
        return;
    }
    _buffers.emplace_back();
    _buffers.back().data.swap(_current_buffer);
    _buffers.back().records = _current_records;
    _current_records = 0;
}

bool AsyncFileLogger::write_log(const buffer& log) {
    std::lock_guard<std::mutex> lock(_buffer_mutex);
    if (log.empty() || !_begin || _pending_records >= _max_pending_logs ||
        log.size() > _max_pending_bytes ||
        _accepted_bytes - _completed_bytes > _max_pending_bytes - log.size()) {
        return false;
    }
    if (!_current_buffer.empty() && (_current_buffer.size() >= _buffer_size ||
        log.size() > _buffer_size - _current_buffer.size())) {
        publish_current_locked();
        _cv.notify_one();
    }
    _current_buffer.append(log);
    ++_current_records;
    ++_pending_records;
    _accepted_bytes += log.size();
    _cv.notify_one();
    return true;
}

void AsyncFileLogger::thread_func() noexcept {
    try {
        std::vector<Batch> ready;
        for (;;) {
            bool stop;
            {
                std::unique_lock<std::mutex> lock(_buffer_mutex);
                _cv.wait(lock, [this] { return !_begin || !_buffers.empty() || _current_records != 0; });
                publish_current_locked();
                ready.swap(_buffers);
                stop = !_begin;
            }
            for (const auto& batch : ready) {
                bool success = false;
                try {
                    success = _file.write(batch.data);
                } catch (const std::exception& error) {
                    std::fprintf(stderr, "GST_log: async write: %s\n", error.what());
                }
                {
                    std::lock_guard<std::mutex> lock(_buffer_mutex);
                    _write_failed = _write_failed || !success;
                    _completed_bytes += batch.data.size();
                    _pending_records -= batch.records;
                }
                if (!success) {
                    std::fprintf(stderr, "GST_log: async logger '%s' failed to write %zu accepted records\n",
                                 _name.c_str(), batch.records);
                }
                _flush_cv.notify_all();
            }
            ready.clear();
            if (stop) {
                break;
            }
        }
    } catch (const std::exception& error) {
        std::fprintf(stderr, "GST_log: async worker stopped unexpectedly: %s\n", error.what());
        std::lock_guard<std::mutex> lock(_buffer_mutex);
        _write_failed = true;
        _begin = false;
    } catch (...) {
        std::fputs("GST_log: async worker stopped unexpectedly\n", stderr);
        std::lock_guard<std::mutex> lock(_buffer_mutex);
        _write_failed = true;
        _begin = false;
    }
    {
        std::lock_guard<std::mutex> lock(_buffer_mutex);
        _worker_stopped = true;
    }
    _flush_cv.notify_all();
}

bool AsyncFileLogger::flush() {
    std::unique_lock<std::mutex> lock(_buffer_mutex);
    const auto target = _accepted_bytes;
    _cv.notify_one();
    _flush_cv.wait(lock, [this, target] { return _completed_bytes >= target || _worker_stopped; });
    return _completed_bytes >= target && !_write_failed;
}

bool AsyncFileLogger::shutdown() {
    std::lock_guard<std::mutex> shutdown_lock(_shutdown_mutex);
    {
        std::lock_guard<std::mutex> lock(_buffer_mutex);
        _begin = false; // Same mutex as the wait predicate: no lost stop notification.
    }
    _cv.notify_all();
    if (_write_worker.joinable()) {
        _write_worker.join();
    }
    const bool closed = _file.close();
    std::lock_guard<std::mutex> lock(_buffer_mutex);
    if (_initialized && !closed) {
        _write_failed = true;
    }
    return !_initialized || (!_write_failed && _completed_bytes == _accepted_bytes);
}
}
}
