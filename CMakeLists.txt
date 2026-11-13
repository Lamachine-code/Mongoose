cmake_minimum_required(VERSION 3.20)
project(Mongoose C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_executable(mongoose src/frontend.c)
target_include_directories(mongoose PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(mongoose PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()
add_test(NAME golden_master_frontend
  COMMAND mongoose ${CMAKE_CURRENT_SOURCE_DIR}/golden-master.snake)
