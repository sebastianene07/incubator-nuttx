/****************************************************************************
 * arch/arm/src/tms570/tms570_boot.c
 *
 *   Copyright (C) 2015 Gregory Nutt. All rights reserved.
 *   Author: Gregory Nutt <gnutt@nuttx.org>
 *
 * This is primarily original code.  However, some logic in this file was
 * inspired/leveraged from TI's Project0 which has a compatible BSD license
 * and credit should be given in any case:
 *
 *   Copyright (c) 2012, Texas Instruments Incorporated
 *   All rights reserved.
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

#include <stdint.h>
#include <assert.h>
#include <debug.h>

#include <arch/board/board.h>

#include "chip.h"
#include "arm.h"
#include "cache.h"
#include "fpu.h"
#include "sctlr.h"
#include "up_internal.h"
#include "up_arch.h"

#include "chip/tms570_sys.h"
#include "chip/tms570_esm.h"
#include "tms570_clockconfig.h"
#include "tms570_boot.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#ifndef CONFIG_ARMV7R_MEMINIT
#  error CONFIG_ARMV7R_MEMINIT is required by this architecture.
#endif

#ifndef CONFIG_ARCH_LOWVECTORS
#  error CONFIG_ARCH_LOWVECTORS is required by this architecture.
#endif

/****************************************************************************
 * Public Data
 ****************************************************************************/

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: tms570_event_export
 *
 * Description:
 *   Enable CPU Event Export by setting the X bit in the PMCR.  In general,
 *   this bit enables the exporting of events to another debug device, such
 *   as a trace macrocell, over an event bus.
 *
 *   For the TMS570, this allows the CPU to signal any single-bit or double
 *   -bit errors detected by its ECC logic for accesses to program flash or
 *   data RAM.
 *
 ****************************************************************************/

static inline void tms570_event_export(void)
{
  uint32_t pmcr = cp15_rdpmcr();
  pmcr |= PCMR_X;
  cp15_wrpmcr(pmcr);
}

/****************************************************************************
 * Name: tms570_enable_ramecc
 *
 * Description:
 *   This function enables the CPU's ECC logic for accesses to B0TCM and
 *   B1TCM.
 *
 ****************************************************************************/

static inline void tms570_enable_ramecc(void)
{
  uint32_t actlr = cp15_rdactlr();
  actlr |= 0x0c000000;
  cp15_wractlr(actlr);
}

/****************************************************************************
 * Name: tms570_memory_initialize
 *
 * Description:
 *   Perform memroy initialization of selected RAMs
 *
 *   This function uses the system module's hardware for auto-initialization
 *   of memories and their associated protection schemes.
 *
 ****************************************************************************/

static void tms570_memory_initialize(uint32_t ramset)
{
  /* Enable Memory Hardware Initialization */

  putreg32(SYS_MINITGCR_ENABLE, TMS570_SYS_MINITGCR);

  /* Enable Memory Hardware Initialization for selected RAM's */

  putreg32(ramset, TMS570_SYS_MSIENA);

  /* Wait until Memory Hardware Initialization complete */

  while((getreg32(TMS570_SYS_MSTCGSTAT) & SYS_MSTCGSTAT_MINIDONE) == 0);

  /* Disable Memory Hardware Initialization */

  putreg32(SYS_MINITGCR_DISABLE, TMS570_SYS_MINITGCR);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: arm_boot
 *
 * Description:
 *   Complete boot operations started in arm_head.S
 *
 * Boot Sequence
 *
 *   1.  The __start entry point in armv7-r/arm_head.S is invoked upon power-
 *       on reset.
 *   2.  __start prepares CPU for code execution.
 *   3a. If CONFIG_ARMV7R_MEMINIT is not defined, then __start will prepare
 *       memory resources by calling arm_data_initialize() and will then
 *       call this function.
 *   3b. Otherwise, this function will be called without having initialized
 *       memory resources!  We need to be very careful in this case.  Here,
 *       this function will call tms570_boardinitialize() which, among other
 *       things, much initialize SDRAM memory.  Upon return, this function
 *       will call arm_data_initialize() to initialize the memory resources
 *   4. This function will initialize all TMS570-specific resources and
 *      return to __start.
 *   4. _start will then branch to os_start() to start the operating system.
 *
 ****************************************************************************/

void arm_boot(void)
{
#ifdef CONFIG_ARCH_RAMFUNCS
  const uint32_t *src;
  uint32_t *dest;
#endif

  /* Enable CPU Event Export.
   *
   * This allows the CPU to signal any single-bit or double-bit errors
   * detected by its ECC logic for accesses to program flash or data RAM.
   */

  tms570_event_export();

  /* Read from the system exception status register to identify the cause of
   * the CPU reset.
   *
   * REVISIT: This logic is not used in the current design.  But if you
   * need to know the cause of the reset, here is where you would want
   * to do that.
   */

  DEBUGASSERT((getreg(TMS570_SYS_ESR) & SYS_ESR_PORST) != 0);

  /* Clear all reset status flags on successful power on reset */

  putreg32(SYS_ESR_RSTALL, TMS570_SYS_ESR);

  /* Check if there were ESM group3 errors during power-up.
   *
   * These could occur during eFuse auto-load or during reads from flash OTP
   * during power-up. Device operation is not reliable and not recommended
   * in this case.
   *
   * An ESM group3 error only drives the nERROR pin low. An external circuit
   * that monitors the nERROR pin must take the appropriate action to ensure
   * that the system is placed in a safe state, as determined by the
   * application.
   */

  ASSERT(getreg32(TMS570_ESM_SR3) == 0);

  /* Initialize clocking to settings provided by board-specific logic */

  tms570_clockconfig();

#ifdef CONFIG_TMS570_SELFTEST
  /* Run a diagnostic check on the memory self-test controller. */
#  warning Missing logic

  /* Run PBIST on CPU RAM. */
#  warning Missing logic

  /* Disable PBIST clocks and disable memory self-test mode */
#  warning Missing logic
#endif /* CONFIG_TMS570_SELFTEST */

  /* Initialize CPU RAM. */

  tms570_memory_initialize(SYS_MSIENA_RAM);

  /* Enable ECC checking for TCRAM accesses. */

  tms570_enable_ramecc();

#ifdef CONFIG_TMS570_SELFTEST
  /* Perform PBIST on all dual-port memories */
#warning Missing logic

  /* Test the CPU ECC mechanism for RAM accesses. */
#warning Missing logic

#endif /* CONFIG_TMS570_SELFTEST */

  /* Release the MibSPI1 modules from local reset. */
#warning Missing logic

  /* Initialize all on-chip SRAMs except for MibSPIx RAMs.
   *
   * The MibSPIx modules have their own auto-initialization mechanism which
   * is triggered as soon as the modules are brought out of local reset.
   *
   * The system module auto-init will hang on the MibSPI RAM if the module
   * is still in local reset.
   */

  tms570_memory_initialize(SYS_MSIENA_VIM_RAM | SYS_MSIENA_N2HET_RAM |
                           SYS_MSIENA_HTU_RAM | SYS_MSIENA_DCAN1_RAM |
                           SYS_MSIENA_DCAN2_RAM | SYS_MSIENA_MIBADC_RAM);

#ifdef CONFIG_ARCH_FPU
  /* Initialize the FPU */

  arm_fpuconfig();
#endif

#ifdef CONFIG_ARMV7R_MEMINIT
  /* If .data and .bss reside in SDRAM, then initialize the data sections
   * now after RAM has been initialized.
   *
   * NOTE that is SDRAM were supported, this call might have to be
   * performed after returning from tms570_board_initialize()
   */

  arm_data_initialize();
#endif

  /* Perform board-specific initialization,  This must include:
   *
   * - Initialization of board-specific memory resources (e.g., SDRAM)
   * - Configuration of board specific resources (GPIOs, LEDs, etc).
   *
   * NOTE: We must use caution prior to this point to make sure that
   * the logic does not access any global variables that might lie
   * in SDRAM.
   */

  tms570_board_initialize();

  /* Perform common, low-level chip initialization (might do nothing) */

  tms570_lowsetup();

#ifdef USE_EARLYSERIALINIT
  /* Perform early serial initialization if we are going to use the serial
   * driver.
   */

  up_earlyserialinit();
#endif
}
