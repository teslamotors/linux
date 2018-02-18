/*
 * Based on arch/arm/mm/proc.S
 *
 * Copyright (C) 2001 Deep Blue Solutions Ltd.
 * Copyright (C) 2012 ARM Ltd.
 * Author: Catalin Marinas <catalin.marinas@arm.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/init.h>
#include <linux/linkage.h>
#include <asm/assembler.h>
#include <asm/asm-offsets.h>
#include <asm/hwcap.h>
#include <asm/pgtable-hwdef.h>
#include <asm/pgtable.h>

#include "proc-macros.S"

#ifdef CONFIG_ARM64_64K_PAGES
#define TCR_TG_FLAGS	TCR_TG0_64K | TCR_TG1_64K
#else
#define TCR_TG_FLAGS	TCR_TG0_4K | TCR_TG1_4K
#endif

#ifdef CONFIG_SMP
#define TCR_SMP_FLAGS	TCR_SHARED
#else
#define TCR_SMP_FLAGS	0
#endif

/* PTWs cacheable, inner/outer WBWA */
#define TCR_CACHE_FLAGS	TCR_IRGN_WBWA | TCR_ORGN_WBWA

#define MAIR(attr, mt)	((attr) << ((mt) * 8))

/*
 *	cpu_cache_off()
 *
 *	Turn the CPU D-cache off.
 */
ENTRY(cpu_cache_off)
	mrs	x0, sctlr_el1
	bic	x0, x0, #1 << 2			// clear SCTLR.C
	msr	sctlr_el1, x0
	isb
	ret
ENDPROC(cpu_cache_off)

/*
 *	cpu_reset(loc)
 *
 *	Perform a soft reset of the system.  Put the CPU into the same state
 *	as it would be if it had been reset, and branch to what would be the
 *	reset vector. It must be executed with the flat identity mapping.
 *
 *	- loc   - location to jump to for soft reset
 */
	.align	5
ENTRY(cpu_reset)
	mrs	x1, sctlr_el1
	bic	x1, x1, #1
	msr	sctlr_el1, x1			// disable the MMU
	isb
	ret	x0
ENDPROC(cpu_reset)

ENTRY(cpu_soft_restart)
	/* Save address of cpu_reset() and reset address */
	mov	x19, x0
	mov	x20, x1

	/* Turn D-cache off */
	bl	cpu_cache_off

	/* Push out all dirty data, and ensure cache is empty */
	bl	flush_cache_all

	mov	x0, x20
	ret	x19
ENDPROC(cpu_soft_restart)

/*
 *	cpu_do_idle()
 *
 *	Idle the processor (wait for interrupt).
 */
ENTRY(cpu_do_idle)
	dsb	sy				// WFI may enter a low-power mode
	wfi
	ret
ENDPROC(cpu_do_idle)

#ifdef CONFIG_CPU_PM
/**
 * cpu_do_suspend - save CPU registers context
 *
 * x0: virtual address of context pointer
 */
ENTRY(cpu_do_suspend)
	mrs	x2, tpidr_el0
	mrs	x3, tpidrro_el0
	mrs	x4, contextidr_el1
	mrs	x5, mair_el1
	mrs	x6, cpacr_el1
	mrs	x7, ttbr1_el1
	mrs	x8, tcr_el1
	mrs	x9, vbar_el1
	mrs	x10, mdscr_el1
	mrs	x11, oslsr_el1
	stp	x2, x3, [x0]
	mrs	x2, midr_el1
	/*
	 * cpuactlr_el1 is not defined on denver since it is an
	 * implementation defined register. Accessing this on Denver
	 * would cause undefined instruction exception on Denver.
	 * So, skip accessing the register on Denver.
	 */
	lsr	x2, x2, #24
	cmp	x2, #0x4e
	beq	skip_actrl_save
	mrs	x12, s3_1_c15_c2_0
skip_actrl_save:
	mrs	x13, sctlr_el1
	stp	x4, x5, [x0, #16]
	stp	x6, x7, [x0, #32]
	stp	x8, x9, [x0, #48]
	stp	x10, x11, [x0, #64]
	/*
	 * Save x12 irrespective of it has actrl_el1's contents as it is
	 * only redundant but not incorrect as it won't be restored on
	 * Denver
	 */
	stp	x12, x13, [x0, #80]
	ret
ENDPROC(cpu_do_suspend)

/**
 * cpu_do_resume - restore CPU register context
 *
 * x0: Physical address of context pointer
 * x1: ttbr0_el1 to be restored
 *
 * Returns:
 *	sctlr_el1 value in x0
 */
ENTRY(cpu_do_resume)
	/*
	 * Invalidate local tlb entries before turning on MMU
	 */
	tlbi	vmalle1
	ldp	x2, x3, [x0]
	ldp	x4, x5, [x0, #16]
	ldp	x6, x7, [x0, #32]
	ldp	x8, x9, [x0, #48]
	ldp	x10, x11, [x0, #64]
	ldp	x12, x13, [x0, #80]
	msr	tpidr_el0, x2
	msr	tpidrro_el0, x3
	msr	contextidr_el1, x4
	msr	mair_el1, x5
	msr	cpacr_el1, x6
	msr	ttbr0_el1, x1
	msr	ttbr1_el1, x7
	msr	tcr_el1, x8
	msr	vbar_el1, x9
	msr	mdscr_el1, x10
	/*
	 * Restore oslsr_el1 by writing oslar_el1
	 */
	ubfx	x11, x11, #1, #1
	msr	oslar_el1, x11
	mrs	x2, midr_el1
	/*
	 * cpuactlr_el1 is an implementation defined register
	 * and Denver uses it to indicate the power state to enter.
	 * If restored same value on restore then it is possible
	 * that CPU would enter unexpected power state on next WFI.
	 * Hence skip restoring this register for Denver.
	 */
	lsr	x2, x2, #24
	cmp	x2, #0x4e
	beq	skip_actlr_restore
	msr	s3_1_c15_c2_0, x12
skip_actlr_restore:
	mov	x0, x13
	dsb	nsh		// Make sure local tlb invalidation completed
	isb
	ret
ENDPROC(cpu_do_resume)
#endif

/*
 *	cpu_do_switch_mm(pgd_phys, tsk)
 *
 *	Set the translation table base pointer to be pgd_phys.
 *
 *	- pgd_phys - physical address of new TTB
 */
ENTRY(cpu_do_switch_mm)
	mmid	w1, x1				// get mm->context.id
	bfi	x0, x1, #48, #16		// set the ASID
	msr	ttbr0_el1, x0			// set TTBR0
	isb
	ret
ENDPROC(cpu_do_switch_mm)

	.section ".text.init", #alloc, #execinstr

/*
 *	__cpu_setup
 *
 *	Initialise the processor for turning the MMU on.  Return in x0 the
 *	value of the SCTLR_EL1 register.
 */
ENTRY(__cpu_setup)
	ic	iallu				// I+BTB cache invalidate
	tlbi	vmalle1is			// invalidate I + D TLBs
	dsb	ish

__cpu_setup_post_flush:

	mov	x0, #3 << 20
	msr	cpacr_el1, x0			// Enable FP/ASIMD
	mov	x0, #1 << 14			// Enable MDSCR_EL1.HDE
	msr	mdscr_el1, x0			// only set HDE in mdscr_el1
	isb					// Unmask debug exceptions now,
	enable_dbg				// since this is per-cpu
	/*
	 * Memory region attributes for LPAE:
	 *
	 *   n = AttrIndx[2:0]
	 *			n	MAIR
	 *   DEVICE_nGnRnE	000	00000000
	 *   DEVICE_nGnRE	001	00000100
	 *   DEVICE_GRE		010	00001100
	 *   NORMAL_NC		011	01000100
	 *   NORMAL		100	11111111
	 */
	ldr	x5, =MAIR(0x00, MT_DEVICE_nGnRnE) | \
		     MAIR(0x04, MT_DEVICE_nGnRE) | \
		     MAIR(0x0c, MT_DEVICE_GRE) | \
		     MAIR(0x44, MT_NORMAL_NC) | \
		     MAIR(0xff, MT_NORMAL)
	msr	mair_el1, x5
	/*
	 * Prepare SCTLR
	 */
	adr	x5, crval
	ldp	w5, w6, [x5]
	mrs	x0, sctlr_el1
	bic	x0, x0, x5			// clear bits
	orr	x0, x0, x6			// set bits
	/*
	 * Set/prepare TCR and TTBR. We use 512GB (39-bit) address range for
	 * both user and kernel.
	 */
	ldr	x10, =TCR_TxSZ(VA_BITS) | TCR_CACHE_FLAGS | TCR_SMP_FLAGS | \
			TCR_TG_FLAGS | TCR_ASID16 | TCR_TBI0
	/*
	 * Read the PARange bits from ID_AA64MMFR0_EL1 and set the IPS bits in
	 * TCR_EL1.
	 */
	mrs	x9, ID_AA64MMFR0_EL1
	bfi	x10, x9, #32, #3
	msr	tcr_el1, x10
	ret					// return to head.S
ENDPROC(__cpu_setup)

ENTRY(__cortexa53_cpu_setup)
	/*
	 * no need to flush cache and invalidate tlb and btbs because
	 * cortex-a53 invalidates them already during reset
	 */
	b	__cpu_setup_post_flush
ENDPROC(__cortexa53_cpu_setup)

ENTRY(__cortexa57_cpu_setup)

	mrs	x0, s3_1_c15_c2_0
	/* enable non-cacheable streaming enhancement */
	orr	x0, x0, #(1<<24)
	msr	s3_1_c15_c2_0, x0
	mrs	x0, s3_1_c11_c0_2
	and	x1, x0, #7
	cmp	x1, #2
	b.eq	__cpu_setup_post_flush
	mov	x1, #7
	bic	x0, x0, x1
	orr	x0, x0, #2			// set data RAM latency to 3
	/*
	 * no need to flush cache and invalidate tlb and btbs because
	 * cortex-a57 invalidates them already during reset
	 */
	msr	s3_1_c11_c0_2, x0

	b	__cpu_setup_post_flush
ENDPROC(__cortexa57_cpu_setup)

	/*
	 *                 n n            T
	 *       U E      WT T UD     US IHBS
	 *       CE0      XWHW CZ     ME TEEA S
	 * .... .IEE .... NEAI TE.I ..AD DEN0 ACAM
	 * 0011 0... 1101 ..0. ..0. 10.. .... .... < hardware reserved
	 * .... .1.. .... 01.1 11.1 ..01 0001 1101 < software settings
	 */
	.type	crval, #object
crval:
	.word	0x000802e2			// clear
	.word	0x0405d11d			// set
