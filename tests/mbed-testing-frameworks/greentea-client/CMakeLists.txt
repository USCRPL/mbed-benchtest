set(GREENTEA_CLIENT_SOURCES
	greentea-client/greentea_metrics.h
	greentea-client/test_env.h
	source/greentea_metrics.cpp
	source/greentea_test_env.cpp)

add_library(greentea-client ${GREENTEA_CLIENT_SOURCES})
target_include_directories(greentea-client PUBLIC .)
target_link_libraries(greentea-client mbed_platform)