#include "AsyncFileLogger.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <iomanip>
#include <filesystem>
#include <limits>

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

constexpr auto kRecoveryRetryInterval = std::chrono::milliseconds(100);
constexpr auto kPartialFlushInterval = std::chrono::milliseconds(100);

bool next_async_rotation_path(
    const std::filesystem::path& log_path,
    std::filesystem::path& result) {
    const std::string timestamp = async_current_datetime_str();
    const std::string stem = log_path.stem().string() + "_" + timestamp;
    const std::string extension = log_path.extension().string();
    std::filesystem::path candidate =
        log_path.parent_path() / (stem + extension);

    std::error_code ec;
    for (std::size_t sequence = 1;; ++sequence) {
        const bool candidate_exists = std::filesystem::exists(candidate, ec);
        if (ec) {
            return false;
        }
        if (!candidate_exists) {
            result = std::move(candidate);
            return true;
        }
        candidate = log_path.parent_path() /
            (stem + "_" + std::to_string(sequence) + extension);
    }
}

void report_async_degraded(bool& degraded) noexcept {
    if (!degraded) {
        std::fputs(
            "GST_log: async file logger degraded; retrying in background\n",
            stderr);
    }
    degraded = true;
}

void report_async_recovered(bool& degraded) noexcept {
    if (degraded) {
        std::fputs("GST_log: async file logger recovered\n", stderr);
    }
    degraded = false;
}

} // anonymous namespace

AsyncFileLogger::AsyncFileLogger(){
}

AsyncFileLogger::~AsyncFileLogger() {
    stop_worker_best_effort();
}

bool AsyncFileLogger::init(const GST::LOG::LogConfig& config) {
    if (config._buffer_size <= 0) {
        std::cerr << "buffer size must be positive" << std::endl;
        return false;
    }
    _buffer_size = static_cast<std::size_t>(config._buffer_size);
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
    _replay_format = LogFormat(config._log_format);
    _level  = config._log_level;
    _name   = config._logger_name;
    _current_buffer->reserve(_buffer_size);
    _next_buffer->reserve(_buffer_size);

    if (config._trunback_type) {
        _trunc_type = TRUNC_TYPE_SYS_TIME;
    } else if (config._log_max_size > 0) {
        _trunc_type      = TRUNC_TYPE_FILE_SZIE;
        _trunc_threshold = config._log_max_size;
    } else {
        _trunc_type = TRUNC_TYPE_NONE;
    }
    _current_date = async_current_date_str();

    _accepted_bytes = 0;
    _flushed_bytes = 0;
    _next_batch_id = 0;
    _partial_flush_scheduled = false;
    _accepting = true;
    _worker_stopped = false;
    _shutdown_complete = false;
    _begin = true;
    _worker_running = true;
    try {
        _write_worker = std::thread(&AsyncFileLogger::thread_func, this);
    } catch (...) {
        _begin = false;
        _worker_running = false;
        _accepting = false;
        _worker_stopped = true;
        _log_stream.close();
        return false;
    }
    return true;
}

//专门用来管理双缓冲的线程
void AsyncFileLogger::thread_func() noexcept {
    bool degraded = false;
    while (_worker_running) {
        try {
            worker_loop(degraded);
            break;
        } catch (...) {
            // thread 入口的最后一道保护：任何异常都不能越过这里触发
            // std::terminate。
            report_async_degraded(degraded);
            try {
                std::this_thread::sleep_for(kRecoveryRetryInterval);
            } catch (...) {
                // 故障报告路径不能再次向线程入口传播异常。
            }
        }
    }

    {
        std::lock_guard<std::mutex> lock(_buffer_mutex);
        _worker_stopped = true;
    }
    _flush_cv.notify_all();
}

void AsyncFileLogger::worker_loop(bool& degraded) {
    buffervecptr tmp_buffers = std::make_unique<std::vector<buffer>>();
    tmp_buffers->reserve(64);
    PendingBatch pending;

    while (_worker_running) {
        try {
            if (pending.data.empty()) {
                {
                    std::unique_lock<std::mutex> lock(_buffer_mutex);
                    while (_worker_running && _buffers->empty()) {
                        if (!_partial_flush_scheduled) {
                            _cv.wait(lock, [this] {
                                return !_worker_running || !_buffers->empty() ||
                                       _partial_flush_scheduled;
                            });
                            continue;
                        }

                        const auto deadline = _partial_flush_deadline;
                        const bool signaled = _cv.wait_until(
                            lock, deadline, [this] {
                                return !_worker_running || !_buffers->empty();
                            });
                        if (!signaled && _partial_flush_scheduled &&
                            std::chrono::steady_clock::now() >=
                                _partial_flush_deadline) {
                            publish_current_locked();
                        }
                    }
                    tmp_buffers.swap(_buffers);
                }

                buffer reusable;
                bool has_reusable = false;
                for (auto& item : *tmp_buffers) {
                    pending.data.append(item);
                    if (!has_reusable) {
                        item.clear();
                        reusable = std::move(item);
                        has_reusable = true;
                    }
                }
                tmp_buffers->clear();
                if (has_reusable) {
                    recycle_buffer(std::move(reusable));
                }
                if (!pending.data.empty()) {
                    pending.batch_id = ++_next_batch_id;
                    pending.attempt = 1;
                    pending.replay_required = false;
                    pending.marker_attempted = false;
                }
            } else {
                // 后端故障期间保留当前失败批次，并限制重试频率。
                std::unique_lock<std::mutex> lock(_buffer_mutex);
                _cv.wait_for(lock, kRecoveryRetryInterval, [this] {
                    return !_worker_running;
                });
            }

            if (pending.data.empty()) {
                continue;
            }

            const auto pending_size =
                static_cast<std::uint64_t>(pending.data.size());
            const WriteResult result = write_pending(pending);
            if (result != WriteResult::Success) {
                if (result == WriteResult::OutcomeUnknown) {
                    mark_outcome_unknown(pending);
                }
                report_async_degraded(degraded);
                continue;
            }

            pending = PendingBatch{};
            {
                std::lock_guard<std::mutex> lock(_buffer_mutex);
                _flushed_bytes += pending_size;
            }
            _flush_cv.notify_all();
            report_async_recovered(degraded);
        } catch (...) {
            // 未预期异常也不能逃出 std::thread 入口。
            report_async_degraded(degraded);
            try {
                std::this_thread::sleep_for(kRecoveryRetryInterval);
            } catch (...) {
                // 保持本次异常已经被 worker 内部消化。
            }
        }
    }

    //写入所有剩余的数据
    try {
        {
            std::unique_lock<std::mutex> lock(_buffer_mutex);
            publish_current_locked();
            tmp_buffers.swap(_buffers);
        }

        for (const auto& item : *tmp_buffers) {
            pending.data.append(item);
        }
        if (!pending.data.empty() && pending.batch_id == 0) {
            pending.batch_id = ++_next_batch_id;
        }
        const auto pending_size =
            static_cast<std::uint64_t>(pending.data.size());
        const WriteResult result = pending.data.empty()
            ? WriteResult::Success
            : write_pending(pending);
        if (!pending.data.empty() && result == WriteResult::Success) {
            {
                std::lock_guard<std::mutex> lock(_buffer_mutex);
                _flushed_bytes += pending_size;
            }
            _flush_cv.notify_all();
            report_async_recovered(degraded);
        } else if (!pending.data.empty()) {
            if (result == WriteResult::OutcomeUnknown) {
                mark_outcome_unknown(pending);
            }
            report_async_degraded(degraded);
            report_discarded_pending(pending.data.size());
        }
    } catch (...) {
        report_async_degraded(degraded);
    }
}

AsyncFileLogger::WriteResult AsyncFileLogger::write_pending(
    PendingBatch& pending) {
    if (!file_trunc()) {
        return WriteResult::NotWritten;
    }

    if (pending.replay_required && !pending.marker_attempted) {
        pending.marker_attempted = true;
        if (!write_replay_marker(pending)) {
            return WriteResult::NotWritten;
        }
    }

    _log_stream.write(
        pending.data.data(),
        static_cast<std::streamsize>(pending.data.size()));
    if (!_log_stream.good()) {
        return WriteResult::OutcomeUnknown;
    }
    _log_stream.flush();
    return _log_stream.good()
        ? WriteResult::Success
        : WriteResult::OutcomeUnknown;
}

bool AsyncFileLogger::write_replay_marker(PendingBatch& pending) noexcept {
    try {
        buffer marker =
            "[GST_log replay] batch_id=" + std::to_string(pending.batch_id) +
            " attempt=" + std::to_string(pending.attempt) +
            " previous-write-unconfirmed";
        if (!_replay_format.format(
                LOG_LEVEL::LEVEL_WARN, marker,
                __FILE__, __LINE__, __func__)) {
            std::fputs(
                "GST_log: replay marker formatting failed; replaying batch\n",
                stderr);
            return true;
        }
        marker.insert(marker.begin(), '\n');
        _log_stream.write(
            marker.data(), static_cast<std::streamsize>(marker.size()));
        if (_log_stream.good()) {
            _log_stream.flush();
        }
        if (_log_stream.good()) {
            return true;
        }
    } catch (...) {
        // Marker diagnostics must never replace or discard the user batch.
    }

    std::fprintf(
        stderr,
        "GST_log: replay marker could not be written; batch_id=%llu "
        "attempt=%llu; replaying original batch\n",
        static_cast<unsigned long long>(pending.batch_id),
        static_cast<unsigned long long>(pending.attempt));
    return false;
}

void AsyncFileLogger::mark_outcome_unknown(PendingBatch& pending) noexcept {
    pending.replay_required = true;
    pending.marker_attempted = false;
    if (pending.attempt != std::numeric_limits<std::uint64_t>::max()) {
        ++pending.attempt;
    }
}

void AsyncFileLogger::report_discarded_pending(std::size_t bytes) noexcept {
    std::fprintf(
        stderr,
        "GST_log: async file logger discarded %zu pending bytes during "
        "destruction\n",
        bytes);
}

bufferptr AsyncFileLogger::take_empty_buffer_locked() {
    if (_next_buffer != nullptr) {
        bufferptr result = std::move(_next_buffer);
        result->clear();
        return result;
    }
    auto result = std::make_unique<buffer>();
    result->reserve(_buffer_size);
    return result;
}

void AsyncFileLogger::publish_current_locked() {
    if (_current_buffer == nullptr || _current_buffer->empty()) {
        _partial_flush_scheduled = false;
        return;
    }
    _buffers->emplace_back(std::move(*_current_buffer));
    _current_buffer = take_empty_buffer_locked();
    _partial_flush_scheduled = false;
}

void AsyncFileLogger::recycle_buffer(buffer&& reusable) {
    reusable.clear();
    std::lock_guard<std::mutex> lock(_buffer_mutex);
    if (_next_buffer == nullptr) {
        _next_buffer = std::make_unique<buffer>(std::move(reusable));
        if (_next_buffer->capacity() < _buffer_size) {
            _next_buffer->reserve(_buffer_size);
        }
    }
}

bool AsyncFileLogger::flush(std::chrono::milliseconds timeout) {
    const auto non_negative_timeout =
        timeout < std::chrono::milliseconds::zero()
            ? std::chrono::milliseconds::zero()
            : timeout;
    return flush_until(std::chrono::steady_clock::now() + non_negative_timeout);
}

bool AsyncFileLogger::flush_until(
    std::chrono::steady_clock::time_point deadline) {
    std::unique_lock<std::mutex> lock(_buffer_mutex);
    if (_shutdown_complete) {
        return true;
    }
    const std::uint64_t target = _accepted_bytes;
    publish_current_locked();
    _cv.notify_one();
    return _flush_cv.wait_until(lock, deadline, [this, target] {
        return _flushed_bytes >= target || _worker_stopped;
    }) && _flushed_bytes >= target;
}

bool AsyncFileLogger::shutdown(std::chrono::milliseconds timeout) {
    std::lock_guard<std::mutex> shutdown_lock(_shutdown_mutex);

    const auto non_negative_timeout =
        timeout < std::chrono::milliseconds::zero()
            ? std::chrono::milliseconds::zero()
            : timeout;
    const auto deadline = std::chrono::steady_clock::now() + non_negative_timeout;
    {
        std::lock_guard<std::mutex> lock(_buffer_mutex);
        if (_shutdown_complete) {
            return true;
        }
        _accepting = false;
        _begin = false;
    }
    if (!flush_until(deadline)) {
        return false;
    }

    _worker_running = false;
    _cv.notify_all();
    if (_write_worker.joinable()) {
        _write_worker.join();
    }
    if (_log_stream.is_open()) {
        _log_stream.flush();
        _log_stream.close();
    }
    {
        std::lock_guard<std::mutex> lock(_buffer_mutex);
        _shutdown_complete = true;
    }
    return true;
}

void AsyncFileLogger::stop_worker_best_effort() noexcept {
    std::lock_guard<std::mutex> shutdown_lock(_shutdown_mutex);
    {
        std::lock_guard<std::mutex> lock(_buffer_mutex);
        _accepting = false;
        _begin = false;
    }
    _worker_running = false;
    _cv.notify_all();
    if (_write_worker.joinable()) {
        _write_worker.join();
    }
    if (_log_stream.is_open()) {
        _log_stream.flush();
        _log_stream.close();
    }
    {
        std::lock_guard<std::mutex> lock(_buffer_mutex);
        _shutdown_complete = true;
    }
}

// 由调用线程调用，实际轮转在 worker 线程内完成，此处为 no-op
bool AsyncFileLogger::trunc_log() {
    return true;
}

// 在 worker 线程内执行，无需额外加锁
bool AsyncFileLogger::file_trunc() {
    // 文件被外部删除时重新创建
    std::error_code ec;
    const bool log_exists = std::filesystem::exists(_log_path, ec);
    if (ec) {
        return false;
    }

    if (!log_exists || !_log_stream.is_open() || !_log_stream.good()) {
        _log_stream.close();
        _log_stream.clear();
        const auto parent = _log_path.parent_path();
        if (!parent.empty()) {
            std::filesystem::create_directories(parent, ec);
            if (ec) {
                return false;
            }
        }
        _log_stream.open(_log_path, std::ios::out | std::ios::app | std::ios::binary);
        return _log_stream.is_open() && _log_stream.good();
    }

    switch (_trunc_type) {
    case TRUNC_TYPE_NONE:
        return true;

    case TRUNC_TYPE_FILE_SZIE: {
        auto sz = std::filesystem::file_size(_log_path, ec);
        if (ec) {
            return false;
        }
        if (sz >= static_cast<uintmax_t>(_trunc_threshold)) {
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
    std::filesystem::path backup;
    if (!next_async_rotation_path(_log_path, backup)) {
        return false;
    }

    _log_stream.flush();
    _log_stream.close();

    std::error_code ec;
    std::filesystem::rename(_log_path, backup, ec);
    bool backup_created = !ec;
    if (ec) {
        std::filesystem::copy_file(_log_path, backup,
            std::filesystem::copy_options::overwrite_existing, ec);
        if (!ec) {
            backup_created = true;
            std::filesystem::remove(_log_path, ec);
        }
    }

    if (!backup_created) {
        // 轮转失败不应破坏已有日志；继续追加原文件，并在下一批重试轮转。
        _log_stream.clear();
        _log_stream.open(_log_path, std::ios::out | std::ios::app | std::ios::binary);
        if (!_log_stream.is_open()) {
            std::cerr << "rotate_file: backup failed and original file could not be reopened: "
                      << _log_path << std::endl;
            return false;
        }
        std::cerr << "rotate_file: backup failed; continuing with original file: "
                  << _log_path << std::endl;
        return true;
    }

    _log_stream.clear();
    _log_stream.open(_log_path, std::ios::out | std::ios::trunc | std::ios::binary);
    if (!_log_stream.is_open()) {
        std::cerr << "rotate_file: failed to open new log file: " << _log_path << std::endl;
        return false;
    }
    return true;
}

bool AsyncFileLogger::write_log(const buffer& log) {
    bool notify_worker = false;
    {
        std::unique_lock<std::mutex> lock(_buffer_mutex);

        if (!_accepting) {
            return false;
        }

        // Preserve FIFO order: publish the older partial buffer before
        // accepting a record that would cross its logical size limit.
        if (!_current_buffer->empty() &&
            log.size() > _buffer_size - _current_buffer->size()) {
            publish_current_locked();
            notify_worker = true;
        }

        const bool starts_partial_buffer = _current_buffer->empty();
        _current_buffer->append(log);
        _accepted_bytes += static_cast<std::uint64_t>(log.size());

        if (_current_buffer->size() >= _buffer_size) {
            publish_current_locked();
            notify_worker = true;
        } else if (starts_partial_buffer) {
            _partial_flush_deadline =
                std::chrono::steady_clock::now() + kPartialFlushInterval;
            _partial_flush_scheduled = true;
            notify_worker = true;
        }
    }
    if (notify_worker) {
        _cv.notify_one();
    }
    return true;
}


}//namespace LOG
}//namespace GST
