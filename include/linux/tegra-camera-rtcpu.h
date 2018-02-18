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

#ifndef _LINUX_TEGRA_CAMERA_RTCPU_H_
#define _LINUX_TEGRA_CAMERA_RTCPU_H_

#include <linux/types.h>

struct device;

void tegra_camrtc_ready(struct device *dev);
int tegra_camrtc_set_halt(struct device *dev, bool halt);
int tegra_camrtc_get_halt(struct device *dev, bool *halt);
int tegra_camrtc_reset(struct device *dev);
int tegra_camrtc_boot(struct device *dev);
int tegra_camrtc_ivc_setup_ready(struct device *dev);
ssize_t tegra_camrtc_print_version(struct device *dev, char *buf, size_t size);
int tegra_camrtc_start(struct device *dev);
int tegra_camrtc_stop(struct device *dev);
int tegra_camrtc_restore(struct device *dev);
bool tegra_camrtc_is_rtcpu_alive(struct device *dev);
#define TEGRA_CAMRTC_VERSION_LEN 128

int tegra_camrtc_command(struct device *dev, u32 command, long timeout);

#endif
