/*
 * arch/arm/mach-tegra/board-p852-panel.c
 *
 * Copyright (c) 2010, NVIDIA Corporation.
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

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/resource.h>
#include <asm/mach-types.h>
#include <linux/platform_device.h>
#include <linux/nvhost.h>
#include <mach/nvmap.h>
#include <mach/irqs.h>
#include <mach/iomap.h>
#include <mach/dc.h>
#include <mach/fb.h>

#include "board-p852.h"
#include "devices.h"

#define CARVEOUT_IRAM {\
	.name = "iram",\
	.usage_mask = NVMAP_HEAP_CARVEOUT_IRAM,\
	.base = TEGRA_IRAM_BASE,\
	.size = TEGRA_IRAM_SIZE,\
	.buddy_size = 0, /* no buddy allocation for IRAM */\
}

static int p852_panel_enable(void)
{
	pr_info("%s\n", __FUNCTION__);
	return 0;
}

static int p852_panel_disable(void)
{
	pr_info("%s\n", __FUNCTION__);
	return 0;
}

#if 0
/* 2.6 data */
static struct resource p852_disp_resources_128MB[] = {
	{
	 .name = "irq",
	 .start = INT_DISPLAY_GENERAL,
	 .end = INT_DISPLAY_GENERAL,
	 .flags = IORESOURCE_IRQ,
	},
	{
	 .name = "regs",
	 .start = TEGRA_DISPLAY_BASE,
	 .end = TEGRA_DISPLAY_BASE + TEGRA_DISPLAY_SIZE - 1,
	 .flags = IORESOURCE_MEM,
	},
	{
	 .name = "fbmem",
	 .start = 0x18012000,
	 .end =   0x18912000 - 1,
	 .flags = IORESOURCE_MEM,
	},
};

static struct resource p852_disp_resources_256MB[] = {
    {
        .name = "irq",
        .start = INT_DISPLAY_GENERAL,
        .end = INT_DISPLAY_GENERAL,
        .flags = IORESOURCE_IRQ,
    },
    {
        .name = "regs",
        .start = TEGRA_DISPLAY_BASE,
        .end = TEGRA_DISPLAY_BASE + TEGRA_DISPLAY_SIZE - 1,
        .flags = IORESOURCE_MEM,
    },
    {
        .name = "fbmem",
        .start = 0x10012000,
        .end =   0x10912000 - 1,
        .flags = IORESOURCE_MEM,
    },
};

static struct tegra_dc_mode p852_panel_modes[] = {
/* Timings for the LG LB070WV4 panel */
	{
	 .pclk = 33230769,

	 .h_ref_to_sync = 1,
	 .v_ref_to_sync = 1,

	 .h_sync_width = 128,
	 .v_sync_width = 2,

	 .h_back_porch = 88,
	 .v_back_porch = 30,

	 .h_front_porch = 40,
	 .v_front_porch = 13,

	 .h_active = 800,
	 .v_active = 480,
	 },
};
#endif

#if 1
/* 3.1 cid data */
static struct tegra_dc_mode p852_panel_modes[] = {
/* Timings for the LG LB070WV4 panel */
	{
	 .pclk = 33260000,

	 .h_ref_to_sync = 1,
	 .v_ref_to_sync = 1,

	 .h_sync_width = 64,
	 .v_sync_width = 3,

	 .h_back_porch = 128,
	 .v_back_porch = 22,

	 .h_front_porch = 64,
	 .v_front_porch = 20,

	 .h_active = 800,
	 .v_active = 480,
	 },
};
#endif

static struct tegra_fb_data p852_fb_data = {
	.win = 0,
	.xres = 800,
	.yres = 480,
	.bits_per_pixel = 16,
};

static struct tegra_dc_out_pin p852_dc_out_pins[] = {
/* E1188 panels support both polarities for HSYNC/VSYNC,
   but keeping the polarities that match customer panel. */
	{
		.name	= TEGRA_DC_OUT_PIN_DATA_ENABLE,
		.pol	= TEGRA_DC_OUT_PIN_POL_HIGH,
	},
	{
		.name	= TEGRA_DC_OUT_PIN_H_SYNC,
		.pol	= TEGRA_DC_OUT_PIN_POL_LOW,
	},
	{
		.name	= TEGRA_DC_OUT_PIN_V_SYNC,
		.pol	= TEGRA_DC_OUT_PIN_POL_LOW,
	},
	{
		.name	= TEGRA_DC_OUT_PIN_PIXEL_CLOCK,
		.pol	= TEGRA_DC_OUT_PIN_POL_LOW,
	},
};

static struct tegra_dc_out p852_disp_out = {
	.type = TEGRA_DC_OUT_RGB,

	.align = TEGRA_DC_ALIGN_MSB,
	.order = TEGRA_DC_ORDER_RED_BLUE,

	// not used in pdk19 .parent_clk = "pll_d_out0",

	.modes = p852_panel_modes,
	.n_modes = ARRAY_SIZE(p852_panel_modes),

	.out_pins	= p852_dc_out_pins,
	.n_out_pins	= ARRAY_SIZE(p852_dc_out_pins),

	.enable = p852_panel_enable,
	.disable = p852_panel_disable,
};

static struct tegra_dc_platform_data p852_disp_pdata = {
	.flags = TEGRA_DC_FLAG_ENABLED,
	.default_out = &p852_disp_out,
	.fb = &p852_fb_data,
};

#if 0
/* 2.6 data */
static struct nvhost_device p852_disp_device = {
	.name = "tegradc",
	.id = 0,
	.resource = p852_disp_resources_128MB,
	.num_resources = ARRAY_SIZE(p852_disp_resources_128MB),
	.dev = {
		.platform_data = &p852_disp_pdata,
		},
};
#endif

static struct nvmap_platform_carveout p852_carveouts_128MB[] = {
	[0] = CARVEOUT_IRAM,
	[1] = {
	       .name = "generic-0",
	       .usage_mask = NVMAP_HEAP_CARVEOUT_GENERIC,
	       .base = 0x18C00000,
	       .size = SZ_128M - 0xC00000,
	       .buddy_size = SZ_32K,
	       },
};

static struct nvmap_platform_carveout p852_carveouts_256MB[] = {
	[0] = CARVEOUT_IRAM,
	[1] = {
	       .name = "generic-0",
	       .usage_mask = NVMAP_HEAP_CARVEOUT_GENERIC,
	       .base = 0x10C00000,
	       .size = SZ_256M - 0xC00000,
	       .buddy_size = SZ_32K,
	       },
};

static struct nvmap_platform_data p852_nvmap_data = {
	.carveouts = p852_carveouts_128MB,
	.nr_carveouts = ARRAY_SIZE(p852_carveouts_128MB),
};

static struct platform_device p852_nvmap_device = {
	.name = "tegra-nvmap",
	.id = -1,
	.dev = {
		.platform_data = &p852_nvmap_data,
		},
};

#define INT_SHR_SEM_INBOX_IBF (-1024)
struct resource tegra_nvavp_resources[] = {
        [0] = {
                .start  = INT_SHR_SEM_INBOX_IBF,
                .end    = INT_SHR_SEM_INBOX_IBF,
                .flags  = IORESOURCE_IRQ,
                .name   = "mbox_from_avp_pending",
        },
};

struct nvhost_device tegra_nvavp_device = {
        .name           = "tegra-avp",
        .id             = -1,
        .num_resources  = ARRAY_SIZE(tegra_nvavp_resources),
        .resource       = tegra_nvavp_resources,
        .dev  = {
                .coherent_dma_mask      = 0xffffffffULL,
        },
};

static struct platform_device *p852_gfx_devices[] __initdata = {
	&tegra_grhost_device,
};

int __init p852_panel_init(void)
{
	int err;

	pr_info("%s\n", __FUNCTION__);

	p852_nvmap_data.carveouts = p852_carveouts_256MB;

	err = platform_device_register(&p852_nvmap_device);
	if (err)
		return err;

	err = platform_add_devices(p852_gfx_devices,
				   ARRAY_SIZE(p852_gfx_devices));

	tegra_disp1_device.dev.platform_data = &p852_disp_pdata;
	if (!err)
		err = nvhost_device_register(&tegra_disp1_device);

	return err;
}
