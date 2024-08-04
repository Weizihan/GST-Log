#pragma once

#include "src/GstLogger.h"

#define INFO(format, ...) GST::LOG::GstLogger::get_Instance()->log(-1, GST::LOG::LOG_LEVEL::LEVEL_INFO, __FILE__, __LINE__, __FUNCTION__, format, ##__VA_ARGS__)
#define WARN(format, ...) GST::LOG::GstLogger::get_Instance()->log(-1, GST::LOG::LOG_LEVEL::LEVEL_WARN, __FILE__, __LINE__, __FUNCTION__, format, ##__VA_ARGS__)
#define ERROR(format, ...) GST::LOG::GstLogger::get_Instance()->log(-1, GST::LOG::LOG_LEVEL::LEVEL_ERROR, __FILE__, __LINE__, __FUNCTION__, format, ##__VA_ARGS__)
#define DEBUG(format, ...) GST::LOG::GstLogger::get_Instance()->log(-1, GST::LOG::LOG_LEVEL::LEVEL_DEBUG, __FILE__, __LINE__, __FUNCTION__, format, ##__VA_ARGS__)
#define FATAL(format, ...) GST::LOG::GstLogger::get_Instance()->log(-1, GST::LOG::LOG_LEVEL::LEVEL_FATAL, __FILE__, __LINE__, __FUNCTION__, format, ##__VA_ARGS__)

#define INFO_INDEX(index, format, ...) GST::LOG::GstLogger::get_Instance()->log(index, GST::LOG::LOG_LEVEL::LEVEL_INFO, __FILE__, __LINE__, __FUNCTION__, format, ##__VA_ARGS__)
#define WARN_INDEX(index, format, ...) GST::LOG::GstLogger::get_Instance()->log(index, GST::LOG::LOG_LEVEL::LEVEL_WARN, __FILE__, __LINE__, __FUNCTION__, format, ##__VA_ARGS__)
#define ERROR_INDEX(index, format, ...) GST::LOG::GstLogger::get_Instance()->log(index, GST::LOG::LOG_LEVEL::LEVEL_ERROR, __FILE__, __LINE__, __FUNCTION__, format, ##__VA_ARGS__)
#define DEBUG_INDEX(index, format, ...) GST::LOG::GstLogger::get_Instance()->log(index, GST::LOG::LOG_LEVEL::LEVEL_DEBUG, __FILE__, __LINE__, __FUNCTION__, format, ##__VA_ARGS__)
#define FATAL_INDEX(index, format, ...) GST::LOG::GstLogger::get_Instance()->log(index, GST::LOG::LOG_LEVEL::LEVEL_FATAL, __FILE__, __LINE__, __FUNCTION__, format, ##__VA_ARGS__)

#define INFO_NAME(logname, format, ...) GST::LOG::GstLogger::get_Instance()->log(logname, GST::LOG::LOG_LEVEL::LEVEL_INFO, __FILE__, __LINE__, __FUNCTION__, format, ##__VA_ARGS__)
#define WARN_NAME(logname, format, ...) GST::LOG::GstLogger::get_Instance()->log(logname, GST::LOG::LOG_LEVEL::LEVEL_WARN, __FILE__, __LINE__, __FUNCTION__, format, ##__VA_ARGS__)
#define ERROR_NAME(logname, format, ...) GST::LOG::GstLogger::get_Instance()->log(logname, GST::LOG::LOG_LEVEL::LEVEL_ERROR, __FILE__, __LINE__, __FUNCTION__, format, ##__VA_ARGS__)
#define DEBUG_NAME(logname, format, ...) GST::LOG::GstLogger::get_Instance()->log(logname, GST::LOG::LOG_LEVEL::LEVEL_DEBUG, __FILE__, __LINE__, __FUNCTION__, format, ##__VA_ARGS__)
#define FATAL_NAME(logname, format, ...) GST::LOG::GstLogger::get_Instance()->log(logname, GST::LOG::LOG_LEVEL::LEVEL_FATAL, __FILE__, __LINE__, __FUNCTION__, format, ##__VA_ARGS__)