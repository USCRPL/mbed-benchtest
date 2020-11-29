#include "ThreadDispatcher.h"
#include "rtxoff_internal.h"

//  ==== Helper functions ====

/// Set Event Flags.
/// \param[in]  ef              event flags object.
/// \param[in]  flags           specifies the flags to set.
/// \return event flags after setting.
static uint32_t EventFlagsSet (osRtxEventFlags_t *ef, uint32_t flags)
{
	uint32_t event_flags;

	ef->event_flags |= flags;
	event_flags = ef->event_flags;

	return event_flags;
}

/// Clear Event Flags.
/// \param[in]  ef              event flags object.
/// \param[in]  flags           specifies the flags to clear.
/// \return event flags before clearing.
static uint32_t EventFlagsClear (osRtxEventFlags_t *ef, uint32_t flags)
{
	uint32_t event_flags;

	event_flags = ef->event_flags;
	ef->event_flags &= ~flags;

	return event_flags;
}

/// Check Event Flags.
/// \param[in]  ef              event flags object.
/// \param[in]  flags           specifies the flags to check.
/// \param[in]  options         specifies flags options (osFlagsXxxx).
/// \return event flags before clearing or 0 if specified flags have not been set.
static uint32_t EventFlagsCheck (osRtxEventFlags_t *ef, uint32_t flags, uint32_t options) {
	uint32_t event_flags;

	if ((options & osFlagsNoClear) == 0U) {

		event_flags = ef->event_flags;
		if ((((options & osFlagsWaitAll) != 0U) && ((event_flags & flags) != flags)) ||
		    (((options & osFlagsWaitAll) == 0U) && ((event_flags & flags) == 0U))) {
			event_flags = 0U;
		} else {
			ef->event_flags &= ~flags;
		}

	} else {
		event_flags = ef->event_flags;
		if ((((options & osFlagsWaitAll) != 0U) && ((event_flags & flags) != flags)) ||
		    (((options & osFlagsWaitAll) == 0U) && ((event_flags & flags) == 0U))) {
			event_flags = 0U;
		}
	}

	return event_flags;
}

//  ==== Post ISR processing ====

/// Event Flags post ISR processing.
/// \param[in]  ef              event flags object.
static void osRtxEventFlagsPostProcess (osRtxEventFlags_t * ef)
{
	osRtxThread_t *thread;
	osRtxThread_t *thread_next;
	uint32_t     event_flags;

	// Check if Threads are waiting for Event Flags
	thread = ef->thread_list;
	while (thread != NULL) {
		thread_next = thread->thread_next;
		event_flags = EventFlagsCheck(ef, thread->wait_flags, thread->flags_options);
		if (event_flags != 0U) {
			osRtxThreadListRemove(thread);
			osRtxThreadWaitExit(thread, event_flags, false);
		}
		thread = thread_next;
	}
}


//  ==== Public API ====

/// Create and Initialize an Event Flags object.
osEventFlagsId_t osEventFlagsNew (const osEventFlagsAttr_t *attr)
{
	ThreadDispatcher::Mutex dispMutex;

	osRtxEventFlags_t *ef;
	uint8_t           flags = 0;
	const char       *name;

	// Process attributes
	if (attr != NULL) {
		name = attr->name;
	} else {
		name = "<anonymous event flags>";
		ef   = NULL;
	}

	ef = new osRtxEventFlags_t();

	if (ef != NULL) {
		// Initialize control block
		ef->id          = osRtxIdEventFlags;
		ef->flags       = flags;
		ef->name        = name;
		ef->thread_list = NULL;
		ef->event_flags = 0U;

		// Register post ISR processing function
		ThreadDispatcher::instance().post_process.event_flags = osRtxEventFlagsPostProcess;

	}

	return ef;
}

/// Get name of an Event Flags object.
const char *osEventFlagsGetName (osEventFlagsId_t ef_id) {

	if (IsIrqMode() || IsIrqMasked()) {
		return nullptr;
	}

	// don't need to lock mutex because the name of an event flags can't be changed

	osRtxEventFlags_t *ef = reinterpret_cast<osRtxEventFlags_t *>(ef_id);

	// Check parameters
	if ((ef == NULL) || (ef->id != osRtxIdEventFlags)) {
		return NULL;
	}

	return ef->name;
}

/// Set the specified Event Flags.
uint32_t osEventFlagsSet (osEventFlagsId_t ef_id, uint32_t flags)
{
	ThreadDispatcher::Mutex mutex;
	osRtxEventFlags_t *ef = reinterpret_cast<osRtxEventFlags_t *>(ef_id);

	osRtxThread_t      *thread;
	osRtxThread_t      *thread_next;
	osRtxThread_t * thisThread = ThreadDispatcher::instance().thread.run.curr;
	uint32_t          event_flags;
	uint32_t          event_flags0;

	// Check parameters
	if ((ef == NULL) || (ef->id != osRtxIdEventFlags) ||
		((flags & ~(((uint32_t)1U << osRtxEventFlagsLimit) - 1U)) != 0U)) {
		return ((uint32_t)osErrorParameter);
	}

	// Set Event Flags
	event_flags = EventFlagsSet(ef, flags);

	if (IsIrqMode() || IsIrqMasked())
	{
		// Register post ISR processing
		ThreadDispatcher::instance().queuePostProcess(reinterpret_cast<osRtxObject_t *>(ef));
	}
	else
	{
		// Check if Threads are waiting for Event Flags
		thread = ef->thread_list;
		while (thread != NULL)
		{
			thread_next = thread->thread_next;
			event_flags0 = EventFlagsCheck(ef, thread->wait_flags, thread->flags_options);
			if (event_flags0 != 0U)
			{
				if ((thread->flags_options & osFlagsNoClear) == 0U)
				{
					event_flags = event_flags0 & ~thread->wait_flags;
				}
				else
				{
					event_flags = event_flags0;
				}
				osRtxThreadListRemove(thread);
				osRtxThreadWaitExit(thread, event_flags0, false);
			}
			thread = thread_next;
		}
		ThreadDispatcher::instance().dispatch(nullptr);

		if (thisThread->state != osRtxThreadRunning)
		{
			// scheduler decided to run another thread
			ThreadDispatcher::instance().blockUntilWoken();
		}
	}

	return event_flags;
}

/// Clear the specified Event Flags.
uint32_t osEventFlagsClear (osEventFlagsId_t ef_id, uint32_t flags)
{
	ThreadDispatcher::Mutex mutex;
	osRtxEventFlags_t *ef = reinterpret_cast<osRtxEventFlags_t *>(ef_id);
	uint32_t          event_flags;

	// Check parameters
	if ((ef == NULL) || (ef->id != osRtxIdEventFlags) ||
		((flags & ~(((uint32_t)1U << osRtxEventFlagsLimit) - 1U)) != 0U)) {
		return ((uint32_t)osErrorParameter);
	}

	// Clear Event Flags
	event_flags = EventFlagsClear(ef, flags);

	return event_flags;
}

/// Get the current Event Flags.
uint32_t osEventFlagsGet (osEventFlagsId_t ef_id)
{
	ThreadDispatcher::Mutex mutex;
	osRtxEventFlags_t *ef = reinterpret_cast<osRtxEventFlags_t *>(ef_id);

	// Check parameters
	if ((ef == NULL) || (ef->id != osRtxIdEventFlags)) {
		return 0U;
	}

	return ef->event_flags;
}

/// Wait for one or more Event Flags to become signaled.
uint32_t osEventFlagsWait (osEventFlagsId_t ef_id, uint32_t flags, uint32_t options, uint32_t timeout)
{
	ThreadDispatcher::Mutex mutex;
	osRtxEventFlags_t *ef = reinterpret_cast<osRtxEventFlags_t *>(ef_id);

	osRtxThread_t     *thread;
	uint32_t          event_flags;

	// Check parameters
	if ((ef == NULL) || (ef->id != osRtxIdEventFlags) ||
		((flags & ~(((uint32_t)1U << osRtxEventFlagsLimit) - 1U)) != 0U)) {
		return ((uint32_t)osErrorParameter);
	}

	// Check Event Flags
	event_flags = EventFlagsCheck(ef, flags, options);
	if (event_flags == 0U)
	{
		if (IsIrqMode() || IsIrqMasked())
		{
			// IRQ active?  Don't block, just act like timeout is 0.
			return osErrorResource;
		}

		// Check if timeout is specified
		if (timeout != 0U) {
			// Suspend current Thread
			osRtxThreadWaitEnter(osRtxThreadWaitingEventFlags, timeout);
			thread = ThreadDispatcher::instance().thread.run.curr;
			osRtxThreadListPut(reinterpret_cast<osRtxObject_t *>(ef), thread);
			// Store waiting flags and options
			thread->wait_flags = flags;
			thread->flags_options = (uint8_t)options;

			ThreadDispatcher::instance().blockUntilWoken();

			if(thread->waitValPresent)
			{
				event_flags = static_cast<uint32_t>(thread->waitExitVal);
				thread->waitValPresent = false;
			}
			else
			{
				event_flags = (uint32_t)osErrorTimeout;
			}

			event_flags = (uint32_t)osErrorTimeout;
		}
		else
		{
			event_flags = (uint32_t)osErrorResource;
		}
	}

	return event_flags;
}

/// Delete an Event Flags object.
osStatus_t osEventFlagsDelete (osEventFlagsId_t ef_id)
{
	ThreadDispatcher::Mutex mutex;
	osRtxEventFlags_t *ef = reinterpret_cast<osRtxEventFlags_t *>(ef_id);

	osRtxThread_t * thread;
	osRtxThread_t * thisThread = ThreadDispatcher::instance().thread.run.curr;

	// Check parameters
	if ((ef == NULL) || (ef->id != osRtxIdEventFlags)) {
		return osErrorParameter;
	}

	// Unblock waiting threads
	if (ef->thread_list != NULL)
	{
		do {
			thread = osRtxThreadListGet(reinterpret_cast<osRtxObject_t *>(ef));
			osRtxThreadWaitExit(thread, (uint32_t)osErrorResource, false);
		} while (ef->thread_list != NULL);
		ThreadDispatcher::instance().dispatch(NULL);
	}

	// Mark object as invalid
	ef->id = osRtxIdInvalid;

	// Free object memory
	delete ef;

	if (thisThread->state != osRtxThreadRunning)
	{
		// scheduler decided to run another thread
		ThreadDispatcher::instance().blockUntilWoken();
	}

	return osOK;
}