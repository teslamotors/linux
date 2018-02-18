/*
 * Copyright (c) 2013-2015, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/clk.h>

#include <media/soc_camera.h>
#include <media/soc_mediabus.h>
#include <media/tegra_v4l2_camera.h>

#include <mach/clk.h>

#include "common.h"

#define VI_BYPASS_CAM_DRV_NAME				"vi-bypass"
#define VI_BYPASS_CAM_CARD_NAME				"vi-bypass"
#define VI_BYPASS_CAM_VERSION				KERNEL_VERSION(0, 0, 5)

struct vi_bypass_camera_clk {
	const char			*name;
	struct clk			*clk;
	u32				freq;
	int				use_devname;
};

struct vi_bypass_camera {
	struct tegra_camera		cam;

	struct vi_bypass_camera_clk	*clks;
	int				num_clks;
};

static bool vi_bypass_port_is_valid(int port)
{
	return (((port) >= TEGRA_CAMERA_PORT_CSI_A) &&
		((port) <= TEGRA_CAMERA_PORT_CSI_F));
}

static s32 vi_bypass_bytes_per_line(u32 width,
				    const struct soc_mbus_pixelfmt *mf)
{
	return soc_mbus_bytes_per_line(width, mf);
}

struct tegra_camera_ops vi_bypass_ops = {
	.port_is_valid	= vi_bypass_port_is_valid,
	.bytes_per_line	= vi_bypass_bytes_per_line,
};

static struct of_device_id vi_bypass_of_match[] = {
	{ .compatible = "nvidia,tegra210-vi-bypass" },
	{ .compatible = "nvidia,tegra186-vi-bypass" },
	{ },
};

static int vi_bypass_probe(struct platform_device *pdev)
{
	struct vi_bypass_camera *vi_bypass_cam;
	int err;

	vi_bypass_cam = devm_kzalloc(&pdev->dev,
				     sizeof(struct vi_bypass_camera),
				     GFP_KERNEL);
	if (!vi_bypass_cam) {
		dev_err(&pdev->dev, "couldn't allocate cam\n");
		return -ENOMEM;
	}

	/* Init VI_BYPASS ops */
	pdev->id = 0;
	platform_set_drvdata(pdev, vi_bypass_cam);
	strlcpy(vi_bypass_cam->cam.card, VI_BYPASS_CAM_CARD_NAME,
		sizeof(vi_bypass_cam->cam.card));
	vi_bypass_cam->cam.version = VI_BYPASS_CAM_VERSION;
	vi_bypass_cam->cam.ops = &vi_bypass_ops;

	err = tegra_camera_init(pdev, &vi_bypass_cam->cam);
	if (err)
		return err;

	return 0;
}

static int vi_bypass_remove(struct platform_device *pdev)
{
	struct soc_camera_host *ici = to_soc_camera_host(&pdev->dev);
	struct tegra_camera *cam = container_of(ici,
						struct tegra_camera, ici);
	struct vi_bypass_camera *vi_bypass_cam = (struct vi_bypass_camera *)cam;

	tegra_camera_deinit(pdev, &vi_bypass_cam->cam);

	return 0;
}

static struct platform_driver vi_bypass_driver = {
	.driver = {
		.name	= VI_BYPASS_CAM_DRV_NAME,
		.of_match_table = of_match_ptr(vi_bypass_of_match),
	},
	.probe		= vi_bypass_probe,
	.remove		= vi_bypass_remove,
};

module_platform_driver(vi_bypass_driver);

MODULE_DESCRIPTION("TEGRA SoC Camera Host SCF VI bypass driver");
