/*
 * Copyright (C) 2018 Tesla Motors, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __MALI_C71_IOCTL_H__
#define __MALI_C71_IOCTL_H__

#define MALI_C71_CDMA_VERSION	0x100

struct mali_c71_cdma {
	uint32_t version;
	uint32_t cdma_len;	/* should always be 0x20000 */
	uint8_t *cdma_buf;
};

struct mali_c71_cdma_region {
	uint32_t offset;
	uint32_t size;
};

struct mali_c71_cdma_region_list {
	struct mali_c71_cdma_region __user *regions;
	uint32_t count;
};

/* All of these should be in the BASE_VIDIOC_PRIVATE range (192-255) */
enum {
	MALI_C71_IOCTL_GET_CDMA = _IOWR('V', 224, struct mali_c71_cdma),
	MALI_C71_IOCTL_SET_CDMA = _IOWR('V', 225, struct mali_c71_cdma),
	MALI_C71_IOCTL_GET_CDMA_CONFIG_REGIONS = _IOWR('V', 226,
				struct mali_c71_cdma_region_list),
	MALI_C71_IOCTL_SET_CDMA_CONFIG_REGIONS = _IOWR('V', 227,
				struct mali_c71_cdma_region_list),
};

#endif /* __MALI_C71_IOCTL_H__ */
