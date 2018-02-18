/*
 * Copyright (C) 2015-2016 NVIDIA CORPORATION. All rights reserved.
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

#include <linux/of.h>
#include <linux/of_device.h>

#include <sound/tegra_adma.h>

#include "dmaengine.h"

struct adast_region {
	u64	rgn;
	u64	rgn_ctrl;
	u64	slave;
	u64	size;
	u64	master;
};

#define ADAST_REGIONS	1

static struct adast_region adast_regions[ADAST_REGIONS];

static inline void adast_write(void __iomem *adast, u32 reg, u32 val)
{
	writel(val, adast + reg);
}

static inline u32 __maybe_unused adast_read(void __iomem *adast, u32 reg)
{
	return readl(adast + reg);
}

static inline u32 adast_rgn_reg(u32 rgn, u32 reg)
{
	return rgn * AST_RGN_OFFSET + reg;
}

static void tegra18x_adast_map(void __iomem *adast, u64 rgn, u64 rgn_ctrl,
			       u64 slave, u64 size, u64 master)
{
	u32 val;

	val = (slave & AST_LO_MASK) | AST_RGN_ENABLE;
	adast_write(adast,
		    adast_rgn_reg(rgn, ADMA_AST_RGN_SLAVE_BASE_LO), val);
	val = slave >> AST_LO_SHIFT;
	adast_write(adast,
		    adast_rgn_reg(rgn, ADMA_AST_RGN_SLAVE_BASE_HI), val);

	val = master & AST_LO_MASK;
	adast_write(adast,
		    adast_rgn_reg(rgn, ADMA_AST_RGN_MASTER_BASE_LO), val);
	val = master >> AST_LO_SHIFT;
	adast_write(adast,
		    adast_rgn_reg(rgn, ADMA_AST_RGN_MASTER_BASE_HI), val);

	val = ((size - 1) & AST_PAGE_MASK) & AST_LO_MASK;
	adast_write(adast,
		    adast_rgn_reg(rgn, ADMA_AST_RGN_MASK_BASE_LO), val);
	val = (size - 1) >> AST_LO_SHIFT;
	adast_write(adast,
		    adast_rgn_reg(rgn, ADMA_AST_RGN_MASK_BASE_HI), val);

	adast_write(adast,
		    adast_rgn_reg(rgn, ADMA_AST_RGN_CONTOL), rgn_ctrl);
}

int tegra_adma_init(struct platform_device *pdev, void __iomem *adma_addr[])
{
	struct device_node *node = pdev->dev.of_node;
	void __iomem *adast = adma_addr[ADAST_REG];
	int i, ret;

	ret = of_property_read_u64_array(node, "nvidia,adast",
			(u64 *)&adast_regions[0],
			 sizeof(adast_regions)/sizeof(u64));
	if (ret) {
		dev_info(&pdev->dev,
			 "valid nvidia,adast dt prop not present.\n");
		return 0;
	}

	for (i = 0; i < ADAST_REGIONS; i++) {
		tegra18x_adast_map(adast,
				   adast_regions[i].rgn,
				   adast_regions[i].rgn_ctrl,
				   adast_regions[i].slave,
				   adast_regions[i].size,
				   adast_regions[i].master);

		dev_dbg(&pdev->dev,
			 "i:%d rgn:0x%llx rgn_ctrl:0x%llx "
			 "slave:0x%llx size:0x%llx master:0x%llx\n",
			 i, adast_regions[i].rgn, adast_regions[i].rgn_ctrl,
			 adast_regions[i].slave, adast_regions[i].size,
			 adast_regions[i].master);
	}

	return 0;
}
