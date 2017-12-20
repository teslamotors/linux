/*
 * arch/arm/mach-tegra/board-p1852-panel.c
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

#include <linux/resource.h>
#include <asm/mach-types.h>
#include <linux/platform_device.h>
#include <linux/nvhost.h>
#include <mach/nvmap.h>
#include <mach/irqs.h>
#include <mach/iomap.h>
#include <mach/dc.h>
#include <mach/fb.h>
#include <mach/gpio-names.h>

#include "board.h"
#include "devices.h"
#include "board-p1852.h"

#define P1852_LVDS_ENA1 TEGRA_GPIO_PV0
#define P1852_LVDS_ENA2 TEGRA_GPIO_PV1
#define P1852_HDMI_HPD  TEGRA_GPIO_PN7
#define P1852_HDMI_RGB  TEGRA_GPIO_PW1
#define P1852_LVDS_SER1_ADDR 0xd
#define P1852_LVDS_SER2_ADDR 0xc


/* RGB panel requires no special enable/disable */
static int p1852_panel_enable(void)
{
	return 0;
}

static int p1852_panel_disable(void)
{
	return 0;
}

/* Enable secondary HDMI */
static int p1852_hdmi_enable(void)
{
	return 0;
}

/* Disable secondary HDMI */
static int p1852_hdmi_disable(void)
{
	return 0;
}

/* Mode data for primary RGB/LVDS out */
static struct tegra_dc_mode p1852_panel_modes[] = {
	{
#if 1 /* Tesla MCU */
                /* 1920x1200@50 */
		.pclk = 128900000,
		.h_ref_to_sync = 1,
		.v_ref_to_sync = 1,
		.h_sync_width = 120,
		.v_sync_width = 15,
		.h_back_porch = 20,
		.v_back_porch = 10,
		.h_front_porch = 20,
		.v_front_porch = 10,
		.h_active = 1920,
		.v_active = 1200,
#else
		/* 800x480@60 */
		.pclk = 32460000,
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
#endif
	},
};

static struct tegra_fb_data p1852_fb_data = {
	.win		= 0,
#if 1 /* Tesla MCU */
	.xres		= 1920,
	.yres		= 1200,
#else
	.xres		= 800,
	.yres		= 480,
#endif
	.bits_per_pixel	= 32,
};

static struct tegra_fb_data p1852_hdmi_fb_data = {
	.win		= 0,
	.xres		= 800,
	.yres		= 480,
	.bits_per_pixel	= 32,
	.flags		= TEGRA_FB_FLIP_ON_PROBE,
};

/* Start of DC_OUT data
 *  disp1 = Primary RGB out
 *  ser1  = Primary LVDS out
 *  ser2  = Secondary LVDS out
 *  hdmi  = Secondary HDMI out
 */

static struct tegra_dc_out p1852_disp1_out = {
	.align		= TEGRA_DC_ALIGN_MSB,
	.order		= TEGRA_DC_ORDER_RED_BLUE,
	.parent_clk	= "pll_d_out0",
	.type		= TEGRA_DC_OUT_RGB,
	.modes		= p1852_panel_modes,
	.n_modes	= ARRAY_SIZE(p1852_panel_modes),
	.enable		= p1852_panel_enable,
	.disable	= p1852_panel_disable,
};

static struct tegra_dc_out p1852_hdmi_out = {
	.align		= TEGRA_DC_ALIGN_MSB,
	.order		= TEGRA_DC_ORDER_RED_BLUE,
	.parent_clk	= "pll_d2_out0",
	.type		= TEGRA_DC_OUT_HDMI,
	.flags		= TEGRA_DC_OUT_HOTPLUG_LOW |
			  TEGRA_DC_OUT_NVHDCP_POLICY_ON_DEMAND,
	.max_pixclock	= KHZ2PICOS(148500),
	.hotplug_gpio	= P1852_HDMI_HPD,
	.enable		= p1852_hdmi_enable,
	.disable	= p1852_hdmi_disable,

	.dcc_bus	= 1,
};

/* End of DC_OUT data */

/* Start of platform data */

static struct tegra_dc_platform_data p1852_disp1_pdata = {
	.flags		= TEGRA_DC_FLAG_ENABLED,
	.default_out	= &p1852_disp1_out,
	.emc_clk_rate	= 300000000,
	.fb		= &p1852_fb_data,
};

static struct tegra_dc_platform_data p1852_hdmi_pdata = {
	.flags		= 0,
	.default_out	= &p1852_hdmi_out,
	.emc_clk_rate	= 300000000,
	.fb		= &p1852_hdmi_fb_data,
};

/* End of platform data */

static struct nvmap_platform_carveout p1852_carveouts[] = {
	[0] = {
		.name		= "iram",
		.usage_mask	= NVMAP_HEAP_CARVEOUT_IRAM,
		.base		= TEGRA_IRAM_BASE + TEGRA_RESET_HANDLER_SIZE,
		.size		= TEGRA_IRAM_SIZE - TEGRA_RESET_HANDLER_SIZE,
		.buddy_size	= 0, /* no buddy allocation for IRAM */
	},
	[1] = {
		.name		= "generic-0",
		.usage_mask	= NVMAP_HEAP_CARVEOUT_GENERIC,
		.base		= 0,	/* Filled in by p1852_panel_init() */
		.size		= 0,	/* Filled in by p1852_panel_init() */
		.buddy_size	= SZ_32K,
	},
};

static struct nvmap_platform_data p1852_nvmap_data = {
	.carveouts	= p1852_carveouts,
	.nr_carveouts	= ARRAY_SIZE(p1852_carveouts),
};

#if defined(CONFIG_TEGRA_NVAVP)
#define INT_SHR_SEM_INBOX_IBF (-1)
struct resource tegra_nvavp_resources[] = {
	[0] = {
		.start	= INT_SHR_SEM_INBOX_IBF,
		.end	= INT_SHR_SEM_INBOX_IBF,
		.flags	= IORESOURCE_IRQ,
		.name	= "mbox_from_avp_pending",
	},
};

struct nvhost_device p1852_nvavp_device = {
	.name		= "tegra-avp",
	.id		= -1,
	.resource	= tegra_nvavp_resources,
	.num_resources	= ARRAY_SIZE(tegra_nvavp_resources),
};
#endif

static struct platform_device *p1852_gfx_devices[] __initdata = {
	&tegra_nvmap_device,
	&tegra_grhost_device
};

static int __init p1852_sku8_panel_init(void)
{
	int err;

	p1852_carveouts[1].base = tegra_carveout_start;
	p1852_carveouts[1].size = tegra_carveout_size;
	tegra_nvmap_device.dev.platform_data = &p1852_nvmap_data;
	/* sku 8 has primary RGB out and secondary HDMI out */
	tegra_disp1_device.dev.platform_data = &p1852_disp1_pdata;
	p1852_disp2_device.dev.platform_data = &p1852_hdmi_pdata;

	err = platform_add_devices(p1852_gfx_devices,
				ARRAY_SIZE(p1852_gfx_devices));

	if (!err)
		err = nvhost_device_register(&tegra_disp1_device);

	if (!err)
		err = nvhost_device_register(&p1852_disp2_device);

#if defined(CONFIG_TEGRA_NVAVP)
	if (!err)
		err = nvhost_device_register(&p1852_nvavp_device);
#endif
	return err;
}

int __init p1852_panel_init(void)
{
	return p1852_sku8_panel_init();
}

