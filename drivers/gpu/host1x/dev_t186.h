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

#ifndef HOST1X_DEV_T186_H
#define HOST1X_DEV_T186_H

#include "dev.h"

extern struct host1x_info host1x05_info;

void host1x_writel(struct host1x *host1x, u32 v, u32 r);
u32 host1x_readl(struct host1x *host1x, u32 r);

static inline void host1x_hw_sync_get_mutex_owner(struct host1x *host,
					struct host1x_syncpt *sp,
					unsigned int mutex_id, bool *cpu, bool *ch,
					unsigned int *chid)
{
	host->syncpt_op->get_mutex_owner(sp, mutex_id, cpu, ch, chid);
}
#endif
