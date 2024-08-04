#include "Logger.h"
#include "LogFormat.h"
#include <iostream>
#include <libgen.h>
#include <fmt/format.h>
#include <fmt/chrono.h>

#include <thread>
#include <functional>

namespace GST{
namespace LOG{
bool LogFormat::format(LOG_LEVEL level, std::string& log, const char* file,
                    int line, const char* func) {
    std::string msg;
    //std::cout << "log1: " << log << std::endl;
    msg.swap(log);
    //std::cout << "log2: " << log << std::endl;
    size_t pos = 0;
    //std::cout << "_pattern: " << _pattern << std::endl;
    while (pos < _pattern.size()) {
        if (_pattern[pos] == '%') {
            if (pos + 1 < _pattern.size()) {
                switch (_pattern[pos + 1]) {
                    case 'P' :
                        //std::cout << "hit P" << std::endl;
                        log += std::to_string(get_thread_id());
                        log.append(" ");
                        break;
                    case 'L' :
                        //std::cout << "hit L" << std::endl;
                        log += get_loglevelstr(level);
                        log.append(" ");
                        break;
                    case 'T':
                        //std::cout << "hit T" << std::endl;
                        log += get_curtime();
                        log.append(" ");
                        break;
                    case 'F':
                        //std::cout << "hit F" << std::endl;
                        log += std::string(func);
                        log.append(" ");
                        break;
                    case 'I':
                        //std::cout << "hit I" << std::endl;
                        log += std::to_string(line);
                        log.append(" ");
                        break;
                    case 'S':
                        //std::cout << "hit S" << std::endl;
                        log += msg;
                        log.append(" ");
                        break;
                    case 'R':
                        //std::cout << "hit R" << std::endl;
                        log += get_filepath(file);
                        log.append(" ");
                        break;
                    case '%':
                        //std::cout << "hit %" << std::endl;
                        log.append(" ");
                        log += "%";

                }
                pos += 2;
            } else {
                log += "%";
                ++pos;
            }
        } else {
            log += _pattern[pos];
            ++pos;
        }
    }
    //std::cout << "log3: " << log << std::endl;
    log.push_back('\n');
    return true;
}

std::string LogFormat::get_filepath(std::string&& file_path) {
    auto index = file_path.find_last_of("/\\");
    if (index == std::string::npos) {
        return std::move(file_path);
    }
    return file_path.substr(index + 1);
}

std::string LogFormat::get_curtime() {
    auto now = fmt::localtime(std::time(nullptr));
    return fmt::format("{:%Y-%m-%d %H:%M:%S}", now);
}

std::string LogFormat::get_loglevelstr(GST::LOG::LOG_LEVEL level) {
    switch (level) {
        case GST::LOG::LOG_LEVEL::LEVEL_DEBUG:
            return "DEBUG";
        case GST::LOG::LOG_LEVEL::LEVEL_ERROR:
            return "ERROR";
        case GST::LOG::LOG_LEVEL::LEVEL_FATAL:
            return "FATAL";
        case GST::LOG::LOG_LEVEL::LEVEL_INFO:
            return "INFO";
        case GST::LOG::LOG_LEVEL::LEVEL_WARN:
            return "WARN";
        default:
            return "unknown";
    }
}

int LogFormat::get_thread_id() {
    std::thread::id threadId = std::this_thread::get_id();
    std::hash<std::thread::id> hasher;
    return static_cast<int>(hasher(threadId));
}
    
}
}
