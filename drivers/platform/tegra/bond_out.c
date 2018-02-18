/*
 * drivers/platform/tegra/bond_out.c
 *
 * Copyright (C) 2014 NVIDIA Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#include <linux/types.h>
#include <linux/io.h>
#include <linux/tegra-soc.h>
#include "iomap.h"

#define OFFSET_BOND_OUT_L 0x70
#define OFFSET_BOND_OUT_H 0x74
#define OFFSET_BOND_OUT_U 0x78
#define OFFSET_BOND_OUT_V 0x390
#define OFFSET_BOND_OUT_W 0x394
#define OFFSET_BOND_OUT_X 0x398
#define OFFSET_BOND_OUT_Y 0x39c

EXPORT_SYMBOL(tegra_bonded_out_dev);

bool tegra_bonded_out_dev(enum tegra_bondout_dev dev)
{
	void __iomem *clk_base = IO_ADDRESS(TEGRA_CLK_RESET_BASE);
	u32 index, offset, reg;
	u32 bondout_offset[] = {
		OFFSET_BOND_OUT_L,
		OFFSET_BOND_OUT_H,
		OFFSET_BOND_OUT_U,
		OFFSET_BOND_OUT_V,
		OFFSET_BOND_OUT_W,
		OFFSET_BOND_OUT_X,
		OFFSET_BOND_OUT_Y
	};

	index = dev >> 5;
	offset = dev & 0x1f;
	reg = readl_relaxed(clk_base + bondout_offset[index]);
	return reg & (1 << offset);
}

