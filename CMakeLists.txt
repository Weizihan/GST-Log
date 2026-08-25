cmake_minimum_required(VERSION 3.16)
project(GST_log VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 源文件
set(GST_LOG_SRC
    GstLog.cpp
    src/GstLogger.cpp
    src/LogConfig.cpp
    src/LogFormat.cpp
    src/logger/AsyncFileLogger.cpp
    src/logger/ConsoleLogger.cpp
    src/logger/FileLogger.cpp
)

# 创建静态库
add_library(GST_Log STATIC ${GST_LOG_SRC})
add_library(GST::Log ALIAS GST_Log)
target_compile_features(GST_Log PUBLIC cxx_std_17)

# PUBLIC：下游 target_link_libraries(GST::Log) 时自动继承这些 include 路径
target_include_directories(GST_Log PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/logger>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/src>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/src/logger>
)

find_package(Threads REQUIRED)
target_link_libraries(GST_Log PUBLIC Threads::Threads)

install(TARGETS GST_Log
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(FILES GstLog.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES
    src/GstLogger.h
    src/LogConfig.h
    src/LogFormat.h
    src/Marco.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/src
)
install(FILES
    src/logger/AsyncFileLogger.h
    src/logger/ConsoleLogger.h
    src/logger/FileLogger.h
    src/logger/Logger.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/src/logger
)

# 是否构建 example
option(BUILD_EXAMPLE "Build example" OFF)

if (BUILD_EXAMPLE)
    file(GLOB EXAMPLE_SRC "${CMAKE_CURRENT_SOURCE_DIR}/example/*.cpp")

    add_executable(example_Async ${EXAMPLE_SRC})
    
    target_link_libraries(example_Async PRIVATE GST::Log)
endif()
