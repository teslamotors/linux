/*
 * drivers/video/tegra/host/t20/3dctx_t20.c
 *
 * Tegra Graphics Host 3d hardware context
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

#include "../nvhost_hwctx.h"
#include "../dev.h"
#include "hardware_t20.h"
#include "syncpt_t20.h"
#include "../3dctx_common.h"

#include <linux/slab.h>

static const struct hwctx_reginfo ctxsave_regs_3d_global[] = {
	HWCTX_REGINFO(0xe00,    4, DIRECT),
	HWCTX_REGINFO(0xe05,   30, DIRECT),
	HWCTX_REGINFO(0xe25,    2, DIRECT),
	HWCTX_REGINFO(0xe28,    2, DIRECT),
	HWCTX_REGINFO(0x001,    2, DIRECT),
	HWCTX_REGINFO(0x00c,   10, DIRECT),
	HWCTX_REGINFO(0x100,   34, DIRECT),
	HWCTX_REGINFO(0x124,    2, DIRECT),
	HWCTX_REGINFO(0x200,    5, DIRECT),
	HWCTX_REGINFO(0x205, 1024, INDIRECT),
	HWCTX_REGINFO(0x207, 1024, INDIRECT),
	HWCTX_REGINFO(0x209,    1, DIRECT),
	HWCTX_REGINFO(0x300,   64, DIRECT),
	HWCTX_REGINFO(0x343,   25, DIRECT),
	HWCTX_REGINFO(0x363,    2, DIRECT),
	HWCTX_REGINFO(0x400,   16, DIRECT),
	HWCTX_REGINFO(0x411,    1, DIRECT),
	HWCTX_REGINFO(0x500,    4, DIRECT),
	HWCTX_REGINFO(0x520,   32, DIRECT),
	HWCTX_REGINFO(0x540,   64, INDIRECT),
	HWCTX_REGINFO(0x600,   16, INDIRECT_4X),
	HWCTX_REGINFO(0x603,  128, INDIRECT),
	HWCTX_REGINFO(0x608,    4, DIRECT),
	HWCTX_REGINFO(0x60e,    1, DIRECT),
	HWCTX_REGINFO(0x700,   64, INDIRECT),
	HWCTX_REGINFO(0x710,   50, DIRECT),
	HWCTX_REGINFO(0x800,   16, INDIRECT_4X),
	HWCTX_REGINFO(0x803,  512, INDIRECT),
	HWCTX_REGINFO(0x805,   64, INDIRECT),
	HWCTX_REGINFO(0x820,   32, DIRECT),
	HWCTX_REGINFO(0x900,   64, INDIRECT),
	HWCTX_REGINFO(0x902,    2, DIRECT),
	HWCTX_REGINFO(0xa02,   10, DIRECT),
	HWCTX_REGINFO(0xe04,    1, DIRECT),
	HWCTX_REGINFO(0xe2a,    1, DIRECT),
};

/* the same context save command sequence is used for all contexts. */
static phys_addr_t save_phys;
static unsigned int save_size;

#define SAVE_BEGIN_V0_SIZE 5
#define SAVE_DIRECT_V0_SIZE 3
#define SAVE_INDIRECT_V0_SIZE 5
#define SAVE_END_V0_SIZE 5
#define SAVE_INCRS 3
#define SAVE_THRESH_OFFSET 1
#define RESTORE_BEGIN_SIZE 4
#define RESTORE_DIRECT_SIZE 1
#define RESTORE_INDIRECT_SIZE 2
#define RESTORE_END_SIZE 1

struct save_info {
	u32 *ptr;
	unsigned int save_count;
	unsigned int restore_count;
	unsigned int save_incrs;
	unsigned int restore_incrs;
};

static u32 *setup_restore_regs_v0(u32 *ptr,
			const struct hwctx_reginfo *regs,
			unsigned int nr_regs)
{
	const struct hwctx_reginfo *rend = regs + nr_regs;

	for ( ; regs != rend; ++regs) {
		u32 offset = regs->offset;
		u32 count = regs->count;
		u32 indoff = offset + 1;
		switch (regs->type) {
		case HWCTX_REGINFO_DIRECT:
			nvhost_3dctx_restore_direct(ptr, offset, count);
			ptr += RESTORE_DIRECT_SIZE;
			break;
		case HWCTX_REGINFO_INDIRECT_4X:
			++indoff;
			/* fall through */
		case HWCTX_REGINFO_INDIRECT:
			nvhost_3dctx_restore_indirect(ptr,
					offset, 0, indoff, count);
			ptr += RESTORE_INDIRECT_SIZE;
			break;
		}
		ptr += count;
	}
	return ptr;
}

static void setup_restore_v0(u32 *ptr)
{
	nvhost_3dctx_restore_begin(ptr);
	ptr += RESTORE_BEGIN_SIZE;

	ptr = setup_restore_regs_v0(ptr,
			ctxsave_regs_3d_global,
			ARRAY_SIZE(ctxsave_regs_3d_global));

	nvhost_3dctx_restore_end(ptr);

	wmb();
}

/*** v0 saver ***/

static void save_push_v0(struct nvhost_cdma *cdma,
			struct nvhost_hwctx *ctx)
{
	nvhost_cdma_push_gather(cdma,
			(void *)NVHOST_CDMA_PUSH_GATHER_CTXSAVE,
			(void *)NVHOST_CDMA_PUSH_GATHER_CTXSAVE,
			nvhost_opcode_gather(save_size),
			save_phys);
}

static void __init save_begin_v0(u32 *ptr)
{
	/* 3d: when done, increment syncpt to base+1 */
	ptr[0] = nvhost_opcode_setclass(NV_GRAPHICS_3D_CLASS_ID, 0, 0);
	ptr[1] = nvhost_opcode_imm_incr_syncpt(NV_SYNCPT_OP_DONE,
			NVSYNCPT_3D); /*  incr 1 */
	/* host: wait for syncpt base+1 */
	ptr[2] = nvhost_opcode_setclass(NV_HOST1X_CLASS_ID,
					NV_CLASS_HOST_WAIT_SYNCPT_BASE, 1);
	ptr[3] = nvhost_class_host_wait_syncpt_base(NVSYNCPT_3D,
						NVWAITBASE_3D, 1);
	/* host: signal context read thread to start reading */
	ptr[4] = nvhost_opcode_imm_incr_syncpt(NV_SYNCPT_IMMEDIATE,
			NVSYNCPT_3D); /* incr 2 */
}

static void __init save_direct_v0(u32 *ptr, u32 start_reg, u32 count)
{
	ptr[0] = nvhost_opcode_nonincr(NV_CLASS_HOST_INDOFF, 1);
	ptr[1] = nvhost_class_host_indoff_reg_read(NV_HOST_MODULE_GR3D,
						start_reg, true);
	ptr[2] = nvhost_opcode_nonincr(NV_CLASS_HOST_INDDATA, count);
}

static void __init save_indirect_v0(u32 *ptr, u32 offset_reg, u32 offset,
			u32 data_reg, u32 count)
{
	ptr[0] = nvhost_opcode_setclass(NV_GRAPHICS_3D_CLASS_ID,
					offset_reg, 1);
	ptr[1] = offset;
	ptr[2] = nvhost_opcode_setclass(NV_HOST1X_CLASS_ID,
					NV_CLASS_HOST_INDOFF, 1);
	ptr[3] = nvhost_class_host_indoff_reg_read(NV_HOST_MODULE_GR3D,
						data_reg, false);
	ptr[4] = nvhost_opcode_nonincr(NV_CLASS_HOST_INDDATA, count);
}

static void __init save_end_v0(u32 *ptr)
{
	/* Wait for context read service to finish (cpu incr 3) */
	ptr[0] = nvhost_opcode_nonincr(NV_CLASS_HOST_WAIT_SYNCPT_BASE, 1);
	ptr[1] = nvhost_class_host_wait_syncpt_base(NVSYNCPT_3D,
			NVWAITBASE_3D, nvhost_3dctx_save_incrs);
	/* Advance syncpoint base */
	ptr[2] = nvhost_opcode_nonincr(NV_CLASS_HOST_INCR_SYNCPT_BASE, 1);
	ptr[3] = nvhost_class_host_incr_syncpt_base(NVWAITBASE_3D,
			nvhost_3dctx_save_incrs);
	/* set class back to the unit */
	ptr[4] = nvhost_opcode_setclass(NV_GRAPHICS_3D_CLASS_ID, 0, 0);
}

static u32 *save_regs_v0(u32 *ptr, unsigned int *pending,
			void __iomem *chan_regs,
			const struct hwctx_reginfo *regs,
			unsigned int nr_regs)
{
	const struct hwctx_reginfo *rend = regs + nr_regs;
	int drain_result = 0;

	for ( ; regs != rend; ++regs) {
		u32 count = regs->count;
		switch (regs->type) {
		case HWCTX_REGINFO_DIRECT:
			ptr += RESTORE_DIRECT_SIZE;
			break;
		case HWCTX_REGINFO_INDIRECT:
		case HWCTX_REGINFO_INDIRECT_4X:
			ptr += RESTORE_INDIRECT_SIZE;
			break;
		}
		drain_result = nvhost_drain_read_fifo(chan_regs,
			ptr, count, pending);
		BUG_ON(drain_result < 0);
		ptr += count;
	}
	return ptr;
}

/*** save ***/

static void __init setup_save_regs(struct save_info *info,
			const struct hwctx_reginfo *regs,
			unsigned int nr_regs)
{
	const struct hwctx_reginfo *rend = regs + nr_regs;
	u32 *ptr = info->ptr;
	unsigned int save_count = info->save_count;
	unsigned int restore_count = info->restore_count;

	for ( ; regs != rend; ++regs) {
		u32 offset = regs->offset;
		u32 count = regs->count;
		u32 indoff = offset + 1;
		switch (regs->type) {
		case HWCTX_REGINFO_DIRECT:
			if (ptr) {
				save_direct_v0(ptr, offset, count);
				ptr += SAVE_DIRECT_V0_SIZE;
			}
			save_count += SAVE_DIRECT_V0_SIZE;
			restore_count += RESTORE_DIRECT_SIZE;
			break;
		case HWCTX_REGINFO_INDIRECT_4X:
			++indoff;
			/* fall through */
		case HWCTX_REGINFO_INDIRECT:
			if (ptr) {
				save_indirect_v0(ptr, offset, 0,
						indoff, count);
				ptr += SAVE_INDIRECT_V0_SIZE;
			}
			save_count += SAVE_INDIRECT_V0_SIZE;
			restore_count += RESTORE_INDIRECT_SIZE;
			break;
		}
		if (ptr) {
			/* SAVE cases only: reserve room for incoming data */
			u32 k = 0;
			/*
			 * Create a signature pattern for indirect data (which
			 * will be overwritten by true incoming data) for
			 * better deducing where we are in a long command
			 * sequence, when given only a FIFO snapshot for debug
			 * purposes.
			*/
			for (k = 0; k < count; k++)
				*(ptr + k) = 0xd000d000 | (offset << 16) | k;
			ptr += count;
		}
		save_count += count;
		restore_count += count;
	}

	info->ptr = ptr;
	info->save_count = save_count;
	info->restore_count = restore_count;
}

static void __init setup_save(u32 *ptr)
{
	struct save_info info = {
		ptr,
		SAVE_BEGIN_V0_SIZE,
		RESTORE_BEGIN_SIZE,
		SAVE_INCRS,
		1
	};

	if (info.ptr) {
		save_begin_v0(info.ptr);
		info.ptr += SAVE_BEGIN_V0_SIZE;
	}

	/* save regs */
	setup_save_regs(&info,
			ctxsave_regs_3d_global,
			ARRAY_SIZE(ctxsave_regs_3d_global));

	if (info.ptr) {
		save_end_v0(info.ptr);
		info.ptr += SAVE_END_V0_SIZE;
	}

	wmb();

	save_size = info.save_count + SAVE_END_V0_SIZE;
	nvhost_3dctx_restore_size = info.restore_count + RESTORE_END_SIZE;
	nvhost_3dctx_save_incrs = info.save_incrs;
	nvhost_3dctx_save_thresh =
			nvhost_3dctx_save_incrs - SAVE_THRESH_OFFSET;
	nvhost_3dctx_restore_incrs = info.restore_incrs;
}



/*** ctx3d ***/

static struct nvhost_hwctx *ctx3d_alloc_v0(struct nvhost_channel *ch)
{
	struct nvhost_hwctx *ctx = nvhost_3dctx_alloc_common(ch, true);
	if (ctx)
		setup_restore_v0(ctx->restore_virt);
	return ctx;
}

static void ctx3d_save_service(struct nvhost_hwctx *ctx)
{
	u32 *ptr = (u32 *)ctx->restore_virt + RESTORE_BEGIN_SIZE;
	unsigned int pending = 0;

	ptr = save_regs_v0(ptr, &pending, ctx->channel->aperture,
			ctxsave_regs_3d_global,
			ARRAY_SIZE(ctxsave_regs_3d_global));

	wmb();
	nvhost_syncpt_cpu_incr(&ctx->channel->dev->syncpt, NVSYNCPT_3D);
}

int __init t20_nvhost_3dctx_handler_init(struct nvhost_hwctx_handler *h)
{
	struct nvhost_channel *ch;
	struct nvmap_client *nvmap;
	u32 *save_ptr;

	ch = container_of(h, struct nvhost_channel, ctxhandler);
	nvmap = ch->dev->nvmap;

	setup_save(NULL);

	nvhost_3dctx_save_buf = nvmap_alloc(nvmap, save_size * 4, 32,
				NVMAP_HANDLE_WRITE_COMBINE);
	if (IS_ERR(nvhost_3dctx_save_buf)) {
		int err = PTR_ERR(nvhost_3dctx_save_buf);
		nvhost_3dctx_save_buf = NULL;
		return err;
	}

	nvhost_3dctx_save_slots = 1;

	save_ptr = nvmap_mmap(nvhost_3dctx_save_buf);
	if (!save_ptr) {
		nvmap_free(nvmap, nvhost_3dctx_save_buf);
		nvhost_3dctx_save_buf = NULL;
		return -ENOMEM;
	}

	save_phys = nvmap_pin(nvmap, nvhost_3dctx_save_buf);

	setup_save(save_ptr);

	h->alloc = ctx3d_alloc_v0;
	h->save_push = save_push_v0;
	h->save_service = ctx3d_save_service;
	h->get = nvhost_3dctx_get;
	h->put = nvhost_3dctx_put;

	return 0;
}
