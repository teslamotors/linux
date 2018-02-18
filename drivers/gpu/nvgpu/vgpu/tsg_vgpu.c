/*
 * Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/kernel.h>
#include <linux/tegra_vgpu.h>

#include "gk20a/gk20a.h"
#include "gk20a/channel_gk20a.h"
#include "gk20a/platform_gk20a.h"
#include "gk20a/tsg_gk20a.h"
#include "vgpu.h"

static int vgpu_tsg_bind_channel(struct tsg_gk20a *tsg,
			struct channel_gk20a *ch)
{
	struct gk20a_platform *platform = gk20a_get_platform(tsg->g->dev);
	struct tegra_vgpu_cmd_msg msg = {};
	struct tegra_vgpu_tsg_bind_unbind_channel_params *p =
				&msg.params.tsg_bind_unbind_channel;
	int err;

	gk20a_dbg_fn("");

	err = gk20a_tsg_bind_channel(tsg, ch);
	if (err)
		return err;

	msg.cmd = TEGRA_VGPU_CMD_TSG_BIND_CHANNEL;
	msg.handle = platform->virt_handle;
	p->tsg_id = tsg->tsgid;
	p->ch_handle = ch->virt_ctx;
	err = vgpu_comm_sendrecv(&msg, sizeof(msg), sizeof(msg));
	err = err ? err : msg.ret;
	if (err) {
		gk20a_err(dev_from_gk20a(tsg->g),
			"vgpu_tsg_bind_channel failed, ch %d tsgid %d",
			ch->hw_chid, tsg->tsgid);
		gk20a_tsg_unbind_channel(ch);
	}

	return err;
}

static int vgpu_tsg_unbind_channel(struct channel_gk20a *ch)
{
	struct gk20a_platform *platform = gk20a_get_platform(ch->g->dev);
	struct tegra_vgpu_cmd_msg msg = {};
	struct tegra_vgpu_tsg_bind_unbind_channel_params *p =
				&msg.params.tsg_bind_unbind_channel;
	int err;

	gk20a_dbg_fn("");

	err = gk20a_tsg_unbind_channel(ch);
	if (err)
		return err;

	msg.cmd = TEGRA_VGPU_CMD_TSG_UNBIND_CHANNEL;
	msg.handle = platform->virt_handle;
	p->ch_handle = ch->virt_ctx;
	err = vgpu_comm_sendrecv(&msg, sizeof(msg), sizeof(msg));
	err = err ? err : msg.ret;
	WARN_ON(err);

	return err;
}

static int vgpu_tsg_set_timeslice(struct tsg_gk20a *tsg, u32 timeslice)
{
	struct gk20a_platform *platform = gk20a_get_platform(tsg->g->dev);
	struct tegra_vgpu_cmd_msg msg = {0};
	struct tegra_vgpu_tsg_timeslice_params *p =
				&msg.params.tsg_timeslice;
	int err;

	gk20a_dbg_fn("");

	msg.cmd = TEGRA_VGPU_CMD_TSG_SET_TIMESLICE;
	msg.handle = platform->virt_handle;
	p->tsg_id = tsg->tsgid;
	p->timeslice_us = timeslice;
	err = vgpu_comm_sendrecv(&msg, sizeof(msg), sizeof(msg));
	err = err ? err : msg.ret;
	WARN_ON(err);

	return err;
}

void vgpu_init_tsg_ops(struct gpu_ops *gops)
{
	gops->fifo.tsg_bind_channel = vgpu_tsg_bind_channel;
	gops->fifo.tsg_unbind_channel = vgpu_tsg_unbind_channel;
	gops->fifo.tsg_set_timeslice = vgpu_tsg_set_timeslice;
}
