/*
 * arch/arm/mach-tegra/panel-s-uhdtv-15-6.c
 *
 * Copyright (c) 2012-2015, NVIDIA CORPORATION. All rights reserved.
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

static bool reg_requested;

static struct regulator *vdd_lcd_bl_en; /* VDD_LCD_BL_EN */

static struct regulator *avdd_io_edp; /* AVDD_IO_EDP */

static struct regulator *vdd_ds_1v8; /* VDD_1V8_AON */
static struct regulator *avdd_3v3_dp; /* EDP_3V3_IN: LCD_RST_GPIO */
static struct regulator *avdd_lcd; /* VDD_LCD_HV */
static u16 en_panel_rst;

/*
 * In platform dts side, followings need to be defined
 * for this panel, additionally.
 * - nvidia,hpd-gpio in panel node
 * - pwm-gpio in backlight node
 */

static int shield_edp_regulator_get(struct device *dev)
{
	int err = 0;

	if (reg_requested)
		return 0;

#if defined(CONFIG_TEGRA_NVDISPLAY)
	vdd_ds_1v8 = regulator_get(dev, "dvdd_lcd");
#else /* !CONFIG_TEGRA_NVDISPLAY */
	vdd_ds_1v8 = regulator_get(dev, "vdd_ds_1v8");
#endif /* CONFIG_TEGRA_NVDISPLAY */

	if (IS_ERR(vdd_ds_1v8)) {
		pr_err("vdd_ds_1v8 regulator get failed\n");
		err = PTR_ERR(vdd_ds_1v8);
		vdd_ds_1v8 = NULL;
		goto fail;
	}

	/* backlight */
	vdd_lcd_bl_en = regulator_get(dev, "vdd_lcd_bl_en");
	if (IS_ERR(vdd_lcd_bl_en)) {
		pr_err("vdd_lcd_bl_en regulator get failed\n");
		err = PTR_ERR(vdd_lcd_bl_en);
		vdd_lcd_bl_en = NULL;
		goto fail;
	}

	/* lcd */
	avdd_lcd = regulator_get(dev, "avdd_lcd");
	if (IS_ERR(avdd_lcd)) {
		pr_err("avdd_lcd regulator get failed\n");
		err = PTR_ERR(avdd_lcd);
		avdd_lcd = NULL;
		goto fail;
	}

	/* LCD_RST
	 *
	 * If the panel reset gpio is explicitly specified in DT,
	 * prefer using it over the avdd_3v3_dp regulator.
	 */
	err = tegra_panel_gpio_get_dt("s-edp,uhdtv-15-6", &panel_of);
	if (err < 0 || !gpio_is_valid(panel_of.panel_gpio[TEGRA_GPIO_RESET])) {
		avdd_3v3_dp = regulator_get(dev, "avdd_3v3_dp");
		if (IS_ERR(avdd_3v3_dp)) {
			pr_err("avdd_3v3_dp regulator get failed\n");
			err = PTR_ERR(avdd_3v3_dp);
			avdd_3v3_dp = NULL;
			goto fail;
		}
	} else {
		en_panel_rst = panel_of.panel_gpio[TEGRA_GPIO_RESET];
	}

#if defined(CONFIG_TEGRA_NVDISPLAY)
	avdd_io_edp = NULL;
#else /* !CONFIG_TEGRA_NVDISPLAY */
	avdd_io_edp = regulator_get(dev, "avdd_io_edp");
	if (IS_ERR(avdd_io_edp)) {
		pr_err("avdd_io_edp regulator get failed\n");
		err = PTR_ERR(avdd_io_edp);
		avdd_io_edp = NULL;
		goto fail;
	}
#endif /* CONFIG_TEGRA_NVDISPLAY */

	reg_requested = true;
	return 0;
fail:
	return err;
}

static int edp_s_uhdtv_15_6_enable(struct device *dev)
{
	int err = 0;

	err = shield_edp_regulator_get(dev);
	if (err < 0) {
		pr_err("edp regulator get failed\n");
		goto fail;
	}

	if (vdd_ds_1v8) {
		err = regulator_enable(vdd_ds_1v8);
		if (err < 0) {
			pr_err("vdd_ds_1v8 regulator enable failed\n");
			goto fail;
		}
	}

	if (avdd_lcd) {
		err = regulator_enable(avdd_lcd);
		if (err < 0) {
			pr_err("avdd_lcd regulator enable failed\n");
			goto fail;
		}
	}

	/* LCD_RST */
	if (gpio_is_valid(en_panel_rst)) {
		gpio_direction_output(en_panel_rst, 1);
		usleep_range(1000, 5000);
		gpio_set_value(en_panel_rst, 0);
		usleep_range(1000, 5000);
		gpio_set_value(en_panel_rst, 1);
	}

	if (avdd_3v3_dp) {
		err = regulator_enable(avdd_3v3_dp);
		if (err < 0) {
			pr_err("avdd_3v3_dp regulator enable failed\n");
			goto fail;
		}
	}

	msleep(110);

	if (vdd_lcd_bl_en) {
		err = regulator_enable(vdd_lcd_bl_en);
		if (err < 0) {
			pr_err("vdd_lcd_bl_en regulator enable failed\n");
			goto fail;
		}
	}
	msleep(180);

	if (avdd_io_edp) {
		err = regulator_enable(avdd_io_edp);
		if (err < 0) {
			pr_err("avdd_io_edp regulator enable failed\n");
			goto fail;
		}
	}

	return 0;
fail:
	return err;
}

static int edp_s_uhdtv_15_6_disable(struct device *dev)
{
	if (vdd_lcd_bl_en)
		regulator_disable(vdd_lcd_bl_en);

	if (gpio_is_valid(en_panel_rst)) {
		gpio_set_value(en_panel_rst, 0);
		usleep_range(1000, 5000);
	}

	if (avdd_3v3_dp)
		regulator_disable(avdd_3v3_dp);

	if (avdd_lcd)
		regulator_disable(avdd_lcd);

	if (vdd_ds_1v8)
		regulator_disable(vdd_ds_1v8);

	if (avdd_io_edp)
		regulator_disable(avdd_io_edp);

	msleep(500);

	return 0;
}

static int edp_s_uhdtv_15_6_postsuspend(void)
{
	return 0;
}

static int edp_s_uhdtv_15_6_bl_notify(struct device *dev, int brightness)
{
	struct backlight_device *bl = NULL;
	struct pwm_bl_data *pb = NULL;
	int cur_sd_brightness = atomic_read(&sd_brightness);
	bl = (struct backlight_device *)dev_get_drvdata(dev);
	pb = (struct pwm_bl_data *)dev_get_drvdata(&bl->dev);

	/* SD brightness is a percentage */
	brightness = (brightness * cur_sd_brightness) / 255;

	/* Apply any backlight response curve */
	if (brightness > 255)
		pr_info("Error: Brightness > 255!\n");
	else if (pb->bl_measured)
		brightness = pb->bl_measured[brightness];

	return brightness;
}

static int edp_s_uhdtv_15_6_check_fb(struct device *dev, struct fb_info *info)
{
	struct platform_device *pdev = NULL;
	pdev = to_platform_device(bus_find_device_by_name(
		&platform_bus_type, NULL, "tegradc.0"));
	return info->device == &pdev->dev;
}

static struct pwm_bl_data_dt_ops edp_s_uhdtv_15_6_pwm_bl_ops = {
	.notify = edp_s_uhdtv_15_6_bl_notify,
	.check_fb = edp_s_uhdtv_15_6_check_fb,
	.blnode_compatible = "s-edp,uhdtv-15-6-bl",
};

struct tegra_panel_ops edp_s_uhdtv_15_6_ops = {
	.enable = edp_s_uhdtv_15_6_enable,
	.disable = edp_s_uhdtv_15_6_disable,
	.postsuspend = edp_s_uhdtv_15_6_postsuspend,
	.pwm_bl_ops = &edp_s_uhdtv_15_6_pwm_bl_ops,
};
