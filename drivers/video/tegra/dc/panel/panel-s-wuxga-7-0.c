/*
 * arch/arm/mach-tegra/panel-s-wuxga-7-0.c
 *
 * Copyright (c) 2015-2016, NVIDIA CORPORATION.  All rights reserved.
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

#define PRISM_THRESHOLD		50
#define HYST_VAL		25

static bool reg_requested;
static struct regulator *avdd_lcd_3v0;
static struct regulator *dvdd_lcd_1v8;
static struct regulator *vpp_lcd;
static struct regulator *vmm_lcd;
static struct device *dc_dev;
static u16 en_panel_rst;

static int dsi_s_wuxga_7_0_regulator_get(struct device *dev)
{
	int err = 0;

	if (reg_requested)
		return 0;

	avdd_lcd_3v0 = regulator_get(dev, "avdd_lcd");
	if (IS_ERR_OR_NULL(avdd_lcd_3v0)) {
		pr_err("avdd_lcd regulator get failed\n");
		err = PTR_ERR(avdd_lcd_3v0);
		avdd_lcd_3v0 = NULL;
		goto fail;
	}

	dvdd_lcd_1v8 = regulator_get(dev, "dvdd_lcd");
	if (IS_ERR_OR_NULL(dvdd_lcd_1v8)) {
		pr_err("dvdd_lcd_1v8 regulator get failed\n");
		err = PTR_ERR(dvdd_lcd_1v8);
		dvdd_lcd_1v8 = NULL;
		goto fail;
	}

	vpp_lcd = regulator_get(dev, "outp");
	if (IS_ERR_OR_NULL(vpp_lcd)) {
		pr_err("vpp_lcd regulator get failed\n");
		err = PTR_ERR(vpp_lcd);
		vpp_lcd = NULL;
		goto fail;
	}

	vmm_lcd = regulator_get(dev, "outn");
	if (IS_ERR_OR_NULL(vmm_lcd)) {
		pr_err("vmm_lcd regulator get failed\n");
		err = PTR_ERR(vmm_lcd);
		vmm_lcd = NULL;
		goto fail;
	}

	reg_requested = true;
	return 0;
fail:
	return err;
}

static int dsi_s_wuxga_7_0_enable(struct device *dev)
{
	int err = 0;

	err = dsi_s_wuxga_7_0_regulator_get(dev);
	if (err < 0) {
		pr_err("dsi regulator get failed\n");
		goto fail;
	}

	err = tegra_panel_gpio_get_dt("s,wuxga-7-0", &panel_of);
	if (err < 0) {
		pr_err("display gpio get failed\n");
		goto fail;
	}

	if (gpio_is_valid(panel_of.panel_gpio[TEGRA_GPIO_RESET]))
		en_panel_rst = panel_of.panel_gpio[TEGRA_GPIO_RESET];
	else {
		pr_err("display reset gpio invalid\n");
		goto fail;
	}


	if (dvdd_lcd_1v8) {
		err = regulator_enable(dvdd_lcd_1v8);
		if (err < 0) {
			pr_err("dvdd_lcd regulator enable failed\n");
			goto fail;
		}
	}

	usleep_range(500, 1500);

	if (avdd_lcd_3v0) {
		err = regulator_enable(avdd_lcd_3v0);
		if (err < 0) {
			pr_err("avdd_lcd regulator enable failed\n");
			goto fail;
		}
	}

	usleep_range(500, 1500);

	if (vpp_lcd) {
		err = regulator_enable(vpp_lcd);
		if (err < 0) {
			pr_err("vpp_lcd regulator enable failed\n");
			goto fail;
		}

		err = regulator_set_voltage(vpp_lcd, 5500000, 5500000);
		if (err < 0) {
			pr_err("vpp_lcd regulator failed changing voltage\n");
			goto fail;
		}
	}

	usleep_range(500, 1500);

	if (vmm_lcd) {
		err = regulator_enable(vmm_lcd);
		if (err < 0) {
			pr_err("vmm_lcd regulator enable failed\n");
			goto fail;
		}

		err = regulator_set_voltage(vmm_lcd, 5500000, 5500000);
		if (err < 0) {
			pr_err("vmm_lcd regulator failed changing voltage\n");
			goto fail;
		}
	}

	usleep_range(7000, 8000);

#if DSI_PANEL_RESET
	if (!tegra_dc_initialized(dev)) {
		err = gpio_direction_output(en_panel_rst, 1);
		if (err < 0) {
			pr_err("setting display reset gpio value failed\n");
			goto fail;
		}
	}
#endif
	dc_dev = dev;
	return 0;
fail:
	return err;
}

static int dsi_s_wuxga_7_0_disable(struct device *dev)
{
	if (gpio_is_valid(en_panel_rst)) {
		/* Wait for 50ms before triggering panel reset */
		msleep(50);
		gpio_set_value(en_panel_rst, 0);
		usleep_range(500, 1000);
	} else
		pr_err("ERROR! display reset gpio invalid\n");

	if (vmm_lcd)
		regulator_disable(vmm_lcd);

	usleep_range(500, 2000);

	if (vpp_lcd)
		regulator_disable(vpp_lcd);

	usleep_range(500, 1500);

	if (avdd_lcd_3v0)
		regulator_disable(avdd_lcd_3v0);

	usleep_range(500, 1500);

	if (dvdd_lcd_1v8)
		regulator_disable(dvdd_lcd_1v8);

	/* Min delay of 140ms required to avoid turning
	 * the panel on too soon after power off */
	msleep(140);

	dc_dev = NULL;
	return 0;
}

static int dsi_s_wuxga_7_0_postsuspend(void)
{
	return 0;
}

static int dsi_s_wuxga_7_0_bl_notify(struct device *dev, int brightness)
{
	int cur_sd_brightness;
	struct backlight_device *bl = NULL;
	struct pwm_bl_data *pb = NULL;
	bl = (struct backlight_device *)dev_get_drvdata(dev);
	pb = (struct pwm_bl_data *)dev_get_drvdata(&bl->dev);

	if (dc_dev) {
		if (brightness <= PRISM_THRESHOLD)
#ifdef CONFIG_TEGRA_NVDISPLAY
			tegra_sd_enbl_dsbl_prism(dc_dev, false);
#else
			nvsd_enbl_dsbl_prism(dc_dev, false);
#endif
		else if (brightness > PRISM_THRESHOLD + HYST_VAL)
#ifdef CONFIG_TEGRA_NVDISPLAY
			tegra_sd_enbl_dsbl_prism(dc_dev, true);
#else
			nvsd_enbl_dsbl_prism(dc_dev, true);
#endif
	}

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

static int dsi_s_wuxga_7_0_check_fb(struct device *dev,
	struct fb_info *info)
{
	struct platform_device *pdev = NULL;
	pdev = to_platform_device(bus_find_device_by_name(
		&platform_bus_type, NULL, "tegradc.0"));
	return info->device == &pdev->dev;
}

static struct pwm_bl_data_dt_ops dsi_s_wuxga_7_0_pwm_bl_ops = {
	.notify = dsi_s_wuxga_7_0_bl_notify,
	.check_fb = dsi_s_wuxga_7_0_check_fb,
	.blnode_compatible = "s,wuxga-7-0-bl",
};
struct tegra_panel_ops dsi_s_wuxga_7_0_ops = {
	.enable = dsi_s_wuxga_7_0_enable,
	.disable = dsi_s_wuxga_7_0_disable,
	.postsuspend = dsi_s_wuxga_7_0_postsuspend,
	.pwm_bl_ops = &dsi_s_wuxga_7_0_pwm_bl_ops,
};
