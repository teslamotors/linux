/*
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fb.h>
#include <linux/backlight.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/regulator/consumer.h>
#include <linux/tegra_dsi_backlight.h>
#include <linux/regmap.h>
#include <linux/edp.h>
#include <mach/dc.h>
#include "../tegra/dc/dsi.h"

struct tegra_dsi_bl_data {
	struct device		*dev;
	struct tegra_dsi_cmd	*dsi_backlight_cmd;
	u32 n_backlight_cmd;
	int which_dc;
	int (*notify)(struct device *dev, int brightness);
	int (*check_fb)(struct device *dev, struct fb_info *info);
};

static int send_backlight_cmd(struct tegra_dsi_bl_data *tbl, int brightness)
{
	int err = 0;
	struct tegra_dc *dc = tegra_dc_get_dc(tbl->which_dc);
	struct tegra_dc_dsi_data *dsi = dc->out_data;
	struct tegra_dsi_cmd *cur_cmd;

	if (tbl->dsi_backlight_cmd)
		cur_cmd = tbl->dsi_backlight_cmd;
	else
		return -EINVAL;

	mutex_lock(&dsi->lock);

	cur_cmd->sp_len_dly.sp.data1 = brightness;
	err = tegra_dsi_send_panel_cmd(dc, dsi,
			tbl->dsi_backlight_cmd,
			tbl->n_backlight_cmd);

	if (err < 0)
		goto fail;

fail:
	mutex_unlock(&dsi->lock);

	return err;
}

static int tegra_dsi_backlight_update_status(struct backlight_device *bl)
{
	struct tegra_dsi_bl_data *tbl = dev_get_drvdata(&bl->dev);
	int brightness = bl->props.brightness;

	if (tbl)
		return send_backlight_cmd(tbl, brightness);
	else
		return dev_err(&bl->dev,
				"tegra display controller not available\n");
}

static int tegra_dsi_backlight_get_brightness(struct backlight_device *bl)
{
	return bl->props.brightness;
}

static const struct backlight_ops tegra_dsi_backlight_ops = {
	.update_status	= tegra_dsi_backlight_update_status,
	.get_brightness	= tegra_dsi_backlight_get_brightness,
};

static int tegra_dsi_bl_probe(struct platform_device *pdev)
{
	struct backlight_device *bl;
	struct backlight_properties props;
	struct tegra_dsi_bl_platform_data *data;
	struct tegra_dsi_bl_data *tbl;
	int ret;

	data = pdev->dev.platform_data;
	if (!data) {
		dev_err(&pdev->dev, "failed to find platform data\n");
		return -EINVAL;
	}

	tbl = kzalloc(sizeof(*tbl), GFP_KERNEL);
	if (!tbl)
		ret = -ENOMEM;

	tbl->dev = &pdev->dev;
	tbl->which_dc = data->which_dc;
	tbl->check_fb = data->check_fb;
	tbl->notify = data->notify;
	tbl->dsi_backlight_cmd = data->dsi_backlight_cmd;
	tbl->n_backlight_cmd = data->n_backlight_cmd;

	props.type = BACKLIGHT_RAW;
	props.max_brightness = 255;
	bl = backlight_device_register(dev_name(&pdev->dev), tbl->dev, tbl,
				       &tegra_dsi_backlight_ops, &props);
	if (IS_ERR(bl)) {
		dev_err(&pdev->dev, "failed to register backlight\n");
		ret = PTR_ERR(bl);
	}

	bl->props.brightness = data->dft_brightness;
	platform_set_drvdata(pdev, bl);

	return 0;
}

static int tegra_dsi_bl_remove(struct platform_device *pdev)
{
	struct backlight_device *bl = platform_get_drvdata(pdev);
	struct tegra_dsi_bl_data *tbl = dev_get_drvdata(&bl->dev);

	backlight_device_unregister(bl);
	kfree(tbl);

	return 0;
}

static struct platform_driver tegra_dsi_bl_driver = {
	.driver = {
		.name	= "tegra-dsi-backlight",
		.owner	= THIS_MODULE,
	},
	.probe	= tegra_dsi_bl_probe,
	.remove	= tegra_dsi_bl_remove,
};
module_platform_driver(tegra_dsi_bl_driver);

static int __init tegra_dsi_bl_init(void)
{
	return platform_driver_register(&tegra_dsi_bl_driver);
}
late_initcall(tegra_dsi_bl_init);

static void __exit tegra_dsi_bl_exit(void)
{
	platform_driver_unregister(&tegra_dsi_bl_driver);
}
module_exit(tegra_dsi_bl_exit);

MODULE_DESCRIPTION("Tegra DSI Backlight Driver");
MODULE_LICENSE("GPL");
