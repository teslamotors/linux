/*
 * arch/arm/mach-tegra/board-norrin-power.c
 *
 * Copyright (c) 2013-2014 NVIDIA Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/i2c.h>
#include <linux/pda_power.h>
#include <linux/platform_device.h>
#include <linux/resource.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/pid_thermal_gov.h>
#include <linux/tegra-fuse.h>
#include <linux/tegra-pmc.h>

#include <asm/mach-types.h>

#include <mach/irqs.h>
#include <mach/edp.h>
#include <linux/tegra_soctherm.h>

#include <linux/platform/tegra/cpu-tegra.h>
#include "pm.h"
#include "tegra-board-id.h"
#include "board.h"
#include "gpio-names.h"
#include "board-common.h"
#include "board-ardbeg.h"
#include <linux/platform/tegra/tegra_cl_dvfs.h>
#include "devices.h"
#include "iomap.h"

static struct tegra_suspend_platform_data norrin_suspend_data = {
	.cpu_timer	= 2000,
	.cpu_off_timer	= 2000,
	.suspend_mode	= TEGRA_SUSPEND_LP0,
	.core_timer	= 0x7e7e,
	.core_off_timer = 2000,
	.corereq_high	= true,
	.sysclkreq_high	= true,
	.cpu_lp2_min_residency = 1000,
};

#ifdef CONFIG_ARCH_TEGRA_HAS_CL_DVFS
/* board parameters for cpu dfll */
static struct tegra_cl_dvfs_cfg_param norrin_cl_dvfs_param = {
	.sample_rate = 12500,

	.force_mode = TEGRA_CL_DVFS_FORCE_FIXED,
	.cf = 10,
	.ci = 0,
	.cg = 2,

	.droop_cut_value = 0xF,
	.droop_restore_ramp = 0x0,
	.scale_out_ramp = 0x0,
};

/* Norrin: fixed 10mV steps from 700mV to 1400mV */
#define PMU_CPU_VDD_MAP_SIZE ((1400000 - 700000) / 10000 + 1)
static struct voltage_reg_map pmu_cpu_vdd_map[PMU_CPU_VDD_MAP_SIZE];
static inline void fill_reg_map(struct board_info *board_info)
{
	int i;
	u32 reg_init_value = 0x0a;

	if ((board_info->board_id == BOARD_PM374) &&
			(board_info->fab == 0x01))
		reg_init_value = 0x1e;

	for (i = 0; i < PMU_CPU_VDD_MAP_SIZE; i++) {
		pmu_cpu_vdd_map[i].reg_value = i + reg_init_value;
		pmu_cpu_vdd_map[i].reg_uV = 700000 + 10000 * i;
	}
}

static struct tegra_cl_dvfs_platform_data norrin_cl_dvfs_data = {
	.dfll_clk_name = "dfll_cpu",
	.pmu_if = TEGRA_CL_DVFS_PMU_I2C,
	.u.pmu_i2c = {
		.fs_rate = 400000,
		.slave_addr = 0x80,
		.reg = 0x00,
	},
	.vdd_map = pmu_cpu_vdd_map,
	.vdd_map_size = PMU_CPU_VDD_MAP_SIZE,

	.cfg_param = &norrin_cl_dvfs_param,
};


static const struct of_device_id dfll_of_match[] = {
	{ .compatible	= "nvidia,tegra124-dfll", },
	{ .compatible	= "nvidia,tegra132-dfll", },
	{ },
};

static int __init norrin_cl_dvfs_init(void)
{
	struct board_info board_info;
	struct device_node *dn = of_find_matching_node(NULL, dfll_of_match);

	/*
	 * Norrin platforms maybe used with different DT variants. Some of them
	 * include DFLL data in DT, some - not. Check DT here, and continue with
	 * platform device registration only if DT DFLL node is not present.
	 */
	if (dn) {
		bool available = of_device_is_available(dn);
		of_node_put(dn);
		if (available)
			return 0;
	}

	tegra_get_board_info(&board_info);

	fill_reg_map(&board_info);
	norrin_cl_dvfs_data.flags = TEGRA_CL_DVFS_DYN_OUTPUT_CFG;
	if (board_info.board_id == BOARD_PM374)
		norrin_cl_dvfs_data.flags |= TEGRA_CL_DVFS_DATA_NEW_NO_USE;
	tegra_cl_dvfs_device.dev.platform_data = &norrin_cl_dvfs_data;
	platform_device_register(&tegra_cl_dvfs_device);

	return 0;
}
#endif

int __init norrin_regulator_init(void)
{

#ifdef CONFIG_ARCH_TEGRA_HAS_CL_DVFS
	norrin_cl_dvfs_init();
#endif
	return 0;
}

int __init norrin_suspend_init(void)
{
	tegra_init_suspend(&norrin_suspend_data);
	return 0;
}

static struct pid_thermal_gov_params soctherm_pid_params = {
	.max_err_temp = 9000,
	.max_err_gain = 1000,

	.gain_p = 1000,
	.gain_d = 0,

	.up_compensation = 20,
	.down_compensation = 20,
};

static struct thermal_zone_params soctherm_tzp = {
	.governor_name = "pid_thermal_gov",
	.governor_params = &soctherm_pid_params,
};

static struct tegra_thermtrip_pmic_data tpdata_as3722 = {
	.reset_tegra = 1,
	.pmu_16bit_ops = 0,
	.controller_type = 0,
	.pmu_i2c_addr = 0x40,
	.i2c_controller_id = 4,
	.poweroff_reg_addr = 0x36,
	.poweroff_reg_data = 0x2,
};

/* This is really v2 rev of the norrin_soctherm_data structure */
static struct soctherm_platform_data norrin_soctherm_data = {
	.therm = {
		[THERM_CPU] = {
			.zone_enable = true,
			.passive_delay = 1000,
			.hotspot_offset = 10000,
			.num_trips = 3,
			.trips = {
				{
					.cdev_type = "tegra-shutdown",
					.trip_temp = 105000,
					.trip_type = THERMAL_TRIP_CRITICAL,
					.upper = THERMAL_NO_LIMIT,
					.lower = THERMAL_NO_LIMIT,
				},
				{
					.cdev_type = "tegra-heavy",
					.trip_temp = 102000,
					.trip_type = THERMAL_TRIP_HOT,
					.upper = THERMAL_NO_LIMIT,
					.lower = THERMAL_NO_LIMIT,
				},
				{
					.cdev_type = "cpu-balanced",
					.trip_temp = 92000,
					.trip_type = THERMAL_TRIP_PASSIVE,
					.upper = THERMAL_NO_LIMIT,
					.lower = THERMAL_NO_LIMIT,
				},
			},
			.tzp = &soctherm_tzp,
		},
		[THERM_GPU] = {
			.zone_enable = true,
			.passive_delay = 1000,
			.hotspot_offset = 5000,
			.num_trips = 3,
			.trips = {
				{
					.cdev_type = "tegra-shutdown",
					.trip_temp = 101000,
					.trip_type = THERMAL_TRIP_CRITICAL,
					.upper = THERMAL_NO_LIMIT,
					.lower = THERMAL_NO_LIMIT,
				},
				{
					.cdev_type = "tegra-heavy",
					.trip_temp = 99000,
					.trip_type = THERMAL_TRIP_HOT,
					.upper = THERMAL_NO_LIMIT,
					.lower = THERMAL_NO_LIMIT,
				},
				{
					.cdev_type = "gpu-balanced",
					.trip_temp = 89000,
					.trip_type = THERMAL_TRIP_PASSIVE,
					.upper = THERMAL_NO_LIMIT,
					.lower = THERMAL_NO_LIMIT,
				},
			},
			.tzp = &soctherm_tzp,
		},
		[THERM_MEM] = {
			.zone_enable = true,
			.num_trips = 1,
			.trips = {
				{
					.cdev_type = "tegra-shutdown",
					.trip_temp = 101000, /* = GPU shut */
					.trip_type = THERMAL_TRIP_CRITICAL,
					.upper = THERMAL_NO_LIMIT,
					.lower = THERMAL_NO_LIMIT,
				},
			},
			.tzp = &soctherm_tzp,
		},
		[THERM_PLL] = {
			.zone_enable = true,
			.tzp = &soctherm_tzp,
		},
	},
	.throttle = {
		[THROTTLE_HEAVY] = {
			.priority = 100,
			.devs = {
				[THROTTLE_DEV_CPU] = {
					.enable = true,
					.depth = 80,
			/* see @PSKIP_CONFIG_NOTE in board-ardbeg-power.c */
					.throttling_depth = "heavy_throttling",
				},
				[THROTTLE_DEV_GPU] = {
					.enable = true,
					.throttling_depth = "heavy_throttling",
				},
			},
		},
	},
};

/* Only the diffs from norrin_soctherm_data structure */
static struct soctherm_platform_data norrin_v1_soctherm_data = {
	.therm = {
		[THERM_CPU] = {
			.zone_enable = true,
			.passive_delay = 1000,
			.hotspot_offset = 10000,
		},
		[THERM_PLL] = {
			.zone_enable = true,
			.passive_delay = 1000,
			.num_trips = 3,
			.trips = {
				{
					.cdev_type = "tegra-shutdown",
					.trip_temp = 97000,
					.trip_type = THERMAL_TRIP_CRITICAL,
					.upper = THERMAL_NO_LIMIT,
					.lower = THERMAL_NO_LIMIT,
				},
				{
					.cdev_type = "tegra-heavy",
					.trip_temp = 94000,
					.trip_type = THERMAL_TRIP_HOT,
					.upper = THERMAL_NO_LIMIT,
					.lower = THERMAL_NO_LIMIT,
				},
				{
					.cdev_type = "cpu-balanced",
					.trip_temp = 84000,
					.trip_type = THERMAL_TRIP_PASSIVE,
					.upper = THERMAL_NO_LIMIT,
					.lower = THERMAL_NO_LIMIT,
				},
			},
			.tzp = &soctherm_tzp,
		},
	},
};

int __init norrin_soctherm_init(void)
{
	const int t13x_cpu_edp_temp_margin = 5000,
		t13x_gpu_edp_temp_margin = 6000;
	int cp_rev, ft_rev;
	struct board_info pmu_board_info;
	struct board_info board_info;
	enum soctherm_therm_id therm_cpu = THERM_CPU;

	tegra_get_board_info(&board_info);

	cp_rev = tegra_fuse_calib_base_get_cp(NULL, NULL);
	ft_rev = tegra_fuse_calib_base_get_ft(NULL, NULL);

	if (cp_rev) {
		/* ATE rev is Old or Mid - use PLLx sensor only */
		norrin_soctherm_data.therm[THERM_CPU] =
			norrin_v1_soctherm_data.therm[THERM_CPU];
		norrin_soctherm_data.therm[THERM_PLL] =
			norrin_v1_soctherm_data.therm[THERM_PLL];
		therm_cpu = THERM_PLL; /* override CPU with PLL zone */
	}

	/* do this only for supported CP,FT fuses */
	if ((cp_rev >= 0) && (ft_rev >= 0)) {
		tegra_platform_edp_init(
			norrin_soctherm_data.therm[therm_cpu].trips,
			&norrin_soctherm_data.therm[therm_cpu].num_trips,
			t13x_cpu_edp_temp_margin);
		tegra_platform_gpu_edp_init(
			norrin_soctherm_data.therm[THERM_GPU].trips,
			&norrin_soctherm_data.therm[THERM_GPU].num_trips,
			t13x_gpu_edp_temp_margin);
		tegra_add_cpu_vmax_trips(
			norrin_soctherm_data.therm[therm_cpu].trips,
			&norrin_soctherm_data.therm[therm_cpu].num_trips);
		tegra_add_tgpu_trips(
			norrin_soctherm_data.therm[THERM_GPU].trips,
			&norrin_soctherm_data.therm[THERM_GPU].num_trips);
		tegra_add_core_vmax_trips(
			norrin_soctherm_data.therm[THERM_PLL].trips,
			&norrin_soctherm_data.therm[THERM_PLL].num_trips);
	}

	if (board_info.board_id == BOARD_PM374 ||
		board_info.board_id == BOARD_PM375 ||
		board_info.board_id == BOARD_E1971 ||
		board_info.board_id == BOARD_E1991) {
		tegra_add_cpu_vmin_trips(
			norrin_soctherm_data.therm[therm_cpu].trips,
			&norrin_soctherm_data.therm[therm_cpu].num_trips);
		tegra_add_gpu_vmin_trips(
			norrin_soctherm_data.therm[THERM_GPU].trips,
			&norrin_soctherm_data.therm[THERM_GPU].num_trips);
		tegra_add_core_vmin_trips(
			norrin_soctherm_data.therm[THERM_PLL].trips,
			&norrin_soctherm_data.therm[THERM_PLL].num_trips);
	}

	if (board_info.board_id == BOARD_PM375)
		tegra_add_cpu_clk_switch_trips(
			norrin_soctherm_data.therm[THERM_CPU].trips,
			&norrin_soctherm_data.therm[THERM_CPU].num_trips);
	tegra_get_pmu_board_info(&pmu_board_info);

	if ((pmu_board_info.board_id == BOARD_PM374) ||
		(pmu_board_info.board_id == BOARD_PM375))
		norrin_soctherm_data.tshut_pmu_trip_data = &tpdata_as3722;
	else
		pr_warn("soctherm THERMTRIP not supported on PMU (BOARD_P%d)\n",
			pmu_board_info.board_id);

	return tegra_soctherm_init(&norrin_soctherm_data);
}
