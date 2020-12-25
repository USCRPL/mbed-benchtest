#include "ThreadDispatcher.h"

#include <system_error>
#include <thread>
#include <cstring>

ThreadDispatcher::Mutex::Mutex()
{
	instance().lockMutex();
}

ThreadDispatcher::Mutex::~Mutex() {
    instance().unlockMutex();
}

ThreadDispatcher &ThreadDispatcher::instance() {
    static ThreadDispatcher instance;
    return instance;
}

thread_local bool isDispatcher = false;

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

void ThreadDispatcher::requestSchedule()
{
    WakeConditionVariable(&kernelModeCondVar);
}
#else

ThreadDispatcher::ThreadDispatcher()
{
    // initialize kernel mode mutex
    pthread_mutexattr_t recursiveAttr;
    pthread_mutexattr_init(&recursiveAttr);
    pthread_mutexattr_settype(&recursiveAttr, PTHREAD_MUTEX_RECURSIVE);

    pthread_mutex_init(&kernelDataMutex, &recursiveAttr);

    pthread_mutexattr_destroy(&recursiveAttr);

#ifdef USE_MAC_TIME
    pthread_cond_init(&kernelModeCondVar, nullptr);
#else
    // initialize kernel mode cond var
    // make sure that the condition variable is using the monotonic clock
    pthread_condattr_t monotonicClockAttr;
    pthread_condattr_init(&monotonicClockAttr);
    pthread_condattr_setclock(&monotonicClockAttr, CLOCK_MONOTONIC); // sadly CLOCK_PROCESS_CPUTIME_ID is not supported here

    pthread_cond_init(&kernelModeCondVar, &monotonicClockAttr);

    pthread_condattr_destroy(&monotonicClockAttr);
#endif
}

void ThreadDispatcher::lockMutex()
{
    int pthread_errorcode = pthread_mutex_lock(&kernelDataMutex);
    if(pthread_errorcode != 0)
    {
        std::cerr << "Error locking kernel mutex: " << std::system_category().message(pthread_errorcode) << std::endl;
    }
}

void ThreadDispatcher::unlockMutex()
{
    int pthread_errorcode = pthread_mutex_unlock(&kernelDataMutex);
    if(pthread_errorcode != 0)
    {
        if(pthread_errorcode == EPERM)
        {
            // On some platforms (seen on Linux), calling pthread_exit() will unwind the stack and trigger]
            // ThreadDispatcher::~Mutex() calls, which then cause the mutex to be unlocked again even if it's
            // already unlocked.  So we can ignore this error.
        }
        else
        {
            std::cerr << "Error unlocking kernel mutex: " << std::system_category().message(pthread_errorcode) << std::endl;
        }
    }
}

void ThreadDispatcher::requestSchedule()
{
    int pthread_errorcode = pthread_cond_signal(&kernelModeCondVar);
    if(pthread_errorcode != 0)
    {
        std::cerr << "Error signaling kernel mode cond var: " << std::system_category().message(pthread_errorcode) << std::endl;
    }
}

#endif

void ThreadDispatcher::dispatchForever()
{
    isDispatcher = true;
	while(true)
	{
		// Dispatch the current thread
#if RTXOFF_DEBUG && RTXOFF_VERBOSE
        std::cerr << "Resuming thread " << thread.run.curr->name << std::endl;
#endif
		thread_suspender_resume(thread.run.curr->osThread, thread.run.curr->suspenderData);

        // wait on the kernel mode cond var
        // note: a spurious wakeup is OK, because if the next thread hasn't changed and there is no timer tick, then
        // waking up won't do anything.
#if USE_WINTHREAD
		SleepConditionVariableCS(&kernelModeCondVar, &kernelDataMutex, OS_TICK_PERIOD_MS);
#else
		// calculate absolute time to wake up
		struct timespec wakeupTime;
		clock_gettime(CLOCK_MONOTONIC, &wakeupTime);
		wakeupTime.tv_nsec += OS_TICK_PERIOD_MS * 1000000;
		wakeupTime.tv_sec += OS_TICK_PERIOD_MS / 1000;
		pthread_cond_timedwait(&kernelModeCondVar, &kernelDataMutex, &wakeupTime);
#endif

		if(thread.run.curr != nullptr)
		{
			// current thread still exists, try to suspend it
#if RTXOFF_DEBUG && RTXOFF_VERBOSE
            std::cerr << "Suspending thread " << thread.run.curr->name << std::endl;
#endif
            thread_suspender_suspend(thread.run.curr->osThread, thread.run.curr->suspenderData);
		}

		if(!interrupt.enabled)
		{
			// If interrupts are disabled, the scheduler can't run on the real processor.
			// Emulate that by switching back immediately.

			if(thread.run.curr == nullptr)
			{
				std::cerr << "RTXOFF Critical Error: Interrupts disabled but don't have a thread to run??" << std::endl;
				exit(4);
			}
			continue;
		}

		if(thread.run.next != nullptr)
		{
			// one of the RTX functions has triggered us to switch to a different thread.
#if RTXOFF_DEBUG && RTXOFF_VERBOSE
            std::cerr << "Curr is now " << thread.run.next->name << std::endl;
#endif
			thread.run.curr = thread.run.next;
			thread.run.next = nullptr;
		}

		if(thread.run.curr == nullptr)
		{
			std::cerr << "RTXOFF Critical Error: Thread terminated but don't have another thread to run!" << std::endl;
			exit(4);
		}

		// check if there are interrupts to process
		if(!interrupt.pendingInterrupts.empty())
		{
			processInterrupts();
			processQueuedISRData();

			// load thread that interrupt handlers say to use
			if(thread.run.next != nullptr)
			{
#if RTXOFF_DEBUG && RTXOFF_VERBOSE
                std::cerr << "Curr is now " << thread.run.next->name << std::endl;
#endif
				thread.run.curr = thread.run.next;
				thread.run.next = nullptr;
			}
		}

		// regardless of what else happened, check if enough time has passed to deliver a tick.
		if(updateTick())
		{
			// deliver the next tick
			onTick();

#if RTXOFF_DEBUG && RTXOFF_VERBOSE
            if (thread.run.next != thread.run.curr)
                std::cerr << "Curr is now " << thread.run.next->name << std::endl;
#endif
			// load thread that tick handler says to use
			thread.run.curr = thread.run.next;
			thread.run.next = nullptr;
		}

		thread.run.curr->state = osRtxThreadRunning;

	}
}

void ThreadDispatcher::switchNextThread(osRtxThread_t * nextThread)
{
	thread.run.next = nextThread;
}

void ThreadDispatcher::dispatch(osRtxThread_t *toDispatch) {

    if (toDispatch == nullptr) {
        osRtxThread_t *thread_ready = thread.ready.thread_list;

        if (kernel.state == osRtxKernelRunning &&
            thread_ready != nullptr &&
            thread_ready->priority > thread.run.curr->priority) {
#if RTXOFF_DEBUG
            std::cerr << thread_ready->name << " is higher priority than the current (or next) thread " << th->name << ", switching to it now." << std::endl;
#endif
			// Preempt running Thread
            osRtxThreadListRemove(thread_ready);
            osRtxThreadBlock(thread.run.curr);
            switchNextThread(thread_ready);
        }
    } else {
        if ((kernel.state == osRtxKernelRunning) &&
            (toDispatch->priority > thread.run.curr->priority)) {
            // Preempt running Thread
            osRtxThreadBlock(thread.run.curr);
            switchNextThread(toDispatch);
#if RTXOFF_DEBUG
            std::cerr << toDispatch->name << " is higher priority than the current (or next) thread " << th->name << ", switching to it now." << std::endl;
#endif
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

	if(thread.run.next != nullptr) {
        thread.run.curr = thread.run.next;
        thread.run.next = nullptr;
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
			if (thread.robin.tick != 0) {
				thread.robin.tick -= kernel.tickDelta;
			}
			if (thread.robin.tick <= 0) {
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

void ThreadDispatcher::blockUntilWoken() {
    // save current thread at the start of the function call.
    osRtxThread_t *currThread = thread.run.curr;

    if (isDispatcher) {
        // cannot block dispatcher. Not a bug; used when the dispatcher uses
        // calls that switch threads like putting something into a queue.
        return;
    }

    requestSchedule();
    unlockMutex();

    // The scheduler thread is now ready to run.  So all we need to do to run it
    // (and switch to another thread) is yield the processor. For loop there in case
    // of timing weirdness.
    while (currThread->state != osRtxThreadRunning) {
		rtxoff_thread_yield();
    }

    lockMutex();
}

bool ThreadDispatcher::updateTick()
{
	using namespace std::chrono;

	auto nowTime = RTXClock::now();
	auto timeDelta = nowTime - lastTickTime;
	if(timeDelta >= tickDuration)
	{
		// note: duration_cast always rounds down.
		kernel.tickDelta = duration_cast<milliseconds>(timeDelta).count();
		kernel.tick += duration_cast<milliseconds>(timeDelta).count();
		lastTickTime += duration_cast<milliseconds>(timeDelta);
		return true;
	}
	else
	{
		return false;
	}
}

void ThreadDispatcher::processInterrupts()
{
	// set global interrupt flag
	interrupt.active = true;

	std::unique_lock<std::recursive_mutex> lock(interrupt.mutex);

	// More interrupts could be added when we call interrupt handlers, so loop in a way that handles that
	while(interrupt.enabled && !interrupt.pendingInterrupts.empty())
	{
		InterruptData * currInterrupt = *interrupt.pendingInterrupts.begin();

#if RTXOFF_DEBUG
		std::cerr << "Calling interrupt vector for IRQ " << currInterrupt->irq << std::endl;
#endif

		// deliver this interrupt
		currInterrupt->active = true;
		if(currInterrupt->vector != nullptr)
		{
			currInterrupt->vector();
		}

		// now remove it from the queue
		currInterrupt->active = false;
		currInterrupt->pending = false;
		interrupt.pendingInterrupts.erase(currInterrupt);
	}

	interrupt.active = false;
	return;
}

void ThreadDispatcher::queuePostProcess(osRtxObject_t *object)
{
	isr_queue.push(object);
}

void ThreadDispatcher::processQueuedISRData()
{
	while(!isr_queue.empty())
	{
		osRtxObject_t *object = isr_queue.front();
		isr_queue.pop();

		if (object == NULL) {
			break;
		}
		switch (object->id) {
			case osRtxIdThread:
				post_process.thread(reinterpret_cast<osRtxThread_t *>(object));
				break;
			case osRtxIdEventFlags:
				post_process.event_flags(reinterpret_cast<osRtxEventFlags_t *>(object));
				break;
			case osRtxIdSemaphore:
				post_process.semaphore(reinterpret_cast<osRtxSemaphore_t *>(object));
				break;
			case osRtxIdMemoryPool:
				post_process.memory_pool(reinterpret_cast<osRtxMemoryPool_t *>(object));
				break;
			case osRtxIdMessage:
				post_process.message(reinterpret_cast<osRtxMessage_t *>(object));
				break;
			default:
				// Should never come here
				break;
		}
	}

	// switch to next preferred thread
	dispatch(nullptr);
}

void ThreadDispatcher::delayListInsert(osRtxThread_t *toDelay, uint32_t delay) {

#if RTXOFF_DEBUG && RTXOFF_VERBOSE
    std::cerr << "Inserting thread " << toDelay->name << " (" << toDelay << ",prev="
              << toDelay->delay_prev
              << ",next=" << toDelay->delay_next << ") into delay list (" << thread.wait_list << ")" << std::endl;
#endif

    osRtxThread_t *prev, *next;

    if (delay == osWaitForever) {
        // append to end
        prev = NULL;

        next = thread.wait_list;
        while (next != NULL) {
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
    } else {
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

void ThreadDispatcher::delayListRemove(osRtxThread_t *toRemove) {
#if RTXOFF_DEBUG && RTXOFF_VERBOSE
    std::cerr << "Removing thread " << toRemove->name << " (" << toRemove << ",prev="
              << toRemove->delay_prev << ",next=" << toRemove->delay_next << ")" << "from delay list ("
              << thread.wait_list << ")" << std::endl;
#endif
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

	delayTopThread->delay -= kernel.tickDelta;

	if (delayTopThread->delay <= 0)
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
			std::cerr << delayTopThread->name << " has finished its waiting period at tick " << kernel.tick << ", moving to ready list" << std::endl;
#endif
			osRtxThreadListRemove(delayTopThread);
			osRtxThreadReadyPut(delayTopThread);

			// proxy a negative delay value onto the next thread
			if(delayTopThread->delay_next != nullptr)
			{
				delayTopThread->delay_next->delay += delayTopThread->delay;
			}

			delayTopThread = delayTopThread->delay_next;

		} while ((delayTopThread != NULL) && (delayTopThread->delay <= 0));
		if (delayTopThread != NULL) {
			delayTopThread->delay_prev = NULL;
		}
		thread.delay_list = delayTopThread;
	}
}




