/*
 * Copyright (c) 2010-2015, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef TEGRA_KFUSE_H
#define TEGRA_KFUSE_H

/* there are 144 32-bit values in total */
#define KFUSE_DATA_SZ (144 * 4)

#ifdef CONFIG_TEGRA_KFUSE
int tegra_kfuse_read(void *dest, size_t len);
#else
static inline int tegra_kfuse_read(void *dest, size_t len)
{
	return -ENOSYS;
}
#endif

#endif
