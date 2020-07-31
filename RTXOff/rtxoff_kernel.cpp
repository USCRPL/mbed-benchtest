//
// File containing RTX kernel functions
//

#include "rtxoff_internal.h"
#include "ThreadDispatcher.h"
#include "RTX_Config.h"

#include <cstring>

/// Initialize the RTOS Kernel.
osStatus_t osKernelInitialize (void)
{
	ThreadDispatcher::Mutex mutex;

	if (ThreadDispatcher::instance().kernel.state == osRtxKernelReady) {
		return osOK;
	}
	if (ThreadDispatcher::instance().kernel.state != osRtxKernelInactive) {
		return osError;
	}

	ThreadDispatcher::instance().kernel.state = osRtxKernelReady;

	return osOK;
}

__NO_RETURN void osRtxIdleThread(void *argument)
{
	while(true)
	{}
}

/// Start the RTOS Kernel scheduler.
osStatus_t osKernelStart (void)
{
	// don't use a Mutex object since we're calling into dispatchForever()
	ThreadDispatcher::instance().lockMutex();

	osRtxThread_t *thread;

	if (ThreadDispatcher::instance().kernel.state != osRtxKernelReady) {
		ThreadDispatcher::instance().unlockMutex();
		return osError;
	}

	// Create Idle Thread
	static const osThreadAttr_t os_idle_thread_attr = {
			"rtxoff_idle",
			osThreadDetached,
			nullptr,
			0,
			nullptr,
			0,
			osPriorityIdle,
			0U,
			0U
	};
	ThreadDispatcher::instance().thread.idle = reinterpret_cast<osRtxThread_t *>(osThreadNew(osRtxIdleThread, NULL, &os_idle_thread_attr));

	// Switch to Ready Thread with highest Priority
	thread = osRtxThreadListGet(&ThreadDispatcher::instance().thread.ready);
	ThreadDispatcher::instance().switchNextThread(thread);
	ThreadDispatcher::instance().thread.run.curr = ThreadDispatcher::instance().thread.run.next;
	ThreadDispatcher::instance().thread.run.next = nullptr;

	ThreadDispatcher::instance().kernel.state = osRtxKernelRunning;

	ThreadDispatcher::instance().dispatchForever();
}
