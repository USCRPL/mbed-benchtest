
#include "rtxoff_internal.h"
#include "ThreadDispatcher.h"

/// Release Mutexes owned by thread.  Called when owner Thread terminates.
/// \param[in]  mutex_list      mutex list.
void osRtxMutexOwnerRelease (osRtxMutex_t *mutex_list)
{
	osRtxMutex_t  *mutex;
	osRtxMutex_t  *mutex_next;
	osRtxThread_t *thread;

	mutex = mutex_list;
	while (mutex != nullptr)
	{
		mutex_next = mutex->owner_next;
		// Check if Mutex is Robust
		if ((mutex->attr & osMutexRobust) != 0U) {
			// Clear Lock counter
			mutex->lock = 0U;
			// Check if Thread is waiting for a Mutex
			if (mutex->thread_list != nullptr) {
				// Wakeup waiting Thread with highest Priority
				thread = osRtxThreadListGet(reinterpret_cast<osRtxObject_t *>(mutex));
				osRtxThreadWaitExit(thread, (uint32_t)osOK, FALSE);
				// Thread is the new Mutex owner
				mutex->owner_thread = thread;
				mutex->owner_prev   = nullptr;
				mutex->owner_next   = thread->mutex_list;
				if (thread->mutex_list != nullptr) {
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
			if ((thread0 != nullptr) && (thread0->priority > priority)) {
				// Higher priority Thread is waiting for Mutex
				priority = thread0->priority;
			}
			mutex0 = mutex0->owner_next;
		} while (mutex0 != nullptr);
		if (thread->priority != priority) {
			thread->priority = priority;
			osRtxThreadListSort(thread);
		}
	}
}

//  ==== Public API ====

/// Create and Initialize a Mutex object.
osMutexId_t osMutexNew (const osMutexAttr_t *attr)
{
	osRtxMutex_t *mutex = nullptr;
	uint32_t    attr_bits;
	uint8_t     flags = 0;
	const char *name;

	// Process attributes
	if (attr != nullptr)
	{
		name      = attr->name;
		attr_bits = attr->attr_bits;
	} else {
		name      = "<anonymous mutex>";
		attr_bits = 0U;
	}

	mutex = new osRtxMutex_t();

	if (mutex != nullptr) {
		// Initialize control block
		mutex->id           = osRtxIdMutex;
		mutex->flags        = flags;
		mutex->attr         = (uint8_t)attr_bits;
		mutex->name         = name;
		mutex->thread_list  = nullptr;
		mutex->owner_thread = nullptr;
		mutex->owner_prev   = nullptr;
		mutex->owner_next   = nullptr;
		mutex->lock         = 0U;

	}

	return mutex;
}

/// Get name of a Mutex object.
const char *osMutexGetName (osMutexId_t mutex_id)
{
	osRtxMutex_t *mutex = reinterpret_cast<osRtxMutex_t *>(mutex_id);

	// Check parameters
	if ((mutex == nullptr) || (mutex->id != osRtxIdMutex)) {
		return nullptr;
	}

	return mutex->name;
}

/// Acquire a Mutex or timeout if it is locked.
osStatus_t osMutexAcquire (osMutexId_t mutex_id, uint32_t timeout)
{
	ThreadDispatcher::Mutex dispMutex;
	osRtxMutex_t *mutex = reinterpret_cast<osRtxMutex_t *>(mutex_id);
	osRtxThread_t *thread;
	osStatus_t   status;

	// Check running thread
	thread = ThreadDispatcher::instance().thread.run.curr;
	if (thread == nullptr) {
		return osError;
	}

	// Check parameters
	if ((mutex == nullptr) || (mutex->id != osRtxIdMutex)) {
		return osErrorParameter;
	}

	// Check if Mutex is not locked
	if (mutex->lock == 0U) {
		// Acquire Mutex
		mutex->owner_thread = thread;
		mutex->owner_prev   = nullptr;
		mutex->owner_next   = thread->mutex_list;
		if (thread->mutex_list != nullptr) {
			thread->mutex_list->owner_prev = mutex;
		}
		thread->mutex_list = mutex;
		mutex->lock = 1U;
		status = osOK;
	} else {
		// Check if Mutex is recursive and running Thread is the owner
		if (((mutex->attr & osMutexRecursive) != 0U) && (mutex->owner_thread == thread))
		{
			// Try to increment lock counter
			if (mutex->lock == osRtxMutexLockLimit) {
				status = osErrorResource;
			} else {
				mutex->lock++;
				status = osOK;
			}
		}
		else
		{
			// Check if timeout is specified
			if (timeout != 0U)
			{
				// Check if Priority inheritance protocol is enabled
				if ((mutex->attr & osMutexPrioInherit) != 0U) {
					// Raise priority of owner Thread if lower than priority of running Thread
					if (mutex->owner_thread->priority < thread->priority) {
						mutex->owner_thread->priority = thread->priority;
						osRtxThreadListSort(mutex->owner_thread);
					}
				}

				// Suspend current Thread
				osRtxThreadWaitEnter(osRtxThreadWaitingMutex, timeout);
				osRtxThreadListPut(reinterpret_cast<osRtxObject_t *>(mutex), thread);

				ThreadDispatcher::instance().blockUntilWoken();

				if(thread->waitValPresent)
				{
					status = static_cast<osStatus_t>(thread->waitExitVal);
					thread->waitValPresent = false;
				}
				else
				{
					status = osErrorTimeout;
				}
			}
			else
			{
				status = osErrorResource;
			}
		}
	}

	return status;
}

/// Release a Mutex that was acquired by \ref osMutexAcquire.
osStatus_t osMutexRelease (osMutexId_t mutex_id)
{
	ThreadDispatcher::Mutex dispMutex;
	osRtxMutex_t *mutex = reinterpret_cast<osRtxMutex_t *>(mutex_id);
	const osRtxMutex_t  *mutex0;
	osRtxThread_t *thisThread;
	int8_t       priority;

	// Check running thread
	thisThread = ThreadDispatcher::instance().thread.run.curr;
	if (thisThread == nullptr) {
		return osError;
	}

	// Check parameters
	if ((mutex == nullptr) || (mutex->id != osRtxIdMutex)) {
		return osErrorParameter;
	}

	// Check if Mutex is not locked
	if (mutex->lock == 0U) {
		return osErrorResource;
	}

	// Check if running Thread is not the owner
	if (mutex->owner_thread != thisThread) {
		return osErrorResource;
	}

	// Decrement Lock counter
	mutex->lock--;

	// Check Lock counter
	if (mutex->lock == 0U) {

		// Remove Mutex from Thread owner list
		if (mutex->owner_next != nullptr) {
			mutex->owner_next->owner_prev = mutex->owner_prev;
		}
		if (mutex->owner_prev != nullptr) {
			mutex->owner_prev->owner_next = mutex->owner_next;
		} else {
			thisThread->mutex_list = mutex->owner_next;
		}

		// Restore running Thread priority
		if ((mutex->attr & osMutexPrioInherit) != 0U) {
			priority = thisThread->priority_base;
			mutex0   = thisThread->mutex_list;
			// Check mutexes owned by running Thread
			while (mutex0 != nullptr) {
				if ((mutex0->thread_list != nullptr) && (mutex0->thread_list->priority > priority)) {
					// Higher priority Thread is waiting for Mutex
					priority = mutex0->thread_list->priority;
				}
				mutex0 = mutex0->owner_next;
			}
			thisThread->priority = priority;
		}

		// Check if Thread is waiting for a Mutex
		if (mutex->thread_list != nullptr) {
			// Wakeup waiting Thread with highest Priority
			osRtxThread_t * nextOwner = osRtxThreadListGet(reinterpret_cast<osRtxObject_t *>(mutex));
			osRtxThreadWaitExit(nextOwner, (uint32_t)osOK, false);

			// Thread is the new Mutex owner
			mutex->owner_thread = nextOwner;
			mutex->owner_prev   = nullptr;
			mutex->owner_next   = nextOwner->mutex_list;
			if (nextOwner->mutex_list != nullptr) {
				nextOwner->mutex_list->owner_prev = mutex;
			}
			nextOwner->mutex_list = mutex;
			mutex->lock = 1U;
		}

		// at this point a new thread might potentially take over
		ThreadDispatcher::instance().dispatch(nullptr);
		if(thisThread->state != osRtxThreadRunning)
		{
			// other thread has higher priority, switch to it
			ThreadDispatcher::instance().blockUntilWoken();
		}
	}

	return osOK;
}
