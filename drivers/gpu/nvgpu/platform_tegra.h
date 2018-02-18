/*
 * GK20A Platform (SoC) Interface
 *
 * Copyright (c) 2014-2016, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef _NVGPU_PLATFORM_TEGRA_H_
#define _NVGPU_PLATFORM_TEGRA_H_

#include <linux/types.h>

struct platform_device;
struct gr_ctx_buffer_desc;

int gk20a_tegra_secure_alloc(struct device *dev,
			     struct gr_ctx_buffer_desc *desc,
			     size_t size);
int gk20a_tegra_secure_page_alloc(struct device *dev);

#endif
