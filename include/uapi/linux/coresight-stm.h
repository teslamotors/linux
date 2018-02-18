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

#ifndef __UAPI_CORESIGHT_STM_H_
#define __UAPI_CORESIGHT_STM_H_

enum {
	STM_ENTITY_NONE			= 0x00,
	STM_ENTITY_TRACE_PRINTK		= 0x01,
	STM_ENTITY_TRACE_USPACE		= 0x02,
	STM_ENTITY_MAX			= 0xFF,
};

enum {
	STM_IOCTL_NONE			= 0x00,
	STM_IOCTL_SET_OPTIONS		= 0x01,
	STM_IOCTL_GET_OPTIONS		= 0x10,
};

enum {
	STM_PKT_TYPE_DATA		= 0x18,
	STM_PKT_TYPE_FLAG		= 0x68,
	STM_PKT_TYPE_TRIG		= 0x78,
};

enum {
	STM_TIME_GUARANTEED		= 0x00,
	STM_TIME_INVARIANT		= 0x80,
};

enum {
	STM_OPTION_NONE			= 0x00,
	STM_OPTION_TIMESTAMPED		= 0x08,
	STM_OPTION_MARKED		= 0x10,
};

#endif
