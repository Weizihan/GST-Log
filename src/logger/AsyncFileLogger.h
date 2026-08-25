#pragma once

#include <cstddef>
#include <cstdint>
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
    bool flush(std::chrono::milliseconds timeout) override;
    bool shutdown(std::chrono::milliseconds timeout) override;
private:
    enum class WriteResult {
        Success,
        NotWritten,
        OutcomeUnknown
    };

    struct PendingBatch {
        buffer data;
        std::uint64_t batch_id{0};
        std::uint64_t attempt{1};
        bool replay_required{false};
        bool marker_attempted{false};
    };

    void thread_func() noexcept;
    void worker_loop(bool& degraded);
    WriteResult write_pending(PendingBatch& pending);
    bool write_replay_marker(PendingBatch& pending) noexcept;
    void mark_outcome_unknown(PendingBatch& pending) noexcept;
    void report_discarded_pending(std::size_t bytes) noexcept;
    bufferptr take_empty_buffer_locked();
    void publish_current_locked();
    void recycle_buffer(buffer&& reusable);
    bool flush_until(std::chrono::steady_clock::time_point deadline);
    void stop_worker_best_effort() noexcept;
    bool file_trunc();   // 在 worker 线程内执行实际的轮转检查
    bool rotate_file();  // 轮转：rename + 重开文件

    std::filesystem::path _log_path;
    std::ofstream _log_stream;
    std::thread _write_worker;
    std::condition_variable _cv;
    std::condition_variable _flush_cv;
    std::mutex _buffer_mutex;
    std::mutex _shutdown_mutex;
    buffervecptr _buffers;
    bufferptr _current_buffer;
    bufferptr _next_buffer;
    std::size_t _buffer_size{64 * 1024};
    std::uint64_t _accepted_bytes{0};
    std::uint64_t _flushed_bytes{0};
    std::uint64_t _next_batch_id{0};
    std::chrono::steady_clock::time_point _partial_flush_deadline{};
    bool _partial_flush_scheduled{false};
    std::atomic<bool> _accepting{false};
    bool _worker_stopped{true};
    bool _shutdown_complete{false};
    std::atomic<bool> _worker_running{false};
    std::string _current_date;  // for TRUNC_TYPE_SYS_TIME
    LogFormat _replay_format;
};

}
}
