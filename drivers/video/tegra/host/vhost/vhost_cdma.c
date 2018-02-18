/*
 * Tegra Graphics Virtualization Host cdma for HOST1X
 *
 * Copyright (c) 2014-2015, NVIDIA Corporation. All rights reserved.
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
#include "vhost.h"
#include "../host1x/host1x.h"
#include "../nvhost_cdma.h"
#include "../nvhost_job.h"
#include "../dev.h"
#include "../bus_client.h"

static int vhost_pb_sendrecv(struct tegra_vhost_cmd_msg *msg, size_t size_in,
		size_t size_out)
{
	void *handle;
	size_t size = size_in;
	void *data = msg;
	int err;

	err = tegra_gr_comm_sendrecv(TEGRA_GR_COMM_CTX_CLIENT,
				tegra_gr_comm_get_server_vmid(),
				TEGRA_VHOST_QUEUE_PB, &handle, &data, &size);
	if (!err) {
		WARN_ON(size < size_out);
		memcpy(msg, data, size_out);
		tegra_gr_comm_release(handle);
	}

	return err;
}

static int vhost_channel_submit(u64 handle, struct nvhost_job *job,
		u32 *pb_base, u32 start, u32 end, u32 job_id)
{
	struct tegra_vhost_cmd_msg *msg;
	struct tegra_vhost_channel_submit_params *p;
	u32 num_entries, i;
	size_t size;
	char *ptr;
	u32 *ptr32;
	int err;

	/* number of opcode/data pairs (8 bytes each) */
	num_entries = ((end - start) & (PUSH_BUFFER_SIZE - 1)) / 8;

	size = sizeof(*msg) + 8 * (num_entries + job->num_syncpts);
	msg = kmalloc(size, GFP_KERNEL);
	if (!msg)
		return -1;

	msg->cmd = TEGRA_VHOST_CMD_HOST1X_CDMA_SUBMIT;
	msg->handle = handle;
	msg->ret = 0;
	p = &msg->params.cdma_submit;
	p->clientid = job->clientid;
	p->job_id = job_id;
	p->timeout = job->timeout;
	p->num_entries = num_entries;
	p->num_syncpts = job->num_syncpts;

	ptr = (char *)msg + sizeof(*msg);

	/* Copy pushbuffer contents first */
	if ((start < end) || (end == 0)) {
		memcpy(ptr, (u8 *)pb_base + start, 8 * num_entries);
		ptr += 8 * num_entries;
	}
	else {
		memcpy(ptr, (u8 *)pb_base + start, PUSH_BUFFER_SIZE - start);
		ptr += PUSH_BUFFER_SIZE - start;
		memcpy(ptr, pb_base, end);
		ptr += end;
	}

	/* Now update syncpt information */
	ptr32 = (u32 *)ptr;
	for (i = 0; i < p->num_syncpts; i++) {
		struct nvhost_job_syncpt *sp = job->sp + i;
		*ptr32++ = sp->id;
		*ptr32++ = sp->fence;
	}

	err = vhost_pb_sendrecv(msg, size, sizeof(*msg));

	err = (err || msg->ret) ? -1 : 0;
	kfree(msg);
	return err;
}

static void vhost_cdma_start(struct nvhost_cdma *cdma)
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
	cdma->running = true;
}

static void vhost_cdma_stop(struct nvhost_cdma *cdma)
{
	struct nvhost_channel *ch = cdma_to_channel(cdma);

	if (!ch || !ch->dev) {
		pr_warn("%s: un-mapped channel\n", __func__);
		return;
	}

	mutex_lock(&cdma->lock);
	if (cdma->running) {
		nvhost_cdma_wait_locked(cdma, CDMA_EVENT_SYNC_QUEUE_EMPTY);
		cdma->running = false;
	}
	mutex_unlock(&cdma->lock);
}

/**
 * Kick channel DMA into action by writing its PUT offset (if it has changed)
 */
static void vhost_cdma_kick(struct nvhost_cdma *cdma)
{
	struct nvhost_job *job;
	u32 put;
	int err;

	job = list_entry(cdma->sync_queue.prev, struct nvhost_job, list);

	put = nvhost_push_buffer_putptr(&cdma->push_buffer);

	if (put != cdma->last_put) {
		u32 start = cdma->last_put - cdma->push_buffer.dma_addr;
		u32 end = put - cdma->push_buffer.dma_addr;
		struct nvhost_channel *ch = cdma_to_channel(cdma);
		struct nvhost_virt_ctx *virt_ctx =
						nvhost_get_virt_data(ch->dev);

		/* Don't hold the lock while we're waiting
		 * for this to complete
		 */
		mutex_unlock(&cdma->lock);
		err = vhost_channel_submit(virt_ctx->handle, job,
					cdma->push_buffer.mapped,
					start, end, cdma->last_put);
		mutex_lock(&cdma->lock);
		if (err)
			pr_err("%s: error return from host1x_cdma_kick\n",
				__func__);
		cdma->last_put = put;
	}
}

/*
 * Timeout functions are only to detect the case that push buffer finished
 * running, but sync points did not all get incremented.
 */
static int vhost_cdma_timeout_init(struct nvhost_cdma *cdma,
				 u32 syncpt_id)
{
	return 0;
}

static void vhost_cdma_timeout_destroy(struct nvhost_cdma *cdma)
{
}

static void vhost_cdma_timeout_pb_cleanup(struct nvhost_cdma *cdma, u32 getptr,
				u32 nr_slots)
{
}

static void vhost_cdma_timeout_teardown_begin(struct nvhost_cdma *cdma, bool skip_reset)
{
}

static void vhost_cdma_timeout_teardown_end(struct nvhost_cdma *cdma,
				u32 getptr)
{
}

void vhost_init_host1x_cdma_ops(struct nvhost_cdma_ops *ops)
{
	ops->start = vhost_cdma_start;
	ops->stop = vhost_cdma_stop;
	ops->kick = vhost_cdma_kick;

	ops->timeout_init = vhost_cdma_timeout_init;
	ops->timeout_destroy = vhost_cdma_timeout_destroy;
	ops->timeout_teardown_begin = vhost_cdma_timeout_teardown_begin;
	ops->timeout_teardown_end = vhost_cdma_timeout_teardown_end;
	ops->timeout_pb_cleanup = vhost_cdma_timeout_pb_cleanup;
}

void vhost_cdma_timeout(struct nvhost_master *dev,
			struct tegra_vhost_chan_timeout_intr_info *info)
{
	struct nvhost_channel *chan;
	struct nvhost_cdma *cdma;
	struct nvhost_job *job, *start_job = NULL, *end_job = NULL;
	struct platform_device *pdev;
	int moduleid = vhost_moduleid_virt_to_hw((int)info->module_id);

	pdev = nvhost_device_list_match_by_id(moduleid);
	if (unlikely(!pdev)) {
		dev_err(&dev->dev->dev, "chan timeout: invlid module id %d\n",
					(int)info->module_id);
		return;
	}
	chan = nvhost_find_chan_by_clientid(pdev, (int)info->client_id);
	if (unlikely(!chan)) {
		dev_err(&dev->dev->dev, "chan timeout: invalid client id %d\n",
					info->client_id);
		return;
	}

	cdma = &chan->cdma;
	mutex_lock(&cdma->lock);

	/* whether all timeout jobs have been unpinned */
	list_for_each_entry(job, &cdma->sync_queue, list) {
		if (job->first_get == info->job_id_start)
			start_job = job;
		if (job->first_get == info->job_id_end) {
			end_job = job;
			break;
		}
	}

	if (!end_job)
		goto out;

	job = start_job;
	list_for_each_entry_from(job, &cdma->sync_queue, list) {
		int i;

		/* set notifier to userspace about submit timeout */
		nvhost_job_set_notifier(job, NVHOST_CHANNEL_SUBMIT_TIMEOUT);

		for (i = 0; i < job->num_syncpts; ++i) {
			struct nvhost_job_syncpt *sp = job->sp + i;

			nvhost_syncpt_update_min(&dev->syncpt, sp->id);
		}

		if (job == end_job)
			break;
	}

out:
	mutex_unlock(&cdma->lock);
}
