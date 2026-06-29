set(compile_flags -std=c++26)
if(CPPORM_TEST_REFLECTION_FLAG)
    list(APPEND compile_flags "${CPPORM_TEST_REFLECTION_FLAG}")
endif()

execute_process(
    COMMAND
        "${CPPORM_TEST_CXX}"
        ${compile_flags}
        "-I${CPPORM_TEST_INCLUDE_DIR}"
        -c "${CPPORM_TEST_SOURCE}"
        -o "${CPPORM_TEST_OUTPUT}"
    RESULT_VARIABLE compile_result
    OUTPUT_VARIABLE compile_output
    ERROR_VARIABLE compile_error
)

set(compile_log "${compile_output}\n${compile_error}")

if(compile_result EQUAL 0)
    message(FATAL_ERROR "Expected compilation to fail, but it succeeded: ${CPPORM_TEST_SOURCE}")
endif()

if(NOT compile_log MATCHES "${CPPORM_TEST_EXPECTED}")
    message(FATAL_ERROR
        "Compilation failed, but did not contain expected message: ${CPPORM_TEST_EXPECTED}\n${compile_log}"
    )
endif()

message(STATUS "Compilation failed as expected: ${CPPORM_TEST_SOURCE}")
