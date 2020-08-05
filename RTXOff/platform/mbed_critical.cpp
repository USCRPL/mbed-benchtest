//
// Header providing fake Mbed critical section functions
//

#include <rtxoff_internal.h>
#include <ThreadDispatcher.h>
#include "mbed_critical.h"

static uint32_t critical_section_reentrancy_counter = 0;

bool core_util_are_interrupts_enabled(void)
{
	return ThreadDispatcher::instance().interrupt.enabled;
}

bool core_util_is_isr_active(void)
{
	return ThreadDispatcher::instance().interrupt.active;
}

bool core_util_in_critical_section(void)
{
	return critical_section_reentrancy_counter > 0;
}

void core_util_critical_section_enter(void)
{
	// disable the thread scheduler by locking the mutex needed for it to exit sleep
	ThreadDispatcher::instance().lockMutex();

	// also disable interrupts (mainly so that trying to call RTXOff functions will trigger an error)
	ThreadDispatcher::instance().interrupt.enabled = false;

	++critical_section_reentrancy_counter;
}

void core_util_critical_section_exit(void)
{

	// If critical_section_enter has not previously been called, do nothing
	if (critical_section_reentrancy_counter == 0) {
		return;
	}

	--critical_section_reentrancy_counter;

	// this is a recursive mutex so we need to unlock it as many times as we locked it
	ThreadDispatcher::instance().unlockMutex();

	if (critical_section_reentrancy_counter == 0)
	{
		ThreadDispatcher::instance().interrupt.enabled = true;
	}
}