/*
 * drivers/video/tegra/host/t20/cdma_t20.c
 *
 * Tegra Graphics Host Command DMA
 *
 * Copyright (c) 2010-2011, NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/slab.h>
#include "../nvhost_cdma.h"
#include "../dev.h"

#include "hardware_t20.h"
#include "syncpt_t20.h"

static void t20_cdma_timeout_handler(struct work_struct *work);

/*
 * push_buffer
 *
 * The push buffer is a circular array of words to be fetched by command DMA.
 * Note that it works slightly differently to the sync queue; fence == cur
 * means that the push buffer is full, not empty.
 */


/**
 * Reset to empty push buffer
 */
static void t20_push_buffer_reset(struct push_buffer *pb)
{
	pb->fence = PUSH_BUFFER_SIZE - 8;
	pb->cur = 0;
}

/**
 * Init push buffer resources
 */
static int t20_push_buffer_init(struct push_buffer *pb)
{
	struct nvhost_cdma *cdma = pb_to_cdma(pb);
	struct nvmap_client *nvmap = cdma_to_nvmap(cdma);
	pb->mem = NULL;
	pb->mapped = NULL;
	pb->phys = 0;
	pb->nvmap = NULL;

	BUG_ON(!cdma_pb_op(cdma).reset);
	cdma_pb_op(cdma).reset(pb);

	/* allocate and map pushbuffer memory */
	pb->mem = nvmap_alloc(nvmap, PUSH_BUFFER_SIZE + 4, 32,
			      NVMAP_HANDLE_WRITE_COMBINE);
	if (IS_ERR_OR_NULL(pb->mem)) {
		pb->mem = NULL;
		goto fail;
	}
	pb->mapped = nvmap_mmap(pb->mem);
	if (pb->mapped == NULL)
		goto fail;

	/* pin pushbuffer and get physical address */
	pb->phys = nvmap_pin(nvmap, pb->mem);
	if (pb->phys >= 0xfffff000) {
		pb->phys = 0;
		goto fail;
	}

	/* memory for storing nvmap client and handles for each opcode pair */
	pb->nvmap = kzalloc(NVHOST_GATHER_QUEUE_SIZE *
				sizeof(struct nvmap_client_handle),
			GFP_KERNEL);
	if (!pb->nvmap)
		goto fail;

	/* put the restart at the end of pushbuffer memory */
	*(pb->mapped + (PUSH_BUFFER_SIZE >> 2)) = nvhost_opcode_restart(pb->phys);

	return 0;

fail:
	cdma_pb_op(cdma).destroy(pb);
	return -ENOMEM;
}

/**
 * Clean up push buffer resources
 */
static void t20_push_buffer_destroy(struct push_buffer *pb)
{
	struct nvhost_cdma *cdma = pb_to_cdma(pb);
	struct nvmap_client *nvmap = cdma_to_nvmap(cdma);
	if (pb->mapped)
		nvmap_munmap(pb->mem, pb->mapped);

	if (pb->phys != 0)
		nvmap_unpin(nvmap, pb->mem);

	if (pb->mem)
		nvmap_free(nvmap, pb->mem);

	kfree(pb->nvmap);

	pb->mem = NULL;
	pb->mapped = NULL;
	pb->phys = 0;
	pb->nvmap = 0;
}

/**
 * Push two words to the push buffer
 * Caller must ensure push buffer is not full
 */
static void t20_push_buffer_push_to(struct push_buffer *pb,
		struct nvmap_client *client,
		struct nvmap_handle *handle, u32 op1, u32 op2)
{
	u32 cur = pb->cur;
	u32 *p = (u32*)((u32)pb->mapped + cur);
	u32 cur_nvmap = (cur/8) & (NVHOST_GATHER_QUEUE_SIZE - 1);
	BUG_ON(cur == pb->fence);
	*(p++) = op1;
	*(p++) = op2;
	pb->nvmap[cur_nvmap].client = client;
	pb->nvmap[cur_nvmap].handle = handle;
	pb->cur = (cur + 8) & (PUSH_BUFFER_SIZE - 1);
}

/**
 * Pop a number of two word slots from the push buffer
 * Caller must ensure push buffer is not empty
 */
static void t20_push_buffer_pop_from(struct push_buffer *pb,
		unsigned int slots)
{
	/* Clear the nvmap references for old items from pb */
	unsigned int i;
	u32 fence_nvmap = pb->fence/8;
	for(i = 0; i < slots; i++) {
		int cur_fence_nvmap = (fence_nvmap+i)
				& (NVHOST_GATHER_QUEUE_SIZE - 1);
		struct nvmap_client_handle *h =
				&pb->nvmap[cur_fence_nvmap];
		h->client = NULL;
		h->handle = NULL;
	}
	/* Advance the next write position */
	pb->fence = (pb->fence + slots * 8) & (PUSH_BUFFER_SIZE - 1);
}

/**
 * Return the number of two word slots free in the push buffer
 */
static u32 t20_push_buffer_space(struct push_buffer *pb)
{
	return ((pb->fence - pb->cur) & (PUSH_BUFFER_SIZE - 1)) / 8;
}

static u32 t20_push_buffer_putptr(struct push_buffer *pb)
{
	return pb->phys + pb->cur;
}

/*
 * The syncpt incr buffer is filled with methods to increment syncpts, which
 * is later GATHER-ed into the mainline PB. It's used when a timed out context
 * is interleaved with other work, so needs to inline the syncpt increments
 * to maintain the count (but otherwise does no work).
 */

/**
 * Init timeout and syncpt incr buffer resources
 */
static int t20_cdma_timeout_init(struct nvhost_cdma *cdma,
				 u32 syncpt_id)
{
	struct nvhost_master *dev = cdma_to_dev(cdma);
	struct nvmap_client *nvmap = cdma_to_nvmap(cdma);
	struct syncpt_buffer *sb = &cdma->syncpt_buffer;
	struct nvhost_channel *ch = cdma_to_channel(cdma);
	u32 i = 0;

	if (syncpt_id == NVSYNCPT_INVALID)
		return -EINVAL;

	/* allocate and map syncpt incr memory */
	sb->mem = nvmap_alloc(nvmap,
			(SYNCPT_INCR_BUFFER_SIZE_WORDS * sizeof(u32)), 32,
			NVMAP_HANDLE_WRITE_COMBINE);
	if (IS_ERR_OR_NULL(sb->mem)) {
		sb->mem = NULL;
		goto fail;
	}
	sb->mapped = nvmap_mmap(sb->mem);
	if (sb->mapped == NULL)
		goto fail;

	/* pin syncpt buffer and get physical address */
	sb->phys = nvmap_pin(nvmap, sb->mem);
	if (sb->phys >= 0xfffff000) {
		sb->phys = 0;
		goto fail;
	}

	dev_dbg(&dev->pdev->dev, "%s: SYNCPT_INCR buffer at 0x%x\n",
		 __func__, sb->phys);

	sb->words_per_incr = (syncpt_id == NVSYNCPT_3D) ? 5 : 3;
	sb->incr_per_buffer = (SYNCPT_INCR_BUFFER_SIZE_WORDS /
				sb->words_per_incr);

	/* init buffer with SETCL and INCR_SYNCPT methods */
	while (i < sb->incr_per_buffer) {
		sb->mapped[i++] = nvhost_opcode_setclass(NV_HOST1X_CLASS_ID,
						0, 0);
		sb->mapped[i++] = nvhost_opcode_imm_incr_syncpt(
						NV_SYNCPT_IMMEDIATE,
						syncpt_id);
		if (syncpt_id == NVSYNCPT_3D) {
			/* also contains base increments */
			sb->mapped[i++] = nvhost_opcode_nonincr(
						NV_CLASS_HOST_INCR_SYNCPT_BASE,
						1);
			sb->mapped[i++] = nvhost_class_host_incr_syncpt_base(
						NVWAITBASE_3D, 1);
		}
		sb->mapped[i++] = nvhost_opcode_setclass(ch->desc->class,
						0, 0);
	}
	wmb();

	INIT_DELAYED_WORK(&cdma->timeout.wq, t20_cdma_timeout_handler);
	cdma->timeout.initialized = true;

	return 0;
fail:
	cdma_op(cdma).timeout_destroy(cdma);
	return -ENOMEM;
}

/**
 * Clean up timeout syncpt buffer resources
 */
static void t20_cdma_timeout_destroy(struct nvhost_cdma *cdma)
{
	struct nvmap_client *nvmap = cdma_to_nvmap(cdma);
	struct syncpt_buffer *sb = &cdma->syncpt_buffer;

	if (sb->mapped)
		nvmap_munmap(sb->mem, sb->mapped);

	if (sb->phys != 0)
		nvmap_unpin(nvmap, sb->mem);

	if (sb->mem)
		nvmap_free(nvmap, sb->mem);

	sb->mem = NULL;
	sb->mapped = NULL;
	sb->phys = 0;

	if (cdma->timeout.initialized)
		cancel_delayed_work(&cdma->timeout.wq);
	cdma->timeout.initialized = false;
}

/**
 * Increment timedout buffer's syncpt via CPU.
 */
static void t20_cdma_timeout_cpu_incr(struct nvhost_cdma *cdma, u32 getptr,
				u32 syncpt_incrs, u32 syncval, u32 nr_slots)
{
	struct nvhost_master *dev = cdma_to_dev(cdma);
	struct push_buffer *pb = &cdma->push_buffer;
	u32 i, getidx;

	for (i = 0; i < syncpt_incrs; i++)
		nvhost_syncpt_cpu_incr(&dev->syncpt, cdma->timeout.syncpt_id);

	/* after CPU incr, ensure shadow is up to date */
	nvhost_syncpt_update_min(&dev->syncpt, cdma->timeout.syncpt_id);

	/* update WAITBASE_3D by same number of incrs */
	if (cdma->timeout.syncpt_id == NVSYNCPT_3D) {
		void __iomem *p;
		p = dev->sync_aperture + HOST1X_SYNC_SYNCPT_BASE_0 +
				(NVWAITBASE_3D * sizeof(u32));
		writel(syncval, p);
	}

	/* NOP all the PB slots */
	getidx = getptr - pb->phys;
	while (nr_slots--) {
		u32 *p = (u32 *)((u32)pb->mapped + getidx);
		*(p++) = NVHOST_OPCODE_NOOP;
		*(p++) = NVHOST_OPCODE_NOOP;
		dev_dbg(&dev->pdev->dev, "%s: NOP at 0x%x\n",
			__func__, pb->phys + getidx);
		getidx = (getidx + 8) & (PUSH_BUFFER_SIZE - 1);
	}
	wmb();
}

/**
 * This routine is called at the point we transition back into a timed
 * ctx. The syncpts are incremented via pushbuffer with a flag indicating
 * whether there's a CTXSAVE that should be still executed (for the
 * preceding HW ctx).
 */
static void t20_cdma_timeout_pb_incr(struct nvhost_cdma *cdma, u32 getptr,
				u32 syncpt_incrs, u32 nr_slots,
				bool exec_ctxsave)
{
	struct nvhost_master *dev = cdma_to_dev(cdma);
	struct syncpt_buffer *sb = &cdma->syncpt_buffer;
	struct push_buffer *pb = &cdma->push_buffer;
	struct nvhost_userctx_timeout *timeout = cdma->timeout.ctx_timeout;
	u32 getidx, *p;

	/* should have enough slots to incr to desired count */
	BUG_ON(syncpt_incrs > (nr_slots * sb->incr_per_buffer));

	getidx = getptr - pb->phys;
	if (exec_ctxsave) {
		/* don't disrupt the CTXSAVE of a good/non-timed out ctx */
		nr_slots -= timeout->hwctx->save_slots;
		syncpt_incrs -= timeout->hwctx->save_incrs;

		getidx += (timeout->hwctx->save_slots * 8);
		getidx &= (PUSH_BUFFER_SIZE - 1);

		dev_dbg(&dev->pdev->dev,
			"%s: exec CTXSAVE of prev ctx (slots %d, incrs %d)\n",
			__func__, nr_slots, syncpt_incrs);
	}

	while (syncpt_incrs) {
		u32 incrs, count;

		/* GATHER count are incrs * number of DWORDs per incr */
		incrs = min(syncpt_incrs, sb->incr_per_buffer);
		count = incrs * sb->words_per_incr;

		p = (u32 *)((u32)pb->mapped + getidx);
		*(p++) = nvhost_opcode_gather(count);
		*(p++) = sb->phys;

		dev_dbg(&dev->pdev->dev,
			"%s: GATHER at 0x%x, from 0x%x, dcount = %d\n",
			__func__,
			pb->phys + getidx, sb->phys,
			(incrs * sb->words_per_incr));

		syncpt_incrs -= incrs;
		getidx = (getidx + 8) & (PUSH_BUFFER_SIZE - 1);
		nr_slots--;
	}

	/* NOP remaining slots */
	while (nr_slots--) {
		p = (u32 *)((u32)pb->mapped + getidx);
		*(p++) = NVHOST_OPCODE_NOOP;
		*(p++) = NVHOST_OPCODE_NOOP;
		dev_dbg(&dev->pdev->dev, "%s: NOP at 0x%x\n",
			__func__, pb->phys + getidx);
		getidx = (getidx + 8) & (PUSH_BUFFER_SIZE - 1);
	}
	wmb();
}

/**
 * Start channel DMA
 */
static void t20_cdma_start(struct nvhost_cdma *cdma)
{
	void __iomem *chan_regs = cdma_to_channel(cdma)->aperture;

	if (cdma->running)
		return;

	BUG_ON(!cdma_pb_op(cdma).putptr);
	cdma->last_put = cdma_pb_op(cdma).putptr(&cdma->push_buffer);

	writel(nvhost_channel_dmactrl(true, false, false),
		chan_regs + HOST1X_CHANNEL_DMACTRL);

	/* set base, put, end pointer (all of memory) */
	writel(0, chan_regs + HOST1X_CHANNEL_DMASTART);
	writel(cdma->last_put, chan_regs + HOST1X_CHANNEL_DMAPUT);
	writel(0xFFFFFFFF, chan_regs + HOST1X_CHANNEL_DMAEND);

	/* reset GET */
	writel(nvhost_channel_dmactrl(true, true, true),
		chan_regs + HOST1X_CHANNEL_DMACTRL);

	/* start the command DMA */
	writel(nvhost_channel_dmactrl(false, false, false),
		chan_regs + HOST1X_CHANNEL_DMACTRL);

	cdma->running = true;
}

/**
 * Similar to t20_cdma_start(), but rather than starting from an idle
 * state (where DMA GET is set to DMA PUT), on a timeout we restore
 * DMA GET from an explicit value (so DMA may again be pending).
 */
static void t20_cdma_timeout_restart(struct nvhost_cdma *cdma, u32 getptr)
{
	struct nvhost_master *dev = cdma_to_dev(cdma);
	void __iomem *chan_regs = cdma_to_channel(cdma)->aperture;

	if (cdma->running)
		return;

	BUG_ON(!cdma_pb_op(cdma).putptr);
	cdma->last_put = cdma_pb_op(cdma).putptr(&cdma->push_buffer);

	writel(nvhost_channel_dmactrl(true, false, false),
		chan_regs + HOST1X_CHANNEL_DMACTRL);

	/* set base, end pointer (all of memory) */
	writel(0, chan_regs + HOST1X_CHANNEL_DMASTART);
	writel(0xFFFFFFFF, chan_regs + HOST1X_CHANNEL_DMAEND);

	/* set GET, by loading the value in PUT (then reset GET) */
	writel(getptr, chan_regs + HOST1X_CHANNEL_DMAPUT);
	writel(nvhost_channel_dmactrl(true, true, true),
		chan_regs + HOST1X_CHANNEL_DMACTRL);

	dev_dbg(&dev->pdev->dev,
		"%s: DMA GET 0x%x, PUT HW 0x%x / shadow 0x%x\n",
		__func__,
		readl(chan_regs + HOST1X_CHANNEL_DMAGET),
		readl(chan_regs + HOST1X_CHANNEL_DMAPUT),
		cdma->last_put);

	/* deassert GET reset and set PUT */
	writel(nvhost_channel_dmactrl(true, false, false),
		chan_regs + HOST1X_CHANNEL_DMACTRL);
	writel(cdma->last_put, chan_regs + HOST1X_CHANNEL_DMAPUT);

	/* start the command DMA */
	writel(nvhost_channel_dmactrl(false, false, false),
		chan_regs + HOST1X_CHANNEL_DMACTRL);

	cdma->running = true;
}

/**
 * Kick channel DMA into action by writing its PUT offset (if it has changed)
 */
static void t20_cdma_kick(struct nvhost_cdma *cdma)
{
	u32 put;
	BUG_ON(!cdma_pb_op(cdma).putptr);

	put = cdma_pb_op(cdma).putptr(&cdma->push_buffer);

	if (put != cdma->last_put) {
		void __iomem *chan_regs = cdma_to_channel(cdma)->aperture;
		wmb();
		writel(put, chan_regs + HOST1X_CHANNEL_DMAPUT);
		cdma->last_put = put;
	}
}

static void t20_cdma_stop(struct nvhost_cdma *cdma)
{
	void __iomem *chan_regs = cdma_to_channel(cdma)->aperture;

	mutex_lock(&cdma->lock);
	if (cdma->running) {
		nvhost_cdma_wait_locked(cdma, CDMA_EVENT_SYNC_QUEUE_EMPTY);
		writel(nvhost_channel_dmactrl(true, false, false),
			chan_regs + HOST1X_CHANNEL_DMACTRL);
		cdma->running = false;
	}
	mutex_unlock(&cdma->lock);
}

/**
 * Retrieve the op pair at a slot offset from a DMA address
 */
void t20_cdma_peek(struct nvhost_cdma *cdma,
			  u32 dmaget, int slot, u32 *out)
{
	u32 offset = dmaget - cdma->push_buffer.phys;
	u32 *p = cdma->push_buffer.mapped;

	offset = ((offset + slot * 8) & (PUSH_BUFFER_SIZE - 1)) >> 2;
	out[0] = p[offset];
	out[1] = p[offset + 1];
}

/**
 * Stops both channel's command processor and CDMA immediately.
 * Also, tears down the channel and resets corresponding module.
 */
void t20_cdma_timeout_teardown_begin(struct nvhost_cdma *cdma)
{
	struct nvhost_master *dev = cdma_to_dev(cdma);
	struct nvhost_channel *ch = cdma_to_channel(cdma);
	u32 cmdproc_stop;

	BUG_ON(cdma->torndown);

	dev_dbg(&dev->pdev->dev,
		"begin channel teardown (channel id %d)\n", ch->chid);

	cmdproc_stop = readl(dev->sync_aperture + HOST1X_SYNC_CMDPROC_STOP);
	cmdproc_stop = nvhost_sync_cmdproc_stop_chid(cmdproc_stop, ch->chid);
	writel(cmdproc_stop, dev->sync_aperture + HOST1X_SYNC_CMDPROC_STOP);

	dev_dbg(&dev->pdev->dev,
		"%s: DMA GET 0x%x, PUT HW 0x%x / shadow 0x%x\n",
		__func__,
		readl(ch->aperture + HOST1X_CHANNEL_DMAGET),
		readl(ch->aperture + HOST1X_CHANNEL_DMAPUT),
		cdma->last_put);

	writel(nvhost_channel_dmactrl(true, false, false),
		ch->aperture + HOST1X_CHANNEL_DMACTRL);

	writel(BIT(ch->chid), dev->sync_aperture + HOST1X_SYNC_CH_TEARDOWN);
	nvhost_module_reset(&dev->pdev->dev, &ch->mod);

	cdma->running = false;
	cdma->torndown = true;
}

void t20_cdma_timeout_teardown_end(struct nvhost_cdma *cdma, u32 getptr)
{
	struct nvhost_master *dev = cdma_to_dev(cdma);
	struct nvhost_channel *ch = cdma_to_channel(cdma);
	u32 cmdproc_stop;

	BUG_ON(!cdma->torndown || cdma->running);

	dev_dbg(&dev->pdev->dev,
		"end channel teardown (id %d, DMAGET restart = 0x%x)\n",
		ch->chid, getptr);

	cmdproc_stop = readl(dev->sync_aperture + HOST1X_SYNC_CMDPROC_STOP);
	cmdproc_stop = nvhost_sync_cmdproc_run_chid(cmdproc_stop, ch->chid);
	writel(cmdproc_stop, dev->sync_aperture + HOST1X_SYNC_CMDPROC_STOP);

	cdma->torndown = false;
	t20_cdma_timeout_restart(cdma, getptr);
}

/**
 * If this timeout fires, it indicates the current sync_queue entry has
 * exceeded its TTL and the userctx should be timed out and remaining
 * submits already issued cleaned up (future submits return an error).
 */
static void t20_cdma_timeout_handler(struct work_struct *work)
{
	struct nvhost_cdma *cdma;
	struct nvhost_master *dev;
	struct nvhost_syncpt *sp;
	struct nvhost_channel *ch;

	u32 syncpt_val;

	u32 prev_cmdproc, cmdproc_stop;

	cdma = container_of(to_delayed_work(work), struct nvhost_cdma,
			    timeout.wq);
	dev = cdma_to_dev(cdma);
	sp = &dev->syncpt;
	ch = cdma_to_channel(cdma);

	mutex_lock(&cdma->lock);

	if (!cdma->timeout.ctx_timeout) {
		dev_dbg(&dev->pdev->dev,
			 "cdma_timeout: expired, but has NULL context\n");
		mutex_unlock(&cdma->lock);
		return;
	}

	/* stop processing to get a clean snapshot */
	prev_cmdproc = readl(dev->sync_aperture + HOST1X_SYNC_CMDPROC_STOP);
	cmdproc_stop = nvhost_sync_cmdproc_stop_chid(prev_cmdproc, ch->chid);
	writel(cmdproc_stop, dev->sync_aperture + HOST1X_SYNC_CMDPROC_STOP);

	dev_dbg(&dev->pdev->dev, "cdma_timeout: cmdproc was 0x%x is 0x%x\n",
		prev_cmdproc, cmdproc_stop);

	syncpt_val = nvhost_syncpt_update_min(&dev->syncpt,
			cdma->timeout.syncpt_id);

	/* has buffer actually completed? */
	if ((s32)(syncpt_val - cdma->timeout.syncpt_val) >= 0) {
		dev_dbg(&dev->pdev->dev,
			 "cdma_timeout: expired, but buffer had completed\n");
		/* restore */
		cmdproc_stop = nvhost_sync_cmdproc_run_chid(prev_cmdproc,
			ch->chid);
		writel(cmdproc_stop,
			dev->sync_aperture + HOST1X_SYNC_CMDPROC_STOP);
		mutex_unlock(&cdma->lock);
		return;
	}

	dev_warn(&dev->pdev->dev,
		"%s: timeout: %d (%s) ctx 0x%p, HW thresh %d, done %d\n",
		__func__,
		cdma->timeout.syncpt_id,
		syncpt_op(sp).name(sp, cdma->timeout.syncpt_id),
		cdma->timeout.ctx_timeout,
		syncpt_val, cdma->timeout.syncpt_val);

	/* stop HW, resetting channel/module */
	cdma_op(cdma).timeout_teardown_begin(cdma);

	nvhost_cdma_update_sync_queue(cdma, sp, &dev->pdev->dev);
	mutex_unlock(&cdma->lock);
}

int nvhost_init_t20_cdma_support(struct nvhost_master *host)
{
	host->op.cdma.start = t20_cdma_start;
	host->op.cdma.stop = t20_cdma_stop;
	host->op.cdma.kick = t20_cdma_kick;

	host->op.cdma.timeout_init = t20_cdma_timeout_init;
	host->op.cdma.timeout_destroy = t20_cdma_timeout_destroy;
	host->op.cdma.timeout_teardown_begin = t20_cdma_timeout_teardown_begin;
	host->op.cdma.timeout_teardown_end = t20_cdma_timeout_teardown_end;
	host->op.cdma.timeout_cpu_incr = t20_cdma_timeout_cpu_incr;
	host->op.cdma.timeout_pb_incr = t20_cdma_timeout_pb_incr;

	host->sync_queue_size = NVHOST_SYNC_QUEUE_SIZE;

	host->op.push_buffer.reset = t20_push_buffer_reset;
	host->op.push_buffer.init = t20_push_buffer_init;
	host->op.push_buffer.destroy = t20_push_buffer_destroy;
	host->op.push_buffer.push_to = t20_push_buffer_push_to;
	host->op.push_buffer.pop_from = t20_push_buffer_pop_from;
	host->op.push_buffer.space = t20_push_buffer_space;
	host->op.push_buffer.putptr = t20_push_buffer_putptr;

	return 0;
}
