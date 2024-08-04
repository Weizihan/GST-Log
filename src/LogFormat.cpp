#include "Logger.h"
#include "LogFormat.h"
#include <iostream>
#include <libgen.h>

#include <thread>
#include <functional>

namespace GST{
namespace LOG{
bool LogFormat::format(LOG_LEVEL level, std::string& log, const char* file,
                    int line, const char* func) {
    std::string msg;
    msg.swap(log);
    size_t pos = 0;
    while (pos < _pattern.size()) {
        if (_pattern[pos] == '%') {
            if (pos + 1 < _pattern.size()) {
                switch (_pattern[pos + 1]) {
                    case 'P' :
                        log += std::to_string(get_thread_id());
                        log.append(" ");
                        break;
                    case 'L' :
                        log += get_loglevelstr(level);
                        log.append(" ");
                        break;
                    case 'T':
                        log += get_curtime();
                        log.append(" ");
                        break;
                    case 'F':
                        log += std::string(func);
                        log.append(" ");
                        break;
                    case 'I':
                        log += std::to_string(line);
                        log.append(" ");
                        break;
                    case 'S':
                        log += msg;
                        log.append(" ");
                        break;
                    case 'R':
                        log += get_filepath(file);
                        log.append(" ");
                        break;
                    case '%':
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
    return "";
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

unsigned long int LogFormat::get_thread_id() {
    std::thread::id threadId = std::this_thread::get_id();
    std::hash<std::thread::id> hasher;
    return static_cast<unsigned long int>(hasher(threadId));
}
    
}
}
