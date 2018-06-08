/*
 * Intel SST FW Log Tracing
 *
 * Copyright (C) 2015-16, Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <sound/compress_driver.h>
#include "../common/sst-dsp.h"
#include "../common/sst-dsp-priv.h"
#include "skl-sst-ipc.h"
#include "skl.h"
#include "skl-fwlog.h"

#define DEF_LOG_PRIORITY 3

/*
 * Initialize trace window and firmware write pointers for the platform
 */
int skl_dsp_init_trace_window(struct sst_dsp *sst, u32 *wp, u32 offset,
				u32 size, int cores)
{
	int idx, alloc_size;
	void **dsp_wps;
	struct sst_dbg_rbuffer **buff;

	alloc_size = sizeof(buff) * cores;
	buff = devm_kzalloc(sst->dev, alloc_size, GFP_KERNEL);
	if (!buff)
		goto failure;

	dsp_wps = devm_kzalloc(sst->dev, sizeof(void *) * cores, GFP_KERNEL);

	if (!dsp_wps)
		goto failure;

	sst->trace_wind.addr = sst->addr.lpe + offset;
	sst->trace_wind.size = size;
	sst->trace_wind.nr_dsp  = cores;
	sst->trace_wind.flags = 0;
	sst->trace_wind.dbg_buffers = buff;
	sst->trace_wind.dsp_wps = dsp_wps;
	sst->trace_wind.log_priority = DEF_LOG_PRIORITY;
	for (idx = 0; idx < cores; idx++)
		sst->trace_wind.dsp_wps[idx] = (void *)(sst->addr.lpe
							+ wp[idx]);
	return 0;

failure:
	dev_err(sst->dev, "Trace buffer init failed for the platform\n");
	return -ENOMEM;
}

/*
 * Initialize ring buffer for a dsp fw logging
 */
int skl_dsp_init_log_buffer(struct sst_dsp *sst, int size,	int core,
				struct snd_compr_stream *stream)
{
	int ret = 0;
	struct sst_dbg_rbuffer *tmp;

	tmp = kzalloc(sizeof(*tmp), GFP_KERNEL);
	if (!tmp)
		return -ENOMEM;

	ret = kfifo_alloc(&tmp->fifo_dsp, size, GFP_KERNEL);
	if (!ret) {
		tmp->stream = stream;
		tmp->total_avail = 0;
		kref_init(&tmp->refcount);
		sst->trace_wind.dbg_buffers[core] = tmp;
	} else
		kfree(tmp);

	return ret;
}
EXPORT_SYMBOL_GPL(skl_dsp_init_log_buffer);

int update_dsp_log_priority(int value, struct skl *skl)
{
	int ret = 0;
	struct skl_sst *ctx = skl->skl_sst;

	ctx->dsp->trace_wind.log_priority = value;
	return ret;
}
EXPORT_SYMBOL_GPL(update_dsp_log_priority);

int get_dsp_log_priority(struct skl *skl)
{
	u32 value;
	struct skl_sst *ctx = skl->skl_sst;

	value = ctx->dsp->trace_wind.log_priority;
	return value;
}
EXPORT_SYMBOL_GPL(get_dsp_log_priority);

unsigned long skl_dsp_log_avail(struct sst_dsp *sst, int core)
{
	struct sst_dbg_rbuffer *buff = sst->trace_wind.dbg_buffers[core];

	if (buff->stream->runtime->state == SNDRV_PCM_STATE_XRUN)
		return 0;

	return buff->total_avail;
}
EXPORT_SYMBOL(skl_dsp_log_avail);

int skl_dsp_get_buff_users(struct sst_dsp *sst, int core)
{
	struct sst_dbg_rbuffer *buff = sst->trace_wind.dbg_buffers[core];

	return refcount_read(&buff->refcount.refcount);
}

void skl_dsp_write_log(struct sst_dsp *sst, void __iomem *src, int core,
				int count)
{
	int i;
	u32 *data = (u32 *)src;
	struct sst_dbg_rbuffer *buff = sst->trace_wind.dbg_buffers[core];

	if (buff->stream->runtime->state == SNDRV_PCM_STATE_XRUN)
		return;

	for (i = 0; i < count; i += 4) {
		if (!kfifo_put(&buff->fifo_dsp, *data)) {
			dev_err(sst->dev, "fw log buffer overrun on dsp %d\n",
					core);
			buff->stream->runtime->state = SNDRV_PCM_STATE_XRUN;
			break;
		}
		data++;
	}
	buff->total_avail += count;
	wake_up(&buff->stream->runtime->sleep);
}

int skl_dsp_copy_log_user(struct sst_dsp *sst, int core,
				void __user *dest, int count)
{
	int copied, ret;
	struct sst_dbg_rbuffer *buff = sst->trace_wind.dbg_buffers[core];

	ret = kfifo_to_user(&buff->fifo_dsp, dest, count, &copied);

	return ret ? ret : copied;
}
EXPORT_SYMBOL_GPL(skl_dsp_copy_log_user);

static void skl_dsp_free_log_buffer(struct kref *ref)
{
	struct sst_dbg_rbuffer *buff = container_of(ref, struct sst_dbg_rbuffer,
							refcount);
	kfifo_free(&buff->fifo_dsp);
	kfree(buff);
}

void skl_dsp_get_log_buff(struct sst_dsp *sst, int core)
{
	struct sst_dbg_rbuffer *buff = sst->trace_wind.dbg_buffers[core];

	kref_get(&buff->refcount);
}
EXPORT_SYMBOL_GPL(skl_dsp_get_log_buff);

void skl_dsp_put_log_buff(struct sst_dsp *sst, int core)
{
	struct sst_dbg_rbuffer *buff = sst->trace_wind.dbg_buffers[core];

	kref_put(&buff->refcount, skl_dsp_free_log_buffer);
}
EXPORT_SYMBOL_GPL(skl_dsp_put_log_buff);

void skl_dsp_done_log_buffer(struct sst_dsp *sst, int core)
{
	skl_dsp_put_log_buff(sst, core);
}
EXPORT_SYMBOL_GPL(skl_dsp_done_log_buffer);

/* Module Information */
MODULE_AUTHOR("Ashish Panwar <ashish.panwar@intel.com");
MODULE_DESCRIPTION("Intel SST FW Log Tracing");
MODULE_LICENSE("GPL v2");
