cmake_minimum_required(VERSION 3.12)
cmake_policy(VERSION 3.12)

# load compilers and flags
# -------------------------------------------------------------

project(mbed-benchtest LANGUAGES C CXX)

include(cmake/Shorthand.cmake)
include(CheckFunctionExists)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_C_STANDARD 99)
add_compile_options(-Wall)

find_package(Threads REQUIRED)

# recurse to subdirectories
# -------------------------------------------------------------

add_subdirectory(RTXOff)
add_subdirectory(mbed-platform)
add_subdirectory(tests)
