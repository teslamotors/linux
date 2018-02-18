/*
 * Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/linkage.h>
#include <linux/init.h>

#include <asm/cache.h>
#include <asm/asm-offsets.h>
#include <asm/assembler.h>
#include <asm/memory.h>

#include <linux/platform/tegra/flowctrl.h>
#include "iomap.h"
#include <linux/platform/tegra/reset.h>

#ifdef CONFIG_PM_SLEEP
/* start of timestamps */
.globl tegra_resume_timestamps_start
tegra_resume_timestamps_start:

.globl tegra_resume_smc_entry_time
tegra_resume_smc_entry_time:
	.long	0
.globl tegra_resume_smc_exit_time
tegra_resume_smc_exit_time:
	.long	0
.globl tegra_resume_entry_time
tegra_resume_entry_time:
	.long	0

/* end of timestamps */
.global tegra_resume_timestamps_end
tegra_resume_timestamps_end:
#endif

	.align L1_CACHE_SHIFT
ENTRY(__tegra_cpu_reset_handler_start)

/*
 * __tegra_cpu_reset_handler:
 *
 * Common handler for all CPU reset events.
 *
 * Register usage within the reset handler:
 *
 *      Others: scratch
 *      x6  = SoC ID << 8
 *      x7  = CPU present (to the OS) mask
 *      x8  = CPU in LP1 state mask
 *      x9  = CPU in LP2 state mask
 *      x10 = CPU number
 *      x11 = CPU mask
 *      x12 = pointer to reset handler data
 *
 * NOTE: This code is copied to IRAM. All code and data accesses
 *       must be position-independent.
 */

	.align L1_CACHE_SHIFT
ENTRY(__tegra_cpu_reset_handler)
	disable_irq

#ifdef CONFIG_SMP
	mrs	x10, mpidr_el1
#else
	mov x10, #0
#endif

	and	x10, x10, #0xf
	and	x10, x10, #0x3		/* x10 = cpu id */

	mov x11, #1
	lsl x11, x11, x10		/* x11 = CPU mask */

	adr x12, __tegra_cpu_reset_handler_data

#ifdef CONFIG_SMP
	/* Does the OS know about this CPU? */
	ldr	x7, [x12, #RESET_DATA(MASK_PRESENT)]
	tst	x7, x11 		/* if !present */
	beq		__die		/* CPU not present (to OS) */
#endif

	/* Waking up from LP1? */
	ldr	x8, [x12, #RESET_DATA(MASK_LP1)]
	tst	x8, x11				/* if in_lp1 */
	beq	__is_not_lp1
	cmp	x10, #0
	bne	__die				/* only CPU0 can be here */
	ldr	lr, [x12, #RESET_DATA(STARTUP_LP1)]
	cmp	lr, #0
	beq	__die				/* no LP1 startup handler */
	ret
__is_not_lp1:

	/* Waking up from LP2? */
	ldr	x9, [x12, #RESET_DATA(MASK_LP2)]
	tst	x9, x11				/* if in_lp2 */
	beq	__is_not_lp2
	ldr	lr, [x12, #RESET_DATA(STARTUP_LP2)]
	cmp	lr, #0
	beq	__die				/* no LP2 startup handler */
	ret

__is_not_lp2:

#ifdef CONFIG_SMP
	ldr	lr, [x12, #RESET_DATA(STARTUP_SECONDARY)]
	cmp	lr, #0
	beq	__die			/* no secondary startup handler */
	ret
#endif

/*
 * We don't know why the CPU reset. Just kill it.
 */

__die:
	/* just spin here for now */
	b	.
ENDPROC(__tegra_cpu_reset_handler)

	.align L1_CACHE_SHIFT
	.type	__tegra_cpu_reset_handler_data, %object
	.globl	__tegra_cpu_reset_handler_data
__tegra_cpu_reset_handler_data:
	.rept	TEGRA_RESET_DATA_SIZE
	.quad	0
	.endr
	.align L1_CACHE_SHIFT

ENTRY(__tegra_cpu_reset_handler_end)
