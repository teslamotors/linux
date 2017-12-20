/*
 * include/linux/tegra_itu656.h
 *
 * Copyright (c) 2010, NVIDIA Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

enum {
	TEGRA_ITU656_MODULE_VI = 0,
};

enum {
	TEGRA_ITU656_VI_CLK
};

#define TEGRA_ITU656_IOCTL_ENABLE		_IOWR('i', 1, uint)
#define TEGRA_ITU656_IOCTL_DISABLE		_IOWR('i', 2, uint)
#define TEGRA_ITU656_IOCTL_CLK_SET		_IOWR('i', 3, uint)
#define TEGRA_ITU656_IOCTL_RESET		_IOWR('i', 4, uint)
