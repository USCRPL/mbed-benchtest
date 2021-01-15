#!/bin/bash -ex

###############################
# Run all mbed-benchtest tests

cd build

# exclude equeue tests for now, as they seem to dislike being run under a CI environment due to timing issues
ctest -j 2 --timeout 30 --output-on-failure -E "equeue.*" .
