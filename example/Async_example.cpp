#include <iostream>
#include <thread>
#include <string>
#include <chrono>
#include <cstdarg>
#include <atomic>
#include <vector>
#include <string>
#include "../GstLog.h"

std::atomic<int> count(0);
void thread_write(const std::string& str, int num) {
    for (int i = 0; i < num; ++i) {
        INFO("%s:%d", str.c_str(), i);
        DEBUG("%s:%d", str.c_str(), i);
        WARN("%s:%d", str.c_str(), i);
        ERROR("%s:%d", str.c_str(), i);
        FATAL("%s:%d", str.c_str(), i);
        count++;
    }
}

int main() {
    auto start_time = std::chrono::high_resolution_clock::now();

    if (!GST::LOG::GstLogger::get_Instance()->init()) {
        std::cout << "Logger initialization failed" << std::endl;
        return 0;
    }

    std::vector<std::thread> threads;

    for(int i = 0; i < 100; i++){
        threads.emplace_back(std::move(std::thread(thread_write, "hello world", 2000000)));
    }

    for(auto &it:threads) {
        it.join();
    }


    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end_time - start_time;
    
    std::cout << "write" << count << "use" << "Total execution time: " << elapsed.count() << " seconds" << std::endl;

    return 0;
}


