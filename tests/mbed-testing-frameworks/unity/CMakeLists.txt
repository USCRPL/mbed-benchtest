set(UNITY_SOURCES
	source/unity.c
	unity/unity.h
	unity/unity_config.h
	unity/unity_internals.h)

add_library(unity ${UNITY_SOURCES})
target_include_directories(unity PUBLIC .)
target_link_libraries(unity utest mbed_platform)