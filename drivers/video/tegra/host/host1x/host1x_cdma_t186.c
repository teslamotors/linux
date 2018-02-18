/*
 * Tegra Graphics Host Command DMA
 *
 * Copyright (c) 2014-2016, NVIDIA Corporation. All rights reserved.
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

#include <linux/slab.h>
#include <linux/scatterlist.h>
#include <linux/delay.h>

#include "nvhost_acm.h"
#include "nvhost_cdma.h"
#include "nvhost_channel.h"
#include "debug.h"
#include "dev.h"
#include "class_ids.h"
#include "chip_support.h"
#include "nvhost_job.h"
#include "nvhost_vm.h"

static inline u32 host1x_channel_dmactrl(int stop, int get_rst, int init_get)
{
	return host1x_channel_dmactrl_dmastop_f(stop)
		| host1x_channel_dmactrl_dmagetrst_f(get_rst)
		| host1x_channel_dmactrl_dmainitget_f(init_get);
}

static void cdma_timeout_handler(struct work_struct *work);

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
static void push_buffer_reset(struct push_buffer *pb)
{
	pb->fence = PUSH_BUFFER_SIZE - 8;
	pb->cur = 0;
}

/**
 * Init push buffer resources
 */
static int push_buffer_init(struct push_buffer *pb)
{
	int err = 0;

	push_buffer_reset(pb);

	err = nvhost_push_buffer_alloc(pb);

	if (err)
		goto fail;

	/* put the restart at the end of pushbuffer memory */
	*(pb->mapped + (PUSH_BUFFER_SIZE >> 2)) =
		nvhost_opcode_restart(pb->dma_addr);

	return 0;

fail:
	nvhost_push_buffer_destroy(pb);
	return err;
}

/**
 * Guarantees that the next N slots (2 words each ) pushed to the buffer will be
 * adjacent and not split across pushbuffer wraparound boundaries. Pushes no-ops
 * to pushbuffer until the boundary has been avoided. The number of words must
 * be less than (PUSH_BUFFER_SIZE - 1)/2.
 */
static void cdma_make_adjacent_space(struct nvhost_cdma *cdma, u32 slots)
{
	int i;
	u32 slots_before_wrap = (PUSH_BUFFER_SIZE - 1 - cdma->push_buffer.cur)/8;

	if (WARN_ON(slots >= (NVHOST_GATHER_QUEUE_SIZE)/2))
		return;

	if (slots_before_wrap >= slots)
		return;

	/* fill the end of the buffer with noops */
	for (i = 0; i < slots_before_wrap; i++)
		nvhost_cdma_push(cdma, NVHOST_OPCODE_NOOP, NVHOST_OPCODE_NOOP);
}

/*
 * The syncpt incr buffer is filled with methods to increment syncpts, which
 * is later GATHER-ed into the mainline PB. It's used when a timed out context
 * is interleaved with other work, so needs to inline the syncpt increments
 * to maintain the count (but otherwise does no work).
 */

/**
 * Init timeout resources
 */
static int cdma_timeout_init(struct nvhost_cdma *cdma,
				 u32 syncpt_id)
{
	INIT_DELAYED_WORK(&cdma->timeout.wq, cdma_timeout_handler);
	cdma->timeout.initialized = true;

	return 0;
}

/**
 * Clean up timeout resources
 */
static void cdma_timeout_destroy(struct nvhost_cdma *cdma)
{
	if (cdma->timeout.initialized)
		cancel_delayed_work(&cdma->timeout.wq);
	cdma->timeout.initialized = false;
}

/**
 * Increment timedout buffer's syncpt via CPU.
 */
static void cdma_timeout_pb_cleanup(struct nvhost_cdma *cdma, u32 getptr,
				u32 nr_slots)
{
	struct nvhost_master *dev = cdma_to_dev(cdma);
	struct push_buffer *pb = &cdma->push_buffer;
	u32 getidx;

	/* NOP all the PB slots */
	getidx = getptr - pb->dma_addr;
	while (nr_slots--) {
		u32 *p = (u32 *)((uintptr_t)pb->mapped + getidx);
		*(p++) = NVHOST_OPCODE_NOOP;
		*(p++) = NVHOST_OPCODE_NOOP;
		dev_dbg(&dev->dev->dev, "%s: NOP at 0x%llx\n",
			__func__, (u64)(pb->dma_addr + getidx));
		getidx = (getidx + 8) & (PUSH_BUFFER_SIZE - 1);
	}
	wmb();
}

/**
 * Start channel DMA
 */
static void cdma_start(struct nvhost_cdma *cdma)
{
	struct nvhost_channel *ch;

	if (cdma->running)
		return;

	ch = cdma_to_channel(cdma);
	if (!ch || !ch->dev) {
		pr_err("%s: channel already un-mapped\n", __func__);
		return;
	}

	cdma->last_put = nvhost_push_buffer_putptr(&cdma->push_buffer);

	host1x_channel_writel(ch, host1x_channel_dmactrl_r(),
			host1x_channel_dmactrl(true, false, false));

	/* set base, put, end pointer (all of memory) */
	host1x_channel_writel(ch, host1x_channel_dmastart_r(), 0);
	host1x_channel_writel(ch, host1x_channel_dmaput_r(), cdma->last_put);
	host1x_channel_writel(ch, host1x_channel_dmaend_r(), 0xFFFFFFFF);

	/* reset GET */
	host1x_channel_writel(ch, host1x_channel_dmactrl_r(),
			host1x_channel_dmactrl(true, true, true));

	/* start the command DMA */
	wmb();
	host1x_channel_writel(ch, host1x_channel_dmactrl_r(),
			host1x_channel_dmactrl(false, false, false));

	cdma->running = true;
}

/**
 * Similar to cdma_start(), but rather than starting from an idle
 * state (where DMA GET is set to DMA PUT), on a timeout we restore
 * DMA GET from an explicit value (so DMA may again be pending).
 */
static void cdma_timeout_restart(struct nvhost_cdma *cdma, u32 getptr)
{
	struct nvhost_master *dev = cdma_to_dev(cdma);
	struct nvhost_channel *ch = cdma_to_channel(cdma);

	if (cdma->running)
		return;

	cdma->last_put = nvhost_push_buffer_putptr(&cdma->push_buffer);

	host1x_channel_writel(ch, host1x_channel_dmactrl_r(),
			host1x_channel_dmactrl(true, false, false));

	/* set base, end pointer (all of memory) */
	host1x_channel_writel(ch, host1x_channel_dmastart_r(), 0);
	host1x_channel_writel(ch, host1x_channel_dmaend_r(), 0xFFFFFFFF);

	/* set GET, by loading the value in PUT (then reset GET) */
	host1x_channel_writel(ch, host1x_channel_dmaput_r(), getptr);
	host1x_channel_writel(ch, host1x_channel_dmaput_hi_r(), 0);
	host1x_channel_writel(ch, host1x_channel_dmactrl_r(),
			host1x_channel_dmactrl(true, true, true));

	dev_dbg(&dev->dev->dev,
		"%s: DMA GET 0x%x, PUT HW 0x%x / shadow 0x%x\n",
		__func__,
		host1x_channel_readl(ch, host1x_channel_dmaget_r()),
		host1x_channel_readl(ch, host1x_channel_dmaput_r()),
		cdma->last_put);

	/* deassert GET reset and set PUT */
	host1x_channel_writel(ch, host1x_channel_dmactrl_r(),
			host1x_channel_dmactrl(true, false, false));
	host1x_channel_writel(ch, host1x_channel_dmaput_r(), cdma->last_put);

	/* reinitialise gather filter for the channel */
	nvhost_channel_init_gather_filter(dev->dev, ch);

	/* start the command DMA */
	wmb();
	host1x_channel_writel(ch, host1x_channel_dmactrl_r(),
			host1x_channel_dmactrl(false, false, false));

	cdma->running = true;
}

/**
 * Kick channel DMA into action by writing its PUT offset (if it has changed)
 */
static void cdma_kick(struct nvhost_cdma *cdma)
{
	u32 put;
	struct nvhost_channel *ch = cdma_to_channel(cdma);

	put = nvhost_push_buffer_putptr(&cdma->push_buffer);

	if (put != cdma->last_put) {
		wmb();
		host1x_channel_writel(ch, host1x_channel_dmaput_r(), put);
		cdma->last_put = put;
	}
}

static void cdma_stop(struct nvhost_cdma *cdma)
{
	struct nvhost_channel *ch = cdma_to_channel(cdma);

	if (!ch || !ch->dev) {
		pr_warn("%s: un-mapped channel\n", __func__);
		return;
	}

	mutex_lock(&cdma->lock);
	if (cdma->running) {
		nvhost_cdma_wait_locked(cdma, CDMA_EVENT_SYNC_QUEUE_EMPTY);
		host1x_channel_writel(ch, host1x_channel_dmactrl_r(),
				host1x_channel_dmactrl(true, false, false));
		cdma->running = false;
	}
	mutex_unlock(&cdma->lock);
}

/**
 * Stops both channel's command processor and CDMA immediately.
 * Also, tears down the channel and resets corresponding module.
 */
static void cdma_timeout_teardown_begin(struct nvhost_cdma *cdma, bool skip_reset)
{
	struct nvhost_channel *ch = cdma_to_channel(cdma);
	struct nvhost_master *dev;
	u32 cmdproc_stop;

	dev = cdma_to_dev(cdma);
	if (cdma->torndown && !cdma->running) {
		dev_warn(&dev->dev->dev, "Already torn down\n");
		return;
	}

	dev_dbg(&dev->dev->dev,
		"begin channel teardown (channel id %d)\n", ch->chid);

	cmdproc_stop = host1x_channel_readl(ch, host1x_sync_cmdproc_stop_r());
	cmdproc_stop |= BIT(0);
	host1x_channel_writel(ch, host1x_sync_cmdproc_stop_r(), cmdproc_stop);

	dev_dbg(&dev->dev->dev,
		"%s: DMA GET 0x%x, PUT HW 0x%x / shadow 0x%x\n",
		__func__,
		host1x_channel_readl(ch, host1x_channel_dmaget_r()),
		host1x_channel_readl(ch, host1x_channel_dmaput_r()),
		cdma->last_put);

	host1x_channel_writel(ch, host1x_channel_dmactrl_r(),
			host1x_channel_dmactrl(true, false, false));

	host1x_channel_writel(ch, host1x_sync_ch_teardown_r(), BIT(0));

	if (!skip_reset && nvhost_channel_is_reset_required(ch))
		nvhost_module_reset(ch->dev, true);

	cdma->running = false;
	cdma->torndown = true;
}

static void cdma_timeout_teardown_end(struct nvhost_cdma *cdma, u32 getptr)
{
	struct nvhost_master *dev;
	struct nvhost_channel *ch = cdma_to_channel(cdma);
	u32 cmdproc_stop;

	dev = cdma_to_dev(cdma);
	dev_dbg(&dev->dev->dev,
		"end channel teardown (id %d, DMAGET restart = 0x%x)\n",
		ch->chid, getptr);

	cmdproc_stop = host1x_channel_readl(ch, host1x_sync_cmdproc_stop_r());
	cmdproc_stop &= ~(BIT(0));
	host1x_channel_writel(ch, host1x_sync_cmdproc_stop_r(), cmdproc_stop);

	cdma->torndown = false;
	cdma_timeout_restart(cdma, getptr);
}

static void cdma_timeout_release_mlock(struct nvhost_cdma *cdma)
{
	struct nvhost_channel *orig_ch = cdma_to_channel(cdma);
	struct nvhost_master *dev = cdma_to_dev(cdma);
	struct nvhost_syncpt *syncpt = &dev->syncpt;
	struct platform_device *pdev = orig_ch->dev;
	struct nvhost_device_data *pdata = platform_get_drvdata(pdev);
	struct nvhost_job *job = NULL;
	struct nvhost_channel *ch;
	dma_addr_t dma_handle = 0;
	DEFINE_DMA_ATTRS(attrs);
	u32 *cpuvaddr = NULL;
	bool ch_own, cpu_own;
	unsigned int owner;
	u32 syncpt_id;
	int err = 0;

	/* nothing to do if resource policy is per device */
	if (pdata->resource_policy == RESOURCE_PER_DEVICE)
		return;

	/* read the owner */
	syncpt_op().mutex_owner(syncpt, pdata->modulemutexes[0],
				&cpu_own, &ch_own, &owner);

	/* if this channel does not own the mlock, quit */
	if (!(ch_own && owner == orig_ch->chid) &&
	    !nvhost_dev_is_virtual(pdev))
		return;

	/* allocate a new channel to execute recovery. use a stack variable
	 * as an identifier to ensure that no-one else can get the same
	 * channel */

	err = nvhost_channel_map(pdata, &ch, &ch);
	if (err) {
		nvhost_err(&pdev->dev, "mlock release failed: failed to allocate recovery channel (%d)\n",
			   err);
		return;
	}

	/* wipe-out the identifier - no-one gets this channel before
	 * the job is done */
	nvhost_channel_remove_identifier(pdata, &ch);

	/* allocate a command buffer */
	cpuvaddr = dma_alloc_attrs(pdev->dev.parent, SZ_4K,
				   &dma_handle, GFP_KERNEL,
				   &attrs);
	if (!cpuvaddr) {
		nvhost_err(&pdev->dev, "mlock release failed: failed to allocate release buffer\n");
		goto err_alloc_buffer;
	}

	/* allocate syncpoint */
	syncpt_id = nvhost_get_syncpt_host_managed(pdata->pdev, 0, NULL);
	if (!syncpt_id) {
		nvhost_err(&pdev->dev, "mlock release failed: failed to allocate syncpoint\n");
		goto err_alloc_syncpt;
	}

	/* make a single increment for the command buffer */
	*cpuvaddr = nvhost_opcode_imm_incr_syncpt(0, syncpt_id);

	job = nvhost_job_alloc(ch, 1, 0, 0, 1);
	if (!job) {
		nvhost_err(&pdev->dev, "mlock release failed: failed to allocate job (%d)\n",
			   err);
		goto err_job_alloc;
	}

	ch->syncpts[0] = syncpt_id;
	job->sp->id = syncpt_id;
	job->sp->incrs = 1;
	job->num_syncpts = 1;

	/* Make increment in device class to force acquiring the mlock */
	err = nvhost_job_add_client_gather_address(job, 1, pdata->class,
						   dma_handle);
	if (err) {
		nvhost_err(&pdev->dev, "mlock release failed: failed to add gather address (%d)\n",
			   err);
		goto err_add_gather;
	}

	/* submit work */
	err = nvhost_channel_submit(job);
	if (err) {
		nvhost_err(&pdev->dev, "mlock release failed: submit failed (%d)\n",
			   err);
		goto err_submit;
	}

	/* WAR to Host1x MLOCK mechanism. Re-acquiring MLOCK for the
	 * same channel may not succeed automatically but we need to
	 * assign the MLOCK using register write */

	while (true) {
		/* Wait a moment */
		mdelay(10);

		/* If MLOCK is no longer assigned for this channel, quit */
		syncpt_op().mutex_owner(syncpt, pdata->modulemutexes[0],
					&cpu_own, &ch_own, &owner);
		if (!ch_own || (owner != orig_ch->chid && owner != ch->chid))
			break;

		/* ..otherwise, reassign MLOCK for this channel */
		host1x_hypervisor_writel(dev->dev,
				 host1x_sync_common_mlock_r() +
				 pdata->modulemutexes[0] * 4,
				 host1x_sync_common_mlock_ch_f(ch->chid) |
				 host1x_sync_common_mlock_locked_f(true));
	}

	/* Wait until the MLOCK is released */
	nvhost_syncpt_wait_timeout_ext(pdev, job->sp->id, job->sp->fence,
				       (u32)MAX_SCHEDULE_TIMEOUT, NULL, NULL);
err_submit:
err_add_gather:
	nvhost_job_put(job);
err_job_alloc:
	nvhost_syncpt_put_ref(syncpt, syncpt_id);
err_alloc_syncpt:
	dma_free_attrs(pdev->dev.parent, SZ_4K, cpuvaddr, dma_handle, &attrs);
err_alloc_buffer:
	nvhost_putchannel(ch, 1);
}

static void cdma_handle_timeout(struct nvhost_cdma *cdma, bool skip_reset)
{
	struct nvhost_master *dev;
	struct nvhost_syncpt *sp;
	struct nvhost_channel *ch;
	int ret;
	bool completed;
	int i;

	u32 syncpt_val;

	u32 prev_cmdproc, cmdproc_stop;

	ch = cdma_to_channel(cdma);
	if (!ch || !ch->dev) {
		pr_warn("%s: Channel un-mapped\n", __func__);
		return;
	}

	dev = cdma_to_dev(cdma);
	sp = &dev->syncpt;

	mutex_lock(&dev->timeout_mutex);

	if (skip_reset) { /* channel_abort() path */
		mutex_lock(&cdma->lock);
	} else {
		ret = mutex_trylock(&cdma->lock);
		if (!ret) {
			schedule_delayed_work(&cdma->timeout.wq,
					      msecs_to_jiffies(10));
			mutex_unlock(&dev->timeout_mutex);
			return;
		}
	}

	if (!cdma->timeout.clientid) {
		dev_dbg(&dev->dev->dev,
			 "cdma_timeout: expired, but has no clientid\n");
		mutex_unlock(&cdma->lock);
		mutex_unlock(&dev->timeout_mutex);
		return;
	}

	/* stop processing to get a clean snapshot */
	prev_cmdproc = host1x_channel_readl(ch, host1x_sync_cmdproc_stop_r());
	cmdproc_stop = prev_cmdproc | BIT(0);
	host1x_channel_writel(ch, host1x_sync_cmdproc_stop_r(), cmdproc_stop);

	dev_dbg(&dev->dev->dev, "cdma_timeout: cmdproc was 0x%x is 0x%x\n",
		prev_cmdproc, cmdproc_stop);

	completed = true;
	for (i = 0; i < cdma->timeout.num_syncpts; ++i) {
		syncpt_val = nvhost_syncpt_update_min(&dev->syncpt,
				cdma->timeout.sp[i].id);

		if (!nvhost_syncpt_is_expired(&dev->syncpt,
			cdma->timeout.sp[i].id, cdma->timeout.sp[i].fence))
			completed = false;
	}

	/* has buffer actually completed? */
	if (completed) {
		dev_dbg(&dev->dev->dev,
			 "cdma_timeout: expired, but buffer had completed\n");
		/* restore */
		cmdproc_stop = prev_cmdproc & ~(BIT(ch->chid));
		host1x_channel_writel(ch, host1x_sync_cmdproc_stop_r(),
				cmdproc_stop);
		mutex_unlock(&cdma->lock);
		mutex_unlock(&dev->timeout_mutex);
		return;
	}

	if (nvhost_debug_force_timeout_dump ||
			cdma->timeout.timeout_debug_dump) {
		for (i = 0; i < cdma->timeout.num_syncpts; ++i) {
			syncpt_val = nvhost_syncpt_read_min(&dev->syncpt,
					cdma->timeout.sp[i].id);
			dev_warn(&dev->dev->dev,
				"%s: timeout: %d (%s) clientid %d, HW thresh %d, done %d\n",
				__func__, cdma->timeout.sp[i].id,
				syncpt_op().name(sp, cdma->timeout.sp[i].id),
				cdma->timeout.clientid, syncpt_val,
				cdma->timeout.sp[i].fence);
		}

		nvhost_debug_dump_locked(cdma_to_dev(cdma), ch->chid);
	}

	/* stop HW, resetting channel/module */
	cdma_op().timeout_teardown_begin(cdma, skip_reset);

	nvhost_cdma_update_sync_queue(cdma, sp, ch->dev);
	mutex_unlock(&cdma->lock);
	mutex_unlock(&dev->timeout_mutex);

	/* ensure that mlocks get released */
	cdma_timeout_release_mlock(cdma);
}

/**
 * If this timeout fires, it indicates the current sync_queue entry has
 * exceeded its TTL and the userctx should be timed out and remaining
 * submits already issued cleaned up (future submits return an error).
 */
static void cdma_timeout_handler(struct work_struct *work)
{
	struct nvhost_cdma *cdma;

	cdma = container_of(to_delayed_work(work), struct nvhost_cdma,
			    timeout.wq);
	cdma_op().handle_timeout(cdma, false);
}

static const struct nvhost_cdma_ops host1x_cdma_ops = {
	.start = cdma_start,
	.stop = cdma_stop,
	.kick = cdma_kick,

	.timeout_init = cdma_timeout_init,
	.timeout_destroy = cdma_timeout_destroy,
	.timeout_teardown_begin = cdma_timeout_teardown_begin,
	.timeout_teardown_end = cdma_timeout_teardown_end,
	.timeout_pb_cleanup = cdma_timeout_pb_cleanup,
	.handle_timeout = cdma_handle_timeout,
	.make_adjacent_space = cdma_make_adjacent_space,
};

static const struct nvhost_pushbuffer_ops host1x_pushbuffer_ops = {
	.init = push_buffer_init,
};

