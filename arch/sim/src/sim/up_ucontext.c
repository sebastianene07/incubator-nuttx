/****************************************************************************
 * arch/sim/src/sim/up_ucontext.c
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

/* This define is required to compile on OSX */

#ifndef _XOPEN_SOURCE
#  define _XOPEN_SOURCE                (500)
#endif

#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdbool.h>
#include <sys/errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <ucontext.h>

/****************************************************************************
 * Preprocessor Definition
 ****************************************************************************/

/* The minimum ucontext stack size */

#define MINIMUM_UCONTEXT_STACK_LENGTH  (127 * 1024)
#define REQUIRED_UCONTEXT_STACK_LENGTH (128 * 1024)
#define STACK_ALIGN_BYTES              (8)
#define STACK_ALIGN_MASK               (STACK_ALIGN_BYTES - 1)
#define STACK_ALIGN_UP(a)              \
  (((a) + STACK_ALIGN_MASK) & ~STACK_ALIGN_MASK)

#define ASSERT(x)                      assert((x))
#define ARRAY_LENGTH(array)            (sizeof((array))/sizeof((array)[0]))

/****************************************************************************
 * Private Types
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* This context is used on the exit path for the initial task */

static ucontext_t g_uctx_main;

/* The CPU0 has an initial stack but ucontext API requires us to define a
 * stack when we create the context with makecontext(..).
 */

static uint8_t g_idle_stack[REQUIRED_UCONTEXT_STACK_LENGTH];

/* This array holds the task context */

static ucontext_t g_task_context[CONFIG_MAX_TASKS];

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: up_create_context
 *
 * Description:
 *   Creates a new ucontext structure and initialize it.
 *
 * Input Parameters:
 *   ucontext_sp   - the ucontext stack pointer
 *   prev_ucontext - the parent ucontext used%i to return to when the
 *    execution of the current context ends.
 *   entry_point   - the entry point of the new context
 *   ucontext_id   - the index of the ucontext in the global ucontext array
 *   stack_len     - the size of the stack in bytes
 *
 * Return Value:
 *   A pointer to the ucontext structure or NULL in case something went
 *   wrong.
 *
 ****************************************************************************/

void *up_create_context(void *ucontext_sp, void *prev_ucontext,
    void (*entry_point)(void), int ucontext_id, size_t stack_len)
{
  ucontext_t *new_context;

  ASSERT(ucontext_id < CONFIG_MAX_TASKS);
  new_context = &g_task_context[ucontext_id];

  int ret = getcontext(new_context);
  ASSERT(ret >= 0);

  /* The idle task for the first CPU does not call up_create_stack because
   * the system already has a stack. We need to specify a stack during
   * ucontext creation so we use g_idle_stack as the stack buffer.
   */

  unsigned long unaligned_sp = stack_len == 0 ?
    (unsigned long)&g_idle_stack[0] :
    (unsigned long)ucontext_sp;
  unsigned long aligned_sp = STACK_ALIGN_UP(unaligned_sp);
  unsigned long alignment_difference = aligned_sp - unaligned_sp;

  sigemptyset(&new_context->uc_sigmask);
  new_context->uc_stack.ss_sp   = (void *)aligned_sp;
  new_context->uc_stack.ss_size = stack_len == 0 ?
    REQUIRED_UCONTEXT_STACK_LENGTH - alignment_difference :
    stack_len - alignment_difference;
  new_context->uc_link          = prev_ucontext == NULL ? &g_uctx_main :
    prev_ucontext;

  /* Assert if the stack size is less than the minimum required stack */

  ASSERT(new_context->uc_stack.ss_size >= MINIMUM_UCONTEXT_STACK_LENGTH);

  makecontext(new_context, entry_point, 0);

  return new_context;
}

/****************************************************************************
 * Name: up_swap_context
 *
 * Description:
 *   Save the current context in the old_ucontext and activate the
 *   context from activate_ucontext.
 *
 * Input Parameters:
 *   old_ucontext       - place where we store the current context
 *   activate_ucontext  - context that we will activate after function
 *     invocation
 *
 ****************************************************************************/

void up_swap_context(void *old_ucontext, void *activate_ucontext)
{
  int ret = swapcontext(old_ucontext, activate_ucontext);
  if (ret < 0)
    {
      fprintf(stderr, "\r\nswap context err:%d\n", errno);
      ASSERT(ret == 0);
    }
}

/****************************************************************************
 * Name: up_set_context
 *
 * Description:
 *   Set the current context to the specified ucontext
 *
 * Input Parameters:
 *   current_context - the current context
 *
 ****************************************************************************/

void up_set_context(void *current_context)
{
  ASSERT(setcontext(current_context) == 0);
}
