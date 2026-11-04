#pragma once
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>
#include "Logger.h"
#include "LogFile.h"
namespace GST {
namespace LOG {
class AsyncFileLogger : public Logger {
public:
    AsyncFileLogger() = default;
    ~AsyncFileLogger() override;
    bool init(const LogConfig& config) override;
    bool write_log(const buffer& log) override;
    bool flush() override;
    bool shutdown() override;
private:
    struct Batch {
        buffer data;
        std::size_t records;
    };
    void publish_current_locked();
    void thread_func() noexcept;
    detail::LogFile _file;
    std::thread _write_worker;
    std::mutex _shutdown_mutex;
    std::mutex _buffer_mutex;
    std::condition_variable _cv;
    std::condition_variable _flush_cv;
    std::vector<Batch> _buffers;
    buffer _current_buffer;
    std::size_t _current_records{0};
    std::size_t _buffer_size{64 * 1024};
    std::size_t _max_pending_logs{8192};
    std::size_t _max_pending_bytes{8 * 1024 * 1024};
    std::size_t _pending_records{0};
    std::uint64_t _accepted_bytes{0};
    std::uint64_t _completed_bytes{0};
    bool _worker_stopped{true};
    bool _write_failed{false};
};
}
}
