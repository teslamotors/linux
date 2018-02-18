/*
 * arch/arm64/mach-tegra/sleep.S
 *
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/init.h>
#include <linux/linkage.h>

#include <asm/assembler.h>
#include <asm/cache.h>

#include "iomap.h"
#include "sleep.h"

#ifdef CONFIG_PM_SLEEP
/*
 * tegra_turn_off_mmu
 *
 * x0 = v2p
 * x1 = physical address to jump to with mmu off
 */
ENTRY(tegra_turn_off_mmu)
	tlbi	vmalle1is	// invalidate TLB
	ic	iallu			// I+BTB cache invalidate
	dsb sy
	isb

	mov32	x3, tegra_shut_off_mmu
	add		x3, x3, x0
	mov		x0, x1
	br		x3
ENDPROC(tegra_turn_off_mmu)

/*
 * tegra_shut_off_mmu
 *
 * x0 = physical address to jump to with mmu off
 *
 * called with VA=PA mapping
 * turns off MMU, icache, dcache and branch prediction
 */
	.align	L1_CACHE_SHIFT
ENTRY(tegra_shut_off_mmu)
	mrs	x1, sctlr_el1
	bic	x1, x1, #1
	dsb sy
	msr	sctlr_el1, x1
	isb
	br 	x0
ENDPROC(tegra_turn_off_mmu)

#endif /* CONFIG_PM_SLEEP */

#ifdef CONFIG_SMP
/*
 *  tegra_secondary_startup
 *
 *   Initial secondary processor boot vector; jumps to kernel's
 *   secondary_startup routine. Used for initial boot and hotplug
 *   of secondary CPUs.
 */
    __CPUINIT
ENTRY(tegra_secondary_startup)
    b   secondary_startup
ENDPROC(tegra_secondary_startup)

#endif /* CONFIG_SMP */
