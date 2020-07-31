
#include "rtxoff_internal.h"

/// Release Mutexes owned by thread.  Called when owner Thread terminates.
/// \param[in]  mutex_list      mutex list.
void osRtxMutexOwnerRelease (osRtxMutex_t *mutex_list)
{
	osRtxMutex_t  *mutex;
	osRtxMutex_t  *mutex_next;
	osRtxThread_t *thread;

	mutex = mutex_list;
	while (mutex != NULL)
	{
		mutex_next = mutex->owner_next;
		// Check if Mutex is Robust
		if ((mutex->attr & osMutexRobust) != 0U) {
			// Clear Lock counter
			mutex->lock = 0U;
			// Check if Thread is waiting for a Mutex
			if (mutex->thread_list != NULL) {
				// Wakeup waiting Thread with highest Priority
				thread = osRtxThreadListGet(reinterpret_cast<osRtxObject_t *>(mutex));
				osRtxThreadWaitExit(thread, (uint32_t)osOK, FALSE);
				// Thread is the new Mutex owner
				mutex->owner_thread = thread;
				mutex->owner_prev   = NULL;
				mutex->owner_next   = thread->mutex_list;
				if (thread->mutex_list != NULL) {
					thread->mutex_list->owner_prev = mutex;
				}
				thread->mutex_list = mutex;
				mutex->lock = 1U;
			}
		}
		mutex = mutex_next;
	}
}

/// Give this thread the highest priority inherited from any of its mutexes.
/// \param[in]  mutex           mutex object.
/// \param[in]  thread_wakeup   thread wakeup object.
void osRtxMutexOwnerRestore (const osRtxMutex_t *mutex, const osRtxThread_t *thread_wakeup)
{
	const osRtxMutex_t  *mutex0;
	osRtxThread_t *thread;
	osRtxThread_t *thread0;
	int8_t       priority;

	// Restore owner Thread priority
	if ((mutex->attr & osMutexPrioInherit) != 0U)
	{
		thread   = mutex->owner_thread;
		priority = thread->priority_base;
		mutex0   = thread->mutex_list;
		// Check Mutexes owned by Thread
		do {
			// Check Threads waiting for Mutex
			thread0 = mutex0->thread_list;
			if (thread0 == thread_wakeup) {
				// Skip thread that is waken-up
				thread0 = thread0->thread_next;
			}
			if ((thread0 != NULL) && (thread0->priority > priority)) {
				// Higher priority Thread is waiting for Mutex
				priority = thread0->priority;
			}
			mutex0 = mutex0->owner_next;
		} while (mutex0 != NULL);
		if (thread->priority != priority) {
			thread->priority = priority;
			osRtxThreadListSort(thread);
		}
	}
}
