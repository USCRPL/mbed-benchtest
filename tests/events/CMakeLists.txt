# Buildfile for event queue tests

add_executable(equeue_c_test equeue/main.cpp)
target_link_libraries(equeue_c_test unity mbed_platform rtxoff)
target_compile_definitions(equeue_c_test PRIVATE MBED_EXTENDED_TESTS=1)

add_test(NAME equeue_c_test
	COMMAND $<TARGET_FILE:equeue_c_test>)

add_executable(equeue_cxx_test queue/main.cpp)
target_link_libraries(equeue_cxx_test unity mbed_platform rtxoff)

add_test(NAME equeue_cxx_test
	COMMAND $<TARGET_FILE:equeue_cxx_test>)