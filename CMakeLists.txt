cmake_minimum_required(VERSION 3.10)

project(GST_log)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/output)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/logger
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/json/include
)

find_library(FMT_LIB fmt PATHS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/fmt/lib REQUIRED)

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

target_link_libraries(example_Async PRIVATE ${FMT_LIB})
target_link_libraries(example_Async PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/json/lib/libnlohmann_json_static.a)

add_custom_target(copy_conf
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/../example/conf ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    COMMENT "Copying configuration files to output directory"
)

add_dependencies(copy_conf example_Async)

add_custom_target(after_build_tasks
    DEPENDS example_Asnyc copy_conf
)

