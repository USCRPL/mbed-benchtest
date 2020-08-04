cmake_minimum_required(VERSION 3.12)

# load compilers and flags
# -------------------------------------------------------------

project(mbed-benchtest LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_C_STANDARD 99)
add_compile_options(-Wall)

# recurse to subdirectories
# -------------------------------------------------------------

add_subdirectory(RTXOff)
add_subdirectory(mbed-platform)
