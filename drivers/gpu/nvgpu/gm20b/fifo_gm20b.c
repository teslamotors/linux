/*
 * GM20B Fifo
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

#include <linux/delay.h>
#include "gk20a/gk20a.h"
#include "fifo_gm20b.h"
#include "hw_ccsr_gm20b.h"
#include "hw_ram_gm20b.h"
#include "hw_fifo_gm20b.h"
#include "hw_top_gm20b.h"

static void channel_gm20b_bind(struct channel_gk20a *c)
{
	struct gk20a *g = c->g;

	u32 inst_ptr = gk20a_mm_inst_block_addr(g, &c->inst_block)
		>> ram_in_base_shift_v();

	gk20a_dbg_info("bind channel %d inst ptr 0x%08x",
		c->hw_chid, inst_ptr);


	gk20a_writel(g, ccsr_channel_inst_r(c->hw_chid),
		ccsr_channel_inst_ptr_f(inst_ptr) |
		gk20a_aperture_mask(g, &c->inst_block,
		 ccsr_channel_inst_target_sys_mem_ncoh_f(),
		 ccsr_channel_inst_target_vid_mem_f()) |
		ccsr_channel_inst_bind_true_f());

	gk20a_writel(g, ccsr_channel_r(c->hw_chid),
		(gk20a_readl(g, ccsr_channel_r(c->hw_chid)) &
		 ~ccsr_channel_enable_set_f(~0)) |
		 ccsr_channel_enable_set_true_f());
	wmb();
	atomic_set(&c->bound, true);
}

static inline u32 gm20b_engine_id_to_mmu_id(struct gk20a *g, u32 engine_id)
{
	u32 fault_id = ~0;
	struct fifo_engine_info_gk20a *engine_info;

	engine_info = gk20a_fifo_get_engine_info(g, engine_id);

	if (engine_info) {
		fault_id = engine_info->fault_id;
	} else {
		gk20a_err(g->dev, "engine_id is not in active list/invalid %d", engine_id);
	}
	return fault_id;
}

static void gm20b_fifo_trigger_mmu_fault(struct gk20a *g,
		unsigned long engine_ids)
{
	unsigned long end_jiffies = jiffies +
		msecs_to_jiffies(gk20a_get_gr_idle_timeout(g));
	unsigned long delay = GR_IDLE_CHECK_DEFAULT;
	unsigned long engine_id;
	int ret = -EBUSY;

	/* trigger faults for all bad engines */
	for_each_set_bit(engine_id, &engine_ids, 32) {
		u32 engine_mmu_fault_id;

		if (!gk20a_fifo_is_valid_engine_id(g, engine_id)) {
			gk20a_err(dev_from_gk20a(g),
				  "faulting unknown engine %ld", engine_id);
		} else {
			engine_mmu_fault_id = gm20b_engine_id_to_mmu_id(g,
								engine_id);
			gk20a_writel(g, fifo_trigger_mmu_fault_r(engine_id),
				     fifo_trigger_mmu_fault_enable_f(1));
		}
	}

	/* Wait for MMU fault to trigger */
	do {
		if (gk20a_readl(g, fifo_intr_0_r()) &
				fifo_intr_0_mmu_fault_pending_f()) {
			ret = 0;
			break;
		}

		usleep_range(delay, delay * 2);
		delay = min_t(u32, delay << 1, GR_IDLE_CHECK_MAX);
	} while (time_before(jiffies, end_jiffies) ||
			!tegra_platform_is_silicon());

	if (ret)
		gk20a_err(dev_from_gk20a(g), "mmu fault timeout");

	/* release mmu fault trigger */
	for_each_set_bit(engine_id, &engine_ids, 32)
		gk20a_writel(g, fifo_trigger_mmu_fault_r(engine_id), 0);
}

static u32 gm20b_fifo_get_num_fifos(struct gk20a *g)
{
	return ccsr_channel__size_1_v();
}

static void gm20b_device_info_data_parse(struct gk20a *g,
						u32 table_entry, u32 *inst_id,
						u32 *pri_base, u32 *fault_id)
{
	if (top_device_info_data_type_v(table_entry) ==
	    top_device_info_data_type_enum2_v()) {
		if (pri_base) {
			*pri_base =
				(top_device_info_data_pri_base_v(table_entry)
				<< top_device_info_data_pri_base_align_v());
		}
		if (fault_id && (top_device_info_data_fault_id_v(table_entry) ==
			top_device_info_data_fault_id_valid_v())) {
			*fault_id =
			    top_device_info_data_fault_id_enum_v(table_entry);
		}
	} else
		gk20a_err(g->dev, "unknown device_info_data %d",
				top_device_info_data_type_v(table_entry));
}
void gm20b_init_fifo(struct gpu_ops *gops)
{
	gops->fifo.bind_channel = channel_gm20b_bind;
	gops->fifo.unbind_channel = channel_gk20a_unbind;
	gops->fifo.disable_channel = channel_gk20a_disable;
	gops->fifo.enable_channel = channel_gk20a_enable;
	gops->fifo.alloc_inst = channel_gk20a_alloc_inst;
	gops->fifo.free_inst = channel_gk20a_free_inst;
	gops->fifo.setup_ramfc = channel_gk20a_setup_ramfc;
	gops->fifo.channel_set_priority = gk20a_channel_set_priority;
	gops->fifo.channel_set_timeslice = gk20a_channel_set_timeslice;

	gops->fifo.preempt_channel = gk20a_fifo_preempt_channel;
	gops->fifo.preempt_tsg = gk20a_fifo_preempt_tsg;
	gops->fifo.update_runlist = gk20a_fifo_update_runlist;
	gops->fifo.trigger_mmu_fault = gm20b_fifo_trigger_mmu_fault;
	gops->fifo.wait_engine_idle = gk20a_fifo_wait_engine_idle;
	gops->fifo.get_num_fifos = gm20b_fifo_get_num_fifos;
	gops->fifo.get_pbdma_signature = gk20a_fifo_get_pbdma_signature;
	gops->fifo.set_runlist_interleave = gk20a_fifo_set_runlist_interleave;
	gops->fifo.force_reset_ch = gk20a_fifo_force_reset_ch;
	gops->fifo.engine_enum_from_type = gk20a_fifo_engine_enum_from_type;
	gops->fifo.device_info_data_parse = gm20b_device_info_data_parse;
	gops->fifo.eng_runlist_base_size = fifo_eng_runlist_base__size_1_v;
	gops->fifo.init_engine_info = gk20a_fifo_init_engine_info;
}
