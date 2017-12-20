/*
 * drivers/video/tegra/host/t30/3dctx_t30.c
 *
 * Tegra Graphics Host 3d hardware context
 *
 * Copyright (c) 2011 NVIDIA Corporation.
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
#include "../t20/hardware_t20.h"
#include "../t20/syncpt_t20.h"
#include "../3dctx_common.h"

#include <mach/gpufuse.h>
#include <mach/hardware.h>
#include <linux/slab.h>

/*  99 > 2, which makes kernel panic if register set is incorrect */
static int register_sets = 99;
static bool s_war_insert_syncpoints;

static const struct hwctx_reginfo ctxsave_regs_3d_global[] = {
	HWCTX_REGINFO(0xe00,    4, DIRECT),
	HWCTX_REGINFO(0xe05,   30, DIRECT),
	HWCTX_REGINFO(0xe25,    2, DIRECT),
	HWCTX_REGINFO(0xe28,    2, DIRECT),
	HWCTX_REGINFO(0xe30,   16, DIRECT),
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
	HWCTX_REGINFO(0x412,    1, DIRECT),
	HWCTX_REGINFO(0x500,    4, DIRECT),
	HWCTX_REGINFO(0x520,   32, DIRECT),
	HWCTX_REGINFO(0x540,   64, INDIRECT),
	HWCTX_REGINFO(0x600,   16, INDIRECT_4X),
	HWCTX_REGINFO(0x603,  128, INDIRECT),
	HWCTX_REGINFO(0x608,    4, DIRECT),
	HWCTX_REGINFO(0x60e,    1, DIRECT),
	HWCTX_REGINFO(0x700,   64, INDIRECT),
	HWCTX_REGINFO(0x710,   50, DIRECT),
	HWCTX_REGINFO(0x750,   16, DIRECT),
	HWCTX_REGINFO(0x800,   16, INDIRECT_4X),
	HWCTX_REGINFO(0x803,  512, INDIRECT),
	HWCTX_REGINFO(0x805,   64, INDIRECT),
	HWCTX_REGINFO(0x820,   32, DIRECT),
	HWCTX_REGINFO(0x900,   64, INDIRECT),
	HWCTX_REGINFO(0x902,    2, DIRECT),
	HWCTX_REGINFO(0x90a,    1, DIRECT),
	HWCTX_REGINFO(0xa02,   10, DIRECT),
	HWCTX_REGINFO(0xb04,    1, DIRECT),
	HWCTX_REGINFO(0xb06,   13, DIRECT),
};

static const struct hwctx_reginfo ctxsave_regs_3d_perset[] = {
	HWCTX_REGINFO(0xe04,    1, DIRECT),
	HWCTX_REGINFO(0xe2a,    1, DIRECT),
	HWCTX_REGINFO(0x413,    1, DIRECT),
	HWCTX_REGINFO(0x90b,    1, DIRECT),
	HWCTX_REGINFO(0xe41,    1, DIRECT),
};

static unsigned int restore_set1_offset;

/* the same context save command sequence is used for all contexts. */
static phys_addr_t save_phys;
static unsigned int save_size;

#define SAVE_BEGIN_V1_SIZE (1 + RESTORE_BEGIN_SIZE)
#define SAVE_DIRECT_V1_SIZE (4 + RESTORE_DIRECT_SIZE)
#define SAVE_INDIRECT_V1_SIZE (6 + RESTORE_INDIRECT_SIZE)
#define SAVE_END_V1_SIZE (9 + RESTORE_END_SIZE)
#define SAVE_INCRS 3
#define SAVE_THRESH_OFFSET 0
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

/*** v1 saver ***/

static void save_push_v1(struct nvhost_cdma *cdma,
			struct nvhost_hwctx *ctx)
{
	/* wait for 3d idle */
	nvhost_cdma_push(cdma,
			nvhost_opcode_setclass(NV_GRAPHICS_3D_CLASS_ID, 0, 0),
			nvhost_opcode_imm_incr_syncpt(NV_SYNCPT_OP_DONE,
					NVSYNCPT_3D));
	nvhost_cdma_push(cdma,
			nvhost_opcode_setclass(NV_HOST1X_CLASS_ID,
					NV_CLASS_HOST_WAIT_SYNCPT_BASE, 1),
			nvhost_class_host_wait_syncpt_base(NVSYNCPT_3D,
							NVWAITBASE_3D, 1));
	/* back to 3d */
	nvhost_cdma_push(cdma,
			nvhost_opcode_setclass(NV_GRAPHICS_3D_CLASS_ID, 0, 0),
			NVHOST_OPCODE_NOOP);
	/* set register set 0 and 1 register read memory output addresses,
	   and send their reads to memory */
	if (register_sets == 2) {
		nvhost_cdma_push(cdma,
				nvhost_opcode_imm(0xb00, 2),
				nvhost_opcode_imm(0xe40, 1));
		nvhost_cdma_push(cdma,
				nvhost_opcode_nonincr(0x904, 1),
				ctx->restore_phys + restore_set1_offset * 4);
		if (s_war_insert_syncpoints)
			nvhost_cdma_push(cdma,
					NVHOST_OPCODE_NOOP,
					nvhost_opcode_imm_incr_syncpt(
							NV_SYNCPT_RD_DONE,
							NVSYNCPT_3D));
	}
	nvhost_cdma_push(cdma,
			nvhost_opcode_imm(0xb00, 1),
			nvhost_opcode_imm(0xe40, 1));
	nvhost_cdma_push(cdma,
			nvhost_opcode_nonincr(0x904, 1),
			ctx->restore_phys);
	/* gather the save buffer */
	nvhost_cdma_push_gather(cdma,
			(void *)NVHOST_CDMA_PUSH_GATHER_CTXSAVE,
			(void *)NVHOST_CDMA_PUSH_GATHER_CTXSAVE,
			nvhost_opcode_gather(save_size),
			save_phys);
}

static void __init save_begin_v1(u32 *ptr)
{
	ptr[0] = nvhost_opcode_nonincr(0x905, RESTORE_BEGIN_SIZE);
	nvhost_3dctx_restore_begin(ptr + 1);
	ptr += RESTORE_BEGIN_SIZE;
}

static void __init save_direct_v1(u32 *ptr, u32 start_reg, u32 count)
{
#if RESTORE_DIRECT_SIZE != 1
#error whoops! code is optimized for RESTORE_DIRECT_SIZE == 1
#endif
	ptr[0] = nvhost_opcode_setclass(NV_GRAPHICS_3D_CLASS_ID, 0x905, 1);
	nvhost_3dctx_restore_direct(ptr + 1, start_reg, count);
	ptr += RESTORE_DIRECT_SIZE;
	ptr[1] = nvhost_opcode_setclass(NV_HOST1X_CLASS_ID,
					NV_CLASS_HOST_INDOFF, 1);
	ptr[2] = nvhost_class_host_indoff_reg_read(NV_HOST_MODULE_GR3D,
						start_reg, true);
	/* TODO could do this in the setclass if count < 6 */
	ptr[3] = nvhost_opcode_nonincr(NV_CLASS_HOST_INDDATA, count);
}

static void __init save_indirect_v1(u32 *ptr, u32 offset_reg, u32 offset,
			u32 data_reg, u32 count)
{
	ptr[0] = nvhost_opcode_setclass(NV_GRAPHICS_3D_CLASS_ID, 0, 0);
	ptr[1] = nvhost_opcode_nonincr(0x905, RESTORE_INDIRECT_SIZE);
	nvhost_3dctx_restore_indirect(ptr + 2, offset_reg, offset, data_reg,
			count);
	ptr += RESTORE_INDIRECT_SIZE;
	ptr[2] = nvhost_opcode_imm(offset_reg, offset);
	ptr[3] = nvhost_opcode_setclass(NV_HOST1X_CLASS_ID,
					NV_CLASS_HOST_INDOFF, 1);
	ptr[4] = nvhost_class_host_indoff_reg_read(NV_HOST_MODULE_GR3D,
						data_reg, false);
	ptr[5] = nvhost_opcode_nonincr(NV_CLASS_HOST_INDDATA, count);
}

static void __init save_end_v1(u32 *ptr)
{
#if RESTORE_END_SIZE != 1
#error whoops! code is optimized for RESTORE_END_SIZE == 1
#endif
	/* write end of restore buffer */
	ptr[0] = nvhost_opcode_setclass(NV_GRAPHICS_3D_CLASS_ID, 0x905, 1);
	nvhost_3dctx_restore_end(ptr + 1);
	ptr += RESTORE_END_SIZE;
	/* reset to dual reg if necessary */
	if (s_war_insert_syncpoints) {
		ptr[1] = nvhost_opcode_imm_incr_syncpt(NV_SYNCPT_RD_DONE,
				NVSYNCPT_3D);
		ptr += 1;
	}
	ptr[1] = nvhost_opcode_imm(0xb00, (1 << register_sets) - 1);
	/* op_done syncpt incr to flush FDC */
	ptr[2] = nvhost_opcode_imm_incr_syncpt(NV_SYNCPT_OP_DONE, NVSYNCPT_3D);
	/* host wait for that syncpt incr, and advance the wait base */
	ptr[3] = nvhost_opcode_setclass(NV_HOST1X_CLASS_ID,
			NV_CLASS_HOST_WAIT_SYNCPT_BASE,
			nvhost_mask2(
					NV_CLASS_HOST_WAIT_SYNCPT_BASE,
					NV_CLASS_HOST_INCR_SYNCPT_BASE));
	ptr[4] = nvhost_class_host_wait_syncpt_base(NVSYNCPT_3D,
				NVWAITBASE_3D, nvhost_3dctx_save_incrs - 1);
	ptr[5] = nvhost_class_host_incr_syncpt_base(NVWAITBASE_3D,
			nvhost_3dctx_save_incrs);
	/* set class back to 3d */
	ptr[6] = nvhost_opcode_setclass(NV_GRAPHICS_3D_CLASS_ID, 0, 0);
	/* send reg reads back to host */
	ptr[7] = nvhost_opcode_imm(0xe40, 0);
	/* final syncpt increment to release waiters */
	ptr[8] = nvhost_opcode_imm(0, NVSYNCPT_3D);
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
				save_direct_v1(ptr, offset, count);
				ptr += SAVE_DIRECT_V1_SIZE;
			}
			save_count += SAVE_DIRECT_V1_SIZE;
			restore_count += RESTORE_DIRECT_SIZE;
			break;
		case HWCTX_REGINFO_INDIRECT_4X:
			++indoff;
			/* fall through */
		case HWCTX_REGINFO_INDIRECT:
			if (ptr) {
				save_indirect_v1(ptr, offset, 0,
						indoff, count);
				ptr += SAVE_INDIRECT_V1_SIZE;
			}
			save_count += SAVE_INDIRECT_V1_SIZE;
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

static void __init switch_gpu(struct save_info *info,
			unsigned int save_src_set,
			u32 save_dest_sets,
			u32 restore_dest_sets)
{
	if (s_war_insert_syncpoints) {
		if (info->ptr) {
			info->ptr[0] = nvhost_opcode_setclass(
					NV_GRAPHICS_3D_CLASS_ID, 0, 0);
			info->ptr[1] = nvhost_opcode_nonincr(0x905, 2);
			info->ptr[2] = nvhost_opcode_imm_incr_syncpt(
					NV_SYNCPT_RD_DONE, NVSYNCPT_3D);
			info->ptr[3] = nvhost_opcode_imm(0xb00,
					restore_dest_sets);
			info->ptr[4] = nvhost_opcode_imm_incr_syncpt(
					NV_SYNCPT_RD_DONE, NVSYNCPT_3D);
			info->ptr[5] = nvhost_opcode_imm(0xb00, save_dest_sets);
			info->ptr[6] = nvhost_opcode_imm(0xb01, save_src_set);
			info->ptr += 7;
		}
		info->save_count += 7;
		info->restore_count += 2;
		info->save_incrs += 1;
		info->restore_incrs += 1;
	} else {
		if (info->ptr) {
			info->ptr[0] = nvhost_opcode_setclass(
					NV_GRAPHICS_3D_CLASS_ID, 0x905, 1);
			info->ptr[1] = nvhost_opcode_imm(0xb00,
					restore_dest_sets);
			info->ptr[2] = nvhost_opcode_imm(0xb00,
					save_dest_sets);
			info->ptr[3] = nvhost_opcode_imm(0xb01, save_src_set);
			info->ptr += 4;
		}
		info->save_count += 4;
		info->restore_count += 1;
	}
}

static void __init setup_save(u32 *ptr)
{
	struct save_info info = {
		ptr,
		SAVE_BEGIN_V1_SIZE,
		RESTORE_BEGIN_SIZE,
		SAVE_INCRS,
		1
	};
	int save_end_size = SAVE_END_V1_SIZE;

	BUG_ON(register_sets > 2);

	if (info.ptr) {
		save_begin_v1(info.ptr);
		info.ptr += SAVE_BEGIN_V1_SIZE;
	}

	/* read from set0, write cmds through set0, restore to sets 0 and 1 */
	if (register_sets == 2)
		switch_gpu(&info, 0, 1, 3);

	/* save regs that are common to both sets */
	setup_save_regs(&info,
			ctxsave_regs_3d_global,
			ARRAY_SIZE(ctxsave_regs_3d_global));

	/* read from set 0, write cmds through set 0, restore to set 0 */
	if (register_sets == 2)
		switch_gpu(&info, 0, 1, 1);

	/* save set 0 specific regs */
	setup_save_regs(&info,
			ctxsave_regs_3d_perset,
			ARRAY_SIZE(ctxsave_regs_3d_perset));

	if (register_sets == 2) {
		/* read from set1, write cmds through set1, restore to set1 */
		switch_gpu(&info, 1, 2, 2);
		/* note offset at which set 1 restore starts */
		restore_set1_offset = info.restore_count;
		/* save set 1 specific regs */
		setup_save_regs(&info,
				ctxsave_regs_3d_perset,
				ARRAY_SIZE(ctxsave_regs_3d_perset));
	}

	/* read from set 0, write cmds through set 1, restore to sets 0 and 1 */
	if (register_sets == 2)
		switch_gpu(&info, 0, 2, 3);

	if (s_war_insert_syncpoints) {
		info.save_incrs += register_sets;
		save_end_size++;
	}

	if (info.ptr) {
		save_end_v1(info.ptr);
		info.ptr += SAVE_END_V1_SIZE;
	}

	wmb();

	save_size = info.save_count + save_end_size;
	nvhost_3dctx_restore_size = info.restore_count + RESTORE_END_SIZE;
	nvhost_3dctx_save_incrs = info.save_incrs;
	nvhost_3dctx_save_thresh = nvhost_3dctx_save_incrs - SAVE_THRESH_OFFSET;
	nvhost_3dctx_restore_incrs = info.restore_incrs;
}


/*** ctx3d ***/

static struct nvhost_hwctx *ctx3d_alloc_v1(struct nvhost_channel *ch)
{
	return nvhost_3dctx_alloc_common(ch, false);
}

int __init t30_nvhost_3dctx_handler_init(struct nvhost_hwctx_handler *h)
{
	struct nvhost_channel *ch;
	struct nvmap_client *nvmap;
	u32 *save_ptr;

	ch = container_of(h, struct nvhost_channel, ctxhandler);
	nvmap = ch->dev->nvmap;

	register_sets = tegra_gpu_register_sets();
	BUG_ON(register_sets == 0 || register_sets > 2);

	/* Tegra3 A01 workaround */
	s_war_insert_syncpoints = tegra_get_revision() == TEGRA_REVISION_A01;

	setup_save(NULL);

	nvhost_3dctx_save_buf = nvmap_alloc(nvmap, save_size * 4, 32,
				NVMAP_HANDLE_WRITE_COMBINE);
	if (IS_ERR(nvhost_3dctx_save_buf)) {
		int err = PTR_ERR(nvhost_3dctx_save_buf);
		nvhost_3dctx_save_buf = NULL;
		return err;
	}

	nvhost_3dctx_save_slots = 6;
	if (register_sets == 2)
		nvhost_3dctx_save_slots += 2;
	if (s_war_insert_syncpoints)
		nvhost_3dctx_save_slots += 1;

	save_ptr = nvmap_mmap(nvhost_3dctx_save_buf);
	if (!save_ptr) {
		nvmap_free(nvmap, nvhost_3dctx_save_buf);
		nvhost_3dctx_save_buf = NULL;
		return -ENOMEM;
	}

	save_phys = nvmap_pin(nvmap, nvhost_3dctx_save_buf);

	setup_save(save_ptr);

	h->alloc = ctx3d_alloc_v1;
	h->save_push = save_push_v1;
	h->save_service = NULL;
	h->get = nvhost_3dctx_get;
	h->put = nvhost_3dctx_put;

	return 0;
}
