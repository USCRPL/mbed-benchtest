//
// Entry point for command line programs
//

#include "cmsis_os2.h"
#include "mbed_rtxoff_storage.h"
#include <iostream>

osThreadAttr_t _main_thread_attr;
mbed_rtos_storage_thread_t _main_obj;

// definition for Mbed global singleton mutex
osMutexId_t               singleton_mutex_id;
mbed_rtos_storage_mutex_t singleton_mutex_obj;

// user program entry point.  Defined elsewhere.
extern "C" int mbed_start();

void mbed_rtos_init_singleton_mutex(void)
{
	const osMutexAttr_t singleton_mutex_attr = {
			.name = "singleton_mutex",
			.attr_bits = osMutexRecursive | osMutexPrioInherit | osMutexRobust,
			.cb_mem = &singleton_mutex_obj,
			.cb_size = sizeof(singleton_mutex_obj)
	};
	singleton_mutex_id = osMutexNew(&singleton_mutex_attr);
}


int main(void)
{
	osKernelInitialize();

	mbed_rtos_init_singleton_mutex();

	// create and start main thread
	_main_thread_attr.priority = osPriorityNormal;
	_main_thread_attr.name = "main";

	osThreadId_t result = osThreadNew(reinterpret_cast<osThreadFunc_t>(&mbed_start), NULL, &_main_thread_attr);
	if ((void *)result == nullptr) {
		std::cerr << "Pre main thread not created" << std::endl;
	}

	std::cout << ">> Starting RTX Off-Board..." << std::endl;
	osKernelStart();
	std::cerr << "Failed to start RTOS" << std::endl;
}