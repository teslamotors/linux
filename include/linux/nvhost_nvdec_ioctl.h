/*
 * include/linux/nvhost_ioctl_ioctl.h
 *
 * Tegra IOCTL Driver
 *
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
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
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __LINUX_NVHOST_IOCTL_IOCTL_H
#define __LINUX_NVHOST_IOCTL_IOCTL_H

#include <linux/ioctl.h>
#include <linux/types.h>

#if !defined(__KERNEL__)
#define __user
#endif

#define NVHOST_NVDEC_IOCTL_MAGIC 'N'

/*
 * /dev/nvhost-ctrl-nvdec devices
 *
 * Opening a '/dev/nvhost-ctrl-nvdec' device node creates a way to send
 * ctrl ioctl to nvdec driver.
 *
 * /dev/nvhost-nvdec is for channel (context specific) operations. We use
 * /dev/nvhost-ctrl-nvdec for global (context independent) operations on
 * nvdec device.
 */

#define NVHOST_NVDEC_IOCTL_POWERON _IOW(NVHOST_NVDEC_IOCTL_MAGIC, 1, uint)
#define NVHOST_NVDEC_IOCTL_POWEROFF _IOW(NVHOST_NVDEC_IOCTL_MAGIC, 2, uint)

#endif
