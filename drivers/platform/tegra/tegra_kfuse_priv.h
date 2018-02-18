/*
 * Copyright (c) 2015, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef TEGRA_KFUSE_PRIV_H
#define TEGRA_KFUSE_PRIV_H

struct kfuse {
	struct platform_device *pdev;
	struct clk *clk;
	void __iomem *aperture;
	void *private_data;
};

struct kfuse *tegra_kfuse_get(void);
u32 tegra_kfuse_readl(struct kfuse *kfuse, unsigned long offset);
void tegra_kfuse_writel(struct kfuse *kfuse, u32 value, unsigned long offset);

#endif
