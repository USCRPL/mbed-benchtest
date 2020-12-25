cmake_minimum_required(VERSION 3.12)
cmake_policy(VERSION 3.12)

# load compilers and flags
# -------------------------------------------------------------

project(mbed-benchtest LANGUAGES C CXX)

include(cmake/Shorthand.cmake)
include(CheckFunctionExists)

if (NOT DEFINED RTXOFF_DEBUG)
	set(RTXOFF_DEBUG 0)
endif ()

if (NOT DEFINED RTXOFF_VERBOSE)
	set(RTXOFF_VERBOSE 0)
endif ()

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")

	set(CMAKE_CXX_FLAGS_RELEASE "/O2")
	set(CMAKE_CXX_FLAGS_DEBUG "/MDd /Zi /Ob0 /Od /RTC1")

	# enable default warnings
	add_compile_options(/W3)

	# enable exceptions
	add_compile_options(/EHsc)

	# enable M_PI
	add_definitions(-D_USE_MATH_DEFINES=1)

	# disable verbose security warnings
	add_definitions(-D_CRT_SECURE_NO_WARNINGS=1)

	# disable standard library max() and min() macros that break things
	add_definitions(-DNOMINMAX)
else()
	# gcc, clang, etc.
	add_compile_options(-Wall)
endif()

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_C_STANDARD 99)

find_package(Threads REQUIRED)

# setup testing
# -------------------------------------------------------------
enable_testing()

# target to run tests
# (note: "make test" is reserved, so use "make run_tests" instead)
add_custom_target(run_tests
	COMMAND ${CMAKE_CTEST_COMMAND} -j 2 --timeout 30 -V ${CMAKE_BINARY_DIR}
	COMMENT "Running mbed-benchtest tests")

# recurse to subdirectories
# -------------------------------------------------------------

add_subdirectory(RTXOff)
add_subdirectory(mbed-platform)
add_subdirectory(tests)
