/*
 * arch/arm/mach-tegra/board-ventana-panel.c
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
#include <linux/earlysuspend.h>
#include <linux/pwm_backlight.h>
#include <mach/nvhost.h>
#include <mach/nvmap.h>
#include <mach/irqs.h>
#include <mach/iomap.h>
#include <mach/dc.h>
#include <mach/fb.h>

#include "devices.h"
#include "gpio-names.h"
#define ventana_pnl_pwr_enb	TEGRA_GPIO_PC6
#define ventana_bl_enb		TEGRA_GPIO_PD4
#define ventana_lvds_shutdown	TEGRA_GPIO_PB2
#define ventana_hdmi_hpd	TEGRA_GPIO_PN7
#define ventana_hdmi_enb	TEGRA_GPIO_PV5

static struct regulator *ventana_hdmi_reg = NULL;
static struct regulator *ventana_hdmi_pll = NULL;


static int ventana_backlight_init(struct device *dev) {
	int ret;

	ret = gpio_request(ventana_bl_enb, "backlight_enb");
	if (ret < 0)
		return ret;

	ret = gpio_direction_output(ventana_bl_enb, 1);
	if (ret < 0)
		gpio_free(ventana_bl_enb);
	else
		tegra_gpio_enable(ventana_bl_enb);

	return ret;
};

static void ventana_backlight_exit(struct device *dev) {
	gpio_set_value(ventana_bl_enb, 0);
	gpio_free(ventana_bl_enb);
	tegra_gpio_disable(ventana_bl_enb);
}

static int ventana_backlight_notify(struct device *unused, int brightness)
{
	gpio_set_value(ventana_bl_enb, !!brightness);
	return brightness;
}

static struct platform_pwm_backlight_data ventana_backlight_data = {
	.pwm_id		= 2,
	.max_brightness	= 255,
	.dft_brightness	= 224,
	.pwm_period_ns	= 5000000,
	.init		= ventana_backlight_init,
	.exit		= ventana_backlight_exit,
	.notify		= ventana_backlight_notify,
};

static struct platform_device ventana_backlight_device = {
	.name	= "pwm-backlight",
	.id	= -1,
	.dev	= {
		.platform_data = &ventana_backlight_data,
	},
};

static int ventana_panel_enable(void)
{
	gpio_set_value(ventana_pnl_pwr_enb, 1);
	gpio_set_value(ventana_lvds_shutdown, 1);
	return 0;
}

static int ventana_panel_disable(void)
{
	gpio_set_value(ventana_lvds_shutdown, 0);
	gpio_set_value(ventana_pnl_pwr_enb, 0);
	return 0;
}

static int ventana_hdmi_enable(void)
{
	gpio_set_value(ventana_hdmi_enb, 1);
	if (!ventana_hdmi_reg) {
		ventana_hdmi_reg = regulator_get(NULL, "avdd_hdmi"); /* LD07 */
		if (IS_ERR_OR_NULL(ventana_hdmi_reg)) {
			pr_err("hdmi: couldn't get regulator avdd_hdmi\n");
			ventana_hdmi_reg = NULL;
			return PTR_ERR(ventana_hdmi_reg);
		}
	}
	regulator_enable(ventana_hdmi_reg);

	if (!ventana_hdmi_pll) {
		ventana_hdmi_pll = regulator_get(NULL, "avdd_hdmi_pll"); /* LD08 */
		if (IS_ERR_OR_NULL(ventana_hdmi_pll)) {
			pr_err("hdmi: couldn't get regulator avdd_hdmi_pll\n");
			ventana_hdmi_pll = NULL;
			regulator_disable(ventana_hdmi_reg);
			ventana_hdmi_reg = NULL;
			return PTR_ERR(ventana_hdmi_pll);
		}
	}
	regulator_enable(ventana_hdmi_pll);
	return 0;
}

static int ventana_hdmi_disable(void)
{
	gpio_set_value(ventana_hdmi_enb, 0);
	regulator_disable(ventana_hdmi_reg);
	regulator_disable(ventana_hdmi_pll);
	return 0;
}

static struct resource ventana_disp1_resources[] = {
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
		.start	= 0x18012000,
		.end	= 0x18414000 - 1, /* enough for 1080P 16bpp */
		.flags	= IORESOURCE_MEM,
	},
};

static struct resource ventana_disp2_resources[] = {
	{
		.name	= "irq",
		.start	= INT_DISPLAY_B_GENERAL,
		.end	= INT_DISPLAY_B_GENERAL,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name	= "regs",
		.start	= TEGRA_DISPLAY2_BASE,
		.end	= TEGRA_DISPLAY2_BASE + TEGRA_DISPLAY2_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "fbmem",
		.flags	= IORESOURCE_MEM,
		.start	= 0x18414000,
		.end	= 0x18BFD000 - 1,
	},
	{
		.name	= "hdmi_regs",
		.start	= TEGRA_HDMI_BASE,
		.end	= TEGRA_HDMI_BASE + TEGRA_HDMI_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct tegra_dc_mode ventana_panel_modes[] = {
	{
		.pclk = 62200000,
		.h_ref_to_sync = 11,
		.v_ref_to_sync = 1,
		.h_sync_width = 58,
		.v_sync_width = 4,
		.h_back_porch = 58,
		.v_back_porch = 4,
		.h_active = 1366,
		.v_active = 768,
		.h_front_porch = 58,
		.v_front_porch = 4,
	},
};

static struct tegra_fb_data ventana_fb_data = {
	.win		= 0,
	.xres		= 1366,
	.yres		= 768,
	.bits_per_pixel	= 16,
};

static struct tegra_fb_data ventana_hdmi_fb_data = {
	.win		= 0,
	.xres		= 1366,
	.yres		= 768,
	.bits_per_pixel	= 16,
};

static struct tegra_dc_out ventana_disp1_out = {
	.type		= TEGRA_DC_OUT_RGB,

	.align		= TEGRA_DC_ALIGN_MSB,
	.order		= TEGRA_DC_ORDER_RED_BLUE,

	.modes	 	= ventana_panel_modes,
	.n_modes 	= ARRAY_SIZE(ventana_panel_modes),

	.enable		= ventana_panel_enable,
	.disable	= ventana_panel_disable,
};

static struct tegra_dc_out ventana_disp2_out = {
	.type		= TEGRA_DC_OUT_HDMI,
	.flags		= TEGRA_DC_OUT_HOTPLUG_HIGH,

	.dcc_bus	= 1,
	.hotplug_gpio	= ventana_hdmi_hpd,

	.align		= TEGRA_DC_ALIGN_MSB,
	.order		= TEGRA_DC_ORDER_RED_BLUE,

	.enable		= ventana_hdmi_enable,
	.disable	= ventana_hdmi_disable,
};

static struct tegra_dc_platform_data ventana_disp1_pdata = {
	.flags		= TEGRA_DC_FLAG_ENABLED,
	.default_out	= &ventana_disp1_out,
	.fb		= &ventana_fb_data,
};

static struct tegra_dc_platform_data ventana_disp2_pdata = {
	.flags		= 0,
	.default_out	= &ventana_disp2_out,
	.fb		= &ventana_hdmi_fb_data,
};

static struct nvhost_device ventana_disp1_device = {
	.name		= "tegradc",
	.id		= 0,
	.resource	= ventana_disp1_resources,
	.num_resources	= ARRAY_SIZE(ventana_disp1_resources),
	.dev = {
		.platform_data = &ventana_disp1_pdata,
	},
};

static struct nvhost_device ventana_disp2_device = {
	.name		= "tegradc",
	.id		= 1,
	.resource	= ventana_disp2_resources,
	.num_resources	= ARRAY_SIZE(ventana_disp2_resources),
	.dev = {
		.platform_data = &ventana_disp2_pdata,
	},
};

static struct nvmap_platform_carveout ventana_carveouts[] = {
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

static struct nvmap_platform_data ventana_nvmap_data = {
	.carveouts	= ventana_carveouts,
	.nr_carveouts	= ARRAY_SIZE(ventana_carveouts),
};

static struct platform_device ventana_nvmap_device = {
	.name	= "tegra-nvmap",
	.id	= -1,
	.dev	= {
		.platform_data = &ventana_nvmap_data,
	},
};

static struct platform_device *ventana_gfx_devices[] __initdata = {
	&ventana_nvmap_device,
	&tegra_grhost_device,
	&tegra_pwfm2_device,
	&ventana_backlight_device,
};

#ifdef CONFIG_HAS_EARLYSUSPEND
/* put early_suspend/late_resume handlers here for the display in order
 * to keep the code out of the display driver, keeping it closer to upstream
 */
struct early_suspend ventana_panel_early_suspender;

static void ventana_panel_early_suspend(struct early_suspend *h)
{
	if (num_registered_fb > 0)
		fb_blank(registered_fb[0], FB_BLANK_POWERDOWN);
}

static void ventana_panel_late_resume(struct early_suspend *h)
{
	if (num_registered_fb > 0)
		fb_blank(registered_fb[0], FB_BLANK_UNBLANK);
}
#endif

int __init ventana_panel_init(void)
{
	int err;
	gpio_request(ventana_pnl_pwr_enb, "pnl_pwr_enb");
	gpio_direction_output(ventana_pnl_pwr_enb, 1);
	tegra_gpio_enable(ventana_pnl_pwr_enb);

	gpio_request(ventana_lvds_shutdown, "lvds_shdn");
	gpio_direction_output(ventana_lvds_shutdown, 1);
	tegra_gpio_enable(ventana_lvds_shutdown);

	tegra_gpio_enable(ventana_hdmi_enb);
	gpio_request(ventana_hdmi_enb, "hdmi_5v_en");
	gpio_direction_output(ventana_hdmi_enb, 1);

	tegra_gpio_enable(ventana_hdmi_hpd);
	gpio_request(ventana_hdmi_hpd, "hdmi_hpd");
	gpio_direction_input(ventana_hdmi_hpd);

#ifdef CONFIG_HAS_EARLYSUSPEND
	ventana_panel_early_suspender.suspend = ventana_panel_early_suspend;
	ventana_panel_early_suspender.resume = ventana_panel_late_resume;
	ventana_panel_early_suspender.level = EARLY_SUSPEND_LEVEL_DISABLE_FB;
	register_early_suspend(&ventana_panel_early_suspender);
#endif

	err = platform_add_devices(ventana_gfx_devices,
				   ARRAY_SIZE(ventana_gfx_devices));

	if (!err)
		err = nvhost_device_register(&ventana_disp1_device);

	if (!err)
		err = nvhost_device_register(&ventana_disp2_device);

	return err;
}

