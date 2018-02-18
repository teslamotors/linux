/*
 * Pascal GPU series Copy Engine.
 *
 * Copyright (c) 2011-2016, NVIDIA CORPORATION.  All rights reserved.
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

#include "gk20a/gk20a.h" /* FERMI and MAXWELL classes defined here */
#include "hw_ce_gp10b.h"
#include "ce_gp10b.h"

static void ce_nonblockpipe_isr(struct gk20a *g, u32 fifo_intr)
{
	gk20a_dbg(gpu_dbg_intr, "ce non-blocking pipe interrupt\n");

	/* wake theads waiting in this channel */
	gk20a_channel_semaphore_wakeup(g, true);
	return;
}

static u32 ce_blockpipe_isr(struct gk20a *g, u32 fifo_intr)
{
	gk20a_dbg(gpu_dbg_intr, "ce blocking pipe interrupt\n");

	return ce_intr_status_blockpipe_pending_f();
}

static u32 ce_launcherr_isr(struct gk20a *g, u32 fifo_intr)
{
	gk20a_dbg(gpu_dbg_intr, "ce launch error interrupt\n");

	return ce_intr_status_launcherr_pending_f();
}

static void gp10b_ce_isr(struct gk20a *g, u32 inst_id, u32 pri_base)
{
	u32 ce_intr = gk20a_readl(g, ce_intr_status_r(inst_id));
	u32 clear_intr = 0;

	gk20a_dbg(gpu_dbg_intr, "ce isr %08x %08x\n", ce_intr, inst_id);

	/* clear blocking interrupts: they exibit broken behavior */
	if (ce_intr & ce_intr_status_blockpipe_pending_f())
		clear_intr |= ce_blockpipe_isr(g, ce_intr);

	if (ce_intr & ce_intr_status_launcherr_pending_f())
		clear_intr |= ce_launcherr_isr(g, ce_intr);

	gk20a_writel(g, ce_intr_status_r(inst_id), clear_intr);
	return;
}

static void gp10b_ce_nonstall_isr(struct gk20a *g, u32 inst_id, u32 pri_base)
{
	u32 ce_intr = gk20a_readl(g, ce_intr_status_r(inst_id));

	gk20a_dbg(gpu_dbg_intr, "ce nonstall isr %08x %08x\n", ce_intr, inst_id);

	if (ce_intr & ce_intr_status_nonblockpipe_pending_f()) {
		gk20a_writel(g, ce_intr_status_r(inst_id),
			ce_intr_status_nonblockpipe_pending_f());
		ce_nonblockpipe_isr(g, ce_intr);
	}

	return;
}
void gp10b_init_ce(struct gpu_ops *gops)
{
	gops->ce2.isr_stall = gp10b_ce_isr;
	gops->ce2.isr_nonstall = gp10b_ce_nonstall_isr;
}
