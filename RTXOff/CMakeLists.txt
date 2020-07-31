set(RTX_OFF_SOURCES
	# adapter file that provides cmsis OS API v1 from API v2
	cmsis_os.h
	cmsis_os1.c

	# thread API header
	cmsis_os2.h

	# header for most RTX program types
	rtxoff_os.h

	# Mbed RTOS adapter headers
	mbed_rtx_conf.h
	mbed_rtxoff_storage.h

	# implementations
	RTX_Config.h
	rtxoff_internal.h
	rtxoff_kernel.cpp
	rtxoff_thread.cpp
	rtxoff_mutex.cpp
	rtxoff_delay.cpp
	ThreadDispatcher.cpp
	ThreadDispatcher.h

	# program entry point
	rtxoff_main.cpp)

add_library(rtxoff STATIC ${RTX_OFF_SOURCES})
target_link_libraries(rtxoff)
target_include_directories(rtxoff PUBLIC .)

# detect windows vs linux
if("${CMAKE_SYSTEM_NAME}" STREQUAL "Windows")
	target_compile_definitions(rtxoff PUBLIC USE_WINTHREAD=1)

	# use the Windows 7 API version
	target_compile_definitions(rtxoff PUBLIC _WIN32_WINNT=0x601)
else()
	target_compile_definitions(rtxoff PUBLIC USE_WINTHREAD=0)
endif()

# manually apply mbed configs
target_compile_definitions(rtxoff PUBLIC MBED_CONF_RTOS_PRESENT=1)

add_subdirectory(arm-cmsis-rtos-validator)