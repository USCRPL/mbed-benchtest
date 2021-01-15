//
// Header to handle suspending and resuming threads.
// Must provide C functions since it's included by rtxoff_os.h
//

#ifndef MBED_BENCHTEST_THREAD_SUSPENDER_H
#define MBED_BENCHTEST_THREAD_SUSPENDER_H

// include OS threading library
#if USE_WINTHREAD
#  define WIN32_LEAN_AND_MEAN 1
#  include <Windows.h>
#else

//#  pragma push_macro("__cplusplus")
//#  define throw __attribute__
#  include <pthread.h>
//#  pragma pop_macro("__cplusplus")
#  include <signal.h>
#endif
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#if USE_WINTHREAD
typedef HANDLE os_thread_id;

struct thread_suspender_data
{
    char dummy; // dummy data, struct must have at least one member
};

#else
typedef pthread_t os_thread_id;

// describes the different states a thread can be in
enum suspender_thread_state {
    RUNNING, // Thread is running, i.e. not in our signal handler
    SUSPENDED, // Thread is blocked in our signal handler
    KILLED // Thread has been killed
};

struct thread_suspender_data {
    // condition variable to force the thread to wake up when in our interrupt handler down
    pthread_cond_t wakeupCondVar;

    // mutex for above cond var
    pthread_mutex_t wakeupMutex;

    // condition variable to indicate that the thread has started
    pthread_cond_t startCondVar;

    // mutex for above cond var and hasStarted
    pthread_mutex_t startMutex;

    // whether the thread should wake up now
    bool shouldWakeUp;

    // whether the thread should terminate now
    bool shouldTerminate;

    // whether the thread has begun starting and entered the suspend handler
    bool hasStarted;

    // True iff the thread is currently in the signal handler waiting for wakeup.
    // Protected by wakeupMutex.
    bool isSuspended;
};
#endif

/**
 * Initialize the thread suspender library.
 */
void thread_suspender_init();

/**
 * Create a new thread in the suspended state.
 * On POSIX this requires a context switch since we need to wait for the thread to start.
 * @param data Pointer which will be filled in with this thread's data struct.
 * @param start_func Function pointer to the function to call when the thread exits.
 * @param on_exit_func If not null, function to call when thread returns from its main function.
 */
os_thread_id thread_suspender_create_suspended_thread(struct thread_suspender_data ** data, void (*start_func)(void* arg), void* arg);

/**
 * Suspend the given thread.  Next time the OS transfers control back to this thread, it will be blocked
 * and unable to execute any more instructions.
 * This is done efficiently without any context switches in thread_suspender_suspend().
 * Should NOT be called from the thread being suspended!
 */
void thread_suspender_suspend(os_thread_id thread, struct thread_suspender_data * data);

/**
 * Suspend the given thread.  Next time the OS transfers control back to this thread, it will continue
 * executing from where it was suspended.
 * This is done efficiently without any context switches in thread_suspender_resume().
 */
void thread_suspender_resume(os_thread_id thread, struct thread_suspender_data * data);

/**
 * Terminate the given thread.  It will terminate at some point in the future and never
 * execute any more instructions.
 */
void thread_suspender_kill(os_thread_id thread, struct thread_suspender_data * data);

/**
 * Call this to exit the current thread.
 * Deallocates the thread's data.  Once this function has been called, no other thread suspender functions
 * may be called on the thread.
 *
 * Automatically called by a thread when:
 *  - it is killed with thread_suspender_kill()
 *  - it returns from its main function
 */
__NO_RETURN void thread_suspender_current_thread_exit();

#ifdef __cplusplus
}
#endif


#endif //MBED_BENCHTEST_THREAD_SUSPENDER_H
