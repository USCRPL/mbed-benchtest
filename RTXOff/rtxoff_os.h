/*
 * Copyright (c) 2013-2019 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the License); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * -----------------------------------------------------------------------------
 *
 * Project:     CMSIS-RTOS RTX
 * Title:       RTX OS definitions
 *
 * -----------------------------------------------------------------------------
 */
 
#ifndef RTX_OS_H_
#define RTX_OS_H_
 
#include <stdint.h>
#include <stddef.h>
#include "cmsis_os2.h"

// include OS threading library
#if USE_WINTHREAD
#  define WIN32_LEAN_AND_MEAN 1
#  include <Windows.h>
#else
#  include <pthread.h>
#  include <mutex>
#  include <condition_variable>
#  include <signal.h>
#endif

#ifdef  __cplusplus
extern "C"
{
#endif

// Language note: This header gets included from both C and C++.
// So, all data structures here must be C compatible -- no classes allowed!
 
 
/// Kernel Information
#define osRtxVersionAPI      20010003   ///< API version (2.1.3)
#define osRtxVersionKernel   00010000   ///< Kernel version (0.1.0)
#define osRtxKernelId     "RTXOff V0.1" ///< Kernel identification string
 
 
//  ==== Common definitions ====
 
/// Object Identifier definitions
#define osRtxIdInvalid          0x00U
#define osRtxIdThread           0xF1U
#define osRtxIdTimer            0xF2U
#define osRtxIdEventFlags       0xF3U
#define osRtxIdMutex            0xF5U
#define osRtxIdSemaphore        0xF6U
#define osRtxIdMemoryPool       0xF7U
#define osRtxIdMessage          0xF9U
#define osRtxIdMessageQueue     0xFAU
 
/// Object Flags definitions
#define osRtxFlagSystemObject   0x01U
#define osRtxFlagSystemMemory   0x02U
 
 
//  ==== Kernel definitions ====
 
/// Kernel State definitions
#define osRtxKernelInactive             ((uint8_t)osKernelInactive)
#define osRtxKernelReady                ((uint8_t)osKernelReady)
#define osRtxKernelRunning              ((uint8_t)osKernelRunning)
#define osRtxKernelLocked               ((uint8_t)osKernelLocked)
#define osRtxKernelSuspended            ((uint8_t)osKernelSuspended)
 
 
//  ==== Thread definitions ====
 
/// Thread State definitions (extending osThreadState)
#define osRtxThreadStateMask            0x0FU
 
#define osRtxThreadInactive             ((uint8_t)osThreadInactive)
#define osRtxThreadReady                ((uint8_t)osThreadReady)
#define osRtxThreadRunning              ((uint8_t)osThreadRunning)
#define osRtxThreadBlocked              ((uint8_t)osThreadBlocked)
#define osRtxThreadTerminated           ((uint8_t)osThreadTerminated)
 
#define osRtxThreadWaitingDelay         ((uint8_t)(osRtxThreadBlocked | 0x10U))
#define osRtxThreadWaitingJoin          ((uint8_t)(osRtxThreadBlocked | 0x20U))
#define osRtxThreadWaitingThreadFlags   ((uint8_t)(osRtxThreadBlocked | 0x30U))
#define osRtxThreadWaitingEventFlags    ((uint8_t)(osRtxThreadBlocked | 0x40U))
#define osRtxThreadWaitingMutex         ((uint8_t)(osRtxThreadBlocked | 0x50U))
#define osRtxThreadWaitingSemaphore     ((uint8_t)(osRtxThreadBlocked | 0x60U))
#define osRtxThreadWaitingMemoryPool    ((uint8_t)(osRtxThreadBlocked | 0x70U))
#define osRtxThreadWaitingMessageGet    ((uint8_t)(osRtxThreadBlocked | 0x80U))
#define osRtxThreadWaitingMessagePut    ((uint8_t)(osRtxThreadBlocked | 0x90U))
 
/// Thread Flags definitions
#define osRtxThreadFlagDefStack 0x10U   ///< Default Stack flag
 
/// Stack Marker definitions
#define osRtxStackMagicWord     0xE25A2EA5U ///< Stack Magic Word (Stack Base)
#define osRtxStackFillPattern   0xCCCCCCCCU ///< Stack Fill Pattern 
 
/// Thread Control Block
typedef struct osRtxThread_s {
  uint8_t                          id;  ///< Object Identifier
  uint8_t                       state;  ///< Object State
  uint8_t                       flags;  ///< Object Flags
  uint8_t                        attr;  ///< Object Attributes
  const char                    *name;  ///< Object Name
  struct osRtxThread_s   *thread_next;  ///< Link pointer to next Thread in Object list
  struct osRtxThread_s   *thread_prev;  ///< Link pointer to previous Thread in Object list
  struct osRtxThread_s    *delay_next;  ///< Link pointer to next Thread in Delay list
  struct osRtxThread_s    *delay_prev;  ///< Link pointer to previous Thread in Delay list
  struct osRtxThread_s   *thread_join;  ///< Thread waiting to join until this thread finishes
  uint32_t                      delay;  ///< Delay Time.  When in delay list, this gives the ADDITIONAL delay time from the previous thread.
  int8_t                     priority;  ///< Thread Priority  Effective priority accounting for mutexes the thread holds.
  int8_t                priority_base;  ///< Base Priority.  Priority that the thread was set to.
  uint8_t                 stack_frame;  ///< Stack Frame (EXC_RETURN[7..0])
  uint8_t               flags_options;  ///< Thread/Event Flags Options
  uint32_t                 wait_flags;  ///< Waiting Thread/Event Flags
  uint32_t               thread_flags;  ///< Thread Flags
  struct osRtxMutex_s     *mutex_list;  ///< Link pointer to list of owned Mutexes

  uint32_t waitExitVal;                 // return value passed from osRtxThreadWaitExit().  Set only when this function is called, not when a thread wait timeout expires.
  uint8_t waitValPresent;               // Whether above value is present.

#if USE_WINTHREAD
  HANDLE osThread;
#else
#error TODO!
#endif
} osRtxThread_t;
 
 
//  ==== Timer definitions ====
 
/// Timer State definitions
#define osRtxTimerInactive      0x00U   ///< Timer Inactive
#define osRtxTimerStopped       0x01U   ///< Timer Stopped
#define osRtxTimerRunning       0x02U   ///< Timer Running
 
/// Timer Type definitions
#define osRtxTimerPeriodic      ((uint8_t)osTimerPeriodic)
 
/// Timer Function Information
typedef struct {
  osTimerFunc_t                  func;  ///< Function Pointer
  void                           *arg;  ///< Function Argument
} osRtxTimerFinfo_t;
 
/// Timer Control Block
typedef struct osRtxTimer_s {
  uint8_t                          id;  ///< Object Identifier
  uint8_t                       state;  ///< Object State
  uint8_t                       flags;  ///< Object Flags
  uint8_t                        type;  ///< Timer Type (Periodic/One-shot)
  const char                    *name;  ///< Object Name
  struct osRtxTimer_s           *prev;  ///< Pointer to previous active Timer
  struct osRtxTimer_s           *next;  ///< Pointer to next active Timer
  uint32_t                       tick;  ///< Timer current Tick
  uint32_t                       load;  ///< Timer Load value
  osRtxTimerFinfo_t             finfo;  ///< Timer Function Info
} osRtxTimer_t;
 
 
//  ==== Event Flags definitions ====
 
/// Event Flags Control Block
typedef struct {
  uint8_t                          id;  ///< Object Identifier
  uint8_t              reserved_state;  ///< Object State (not used)
  uint8_t                       flags;  ///< Object Flags
  uint8_t                    reserved;
  const char                    *name;  ///< Object Name
  osRtxThread_t          *thread_list;  ///< Waiting Threads List
  uint32_t                event_flags;  ///< Event Flags
} osRtxEventFlags_t;
 
 
//  ==== Mutex definitions ====
 
/// Mutex Control Block
typedef struct osRtxMutex_s {
  uint8_t                          id;  ///< Object Identifier
  uint8_t              reserved_state;  ///< Object State (not used)
  uint8_t                       flags;  ///< Object Flags
  uint8_t                        attr;  ///< Object Attributes
  const char                    *name;  ///< Object Name
  osRtxThread_t          *thread_list;  ///< Waiting Threads List
  osRtxThread_t         *owner_thread;  ///< Owner Thread
  struct osRtxMutex_s     *owner_prev;  ///< Pointer to previous owned Mutex
  struct osRtxMutex_s     *owner_next;  ///< Pointer to next owned Mutex
  uint8_t                        lock;  ///< Lock counter
  uint8_t                  padding[3];
} osRtxMutex_t;
 
 
//  ==== Semaphore definitions ====
 
/// Semaphore Control Block
typedef struct {
  uint8_t                          id;  ///< Object Identifier
  uint8_t              reserved_state;  ///< Object State (not used)
  uint8_t                       flags;  ///< Object Flags
  uint8_t                    reserved;
  const char                    *name;  ///< Object Name
  osRtxThread_t          *thread_list;  ///< Waiting Threads List
  uint16_t                     tokens;  ///< Current number of tokens
  uint16_t                 max_tokens;  ///< Maximum number of tokens
} osRtxSemaphore_t;
 
 
//  ==== Memory Pool definitions ====
 
/// Memory Pool Information
typedef struct {
  uint32_t                 max_blocks;  ///< Maximum number of Blocks
  uint32_t                used_blocks;  ///< Number of used Blocks
  uint32_t                 block_size;  ///< Block Size
  void                    *block_base;  ///< Block Memory Base Address
  void                     *block_lim;  ///< Block Memory Limit Address
  void                    *block_free;  ///< First free Block Address
} osRtxMpInfo_t;
 
/// Memory Pool Control Block
typedef struct {
  uint8_t                          id;  ///< Object Identifier
  uint8_t              reserved_state;  ///< Object State (not used)
  uint8_t                       flags;  ///< Object Flags
  uint8_t                    reserved;
  const char                    *name;  ///< Object Name
  osRtxThread_t          *thread_list;  ///< Waiting Threads List
  osRtxMpInfo_t               mp_info;  ///< Memory Pool Info
} osRtxMemoryPool_t;
 
 
//  ==== Message Queue definitions ====
 
/// Message Control Block
typedef struct osRtxMessage_s {
  uint8_t                          id;  ///< Object Identifier
  uint8_t              reserved_state;  ///< Object State (not used)
  uint8_t                       flags;  ///< Object Flags
  uint8_t                    priority;  ///< Message Priority
  struct osRtxMessage_s         *prev;  ///< Pointer to previous Message
  struct osRtxMessage_s         *next;  ///< Pointer to next Message
} osRtxMessage_t;
 
/// Message Queue Control Block
typedef struct {
  uint8_t                          id;  ///< Object Identifier
  uint8_t              reserved_state;  ///< Object State (not used)
  uint8_t                       flags;  ///< Object Flags
  uint8_t                    reserved;
  const char                    *name;  ///< Object Name
  osRtxThread_t          *thread_list;  ///< Waiting Threads List
  osRtxMpInfo_t               mp_info;  ///< Memory Pool Info
  uint32_t                   msg_size;  ///< Message Size
  uint32_t                  msg_count;  ///< Number of queued Messages
  osRtxMessage_t           *msg_first;  ///< Pointer to first Message
  osRtxMessage_t            *msg_last;  ///< Pointer to last Message
} osRtxMessageQueue_t;
 
 
//  ==== Generic Object definitions ====
 
/// Generic Object Control Block
typedef struct {
  uint8_t                          id;  ///< Object Identifier
  uint8_t                       state;  ///< Object State
  uint8_t                       flags;  ///< Object Flags
  uint8_t                    reserved;
  const char                    *name;  ///< Object Name
  osRtxThread_t          *thread_list;  ///< Threads List
} osRtxObject_t;
 
 
//  ==== OS Runtime Information definitions ====

/// OS Runtime Object Memory Usage structure
typedef struct {
  uint32_t cnt_alloc;                   ///< Counter for alloc
  uint32_t cnt_free;                    ///< Counter for free
  uint32_t max_used;                    ///< Maximum used
} osRtxObjectMemUsage_t;
 
/// OS Runtime Object Memory Usage variables
extern osRtxObjectMemUsage_t osRtxThreadMemUsage;
extern osRtxObjectMemUsage_t osRtxTimerMemUsage;
extern osRtxObjectMemUsage_t osRtxEventFlagsMemUsage;
extern osRtxObjectMemUsage_t osRtxMutexMemUsage;
extern osRtxObjectMemUsage_t osRtxSemaphoreMemUsage;
extern osRtxObjectMemUsage_t osRtxMemoryPoolMemUsage;
extern osRtxObjectMemUsage_t osRtxMessageQueueMemUsage;
 
 
//  ==== OS API definitions ====
 
// Object Limits definitions
#define osRtxThreadFlagsLimit    31U    ///< number of Thread Flags available per thread
#define osRtxEventFlagsLimit     31U    ///< number of Event Flags available per object
#define osRtxMutexLockLimit      255U   ///< maximum number of recursive mutex locks
#define osRtxSemaphoreTokenLimit 65535U ///< maximum number of tokens per semaphore
 
// Control Block sizes
#define osRtxThreadCbSize        sizeof(osRtxThread_t)
#define osRtxTimerCbSize         sizeof(osRtxTimer_t)
#define osRtxEventFlagsCbSize    sizeof(osRtxEventFlags_t)
#define osRtxMutexCbSize         sizeof(osRtxMutex_t)
#define osRtxSemaphoreCbSize     sizeof(osRtxSemaphore_t)
#define osRtxMemoryPoolCbSize    sizeof(osRtxMemoryPool_t)
#define osRtxMessageQueueCbSize  sizeof(osRtxMessageQueue_t)
 
/// Memory size in bytes for Memory Pool storage.
/// \param         block_count   maximum number of memory blocks in memory pool.
/// \param         block_size    memory block size in bytes.
#define osRtxMemoryPoolMemSize(block_count, block_size) \
  (4*(block_count)*(((block_size)+3)/4))
 
/// Memory size in bytes for Message Queue storage.
/// \param         msg_count     maximum number of messages in queue.
/// \param         msg_size      maximum message size in bytes.
#define osRtxMessageQueueMemSize(msg_count, msg_size) \
  (4*(msg_count)*(3+(((msg_size)+3)/4)))
 
 
//  ==== OS External Functions ====
 
// OS Error Codes
#define osRtxErrorStackUnderflow        1U  ///< Stack overflow, i.e. stack pointer below its lower memory limit for descending stacks.
#define osRtxErrorISRQueueOverflow      2U  ///< ISR Queue overflow detected when inserting object.
#define osRtxErrorTimerQueueOverflow    3U  ///< User Timer Callback Queue overflow detected for timer.
#define osRtxErrorClibSpace             4U  ///< Standard C/C++ library libspace not available: increase \c OS_THREAD_LIBSPACE_NUM.
#define osRtxErrorClibMutex             5U  ///< Standard C/C++ library mutex initialization failed.
 
/// OS Error Callback function
extern uint32_t osRtxErrorNotify (uint32_t code, void *object_id);
 
/// OS Idle Thread
extern __NO_RETURN void osRtxIdleThread (void *argument);
 
/// OS Exception handlers
extern void SVC_Handler     (void);
extern void PendSV_Handler  (void);
extern void SysTick_Handler (void);


#ifdef  __cplusplus
}
#endif
 
#endif  // RTX_OS_H_
