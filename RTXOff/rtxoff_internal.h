//
// Header for internal RTXOff functions.  Not included by any public header.
//

#ifndef RTXOFF_INTERNAL_H
#define RTXOFF_INTERNAL_H

#include "cmsis_os.h"
#include "rtxoff_os.h"

#include <string>
#include <iostream>

#if USE_WINTHREAD == 0
#  include <mutex>
#  include <condition_variable>
#endif

// RTXOff Debugging (to cerr, so that it is in correct order)
#define RTXOFF_DEBUG 0

// Kernel functions
void rtxOffDefaultIdleFunc();

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
void osRtxThreadWaitExit(osRtxThread_t *thread, uint32_t ret_val, bool dispatch);
bool osRtxThreadWaitEnter (uint8_t state, uint32_t timeout);

// Mutex Library functions
void osRtxMutexOwnerRelease (osRtxMutex_t *mutex_list);
void osRtxMutexOwnerRestore (const osRtxMutex_t *mutex, const osRtxThread_t *thread_wakeup);

#endif //RTXOFF_INTERNAL_H
