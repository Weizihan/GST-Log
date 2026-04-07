cmake_minimum_required(VERSION 3.10)
project(GST_log)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/output)

# 头文件目录
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/logger
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# 源文件
file(GLOB GST_LOG_SRC
    "${CMAKE_CURRENT_SOURCE_DIR}/src/logger/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

# 创建静态库
add_library(Tlog STATIC ${GST_LOG_SRC})
find_package(Threads REQUIRED)
target_link_libraries(Tlog PUBLIC Threads::Threads)

# 安装头文件和库
install(TARGETS Tlog DESTINATION lib)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/ DESTINATION include)

# 是否构建 example
option(BUILD_EXAMPLE "Build example" OFF)

if (BUILD_EXAMPLE)
    file(GLOB EXAMPLE_SRC "${CMAKE_CURRENT_SOURCE_DIR}/example/*.cpp")

    add_executable(example_Async ${EXAMPLE_SRC})
    
    target_link_libraries(example_Async PRIVATE Tlog)
endif()
