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
#  include <pthread.h>
#  include <signal.h>
#endif
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#if USE_WINTHREAD
typdef HANDLE os_thread_id;

struct thread_suspender_data
{
    // empty
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

    // whether the thread should wake up now
    bool shouldWakeUp;

    // whether the thread should terminate now
    bool shouldTerminate;
};
#endif

/**
 * Initialize the thread suspender library.
 */
void thread_suspender_init();

/**
 * Initialize the thread suspender library data for a specific thread.
 * MUST be called from the context of the thread itself (for access to thread local storage)!
 * @param thread
 * @param data
 */
void thread_suspender_thread_init(os_thread_id thread, struct thread_suspender_data * data);

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
 * Terminate the given suspended thread.  It will terminate at some point in the future and never
 * execute any instructions after where it was suspended.
 */
void thread_suspender_kill(os_thread_id thread, struct thread_suspender_data * data);

/**
 * Destroy the suspender data for a thread.  Should be called at some point to prevent a memory leak.
 * May be called from the thread itself, or from another thread once the thread doesn't need to be suspended anymore.
 * @param thread
 * @param data
 */
void thread_suspender_destroy_data(os_thread_id thread, struct thread_suspender_data * data);

#ifdef __cplusplus
}
#endif


#endif //MBED_BENCHTEST_THREAD_SUSPENDER_H
