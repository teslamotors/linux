/*
 * Copyright (C) 2015-2016, NVIDIA Corporation. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/platform_device.h>
#include <linux/tegra_nvadsp.h>
#include <linux/tegra-hsp.h>
#include <linux/irqchip/tegra-agic.h>

static void nvadsp_dbell_handler(void *data)
{
	struct platform_device *pdev = data;
	struct device *dev = &pdev->dev;

	dev_info(dev, "APE DBELL handler\n");
}


int nvadsp_os_init(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int ret;

	ret = tegra_hsp_db_add_handler(HSP_MASTER_APE,
				       nvadsp_dbell_handler, pdev);
	if (ret) {
		dev_err(dev, "failed to add HSP_MASTER_APE DB handler\n");
		goto end;
	}
 end:
	return ret;
}
