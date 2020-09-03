/****************************************************************************
 * arch/sim/src/sim/up_blocktask.c
 *
 *   Copyright (C) 2007-2009, 2013, 2015 Gregory Nutt. All rights reserved.
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

#include <stdbool.h>
#include <sched.h>
#include <debug.h>

#include <nuttx/arch.h>
#include <nuttx/sched.h>

#include "sched/sched.h"
#include "up_internal.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: up_block_task
 *
 * Description:
 *   The currently executing task at the head of the ready to run list must
 *   be stopped.  Save its context and move it to the inactive list
 *   specified by task_state.
 *
 * Input Parameters:
 *   tcb: Refers to a task in the ready-to-run list (normally the task at
 *     the head of the list).  It must be stopped, its context saved and
 *     moved into one of the waiting task lists.  If it was the task at the
 *     head of the ready-to-run list, then a context switch to the new
 *     ready to run task must be performed.
 *   task_state: Specifies which waiting task list should hold the blocked
 *     task TCB.
 *
 ****************************************************************************/

void up_block_task(struct tcb_s *tcb, tstate_t task_state)
{
  FAR struct tcb_s *rtcb = this_task();
  FAR struct tcb_s *prev_tcb;
  bool switch_needed;

  /* Verify that the context switch can be performed */

  DEBUGASSERT((tcb->task_state >= FIRST_READY_TO_RUN_STATE) &&
              (tcb->task_state <= LAST_READY_TO_RUN_STATE));

  /* sinfo("Blocking TCB=%p\n", tcb); */

  /* Remove the tcb task from the ready-to-run list.  If we are blocking the
   * task at the head of the task list (the most likely case), then a
   * context switch to the next ready-to-run task is needed. In this case,
   * it should also be true that rtcb == tcb.
   */

  switch_needed = nxsched_remove_readytorun(tcb);

  /* Add the task to the specified blocked task list */

  nxsched_add_blocked(tcb, (tstate_t)task_state);

  /* If there are any pending tasks, then add them to the ready-to-run
   * task list now
   */

  if (g_pendingtasks.head)
    {
      switch_needed |= nxsched_merge_pending();
    }

  /* Now, perform the context switch if one is needed */

  if (switch_needed)
    {
      /* Update scheduler parameters */

      nxsched_suspend_scheduler(rtcb);

      /* Save the previous running TCB */

      prev_tcb = rtcb;

      /* Restore the exception context of the rtcb at the (new) head
       * of the ready-to-run task list.
       */

      rtcb = this_task();
      sinfo("New Active Task TCB=%p\n", rtcb);

      /* The way that we handle signals in the simulation is kind of
       * a kludge.  This would be unsafe in a truly multi-threaded,
       * interrupt driven environment.
       */

      if (rtcb->xcp.sigdeliver)
        {
          sinfo("Delivering signals TCB=%p\n", rtcb);
          ((sig_deliver_t)rtcb->xcp.sigdeliver)(rtcb);
          rtcb->xcp.sigdeliver = NULL;
        }

      /* Reset scheduler parameters */

      nxsched_resume_scheduler(rtcb);

      /* Then switch contexts */

      up_swap_context(prev_tcb->xcp.ucontext_buffer,
                      rtcb->xcp.ucontext_buffer);
    }
}
