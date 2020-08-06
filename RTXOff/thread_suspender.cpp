
#include "cmsis_os2.h"
#include "thread_suspender.h"
#include <iostream>
#include <system_error>

#if USE_WINTHREAD

// Windows implementation of thread suspender.  Very simple since
// this is provided by win32 threads.

void thread_suspender_init()
{
	// Do nothing.
}

os_thread_id thread_suspender_create_suspended_thread(struct thread_suspender_data ** data, void (*start_func)(void* arg), void* arg)
{
	// Windows implementation doesn't need data
	data = nullptr;

	os_thread_id thread = CreateThread(nullptr,
									0,
									reinterpret_cast<LPTHREAD_START_ROUTINE>(start_func),
									reinterpret_cast<void*>(arg),
									CREATE_SUSPENDED,
									nullptr);

	if(thread == nullptr)
	{
		std::cerr << "Error creating thread: " << std::system_category().message(GetLastError()) << std::endl;
		return nullptr;
	}

	return thread;
}

void thread_suspender_suspend(os_thread_id thread, struct thread_suspender_data * data)
{
	if(SuspendThread(thread) < 0)
	{
		std::cerr << "Error suspending thread: " << std::system_category().message(GetLastError()) << std::endl;
	}
}

void thread_suspender_resume(os_thread_id thread, struct thread_suspender_data * data)
{
	if(ResumeThread(thread) < 0)
	{
		std::cerr << "Error resuming RTX thread: " << std::system_category().message(GetLastError()) << std::endl;
	}
}

void thread_suspender_kill(os_thread_id thread, struct thread_suspender_data * data)
{
	if(!TerminateThread(thread, 0))
	{
		std::cerr << "Error terminating RTX thread: " << std::system_category().message(GetLastError()) << std::endl;
	}
}

__NO_RETURN void thread_suspender_current_thread_exit()
{
	ExitThread(0);
}

#else
#endif