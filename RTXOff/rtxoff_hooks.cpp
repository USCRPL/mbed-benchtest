//
// Fake MBed RTOS Idle API
//

#include "rtos_hooks.h"
#include "rtxoff_internal.h"
#include "ThreadDispatcher.h"

void rtos_attach_idle_hook(void (*fptr)(void))
{
	// lock mutex to make sure we don't switch to the idle thread at this point
	ThreadDispatcher::Mutex mutex;

	ThreadDispatcher::instance().hooks.idle_hook = fptr;
}

void rtos_attach_thread_terminate_hook(void (*fptr)(osThreadId_t id))
{
    // lock mutex to make sure we don't switch to the idle thread at this point
    ThreadDispatcher::Mutex mutex;

    ThreadDispatcher::instance().hooks.thread_terminate_hook = fptr;
}
