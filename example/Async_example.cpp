#include <atomic>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "../GstLog.h"
#include "../src/LogConfig.h"

namespace {

std::atomic<uint64_t> g_log_total_count{0};

void thread_log_task(const std::string& payload, uint64_t log_count_per_thread) {
    for (uint64_t i = 0; i < log_count_per_thread; ++i) {
        INFO("%s", payload.c_str());
        g_log_total_count.fetch_add(1, std::memory_order_relaxed);
    }
}

void print_report(
    const std::string& mode,
    const std::string& log_path,
    uint32_t thread_num,
    uint64_t log_count_per_thread,
    double elapsed_sec,
    uint64_t total_logs) {
    const uint64_t actual_size = std::filesystem::exists(log_path)
        ? std::filesystem::file_size(log_path)
        : 0;
    const uint64_t expected_size = total_logs * 66; // 64 payload + " " + "\n"

    const double mb = static_cast<double>(actual_size) / (1024.0 * 1024.0);
    const double lines_per_sec = elapsed_sec > 0.0
        ? static_cast<double>(total_logs) / elapsed_sec
        : 0.0;
    const double mb_per_sec = elapsed_sec > 0.0 ? mb / elapsed_sec : 0.0;

    std::cout << "\n=== Benchmark Result ===\n"
              << "mode: " << mode << "\n"
              << "threads: " << thread_num << "\n"
              << "logs per thread: " << log_count_per_thread << "\n"
              << "total logs: " << total_logs << "\n"
              << "elapsed: " << elapsed_sec << " sec\n"
              << "actual file size: " << actual_size << " bytes (" << mb << " MiB)\n"
              << "expected file size(approx): " << expected_size << " bytes\n"
              << "throughput: " << static_cast<uint64_t>(lines_per_sec) << " logs/sec"
              << " | " << mb_per_sec << " MiB/sec\n";
}

} // namespace

int main(int argc, char** argv) {
    // usage:
    //   ./example_Async              (default: async)
    //   ./example_Async file         (sync file logger)
    //   ./example_Async async
    const std::string mode = (argc > 1) ? argv[1] : "async";
    const uint32_t thread_num = 50;
    const uint64_t log_count_per_thread = 150000;
    const std::string payload(1024, 'X');
    const std::string log_path = (mode == "file")
        ? "output/file_bench.log"
        : "output/async_bench.log";

    std::error_code ec;
    std::filesystem::create_directories("output", ec);
    std::filesystem::remove(log_path, ec);

    GST::LOG::LogConfig cfg;
    cfg._logger_name = (mode == "file") ? "File" : "Async";
    cfg._log_level = GST::LOG::LOG_LEVEL::LEVEL_INFO;
    cfg._log_format = "%S"; // keep one-line format for predictable size
    cfg._log_target = log_path;
    cfg._buffer_size = 64 * 1024;
    cfg._is_colorful_log = false;

    if (!GST::LOG::GstLogger::get_Instance()->init(cfg)) {
        std::cerr << "logger init failed, mode=" << mode << "\n";
        return 1;
    }

    auto start = std::chrono::high_resolution_clock::now();
    std::vector<std::thread> threads;
    threads.reserve(thread_num);
    for (uint32_t i = 0; i < thread_num; ++i) {
        threads.emplace_back(thread_log_task, payload, log_count_per_thread);
    }
    for (auto& t : threads) {
        t.join();
    }
    auto end = std::chrono::high_resolution_clock::now();

    if (mode != "file") {
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }

    const double elapsed_sec = std::chrono::duration<double>(end - start).count();
    const uint64_t total_logs = g_log_total_count.load(std::memory_order_relaxed);
    print_report(mode, log_path, thread_num, log_count_per_thread, elapsed_sec, total_logs);
    return 0;
}