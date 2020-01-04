/*
 * arch/arm/mach-tegra/headsmp.S
 *
 * SMP initialization routines for Tegra SoCs
 *
 * Copyright (c) 2009-2010, NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/linkage.h>
#include <linux/init.h>

#include <asm/assembler.h>
#include <asm/cache.h>

#include <mach/iomap.h>
#include <mach/io.h>

#include "power-macros.S"

#define RST_DEVICES_U	0xc

/*        .section ".cpuinit.text", "ax"*/

.macro	poke_ev, val, tmp
	mov32	\tmp, (TEGRA_EXCEPTION_VECTORS_BASE + 0x100)
	str	\val, [\tmp]
.endm

/*
 *	tegra_secondary_startup
 *
 *	 Initial secondary processor boot vector; jumps to kernel's
 *	 secondary_startup routine
 */
ENTRY(tegra_secondary_startup)
	msr	cpsr_fsxc, #0xd3
	bl	__invalidate_cpu_state
	cpu_id	r0
	enable_coresite r1
	poke_ev	r0, r1
	b	secondary_startup
ENDPROC(tegra_secondary_startup)

/*
 *	__enable_coresite_access
 *
 *	  Called only on CPU0 to take the CoreSight debug interface out of
 *	  reset. Called with MMU disabled.
 */
	.align L1_CACHE_SHIFT
ENTRY(__enable_coresite_access)
	mov32	r0, (TEGRA_CLK_RESET_BASE + RST_DEVICES_U)
	mov32	r2, (TEGRA_TMRUS_BASE)

	/* assert reset for 2usec */
	ldr	r1, [r0]
#ifndef CONFIG_TEGRA_FPGA_PLATFORM
	orr	r1, #(1<<9)
	str	r1, [r0]
#endif
	wait_for_us r3, r2, r4
	add	r3, r3, #2
	bic	r1, r1, #(1<<9)
	wait_until r3, r2, r4
	str	r1, [r0]
	/* Enable CoreSight */
	enable_coresite r3
	bx	lr
ENDPROC(__enable_coresite_access)
