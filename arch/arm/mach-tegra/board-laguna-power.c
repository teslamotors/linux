/*
 * arch/arm/mach-tegra/board-laguna-power.c
 *
 * Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.
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
#include <linux/regulator/machine.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/fixed.h>
#include <linux/gpio.h>
#include <linux/regulator/userspace-consumer.h>
#include <linux/pid_thermal_gov.h>
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
#include "board-pmu-defines.h"
#include "board-ardbeg.h"
#include <linux/platform/tegra/tegra_cl_dvfs.h>
#include "devices.h"
#include "iomap.h"

static struct tegra_suspend_platform_data laguna_suspend_data = {
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
static struct tegra_cl_dvfs_cfg_param laguna_cl_dvfs_param = {
	.sample_rate = 12500,

	.force_mode = TEGRA_CL_DVFS_FORCE_FIXED,
	.cf = 10,
	.ci = 0,
	.cg = 2,

	.droop_cut_value = 0xF,
	.droop_restore_ramp = 0x0,
	.scale_out_ramp = 0x0,
};
#endif

/* Laguna: fixed 10mV steps from 700mV to 1400mV */
#define PMU_CPU_VDD_MAP_SIZE ((1400000 - 700000) / 10000 + 1)
static struct voltage_reg_map pmu_cpu_vdd_map[PMU_CPU_VDD_MAP_SIZE];
static inline void fill_reg_map(void)
{
	int i;
	u32 reg_init_value = 0x0a;
	struct board_info board_info;

	tegra_get_board_info(&board_info);
	if (board_info.board_id == BOARD_PM375 ||
	((board_info.board_id == BOARD_PM359) &&
	((board_info.sku >= 0x0003) ||
	((board_info.sku == 0x0002) && (board_info.major_revision == 'B')) ||
	((board_info.sku == 0x0001) && (board_info.major_revision == 'C')) ||
	((board_info.sku == 0x0000) && (board_info.major_revision == 'C')))))
		reg_init_value = 0x1e;

	for (i = 0; i < PMU_CPU_VDD_MAP_SIZE; i++) {
		pmu_cpu_vdd_map[i].reg_value = i + reg_init_value;
		pmu_cpu_vdd_map[i].reg_uV = 700000 + 10000 * i;
	}
}


#ifdef CONFIG_ARCH_TEGRA_HAS_CL_DVFS
static struct tegra_cl_dvfs_platform_data laguna_cl_dvfs_data = {
	.dfll_clk_name = "dfll_cpu",
	.pmu_if = TEGRA_CL_DVFS_PMU_I2C,
	.u.pmu_i2c = {
		.fs_rate = 400000,
		.slave_addr = 0x80,
		.reg = 0x00,
	},
	.vdd_map = pmu_cpu_vdd_map,
	.vdd_map_size = PMU_CPU_VDD_MAP_SIZE,

	.cfg_param = &laguna_cl_dvfs_param,
};

static const struct of_device_id dfll_of_match[] = {
	{ .compatible	= "nvidia,tegra124-dfll", },
	{ .compatible	= "nvidia,tegra132-dfll", },
	{ },
};

static int __init laguna_cl_dvfs_init(void)
{
	struct device_node *dn = of_find_matching_node(NULL, dfll_of_match);

	/*
	 * Laguna platforms maybe used with different DT variants. Some of them
	 * include DFLL data in DT, some - not. Check DT here, and continue with
	 * platform device registration only if DT DFLL node is not present.
	 */
	if (dn) {
		bool available = of_device_is_available(dn);
		of_node_put(dn);
		if (available)
			return 0;
	}

	fill_reg_map();
	laguna_cl_dvfs_data.flags = TEGRA_CL_DVFS_DYN_OUTPUT_CFG;
	tegra_cl_dvfs_device.dev.platform_data = &laguna_cl_dvfs_data;
	platform_device_register(&tegra_cl_dvfs_device);

	return 0;
}
#endif

int __init laguna_regulator_init(void)
{

#ifdef CONFIG_ARCH_TEGRA_HAS_CL_DVFS
	laguna_cl_dvfs_init();
#endif
	return 0;
}

int __init laguna_suspend_init(void)
{
	tegra_init_suspend(&laguna_suspend_data);
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

static struct soctherm_platform_data laguna_soctherm_data = {
	.therm = {
		[THERM_CPU] = {
			.zone_enable = true,
			.passive_delay = 1000,
			.hotspot_offset = 6000,
			.num_trips = 3,
			.trips = {
				{
					.cdev_type = "tegra-shutdown",
					.trip_temp = 103000,
					.trip_type = THERMAL_TRIP_CRITICAL,
					.upper = THERMAL_NO_LIMIT,
					.lower = THERMAL_NO_LIMIT,
				},
				{
					.cdev_type = "tegra-heavy",
					.trip_temp = 101000,
					.trip_type = THERMAL_TRIP_HOT,
					.upper = THERMAL_NO_LIMIT,
					.lower = THERMAL_NO_LIMIT,
				},
				{
					.cdev_type = "tegra-balanced",
					.trip_temp = 91000,
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
			.hotspot_offset = 6000,
			.num_trips = 3,
			.trips = {
				{
					.cdev_type = "tegra-shutdown",
					.trip_temp = 104000,
					.trip_type = THERMAL_TRIP_CRITICAL,
					.upper = THERMAL_NO_LIMIT,
					.lower = THERMAL_NO_LIMIT,
				},
				{
					.cdev_type = "tegra-balanced",
					.trip_temp = 92000,
					.trip_type = THERMAL_TRIP_PASSIVE,
					.upper = THERMAL_NO_LIMIT,
					.lower = THERMAL_NO_LIMIT,
				},
/*
				{
					.cdev_type = "gk20a_cdev",
					.trip_temp = 102000,
					.trip_type = THERMAL_TRIP_PASSIVE,
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
*/
			},
			.tzp = &soctherm_tzp,
		},
		[THERM_MEM] = {
			.zone_enable = true,
			.num_trips = 1,
			.trips = {
				{
					.cdev_type = "tegra-shutdown",
					.trip_temp = 104000, /* = GPU shut */
					.trip_type = THERMAL_TRIP_CRITICAL,
					.upper = THERMAL_NO_LIMIT,
					.lower = THERMAL_NO_LIMIT,
				},
			},
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
				},
				[THROTTLE_DEV_GPU] = {
					.enable = false,
					.throttling_depth = "heavy_throttling",
				},
			},
		},
	},
};

int __init laguna_soctherm_init(void)
{
	tegra_platform_edp_init(laguna_soctherm_data.therm[THERM_CPU].trips,
			&laguna_soctherm_data.therm[THERM_CPU].num_trips,
			7000); /* edp temperature margin */
	tegra_platform_gpu_edp_init(
			laguna_soctherm_data.therm[THERM_GPU].trips,
			&laguna_soctherm_data.therm[THERM_GPU].num_trips,
			7000);
	tegra_add_cpu_vmax_trips(laguna_soctherm_data.therm[THERM_CPU].trips,
			&laguna_soctherm_data.therm[THERM_CPU].num_trips);
	tegra_add_tgpu_trips(laguna_soctherm_data.therm[THERM_GPU].trips,
			&laguna_soctherm_data.therm[THERM_GPU].num_trips);
	tegra_add_vc_trips(laguna_soctherm_data.therm[THERM_CPU].trips,
			&laguna_soctherm_data.therm[THERM_CPU].num_trips);
	tegra_add_core_vmax_trips(laguna_soctherm_data.therm[THERM_PLL].trips,
			&laguna_soctherm_data.therm[THERM_PLL].num_trips);

	return tegra_soctherm_init(&laguna_soctherm_data);
}
