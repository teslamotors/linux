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

#ifndef TEGRA18_KFUSE_PRIV_H
#define TEGRA18_KFUSE_PRIV_H

#include <linux/platform/tegra/tegra_kfuse.h>

struct kfuse;

int tegra18_kfuse_init(struct kfuse *kfuse);
int tegra18_kfuse_deinit(struct kfuse *kfuse);

#endif
