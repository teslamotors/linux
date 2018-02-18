/*
 * Copyright (c) 2014, NVIDIA Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <asm/asm-offsets.h>
#include <asm/ptrace.h>
#include <linux/linkage.h>
#include <linux/init.h>

#ifdef CONFIG_ARM64

/* uint32_t tlk_generic_smc(uint32_t arg0, uint32_t arg1, uint32_t arg2) */
ENTRY(_tlk_generic_smc)
	smc	#0
	ret
ENDPROC(_tlk_generic_smc)

#ifdef CONFIG_TRUSTED_LITTLE_KERNEL
ENTRY(tlk_fiq_glue_aarch64)
	sub	sp, sp, #S_FRAME_SIZE
	stp	x0, x1, [sp, #16 * 0]
	stp	x2, x3, [sp, #16 * 1]
	stp	x4, x5, [sp, #16 * 2]
	stp	x6, x7, [sp, #16 * 3]
	stp	x8, x9, [sp, #16 * 4]
	stp	x10, x11, [sp, #16 * 5]
	stp	x12, x13, [sp, #16 * 6]
	stp	x14, x15, [sp, #16 * 7]
	stp	x16, x17, [sp, #16 * 8]
	stp	x18, x19, [sp, #16 * 9]
	stp	x20, x21, [sp, #16 * 10]
	stp	x22, x23, [sp, #16 * 11]
	stp	x24, x25, [sp, #16 * 12]
	stp	x26, x27, [sp, #16 * 13]
	stp	x28, x29, [sp, #16 * 14]
	ldr	x0, =0x82000006		/* get FIQ regs */
	smc	#0
	stp	x0, x1, [sp, #S_PC]	/* original pc, cpsr */
	tst	x1, PSR_MODE_MASK
	csel	x2, x2, x3, eq		/* sp el0, sp el1 */
	stp	x30, x2, [sp, #S_LR]	/* lr, original sp */
	mov	x0, sp
	mov	x1, x3
	bl	tlk_fiq_handler		/* handle FIQ exception */
	b	.
ENDPROC(tlk_fiq_glue_aarch64)
#endif

#else

ENTRY(_tlk_generic_smc)
	smc	#0
	mov	pc, lr
ENDPROC(_tlk_generic_smc)

#endif
