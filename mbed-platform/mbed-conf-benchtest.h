//
// Mbed config header used for testing stubs.
// MBed doesn't provide this, so we have to do it ourselves.
//

#define MBED_CONF_CELLULAR_USE_SMS 1

#define DEVICE_ANALOGIN 1
#define DEVICE_ANALOGOUT 1
#define DEVICE_CAN 1
#define DEVICE_EMAC 1
#define DEVICE_FLASH 1
#define DEVICE_I2C 1
#define DEVICE_I2CSLAVE 1
#define DEVICE_I2C_ASYNCH 1
#define DEVICE_INTERRUPTIN 1
#define DEVICE_LPTICKER 1
#define DEVICE_MPU 1
#define DEVICE_PORTIN 1
#define DEVICE_PORTINOUT 1
#define DEVICE_PORTOUT 1
#define DEVICE_PWMOUT 1
#define DEVICE_RESET_REASON 1
#define DEVICE_RTC 1
#define DEVICE_SERIAL 1
#define DEVICE_SERIAL_ASYNCH 1
#define DEVICE_SERIAL_FC 1
#define DEVICE_SLEEP 1
#define DEVICE_SPI 1
#define DEVICE_SPISLAVE 1
#define DEVICE_SPI_ASYNCH 1
#define DEVICE_STDIO_MESSAGES 1
#define DEVICE_TRNG 1
#define DEVICE_USBDEVICE 1
#define DEVICE_USTICKER 1
#define DEVICE_WATCHDOG 1

#define MBED_CONF_RTOS_API_PRESENT 1
#define MBED_CONF_RTOS_PRESENT 1

#define MBED_CONF_PLATFORM_CALLBACK_COMPARABLE                            1                                       // set by library:platform
#define MBED_CONF_PLATFORM_CALLBACK_NONTRIVIAL                            0                                       // set by library:platform
#define MBED_CONF_PLATFORM_CRASH_CAPTURE_ENABLED                          1                                       // set by library:platform[NUCLEO_F429ZI]
#define MBED_CONF_PLATFORM_CTHUNK_COUNT_MAX                               8                                       // set by library:platform
#define MBED_CONF_PLATFORM_DEFAULT_SERIAL_BAUD_RATE                       9600                                    // set by library:platform
#define MBED_CONF_PLATFORM_ERROR_ALL_THREADS_INFO                         0                                       // set by library:platform
#define MBED_CONF_PLATFORM_ERROR_FILENAME_CAPTURE_ENABLED                 0                                       // set by library:platform
#define MBED_CONF_PLATFORM_ERROR_HIST_ENABLED                             0                                       // set by library:platform
#define MBED_CONF_PLATFORM_ERROR_HIST_SIZE                                4                                       // set by library:platform
#define MBED_CONF_PLATFORM_ERROR_REBOOT_MAX                               1                                       // set by library:platform
#define MBED_CONF_PLATFORM_FATAL_ERROR_AUTO_REBOOT_ENABLED                1                                       // set by library:platform[NUCLEO_F429ZI]
#define MBED_CONF_PLATFORM_MAX_ERROR_FILENAME_LEN                         16                                      // set by library:platform
#define MBED_CONF_PLATFORM_MINIMAL_PRINTF_ENABLE_64_BIT                   1                                       // set by library:platform
#define MBED_CONF_PLATFORM_MINIMAL_PRINTF_ENABLE_FLOATING_POINT           0                                       // set by library:platform
#define MBED_CONF_PLATFORM_MINIMAL_PRINTF_SET_FLOATING_POINT_MAX_DECIMALS 6                                       // set by library:platform
#define MBED_CONF_PLATFORM_POLL_USE_LOWPOWER_TIMER                        0                                       // set by library:platform
#define MBED_CONF_PLATFORM_STDIO_BAUD_RATE                                115200                                  // set by application[*]
#define MBED_CONF_PLATFORM_STDIO_BUFFERED_SERIAL                          0                                       // set by library:platform
#define MBED_CONF_PLATFORM_STDIO_CONVERT_NEWLINES                         1                                       // set by library:platform
#define MBED_CONF_PLATFORM_STDIO_CONVERT_TTY_NEWLINES                     1                                       // set by library:platform
#define MBED_CONF_PLATFORM_STDIO_FLUSH_AT_EXIT                            1                                       // set by library:platform
#define MBED_CONF_PLATFORM_STDIO_MINIMAL_CONSOLE_ONLY                     0
