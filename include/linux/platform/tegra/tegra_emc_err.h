/*
 * Copyright (c) 2016, NVIDIA Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#ifndef __EMC_ERR_H
#define __EMC_ERR_H

#include <linux/kernel.h>

#if defined(CONFIG_ARCH_TEGRA_18x_SOC)
void tegra_emcerr_init(struct dentry *mc_parent,
					struct platform_device *pdev);
#else
static void tegra_emcerr_init(struct dentry *mc_parent,
					struct platform_device *pdev)
{
}
#endif

#endif
