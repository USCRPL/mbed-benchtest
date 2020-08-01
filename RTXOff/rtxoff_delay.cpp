
#include "rtxoff_internal.h"
#include "ThreadDispatcher.h"

/// Wait for Timeout (Time Delay).
osStatus_t osDelay (uint32_t ticks)
{
	if (IsIrqMode() || IsIrqMasked())
	{
		return osErrorISR;
	}

	ThreadDispatcher::Mutex mutex;

	if (ticks != 0U) {
		osRtxThreadWaitEnter(osRtxThreadWaitingDelay, ticks);

		ThreadDispatcher::instance().blockUntilWoken();
#if RTXOFF_DEBUG
		std::cerr << "Delay returning, ticks = " << ThreadDispatcher::instance().kernel.tick << std::endl;
#endif
	}
	return osOK;
}

/// Wait until specified time.
osStatus_t osDelayUntil (uint32_t ticks)
{
	if (IsIrqMode() || IsIrqMasked())
	{
		return osErrorISR;
	}

	ThreadDispatcher::Mutex mutex;

	ticks -= ThreadDispatcher::instance().kernel.tick;
	if ((ticks == 0U) || (ticks > 0x7FFFFFFFU)) {
		return osErrorParameter;
	}

	osRtxThreadWaitEnter(osRtxThreadWaitingDelay, ticks);
	ThreadDispatcher::instance().blockUntilWoken();

	return osOK;
}