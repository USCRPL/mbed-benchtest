#!/bin/bash -ex

###############################
# Compile mbed-benchtest

mkdir build
cd build
cmake ..
make