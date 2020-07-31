//
// Entry point for command line programs
//

#include "cmsis_os2.h"
#include "mbed_rtxoff_storage.h"
#include <iostream>

osThreadAttr_t _main_thread_attr;
mbed_rtos_storage_thread_t _main_obj;

// user program entry point.  Defined elsewhere.
extern "C" int mbed_start();

int main(void)
{
	osKernelInitialize();

	// create and start main thread
	_main_thread_attr.priority = osPriorityNormal;
	_main_thread_attr.name = "main";

	osThreadId_t result = osThreadNew((osThreadFunc_t)mbed_start, NULL, &_main_thread_attr);
	if ((void *)result == nullptr) {
		std::cerr << "Pre main thread not created" << std::endl;
	}

	std::cout << ">> Starting RTX Off-Board..." << std::endl;
	osKernelStart();
	std::cerr << "Failed to start RTOS" << std::endl;
}