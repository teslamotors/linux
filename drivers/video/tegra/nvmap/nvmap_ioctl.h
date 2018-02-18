/*
 * drivers/video/tegra/nvmap/nvmap_ioctl.h
 *
 * ioctl declarations for nvmap
 *
 * Copyright (c) 2010-2017, NVIDIA CORPORATION. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __VIDEO_TEGRA_NVMAP_IOCTL_H
#define __VIDEO_TEGRA_NVMAP_IOCTL_H

#include <linux/nvmap.h>

int nvmap_ioctl_pinop(struct file *filp, bool is_pin, void __user *arg,
	bool is32);

int nvmap_ioctl_getid(struct file *filp, void __user *arg);

int nvmap_ioctl_get_ivcid(struct file *filp, void __user *arg);

int nvmap_ioctl_getfd(struct file *filp, void __user *arg);

int nvmap_ioctl_alloc(struct file *filp, void __user *arg);

int nvmap_ioctl_alloc_kind(struct file *filp, void __user *arg);

int nvmap_ioctl_alloc_ivm(struct file *filp, void __user *arg);

int nvmap_ioctl_vpr_floor_size(struct file *filp, void __user *arg);

int nvmap_ioctl_free(struct file *filp, unsigned long arg);

int nvmap_ioctl_create(struct file *filp, unsigned int cmd, void __user *arg);

int nvmap_ioctl_create_from_va(struct file *filp, void __user *arg);

int nvmap_ioctl_create_from_ivc(struct file *filp, void __user *arg);

int nvmap_ioctl_get_ivc_heap(struct file *filp, void __user *arg);

int nvmap_map_into_caller_ptr(struct file *filp, void __user *arg, bool is32);

int nvmap_ioctl_cache_maint(struct file *filp, void __user *arg, bool is32);

int nvmap_ioctl_rw_handle(struct file *filp, int is_read, void __user *arg,
	bool is32);

int nvmap_ioctl_cache_maint_list(struct file *filp, void __user *arg,
	bool is_rsrv_op);

int nvmap_ioctl_gup_test(struct file *filp, void __user *arg);

int nvmap_ioctl_set_tag_label(struct file *filp, void __user *arg);

#endif	/*  __VIDEO_TEGRA_NVMAP_IOCTL_H */
