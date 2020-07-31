#include "ThreadDispatcher.h"

#include <system_error>
#include <thread>

ThreadDispatcher::Mutex::Mutex()
{
	instance().lockMutex();
}

ThreadDispatcher::Mutex::~Mutex()
{
	instance().unlockMutex();
}

ThreadDispatcher &ThreadDispatcher::instance()
{
	static ThreadDispatcher instance;
	return instance;
}

#if USE_WINTHREAD
ThreadDispatcher::ThreadDispatcher()
{
	InitializeConditionVariable(&kernelModeCondVar);
	InitializeCriticalSection(&kernelDataMutex);
}

void ThreadDispatcher::lockMutex()
{
	EnterCriticalSection(&kernelDataMutex);
}

void ThreadDispatcher::unlockMutex()
{
	LeaveCriticalSection(&kernelDataMutex);
}

void ThreadDispatcher::dispatchForever()
{
	while(true)
	{
		// sanity check that thread is still running
		DWORD exitCode;
		GetExitCodeThread(thread.run.curr->osThread, &exitCode);
		if(exitCode != STILL_ACTIVE)
		{
			std::cerr << "RTXOff Sanity Check Failure: thread " << thread.run.curr->name << " has finished execution but is still active according to RTX scheduler." << std::endl;
			std::cerr << "Make sure that all threads call osThreadExit() instead of simply returning from their main functions." << std::endl;
			exit(7);
		}

		// Dispatch the current thread
		if(ResumeThread(thread.run.curr->osThread) < 0)
		{
			std::cerr << "Error resuming RTX thread " << thread.run.curr->name << ": " << std::system_category().message(GetLastError()) << std::endl;
		}

		// wait on the kernel mode cond var
		// note: a spurious wakeup is OK, because if the next thread hasn't changed and there is no timer tick, then
		// waking up won't do anything.
		bool sleepTimedOut = SleepConditionVariableCS(&kernelModeCondVar, &kernelDataMutex, OS_TICK_PERIOD_MS) == 0;


		if(thread.run.curr != nullptr)
		{
			// current thread still exists, try to suspend it
			if(SuspendThread(thread.run.curr->osThread) < 0)
			{
				std::cerr << "Error suspending RTX thread " << thread.run.curr->name << ": " << std::system_category().message(GetLastError()) << std::endl;
			}
		}

		if(thread.run.next != nullptr)
		{
			// one of the RTX functions has triggered us to switch to a different thread.
			thread.run.curr = thread.run.next;
			thread.run.next = nullptr;
		}

		if(thread.run.curr == nullptr)
		{
			std::cerr << "RTXOFF Critical Error: Thread terminated but don't have another thread to run!" << std::endl;
			exit(4);
		}

		// regardless of what else happened, check if enough time has passed to deliver a tick.
		if(std::chrono::steady_clock::now() - lastTickTime >= std::chrono::milliseconds(OS_TICK_PERIOD_MS))
		{
			lastTickTime = std::chrono::steady_clock::now();

			// deliver the next tick
			onTick();

			// load thread that tick handler says to use
			thread.run.curr = thread.run.next;
			thread.run.next = nullptr;
		}

	}
}

void ThreadDispatcher::requestSchedule()
{
	WakeConditionVariable(&kernelModeCondVar);
}
#else
#error TODO
#endif

void ThreadDispatcher::switchNextThread(osRtxThread_t * nextThread)
{
	nextThread->state = osRtxThreadRunning;
	thread.run.next = nextThread;
}

void ThreadDispatcher::dispatch(osRtxThread_t *toDispatch)
{
	if (toDispatch == nullptr)
	{
		osRtxThread_t * thread_ready = thread.ready.thread_list;
		if ((kernel.state == osRtxKernelRunning) &&
			(thread_ready != NULL) &&
			(thread_ready->priority > thread.run.curr->priority))
		{
#if RTXOFF_DEBUG
			std::cerr << thread_ready->name << " is higher priority than the current thread " << thread.run.curr->name << ", switching to it now." << std::endl;
#endif
			// Preempt running Thread
			osRtxThreadListRemove(thread_ready);
			osRtxThreadBlock(thread.run.curr);
			switchNextThread(thread_ready);
		}
	}
	else
	{
		if ((kernel.state == osRtxKernelRunning) &&
			(toDispatch->priority > thread.run.curr->priority))
		{
			// Preempt running Thread
			osRtxThreadBlock(thread.run.curr);
			switchNextThread(toDispatch);
		} else {
			// Put Thread into Ready list
			osRtxThreadReadyPut(toDispatch);
		}
	}
}

void ThreadDispatcher::onTick()
{
	kernel.tick++;

	// Process Timers
	if (timer.tick != NULL) {
		timer.tick();
	}

	// Process Thread Delays
	delayListTick();

	dispatch(nullptr);

	if(thread.run.next == nullptr)
	{
		// dispatch did not choose a different thread
		thread.run.next = thread.run.curr;
	}

	// Check Round Robin timeout
	if (thread.robin.timeout != 0U)
	{
		if (thread.robin.thread != thread.run.next)
		{
			// thread has changed, reset round robin
			thread.robin.thread = thread.run.next;
			thread.robin.tick   = thread.robin.timeout;
		}
		else
		{
			if (thread.robin.tick != 0U) {
				thread.robin.tick--;
			}
			if (thread.robin.tick == 0U) {
				// Round Robin Timeout
				if (kernel.state == osRtxKernelRunning)
				{
					osRtxThread_t * nextThread = thread.ready.thread_list;
					if ((nextThread != NULL) && (nextThread->priority == thread.robin.thread->priority)) {
						osRtxThreadListRemove(nextThread);
						osRtxThreadReadyPut(thread.robin.thread);
						switchNextThread(nextThread);
						thread.robin.thread = nextThread;
						thread.robin.tick   = thread.robin.timeout;
					}
				}
			}
		}
	}
}

void ThreadDispatcher::blockUntilWoken()
{
	// save current thread at the start of the function call.
	osRtxThread_t * currThread = thread.run.curr;

	requestSchedule();
	unlockMutex();

	// The scheduler thread is now ready to run.  So all we need to do to run it
	// (and switch to another thread) is yield the processor. For loop there in case
	// of timing weirdness.
	while(currThread->state != osRtxThreadRunning)
	{
#if USE_WINTHREAD
		SwitchToThread();
#else
#error TODO
#endif
	}

	lockMutex();
}

void ThreadDispatcher::delayListInsert(osRtxThread_t *toDelay, uint32_t delay)
{
	osRtxThread_t *prev, *next;

	if (delay == osWaitForever)
	{
		// append to end
		prev = NULL;
		next = thread.wait_list;
		while (next != NULL)
		{
			prev = next;
			next = next->delay_next;
		}
		toDelay->delay = delay;
		toDelay->delay_prev = prev;
		toDelay->delay_next = NULL;
		if (prev != NULL) {
			prev->delay_next = toDelay;
		} else {
			thread.wait_list = toDelay;
		}
	}
	else
	{
		prev = NULL;
		next = thread.delay_list;
		while ((next != NULL) && (next->delay <= delay)) {
			delay -= next->delay;
			prev = next;
			next = next->delay_next;
		}
		toDelay->delay = delay;
		toDelay->delay_prev = prev;
		toDelay->delay_next = next;
		if (prev != NULL) {
			prev->delay_next = toDelay;
		} else {
			thread.delay_list = toDelay;
		}
		if (next != NULL) {
			next->delay -= delay;
			next->delay_prev = toDelay;
		}
	}
}

void ThreadDispatcher::delayListRemove(osRtxThread_t *toRemove)
{
	if (toRemove->delay == osWaitForever) {
		if (toRemove->delay_next != NULL) {
			toRemove->delay_next->delay_prev = toRemove->delay_prev;
		}
		if (toRemove->delay_prev != NULL) {
			toRemove->delay_prev->delay_next = toRemove->delay_next;
			toRemove->delay_prev = NULL;
		} else {
			thread.wait_list = toRemove->delay_next;
		}
	} else {
		if (toRemove->delay_next != NULL) {
			toRemove->delay_next->delay += toRemove->delay;
			toRemove->delay_next->delay_prev = toRemove->delay_prev;
		}
		if (toRemove->delay_prev != NULL) {
			toRemove->delay_prev->delay_next = toRemove->delay_next;
			toRemove->delay_prev = NULL;
		} else {
			thread.delay_list = toRemove->delay_next;
		}
	}
}

void ThreadDispatcher::delayListTick()
{
	osRtxThread_t *delayTopThread;
	osRtxObject_t *object;

	delayTopThread = thread.delay_list;
	if (delayTopThread == NULL) {
		return;
	}

	delayTopThread->delay--;

	if (delayTopThread->delay == 0U)
	{
		do {
			switch (delayTopThread->state) {
				case osRtxThreadWaitingDelay:
					break;
				case osRtxThreadWaitingThreadFlags:
					break;
				case osRtxThreadWaitingEventFlags:
					break;
				case osRtxThreadWaitingMutex:
					object = reinterpret_cast<osRtxObject_t *>(osRtxThreadListRoot(delayTopThread));
					osRtxMutexOwnerRestore(reinterpret_cast<osRtxMutex_t *>(object), delayTopThread);
					break;
				case osRtxThreadWaitingSemaphore:
					break;
				case osRtxThreadWaitingMemoryPool:
					break;
				case osRtxThreadWaitingMessageGet:
					break;
				case osRtxThreadWaitingMessagePut:
					break;
				default:
					// Invalid
					break;
			}
#if RTXOFF_DEBUG
			std::cerr << delayTopThread << " has finished its waiting period, moving to ready list" << std::endl;
#endif
			osRtxThreadListRemove(delayTopThread);
			osRtxThreadReadyPut(delayTopThread);
			delayTopThread = delayTopThread->delay_next;
		} while ((delayTopThread != NULL) && (delayTopThread->delay == 0U));
		if (delayTopThread != NULL) {
			delayTopThread->delay_prev = NULL;
		}
		thread.delay_list = delayTopThread;
	}
}



