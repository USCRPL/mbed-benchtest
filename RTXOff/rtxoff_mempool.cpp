//
// RTXOff mempool functionality
//

#include <cstring>
#include "ThreadDispatcher.h"


//  ==== Library functions ====

/// Initialize Memory Pool.
/// \param[in]  mp_info         memory pool info.
/// \param[in]  block_count     maximum number of memory blocks in memory pool.
/// \param[in]  block_size      size of a memory block in bytes.
/// \param[in]  block_mem       pointer to memory for block storage.
/// \return 1 - success, 0 - failure.
uint32_t osRtxMemoryPoolInit(osRtxMpInfo_t *mp_info, uint32_t block_count, uint32_t block_size, void *block_mem) {
    //lint --e{9079} --e{9087} "conversion from pointer to void to pointer to other type" [MISRA Note 6]
    void *mem;
    void *block;

    // Check parameters
    if ((mp_info == nullptr) || (block_count == 0U) || (block_size == 0U) || (block_mem == nullptr)) {
        //lint -e{904} "Return statement before end of function" [MISRA Note 1]
        return 0U;
    }

    // Initialize information structure
    mp_info->max_blocks = block_count;
    mp_info->used_blocks = 0U;
    mp_info->block_size = block_size;
    mp_info->block_base = block_mem;
    mp_info->block_free = block_mem;
    mp_info->block_lim = &(((uint8_t *) block_mem)[block_count * block_size]);

    // add event
    // EvrRtxMemoryBlockInit(mp_info, block_count, block_size, block_mem);

    // Link all free blocks
    mem = block_mem;
    while (--block_count != 0U) {
        block = &((uint8_t *) mem)[block_size];
        *((void **) mem) = block;
        mem = block;
    }
    *((void **) mem) = nullptr;

    return 1U;
}

/// Allocate a memory block from a Memory Pool.
/// \param[in]  mp_info         memory pool info.
/// \return address of the allocated memory block or nullptr in case of no memory is available.
void *osRtxMemoryPoolAlloc(osRtxMpInfo_t *mp_info) {

    void *block;

    if (mp_info == nullptr) {
        // add event
        // EvrRtxMemoryBlockAlloc(nullptr, nullptr);
        //lint -e{904} "Return statement before end of function" [MISRA Note 1]
        return nullptr;
    }

    {
        ThreadDispatcher::Mutex mutex;
        block = mp_info->block_free;
        if (block != nullptr) {
            mp_info->block_free = *((void **) block);
            mp_info->used_blocks++;
        }
    }
    // add event
    // EvrRtxMemoryBlockAlloc(mp_info, block);

    return block;
}

/// Return an allocated memory block back to a Memory Pool.
/// \param[in]  mp_info         memory pool info.
/// \param[in]  block           address of the allocated memory block to be returned to the memory pool.
/// \return status code that indicates the execution status of the function.
osStatus_t osRtxMemoryPoolFree(osRtxMpInfo_t *mp_info, void *block) {

    //lint -e{946} "Relational operator applied to pointers"
    if ((mp_info == nullptr) || (block < mp_info->block_base) || (block >= mp_info->block_lim)) {
        // add event
        // EvrRtxMemoryBlockFree(mp_info, block, (int32_t) osErrorParameter);
        //lint -e{904} "Return statement before end of function" [MISRA Note 1]
        return (mp_info == nullptr) ? osErrorParameter : static_cast<osStatus_t>(osErrorValue);
    }

    {
        ThreadDispatcher::Mutex mutex;
        //lint --e{9079} --e{9087} "conversion from pointer to void to pointer to other type"
        *((void **) block) = mp_info->block_free;
        mp_info->block_free = block;
        mp_info->used_blocks--;
    }

    // add event
    // EvrRtxMemoryBlockFree(mp_info, block, (int32_t) osOK);

    return osOK;
}


//  ==== Post ISR processing ====

/// Memory Pool post ISR processing.
/// \param[in]  mp              memory pool object.
static void osRtxMemoryPoolPostProcess(osRtxMemoryPool_t *mp) {
    void *block;
    osRtxThread_t *thread;

    // Check if Thread is waiting to allocate memory
    if (mp->thread_list != nullptr) {
        // Allocate memory
        block = osRtxMemoryPoolAlloc(&mp->mp_info);
        if (block != nullptr) {
            // Wakeup waiting Thread with highest Priority
            thread = osRtxThreadListGet(reinterpret_cast<osRtxObject_t *>(mp));
            //lint -e{923} "cast from pointer to unsigned int"
            osRtxThreadWaitExit(thread, (uint64_t) block, false);
            // add event
            // EvrRtxMemoryPoolAllocated(mp, block);
        }
    }
}

//  ==== Service Calls ====

/// Create and Initialize a Memory Pool object.
/// \note API identical to osMemoryPoolNew
static osMemoryPoolId_t svcRtxMemoryPoolNew(uint32_t block_count, uint32_t block_size, const osMemoryPoolAttr_t *attr) {
    osRtxMemoryPool_t *mp;
    void *mp_mem;
    uint32_t mp_size;
    uint32_t b_count;
    uint32_t b_size;
    uint32_t size;
    uint8_t flags;
    const char *name;

    // Check parameters
    if ((block_count == 0U) || (block_size == 0U)) {
        // add event
        // EvrRtxMemoryPoolError(nullptr, (int32_t) osErrorParameter);
        //lint -e{904} "Return statement before end of function" [MISRA Note 1]
        return nullptr;
    }
    b_count = block_count;
    b_size = align(block_size);
    if ((__builtin_clz(b_count) + __builtin_clz(b_size)) < 32U) {
        // add event
        // EvrRtxMemoryPoolError(nullptr, (int32_t) osErrorParameter);
        //lint -e{904} "Return statement before end of function" [MISRA Note 1]
        return nullptr;
    }

    size = b_count * b_size;

    // Process attributes
    if (attr != nullptr) {
        name = attr->name;
        //lint -e{9079} "conversion from pointer to void to pointer to other type" [MISRA Note 6]
        mp = static_cast<osRtxMemoryPool_t *>(attr->cb_mem);
        //lint -e{9079} "conversion from pointer to void to pointer to other type" [MISRA Note 6]
        mp_mem = attr->mp_mem;
        mp_size = attr->mp_size;
        if (mp != nullptr) {
            //lint -e(923) -e(9078) "cast from pointer to unsigned int" [MISRA Note 7]
            if (!is_aligned_p(mp) || (attr->cb_size < sizeof(osRtxMemoryPool_t))) {
                // add event
                // EvrRtxMemoryPoolError(nullptr, osRtxErrorInvalidControlBlock);
                //lint -e{904} "Return statement before end of function" [MISRA Note 1]
                return nullptr;
            }
        } else {
            if (attr->cb_size != 0U) {
                // add event
                // EvrRtxMemoryPoolError(nullptr, osRtxErrorInvalidControlBlock);
                //lint -e{904} "Return statement before end of function" [MISRA Note 1]
                return nullptr;
            }
        }
        if (mp_mem != nullptr) {
            //lint -e(923) -e(9078) "cast from pointer to unsigned int" [MISRA Note 7]
            if (!is_aligned_p(mp_mem) || (mp_size < size)) {
                // add event
                // EvrRtxMemoryPoolError(nullptr, osRtxErrorInvalidDataMemory);
                //lint -e{904} "Return statement before end of function" [MISRA Note 1]
                return nullptr;
            }
        } else {
            if (mp_size != 0U) {
                // add event
                // EvrRtxMemoryPoolError(nullptr, osRtxErrorInvalidDataMemory);
                //lint -e{904} "Return statement before end of function" [MISRA Note 1]
                return nullptr;
            }
        }
    } else {
        name = nullptr;
        mp = nullptr;
        mp_mem = nullptr;
    }

    // Allocate object memory if not provided
    if (mp == nullptr) {
        mp = new osRtxMemoryPool_t;
        flags = osRtxFlagSystemObject;
    } else {
        flags = 0U;
    }

    // Allocate data memory if not provided
    if ((mp != nullptr) && (mp_mem == nullptr)) {
        //lint -e{9079} "conversion from pointer to void to pointer to other type" [MISRA Note 5]
        mp_mem = new uint8_t[size];
        if (mp_mem == nullptr) {
            if ((flags & osRtxFlagSystemObject) != 0U) {
                delete mp;
            }
            mp = nullptr;
        } else {
            memset(mp_mem, 0, size);
        }
        flags |= osRtxFlagSystemMemory;
    }

    if (mp != nullptr) {
        // Initialize control block
        mp->id = osRtxIdMemoryPool;
        mp->flags = flags;
        mp->name = name;
        mp->thread_list = nullptr;
        (void) osRtxMemoryPoolInit(&mp->mp_info, b_count, b_size, mp_mem);

        // Register post ISR processing function
        ThreadDispatcher::instance().post_process.memory_pool = osRtxMemoryPoolPostProcess;

        // add event
        // EvrRtxMemoryPoolCreated(mp, mp->name);
    } else {
        // add event
        // EvrRtxMemoryPoolError(nullptr, (int32_t) osErrorNoMemory);
    }

    return mp;
}

/// Get name of a Memory Pool object.
/// \note API identical to osMemoryPoolGetName
static const char *svcRtxMemoryPoolGetName(osMemoryPoolId_t mp_id) {
    auto *mp = reinterpret_cast<osRtxMemoryPool_t *>(mp_id);

    // Check parameters
    if ((mp == nullptr) || (mp->id != osRtxIdMemoryPool)) {
        // add event
        // EvrRtxMemoryPoolGetName(mp, nullptr);
        //lint -e{904} "Return statement before end of function" [MISRA Note 1]
        return nullptr;
    }

    // add event
    // EvrRtxMemoryPoolGetName(mp, mp->name);

    return mp->name;
}

/// Allocate a memory block from a Memory Pool.
/// \note API identical to osMemoryPoolAlloc
static void *svcRtxMemoryPoolAlloc(osMemoryPoolId_t mp_id, uint32_t timeout) {
    auto *mp = reinterpret_cast<osRtxMemoryPool_t *>(mp_id);
    void *block;

    // Check parameters
    if ((mp == nullptr) || (mp->id != osRtxIdMemoryPool)) {
        // add event
        // EvrRtxMemoryPoolError(mp, (int32_t) osErrorParameter);
        //lint -e{904} "Return statement before end of function" [MISRA Note 1]
        return nullptr;
    }

    // Allocate memory
    block = osRtxMemoryPoolAlloc(&mp->mp_info);
    if (block != nullptr) {
        // add event
        // EvrRtxMemoryPoolAllocated(mp, block);
    } else {
        // No memory available
        if (timeout != 0U) {
            // add event
            // EvrRtxMemoryPoolAllocPending(mp, timeout);
            // Suspend current Thread
            if (osRtxThreadWaitEnter(osRtxThreadWaitingMemoryPool, timeout)) {
                osRtxThreadListPut(reinterpret_cast<osRtxObject_t *>(mp), ThreadDispatcher::instance().thread.run.curr);
            } else {
                // add event
                // EvrRtxMemoryPoolAllocTimeout(mp);
            }
        } else {
            // add event
            // EvrRtxMemoryPoolAllocFailed(mp);
        }
    }

    return block;
}

/// Return an allocated memory block back to a Memory Pool.
/// \note API identical to osMemoryPoolFree
static osStatus_t svcRtxMemoryPoolFree(osMemoryPoolId_t mp_id, void *block) {
    auto *mp = reinterpret_cast<osRtxMemoryPool_t *>(mp_id);
    void *block0;
    osRtxThread_t *thread;
    osStatus_t status;

    // Check parameters
    if ((mp == nullptr) || (mp->id != osRtxIdMemoryPool)) {
        // add event
        // EvrRtxMemoryPoolError(mp, (int32_t) osErrorParameter);
        //lint -e{904} "Return statement before end of function" [MISRA Note 1]
        return osErrorParameter;
    }

    // Free memory
    status = osRtxMemoryPoolFree(&mp->mp_info, block);
    if (status == osOK) {
        // add event
        // EvrRtxMemoryPoolDeallocated(mp, block);
        // Check if Thread is waiting to allocate memory
        if (mp->thread_list != nullptr) {
            // Allocate memory
            block0 = osRtxMemoryPoolAlloc(&mp->mp_info);
            if (block0 != nullptr) {
                // Wakeup waiting Thread with highest Priority
                thread = osRtxThreadListGet(reinterpret_cast<osRtxObject_t *>(mp));
                //lint -e{923} "cast from pointer to unsigned int"
                osRtxThreadWaitExit(thread, (uint64_t) block0, true);
                // add event
                // EvrRtxMemoryPoolAllocated(mp, block0);
            }
        }
    } else {
        // add event
        // EvrRtxMemoryPoolFreeFailed(mp, block);
    }

    return status;
}

/// Get maximum number of memory blocks in a Memory Pool.
/// \note API identical to osMemoryPoolGetCapacity
static uint32_t svcRtxMemoryPoolGetCapacity(osMemoryPoolId_t mp_id) {
    auto *mp = reinterpret_cast<osRtxMemoryPool_t *>(mp_id);
    // Check parameters
    if ((mp == nullptr) || (mp->id != osRtxIdMemoryPool)) {
        // add event
        // EvrRtxMemoryPoolGetCapacity(mp, 0U);
        //lint -e{904} "Return statement before end of function" [MISRA Note 1]
        return 0U;
    }

    // add event
    // EvrRtxMemoryPoolGetCapacity(mp, mp->mp_info.max_blocks);

    return mp->mp_info.max_blocks;
}

/// Get memory block size in a Memory Pool.
/// \note API identical to osMemoryPoolGetBlockSize
static uint32_t svcRtxMemoryPoolGetBlockSize(osMemoryPoolId_t mp_id) {
    auto *mp = reinterpret_cast<osRtxMemoryPool_t *>(mp_id);
    // Check parameters
    if ((mp == nullptr) || (mp->id != osRtxIdMemoryPool)) {
        // add event
        // EvrRtxMemoryPoolGetBlockSize(mp, 0U);
        //lint -e{904} "Return statement before end of function" [MISRA Note 1]
        return 0U;
    }

    // add event
    // EvrRtxMemoryPoolGetBlockSize(mp, mp->mp_info.block_size);

    return mp->mp_info.block_size;
}

/// Get number of memory blocks used in a Memory Pool.
/// \note API identical to osMemoryPoolGetCount
static uint32_t svcRtxMemoryPoolGetCount(osMemoryPoolId_t mp_id) {
    auto *mp = reinterpret_cast<osRtxMemoryPool_t *>(mp_id);
    // Check parameters
    if ((mp == nullptr) || (mp->id != osRtxIdMemoryPool)) {
        // add event
        // EvrRtxMemoryPoolGetCount(mp, 0U);
        //lint -e{904} "Return statement before end of function" [MISRA Note 1]
        return 0U;
    }

    // add event
    // EvrRtxMemoryPoolGetCount(mp, mp->mp_info.used_blocks);

    return mp->mp_info.used_blocks;
}

/// Get number of memory blocks available in a Memory Pool.
/// \note API identical to osMemoryPoolGetSpace
static uint32_t svcRtxMemoryPoolGetSpace(osMemoryPoolId_t mp_id) {
    auto *mp = reinterpret_cast<osRtxMemoryPool_t *>(mp_id);
    // Check parameters
    if ((mp == nullptr) || (mp->id != osRtxIdMemoryPool)) {
        // add event
        // EvrRtxMemoryPoolGetSpace(mp, 0U);
        //lint -e{904} "Return statement before end of function" [MISRA Note 1]
        return 0U;
    }

    // add event
    // EvrRtxMemoryPoolGetSpace(mp, mp->mp_info.max_blocks - mp->mp_info.used_blocks);

    return (mp->mp_info.max_blocks - mp->mp_info.used_blocks);
}

/// Delete a Memory Pool object.
/// \note API identical to osMemoryPoolDelete
static osStatus_t svcRtxMemoryPoolDelete(osMemoryPoolId_t mp_id) {
    auto *mp = reinterpret_cast<osRtxMemoryPool_t *>(mp_id);
    osRtxThread_t *thread;

    // Check parameters
    if ((mp == nullptr) || (mp->id != osRtxIdMemoryPool)) {
        // add event
        // EvrRtxMemoryPoolError(mp, (int32_t) osErrorParameter);
        //lint -e{904} "Return statement before end of function" [MISRA Note 1]
        return osErrorParameter;
    }

    // Unblock waiting threads
    if (mp->thread_list != nullptr) {
        do {
            thread = osRtxThreadListGet(reinterpret_cast<osRtxObject_t *>(mp));
            osRtxThreadWaitExit(thread, 0U, false);
        } while (mp->thread_list != nullptr);

        // at this point a new thread might potentially take over
        ThreadDispatcher::instance().dispatch(nullptr);

        if (ThreadDispatcher::instance().thread.run.curr->state != osRtxThreadRunning) {
            // other thread has higher priority, switch to it
            ThreadDispatcher::instance().blockUntilWoken();
        }

    }

    // Mark object as invalid
    mp->id = osRtxIdInvalid;

    // Free data memory
    if ((mp->flags & osRtxFlagSystemMemory) != 0U) {
        delete static_cast<uint8_t *>(mp->mp_info.block_base);
    }

    // Free object memory
    if ((mp->flags & osRtxFlagSystemObject) != 0U) {
        delete mp;
    }

    // add event
    // EvrRtxMemoryPoolDestroyed(mp);

    return osOK;
}



//  ==== ISR Calls ====

/// Allocate a memory block from a Memory Pool.
/// \note API identical to osMemoryPoolAlloc
static inline
void *isrRtxMemoryPoolAlloc(osMemoryPoolId_t mp_id, uint32_t timeout) {
    auto *mp = reinterpret_cast<osRtxMemoryPool_t *>(mp_id);
    void *block;

    // Check parameters
    if ((mp == nullptr) || (mp->id != osRtxIdMemoryPool) || (timeout != 0U)) {
        // add event
        // EvrRtxMemoryPoolError(mp, (int32_t) osErrorParameter);
        //lint -e{904} "Return statement before end of function" [MISRA Note 1]
        return nullptr;
    }

    // Allocate memory
    block = osRtxMemoryPoolAlloc(&mp->mp_info);
    if (block == nullptr) {
        // add event
        // EvrRtxMemoryPoolAllocFailed(mp);
    } else {
        // add event
        // EvrRtxMemoryPoolAllocated(mp, block);
    }

    return block;
}

/// Return an allocated memory block back to a Memory Pool.
/// \note API identical to osMemoryPoolFree
static inline
osStatus_t
isrRtxMemoryPoolFree(osMemoryPoolId_t mp_id, void *block) {
    osRtxMemoryPool_t *mp = reinterpret_cast<osRtxMemoryPool_t *>(mp_id);
    osStatus_t status;

    // Check parameters
    if ((mp == nullptr) || (mp->id != osRtxIdMemoryPool)) {
        // add event
        // EvrRtxMemoryPoolError(mp, (int32_t) osErrorParameter);
        //lint -e{904} "Return statement before end of function" [MISRA Note 1]
        return osErrorParameter;
    }

    // Free memory
    status = osRtxMemoryPoolFree(&mp->mp_info, block);
    if (status == osOK) {
        // Register post ISR processing

        ThreadDispatcher::instance().queuePostProcess(reinterpret_cast<osRtxObject_t *>(mp));
        // add event
        // EvrRtxMemoryPoolDeallocated(mp, block);
    } else {
        // add event
        // EvrRtxMemoryPoolFreeFailed(mp, block);
}

return
status;
}


//  ==== Public API ====

/// Create and Initialize a Memory Pool object.
osMemoryPoolId_t osMemoryPoolNew(uint32_t block_count, uint32_t block_size, const osMemoryPoolAttr_t *attr) {
    osMemoryPoolId_t mp_id;

    // add event
    // EvrRtxMemoryPoolNew(block_count, block_size, attr);
    if (IsIrqMode() || IsIrqMasked()) {
        // add event
        // EvrRtxMemoryPoolError(nullptr, (int32_t) osErrorISR);
        mp_id = nullptr;
    } else {
        mp_id = svcRtxMemoryPoolNew(block_count, block_size, attr);
    }
    return mp_id;
}

/// Get name of a Memory Pool object.
const char *osMemoryPoolGetName(osMemoryPoolId_t mp_id) {
    const char *name;

    if (IsIrqMode() || IsIrqMasked()) {
        // add event
        // EvrRtxMemoryPoolGetName(mp_id, nullptr);
        name = nullptr;
    } else {
        name = svcRtxMemoryPoolGetName(mp_id);
    }
    return name;
}

/// Allocate a memory block from a Memory Pool.
void *osMemoryPoolAlloc(osMemoryPoolId_t mp_id, uint32_t timeout) {
    void *memory;

    // add event
    // EvrRtxMemoryPoolAlloc(mp_id, timeout);
    if (IsIrqMode() || IsIrqMasked()) {
        memory = isrRtxMemoryPoolAlloc(mp_id, timeout);
    } else {
        memory = svcRtxMemoryPoolAlloc(mp_id, timeout);
    }
    return memory;
}

/// Return an allocated memory block back to a Memory Pool.
osStatus_t osMemoryPoolFree(osMemoryPoolId_t mp_id, void *block) {
    osStatus_t status;

    // add event
    // EvrRtxMemoryPoolFree(mp_id, block);
    if (IsIrqMode() || IsIrqMasked()) {
        status = isrRtxMemoryPoolFree(mp_id, block);
    } else {
        status = svcRtxMemoryPoolFree(mp_id, block);
    }
    return status;
}

/// Get maximum number of memory blocks in a Memory Pool.
uint32_t osMemoryPoolGetCapacity(osMemoryPoolId_t mp_id) {
    uint32_t capacity;

    if (IsIrqMode() || IsIrqMasked()) {
        capacity = svcRtxMemoryPoolGetCapacity(mp_id);
    } else {
        capacity = svcRtxMemoryPoolGetCapacity(mp_id);
    }
    return capacity;
}

/// Get memory block size in a Memory Pool.
uint32_t osMemoryPoolGetBlockSize(osMemoryPoolId_t mp_id) {
    uint32_t block_size;

    if (IsIrqMode() || IsIrqMasked()) {
        block_size = svcRtxMemoryPoolGetBlockSize(mp_id);
    } else {
        block_size = svcRtxMemoryPoolGetBlockSize(mp_id);
    }
    return block_size;
}

/// Get number of memory blocks used in a Memory Pool.
uint32_t osMemoryPoolGetCount(osMemoryPoolId_t mp_id) {
    uint32_t count;

    if (IsIrqMode() || IsIrqMasked()) {
        count = svcRtxMemoryPoolGetCount(mp_id);
    } else {
        count = svcRtxMemoryPoolGetCount(mp_id);
    }
    return count;
}

/// Get number of memory blocks available in a Memory Pool.
uint32_t osMemoryPoolGetSpace(osMemoryPoolId_t mp_id) {
    uint32_t space;

    if (IsIrqMode() || IsIrqMasked()) {
        space = svcRtxMemoryPoolGetSpace(mp_id);
    } else {
        space = svcRtxMemoryPoolGetSpace(mp_id);
    }
    return space;
}

/// Delete a Memory Pool object.
osStatus_t osMemoryPoolDelete(osMemoryPoolId_t mp_id) {
    osStatus_t status;

    // add event
    // EvrRtxMemoryPoolDelete(mp_id);
    if (IsIrqMode() || IsIrqMasked()) {
        // add event
        // EvrRtxMemoryPoolError(mp_id, (int32_t) osErrorISR);
        status = osErrorISR;
    } else {
        status = svcRtxMemoryPoolDelete(mp_id);
    }
    return status;
}
