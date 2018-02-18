/*
 * drivers/video/tegra/host/t124/t124.c
 *
 * Tegra Graphics Init for T124 Architecture Chips
 *
 * Copyright (c) 2011-2016, NVIDIA CORPORATION. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.>
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <linux/slab.h>
#include <linux/tegra-powergate.h>
#if defined(CONFIG_ARCH_TEGRA_210_SOC)
#include <soc/tegra/fuse.h>
#else
#include <linux/tegra-fuse.h>
#endif

#include <linux/platform/tegra/mc.h>

#include "dev.h"
#include "nvhost_channel.h"
#include "nvhost_job.h"
#include "class_ids.h"
#include "t124.h"
#include "host1x/host1x.h"
#include "hardware_t124.h"
#include "syncpt_t124.h"
#include "tsec/tsec.h"
#include "flcn/flcn.h"
#if defined(CONFIG_VIDEO_TEGRA_VI) || defined(CONFIG_VIDEO_TEGRA_VI_MODULE)
#include "vi.h"
#endif
#include "isp/isp.h"
#include "isp/isp_isr_v1.h"
#include "scale_emc.h"
#include "chip_support.h"
#include "nvhost_scale.h"
#include "vhost/vhost.h"

#include "../../../../arch/arm/mach-tegra/iomap.h"

#include "cg_regs.c"

#define HOST_EMC_FLOOR 300000000
#define VI_CLOCKGATE_DELAY 60
#define VI_POWERGATE_DELAY 500
#define ISP_CLOCKGATE_DELAY 60
#define ISP_POWERGATE_DELAY 500
#define TSEC_POWERGATE_DELAY 500
#define HOST1X_POWERGATE_DELAY 50

#ifndef INT_HOST1X_MPCORE_SYNCPT
#define INT_HOST1X_MPCORE_SYNCPT -1
#endif
#ifndef INT_HOST1X_MPCORE_GENERAL
#define INT_HOST1X_MPCORE_GENERAL -1
#endif

#ifdef CONFIG_ARCH_TEGRA
static struct resource tegra_host1x04_resources[] = {
	{
		.start = TEGRA_HOST1X_BASE,
		.end = TEGRA_HOST1X_BASE + TEGRA_HOST1X_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	{
		.start = INT_HOST1X_MPCORE_SYNCPT,
		.end = INT_HOST1X_MPCORE_SYNCPT,
		.flags = IORESOURCE_IRQ,
	},
	{
		.start = INT_HOST1X_MPCORE_GENERAL,
		.end = INT_HOST1X_MPCORE_GENERAL,
		.flags = IORESOURCE_IRQ,
	},
};
#endif

static struct host1x_device_info host1x04_info = {
	.nb_channels	= T124_NVHOST_NUMCHANNELS,
	.ch_base	= 0,
	.ch_limit	= T124_NVHOST_NUMCHANNELS,
	.nb_mlocks	= NV_HOST1X_NB_MLOCKS,
	.initialize_chip_support = nvhost_init_t124_support,
	.nb_hw_pts	= NV_HOST1X_SYNCPT_NB_PTS,
	.nb_pts		= NV_HOST1X_SYNCPT_NB_PTS,
	.pts_base	= 0,
	.pts_limit	= NV_HOST1X_SYNCPT_NB_PTS,
	.syncpt_policy	= SYNCPT_PER_CHANNEL,
	.nb_actmons	= 1,
	.firmware_area_size = SZ_1M,
};

struct nvhost_device_data t124_host1x_info = {
	.clocks		= {{"host1x", 81600000}, {"actmon", UINT_MAX} },
	NVHOST_MODULE_NO_POWERGATE_ID,
	.can_powergate   = true,
	.powergate_delay = HOST1X_POWERGATE_DELAY,
	.private_data	= &host1x04_info,
	.finalize_poweron = nvhost_host1x_finalize_poweron,
	.prepare_poweroff = nvhost_host1x_prepare_poweroff,
};


static struct platform_device tegra_host1x04_device = {
	.name		= "host1x",
	.id		= -1,
#ifdef CONFIG_ARCH_TEGRA
	.resource	= tegra_host1x04_resources,
	.num_resources	= ARRAY_SIZE(tegra_host1x04_resources),
#endif
	.dev            = {
		.platform_data = &t124_host1x_info,
	},
};


#ifdef CONFIG_TEGRA_GRHOST_ISP
static struct resource isp_resources[] = {
	{
		.name = "regs",
		.start = TEGRA_ISP_BASE,
		.end = TEGRA_ISP_BASE + TEGRA_ISP_SIZE - 1,
		.flags = IORESOURCE_MEM,
	}
};

static struct platform_device tegra_isp01b_device;
struct nvhost_device_data t124_isp_info = {
	.num_channels	= 1,
	/* FIXME: control clocks from user space instead of hard-coding here */
	.moduleid        = NVHOST_MODULE_ISP,
	.class           = NV_VIDEO_STREAMING_ISP_CLASS_ID,
	.modulemutexes   = {NVMODMUTEX_ISP_0},
	.devfs_name      = "isp",
	.exclusive       = true,
	.keepalive       = true,
	.can_powergate   = true,
	.clockgate_delay = ISP_CLOCKGATE_DELAY,
	.powergate_delay = ISP_POWERGATE_DELAY,
	.clocks          = {
		{"isp", UINT_MAX, 0, TEGRA_MC_CLIENT_ISP},
		{"emc", 0, NVHOST_MODULE_ID_EXTERNAL_MEMORY_CONTROLLER},
		{"sclk", 80000000} },
	.finalize_poweron = nvhost_isp_t124_finalize_poweron,
	.prepare_poweroff = nvhost_isp_t124_prepare_poweroff,
	.hw_init          = nvhost_isp_register_isr_v1,
	.ctrl_ops         = &tegra_isp_ctrl_ops,
};
static struct platform_device tegra_isp01_device = {
	.name          = "isp",
	.resource      = isp_resources,
	.num_resources = ARRAY_SIZE(isp_resources),
	.dev           = {
		.platform_data = &t124_isp_info,
	},
};

static struct resource ispb_resources[] = {
	{
		.name = "regs",
		.start = TEGRA_ISPB_BASE,
		.end = TEGRA_ISPB_BASE + TEGRA_ISPB_SIZE - 1,
		.flags = IORESOURCE_MEM,
	}
};


struct nvhost_device_data t124_ispb_info = {
	.num_channels	= 1,
	/* FIXME: control clocks from user space instead of hard-coding here */
	.moduleid        = (1 << 16) | NVHOST_MODULE_ISP,
	.devfs_name      = "isp.1",
	.class           = NV_VIDEO_STREAMING_ISPB_CLASS_ID,
	.modulemutexes   = {NVMODMUTEX_ISP_1},
	.exclusive       = true,
	.keepalive       = true,
	.can_powergate   = true,
	.clockgate_delay = ISP_CLOCKGATE_DELAY,
	.powergate_delay = ISP_POWERGATE_DELAY,
	.clocks          = {
		{"isp", UINT_MAX, 0, TEGRA_MC_CLIENT_ISPB},
		{"emc", 0, NVHOST_MODULE_ID_EXTERNAL_MEMORY_CONTROLLER},
		{"sclk", 80000000} },
	.finalize_poweron = nvhost_isp_t124_finalize_poweron,
	.prepare_poweroff = nvhost_isp_t124_prepare_poweroff,
	.hw_init          = nvhost_isp_register_isr_v1,
	.ctrl_ops         = &tegra_isp_ctrl_ops,
};

static struct platform_device tegra_isp01b_device = {
	.name          = "isp",
	.id            = 1, /* .1 on the dev node */
	.resource      = ispb_resources,
	.num_resources = ARRAY_SIZE(ispb_resources),
	.dev  = {
		.platform_data = &t124_ispb_info,
	},
};

#endif

#if defined(CONFIG_VIDEO_TEGRA_VI) || defined(CONFIG_VIDEO_TEGRA_VI_MODULE)

static struct resource vi_resources[] = {
	{
		.name = "regs",
		.start = TEGRA_VI_BASE,
		.end = TEGRA_VI_BASE + TEGRA_VI_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
};

struct nvhost_device_data t124_vi_info = {
	.num_channels	= 2,
	/* FIXME: resolve powergating dependency with DIS */
	/* FIXME: control clocks from user space instead of hard-coding here */
	.moduleid         = NVHOST_MODULE_VI,
	.class            = NV_VIDEO_STREAMING_VI_CLASS_ID,
	.modulemutexes    = {NVMODMUTEX_VI_0},
	.devfs_name       = "vi",
	.exclusive        = true,
	.keepalive       = true,
	.can_powergate    = true,
	.clockgate_delay  = VI_CLOCKGATE_DELAY,
	.powergate_delay  = VI_POWERGATE_DELAY,
	.clocks           = {
		{"vi_bypass", UINT_MAX, 0},
		{"csi", 0},
		{"cilab", 102000000},
		{"cilcd", 102000000},
		{"cile", 102000000},
		{"emc", 0, NVHOST_MODULE_ID_EXTERNAL_MEMORY_CONTROLLER},
		{"sclk", 80000000} },
	.prepare_poweroff = nvhost_vi_prepare_poweroff,
	.finalize_poweron = nvhost_vi_finalize_poweron,
	.ctrl_ops         = &tegra_vi_ctrl_ops,
	.reset            = nvhost_vi_reset_all,
};
EXPORT_SYMBOL(t124_vi_info);

static struct platform_device tegra_vi01_device = {
	.name		= "vi",
	.id		= -1,
	.resource	= vi_resources,
	.num_resources	= ARRAY_SIZE(vi_resources),
	.dev		= {
		.platform_data = &t124_vi_info,
	},
};

#endif

#if defined(CONFIG_TEGRA_GRHOST_NVENC)
static struct resource msenc_resources[] = {
	{
		.name = "regs",
		.start = TEGRA_MSENC_BASE,
		.end = TEGRA_MSENC_BASE + TEGRA_MSENC_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
};

struct nvhost_device_data t124_msenc_info = {
	.num_channels	= 1,
	.version	= NVHOST_ENCODE_FLCN_VER(3, 1),
	.modulemutexes	= {NVMODMUTEX_MSENC},
	.devfs_name	= "msenc",
	.class		= NV_VIDEO_ENCODE_MSENC_CLASS_ID,
	.clocks		= {{"msenc", UINT_MAX, 0, TEGRA_MC_CLIENT_MSENC},
			  {"emc", HOST_EMC_FLOOR,
				NVHOST_MODULE_ID_EXTERNAL_MEMORY_CONTROLLER} },
	NVHOST_DEFAULT_CLOCKGATE_DELAY,
	.moduleid	= NVHOST_MODULE_MSENC,
	.powergate_delay = 100,
	.can_powergate	= true,
	.poweron_reset	= true,
	.finalize_poweron = nvhost_flcn_finalize_poweron,
	.scaling_init	= nvhost_scale_init,
	.scaling_deinit	= nvhost_scale_deinit,
	.actmon_regs	= HOST1X_CHANNEL_ACTMON1_REG_BASE,
	.actmon_enabled	= true,
	.firmware_name	= "nvhost_msenc031.fw",
	.resource_policy = RESOURCE_PER_CHANNEL_INSTANCE,
	.serialize	= true,
};

static struct platform_device tegra_msenc03_device = {
	.name	       = "msenc",
	.id	       = -1,
	.resource      = msenc_resources,
	.num_resources = ARRAY_SIZE(msenc_resources),
	.dev           = {
		.platform_data = &t124_msenc_info,
	},
};
#endif

#if defined(CONFIG_TEGRA_GRHOST_TSEC)
static struct resource tsec_resources[] = {
	{
		.name = "regs",
		.start = TEGRA_TSEC_BASE,
		.end = TEGRA_TSEC_BASE + TEGRA_TSEC_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
};

struct nvhost_device_data t124_tsec_info = {
	.num_channels	= 1,
	.version       = NVHOST_ENCODE_TSEC_VER(1, 0),
	.class         = NV_TSEC_CLASS_ID,
	.modulemutexes = {NVMODMUTEX_TSECA},
	.devfs_name    = "tsec",
	.exclusive     = false,
	.clocks	       = {{"tsec", UINT_MAX, 0, TEGRA_MC_CLIENT_TSEC},
			 {"emc", HOST_EMC_FLOOR} },
	NVHOST_MODULE_NO_POWERGATE_ID,
	NVHOST_DEFAULT_CLOCKGATE_DELAY,
	.can_powergate   = true,
	.poweron_reset   = true,
	.powergate_delay = TSEC_POWERGATE_DELAY,
	.keepalive       = true,
	.moduleid      = NVHOST_MODULE_TSEC,
	.finalize_poweron = nvhost_tsec_finalize_poweron,
	.prepare_poweroff = nvhost_tsec_prepare_poweroff,
	.resource_policy  = RESOURCE_PER_CHANNEL_INSTANCE,
	.serialize        = true,
};

static struct platform_device tegra_tsec01_device = {
	.name		= "tsec",
	.id		= -1,
	.resource	= tsec_resources,
	.num_resources	= ARRAY_SIZE(tsec_resources),
	.dev		= {
		.platform_data = &t124_tsec_info,
	},
};
#endif

#ifdef CONFIG_ARCH_TEGRA_VIC
static struct resource vic03_resources[] = {
	{
	.name = "base",
	.start = TEGRA_VIC_BASE,
	.end = TEGRA_VIC_BASE + TEGRA_VIC_SIZE - 1,
	.flags = IORESOURCE_MEM,
	},
};

struct nvhost_device_data t124_vic_info = {
	.num_channels	= 1,
	.modulemutexes	= {NVMODMUTEX_VIC},
	.devfs_name	= "vic",
	.clocks		= {{"vic03", UINT_MAX, 0, TEGRA_MC_CLIENT_VIC},
			{"emc", UINT_MAX,
				NVHOST_MODULE_ID_EXTERNAL_MEMORY_CONTROLLER},
			{"vic_floor", 0,
				NVHOST_MODULE_ID_CBUS_FLOOR},
			{"emc_shared", 0,
				NVHOST_MODULE_ID_EMC_SHARED} },
	.version = NVHOST_ENCODE_FLCN_VER(3, 0),
	NVHOST_DEFAULT_CLOCKGATE_DELAY,
	.moduleid      = NVHOST_MODULE_VIC,
	.class                  = NV_GRAPHICS_VIC_CLASS_ID,
	.can_powergate		= true,
	.engine_can_cg		= true,
	.engine_cg_regs		= t12x_vic_gating_registers,
	.poweron_reset		= true,
	.powergate_delay	= 500,
	.finalize_poweron	= nvhost_vic_finalize_poweron,
	.scaling_init		= nvhost_scale_emc_init,
	.scaling_deinit		= nvhost_scale_emc_deinit,
	.busy			= nvhost_scale_notify_busy,
	.idle			= nvhost_scale_notify_idle,
	.scaling_post_cb	= &nvhost_scale_emc_callback,
	.devfreq_governor	= "nvhost_podgov",
	.actmon_regs		= HOST1X_CHANNEL_ACTMON2_REG_BASE,
	.actmon_enabled		= true,
	.linear_emc		= true,
	.push_work_done		= true,
	.firmware_name		= "vic03_ucode.bin",
	.aggregate_constraints	= nvhost_vic_aggregate_constraints,
	.num_ppc		= 2,
	.resource_policy	= RESOURCE_PER_CHANNEL_INSTANCE,
	.serialize		= true,
};

static struct platform_device tegra_vic03_device = {
	.name	       = "vic03",
	.num_resources = 1,
	.resource      = vic03_resources,
	.dev           = {
		.platform_data = &t124_vic_info,
	},
};
#endif

/*
 * T132 overrides for platform data.
 */

#if defined(CONFIG_TEGRA_GRHOST_NVENC)
static struct nvhost_device_data t132_msenc_info = {
	.num_channels	= 1,
	.devfs_name     = "msenc",
	.version	= NVHOST_ENCODE_FLCN_VER(3, 1),
	.modulemutexes	= {NVMODMUTEX_MSENC},
	.class		= NV_VIDEO_ENCODE_MSENC_CLASS_ID,
	.clocks		= {{"msenc", UINT_MAX, 0, TEGRA_MC_CLIENT_MSENC},
			  {"emc", HOST_EMC_FLOOR,
				NVHOST_MODULE_ID_EXTERNAL_MEMORY_CONTROLLER} },
	NVHOST_DEFAULT_CLOCKGATE_DELAY,
	.moduleid	= NVHOST_MODULE_MSENC,
	.powergate_delay = 100,
	.can_powergate	= true,
	.poweron_reset	= true,
	.finalize_poweron = nvhost_flcn_finalize_poweron,
	.firmware_name	= "nvhost_msenc031.fw",
	.resource_policy = RESOURCE_PER_CHANNEL_INSTANCE,
	.serialize	= true,
};
#endif

static struct {
	struct nvhost_device_data *from;
	struct nvhost_device_data *to;
} t132_override[] = {
#if defined(CONFIG_TEGRA_GRHOST_NVENC)
	{&t124_msenc_info, &t132_msenc_info},
#endif
};

static struct platform_device *t124_devices[] = {
#ifdef CONFIG_TEGRA_GRHOST_ISP
	&tegra_isp01_device,
	&tegra_isp01b_device,
#endif
#if defined(CONFIG_VIDEO_TEGRA_VI) || defined(CONFIG_VIDEO_TEGRA_VI_MODULE)
	&tegra_vi01_device,
#endif
#if defined(CONFIG_TEGRA_GRHOST_NVENC)
	&tegra_msenc03_device,
#endif
#if defined(CONFIG_TEGRA_GRHOST_TSEC)
	&tegra_tsec01_device,
#endif
#ifdef CONFIG_ARCH_TEGRA_VIC
	&tegra_vic03_device,
#endif
};


struct platform_device *tegra12_register_host1x_devices(void)
{
	int i = 0;
	struct platform_device *pdev;

	nvhost_dbg_fn("");

	/* register host1x device first */
	platform_device_register(&tegra_host1x04_device);
	tegra_host1x04_device.dev.parent = NULL;

	/* register clients with host1x device as parent */
	for (i = 0; i < ARRAY_SIZE(t124_devices); i++) {
		pdev = t124_devices[i];
		pdev->dev.parent = &tegra_host1x04_device.dev;
		platform_device_register(pdev);
	}

	return &tegra_host1x04_device;
}

#include "host1x/host1x_channel.c"

static void t124_set_nvhost_chanops(struct nvhost_channel *ch)
{
	if (ch)
		ch->ops = host1x_channel_ops;
}

int nvhost_init_t124_channel_support(struct nvhost_master *host,
       struct nvhost_chip_support *op)
{
	op->nvhost_dev.set_nvhost_chanops = t124_set_nvhost_chanops;

	return 0;
}

static void t124_remove_support(struct nvhost_chip_support *op)
{
	kfree(op->priv);
	op->priv = NULL;
}

#include "host1x/host1x_cdma.c"
#include "host1x/host1x_debug.c"
#include "host1x/host1x_syncpt.c"
#include "host1x/host1x_intr.c"
#include "host1x/host1x_actmon_t124.c"

int nvhost_init_t124_support(struct nvhost_master *host,
       struct nvhost_chip_support *op)
{
	int i = 0;
	int err;
	struct t124 *t124 = NULL;
	struct nvhost_device_data *data = platform_get_drvdata(host->dev);

	/* Select the soc name */
#if defined(CONFIG_ARCH_TEGRA_210_SOC)
	if (tegra_get_chip_id() == TEGRA124)
#else
	if (tegra_get_chipid() == TEGRA_CHIPID_TEGRA12)
#endif
		op->soc_name = "tegra12x";
	else
		op->soc_name = "tegra13x";

	/* don't worry about cleaning up on failure... "remove" does it. */
	err = nvhost_init_t124_channel_support(host, op);
	if (err)
		return err;

	op->cdma = host1x_cdma_ops;
	op->push_buffer = host1x_pushbuffer_ops;
	op->debug = host1x_debug_ops;
	host->sync_aperture = host->aperture + HOST1X_CHANNEL_SYNC_REG_BASE;
	op->syncpt = host1x_syncpt_ops;
	op->intr = host1x_intr_ops;
	op->actmon = host1x_actmon_ops;


	if (nvhost_dev_is_virtual(host->dev)) {
		data->can_powergate = false;
		vhost_init_host1x_syncpt_ops(&op->syncpt);
		vhost_init_host1x_intr_ops(&op->intr);
		vhost_init_host1x_cdma_ops(&op->cdma);
		vhost_init_host1x_debug_ops(&op->debug);
	}

	t124 = kzalloc(sizeof(struct t124), GFP_KERNEL);
	if (!t124) {
		err = -ENOMEM;
		goto err;
	}

	t124->host = host;
	op->priv = t124;
	op->remove_support = t124_remove_support;

#if defined(CONFIG_ARCH_TEGRA_210_SOC)
	if (tegra_get_chip_id() == TEGRA132) {
#else
	if (tegra_get_chipid() == TEGRA_CHIPID_TEGRA13) {
#endif
		for (i = 0; i < ARRAY_SIZE(t132_override); i++) {
			struct nvhost_device_data *from = t132_override[i].from;
			struct nvhost_device_data *to = t132_override[i].to;

			/* replace the platform data by t132 data */
			*from = *to;
		}
	}

	return 0;

err:
	kfree(t124);

	op->priv = NULL;
	op->remove_support = NULL;
	return err;
}
