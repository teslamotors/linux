/*
 * arch/arm/mach-tegra/board-harmony-panel.c
 *
 * Copyright (C) 2010 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/resource.h>
#include <linux/platform_device.h>
#include <linux/pwm_backlight.h>
#include <linux/mm.h>
#include <mach/nvhost.h>
#include <mach/nvmap.h>
#include <mach/irqs.h>
#include <mach/iomap.h>
#include <mach/dc.h>
#include <mach/fb.h>
#include <mach/gpio.h>
#include <asm/mach-types.h>

#include "devices.h"
#include "gpio-names.h"
#include "board-harmony.h"

#define harmony_bl_enb          TEGRA_GPIO_PB5
#define harmony_lvds_shutdown   TEGRA_GPIO_PB2
#define harmony_bl_pwm          TEGRA_GPIO_PB4
#define harmony_bl_vdd          TEGRA_GPIO_PW0
#define harmony_en_vdd_pnl    TEGRA_GPIO_PC6
#define harmony_hdmi_hpd        TEGRA_GPIO_PN7

static struct platform_pwm_backlight_data harmony_backlight_data = {
	.pwm_id = 0,
	.max_brightness = 255,
	.dft_brightness = 224,
	.pwm_period_ns = 5000000,
	.init = NULL,
	.exit = NULL,
	.notify = NULL,
};

static struct platform_device harmony_backlight_device = {
	.name = "pwm-backlight",
	.id = -1,
	.dev = {
		.platform_data = &harmony_backlight_data,
	},
};

static int harmony_panel_enable(void)
{
	static struct regulator *reg = NULL;
	static struct regulator *reg_crt = NULL;

	printk(KERN_ERR "%s:%d\n", __func__, __LINE__);
	if (reg == NULL) {
		reg = regulator_get(NULL, "avdd_lvds");
		if (WARN_ON(IS_ERR(reg)))
			pr_err("%s: couldn't get regulator avdd_lvds: %ld\n",
			       __func__, PTR_ERR(reg));
		else
			regulator_enable(reg);
	}

	if (reg_crt == NULL) {
		/* enable CRT rail */
		reg_crt = regulator_get(NULL, "avdd_vdac");
		if (IS_ERR(reg_crt))
			pr_err("%s: couldn't get regulator avdd_vdac: %ld\n",
					__func__, PTR_ERR(reg_crt));
		else
			regulator_enable(reg_crt);
	}

	gpio_set_value(harmony_lvds_shutdown, 1);
	return 0;
}

static int harmony_panel_disable(void)
{
	gpio_set_value(harmony_lvds_shutdown, 0);
	return 0;
}

/* Display Controller */
#define FBMEM_START_128M   0x18012000
#define FBMEM_SIZE_128M    0x258000
#define FBMEM_START_80M    0x1B012000
#define FBMEM_SIZE_80M     0x258000

static struct resource harmony_disp1_resources[] = {
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
	 .start = FBMEM_START_128M,
	 .end = FBMEM_START_128M + FBMEM_SIZE_128M - 1,
	 .flags = IORESOURCE_MEM,
	 },
};

static struct tegra_dc_mode harmony_panel_modes[] = {
	{
	 .pclk = 42430000,
	 .h_ref_to_sync = 4,
	 .v_ref_to_sync = 2,
	 .h_sync_width = 136,
	 .v_sync_width = 4,
	 .h_back_porch = 138,
	 .v_back_porch = 21,
	 .h_active = 1024,
	 .v_active = 600,
	 .h_front_porch = 34,
	 .v_front_porch = 4,
	 },
};

static struct tegra_fb_data harmony_fb_data = {
	.win = 0,
	.xres = 1024,
	.yres = 600,
	.bits_per_pixel = 16,
};

static struct tegra_dc_out harmony_disp1_out = {
	.type = TEGRA_DC_OUT_RGB,

	.align = TEGRA_DC_ALIGN_MSB,
	.order = TEGRA_DC_ORDER_RED_BLUE,

	.modes = harmony_panel_modes,
	.n_modes = ARRAY_SIZE(harmony_panel_modes),

	.enable = harmony_panel_enable,
	.disable = harmony_panel_disable,
};

static struct tegra_dc_platform_data harmony_disp1_pdata = {
	.flags = TEGRA_DC_FLAG_ENABLED,
	.default_out = &harmony_disp1_out,
	.fb = &harmony_fb_data,
};

static struct nvhost_device harmony_disp1_device = {
	.name = "tegradc",
	.id = 0,
	.resource = harmony_disp1_resources,
	.num_resources = ARRAY_SIZE(harmony_disp1_resources),
	.dev = {
		.platform_data = &harmony_disp1_pdata,
	},
};

static struct nvmap_platform_carveout harmony_carveouts[] = {
	[0] = {
	       .name = "iram",
	       .usage_mask = NVMAP_HEAP_CARVEOUT_IRAM,
	       .base = TEGRA_IRAM_BASE,
	       .size = TEGRA_IRAM_SIZE,
	       .buddy_size = 0,	/* no buddy allocation for IRAM */
	       },
	[1] = {
	       .name = "generic-0",
	       .usage_mask = NVMAP_HEAP_CARVEOUT_GENERIC,
	       .base = 0x18C00000,
	       .size = SZ_128M - 0xC00000,
	       .buddy_size = SZ_32K,
	       },
};

static struct nvmap_platform_data harmony_nvmap_data = {
	.carveouts = harmony_carveouts,
	.nr_carveouts = ARRAY_SIZE(harmony_carveouts),
};

static struct platform_device harmony_nvmap_device = {
	.name = "tegra-nvmap",
	.id = -1,
	.dev = {
		.platform_data = &harmony_nvmap_data,
		},
};

static struct platform_device *harmony_gfx_devices[] __initdata = {
	&harmony_nvmap_device,
	&tegra_grhost_device,
	&tegra_pwfm0_device,
	&harmony_backlight_device,
};

int __init harmony_panel_init(void)
{
	int err;
	struct sysinfo minfo;

	printk("harmony_panel_init \n");

	tegra_gpio_enable(harmony_bl_enb);
	tegra_gpio_enable(harmony_en_vdd_pnl);
	tegra_gpio_enable(harmony_bl_vdd);
	tegra_gpio_enable(harmony_lvds_shutdown);

	/* enable hdmi hotplug gpio for hotplug detection */
	tegra_gpio_enable(harmony_hdmi_hpd);

	si_meminfo(&minfo);
	if ((minfo.totalram << (PAGE_SHIFT - 10)) <= SZ_512K) {
		harmony_carveouts[1].base = 0x1BC00000;
		harmony_carveouts[1].size = SZ_64M + SZ_16M - 0xC00000;
		harmony_disp1_resources[2].start = FBMEM_START_80M;
		harmony_disp1_resources[2].end = FBMEM_START_80M + FBMEM_SIZE_80M - 1;
	}

	err = platform_add_devices(harmony_gfx_devices,
				   ARRAY_SIZE(harmony_gfx_devices));

	if (!err)
		err = nvhost_device_register(&harmony_disp1_device);

	return err;
}
