/* mbed Microcontroller Library
 * Copyright (c) 2006-2012 ARM Limited
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#ifndef MBED_RTX_CONF_H
#define MBED_RTX_CONF_H

/** Any access to RTX5 specific data structures used in common code should be wrapped in ifdef MBED_OS_BACKEND_RTX5 */
#define MBED_OS_BACKEND_RTX5

#if defined(MBED_CONF_APP_THREAD_STACK_SIZE)
#define OS_STACK_SIZE               MBED_CONF_APP_THREAD_STACK_SIZE
#else
#define OS_STACK_SIZE               MBED_CONF_RTOS_THREAD_STACK_SIZE
#endif

#ifdef MBED_CONF_APP_TIMER_THREAD_STACK_SIZE
#define OS_TIMER_THREAD_STACK_SIZE  MBED_CONF_APP_TIMER_THREAD_STACK_SIZE
#else
#define OS_TIMER_THREAD_STACK_SIZE  MBED_CONF_RTOS_TIMER_THREAD_STACK_SIZE
#endif

// Increase the idle thread stack size when tickless is enabled
#if defined(EXTRA_IDLE_STACK_REQUIRED) || (defined(MBED_TICKLESS) && defined(LPTICKER_DELAY_TICKS) && (LPTICKER_DELAY_TICKS > 0))
#define EXTRA_IDLE_STACK MBED_CONF_RTOS_IDLE_THREAD_STACK_SIZE_TICKLESS_EXTRA
#else
#define EXTRA_IDLE_STACK 0
#endif

// Increase the idle thread stack size when debug is enabled
#if defined(MBED_DEBUG) || defined(MBED_ALL_STATS_ENABLED)
#define EXTRA_IDLE_STACK_DEBUG MBED_CONF_RTOS_IDLE_THREAD_STACK_SIZE_DEBUG_EXTRA
#else
#define EXTRA_IDLE_STACK_DEBUG 0
#endif

#ifdef MBED_CONF_APP_IDLE_THREAD_STACK_SIZE
#define OS_IDLE_THREAD_STACK_SIZE   MBED_CONF_APP_IDLE_THREAD_STACK_SIZE
#else
#define OS_IDLE_THREAD_STACK_SIZE   (MBED_CONF_RTOS_IDLE_THREAD_STACK_SIZE + EXTRA_IDLE_STACK + EXTRA_IDLE_STACK_DEBUG)
#endif

// The number of threads available to applications that need to use
// CMSIS-RTOSv2 Object-specific Memory Pools
#if MBED_CONF_RTOS_THREAD_NUM > 0
#define OS_THREAD_OBJ_MEM 1
#define OS_THREAD_NUM     MBED_CONF_RTOS_THREAD_NUM
#endif

// The total amount of memory for all thread stacks combined available to
// applications that need to use CMSIS-RTOSv2 Object-specific Memory Pools for
// threads
#if MBED_CONF_RTOS_THREAD_USER_STACK_SIZE > 0
#define OS_THREAD_USER_STACK_SIZE MBED_CONF_RTOS_THREAD_USER_STACK_SIZE
#endif

// The number of timers available to applications that need to use CMSIS-RTOSv2
// Object-specific Memory Pools
#if MBED_CONF_RTOS_TIMER_NUM > 0
#define OS_TIMER_OBJ_MEM 1
#define OS_TIMER_NUM     MBED_CONF_RTOS_TIMER_NUM
#endif

// The number of event flag objects available to applications that need to use
// CMSIS-RTOSv2 Object-specific Memory Pools
#if MBED_CONF_RTOS_EVFLAGS_NUM > 0
#define OS_EVFLAGS_OBJ_MEM 1
#define OS_EVFLAGS_NUM     MBED_CONF_RTOS_EVFLAGS_NUM
#endif

// The number of mutexes available to applications that need to use
// CMSIS-RTOSv2 Object-specific Memory Pools
#if MBED_CONF_RTOS_MUTEX_NUM > 0
#define OS_MUTEX_OBJ_MEM 1
#define OS_MUTEX_NUM     MBED_CONF_RTOS_MUTEX_NUM
#endif

// The number of semaphores available to applications that need to use
// CMSIS-RTOSv2 Object-specific Memory Pools
#if MBED_CONF_RTOS_SEMAPHORE_NUM > 0
#define OS_SEMAPHORE_OBJ_MEM 1
#define OS_SEMAPHORE_NUM     MBED_CONF_RTOS_SEMAPHORE_NUM
#endif

// The number of message queues available to applications that need to use
// CMSIS-RTOSv2 Object-specific Memory Pools
#if MBED_CONF_RTOS_MSGQUEUE_NUM > 0
#define OS_MSGQUEUE_OBJ_MEM 1
#define OS_MSGQUEUE_NUM     MBED_CONF_RTOS_MSGQUEUE_NUM
#endif

// The total amount of memory for all message queues combined available to
// applications that need to use CMSIS-RTOSv2 Object-specific Memory Pools for
// message queues
#if MBED_CONF_RTOS_MSGQUEUE_DATA_SIZE > 0
#define OS_MSGQUEUE_DATA_SIZE MBED_CONF_RTOS_MSGQUEUE_DATA_SIZE
#endif

#define OS_DYNAMIC_MEM_SIZE         0

#if defined(OS_TICK_FREQ) && (OS_TICK_FREQ != 1000)
#error "OS Tickrate must be 1000 for system timing"
#endif

#if !defined(OS_STACK_WATERMARK) && (defined(MBED_STACK_STATS_ENABLED) || defined(MBED_ALL_STATS_ENABLED))
#define OS_STACK_WATERMARK          1
#endif

#if !defined(OS_STACK_WATERMARK) && defined(MBED_THREAD_STATS_ENABLED)
#define OS_STACK_WATERMARK          1
#endif


#define OS_IDLE_THREAD_TZ_MOD_ID     1
#define OS_TIMER_THREAD_TZ_MOD_ID    1


// Don't adopt default multi-thread support for ARM/ARMC6 toolchains from RTX code base.
// Provide Mbed-specific instead.
#define RTX_NO_MULTITHREAD_CLIB
// LIBSPACE default value set for ARMCC
#ifndef OS_THREAD_LIBSPACE_NUM
#define OS_THREAD_LIBSPACE_NUM      4
#endif

#define OS_IDLE_THREAD_NAME         "rtx_idle"
#define OS_TIMER_THREAD_NAME        "rtx_timer"

#endif