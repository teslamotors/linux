/*
 * Copyright (c) 2012-2014, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __TEGRA_THROUGHPUT_IOCTL_H
#define __TEGRA_THROUGHPUT_IOCTL_H

#include <linux/ioctl.h>

#define TEGRA_THROUGHPUT_MAGIC 'g'

struct tegra_throughput_target_fps_args {
	__u32 target_fps;
} __packed;

#define TEGRA_THROUGHPUT_IOCTL_TARGET_FPS \
	_IOW(TEGRA_THROUGHPUT_MAGIC, 1, struct tegra_throughput_target_fps_args)
#define TEGRA_THROUGHPUT_IOCTL_MAXNR \
	(_IOC_NR(TEGRA_THROUGHPUT_IOCTL_TARGET_FPS))

#endif /* !defined(__TEGRA_THROUGHPUT_IOCTL_H) */

