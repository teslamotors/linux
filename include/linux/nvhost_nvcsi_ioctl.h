/*
 * include/linux/nvhost_nvcsi_ioctl.h
 *
 * Tegra NVCSI Driver
 *
 * Copyright (c) 2015, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __LINUX_NVHOST_NVCSI_IOCTL_H
#define __LINUX_NVHOST_NVCSI_IOCTL_H

#include <linux/ioctl.h>
#include <linux/types.h>

#if !defined(__KERNEL__)
#define __user
#endif

#define NVHOST_NVCSI_IOCTL_MAGIC 'N'

#define NVHOST_NVCSI_IOCTL_SET_NVCSI_CLK _IOW(NVHOST_NVCSI_IOCTL_MAGIC, 0, long)

#endif
