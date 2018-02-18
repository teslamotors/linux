/*
 * Tegra Graphics Virtualization Host Channel operations for HOST1X
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

#include "vhost.h"
#include "../host1x/host1x.h"

u32 vhost_channel_alloc_clientid(u64 handle, u32 id)
{
	struct tegra_vhost_cmd_msg msg;
	struct tegra_vhost_channel_clientid_params *p = &msg.params.clientid;
	int err;

	msg.cmd = TEGRA_VHOST_CMD_CHANNEL_ALLOC_CLIENTID;
	msg.handle = handle;

	p->moduleid = id;
	err = vhost_sendrecv(&msg);

	return (err || msg.ret) ? 0 : p->clientid;
}
