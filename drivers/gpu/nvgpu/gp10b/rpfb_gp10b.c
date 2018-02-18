/*
 * GP10B RPFB
 *
 * Copyright (c) 2015, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/pm_runtime.h>
#include <linux/dma-mapping.h>
#include "gk20a/gk20a.h"
#include "rpfb_gp10b.h"
#include "hw_fifo_gp10b.h"
#include "hw_fb_gp10b.h"
#include "hw_bus_gp10b.h"
#include "hw_gmmu_gp10b.h"

int gp10b_replayable_pagefault_buffer_init(struct gk20a *g)
{
	u32 addr_lo;
	u32 addr_hi;
	struct vm_gk20a *vm = &g->mm.bar2.vm;
	int err;
	size_t rbfb_size = NV_UVM_FAULT_BUF_SIZE *
		fifo_replay_fault_buffer_size_hw_entries_v();

	gk20a_dbg_fn("");

	if (!g->mm.bar2_desc.gpu_va) {
		err = gk20a_gmmu_alloc_map_sys(vm, rbfb_size,
						&g->mm.bar2_desc);
		if (err) {
			dev_err(dev_from_gk20a(g),
			"%s Error in replayable fault buffer\n", __func__);
			return err;
		}
	}
	addr_lo = u64_lo32(g->mm.bar2_desc.gpu_va >> 12);
	addr_hi = u64_hi32(g->mm.bar2_desc.gpu_va);
	gk20a_writel(g, fifo_replay_fault_buffer_hi_r(),
			fifo_replay_fault_buffer_hi_base_f(addr_hi));

	gk20a_writel(g, fifo_replay_fault_buffer_lo_r(),
			fifo_replay_fault_buffer_lo_base_f(addr_lo) |
			fifo_replay_fault_buffer_lo_enable_true_v());
	gk20a_dbg_fn("done");
	return 0;
}

void gp10b_replayable_pagefault_buffer_deinit(struct gk20a *g)
{
	struct vm_gk20a *vm = &g->mm.bar2.vm;

	gk20a_gmmu_unmap_free(vm, &g->mm.bar2_desc);
}

u32 gp10b_replayable_pagefault_buffer_get_index(struct gk20a *g)
{
	u32 get_idx = 0;

	gk20a_dbg_fn("");

	get_idx = gk20a_readl(g, fifo_replay_fault_buffer_get_r());

	if (get_idx >= fifo_replay_fault_buffer_size_hw_entries_v())
		dev_err(dev_from_gk20a(g), "%s Error in replayable fault buffer\n",
			__func__);
	gk20a_dbg_fn("done");
	return get_idx;
}

u32 gp10b_replayable_pagefault_buffer_put_index(struct gk20a *g)
{
	u32 put_idx = 0;

	gk20a_dbg_fn("");
	put_idx = gk20a_readl(g, fifo_replay_fault_buffer_put_r());

	if (put_idx >= fifo_replay_fault_buffer_size_hw_entries_v())
		dev_err(dev_from_gk20a(g), "%s Error in UVM\n",
			__func__);
	gk20a_dbg_fn("done");
	return put_idx;
}

bool gp10b_replayable_pagefault_buffer_is_empty(struct gk20a *g)
{
	u32 get_idx = gk20a_readl(g, fifo_replay_fault_buffer_get_r());
	u32 put_idx = gk20a_readl(g, fifo_replay_fault_buffer_put_r());

	return (get_idx == put_idx ? true : false);
}

bool gp10b_replayable_pagefault_buffer_is_full(struct gk20a *g)
{
	u32 get_idx = gk20a_readl(g, fifo_replay_fault_buffer_get_r());
	u32 put_idx = gk20a_readl(g, fifo_replay_fault_buffer_put_r());
	u32 hw_entries = gk20a_readl(g, fifo_replay_fault_buffer_size_r());

	return (get_idx == ((put_idx + 1) % hw_entries) ? true : false);
}

bool gp10b_replayable_pagefault_buffer_is_overflow(struct gk20a *g)
{
	u32 info = gk20a_readl(g, fifo_replay_fault_buffer_info_r());

	return fifo_replay_fault_buffer_info_overflow_f(info);
}

void gp10b_replayable_pagefault_buffer_clear_overflow(struct gk20a *g)
{
	u32 info = gk20a_readl(g, fifo_replay_fault_buffer_info_r());

	info |= fifo_replay_fault_buffer_info_overflow_clear_v();
	gk20a_writel(g, fifo_replay_fault_buffer_info_r(), info);

}

void gp10b_replayable_pagefault_buffer_info(struct gk20a *g)
{

	gk20a_dbg_fn("");
	pr_info("rpfb low: 0x%x\n",
		(gk20a_readl(g, fifo_replay_fault_buffer_lo_r()) >> 12));
	pr_info("rpfb hi: 0x%x\n",
		gk20a_readl(g, fifo_replay_fault_buffer_hi_r()));
	pr_info("rpfb enabled: 0x%x\n",
		(gk20a_readl(g, fifo_replay_fault_buffer_lo_r()) & 0x1));
	pr_info("rpfb size: %d\n",
		gk20a_readl(g, fifo_replay_fault_buffer_size_r()));
	pr_info("rpfb get index: %d\n",
		gp10b_replayable_pagefault_buffer_get_index(g));
	pr_info("rpfb put index: %d\n",
		gp10b_replayable_pagefault_buffer_put_index(g));
	pr_info("rpfb empty: %d\n",
		gp10b_replayable_pagefault_buffer_is_empty(g));
	pr_info("rpfb full  %d\n",
		gp10b_replayable_pagefault_buffer_is_full(g));
	pr_info("rpfb overflow  %d\n",
		gp10b_replayable_pagefault_buffer_is_overflow(g));

	gk20a_dbg_fn("done");
}
