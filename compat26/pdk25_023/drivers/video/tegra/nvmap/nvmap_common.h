/*
 * drivers/video/tegra/nvmap/nvmap_common.h
 *
 * GPU memory management driver for Tegra
 *
 * Copyright (c) 2011, NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *'
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

extern void compat26_v7_flush_kern_cache_all(void *);
extern void compat26_v7_clean_kern_cache_all(void *);

#define FLUSH_CLEAN_BY_SET_WAY_THRESHOLD (8 * PAGE_SIZE)

static inline void inner_flush_cache_all(void)
{
	on_each_cpu(compat26_v7_flush_kern_cache_all, NULL, 1);
}

static inline void inner_clean_cache_all(void)
{
	on_each_cpu(compat26_v7_clean_kern_cache_all, NULL, 1);
}
