/*
 * drivers/video/tegra/host/nvhost_channel.c
 *
 * Tegra Graphics Host Channel
 *
 * Copyright (c) 2010-2016, NVIDIA Corporation.  All rights reserved.
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

#include "nvhost_channel.h"
#include "dev.h"
#include "nvhost_acm.h"
#include "nvhost_job.h"
#include "nvhost_vm.h"
#include "chip_support.h"
#include "vhost/vhost.h"

#include <trace/events/nvhost.h>
#include <linux/nvhost_ioctl.h>
#include <linux/delay.h>
#include <linux/nvhost.h>
#include <linux/slab.h>

#define NVHOST_CHANNEL_LOW_PRIO_MAX_WAIT 50

/* Memory allocation for all supported channels */
int nvhost_alloc_channels(struct nvhost_master *host)
{
	struct nvhost_channel *ch;
	int index, err;

	host->chlist = kzalloc(nvhost_channel_nb_channels(host) *
			       sizeof(struct nvhost_channel *), GFP_KERNEL);
	if (host->chlist == NULL)
		return -ENOMEM;

	mutex_init(&host->chlist_mutex);
	mutex_init(&host->ch_alloc_mutex);

	for (index = 0;	index < nvhost_channel_nb_channels(host); index++) {
		ch = kzalloc(sizeof(*ch), GFP_KERNEL);
		if (!ch) {
			dev_err(&host->dev->dev, "failed to alloc channels\n");
			return -ENOMEM;
		}

		/* initialize data structures */
		nvhost_set_chanops(ch);
		mutex_init(&ch->submitlock);
		mutex_init(&ch->syncpts_lock);
		ch->chid = nvhost_channel_get_id_from_index(host, index);

		/* initialize channel cdma */
		err = nvhost_cdma_init(host->dev, &ch->cdma);
		if (err) {
			dev_err(&host->dev->dev, "failed to initialize cdma\n");
			return err;
		}

		/* initialize hw specifics */
		err = channel_op(ch).init(ch, host);
		if (err < 0) {
			dev_err(&host->dev->dev, "failed to init channel %d\n",
				ch->chid);
			return err;
		}

		/* store the channel */
		host->chlist[index] = ch;

		/* initialise gather filter for the channel */
		nvhost_channel_init_gather_filter(host->dev, ch);
	}

	return 0;
}

int nvhost_channel_remove_identifier(struct nvhost_device_data *pdata,
			void *identifier)
{
	struct nvhost_master *host = nvhost_get_host(pdata->pdev);
	struct nvhost_channel *ch = NULL;
	int index, max_channels;

	mutex_lock(&host->chlist_mutex);

	max_channels = nvhost_channel_nb_channels(host);

	for (index = 0; index < max_channels; index++) {
		ch = host->chlist[index];
		if (ch->identifier == identifier) {
			ch->identifier = NULL;
			mutex_unlock(&host->chlist_mutex);
			return 0;
		}
	}

	mutex_unlock(&host->chlist_mutex);
	return 0;
}

/* Unmap channel from device and free all resources, deinit device */
static void nvhost_channel_unmap_locked(struct kref *ref)
{
	struct nvhost_channel *ch = container_of(ref, struct nvhost_channel,
							refcount);
	struct nvhost_device_data *pdata;
	struct nvhost_master *host;
	int i = 0;
	int index;
	int err;

	if (!ch->dev) {
		pr_err("%s: freeing unmapped channel\n", __func__);
		return;
	}

	pdata = platform_get_drvdata(ch->dev);
	host = nvhost_get_host(pdata->pdev);

	err = nvhost_module_busy(host->dev);
	if (err) {
		WARN(1, "failed to power-up host1x. leaking syncpts");
		goto err_module_busy;
	}

	/* turn off channel cdma */
	channel_cdma_op().stop(&ch->cdma);

	/* log this event */
	dev_dbg(&ch->dev->dev, "channel %d un-mapped\n", ch->chid);
	trace_nvhost_channel_unmap_locked(pdata->pdev->name, ch->chid,
		pdata->num_mapped_chs);

	/* Release channel syncpoints */
	for (i = 0; i < NVHOST_MODULE_MAX_SYNCPTS; ++i) {
		/* skip over unused syncpoints */
		if (!ch->syncpts[i])
			continue;

		/* first, mark syncpoint as unused by hardware */
		nvhost_syncpt_mark_unused(&host->syncpt, ch->syncpts[i]);

		/* drop syncpoints reference if we allocate syncpoints
		 * per channels
		 */
		if (pdata->resource_policy == RESOURCE_PER_DEVICE)
			nvhost_syncpt_put_ref(&host->syncpt, ch->syncpts[i]);

		/* finally, clear information from channel bookkeeping */
		ch->syncpts[i] = 0;
	}

	if (ch->client_managed_syncpt) {
		/* mark syncpoint as unused */
		nvhost_syncpt_mark_unused(&host->syncpt,
					  ch->client_managed_syncpt);

		/* release it */
		if (pdata->resource_policy == RESOURCE_PER_DEVICE)
			nvhost_syncpt_put_ref(&host->syncpt,
					ch->client_managed_syncpt);

		/* ..and handle bookkeeping */
		ch->client_managed_syncpt = 0;
	}

	nvhost_module_idle(host->dev);

err_module_busy:

	/* drop reference to the vm */
	nvhost_vm_put(ch->vm);

	index = nvhost_channel_get_index_from_id(host, ch->chid);
	clear_bit(index, host->allocated_channels);

	ch->vm = NULL;
	ch->dev = NULL;
	ch->identifier = NULL;
}

/* Maps free channel with device */
int nvhost_channel_map(struct nvhost_device_data *pdata,
			struct nvhost_channel **channel,
			void *identifier)
{
	struct nvhost_master *host = NULL;
	struct nvhost_channel *ch = NULL;
	int max_channels = 0;
	int index = 0;

	if (!pdata) {
		pr_err("%s: NULL device data\n", __func__);
		return -EINVAL;
	}

	host = nvhost_get_host(pdata->pdev);

	mutex_lock(&host->ch_alloc_mutex);
	mutex_lock(&host->chlist_mutex);
	max_channels = nvhost_channel_nb_channels(host);

	/* check if the channel is still in use */
	for (index = 0; index < max_channels; index++) {
		ch = host->chlist[index];
		if (ch->identifier == identifier
			&& kref_get_unless_zero(&ch->refcount)) {
			/* yes, client can continue using it */
			*channel = ch;
			mutex_unlock(&host->chlist_mutex);
			mutex_unlock(&host->ch_alloc_mutex);

			trace_nvhost_channel_remap(pdata->pdev->name, ch->chid,
						 pdata->num_mapped_chs,
						 identifier);
			return 0;
		}
	}

	do {
		index = find_first_zero_bit(host->allocated_channels,
					    max_channels);
		if (index >= max_channels) {
			mutex_unlock(&host->chlist_mutex);
			mdelay(1);
			mutex_lock(&host->chlist_mutex);
		}
	} while (index >= max_channels);

	/* Reserve the channel */
	set_bit(index, host->allocated_channels);
	ch = host->chlist[index];

	/* Bind the reserved channel to the device */
	ch->dev = pdata->pdev;
	ch->identifier = identifier;
	kref_init(&ch->refcount);

	/* channel is allocated, release mutex */
	mutex_unlock(&host->chlist_mutex);

	/* allocate vm */
	ch->vm = nvhost_vm_allocate(pdata->pdev,
				    (void *)(uintptr_t)current->pid);
	if (!ch->vm)
		goto err_alloc_vm;

	/* Handle logging */
	trace_nvhost_channel_map(pdata->pdev->name, ch->chid,
				 pdata->num_mapped_chs,
				 identifier);
	dev_dbg(&ch->dev->dev, "channel %d mapped\n", ch->chid);

	mutex_unlock(&host->ch_alloc_mutex);

	*channel = ch;

	return 0;

err_alloc_vm:
	/* re-acquire chlist mutex for freeing the channel */
	mutex_lock(&host->chlist_mutex);
	clear_bit(index, host->allocated_channels);
	ch->dev = NULL;
	ch->identifier = NULL;
	mutex_unlock(&host->chlist_mutex);

	mutex_unlock(&host->ch_alloc_mutex);

	return -ENOMEM;
}
EXPORT_SYMBOL(nvhost_channel_map);

/* Free channel memory and list */
int nvhost_channel_list_free(struct nvhost_master *host)
{
	int i;

	for (i = 0; i < nvhost_channel_nb_channels(host); i++)
		kfree(host->chlist[i]);

	dev_info(&host->dev->dev, "channel list free'd\n");

	return 0;
}

int nvhost_channel_abort(struct nvhost_device_data *pdata,
			void *identifier)
{
	int index;
	int max_channels;
	struct nvhost_master *host = NULL;
	struct nvhost_channel *ch = NULL;

	host = nvhost_get_host(pdata->pdev);
	max_channels = nvhost_channel_nb_channels(host);

	mutex_lock(&host->ch_alloc_mutex);
	mutex_lock(&host->chlist_mutex);

	/* first check if channel is mapped */
	for (index = 0; index < max_channels; index++) {
		ch = host->chlist[index];
		if (ch->identifier == identifier
				&& kref_get_unless_zero(&ch->refcount))
			break;
	}

	mutex_unlock(&host->chlist_mutex);
	mutex_unlock(&host->ch_alloc_mutex);

	if (index == max_channels) /* no channel mapped */
		return 0;

	cdma_op().handle_timeout(&ch->cdma, true);

	kref_put(&ch->refcount, nvhost_channel_unmap_locked);

	return 0;
}

bool nvhost_channel_is_reset_required(struct nvhost_channel *ch)
{
	bool reset_required = false;
	struct nvhost_device_data *pdata = platform_get_drvdata(ch->dev);
	struct nvhost_master *master = nvhost_get_host(pdata->pdev);
	struct nvhost_syncpt *syncpt = &master->syncpt;
	unsigned int owner;
	bool ch_own, cpu_own;

	/* if resources are allocated per channel instance, the channel does
	 * not necessaryly hold the mlock */
	if (pdata->resource_policy == RESOURCE_PER_CHANNEL_INSTANCE) {
		/* check the owner */
		syncpt_op().mutex_owner(syncpt, pdata->modulemutexes[0],
					&cpu_own, &ch_own, &owner);

		/* if this channel owns the lock, we need to reset the engine */
		if (ch_own && owner == ch->chid)
			reset_required = true;
	} else {
		/* if we allocate the resource per channel, the module is always
		 * contamined */
		reset_required = true;
	}

	return reset_required;
}

void nvhost_channel_init_gather_filter(struct platform_device *pdev,
	struct nvhost_channel *ch)
{
	if (channel_op(ch).init_gather_filter)
		channel_op(ch).init_gather_filter(pdev, ch);
}

int nvhost_channel_submit(struct nvhost_job *job)
{
	return channel_op(job->ch).submit(job);
}
EXPORT_SYMBOL(nvhost_channel_submit);

void nvhost_getchannel(struct nvhost_channel *ch)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(ch->dev);
	trace_nvhost_getchannel(pdata->pdev->name,
				atomic_read(&ch->refcount.refcount), ch->chid);
	kref_get(&ch->refcount);
}

void nvhost_putchannel(struct nvhost_channel *ch, int cnt)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(ch->dev);
	struct nvhost_master *host = nvhost_get_host(pdata->pdev);

	trace_nvhost_putchannel(pdata->pdev->name,
				atomic_read(&ch->refcount.refcount), ch->chid);

	/* Avoid race in case where one thread is acquiring a channel with
	 * the same identifier that we are dropping now; If we first drop
	 * the reference counter and then acquire the mutex, channel map
	 * routine may have acquired another channel with the same identifier.
	 * This may happen if all submits from one user get finished - and
	 * the very same user submits more work.
	 *
	 * In order to avoid above race, always acquire chlist mutex before
	 * entering channel unmap routine.
	 */

	mutex_lock(&host->chlist_mutex);
	kref_sub(&ch->refcount, cnt, nvhost_channel_unmap_locked);
	mutex_unlock(&host->chlist_mutex);
}
EXPORT_SYMBOL(nvhost_putchannel);

int nvhost_channel_suspend(struct nvhost_master *host)
{
	int i;

	for (i = 0; i < nvhost_channel_nb_channels(host); i++) {
		struct nvhost_channel *ch = host->chlist[i];
		if (channel_cdma_op().stop && ch->dev)
			channel_cdma_op().stop(&ch->cdma);
	}

	return 0;
}

int nvhost_channel_nb_channels(struct nvhost_master *host)
{
	return host->info.nb_channels;
}

int nvhost_channel_ch_base(struct nvhost_master *host)
{
	return host->info.ch_base;
}

int nvhost_channel_ch_limit(struct nvhost_master *host)
{
	return host->info.ch_limit;
}

int nvhost_channel_get_id_from_index(struct nvhost_master *host, int index)
{
	return nvhost_channel_ch_base(host) + index;
}

int nvhost_channel_get_index_from_id(struct nvhost_master *host, int chid)
{
	return chid - nvhost_channel_ch_base(host);
}
