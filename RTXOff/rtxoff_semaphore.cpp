//
// RTXOff semaphore functionality
//

#include "ThreadDispatcher.h"

//  ==== Helper functions ====

/// Decrement Semaphore tokens.
/// \param[in]  semaphore       semaphore object.
/// \return 1 - success, 0 - failure.
static uint32_t SemaphoreTokenDecrement (osRtxSemaphore_t *semaphore)
{
	uint32_t ret;

	if (semaphore->tokens != 0U) {
		semaphore->tokens--;
		ret = 1U;
	} else {
		ret = 0U;
	}

	return ret;
}

/// Increment Semaphore tokens.
/// \param[in]  semaphore       semaphore object.
/// \return 1 - success, 0 - failure.
static uint32_t SemaphoreTokenIncrement (osRtxSemaphore_t *semaphore) {
	uint32_t ret;

	if (semaphore->tokens < semaphore->max_tokens) {
		semaphore->tokens++;
		ret = 1U;
	} else {
		ret = 0U;
	}

	return ret;
}

//  ==== Post ISR processing ====

/// Semaphore post ISR processing.
/// Called in the scheduler context after we exit ISR mode.
/// \param[in]  semaphore       semaphore object.
static void osRtxSemaphorePostProcess (osRtxSemaphore_t *semaphore) {
	osRtxThread_t *thread;

	// Check if Thread is waiting for a token
	if (semaphore->thread_list != nullptr) {
		// Try to acquire token
		if (SemaphoreTokenDecrement(semaphore) != 0U) {
			// Wakeup waiting Thread with highest Priority
			thread = osRtxThreadListGet(reinterpret_cast<osRtxObject_t *>((semaphore)));
			osRtxThreadWaitExit(thread, (uint32_t)osOK, false);
		}
	}
}

//  ==== Public API ====

/// Create and Initialize a Semaphore object.
osSemaphoreId_t osSemaphoreNew (uint32_t max_count, uint32_t initial_count, const osSemaphoreAttr_t *attr)
{
	if (IsIrqMode() || IsIrqMasked()) {
		return nullptr;
	}

	ThreadDispatcher::Mutex mutex;

	osRtxSemaphore_t *semaphore = nullptr;
	uint8_t         flags = 0;
	const char     *name;

	// Check parameters
	if ((max_count == 0U) || (max_count > osRtxSemaphoreTokenLimit) || (initial_count > max_count)) {
		return nullptr;
	}

	// Process attributes
	if (attr != nullptr)
	{
		name      = attr->name;
	}
	else
	{
		name      = "<anonymous semaphore>";
	}

	semaphore = new osRtxSemaphore_t();

	// Initialize control block
	semaphore->id          = osRtxIdSemaphore;
	semaphore->flags       = flags;
	semaphore->name        = name;
	semaphore->thread_list = nullptr;
	semaphore->tokens      = (uint16_t)initial_count;
	semaphore->max_tokens  = (uint16_t)max_count;

	// Register post ISR processing function
	ThreadDispatcher::instance().post_process.semaphore = osRtxSemaphorePostProcess;

	return semaphore;
}

/// Get name of a Semaphore object.
const char *osSemaphoreGetName (osSemaphoreId_t semaphore_id) {
	if (IsIrqMode() || IsIrqMasked()) {
		return nullptr;
	}

	osRtxSemaphore_t * semaphore = reinterpret_cast<osRtxSemaphore_t *>(semaphore_id);

	// Check parameters
	if ((semaphore == nullptr) || (semaphore->id != osRtxIdSemaphore)) {
		return nullptr;
	}

	return semaphore->name;
}

/// Acquire a Semaphore token or timeout if no tokens are available.
osStatus_t osSemaphoreAcquire (osSemaphoreId_t semaphore_id, uint32_t timeout)
{
	osRtxSemaphore_t * semaphore = reinterpret_cast<osRtxSemaphore_t *>(semaphore_id);
	ThreadDispatcher::Mutex mutex;

	osStatus_t      status;

	// Check parameters
	if ((semaphore == nullptr) || (semaphore->id != osRtxIdSemaphore))
	{
		return osErrorParameter;
	}

	// Try to acquire token
	if (SemaphoreTokenDecrement(semaphore) != 0U)
	{
		status = osOK;
	}
	else
	{
		// No token available

		if (IsIrqMode() || IsIrqMasked())
		{
			// IRQ active?  Don't block, just act like timeout is 0.
			return osErrorResource;
		}

		if (timeout != 0U)
		{
			osRtxThread_t * thisThread = ThreadDispatcher::instance().thread.run.curr;

			// Suspend current Thread
			osRtxThreadWaitEnter(osRtxThreadWaitingSemaphore, timeout);
			osRtxThreadListPut(reinterpret_cast<osRtxObject_t *>(semaphore), thisThread);

			ThreadDispatcher::instance().blockUntilWoken();

			if(thisThread->waitValPresent)
			{
				status = static_cast<osStatus_t>(thisThread->waitExitVal);
				thisThread->waitValPresent = false;
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

	return status;
}

/// Release a Semaphore token that was acquired by osSemaphoreAcquire.
osStatus_t osSemaphoreRelease (osSemaphoreId_t semaphore_id)
{
	osRtxSemaphore_t * semaphore = reinterpret_cast<osRtxSemaphore_t *>(semaphore_id);
	ThreadDispatcher::Mutex mutex;

	osRtxThread_t * thread;
	osRtxThread_t * thisThread = ThreadDispatcher::instance().thread.run.curr;
	osStatus_t      status;

	// Check parameters
	if ((semaphore == nullptr) || (semaphore->id != osRtxIdSemaphore)) {
		return osErrorParameter;
	}

	if(IsIrqMode() || IsIrqMasked())
	{
		// Try to release token
		if (SemaphoreTokenIncrement(semaphore) != 0U)
		{
			// Register post ISR processing
			ThreadDispatcher::instance().queuePostProcess(reinterpret_cast<osRtxObject_t *>(semaphore));
			status = osOK;
		} else {
			status = osErrorResource;
		}
	}
	else
	{
		// Check if Thread is waiting for a token
		if (semaphore->thread_list != nullptr)
		{
			// Wakeup waiting Thread with highest Priority
			thread = osRtxThreadListGet(reinterpret_cast<osRtxObject_t *>(semaphore));
			osRtxThreadWaitExit(thread, (uint32_t)osOK, true);

			if(thisThread->state != osRtxThreadRunning)
			{
				// other thread has higher priority, switch to it
				ThreadDispatcher::instance().blockUntilWoken();
			}

			status = osOK;
		} else {
			// Try to release token
			if (SemaphoreTokenIncrement(semaphore) != 0U) {
				status = osOK;
			} else {
				status = osErrorResource;
			}
		}
	}


	return status;
}

/// Get current Semaphore token count.
uint32_t osSemaphoreGetCount (osSemaphoreId_t semaphore_id) {
	osRtxSemaphore_t * semaphore = reinterpret_cast<osRtxSemaphore_t *>(semaphore_id);

	// Check parameters
	if ((semaphore == nullptr) || (semaphore->id != osRtxIdSemaphore)) {
		return 0U;
	}

	return semaphore->tokens;
}

/// Delete a Semaphore object.
osStatus_t osSemaphoreDelete (osSemaphoreId_t semaphore_id)
{
	if(IsIrqMode() || IsIrqMasked())
	{
		return osErrorISR;
	}

	osRtxSemaphore_t * semaphore = reinterpret_cast<osRtxSemaphore_t *>(semaphore_id);
	ThreadDispatcher::Mutex mutex;

	osRtxThread_t    *thread;
	osRtxThread_t * thisThread = ThreadDispatcher::instance().thread.run.curr;

	// Check parameters
	if ((semaphore == nullptr) || (semaphore->id != osRtxIdSemaphore)) {
		return osErrorParameter;
	}

	// Unblock waiting threads
	if (semaphore->thread_list != nullptr)
	{
		do {
			thread = osRtxThreadListGet(reinterpret_cast<osRtxObject_t *>(semaphore));
			osRtxThreadWaitExit(thread, (uint32_t)osErrorResource, false);
		} while (semaphore->thread_list != nullptr);
		ThreadDispatcher::instance().dispatch(nullptr);
	}

	// Mark object as invalid
	semaphore->id = osRtxIdInvalid;

	// Free object memory
	delete semaphore;

	if (thisThread->state != osRtxThreadRunning)
	{
		// scheduler decided to run another thread
		ThreadDispatcher::instance().blockUntilWoken();
	}

	return osOK;
}
