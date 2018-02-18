/*
 * arch/arm/mach-tegra/panel-lgd-wxga-7-0.c
 *
 * Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>

#include <mach/dc.h>

#include "board.h"
#include "board-panel.h"
#include "devices.h"

static bool reg_requested;
static struct regulator *avdd_lcd_3v3;
static struct regulator *dvdd_lcd;
static struct regulator *vdd_lcd_bl_en;

static int tegratab_dsi_regulator_get(struct device *dev)
{
	int err = 0;

	if (reg_requested)
		return 0;
	avdd_lcd_3v3 = regulator_get(dev, "avdd_lcd");
	if (IS_ERR(avdd_lcd_3v3)) {
		pr_err("avdd_lcd regulator get failed\n");
		err = PTR_ERR(avdd_lcd_3v3);
		avdd_lcd_3v3 = NULL;
		goto fail;
	}

	vdd_lcd_bl_en = regulator_get(dev, "vdd_lcd_bl_en");
	if (IS_ERR(vdd_lcd_bl_en)) {
		pr_err("vdd_lcd_bl_en regulator get failed\n");
		err = PTR_ERR(vdd_lcd_bl_en);
		vdd_lcd_bl_en = NULL;
		goto fail;
	}

	dvdd_lcd = regulator_get(dev, "dvdd_lcd");
	if (IS_ERR(dvdd_lcd)) {
		pr_err("dvdd_lcd regulator get failed\n");
		err = PTR_ERR(dvdd_lcd);
		dvdd_lcd = NULL;
		goto fail;
	}
	reg_requested = true;
	return 0;
fail:
	return err;
}

static int dsi_lgd_wxga_7_0_enable(struct device *dev)
{
	int err = 0;

	err = tegratab_dsi_regulator_get(dev);

	if (err < 0) {
		pr_err("dsi regulator get failed\n");
		goto fail;
	}

	err = tegra_panel_gpio_get_dt("lg,wxga-7", &panel_of);
	if (err < 0) {
		pr_err("dsi gpio request failed\n");
		goto fail;
	}

	/*
	 * Turning on 1.8V then AVDD after 5ms is required per spec.
	 */
	msleep(20);

	if (avdd_lcd_3v3) {
		err = regulator_enable(avdd_lcd_3v3);
		if (err < 0) {
			pr_err("avdd_lcd regulator enable failed\n");
			goto fail;
		}
		regulator_set_voltage(avdd_lcd_3v3, 3300000, 3300000);
	}

	msleep(150);
	if (dvdd_lcd) {
		err = regulator_enable(dvdd_lcd);
		if (err < 0) {
			pr_err("dvdd_lcd regulator enable failed\n");
			goto fail;
		}
	}

	msleep(100);
	if (vdd_lcd_bl_en) {
		err = regulator_enable(vdd_lcd_bl_en);
		if (err < 0) {
			pr_err("vdd_lcd_bl_en regulator enable failed\n");
			goto fail;
		}
	}

	msleep(100);

	return 0;
fail:
	return err;
}

static int dsi_lgd_wxga_7_0_disable(struct device *dev)
{
	if (vdd_lcd_bl_en)
		regulator_disable(vdd_lcd_bl_en);

	if (dvdd_lcd)
		regulator_disable(dvdd_lcd);

	if (avdd_lcd_3v3)
		regulator_disable(avdd_lcd_3v3);

	return 0;
}

static int dsi_lgd_wxga_7_0_postsuspend(void)
{
	return 0;
}

static int dsi_lgd_wxga_7_0_bl_notify(struct device *dev, int brightness)
{
	struct backlight_device *bl = NULL;
	struct pwm_bl_data *pb = NULL;
	bl = (struct backlight_device *)dev_get_drvdata(dev);
	pb = (struct pwm_bl_data *)dev_get_drvdata(&bl->dev);
	/*
	 * In early panel bring-up, we will
	 * not enable PRISM.
	 * Just use same brightness that is delivered from user side.
	 * TODO...
	 * use PRSIM brightness later.
	 */
	if (brightness > 255) {
		pr_info("Error: Brightness > 255!\n");
		brightness = 255;
	} else if (pb->bl_measured)
		brightness = pb->bl_measured[brightness];

	return brightness;
}

static int dsi_lgd_wxga_7_0_check_fb(struct device *dev, struct fb_info *info)
{
	struct platform_device *pdev = NULL;
	pdev = to_platform_device(bus_find_device_by_name(
		&platform_bus_type, NULL, "tegradc.0"));
	return info->device == &pdev->dev;
}

static struct pwm_bl_data_dt_ops dsi_lgd_wxga_7_0_pwm_bl_ops = {
	.notify = dsi_lgd_wxga_7_0_bl_notify,
	.check_fb = dsi_lgd_wxga_7_0_check_fb,
	.blnode_compatible = "lg,wxga-7-bl",
};

struct tegra_panel_ops dsi_lgd_wxga_7_0_ops = {
	.enable = dsi_lgd_wxga_7_0_enable,
	.disable = dsi_lgd_wxga_7_0_disable,
	.postsuspend = dsi_lgd_wxga_7_0_postsuspend,
	.pwm_bl_ops = &dsi_lgd_wxga_7_0_pwm_bl_ops,
};
