/* Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
 * Copyright (c) 2015, NVIDIA CORPORATION. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __LINUX_CORESIGHT_STM_H_
#define __LINUX_CORESIGHT_STM_H_

#include <uapi/linux/coresight-stm.h>

#define stm_pkt_opts(type, time, opts) (((type) | (time)) & ~(opts))

#ifdef CONFIG_CORESIGHT_STM
extern int stm_trace(u32 options, int channel_id,
		     u8 entity_id, const void *data, u32 size);
#else
static inline int stm_trace(u32 options, int channel_id,
			    u8 entity_id, const void *data, u32 size)
{
	return 0;
}
#endif

#endif
