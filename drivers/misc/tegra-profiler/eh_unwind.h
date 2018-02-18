/*
 * drivers/misc/tegra-profiler/eh_unwind.h
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

#ifndef __QUADD_EH_UNWIND_H__
#define __QUADD_EH_UNWIND_H__

#include <linux/tegra_profiler.h>

struct pt_regs;
struct quadd_callchain;
struct quadd_ctx;
struct quadd_extables;
struct task_struct;
struct quadd_mmap_area;

unsigned int
quadd_get_user_cc_arm32_ehabi(struct pt_regs *regs,
			      struct quadd_callchain *cc,
			      struct task_struct *task,
			      int is_sched);

int quadd_unwind_init(void);
void quadd_unwind_deinit(void);

int quadd_unwind_start(struct task_struct *task);
void quadd_unwind_stop(void);

int quadd_unwind_set_extab(struct quadd_sections *extabs,
			   struct quadd_mmap_area *mmap);
void quadd_unwind_delete_mmap(struct quadd_mmap_area *mmap);

int
quadd_is_ex_entry_exist_arm32_ehabi(struct pt_regs *regs,
				    unsigned long addr,
				    struct task_struct *task);

void
quadd_unwind_set_tail_info(unsigned long vm_start,
			   int secid,
			   unsigned long tf_start,
			   unsigned long tf_end);


struct extab_info {
	unsigned long addr;
	unsigned long length;

	unsigned long mmap_offset;

	unsigned long tf_start;
	unsigned long tf_end;
};

struct ex_region_info {
	unsigned long vm_start;
	unsigned long vm_end;

	struct extab_info ex_sec[QUADD_SEC_TYPE_MAX];
	struct quadd_mmap_area *mmap;

	struct list_head list;
};

long
quadd_get_dw_frames(unsigned long key, struct ex_region_info *ri);

#endif	/* __QUADD_EH_UNWIND_H__ */
