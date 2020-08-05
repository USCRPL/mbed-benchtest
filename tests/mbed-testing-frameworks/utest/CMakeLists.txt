set(UTEST_SOURCES
	mbed-utest-shim.cpp
	source/unity_handler.cpp
	source/utest_case.cpp
	source/utest_default_handlers.cpp
	source/utest_greentea_handlers.cpp
	source/utest_harness.cpp
	source/utest_print.cpp
	source/utest_shim.cpp
	source/utest_stack_trace.cpp
	source/utest_types.cpp
	utest/unity_handler.h
	utest/utest.h
	utest/utest_case.h
	utest/utest_default_handlers.h
	utest/utest_harness.h
	utest/utest_print.h
	utest/utest_scheduler.h
	utest/utest_shim.h
	utest/utest_specification.h
	utest/utest_stack_trace.h
	utest/utest_types.h
	)

add_library(utest ${UTEST_SOURCES})
target_include_directories(utest PUBLIC .)
target_link_libraries(utest greentea-client mbed_platform)