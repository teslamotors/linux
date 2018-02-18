/*
 * Tegra Virtualized GPU Platform Interface
 *
 * Copyright (c) 2014-2016, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/of_platform.h>

#include "gk20a.h"
#include "hal_gk20a.h"
#include "platform_gk20a.h"

static int gk20a_tegra_probe(struct device *dev)
{
	struct gk20a_platform *platform = dev_get_drvdata(dev);
	struct device_node *np = dev->of_node;
	const __be32 *host1x_ptr;
	struct platform_device *host1x_pdev = NULL;

	host1x_ptr = of_get_property(np, "nvidia,host1x", NULL);
	if (host1x_ptr) {
		struct device_node *host1x_node =
			of_find_node_by_phandle(be32_to_cpup(host1x_ptr));

		host1x_pdev = of_find_device_by_node(host1x_node);
		if (!host1x_pdev) {
			dev_warn(dev, "host1x device not available");
			return -EPROBE_DEFER;
		}

	} else {
		host1x_pdev = to_platform_device(dev->parent);
		dev_warn(dev, "host1x reference not found. assuming host1x to be parent");
	}

	platform->g->host1x_dev = host1x_pdev;

	return 0;
}

struct gk20a_platform vgpu_tegra_platform = {
	.has_syncpoints = true,
	.aggressive_sync_destroy_thresh = 64,

	/* power management configuration */
	.can_railgate		= false,
	.can_elpg               = false,
	.enable_slcg            = false,
	.enable_blcg            = false,
	.enable_elcg            = false,
	.enable_elpg            = false,
	.enable_aelpg           = false,

	.probe = gk20a_tegra_probe,
	.default_big_page_size	= SZ_128K,

	.virtual_dev = true,
};
