/*
 * drivers/video/tegra/host/nvhost_channel.h
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

#ifndef __NVHOST_CHANNEL_H
#define __NVHOST_CHANNEL_H

#include <linux/cdev.h>
#include <linux/io.h>
#include "nvhost_cdma.h"

#define NVHOST_MAX_WAIT_CHECKS		256
#define NVHOST_MAX_GATHERS		512
#define NVHOST_MAX_HANDLES		1280
#define NVHOST_MAX_POWERGATE_IDS	2

struct nvhost_master;
struct platform_device;
struct nvhost_channel;

struct nvhost_channel_ops {
	const char *soc_name;
	int (*init)(struct nvhost_channel *,
		    struct nvhost_master *);
	int (*submit)(struct nvhost_job *job);
	int (*init_gather_filter)(struct platform_device *pdev,
		struct nvhost_channel *ch);
};

struct nvhost_channel {
	struct nvhost_channel_ops ops;
	struct kref refcount;
	int chid;
	int dev_chid;
	struct mutex submitlock;
	void __iomem *aperture;
	struct platform_device *dev;
	struct nvhost_cdma cdma;

	/* pointer to channel address space */
	struct nvhost_vm *vm;

	/* channel syncpoints */
	struct mutex syncpts_lock;
	u32 syncpts[NVHOST_MODULE_MAX_SYNCPTS];
	u32 client_managed_syncpt;

	bool cdma_initialized;
	/* owner identifier */
	void *identifier;
};

#define channel_op(ch)		(ch->ops)

int nvhost_alloc_channels(struct nvhost_master *host);
int nvhost_channel_remove_identifier(struct nvhost_device_data *pdata,
			void *identifier);
int nvhost_channel_unmap(struct nvhost_channel *ch);
int nvhost_channel_release(struct nvhost_device_data *pdata);
int nvhost_channel_list_free(struct nvhost_master *host);
struct nvhost_channel *nvhost_check_channel(struct nvhost_device_data *pdata);
int nvhost_channel_init(struct nvhost_channel *ch,
	struct nvhost_master *dev);

void nvhost_getchannel(struct nvhost_channel *ch);
int nvhost_channel_suspend(struct nvhost_master *host);

int nvhost_channel_read_reg(struct nvhost_channel *channel,
	u32 offset, u32 *value);

struct nvhost_channel *nvhost_alloc_channel_internal(int chindex,
	int max_channels);

void nvhost_channel_init_gather_filter(struct platform_device *pdev,
	struct nvhost_channel *ch);

bool nvhost_channel_is_reset_required(struct nvhost_channel *ch);

int nvhost_channel_abort(struct nvhost_device_data *pdata,
			void *identifier);

int nvhost_channel_nb_channels(struct nvhost_master *host);
int nvhost_channel_ch_base(struct nvhost_master *host);
int nvhost_channel_ch_limit(struct nvhost_master *host);
int nvhost_channel_get_id_from_index(struct nvhost_master *host, int index);
int nvhost_channel_get_index_from_id(struct nvhost_master *host, int chid);
int nvhost_channel_set_syncpoint_name(struct nvhost_syncpt *sp, u32 syncpt_id,
	const char *syncpt_name);
#endif
