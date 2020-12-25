set(CMSIS_RTOS_VALIDATOR_SOURCES
	Source/cmsis_rv.c
	Source/RV_Thread.c
	Source/RV_Mutex.c
	Source/RV_Signal.c
	Source/RV_Report.c
	Source/RV_Semaphore.c
	Source/RV_Framework.c
	Source/RV_MemoryPool.c
	Source/RV_MsgQueue.c
	Source/RV_Timer.c)

add_executable(cmsis_rtos_validator ${CMSIS_RTOS_VALIDATOR_SOURCES})
target_link_libraries(cmsis_rtos_validator rtxoff)
target_include_directories(cmsis_rtos_validator PRIVATE
	Include
	Source/Config)

# add the executable to CTest
add_test(NAME cmsis_rtos_validator
	COMMAND $<TARGET_FILE:cmsis_rtos_validator>)