## mbed-benchtest
mbed-benchtest provides tools for testing your Mbed applications on a desktop computer.

### RTXOff
RTX Off-Board is a modified version of Mbed's RTX RTOS that runs on a desktop computer and translates RTOS calls tp the host PC's threading library.  Using it, you can run multithreaded MBed applications in an environment very similar to a real processor.

RTXOff uses nearly the same exact same thread scheduling code as RTX itself, so its thread switching behavior should be very similar to that of the RTOS on a real target.  Just like RTX, the scheduler will always immediately switch to the highest priority thread that can currently run.  Also note that at most one RTX thread can be running (as in, not suspended) at the same time.

The only place where RTXOff will have different behavior than RTX is preemptive scheduling -- when the scheduler interrupts your code at a scheduled time.  This behavior is what switches threads in a round-robin method to share the CPU, ahd it's also what takes sleeping threads off the waiting list and runs them at the correct time.  RTXOff guarantees that preemptive scheduling will *do* the same thing, but not that it will *happen* at the same time.  So if your code is sensitive to differences in when scheduling interrupts happen, it may run differently under RTXOff. 

**Current Limitations**:
- Main Function: Without toolchain support, there's no way to override your app's main() function.  So, your app's main should be called `int mbed_start()` (`extern "C" int mbed_start()` if in C++).  RTXOff's main thread will call this function when it starts.
- Only Windows MinGW is supported right now, though POSIX support is being worked on.
- RTXOff does not use or check the memory that your code allocates for the thread object and its stack.  Even if RTXOff did check stack sizes, your program's stack would be a different size when compiled for desktop than when built for ARM.  So, you cannot verify that your threads have enough stack space to run with RTXOff.