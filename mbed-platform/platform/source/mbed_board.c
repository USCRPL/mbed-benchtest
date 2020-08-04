/* mbed Microcontroller Library
 * Copyright (c) 2006-2013 ARM Limited
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <stdio.h>
#include <string.h>
#include "platform/mbed_wait_api.h"
#include "platform/mbed_toolchain.h"
#include "platform/mbed_interface.h"
#include "platform/mbed_retarget.h"
#include "platform/mbed_critical.h"

WEAK MBED_NORETURN void mbed_die(void)
{
	fprintf(stderr, "He's mbed_dead(), Jim!\n");
	while(true) {}
}

void mbed_error_printf(const char *format, ...)
{
    va_list arg;
    va_start(arg, format);
    mbed_error_vprintf(format, arg);
    va_end(arg);
}

void mbed_error_vprintf(const char *format, va_list arg)
{
    char buffer[132];
    int size = vsnprintf(buffer, sizeof buffer, format, arg);
    if ((unsigned int)size >= sizeof buffer) {
        /* Output was truncated - indicate by overwriting tail of buffer
         * with ellipsis, newline and null terminator.
         */
        static const char ellipsis[] = "...\n";
        memcpy(&buffer[sizeof buffer - sizeof ellipsis], ellipsis, sizeof ellipsis);
    }
    if (size > 0) {
        mbed_error_puts(buffer);
    }
}

void mbed_error_puts(const char *str)
{
    core_util_critical_section_enter();
#if MBED_CONF_PLATFORM_STDIO_CONVERT_NEWLINES || MBED_CONF_PLATFORM_STDIO_CONVERT_TTY_NEWLINES
    char stdio_out_prev = '\0';
    for (; *str != '\0'; str++) {
        if (*str == '\n' && stdio_out_prev != '\r') {
            const char cr = '\r';
            fwrite(&cr, 1, 1, stderr);
        }
        fwrite(str, 1, 1, stderr);
        stdio_out_prev = *str;
    }
#else
    write(STDERR_FILENO, str, strlen(str));
#endif
    core_util_critical_section_exit();
}
