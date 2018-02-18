/*
 * Tegra Host Virtualization Interfaces to Server
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

#ifndef __TEGRA_VHOST_H
#define __TEGRA_VHOST_H

enum {
	/* Must start at 1, 0 is used for VGPU */
	TEGRA_VHOST_MODULE_HOST = 1,
	TEGRA_VHOST_MODULE_VIC,
	TEGRA_VHOST_MODULE_VI,
	TEGRA_VHOST_MODULE_ISP,
	TEGRA_VHOST_MODULE_MSENC,
	TEGRA_VHOST_MODULE_NVDEC,
	TEGRA_VHOST_MODULE_NVJPG,
};

enum {
	TEGRA_VHOST_QUEUE_CMD = 0,
	TEGRA_VHOST_QUEUE_PB,
	TEGRA_VHOST_QUEUE_INTR,
	/* See also TEGRA_VGPU_QUEUE_* in tegra_vgpu.h */
};

enum {
	TEGRA_VHOST_CMD_CONNECT = 0,
	TEGRA_VHOST_CMD_DISCONNECT,
	TEGRA_VHOST_CMD_ABORT,
	TEGRA_VHOST_CMD_SYNCPT_WRITE,
	TEGRA_VHOST_CMD_SYNCPT_READ,
	TEGRA_VHOST_CMD_SYNCPT_CPU_INCR,
	TEGRA_VHOST_CMD_WAITBASE_WRITE,
	TEGRA_VHOST_CMD_WAITBASE_READ,
	TEGRA_VHOST_CMD_MUTEX_TRY_LOCK,
	TEGRA_VHOST_CMD_MUTEX_UNLOCK,
	TEGRA_VHOST_CMD_SYNCPT_ENABLE_INTR,
	TEGRA_VHOST_CMD_SYNCPT_DISABLE_INTR,
	TEGRA_VHOST_CMD_SYNCPT_DISABLE_INTR_ALL,
	TEGRA_VHOST_CMD_SYNCPT_GET_RANGE,
	TEGRA_VHOST_CMD_CHANNEL_ALLOC_CLIENTID,
	TEGRA_VHOST_CMD_HOST1X_CDMA_SUBMIT,
	TEGRA_VHOST_CMD_HOST1X_REGRDWR,
};

struct tegra_vhost_connect_params {
	u32 module;
	u64 handle;
};

struct tegra_vhost_syncpt_params {
	u32 id;
	u32 val;
};

struct tegra_vhost_syncpt_range_params {
	u32 base;
	u32 size;
};

struct tegra_vhost_waitbase_params {
	u32 id;
	u32 val;
};

struct tegra_vhost_mutex_params {
	u32 id;
	u32 locked;
	u32 owner;
};

struct tegra_vhost_syncpt_intr_params {
	u32 id;
	u32 thresh;
};

struct tegra_vhost_channel_clientid_params {
	u32 moduleid;
	u32 clientid;
};

struct tegra_vhost_channel_submit_params {
	u32 clientid;
	u32 job_id;
	u32 num_entries;
	u32 num_syncpts;
	u32 timeout;
};

#define REGRDWR_ARRAY_SIZE (u32)4
struct tegra_vhost_channel_regrdwr_params {
	u32 moduleid;
	u32 count;
	u32 write;
	u32 regs[REGRDWR_ARRAY_SIZE];
};

struct tegra_vhost_cmd_msg {
	u32 cmd;
	int ret;
	u64 handle;
	union {
		struct tegra_vhost_connect_params connect;
		struct tegra_vhost_syncpt_params syncpt;
		struct tegra_vhost_syncpt_range_params syncpt_range;
		struct tegra_vhost_waitbase_params waitbase;
		struct tegra_vhost_mutex_params mutex;
		struct tegra_vhost_syncpt_intr_params syncpt_intr;
		struct tegra_vhost_channel_clientid_params clientid;
		struct tegra_vhost_channel_submit_params cdma_submit;
		struct tegra_vhost_channel_regrdwr_params regrdwr;
	} params;
};

enum {
	TEGRA_VHOST_EVENT_SYNCPT_INTR = 0,
	TEGRA_VHOST_EVENT_CHAN_TIMEOUT_INTR,
	TEGRA_VHOST_EVENT_ABORT
};

struct tegra_vhost_syncpt_intr_info {
	u32 id;
	u32 thresh;
};

struct tegra_vhost_chan_timeout_intr_info {
	u32 module_id;
	u32 client_id;
	u32 job_id_start;
	u32 job_id_end;
};

struct tegra_vhost_intr_msg {
	unsigned int event;
	union {
		struct tegra_vhost_syncpt_intr_info syncpt_intr;
		struct tegra_vhost_chan_timeout_intr_info chan_timeout;
	} info;
};

#define TEGRA_VHOST_QUEUE_SIZES			\
	sizeof(struct tegra_vhost_cmd_msg),	\
	4096,					\
	sizeof(struct tegra_vhost_intr_msg)

#endif
