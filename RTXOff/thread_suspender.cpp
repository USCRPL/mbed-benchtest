
#include "cmsis_os2.h"
#include "thread_suspender.h"
#include <iostream>
#include <system_error>
#include <cstring>

#if USE_WINTHREAD

// Windows implementation of thread suspender.  Very simple since
// this is provided by win32 threads.

void thread_suspender_init()
{
	// Do nothing.
}

os_thread_id thread_suspender_create_suspended_thread(struct thread_suspender_data ** data, void (*start_func)(void* arg), void* arg)
{
	// Windows implementation doesn't need data
	data = nullptr;

	os_thread_id thread = CreateThread(nullptr,
									0,
									reinterpret_cast<LPTHREAD_START_ROUTINE>(start_func),
									reinterpret_cast<void*>(arg),
									CREATE_SUSPENDED,
									nullptr);

	if(thread == nullptr)
	{
		std::cerr << "Error creating thread: " << std::system_category().message(GetLastError()) << std::endl;
		return nullptr;
	}

	return thread;
}

void thread_suspender_suspend(os_thread_id thread, struct thread_suspender_data * data)
{
	if(SuspendThread(thread) < 0)
	{
		std::cerr << "Error suspending thread: " << std::system_category().message(GetLastError()) << std::endl;
	}
}

void thread_suspender_resume(os_thread_id thread, struct thread_suspender_data * data)
{
	if(ResumeThread(thread) < 0)
	{
		std::cerr << "Error resuming RTX thread: " << std::system_category().message(GetLastError()) << std::endl;
	}
}

void thread_suspender_kill(os_thread_id thread, struct thread_suspender_data * data)
{
	if(!TerminateThread(thread, 0))
	{
		std::cerr << "Error terminating RTX thread: " << std::system_category().message(GetLastError()) << std::endl;
	}
}

__NO_RETURN void thread_suspender_current_thread_exit()
{
	ExitThread(0);
}

#else

#define SUSPEND_SIGNAL SIGUSR1

// pointer to this thread's thread data
thread_local thread_suspender_data * myData;

void suspendSignalHandler(int signum)
{
    if(myData == nullptr)
    {
        std::cerr << "RTXOff internal error: no thread data in signal handler" << std::endl;
        exit(9);
    }

    // wait until someone tells us to wake up or terminate
    pthread_mutex_lock(&myData->wakeupMutex);
    myData->isSuspended = true;
    while(!myData->shouldWakeUp && !myData->shouldTerminate)
    { 
        pthread_cond_wait(&myData->wakeupCondVar, &myData->wakeupMutex);
    }
    myData->isSuspended = false;
    if(myData->shouldTerminate)
    {
        pthread_mutex_unlock(&myData->wakeupMutex);
        myData->shouldTerminate = false;
        thread_suspender_current_thread_exit();
    }
    else
    {
        myData->shouldWakeUp = false;
        pthread_mutex_unlock(&myData->wakeupMutex);
        return;
    }
}


void thread_suspender_init()
{
    struct sigaction newSigaction;
    memset(&newSigaction, 0, sizeof(newSigaction));
    newSigaction.sa_handler = &suspendSignalHandler;
    sigaction(SUSPEND_SIGNAL, &newSigaction, nullptr);
}

/**
 * Create a new suspender data and construct the items in it
 * @return
 */
static thread_suspender_data * createSuspenderData()
{
    thread_suspender_data * data = new thread_suspender_data();

    data->shouldWakeUp = false;
    data->shouldTerminate = false;
    data->hasStarted = false;
    data->isSuspended = false;

    pthread_mutex_init(&data->wakeupMutex, nullptr);
    pthread_mutex_init(&data->startMutex, nullptr);

    pthread_cond_init(&data->wakeupCondVar, nullptr);
    pthread_cond_init(&data->startCondVar, nullptr);

    return data;
}

struct SuspenderThreadStartData
{
    thread_suspender_data * suspenderData;
    void (*start_func)(void* arg);
    void *argument;
};

static void * suspenderStartRoutine(SuspenderThreadStartData * startData)
{
    // assign thread local variable
    myData = startData->suspenderData;

    // save copy of data on our stack
    SuspenderThreadStartData startDataCopy = *startData;

    // indicate that we have started
    pthread_mutex_lock(&myData->startMutex);
    myData->hasStarted = true;
    pthread_cond_signal(&myData->startCondVar);
    pthread_mutex_unlock(&myData->startMutex);

    // enter suspend handler once
    suspendSignalHandler(SUSPEND_SIGNAL);

    // now execute user thread function
    startDataCopy.start_func(startDataCopy.argument);

    // if user function returns, kill thread
    thread_suspender_current_thread_exit();
}

os_thread_id thread_suspender_create_suspended_thread(struct thread_suspender_data ** data, void (*start_func)(void* arg), void* arg)
{
    *data = createSuspenderData();

    SuspenderThreadStartData startData;
    startData.suspenderData = *data;
    startData.start_func = start_func;
    startData.argument = arg;

    // start thread in OS
    pthread_t thread;
    int pthreadRetval = pthread_create(&thread, nullptr, reinterpret_cast<void * (*)(void *)>(&suspenderStartRoutine), &startData);
    if(pthreadRetval != 0)
    {
        std::cerr << "Error creating thread: " << std::system_category().message(pthreadRetval) << std::endl;
    }

    // Wait for thread to start.
    // This is important because we need to confirm that its copy of myData has been initialized before
    // we let anyone call any other suspender functions on it.
    pthread_mutex_lock(&(*data)->startMutex);
    while(!(*data)->hasStarted)
    {
        pthread_cond_wait(&(*data)->startCondVar, &(*data)->startMutex);
    }
    pthread_cond_signal(&(*data)->startCondVar);
    pthread_mutex_unlock(&(*data)->startMutex);

    // thread has started, we're done here
    return thread;
}

void thread_suspender_suspend(os_thread_id thread, struct thread_suspender_data * data)
{
    // the thread might still be in a signal handler from a previous suspension, so we need to handle this case
    // without erroring.
    pthread_mutex_lock(&data->wakeupMutex);
    if(!data->isSuspended)
    {
        pthread_kill(thread, SUSPEND_SIGNAL);
    }
    pthread_mutex_unlock(&data->wakeupMutex);
}

void thread_suspender_resume(os_thread_id thread, struct thread_suspender_data * data)
{
    pthread_mutex_lock(&data->wakeupMutex);
    data->shouldWakeUp = true;
    pthread_cond_signal(&data->wakeupCondVar);
    pthread_mutex_unlock(&data->wakeupMutex);
}

void thread_suspender_kill(os_thread_id thread, struct thread_suspender_data * data)
{
    pthread_mutex_lock(&data->wakeupMutex);
    if(!data->isSuspended)
    {
        // signal the thread to enter the suspend handler
        pthread_kill(thread, SUSPEND_SIGNAL);
    }
    data->shouldTerminate = true;
    pthread_cond_signal(&data->wakeupCondVar);
    pthread_mutex_unlock(&data->wakeupMutex);
}

__NO_RETURN void thread_suspender_current_thread_exit()
{
    // note: this function is called with all thread data mutexes unlocked

    // dealloc mutexes
    pthread_mutex_destroy(&myData->wakeupMutex);
    pthread_mutex_destroy(&myData->startMutex);

    // dealloc cond vars
    pthread_cond_destroy(&myData->wakeupCondVar);
    pthread_cond_destroy(&myData->startCondVar);

    // delete memory
    delete myData;
    myData = nullptr;

    pthread_exit(0);
}

#endif