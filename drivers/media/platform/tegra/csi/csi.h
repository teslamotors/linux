/*
 * NVIDIA Tegra CSI Device Header
 *
 * Copyright (c) 2015-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * Author: Bryan Wu <pengw@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __CSI_H_
#define __CSI_H_

#include <media/media-entity.h>
#include <media/v4l2-async.h>
#include <media/v4l2-ctrls.h>

#include "vi/vi.h"

enum tegra_csi_port_num {
	PORT_A = 0,
	PORT_B = 1,
	PORT_C = 2,
	PORT_D = 3,
	PORT_E = 4,
	PORT_F = 5,
};

struct tegra_csi_port {
	void __iomem *pixel_parser;
	void __iomem *cil;
	void __iomem *tpg;

	/* One pair of sink/source pad has one format */
	struct v4l2_mbus_framefmt format;
	const struct tegra_video_format *core_format;
	unsigned int lanes;

	enum tegra_csi_port_num num;
};

struct tegra_csi_device {
	struct v4l2_subdev subdev;
	struct device *dev;
	void __iomem *iomem;
	struct clk *clk;

	struct tegra_csi_port *ports;
	struct media_pad *pads;

	int num_ports;
};

static inline struct tegra_csi_device *to_csi(struct v4l2_subdev *subdev)
{
	return container_of(subdev, struct tegra_csi_device, subdev);
}

int tegra_csi_media_controller_init(struct tegra_csi_device *csi,
				struct platform_device *pdev);
int tegra_csi_media_controller_remove(struct tegra_csi_device *csi);
#endif
