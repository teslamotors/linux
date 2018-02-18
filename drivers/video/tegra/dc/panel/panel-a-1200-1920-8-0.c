/*
 * arch/arm/mach-tegra/panel-a-1200-1920-8-0.c
 *
 * Copyright (c) 2013-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <mach/dc.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include "board.h"
#include "board-panel.h"
#include "devices.h"

#define DSI_PANEL_RESET		1

static bool reg_requested;
static struct regulator *avdd_lcd_3v3;
static struct regulator *vdd_lcd_bl_en;
static struct regulator *dvdd_lcd_1v8;
static struct device *dc_dev;
static u16 en_panel_rst;

static int dsi_a_1200_1920_8_0_regulator_get(struct device *dev)
{
	int err = 0;

	if (reg_requested)
		return 0;

	avdd_lcd_3v3 = regulator_get(dev, "avdd_lcd");
	if (IS_ERR_OR_NULL(avdd_lcd_3v3)) {
		pr_err("avdd_lcd regulator get failed\n");
		err = PTR_ERR(avdd_lcd_3v3);
		avdd_lcd_3v3 = NULL;
		goto fail;
	}

	dvdd_lcd_1v8 = regulator_get(dev, "dvdd_lcd");
	if (IS_ERR_OR_NULL(dvdd_lcd_1v8)) {
		pr_err("dvdd_lcd_1v8 regulator get failed\n");
		err = PTR_ERR(dvdd_lcd_1v8);
		dvdd_lcd_1v8 = NULL;
		goto fail;
	}

	vdd_lcd_bl_en = regulator_get(dev, "vdd_lcd_bl_en");
	if (IS_ERR_OR_NULL(vdd_lcd_bl_en)) {
		err = PTR_ERR(vdd_lcd_bl_en);
		pr_warn("warning: vdd_lcd_bl_en regulator get failed, err = %d\n", err);
		vdd_lcd_bl_en = NULL;
	}

	reg_requested = true;
	return 0;
fail:
	return err;
}

static int dsi_a_1200_1920_8_0_enable(struct device *dev)
{
	int err = 0;

	err = dsi_a_1200_1920_8_0_regulator_get(dev);
	if (err < 0) {
		pr_err("dsi regulator get failed\n");
		goto fail;
	}

	err = tegra_panel_gpio_get_dt("a,wuxga-8-0", &panel_of);
	if (err < 0) {
		pr_err("dsi gpio request failed\n");
		goto fail;
	}

	/* If panel rst gpio is specified in device tree,
	 * use that
	 */
	if (gpio_is_valid(panel_of.panel_gpio[TEGRA_GPIO_RESET]))
		en_panel_rst = panel_of.panel_gpio[TEGRA_GPIO_RESET];

	if (avdd_lcd_3v3) {
		err = regulator_enable(avdd_lcd_3v3);
		if (err < 0) {
			pr_err("avdd_lcd regulator enable failed\n");
			goto fail;
		}
	}

	if (dvdd_lcd_1v8) {
		err = regulator_enable(dvdd_lcd_1v8);
		if (err < 0) {
			pr_err("dvdd_lcd regulator enable failed\n");
			goto fail;
		}
	}

	if (vdd_lcd_bl_en) {
		err = regulator_enable(vdd_lcd_bl_en);
		if (err < 0) {
			pr_err("vdd_lcd_bl_en regulator enable failed\n");
			goto fail;
		}
	}

	msleep(20);
#if DSI_PANEL_RESET
	if (!tegra_dc_initialized(dev)) {
		gpio_direction_output(en_panel_rst, 1);
		usleep_range(1000, 5000);
		gpio_set_value(en_panel_rst, 0);
		usleep_range(1000, 5000);
		gpio_set_value(en_panel_rst, 1);
		msleep(20);
	}
#endif
	dc_dev = dev;
	return 0;
fail:
	return err;
}

static int dsi_a_1200_1920_8_0_disable(struct device *dev)
{
	if (gpio_is_valid(en_panel_rst)) {
		/* Wait for 50ms before triggering panel reset */
		msleep(50);
		gpio_set_value(en_panel_rst, 0);
	}

	if (vdd_lcd_bl_en)
		regulator_disable(vdd_lcd_bl_en);

	if (avdd_lcd_3v3)
		regulator_disable(avdd_lcd_3v3);

	if (dvdd_lcd_1v8)
		regulator_disable(dvdd_lcd_1v8);

	dc_dev = NULL;
	return 0;
}

static int dsi_a_1200_1920_8_0_postsuspend(void)
{
	return 0;
}

static int dsi_a_1200_1920_8_0_bl_notify(struct device *dev, int brightness)
{
	int cur_sd_brightness;
	struct backlight_device *bl = NULL;
	struct pwm_bl_data *pb = NULL;
	bl = (struct backlight_device *)dev_get_drvdata(dev);
	pb = (struct pwm_bl_data *)dev_get_drvdata(&bl->dev);

	if (dc_dev)
#ifdef CONFIG_TEGRA_NVDISPLAY
		tegra_sd_check_prism_thresh(dc_dev, brightness);
#else
		nvsd_check_prism_thresh(dc_dev, brightness);
#endif

	cur_sd_brightness = atomic_read(&sd_brightness);
	/* SD brightness is a percentage */
	brightness = (brightness * cur_sd_brightness) / 255;

	/* Apply any backlight response curve */
	if (brightness > 255)
		pr_info("Error: Brightness > 255!\n");
	else if (pb->bl_measured)
		brightness = pb->bl_measured[brightness];

	return brightness;
}

static int dsi_a_1200_1920_8_0_check_fb(struct device *dev,
	struct fb_info *info)
{
	struct platform_device *pdev = NULL;
	pdev = to_platform_device(bus_find_device_by_name(
		&platform_bus_type, NULL, "tegradc.0"));
	return info->device == &pdev->dev;
}

static struct pwm_bl_data_dt_ops dsi_a_1200_1920_8_0_pwm_bl_ops = {
	.notify = dsi_a_1200_1920_8_0_bl_notify,
	.check_fb = dsi_a_1200_1920_8_0_check_fb,
	.blnode_compatible = "a,wuxga-8-0-bl",
};
struct tegra_panel_ops dsi_a_1200_1920_8_0_ops = {
	.enable = dsi_a_1200_1920_8_0_enable,
	.disable = dsi_a_1200_1920_8_0_disable,
	.postsuspend = dsi_a_1200_1920_8_0_postsuspend,
	.pwm_bl_ops = &dsi_a_1200_1920_8_0_pwm_bl_ops,
};
