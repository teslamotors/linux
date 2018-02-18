/*
 * Tegra Graphics Virtualization Host Syncpoints for HOST1X
 *
 * Copyright (c) 2014, NVIDIA Corporation. All rights reserved.
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

#include "nvhost_syncpt.h"
#include "vhost.h"
#include "../host1x/host1x.h"

#define INVALID_REG_VAL 0xdeadcafe

int vhost_syncpt_get_range(u64 handle, u32 *base, u32 *size)
{
	struct tegra_vhost_cmd_msg msg;
	int err;

	struct tegra_vhost_syncpt_range_params *p = &msg.params.syncpt_range;

	msg.cmd = TEGRA_VHOST_CMD_SYNCPT_GET_RANGE;
	msg.handle = handle;
	err = vhost_sendrecv(&msg);

	if (err || msg.ret)
		return -1;

	if (base)
		*base = p->base;
	if (size)
		*size = p->size;
	return 0;
}

static void vhost_syncpt_reset(struct nvhost_syncpt *sp, u32 id)
{
	struct nvhost_master *dev = syncpt_to_dev(sp);
	struct nvhost_virt_ctx *ctx = nvhost_get_virt_data(dev->dev);
	struct tegra_vhost_cmd_msg msg;
	struct tegra_vhost_syncpt_params *p = &msg.params.syncpt;
	int min = nvhost_syncpt_read_min(sp, id);
	int err;

	msg.cmd = TEGRA_VHOST_CMD_SYNCPT_WRITE;
	msg.handle = ctx->handle;
	p->id = id;
	p->val = min;
	err = vhost_sendrecv(&msg);
	WARN_ON(err || msg.ret);
}

static u32 vhost_syncpt_read(u64 handle, u32 id)
{
	struct tegra_vhost_cmd_msg msg;
	struct tegra_vhost_syncpt_params *p = &msg.params.syncpt;
	int err;

	msg.cmd = TEGRA_VHOST_CMD_SYNCPT_READ;
	msg.handle = handle;
	p->id = id;
	err = vhost_sendrecv(&msg);

	if (WARN_ON(err || msg.ret))
		return INVALID_REG_VAL;

	return p->val;
}

static u32 vhost_syncpt_update_min(struct nvhost_syncpt *sp, u32 id)
{
	struct nvhost_master *dev = syncpt_to_dev(sp);
	struct nvhost_virt_ctx *ctx = nvhost_get_virt_data(dev->dev);
	u32 old, live;

	do {
		old = nvhost_syncpt_read_min(sp, id);
		live = vhost_syncpt_read(ctx->handle, id);
	} while ((u32)atomic_cmpxchg(&sp->min_val[id], old, live) != old);

	return live;
}

static void vhost_syncpt_cpu_incr(struct nvhost_syncpt *sp, u32 id)
{
	struct nvhost_master *dev = syncpt_to_dev(sp);
	struct nvhost_virt_ctx *ctx = nvhost_get_virt_data(dev->dev);
	struct tegra_vhost_cmd_msg msg;
	struct tegra_vhost_syncpt_params *p = &msg.params.syncpt;
	int err;

	if (!nvhost_syncpt_client_managed(sp, id)
			&& nvhost_syncpt_min_eq_max(sp, id)) {
		dev_err(&syncpt_to_dev(sp)->dev->dev,
			"Trying to increment syncpoint id %d beyond max\n",
			id);
		nvhost_debug_dump(syncpt_to_dev(sp));
		return;
	}

	msg.cmd = TEGRA_VHOST_CMD_SYNCPT_CPU_INCR;
	msg.handle = ctx->handle;
	p->id = id;
	err = vhost_sendrecv(&msg);
	WARN_ON(err || msg.ret);
}

static int vhost_syncpt_mutex_try_lock(struct nvhost_syncpt *sp,
				unsigned int idx)
{
	struct nvhost_master *dev = syncpt_to_dev(sp);
	struct nvhost_virt_ctx *ctx = nvhost_get_virt_data(dev->dev);
	struct tegra_vhost_cmd_msg msg;
	struct tegra_vhost_mutex_params *p = &msg.params.mutex;
	int err;

	msg.cmd = TEGRA_VHOST_CMD_MUTEX_TRY_LOCK;
	msg.handle = ctx->handle;
	p->id = idx;
	err = vhost_sendrecv(&msg);

	if (WARN_ON(err || msg.ret))
		return INVALID_REG_VAL;

	return p->locked;
}

static void vhost_syncpt_mutex_unlock(struct nvhost_syncpt *sp,
				unsigned int idx)
{
	struct nvhost_master *dev = syncpt_to_dev(sp);
	struct nvhost_virt_ctx *ctx = nvhost_get_virt_data(dev->dev);
	struct tegra_vhost_cmd_msg msg;
	struct tegra_vhost_mutex_params *p = &msg.params.mutex;
	int err;

	msg.cmd = TEGRA_VHOST_CMD_MUTEX_UNLOCK;
	msg.handle = ctx->handle;
	p->id = idx;
	err = vhost_sendrecv(&msg);
	WARN_ON(err || msg.ret);
}

void vhost_init_host1x_syncpt_ops(struct nvhost_syncpt_ops *ops)
{
	ops->reset = vhost_syncpt_reset;
	ops->update_min = vhost_syncpt_update_min;
	ops->cpu_incr = vhost_syncpt_cpu_incr;
	ops->mutex_try_lock = vhost_syncpt_mutex_try_lock;
	ops->mutex_unlock_nvh = vhost_syncpt_mutex_unlock;
};
