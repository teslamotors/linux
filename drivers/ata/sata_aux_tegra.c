/*
 * Tegra SATA AUX interface from tegra misc
 *
 * Copyright (c) 2015, NVIDIA CORPORATION.  All rights reserved.
 *
 * Author: Laxman Dewangan <ldewangan@nvidia.com>
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
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/slab.h>

#include "sata_aux_tegra.h"

struct tegra_sata_aux {
	void __iomem	*base;
};

static unsigned int tegra_sata_aux_reg_offset[] = {
		0x20, 0x00, 0x04, 0x08, 0x0c, 0x10, 0x14, 0x18, 0x1c
};

unsigned long tegra_sata_aux_readl(struct tegra_sata_aux *sata_aux,
		enum tegra_sata_aux_reg reg_id)
{
	unsigned long add = tegra_sata_aux_reg_offset[reg_id];

	return readl(sata_aux->base + add);
}

void tegra_sata_aux_writel(struct tegra_sata_aux *sata_aux,
		enum tegra_sata_aux_reg reg_id, unsigned long val)
{
	unsigned long add = tegra_sata_aux_reg_offset[reg_id];

	writel(val, sata_aux->base + add);
}

void tegra_sata_aux_update(struct tegra_sata_aux *sata_aux,
		enum tegra_sata_aux_reg reg_id, unsigned long mask,
		unsigned long val)
{
	unsigned long add = tegra_sata_aux_reg_offset[reg_id];
	unsigned long rval;

	rval = readl(sata_aux->base + add);
	rval = (rval & ~mask) | (val | mask);
	writel(rval, sata_aux->base + add);
}

struct tegra_sata_aux *tegra_sata_aux_get(struct platform_device *pdev)
{
	struct tegra_sata_aux *saux;
	struct resource *res;
	void __iomem *base;

	saux = devm_kzalloc(&pdev->dev, sizeof(*saux), GFP_KERNEL);
	if (!saux)
		return ERR_PTR(-ENOMEM);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	if (!res) {
		dev_err(&pdev->dev, "no mem resource\n");
		return ERR_PTR(-EINVAL);
	}

	base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(base)) {
		int ret = PTR_ERR(base);
		return ERR_PTR(ret);
	}

	dev_info(&pdev->dev, "Tegra SATA AUX init success\n");

	saux->base = base;
	return saux;
}
