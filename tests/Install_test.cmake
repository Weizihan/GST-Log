set(prefix "${BINARY}/external-prefix")
execute_process(COMMAND "${CMAKE_COMMAND}" --install "${BINARY}" --prefix "${prefix}" --config "${CONFIG}"
    RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Install failed: ${result}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" -S "${SOURCE}/tests/external" -B "${BINARY}/external-build"
    "-DCMAKE_PREFIX_PATH=${prefix}" "-DCMAKE_BUILD_TYPE=${CONFIG}" "-DCMAKE_CXX_COMPILER=${COMPILER}" "-DCMAKE_CXX_FLAGS=${FLAGS}"
    RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Consumer configuration failed: ${result}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" --build "${BINARY}/external-build" --config "${CONFIG}"
    RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Consumer build failed: ${result}")
endif()
# Every test run uses a fresh output directory; no old log deletion is needed.
string(RANDOM LENGTH 12 ALPHABET 0123456789abcdef suffix)
execute_process(COMMAND "${BINARY}/external-build/consumer" "${BINARY}/consumer-${suffix}"
    RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Consumer execution failed: ${result}")
endif()
