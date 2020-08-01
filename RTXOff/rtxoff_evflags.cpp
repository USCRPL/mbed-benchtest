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
		//ThreadDispatcher::instance().post_process.event_flags = osRtxEventFlagsPostProcess;

	}

	return ef;
}