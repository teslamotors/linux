/*
 * drivers/video/tegra/host/t20/t20.h
 *
 * Tegra Graphics Chip support for T20
 *
 * Copyright (c) 2011, NVIDIA Corporation.
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
#ifndef _NVHOST_T20_H_
#define _NVHOST_T20_H_

struct nvhost_master;

int nvhost_init_t20_channel_support(struct nvhost_master *);
int nvhost_init_t20_cdma_support(struct nvhost_master *);
int nvhost_init_t20_debug_support(struct nvhost_master *);
int nvhost_init_t20_syncpt_support(struct nvhost_master *);
int nvhost_init_t20_intr_support(struct nvhost_master *);
int nvhost_init_t20_cpuaccess_support(struct nvhost_master *);

#endif /* _NVHOST_T20_H_ */
