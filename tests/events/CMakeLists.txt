# Buildfile for event queue tests

add_executable(equeue_equeue_test equeue/main.cpp)
target_link_libraries(equeue_equeue_test unity mbed_platform rtxoff)
target_compile_definitions(equeue_equeue_test PRIVATE MBED_EXTENDED_TESTS=1)