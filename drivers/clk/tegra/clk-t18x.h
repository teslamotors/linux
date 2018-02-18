/*
 * Copyright (c) 2015, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __TEGRA186_CLK_H
#define __TEGRA186_CLK_H

/**
 * struct tegra_bpmp_clk
 *
 * @hw:		handle between common and hardware-specific interfaces
 * @clk_num:	bpmp clk identifier
 */
struct tegra_clk_bpmp {
	struct clk_hw	hw;
	int		clk_num;
	int		num_parents;
	int		parent;
	int		parent_ids[0];
};

#define to_clk_bpmp(_hw) container_of(_hw, struct tegra_clk_bpmp, hw)

struct clk *tegra_clk_register_bpmp(const char *name, int parent,
		const char **parent_names, int *parent_ids, int num_parents,
		unsigned long flags, int clk_num, int bpmp_flags);

struct clk **tegra_bpmp_clk_init(struct device_node *np);

#endif
