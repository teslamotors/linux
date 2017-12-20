/*
 * drivers/video/tegra/host/nvhost_hwctx.h
 *
 * Tegra Graphics Host Hardware Context Interface
 *
 * Copyright (c) 2010-2011, NVIDIA Corporation.
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

#ifndef __NVHOST_HWCTX_H
#define __NVHOST_HWCTX_H

#include <linux/string.h>
#include <linux/kref.h>

#include <linux/nvhost.h>
#include <mach/nvmap.h>

struct nvhost_channel;
struct nvhost_cdma;
struct nvhost_userctx_timeout;

struct nvhost_hwctx {
	struct kref ref;

	struct nvhost_channel *channel;
	struct nvhost_userctx_timeout *timeout;
	bool valid;

	struct nvmap_handle_ref *save;
	u32 save_incrs;
	u32 save_thresh;
	u32 save_slots;

	struct nvmap_handle_ref *restore;
	u32 *restore_virt;
	phys_addr_t restore_phys;
	u32 restore_size;
	u32 restore_incrs;
};

struct nvhost_hwctx_handler {
	struct nvhost_hwctx * (*alloc) (struct nvhost_channel *ch);
	void (*get) (struct nvhost_hwctx *ctx);
	void (*put) (struct nvhost_hwctx *ctx);
	void (*save_push) (struct nvhost_cdma *cdma, struct nvhost_hwctx *ctx);
	void (*save_service) (struct nvhost_hwctx *ctx);
};


struct hwctx_reginfo {
	unsigned int offset:12;
	unsigned int count:16;
	unsigned int type:2;
};

enum {
	HWCTX_REGINFO_DIRECT = 0,
	HWCTX_REGINFO_INDIRECT,
	HWCTX_REGINFO_INDIRECT_4X
};

#define HWCTX_REGINFO(offset, count, type) {offset, count, HWCTX_REGINFO_##type}

#endif
