/*
 * drivers/video/tegra/host/3dctx_common.h
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

#ifndef __NVHOST_3DCTX_COMMON_H
#define __NVHOST_3DCTX_COMMON_H

#include "nvhost_acm.h"
#include <linux/types.h>

/* Internal variables used by common 3D context switch functions */
extern unsigned int nvhost_3dctx_restore_size;
extern unsigned int nvhost_3dctx_restore_incrs;
extern struct nvmap_handle_ref *nvhost_3dctx_save_buf;
extern unsigned int nvhost_3dctx_save_incrs;
extern unsigned int nvhost_3dctx_save_thresh;
extern unsigned int nvhost_3dctx_save_slots;

struct nvhost_hwctx;
struct nvhost_channel;
struct kref;

/* Functions used commonly by all 3D context switch modules */
extern void nvhost_3dctx_restore_begin(u32 *ptr);
extern void nvhost_3dctx_restore_direct(u32 *ptr, u32 start_reg, u32 count);
extern void nvhost_3dctx_restore_indirect(u32 *ptr, u32 offset_reg,
		u32 offset,	u32 data_reg, u32 count);
extern void nvhost_3dctx_restore_end(u32 *ptr);
extern struct nvhost_hwctx *nvhost_3dctx_alloc_common(
		struct nvhost_channel *ch, bool map_restore);
void nvhost_3dctx_get(struct nvhost_hwctx *ctx);
void nvhost_3dctx_free(struct kref *ref);
void nvhost_3dctx_put(struct nvhost_hwctx *ctx);
int nvhost_3dctx_prepare_power_off(struct nvhost_module *mod);

#endif
