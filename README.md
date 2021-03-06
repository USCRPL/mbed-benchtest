## mbed-benchtest
mbed-benchtest provides tools for testing your Mbed applications on a desktop computer.

### RTXOff
RTX Off-Board is a modified version of Mbed's RTX RTOS that runs on a desktop computer and translates RTOS calls to the host PC's threading library.  Using it, you can run multithreaded MBed applications in an environment very similar to a real processor.

### Directory structure
#### mbed-platform
Copied from MBed, with some modifications (TODO: describe those modifications). These files the outward interface of MBedOS that is used by firmware code.

#### RTXoff
The implementation of MBed's RTX RTOS for the host OS. Implements the CMSIS API. Most code is copied from mbed, with many system-specific modifications.

#### tests
##### arm-cmsis-rtos-validator 
Taken from [this repository](https://github.com/xpacks/arm-cmsis-rtos-validator), this validator is used to validate the RTXOff implementation

##### events
Tests for the Mbed OS event queue functionality.  We used these to help validate that both threads and event queues were working. 

##### mbed-testing-frameworks
Dependencies for Mbed unit tests such as those in the events folder.  Modified by us to run on desktop.

### Detailed behavior
RTXOff uses nearly the same exact same thread scheduling code as RTX itself, so its thread switching behavior should be very similar to that of the RTOS on a real target.  Just like RTX, the scheduler will always immediately switch to the highest priority thread that can currently run.  Also note that at most one RTX thread can be running (as in, not suspended) at the same time.

#### Preemptive scheduling
The only place where RTXOff will have different behavior than RTX is preemptive scheduling -- when the scheduler interrupts your code at a scheduled time.  This behavior is what switches threads in a round-robin method to share the CPU, and it's also what takes sleeping threads off the waiting list and runs them at the correct time.  RTXOff guarantees that preemptive scheduling will *do* the same thing, but not that it will *happen* at the same time.  So if your code is sensitive to differences in when scheduling interrupts happen, it may run differently under RTXOff. 

Note that these timing inaccuracies also apply to timed waits in certain cases because it can take a fair amount of time for the scheduler to wake up from sleep or switch away from the idle thread.  On real systems I see about +-20ms accuracy on timeouts and delay times using the more accurate `RTXOFF_USE_PROCESS_CLOCK=1` setting (measured using the kernel's own clock).  On a real system, these would be accurate to the scheduler tick (default 1ms).  Note that this is _only_ an issue with wait timeouts -- thread switches due to one thread blocking (or doing something else that causes a switch such as starting a new high-priority thread) happen almost immediately, just like the real processor.

#### Interrupt support
RTXOff supports interrupts, using the standard [NVIC interrupt functions](https://www.keil.com/pack/doc/CMSIS/Core/html/group__NVIC__gr.html).  This allows you to test code that uses interrupts in a reasonable way -- just write testing code that calls NVIC_EnableIRQ() at the appropriate time to trigger an interrupt in your code.  Note that the NVIC_XXX functions are safe to call from any thread, unlike all other cmsis-rtos API functions which are only safe to call from RTOS threads.  RTXOff interrupts do support priority (the interrupt with lowest priority value will be delivered first if multiple are triggered), but they do *not* support interrupting a currently executing interrupt with another interrupt (which is what happens on the processor if a higher priority interrupt is triggered).  Instead, the new interrupt will be executed as soon as the current one returns.

### Current Limitations / Things to Know
- Main Function: Without toolchain support, there's no way to override your app's main() function.  So, your app's main should be called `int mbed_start()` (`extern "C" int mbed_start()` if in C++).  RTXOff's main thread will call this function when it starts.
- RTXOff does not use or check the memory that your code allocates for RTOS objects and thread stacks.  Even if RTXOff did check stack sizes, your program's stack would be a different size when compiled for desktop than when built for ARM.  So, you cannot verify that your threads have enough stack space to run with RTXOff.
- The scheduler tick frequency must be between 1Hz-1000Hz (so the tick period must be between 1ms-1000ms)
- Be aware that RTXOff uses thread suspension to implement context switches, and not all host OS functions (e.g. printf()) are OK with this.  Calling these functions from multiple threads can potentially result in a deadlock if a thread switch occurs from one thread in the middle of printf() to another thread that then calls printf().  A stopgap solution is to disable interrupts (using `core_util_critical_section_enter()`) when calling these functions so the scheduler cannot switch threads.  However, a better solution would be to proxy these types of operations to another thread.  We are thinking about ways to implement this.
    - Confused about how this happens?  Imagine this: 
        - Thread A calls printf().  printf() locks an internal mutex to prevent other threads from entering printf().
        - The RTXOff scheduler preempts thread A and suspends it.
        - Thread B (which has higher priority than A) exits a timed wait.
        - The RTXOff scheduler switches to B
        - B calls printf(), which then attempts to lock the mutex that A is holding.
        - The RTXOff scheduler will keep preempting B on schedule, but the underlying OS will never allow B to continue without the mutex that A is holding.  However, the scheduler will never transfer to control to A while B (which has higher priority) is running.
- To emulate the behavior of the target most closely, RTXOff will never shut down on its own, even if all the threads you create have exited (since the idle thread is still running).  However, this causes problems when trying to e.g. run Valgrind on your programs.  The easiest solution is to have one of the RTX threads call exit() at some point, which will close the process.  There will be a few memory blocks that show as leaked inside RTXOff but these are expected, they're for storing thread data for running threads.
 