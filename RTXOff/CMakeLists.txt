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
	rtos_hooks.h
	rtxoff_hooks.cpp

	# implementations
	RTX_Config.h
	rtxoff_internal.h
	rtxoff_kernel.cpp
	rtxoff_thread.cpp
	rtxoff_mutex.cpp
	rtxoff_delay.cpp
	rtxoff_evflags.cpp
	rtxoff_semaphore.cpp
	rtxoff_mempool.cpp
	rtxoff_msgqueue.cpp
	rtxoff_timer.cpp
	ThreadDispatcher.cpp
	ThreadDispatcher.h
	rtxoff_clock.h
	rtxoff_clock.cpp
	thread_suspender.h
	thread_suspender.cpp

	# program entry point
	rtxoff_main.cpp)

add_library(rtxoff STATIC ${RTX_OFF_SOURCES})
target_link_libraries(rtxoff)
target_include_directories(rtxoff PUBLIC . platform)

# link with CMake pthreads
target_link_libraries(rtxoff Threads::Threads)

# Turn off exceptions.  On some platforms pthread_exit() uses an exception to unwind the stack,
# and this causes issues (SIGABRT when an exception propagates out of a pthread function).
# RTXOff will CRASH INTERMITTENTLY on Linux without this option!
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag(-fno-exceptions SUPPORTS_FNO_EXCEPTIONS)
if(SUPPORTS_FNO_EXCEPTIONS)
	target_compile_options(rtxoff PUBLIC -fno-exceptions)
endif()

# detect windows vs linux
if("${CMAKE_SYSTEM_NAME}" STREQUAL "Windows")
	target_compile_definitions(rtxoff PUBLIC USE_WINTHREAD=1)

	# use the Windows 7 API version
	target_compile_definitions(rtxoff PUBLIC _WIN32_WINNT=0x601)
else()
	target_compile_definitions(rtxoff PUBLIC USE_WINTHREAD=0 _GNU_SOURCE=1)

	# check for a thread yield function
	set(CMAKE_REQUIRED_LIBRARIES Threads::Threads)
	check_all_functions(pthread_yield sched_yield)

	if(NOT (HAVE_PTHREAD_YIELD OR HAVE_SCHED_YIELD))
		message(FATAL_ERROR "Your system does not provide pthread_yield() or sched_yield().  RTXOff cannot build.")
	endif()
	if(HAVE_PTHREAD_YIELD)
		target_compile_definitions(rtxoff PRIVATE HAVE_PTHREAD_YIELD)
	endif()

	# Check for a linux API pthread_setname_np (as opposed to the Mac version that works differently).
	# The situation is summarized here: https://stackoverflow.com/questions/2369738/how-to-set-the-name-of-a-thread-in-linux-pthreads/7989973#7989973
	include(CheckCSourceCompiles)
	check_c_source_compiles(
		"#include <pthread.h>
		int main(int argc, char** argv)
		{
		    pthread_setname_np(pthread_self(), \"TestThread\");
		}"
		PTHREAD_SETNAME_NP_WORKS)

	if(PTHREAD_SETNAME_NP_WORKS)
		target_compile_definitions(rtxoff PRIVATE HAVE_PTHREAD_SETNAME_NP)
	endif()

	# tell code to use more limited timing functions available on OS X
	if("${CMAKE_SYSTEM_NAME}" STREQUAL "Darwin")
		target_compile_definitions(rtxoff PRIVATE USE_MAC_TIME)
	endif()

endif()

# manually apply mbed configs
target_compile_definitions(rtxoff PUBLIC MBED_CONF_RTOS_PRESENT=1)
