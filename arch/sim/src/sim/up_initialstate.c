/****************************************************************************
 * arch/sim/src/sim/up_initialstate.c
 *
 *   Copyright (C) 2007-2009 Gregory Nutt. All rights reserved.
 *   Author: Gregory Nutt <gnutt@nuttx.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name NuttX nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <setjmp.h>
#include <stdint.h>
#include <string.h>

#include <nuttx/arch.h>

#include "sched/sched.h"
#include "up_internal.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

static sigjmp_buf g_jmpbuf[CONFIG_MAX_TASKS];

/****************************************************************************
 * Name: up_initial_state
 *
 * Description:
 *   A new thread is being started and a new TCB
 *   has been created. This function is called to initialize
 *   the processor specific portions of the new TCB.
 *
 *   This function must setup the initial architecture registers
 *   and/or  stack so that execution will begin at tcb->start
 *   on the next context switch.
 *
 ****************************************************************************/

void up_initial_state(struct tcb_s *tcb)
{
#ifdef CONFIG_SIM_PREEMPTIBLE
  int task_index = PIDHASH(tcb->pid);
  tcb->xcp.sig_jump_buffer = &g_jmpbuf[task_index];
  if (up_setjmp(g_jmpbuf[task_index]))
    {
      tcb->start();
    }
  else
    {
      xcpt_reg_t *regs = &g_jmpbuf[task_index];
      regs[JB_SP] = (xcpt_reg_t)tcb->adj_stack_ptr - sizeof(xcpt_reg_t);
      regs[JB_PC] = (xcpt_reg_t)tcb->start;
      tcb->xcp.is_initialized = 0;
    }
#else
  memset(&tcb->xcp, 0, sizeof(struct xcptcontext));
  tcb->xcp.regs[JB_SP] = (xcpt_reg_t)tcb->adj_stack_ptr - sizeof(xcpt_reg_t);
  tcb->xcp.regs[JB_PC] = (xcpt_reg_t)tcb->start;
  int task_index = PIDHASH(tcb->pid);
#endif
}
