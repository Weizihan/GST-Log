cmake_minimum_required(VERSION 3.10)

project(GST_log)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Set output directory for binaries
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/output)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/logger
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/json/include
)

# Find FMT library
find_library(FMT_LIB fmt PATHS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/fmt/lib REQUIRED)

# Source files
file(GLOB SRC_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/example/*.cpp"
)

# Compile options
set(compile_option "-pthread")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${compile_option}")

# Create executable
add_executable(example_Async
    ${SRC_FILES}
    ./example/Async_example.cpp
)

# Link libraries
target_link_libraries(example_Async PRIVATE ${FMT_LIB})
target_link_libraries(example_Async PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/json/lib/libnlohmann_json_static.a)

# Add custom target to copy files
add_custom_target(copy_conf
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/../example/conf ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    COMMENT "Copying configuration files to output directory"
)

# Ensure copy_conf runs after all other targets
add_dependencies(copy_conf example_Async)

# Make sure the custom target runs after all build targets
add_custom_target(after_build_tasks
    DEPENDS example_Asnyc copy_conf
)
