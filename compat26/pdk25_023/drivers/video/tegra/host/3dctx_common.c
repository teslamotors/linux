/*
 * drivers/video/tegra/host/3dctx_common.c
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

/*** restore ***/

#include <mach/nvmap.h>
#include <linux/slab.h>
#include "3dctx_common.h"
#include "t20/hardware_t20.h"
#include "t20/syncpt_t20.h"
#include "nvhost_hwctx.h"
#include "dev.h"

unsigned int nvhost_3dctx_restore_size;
unsigned int nvhost_3dctx_restore_incrs;
struct nvmap_handle_ref *nvhost_3dctx_save_buf;
unsigned int nvhost_3dctx_save_incrs;
unsigned int nvhost_3dctx_save_thresh;
unsigned int nvhost_3dctx_save_slots;

void nvhost_3dctx_restore_begin(u32 *ptr)
{
	/* set class to host */
	ptr[0] = nvhost_opcode_setclass(NV_HOST1X_CLASS_ID,
					NV_CLASS_HOST_INCR_SYNCPT_BASE, 1);
	/* increment sync point base */
	ptr[1] = nvhost_class_host_incr_syncpt_base(NVWAITBASE_3D,
			nvhost_3dctx_restore_incrs);
	/* set class to 3D */
	ptr[2] = nvhost_opcode_setclass(NV_GRAPHICS_3D_CLASS_ID, 0, 0);
	/* program PSEQ_QUAD_ID */
	ptr[3] = nvhost_opcode_imm(0x545, 0);
}

void nvhost_3dctx_restore_direct(u32 *ptr, u32 start_reg, u32 count)
{
	ptr[0] = nvhost_opcode_incr(start_reg, count);
}

void nvhost_3dctx_restore_indirect(u32 *ptr, u32 offset_reg, u32 offset,
			u32 data_reg, u32 count)
{
	ptr[0] = nvhost_opcode_imm(offset_reg, offset);
	ptr[1] = nvhost_opcode_nonincr(data_reg, count);
}

void nvhost_3dctx_restore_end(u32 *ptr)
{
	/* syncpt increment to track restore gather. */
	ptr[0] = nvhost_opcode_imm_incr_syncpt(
			NV_SYNCPT_OP_DONE, NVSYNCPT_3D);
}

/*** ctx3d ***/

struct nvhost_hwctx *nvhost_3dctx_alloc_common(struct nvhost_channel *ch,
					bool map_restore)
{
	struct nvmap_client *nvmap = ch->dev->nvmap;
	struct nvhost_hwctx *ctx;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return NULL;
	ctx->restore = nvmap_alloc(nvmap, nvhost_3dctx_restore_size * 4, 32,
		map_restore ? NVMAP_HANDLE_WRITE_COMBINE
			    : NVMAP_HANDLE_UNCACHEABLE);
	if (IS_ERR_OR_NULL(ctx->restore))
		goto fail;

	if (map_restore) {
		ctx->restore_virt = nvmap_mmap(ctx->restore);
		if (!ctx->restore_virt)
			goto fail;
	} else
		ctx->restore_virt = NULL;

	kref_init(&ctx->ref);
	ctx->channel = ch;
	ctx->valid = false;
	ctx->save = nvhost_3dctx_save_buf;
	ctx->save_incrs = nvhost_3dctx_save_incrs;
	ctx->save_thresh = nvhost_3dctx_save_thresh;
	ctx->save_slots = nvhost_3dctx_save_slots;
	ctx->restore_phys = nvmap_pin(nvmap, ctx->restore);
	if (IS_ERR_VALUE(ctx->restore_phys))
		goto fail;

	ctx->restore_size = nvhost_3dctx_restore_size;
	ctx->restore_incrs = nvhost_3dctx_restore_incrs;
	return ctx;

fail:
	if (map_restore && ctx->restore_virt) {
		nvmap_munmap(ctx->restore, ctx->restore_virt);
		ctx->restore_virt = NULL;
	}
	nvmap_free(nvmap, ctx->restore);
	ctx->restore = NULL;
	kfree(ctx);
	return NULL;
}

void nvhost_3dctx_get(struct nvhost_hwctx *ctx)
{
	kref_get(&ctx->ref);
}

void nvhost_3dctx_free(struct kref *ref)
{
	struct nvhost_hwctx *ctx = container_of(ref, struct nvhost_hwctx, ref);
	struct nvmap_client *nvmap = ctx->channel->dev->nvmap;

	if (ctx->restore_virt) {
		nvmap_munmap(ctx->restore, ctx->restore_virt);
		ctx->restore_virt = NULL;
	}
	nvmap_unpin(nvmap, ctx->restore);
	ctx->restore_phys = 0;
	nvmap_free(nvmap, ctx->restore);
	ctx->restore = NULL;
	kfree(ctx);
}

void nvhost_3dctx_put(struct nvhost_hwctx *ctx)
{
	kref_put(&ctx->ref, nvhost_3dctx_free);
}

int nvhost_3dctx_prepare_power_off(struct nvhost_module *mod)
{
	struct nvhost_channel *ch =
			container_of(mod, struct nvhost_channel, mod);
	struct nvhost_hwctx *hwctx_to_save;
	DECLARE_WAIT_QUEUE_HEAD_ONSTACK(wq);
	u32 syncpt_incrs, syncpt_val;
	int err = 0;
	void *ref;
	void *ctx_waiter = NULL, *wakeup_waiter = NULL;

	ctx_waiter = nvhost_intr_alloc_waiter();
	wakeup_waiter = nvhost_intr_alloc_waiter();
	if (!ctx_waiter || !wakeup_waiter) {
		err = -ENOMEM;
		goto done;
	}

	if (mod->desc->busy)
		mod->desc->busy(mod);

	mutex_lock(&ch->submitlock);
	hwctx_to_save = ch->cur_ctx;
	if (!hwctx_to_save) {
		mutex_unlock(&ch->submitlock);
		goto done;
	}

	err = nvhost_cdma_begin(&ch->cdma, hwctx_to_save->timeout);
	if (err) {
		mutex_unlock(&ch->submitlock);
		goto done;
	}

	hwctx_to_save->valid = true;
	ch->ctxhandler.get(hwctx_to_save);
	ch->cur_ctx = NULL;

	syncpt_incrs = hwctx_to_save->save_incrs;
	syncpt_val = nvhost_syncpt_incr_max(&ch->dev->syncpt,
					NVSYNCPT_3D, syncpt_incrs);

	ch->ctxhandler.save_push(&ch->cdma, hwctx_to_save);
	nvhost_cdma_end(&ch->cdma, ch->dev->nvmap, NVSYNCPT_3D, syncpt_val,
			NULL, 0, hwctx_to_save->timeout);

	err = nvhost_intr_add_action(&ch->dev->intr, NVSYNCPT_3D,
			syncpt_val - syncpt_incrs + hwctx_to_save->save_thresh,
			NVHOST_INTR_ACTION_CTXSAVE, hwctx_to_save,
			ctx_waiter,
			NULL);
	ctx_waiter = NULL;
	WARN(err, "Failed to set context save interrupt");

	err = nvhost_intr_add_action(&ch->dev->intr, NVSYNCPT_3D, syncpt_val,
			NVHOST_INTR_ACTION_WAKEUP, &wq,
			wakeup_waiter,
			&ref);
	wakeup_waiter = NULL;
	WARN(err, "Failed to set wakeup interrupt");
	wait_event(wq,
		nvhost_syncpt_is_expired(&ch->dev->syncpt,
				NVSYNCPT_3D, syncpt_val));

	nvhost_intr_put_ref(&ch->dev->intr, ref);

	nvhost_cdma_update(&ch->cdma);

	mutex_unlock(&ch->submitlock);

done:
	kfree(ctx_waiter);
	kfree(wakeup_waiter);
	return err;
}
