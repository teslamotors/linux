/*
 * drivers/video/tegra/host/t20/3dctx_t20.h
 *
 * Tegra Graphics Host Syncpoints for T20
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

#ifndef __NVHOST_3DCTX_T20_H
#define __NVHOST_3DCTX_T20_H

struct nvhost_hwctx_handler;

int t20_nvhost_3dctx_handler_init(struct nvhost_hwctx_handler *h);

#endif
