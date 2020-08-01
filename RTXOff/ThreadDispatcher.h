//
// Class responsible for controlling execution of RTX threads.
//

#ifndef THREADDISPATCHER_H
#define THREADDISPATCHER_H

#include "rtxoff_internal.h"
#include "rtxoff_nvic.h"

#include "RTX_Config.h"

#include <chrono>
#include <map>
#include <set>
#include <mutex>

struct InterruptData
{
	IRQn_Type irq;
	InterruptData(IRQn_Type _irq): irq(_irq) {}
	bool enabled = false; // Whether this interrupt is enabled (can be triggered)
	bool pending = false; // Whether this interrupt is pending (will be called)
	bool active = false; // whether this interrupt is currently being delivered
	void (*vector)() = nullptr; // Interrupt vector to call for this interrupt
	uint8_t priority = 0x0; // Priority.  Lower value will be triggered first.
};

struct InterruptDataComparator
{
	bool operator()(InterruptData const * lhs, InterruptData const * rhs) const
	{
		if(lhs->priority != rhs->priority)
		{
			// we want the lowest priority to sort first in the set.
			return lhs->priority < rhs->priority;
		}
		else
		{
			// per the specs, fall back to interrupt number
			return lhs->irq < rhs->irq;
		}
	}
};

/**
 * To implement CMSIS-RTOS on top of a desktop OS, one OS thread is maintained for each RTX
 * thread.  However, the thread dispatcher follows a single rule: only one RTX thread can be running at a time.
 * Periodically, the dispatcher will stop the RTX thread, either due to a timer tick or due to a wait on an object.
 * At that point, it is the job of the dispatcher to find the next thread to execute and run it.
 *
 * Data in this class is accessed by both the RTX threads and the dispatcher thread.  kernelDataMutex is
 * used to protect access to it to only one thread at a time.  Any API function that interacts with kernel data must lock this mutex.
 *
 */
class ThreadDispatcher
{
public:

	// RTX OS data
	struct {                              ///< Kernel Info
		uint8_t                     state = osRtxKernelInactive;  ///< State
		volatile uint8_t          blocked;  ///< Blocked
		uint8_t                    pendSV;  ///< Pending SV
		uint8_t                  reserved;
		uint32_t                     tick = 0;  ///< Tick counter
	} kernel;

	// Thread data, same as RTX
	struct {                              ///< Thread Info
		struct {                            ///< Thread Run Info

			// Current thread variable.
			// When in an RTX thread, this indicates the current thread that you are executing in.
			// Can be cleared by RTX functions that delete threads; these functions always switch to the
			// scheduler immediately afterward to choose a new thread.
			osRtxThread_t             *curr = nullptr;  ///< Current running Thread

			// Next thread variable.
			// When the scheduler is invoked, if this variable is not null, then the thread listed here
			// becomes the new running thread.  After a thread is loaded from next, next is set back to null.
			// onTick() always sets the next thread, but this can be the same as the current thread.
			osRtxThread_t             *next = nullptr;  ///< Next Thread to Run
		} run;
		osRtxObject_t 				 ready;  ///< Ready List Object.  Linked list of threads sorted by priority.
		osRtxThread_t               *idle;  ///< Idle Thread
		osRtxThread_t         *delay_list;  ///< Delay List
		osRtxThread_t          *wait_list;  ///< Wait List (no Timeout)
		osRtxThread_t     *terminate_list;  ///< Terminate Thread List
		struct {                            ///< Thread Round Robin Info
			osRtxThread_t           *thread = nullptr;  ///< Round Robin Thread
			uint32_t                   tick = 0;  ///< Round Robin Time Tick
			uint32_t                timeout = OS_ROBIN_TIMEOUT;  ///< Round Robin Timeout
		} robin;
	} thread;

	struct {                              ///< Timer Info
		osRtxTimer_t                *list = nullptr;  ///< Active Timer List
		osRtxThread_t             *thread = nullptr;  ///< Timer Thread
		osRtxMessageQueue_t           *mq = nullptr;  ///< Timer Message Queue
		void                (*tick)() = nullptr;  ///< Timer Tick Function
	} timer;

	struct {
		bool active; // number of the interrupt that is currenty active
		uint32_t priorityGroupMask;  // Priority group mask, see PRIGROUP register description
		std::map<IRQn_Type, InterruptData> interruptData; // data for each interrupt.
		std::set<InterruptData *, InterruptDataComparator> pendingInterrupts; // Set of interrupts that are pending, sorted by priority.
		std::recursive_mutex mutex; // Seperate mutex to protect data in this struct.  OK to use std::mutex since we don't need special OS features.
	} interrupt;

	// Time of the last system tick.  Once the clock time goes one tick period past this,
	// we call the tick handler.
	std::chrono::steady_clock::time_point lastTickTime;


#if USE_WINTHREAD
	// Kernel mode condition var.
	// Signaling this causes the OS to return from "user mode" to "kernel mode",
	// and begin executing the thread switcher.
	CONDITION_VARIABLE kernelModeCondVar = {0};

	// Kernel data mutex.  Must be held by any thread accessing or changing this class.
	// This prevents e.g. a timer tick happening when an RTX thread is in the middle
	// of calling osThreadNew().
	CRITICAL_SECTION kernelDataMutex = {0};
#else
#error TODO
#endif

	// create an instance of one of these objects
	// to lock the kernel data mutex.
	// When the object goes out of scope and gets destroyed, the
	// mutex will be released.
	class Mutex
	{
	public:
		explicit Mutex();

		~Mutex();
	};

	/**
	 * Constructor, creates needed OS threading objects.
	 */
	ThreadDispatcher();

	/**
	 * Singleton accessor
	 * @return
	 */
	static ThreadDispatcher & instance();

	ThreadDispatcher(ThreadDispatcher const & other) = delete;
	ThreadDispatcher& operator=(ThreadDispatcher const & other) = delete;

	// called from mutex class to lock and unlock the global mutex
	void lockMutex();
	void unlockMutex();

	/**
	 * Run the dispatcher forever without returning.  Called by osKernelStart().
	 * Should be called with the data mutex already locked.
	 */
	__NO_RETURN void dispatchForever();

	/**
	 * Set this thread as the next to run.
	 */
	void switchNextThread(osRtxThread_t * nextThread);

	/**
	 * Dispatch a thread.  If argument is nullptr: run the top ready thread iff it has higher priority than
	 * the current thread.
	 * If argument is not nullptr, then add this thread to the ready queue, setting it as next if it has higher
	 * priority.
	 * @param toDispatch
	 */
	void dispatch(osRtxThread_t * toDispatch);

	/**
	 * Called by dispatchForever() whenever a scheduler tick occurs.
	 * Handles checking if a thread has exceeded the round robin timeout and switching it out if so.
	 */
	void onTick();

	/**
	 * Cause the scheduler to be woken up and schedule whatever thread is currently set as the next one.
	 * This will happen as soon as the kernel mode mutex is released if you are currently holding it.
	 */
	void requestSchedule();

	/**
	 * Call this from an RTX thread. Requests a schedule, yields to the scheduler thread,
	 * and doesn't return until unless it's the current running thread.
	 *
	 * Expects to be called with the kernel mode mutex locked.
	 */
	void blockUntilWoken();

	/**
	 * Process interrupts in the interrupt queue by calling the interrupt handler functions.
	 * Continues to process interrupts until there are no more left to deliver.
	 * Interrupt vectors will be run synchronously in the scheduler thread.
	 */
	void processInterrupts();

	// RTX Delay list functions
	// -------------------------------------------------------

	/// Insert a Thread into the Delay list sorted by Delay (Lowest at Head).
	/// \param[in]  thread          thread object.
	/// \param[in]  delay           delay value.
	void delayListInsert(osRtxThread_t * toDelay, uint32_t delay);

	/// Remove a Thread from the Delay list.
	/// \param[in]  thread          thread object.
	void delayListRemove(osRtxThread_t * toRemove);

	/// Process Thread Delay Tick (executed each System Tick).
	void delayListTick();
};

#endif //THREADDISPATCHER_H
