//
// Implementations for RTXOff's fake interrupt controller
//

#include "rtxoff_nvic.h"
#include "rtxoff_internal.h"
#include "ThreadDispatcher.h"

#include <map>
#include <set>

// Called after an interrupt is added to the interrupt queue.
// Should be called with the interrupts mutex held (which is passed in here so we can unlock it).
void deliverNewInterrupt(InterruptData & interrupt, std::unique_lock<std::recursive_mutex> & interruptDataLock)
{
	if(ThreadDispatcher::instance().interrupt.active)
	{
		// We are already in an interrupt vector (or at least one is currently running in another thread).
		// Do nothing since once the vector returns the new interrupt will be processed.
	}
	else
	{
		// unlock the interrupts mutex so the scheduler can run
		interruptDataLock.unlock();

		// Note: It should be more or less safe to access interrupt.pending and interrupt.enabled here even without the mutex locked.
		// We wouldn't want to access the other ThreadDispatcher interrupt data because other threads (e.g. test harness
		// code) could be changing those.  The only issue would be if two threads (at least one of which has to be
		// a test harness thread) try to SetPending the same interrupt at the same time, it could be called once or
		// twice, but that's basically expected behavior anyway.

		// Following real-time behavior, wait for the new interrupt to be executed.
		while(ThreadDispatcher::instance().interrupt.enabled && interrupt.enabled && interrupt.pending)
		{

			// request the scheduler to run
			ThreadDispatcher::instance().requestSchedule();
#if USE_WINTHREAD
			SwitchToThread();
#else
#error TODO
#endif
		}

		interruptDataLock.lock();
	}
}

/**
 * Get a reference to an interrupt data, adding it to the map if needed.
 * @param IRQn
 * @return
 */
InterruptData & getInterruptData(IRQn_Type IRQn)
{
	auto interruptDataIter = ThreadDispatcher::instance().interrupt.interruptData.find(IRQn);
	if(interruptDataIter == ThreadDispatcher::instance().interrupt.interruptData.end())
	{
		interruptDataIter = ThreadDispatcher::instance().interrupt.interruptData.insert(std::make_pair(IRQn, InterruptData(IRQn))).first;
	}
	return interruptDataIter->second;
}

// implementations
void NVIC_SetPriorityGrouping(uint32_t PriorityGroup)
{
	// NOTE: The priority group mask does not actually do anything in RTXOff because it doesn't support
	// interrupts interrupting interrupts.  We do store the value though.
	std::unique_lock<std::recursive_mutex> interruptDataLock(ThreadDispatcher::instance().interrupt.mutex);
	ThreadDispatcher::instance().interrupt.priorityGroupMask = PriorityGroup;
}

uint32_t NVIC_GetPriorityGrouping(void)
{
	std::unique_lock<std::recursive_mutex> interruptDataLock(ThreadDispatcher::instance().interrupt.mutex);
	return ThreadDispatcher::instance().interrupt.priorityGroupMask;
}

void NVIC_EnableIRQ(IRQn_Type IRQn)
{
	std::unique_lock<std::recursive_mutex> interruptDataLock(ThreadDispatcher::instance().interrupt.mutex);

	InterruptData & interruptData = getInterruptData(IRQn);
	interruptData.enabled = true;

	// if interrupt was previously pending, add it to the queue.
	if(interruptData.pending)
	{
		ThreadDispatcher::instance().interrupt.pendingInterrupts.insert(&interruptData);
		deliverNewInterrupt(interruptData, interruptDataLock);
	}
}

uint32_t NVIC_GetEnableIRQ(IRQn_Type IRQn)
{
	std::unique_lock<std::recursive_mutex> interruptDataLock(ThreadDispatcher::instance().interrupt.mutex);
	return getInterruptData(IRQn).enabled;
}

void NVIC_DisableIRQ(IRQn_Type IRQn)
{
	std::unique_lock<std::recursive_mutex> interruptDataLock(ThreadDispatcher::instance().interrupt.mutex);
	getInterruptData(IRQn).enabled = false;
}

void NVIC_SetVector(IRQn_Type IRQn, void (*vector)())
{
	std::unique_lock<std::recursive_mutex> interruptDataLock(ThreadDispatcher::instance().interrupt.mutex);
	getInterruptData(IRQn).vector = vector;
}

void (*NVIC_GetVector(IRQn_Type IRQn))()
{
	std::unique_lock<std::recursive_mutex> interruptDataLock(ThreadDispatcher::instance().interrupt.mutex);
	return getInterruptData(IRQn).vector;
}

uint32_t NVIC_GetPendingIRQ(IRQn_Type IRQn)
{
	std::unique_lock<std::recursive_mutex> interruptDataLock(ThreadDispatcher::instance().interrupt.mutex);
	return getInterruptData(IRQn).pending;
}

void NVIC_SetPendingIRQ(IRQn_Type IRQn)
{
	std::unique_lock<std::recursive_mutex> interruptDataLock(ThreadDispatcher::instance().interrupt.mutex);

	InterruptData & interruptData = getInterruptData(IRQn);

	interruptData.pending = true;
	ThreadDispatcher::instance().interrupt.pendingInterrupts.insert(&interruptData);

	deliverNewInterrupt(interruptData, interruptDataLock);
}

void NVIC_ClearPendingIRQ(IRQn_Type IRQn)
{
	std::unique_lock<std::recursive_mutex> interruptDataLock(ThreadDispatcher::instance().interrupt.mutex);

	InterruptData & interruptData = getInterruptData(IRQn);

	interruptData.pending = false;
	ThreadDispatcher::instance().interrupt.pendingInterrupts.erase(&interruptData);
}

uint32_t NVIC_GetActive(IRQn_Type IRQn) {
	std::unique_lock<std::recursive_mutex> interruptDataLock(ThreadDispatcher::instance().interrupt.mutex);

	return getInterruptData(IRQn).active;
}

void NVIC_SetPriority(IRQn_Type IRQn, uint32_t priority) {
	std::unique_lock<std::recursive_mutex> interruptDataLock(ThreadDispatcher::instance().interrupt.mutex);

	getInterruptData(IRQn).priority = priority;
}

uint32_t NVIC_GetPriority(IRQn_Type IRQn)
{
	std::unique_lock<std::recursive_mutex> interruptDataLock(ThreadDispatcher::instance().interrupt.mutex);

	return getInterruptData(IRQn).priority;
}

// value from core_cm3.h
#define NVIC_PRIO_BITS 5

// note: these functions copied exactly from the arm implementation
uint32_t NVIC_EncodePriority(uint32_t PriorityGroup, uint32_t PreemptPriority, uint32_t SubPriority) {
	uint32_t PriorityGroupTmp = (PriorityGroup & (uint32_t) 0x07UL);   /* only values 0..7 are used          */
	uint32_t PreemptPriorityBits;
	uint32_t SubPriorityBits;

	PreemptPriorityBits = ((7UL - PriorityGroupTmp) > (uint32_t) (NVIC_PRIO_BITS)) ? (uint32_t) (NVIC_PRIO_BITS)
	                                                                               : (uint32_t) (7UL -
	                                                                                             PriorityGroupTmp);
	SubPriorityBits = ((PriorityGroupTmp + (uint32_t) (NVIC_PRIO_BITS)) < (uint32_t) 7UL) ? (uint32_t) 0UL
	                                                                                      : (uint32_t) (
					(PriorityGroupTmp - 7UL) + (uint32_t) (NVIC_PRIO_BITS));

	return (
			((PreemptPriority & (uint32_t) ((1UL << (PreemptPriorityBits)) - 1UL)) << SubPriorityBits) |
			((SubPriority & (uint32_t) ((1UL << (SubPriorityBits)) - 1UL)))
	);
}

void NVIC_DecodePriority(uint32_t Priority, uint32_t PriorityGroup, uint32_t *const pPreemptPriority,
                         uint32_t *const pSubPriority) {
	uint32_t PriorityGroupTmp = (PriorityGroup & (uint32_t) 0x07UL);   /* only values 0..7 are used          */
	uint32_t PreemptPriorityBits;
	uint32_t SubPriorityBits;

	PreemptPriorityBits = ((7UL - PriorityGroupTmp) > (uint32_t) (NVIC_PRIO_BITS)) ? (uint32_t) (NVIC_PRIO_BITS)
	                                                                               : (uint32_t) (7UL -
	                                                                                             PriorityGroupTmp);
	SubPriorityBits = ((PriorityGroupTmp + (uint32_t) (NVIC_PRIO_BITS)) < (uint32_t) 7UL) ? (uint32_t) 0UL
	                                                                                      : (uint32_t) (
					(PriorityGroupTmp - 7UL) + (uint32_t) (NVIC_PRIO_BITS));

	*pPreemptPriority = (Priority >> SubPriorityBits) & (uint32_t) ((1UL << (PreemptPriorityBits)) - 1UL);
	*pSubPriority = (Priority) & (uint32_t) ((1UL << (SubPriorityBits)) - 1UL);
}

