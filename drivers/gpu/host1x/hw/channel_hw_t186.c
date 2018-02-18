/*
 * Tegra host1x Channel
 *
 * Copyright (C) 2016 NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/host1x.h>
#include <linux/slab.h>
#include <linux/iommu.h>
#include <linux/pm_runtime.h>

#include <trace/events/host1x.h>

#include "dev_t186.h"
#include "channel.h"
#include "intr.h"
#include "job.h"

#define TRACE_MAX_LENGTH 128U

static void trace_write_gather(struct host1x_cdma *cdma, struct host1x_bo *bo,
			       u32 offset, u32 words)
{
	struct device *dev = cdma_to_channel(cdma)->dev;
	void *mem = NULL;

	if (host1x_debug_trace_cmdbuf)
		mem = host1x_bo_mmap(bo);

	if (mem) {
		u32 i;
		/*
		 * Write in batches of 128 as there seems to be a limit
		 * of how much you can output to ftrace at once.
		 */
		for (i = 0; i < words; i += TRACE_MAX_LENGTH) {
			u32 num_words = min(words - i, TRACE_MAX_LENGTH);
			u32 next_offset = offset + i * sizeof(u32);

			trace_host1x_cdma_push_gather(dev_name(dev), bo,
						      num_words,
						      next_offset,
						      mem);
		}

		host1x_bo_munmap(bo, mem);
	}
}

static void channel_push_wait(struct host1x_channel *channel,
			     u32 id, u32 thresh)
{
	host1x_cdma_push(&channel->cdma,
			 host1x_opcode_setclass(HOST1X_CLASS_HOST1X,
				HOST1X_UCLASS_LOAD_SYNCPT_PAYLOAD_32, 1),
			 thresh);
	host1x_cdma_push(&channel->cdma,
			 host1x_opcode_setclass(HOST1X_CLASS_HOST1X,
				HOST1X_UCLASS_WAIT_SYNCPT_32, 1),
			 id);
}

static inline void serialize(struct host1x_job *job)
{
	struct host1x_channel *ch = job->channel;
	struct host1x *host = dev_get_drvdata(ch->dev->parent);
	unsigned int i;

	for (i = 0; i < job->num_syncpts; ++i) {
		u32 syncpt_id = job->syncpts[i].id;
		struct host1x_syncpt *syncpt =
			host1x_syncpt_get(host, syncpt_id);

		/*
		 * Force serialization by inserting a host wait for the
		 * previous job to finish before this one can commence.
		 */
		channel_push_wait(ch, syncpt_id, host1x_syncpt_read_max(syncpt));
	}
}

static void add_sync_waits(struct host1x_job *job)
{
	struct host1x *host = dev_get_drvdata(job->channel->dev->parent);
	unsigned int i;

	for (i = 0; i < job->num_gathers; i++) {
		struct host1x_job_gather *g = &job->gathers[i];

		if (g->pre_fence)
			host1x_sync_fence_wait(g->pre_fence, host,
					       job->channel);
	}
}

static void submit_gathers(struct host1x_job *job)
{
	struct host1x_cdma *cdma = &job->channel->cdma;
	unsigned int i;
	u32 cur_class = 0;

	cur_class = HOST1X_CLASS_HOST1X;
	host1x_cdma_push(cdma,
		host1x_opcode_acquire_mlock(cur_class),
		host1x_opcode_setclass(cur_class, 0, 0));

	add_sync_waits(job);

	for (i = 0; i < job->num_gathers; i++) {
		struct host1x_job_gather *g = &job->gathers[i];
		u32 op1 = host1x_opcode_gather(g->words);
		u32 op2 = g->base + g->offset;

		/* add a setclass for modules that require it */
		if (cur_class != g->class_id) {
			if (cur_class)
				host1x_cdma_push(cdma,
					HOST1X_OPCODE_NOP,
					host1x_opcode_release_mlock(cur_class));

			host1x_cdma_push(cdma,
				host1x_opcode_acquire_mlock(g->class_id),
				host1x_opcode_setclass(g->class_id, 0, 0));
			cur_class = g->class_id;
		}

		trace_write_gather(cdma, g->bo, g->offset, op1 & 0xffff);
		host1x_cdma_push(cdma, op1, op2);
	}

	if (job->serialize)
		serialize(job);

	if (cur_class)
		host1x_cdma_push(cdma,
			HOST1X_OPCODE_NOP,
			host1x_opcode_release_mlock(cur_class));
}

static inline void synchronize_syncpt_base(struct host1x_job *job)
{
	struct host1x_channel *ch = job->channel;
	struct host1x *host = dev_get_drvdata(ch->dev->parent);
	unsigned int i;

	for (i = 0; i < job->num_syncpts; ++i) {
		u32 syncpt_id = job->syncpts[i].id;
		struct host1x_syncpt *syncpt =
			host1x_syncpt_get(host, syncpt_id);
		u32 base_id, value;

		if (!syncpt->base)
			continue;

		value = host1x_syncpt_read_max(syncpt);
		base_id = syncpt->base->id;

		host1x_cdma_push(&job->channel->cdma,
			 host1x_opcode_setclass(HOST1X_CLASS_HOST1X,
				HOST1X_UCLASS_LOAD_SYNCPT_BASE, 1),
			 HOST1X_UCLASS_LOAD_SYNCPT_BASE_BASE_INDX_F(base_id) |
			 HOST1X_UCLASS_LOAD_SYNCPT_BASE_VALUE_F(value));
	}
}

static int channel_submit(struct host1x_job *job)
{
	struct host1x_channel *ch = job->channel;
	struct host1x_syncpt *syncpt;
	u32 prev_max = 0;
	int err;
	struct host1x_waitlist *completed_waiter[job->num_syncpts];
	struct host1x *host = dev_get_drvdata(ch->dev->parent);
	unsigned int i;
	int streamid = 0;

	trace_host1x_channel_submit(dev_name(ch->dev),
				    job->num_gathers, job->num_relocs,
				    job->num_waitchk, job->syncpts[0].id,
				    job->syncpts[0].incrs);

	/* before error checks, return current max */
	syncpt = host1x_syncpt_get(host, job->syncpts[0].id);
	prev_max = host1x_syncpt_read_max(syncpt);

	/* keep device powered on */
	for (i = 0; i < job->num_syncpts; ++i)
		pm_runtime_get_sync(ch->dev);

	/* get submit lock */
	err = mutex_lock_interruptible(&ch->submitlock);
	if (err)
		goto error;

	for (i = 0; i < job->num_syncpts; i++) {
		completed_waiter[i] = kzalloc(sizeof(*completed_waiter[i]),
					      GFP_KERNEL);
		if (!completed_waiter[i]) {
			mutex_unlock(&ch->submitlock);
			err = -ENOMEM;
			goto error;
		}
	}

	streamid = iommu_get_hwid(host->dev->archdata.iommu, host->dev, 0);
	if (streamid >= 0)
		host1x_ch_writel(ch, streamid, HOST1X_CHANNEL_SMMU_STREAMID);

	/* begin a CDMA submit */
	err = host1x_cdma_begin(&ch->cdma, job);
	if (err) {
		mutex_unlock(&ch->submitlock);
		goto error;
	}

	/* Synchronize base register to allow using it for relative waiting */
	synchronize_syncpt_base(job);

	/* Increment syncpoint maximum values */
	for (i = 0; i < job->num_syncpts; ++i) {
		u32 id;
		u32 incrs;

		id = job->syncpts[i].id;
		incrs = job->syncpts[i].incrs;
		syncpt = host1x_syncpt_get(host, id);
		job->syncpts[i].end = host1x_syncpt_incr_max(syncpt, incrs);
	}

	submit_gathers(job);

	/* end CDMA submit & stash pinned hMems into sync queue */
	host1x_cdma_end(&ch->cdma, job);

	trace_host1x_channel_submitted(dev_name(ch->dev), prev_max,
				       job->syncpts[0].end);

	/* schedule submit complete interrupts */
	for (i = 0; i < job->num_syncpts; ++i) {
		u32 syncpt_id = job->syncpts[i].id;
		u32 syncpt_end = job->syncpts[i].end;

		err = host1x_intr_add_action(host, syncpt_id, syncpt_end,
					     HOST1X_INTR_ACTION_SUBMIT_COMPLETE,
					     ch, completed_waiter[i], NULL);
		completed_waiter[i] = NULL;
		WARN(err, "Failed to set submit complete interrupt");
	}

	mutex_unlock(&ch->submitlock);

	return 0;

error:
	for (i = 0; i < job->num_syncpts; i++)
		kfree(completed_waiter[i]);
	return err;
}

static int host1x_channel_init(struct host1x_channel *ch, struct host1x *dev,
			       unsigned int index)
{
	ch->id = index;
	mutex_init(&ch->reflock);
	mutex_init(&ch->submitlock);

	ch->regs = dev->regs + (HOST1X_CHANNEL_CH_APERTURE_START +
				index * HOST1X_CHANNEL_CH_APERTURE_SIZE);
	return 0;
}

static void channel_enable_gather_filter(struct host1x_channel *ch)
{
	struct host1x *host = dev_get_drvdata(ch->dev->parent);
	u32 val;

	val = host1x_readl(host, HOST1X_CHANNEL_FILTER_GBUFFER
				 + BIT_WORD(ch->id) * sizeof(u32));

	host1x_writel(host, val | BIT_MASK(ch->id),
				HOST1X_CHANNEL_FILTER_GBUFFER
				+ BIT_WORD(ch->id) * sizeof(u32));
}

static const struct host1x_channel_ops host1x_channel_t186_ops = {
	.init = host1x_channel_init,
	.submit = channel_submit,
	.push_wait = channel_push_wait,
	.enable_gather_filter = channel_enable_gather_filter,
};
