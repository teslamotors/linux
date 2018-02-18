/*
 * Copyright (C) 2016 NVIDIA CORPORATION.  All rights reserved.
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

#include "dev.h"
#include "dev_t186.h"

#include "hw/host1x05.h"

struct host1x_info host1x05_info = {
	.nb_channels = 63,
	.nb_pts = 576,
	.nb_mlocks = 24,
	.nb_bases = 16,
	.init = host1x05_init,
	.sync_offset = 0x0,
	.gather_filter_enabled  = true,
};

void host1x_writel(struct host1x *host1x, u32 v, u32 r)
{
	writel(v, host1x->regs + r);
}

u32 host1x_readl(struct host1x *host1x, u32 r)
{
	return readl(host1x->regs + r);
}
