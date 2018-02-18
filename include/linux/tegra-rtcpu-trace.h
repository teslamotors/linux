/*
 * Copyright (c) 2015-2016 NVIDIA CORPORATION.  All rights reserved.
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

#ifndef _LINUX_TEGRA_RTCPU_TRACE_H_
#define _LINUX_TEGRA_RTCPU_TRACE_H_

#include <linux/types.h>

struct tegra_rtcpu_trace;

struct tegra_rtcpu_trace *tegra_rtcpu_trace_create(struct device *dev,
	struct tegra_ast *ast, u32 sid);
void tegra_rtcpu_trace_destroy(struct tegra_rtcpu_trace *tracer);

#endif
