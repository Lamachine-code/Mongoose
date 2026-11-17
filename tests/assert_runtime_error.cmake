execute_process(
    COMMAND "${PROGRAM}" "${SOURCE}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr
)
if(result EQUAL 0)
    message(FATAL_ERROR "Expected Mongoose to fail at runtime, but it exited successfully")
endif()
if(NOT stderr MATCHES ":3: runtime error: division by zero")
    message(FATAL_ERROR "Runtime diagnostic did not include the expected file line and error. stderr: ${stderr}")
endif()
