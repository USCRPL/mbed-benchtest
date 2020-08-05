/****************************************************************************
 * Copyright (c) 2015, ARM Limited, All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ****************************************************************************
 */

#include "utest/utest_shim.h"
#include "utest/utest_stack_trace.h"

#include "platform/mbed_critical.h"
#include <chrono>
#include <ThisThread.h>

// only one callback is active at any given time
static utest_v1_harness_callback_t ticker_callback = nullptr;
static std::chrono::steady_clock::time_point nextCallbackTime; // time to deliver the callback.
static bool callbackDeliverImmediate = false;

static int32_t utest_us_ticker_init()
{
    // empty
    return 0;
}

/**
 * Set the next event to be called and the time at which to call it.
 * Can be called from a thread other than the main thread.
 *
 * @param callback
 * @param delay_ms
 * @return
 */
static void *utest_us_ticker_post(const utest_v1_harness_callback_t callback, uint32_t delay_ms)
{
    UTEST_LOG_FUNCTION();

	ticker_callback = callback;

    if (delay_ms) {
		nextCallbackTime = std::chrono::steady_clock::now() + std::chrono::milliseconds(delay_ms);
    }
    else {
		nextCallbackTime = std::chrono::steady_clock::time_point();
    }

    // return a bogus handle
    return (void*)1;
}

/**
 * Cancel the current pending event.  Can be called from a thread other than the main thread.
 * @param handle
 * @return
 */
static int32_t utest_us_ticker_cancel(void *handle)
{
	ticker_callback = nullptr;
	return 0;
}

/**
 * Run the current event at the correct time.
 * @return
 */
static int32_t utest_us_ticker_run()
{
    UTEST_LOG_FUNCTION();
    while(1)
    {
        // enter a critical section so we don't get interrupted by an interrupt changing the data
        core_util_critical_section_enter();
        if(ticker_callback != nullptr)
		{
        	if(callbackDeliverImmediate || std::chrono::steady_clock::now() >= nextCallbackTime)
			{
        		// remove the callback
				utest_v1_harness_callback_t ticker_callback_copy = ticker_callback;
        		ticker_callback = nullptr;
        		callbackDeliverImmediate = false;
        		core_util_critical_section_exit();
				ticker_callback_copy();
			}
        	else
			{
        		core_util_critical_section_exit();
			}
		}
        else
		{
        	core_util_critical_section_exit();
		}

        // let the scheduler switch to another thread if it has one
        rtos::ThisThread::yield();
    }
}


extern "C" {
static const utest_v1_scheduler_t utest_v1_scheduler =
{
    utest_us_ticker_init,
    utest_us_ticker_post,
    utest_us_ticker_cancel,
    utest_us_ticker_run
};
utest_v1_scheduler_t utest_v1_get_scheduler()
{
    UTEST_LOG_FUNCTION();
    return utest_v1_scheduler;
}
}
