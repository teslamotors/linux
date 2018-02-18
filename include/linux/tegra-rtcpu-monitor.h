/*
 * Copyright (c) 2015-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef INCLUDE_RTCPU_MONITOR_H
#define INCLUDE_RTCPU_MONITOR_H

struct device;
struct tegra_camrtc_mon;

int tegra_camrtc_mon_restore_rtcpu(struct tegra_camrtc_mon *);
struct tegra_camrtc_mon *tegra_camrtc_mon_create(struct device *);
int tegra_cam_rtcpu_mon_destroy(struct tegra_camrtc_mon *);

#endif /* INCLUDE_RTCPU_MONITOR_H */
