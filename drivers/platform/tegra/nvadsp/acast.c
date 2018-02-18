/*
 * Copyright (C) 2016 NVIDIA CORPORATION. All rights reserved.
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

#include <linux/platform_device.h>
#include <linux/io.h>
#include "dev-t18x.h"

#define AST_CONTROL					0x000
#define AST_STREAMID_CTL_0				0x020
#define AST_STREAMID_CTL_1				0x024
#define AST_RGN_SLAVE_BASE_LO				0x100
#define AST_RGN_SLAVE_BASE_HI				0x104
#define AST_RGN_MASK_BASE_LO				0x108
#define AST_RGN_MASK_BASE_HI				0x10c
#define AST_RGN_MASTER_BASE_LO				0x110
#define AST_RGN_MASTER_BASE_HI				0x114
#define AST_RGN_CONTOL					0x118

#define AST_PAGE_MASK					(~0xFFF)
#define AST_LO_SHIFT					32
#define AST_LO_MASK					0xFFFFFFFF
#define AST_PHY_SID_IDX					0
#define AST_APE_SID_IDX					1
#define AST_NS						(1 << 3)
#define AST_VMINDEX(IDX)				(IDX << 15)
#define AST_RGN_ENABLE					(1 << 0)
#define AST_RGN_OFFSET					0x20

struct acast_region {
	u64	rgn;
	u64	rgn_ctrl;
	u64	slave;
	u64	size;
	u64	master;
};

#define ACAST_REGIONS	1
#define ACAST_BASE	0x02994000
#define ACAST_SIZE	0x1FFF

#define ACAST_GLOBAL_CTRL_VAL		0x07b80009
#define ACAST_STREAMID_CTL_0_VAL	0x00007f01
#define ACAST_STREAMID_CTL_1_VAL	0x00001e01

static struct acast_region acast_regions[ACAST_REGIONS] = {
	{
		0x2,
		0x00008008,
		0x40000000,
		0x20000000,
		0x40000000,
	},
};

static void __iomem *acast_base;

static inline void acast_write(void __iomem *acast, u32 reg, u32 val)
{
	writel(val, acast + reg);
}

static inline u32 __maybe_unused acast_read(void __iomem *acast, u32 reg)
{
	return readl(acast + reg);
}

static inline u32 acast_rgn_reg(u32 rgn, u32 reg)
{
	return rgn * AST_RGN_OFFSET + reg;
}

static void tegra18x_acast_map(void __iomem *acast, u64 rgn, u64 rgn_ctrl,
			       u64 slave, u64 size, u64 master)
{
	u32 val;

	val = (slave & AST_LO_MASK) | AST_RGN_ENABLE;
	acast_write(acast,
		    acast_rgn_reg(rgn, AST_RGN_SLAVE_BASE_LO), val);
	val = slave >> AST_LO_SHIFT;
	acast_write(acast,
		    acast_rgn_reg(rgn, AST_RGN_SLAVE_BASE_HI), val);

	val = master & AST_LO_MASK;
	acast_write(acast,
		    acast_rgn_reg(rgn, AST_RGN_MASTER_BASE_LO), val);
	val = master >> AST_LO_SHIFT;
	acast_write(acast,
		    acast_rgn_reg(rgn, AST_RGN_MASTER_BASE_HI), val);

	val = ((size - 1) & AST_PAGE_MASK) & AST_LO_MASK;
	acast_write(acast,
		    acast_rgn_reg(rgn, AST_RGN_MASK_BASE_LO), val);
	val = (size - 1) >> AST_LO_SHIFT;
	acast_write(acast,
		    acast_rgn_reg(rgn, AST_RGN_MASK_BASE_HI), val);

	acast_write(acast,
		    acast_rgn_reg(rgn, AST_RGN_CONTOL), rgn_ctrl);
}

int nvadsp_acast_init(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int i;

	if (!acast_base) {
		acast_base = devm_ioremap_nocache(dev, ACAST_BASE, ACAST_SIZE);
		if (IS_ERR_OR_NULL(acast_base)) {
			dev_err(dev, "failed to map ACAST\n");
			return PTR_ERR(acast_base);
		}
	}

	for (i = 0; i < ACAST_REGIONS; i++) {
		tegra18x_acast_map(acast_base,
				   acast_regions[i].rgn,
				   acast_regions[i].rgn_ctrl,
				   acast_regions[i].slave,
				   acast_regions[i].size,
				   acast_regions[i].master);

		dev_dbg(dev,
			"i:%d rgn:0x%llx rgn_ctrl:0x%llx "
			"slave:0x%llx size:0x%llx master:0x%llx\n",
			i, acast_regions[i].rgn, acast_regions[i].rgn_ctrl,
			acast_regions[i].slave, acast_regions[i].size,
			acast_regions[i].master);
	}

	return 0;
}
