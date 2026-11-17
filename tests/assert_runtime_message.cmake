execute_process(
    COMMAND "${PROGRAM}" "${SOURCE}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr
)
if(result EQUAL 0)
    message(FATAL_ERROR "Expected Mongoose to fail, but it exited successfully")
endif()
string(FIND "${stderr}" "${EXPECTED}" position)
if(position EQUAL -1)
    message(FATAL_ERROR "Expected diagnostic '${EXPECTED}' was not found. stderr: ${stderr}")
endif()
