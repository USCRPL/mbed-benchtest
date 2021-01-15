

rem ###############################
rem Compile mbed-benchtest

mkdir build
cd build
cmake -Ax64 ..

rem only build the validator and needed subprojects, since the other tests don't 
rem currently build with msvc
msbuild /m:2 /property:Configuration=Debug tests\arm-cmsis-rtos-validator\cmsis_rtos_validator.vcxproj