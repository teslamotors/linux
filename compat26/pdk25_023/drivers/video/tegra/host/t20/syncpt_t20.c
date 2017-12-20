/*
 * drivers/video/tegra/host/t20/syncpt_t20.c
 *
 * Tegra Graphics Host Syncpoints for T20
 *
 * Copyright (c) 2010-2012, NVIDIA Corporation.
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

#include <linux/nvhost_ioctl.h>
#include "../nvhost_syncpt.h"
#include "../dev.h"

#include "syncpt_t20.h"
#include "hardware_t20.h"

/**
 * Write the current syncpoint value back to hw.
 */
static void t20_syncpt_reset(struct nvhost_syncpt *sp, u32 id)
{
	struct nvhost_master *dev = syncpt_to_dev(sp);
	int min = nvhost_syncpt_read_min(sp, id);
	writel(min, dev->sync_aperture + (HOST1X_SYNC_SYNCPT_0 + id * 4));
}

/**
 * Write the current waitbase value back to hw.
 */
static void t20_syncpt_reset_wait_base(struct nvhost_syncpt *sp, u32 id)
{
	struct nvhost_master *dev = syncpt_to_dev(sp);
	writel(sp->base_val[id],
		dev->sync_aperture + (HOST1X_SYNC_SYNCPT_BASE_0 + id * 4));
}

/**
 * Read waitbase value from hw.
 */
static void t20_syncpt_read_wait_base(struct nvhost_syncpt *sp, u32 id)
{
	struct nvhost_master *dev = syncpt_to_dev(sp);
	sp->base_val[id] = readl(dev->sync_aperture +
				(HOST1X_SYNC_SYNCPT_BASE_0 + id * 4));
}

/**
 * Updates the last value read from hardware.
 * (was nvhost_syncpt_update_min)
 */
static u32 t20_syncpt_update_min(struct nvhost_syncpt *sp, u32 id)
{
	struct nvhost_master *dev = syncpt_to_dev(sp);
	void __iomem *sync_regs = dev->sync_aperture;
	u32 old, live;

	do {
		old = nvhost_syncpt_read_min(sp, id);
		live = readl(sync_regs + (HOST1X_SYNC_SYNCPT_0 + id * 4));
	} while ((u32)atomic_cmpxchg(&sp->min_val[id], old, live) != old);

	if (!nvhost_syncpt_check_max(sp, id, live)) {
		dev_err(&syncpt_to_dev(sp)->pdev->dev,
				"%s failed: id=%u\n",
				__func__,
				id);
		nvhost_debug_dump(syncpt_to_dev(sp));
		BUG();
	}
	return live;
}

/**
 * Write a cpu syncpoint increment to the hardware, without touching
 * the cache. Caller is responsible for host being powered.
 */
static void t20_syncpt_cpu_incr(struct nvhost_syncpt *sp, u32 id)
{
	struct nvhost_master *dev = syncpt_to_dev(sp);
	BUG_ON(!nvhost_module_powered(&dev->mod));
	if (!client_managed(id) && nvhost_syncpt_min_eq_max(sp, id)) {
		dev_err(&syncpt_to_dev(sp)->pdev->dev,
				"Syncpoint id %d\n",
				id);
		nvhost_debug_dump(syncpt_to_dev(sp));
		BUG();
	}
	writel(BIT(id), dev->sync_aperture + HOST1X_SYNC_SYNCPT_CPU_INCR);
	wmb();
}

/* check for old WAITs to be removed (avoiding a wrap) */
static int t20_syncpt_wait_check(struct nvhost_syncpt *sp,
				 struct nvmap_client *nvmap,
				 u32 waitchk_mask,
				 struct nvhost_waitchk *wait,
				 struct nvhost_waitchk *waitend)
{
	u32 idx;
	int err = 0;

	/* get current syncpt values */
	for (idx = 0; idx < NV_HOST1X_SYNCPT_NB_PTS; idx++) {
		if (BIT(idx) & waitchk_mask)
			nvhost_syncpt_update_min(sp, idx);
	}

	BUG_ON(!wait && !waitend);

	/* compare syncpt vs wait threshold */
	while (wait != waitend) {
		u32 override;

		BUG_ON(wait->syncpt_id >= NV_HOST1X_SYNCPT_NB_PTS);

		if (nvhost_syncpt_is_expired(sp,
				wait->syncpt_id, wait->thresh)) {
			/*
			 * NULL an already satisfied WAIT_SYNCPT host method,
			 * by patching its args in the command stream. The
			 * method data is changed to reference a reserved
			 * (never given out or incr) NVSYNCPT_GRAPHICS_HOST
			 * syncpt with a matching threshold value of 0, so
			 * is guaranteed to be popped by the host HW.
			 */
			dev_dbg(&syncpt_to_dev(sp)->pdev->dev,
			    "drop WAIT id %d (%s) thresh 0x%x, min 0x%x\n",
			    wait->syncpt_id,
			    nvhost_syncpt_name(wait->syncpt_id),
			    wait->thresh,
			    nvhost_syncpt_read(sp, wait->syncpt_id));

			/* patch the wait */
			override = nvhost_class_host_wait_syncpt(
					NVSYNCPT_GRAPHICS_HOST, 0);
			err = nvmap_patch_word(nvmap,
					(struct nvmap_handle *)wait->mem,
					wait->offset, override);
			if (err)
				break;
		}
		wait++;
	}
	return err;
}


static const char *s_syncpt_names[32] = {
	"gfx_host",
	"", "", "", "", "", "", "",
	"disp0_a", "disp1_a", "avp_0",
	"csi_vi_0", "csi_vi_1",
	"vi_isp_0", "vi_isp_1", "vi_isp_2", "vi_isp_3", "vi_isp_4",
	"2d_0", "2d_1",
	"disp0_b", "disp1_b",
	"3d",
	"mpe",
	"disp0_c", "disp1_c",
	"vblank0", "vblank1",
	"mpe_ebm_eof", "mpe_wr_safe",
	"2d_tinyblt",
	"dsi"
};

static const char *t20_syncpt_name(struct nvhost_syncpt *s, u32 id)
{
	BUG_ON(id >= ARRAY_SIZE(s_syncpt_names));
	return s_syncpt_names[id];
}

static void t20_syncpt_debug(struct nvhost_syncpt *sp)
{
	u32 i;
	for (i = 0; i < NV_HOST1X_SYNCPT_NB_PTS; i++) {
		u32 max = nvhost_syncpt_read_max(sp, i);
		if (!max)
			continue;
		dev_info(&syncpt_to_dev(sp)->pdev->dev,
			"id %d (%s) min %d max %d\n",
			 i, syncpt_op(sp).name(sp, i),
			nvhost_syncpt_update_min(sp, i), max);

	}

	for (i = 0; i < NV_HOST1X_SYNCPT_NB_BASES; i++) {
		u32 base_val;
		t20_syncpt_read_wait_base(sp, i);
		base_val = sp->base_val[i];
		if (base_val)
			dev_info(&syncpt_to_dev(sp)->pdev->dev,
					"waitbase id %d val %d\n",
					i, base_val);

	}
}

int nvhost_init_t20_syncpt_support(struct nvhost_master *host)
{

	host->sync_aperture = host->aperture +
		(NV_HOST1X_CHANNEL0_BASE +
			HOST1X_CHANNEL_SYNC_REG_BASE);

	host->op.syncpt.reset = t20_syncpt_reset;
	host->op.syncpt.reset_wait_base = t20_syncpt_reset_wait_base;
	host->op.syncpt.read_wait_base = t20_syncpt_read_wait_base;
	host->op.syncpt.update_min = t20_syncpt_update_min;
	host->op.syncpt.cpu_incr = t20_syncpt_cpu_incr;
	host->op.syncpt.wait_check = t20_syncpt_wait_check;
	host->op.syncpt.debug = t20_syncpt_debug;
	host->op.syncpt.name = t20_syncpt_name;

	host->syncpt.nb_pts = NV_HOST1X_SYNCPT_NB_PTS;
	host->syncpt.nb_bases = NV_HOST1X_SYNCPT_NB_BASES;
	host->syncpt.client_managed = NVSYNCPTS_CLIENT_MANAGED;

	return 0;
}
