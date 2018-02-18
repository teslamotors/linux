/*
 * board-common.c: Implement function which is common across
 * different boards.
 *
 * Copyright (c) 2011-2015, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/clk.h>
#include <linux/of.h>
#include <linux/serial_8250.h>
#include <linux/clk/tegra.h>
#include <linux/tegra-soc.h>

#include <mach/edp.h>

#include "board.h"
#include "board-common.h"
#include "devices.h"
#include <linux/platform/tegra/clock.h>
#include <linux/platform/tegra/dvfs.h>
#include <linux/platform/tegra/cpu-tegra.h>

static struct platform_device vibrator_device = {
	.name = "tegra-vibrator",
	.id = -1,
};

int tegra_vibrator_init(void)
{
	return platform_device_register(&vibrator_device);
}

void tegra_platform_edp_init(struct thermal_trip_info *trips,
				int *num_trips, int margin)
{
#ifdef CONFIG_TEGRA_EDP_LIMITS
	struct thermal_trip_info *trip_state;
	const int cpu_edp_temps[] = { /* degree celcius (C) */
		23, 40, 50, 60, 70, 74, 78, 82, 86, 90, 94, 98, 102,
	};
	int i;

	if (!trips || !num_trips)
		return;

	if (ARRAY_SIZE(cpu_edp_temps) > MAX_THROT_TABLE_SIZE)
		BUG();

	for (i = 0; i < ARRAY_SIZE(cpu_edp_temps) - 1; i++) {
		trip_state = &trips[*num_trips];

		trip_state->cdev_type = "cpu_edp";
		trip_state->trip_temp =
			(cpu_edp_temps[i] * 1000) - margin;
		trip_state->trip_type = THERMAL_TRIP_ACTIVE;
		trip_state->upper = trip_state->lower = i + 1;

		(*num_trips)++;

		if (*num_trips >= THERMAL_MAX_TRIPS)
			BUG();
	}
#endif
}

#define C_TO_K(c) (c+273)
void tegra_platform_gpu_edp_init(struct thermal_trip_info *trips,
				int *num_trips, int margin)
{
#ifdef CONFIG_TEGRA_GPU_EDP
	struct thermal_trip_info *trip_state;
	int temps[] = { /* degree celcius (C) */
		20, 50, 70, 75, 80, 85, 90, 95, 100, 105,
	};
	int n_temps = ARRAY_SIZE(temps);
	int i;

	if (!trips || !num_trips)
		return;

	if (n_temps > MAX_THROT_TABLE_SIZE)
		BUG();

	for (i = 0; i < n_temps-1; i++) {
		trip_state = &trips[*num_trips];

		trip_state->cdev_type = "gpu_edp";
		trip_state->trip_temp =
			(temps[i] * 1000) - margin;
		trip_state->trip_type = THERMAL_TRIP_ACTIVE;
		trip_state->upper = trip_state->lower =
			C_TO_K(temps[i + 1]);

		(*num_trips)++;

		if (*num_trips >= THERMAL_MAX_TRIPS)
			BUG();
	}
#endif
}

static void tegra_add_trip_points(struct thermal_trip_info *trips,
				int *num_trips,
				struct tegra_cooling_device *cdev_data)
{
	int i;
	struct thermal_trip_info *trip_state;

	if (!trips || !num_trips || !cdev_data)
		return;

	if (*num_trips + cdev_data->trip_temperatures_num > THERMAL_MAX_TRIPS) {
		WARN(1, "%s: cooling device %s has too many trips\n",
		     __func__, cdev_data->cdev_type);
		return;
	}

	for (i = 0; i < cdev_data->trip_temperatures_num; i++) {
		trip_state = &trips[*num_trips];

		trip_state->cdev_type = cdev_data->cdev_type;
		trip_state->trip_temp = cdev_data->trip_temperatures[i] * 1000;
		trip_state->trip_type = THERMAL_TRIP_ACTIVE;
		trip_state->upper = trip_state->lower = i + 1;
		trip_state->hysteresis = 1000;
		trip_state->mask = 1;

		(*num_trips)++;
	}
}

void tegra_add_all_vmin_trips(struct thermal_trip_info *trips, int *num_trips)
{
	tegra_add_trip_points(trips, num_trips, tegra_dvfs_get_cpu_vmin_cdev());
	tegra_add_trip_points(trips, num_trips,
			      tegra_dvfs_get_core_vmin_cdev());
	tegra_add_trip_points(trips, num_trips, tegra_dvfs_get_gpu_vmin_cdev());
}

void tegra_add_cpu_vmin_trips(struct thermal_trip_info *trips, int *num_trips)
{
	tegra_add_trip_points(trips, num_trips,
					tegra_dvfs_get_cpu_vmin_cdev());
}

void tegra_add_gpu_vmin_trips(struct thermal_trip_info *trips, int *num_trips)
{
	tegra_add_trip_points(trips, num_trips,
					tegra_dvfs_get_gpu_vmin_cdev());
}

void tegra_add_core_vmin_trips(struct thermal_trip_info *trips, int *num_trips)
{
	tegra_add_trip_points(trips, num_trips,
					tegra_dvfs_get_core_vmin_cdev());
}

void tegra_add_cpu_vmax_trips(struct thermal_trip_info *trips, int *num_trips)
{
	tegra_add_trip_points(trips, num_trips, tegra_dvfs_get_cpu_vmax_cdev());
}

void tegra_add_core_edp_trips(struct thermal_trip_info *trips, int *num_trips)
{
	tegra_add_trip_points(trips, num_trips, tegra_core_edp_get_cdev());
}

void tegra_add_tgpu_trips(struct thermal_trip_info *trips, int *num_trips)
{
	tegra_add_trip_points(trips, num_trips, tegra_dvfs_get_gpu_vts_cdev());
}

void tegra_add_vc_trips(struct thermal_trip_info *trips, int *num_trips)
{
	tegra_add_trip_points(trips, num_trips, tegra_vc_get_cdev());
}
void tegra_add_core_vmax_trips(struct thermal_trip_info *trips, int *num_trips)
{
	tegra_add_trip_points(trips, num_trips,
			      tegra_dvfs_get_core_vmax_cdev());
}

void tegra_add_cpu_clk_switch_trips(struct thermal_trip_info *trips,
							int *num_trips)
{
	tegra_add_trip_points(trips, num_trips,
				 tegra_dvfs_get_cpu_clk_switch_cdev());
}

struct pinctrl_dev *tegra_get_pinctrl_device_handle(void)
{
	static struct pinctrl_dev *pctl_dev = NULL;

	if (pctl_dev)
		return pctl_dev;

	pctl_dev = pinctrl_get_dev_from_of_compatible(
					"nvidia,tegra124-pinmux");
	if (!pctl_dev)
		pr_err("%s(): ERROR: No Tegra pincontrol driver\n", __func__);

	return pctl_dev;
}

static const char *uart_dt_nodes[] = {
	"/serial@70006000",
	"/serial@70006040",
	"/serial@70006200",
	"/serial@70006300",
};

bool tegra_is_port_available_from_dt(int uart_port)
{
	struct device_node *np;

	if ((uart_port < 0) || (uart_port > 3)) {
		pr_err("%s(): ERROR: INvalid uart port %d\n",
			__func__, uart_port);
		return false;
	}

	np = of_find_node_by_path(uart_dt_nodes[uart_port]);
	if (!np) {
		pr_err("%s(): ERROR: node %s not found\n",
			__func__, uart_dt_nodes[uart_port]);
		return false;
	}
	if (of_device_is_available(np)) {
		pr_info("%s(): Node %s enabled from DT\n",
			__func__, uart_dt_nodes[uart_port]);
		return true;
	}
	pr_info("%s(): Node %s disabled from DT\n",
			__func__, uart_dt_nodes[uart_port]);
	return false;
}
