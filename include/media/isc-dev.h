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
 */

#ifndef __ISC_DEV_H__
#define __ISC_DEV_H__

#define ISC_DEV_IOCTL_RDWR	_IOW('o', 3, struct isc_dev_pkg)

#ifdef CONFIG_COMPAT
#define ISC_DEV_IOCTL_RDWR32	_IOW('o', 3, struct isc_dev_pkg32)
#endif

#define MAX_ISC_DEV_PAK_SIZE	32
#define ISC_DEV_PKG_FLAG_WR	1

struct __attribute__ ((__packed__)) isc_dev_pkg {
	__u16 offset;
	__u16 offset_len;
	__u16 size;
	__u32 flags;
	unsigned long buffer;
};

struct __attribute__ ((__packed__)) isc_dev_pkg32 {
	__u16 offset;
	__u16 offset_len;
	__u16 size;
	__u32 flags;
	__u32 buffer;
};

#ifdef __KERNEL__
#include <linux/ioctl.h>  /* For IOCTL macros */
#include <linux/regmap.h>

#define MAX_ISC_NAME_LENGTH	32

struct isc_dev_platform_data {
	struct device *pdev; /* parent device of isc_dev */
	int reg_bits;
	int val_bits;
	char drv_name[MAX_ISC_NAME_LENGTH];
};
#endif /* __KERNEL__ */

#endif  /* __ISC_DEV_H__ */
