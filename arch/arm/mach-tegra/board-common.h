/*
 * board-common.h: Common function API declaration for all board files.
 *
 * Copyright (c) 2012-2014, NVIDIA CORPORATION.  All rights reserved.
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __MACH_TEGRA_BOARD_COMMON_H
#define __MACH_TEGRA_BOARD_COMMON_H

#include <linux/thermal.h>
#include <linux/platform_data/thermal_sensors.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/consumer.h>

int tegra_vibrator_init(void);

void tegra_platform_edp_init(struct thermal_trip_info *trips,
					int *num_trips, int margin);
void tegra_platform_gpu_edp_init(struct thermal_trip_info *trips,
					int *num_trips, int margin);

void tegra_add_all_vmin_trips(struct thermal_trip_info *trips, int *num_trips);
void tegra_add_cpu_vmin_trips(struct thermal_trip_info *trips, int *num_trips);
void tegra_add_gpu_vmin_trips(struct thermal_trip_info *trips, int *num_trips);
void tegra_add_core_vmin_trips(struct thermal_trip_info *trips, int *num_trips);
void tegra_add_cpu_vmax_trips(struct thermal_trip_info *trips, int *num_trips);
void tegra_add_core_edp_trips(struct thermal_trip_info *trips, int *num_trips);
void tegra_add_tgpu_trips(struct thermal_trip_info *trips, int *num_trips);
void tegra_add_cpu_clk_switch_trips(struct thermal_trip_info *trips,
					int *num_trips);

void tegra_add_vc_trips(struct thermal_trip_info *trips, int *num_trips);
void tegra_add_core_vmax_trips(struct thermal_trip_info *trips, int *num_trips);
struct pinctrl_dev *tegra_get_pinctrl_device_handle(void);
bool tegra_is_port_available_from_dt(int uart_port);
#endif
