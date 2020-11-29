//
// RTXOff timer functionality
//

#include <cstring>
#include "ThreadDispatcher.h"

//  ==== Helper functions ====

/// Insert Timer into the Timer List sorted by Time.
/// \param[in]  timer           timer object.
/// \param[in]  tick            timer tick.
static void TimerInsert(osRtxTimer_t *timer, uint32_t tick) {
    osRtxTimer_t *prev, *next;

    prev = nullptr;
    next = ThreadDispatcher::instance().timer.list;
    while ((next != nullptr) && (next->tick <= tick)) {
        tick -= next->tick;
        prev = next;
        next = next->next;
    }
    timer->tick = tick;
    timer->prev = prev;
    timer->next = next;
    if (next != nullptr) {
        next->tick -= timer->tick;
        next->prev = timer;
    }
    if (prev != nullptr) {
        prev->next = timer;
    } else {
        ThreadDispatcher::instance().timer.list = timer;
    }
}

/// Remove Timer from the Timer List.
/// \param[in]  timer           timer object.
static void TimerRemove(const osRtxTimer_t *timer) {

    if (timer->next != nullptr) {
        timer->next->tick += timer->tick;
        timer->next->prev = timer->prev;
    }
    if (timer->prev != nullptr) {
        timer->prev->next = timer->next;
    } else {
        ThreadDispatcher::instance().timer.list = timer->next;
    }
}

/// Unlink Timer from the Timer List Head.
/// \param[in]  timer           timer object.
static void TimerUnlink(const osRtxTimer_t *timer) {

    if (timer->next != nullptr) {
        timer->next->prev = timer->prev;
    }
    ThreadDispatcher::instance().timer.list = timer->next;
}


//  ==== Library functions ====

/// Timer Tick (called each SysTick).
static void osRtxTimerTick(void) {
    osRtxTimer_t *timer;
    osStatus_t status;

    timer = ThreadDispatcher::instance().timer.list;
    if (timer == nullptr) {
        //lint -e{904} "Return statement before end of function" [MISRA Note 1]
        return;
    }

    timer->tick--;
    while ((timer != nullptr) && (timer->tick == 0U)) {
        TimerUnlink(timer);
        status = osMessageQueuePut(ThreadDispatcher::instance().timer.mq, &timer->finfo, 0U, 0U);
        if (status != osOK) {
#if RTXOFF_DEBUG
            std::cerr << "cannot put timer in queue (status=" << status << ")" << std::endl;
#endif
            // TODO throw or something
            //(void) osRtxErrorNotify(osRtxErrorTimerQueueOverflow, timer);
        }
        if (timer->type == osRtxTimerPeriodic) {
            TimerInsert(timer, timer->load);
        } else {
            timer->state = osRtxTimerStopped;
        }
        timer = ThreadDispatcher::instance().timer.list;
    }
}

/// Timer Thread
[[noreturn]] void osRtxTimerThread(void *argument) {
    osRtxTimerFinfo_t finfo;
    osStatus_t status;
    (void) argument;

    const osMessageQueueAttr_t os_timer_mq_attr = {
            NULL,
            0U,
            nullptr,
            0,
            nullptr,
            0
    };

    ThreadDispatcher::instance().timer.mq = reinterpret_cast<osRtxMessageQueue_t *>(
            osMessageQueueNew(OS_TIMER_CB_QUEUE, sizeof(osRtxTimerFinfo_t), &os_timer_mq_attr)
    );

    ThreadDispatcher::instance().timer.tick = osRtxTimerTick;

    for (;;) {
        //lint -e{934} "Taking address of near auto variable"
        status = osMessageQueueGet(ThreadDispatcher::instance().timer.mq, &finfo, nullptr, osWaitForever);
        if (status == osOK) {
            (finfo.func)(finfo.arg);
        }
    }
}

//  ==== Service Calls ====

/// Create and Initialize a timer.
/// \note API identical to osTimerNew
osTimerId_t osTimerNew(osTimerFunc_t func, osTimerType_t type, void *argument, const osTimerAttr_t *attr) {
    osRtxTimer_t *timer;
    uint8_t flags;
    const char *name;

    if (IsIrqMode() || IsIrqMasked())
        return nullptr;

    // Check parameters
    if ((func == nullptr) || ((type != osTimerOnce) && (type != osTimerPeriodic))) {
        return nullptr;
    }

    // Process attributes
    if (attr != nullptr) {
        name = attr->name;
        //lint -e{9079} "conversion from pointer to void to pointer to other type" [MISRA Note 6]
        timer = static_cast<osRtxTimer_t *>(attr->cb_mem);
        if (timer != nullptr) {
            //lint -e(923) -e(9078) "cast from pointer to unsigned int" [MISRA Note 7]
            if ((((uint64_t) timer & 3U) != 0U) || (attr->cb_size < sizeof(osRtxTimer_t))) {
                return nullptr;
            }
        } else {
            if (attr->cb_size != 0U) {
                return nullptr;
            }
        }
    } else {
        name = nullptr;
        timer = nullptr;
    }

    // Allocate object memory if not provided
    if (timer == nullptr) {
        timer = new osRtxTimer_t;
        flags = osRtxFlagSystemObject;
    } else {
        flags = 0U;
    }

    if (timer != nullptr) {
        // Initialize control block
        timer->id = osRtxIdTimer;
        timer->state = osRtxTimerStopped;
        timer->flags = flags;
        timer->type = (uint8_t) type;
        timer->name = name;
        timer->prev = nullptr;
        timer->next = nullptr;
        timer->tick = 0U;
        timer->load = 0U;
        timer->finfo.func = func;
        timer->finfo.arg = argument;
    }

    return timer;
}

/// Get name of a timer.
/// \note API identical to osTimerGetName
const char *osTimerGetName(osTimerId_t timer_id) {
    auto timer = reinterpret_cast<osRtxTimer_t *>(timer_id);

    if (IsIrqMode() || IsIrqMasked())
        return nullptr;

    // Check parameters
    if ((timer == nullptr) || (timer->id != osRtxIdTimer)) {
        return nullptr;
    }

    return timer->name;
}

/// Start or restart a timer.
/// \note API identical to osTimerStart
osStatus_t osTimerStart(osTimerId_t timer_id, uint32_t ticks) {
    auto timer = reinterpret_cast<osRtxTimer_t *>(timer_id);

    if (IsIrqMode() || IsIrqMasked())
        return osErrorISR;

    // Check parameters
    if ((timer == nullptr) || (timer->id != osRtxIdTimer) || (ticks == 0U)) {
        return osErrorParameter;
    }

    if (timer->state == osRtxTimerRunning) {
        TimerRemove(timer);
    } else {
        if (ThreadDispatcher::instance().timer.tick == nullptr) {
            return osErrorResource;
        } else {
            timer->state = osRtxTimerRunning;
            timer->load = ticks;
        }
    }

    TimerInsert(timer, ticks);

    return osOK;
}

/// Stop a timer.
/// \note API identical to osTimerStop
osStatus_t osTimerStop(osTimerId_t timer_id) {
    auto timer = reinterpret_cast<osRtxTimer_t *>(timer_id);

    if (IsIrqMode() || IsIrqMasked())
        return osErrorISR;

    // Check parameters
    if ((timer == nullptr) || (timer->id != osRtxIdTimer)) {
        return osErrorParameter;
    }

    // Check object state
    if (timer->state != osRtxTimerRunning) {
        return osErrorResource;
    }

    timer->state = osRtxTimerStopped;

    TimerRemove(timer);

    return osOK;
}

/// Check if a timer is running.
/// \note API identical to osTimerIsRunning
uint32_t osTimerIsRunning(osTimerId_t timer_id) {
    auto timer = reinterpret_cast<osRtxTimer_t *>(timer_id);

    if (IsIrqMode() || IsIrqMasked())
        return 0U;

    // Check parameters
    if ((timer == nullptr) || (timer->id != osRtxIdTimer)) {
        return 0U;
    }

    if (timer->state == osRtxTimerRunning) {
        return 1U;
    } else {
        return 0U;
    }
}

/// Delete a timer.
/// \note API identical to osTimerDelete
osStatus_t osTimerDelete(osTimerId_t timer_id) {
    auto timer = reinterpret_cast<osRtxTimer_t *>(timer_id);

    if (IsIrqMode() || IsIrqMasked())
        return osErrorISR;

    // Check parameters
    if ((timer == nullptr) || (timer->id != osRtxIdTimer)) {
        return osErrorParameter;
    }

    if (timer->state == osRtxTimerRunning) {
        TimerRemove(timer);
    }

    // Mark object as inactive and invalid
    timer->state = osRtxTimerInactive;
    timer->id = osRtxIdInvalid;

    // Free object memory
    if ((timer->flags & osRtxFlagSystemObject) != 0U) {
        delete timer;
    }

    return osOK;
}
