/*
 * drivers/video/tegra/host/t30/channel_t30.c
 *
 * Tegra Graphics Host Channel
 *
 * Copyright (c) 2010-2013, NVIDIA Corporation.
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

#include <linux/mutex.h>
#include <mach/powergate.h>
#include "../dev.h"
#include "../t20/channel_t20.h"
#include "../t20/hardware_t20.h"
#include "../t20/t20.h"
#include "../t20/syncpt_t20.h"
#include "../3dctx_common.h"
#include "3dctx_t30.h"
#include "scale3d.h"

#define NVMODMUTEX_2D_FULL   (1)
#define NVMODMUTEX_2D_SIMPLE (2)
#define NVMODMUTEX_2D_SB_A   (3)
#define NVMODMUTEX_2D_SB_B   (4)
#define NVMODMUTEX_3D        (5)
#define NVMODMUTEX_DISPLAYA  (6)
#define NVMODMUTEX_DISPLAYB  (7)
#define NVMODMUTEX_VI        (8)
#define NVMODMUTEX_DSI       (9)
#define NV_FIFO_READ_TIMEOUT 200000

#define HOST_EMC_FLOOR 300000000
#ifndef TEGRA_POWERGATE_3D1
#define TEGRA_POWERGATE_3D1 -1
#endif

const struct nvhost_channeldesc nvhost_t30_channelmap[] = {
{
	/* channel 0 */
	.name	       = "display",
	.syncpts       = BIT(NVSYNCPT_DISP0_A) | BIT(NVSYNCPT_DISP1_A) |
			 BIT(NVSYNCPT_DISP0_B) | BIT(NVSYNCPT_DISP1_B) |
			 BIT(NVSYNCPT_DISP0_C) | BIT(NVSYNCPT_DISP1_C) |
			 BIT(NVSYNCPT_VBLANK0) | BIT(NVSYNCPT_VBLANK1),
	.modulemutexes = BIT(NVMODMUTEX_DISPLAYA) | BIT(NVMODMUTEX_DISPLAYB),
	.module        = {
			NVHOST_MODULE_NO_POWERGATE_IDS,
			NVHOST_DEFAULT_CLOCKGATE_DELAY,
			},
},
{
	/* channel 1 */
	.name	       = "gr3d",
	.syncpts       = BIT(NVSYNCPT_3D),
	.waitbases     = BIT(NVWAITBASE_3D),
	.modulemutexes = BIT(NVMODMUTEX_3D),
	.class	       = NV_GRAPHICS_3D_CLASS_ID,
	.waitbasesync  = false,
	.module        = {
			.prepare_poweroff = nvhost_3dctx_prepare_power_off,
			.busy = nvhost_scale3d_notify_busy,
			.idle = nvhost_scale3d_notify_idle,
			.init = nvhost_scale3d_init,
			.deinit = nvhost_scale3d_deinit,
			.suspend = nvhost_scale3d_suspend,
			.clocks = {{"gr3d", UINT_MAX, true},
					{"gr3d2", UINT_MAX, true},
					{"emc", HOST_EMC_FLOOR} },
			.powergate_ids = {TEGRA_POWERGATE_3D,
					TEGRA_POWERGATE_3D1},
			NVHOST_DEFAULT_CLOCKGATE_DELAY,
			.can_powergate = false,
			.powergate_delay = 100,
	},
},
{
	/* channel 2 */
	.name	       = "gr2d",
	.syncpts       = BIT(NVSYNCPT_2D_0) | BIT(NVSYNCPT_2D_1),
	.waitbases     = BIT(NVWAITBASE_2D_0) | BIT(NVWAITBASE_2D_1),
	.modulemutexes = BIT(NVMODMUTEX_2D_FULL) | BIT(NVMODMUTEX_2D_SIMPLE) |
			 BIT(NVMODMUTEX_2D_SB_A) | BIT(NVMODMUTEX_2D_SB_B),
	.serialize     = true,
	.module        = {
			.clocks = {{"gr2d", 0, true},
					{"epp", UINT_MAX, true},
					{"emc", HOST_EMC_FLOOR} },
			NVHOST_MODULE_NO_POWERGATE_IDS,
			.clockgate_delay = 0,
			},
},
{
	/* channel 3 */
	.name	 = "isp",
	.syncpts = 0,
	.module         = {
			NVHOST_MODULE_NO_POWERGATE_IDS,
			NVHOST_DEFAULT_CLOCKGATE_DELAY,
			},
},
{
	/* channel 4 */
	.name	       = "vi",
	.syncpts       = BIT(NVSYNCPT_CSI_VI_0) | BIT(NVSYNCPT_CSI_VI_1) |
			 BIT(NVSYNCPT_VI_ISP_0) | BIT(NVSYNCPT_VI_ISP_1) |
			 BIT(NVSYNCPT_VI_ISP_2) | BIT(NVSYNCPT_VI_ISP_3) |
			 BIT(NVSYNCPT_VI_ISP_4),
	.modulemutexes = BIT(NVMODMUTEX_VI),
	.exclusive     = true,
	.module        = {
			NVHOST_MODULE_NO_POWERGATE_IDS,
			NVHOST_DEFAULT_CLOCKGATE_DELAY,
			},
},
{
	/* channel 5 */
	.name	       = "mpe",
	.syncpts       = BIT(NVSYNCPT_MPE) | BIT(NVSYNCPT_MPE_EBM_EOF) |
			 BIT(NVSYNCPT_MPE_WR_SAFE),
	.waitbases     = BIT(NVWAITBASE_MPE),
	.class	       = NV_VIDEO_ENCODE_MPEG_CLASS_ID,
	.exclusive     = true,
	.keepalive     = true,
	.module        = {
			.clocks = {{"mpe", UINT_MAX, true},
					{"emc", UINT_MAX}, {} },
			.powergate_ids  = {TEGRA_POWERGATE_MPE, -1},
			NVHOST_DEFAULT_CLOCKGATE_DELAY,
			.can_powergate  = true,
			.powergate_delay = 100,
			},
},
{
	/* channel 6 */
	.name	       = "dsi",
	.syncpts       = BIT(NVSYNCPT_DSI),
	.modulemutexes = BIT(NVMODMUTEX_DSI),
	.module        = {
			NVHOST_MODULE_NO_POWERGATE_IDS,
			NVHOST_DEFAULT_CLOCKGATE_DELAY,
	},
} };

#define NVHOST_CHANNEL_BASE 0

static inline int t30_nvhost_hwctx_handler_init(
	struct nvhost_hwctx_handler *h,
	const char *module)
{
	if (strcmp(module, "gr3d") == 0)
		return t30_nvhost_3dctx_handler_init(h);

	return 0;
}

static inline void __iomem *t30_channel_aperture(void __iomem *p, int ndx)
{
	ndx += NVHOST_CHANNEL_BASE;
	p += NV_HOST1X_CHANNEL0_BASE;
	p += ndx * NV_HOST1X_CHANNEL_MAP_SIZE_BYTES;
	return p;
}

static int t30_channel_init(struct nvhost_channel *ch,
			    struct nvhost_master *dev, int index)
{
	ch->dev = dev;
	ch->chid = index;
	ch->desc = nvhost_t30_channelmap + index;
	mutex_init(&ch->reflock);
	mutex_init(&ch->submitlock);

	ch->aperture = t30_channel_aperture(dev->aperture, index);

	return t30_nvhost_hwctx_handler_init(&ch->ctxhandler, ch->desc->name);
}

int nvhost_init_t30_channel_support(struct nvhost_master *host)
{
	int result = nvhost_init_t20_channel_support(host);
	host->op.channel.init = t30_channel_init;

	return result;
}
