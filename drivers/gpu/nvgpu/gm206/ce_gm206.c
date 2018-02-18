/*
 * GM206 Copy Engine.
 *
 * Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
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
 * You should have received a copy of the GNU General Public License along with
 * this program.
 */

/*TODO: remove uncecessary */
#include "gk20a/gk20a.h"
#include "ce_gm206.h"

/*TODO: remove uncecessary */
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/scatterlist.h>
#include <trace/events/gk20a.h>
#include <linux/dma-mapping.h>
#include <linux/nvhost.h>

#include "gk20a/debug_gk20a.h"
#include "gk20a/semaphore_gk20a.h"
#include "hw_ce2_gm206.h"
#include "hw_pbdma_gm206.h"
#include "hw_ccsr_gm206.h"
#include "hw_ram_gm206.h"
#include "hw_top_gm206.h"
#include "hw_mc_gm206.h"
#include "hw_gr_gm206.h"

/* TODO: We need generic way for query the intr_status register offset.
 * As of now, there is no way to query this information from dev_ceN_pri.h */
#define COP_INTR_STATUS_OFFSET 0x908

static u32 ce_nonblockpipe_isr(struct gk20a *g, u32 fifo_intr, u32 inst_id)
{
	gk20a_dbg(gpu_dbg_intr, "ce non-blocking pipe interrupt\n");

	return ce2_intr_status_nonblockpipe_pending_f();
}

static u32 ce_blockpipe_isr(struct gk20a *g, u32 fifo_intr, u32 inst_id)
{
	gk20a_dbg(gpu_dbg_intr, "ce blocking pipe interrupt\n");

	return ce2_intr_status_blockpipe_pending_f();
}

static u32 ce_launcherr_isr(struct gk20a *g, u32 fifo_intr, u32 inst_id)
{
	gk20a_dbg(gpu_dbg_intr, "ce launch error interrupt\n");

	return ce2_intr_status_launcherr_pending_f();
}

static void gm206_ce_isr(struct gk20a *g, u32 inst_id, u32 pri_base)
{
	u32 ce_intr_status_reg = (pri_base + COP_INTR_STATUS_OFFSET);
	u32 ce_intr = gk20a_readl(g, ce_intr_status_reg);
	u32 clear_intr = 0;

	gk20a_dbg(gpu_dbg_intr, "ce isr %08x %08x\n", ce_intr, inst_id);

	/* clear blocking interrupts: they exibit broken behavior */
	if (ce_intr & ce2_intr_status_blockpipe_pending_f())
		clear_intr |= ce_blockpipe_isr(g, ce_intr, inst_id);

	if (ce_intr & ce2_intr_status_launcherr_pending_f())
		clear_intr |= ce_launcherr_isr(g, ce_intr, inst_id);

	gk20a_writel(g, ce_intr_status_reg, clear_intr);
	return;
}

static void gm206_ce_nonstall_isr(struct gk20a *g, u32 inst_id, u32 pri_base)
{
	u32 ce_intr_status_reg = (pri_base + COP_INTR_STATUS_OFFSET);
	u32 ce_intr = gk20a_readl(g, ce_intr_status_reg);

	gk20a_dbg(gpu_dbg_intr, "ce nonstall isr %08x %08x\n", ce_intr, inst_id);

	if (ce_intr & ce2_intr_status_nonblockpipe_pending_f()) {
		gk20a_writel(g, ce_intr_status_reg,
			ce_nonblockpipe_isr(g, ce_intr, inst_id));

		/* wake threads waiting in this channel */
		gk20a_channel_semaphore_wakeup(g, true);
	}

	return;
}

void gm206_init_ce(struct gpu_ops *gops)
{
	gops->ce2.isr_stall = gm206_ce_isr;
	gops->ce2.isr_nonstall = gm206_ce_nonstall_isr;
}
