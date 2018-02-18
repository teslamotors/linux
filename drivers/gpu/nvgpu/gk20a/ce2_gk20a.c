/*
 * GK20A Graphics Copy Engine  (gr host)
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
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 */

/*TODO: remove uncecessary */
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/scatterlist.h>
#include <trace/events/gk20a.h>
#include <linux/dma-mapping.h>
#include <linux/nvhost.h>
#include <linux/debugfs.h>

#include "gk20a.h"
#include "debug_gk20a.h"
#include "semaphore_gk20a.h"
#include "hw_ce2_gk20a.h"
#include "hw_pbdma_gk20a.h"
#include "hw_ccsr_gk20a.h"
#include "hw_ram_gk20a.h"
#include "hw_top_gk20a.h"
#include "hw_mc_gk20a.h"
#include "hw_gr_gk20a.h"

static u32 ce2_nonblockpipe_isr(struct gk20a *g, u32 fifo_intr)
{
	gk20a_dbg(gpu_dbg_intr, "ce2 non-blocking pipe interrupt\n");

	return ce2_intr_status_nonblockpipe_pending_f();
}

static u32 ce2_blockpipe_isr(struct gk20a *g, u32 fifo_intr)
{
	gk20a_dbg(gpu_dbg_intr, "ce2 blocking pipe interrupt\n");

	return ce2_intr_status_blockpipe_pending_f();
}

static u32 ce2_launcherr_isr(struct gk20a *g, u32 fifo_intr)
{
	gk20a_dbg(gpu_dbg_intr, "ce2 launch error interrupt\n");

	return ce2_intr_status_launcherr_pending_f();
}

void gk20a_ce2_isr(struct gk20a *g, u32 inst_id, u32 pri_base)
{
	u32 ce2_intr = gk20a_readl(g, ce2_intr_status_r());
	u32 clear_intr = 0;

	gk20a_dbg(gpu_dbg_intr, "ce2 isr %08x\n", ce2_intr);

	/* clear blocking interrupts: they exibit broken behavior */
	if (ce2_intr & ce2_intr_status_blockpipe_pending_f())
		clear_intr |= ce2_blockpipe_isr(g, ce2_intr);

	if (ce2_intr & ce2_intr_status_launcherr_pending_f())
		clear_intr |= ce2_launcherr_isr(g, ce2_intr);

	gk20a_writel(g, ce2_intr_status_r(), clear_intr);
	return;
}

void gk20a_ce2_nonstall_isr(struct gk20a *g, u32 inst_id, u32 pri_base)
{
	u32 ce2_intr = gk20a_readl(g, ce2_intr_status_r());

	gk20a_dbg(gpu_dbg_intr, "ce2 nonstall isr %08x\n", ce2_intr);

	if (ce2_intr & ce2_intr_status_nonblockpipe_pending_f()) {
		gk20a_writel(g, ce2_intr_status_r(),
			ce2_nonblockpipe_isr(g, ce2_intr));

		/* wake threads waiting in this channel */
		gk20a_channel_semaphore_wakeup(g, true);
	}

	return;
}
void gk20a_init_ce2(struct gpu_ops *gops)
{
	gops->ce2.isr_stall = gk20a_ce2_isr;
	gops->ce2.isr_nonstall = gk20a_ce2_nonstall_isr;
}

/* static CE app api */
static void gk20a_ce_notify_all_user(struct gk20a *g, u32 event)
{
	struct gk20a_ce_app *ce_app = &g->ce_app;
	struct gk20a_gpu_ctx *ce_ctx, *ce_ctx_save;

	if (!ce_app->initialised)
		return;

	mutex_lock(&ce_app->app_mutex);

	list_for_each_entry_safe(ce_ctx, ce_ctx_save,
			&ce_app->allocated_contexts, list) {
		if (ce_ctx->user_event_callback) {
			ce_ctx->user_event_callback(ce_ctx->ctx_id,
				event);
		}
	}

	mutex_unlock(&ce_app->app_mutex);
}

static void gk20a_ce_finished_ctx_cb(struct channel_gk20a *ch, void *data)
{
	struct gk20a_gpu_ctx *ce_ctx = data;
	bool channel_idle;
	u32 event;

	channel_gk20a_joblist_lock(ch);
	channel_idle = channel_gk20a_joblist_is_empty(ch);
	channel_gk20a_joblist_unlock(ch);

	if (!channel_idle)
		return;

	gk20a_dbg(gpu_dbg_fn, "ce: finished %p", ce_ctx);

	if (ch->has_timedout)
		event = NVGPU_CE_CONTEXT_JOB_TIMEDOUT;
	else
		event = NVGPU_CE_CONTEXT_JOB_COMPLETED;

	if (ce_ctx->user_event_callback)
		ce_ctx->user_event_callback(ce_ctx->ctx_id,
			event);

	++ce_ctx->completed_seq_number;
}

static void gk20a_ce_free_command_buffer_stored_fence(struct gk20a_gpu_ctx *ce_ctx)
{
	u32 cmd_buf_index;
	u32 cmd_buf_read_offset;
	u32 fence_index;
	u32 *cmd_buf_cpu_va;

	for (cmd_buf_index = 0;
		cmd_buf_index < ce_ctx->cmd_buf_end_queue_offset;
		cmd_buf_index++) {
		cmd_buf_read_offset = (cmd_buf_index *
			(NVGPU_CE_MAX_COMMAND_BUFF_SIZE_PER_KICKOFF / sizeof(u32)));

		/* at end of command buffer has gk20a_fence for command buffer sync */
		fence_index = (cmd_buf_read_offset +
			((NVGPU_CE_MAX_COMMAND_BUFF_SIZE_PER_KICKOFF / sizeof(u32)) -
			(NVGPU_CE_MAX_COMMAND_BUFF_SIZE_FOR_TRACING / sizeof(u32))));

		cmd_buf_cpu_va = (u32 *)ce_ctx->cmd_buf_mem.cpu_va;

		/* 0 is treated as invalid pre-sync */
		if (cmd_buf_cpu_va[fence_index]) {
			struct gk20a_fence * ce_cmd_buf_fence_in = NULL;

			memcpy((void *)&ce_cmd_buf_fence_in,
					(void *)(cmd_buf_cpu_va + fence_index),
					sizeof(struct gk20a_fence *));
			gk20a_fence_put(ce_cmd_buf_fence_in);
			/* Reset the stored last pre-sync */
			memset((void *)(cmd_buf_cpu_va + fence_index),
					0,
					NVGPU_CE_MAX_COMMAND_BUFF_SIZE_FOR_TRACING);
		}
	}
}

/* assume this api should need to call under mutex_lock(&ce_app->app_mutex) */
static void gk20a_ce_delete_gpu_context(struct gk20a_gpu_ctx *ce_ctx)
{
	ce_ctx->gpu_ctx_state = NVGPU_CE_GPU_CTX_DELETED;

	mutex_lock(&ce_ctx->gpu_ctx_mutex);

	gk20a_ce_free_command_buffer_stored_fence(ce_ctx);

	gk20a_gmmu_unmap_free(ce_ctx->vm, &ce_ctx->cmd_buf_mem);

	/* free the channel */
	if (ce_ctx->ch)
		gk20a_channel_close(ce_ctx->ch);

	/* housekeeping on app */
	list_del(&ce_ctx->list);

	mutex_unlock(&ce_ctx->gpu_ctx_mutex);
	mutex_destroy(&ce_ctx->gpu_ctx_mutex);

	kfree(ce_ctx);
}

static inline int gk20a_ce_get_method_size(int request_operation)
{
	/* failure size */
	int methodsize = ~0;

	if (request_operation & NVGPU_CE_PHYS_MODE_TRANSFER)
		methodsize = 10 * 2 * sizeof(u32);
	else if (request_operation & NVGPU_CE_MEMSET)
		methodsize = 9 * 2 * sizeof(u32);

	return methodsize;
}

static inline int gk20a_get_valid_launch_flags(struct gk20a *g, int launch_flags)
{
	/* there is no local memory available,
	don't allow local memory related CE flags */
	if (!g->mm.vidmem.size) {
		launch_flags &= ~(NVGPU_CE_SRC_LOCATION_LOCAL_FB |
			NVGPU_CE_DST_LOCATION_LOCAL_FB);
	}
	return launch_flags;
}

static int gk20a_ce_prepare_submit(u64 src_buf,
		u64 dst_buf,
		u64 size,
		u32 *cmd_buf_cpu_va,
		u32 max_cmd_buf_size,
		unsigned int payload,
		int launch_flags,
		int request_operation,
		u32 dma_copy_class,
		struct gk20a_fence *gk20a_fence_in)
{
	u32 launch = 0;
	u32 methodSize = 0;

	/* failure case handling */
	if ((gk20a_ce_get_method_size(request_operation) > max_cmd_buf_size) ||
		(!size) ||
		(request_operation > NVGPU_CE_MEMSET))
		return 0;

	/* set the channel object */
	cmd_buf_cpu_va[methodSize++] = 0x20018000;
	cmd_buf_cpu_va[methodSize++] = dma_copy_class;

	if (request_operation & NVGPU_CE_PHYS_MODE_TRANSFER) {
		/* setup the source */
		cmd_buf_cpu_va[methodSize++] = 0x20018101;
		cmd_buf_cpu_va[methodSize++] = (u64_lo32(src_buf) &
					NVGPU_CE_LOWER_ADDRESS_OFFSET_MASK);

		cmd_buf_cpu_va[methodSize++] = 0x20018100;
		cmd_buf_cpu_va[methodSize++] = (u64_hi32(src_buf) &
					NVGPU_CE_UPPER_ADDRESS_OFFSET_MASK);

		cmd_buf_cpu_va[methodSize++] = 0x20018098;
		if (launch_flags & NVGPU_CE_SRC_LOCATION_LOCAL_FB) {
			cmd_buf_cpu_va[methodSize++] = 0x00000000;
		} else if (launch_flags & NVGPU_CE_SRC_LOCATION_NONCOHERENT_SYSMEM) {
			cmd_buf_cpu_va[methodSize++] = 0x00000002;
		} else {
			cmd_buf_cpu_va[methodSize++] = 0x00000001;
		}

		launch |= 0x00001000;
	} else if (request_operation & NVGPU_CE_MEMSET) {
		cmd_buf_cpu_va[methodSize++] = 0x200181c2;
		cmd_buf_cpu_va[methodSize++] = 0x00030004;

		cmd_buf_cpu_va[methodSize++] = 0x200181c0;
		cmd_buf_cpu_va[methodSize++] = payload;

		launch |= 0x00000400;

		/* converted into number of words */
		size /= sizeof(u32);
	}

	/* setup the destination/output */
	cmd_buf_cpu_va[methodSize++] = 0x20018103;
	cmd_buf_cpu_va[methodSize++] = (u64_lo32(dst_buf) & NVGPU_CE_LOWER_ADDRESS_OFFSET_MASK);

	cmd_buf_cpu_va[methodSize++] = 0x20018102;
	cmd_buf_cpu_va[methodSize++] = (u64_hi32(dst_buf) & NVGPU_CE_UPPER_ADDRESS_OFFSET_MASK);

	cmd_buf_cpu_va[methodSize++] = 0x20018099;
	if (launch_flags & NVGPU_CE_DST_LOCATION_LOCAL_FB) {
		cmd_buf_cpu_va[methodSize++] = 0x00000000;
	} else if (launch_flags & NVGPU_CE_DST_LOCATION_NONCOHERENT_SYSMEM) {
		cmd_buf_cpu_va[methodSize++] = 0x00000002;
	} else {
		cmd_buf_cpu_va[methodSize++] = 0x00000001;
	}

	launch |= 0x00002000;

	/* setup the format */
	cmd_buf_cpu_va[methodSize++] = 0x20018107;
	cmd_buf_cpu_va[methodSize++] = 1;
	cmd_buf_cpu_va[methodSize++] = 0x20018106;
	cmd_buf_cpu_va[methodSize++] =  u64_lo32(size);

	launch |= 0x00000004;

	if (launch_flags & NVGPU_CE_SRC_MEMORY_LAYOUT_BLOCKLINEAR)
		launch |= 0x00000000;
	else
		launch |= 0x00000080;

	if (launch_flags & NVGPU_CE_DST_MEMORY_LAYOUT_BLOCKLINEAR)
		launch |= 0x00000000;
	else
		launch |= 0x00000100;

	if (launch_flags & NVGPU_CE_DATA_TRANSFER_TYPE_NON_PIPELINED)
		launch |= 0x00000002;
	else
		launch |= 0x00000001;

	cmd_buf_cpu_va[methodSize++] = 0x200180c0;
	cmd_buf_cpu_va[methodSize++] = launch;

	return methodSize;
}

/* global CE app related apis */
int gk20a_init_ce_support(struct gk20a *g)
{
	struct gk20a_ce_app *ce_app = &g->ce_app;

	if (ce_app->initialised) {
		/* assume this happen during poweron/poweroff GPU sequence */
		ce_app->app_state = NVGPU_CE_ACTIVE;
		gk20a_ce_notify_all_user(g, NVGPU_CE_CONTEXT_RESUME);
		return 0;
	}

	gk20a_dbg(gpu_dbg_fn, "ce: init");

	mutex_init(&ce_app->app_mutex);
	mutex_lock(&ce_app->app_mutex);

	INIT_LIST_HEAD(&ce_app->allocated_contexts);
	ce_app->ctx_count = 0;
	ce_app->next_ctx_id = 0;
	ce_app->initialised = true;
	ce_app->app_state = NVGPU_CE_ACTIVE;

	mutex_unlock(&ce_app->app_mutex);
	gk20a_dbg(gpu_dbg_cde_ctx, "ce: init finished");

	return 0;
}

void gk20a_ce_destroy(struct gk20a *g)
{
	struct gk20a_ce_app *ce_app = &g->ce_app;
	struct gk20a_gpu_ctx *ce_ctx, *ce_ctx_save;

	if (!ce_app->initialised)
		return;

	ce_app->app_state = NVGPU_CE_SUSPEND;
	ce_app->initialised = false;

	mutex_lock(&ce_app->app_mutex);

	list_for_each_entry_safe(ce_ctx, ce_ctx_save,
			&ce_app->allocated_contexts, list) {
		gk20a_ce_delete_gpu_context(ce_ctx);
	}

	INIT_LIST_HEAD(&ce_app->allocated_contexts);
	ce_app->ctx_count = 0;
	ce_app->next_ctx_id = 0;

	mutex_unlock(&ce_app->app_mutex);
	mutex_destroy(&ce_app->app_mutex);
}

void gk20a_ce_suspend(struct gk20a *g)
{
	struct gk20a_ce_app *ce_app = &g->ce_app;

	if (!ce_app->initialised)
		return;

	ce_app->app_state = NVGPU_CE_SUSPEND;
	gk20a_ce_notify_all_user(g, NVGPU_CE_CONTEXT_SUSPEND);

	return;
}

/* CE app utility functions */
u32 gk20a_ce_create_context_with_cb(struct device *dev,
		int runlist_id,
		int priority,
		int timeslice,
		int runlist_level,
		ce_event_callback user_event_callback)
{
	struct gk20a_gpu_ctx *ce_ctx;
	struct gk20a *g = gk20a_from_dev(dev);
	struct gk20a_ce_app *ce_app = &g->ce_app;
	u32 ctx_id = ~0;
	int err = 0;

	if (!ce_app->initialised || ce_app->app_state != NVGPU_CE_ACTIVE)
		return ctx_id;

	ce_ctx = kzalloc(sizeof(*ce_ctx), GFP_KERNEL);
	if (!ce_ctx)
		return ctx_id;

	mutex_init(&ce_ctx->gpu_ctx_mutex);

	ce_ctx->g = g;
	ce_ctx->dev = g->dev;
	ce_ctx->user_event_callback = user_event_callback;

	ce_ctx->cmd_buf_read_queue_offset = 0;
	ce_ctx->cmd_buf_end_queue_offset =
		(NVGPU_CE_COMMAND_BUF_SIZE / NVGPU_CE_MAX_COMMAND_BUFF_SIZE_PER_KICKOFF);

	ce_ctx->submitted_seq_number = 0;
	ce_ctx->completed_seq_number = 0;

	ce_ctx->vm = &g->mm.ce.vm;

	/* always kernel client needs privileged channel */
	ce_ctx->ch = gk20a_open_new_channel_with_cb(g, gk20a_ce_finished_ctx_cb,
					ce_ctx,
					runlist_id,
					true);
	ce_ctx->ch->wdt_enabled = false;
	if (!ce_ctx->ch) {
		gk20a_err(ce_ctx->dev, "ce: gk20a channel not available");
		goto end;
	 }

	/* bind the channel to the vm */
	err = __gk20a_vm_bind_channel(&g->mm.ce.vm, ce_ctx->ch);
	if (err) {
		gk20a_err(ce_ctx->dev, "ce: could not bind vm");
		goto end;
	}

	/* allocate gpfifo (1024 should be more than enough) */
	err = gk20a_alloc_channel_gpfifo(ce_ctx->ch,
		&(struct nvgpu_alloc_gpfifo_ex_args){1024, 0, 0, {}});
	if (err) {
		gk20a_err(ce_ctx->dev, "ce: unable to allocate gpfifo");
		goto end;
	}

	/* allocate command buffer (4096 should be more than enough) from sysmem*/
	err = gk20a_gmmu_alloc_map_sys(ce_ctx->vm, NVGPU_CE_COMMAND_BUF_SIZE, &ce_ctx->cmd_buf_mem);
	 if (err) {
		gk20a_err(ce_ctx->dev,
			"ce: could not allocate command buffer for CE context");
		goto end;
	}

	memset(ce_ctx->cmd_buf_mem.cpu_va, 0x00, ce_ctx->cmd_buf_mem.size);

	/* -1 means default channel priority */
	if (priority != -1) {
		err = gk20a_channel_set_priority(ce_ctx->ch, priority);
		if (err) {
			gk20a_err(ce_ctx->dev,
				"ce: could not set the channel priority for CE context");
			goto end;
		}
	}

	/* -1 means default channel timeslice value */
	if (timeslice != -1) {
		err = gk20a_channel_set_timeslice(ce_ctx->ch, timeslice);
		if (err) {
			gk20a_err(ce_ctx->dev,
				"ce: could not set the channel timeslice value for CE context");
			goto end;
		}
	}

	/* -1 means default channel runlist level */
	if (runlist_level != -1) {
		err = gk20a_channel_set_runlist_interleave(ce_ctx->ch, runlist_level);
		if (err) {
			gk20a_err(ce_ctx->dev,
				"ce: could not set the runlist interleave for CE context");
			goto end;
		}
	}

	mutex_lock(&ce_app->app_mutex);
	ctx_id = ce_ctx->ctx_id = ce_app->next_ctx_id;
	list_add(&ce_ctx->list, &ce_app->allocated_contexts);
	++ce_app->next_ctx_id;
	++ce_app->ctx_count;
	mutex_unlock(&ce_app->app_mutex);

	ce_ctx->gpu_ctx_state = NVGPU_CE_GPU_CTX_ALLOCATED;

end:
	if (ctx_id == ~0) {
		mutex_lock(&ce_app->app_mutex);
		gk20a_ce_delete_gpu_context(ce_ctx);
		mutex_unlock(&ce_app->app_mutex);
	}
	return ctx_id;

}
EXPORT_SYMBOL(gk20a_ce_create_context_with_cb);

int gk20a_ce_execute_ops(struct device *dev,
		u32 ce_ctx_id,
		u64 src_buf,
		u64 dst_buf,
		u64 size,
		unsigned int payload,
		int launch_flags,
		int request_operation,
		struct gk20a_fence *gk20a_fence_in,
		u32 submit_flags,
		struct gk20a_fence **gk20a_fence_out)
{
	int ret = -EPERM;
	struct gk20a *g = gk20a_from_dev(dev);
	struct gk20a_ce_app *ce_app = &g->ce_app;
	struct gk20a_gpu_ctx *ce_ctx, *ce_ctx_save;
	bool found = false;
	u32 *cmd_buf_cpu_va;
	u64 cmd_buf_gpu_va = 0;
	u32 methodSize;
	u32 cmd_buf_read_offset;
	u32 fence_index;
	struct nvgpu_gpfifo gpfifo;
	struct nvgpu_fence fence = {0,0};
	struct gk20a_fence *ce_cmd_buf_fence_out = NULL;
	struct nvgpu_gpu_characteristics *gpu_capability = &g->gpu_characteristics;

	if (!ce_app->initialised ||ce_app->app_state != NVGPU_CE_ACTIVE)
		goto end;

	mutex_lock(&ce_app->app_mutex);

	list_for_each_entry_safe(ce_ctx, ce_ctx_save,
			&ce_app->allocated_contexts, list) {
		if (ce_ctx->ctx_id == ce_ctx_id) {
			found = true;
			break;
		}
	}

	mutex_unlock(&ce_app->app_mutex);

	if (!found) {
		ret = -EINVAL;
		goto end;
	}

	if (ce_ctx->gpu_ctx_state != NVGPU_CE_GPU_CTX_ALLOCATED) {
		ret = -ENODEV;
		goto end;
	}

	mutex_lock(&ce_ctx->gpu_ctx_mutex);

	ce_ctx->cmd_buf_read_queue_offset %= ce_ctx->cmd_buf_end_queue_offset;

	cmd_buf_read_offset = (ce_ctx->cmd_buf_read_queue_offset *
			(NVGPU_CE_MAX_COMMAND_BUFF_SIZE_PER_KICKOFF / sizeof(u32)));

	/* at end of command buffer has gk20a_fence for command buffer sync */
	fence_index = (cmd_buf_read_offset +
			((NVGPU_CE_MAX_COMMAND_BUFF_SIZE_PER_KICKOFF / sizeof(u32)) -
			(NVGPU_CE_MAX_COMMAND_BUFF_SIZE_FOR_TRACING / sizeof(u32))));

	if (sizeof(struct gk20a_fence *) > NVGPU_CE_MAX_COMMAND_BUFF_SIZE_FOR_TRACING) {
		ret = -ENOMEM;
		goto noop;
	}

	cmd_buf_cpu_va = (u32 *)ce_ctx->cmd_buf_mem.cpu_va;

	/* 0 is treated as invalid pre-sync */
	if (cmd_buf_cpu_va[fence_index]) {
		struct gk20a_fence * ce_cmd_buf_fence_in = NULL;

		memcpy((void *)&ce_cmd_buf_fence_in,
				(void *)(cmd_buf_cpu_va + fence_index),
				sizeof(struct gk20a_fence *));
		ret = gk20a_fence_wait(ce_cmd_buf_fence_in, gk20a_get_gr_idle_timeout(g));

		gk20a_fence_put(ce_cmd_buf_fence_in);
		/* Reset the stored last pre-sync */
		memset((void *)(cmd_buf_cpu_va + fence_index),
				0,
				NVGPU_CE_MAX_COMMAND_BUFF_SIZE_FOR_TRACING);
		if (ret)
			goto noop;
	}

	cmd_buf_gpu_va = (ce_ctx->cmd_buf_mem.gpu_va + (u64)(cmd_buf_read_offset *sizeof(u32)));

	methodSize = gk20a_ce_prepare_submit(src_buf,
					dst_buf,
					size,
					&cmd_buf_cpu_va[cmd_buf_read_offset],
					NVGPU_CE_MAX_COMMAND_BUFF_SIZE_PER_KICKOFF,
					payload,
					gk20a_get_valid_launch_flags(g, launch_flags),
					request_operation,
					gpu_capability->dma_copy_class,
					gk20a_fence_in);

	if (methodSize) {
		/* TODO: Remove CPU pre-fence wait */
		if (gk20a_fence_in) {
			ret = gk20a_fence_wait(gk20a_fence_in, gk20a_get_gr_idle_timeout(g));
			gk20a_fence_put(gk20a_fence_in);
			if (ret)
				goto noop;
		}

		/* store the element into gpfifo */
		gpfifo.entry0 =
			u64_lo32(cmd_buf_gpu_va);
		gpfifo.entry1 =
			(u64_hi32(cmd_buf_gpu_va) |
			pbdma_gp_entry1_length_f(methodSize));

		/* take always the postfence as it is needed for protecting the ce context */
		submit_flags |= NVGPU_SUBMIT_GPFIFO_FLAGS_FENCE_GET;

		wmb();

		ret = gk20a_submit_channel_gpfifo(ce_ctx->ch, &gpfifo, NULL,
					1, submit_flags, &fence,
					&ce_cmd_buf_fence_out, false);

		if (!ret) {
			memcpy((void *)(cmd_buf_cpu_va + fence_index),
					(void *)&ce_cmd_buf_fence_out,
					sizeof(struct gk20a_fence *));

			if (gk20a_fence_out) {
				gk20a_fence_get(ce_cmd_buf_fence_out);
				*gk20a_fence_out = ce_cmd_buf_fence_out;
			}

			/* Next available command buffer queue Index */
			++ce_ctx->cmd_buf_read_queue_offset;
			++ce_ctx->submitted_seq_number;
			}
	} else
		ret = -ENOMEM;
noop:
	mutex_unlock(&ce_ctx->gpu_ctx_mutex);
end:
	return ret;
}
EXPORT_SYMBOL(gk20a_ce_execute_ops);

void gk20a_ce_delete_context(struct device *dev,
		u32 ce_ctx_id)
{
	struct gk20a *g = gk20a_from_dev(dev);
	struct gk20a_ce_app *ce_app = &g->ce_app;
	struct gk20a_gpu_ctx *ce_ctx, *ce_ctx_save;

	if (!ce_app->initialised ||ce_app->app_state != NVGPU_CE_ACTIVE)
		return;

	mutex_lock(&ce_app->app_mutex);

	list_for_each_entry_safe(ce_ctx, ce_ctx_save,
			&ce_app->allocated_contexts, list) {
		if (ce_ctx->ctx_id == ce_ctx_id) {
			gk20a_ce_delete_gpu_context(ce_ctx);
			--ce_app->ctx_count;
			break;
		}
	}

	mutex_unlock(&ce_app->app_mutex);
	return;
}
EXPORT_SYMBOL(gk20a_ce_delete_context);

#ifdef CONFIG_DEBUG_FS
void gk20a_ce_debugfs_init(struct device *dev)
{
	struct gk20a_platform *platform = dev_get_drvdata(dev);
	struct gk20a *g = get_gk20a(dev);

	if (!platform->has_ce)
		return;

	debugfs_create_u32("ce_app_ctx_count", S_IWUSR | S_IRUGO,
			   platform->debugfs, &g->ce_app.ctx_count);
	debugfs_create_u32("ce_app_state", S_IWUSR | S_IRUGO,
			   platform->debugfs, &g->ce_app.app_state);
	debugfs_create_u32("ce_app_next_ctx_id", S_IWUSR | S_IRUGO,
			   platform->debugfs, &g->ce_app.next_ctx_id);
}
#endif
