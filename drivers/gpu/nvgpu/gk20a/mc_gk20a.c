/*
 * GK20A Master Control
 *
 * Copyright (c) 2014-2016, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/types.h>
#include <trace/events/gk20a.h>

#include "gk20a.h"
#include "mc_gk20a.h"
#include "hw_mc_gk20a.h"

irqreturn_t mc_gk20a_isr_stall(struct gk20a *g)
{
	u32 mc_intr_0;

	trace_mc_gk20a_intr_stall(dev_name(g->dev));

	if (!g->power_on)
		return IRQ_NONE;

	/* not from gpu when sharing irq with others */
	mc_intr_0 = gk20a_readl(g, mc_intr_0_r());
	if (unlikely(!mc_intr_0))
		return IRQ_NONE;

	gk20a_writel(g, mc_intr_en_0_r(),
		mc_intr_en_0_inta_disabled_f());

	/* flush previous write */
	gk20a_readl(g, mc_intr_en_0_r());

	atomic_inc(&g->hw_irq_stall_count);

	trace_mc_gk20a_intr_stall_done(dev_name(g->dev));

	return IRQ_WAKE_THREAD;
}

irqreturn_t mc_gk20a_isr_nonstall(struct gk20a *g)
{
	u32 mc_intr_1;

	if (!g->power_on)
		return IRQ_NONE;

	/* not from gpu when sharing irq with others */
	mc_intr_1 = gk20a_readl(g, mc_intr_1_r());
	if (unlikely(!mc_intr_1))
		return IRQ_NONE;

	gk20a_writel(g, mc_intr_en_1_r(),
		mc_intr_en_1_inta_disabled_f());

	/* flush previous write */
	gk20a_readl(g, mc_intr_en_1_r());

	atomic_inc(&g->hw_irq_nonstall_count);

	return IRQ_WAKE_THREAD;
}

irqreturn_t mc_gk20a_intr_thread_stall(struct gk20a *g)
{
	u32 mc_intr_0;
	int hw_irq_count;
	u32 engine_id_idx;
	u32 active_engine_id = 0;
	u32 engine_enum = ENGINE_INVAL_GK20A;

	gk20a_dbg(gpu_dbg_intr, "interrupt thread launched");

	trace_mc_gk20a_intr_thread_stall(dev_name(g->dev));

	mc_intr_0 = gk20a_readl(g, mc_intr_0_r());
	hw_irq_count = atomic_read(&g->hw_irq_stall_count);

	gk20a_dbg(gpu_dbg_intr, "stall intr %08x\n", mc_intr_0);

	for (engine_id_idx = 0; engine_id_idx < g->fifo.num_engines; engine_id_idx++) {
		active_engine_id = g->fifo.active_engines_list[engine_id_idx];

		if (mc_intr_0 & g->fifo.engine_info[active_engine_id].intr_mask) {
			engine_enum = g->fifo.engine_info[active_engine_id].engine_enum;
			/* GR Engine */
			if (engine_enum == ENGINE_GR_GK20A) {
				gr_gk20a_elpg_protected_call(g, gk20a_gr_isr(g));
			}

			/* CE Engine */
			if (((engine_enum == ENGINE_GRCE_GK20A) ||
				(engine_enum == ENGINE_ASYNC_CE_GK20A)) &&
				g->ops.ce2.isr_stall){
					g->ops.ce2.isr_stall(g,
					g->fifo.engine_info[active_engine_id].inst_id,
					g->fifo.engine_info[active_engine_id].pri_base);
			}
		}
	}
	if (mc_intr_0 & mc_intr_0_pfifo_pending_f())
		gk20a_fifo_isr(g);
	if (mc_intr_0 & mc_intr_0_pmu_pending_f())
		gk20a_pmu_isr(g);
	if (mc_intr_0 & mc_intr_0_priv_ring_pending_f())
		gk20a_priv_ring_isr(g);
	if (mc_intr_0 & mc_intr_0_ltc_pending_f())
		g->ops.ltc.isr(g);
	if (mc_intr_0 & mc_intr_0_pbus_pending_f())
		gk20a_pbus_isr(g);

	/* sync handled irq counter before re-enabling interrupts */
	atomic_set(&g->sw_irq_stall_last_handled, hw_irq_count);

	gk20a_writel(g, mc_intr_en_0_r(),
		mc_intr_en_0_inta_hardware_f());

	/* flush previous write */
	gk20a_readl(g, mc_intr_en_0_r());

	wake_up_all(&g->sw_irq_stall_last_handled_wq);

	trace_mc_gk20a_intr_thread_stall_done(dev_name(g->dev));

	return IRQ_HANDLED;
}

irqreturn_t mc_gk20a_intr_thread_nonstall(struct gk20a *g)
{
	u32 mc_intr_1;
	int hw_irq_count;
	u32 engine_id_idx;
	u32 active_engine_id = 0;
	u32 engine_enum = ENGINE_INVAL_GK20A;

	gk20a_dbg(gpu_dbg_intr, "interrupt thread launched");

	mc_intr_1 = gk20a_readl(g, mc_intr_1_r());
	hw_irq_count = atomic_read(&g->hw_irq_nonstall_count);

	gk20a_dbg(gpu_dbg_intr, "non-stall intr %08x\n", mc_intr_1);

	if (mc_intr_1 & mc_intr_0_pfifo_pending_f())
		gk20a_fifo_nonstall_isr(g);
	if (mc_intr_1 & mc_intr_0_priv_ring_pending_f())
		gk20a_priv_ring_isr(g);

	for (engine_id_idx = 0; engine_id_idx < g->fifo.num_engines; engine_id_idx++) {
		active_engine_id = g->fifo.active_engines_list[engine_id_idx];

		if (mc_intr_1 & g->fifo.engine_info[active_engine_id].intr_mask) {
			engine_enum = g->fifo.engine_info[active_engine_id].engine_enum;
			/* GR Engine */
			if (engine_enum == ENGINE_GR_GK20A) {
				gk20a_gr_nonstall_isr(g);
			}

			/* CE Engine */
			if (((engine_enum == ENGINE_GRCE_GK20A) ||
				(engine_enum == ENGINE_ASYNC_CE_GK20A)) &&
				g->ops.ce2.isr_nonstall) {
					g->ops.ce2.isr_nonstall(g,
					g->fifo.engine_info[active_engine_id].inst_id,
					g->fifo.engine_info[active_engine_id].pri_base);
			}
		}
	}

	/* sync handled irq counter before re-enabling interrupts */
	atomic_set(&g->sw_irq_nonstall_last_handled, hw_irq_count);

	gk20a_writel(g, mc_intr_en_1_r(),
		mc_intr_en_1_inta_hardware_f());

	/* flush previous write */
	gk20a_readl(g, mc_intr_en_1_r());

	wake_up_all(&g->sw_irq_nonstall_last_handled_wq);

	return IRQ_HANDLED;
}

void mc_gk20a_intr_enable(struct gk20a *g)
{
	u32 eng_intr_mask = gk20a_fifo_engine_interrupt_mask(g);

	gk20a_writel(g, mc_intr_mask_1_r(),
		     mc_intr_0_pfifo_pending_f()
		     | eng_intr_mask);
	gk20a_writel(g, mc_intr_en_1_r(),
		mc_intr_en_1_inta_hardware_f());

	gk20a_writel(g, mc_intr_mask_0_r(),
		     mc_intr_0_pfifo_pending_f()
		     | mc_intr_0_priv_ring_pending_f()
		     | mc_intr_0_ltc_pending_f()
		     | mc_intr_0_pbus_pending_f()
		     | eng_intr_mask);
	gk20a_writel(g, mc_intr_en_0_r(),
		mc_intr_en_0_inta_hardware_f());
}

void mc_gk20a_intr_unit_config(struct gk20a *g, bool enable,
		bool is_stalling, u32 mask)
{
	u32 mask_reg = (is_stalling ? mc_intr_mask_0_r() :
					mc_intr_mask_1_r());

	if (enable) {
		gk20a_writel(g, mask_reg,
			gk20a_readl(g, mask_reg) |
			mask);
	} else {
		gk20a_writel(g, mask_reg,
			gk20a_readl(g, mask_reg) &
			~mask);
	}
}

void gk20a_init_mc(struct gpu_ops *gops)
{
	gops->mc.intr_enable = mc_gk20a_intr_enable;
	gops->mc.intr_unit_config = mc_gk20a_intr_unit_config;
	gops->mc.isr_stall = mc_gk20a_isr_stall;
	gops->mc.isr_nonstall = mc_gk20a_isr_nonstall;
	gops->mc.isr_thread_stall = mc_gk20a_intr_thread_stall;
	gops->mc.isr_thread_nonstall = mc_gk20a_intr_thread_nonstall;
}
