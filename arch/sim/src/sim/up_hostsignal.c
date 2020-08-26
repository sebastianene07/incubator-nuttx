/****************************************************************************
 * arch/sim/src/sim/up_hostsignal.c
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <assert.h>
#include <signal.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* The alternative signal stack used for the simulated interrupts */

static stack_t g_ss;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: is_stack_downwards
 *
 * Description:
 *   This function returns true if the stack places elements downwards.
 *
 * Input Parameters:
 *   stack_arg - the address of a variable from the previous function stack
 *    frame
 *
 * Returned Value:
 *   True if the stack grows downwards otherwise false.
 *
 ****************************************************************************/

static bool is_stack_downwards(void *stack_arg)
{
  int stack_arg_local;
  return stack_arg > (void *)&stack_arg_local;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: sim_setup_alternate_sigstack
 *
 * Description:
 *   This function sets up an alternative stack for the signals from the
 *   host heap memory.
 *
 ****************************************************************************/

void sim_setup_alternate_sigstack(void)
{
  g_ss.ss_sp = malloc(SIGSTKSZ);
  assert(g_ss.ss_sp != NULL);

  g_ss.ss_size  = SIGSTKSZ;
  g_ss.ss_flags = SS_ONSTACK;

  int ret = sigaltstack(&g_ss, NULL);
  assert(ret >= 0);
}

/****************************************************************************
 * Name: sim_release_alternate_sigstack
 *
 * Description:
 *   This function tears down the heap memory allocated for the alternative
 *   signal stack and informs the system that we no longer use an alternative
 *   stack.
 *
 ****************************************************************************/

void sim_release_alternate_sigstack(void)
{
  g_ss.ss_size  = SIGSTKSZ;
  g_ss.ss_flags = SS_DISABLE;

  int ret = sigaltstack(&g_ss, NULL);
  assert(ret >= 0);

  free(g_ss.ss_sp);
}

/****************************************************************************
 * Name: host_up_interrupt_context
 *
 * Description:
 *   This function verifies if we are running on the signal handler stack.
 *
 * Returned Value:
 *   Return zero if we are not running on the signal stack otherwise return
 *   a non zero value.
 *
 ****************************************************************************/

int host_up_interrupt_context(void)
{
  uint8_t test_address;
  unsigned long verify_stack_arg = (unsigned long)&test_address;
  unsigned long stack_start = (unsigned long)g_ss.ss_sp;
  unsigned long stack_end;

  if (is_stack_downwards(&verify_stack_arg) == true)
    {
      stack_end = stack_start - g_ss.ss_size;
      return stack_start > verify_stack_arg && stack_end < verify_stack_arg;
    }
  else
    {
      stack_end = stack_start + g_ss.ss_size;
      return stack_start < verify_stack_arg && stack_end < verify_stack_arg;
    }
}
