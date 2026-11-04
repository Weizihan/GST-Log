#pragma once
#include <memory>

#include "src/LogConfig.h"
#include "src/Marco.h"
#include "src/GstLogger.h"

namespace GST {
namespace LOG {
std::shared_ptr<GstLogger> get_Instance();
}
}

#define INFO(...) GST::LOG::get_Instance()->log(-1, GST::LOG::LOG_LEVEL::LEVEL_INFO, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define WARN(...) GST::LOG::get_Instance()->log(-1, GST::LOG::LOG_LEVEL::LEVEL_WARN, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define ERROR(...) GST::LOG::get_Instance()->log(-1, GST::LOG::LOG_LEVEL::LEVEL_ERROR, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define DEBUG(...) GST::LOG::get_Instance()->log(-1, GST::LOG::LOG_LEVEL::LEVEL_DEBUG, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define FATAL(...) GST::LOG::get_Instance()->log(-1, GST::LOG::LOG_LEVEL::LEVEL_FATAL, __FILE__, __LINE__, __func__, __VA_ARGS__)

#define INFO_INDEX(index, ...) GST::LOG::get_Instance()->log(index, GST::LOG::LOG_LEVEL::LEVEL_INFO, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define WARN_INDEX(index, ...) GST::LOG::get_Instance()->log(index, GST::LOG::LOG_LEVEL::LEVEL_WARN, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define ERROR_INDEX(index, ...) GST::LOG::get_Instance()->log(index, GST::LOG::LOG_LEVEL::LEVEL_ERROR, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define DEBUG_INDEX(index, ...) GST::LOG::get_Instance()->log(index, GST::LOG::LOG_LEVEL::LEVEL_DEBUG, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define FATAL_INDEX(index, ...) GST::LOG::get_Instance()->log(index, GST::LOG::LOG_LEVEL::LEVEL_FATAL, __FILE__, __LINE__, __func__, __VA_ARGS__)

#define INFO_NAME(logname, ...) GST::LOG::get_Instance()->log(logname, GST::LOG::LOG_LEVEL::LEVEL_INFO, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define WARN_NAME(logname, ...) GST::LOG::get_Instance()->log(logname, GST::LOG::LOG_LEVEL::LEVEL_WARN, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define ERROR_NAME(logname, ...) GST::LOG::get_Instance()->log(logname, GST::LOG::LOG_LEVEL::LEVEL_ERROR, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define DEBUG_NAME(logname, ...) GST::LOG::get_Instance()->log(logname, GST::LOG::LOG_LEVEL::LEVEL_DEBUG, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define FATAL_NAME(logname, ...) GST::LOG::get_Instance()->log(logname, GST::LOG::LOG_LEVEL::LEVEL_FATAL, __FILE__, __LINE__, __func__, __VA_ARGS__)
