/*
 * arch/arm/mach-tegra/board-whistler-panel.c
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
#include <linux/pwm_backlight.h>
#include <mach/nvhost.h>
#include <mach/nvmap.h>
#include <mach/irqs.h>
#include <mach/iomap.h>
#include <mach/dc.h>
#include <mach/fb.h>

#include "devices.h"
#include "gpio-names.h"
#include "board.h"

#define whistler_bl_enb		TEGRA_GPIO_PW1

static int whistler_backlight_init(struct device *dev) {
	int ret;

	ret = gpio_request(whistler_bl_enb, "backlight_enb");
	if (ret < 0)
		return ret;

	ret = gpio_direction_output(whistler_bl_enb, 1);
	if (ret < 0)
		gpio_free(whistler_bl_enb);
	else
		tegra_gpio_enable(whistler_bl_enb);

	return ret;
};

static void whistler_backlight_exit(struct device *dev) {
	gpio_set_value(whistler_bl_enb, 0);
	gpio_free(whistler_bl_enb);
	tegra_gpio_disable(whistler_bl_enb);
}

static int whistler_backlight_notify(struct device *unused, int brightness)
{
	gpio_set_value(whistler_bl_enb, !!brightness);
	return brightness;
}

static struct platform_pwm_backlight_data whistler_backlight_data = {
	.pwm_id		= 2,
	.max_brightness	= 255,
	.dft_brightness	= 224,
	.pwm_period_ns	= 5000000,
	.init		= whistler_backlight_init,
	.exit		= whistler_backlight_exit,
	.notify		= whistler_backlight_notify,
};

static struct platform_device whistler_backlight_device = {
	.name	= "pwm-backlight",
	.id	= -1,
	.dev	= {
		.platform_data = &whistler_backlight_data,
	},
};

static struct resource whistler_disp1_resources[] = {
	{
		.name	= "irq",
		.start	= INT_DISPLAY_GENERAL,
		.end	= INT_DISPLAY_GENERAL,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name	= "regs",
		.start	= TEGRA_DISPLAY_BASE,
		.end	= TEGRA_DISPLAY_BASE + TEGRA_DISPLAY_SIZE-1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "fbmem",
		.flags	= IORESOURCE_MEM,
	},
};

static struct tegra_dc_mode whistler_panel_modes[] = {
	{
		.pclk = 27000000,
		.h_ref_to_sync = 4,
		.v_ref_to_sync = 2,
		.h_sync_width = 10,
		.v_sync_width = 3,
		.h_back_porch = 20,
		.v_back_porch = 3,
		.h_active = 800,
		.v_active = 480,
		.h_front_porch = 70,
		.v_front_porch = 3,
	},
};

static struct tegra_dc_out_pin whistler_dc_out_pins[] = {
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

static struct tegra_dc_out whistler_disp1_out = {
	.type		= TEGRA_DC_OUT_RGB,

	.align		= TEGRA_DC_ALIGN_MSB,
	.order		= TEGRA_DC_ORDER_RED_BLUE,

	.modes	 	= whistler_panel_modes,
	.n_modes 	= ARRAY_SIZE(whistler_panel_modes),

	.out_pins	= whistler_dc_out_pins,
	.n_out_pins	= ARRAY_SIZE(whistler_dc_out_pins),
};

static struct tegra_fb_data whistler_fb_data = {
	.win		= 0,
	.xres		= 800,
	.yres		= 480,
	.bits_per_pixel	= 32,
};

static struct tegra_dc_platform_data whistler_disp1_pdata = {
	.flags		= TEGRA_DC_FLAG_ENABLED,
	.default_out	= &whistler_disp1_out,
	.fb		= &whistler_fb_data,
};

static struct nvhost_device whistler_disp1_device = {
	.name		= "tegradc",
	.id		= 0,
	.resource	= whistler_disp1_resources,
	.num_resources	= ARRAY_SIZE(whistler_disp1_resources),
	.dev = {
		.platform_data = &whistler_disp1_pdata,
	},
};

static struct nvmap_platform_carveout whistler_carveouts[] = {
	[0] = {
		.name		= "iram",
		.usage_mask	= NVMAP_HEAP_CARVEOUT_IRAM,
		.base		= TEGRA_IRAM_BASE,
		.size		= TEGRA_IRAM_SIZE,
		.buddy_size	= 0, /* no buddy allocation for IRAM */
	},
	[1] = {
		.name		= "generic-0",
		.usage_mask	= NVMAP_HEAP_CARVEOUT_GENERIC,
		.base		= 0x18C00000,
		.size		= SZ_128M - 0xC00000,
		.buddy_size	= SZ_32K,
	},
};

static struct nvmap_platform_data whistler_nvmap_data = {
	.carveouts	= whistler_carveouts,
	.nr_carveouts	= ARRAY_SIZE(whistler_carveouts),
};

static struct platform_device whistler_nvmap_device = {
	.name	= "tegra-nvmap",
	.id	= -1,
	.dev	= {
		.platform_data = &whistler_nvmap_data,
	},
};

static struct platform_device *whistler_gfx_devices[] __initdata = {
	&whistler_nvmap_device,
	&tegra_grhost_device,
	&tegra_pwfm2_device,
	&whistler_backlight_device,
};

int __init whistler_panel_init(void)
{
	int err;
	struct resource *res;

	whistler_carveouts[1].base = tegra_carveout_start;
	whistler_carveouts[1].size = tegra_carveout_size;

	err = platform_add_devices(whistler_gfx_devices,
				   ARRAY_SIZE(whistler_gfx_devices));

	res = nvhost_get_resource_byname(&whistler_disp1_device,
					 IORESOURCE_MEM, "fbmem");
	res->start = tegra_fb_start;
	res->end = tegra_fb_start + tegra_fb_size - 1;

	if (!err)
		err = nvhost_device_register(&whistler_disp1_device);

	return err;
}

