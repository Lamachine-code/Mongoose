execute_process(
    COMMAND "${PROGRAM}" "${SOURCE}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr
)
if(result EQUAL 0)
    message(FATAL_ERROR "Expected the source to fail, but it exited successfully")
endif()
if(NOT stderr MATCHES "${EXPECTED}")
    message(FATAL_ERROR "Diagnostic did not match '${EXPECTED}'. stderr: ${stderr}")
endif()
