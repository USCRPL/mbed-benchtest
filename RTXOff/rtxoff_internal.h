//
// Header for internal RTXOff functions.  Not included by any public header.
//

#ifndef RTXOFF_INTERNAL_H
#define RTXOFF_INTERNAL_H

#include "cmsis_os.h"
#include "rtxoff_os.h"
#include "align.h"

#include <string>
#include <iostream>

#if USE_WINTHREAD == 0
#  include <mutex>
#  include <condition_variable>
#endif


// handle thread yield function
#if USE_WINTHREAD
#define rtxoff_thread_yield() SwitchToThread()
#else
#ifdef HAVE_PTHREAD_YIELD
#define rtxoff_thread_yield() pthread_yield()
#else
#include <sched.h>
#define rtxoff_thread_yield() sched_yield()
#endif
#endif

// RTXOff Debugging (to cerr, so that it is in correct order)
#ifndef RTXOFF_DEBUG
#define RTXOFF_DEBUG 1
#endif

// Kernel functions
void rtxOffDefaultIdleFunc();
void rtxOffDefaultThreadTerminateFunc(osThreadId_t id);

// Thread Library functions
void printThreadLL(osRtxThread_t * head);
osRtxThread_t *osRtxThreadListGet(osRtxObject_t *object);
void osRtxThreadListRemove (osRtxThread_t *thread);
void osRtxThreadListPut (osRtxObject_t *object, osRtxThread_t *thread);
void osRtxThreadListSort (osRtxThread_t *thread);
void osRtxThreadBlock (osRtxThread_t *thread);
void osRtxThreadListUnlink (osRtxThread_t **thread_list, osRtxThread_t *thread);
void osRtxThreadReadyPut (osRtxThread_t * thread);
void *osRtxThreadListRoot (osRtxThread_t *thread);
void osRtxThreadWaitExit(osRtxThread_t *thread, uint64_t ret_val, bool dispatch);
bool osRtxThreadWaitEnter (uint8_t state, uint32_t timeout);

[[noreturn]] void osRtxTimerThread (void *argument);

// Mutex Library functions
void osRtxMutexOwnerRelease (osRtxMutex_t *mutex_list);
void osRtxMutexOwnerRestore (const osRtxMutex_t *mutex, const osRtxThread_t *thread_wakeup);


uint32_t osRtxMemoryPoolInit(osRtxMpInfo_t *mp_info, uint32_t block_count, uint32_t block_size, void *block_mem);
void *osRtxMemoryPoolAlloc(osRtxMpInfo_t *mp_info);
osStatus_t osRtxMemoryPoolFree(osRtxMpInfo_t *mp_info, void *block);


#endif //RTXOFF_INTERNAL_H
