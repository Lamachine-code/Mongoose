execute_process(
    COMMAND "${PROGRAM}" "${SOURCE}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr
)
if(result EQUAL 0)
    message(FATAL_ERROR "Expected a runtime failure, but the program exited successfully")
endif()
foreach(expected IN ITEMS
    "runtime error: division by zero"
    "at inner ("
    "at middle ("
    "at outer ("
    "at <main> ("
)
    string(FIND "${stderr}" "${expected}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "Missing stack trace entry '${expected}'. stderr: ${stderr}")
    endif()
endforeach()
