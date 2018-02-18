/*
 * Copyright (c) 2015-2016, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/tegra_gr_comm.h>
#include <linux/tegra_vgpu.h>

#include "gk20a/gk20a.h"
#include "gk20a/channel_gk20a.h"
#include "gk20a/dbg_gpu_gk20a.h"
#include "vgpu.h"

static int vgpu_exec_regops(struct dbg_session_gk20a *dbg_s,
		      struct nvgpu_dbg_gpu_reg_op *ops,
		      u64 num_ops)
{
	struct channel_gk20a *ch;
	struct gk20a_platform *platform = gk20a_get_platform(dbg_s->g->dev);
	struct tegra_vgpu_cmd_msg msg;
	struct tegra_vgpu_reg_ops_params *p = &msg.params.reg_ops;
	void *oob;
	size_t oob_size;
	void *handle = NULL;
	int ops_size, err = 0;

	gk20a_dbg_fn("");
	BUG_ON(sizeof(*ops) != sizeof(struct tegra_vgpu_reg_op));

	handle = tegra_gr_comm_oob_get_ptr(TEGRA_GR_COMM_CTX_CLIENT,
					tegra_gr_comm_get_server_vmid(),
					TEGRA_VGPU_QUEUE_CMD,
					&oob, &oob_size);
	if (!handle)
		return -EINVAL;

	ops_size = sizeof(*ops) * num_ops;
	if (oob_size < ops_size) {
		err = -ENOMEM;
		goto fail;
	}

	memcpy(oob, ops, ops_size);

	msg.cmd = TEGRA_VGPU_CMD_REG_OPS;
	msg.handle = platform->virt_handle;
	ch = nvgpu_dbg_gpu_get_session_channel(dbg_s);
	p->handle = ch ? ch->virt_ctx : 0;
	p->num_ops = num_ops;
	p->is_profiler = dbg_s->is_profiler;
	err = vgpu_comm_sendrecv(&msg, sizeof(msg), sizeof(msg));
	err = err ? err : msg.ret;
	if (!err)
		memcpy(ops, oob, ops_size);

fail:
	tegra_gr_comm_oob_put_ptr(handle);
	return err;
}

static int vgpu_dbg_set_powergate(struct dbg_session_gk20a *dbg_s, __u32 mode)
{
	struct gk20a_platform *platform = gk20a_get_platform(dbg_s->g->dev);
	struct tegra_vgpu_cmd_msg msg;
	struct tegra_vgpu_set_powergate_params *p = &msg.params.set_powergate;
	int err = 0;

	gk20a_dbg_fn("");

	/* Just return if requested mode is the same as the session's mode */
	switch (mode) {
	case NVGPU_DBG_GPU_POWERGATE_MODE_DISABLE:
		if (dbg_s->is_pg_disabled)
			return 0;
		dbg_s->is_pg_disabled = true;
		break;
	case NVGPU_DBG_GPU_POWERGATE_MODE_ENABLE:
		if (!dbg_s->is_pg_disabled)
			return 0;
		dbg_s->is_pg_disabled = false;
		break;
	default:
		return -EINVAL;
	}

	msg.cmd = TEGRA_VGPU_CMD_SET_POWERGATE;
	msg.handle = platform->virt_handle;
	p->mode = mode;
	err = vgpu_comm_sendrecv(&msg, sizeof(msg), sizeof(msg));
	err = err ? err : msg.ret;
	return err;
}

void vgpu_dbg_init(void)
{
	dbg_gpu_session_ops_gk20a.exec_reg_ops = vgpu_exec_regops;
	dbg_gpu_session_ops_gk20a.dbg_set_powergate = vgpu_dbg_set_powergate;
}
