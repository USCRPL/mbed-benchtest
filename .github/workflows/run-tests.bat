rem ###############################
rem Run the RTOS Validator tests

cd build
ctest --timeout 30 --output-on-failure -R cmsis_rtos_validator -C Debug .
