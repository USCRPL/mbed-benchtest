/*
 * This file is part of the µOS++ distribution.
 *   (https://github.com/micro-os-plus)
 * Copyright (c) 2016 Liviu Ionescu.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom
 * the Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "cmsis_rv.h"

#include <cmsis-plus/rtos/os-c-api.h>
#include <cmsis-plus/diag/trace.h>

#if defined(__APPLE__) || defined(__linux__)

#include <signal.h>
#include <unistd.h>

void
sigusr1_init (void);

#endif

int
//os_main (int argc, char* argv[])
os_main (int argc __attribute__((unused)), char* argv[] __attribute__((unused)))
{
#if defined(__APPLE__)  || defined(__linux__)
  sigusr1_init ();
#endif

  return cmsis_rv ();
}

extern void
(*TST_IRQHandler) (void);

#if defined(__ARM_EABI__)

void
WWDG_IRQHandler (void);

void
WWDG_IRQHandler (void)
  {
    if (TST_IRQHandler != NULL)
      {
        TST_IRQHandler ();
        return;
      }
    asm volatile ("bkpt 0");
    for (;;)
    ;
  }

#elif defined(__APPLE__) || defined(__linux__)

extern uint32_t signal_nesting;

static sigset_t sigusr1_set;

void
sigusr1_handler (int signum);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"

void
sigusr1_init (void)
{
  struct sigaction sa;
  sigemptyset(&sa.sa_mask);
  sigaddset(&sa.sa_mask, SIGALRM);
#if defined(__APPLE__)
  sa.__sigaction_u.__sa_handler = sigusr1_handler;
#elif defined(__linux__)
#pragma GCC diagnostic push
#if defined(__clang__)
#pragma clang diagnostic ignored "-Wdisabled-macro-expansion"
#endif
  sa.sa_handler = sigusr1_handler;
#pragma GCC diagnostic pop
#else
#error Unsupported platform.
#endif

  // restart system on signal return
  sa.sa_flags = SA_RESTART;
  sigaction (SIGUSR1, &sa, NULL);

  sigemptyset(&sigusr1_set);
  sigaddset(&sigusr1_set, SIGUSR1);

  sigprocmask (SIG_BLOCK, &sigusr1_set, NULL);
}

#pragma GCC diagnostic pop

void
sigusr1_handler (int signum)
{
  if (signum == SIGUSR1)
    {
      if (TST_IRQHandler != NULL)
        {
          signal_nesting++;
          TST_IRQHandler ();
          signal_nesting--;
        }
    }
  else
    {
      char q = '?';
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-result"
      write (2, &q, 1);
#pragma GCC diagnostic pop
    }
}

void
NVIC_DisableIRQ (int i);

void
NVIC_EnableIRQ (int i);

void
NVIC_SetPendingIRQ (int i);

void
NVIC_DisableIRQ (int i __attribute__((unused)))
{
#if defined(TRACE)
  trace_printf ("%s()\n", __func__);
#endif
  sigprocmask (SIG_BLOCK, &sigusr1_set, NULL);
}

void
NVIC_EnableIRQ (int i __attribute__((unused)))
{
#if defined(TRACE)
  trace_printf ("%s()\n", __func__);
#endif
  sigprocmask (SIG_UNBLOCK, &sigusr1_set, NULL);
}

void
NVIC_SetPendingIRQ (int i __attribute__((unused)))
{
#if defined(TRACE)
  trace_printf ("%s()\n", __func__);
#endif
  kill (getpid (), SIGUSR1);
}

#endif
