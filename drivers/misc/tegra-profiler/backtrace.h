/*
 * drivers/misc/tegra-profiler/backtrace.h
 *
 * Copyright (c) 2015-2016, NVIDIA CORPORATION.  All rights reserved.
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
 */

#ifndef __QUADD_BACKTRACE_H
#define __QUADD_BACKTRACE_H

#include <linux/mm.h>
#include <linux/bitops.h>

#define QUADD_MAX_STACK_DEPTH		64

#define QUADD_UNW_TYPES_SIZE \
	DIV_ROUND_UP(QUADD_MAX_STACK_DEPTH * 4, sizeof(u32) * BITS_PER_BYTE)

struct quadd_hrt_ctx;

struct quadd_unw_methods {
	unsigned int
		fp:1,
		ut:1,
		ut_ce:1,
		dwarf:1;
};

struct quadd_callchain {
	int nr;

	union {
		u32 ip_32[QUADD_MAX_STACK_DEPTH];
		u64 ip_64[QUADD_MAX_STACK_DEPTH];
	};

	u32 types[QUADD_UNW_TYPES_SIZE];

	int cs_64;

	struct quadd_unw_methods um;

	unsigned int urc_fp;
	unsigned int urc_ut;
	unsigned int urc_dwarf;

	unsigned long curr_sp;
	unsigned long curr_fp;
	unsigned long curr_fp_thumb;
	unsigned long curr_pc;
	unsigned long curr_lr;

	struct quadd_hrt_ctx *hrt;
};

struct quadd_ctx;
struct pt_regs;

unsigned int
quadd_get_user_callchain(struct pt_regs *regs,
			 struct quadd_callchain *cc_data,
			 struct quadd_ctx *ctx,
			 struct task_struct *task,
			 int is_sched);

int
quadd_callchain_store(struct quadd_callchain *cc,
		      unsigned long ip, unsigned int type);

unsigned long
quadd_user_stack_pointer(struct pt_regs *regs);

unsigned long
quadd_get_user_frame_pointer(struct pt_regs *regs);

unsigned long
quadd_user_link_register(struct pt_regs *regs);

static inline int
is_vma_addr(unsigned long addr, struct vm_area_struct *vma,
	    unsigned long nbytes)
{
	return	vma &&
		addr >= vma->vm_start &&
		addr < vma->vm_end - nbytes;
}

static inline int
validate_pc_addr(unsigned long addr, unsigned long nbytes)
{
	return addr && addr < TASK_SIZE - nbytes;
}

static inline int
validate_stack_addr(unsigned long addr,
		    struct vm_area_struct *vma,
		    unsigned long nbytes)
{
	if (addr & 0x03)
		return 0;

	return is_vma_addr(addr, vma, nbytes);
}

#endif  /* __QUADD_BACKTRACE_H */
