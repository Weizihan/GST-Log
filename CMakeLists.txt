cmake_minimum_required(VERSION 3.16)
project(GST_log VERSION 0.2.0 LANGUAGES CXX)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
find_package(Threads REQUIRED)

add_library(GST_Log STATIC
    GstLog.cpp src/GstLogger.cpp src/LogConfig.cpp src/LogFormat.cpp
    src/logger/Logger.cpp src/logger/LogFile.cpp src/logger/FileLogger.cpp
    src/logger/AsyncFileLogger.cpp src/logger/ConsoleLogger.cpp)
add_library(GST::Log ALIAS GST_Log)
set_target_properties(GST_Log PROPERTIES EXPORT_NAME Log)
target_compile_features(GST_Log PUBLIC cxx_std_17)
target_include_directories(GST_Log PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/logger>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/src>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/src/logger>)
target_link_libraries(GST_Log PUBLIC Threads::Threads)

install(TARGETS GST_Log EXPORT GSTLogTargets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES GstLog.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY src/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/src FILES_MATCHING PATTERN "*.h")
install(EXPORT GSTLogTargets NAMESPACE GST:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/GSTLog)
configure_package_config_file(cmake/GSTLogConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/GSTLogConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/GSTLog)
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/GSTLogConfigVersion.cmake
    VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/GSTLogConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/GSTLogConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/GSTLog)

option(BUILD_EXAMPLE "Build example" OFF)
if(BUILD_EXAMPLE)
    add_executable(example_Async example/Async_example.cpp)
    target_link_libraries(example_Async PRIVATE GST::Log)
endif()

option(GST_LOG_BUILD_TESTS "Build GST-Log regression tests" OFF)
if(GST_LOG_BUILD_TESTS)
    enable_testing()
    add_executable(log_tests tests/Log_test.cpp)
    target_link_libraries(log_tests PRIVATE GST::Log)
    target_link_options(log_tests PRIVATE
        "LINKER:--wrap=gst_log_wall_time" "LINKER:--wrap=gst_log_steady_time"
        "LINKER:--wrap=write" "LINKER:--wrap=rename" "LINKER:--wrap=open")
    foreach(case format registry compatibility paths rotation_sync rotation_async async_lifecycle errors concurrent)
        add_test(NAME gst_log.${case} COMMAND log_tests ${case})
        set_tests_properties(gst_log.${case} PROPERTIES TIMEOUT 20)
    endforeach()
    add_executable(log_early_test tests/EarlyStop_test.cpp)
    target_link_libraries(log_early_test PRIVATE GST::Log ${CMAKE_DL_LIBS})
    add_test(NAME gst_log.early_stop COMMAND log_early_test)
    set_tests_properties(gst_log.early_stop PROPERTIES TIMEOUT 20)
    add_test(NAME gst_log.external COMMAND ${CMAKE_COMMAND}
        "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}" "-DBINARY=${CMAKE_CURRENT_BINARY_DIR}"
        "-DCONFIG=$<CONFIG>" "-DCOMPILER=${CMAKE_CXX_COMPILER}" "-DFLAGS=${CMAKE_CXX_FLAGS}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/Install_test.cmake")
    set_tests_properties(gst_log.external PROPERTIES TIMEOUT 60)
endif()
