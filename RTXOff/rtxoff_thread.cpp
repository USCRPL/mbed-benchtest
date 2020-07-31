//
// Implementations for RTXOff thread functions
//

#include "rtxoff_internal.h"
#include "ThreadDispatcher.h"

//  ==== Helper functions ====

/// Set Thread Flags.
/// \param[in]  thread          thread object.
/// \param[in]  flags           specifies the flags to set.
/// \return thread flags after setting.
static uint32_t ThreadFlagsSet (osRtxThread_t *thread, uint32_t flags) {
	uint32_t thread_flags;

	thread->thread_flags |= flags;
	thread_flags = thread->thread_flags;

	return thread_flags;
}

/// Clear Thread Flags.
/// \param[in]  thread          thread object.
/// \param[in]  flags           specifies the flags to clear.
/// \return thread flags before clearing.
static uint32_t ThreadFlagsClear (osRtxThread_t *thread, uint32_t flags) {

	uint32_t thread_flags;
	thread_flags = thread->thread_flags;
	thread->thread_flags &= ~flags;

	return thread_flags;
}

/// Check Thread Flags.
/// \param[in]  thread          thread object.
/// \param[in]  flags           specifies the flags to check.
/// \param[in]  options         specifies flags options (osFlagsXxxx).
/// \return thread flags before clearing or 0 if specified flags have not been set.
static uint32_t ThreadFlagsCheck (osRtxThread_t *thread, uint32_t flags, uint32_t options) {
	uint32_t thread_flags;

	if ((options & osFlagsNoClear) == 0U) {

		thread_flags = thread->thread_flags;
		if ((((options & osFlagsWaitAll) != 0U) && ((thread_flags & flags) != flags)) ||
			(((options & osFlagsWaitAll) == 0U) && ((thread_flags & flags) == 0U))) {
			thread_flags = 0U;
		} else {
			thread->thread_flags &= ~flags;
		}


	} else {
		thread_flags = thread->thread_flags;
		if ((((options & osFlagsWaitAll) != 0U) && ((thread_flags & flags) != flags)) ||
			(((options & osFlagsWaitAll) == 0U) && ((thread_flags & flags) == 0U))) {
			thread_flags = 0U;
		}
	}

	return thread_flags;
}



//  ==== Library functions ====

// Debugging function: print a linked list of threads
void printThreadLL(osRtxThread_t * head)
{
	std::cerr << "[";
	while (head != nullptr)
	{
		std::cerr << head->name;
		if(head->thread_next != nullptr)
		{
			std::cerr << ", ";
		}
		head = head->thread_next;
	}
	std::cerr << "]" << std::endl;
}

/// Put a Thread into specified Object list sorted by Priority (Highest at Head).
/// \param[in]  object          generic object.
/// \param[in]  thread          thread object.
void osRtxThreadListPut (osRtxObject_t *object, osRtxThread_t *thread) {
	osRtxThread_t *prev, *next;
	int32_t      priority;

	priority = thread->priority;

	prev = reinterpret_cast<osRtxThread_t *>(object);
	next = prev->thread_next;
	while ((next != NULL) && (next->priority >= priority)) {
		prev = next;
		next = next->thread_next;
	}
	thread->thread_prev = prev;
	thread->thread_next = next;
	prev->thread_next = thread;
	if (next != NULL) {
		next->thread_prev = thread;
	}
}

/// Re-sort a Thread in linked Object list by Priority (Highest at Head).
/// \param[in]  thread          thread object.
void osRtxThreadListSort (osRtxThread_t *thread)
{
	osRtxObject_t *object;
	osRtxThread_t *thread0;

	// Search for head of list
	thread0 = thread;
	while ((thread0 != NULL) && (thread0->id == osRtxIdThread)) {
		thread0 = thread0->thread_prev;
	}
	object = reinterpret_cast<osRtxObject_t *>(thread0);

	if (object != NULL) {
		osRtxThreadListRemove(thread);
		osRtxThreadListPut(object, thread);
	}
}


/// Get a Thread with Highest Priority from specified Object list and remove it.
/// \param[in]  object          generic object.
/// \return thread object.
osRtxThread_t *osRtxThreadListGet (osRtxObject_t *object) {
	osRtxThread_t *thread;

	thread = object->thread_list;
	object->thread_list = thread->thread_next;
	if (thread->thread_next != nullptr) {
		thread->thread_next->thread_prev = reinterpret_cast<osRtxThread_t *>(object);
	}
	thread->thread_prev = nullptr;

	return thread;
}

/// Remove a Thread from linked Object list.
/// \param[in]  thread          thread object.
void osRtxThreadListRemove (osRtxThread_t *thread) {

	if (thread->thread_prev != NULL) {
		thread->thread_prev->thread_next = thread->thread_next;
		if (thread->thread_next != NULL) {
			thread->thread_next->thread_prev = thread->thread_prev;
		}
		thread->thread_prev = NULL;
	}
}

/// Block running Thread execution and register it as Ready to Run.
/// NOTE: this is exactly the same as osRtxThreadReadyPut(), except that this adds the thread
/// to the FRONT of the queue at this priority instead of the back.
/// \param[in]  thread          running thread object.
void osRtxThreadBlock (osRtxThread_t *thread) {
	osRtxThread_t *prev, *next;
	int32_t      priority;

	thread->state = osRtxThreadReady;

	priority = thread->priority;

	prev = reinterpret_cast<osRtxThread_t *>(&ThreadDispatcher::instance().thread.ready);
	next = prev->thread_next;

	// find point in linked list to insert this thread
	while ((next != nullptr) && (next->priority > priority)) {
		prev = next;
		next = next->thread_next;
	}

	// insert to LL
	thread->thread_prev = prev;
	thread->thread_next = next;
	prev->thread_next = thread;
	if (next != NULL) {
		next->thread_prev = thread;
	}
}

/// Unlink a Thread from specified linked list.
/// \param[in]  thread          thread object.
void osRtxThreadListUnlink (osRtxThread_t **thread_list, osRtxThread_t *thread) {

	if (thread->thread_next != NULL) {
		thread->thread_next->thread_prev = thread->thread_prev;
	}
	if (thread->thread_prev != NULL) {
		thread->thread_prev->thread_next = thread->thread_next;
		thread->thread_prev = NULL;
	} else {
		*thread_list = thread->thread_next;
	}
}

/// Free Thread resources.
/// \param[in]  thread          thread object.
static void osRtxThreadFree (osRtxThread_t *thread)
{
	// Mark object as inactive and invalid
	thread->state = osRtxThreadInactive;
	thread->id    = osRtxIdInvalid;

	// remove OS thread handle
#if USE_WINTHREAD
	CloseHandle(thread->osThread);
#else
#error TODO
#endif

	// delete thread object itself
	delete thread;
}

/// Mark a Thread as Ready and put it into Ready list (sorted by Priority).
/// \param[in]  thread          thread object.
void osRtxThreadReadyPut (osRtxThread_t * thread)
{
#if RTXOFF_DEBUG
	std::cerr << "Putting " << thread->name << " onto ready list" << std::endl;
#endif
	thread->state = osRtxThreadReady;
	osRtxThreadListPut(&ThreadDispatcher::instance().thread.ready, thread);
}

void *osRtxThreadListRoot (osRtxThread_t *thread)
{
	osRtxThread_t *thread0;

	thread0 = thread;
	while (thread0->id == osRtxIdThread) {
		thread0 = thread0->thread_prev;
	}
	return thread0;
}

/// Exit Thread wait state.
/// \param[in]  thread          thread object.
/// \param[in]  ret_val         return value.
/// \param[in]  dispatch        dispatch flag.
void osRtxThreadWaitExit(osRtxThread_t *thread, uint32_t ret_val, bool dispatch)
{
	ThreadDispatcher::instance().delayListRemove(thread);
	thread->waitExitVal = ret_val;
	thread->waitValPresent = 1;
	if (dispatch)
	{
		ThreadDispatcher::instance().dispatch(thread);
	}
	else
	{
		osRtxThreadReadyPut(thread);
	}
}

/// Send the running thread into the wait state
/// \param[in]  state           new thread state.
/// \param[in]  timeout         timeout.
/// \return true - success, false - failure.
bool osRtxThreadWaitEnter (uint8_t state, uint32_t timeout) {
	osRtxThread_t *thread;

	// Check if Kernel is running
	if (ThreadDispatcher::instance().kernel.state != osRtxKernelRunning) {
		return false;
	}

	// Check if any thread is ready
	if (ThreadDispatcher::instance().thread.ready.thread_list == NULL) {
		return false;
	}

	// Get running thread
	thread = ThreadDispatcher::instance().thread.run.curr;

	thread->state = state;
	ThreadDispatcher::instance().delayListInsert(thread, timeout);
	osRtxThread_t * newThread = osRtxThreadListGet(&ThreadDispatcher::instance().thread.ready);
	ThreadDispatcher::instance().switchNextThread(newThread);

#if RTXOFF_DEBUG
	std::cerr << thread->name << " is waiting for " << timeout << " ticks (currently at tick " << ThreadDispatcher::instance().kernel.tick <<
		"), switching to " << newThread->name << " in the meantime" << std::endl;
#endif

	return true;
}

osThreadId_t osThreadNew (osThreadFunc_t func, void *argument, const osThreadAttr_t *attr)
{
	if (func == NULL) {
		return NULL;
	}

	ThreadDispatcher::Mutex mutex;

	osRtxThread_t * callingThread = ThreadDispatcher::instance().thread.run.curr;
	osRtxThread_t *thread = nullptr;

	uint32_t      attr_bits;
	osPriority_t  priority;
	uint8_t       flags = 0;
	const char   *name;

	// Process attributes
	if (attr != NULL)
	{
		name       = attr->name;
		attr_bits  = attr->attr_bits;
		priority   = attr->priority;
		if (priority == osPriorityNone)
		{
			priority = osPriorityNormal;
		} else {
			if ((priority < osPriorityIdle) || (priority > osPriorityISR)) {
				return NULL;
			}
		}
	} else {
		name       = NULL;
		attr_bits  = 0U;
		priority   = osPriorityNormal;
	}

	// make sure all threads have names (makes debugging easier)
	if(name == nullptr)
	{
		name = "<anonymous thread>";
	}

	// create object memory
	thread = new osRtxThread_t();

	// Initialize control block
	thread->id            = osRtxIdThread;
	thread->state         = osRtxThreadReady;
	thread->flags         = flags;
	thread->attr          = (uint8_t)attr_bits;
	thread->name          = name;
	thread->thread_next   = NULL;
	thread->thread_prev   = NULL;
	thread->delay_next    = NULL;
	thread->delay_prev    = NULL;
	thread->thread_join   = NULL;
	thread->delay         = 0U;
	thread->priority      = (int8_t)priority;
	thread->priority_base = (int8_t)priority;
	thread->flags_options = 0U;
	thread->wait_flags    = 0U;
	thread->thread_flags  = 0U;
	thread->mutex_list    = NULL;
	thread->waitValPresent = 0;

	// Create OS thread
#if USE_WINTHREAD
	thread->osThread = CreateThread(nullptr,
			0,
			reinterpret_cast<LPTHREAD_START_ROUTINE>(func),
			argument,
			CREATE_SUSPENDED,
			nullptr);

	if(thread->osThread == nullptr)
	{
		std::cerr << "Error creating thread: " << std::system_category().message(GetLastError()) << std::endl;
		return nullptr;
	}
#else
#error TODO
#endif

	//std::cerr << "Before creating thread " << name << std::endl;
	//std::cerr << "Ready list is: ";
	//printThreadLL(reinterpret_cast<osRtxThread_t *>(ThreadDispatcher::instance().thread.ready.thread_list));

	ThreadDispatcher::instance().dispatch(thread);

#if RTXOFF_DEBUG
	std::cerr << "Created thread " << name << std::endl;
	std::cerr << "Ready list is now: ";
	printThreadLL(reinterpret_cast<osRtxThread_t *>(ThreadDispatcher::instance().thread.ready.thread_list));
#endif

	if(ThreadDispatcher::instance().kernel.state == osRtxKernelRunning
		&& callingThread->state != osThreadRunning)
	{
		// the new thread we started is now running instead of this thread, so switch to it
		ThreadDispatcher::instance().blockUntilWoken();
	}

	return thread;
}

/// Get name of a thread.
const char *osThreadGetName (osThreadId_t thread_id)
{
	osRtxThread_t *thread = reinterpret_cast<osRtxThread_t *>(thread_id);

	// Check parameters
	if ((thread == nullptr) || (thread->id != osRtxIdThread)) {
		return nullptr;
	}
	return thread->name;
}

/// Return the thread ID of the current running thread.
osThreadId_t osThreadGetId()
{
	ThreadDispatcher::Mutex mutex;
	return reinterpret_cast<osThreadId_t>(ThreadDispatcher::instance().thread.run.curr);
}

/// Get current thread state of a thread.
osThreadState_t osThreadGetState(osThreadId_t thread_id)
{
	osRtxThread_t *thread = reinterpret_cast<osRtxThread_t *>(thread_id);

	// Check parameters
	if ((thread == nullptr) || (thread->id != osRtxIdThread)) {
		return osThreadError;
	}
	return static_cast<osThreadState_t>(thread->state & osRtxThreadStateMask);
}

/// Change priority of a thread.
osStatus_t osThreadSetPriority (osThreadId_t thread_id, osPriority_t priority)
{
	ThreadDispatcher::Mutex mutex;

	osRtxThread_t *thread = reinterpret_cast<osRtxThread_t *>(thread_id);
	osRtxThread_t * thisThread = ThreadDispatcher::instance().thread.run.curr;

	// Check parameters
	if ((thread == NULL) || (thread->id != osRtxIdThread) ||
		(priority < osPriorityIdle) || (priority > osPriorityISR)) {
		return osErrorParameter;
	}

	// Check object state
	if (thread->state == osRtxThreadTerminated) {
		return osErrorResource;
	}

	if (thread->priority   != (int8_t)priority)
	{
		thread->priority      = (int8_t)priority;
		thread->priority_base = (int8_t)priority;
		osRtxThreadListSort(thread);
		ThreadDispatcher::instance().dispatch(nullptr);

#if RTXOFF_DEBUG
		std::cerr << "Changed priority of " << thread->name << " to " << priority << ", now executing " << ThreadDispatcher::instance().thread.run.curr->name << std::endl;
#endif

		if(thisThread->state != osRtxThreadRunning)
		{
			// other thread has higher priority, switch to it
			ThreadDispatcher::instance().blockUntilWoken();
		}
	}

	return osOK;
}

/// Get current priority of a thread.
osPriority_t osThreadGetPriority (osThreadId_t thread_id)
{
	ThreadDispatcher::Mutex mutex;

	osRtxThread_t *thread = reinterpret_cast<osRtxThread_t *>(thread_id);

	// Check parameters
	if ((thread == NULL) || (thread->id != osRtxIdThread)){
		return osPriorityError;
	}

	// Check object state
	if (thread->state == osRtxThreadTerminated) {
		return osPriorityError;
	}

	return static_cast<osPriority_t>(thread->priority);
}

/// Pass control to next thread that is in state READY.
osStatus_t osThreadYield (void) {
	ThreadDispatcher::Mutex mutex;

	osRtxThread_t *thread_running;
	osRtxThread_t *thread_ready;

	if (ThreadDispatcher::instance().kernel.state == osRtxKernelRunning) {
		thread_running = ThreadDispatcher::instance().thread.run.curr;
		thread_ready   = ThreadDispatcher::instance().thread.ready.thread_list;
		if ((thread_ready != NULL) &&
			(thread_ready->priority == thread_running->priority))
		{
			osRtxThreadListRemove(thread_ready);
			osRtxThreadReadyPut(thread_running);
			ThreadDispatcher::instance().switchNextThread(thread_ready);
			ThreadDispatcher::instance().blockUntilWoken();
		}
	}

	return osOK;
}

/// Suspend execution of a thread.
osStatus_t osThreadSuspend (osThreadId_t thread_id)
{
	ThreadDispatcher::Mutex mutex;
	osRtxThread_t *thread = reinterpret_cast<osRtxThread_t *>(thread_id);
	osStatus_t   status;

	// Check parameters
	if ((thread == NULL) || (thread->id != osRtxIdThread)) {
		return osErrorParameter;
	}

	// Check object state
	switch (thread->state & osRtxThreadStateMask) {
		case osRtxThreadRunning:
			if ((ThreadDispatcher::instance().kernel.state != osRtxKernelRunning) ||
				(ThreadDispatcher::instance().thread.ready.thread_list == NULL))
			{
				status = osErrorResource;
			} else {
				status = osOK;
			}
			break;
		case osRtxThreadReady:
			osRtxThreadListRemove(thread);
			status = osOK;
			break;
		case osRtxThreadBlocked:
			osRtxThreadListRemove(thread);
			ThreadDispatcher::instance().delayListRemove(thread);
			status = osOK;
			break;
		case osRtxThreadInactive:
		case osRtxThreadTerminated:
		default:
			status = osErrorResource;
			break;
	}

	if (status == osOK)
	{
		bool suspendingSelf = thread->state == osRtxThreadRunning;

		if (suspendingSelf) {
			ThreadDispatcher::instance().switchNextThread(osRtxThreadListGet(&ThreadDispatcher::instance().thread.ready));
		}

		// Update Thread State and put it into Delay list
		thread->state = osRtxThreadBlocked;
		thread->thread_prev = NULL;
		thread->thread_next = NULL;
		ThreadDispatcher::instance().delayListInsert(thread, osWaitForever);

		if(suspendingSelf)
		{
			ThreadDispatcher::instance().blockUntilWoken();
		}
	}

	return status;
}

/// Detach a thread (thread storage can be reclaimed when thread terminates).
osStatus_t osThreadDetach (osThreadId_t thread_id)
{
	ThreadDispatcher::Mutex mutex;
	osRtxThread_t *thread = reinterpret_cast<osRtxThread_t *>(thread_id);

	// Check parameters
	if ((thread == NULL) || (thread->id != osRtxIdThread)) {
		return osErrorParameter;
	}

	// Check object attributes
	if ((thread->attr & osThreadJoinable) == 0U) {
		return osErrorResource;
	}

	if (thread->state == osRtxThreadTerminated) {
		osRtxThreadListUnlink(&ThreadDispatcher::instance().thread.terminate_list, thread);
		osRtxThreadFree(thread);
	} else {
		thread->attr &= ~osThreadJoinable;
	}

	return osOK;
}

/// Wait for specified thread to terminate.
osStatus_t osThreadJoin (osThreadId_t thread_id)
{
	ThreadDispatcher::Mutex mutex;
	osRtxThread_t *thread = reinterpret_cast<osRtxThread_t *>(thread_id);
	osStatus_t   status;

	// Check parameters
	if ((thread == NULL) || (thread->id != osRtxIdThread)) {
		return osErrorParameter;
	}

	// Check object attributes
	if ((thread->attr & osThreadJoinable) == 0U) {
		return osErrorResource;
	}

	// Check object state
	if (thread->state == osRtxThreadRunning) {
		return osErrorResource;
	}

	if (thread->state == osRtxThreadTerminated)
	{
		osRtxThreadListUnlink(&ThreadDispatcher::instance().thread.terminate_list, thread);
		osRtxThreadFree(thread);
		status = osOK;
	}
	else
	{
		// Suspend current Thread
		if (osRtxThreadWaitEnter(osRtxThreadWaitingJoin, osWaitForever))
		{
			thread->thread_join = ThreadDispatcher::instance().thread.run.curr;
			thread->attr &= ~osThreadJoinable;
			ThreadDispatcher::instance().blockUntilWoken();
		}
		status = osErrorResource;
	}

	return status;
}

/// Terminate execution of current running thread.
__NO_RETURN void osThreadExit (void)
{
	ThreadDispatcher::instance().lockMutex();

	osRtxThread_t *thread;

	// Check if switch to next Ready Thread is possible
	if ((ThreadDispatcher::instance().kernel.state != osRtxKernelRunning) ||
		(ThreadDispatcher::instance().thread.ready.thread_list == NULL)) {

		// can't return an error code...
		ThreadDispatcher::instance().unlockMutex();
		for (;;) {}
	}

	// Get running thread
	thread = ThreadDispatcher::instance().thread.run.curr;

	// Release owned Mutexes
	osRtxMutexOwnerRelease(thread->mutex_list);

	// Wakeup Thread waiting to Join
	if (thread->thread_join != NULL) {
		osRtxThreadWaitExit(thread->thread_join, (uint32_t)osOK, FALSE);
	}

	// Switch to next Ready Thread
	ThreadDispatcher::instance().switchNextThread(osRtxThreadListGet(&ThreadDispatcher::instance().thread.ready));
	ThreadDispatcher::instance().thread.run.curr = nullptr;

#if RTXOFF_DEBUG
	std::cerr << "Exited thread " << thread->name << ", switching to " << ThreadDispatcher::instance().thread.run.next->name << std::endl;
#endif

	if ((thread->attr & osThreadJoinable) == 0U)
	{
		// This thread has already been detached.
		osRtxThreadFree(thread);
	}
	else
	{
		// Update Thread State and put it into Terminate Thread list
		thread->state = osRtxThreadTerminated;
		thread->thread_prev = NULL;
		thread->thread_next = ThreadDispatcher::instance().thread.terminate_list;
		if (ThreadDispatcher::instance().thread.terminate_list != NULL) {
			ThreadDispatcher::instance().thread.terminate_list->thread_prev = thread;
		}
		ThreadDispatcher::instance().thread.terminate_list = thread;
	}

	// now trigger the scheduler and exit this thread
	ThreadDispatcher::instance().requestSchedule();
	ThreadDispatcher::instance().unlockMutex();

#if USE_WINTHREAD
	ExitThread(0);
#else
#error TODO
#endif
}

/// Terminate execution of a thread.
osStatus_t osThreadTerminate (osThreadId_t thread_id)
{
	ThreadDispatcher::instance().lockMutex();
	osRtxThread_t *thread = reinterpret_cast<osRtxThread_t *>(thread_id);

	osStatus_t   status;

	// Check parameters
	if ((thread == NULL) || (thread->id != osRtxIdThread))
	{
		ThreadDispatcher::instance().unlockMutex();
		return osErrorParameter;
	}

	// Check object state
	switch (thread->state & osRtxThreadStateMask) {
		case osRtxThreadRunning:
			if ((ThreadDispatcher::instance().kernel.state != osRtxKernelRunning) ||
				(ThreadDispatcher::instance().thread.ready.thread_list == NULL)) {
				status = osErrorResource;
			} else {
				status = osOK;
			}
			break;
		case osRtxThreadReady:
			osRtxThreadListRemove(thread);
			status = osOK;
			break;
		case osRtxThreadBlocked:
			osRtxThreadListRemove(thread);
			ThreadDispatcher::instance().delayListRemove(thread);
			status = osOK;
			break;
		case osRtxThreadInactive:
		case osRtxThreadTerminated:
		default:
			status = osErrorResource;
			break;
	}

	if (status == osOK)
	{
		// Release owned Mutexes
		osRtxMutexOwnerRelease(thread->mutex_list);

		// Wakeup Thread waiting to Join
		if (thread->thread_join != NULL) {
			osRtxThreadWaitExit(thread->thread_join, (uint32_t)osOK, FALSE);
		}

		bool terminatingSelf = thread->state == osRtxThreadRunning;


		// Switch to next Ready Thread when terminating running Thread
		if (terminatingSelf)
		{
			ThreadDispatcher::instance().switchNextThread(osRtxThreadListGet(&ThreadDispatcher::instance().thread.ready));
			ThreadDispatcher::instance().thread.run.curr = nullptr;
		} else {
			ThreadDispatcher::instance().dispatch(nullptr);
		}

		if(!terminatingSelf)
		{
			// terminate the OS thread
#if USE_WINTHREAD
			if(!TerminateThread(thread->osThread, 0))
			{
				std::cerr << "Error terminating RTX thread: " << std::system_category().message(GetLastError()) << std::endl;
			}
#else
#error TODO
#endif
		}

		if ((thread->attr & osThreadJoinable) == 0U)
		{
			osRtxThreadFree(thread);
		}
		else
		{
			// Update Thread State and put it into Terminate Thread list
			thread->state = osRtxThreadTerminated;
			thread->thread_prev = NULL;
			thread->thread_next = ThreadDispatcher::instance().thread.terminate_list;
			if (ThreadDispatcher::instance().thread.terminate_list != NULL) {
				ThreadDispatcher::instance().thread.terminate_list->thread_prev = thread;
			}
			ThreadDispatcher::instance().thread.terminate_list = thread;
		}

		if(terminatingSelf)
		{
			// now trigger the scheduler and exit this thread
			ThreadDispatcher::instance().requestSchedule();
			ThreadDispatcher::instance().unlockMutex();

#if USE_WINTHREAD
			ExitThread(0);
#else
#error TODO
#endif
		}
	}

	ThreadDispatcher::instance().unlockMutex();
	return status;
}

/// Get number of active threads.
uint32_t osThreadGetCount (void)
{
	ThreadDispatcher::Mutex mutex;
	const osRtxThread_t *thread;
	uint32_t count;

	// Running Thread
	count = 1U;

	// Ready List
	for (thread = ThreadDispatcher::instance().thread.ready.thread_list;
		 thread != NULL; thread = thread->thread_next)
	{
		count++;
	}

	// Delay List
	for (thread = ThreadDispatcher::instance().thread.delay_list;
		 thread != NULL; thread = thread->delay_next)
	{
		count++;
	}

	// Wait List
	for (thread = ThreadDispatcher::instance().thread.wait_list;
		 thread != NULL; thread = thread->delay_next)
	{
		count++;
	}

	return count;
}

/// Enumerate active threads.
uint32_t osThreadEnumerate (osThreadId_t *thread_array, uint32_t array_items)
{
	ThreadDispatcher::Mutex mutex;
	osRtxThread_t *thread;
	uint32_t     count;

	// Check parameters
	if ((thread_array == NULL) || (array_items == 0U)) {
		return 0U;
	}

	// Running Thread
	*thread_array = ThreadDispatcher::instance().thread.run.curr;
	thread_array++;
	count = 1U;

	// Ready List
	for (thread = ThreadDispatcher::instance().thread.ready.thread_list;
		 (thread != NULL) && (count < array_items); thread = thread->thread_next) {
		*thread_array = thread;
		thread_array++;
		count++;
	}

	// Delay List
	for (thread = ThreadDispatcher::instance().thread.delay_list;
		 (thread != NULL) && (count < array_items); thread = thread->delay_next) {
		*thread_array = thread;
		thread_array++;
		count++;
	}

	// Wait List
	for (thread = ThreadDispatcher::instance().thread.wait_list;
		 (thread != NULL) && (count < array_items); thread = thread->delay_next) {
		*thread_array = thread;
		thread_array++;
		count++;
	}

	return count;
}

/// Set the specified Thread Flags of a thread.
uint32_t osThreadFlagsSet (osThreadId_t thread_id, uint32_t flags) {
	ThreadDispatcher::Mutex mutex;
	osRtxThread_t *thread = reinterpret_cast<osRtxThread_t *>(thread_id);

	uint32_t     thread_flags;
	uint32_t     thread_flags0;

	// Check parameters
	if ((thread == NULL) || (thread->id != osRtxIdThread) ||
		((flags & ~(((uint32_t)1U << osRtxThreadFlagsLimit) - 1U)) != 0U)) {
		return ((uint32_t)osErrorParameter);
	}

	// Check object state
	if (thread->state == osRtxThreadTerminated) {
		return ((uint32_t)osErrorResource);
	}

	// Set Thread Flags
	thread_flags = ThreadFlagsSet(thread, flags);

	// Check if Thread is waiting for Thread Flags
	if (thread->state == osRtxThreadWaitingThreadFlags) {
		thread_flags0 = ThreadFlagsCheck(thread, thread->wait_flags, thread->flags_options);
		if (thread_flags0 != 0U) {
			if ((thread->flags_options & osFlagsNoClear) == 0U) {
				thread_flags = thread_flags0 & ~thread->wait_flags;
			} else {
				thread_flags = thread_flags0;
			}
			osRtxThreadWaitExit(thread, thread_flags0, true);
		}
	}

	return thread_flags;
}

/// Clear the specified Thread Flags of current running thread.
uint32_t osThreadFlagsClear (uint32_t flags)
{
	ThreadDispatcher::Mutex mutex;
	osRtxThread_t *thread;
	uint32_t     thread_flags;

	// Check running thread
	thread = ThreadDispatcher::instance().thread.run.curr;
	if (thread == NULL) {
		return ((uint32_t)osError);
	}

	// Check parameters
	if ((flags & ~(((uint32_t)1U << osRtxThreadFlagsLimit) - 1U)) != 0U) {
		return ((uint32_t)osErrorParameter);
	}

	// Clear Thread Flags
	thread_flags = ThreadFlagsClear(thread, flags);

	return thread_flags;
}

/// Get the current Thread Flags of current running thread.
uint32_t osThreadFlagsGet (void)
{
	ThreadDispatcher::Mutex mutex;
	const osRtxThread_t *thread;

	// Check running thread
	thread = ThreadDispatcher::instance().thread.run.curr;
	if (thread == NULL) {
		return 0U;
	}

	return thread->thread_flags;
}


/// Wait for one or more Thread Flags of the current running thread to become signaled.
uint32_t osThreadFlagsWait (uint32_t flags, uint32_t options, uint32_t timeout)
{
	osRtxThread_t *thread;
	uint32_t     thread_flags;

	ThreadDispatcher::instance().lockMutex();

	// Check running thread
	thread = ThreadDispatcher::instance().thread.run.curr;
	if (thread == NULL) {
		ThreadDispatcher::instance().unlockMutex();
		return ((uint32_t)osError);
	}

	// Check parameters
	if ((flags & ~(((uint32_t)1U << osRtxThreadFlagsLimit) - 1U)) != 0U) {
		ThreadDispatcher::instance().unlockMutex();
		return ((uint32_t)osErrorParameter);
	}

	// Check Thread Flags
	thread_flags = ThreadFlagsCheck(thread, flags, options);
	if (thread_flags == 0U) {
		// Check if timeout is specified
		if (timeout != 0U) {
			// Store waiting flags and options
			thread->wait_flags = flags;
			thread->flags_options = (uint8_t)options;
			// Suspend current Thread
			osRtxThreadWaitEnter(osRtxThreadWaitingThreadFlags, timeout);

			ThreadDispatcher::instance().blockUntilWoken();

			if(thread->waitValPresent)
			{
				thread_flags = thread->waitExitVal;
			}
			else
			{
				thread_flags = (uint32_t)osErrorTimeout;
			}
		}
		else
		{
			thread_flags = (uint32_t)osErrorResource;
		}
	}
	return thread_flags;
}
