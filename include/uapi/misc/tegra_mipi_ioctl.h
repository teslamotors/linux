/*
 * Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __TEGRA_MIPI_IOCTL_H
#define __TEGRA_MIPI_IOCTL_H

#include <linux/ioctl.h>
#include <linux/types.h>

#define TEGRA_MIPI_IOCTL_MAGIC 'M'

#define TEGRA_MIPI_IOCTL_BIAS_PAD_CTRL _IOW(TEGRA_MIPI_IOCTL_MAGIC, 1, u32)
#define TEGRA_MIPI_IOCTL_CAL _IOW(TEGRA_MIPI_IOCTL_MAGIC, 2, u32)

#endif
