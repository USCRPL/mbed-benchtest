//
// RTXOff msgqueue functionality
//

#include <cstring>
#include "ThreadDispatcher.h"

//  ==== Helper functions ====

/// Put a Message into Queue sorted by Priority (Highest at Head).
/// \param[in]  mq              message queue object.
/// \param[in]  msg             message object.
static void MessageQueuePut(osRtxMessageQueue_t *mq, osRtxMessage_t *msg) {

    osRtxMessage_t *prev, *next;

    if (mq->msg_last != nullptr) {
        prev = mq->msg_last;
        next = nullptr;
        while ((prev != nullptr) && (prev->priority < msg->priority)) {
            next = prev;
            prev = prev->prev;
        }
        msg->prev = prev;
        msg->next = next;
        if (prev != nullptr) {
            prev->next = msg;
        } else {
            mq->msg_first = msg;
        }
        if (next != nullptr) {
            next->prev = msg;
        } else {
            mq->msg_last = msg;
        }
    } else {
        msg->prev = nullptr;
        msg->next = nullptr;
        mq->msg_first = msg;
        mq->msg_last = msg;
    }

    mq->msg_count++;
}

/// Get a Message from Queue with Highest Priority.
/// \param[in]  mq              message queue object.
/// \return message object or nullptr.
static osRtxMessage_t *MessageQueueGet(osRtxMessageQueue_t *mq) {

    osRtxMessage_t *msg;
    uint32_t count;
    uint8_t flags;

    count = mq->msg_count;
    if (count != 0U) {
        mq->msg_count--;
    }

    if (count != 0U) {
        msg = mq->msg_first;

        while (msg != nullptr) 
        {
            flags = msg->flags;
            msg->flags = 1U;

            if (flags == 0U) {
                break;
            }
            msg = msg->next;
        }
    } else {
        msg = nullptr;
    }

    return msg;
}


/// Remove a Message from Queue
/// \param[in]  mq              message queue object.
/// \param[in]  msg             message object.
static void MessageQueueRemove(osRtxMessageQueue_t *mq, const osRtxMessage_t *msg) {

    if (msg->prev != nullptr) {
        msg->prev->next = msg->next;
    } else {
        mq->msg_first = msg->next;
    }
    if (msg->next != nullptr) {
        msg->next->prev = msg->prev;
    } else {
        mq->msg_last = msg->prev;
    }
}

static void sendWaitingMessage(osRtxMessageQueue_t *mq, bool dispatch) {
    auto *msg = static_cast<osRtxMessage_t *>(osRtxMemoryPoolAlloc(&mq->mp_info));
    if (msg != nullptr) {
        // Wakeup waiting Thread with highest Priority
        osRtxThread_t *thread = osRtxThreadListGet(reinterpret_cast<osRtxObject_t *>(mq));
        osRtxThreadWaitExit(thread, (uint32_t) osOK, dispatch);

        const void *ptr_src = thread->queueBlockedData.msg_body.send;
        memcpy(&msg[1], ptr_src, mq->msg_size);
        // Store Message into Queue
        msg->id = osRtxIdMessage;
        msg->flags = 0U;
        msg->priority = thread->queueBlockedData.msg_prio.send;
        MessageQueuePut(mq, msg);
        thread->waitValPresent = true;

    }
}

static void receiveWaitingMessage(osRtxMessageQueue_t *mq, const void *msg_body, uint8_t msg_prio, bool dispatch) {
    // Wakeup waiting Thread with highest Priority

    osRtxThread_t *thread = osRtxThreadListGet(reinterpret_cast<osRtxObject_t *>(mq));
    osRtxThreadWaitExit(thread, (uint32_t) osOK, dispatch);

    void *ptr_dst = thread->queueBlockedData.msg_body.receive;
    memcpy(ptr_dst, msg_body, mq->msg_size);

    uint8_t *rec;
    if ((rec = thread->queueBlockedData.msg_prio.receive) != nullptr) {
        *rec = msg_prio;
    }
    thread->waitValPresent = true;
}

//  ==== Post ISR processing ====

/// Message Queue post ISR processing.
/// \param[in]  msg             message object.
static void osRtxMessageQueuePostProcess(osRtxMessage_t *msg) {
    osRtxMessageQueue_t *mq;

    if (msg->flags != 0U) {
        // Remove Message
        //lint -e{9079} -e{9087} "cast between pointers to different object types"
        mq = *((osRtxMessageQueue_t **) (void *) &msg[1]);
        MessageQueueRemove(mq, msg);
        // Free memory
        msg->id = osRtxIdInvalid;
        (void) osRtxMemoryPoolFree(&mq->mp_info, msg);
        // Check if Thread is waiting to send a Message
        if (mq->thread_list != nullptr) {
            sendWaitingMessage(mq, false);
        }
    } else {
        // New Message
        mq = reinterpret_cast<osRtxMessageQueue_t *>(msg->next);

        // Check if Thread is waiting to receive a Message
        if ((mq->thread_list != nullptr) && (mq->thread_list->state == osRtxThreadWaitingMessageGet)) {
            receiveWaitingMessage(mq, &msg[1], msg->priority, false);

            // Free memory
            msg->id = osRtxIdInvalid;
            (void) osRtxMemoryPoolFree(&mq->mp_info, msg);

        } else {
            MessageQueuePut(mq, msg);
        }
    }
}


//  ==== Service Calls ====

/// Create and Initialize a Message Queue object.
/// \note API identical to osMessageQueueNew
osMessageQueueId_t osMessageQueueNew(uint32_t msg_count, uint32_t msg_size, const osMessageQueueAttr_t *attr) {
    ThreadDispatcher::Mutex mutex;
    if (IsIrqMode() || IsIrqMasked()) {
        return nullptr;
    }
    osRtxMessageQueue_t *mq;
    void *mq_mem;
    uint32_t mq_size;
    uint32_t block_size;
    uint32_t size;
    uint8_t flags;
    const char *name;

    // Check parameters
    if ((msg_count == 0U) || (msg_size == 0U)) {

        //lint -e{904} "Return statement before end of function" [MISRA Note 1]
        return nullptr;
    }
    block_size = ((msg_size + 3U) & ~3UL) + sizeof(osRtxMessage_t);
    if ((__builtin_clz(msg_count) + __builtin_clz(block_size)) < 32U) {

        //lint -e{904} "Return statement before end of function" [MISRA Note 1]
        return nullptr;
    }

    size = msg_count * block_size;

    // Process attributes
    if (attr != nullptr) {
        name = attr->name;
        //lint -e{9079} "conversion from pointer to void to pointer to other type" [MISRA Note 6]
        mq = static_cast<osRtxMessageQueue_t *>(attr->cb_mem);
        //lint -e{9079} "conversion from pointer to void to pointer to other type" [MISRA Note 6]
        mq_mem = attr->mq_mem;
        mq_size = attr->mq_size;
        if (mq != nullptr) {
            //lint -e(923) -e(9078) "cast from pointer to unsigned int" [MISRA Note 7]
            if (((reinterpret_cast<uint64_t>(mq) & 3U) != 0U) || (attr->cb_size < sizeof(osRtxMessageQueue_t))) {

                //lint -e{904} "Return statement before end of function" [MISRA Note 1]
                return nullptr;
            }
        } else {
            if (attr->cb_size != 0U) {

                //lint -e{904} "Return statement before end of function" [MISRA Note 1]
                return nullptr;
            }
        }
        if (mq_mem != nullptr) {
            //lint -e(923) -e(9078) "cast from pointer to unsigned int" [MISRA Note 7]
            if (((reinterpret_cast<uint64_t>(mq_mem) & 3U) != 0U) || (mq_size < size)) {

                //lint -e{904} "Return statement before end of function" [MISRA Note 1]
                return nullptr;
            }
        } else {
            if (mq_size != 0U) {

                //lint -e{904} "Return statement before end of function" [MISRA Note 1]
                return nullptr;
            }
        }
    } else {
        name = nullptr;
        mq = nullptr;
        mq_mem = nullptr;
    }

    // Allocate object memory if not provided
    if (mq == nullptr) {
        mq = new osRtxMessageQueue_t;
        flags = osRtxFlagSystemObject;
    } else {
        flags = 0U;
    }

    // Allocate data memory if not provided
    if (mq_mem == nullptr) {
        //lint -e{9079} "conversion from pointer to void to pointer to other type" [MISRA Note 5]
        mq_mem = new uint8_t[size];
        memset(mq_mem, 0, size);

        flags |= osRtxFlagSystemMemory;
    }

    // Initialize control block
    mq->id = osRtxIdMessageQueue;
    mq->flags = flags;
    mq->name = name;
    mq->thread_list = nullptr;
    mq->msg_size = msg_size;
    mq->msg_count = 0U;
    mq->msg_first = nullptr;
    mq->msg_last = nullptr;

    (void) osRtxMemoryPoolInit(&mq->mp_info, msg_count, block_size, mq_mem);

    // Register post ISR processing function
    ThreadDispatcher::instance().post_process.message = osRtxMessageQueuePostProcess;

    return mq;
}

/// Get name of a Message Queue object.
/// \note API identical to osMessageQueueGetName
const char *osMessageQueueGetName(osMessageQueueId_t mq_id) {
    if (IsIrqMode() || IsIrqMasked()) {
        return nullptr;
    }
    auto *mq = static_cast<osRtxMessageQueue_t *>(mq_id);

    // Check parameters
    if ((mq == nullptr) || (mq->id != osRtxIdMessageQueue)) {

        //lint -e{904} "Return statement before end of function" [MISRA Note 1]
        return nullptr;
    }

    return mq->name;
}

/// Put a Message into a Queue or timeout if Queue is full.
/// \note API identical to osMessageQueuePut
osStatus_t
osMessageQueuePut(osMessageQueueId_t mq_id, const void *msg_ptr, uint8_t msg_prio, uint32_t timeout) {
    ThreadDispatcher::Mutex mutex;
    auto *mq = static_cast<osRtxMessageQueue_t *>(mq_id);
    osRtxMessage_t *msg;
    osStatus_t status;

    // Check parameters
    if ((mq == nullptr) || (mq->id != osRtxIdMessageQueue) || (msg_ptr == nullptr)) {

        //lint -e{904} "Return statement before end of function" [MISRA Note 1]
        return osErrorParameter;
    }

    bool isISR = IsIrqMode() || IsIrqMasked();

    if (isISR && timeout != 0U) {
        return osErrorParameter;
    }

    // Check if Thread is waiting to receive a Message
    if (!isISR && (mq->thread_list != nullptr) && (mq->thread_list->state == osRtxThreadWaitingMessageGet)) {
        receiveWaitingMessage(mq, msg_ptr, msg_prio, true);

        if (ThreadDispatcher::instance().thread.run.curr->state != osRtxThreadRunning) {
            // other thread has higher priority, switch to it
            ThreadDispatcher::instance().blockUntilWoken();
        }

        status = osOK;
    } else {
        // Try to allocate memory
        msg = static_cast<osRtxMessage_t *>(osRtxMemoryPoolAlloc(&mq->mp_info));
        if (msg != nullptr) {
            // Copy Message
            memcpy(&msg[1], msg_ptr, mq->msg_size);
            // Put Message into Queue
            msg->id = osRtxIdMessage;
            msg->flags = 0U;
            msg->priority = msg_prio;

            if (isISR) {
                // Register post ISR processing
                //lint -e{9079} -e{9087} "cast between pointers to different object types"
                *((const void **) (void *) &msg->prev) = msg_ptr;
                //lint -e{9079} -e{9087} "cast between pointers to different object types"
                *((void **) &msg->next) = mq;
                ThreadDispatcher::instance().queuePostProcess(reinterpret_cast<osRtxObject_t *>(msg));
            } else {
                MessageQueuePut(mq, msg);
            }

            status = osOK;
        } else {
            // No memory available
            if (timeout != 0U) {

                // Suspend current Thread
                if (osRtxThreadWaitEnter(osRtxThreadWaitingMessagePut, timeout)) {
                    auto currentThread = ThreadDispatcher::instance().thread.run.curr;
                    osRtxThreadListPut(reinterpret_cast<osRtxObject_t *>(mq), currentThread);

                    currentThread->queueBlockedData.msg_body.send = msg_ptr;
                    currentThread->queueBlockedData.msg_prio.send = msg_prio;
                    currentThread->waitValPresent = false;

                    ThreadDispatcher::instance().blockUntilWoken();

                    if(currentThread->waitValPresent) {
                        currentThread->waitValPresent = false;
                        return osOK;
                    }
                }
                status = osErrorTimeout;
            } else {
                status = osErrorResource;
            }
        }
    }

    return status;
}

/// Get a Message from a Queue or timeout if Queue is empty.
/// \note API identical to osMessageQueueGet
osStatus_t osMessageQueueGet(osMessageQueueId_t mq_id, void *msg_ptr, uint8_t *msg_prio, uint32_t timeout) {
    ThreadDispatcher::Mutex mutex;
    auto *mq = static_cast<osRtxMessageQueue_t *>(mq_id);
    osRtxMessage_t *msg;
    osStatus_t status;

    // Check parameters
    if ((mq == nullptr) || (mq->id != osRtxIdMessageQueue) || (msg_ptr == nullptr)) {

        //lint -e{904} "Return statement before end of function" [MISRA Note 1]
        return osErrorParameter;
    }

    bool isISR = IsIrqMode() || IsIrqMasked();

    if (isISR && timeout != 0U) {
        return osErrorParameter;
    }

    // Get Message from Queue
    msg = MessageQueueGet(mq);
    if (msg != nullptr) {
        if (!isISR)
            MessageQueueRemove(mq, msg);

        // Copy Message
        memcpy(msg_ptr, &msg[1], mq->msg_size);
        if (msg_prio != nullptr) {
            *msg_prio = msg->priority;
        }

        if (isISR) {
            *((osRtxMessageQueue_t **) (void *) &msg[1]) = mq;
            ThreadDispatcher::instance().queuePostProcess(reinterpret_cast<osRtxObject_t *>(msg));
            return osOK;
        }

        // Free memory
        msg->id = osRtxIdInvalid;
        (void) osRtxMemoryPoolFree(&mq->mp_info, msg);
        // Check if Thread is waiting to send a Message
        if (mq->thread_list != nullptr) {
            // Try to allocate memory
            //lint -e{9079} "conversion from pointer to void to pointer to other type" [MISRA Note 5]
            sendWaitingMessage(mq, true);

            if (ThreadDispatcher::instance().thread.run.curr->state != osRtxThreadRunning) {
                // other thread has higher priority, switch to it
                ThreadDispatcher::instance().blockUntilWoken();
            }
        }
        status = osOK;
    } else {
        // No Message available
        if (timeout != 0U) {

            // Suspend current Thread
            if (osRtxThreadWaitEnter(osRtxThreadWaitingMessageGet, timeout)) {
                auto currentThread = ThreadDispatcher::instance().thread.run.curr;
                osRtxThreadListPut(reinterpret_cast<osRtxObject_t *>(mq), currentThread);

                currentThread->queueBlockedData.msg_body.receive = msg_ptr;
                currentThread->queueBlockedData.msg_prio.receive = msg_prio;
                currentThread->waitValPresent = false;

                ThreadDispatcher::instance().blockUntilWoken();

                if(currentThread->waitValPresent) {
                    currentThread->waitValPresent = false;
                    return osOK;
                }

            }
            status = osErrorTimeout;
        } else {
            status = osErrorResource;
        }
    }

    return status;
}

/// Get maximum number of messages in a Message Queue.
/// \note API identical to osMessageQueueGetCapacity
uint32_t osMessageQueueGetCapacity(osMessageQueueId_t mq_id) {
    auto *mq = static_cast<osRtxMessageQueue_t *>(mq_id);

    // Check parameters
    if ((mq == nullptr) || (mq->id != osRtxIdMessageQueue)) {
        //lint -e{904} "Return statement before end of function" [MISRA Note 1]
        return 0U;
    }

    return mq->mp_info.max_blocks;
}

/// Get maximum message size in a Memory Pool.
/// \note API identical to osMessageQueueGetMsgSize
uint32_t osMessageQueueGetMsgSize(osMessageQueueId_t mq_id) {
    auto *mq = static_cast<osRtxMessageQueue_t *>(mq_id);
    // Check parameters
    if ((mq == nullptr) || (mq->id != osRtxIdMessageQueue)) {

        //lint -e{904} "Return statement before end of function" [MISRA Note 1]
        return 0U;
    }

    return mq->msg_size;
}

/// Get number of queued messages in a Message Queue.
/// \note API identical to osMessageQueueGetCount
uint32_t osMessageQueueGetCount(osMessageQueueId_t mq_id) {
    auto *mq = static_cast<osRtxMessageQueue_t *>(mq_id);
// Check parameters
    if ((mq == nullptr) || (mq->id != osRtxIdMessageQueue)) {

        //lint -e{904} "Return statement before end of function" [MISRA Note 1]
        return 0U;
    }

    return mq->msg_count;
}

/// Get number of available slots for messages in a Message Queue.
/// \note API identical to osMessageQueueGetSpace
uint32_t osMessageQueueGetSpace(osMessageQueueId_t mq_id) {
    auto *mq = static_cast<osRtxMessageQueue_t *>(mq_id);
// Check parameters
    if ((mq == nullptr) || (mq->id != osRtxIdMessageQueue)) {

        //lint -e{904} "Return statement before end of function" [MISRA Note 1]
        return 0U;
    }

    return (mq->mp_info.max_blocks - mq->msg_count);
}

/// Reset a Message Queue to initial empty state.
/// \note API identical to osMessageQueueReset
osStatus_t osMessageQueueReset(osMessageQueueId_t mq_id) {
    ThreadDispatcher::Mutex mutex;
    auto *mq = static_cast<osRtxMessageQueue_t *>(mq_id);
    osRtxMessage_t *msg;

    if (IsIrqMode() || IsIrqMasked()) {
        return osErrorISR;
    }

    // Check parameters
    if ((mq == nullptr) || (mq->id != osRtxIdMessageQueue)) {

        //lint -e{904} "Return statement before end of function" [MISRA Note 1]
        return osErrorParameter;
    }

    // Remove Messages from Queue
    for (;;) {
        // Get Message from Queue
        msg = MessageQueueGet(mq);
        if (msg == nullptr) {
            break;
        }
        MessageQueueRemove(mq, msg);

        // Free memory
        msg->id = osRtxIdInvalid;
        delete msg;
    }

    // Check if Threads are waiting to send Messages
    if ((mq->thread_list != nullptr) && (mq->thread_list->state == osRtxThreadWaitingMessagePut)) {
        do {
            sendWaitingMessage(mq, false);
        } while (mq->thread_list != nullptr);

        ThreadDispatcher::instance().dispatch(nullptr);

        if (ThreadDispatcher::instance().thread.run.curr->state != osRtxThreadRunning) {
            // other thread has higher priority, switch to it
            ThreadDispatcher::instance().blockUntilWoken();
        }
    }


    return osOK;
}

/// Delete a Message Queue object.
/// \note API identical to osMessageQueueDelete
osStatus_t osMessageQueueDelete(osMessageQueueId_t mq_id) {
    ThreadDispatcher::Mutex mutex;
    auto *mq = static_cast<osRtxMessageQueue_t *>(mq_id);
    osRtxThread_t *thread;

    if (IsIrqMode() || IsIrqMasked()) {
        return osErrorISR;
    }

    // Check parameters
    if ((mq == nullptr) || (mq->id != osRtxIdMessageQueue)) {


        //lint -e{904} "Return statement before end of function" [MISRA Note 1]
        return osErrorParameter;
    }

    // Unblock waiting threads
    if (mq->thread_list != nullptr) {
        do {
            thread = osRtxThreadListGet(reinterpret_cast<osRtxObject_t *>(mq));
            osRtxThreadWaitExit(thread, (uint32_t) osErrorResource, false);
        } while (mq->thread_list != nullptr);

        ThreadDispatcher::instance().dispatch(nullptr);

        if (ThreadDispatcher::instance().thread.run.curr->state != osRtxThreadRunning) {
            // other thread has higher priority, switch to it
            ThreadDispatcher::instance().blockUntilWoken();
        }
    }

    // Mark object as invalid
    mq->id = osRtxIdInvalid;

    // Free data memory
    if ((mq->flags & osRtxFlagSystemMemory) != 0U) {
        delete static_cast<uint8_t *>(mq->mp_info.block_base);
    }

    // Free object memory
    if ((mq->flags & osRtxFlagSystemObject) != 0U) {
        delete mq;
    }

    return osOK;
}