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
void deliverNewInterrupts(std::unique_lock<std::recursive_mutex> & interruptDataLock)
{
	if(ThreadDispatcher::instance().interrupt.active)
	{
		// We are already in an interrupt vector (or at least one is currently running in another thread).
		// Do nothing since once the vector returns the new interrupt will be processed.
	}
	else
	{
		// Following real-time behavior, wait for the new interrupt to be executed.
		while(true)
		{
			// unlock the interrupts mutex so the scheduler can run
			interruptDataLock.unlock();

			// request the scheduler to run
			ThreadDispatcher::instance().requestSchedule();
#if USE_WINTHREAD
			SwitchToThread();
#else
#error TODO
#endif
			// now relock the mutex and check if there are any interrupts left
			interruptDataLock.lock();
			if(ThreadDispatcher::instance().interrupt.pendingInterrupts.empty())
			{
				return;
			}
		}

		ThreadDispatcher::instance().
	}
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
	ThreadDispatcher::instance().interrupt.interruptData[IRQn].enabled = true;

	// if interrupt was previously pending, add it to the queue.
	if(ThreadDispatcher::instance().interrupt.interruptData[IRQn].pending)
	{
		ThreadDispatcher::instance().interrupt.pendingInterrupts.insert(&ThreadDispatcher::instance().interrupt.interruptData[IRQn]);
		deliverNewInterrupts(interruptDataLock);
	}
}

uint32_t NVIC_GetEnableIRQ(IRQn_Type IRQn)
{
	std::unique_lock<std::recursive_mutex> interruptDataLock(ThreadDispatcher::instance().interrupt.mutex);
	return ThreadDispatcher::instance().interrupt.interruptData[IRQn].enabled;
}

void NVIC_DisableIRQ(IRQn_Type IRQn)
{
	std::unique_lock<std::recursive_mutex> interruptDataLock(ThreadDispatcher::instance().interrupt.mutex);
	ThreadDispatcher::instance().interrupt.interruptData[IRQn].enabled = false;
}

void NVIC_SetVector(IRQn_Type IRQn, void (*vector)())
{
	std::unique_lock<std::recursive_mutex> interruptDataLock(ThreadDispatcher::instance().interrupt.mutex);
	ThreadDispatcher::instance().interrupt.interruptData[IRQn].vector = vector;
}

void (*NVIC_GetVector(IRQn_Type IRQn))()
{
	std::unique_lock<std::recursive_mutex> interruptDataLock(ThreadDispatcher::instance().interrupt.mutex);
	return ThreadDispatcher::instance().interrupt.interruptData[IRQn].vector;
}

uint32_t NVIC_GetPendingIRQ(IRQn_Type IRQn)
{
	std::unique_lock<std::recursive_mutex> interruptDataLock(ThreadDispatcher::instance().interrupt.mutex);
	return ThreadDispatcher::instance().interrupt.interruptData[IRQn].pending;
}

void NVIC_SetPendingIRQ(IRQn_Type IRQn)
{
	std::unique_lock<std::recursive_mutex> interruptDataLock(ThreadDispatcher::instance().interrupt.mutex);
	ThreadDispatcher::instance().interrupt.interruptData[IRQn].pending = true;
	ThreadDispatcher::instance().interrupt.pendingInterrupts.insert(&interruptData[IRQn]);

	deliverNewInterrupts(interruptDataLock);
}

void NVIC_ClearPendingIRQ(IRQn_Type IRQn)
{
	std::unique_lock<std::recursive_mutex> interruptDataLock(ThreadDispatcher::instance().interrupt.mutex);

	ThreadDispatcher::instance().interrupt.interruptData[IRQn].pending = false;
	ThreadDispatcher::instance().interrupt.pendingInterrupts.erase(&interruptData[IRQn]);
}

uint32_t NVIC_GetActive(IRQn_Type IRQn) {
	std::unique_lock<std::recursive_mutex> interruptDataLock(ThreadDispatcher::instance().interrupt.mutex);

	return ThreadDispatcher::instance().interrupt.interruptData[IRQn].active;
}

void NVIC_SetPriority(IRQn_Type IRQn, uint32_t priority) {
	std::unique_lock<std::recursive_mutex> interruptDataLock(ThreadDispatcher::instance().interrupt.mutex);

	ThreadDispatcher::instance().interrupt.interruptData[IRQn].priority = priority;
}

uint32_t NVIC_GetPriority(IRQn_Type IRQn)
{
	std::unique_lock<std::recursive_mutex> interruptDataLock(ThreadDispatcher::instance().interrupt.mutex);

	return ThreadDispatcher::instance().interrupt.interruptData[IRQn].priority;
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

