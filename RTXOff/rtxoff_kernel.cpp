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
	if (IsIrqMode() || IsIrqMasked())
	{
		return osErrorISR;
	}

	ThreadDispatcher::Mutex mutex;

	if (ThreadDispatcher::instance().kernel.state == osRtxKernelReady) {
		return osOK;
	}
	if (ThreadDispatcher::instance().kernel.state != osRtxKernelInactive) {
		return osError;
	}

	thread_suspender_init();

	ThreadDispatcher::instance().kernel.state = osRtxKernelReady;

	return osOK;
}

///  Get RTOS Kernel Information.
osStatus_t osKernelGetInfo (osVersion_t *version, char *id_buf, uint32_t id_size)
{
	uint32_t size;

	if (version != NULL) {
		version->api    = osRtxVersionAPI;
		version->kernel = osRtxVersionKernel;
	}

	if ((id_buf != NULL) && (id_size != 0U)) {
		if (id_size > sizeof(osRtxKernelId)) {
			size = sizeof(osRtxKernelId);
		} else {
			size = id_size;
		}
		memcpy(id_buf, osRtxKernelId, size);
	}

	return osOK;
}

/// Get the current RTOS Kernel state.
osKernelState_t osKernelGetState (void)
{
	ThreadDispatcher::Mutex mutex;
	return static_cast<osKernelState_t>(ThreadDispatcher::instance().kernel.state);
}

__NO_RETURN void osRtxIdleThread(void *argument)
{
	while(true)
	{
		ThreadDispatcher::instance().hooks.idle_hook();
	}
}

void rtxOffDefaultIdleFunc()
{
	// do nothing
}

/// Start the RTOS Kernel scheduler.
osStatus_t osKernelStart (void)
{
	if (IsIrqMode() || IsIrqMasked())
	{
		return osErrorISR;
	}

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

/// Get the RTOS kernel tick count.
uint32_t osKernelGetTickCount (void) {
	return ThreadDispatcher::instance().kernel.tick;
}

/// Get the RTOS kernel tick frequency.
uint32_t osKernelGetTickFreq (void) {
	return OS_TICK_FREQ;
}

/// Get the RTOS kernel system timer count.
/// This should be finer grain than the tick count.
uint32_t osKernelGetSysTimerCount (void) {
	return RTXClock::now().time_since_epoch().count();
}

/// Get the RTOS kernel system timer frequency.
uint32_t osKernelGetSysTimerFreq (void)
{
	typedef RTXClock::duration::period clockTickRatio;

	// take the inverse of the clock tick period
	return clockTickRatio::den / clockTickRatio::num;
}