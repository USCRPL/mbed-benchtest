cmake_minimum_required(VERSION 3.12)
cmake_policy(VERSION 3.12)

# load compilers and flags
# -------------------------------------------------------------

project(mbed-benchtest LANGUAGES C CXX)

include(cmake/Shorthand.cmake)
include(CheckFunctionExists)

if (NOT DEFINED RTXOFF_USE_32BIT)
	set(RTXOFF_USE_32BIT 0)
endif ()
add_compile_definitions(RTXOFF_USE_32BIT=${RTXOFF_USE_32BIT})

if (NOT DEFINED RTXOFF_DEBUG AND ${CMAKE_BUILD_TYPE} STREQUAL "Release")
	set(RTXOFF_DEBUG 0)
else ()
	set(RTXOFF_DEBUG 1)
endif ()

if (NOT DEFINED RTXOFF_VERBOSE)
	set(RTXOFF_VERBOSE 0)
endif ()

add_compile_definitions(RTXOFF_DEBUG=${RTXOFF_DEBUG})

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
else()
	# gcc, clang, etc.
	add_compile_options(-Wall)

	if (RTXOFF_USE_32BIT)
		add_definitions("-m32")
		SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -m32")
	endif ()
endif()

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_C_STANDARD 99)

find_package(Threads REQUIRED)

# recurse to subdirectories
# -------------------------------------------------------------

add_subdirectory(RTXOff)
add_subdirectory(mbed-platform)
add_subdirectory(tests)
