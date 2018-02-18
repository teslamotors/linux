/*
 * drivers/video/tegra/host/gk20a/hal_gk20a.h
 *
 * GK20A Hardware Abstraction Layer functions definitions.
 *
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __HAL_GK20A__
#define __HAL_GK20A__

#include <linux/kernel.h>

struct gk20a;

int gk20a_init_hal(struct gk20a *g);

#endif /* __HAL_GK20A__ */
