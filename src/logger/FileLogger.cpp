#include "FileLogger.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace GST {
namespace LOG {

static std::string current_date_str() {
    auto now = std::chrono::system_clock::now();
    auto tt  = std::chrono::system_clock::to_time_t(now);
    struct tm tm_info{};
    localtime_r(&tt, &tm_info);
    char buf[9];
    strftime(buf, sizeof(buf), "%Y%m%d", &tm_info);
    return buf;
}

static std::string current_datetime_str() {
    auto now = std::chrono::system_clock::now();
    auto tt  = std::chrono::system_clock::to_time_t(now);
    struct tm tm_info{};
    localtime_r(&tt, &tm_info);
    char buf[16];
    strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", &tm_info);
    return buf;
}

FileLogger::~FileLogger() {
    _begin = false;
    if (_log_stream.is_open()) {
        _log_stream.flush();
        _log_stream.close();
    }
}

bool FileLogger::init(const LogConfig& config) {
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
    _level  = config._log_level;
    _format = LogFormat(config._log_format);
    _name   = config._logger_name;

    if (config._trunback_type) {
        _trunc_type = TRUNC_TYPE_SYS_TIME;
    } else if (config._log_max_size > 0) {
        _trunc_type      = TRUNC_TYPE_FILE_SZIE;
        _trunc_threshold = config._log_max_size;
    } else {
        _trunc_type = TRUNC_TYPE_NONE;
    }

    _current_date = current_date_str();
    _begin = true;
    return true;
}

bool FileLogger::trunc_log() {
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
        std::string today = current_date_str();
        if (today != _current_date) {
            _current_date = today;
            return rotate_file();
        }
        return true;
    }
    }
    return true;
}

// 关闭当前文件，重命名为带时间戳的备份，再开新文件
bool FileLogger::rotate_file() {
    _log_stream.flush();
    _log_stream.close();

    std::filesystem::path backup =
        _log_path.parent_path() /
        (_log_path.stem().string() + "_" + current_datetime_str() +
         _log_path.extension().string());

    std::error_code ec;
    std::filesystem::rename(_log_path, backup, ec);
    // rename 失败（如跨设备）时降级为复制+删除
    if (ec) {
        std::filesystem::copy_file(_log_path, backup,
            std::filesystem::copy_options::overwrite_existing, ec);
        if (!ec) {
            std::filesystem::remove(_log_path, ec);
        }
    }

    _log_stream.open(_log_path, std::ios::out | std::ios::trunc | std::ios::binary);
    return _log_stream.is_open();
}

bool FileLogger::write_log(const buffer& log) {
    _log_stream.write(log.data(), static_cast<std::streamsize>(log.size()));
    if(!_log_stream.good()) {
        return false;
    }
    _log_stream.flush();
    return true;
}


}
}