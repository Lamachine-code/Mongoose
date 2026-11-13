cmake_minimum_required(VERSION 3.20)
project(Mongoose C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_executable(mongoose
  src/main_frontend.c
  src/lexer_frontend.c
  src/parser_frontend.c
  src/semantic_frontend.c
)
target_include_directories(mongoose PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(mongoose PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()
add_test(NAME golden_master_frontend
  COMMAND mongoose ${CMAKE_CURRENT_SOURCE_DIR}/golden-master.snake)

add_test(NAME floor_division_is_not_a_comment
  COMMAND mongoose ${CMAKE_CURRENT_SOURCE_DIR}/tests/floor_division.snake)
add_test(NAME undefined_identifier_is_rejected
  COMMAND mongoose ${CMAKE_CURRENT_SOURCE_DIR}/tests/undefined_identifier.snake)
set_tests_properties(undefined_identifier_is_rejected PROPERTIES WILL_FAIL TRUE)
add_test(NAME return_outside_function_is_rejected
  COMMAND mongoose ${CMAKE_CURRENT_SOURCE_DIR}/tests/return_outside_function.snake)
set_tests_properties(return_outside_function_is_rejected PROPERTIES WILL_FAIL TRUE)

add_test(NAME forward_function_call
  COMMAND mongoose ${CMAKE_CURRENT_SOURCE_DIR}/tests/forward_function_call.snake)
