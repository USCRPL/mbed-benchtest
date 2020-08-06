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

	# Fake NVIC API
	rtxoff_nvic.h
	rtxoff_nvic.cpp

	# Fake Mbed APIs
	platform/mbed_critical.h
	platform/mbed_critical.cpp
	rtos_idle.h
	rtxoff_idle.cpp

	# implementations
	RTX_Config.h
	rtxoff_internal.h
	rtxoff_kernel.cpp
	rtxoff_thread.cpp
	rtxoff_mutex.cpp
	rtxoff_delay.cpp
	rtxoff_evflags.cpp
	rtxoff_semaphore.cpp
	ThreadDispatcher.cpp
	ThreadDispatcher.h
	rtxoff_clock.h
	rtxoff_clock.cpp
	thread_suspender.h
	thread_suspender.cpp

	# program entry point
	rtxoff_main.cpp thread_suspender.h)

add_library(rtxoff STATIC ${RTX_OFF_SOURCES})
target_link_libraries(rtxoff)
target_include_directories(rtxoff PUBLIC . platform)

# link with CMake pthreads
target_link_libraries(rtxoff Threads::Threads)

# detect windows vs linux
if("${CMAKE_SYSTEM_NAME}" STREQUAL "Windows")
	target_compile_definitions(rtxoff PUBLIC USE_WINTHREAD=1)

	# use the Windows 7 API version
	target_compile_definitions(rtxoff PUBLIC _WIN32_WINNT=0x601)
else()
	target_compile_definitions(rtxoff PUBLIC USE_WINTHREAD=0 _GNU_SOURCE=1)

	# check if the nonstandard pthread_setname_np() function exists
	set(CMAKE_REQUIRED_LIBRARIES Threads::Threads)
	check_all_functions(pthread_setname_np)

	if(HAVE_PTHREAD_SETNAME_NP)
		target_compile_definitions(rtxoff PRIVATE HAVE_PTHREAD_SETNAME_NP)
	endif()
endif()

# manually apply mbed configs
target_compile_definitions(rtxoff PUBLIC MBED_CONF_RTOS_PRESENT=1)
