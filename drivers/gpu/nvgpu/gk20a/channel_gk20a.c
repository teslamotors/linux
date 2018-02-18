/*
 * GK20A Graphics channel
 *
 * Copyright (c) 2011-2017, NVIDIA CORPORATION.  All rights reserved.
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
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/nvhost.h>
#include <linux/list.h>
#include <linux/delay.h>
#include <linux/highmem.h> /* need for nvmap.h*/
#include <trace/events/gk20a.h>
#include <linux/scatterlist.h>
#include <linux/file.h>
#include <linux/anon_inodes.h>
#include <linux/dma-buf.h>
#include <linux/vmalloc.h>
#include <linux/circ_buf.h>

#include "debug_gk20a.h"
#include "ctxsw_trace_gk20a.h"

#include "gk20a.h"
#include "dbg_gpu_gk20a.h"
#include "fence_gk20a.h"
#include "semaphore_gk20a.h"

#include "hw_ram_gk20a.h"
#include "hw_fifo_gk20a.h"
#include "hw_pbdma_gk20a.h"
#include "hw_ccsr_gk20a.h"
#include "hw_ltc_gk20a.h"

#define NVMAP_HANDLE_PARAM_SIZE 1

#define NVGPU_CHANNEL_MIN_TIMESLICE_US 1000
#define NVGPU_CHANNEL_MAX_TIMESLICE_US 50000

/*
 * Although channels do have pointers back to the gk20a struct that they were
 * created under in cases where the driver is killed that pointer can be bad.
 * The channel memory can be freed before the release() function for a given
 * channel is called. This happens when the driver dies and userspace doesn't
 * get a chance to call release() until after the entire gk20a driver data is
 * unloaded and freed.
 */
struct channel_priv {
	struct gk20a *g;
	struct channel_gk20a *c;
};

static struct channel_gk20a *allocate_channel(struct fifo_gk20a *f);
static void free_channel(struct fifo_gk20a *f, struct channel_gk20a *c);

static void free_priv_cmdbuf(struct channel_gk20a *c,
			     struct priv_cmd_entry *e);

static int channel_gk20a_alloc_priv_cmdbuf(struct channel_gk20a *c);
static void channel_gk20a_free_priv_cmdbuf(struct channel_gk20a *c);

static void channel_gk20a_free_prealloc_resources(struct channel_gk20a *c);

static void channel_gk20a_joblist_add(struct channel_gk20a *c,
		struct channel_gk20a_job *job);
static void channel_gk20a_joblist_delete(struct channel_gk20a *c,
		struct channel_gk20a_job *job);
static struct channel_gk20a_job *channel_gk20a_joblist_peek(
		struct channel_gk20a *c);

static int channel_gk20a_commit_userd(struct channel_gk20a *c);
static int channel_gk20a_setup_userd(struct channel_gk20a *c);

static void channel_gk20a_bind(struct channel_gk20a *ch_gk20a);

static int channel_gk20a_update_runlist(struct channel_gk20a *c,
					bool add);
static void gk20a_free_error_notifiers(struct channel_gk20a *ch);

static u32 gk20a_get_channel_watchdog_timeout(struct channel_gk20a *ch);

static void gk20a_channel_clean_up_jobs(struct channel_gk20a *c,
					bool clean_all);
static void gk20a_channel_cancel_job_clean_up(struct channel_gk20a *c,
				bool wait_for_completion);

/* allocate GPU channel */
static struct channel_gk20a *allocate_channel(struct fifo_gk20a *f)
{
	struct channel_gk20a *ch = NULL;
	struct gk20a_platform *platform;

	platform = gk20a_get_platform(f->g->dev);

	mutex_lock(&f->free_chs_mutex);
	if (!list_empty(&f->free_chs)) {
		ch = list_first_entry(&f->free_chs, struct channel_gk20a,
				free_chs);
		list_del(&ch->free_chs);
		WARN_ON(atomic_read(&ch->ref_count));
		WARN_ON(ch->referenceable);
		f->used_channels++;
	}
	mutex_unlock(&f->free_chs_mutex);

	if (platform->aggressive_sync_destroy_thresh &&
			(f->used_channels >
			 platform->aggressive_sync_destroy_thresh))
		platform->aggressive_sync_destroy = true;

	return ch;
}

static void free_channel(struct fifo_gk20a *f,
		struct channel_gk20a *ch)
{
	struct gk20a_platform *platform = gk20a_get_platform(f->g->dev);

	trace_gk20a_release_used_channel(ch->hw_chid);
	/* refcount is zero here and channel is in a freed/dead state */
	mutex_lock(&f->free_chs_mutex);
	/* add to head to increase visibility of timing-related bugs */
	list_add(&ch->free_chs, &f->free_chs);
	f->used_channels--;
	mutex_unlock(&f->free_chs_mutex);

	if (platform->aggressive_sync_destroy_thresh &&
			(f->used_channels <
			 platform->aggressive_sync_destroy_thresh))
		platform->aggressive_sync_destroy = false;
}

int channel_gk20a_commit_va(struct channel_gk20a *c)
{
	gk20a_dbg_fn("");

	gk20a_init_inst_block(&c->inst_block, c->vm,
			c->vm->gmmu_page_sizes[gmmu_page_size_big]);

	return 0;
}

static int channel_gk20a_commit_userd(struct channel_gk20a *c)
{
	u32 addr_lo;
	u32 addr_hi;
	struct gk20a *g = c->g;

	gk20a_dbg_fn("");

	addr_lo = u64_lo32(c->userd_iova >> ram_userd_base_shift_v());
	addr_hi = u64_hi32(c->userd_iova);

	gk20a_dbg_info("channel %d : set ramfc userd 0x%16llx",
		c->hw_chid, (u64)c->userd_iova);

	gk20a_mem_wr32(g, &c->inst_block,
		       ram_in_ramfc_w() + ram_fc_userd_w(),
		       gk20a_aperture_mask(g, &g->fifo.userd,
			pbdma_userd_target_sys_mem_ncoh_f(),
			pbdma_userd_target_vid_mem_f()) |
		       pbdma_userd_addr_f(addr_lo));

	gk20a_mem_wr32(g, &c->inst_block,
		       ram_in_ramfc_w() + ram_fc_userd_hi_w(),
		       pbdma_userd_hi_addr_f(addr_hi));

	return 0;
}

int gk20a_channel_get_timescale_from_timeslice(struct gk20a *g,
		int timeslice_period,
		int *__timeslice_timeout, int *__timeslice_scale)
{
	struct gk20a_platform *platform = dev_get_drvdata(g->dev);
	int value = scale_ptimer(timeslice_period,
			ptimer_scalingfactor10x(platform->ptimer_src_freq));
	int shift = 0;

	/* value field is 8 bits long */
	while (value >= 1 << 8) {
		value >>= 1;
		shift++;
	}

	/* time slice register is only 18bits long */
	if ((value << shift) >= 1<<19) {
		pr_err("Requested timeslice value is clamped to 18 bits\n");
		value = 255;
		shift = 10;
	}

	*__timeslice_timeout = value;
	*__timeslice_scale = shift;

	return 0;
}

static int channel_gk20a_set_schedule_params(struct channel_gk20a *c)
{
	int shift = 0, value = 0;

	gk20a_channel_get_timescale_from_timeslice(c->g,
		c->timeslice_us, &value, &shift);

	/* disable channel */
	c->g->ops.fifo.disable_channel(c);

	/* preempt the channel */
	WARN_ON(c->g->ops.fifo.preempt_channel(c->g, c->hw_chid));

	/* set new timeslice */
	gk20a_mem_wr32(c->g, &c->inst_block, ram_fc_runlist_timeslice_w(),
		value | (shift << 12) |
		fifo_runlist_timeslice_enable_true_f());

	/* enable channel */
	gk20a_writel(c->g, ccsr_channel_r(c->hw_chid),
		gk20a_readl(c->g, ccsr_channel_r(c->hw_chid)) |
		ccsr_channel_enable_set_true_f());

	return 0;
}

u32 channel_gk20a_pbdma_acquire_val(struct channel_gk20a *c)
{
	u32 val, exp, man;
	u64 timeout;
	int val_len;

	val = pbdma_acquire_retry_man_2_f() |
		pbdma_acquire_retry_exp_2_f();

	if (!c->g->timeouts_enabled || !c->wdt_enabled)
		return val;

	timeout = gk20a_get_channel_watchdog_timeout(c);
	do_div(timeout, 2); /* set acquire timeout to half of channel wdt */
	timeout *= 1000000UL; /* ms -> ns */
	do_div(timeout, 1024); /* in unit of 1024ns */
	val_len = fls(timeout >> 32) + 32;
	if (val_len == 32)
		val_len = fls(timeout);
	if (val_len > 16 + pbdma_acquire_timeout_exp_max_v()) { /* man: 16bits */
		exp = pbdma_acquire_timeout_exp_max_v();
		man = pbdma_acquire_timeout_man_max_v();
	} else if (val_len > 16) {
		exp = val_len - 16;
		man = timeout >> exp;
	} else {
		exp = 0;
		man = timeout;
	}

	val |= pbdma_acquire_timeout_exp_f(exp) |
		pbdma_acquire_timeout_man_f(man) |
		pbdma_acquire_timeout_en_enable_f();

	return val;
}

void gk20a_channel_setup_ramfc_for_privileged_channel(struct channel_gk20a *c)
{
	struct gk20a *g = c->g;
	struct mem_desc *mem = &c->inst_block;

	gk20a_dbg_info("channel %d : set ramfc privileged_channel", c->hw_chid);

	/* Enable HCE priv mode for phys mode transfer */
	gk20a_mem_wr32(g, mem, ram_fc_hce_ctrl_w(),
		pbdma_hce_ctrl_hce_priv_mode_yes_f());
}

int channel_gk20a_setup_ramfc(struct channel_gk20a *c,
			u64 gpfifo_base, u32 gpfifo_entries, u32 flags)
{
	struct gk20a *g = c->g;
	struct mem_desc *mem = &c->inst_block;

	gk20a_dbg_fn("");

	gk20a_memset(g, mem, 0, 0, ram_fc_size_val_v());

	gk20a_mem_wr32(g, mem, ram_fc_gp_base_w(),
		pbdma_gp_base_offset_f(
		u64_lo32(gpfifo_base >> pbdma_gp_base_rsvd_s())));

	gk20a_mem_wr32(g, mem, ram_fc_gp_base_hi_w(),
		pbdma_gp_base_hi_offset_f(u64_hi32(gpfifo_base)) |
		pbdma_gp_base_hi_limit2_f(ilog2(gpfifo_entries)));

	gk20a_mem_wr32(g, mem, ram_fc_signature_w(),
		 c->g->ops.fifo.get_pbdma_signature(c->g));

	gk20a_mem_wr32(g, mem, ram_fc_formats_w(),
		pbdma_formats_gp_fermi0_f() |
		pbdma_formats_pb_fermi1_f() |
		pbdma_formats_mp_fermi0_f());

	gk20a_mem_wr32(g, mem, ram_fc_pb_header_w(),
		pbdma_pb_header_priv_user_f() |
		pbdma_pb_header_method_zero_f() |
		pbdma_pb_header_subchannel_zero_f() |
		pbdma_pb_header_level_main_f() |
		pbdma_pb_header_first_true_f() |
		pbdma_pb_header_type_inc_f());

	gk20a_mem_wr32(g, mem, ram_fc_subdevice_w(),
		pbdma_subdevice_id_f(1) |
		pbdma_subdevice_status_active_f() |
		pbdma_subdevice_channel_dma_enable_f());

	gk20a_mem_wr32(g, mem, ram_fc_target_w(), pbdma_target_engine_sw_f());

	gk20a_mem_wr32(g, mem, ram_fc_acquire_w(),
		channel_gk20a_pbdma_acquire_val(c));

	gk20a_mem_wr32(g, mem, ram_fc_runlist_timeslice_w(),
		fifo_runlist_timeslice_timeout_128_f() |
		fifo_runlist_timeslice_timescale_3_f() |
		fifo_runlist_timeslice_enable_true_f());

	gk20a_mem_wr32(g, mem, ram_fc_pb_timeslice_w(),
		fifo_pb_timeslice_timeout_16_f() |
		fifo_pb_timeslice_timescale_0_f() |
		fifo_pb_timeslice_enable_true_f());

	gk20a_mem_wr32(g, mem, ram_fc_chid_w(), ram_fc_chid_id_f(c->hw_chid));

	if (c->is_privileged_channel)
		gk20a_channel_setup_ramfc_for_privileged_channel(c);

	return channel_gk20a_commit_userd(c);
}

static int channel_gk20a_setup_userd(struct channel_gk20a *c)
{
	struct gk20a *g = c->g;
	struct mem_desc *mem = &g->fifo.userd;
	u32 offset = c->hw_chid * g->fifo.userd_entry_size / sizeof(u32);

	gk20a_dbg_fn("");

	gk20a_mem_wr32(g, mem, offset + ram_userd_put_w(), 0);
	gk20a_mem_wr32(g, mem, offset + ram_userd_get_w(), 0);
	gk20a_mem_wr32(g, mem, offset + ram_userd_ref_w(), 0);
	gk20a_mem_wr32(g, mem, offset + ram_userd_put_hi_w(), 0);
	gk20a_mem_wr32(g, mem, offset + ram_userd_ref_threshold_w(), 0);
	gk20a_mem_wr32(g, mem, offset + ram_userd_gp_top_level_get_w(), 0);
	gk20a_mem_wr32(g, mem, offset + ram_userd_gp_top_level_get_hi_w(), 0);
	gk20a_mem_wr32(g, mem, offset + ram_userd_get_hi_w(), 0);
	gk20a_mem_wr32(g, mem, offset + ram_userd_gp_get_w(), 0);
	gk20a_mem_wr32(g, mem, offset + ram_userd_gp_put_w(), 0);

	return 0;
}

static void channel_gk20a_bind(struct channel_gk20a *c)
{
	struct gk20a *g = c->g;
	u32 inst_ptr = gk20a_mm_inst_block_addr(g, &c->inst_block)
		>> ram_in_base_shift_v();

	gk20a_dbg_info("bind channel %d inst ptr 0x%08x",
		c->hw_chid, inst_ptr);


	gk20a_writel(g, ccsr_channel_r(c->hw_chid),
		(gk20a_readl(g, ccsr_channel_r(c->hw_chid)) &
		 ~ccsr_channel_runlist_f(~0)) |
		 ccsr_channel_runlist_f(c->runlist_id));

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

void channel_gk20a_unbind(struct channel_gk20a *ch_gk20a)
{
	struct gk20a *g = ch_gk20a->g;

	gk20a_dbg_fn("");

	if (atomic_cmpxchg(&ch_gk20a->bound, true, false)) {
		gk20a_writel(g, ccsr_channel_inst_r(ch_gk20a->hw_chid),
			ccsr_channel_inst_ptr_f(0) |
			ccsr_channel_inst_bind_false_f());
	}
}

int channel_gk20a_alloc_inst(struct gk20a *g, struct channel_gk20a *ch)
{
	int err;

	gk20a_dbg_fn("");

	err = gk20a_alloc_inst_block(g, &ch->inst_block);
	if (err)
		return err;

	gk20a_dbg_info("channel %d inst block physical addr: 0x%16llx",
		ch->hw_chid, gk20a_mm_inst_block_addr(g, &ch->inst_block));

	gk20a_dbg_fn("done");
	return 0;
}

void channel_gk20a_free_inst(struct gk20a *g, struct channel_gk20a *ch)
{
	gk20a_free_inst_block(g, &ch->inst_block);
}

static int channel_gk20a_update_runlist(struct channel_gk20a *c, bool add)
{
	return c->g->ops.fifo.update_runlist(c->g, c->runlist_id, c->hw_chid, add, true);
}

void channel_gk20a_enable(struct channel_gk20a *ch)
{
	/* enable channel */
	gk20a_writel(ch->g, ccsr_channel_r(ch->hw_chid),
		gk20a_readl(ch->g, ccsr_channel_r(ch->hw_chid)) |
		ccsr_channel_enable_set_true_f());
}

void channel_gk20a_disable(struct channel_gk20a *ch)
{
	/* disable channel */
	gk20a_writel(ch->g, ccsr_channel_r(ch->hw_chid),
		gk20a_readl(ch->g,
			ccsr_channel_r(ch->hw_chid)) |
			ccsr_channel_enable_clr_true_f());
}

int gk20a_enable_channel_tsg(struct gk20a *g, struct channel_gk20a *ch)
{
	struct tsg_gk20a *tsg;

	if (gk20a_is_channel_marked_as_tsg(ch)) {
		tsg = &g->fifo.tsg[ch->tsgid];
		gk20a_enable_tsg(tsg);
	} else {
		g->ops.fifo.enable_channel(ch);
	}

	return 0;
}

int gk20a_disable_channel_tsg(struct gk20a *g, struct channel_gk20a *ch)
{
	struct tsg_gk20a *tsg;

	if (gk20a_is_channel_marked_as_tsg(ch)) {
		tsg = &g->fifo.tsg[ch->tsgid];
		gk20a_disable_tsg(tsg);
	} else {
		g->ops.fifo.disable_channel(ch);
	}

	return 0;
}

void gk20a_channel_abort_clean_up(struct channel_gk20a *ch)
{
	struct channel_gk20a_job *job, *n;
	bool released_job_semaphore = false;
	bool pre_alloc_enabled = channel_gk20a_is_prealloc_enabled(ch);

	gk20a_channel_cancel_job_clean_up(ch, true);

	/* ensure no fences are pending */
	mutex_lock(&ch->sync_lock);
	if (ch->sync)
		ch->sync->set_min_eq_max(ch->sync);
	mutex_unlock(&ch->sync_lock);

	/* release all job semaphores (applies only to jobs that use
	   semaphore synchronization) */
	channel_gk20a_joblist_lock(ch);
	if (pre_alloc_enabled) {
		int tmp_get = ch->joblist.pre_alloc.get;
		int put = ch->joblist.pre_alloc.put;

		/*
		 * ensure put is read before any subsequent reads.
		 * see corresponding wmb in gk20a_channel_add_job()
		 */
		rmb();

		while (tmp_get != put) {
			job = &ch->joblist.pre_alloc.jobs[tmp_get];
			if (job->post_fence->semaphore) {
				__gk20a_semaphore_release(
					job->post_fence->semaphore, true);
				released_job_semaphore = true;
			}
			tmp_get = (tmp_get + 1) % ch->joblist.pre_alloc.length;
		}
	} else {
		list_for_each_entry_safe(job, n,
				&ch->joblist.dynamic.jobs, list) {
			if (job->post_fence->semaphore) {
				__gk20a_semaphore_release(
					job->post_fence->semaphore, true);
				released_job_semaphore = true;
			}
		}
	}
	channel_gk20a_joblist_unlock(ch);

	if (released_job_semaphore)
		wake_up_interruptible_all(&ch->semaphore_wq);

	gk20a_channel_update(ch, 0);
}

void gk20a_channel_abort(struct channel_gk20a *ch, bool channel_preempt)
{
	gk20a_dbg_fn("");

	if (gk20a_is_channel_marked_as_tsg(ch))
		return gk20a_fifo_abort_tsg(ch->g, ch->tsgid, channel_preempt);

	/* make sure new kickoffs are prevented */
	ch->has_timedout = true;

	ch->g->ops.fifo.disable_channel(ch);

	if (channel_preempt)
		ch->g->ops.fifo.preempt_channel(ch->g, ch->hw_chid);

	gk20a_channel_abort_clean_up(ch);
}

int gk20a_wait_channel_idle(struct channel_gk20a *ch)
{
	bool channel_idle = false;
	unsigned long end_jiffies = jiffies +
		msecs_to_jiffies(gk20a_get_gr_idle_timeout(ch->g));

	do {
		channel_gk20a_joblist_lock(ch);
		channel_idle = channel_gk20a_joblist_is_empty(ch);
		channel_gk20a_joblist_unlock(ch);
		if (channel_idle)
			break;

		usleep_range(1000, 3000);
	} while (time_before(jiffies, end_jiffies)
			|| !tegra_platform_is_silicon());

	if (!channel_idle) {
		gk20a_err(dev_from_gk20a(ch->g), "jobs not freed for channel %d\n",
				ch->hw_chid);
		return -EBUSY;
	}

	return 0;
}

void gk20a_disable_channel(struct channel_gk20a *ch)
{
	gk20a_channel_abort(ch, true);
	channel_gk20a_update_runlist(ch, false);
}

#if defined(CONFIG_GK20A_CYCLE_STATS)

static void gk20a_free_cycle_stats_buffer(struct channel_gk20a *ch)
{
	/* disable existing cyclestats buffer */
	mutex_lock(&ch->cyclestate.cyclestate_buffer_mutex);
	if (ch->cyclestate.cyclestate_buffer_handler) {
		dma_buf_vunmap(ch->cyclestate.cyclestate_buffer_handler,
				ch->cyclestate.cyclestate_buffer);
		dma_buf_put(ch->cyclestate.cyclestate_buffer_handler);
		ch->cyclestate.cyclestate_buffer_handler = NULL;
		ch->cyclestate.cyclestate_buffer = NULL;
		ch->cyclestate.cyclestate_buffer_size = 0;
	}
	mutex_unlock(&ch->cyclestate.cyclestate_buffer_mutex);
}

static int gk20a_channel_cycle_stats(struct channel_gk20a *ch,
		       struct nvgpu_cycle_stats_args *args)
{
	struct dma_buf *dmabuf;
	void *virtual_address;

	/* is it allowed to handle calls for current GPU? */
	if (0 == (ch->g->gpu_characteristics.flags &
			NVGPU_GPU_FLAGS_SUPPORT_CYCLE_STATS))
		return -ENOSYS;

	if (args->dmabuf_fd && !ch->cyclestate.cyclestate_buffer_handler) {

		/* set up new cyclestats buffer */
		dmabuf = dma_buf_get(args->dmabuf_fd);
		if (IS_ERR(dmabuf))
			return PTR_ERR(dmabuf);
		virtual_address = dma_buf_vmap(dmabuf);
		if (!virtual_address)
			return -ENOMEM;

		ch->cyclestate.cyclestate_buffer_handler = dmabuf;
		ch->cyclestate.cyclestate_buffer = virtual_address;
		ch->cyclestate.cyclestate_buffer_size = dmabuf->size;
		return 0;

	} else if (!args->dmabuf_fd &&
			ch->cyclestate.cyclestate_buffer_handler) {
		gk20a_free_cycle_stats_buffer(ch);
		return 0;

	} else if (!args->dmabuf_fd &&
			!ch->cyclestate.cyclestate_buffer_handler) {
		/* no requst from GL */
		return 0;

	} else {
		pr_err("channel already has cyclestats buffer\n");
		return -EINVAL;
	}
}


static int gk20a_flush_cycle_stats_snapshot(struct channel_gk20a *ch)
{
	int ret;

	mutex_lock(&ch->cs_client_mutex);
	if (ch->cs_client)
		ret = gr_gk20a_css_flush(ch->g, ch->cs_client);
	else
		ret = -EBADF;
	mutex_unlock(&ch->cs_client_mutex);

	return ret;
}

static int gk20a_attach_cycle_stats_snapshot(struct channel_gk20a *ch,
				u32 dmabuf_fd,
				u32 perfmon_id_count,
				u32 *perfmon_id_start)
{
	int ret;

	mutex_lock(&ch->cs_client_mutex);
	if (ch->cs_client) {
		ret = -EEXIST;
	} else {
		ret = gr_gk20a_css_attach(ch->g,
					dmabuf_fd,
					perfmon_id_count,
					perfmon_id_start,
					&ch->cs_client);
	}
	mutex_unlock(&ch->cs_client_mutex);

	return ret;
}

static int gk20a_free_cycle_stats_snapshot(struct channel_gk20a *ch)
{
	int ret;

	mutex_lock(&ch->cs_client_mutex);
	if (ch->cs_client) {
		ret = gr_gk20a_css_detach(ch->g, ch->cs_client);
		ch->cs_client = NULL;
	} else {
		ret = 0;
	}
	mutex_unlock(&ch->cs_client_mutex);

	return ret;
}

static int gk20a_channel_cycle_stats_snapshot(struct channel_gk20a *ch,
			struct nvgpu_cycle_stats_snapshot_args *args)
{
	int ret;

	/* is it allowed to handle calls for current GPU? */
	if (0 == (ch->g->gpu_characteristics.flags &
			NVGPU_GPU_FLAGS_SUPPORT_CYCLE_STATS_SNAPSHOT))
		return -ENOSYS;

	if (!args->dmabuf_fd)
		return -EINVAL;

	/* handle the command (most frequent cases first) */
	switch (args->cmd) {
	case NVGPU_IOCTL_CHANNEL_CYCLE_STATS_SNAPSHOT_CMD_FLUSH:
		ret = gk20a_flush_cycle_stats_snapshot(ch);
		args->extra = 0;
		break;

	case NVGPU_IOCTL_CHANNEL_CYCLE_STATS_SNAPSHOT_CMD_ATTACH:
		ret = gk20a_attach_cycle_stats_snapshot(ch,
						args->dmabuf_fd,
						args->extra,
						&args->extra);
		break;

	case NVGPU_IOCTL_CHANNEL_CYCLE_STATS_SNAPSHOT_CMD_DETACH:
		ret = gk20a_free_cycle_stats_snapshot(ch);
		args->extra = 0;
		break;

	default:
		pr_err("cyclestats: unknown command %u\n", args->cmd);
		ret = -EINVAL;
		break;
	}

	return ret;
}
#endif

static int gk20a_channel_set_wdt_status(struct channel_gk20a *ch,
		struct nvgpu_channel_wdt_args *args)
{
	if (args->wdt_status == NVGPU_IOCTL_CHANNEL_DISABLE_WDT)
		ch->wdt_enabled = false;
	else if (args->wdt_status == NVGPU_IOCTL_CHANNEL_ENABLE_WDT)
		ch->wdt_enabled = true;

	return 0;
}

int gk20a_channel_set_runlist_interleave(struct channel_gk20a *ch,
						u32 level)
{
	struct gk20a *g = ch->g;
	int ret;

	if (gk20a_is_channel_marked_as_tsg(ch)) {
		gk20a_err(dev_from_gk20a(g), "invalid operation for TSG!\n");
		return -EINVAL;
	}

	switch (level) {
	case NVGPU_RUNLIST_INTERLEAVE_LEVEL_LOW:
	case NVGPU_RUNLIST_INTERLEAVE_LEVEL_MEDIUM:
	case NVGPU_RUNLIST_INTERLEAVE_LEVEL_HIGH:
		ret = g->ops.fifo.set_runlist_interleave(g, ch->hw_chid,
							false, 0, level);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret ? ret : g->ops.fifo.update_runlist(g, ch->runlist_id, ~0, true, true);
}

static int gk20a_init_error_notifier(struct channel_gk20a *ch,
		struct nvgpu_set_error_notifier *args)
{
	struct device *dev = dev_from_gk20a(ch->g);
	struct dma_buf *dmabuf;
	void *va;
	u64 end = args->offset + sizeof(struct nvgpu_notification);

	if (!args->mem) {
		pr_err("gk20a_init_error_notifier: invalid memory handle\n");
		return -EINVAL;
	}

	dmabuf = dma_buf_get(args->mem);

	gk20a_free_error_notifiers(ch);

	if (IS_ERR(dmabuf)) {
		pr_err("Invalid handle: %d\n", args->mem);
		return -EINVAL;
	}

	if (end > dmabuf->size || end < sizeof(struct nvgpu_notification)) {
		dma_buf_put(dmabuf);
		gk20a_err(dev, "gk20a_init_error_notifier: invalid offset\n");
		return -EINVAL;
	}

	/* map handle */
	va = dma_buf_vmap(dmabuf);
	if (!va) {
		dma_buf_put(dmabuf);
		pr_err("Cannot map notifier handle\n");
		return -ENOMEM;
	}

	ch->error_notifier = va + args->offset;
	ch->error_notifier_va = va;
	memset(ch->error_notifier, 0, sizeof(struct nvgpu_notification));

	/* set channel notifiers pointer */
	mutex_lock(&ch->error_notifier_mutex);
	ch->error_notifier_ref = dmabuf;
	mutex_unlock(&ch->error_notifier_mutex);

	return 0;
}

void gk20a_set_error_notifier(struct channel_gk20a *ch, __u32 error)
{
	bool notifier_set = false;

	mutex_lock(&ch->error_notifier_mutex);
	if (ch->error_notifier_ref) {
		struct timespec time_data;
		u64 nsec;
		getnstimeofday(&time_data);
		nsec = ((u64)time_data.tv_sec) * 1000000000u +
				(u64)time_data.tv_nsec;
		ch->error_notifier->time_stamp.nanoseconds[0] =
				(u32)nsec;
		ch->error_notifier->time_stamp.nanoseconds[1] =
				(u32)(nsec >> 32);
		ch->error_notifier->info32 = error;
		ch->error_notifier->status = 0xffff;

		notifier_set = true;
	}
	mutex_unlock(&ch->error_notifier_mutex);

	if (notifier_set)
		gk20a_err(dev_from_gk20a(ch->g),
		    "error notifier set to %d for ch %d", error, ch->hw_chid);
}

static void gk20a_free_error_notifiers(struct channel_gk20a *ch)
{
	mutex_lock(&ch->error_notifier_mutex);
	if (ch->error_notifier_ref) {
		dma_buf_vunmap(ch->error_notifier_ref, ch->error_notifier_va);
		dma_buf_put(ch->error_notifier_ref);
		ch->error_notifier_ref = NULL;
		ch->error_notifier = NULL;
		ch->error_notifier_va = NULL;
	}
	mutex_unlock(&ch->error_notifier_mutex);
}

static void gk20a_wait_until_counter_is_N(
	struct channel_gk20a *ch, atomic_t *counter, int wait_value,
	wait_queue_head_t *wq, const char *caller, const char *counter_name)
{
	while (true) {
		if (wait_event_timeout(
			    *wq,
			    atomic_read(counter) == wait_value,
			    msecs_to_jiffies(5000)) > 0)
			break;

		gk20a_warn(dev_from_gk20a(ch->g),
			   "%s: channel %d, still waiting, %s left: %d, waiting for: %d",
			   caller, ch->hw_chid, counter_name,
			   atomic_read(counter), wait_value);
	}
}

/* call ONLY when no references to the channel exist: after the last put */
static void gk20a_free_channel(struct channel_gk20a *ch, bool force)
{
	struct gk20a *g = ch->g;
	struct fifo_gk20a *f = &g->fifo;
	struct gr_gk20a *gr = &g->gr;
	struct vm_gk20a *ch_vm = ch->vm;
	unsigned long timeout = gk20a_get_gr_idle_timeout(g);
	struct dbg_session_gk20a *dbg_s;
	struct dbg_session_data *session_data, *tmp_s;
	struct dbg_session_channel_data *ch_data, *tmp;
	bool was_reset;

	gk20a_dbg_fn("");

	WARN_ON(ch->g == NULL);

	trace_gk20a_free_channel(ch->hw_chid);

	/* abort channel and remove from runlist */
	gk20a_disable_channel(ch);

	/* wait until there's only our ref to the channel */
	if (!force)
		gk20a_wait_until_counter_is_N(
			ch, &ch->ref_count, 1, &ch->ref_count_dec_wq,
			__func__, "references");

	/* wait until all pending interrupts for recently completed
	 * jobs are handled */
	nvgpu_wait_for_deferred_interrupts(g);

	/* prevent new refs */
	raw_spin_lock(&ch->ref_obtain_lock);
	if (!ch->referenceable) {
		raw_spin_unlock(&ch->ref_obtain_lock);
		gk20a_err(dev_from_gk20a(ch->g),
			  "Extra %s() called to channel %u",
			  __func__, ch->hw_chid);
		return;
	}
	ch->referenceable = false;
	raw_spin_unlock(&ch->ref_obtain_lock);

	/* matches with the initial reference in gk20a_open_new_channel() */
	atomic_dec(&ch->ref_count);

	/* wait until no more refs to the channel */
	if (!force)
		gk20a_wait_until_counter_is_N(
			ch, &ch->ref_count, 0, &ch->ref_count_dec_wq,
			__func__, "references");

	/* if engine reset was deferred, perform it now */
	mutex_lock(&f->deferred_reset_mutex);
	if (g->fifo.deferred_reset_pending) {
		gk20a_dbg(gpu_dbg_intr | gpu_dbg_gpu_dbg, "engine reset was"
			   " deferred, running now");
		was_reset = mutex_is_locked(&g->fifo.gr_reset_mutex);
		mutex_lock(&g->fifo.gr_reset_mutex);
		/* if lock is already taken, a reset is taking place
		so no need to repeat */
		if (!was_reset)
			gk20a_fifo_deferred_reset(g, ch);
		mutex_unlock(&g->fifo.gr_reset_mutex);
	}
	mutex_unlock(&f->deferred_reset_mutex);

	if (!gk20a_channel_as_bound(ch))
		goto unbind;

	gk20a_dbg_info("freeing bound channel context, timeout=%ld",
			timeout);

	gk20a_free_error_notifiers(ch);

	if (g->ops.fecs_trace.unbind_channel)
		g->ops.fecs_trace.unbind_channel(g, ch);

	/* release channel ctx */
	g->ops.gr.free_channel_ctx(ch);

	gk20a_gr_flush_channel_tlb(gr);

	memset(&ch->ramfc, 0, sizeof(struct mem_desc_sub));

	gk20a_gmmu_unmap_free(ch_vm, &ch->gpfifo.mem);
	nvgpu_free(ch->gpfifo.pipe);
	memset(&ch->gpfifo, 0, sizeof(struct gpfifo_desc));

#if defined(CONFIG_GK20A_CYCLE_STATS)
	gk20a_free_cycle_stats_buffer(ch);
	gk20a_free_cycle_stats_snapshot(ch);
#endif

	channel_gk20a_free_priv_cmdbuf(ch);

	/* sync must be destroyed before releasing channel vm */
	mutex_lock(&ch->sync_lock);
	if (ch->sync) {
		gk20a_channel_sync_destroy(ch->sync);
		ch->sync = NULL;
	}
	mutex_unlock(&ch->sync_lock);

	/*
	 * free the channel used semaphore index.
	 * we need to do this before releasing the address space,
	 * as the semaphore pool might get freed after that point.
	 */
	if (ch->hw_sema)
		gk20a_semaphore_free_hw_sema(ch);

	/*
	 * When releasing the channel we unbind the VM - so release the ref.
	 */
	gk20a_vm_put(ch_vm);

	spin_lock(&ch->update_fn_lock);
	ch->update_fn = NULL;
	ch->update_fn_data = NULL;
	spin_unlock(&ch->update_fn_lock);
	cancel_work_sync(&ch->update_fn_work);
	cancel_delayed_work_sync(&ch->clean_up.wq);
	cancel_delayed_work_sync(&ch->timeout.wq);

	/* make sure we don't have deferred interrupts pending that
	 * could still touch the channel */
	nvgpu_wait_for_deferred_interrupts(g);

unbind:
	if (gk20a_is_channel_marked_as_tsg(ch))
		g->ops.fifo.tsg_unbind_channel(ch);

	g->ops.fifo.unbind_channel(ch);
	g->ops.fifo.free_inst(g, ch);

	ch->vpr = false;
	ch->deterministic = false;
	ch->vm = NULL;

	WARN_ON(ch->sync);

	/* unlink all debug sessions */
	mutex_lock(&g->dbg_sessions_lock);

	list_for_each_entry_safe(session_data, tmp_s,
				&ch->dbg_s_list, dbg_s_entry) {
		dbg_s = session_data->dbg_s;
		mutex_lock(&dbg_s->ch_list_lock);
		list_for_each_entry_safe(ch_data, tmp,
					&dbg_s->ch_list, ch_entry) {
			if (ch_data->chid == ch->hw_chid)
				dbg_unbind_single_channel_gk20a(dbg_s, ch_data);
		}
		mutex_unlock(&dbg_s->ch_list_lock);
	}

	mutex_unlock(&g->dbg_sessions_lock);

	/* free pre-allocated resources, if applicable */
	if (channel_gk20a_is_prealloc_enabled(ch))
		channel_gk20a_free_prealloc_resources(ch);

	/* make sure we catch accesses of unopened channels in case
	 * there's non-refcounted channel pointers hanging around */
	ch->g = NULL;
	wmb();

	/* ALWAYS last */
	free_channel(f, ch);
}

/* Try to get a reference to the channel. Return nonzero on success. If fails,
 * the channel is dead or being freed elsewhere and you must not touch it.
 *
 * Always when a channel_gk20a pointer is seen and about to be used, a
 * reference must be held to it - either by you or the caller, which should be
 * documented well or otherwise clearly seen. This usually boils down to the
 * file from ioctls directly, or an explicit get in exception handlers when the
 * channel is found by a hw_chid.
 *
 * Most global functions in this file require a reference to be held by the
 * caller.
 */
struct channel_gk20a *_gk20a_channel_get(struct channel_gk20a *ch,
					 const char *caller) {
	struct channel_gk20a *ret;

	raw_spin_lock(&ch->ref_obtain_lock);

	if (likely(ch->referenceable)) {
		atomic_inc(&ch->ref_count);
		ret = ch;
	} else
		ret = NULL;

	raw_spin_unlock(&ch->ref_obtain_lock);

	if (ret)
		trace_gk20a_channel_get(ch->hw_chid, caller);

	return ret;
}

void _gk20a_channel_put(struct channel_gk20a *ch, const char *caller)
{
	trace_gk20a_channel_put(ch->hw_chid, caller);
	atomic_dec(&ch->ref_count);
	wake_up_all(&ch->ref_count_dec_wq);

	/* More puts than gets. Channel is probably going to get
	 * stuck. */
	WARN_ON(atomic_read(&ch->ref_count) < 0);

	/* Also, more puts than gets. ref_count can go to 0 only if
	 * the channel is closing. Channel is probably going to get
	 * stuck. */
	WARN_ON(atomic_read(&ch->ref_count) == 0 && ch->referenceable);
}

void gk20a_channel_close(struct channel_gk20a *ch)
{
	gk20a_free_channel(ch, false);
}

/*
 * Be careful with this - it is meant for terminating channels when we know the
 * driver is otherwise dying. Ref counts and the like are ignored by this
 * version of the cleanup.
 */
void __gk20a_channel_kill(struct channel_gk20a *ch)
{
	gk20a_free_channel(ch, true);
}

struct channel_gk20a *gk20a_get_channel_from_file(int fd)
{
	struct channel_priv *priv;
	struct file *f = fget(fd);

	if (!f)
		return NULL;

	if (f->f_op != &gk20a_channel_ops) {
		fput(f);
		return NULL;
	}

	priv = (struct channel_priv *)f->private_data;
	fput(f);
	return priv->c;
}

int gk20a_channel_release(struct inode *inode, struct file *filp)
{
	struct channel_priv *priv = filp->private_data;
	struct channel_gk20a *ch = priv->c;
	struct gk20a *g = priv->g;

	int err;

	err = gk20a_busy(g->dev);
	if (err) {
		gk20a_err(dev_from_gk20a(g), "failed to release a channel!");
		return err;
	}

	trace_gk20a_channel_release(dev_name(g->dev));

	gk20a_channel_close(ch);
	gk20a_idle(g->dev);

	filp->private_data = NULL;
	return 0;
}

static void gk20a_channel_update_runcb_fn(struct work_struct *work)
{
	struct channel_gk20a *ch =
		container_of(work, struct channel_gk20a, update_fn_work);
	void (*update_fn)(struct channel_gk20a *, void *);
	void *update_fn_data;

	spin_lock(&ch->update_fn_lock);
	update_fn = ch->update_fn;
	update_fn_data = ch->update_fn_data;
	spin_unlock(&ch->update_fn_lock);

	if (update_fn)
		update_fn(ch, update_fn_data);
}

struct channel_gk20a *gk20a_open_new_channel_with_cb(struct gk20a *g,
		void (*update_fn)(struct channel_gk20a *, void *),
		void *update_fn_data,
		int runlist_id,
		bool is_privileged_channel)
{
	struct channel_gk20a *ch = gk20a_open_new_channel(g, runlist_id, is_privileged_channel);

	if (ch) {
		spin_lock(&ch->update_fn_lock);
		ch->update_fn = update_fn;
		ch->update_fn_data = update_fn_data;
		spin_unlock(&ch->update_fn_lock);
	}

	return ch;
}

struct channel_gk20a *gk20a_open_new_channel(struct gk20a *g,
		s32 runlist_id,
		bool is_privileged_channel)
{
	struct fifo_gk20a *f = &g->fifo;
	struct channel_gk20a *ch;
	struct gk20a_event_id_data *event_id_data, *event_id_data_temp;

	/* compatibility with existing code */
	if (!gk20a_fifo_is_valid_runlist_id(g, runlist_id)) {
		runlist_id = gk20a_fifo_get_gr_runlist_id(g);
	}

	gk20a_dbg_fn("");

	ch = allocate_channel(f);
	if (ch == NULL) {
		/* TBD: we want to make this virtualizable */
		gk20a_err(dev_from_gk20a(g), "out of hw chids");
		return NULL;
	}

	trace_gk20a_open_new_channel(ch->hw_chid);

	BUG_ON(ch->g);
	ch->g = g;

	/* Runlist for the channel */
	ch->runlist_id = runlist_id;

	/* Channel privilege level */
	ch->is_privileged_channel = is_privileged_channel;

	if (g->ops.fifo.alloc_inst(g, ch)) {
		ch->g = NULL;
		free_channel(f, ch);
		gk20a_err(dev_from_gk20a(g),
			   "failed to open gk20a channel, out of inst mem");
		return NULL;
	}

	/* now the channel is in a limbo out of the free list but not marked as
	 * alive and used (i.e. get-able) yet */

	ch->pid = current->pid;
	ch->tgid = current->tgid;  /* process granularity for FECS traces */

	/* unhook all events created on this channel */
	mutex_lock(&ch->event_id_list_lock);
	list_for_each_entry_safe(event_id_data, event_id_data_temp,
				&ch->event_id_list,
				event_id_node) {
		list_del_init(&event_id_data->event_id_node);
	}
	mutex_unlock(&ch->event_id_list_lock);

	/* By default, channel is regular (non-TSG) channel */
	ch->tsgid = NVGPU_INVALID_TSG_ID;

	/* reset timeout counter and update timestamp */
	ch->timeout_accumulated_ms = 0;
	ch->timeout_gpfifo_get = 0;
	/* set gr host default timeout */
	ch->timeout_ms_max = gk20a_get_gr_idle_timeout(g);
	ch->timeout_debug_dump = true;
	ch->has_timedout = false;
	ch->wdt_enabled = true;
	ch->obj_class = 0;
	ch->clean_up.scheduled = false;
	ch->interleave_level = NVGPU_RUNLIST_INTERLEAVE_LEVEL_LOW;
	ch->timeslice_us = g->timeslice_low_priority_us;

	/* The channel is *not* runnable at this point. It still needs to have
	 * an address space bound and allocate a gpfifo and grctx. */

	init_waitqueue_head(&ch->notifier_wq);
	init_waitqueue_head(&ch->semaphore_wq);

	ch->update_fn = NULL;
	ch->update_fn_data = NULL;
	spin_lock_init(&ch->update_fn_lock);
	INIT_WORK(&ch->update_fn_work, gk20a_channel_update_runcb_fn);

	/* Mark the channel alive, get-able, with 1 initial use
	 * references. The initial reference will be decreased in
	 * gk20a_free_channel() */
	ch->referenceable = true;
	atomic_set(&ch->ref_count, 1);
	wmb();

	return ch;
}

/* note: runlist_id -1 is synonym for the ENGINE_GR_GK20A runlist id */
static int __gk20a_channel_open(struct gk20a *g, struct file *filp, s32 runlist_id)
{
	int err;
	struct channel_gk20a *ch;
	struct channel_priv *priv;

	gk20a_dbg_fn("");

	trace_gk20a_channel_open(dev_name(g->dev));

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	err = gk20a_busy(g->dev);
	if (err) {
		gk20a_err(dev_from_gk20a(g), "failed to power on, %d", err);
		goto fail_busy;
	}
	/* All the user space channel should be non privilege */
	ch = gk20a_open_new_channel(g, runlist_id, false);
	gk20a_idle(g->dev);
	if (!ch) {
		gk20a_err(dev_from_gk20a(g),
			"failed to get f");
		err = -ENOMEM;
		goto fail_busy;
	}

	gk20a_channel_trace_sched_param(
		trace_gk20a_channel_sched_defaults, ch);

	priv->g = g;
	priv->c = ch;

	filp->private_data = priv;
	return 0;

fail_busy:
	kfree(priv);
	return err;
}

int gk20a_channel_open(struct inode *inode, struct file *filp)
{
	struct gk20a *g = container_of(inode->i_cdev,
			struct gk20a, channel.cdev);
	int ret;

	gk20a_dbg_fn("start");
	ret = __gk20a_channel_open(g, filp, -1);

	gk20a_dbg_fn("end");
	return ret;
}

int gk20a_channel_open_ioctl(struct gk20a *g,
		struct nvgpu_channel_open_args *args)
{
	int err;
	int fd;
	struct file *file;
	char *name;
	s32 runlist_id = args->in.runlist_id;

	err = get_unused_fd_flags(O_RDWR);
	if (err < 0)
		return err;
	fd = err;

	name = kasprintf(GFP_KERNEL, "nvhost-%s-fd%d",
			dev_name(g->dev), fd);
	if (!name) {
		err = -ENOMEM;
		goto clean_up;
	}

	file = anon_inode_getfile(name, g->channel.cdev.ops, NULL, O_RDWR);
	kfree(name);
	if (IS_ERR(file)) {
		err = PTR_ERR(file);
		goto clean_up;
	}

	err = __gk20a_channel_open(g, file, runlist_id);
	if (err)
		goto clean_up_file;

	fd_install(fd, file);
	args->out.channel_fd = fd;
	return 0;

clean_up_file:
	fput(file);
clean_up:
	put_unused_fd(fd);
	return err;
}

/* allocate private cmd buffer.
   used for inserting commands before/after user submitted buffers. */
static int channel_gk20a_alloc_priv_cmdbuf(struct channel_gk20a *c)
{
	struct device *d = dev_from_gk20a(c->g);
	struct vm_gk20a *ch_vm = c->vm;
	struct priv_cmd_queue *q = &c->priv_cmd_q;
	u32 size;
	int err = 0;

	/*
	 * Compute the amount of priv_cmdbuf space we need. In general the worst
	 * case is the kernel inserts both a semaphore pre-fence and post-fence.
	 * Any sync-pt fences will take less memory so we can ignore them for
	 * now.
	 *
	 * A semaphore ACQ (fence-wait) is 8 dwords: semaphore_a, semaphore_b,
	 * semaphore_c, and semaphore_d. A semaphore INCR (fence-get) will be 10
	 * dwords: all the same as an ACQ plus a non-stalling intr which is
	 * another 2 dwords.
	 *
	 * Lastly the number of gpfifo entries per channel is fixed so at most
	 * we can use 2/3rds of the gpfifo entries (1 pre-fence entry, one
	 * userspace entry, and one post-fence entry). Thus the computation is:
	 *
	 *   (gpfifo entry number * (2 / 3) * (8 + 10) * 4 bytes.
	 */
	size = roundup_pow_of_two(c->gpfifo.entry_num *
				  2 * 18 * sizeof(u32) / 3);

	err = gk20a_gmmu_alloc_map_sys(ch_vm, size, &q->mem);
	if (err) {
		gk20a_err(d, "%s: memory allocation failed\n", __func__);
		goto clean_up;
	}

	q->size = q->mem.size / sizeof (u32);

	return 0;

clean_up:
	channel_gk20a_free_priv_cmdbuf(c);
	return err;
}

static void channel_gk20a_free_priv_cmdbuf(struct channel_gk20a *c)
{
	struct vm_gk20a *ch_vm = c->vm;
	struct priv_cmd_queue *q = &c->priv_cmd_q;

	if (q->size == 0)
		return;

	gk20a_gmmu_unmap_free(ch_vm, &q->mem);

	memset(q, 0, sizeof(struct priv_cmd_queue));
}

/* allocate a cmd buffer with given size. size is number of u32 entries */
int gk20a_channel_alloc_priv_cmdbuf(struct channel_gk20a *c, u32 orig_size,
			     struct priv_cmd_entry *e)
{
	struct priv_cmd_queue *q = &c->priv_cmd_q;
	u32 free_count;
	u32 size = orig_size;

	gk20a_dbg_fn("size %d", orig_size);

	if (!e) {
		gk20a_err(dev_from_gk20a(c->g),
			"ch %d: priv cmd entry is null",
			c->hw_chid);
		return -EINVAL;
	}

	/* if free space in the end is less than requested, increase the size
	 * to make the real allocated space start from beginning. */
	if (q->put + size > q->size)
		size = orig_size + (q->size - q->put);

	gk20a_dbg_info("ch %d: priv cmd queue get:put %d:%d",
			c->hw_chid, q->get, q->put);

	free_count = (q->size - (q->put - q->get) - 1) % q->size;

	if (size > free_count)
		return -EAGAIN;

	e->size = orig_size;
	e->mem = &q->mem;

	/* if we have increased size to skip free space in the end, set put
	   to beginning of cmd buffer (0) + size */
	if (size != orig_size) {
		e->off = 0;
		e->gva = q->mem.gpu_va;
		q->put = orig_size;
	} else {
		e->off = q->put;
		e->gva = q->mem.gpu_va + q->put * sizeof(u32);
		q->put = (q->put + orig_size) & (q->size - 1);
	}

	/* we already handled q->put + size > q->size so BUG_ON this */
	BUG_ON(q->put > q->size);

	/*
	 * commit the previous writes before making the entry valid.
	 * see the corresponding rmb() in gk20a_free_priv_cmdbuf().
	 */
	wmb();

	e->valid = true;
	gk20a_dbg_fn("done");

	return 0;
}

/* Don't call this to free an explict cmd entry.
 * It doesn't update priv_cmd_queue get/put */
static void free_priv_cmdbuf(struct channel_gk20a *c,
			     struct priv_cmd_entry *e)
{
	if (channel_gk20a_is_prealloc_enabled(c))
		memset(e, 0, sizeof(struct priv_cmd_entry));
	else
		kfree(e);
}

static int channel_gk20a_alloc_job(struct channel_gk20a *c,
		struct channel_gk20a_job **job_out)
{
	int err = 0;

	if (channel_gk20a_is_prealloc_enabled(c)) {
		int put = c->joblist.pre_alloc.put;
		int get = c->joblist.pre_alloc.get;

		/*
		 * ensure all subsequent reads happen after reading get.
		 * see corresponding wmb in gk20a_channel_clean_up_jobs()
		 */
		rmb();

		if (CIRC_SPACE(put, get, c->joblist.pre_alloc.length))
			*job_out = &c->joblist.pre_alloc.jobs[put];
		else {
			gk20a_warn(dev_from_gk20a(c->g),
					"out of job ringbuffer space\n");
			err = -EAGAIN;
		}
	} else {
		*job_out = kzalloc(sizeof(struct channel_gk20a_job),
				GFP_KERNEL);
		if (!job_out)
			err = -ENOMEM;
	}

	return err;
}

static void channel_gk20a_free_job(struct channel_gk20a *c,
		struct channel_gk20a_job *job)
{
	/*
	 * In case of pre_allocated jobs, we need to clean out
	 * the job but maintain the pointers to the priv_cmd_entry,
	 * since they're inherently tied to the job node.
	 */
	if (channel_gk20a_is_prealloc_enabled(c)) {
		struct priv_cmd_entry *wait_cmd = job->wait_cmd;
		struct priv_cmd_entry *incr_cmd = job->incr_cmd;
		memset(job, 0, sizeof(*job));
		job->wait_cmd = wait_cmd;
		job->incr_cmd = incr_cmd;
	} else
		kfree(job);
}

void channel_gk20a_joblist_lock(struct channel_gk20a *c)
{
	mutex_lock(&c->joblist.lock);
}

void channel_gk20a_joblist_unlock(struct channel_gk20a *c)
{
	mutex_unlock(&c->joblist.lock);
}

static struct channel_gk20a_job *channel_gk20a_joblist_peek(
		struct channel_gk20a *c)
{
	int get;
	struct channel_gk20a_job *job = NULL;

	if (channel_gk20a_is_prealloc_enabled(c)) {
		if (!channel_gk20a_joblist_is_empty(c)) {
			get = c->joblist.pre_alloc.get;
			job = &c->joblist.pre_alloc.jobs[get];
		}
	} else {
		if (!list_empty(&c->joblist.dynamic.jobs))
			job = list_first_entry(&c->joblist.dynamic.jobs,
				       struct channel_gk20a_job, list);
	}

	return job;
}

static void channel_gk20a_joblist_add(struct channel_gk20a *c,
		struct channel_gk20a_job *job)
{
	if (channel_gk20a_is_prealloc_enabled(c)) {
		c->joblist.pre_alloc.put = (c->joblist.pre_alloc.put + 1) %
				(c->joblist.pre_alloc.length);
	} else {
		list_add_tail(&job->list, &c->joblist.dynamic.jobs);
	}
}

static void channel_gk20a_joblist_delete(struct channel_gk20a *c,
		struct channel_gk20a_job *job)
{
	if (channel_gk20a_is_prealloc_enabled(c)) {
		c->joblist.pre_alloc.get = (c->joblist.pre_alloc.get + 1) %
				(c->joblist.pre_alloc.length);
	} else {
		list_del_init(&job->list);
	}
}

bool channel_gk20a_joblist_is_empty(struct channel_gk20a *c)
{
	if (channel_gk20a_is_prealloc_enabled(c)) {
		int get = c->joblist.pre_alloc.get;
		int put = c->joblist.pre_alloc.put;
		return !(CIRC_CNT(put, get, c->joblist.pre_alloc.length));
	}

	return list_empty(&c->joblist.dynamic.jobs);
}

bool channel_gk20a_is_prealloc_enabled(struct channel_gk20a *c)
{
	bool pre_alloc_enabled = c->joblist.pre_alloc.enabled;

	rmb();
	return pre_alloc_enabled;
}

static int channel_gk20a_prealloc_resources(struct channel_gk20a *c,
	       unsigned int num_jobs)
{
	int i, err;
	size_t size;
	struct priv_cmd_entry *entries = NULL;

	if (channel_gk20a_is_prealloc_enabled(c) || !num_jobs)
		return -EINVAL;

	/*
	 * pre-allocate the job list.
	 * since vmalloc take in an unsigned long, we need
	 * to make sure we don't hit an overflow condition
	 */
	size = sizeof(struct channel_gk20a_job);
	if (num_jobs <= ULONG_MAX / size)
		c->joblist.pre_alloc.jobs = vzalloc(num_jobs * size);
	if (!c->joblist.pre_alloc.jobs) {
		err = -ENOMEM;
		goto clean_up;
	}

	/*
	 * pre-allocate 2x priv_cmd_entry for each job up front.
	 * since vmalloc take in an unsigned long, we need
	 * to make sure we don't hit an overflow condition
	 */
	size = sizeof(struct priv_cmd_entry);
	if (num_jobs <= ULONG_MAX / (size << 1))
		entries = vzalloc((num_jobs << 1) * size);
	if (!entries) {
		err = -ENOMEM;
		goto clean_up_joblist;
	}

	for (i = 0; i < num_jobs; i++) {
		c->joblist.pre_alloc.jobs[i].wait_cmd = &entries[i];
		c->joblist.pre_alloc.jobs[i].incr_cmd =
			&entries[i + num_jobs];
	}

	/* pre-allocate a fence pool */
	err = gk20a_alloc_fence_pool(c, num_jobs);
	if (err)
		goto clean_up_priv_cmd;

	c->joblist.pre_alloc.length = num_jobs;

	/*
	 * commit the previous writes before setting the flag.
	 * see corresponding rmb in channel_gk20a_is_prealloc_enabled()
	 */
	wmb();
	c->joblist.pre_alloc.enabled = true;

	return 0;

clean_up_priv_cmd:
	vfree(entries);
clean_up_joblist:
	vfree(c->joblist.pre_alloc.jobs);
clean_up:
	memset(&c->joblist.pre_alloc, 0, sizeof(c->joblist.pre_alloc));
	return err;
}

static void channel_gk20a_free_prealloc_resources(struct channel_gk20a *c)
{
	vfree(c->joblist.pre_alloc.jobs[0].wait_cmd);
	vfree(c->joblist.pre_alloc.jobs);
	gk20a_free_fence_pool(c);

	/*
	 * commit the previous writes before disabling the flag.
	 * see corresponding rmb in channel_gk20a_is_prealloc_enabled()
	 */
	wmb();
	c->joblist.pre_alloc.enabled = false;
}

int gk20a_alloc_channel_gpfifo(struct channel_gk20a *c,
		struct nvgpu_alloc_gpfifo_ex_args *args)
{
	struct gk20a *g = c->g;
	struct device *d = dev_from_gk20a(g);
	struct gk20a_platform *platform = gk20a_get_platform(d);
	struct vm_gk20a *ch_vm;
	u32 gpfifo_size;
	int err = 0;

	gpfifo_size = args->num_entries;

	if (args->flags & NVGPU_ALLOC_GPFIFO_EX_FLAGS_VPR_ENABLED)
		c->vpr = true;

	if (args->flags & NVGPU_ALLOC_GPFIFO_EX_FLAGS_DETERMINISTIC)
		c->deterministic = true;

	/* an address space needs to have been bound at this point. */
	if (!gk20a_channel_as_bound(c)) {
		gk20a_err(d,
			    "not bound to an address space at time of gpfifo"
			    " allocation.");
		return -EINVAL;
	}
	ch_vm = c->vm;

	c->ramfc.offset = 0;
	c->ramfc.size = ram_in_ramfc_s() / 8;

	if (c->gpfifo.mem.size) {
		gk20a_err(d, "channel %d :"
			   "gpfifo already allocated", c->hw_chid);
		return -EEXIST;
	}

	err = gk20a_gmmu_alloc_map_sys(ch_vm,
			gpfifo_size * sizeof(struct nvgpu_gpfifo),
			&c->gpfifo.mem);
	if (err) {
		gk20a_err(d, "%s: memory allocation failed\n", __func__);
		goto clean_up;
	}

	if (c->gpfifo.mem.aperture == APERTURE_VIDMEM || g->mm.force_pramin) {
		c->gpfifo.pipe = nvgpu_alloc(
				gpfifo_size * sizeof(struct nvgpu_gpfifo),
				false);
		if (!c->gpfifo.pipe) {
			err = -ENOMEM;
			goto clean_up_unmap;
		}
	}

	c->gpfifo.entry_num = gpfifo_size;
	c->gpfifo.get = c->gpfifo.put = 0;

	gk20a_dbg_info("channel %d : gpfifo_base 0x%016llx, size %d",
		c->hw_chid, c->gpfifo.mem.gpu_va, c->gpfifo.entry_num);

	channel_gk20a_setup_userd(c);

	if (!platform->aggressive_sync_destroy_thresh) {
		mutex_lock(&c->sync_lock);
		c->sync = gk20a_channel_sync_create(c);
		if (!c->sync) {
			err = -ENOMEM;
			mutex_unlock(&c->sync_lock);
			goto clean_up_unmap;
		}
		mutex_unlock(&c->sync_lock);

		if (g->ops.fifo.resetup_ramfc) {
			err = g->ops.fifo.resetup_ramfc(c);
			if (err)
				goto clean_up_sync;
		}
	}

	err = g->ops.fifo.setup_ramfc(c, c->gpfifo.mem.gpu_va,
					c->gpfifo.entry_num, args->flags);
	if (err)
		goto clean_up_sync;

	/* TBD: setup engine contexts */

	if (args->num_inflight_jobs) {
		err = channel_gk20a_prealloc_resources(c,
				args->num_inflight_jobs);
		if (err)
			goto clean_up_sync;
	}

	err = channel_gk20a_alloc_priv_cmdbuf(c);
	if (err)
		goto clean_up_prealloc;

	err = channel_gk20a_update_runlist(c, true);
	if (err)
		goto clean_up_priv_cmd;

	g->ops.fifo.bind_channel(c);

	gk20a_dbg_fn("done");
	return 0;

clean_up_priv_cmd:
	channel_gk20a_free_priv_cmdbuf(c);
clean_up_prealloc:
	if (args->num_inflight_jobs)
		channel_gk20a_free_prealloc_resources(c);
clean_up_sync:
	gk20a_channel_sync_destroy(c->sync);
	c->sync = NULL;
clean_up_unmap:
	nvgpu_free(c->gpfifo.pipe);
	gk20a_gmmu_unmap_free(ch_vm, &c->gpfifo.mem);
clean_up:
	memset(&c->gpfifo, 0, sizeof(struct gpfifo_desc));
	gk20a_err(d, "fail");
	return err;
}

/* Update with this periodically to determine how the gpfifo is draining. */
static inline u32 update_gp_get(struct gk20a *g,
				struct channel_gk20a *c)
{
	u32 new_get = gk20a_bar1_readl(g,
		c->userd_gpu_va + sizeof(u32) * ram_userd_gp_get_w());
	if (new_get < c->gpfifo.get)
		c->gpfifo.wrap = !c->gpfifo.wrap;
	c->gpfifo.get = new_get;
	return new_get;
}

static inline u32 gp_free_count(struct channel_gk20a *c)
{
	return (c->gpfifo.entry_num - (c->gpfifo.put - c->gpfifo.get) - 1) %
		c->gpfifo.entry_num;
}

bool gk20a_channel_update_and_check_timeout(struct channel_gk20a *ch,
		u32 timeout_delta_ms, bool *progress)
{
	u32 gpfifo_get = update_gp_get(ch->g, ch);

	/* Count consequent timeout isr */
	if (gpfifo_get == ch->timeout_gpfifo_get) {
		/* we didn't advance since previous channel timeout check */
		ch->timeout_accumulated_ms += timeout_delta_ms;
		*progress = false;
	} else {
		/* first timeout isr encountered */
		ch->timeout_accumulated_ms = timeout_delta_ms;
		*progress = true;
	}

	ch->timeout_gpfifo_get = gpfifo_get;

	return ch->g->timeouts_enabled &&
		ch->timeout_accumulated_ms > ch->timeout_ms_max;
}

static u32 gk20a_get_channel_watchdog_timeout(struct channel_gk20a *ch)
{
	struct gk20a_platform *platform = gk20a_get_platform(ch->g->dev);
	return platform->ch_wdt_timeout_ms;
}

static u32 get_gp_free_count(struct channel_gk20a *c)
{
	update_gp_get(c->g, c);
	return gp_free_count(c);
}

static void trace_write_pushbuffer(struct channel_gk20a *c,
				   struct nvgpu_gpfifo *g)
{
	void *mem = NULL;
	unsigned int words;
	u64 offset;
	struct dma_buf *dmabuf = NULL;

	if (gk20a_debug_trace_cmdbuf) {
		u64 gpu_va = (u64)g->entry0 |
			(u64)((u64)pbdma_gp_entry1_get_hi_v(g->entry1) << 32);
		int err;

		words = pbdma_gp_entry1_length_v(g->entry1);
		err = gk20a_vm_find_buffer(c->vm, gpu_va, &dmabuf, &offset);
		if (!err)
			mem = dma_buf_vmap(dmabuf);
	}

	if (mem) {
		u32 i;
		/*
		 * Write in batches of 128 as there seems to be a limit
		 * of how much you can output to ftrace at once.
		 */
		for (i = 0; i < words; i += 128U) {
			trace_gk20a_push_cmdbuf(
				dev_name(c->g->dev),
				0,
				min(words - i, 128U),
				offset + i * sizeof(u32),
				mem);
		}
		dma_buf_vunmap(dmabuf, mem);
	}
}

static void trace_write_pushbuffer_range(struct channel_gk20a *c,
					 struct nvgpu_gpfifo *g,
					 struct nvgpu_gpfifo __user *user_gpfifo,
					 int offset,
					 int count)
{
	u32 size;
	int i;
	struct nvgpu_gpfifo *gp;
	bool gpfifo_allocated = false;

	if (!gk20a_debug_trace_cmdbuf)
		return;

	if (!g && !user_gpfifo)
		return;

	if (!g) {
		size = count * sizeof(struct nvgpu_gpfifo);
		if (size) {
			g = nvgpu_alloc(size, false);
			if (!g)
				return;

			if (copy_from_user(g, user_gpfifo, size)) {
				nvgpu_free(g);
				return;
			}
		}
		gpfifo_allocated = true;
	}

	gp = g + offset;
	for (i = 0; i < count; i++, gp++)
		trace_write_pushbuffer(c, gp);

	if (gpfifo_allocated)
		nvgpu_free(g);
}

static void gk20a_channel_timeout_start(struct channel_gk20a *ch)
{
	struct gk20a_platform *platform = gk20a_get_platform(ch->g->dev);

	if (!ch->g->timeouts_enabled || !platform->ch_wdt_timeout_ms)
		return;

	if (!ch->wdt_enabled)
		return;

	raw_spin_lock(&ch->timeout.lock);

	if (ch->timeout.initialized) {
		raw_spin_unlock(&ch->timeout.lock);
		return;
	}

	ch->timeout.gp_get = gk20a_bar1_readl(ch->g,
		ch->userd_gpu_va + sizeof(u32) * ram_userd_gp_get_w());
	ch->timeout.initialized = true;
	raw_spin_unlock(&ch->timeout.lock);

	schedule_delayed_work(&ch->timeout.wq,
	       msecs_to_jiffies(gk20a_get_channel_watchdog_timeout(ch)));
}

static void gk20a_channel_timeout_stop(struct channel_gk20a *ch)
{
	raw_spin_lock(&ch->timeout.lock);
	if (!ch->timeout.initialized) {
		raw_spin_unlock(&ch->timeout.lock);
		return;
	}
	raw_spin_unlock(&ch->timeout.lock);

	cancel_delayed_work_sync(&ch->timeout.wq);

	raw_spin_lock(&ch->timeout.lock);
	ch->timeout.initialized = false;
	raw_spin_unlock(&ch->timeout.lock);
}

void gk20a_channel_timeout_restart_all_channels(struct gk20a *g)
{
	u32 chid;
	struct fifo_gk20a *f = &g->fifo;

	for (chid = 0; chid < f->num_channels; chid++) {
		struct channel_gk20a *ch = &f->channel[chid];

		if (gk20a_channel_get(ch)) {
			raw_spin_lock(&ch->timeout.lock);
			if (!ch->timeout.initialized) {
				raw_spin_unlock(&ch->timeout.lock);
				gk20a_channel_put(ch);
				continue;
			}
			raw_spin_unlock(&ch->timeout.lock);

			cancel_delayed_work_sync(&ch->timeout.wq);
			if (!ch->has_timedout)
				schedule_delayed_work(&ch->timeout.wq,
				       msecs_to_jiffies(
				       gk20a_get_channel_watchdog_timeout(ch)));

			gk20a_channel_put(ch);
		}
	}
}

static void gk20a_channel_timeout_handler(struct work_struct *work)
{
	u32 gp_get;
	struct gk20a *g;
	struct channel_gk20a *ch;
	struct channel_gk20a *failing_ch;
	u32 engine_id;
	int id = -1;
	bool is_tsg = false;

	ch = container_of(to_delayed_work(work), struct channel_gk20a,
			timeout.wq);
	ch = gk20a_channel_get(ch);
	if (!ch)
		return;

	g = ch->g;

	if (gk20a_busy(dev_from_gk20a(g))) {
		gk20a_channel_put(ch);
		return;
	}

	/* Need global lock since multiple channels can timeout at a time */
	mutex_lock(&g->ch_wdt_lock);

	/* Get timed out job and reset the timer */
	raw_spin_lock(&ch->timeout.lock);
	gp_get = ch->timeout.gp_get;
	ch->timeout.initialized = false;
	raw_spin_unlock(&ch->timeout.lock);

	if (gk20a_bar1_readl(g,
		ch->userd_gpu_va + sizeof(u32) * ram_userd_gp_get_w()) !=
								gp_get) {
		gk20a_channel_timeout_start(ch);
		goto fail_unlock;
	}

	gk20a_err(dev_from_gk20a(g), "Job on channel %d timed out",
		  ch->hw_chid);

	gk20a_debug_dump(g->dev);
	gk20a_gr_debug_dump(g->dev);

	/* Get failing engine data */
	engine_id = gk20a_fifo_get_failing_engine_data(g, &id, &is_tsg);

	if (!gk20a_fifo_is_valid_engine_id(g, engine_id)) {
		/* If no failing engine, abort the channels */
		if (gk20a_is_channel_marked_as_tsg(ch)) {
			struct tsg_gk20a *tsg = &g->fifo.tsg[ch->tsgid];

			gk20a_fifo_set_ctx_mmu_error_tsg(g, tsg);
			gk20a_fifo_abort_tsg(g, ch->tsgid, false);
		} else {
			gk20a_fifo_set_ctx_mmu_error_ch(g, ch);
			gk20a_channel_abort(ch, false);
		}
	} else {
		/* If failing engine, trigger recovery */
		failing_ch = gk20a_channel_get(&g->fifo.channel[id]);
		if (!failing_ch)
			goto fail_enable_ctxsw;

		if (failing_ch->hw_chid != ch->hw_chid) {
			gk20a_channel_timeout_start(ch);

			raw_spin_lock(&failing_ch->timeout.lock);
			failing_ch->timeout.initialized = false;
			raw_spin_unlock(&failing_ch->timeout.lock);
		}

		gk20a_fifo_recover(g, BIT(engine_id),
			failing_ch->hw_chid, is_tsg,
			true, failing_ch->timeout_debug_dump);

		gk20a_channel_put(failing_ch);
	}

fail_enable_ctxsw:
	gr_gk20a_enable_ctxsw(g);
fail_unlock:
	mutex_unlock(&g->ch_wdt_lock);
	gk20a_channel_put(ch);
	gk20a_idle(dev_from_gk20a(g));
}

int gk20a_free_priv_cmdbuf(struct channel_gk20a *c, struct priv_cmd_entry *e)
{
	struct priv_cmd_queue *q = &c->priv_cmd_q;
	struct device *d = dev_from_gk20a(c->g);

	if (!e)
		return 0;

	if (e->valid) {
		/* read the entry's valid flag before reading its contents */
		rmb();
		if ((q->get != e->off) && e->off != 0)
			gk20a_err(d, "requests out-of-order, ch=%d\n",
				  c->hw_chid);
		q->get = e->off + e->size;
	}

	free_priv_cmdbuf(c, e);

	return 0;
}

static void gk20a_channel_schedule_job_clean_up(struct channel_gk20a *c)
{
	mutex_lock(&c->clean_up.lock);

	if (c->clean_up.scheduled) {
		mutex_unlock(&c->clean_up.lock);
		return;
	}

	c->clean_up.scheduled = true;
	schedule_delayed_work(&c->clean_up.wq, 1);

	mutex_unlock(&c->clean_up.lock);
}

static void gk20a_channel_cancel_job_clean_up(struct channel_gk20a *c,
				bool wait_for_completion)
{
	if (wait_for_completion)
		cancel_delayed_work_sync(&c->clean_up.wq);

	mutex_lock(&c->clean_up.lock);
	c->clean_up.scheduled = false;
	mutex_unlock(&c->clean_up.lock);
}

static int gk20a_channel_add_job(struct channel_gk20a *c,
				 struct channel_gk20a_job *job,
				 bool skip_buffer_refcounting)
{
	struct vm_gk20a *vm = c->vm;
	struct mapped_buffer_node **mapped_buffers = NULL;
	int err = 0, num_mapped_buffers = 0;
	bool pre_alloc_enabled = channel_gk20a_is_prealloc_enabled(c);

	if (!skip_buffer_refcounting) {
		err = gk20a_vm_get_buffers(vm, &mapped_buffers,
					&num_mapped_buffers);
		if (err)
			return err;
	}

	/* put() is done in gk20a_channel_update() when the job is done */
	c = gk20a_channel_get(c);

	if (c) {
		job->num_mapped_buffers = num_mapped_buffers;
		job->mapped_buffers = mapped_buffers;

		gk20a_channel_timeout_start(c);

		if (!pre_alloc_enabled)
			channel_gk20a_joblist_lock(c);

		/*
		 * ensure all pending write complete before adding to the list.
		 * see corresponding rmb in gk20a_channel_clean_up_jobs() &
		 * gk20a_channel_abort_clean_up()
		 */
		wmb();
		channel_gk20a_joblist_add(c, job);

		if (!pre_alloc_enabled)
			channel_gk20a_joblist_unlock(c);
	} else {
		err = -ETIMEDOUT;
		goto err_put_buffers;
	}

	return 0;

err_put_buffers:
	gk20a_vm_put_buffers(vm, mapped_buffers, num_mapped_buffers);

	return err;
}

static void gk20a_channel_clean_up_runcb_fn(struct work_struct *work)
{
	struct channel_gk20a *c = container_of(to_delayed_work(work),
			struct channel_gk20a, clean_up.wq);

	gk20a_channel_clean_up_jobs(c, true);
}

static void gk20a_channel_clean_up_jobs(struct channel_gk20a *c,
					bool clean_all)
{
	struct vm_gk20a *vm;
	struct channel_gk20a_job *job;
	struct gk20a_platform *platform;
	struct gk20a *g;
	int job_finished = 0;

	c = gk20a_channel_get(c);
	if (!c)
		return;

	if (!c->g->power_on) { /* shutdown case */
		gk20a_channel_put(c);
		return;
	}

	vm = c->vm;
	g = c->g;
	platform = gk20a_get_platform(g->dev);

	gk20a_channel_cancel_job_clean_up(c, false);
	gk20a_channel_timeout_stop(c);

	while (1) {
		bool completed;

		channel_gk20a_joblist_lock(c);
		if (channel_gk20a_joblist_is_empty(c)) {
			channel_gk20a_joblist_unlock(c);
			break;
		}

		/*
		 * ensure that all subsequent reads occur after checking
		 * that we have a valid node. see corresponding wmb in
		 * gk20a_channel_add_job().
		 */
		rmb();
		job = channel_gk20a_joblist_peek(c);
		channel_gk20a_joblist_unlock(c);

		completed = gk20a_fence_is_expired(job->post_fence);
		if (!completed) {
			gk20a_channel_timeout_start(c);
			break;
		}

		gk20a_channel_timeout_stop(c);

		WARN_ON(!c->sync);

		if (c->sync) {
			c->sync->signal_timeline(c->sync);

			if (platform->aggressive_sync_destroy_thresh) {
				mutex_lock(&c->sync_lock);
				if (atomic_dec_and_test(&c->sync->refcount) &&
						platform->aggressive_sync_destroy) {
					gk20a_channel_sync_destroy(c->sync);
					c->sync = NULL;
				}
				mutex_unlock(&c->sync_lock);
			}
		}

		if (job->num_mapped_buffers)
			gk20a_vm_put_buffers(vm, job->mapped_buffers,
				job->num_mapped_buffers);

		/* Remove job from channel's job list before we close the
		 * fences, to prevent other callers (gk20a_channel_abort) from
		 * trying to dereference post_fence when it no longer exists.
		 */
		channel_gk20a_joblist_lock(c);
		channel_gk20a_joblist_delete(c, job);
		channel_gk20a_joblist_unlock(c);

		/* Close the fences (this will unref the semaphores and release
		 * them to the pool). */
		gk20a_fence_put(job->pre_fence);
		gk20a_fence_put(job->post_fence);

		/* Free the private command buffers (wait_cmd first and
		 * then incr_cmd i.e. order of allocation) */
		gk20a_free_priv_cmdbuf(c, job->wait_cmd);
		gk20a_free_priv_cmdbuf(c, job->incr_cmd);

		/* another bookkeeping taken in add_job. caller must hold a ref
		 * so this wouldn't get freed here. */
		gk20a_channel_put(c);

		/*
		 * ensure all pending writes complete before freeing up the job.
		 * see corresponding rmb in channel_gk20a_alloc_job().
		 */
		wmb();

		channel_gk20a_free_job(c, job);
		job_finished = 1;
		gk20a_idle(g->dev);

		if (!clean_all)
			break;
	}

	if (job_finished && c->update_fn)
		schedule_work(&c->update_fn_work);

	gk20a_channel_put(c);
}

void gk20a_channel_update(struct channel_gk20a *c, int nr_completed)
{
	c = gk20a_channel_get(c);
	if (!c)
		return;

	if (!c->g->power_on) { /* shutdown case */
		gk20a_channel_put(c);
		return;
	}

	trace_gk20a_channel_update(c->hw_chid);
	gk20a_channel_schedule_job_clean_up(c);

	gk20a_channel_put(c);
}

static void gk20a_submit_append_priv_cmdbuf(struct channel_gk20a *c,
		struct priv_cmd_entry *cmd)
{
	struct gk20a *g = c->g;
	struct mem_desc *gpfifo_mem = &c->gpfifo.mem;
	struct nvgpu_gpfifo x = {
		.entry0 = u64_lo32(cmd->gva),
		.entry1 = u64_hi32(cmd->gva) |
			pbdma_gp_entry1_length_f(cmd->size)
	};

	gk20a_mem_wr_n(g, gpfifo_mem, c->gpfifo.put * sizeof(x),
			&x, sizeof(x));

	if (cmd->mem->aperture == APERTURE_SYSMEM)
		trace_gk20a_push_cmdbuf(dev_name(g->dev), 0, cmd->size, 0,
				cmd->mem->cpu_va + cmd->off * sizeof(u32));

	c->gpfifo.put = (c->gpfifo.put + 1) & (c->gpfifo.entry_num - 1);
}

/*
 * Copy source gpfifo entries into the gpfifo ring buffer, potentially
 * splitting into two memcpys to handle wrap-around.
 */
static int gk20a_submit_append_gpfifo(struct channel_gk20a *c,
		struct nvgpu_gpfifo *kern_gpfifo,
		struct nvgpu_gpfifo __user *user_gpfifo,
		u32 num_entries)
{
	/* byte offsets */
	u32 gpfifo_size = c->gpfifo.entry_num * sizeof(struct nvgpu_gpfifo);
	u32 len = num_entries * sizeof(struct nvgpu_gpfifo);
	u32 start = c->gpfifo.put * sizeof(struct nvgpu_gpfifo);
	u32 end = start + len; /* exclusive */
	struct mem_desc *gpfifo_mem = &c->gpfifo.mem;
	struct nvgpu_gpfifo *cpu_src;
	int err;

	if (user_gpfifo && !c->gpfifo.pipe) {
		/*
		 * This path (from userspace to sysmem) is special in order to
		 * avoid two copies unnecessarily (from user to pipe, then from
		 * pipe to gpu sysmem buffer).
		 *
		 * As a special case, the pipe buffer exists if PRAMIN writes
		 * are forced, although the buffers may not be in vidmem in
		 * that case.
		 */
		if (end > gpfifo_size) {
			/* wrap-around */
			int length0 = gpfifo_size - start;
			int length1 = len - length0;
			void __user *user2 = (u8 __user *)user_gpfifo + length0;

			err = copy_from_user(gpfifo_mem->cpu_va + start,
					user_gpfifo, length0);
			if (err)
				return err;

			err = copy_from_user(gpfifo_mem->cpu_va,
					user2, length1);
			if (err)
				return err;
		} else {
			err = copy_from_user(gpfifo_mem->cpu_va + start,
					user_gpfifo, len);
			if (err)
				return err;
		}

		trace_write_pushbuffer_range(c, NULL, user_gpfifo,
				0, num_entries);
		goto out;
	} else if (user_gpfifo) {
		/* from userspace to vidmem or sysmem when pramin forced, use
		 * the common copy path below */
		err = copy_from_user(c->gpfifo.pipe, user_gpfifo, len);
		if (err)
			return err;

		cpu_src = c->gpfifo.pipe;
	} else {
		/* from kernel to either sysmem or vidmem, don't need
		 * copy_from_user so use the common path below */
		cpu_src = kern_gpfifo;
	}

	if (end > gpfifo_size) {
		/* wrap-around */
		int length0 = gpfifo_size - start;
		int length1 = len - length0;
		void *src2 = (u8 *)cpu_src + length0;

		gk20a_mem_wr_n(c->g, gpfifo_mem, start, cpu_src, length0);
		gk20a_mem_wr_n(c->g, gpfifo_mem, 0, src2, length1);
	} else {
		gk20a_mem_wr_n(c->g, gpfifo_mem, start, cpu_src, len);

	}

	trace_write_pushbuffer_range(c, cpu_src, NULL, 0, num_entries);

out:
	c->gpfifo.put = (c->gpfifo.put + num_entries) &
		(c->gpfifo.entry_num - 1);

	return 0;
}

/*
 * Handle the submit synchronization - pre-fences and post-fences.
 */
static int gk20a_submit_prepare_syncs(struct channel_gk20a *c,
				      struct nvgpu_fence *fence,
				      struct channel_gk20a_job *job,
				      struct priv_cmd_entry **wait_cmd,
				      struct priv_cmd_entry **incr_cmd,
				      struct gk20a_fence **pre_fence,
				      struct gk20a_fence **post_fence,
				      bool force_need_sync_fence,
				      bool register_irq,
				      u32 flags)
{
	struct gk20a *g = c->g;
	struct gk20a_platform *platform = gk20a_get_platform(g->dev);
	bool need_sync_fence = false;
	bool new_sync_created = false;
	int wait_fence_fd = -1;
	int err;
	bool need_wfi = !(flags & NVGPU_SUBMIT_GPFIFO_FLAGS_SUPPRESS_WFI);
	bool pre_alloc_enabled = channel_gk20a_is_prealloc_enabled(c);

	/*
	 * If user wants to always allocate sync_fence_fds then respect that;
	 * otherwise, allocate sync_fence_fd based on user flags.
	 */
	if (force_need_sync_fence)
		need_sync_fence = true;

	if (platform->aggressive_sync_destroy_thresh) {
		mutex_lock(&c->sync_lock);
		if (!c->sync) {
			c->sync = gk20a_channel_sync_create(c);
			if (!c->sync) {
				err = -ENOMEM;
				mutex_unlock(&c->sync_lock);
				goto fail;
			}
			new_sync_created = true;
		}
		atomic_inc(&c->sync->refcount);
		mutex_unlock(&c->sync_lock);
	}

	if (g->ops.fifo.resetup_ramfc && new_sync_created) {
		err = g->ops.fifo.resetup_ramfc(c);
		if (err)
			goto fail;
	}

	/*
	 * Optionally insert syncpt wait in the beginning of gpfifo submission
	 * when user requested and the wait hasn't expired. Validate that the id
	 * makes sense, elide if not. The only reason this isn't being
	 * unceremoniously killed is to keep running some tests which trigger
	 * this condition.
	 */
	if (flags & NVGPU_SUBMIT_GPFIFO_FLAGS_FENCE_WAIT) {
		job->pre_fence = gk20a_alloc_fence(c);
		if (!pre_alloc_enabled)
			job->wait_cmd = kzalloc(sizeof(struct priv_cmd_entry),
						GFP_KERNEL);

		if (!job->wait_cmd || !job->pre_fence) {
			err = -ENOMEM;
			goto clean_up_pre_fence;
		}

		if (flags & NVGPU_SUBMIT_GPFIFO_FLAGS_SYNC_FENCE) {
			wait_fence_fd = fence->id;
			err = c->sync->wait_fd(c->sync, wait_fence_fd,
					       job->wait_cmd, job->pre_fence);
		} else {
			err = c->sync->wait_syncpt(c->sync, fence->id,
						   fence->value, job->wait_cmd,
						   job->pre_fence);
		}

		if (!err) {
			if (job->wait_cmd->valid)
				*wait_cmd = job->wait_cmd;
			*pre_fence = job->pre_fence;
		} else
			goto clean_up_pre_fence;
	}

	if ((flags & NVGPU_SUBMIT_GPFIFO_FLAGS_FENCE_GET) &&
	    (flags & NVGPU_SUBMIT_GPFIFO_FLAGS_SYNC_FENCE))
		need_sync_fence = true;

	/*
	 * Always generate an increment at the end of a GPFIFO submission. This
	 * is used to keep track of method completion for idle railgating. The
	 * sync_pt/semaphore PB is added to the GPFIFO later on in submit.
	 */
	job->post_fence = gk20a_alloc_fence(c);
	if (!pre_alloc_enabled)
		job->incr_cmd = kzalloc(sizeof(struct priv_cmd_entry),
					GFP_KERNEL);

	if (!job->incr_cmd || !job->post_fence) {
		err = -ENOMEM;
		goto clean_up_post_fence;
	}

	if (flags & NVGPU_SUBMIT_GPFIFO_FLAGS_FENCE_GET)
		err = c->sync->incr_user(c->sync, wait_fence_fd, job->incr_cmd,
				 job->post_fence, need_wfi, need_sync_fence,
				 register_irq);
	else
		err = c->sync->incr(c->sync, job->incr_cmd,
				    job->post_fence, need_sync_fence,
				    register_irq);
	if (!err) {
		*incr_cmd = job->incr_cmd;
		*post_fence = job->post_fence;
	} else
		goto clean_up_post_fence;

	return 0;

clean_up_post_fence:
	gk20a_fence_put(job->post_fence);
	job->post_fence = NULL;
	free_priv_cmdbuf(c, job->incr_cmd);
	if (!pre_alloc_enabled)
		job->incr_cmd = NULL;
clean_up_pre_fence:
	gk20a_fence_put(job->pre_fence);
	job->pre_fence = NULL;
	free_priv_cmdbuf(c, job->wait_cmd);
	if (!pre_alloc_enabled)
		job->wait_cmd = NULL;
	*wait_cmd = NULL;
	*pre_fence = NULL;
fail:
	return err;
}

int gk20a_submit_channel_gpfifo(struct channel_gk20a *c,
				struct nvgpu_gpfifo *gpfifo,
				struct nvgpu_submit_gpfifo_args *args,
				u32 num_entries,
				u32 flags,
				struct nvgpu_fence *fence,
				struct gk20a_fence **fence_out,
				bool force_need_sync_fence)
{
	struct gk20a *g = c->g;
	struct device *d = dev_from_gk20a(g);
	struct priv_cmd_entry *wait_cmd = NULL;
	struct priv_cmd_entry *incr_cmd = NULL;
	struct gk20a_fence *pre_fence = NULL;
	struct gk20a_fence *post_fence = NULL;
	struct channel_gk20a_job *job = NULL;
	/* we might need two extra gpfifo entries - one for pre fence
	 * and one for post fence. */
	const int extra_entries = 2;
	bool skip_buffer_refcounting = (flags &
			NVGPU_SUBMIT_GPFIFO_FLAGS_SKIP_BUFFER_REFCOUNTING);
	int err = 0;
	bool need_job_tracking;
	bool need_deferred_cleanup = false;
	struct nvgpu_gpfifo __user *user_gpfifo = args ?
		(struct nvgpu_gpfifo __user *)(uintptr_t)args->gpfifo : NULL;
	struct gk20a_platform *platform = gk20a_get_platform(d);

	if (c->has_timedout)
		return -ETIMEDOUT;

	/* fifo not large enough for request. Return error immediately.
	 * Kernel can insert gpfifo entries before and after user gpfifos.
	 * So, add extra_entries in user request. Also, HW with fifo size N
	 * can accept only N-1 entreis and so the below condition */
	if (c->gpfifo.entry_num - 1 < num_entries + extra_entries) {
		gk20a_err(d, "not enough gpfifo space allocated");
		return -ENOMEM;
	}

	if (!gpfifo && !args)
		return -EINVAL;

	if ((flags & (NVGPU_SUBMIT_GPFIFO_FLAGS_FENCE_WAIT |
		      NVGPU_SUBMIT_GPFIFO_FLAGS_FENCE_GET)) &&
	    !fence)
		return -EINVAL;

	/* an address space needs to have been bound at this point. */
	if (!gk20a_channel_as_bound(c)) {
		gk20a_err(d,
			    "not bound to an address space at time of gpfifo"
			    " submission.");
		return -EINVAL;
	}

#ifdef CONFIG_DEBUG_FS
	/* update debug settings */
	if (g->ops.ltc.sync_debugfs)
		g->ops.ltc.sync_debugfs(g);
#endif

	gk20a_dbg_info("channel %d", c->hw_chid);

	/*
	 * Job tracking is necessary for any of the following conditions:
	 *  - pre- or post-fence functionality
	 *  - channel wdt
	 *  - GPU rail-gating
	 *  - buffer refcounting
	 *
	 * If none of the conditions are met, then job tracking is not
	 * required and a fast submit can be done (ie. only need to write
	 * out userspace GPFIFO entries and update GP_PUT).
	 */
	need_job_tracking = (flags & NVGPU_SUBMIT_GPFIFO_FLAGS_FENCE_WAIT) ||
			(flags & NVGPU_SUBMIT_GPFIFO_FLAGS_FENCE_GET) ||
			c->wdt_enabled ||
			platform->can_railgate ||
			!skip_buffer_refcounting;

	if (need_job_tracking) {
		bool need_sync_framework = false;

		/*
		 * If the channel is to have deterministic latency and
		 * job tracking is required, the channel must have
		 * pre-allocated resources. Otherwise, we fail the submit here
		 */
		if (c->deterministic && !channel_gk20a_is_prealloc_enabled(c))
			return -EINVAL;

		need_sync_framework = force_need_sync_fence ||
			gk20a_channel_sync_needs_sync_framework(c) ||
			(flags & NVGPU_SUBMIT_GPFIFO_FLAGS_SYNC_FENCE &&
			(flags & NVGPU_SUBMIT_GPFIFO_FLAGS_FENCE_WAIT ||
			 flags & NVGPU_SUBMIT_GPFIFO_FLAGS_FENCE_GET));

		/*
		 * Deferred clean-up is necessary for any of the following
		 * conditions:
		 * - channel's deterministic flag is not set
		 * - dependency on sync framework, which could make the
		 *   behavior of the clean-up operation non-deterministic
		 *   (should not be performed in the submit path)
		 * - channel wdt
		 * - GPU rail-gating
		 * - buffer refcounting
		 *
		 * If none of the conditions are met, then deferred clean-up
		 * is not required, and we clean-up one job-tracking
		 * resource in the submit path.
		 */
		need_deferred_cleanup = !c->deterministic ||
					need_sync_framework ||
					c->wdt_enabled ||
					platform->can_railgate ||
					!skip_buffer_refcounting;

		/*
		 * For deterministic channels, we don't allow deferred clean_up
		 * processing to occur. In cases we hit this, we fail the submit
		 */
		if (c->deterministic && need_deferred_cleanup)
			return -EINVAL;

		/* gk20a_channel_update releases this ref. */
		err = gk20a_busy(g->dev);
		if (err) {
			gk20a_err(d, "failed to host gk20a to submit gpfifo");
			return err;
		}

		if (!need_deferred_cleanup) {
			/* clean up a single job */
			gk20a_channel_clean_up_jobs(c, false);
		}
	}

	trace_gk20a_channel_submit_gpfifo(dev_name(c->g->dev),
					  c->hw_chid,
					  num_entries,
					  flags,
					  fence ? fence->id : 0,
					  fence ? fence->value : 0);

	gk20a_dbg_info("pre-submit put %d, get %d, size %d",
		c->gpfifo.put, c->gpfifo.get, c->gpfifo.entry_num);

	/*
	 * Make sure we have enough space for gpfifo entries. Check cached
	 * values first and then read from HW. If no space, return EAGAIN
	 * and let userpace decide to re-try request or not.
	 */
	if (gp_free_count(c) < num_entries + extra_entries) {
		if (get_gp_free_count(c) < num_entries + extra_entries) {
			err = -EAGAIN;
			goto clean_up;
		}
	}

	if (c->has_timedout) {
		err = -ETIMEDOUT;
		goto clean_up;
	}

	if (need_job_tracking) {
		err = channel_gk20a_alloc_job(c, &job);
		if (err)
			goto clean_up;

		err = gk20a_submit_prepare_syncs(c, fence, job,
						 &wait_cmd, &incr_cmd,
						 &pre_fence, &post_fence,
						 force_need_sync_fence,
						 need_deferred_cleanup,
						 flags);
		if (err)
			goto clean_up_job;
	}

	if (wait_cmd)
		gk20a_submit_append_priv_cmdbuf(c, wait_cmd);

	if (gpfifo || user_gpfifo)
		err = gk20a_submit_append_gpfifo(c, gpfifo, user_gpfifo,
				num_entries);
	if (err)
		goto clean_up_job;

	/*
	 * And here's where we add the incr_cmd we generated earlier. It should
	 * always run!
	 */
	if (incr_cmd)
		gk20a_submit_append_priv_cmdbuf(c, incr_cmd);

	if (fence_out)
		*fence_out = gk20a_fence_get(post_fence);

	if (need_job_tracking)
		/* TODO! Check for errors... */
		gk20a_channel_add_job(c, job, skip_buffer_refcounting);

	gk20a_bar1_writel(g,
		c->userd_gpu_va + 4 * ram_userd_gp_put_w(),
		c->gpfifo.put);

	trace_gk20a_channel_submitted_gpfifo(dev_name(c->g->dev),
				c->hw_chid,
				num_entries,
				flags,
				post_fence ? post_fence->syncpt_id : 0,
				post_fence ? post_fence->syncpt_value : 0);

	gk20a_dbg_info("post-submit put %d, get %d, size %d",
		c->gpfifo.put, c->gpfifo.get, c->gpfifo.entry_num);

	gk20a_dbg_fn("done");
	return err;

clean_up_job:
	channel_gk20a_free_job(c, job);
clean_up:
	gk20a_dbg_fn("fail");
	gk20a_fence_put(pre_fence);
	gk20a_fence_put(post_fence);
	if (need_deferred_cleanup)
		gk20a_idle(g->dev);
	return err;
}

int gk20a_init_channel_support(struct gk20a *g, u32 chid)
{
	struct channel_gk20a *c = g->fifo.channel+chid;
	c->g = NULL;
	c->hw_chid = chid;
	atomic_set(&c->bound, false);
	raw_spin_lock_init(&c->ref_obtain_lock);
	atomic_set(&c->ref_count, 0);
	c->referenceable = false;
	init_waitqueue_head(&c->ref_count_dec_wq);
	mutex_init(&c->ioctl_lock);
	mutex_init(&c->joblist.lock);
	mutex_init(&c->error_notifier_mutex);
	raw_spin_lock_init(&c->timeout.lock);
	mutex_init(&c->sync_lock);
	INIT_DELAYED_WORK(&c->timeout.wq, gk20a_channel_timeout_handler);
	INIT_DELAYED_WORK(&c->clean_up.wq, gk20a_channel_clean_up_runcb_fn);
	mutex_init(&c->clean_up.lock);
	INIT_LIST_HEAD(&c->joblist.dynamic.jobs);
#if defined(CONFIG_GK20A_CYCLE_STATS)
	mutex_init(&c->cyclestate.cyclestate_buffer_mutex);
	mutex_init(&c->cs_client_mutex);
#endif
	INIT_LIST_HEAD(&c->dbg_s_list);
	INIT_LIST_HEAD(&c->event_id_list);
	mutex_init(&c->event_id_list_lock);
	mutex_init(&c->dbg_s_lock);
	list_add(&c->free_chs, &g->fifo.free_chs);

	return 0;
}

static int gk20a_channel_wait_semaphore(struct channel_gk20a *ch,
					ulong id, u32 offset,
					u32 payload, long timeout)
{
	struct device *dev = ch->g->dev;
	struct dma_buf *dmabuf;
	void *data;
	u32 *semaphore;
	int ret = 0;
	long remain;

	/* do not wait if channel has timed out */
	if (ch->has_timedout)
		return -ETIMEDOUT;

	dmabuf = dma_buf_get(id);
	if (IS_ERR(dmabuf)) {
		gk20a_err(dev, "invalid notifier nvmap handle 0x%lx", id);
		return -EINVAL;
	}

	data = dma_buf_kmap(dmabuf, offset >> PAGE_SHIFT);
	if (!data) {
		gk20a_err(dev, "failed to map notifier memory");
		ret = -EINVAL;
		goto cleanup_put;
	}

	semaphore = data + (offset & ~PAGE_MASK);

	remain = wait_event_interruptible_timeout(
			ch->semaphore_wq,
			*semaphore == payload || ch->has_timedout,
			timeout);

	if (remain == 0 && *semaphore != payload)
		ret = -ETIMEDOUT;
	else if (remain < 0)
		ret = remain;

	dma_buf_kunmap(dmabuf, offset >> PAGE_SHIFT, data);
cleanup_put:
	dma_buf_put(dmabuf);
	return ret;
}

static int gk20a_channel_wait(struct channel_gk20a *ch,
			      struct nvgpu_wait_args *args)
{
	struct device *d = dev_from_gk20a(ch->g);
	struct dma_buf *dmabuf;
	struct notification *notif;
	struct timespec tv;
	u64 jiffies;
	ulong id;
	u32 offset;
	unsigned long timeout;
	int remain, ret = 0;
	u64 end;

	gk20a_dbg_fn("");

	if (ch->has_timedout)
		return -ETIMEDOUT;

	if (args->timeout == NVGPU_NO_TIMEOUT)
		timeout = MAX_SCHEDULE_TIMEOUT;
	else
		timeout = (u32)msecs_to_jiffies(args->timeout);

	switch (args->type) {
	case NVGPU_WAIT_TYPE_NOTIFIER:
		id = args->condition.notifier.dmabuf_fd;
		offset = args->condition.notifier.offset;
		end = offset + sizeof(struct notification);

		dmabuf = dma_buf_get(id);
		if (IS_ERR(dmabuf)) {
			gk20a_err(d, "invalid notifier nvmap handle 0x%lx",
				   id);
			return -EINVAL;
		}

		if (end > dmabuf->size || end < sizeof(struct notification)) {
			dma_buf_put(dmabuf);
			gk20a_err(d, "invalid notifier offset\n");
			return -EINVAL;
		}

		notif = dma_buf_vmap(dmabuf);
		if (!notif) {
			gk20a_err(d, "failed to map notifier memory");
			return -ENOMEM;
		}

		notif = (struct notification *)((uintptr_t)notif + offset);

		/* user should set status pending before
		 * calling this ioctl */
		remain = wait_event_interruptible_timeout(
				ch->notifier_wq,
				notif->status == 0 || ch->has_timedout,
				timeout);

		if (remain == 0 && notif->status != 0) {
			ret = -ETIMEDOUT;
			goto notif_clean_up;
		} else if (remain < 0) {
			ret = -EINTR;
			goto notif_clean_up;
		}

		/* TBD: fill in correct information */
		jiffies = get_jiffies_64();
		jiffies_to_timespec(jiffies, &tv);
		notif->timestamp.nanoseconds[0] = tv.tv_nsec;
		notif->timestamp.nanoseconds[1] = tv.tv_sec;
		notif->info32 = 0xDEADBEEF; /* should be object name */
		notif->info16 = ch->hw_chid; /* should be method offset */

notif_clean_up:
		dma_buf_vunmap(dmabuf, notif);
		return ret;

	case NVGPU_WAIT_TYPE_SEMAPHORE:
		ret = gk20a_channel_wait_semaphore(ch,
				args->condition.semaphore.dmabuf_fd,
				args->condition.semaphore.offset,
				args->condition.semaphore.payload,
				timeout);

		break;

	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static unsigned int gk20a_event_id_poll(struct file *filep, poll_table *wait)
{
	unsigned int mask = 0;
	struct gk20a_event_id_data *event_id_data = filep->private_data;
	struct gk20a *g = event_id_data->g;
	u32 event_id = event_id_data->event_id;

	gk20a_dbg(gpu_dbg_fn | gpu_dbg_info, "");

	poll_wait(filep, &event_id_data->event_id_wq, wait);

	mutex_lock(&event_id_data->lock);

	if (event_id_data->is_tsg) {
		struct tsg_gk20a *tsg = g->fifo.tsg + event_id_data->id;

		if (event_id_data->event_posted) {
			gk20a_dbg_info(
				"found pending event_id=%d on TSG=%d\n",
				event_id, tsg->tsgid);
			mask = (POLLPRI | POLLIN);
			event_id_data->event_posted = false;
		}
	} else {
		struct channel_gk20a *ch = g->fifo.channel
					   + event_id_data->id;

		if (event_id_data->event_posted) {
			gk20a_dbg_info(
				"found pending event_id=%d on chid=%d\n",
				event_id, ch->hw_chid);
			mask = (POLLPRI | POLLIN);
			event_id_data->event_posted = false;
		}
	}

	mutex_unlock(&event_id_data->lock);

	return mask;
}

static int gk20a_event_id_release(struct inode *inode, struct file *filp)
{
	struct gk20a_event_id_data *event_id_data = filp->private_data;
	struct gk20a *g = event_id_data->g;

	if (event_id_data->is_tsg) {
		struct tsg_gk20a *tsg = g->fifo.tsg + event_id_data->id;

		mutex_lock(&tsg->event_id_list_lock);
		list_del_init(&event_id_data->event_id_node);
		mutex_unlock(&tsg->event_id_list_lock);
	} else {
		struct channel_gk20a *ch = g->fifo.channel + event_id_data->id;

		mutex_lock(&ch->event_id_list_lock);
		list_del_init(&event_id_data->event_id_node);
		mutex_unlock(&ch->event_id_list_lock);
	}

	kfree(event_id_data);
	filp->private_data = NULL;

	return 0;
}

const struct file_operations gk20a_event_id_ops = {
	.owner = THIS_MODULE,
	.poll = gk20a_event_id_poll,
	.release = gk20a_event_id_release,
};

static int gk20a_channel_get_event_data_from_id(struct channel_gk20a *ch,
				int event_id,
				struct gk20a_event_id_data **event_id_data)
{
	struct gk20a_event_id_data *local_event_id_data;
	bool event_found = false;

	mutex_lock(&ch->event_id_list_lock);
	list_for_each_entry(local_event_id_data, &ch->event_id_list,
						 event_id_node) {
		if (local_event_id_data->event_id == event_id) {
			event_found = true;
			break;
		}
	}
	mutex_unlock(&ch->event_id_list_lock);

	if (event_found) {
		*event_id_data = local_event_id_data;
		return 0;
	} else {
		return -1;
	}
}

void gk20a_channel_event_id_post_event(struct channel_gk20a *ch,
				       int event_id)
{
	struct gk20a_event_id_data *event_id_data;
	int err = 0;

	err = gk20a_channel_get_event_data_from_id(ch, event_id,
						&event_id_data);
	if (err)
		return;

	mutex_lock(&event_id_data->lock);

	gk20a_dbg_info(
		"posting event for event_id=%d on ch=%d\n",
		event_id, ch->hw_chid);
	event_id_data->event_posted = true;

	wake_up_interruptible_all(&event_id_data->event_id_wq);

	mutex_unlock(&event_id_data->lock);
}

static int gk20a_channel_event_id_enable(struct channel_gk20a *ch,
					 int event_id,
					 int *fd)
{
	int err = 0;
	int local_fd;
	struct file *file;
	char *name;
	struct gk20a_event_id_data *event_id_data;

	err = gk20a_channel_get_event_data_from_id(ch,
				event_id, &event_id_data);
	if (err == 0) /* We already have event enabled */
		return -EINVAL;

	err = get_unused_fd_flags(O_RDWR);
	if (err < 0)
		return err;
	local_fd = err;

	name = kasprintf(GFP_KERNEL, "nvgpu-event%d-fd%d",
			event_id, local_fd);

	file = anon_inode_getfile(name, &gk20a_event_id_ops,
				  NULL, O_RDWR);
	kfree(name);
	if (IS_ERR(file)) {
		err = PTR_ERR(file);
		goto clean_up;
	}

	event_id_data = kzalloc(sizeof(*event_id_data), GFP_KERNEL);
	if (!event_id_data) {
		err = -ENOMEM;
		goto clean_up_file;
	}
	event_id_data->g = ch->g;
	event_id_data->id = ch->hw_chid;
	event_id_data->is_tsg = false;
	event_id_data->event_id = event_id;

	init_waitqueue_head(&event_id_data->event_id_wq);
	mutex_init(&event_id_data->lock);
	INIT_LIST_HEAD(&event_id_data->event_id_node);

	mutex_lock(&ch->event_id_list_lock);
	list_add_tail(&event_id_data->event_id_node, &ch->event_id_list);
	mutex_unlock(&ch->event_id_list_lock);

	fd_install(local_fd, file);
	file->private_data = event_id_data;

	*fd = local_fd;

	return 0;

clean_up_file:
	fput(file);
clean_up:
	put_unused_fd(local_fd);
	return err;
}

static int gk20a_channel_event_id_ctrl(struct channel_gk20a *ch,
		struct nvgpu_event_id_ctrl_args *args)
{
	int err = 0;
	int fd = -1;

	if (args->event_id < 0 ||
	    args->event_id >= NVGPU_IOCTL_CHANNEL_EVENT_ID_MAX)
		return -EINVAL;

	if (gk20a_is_channel_marked_as_tsg(ch))
		return -EINVAL;

	switch (args->cmd) {
	case NVGPU_IOCTL_CHANNEL_EVENT_ID_CMD_ENABLE:
		err = gk20a_channel_event_id_enable(ch, args->event_id, &fd);
		if (!err)
			args->event_fd = fd;
		break;

	default:
		gk20a_err(dev_from_gk20a(ch->g),
			   "unrecognized channel event id cmd: 0x%x",
			   args->cmd);
		err = -EINVAL;
		break;
	}

	return err;
}

int gk20a_channel_set_priority(struct channel_gk20a *ch, u32 priority)
{
	if (gk20a_is_channel_marked_as_tsg(ch)) {
		gk20a_err(dev_from_gk20a(ch->g),
			"invalid operation for TSG!\n");
		return -EINVAL;
	}

	/* set priority of graphics channel */
	switch (priority) {
	case NVGPU_PRIORITY_LOW:
		ch->timeslice_us = ch->g->timeslice_low_priority_us;
		break;
	case NVGPU_PRIORITY_MEDIUM:
		ch->timeslice_us = ch->g->timeslice_medium_priority_us;
		break;
	case NVGPU_PRIORITY_HIGH:
		ch->timeslice_us = ch->g->timeslice_high_priority_us;
		break;
	default:
		pr_err("Unsupported priority");
		return -EINVAL;
	}

	return channel_gk20a_set_schedule_params(ch);
}

int gk20a_channel_set_timeslice(struct channel_gk20a *ch, u32 timeslice)
{
	if (gk20a_is_channel_marked_as_tsg(ch)) {
		gk20a_err(dev_from_gk20a(ch->g),
			"invalid operation for TSG!\n");
		return -EINVAL;
	}

	if (timeslice < NVGPU_CHANNEL_MIN_TIMESLICE_US ||
		timeslice > NVGPU_CHANNEL_MAX_TIMESLICE_US)
		return -EINVAL;

	ch->timeslice_us = timeslice;

	return channel_gk20a_set_schedule_params(ch);
}

static int gk20a_channel_zcull_bind(struct channel_gk20a *ch,
			    struct nvgpu_zcull_bind_args *args)
{
	struct gk20a *g = ch->g;
	struct gr_gk20a *gr = &g->gr;

	gk20a_dbg_fn("");

	return g->ops.gr.bind_ctxsw_zcull(g, gr, ch,
				args->gpu_va, args->mode);
}

/* in this context the "channel" is the host1x channel which
 * maps to *all* gk20a channels */
int gk20a_channel_suspend(struct gk20a *g)
{
	struct fifo_gk20a *f = &g->fifo;
	u32 chid;
	bool channels_in_use = false;
	u32 active_runlist_ids = 0;

	gk20a_dbg_fn("");

	for (chid = 0; chid < f->num_channels; chid++) {
		struct channel_gk20a *ch = &f->channel[chid];
		if (gk20a_channel_get(ch)) {
			gk20a_dbg_info("suspend channel %d", chid);
			/* disable channel */
			gk20a_disable_channel_tsg(g, ch);
			/* preempt the channel */
			gk20a_fifo_preempt(g, ch);
			gk20a_channel_timeout_stop(ch);
			gk20a_channel_cancel_job_clean_up(ch, true);
			/* wait for channel update notifiers */
			if (ch->update_fn)
				cancel_work_sync(&ch->update_fn_work);

			channels_in_use = true;

			active_runlist_ids |= BIT(ch->runlist_id);

			gk20a_channel_put(ch);
		}
	}

	if (channels_in_use) {
		gk20a_fifo_update_runlist_ids(g, active_runlist_ids, ~0, false, true);

		for (chid = 0; chid < f->num_channels; chid++) {
			if (gk20a_channel_get(&f->channel[chid])) {
				g->ops.fifo.unbind_channel(&f->channel[chid]);
				gk20a_channel_put(&f->channel[chid]);
			}
		}
	}

	gk20a_dbg_fn("done");
	return 0;
}

int gk20a_channel_resume(struct gk20a *g)
{
	struct fifo_gk20a *f = &g->fifo;
	u32 chid;
	bool channels_in_use = false;
	u32 active_runlist_ids = 0;

	gk20a_dbg_fn("");

	for (chid = 0; chid < f->num_channels; chid++) {
		if (gk20a_channel_get(&f->channel[chid])) {
			gk20a_dbg_info("resume channel %d", chid);
			g->ops.fifo.bind_channel(&f->channel[chid]);
			channels_in_use = true;
			active_runlist_ids |= BIT(f->channel[chid].runlist_id);
			gk20a_channel_put(&f->channel[chid]);
		}
	}

	if (channels_in_use)
		gk20a_fifo_update_runlist_ids(g, active_runlist_ids, ~0, true, true);

	gk20a_dbg_fn("done");
	return 0;
}

void gk20a_channel_semaphore_wakeup(struct gk20a *g, bool post_events)
{
	struct fifo_gk20a *f = &g->fifo;
	u32 chid;

	gk20a_dbg_fn("");

	/*
	 * Ensure that all pending writes are actually done  before trying to
	 * read semaphore values from DRAM.
	 */
	g->ops.mm.fb_flush(g);

	for (chid = 0; chid < f->num_channels; chid++) {
		struct channel_gk20a *c = g->fifo.channel+chid;
		if (gk20a_channel_get(c)) {
			if (atomic_read(&c->bound)) {
				wake_up_interruptible_all(&c->semaphore_wq);
				if (post_events) {
					if (gk20a_is_channel_marked_as_tsg(c)) {
						struct tsg_gk20a *tsg =
							&g->fifo.tsg[c->tsgid];

						gk20a_tsg_event_id_post_event(tsg,
						    NVGPU_IOCTL_CHANNEL_EVENT_ID_BLOCKING_SYNC);
					} else {
						gk20a_channel_event_id_post_event(c,
						    NVGPU_IOCTL_CHANNEL_EVENT_ID_BLOCKING_SYNC);
					}
				}
				/*
				 * Only non-deterministic channels get the
				 * channel_update callback. We don't allow
				 * semaphore-backed syncs for these channels
				 * anyways, since they have a dependency on
				 * the sync framework.
				 * If deterministic channels are receiving a
				 * semaphore wakeup, it must be for a
				 * user-space managed
				 * semaphore.
				 */
				if (!c->deterministic)
					gk20a_channel_update(c, 0);
			}
			gk20a_channel_put(c);
		}
	}
}

static int gk20a_ioctl_channel_submit_gpfifo(
	struct channel_gk20a *ch,
	struct nvgpu_submit_gpfifo_args *args)
{
	struct gk20a_fence *fence_out;
	int ret = 0;
	gk20a_dbg_fn("");

	if (ch->has_timedout)
		return -ETIMEDOUT;

	ret = gk20a_submit_channel_gpfifo(ch, NULL, args, args->num_entries,
					  args->flags, &args->fence,
					  &fence_out, false);

	if (ret)
		goto clean_up;

	/* Convert fence_out to something we can pass back to user space. */
	if (args->flags & NVGPU_SUBMIT_GPFIFO_FLAGS_FENCE_GET) {
		if (args->flags & NVGPU_SUBMIT_GPFIFO_FLAGS_SYNC_FENCE) {
			int fd = gk20a_fence_install_fd(fence_out);
			if (fd < 0)
				ret = fd;
			else
				args->fence.id = fd;
		} else {
			args->fence.id = fence_out->syncpt_id;
			args->fence.value = fence_out->syncpt_value;
		}
	}
	gk20a_fence_put(fence_out);

clean_up:
	return ret;
}

void gk20a_init_channel(struct gpu_ops *gops)
{
	gops->fifo.bind_channel = channel_gk20a_bind;
	gops->fifo.unbind_channel = channel_gk20a_unbind;
	gops->fifo.disable_channel = channel_gk20a_disable;
	gops->fifo.enable_channel = channel_gk20a_enable;
	gops->fifo.alloc_inst = channel_gk20a_alloc_inst;
	gops->fifo.free_inst = channel_gk20a_free_inst;
	gops->fifo.setup_ramfc = channel_gk20a_setup_ramfc;
	gops->fifo.channel_set_priority = gk20a_channel_set_priority;
	gops->fifo.channel_set_timeslice = gk20a_channel_set_timeslice;
}

long gk20a_channel_ioctl(struct file *filp,
	unsigned int cmd, unsigned long arg)
{
	struct channel_priv *priv = filp->private_data;
	struct channel_gk20a *ch = priv->c;
	struct device *dev = ch->g->dev;
	u8 buf[NVGPU_IOCTL_CHANNEL_MAX_ARG_SIZE] = {0};
	int err = 0;

	gk20a_dbg_fn("start %d", _IOC_NR(cmd));

	if ((_IOC_TYPE(cmd) != NVGPU_IOCTL_MAGIC) ||
		(_IOC_NR(cmd) == 0) ||
		(_IOC_NR(cmd) > NVGPU_IOCTL_CHANNEL_LAST) ||
		(_IOC_SIZE(cmd) > NVGPU_IOCTL_CHANNEL_MAX_ARG_SIZE))
		return -EINVAL;

	if (_IOC_DIR(cmd) & _IOC_WRITE) {
		if (copy_from_user(buf, (void __user *)arg, _IOC_SIZE(cmd)))
			return -EFAULT;
	}

	/* take a ref or return timeout if channel refs can't be taken */
	ch = gk20a_channel_get(ch);
	if (!ch)
		return -ETIMEDOUT;

	/* protect our sanity for threaded userspace - most of the channel is
	 * not thread safe */
	mutex_lock(&ch->ioctl_lock);

	/* this ioctl call keeps a ref to the file which keeps a ref to the
	 * channel */

	switch (cmd) {
	case NVGPU_IOCTL_CHANNEL_OPEN:
		err = gk20a_channel_open_ioctl(ch->g,
			(struct nvgpu_channel_open_args *)buf);
		break;
	case NVGPU_IOCTL_CHANNEL_SET_NVMAP_FD:
		break;
	case NVGPU_IOCTL_CHANNEL_ALLOC_OBJ_CTX:
		err = gk20a_busy(dev);
		if (err) {
			dev_err(dev,
				"%s: failed to host gk20a for ioctl cmd: 0x%x",
				__func__, cmd);
			break;
		}
		err = ch->g->ops.gr.alloc_obj_ctx(ch,
				(struct nvgpu_alloc_obj_ctx_args *)buf);
		gk20a_idle(dev);
		break;
	case NVGPU_IOCTL_CHANNEL_ALLOC_GPFIFO_EX:
	{
		struct nvgpu_alloc_gpfifo_ex_args *alloc_gpfifo_ex_args =
			(struct nvgpu_alloc_gpfifo_ex_args *)buf;

		err = gk20a_busy(dev);
		if (err) {
			dev_err(dev,
				"%s: failed to host gk20a for ioctl cmd: 0x%x",
				__func__, cmd);
			break;
		}

		if (!is_power_of_2(alloc_gpfifo_ex_args->num_entries)) {
			err = -EINVAL;
			gk20a_idle(dev);
			break;
		}
		err = gk20a_alloc_channel_gpfifo(ch, alloc_gpfifo_ex_args);
		gk20a_idle(dev);
		break;
	}
	case NVGPU_IOCTL_CHANNEL_ALLOC_GPFIFO:
	{
		struct nvgpu_alloc_gpfifo_ex_args alloc_gpfifo_ex_args;
		struct nvgpu_alloc_gpfifo_args *alloc_gpfifo_args =
			(struct nvgpu_alloc_gpfifo_args *)buf;

		err = gk20a_busy(dev);
		if (err) {
			dev_err(dev,
				"%s: failed to host gk20a for ioctl cmd: 0x%x",
				__func__, cmd);
			break;
		}

		/* prepare new args structure */
		memset(&alloc_gpfifo_ex_args, 0,
				sizeof(struct nvgpu_alloc_gpfifo_ex_args));
		/*
		 * Kernel can insert one extra gpfifo entry before user
		 * submitted gpfifos and another one after, for internal usage.
		 * Triple the requested size.
		 */
		alloc_gpfifo_ex_args.num_entries = roundup_pow_of_two(
				alloc_gpfifo_args->num_entries * 3);
		alloc_gpfifo_ex_args.flags = alloc_gpfifo_args->flags;

		err = gk20a_alloc_channel_gpfifo(ch, &alloc_gpfifo_ex_args);
		gk20a_idle(dev);
		break;
	}
	case NVGPU_IOCTL_CHANNEL_SUBMIT_GPFIFO:
		err = gk20a_ioctl_channel_submit_gpfifo(ch,
				(struct nvgpu_submit_gpfifo_args *)buf);
		break;
	case NVGPU_IOCTL_CHANNEL_WAIT:
		err = gk20a_busy(dev);
		if (err) {
			dev_err(dev,
				"%s: failed to host gk20a for ioctl cmd: 0x%x",
				__func__, cmd);
			break;
		}

		/* waiting is thread-safe, not dropping this mutex could
		 * deadlock in certain conditions */
		mutex_unlock(&ch->ioctl_lock);

		err = gk20a_channel_wait(ch,
				(struct nvgpu_wait_args *)buf);

		mutex_lock(&ch->ioctl_lock);

		gk20a_idle(dev);
		break;
	case NVGPU_IOCTL_CHANNEL_ZCULL_BIND:
		err = gk20a_busy(dev);
		if (err) {
			dev_err(dev,
				"%s: failed to host gk20a for ioctl cmd: 0x%x",
				__func__, cmd);
			break;
		}
		err = gk20a_channel_zcull_bind(ch,
				(struct nvgpu_zcull_bind_args *)buf);
		gk20a_idle(dev);
		break;
	case NVGPU_IOCTL_CHANNEL_SET_ERROR_NOTIFIER:
		err = gk20a_busy(dev);
		if (err) {
			dev_err(dev,
				"%s: failed to host gk20a for ioctl cmd: 0x%x",
				__func__, cmd);
			break;
		}
		err = gk20a_init_error_notifier(ch,
				(struct nvgpu_set_error_notifier *)buf);
		gk20a_idle(dev);
		break;
#ifdef CONFIG_GK20A_CYCLE_STATS
	case NVGPU_IOCTL_CHANNEL_CYCLE_STATS:
		err = gk20a_busy(dev);
		if (err) {
			dev_err(dev,
				"%s: failed to host gk20a for ioctl cmd: 0x%x",
				__func__, cmd);
			break;
		}
		err = gk20a_channel_cycle_stats(ch,
				(struct nvgpu_cycle_stats_args *)buf);
		gk20a_idle(dev);
		break;
#endif
	case NVGPU_IOCTL_CHANNEL_SET_TIMEOUT:
	{
		u32 timeout =
			(u32)((struct nvgpu_set_timeout_args *)buf)->timeout;
		gk20a_dbg(gpu_dbg_gpu_dbg, "setting timeout (%d ms) for chid %d",
			   timeout, ch->hw_chid);
		ch->timeout_ms_max = timeout;
		gk20a_channel_trace_sched_param(
			trace_gk20a_channel_set_timeout, ch);
		break;
	}
	case NVGPU_IOCTL_CHANNEL_SET_TIMEOUT_EX:
	{
		u32 timeout =
			(u32)((struct nvgpu_set_timeout_args *)buf)->timeout;
		bool timeout_debug_dump = !((u32)
			((struct nvgpu_set_timeout_ex_args *)buf)->flags &
			(1 << NVGPU_TIMEOUT_FLAG_DISABLE_DUMP));
		gk20a_dbg(gpu_dbg_gpu_dbg, "setting timeout (%d ms) for chid %d",
			   timeout, ch->hw_chid);
		ch->timeout_ms_max = timeout;
		ch->timeout_debug_dump = timeout_debug_dump;
		gk20a_channel_trace_sched_param(
			trace_gk20a_channel_set_timeout, ch);
		break;
	}
	case NVGPU_IOCTL_CHANNEL_GET_TIMEDOUT:
		((struct nvgpu_get_param_args *)buf)->value =
			ch->has_timedout;
		break;
	case NVGPU_IOCTL_CHANNEL_SET_PRIORITY:
		err = gk20a_busy(dev);
		if (err) {
			dev_err(dev,
				"%s: failed to host gk20a for ioctl cmd: 0x%x",
				__func__, cmd);
			break;
		}
		err = ch->g->ops.fifo.channel_set_priority(ch,
			((struct nvgpu_set_priority_args *)buf)->priority);

		gk20a_idle(dev);
		gk20a_channel_trace_sched_param(
			trace_gk20a_channel_set_priority, ch);
		break;
	case NVGPU_IOCTL_CHANNEL_ENABLE:
		err = gk20a_busy(dev);
		if (err) {
			dev_err(dev,
				"%s: failed to host gk20a for ioctl cmd: 0x%x",
				__func__, cmd);
			break;
		}
		if (ch->g->ops.fifo.enable_channel)
			ch->g->ops.fifo.enable_channel(ch);
		else
			err = -ENOSYS;
		gk20a_idle(dev);
		break;
	case NVGPU_IOCTL_CHANNEL_DISABLE:
		err = gk20a_busy(dev);
		if (err) {
			dev_err(dev,
				"%s: failed to host gk20a for ioctl cmd: 0x%x",
				__func__, cmd);
			break;
		}
		if (ch->g->ops.fifo.disable_channel)
			ch->g->ops.fifo.disable_channel(ch);
		else
			err = -ENOSYS;
		gk20a_idle(dev);
		break;
	case NVGPU_IOCTL_CHANNEL_PREEMPT:
		err = gk20a_busy(dev);
		if (err) {
			dev_err(dev,
				"%s: failed to host gk20a for ioctl cmd: 0x%x",
				__func__, cmd);
			break;
		}
		err = gk20a_fifo_preempt(ch->g, ch);
		gk20a_idle(dev);
		break;
	case NVGPU_IOCTL_CHANNEL_FORCE_RESET:
		err = gk20a_busy(dev);
		if (err) {
			dev_err(dev,
				"%s: failed to host gk20a for ioctl cmd: 0x%x",
				__func__, cmd);
			break;
		}
		err = ch->g->ops.fifo.force_reset_ch(ch, true);
		gk20a_idle(dev);
		break;
	case NVGPU_IOCTL_CHANNEL_EVENT_ID_CTRL:
		err = gk20a_channel_event_id_ctrl(ch,
			      (struct nvgpu_event_id_ctrl_args *)buf);
		break;
#ifdef CONFIG_GK20A_CYCLE_STATS
	case NVGPU_IOCTL_CHANNEL_CYCLE_STATS_SNAPSHOT:
		err = gk20a_busy(dev);
		if (err) {
			dev_err(dev,
				"%s: failed to host gk20a for ioctl cmd: 0x%x",
				__func__, cmd);
			break;
		}
		err = gk20a_channel_cycle_stats_snapshot(ch,
				(struct nvgpu_cycle_stats_snapshot_args *)buf);
		gk20a_idle(dev);
		break;
#endif
	case NVGPU_IOCTL_CHANNEL_WDT:
		err = gk20a_channel_set_wdt_status(ch,
				(struct nvgpu_channel_wdt_args *)buf);
		break;
	case NVGPU_IOCTL_CHANNEL_SET_RUNLIST_INTERLEAVE:
		err = gk20a_busy(dev);
		if (err) {
			dev_err(dev,
				"%s: failed to host gk20a for ioctl cmd: 0x%x",
				__func__, cmd);
			break;
		}
		err = gk20a_channel_set_runlist_interleave(ch,
			((struct nvgpu_runlist_interleave_args *)buf)->level);

		gk20a_idle(dev);
		gk20a_channel_trace_sched_param(
			trace_gk20a_channel_set_runlist_interleave, ch);
		break;
	case NVGPU_IOCTL_CHANNEL_SET_TIMESLICE:
		err = gk20a_busy(dev);
		if (err) {
			dev_err(dev,
				"%s: failed to host gk20a for ioctl cmd: 0x%x",
				__func__, cmd);
			break;
		}
		err = ch->g->ops.fifo.channel_set_timeslice(ch,
			((struct nvgpu_timeslice_args *)buf)->timeslice_us);

		gk20a_idle(dev);
		gk20a_channel_trace_sched_param(
			trace_gk20a_channel_set_timeslice, ch);
		break;
	case NVGPU_IOCTL_CHANNEL_SET_PREEMPTION_MODE:
		if (ch->g->ops.gr.set_preemption_mode) {
			err = gk20a_busy(dev);
			if (err) {
				dev_err(dev,
					"%s: failed to host gk20a for ioctl cmd: 0x%x",
					__func__, cmd);
				break;
			}
			err = ch->g->ops.gr.set_preemption_mode(ch,
			     ((struct nvgpu_preemption_mode_args *)buf)->graphics_preempt_mode,
			     ((struct nvgpu_preemption_mode_args *)buf)->compute_preempt_mode);
			gk20a_idle(dev);
		} else {
			err = -EINVAL;
		}
		break;
	default:
		dev_dbg(dev, "unrecognized ioctl cmd: 0x%x", cmd);
		err = -ENOTTY;
		break;
	}

	if ((err == 0) && (_IOC_DIR(cmd) & _IOC_READ))
		err = copy_to_user((void __user *)arg, buf, _IOC_SIZE(cmd));

	mutex_unlock(&ch->ioctl_lock);

	gk20a_channel_put(ch);

	gk20a_dbg_fn("end");

	return err;
}
