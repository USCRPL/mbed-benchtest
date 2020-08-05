//
// Header that provides a clock that the kernel runs off of.
//

#ifndef MBED_BENCHTEST_RTXOFF_CLOCK_H
#define MBED_BENCHTEST_RTXOFF_CLOCK_H

#include "RTX_Config.h"
#include <chrono>

#if RTXOFF_USE_PROCESS_CLOCK
/** A C++11 chrono TrivialClock for the RTXOff kernel millisecond tick count.
 * Inspired by a similar feature in Mbed RTOS.
 *
 */
struct RTXClock {
	RTXClock() = delete;
	/* Standard TrivialClock fields */
	using duration = std::chrono::microseconds; // note: we use microseconds because the Windows and POSIX clocks have roughly microsecond precision
	using rep = duration::rep;
	using period = duration::period;
	using time_point = std::chrono::time_point<RTXClock>;

	static constexpr bool is_steady = true;
	static time_point now();
};
#else
typedef std::chrono::steady_clock RTXClock;
#endif

#endif //MBED_BENCHTEST_RTXOFF_CLOCK_H
