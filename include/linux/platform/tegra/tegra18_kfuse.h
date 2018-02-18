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
 */

#ifndef TEGRA18_KFUSE_H
#define TEGRA18_KFUSE_H

#include <linux/platform/tegra/tegra_kfuse.h>

struct kfuse;

#ifdef CONFIG_TEGRA_KFUSE
void tegra_kfuse_disable_sensing(void);
int tegra_kfuse_enable_sensing(void);
#else

static inline void tegra_kfuse_disable_sensing(void)
{
}

static inline int tegra_kfuse_enable_sensing(void)
{
	return -ENOSYS;
}

#endif

#endif
