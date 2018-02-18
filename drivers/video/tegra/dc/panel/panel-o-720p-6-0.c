/*
 * arch/arm/mach-tegra/panel-o-720p-6-0.c
 *
 * Copyright (c) 2015, NVIDIA CORPORATION.  All rights reserved.
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
#include "gpio-names.h"

static u16 en_panel_rst;

static struct regulator *pavdd_lcd_reg;
static struct regulator *navdd_lcd_reg;
static struct regulator *bl_en_reg;

static bool reg_requested;

static int reg_get(struct device *dev)
{
	int err = 0;

	if (reg_requested)
		return 0;

	pavdd_lcd_reg = regulator_get(dev, "pavdd_lcd");
	if (IS_ERR(pavdd_lcd_reg)) {
		pr_err("pavdd_lcd_reg regulator get failed\n");
		err = PTR_ERR(pavdd_lcd_reg);
		pavdd_lcd_reg = NULL;
		goto fail;
	}
	navdd_lcd_reg = regulator_get(dev, "navdd_lcd");
	if (IS_ERR(navdd_lcd_reg)) {
		pr_err("navdd_lcd_reg regulator get failed\n");
		err = PTR_ERR(navdd_lcd_reg);
		navdd_lcd_reg = NULL;
		goto fail;
	}

	bl_en_reg = regulator_get(dev, "vdd_lcd_bl_en");
	if (IS_ERR(bl_en_reg)) {
		pr_err("bl_en_reg regulator get failed\n");
		err = PTR_ERR(bl_en_reg);
		bl_en_reg = NULL;
		goto fail;
	}
	reg_requested = true;
	return 0;
fail:
	return err;
}

static int dsi_o_720p_6_0_enable(struct device *dev)
{
	int err = 0;

	err = reg_get(dev);
	if (err < 0) {
		pr_err("dsi regulator get failed\n");
		goto fail;
	}

	err = tegra_panel_gpio_get_dt("o,720-1280-6-0", &panel_of);
	if (err < 0) {
		pr_err("dsi gpio request failed\n");
		goto fail;
	}
	/* If panel rst gpio is specified in device tree,
	 * use that.
	 */
	if (gpio_is_valid(panel_of.panel_gpio[TEGRA_GPIO_RESET]))
		en_panel_rst = panel_of.panel_gpio[TEGRA_GPIO_RESET];
	else
		pr_warn("rst gpio is not defined in DT\n");
	gpio_direction_output(en_panel_rst, 0);

	if (pavdd_lcd_reg) {
		err = regulator_enable(pavdd_lcd_reg);
		if (err < 0) {
			pr_err("pavdd lcd reg regulator enable failed\n");
			goto fail;
		}
	}
	msleep(20);
	if (navdd_lcd_reg) {
		err = regulator_enable(navdd_lcd_reg);
		if (err < 0) {
			pr_err("navdd lcd reg regulator enable failed\n");
			goto fail;
		}
	}
	msleep(20);
	gpio_set_value(en_panel_rst, 1);

	return 0;
fail:
	return err;
}

static int dsi_o_720p_6_0_postpoweron(struct device *dev)
{
/*
 * Having reset control in postpoweron.
 *  - dc->out->postpoweron => reset control
 *  - dc->out_ops->postpoweron => dsi init command trigger
 */
	int err;

	if (bl_en_reg) {
		err = regulator_enable(bl_en_reg);
		if (err < 0) {
			pr_err("bl_en_reg regulator enable failed\n");
			goto fail;
		}
	}

	return 0;
fail:
	return err;
}

static int dsi_o_720p_6_0_disable(struct device *dev)
{
	if (bl_en_reg)
		regulator_disable(bl_en_reg);
	usleep_range(1000, 1020);
	gpio_set_value(en_panel_rst, 0);
	usleep_range(1000, 1020);

	if (navdd_lcd_reg)
		regulator_disable(navdd_lcd_reg);
	msleep(20);
	if (pavdd_lcd_reg)
		regulator_disable(pavdd_lcd_reg);
	msleep(20);

	return 0;
}

static int dsi_o_720p_6_0_bl_notify(struct device *dev, int brightness)
{
	/*
	 * Just return delivered brightness from OS
	 * in earlier bring-up stage.
	 */
	if (brightness > 255)
		pr_info("Error: Brightness > 255!\n");
	return brightness;
}

static int dsi_o_720p_6_0_bl_check_fb(struct device *dev, struct fb_info *info)
{
	struct platform_device *pdev = NULL;
	pdev = to_platform_device(bus_find_device_by_name(
		&platform_bus_type, NULL, "tegradc.0"));
	return info->device == &pdev->dev;
}

static struct pwm_bl_data_dt_ops dsi_o_720p_6_0_pwm_bl_ops = {
	.notify = dsi_o_720p_6_0_bl_notify,
	.check_fb = dsi_o_720p_6_0_bl_check_fb,
	.blnode_compatible = "o,720-1280-6-0-bl",
};

struct tegra_panel_ops dsi_o_720p_6_0_ops = {
	.enable = dsi_o_720p_6_0_enable,
	.disable = dsi_o_720p_6_0_disable,
	.postpoweron = dsi_o_720p_6_0_postpoweron,
	.pwm_bl_ops = &dsi_o_720p_6_0_pwm_bl_ops,
};
