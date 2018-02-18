/*
 * Virtualized GPU Fifo
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

#include <linux/dma-mapping.h>
#include <trace/events/gk20a.h>

#include "vgpu/vgpu.h"
#include "gk20a/ctxsw_trace_gk20a.h"
#include "gk20a/hw_fifo_gk20a.h"
#include "gk20a/hw_ram_gk20a.h"

static void vgpu_channel_bind(struct channel_gk20a *ch)
{
	struct gk20a_platform *platform = gk20a_get_platform(ch->g->dev);
	struct tegra_vgpu_cmd_msg msg;
	struct tegra_vgpu_channel_config_params *p =
			&msg.params.channel_config;
	int err;

	gk20a_dbg_info("bind channel %d", ch->hw_chid);

	msg.cmd = TEGRA_VGPU_CMD_CHANNEL_BIND;
	msg.handle = platform->virt_handle;
	p->handle = ch->virt_ctx;
	err = vgpu_comm_sendrecv(&msg, sizeof(msg), sizeof(msg));
	WARN_ON(err || msg.ret);

	wmb();
	atomic_set(&ch->bound, true);
}

static void vgpu_channel_unbind(struct channel_gk20a *ch)
{
	struct gk20a_platform *platform = gk20a_get_platform(ch->g->dev);

	gk20a_dbg_fn("");

	if (atomic_cmpxchg(&ch->bound, true, false)) {
		struct tegra_vgpu_cmd_msg msg;
		struct tegra_vgpu_channel_config_params *p =
				&msg.params.channel_config;
		int err;

		msg.cmd = TEGRA_VGPU_CMD_CHANNEL_UNBIND;
		msg.handle = platform->virt_handle;
		p->handle = ch->virt_ctx;
		err = vgpu_comm_sendrecv(&msg, sizeof(msg), sizeof(msg));
		WARN_ON(err || msg.ret);
	}

}

static int vgpu_channel_alloc_inst(struct gk20a *g, struct channel_gk20a *ch)
{
	struct gk20a_platform *platform = gk20a_get_platform(g->dev);
	struct tegra_vgpu_cmd_msg msg;
	struct tegra_vgpu_channel_hwctx_params *p = &msg.params.channel_hwctx;
	int err;

	gk20a_dbg_fn("");

	msg.cmd = TEGRA_VGPU_CMD_CHANNEL_ALLOC_HWCTX;
	msg.handle = platform->virt_handle;
	p->id = ch->hw_chid;
	p->pid = (u64)current->tgid;
	err = vgpu_comm_sendrecv(&msg, sizeof(msg), sizeof(msg));
	if (err || msg.ret) {
		gk20a_err(dev_from_gk20a(g), "fail");
		return -ENOMEM;
	}

	ch->virt_ctx = p->handle;
	gk20a_dbg_fn("done");
	return 0;
}

static void vgpu_channel_free_inst(struct gk20a *g, struct channel_gk20a *ch)
{
	struct gk20a_platform *platform = gk20a_get_platform(g->dev);
	struct tegra_vgpu_cmd_msg msg;
	struct tegra_vgpu_channel_hwctx_params *p = &msg.params.channel_hwctx;
	int err;

	gk20a_dbg_fn("");

	msg.cmd = TEGRA_VGPU_CMD_CHANNEL_FREE_HWCTX;
	msg.handle = platform->virt_handle;
	p->handle = ch->virt_ctx;
	err = vgpu_comm_sendrecv(&msg, sizeof(msg), sizeof(msg));
	WARN_ON(err || msg.ret);
}

static void vgpu_channel_enable(struct channel_gk20a *ch)
{
	struct gk20a_platform *platform = gk20a_get_platform(ch->g->dev);
	struct tegra_vgpu_cmd_msg msg;
	struct tegra_vgpu_channel_config_params *p =
			&msg.params.channel_config;
	int err;

	gk20a_dbg_fn("");

	msg.cmd = TEGRA_VGPU_CMD_CHANNEL_ENABLE;
	msg.handle = platform->virt_handle;
	p->handle = ch->virt_ctx;
	err = vgpu_comm_sendrecv(&msg, sizeof(msg), sizeof(msg));
	WARN_ON(err || msg.ret);
}

static void vgpu_channel_disable(struct channel_gk20a *ch)
{
	struct gk20a_platform *platform = gk20a_get_platform(ch->g->dev);
	struct tegra_vgpu_cmd_msg msg;
	struct tegra_vgpu_channel_config_params *p =
			&msg.params.channel_config;
	int err;

	gk20a_dbg_fn("");

	msg.cmd = TEGRA_VGPU_CMD_CHANNEL_DISABLE;
	msg.handle = platform->virt_handle;
	p->handle = ch->virt_ctx;
	err = vgpu_comm_sendrecv(&msg, sizeof(msg), sizeof(msg));
	WARN_ON(err || msg.ret);
}

static int vgpu_channel_setup_ramfc(struct channel_gk20a *ch, u64 gpfifo_base,
				u32 gpfifo_entries, u32 flags)
{
	struct gk20a_platform *platform = gk20a_get_platform(ch->g->dev);
	struct device __maybe_unused *d = dev_from_gk20a(ch->g);
	struct dma_iommu_mapping *mapping = to_dma_iommu_mapping(d);
	struct tegra_vgpu_cmd_msg msg;
	struct tegra_vgpu_ramfc_params *p = &msg.params.ramfc;
	int err;

	gk20a_dbg_fn("");

	msg.cmd = TEGRA_VGPU_CMD_CHANNEL_SETUP_RAMFC;
	msg.handle = platform->virt_handle;
	p->handle = ch->virt_ctx;
	p->gpfifo_va = gpfifo_base;
	p->num_entries = gpfifo_entries;
	p->userd_addr = ch->userd_iova;
	p->iova = mapping ? 1 : 0;
	err = vgpu_comm_sendrecv(&msg, sizeof(msg), sizeof(msg));

	return (err || msg.ret) ? -ENOMEM : 0;
}

static int vgpu_fifo_init_engine_info(struct fifo_gk20a *f)
{
	struct fifo_engine_info_gk20a *gr_info;
	struct fifo_engine_info_gk20a *ce_info;
	const u32 gr_sw_id = ENGINE_GR_GK20A;
	const u32 ce_sw_id = ENGINE_GRCE_GK20A;

	gk20a_dbg_fn("");

	f->num_engines = 2;

	gr_info = &f->engine_info[0];

	/* FIXME: retrieve this from server */
	gr_info->runlist_id = 0;
	gr_info->engine_enum = gr_sw_id;
	f->active_engines_list[0] = 0;

	ce_info = &f->engine_info[1];
	ce_info->runlist_id = 0;
	ce_info->inst_id = 2;
	ce_info->engine_enum = ce_sw_id;
	f->active_engines_list[1] = 1;

	return 0;
}

static int init_runlist(struct gk20a *g, struct fifo_gk20a *f)
{
	struct fifo_runlist_info_gk20a *runlist;
	struct device *d = dev_from_gk20a(g);
	s32 runlist_id = -1;
	u32 i;
	u64 runlist_size;

	gk20a_dbg_fn("");

	f->max_runlists = g->ops.fifo.eng_runlist_base_size();
	f->runlist_info = kzalloc(sizeof(struct fifo_runlist_info_gk20a) *
				  f->max_runlists, GFP_KERNEL);
	if (!f->runlist_info)
		goto clean_up_runlist;

	memset(f->runlist_info, 0, (sizeof(struct fifo_runlist_info_gk20a) *
		f->max_runlists));

	for (runlist_id = 0; runlist_id < f->max_runlists; runlist_id++) {
		runlist = &f->runlist_info[runlist_id];

		runlist->active_channels =
			kzalloc(DIV_ROUND_UP(f->num_channels, BITS_PER_BYTE),
				GFP_KERNEL);
		if (!runlist->active_channels)
			goto clean_up_runlist;

			runlist_size  = sizeof(u16) * f->num_channels;
			for (i = 0; i < MAX_RUNLIST_BUFFERS; i++) {
				int err = gk20a_gmmu_alloc_sys(g, runlist_size,
						&runlist->mem[i]);
				if (err) {
					dev_err(d, "memory allocation failed\n");
					goto clean_up_runlist;
				}
			}
		mutex_init(&runlist->mutex);

		/* None of buffers is pinned if this value doesn't change.
		    Otherwise, one of them (cur_buffer) must have been pinned. */
		runlist->cur_buffer = MAX_RUNLIST_BUFFERS;
	}

	gk20a_dbg_fn("done");
	return 0;

clean_up_runlist:
	gk20a_fifo_delete_runlist(f);
	gk20a_dbg_fn("fail");
	return -ENOMEM;
}

static int vgpu_init_fifo_setup_sw(struct gk20a *g)
{
	struct gk20a_platform *platform = gk20a_get_platform(g->dev);
	struct fifo_gk20a *f = &g->fifo;
	struct device *d = dev_from_gk20a(g);
	int chid, err = 0;

	gk20a_dbg_fn("");

	if (f->sw_ready) {
		gk20a_dbg_fn("skip init");
		return 0;
	}

	f->g = g;

	err = vgpu_get_attribute(platform->virt_handle,
				TEGRA_VGPU_ATTRIB_NUM_CHANNELS,
				&f->num_channels);
	if (err)
		return -ENXIO;

	f->max_engines = nvgpu_get_litter_value(g, GPU_LIT_HOST_NUM_ENGINES);

	f->userd_entry_size = 1 << ram_userd_base_shift_v();

	err = gk20a_gmmu_alloc_sys(g, f->userd_entry_size * f->num_channels,
			&f->userd);
	if (err) {
		dev_err(d, "memory allocation failed\n");
		goto clean_up;
	}

	/* bar1 va */
	f->userd.gpu_va = vgpu_bar1_map(g, &f->userd.sgt, f->userd.size);
	if (!f->userd.gpu_va) {
		dev_err(d, "gmmu mapping failed\n");
		goto clean_up;
	}

       /* if reduced BAR1 range is specified, use offset of 0
          (server returns offset assuming full BAR1 range) */
       if (resource_size(g->bar1_mem) == (resource_size_t)f->userd.size)
		f->userd.gpu_va = 0;

	gk20a_dbg(gpu_dbg_map_v, "userd bar1 va = 0x%llx", f->userd.gpu_va);

	f->channel = vzalloc(f->num_channels * sizeof(*f->channel));
	f->tsg = vzalloc(f->num_channels * sizeof(*f->tsg));
	f->engine_info = kzalloc(f->max_engines * sizeof(*f->engine_info),
				GFP_KERNEL);
	f->active_engines_list = kzalloc(f->max_engines * sizeof(u32),
				GFP_KERNEL);

	if (!(f->channel && f->tsg && f->engine_info && f->active_engines_list)) {
		err = -ENOMEM;
		goto clean_up;
	}
	memset(f->active_engines_list, 0xff, (f->max_engines * sizeof(u32)));

	g->ops.fifo.init_engine_info(f);

	init_runlist(g, f);

	INIT_LIST_HEAD(&f->free_chs);
	mutex_init(&f->free_chs_mutex);

	for (chid = 0; chid < f->num_channels; chid++) {
		f->channel[chid].userd_iova =
			g->ops.mm.get_iova_addr(g, f->userd.sgt->sgl, 0)
				+ chid * f->userd_entry_size;
		f->channel[chid].userd_gpu_va =
			f->userd.gpu_va + chid * f->userd_entry_size;

		gk20a_init_channel_support(g, chid);
		gk20a_init_tsg_support(g, chid);
	}
	mutex_init(&f->tsg_inuse_mutex);

	f->deferred_reset_pending = false;
	mutex_init(&f->deferred_reset_mutex);

	f->sw_ready = true;

	gk20a_dbg_fn("done");
	return 0;

clean_up:
	gk20a_dbg_fn("fail");
	/* FIXME: unmap from bar1 */
	gk20a_gmmu_free(g, &f->userd);

	memset(&f->userd, 0, sizeof(f->userd));

	vfree(f->channel);
	f->channel = NULL;
	vfree(f->tsg);
	f->tsg = NULL;
	kfree(f->engine_info);
	f->engine_info = NULL;
	kfree(f->active_engines_list);
	f->active_engines_list = NULL;

	return err;
}

static int vgpu_init_fifo_setup_hw(struct gk20a *g)
{
	gk20a_dbg_fn("");

	/* test write, read through bar1 @ userd region before
	 * turning on the snooping */
	{
		struct fifo_gk20a *f = &g->fifo;
		u32 v, v1 = 0x33, v2 = 0x55;

		u32 bar1_vaddr = f->userd.gpu_va;
		volatile u32 *cpu_vaddr = f->userd.cpu_va;

		gk20a_dbg_info("test bar1 @ vaddr 0x%x",
			   bar1_vaddr);

		v = gk20a_bar1_readl(g, bar1_vaddr);

		*cpu_vaddr = v1;
		smp_mb();

		if (v1 != gk20a_bar1_readl(g, bar1_vaddr)) {
			gk20a_err(dev_from_gk20a(g), "bar1 broken @ gk20a!");
			return -EINVAL;
		}

		gk20a_bar1_writel(g, bar1_vaddr, v2);

		if (v2 != gk20a_bar1_readl(g, bar1_vaddr)) {
			gk20a_err(dev_from_gk20a(g), "bar1 broken @ gk20a!");
			return -EINVAL;
		}

		/* is it visible to the cpu? */
		if (*cpu_vaddr != v2) {
			gk20a_err(dev_from_gk20a(g),
				"cpu didn't see bar1 write @ %p!",
				cpu_vaddr);
		}

		/* put it back */
		gk20a_bar1_writel(g, bar1_vaddr, v);
	}

	gk20a_dbg_fn("done");

	return 0;
}

int vgpu_init_fifo_support(struct gk20a *g)
{
	u32 err;

	gk20a_dbg_fn("");

	err = vgpu_init_fifo_setup_sw(g);
	if (err)
		return err;

	err = vgpu_init_fifo_setup_hw(g);
	return err;
}

static int vgpu_fifo_preempt_channel(struct gk20a *g, u32 hw_chid)
{
	struct gk20a_platform *platform = gk20a_get_platform(g->dev);
	struct fifo_gk20a *f = &g->fifo;
	struct tegra_vgpu_cmd_msg msg;
	struct tegra_vgpu_channel_config_params *p =
			&msg.params.channel_config;
	int err;

	gk20a_dbg_fn("");

	msg.cmd = TEGRA_VGPU_CMD_CHANNEL_PREEMPT;
	msg.handle = platform->virt_handle;
	p->handle = f->channel[hw_chid].virt_ctx;
	err = vgpu_comm_sendrecv(&msg, sizeof(msg), sizeof(msg));

	if (err || msg.ret) {
		gk20a_err(dev_from_gk20a(g),
			"preempt channel %d failed\n", hw_chid);
		err = -ENOMEM;
	}

	return err;
}

static int vgpu_fifo_preempt_tsg(struct gk20a *g, u32 tsgid)
{
	struct gk20a_platform *platform = gk20a_get_platform(g->dev);
	struct tegra_vgpu_cmd_msg msg;
	struct tegra_vgpu_tsg_preempt_params *p =
			&msg.params.tsg_preempt;
	int err;

	gk20a_dbg_fn("");

	msg.cmd = TEGRA_VGPU_CMD_TSG_PREEMPT;
	msg.handle = platform->virt_handle;
	p->tsg_id = tsgid;
	err = vgpu_comm_sendrecv(&msg, sizeof(msg), sizeof(msg));
	err = err ? err : msg.ret;

	if (err) {
		gk20a_err(dev_from_gk20a(g),
			"preempt tsg %u failed\n", tsgid);
	}

	return err;
}

static int vgpu_submit_runlist(u64 handle, u8 runlist_id, u16 *runlist,
			u32 num_entries)
{
	struct tegra_vgpu_cmd_msg *msg;
	struct tegra_vgpu_runlist_params *p;
	size_t size = sizeof(*msg) + sizeof(*runlist) * num_entries;
	char *ptr;
	int err;

	msg = kmalloc(size, GFP_KERNEL);
	if (!msg)
		return -1;

	msg->cmd = TEGRA_VGPU_CMD_SUBMIT_RUNLIST;
	msg->handle = handle;
	p = &msg->params.runlist;
	p->runlist_id = runlist_id;
	p->num_entries = num_entries;

	ptr = (char *)msg + sizeof(*msg);
	memcpy(ptr, runlist, sizeof(*runlist) * num_entries);
	err = vgpu_comm_sendrecv(msg, size, sizeof(*msg));

	err = (err || msg->ret) ? -1 : 0;
	kfree(msg);
	return err;
}

static int vgpu_fifo_update_runlist_locked(struct gk20a *g, u32 runlist_id,
					u32 hw_chid, bool add,
					bool wait_for_finish)
{
	struct gk20a_platform *platform = gk20a_get_platform(g->dev);
	struct fifo_gk20a *f = &g->fifo;
	struct fifo_runlist_info_gk20a *runlist;
	u16 *runlist_entry = NULL;
	u32 count = 0;

	gk20a_dbg_fn("");

	runlist = &f->runlist_info[runlist_id];

	/* valid channel, add/remove it from active list.
	   Otherwise, keep active list untouched for suspend/resume. */
	if (hw_chid != ~0) {
		if (add) {
			if (test_and_set_bit(hw_chid,
				runlist->active_channels) == 1)
				return 0;
		} else {
			if (test_and_clear_bit(hw_chid,
				runlist->active_channels) == 0)
				return 0;
		}
	}

	if (hw_chid != ~0 || /* add/remove a valid channel */
	    add /* resume to add all channels back */) {
		u32 chid;

		runlist_entry = runlist->mem[0].cpu_va;
		for_each_set_bit(chid,
			runlist->active_channels, f->num_channels) {
			gk20a_dbg_info("add channel %d to runlist", chid);
			runlist_entry[0] = chid;
			runlist_entry++;
			count++;
		}
	} else	/* suspend to remove all channels */
		count = 0;

	return vgpu_submit_runlist(platform->virt_handle, runlist_id,
				runlist->mem[0].cpu_va, count);
}

/* add/remove a channel from runlist
   special cases below: runlist->active_channels will NOT be changed.
   (hw_chid == ~0 && !add) means remove all active channels from runlist.
   (hw_chid == ~0 &&  add) means restore all active channels on runlist. */
static int vgpu_fifo_update_runlist(struct gk20a *g, u32 runlist_id,
				u32 hw_chid, bool add, bool wait_for_finish)
{
	struct fifo_runlist_info_gk20a *runlist = NULL;
	struct fifo_gk20a *f = &g->fifo;
	u32 ret = 0;

	gk20a_dbg_fn("");

	runlist = &f->runlist_info[runlist_id];

	mutex_lock(&runlist->mutex);

	ret = vgpu_fifo_update_runlist_locked(g, runlist_id, hw_chid, add,
					wait_for_finish);

	mutex_unlock(&runlist->mutex);
	return ret;
}

static int vgpu_fifo_wait_engine_idle(struct gk20a *g)
{
	gk20a_dbg_fn("");

	return 0;
}

static int vgpu_channel_set_priority(struct channel_gk20a *ch, u32 priority)
{
	struct gk20a_platform *platform = gk20a_get_platform(ch->g->dev);
	struct tegra_vgpu_cmd_msg msg;
	struct tegra_vgpu_channel_priority_params *p =
			&msg.params.channel_priority;
	int err;

	gk20a_dbg_info("channel %d set priority %u", ch->hw_chid, priority);

	msg.cmd = TEGRA_VGPU_CMD_CHANNEL_SET_PRIORITY;
	msg.handle = platform->virt_handle;
	p->handle = ch->virt_ctx;
	p->priority = priority;
	err = vgpu_comm_sendrecv(&msg, sizeof(msg), sizeof(msg));
	WARN_ON(err || msg.ret);

	return err ? err : msg.ret;
}

static int vgpu_fifo_tsg_set_runlist_interleave(struct gk20a *g,
					u32 tsgid,
					u32 runlist_id,
					u32 new_level)
{
	struct gk20a_platform *platform = gk20a_get_platform(g->dev);
	struct tegra_vgpu_cmd_msg msg = {0};
	struct tegra_vgpu_tsg_runlist_interleave_params *p =
			&msg.params.tsg_interleave;
	int err;

	gk20a_dbg_fn("");

	msg.cmd = TEGRA_VGPU_CMD_TSG_SET_RUNLIST_INTERLEAVE;
	msg.handle = platform->virt_handle;
	p->tsg_id = tsgid;
	p->level = new_level;
	err = vgpu_comm_sendrecv(&msg, sizeof(msg), sizeof(msg));
	WARN_ON(err || msg.ret);
	return err ? err : msg.ret;
}

static int vgpu_fifo_set_runlist_interleave(struct gk20a *g,
					u32 id,
					bool is_tsg,
					u32 runlist_id,
					u32 new_level)
{
	struct gk20a_platform *platform = gk20a_get_platform(g->dev);
	struct tegra_vgpu_cmd_msg msg;
	struct tegra_vgpu_channel_runlist_interleave_params *p =
			&msg.params.channel_interleave;
	struct channel_gk20a *ch;
	int err;

	gk20a_dbg_fn("");

	if (is_tsg)
		return vgpu_fifo_tsg_set_runlist_interleave(g, id,
						runlist_id, new_level);

	ch = &g->fifo.channel[id];
	msg.cmd = TEGRA_VGPU_CMD_CHANNEL_SET_RUNLIST_INTERLEAVE;
	msg.handle = platform->virt_handle;
	p->handle = ch->virt_ctx;
	p->level = new_level;
	err = vgpu_comm_sendrecv(&msg, sizeof(msg), sizeof(msg));
	WARN_ON(err || msg.ret);
	return err ? err : msg.ret;
}

static int vgpu_channel_set_timeslice(struct channel_gk20a *ch, u32 timeslice)
{
	struct gk20a_platform *platform = gk20a_get_platform(ch->g->dev);
	struct tegra_vgpu_cmd_msg msg;
	struct tegra_vgpu_channel_timeslice_params *p =
			&msg.params.channel_timeslice;
	int err;

	gk20a_dbg_fn("");

	msg.cmd = TEGRA_VGPU_CMD_CHANNEL_SET_TIMESLICE;
	msg.handle = platform->virt_handle;
	p->handle = ch->virt_ctx;
	p->timeslice_us = timeslice;
	err = vgpu_comm_sendrecv(&msg, sizeof(msg), sizeof(msg));
	WARN_ON(err || msg.ret);
	return err ? err : msg.ret;
}

static int vgpu_fifo_force_reset_ch(struct channel_gk20a *ch, bool verbose)
{
	struct tsg_gk20a *tsg = NULL;
	struct channel_gk20a *ch_tsg = NULL;
	struct gk20a *g = ch->g;
	struct gk20a_platform *platform = gk20a_get_platform(ch->g->dev);
	struct tegra_vgpu_cmd_msg msg = {0};
	struct tegra_vgpu_channel_config_params *p =
			&msg.params.channel_config;
	int err;

	gk20a_dbg_fn("");

	if (gk20a_is_channel_marked_as_tsg(ch)) {
		tsg = &g->fifo.tsg[ch->tsgid];

		mutex_lock(&tsg->ch_list_lock);

		list_for_each_entry(ch_tsg, &tsg->ch_list, ch_entry) {
			if (gk20a_channel_get(ch_tsg)) {
				gk20a_set_error_notifier(ch_tsg,
				       NVGPU_CHANNEL_RESETCHANNEL_VERIF_ERROR);
				gk20a_channel_put(ch_tsg);
			}
		}

		mutex_unlock(&tsg->ch_list_lock);
	} else {
		gk20a_set_error_notifier(ch,
			NVGPU_CHANNEL_RESETCHANNEL_VERIF_ERROR);
	}

	msg.cmd = TEGRA_VGPU_CMD_CHANNEL_FORCE_RESET;
	msg.handle = platform->virt_handle;
	p->handle = ch->virt_ctx;
	err = vgpu_comm_sendrecv(&msg, sizeof(msg), sizeof(msg));
	WARN_ON(err || msg.ret);
	return err ? err : msg.ret;
}

static void vgpu_fifo_set_ctx_mmu_error(struct gk20a *g,
		struct channel_gk20a *ch)
{
	if (ch->error_notifier) {
		if (ch->error_notifier->status == 0xffff) {
			/* If error code is already set, this mmu fault
			 * was triggered as part of recovery from other
			 * error condition.
			 * Don't overwrite error flag. */
		} else {
			gk20a_set_error_notifier(ch,
				NVGPU_CHANNEL_FIFO_ERROR_MMU_ERR_FLT);
		}
	}
	/* mark channel as faulted */
	ch->has_timedout = true;
	wmb();
	/* unblock pending waits */
	wake_up(&ch->semaphore_wq);
	wake_up(&ch->notifier_wq);
}

int vgpu_fifo_isr(struct gk20a *g, struct tegra_vgpu_fifo_intr_info *info)
{
	struct fifo_gk20a *f = &g->fifo;
	struct channel_gk20a *ch = gk20a_channel_get(&f->channel[info->chid]);

	gk20a_dbg_fn("");
	if (!ch)
		return 0;

	gk20a_err(dev_from_gk20a(g), "fifo intr (%d) on ch %u",
		info->type, info->chid);

	trace_gk20a_channel_reset(ch->hw_chid, ch->tsgid);

	switch (info->type) {
	case TEGRA_VGPU_FIFO_INTR_PBDMA:
		gk20a_set_error_notifier(ch, NVGPU_CHANNEL_PBDMA_ERROR);
		break;
	case TEGRA_VGPU_FIFO_INTR_CTXSW_TIMEOUT:
		gk20a_set_error_notifier(ch,
					NVGPU_CHANNEL_FIFO_ERROR_IDLE_TIMEOUT);
		break;
	case TEGRA_VGPU_FIFO_INTR_MMU_FAULT:
		vgpu_fifo_set_ctx_mmu_error(g, ch);
		gk20a_channel_abort(ch, false);
		break;
	default:
		WARN_ON(1);
		break;
	}

	gk20a_channel_put(ch);
	return 0;
}

int vgpu_fifo_nonstall_isr(struct gk20a *g,
			struct tegra_vgpu_fifo_nonstall_intr_info *info)
{
	gk20a_dbg_fn("");

	switch (info->type) {
	case TEGRA_VGPU_FIFO_NONSTALL_INTR_CHANNEL:
		gk20a_channel_semaphore_wakeup(g, false);
		break;
	default:
		WARN_ON(1);
		break;
	}

	return 0;
}

void vgpu_init_fifo_ops(struct gpu_ops *gops)
{
	gops->fifo.bind_channel = vgpu_channel_bind;
	gops->fifo.unbind_channel = vgpu_channel_unbind;
	gops->fifo.enable_channel = vgpu_channel_enable;
	gops->fifo.disable_channel = vgpu_channel_disable;
	gops->fifo.alloc_inst = vgpu_channel_alloc_inst;
	gops->fifo.free_inst = vgpu_channel_free_inst;
	gops->fifo.setup_ramfc = vgpu_channel_setup_ramfc;
	gops->fifo.preempt_channel = vgpu_fifo_preempt_channel;
	gops->fifo.preempt_tsg = vgpu_fifo_preempt_tsg;
	gops->fifo.update_runlist = vgpu_fifo_update_runlist;
	gops->fifo.wait_engine_idle = vgpu_fifo_wait_engine_idle;
	gops->fifo.channel_set_priority = vgpu_channel_set_priority;
	gops->fifo.set_runlist_interleave = vgpu_fifo_set_runlist_interleave;
	gops->fifo.channel_set_timeslice = vgpu_channel_set_timeslice;
	gops->fifo.force_reset_ch = vgpu_fifo_force_reset_ch;
	gops->fifo.init_engine_info = vgpu_fifo_init_engine_info;
}
