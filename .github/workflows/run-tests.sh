#!/bin/bash -ex

###############################
# Run all mbed-benchtest tests

cd build/tests

echo ">> Running cmsis-rtos-validator"
cd arm-cmsis-rtos-validator
./cmsis_rtos_validator

echo ">> Running event queue tests"
cd ../events
./equeue_c_test.exe
./equeue_cxx_test.exe
./equeue_equeue_test.exe
./equeue_queue_test.exe

