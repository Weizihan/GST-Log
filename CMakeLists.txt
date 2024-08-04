cmake_minimum_required(VERSION 3.10)

project(GST_log)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/output)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/logger
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
)

file(GLOB SRC_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/example/*.cpp"
)

set(compile_option "-pthread")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${compile_option}")

add_executable(example_Async
    ${SRC_FILES}
    ./example/Async_example.cpp
)

