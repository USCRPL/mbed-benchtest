#include "rtxoff_clock.h"

using namespace std::chrono;

#if RTXOFF_USE_PROCESS_CLOCK
#if USE_WINTHREAD

#include <processthreadsapi.h>

// Convert a FILETIME to number of 100 nanosecond intervals
uint64_t filetimeToHundredNS(FILETIME & filetime)
{
	ULARGE_INTEGER largeInt;
	largeInt.HighPart = filetime.dwHighDateTime;
	largeInt.LowPart = filetime.dwLowDateTime;
	return largeInt.QuadPart;
}

RTXClock::time_point RTXClock::now()
{
	// get process times from OS
	FILETIME creationTime, exitTime, kernelTime, userTime;
	GetProcessTimes(GetCurrentProcess(), &creationTime, &exitTime, &kernelTime, &userTime);

	uint64_t totalTimeHundredNS = filetimeToHundredNS(userTime);
	return time_point(microseconds(totalTimeHundredNS / 10));
}

#else
#error TODO
#endif
#endif
