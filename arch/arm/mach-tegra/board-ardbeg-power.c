/*
 * arch/arm/mach-tegra/board-ardbeg-power.c
 *
 * Copyright (c) 2013-2015, NVIDIA CORPORATION. All rights reserved.
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
#include <linux/platform_device.h>
#include <linux/resource.h>
#include <linux/io.h>
#include <mach/edp.h>
#include <mach/irqs.h>
#include <linux/edp.h>
#include <linux/platform_data/tegra_edp.h>
#include <linux/pid_thermal_gov.h>
#include <linux/regulator/fixed.h>
#include <linux/regulator/machine.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/regulator/tegra-dfll-bypass-regulator.h>
#include <linux/tegra-fuse.h>
#include <linux/tegra-pmc.h>
#include <linux/pinctrl/pinconf-tegra.h>

#include <asm/mach-types.h>
#include <linux/tegra_soctherm.h>

#include "pm.h"
#include <linux/platform/tegra/dvfs.h>
#include "board.h"
#include <linux/platform/tegra/common.h>
#include "tegra-board-id.h"
#include "board-pmu-defines.h"
#include "board-common.h"
#include "board-ardbeg.h"
#include "board-pmu-defines.h"
#include "devices.h"
#include "iomap.h"
#include <linux/platform/tegra/tegra_cl_dvfs.h>

#define E1735_EMULATE_E1767_SKU	1001
static u32 tegra_chip_id;
static struct tegra_suspend_platform_data ardbeg_suspend_data = {
	.cpu_timer      = 500,
	.cpu_off_timer  = 300,
	.cpu_suspend_freq = 204000,
	.suspend_mode   = TEGRA_SUSPEND_LP0,
	.core_timer     = 0x157e,
	.core_off_timer = 10,
	.corereq_high   = true,
	.sysclkreq_high = true,
	.cpu_lp2_min_residency = 1000,
	.min_residency_vmin_fmin = 1000,
	.min_residency_ncpu_fast = 8000,
	.min_residency_ncpu_slow = 5000,
	.min_residency_mclk_stop = 5000,
	.min_residency_crail = 20000,
#ifdef CONFIG_TEGRA_LP1_LOW_COREVOLTAGE
	.lp1_lowvolt_support = true,
	.i2c_base_addr = TEGRA_I2C5_BASE,
	.pmuslave_addr = 0xB0,
	.core_reg_addr = 0x33,
	.lp1_core_volt_low_cold = 0x33,
	.lp1_core_volt_low = 0x33,
	.lp1_core_volt_high = 0x42,
#endif
};

/************************ ARDBEG CL-DVFS DATA *********************/
#define E1735_CPU_VDD_MAP_SIZE		33
#define E1735_CPU_VDD_MIN_UV		675000
#define E1735_CPU_VDD_STEP_UV		18750
#define E1735_CPU_VDD_STEP_US		80
#define E1735_CPU_VDD_BOOT_UV		1000000
#define ARDBEG_DEFAULT_CVB_ALIGNMENT	10000

#define E2141_CPU_VDD_MIN_UV		703000
#define E2141_CPU_VDD_STEP_UV		19200
#ifdef CONFIG_ARCH_TEGRA_HAS_CL_DVFS
/* E1735 board parameters for cpu dfll */
static struct tegra_cl_dvfs_cfg_param e1735_cl_dvfs_param = {
	.sample_rate = 50000,

	.force_mode = TEGRA_CL_DVFS_FORCE_FIXED,
	.cf = 10,
	.ci = 0,
	.cg = 2,

	.droop_cut_value = 0xF,
	.droop_restore_ramp = 0x0,
	.scale_out_ramp = 0x0,
};

/* E1735 dfll bypass device for legacy dvfs control */
static struct regulator_consumer_supply e1735_dfll_bypass_consumers[] = {
	REGULATOR_SUPPLY("vdd_cpu", NULL),
};
DFLL_BYPASS(e1735,
	    E1735_CPU_VDD_MIN_UV, E1735_CPU_VDD_STEP_UV, E1735_CPU_VDD_BOOT_UV,
	    E1735_CPU_VDD_MAP_SIZE, E1735_CPU_VDD_STEP_US, TEGRA_GPIO_PX2);

static struct tegra_cl_dvfs_platform_data e1735_cl_dvfs_data = {
	.dfll_clk_name = "dfll_cpu",
	.pmu_if = TEGRA_CL_DVFS_PMU_PWM,
	.u.pmu_pwm = {
		.pwm_rate = 12750000,
		.min_uV = E1735_CPU_VDD_MIN_UV,
		.step_uV = E1735_CPU_VDD_STEP_UV,
		.out_gpio = TEGRA_GPIO_PS5,
		.out_enable_high = false,
#ifdef CONFIG_REGULATOR_TEGRA_DFLL_BYPASS
		.dfll_bypass_dev = &e1735_dfll_bypass_dev,
#endif
	},

	.cfg_param = &e1735_cl_dvfs_param,
};

#ifdef CONFIG_REGULATOR_TEGRA_DFLL_BYPASS
static void e1735_suspend_dfll_bypass(void)
{
	__gpio_set_value(TEGRA_GPIO_PS5, 1); /* tristate external PWM buffer */
}

static void e1735_resume_dfll_bypass(void)
{
	__gpio_set_value(TEGRA_GPIO_PS5, 0); /* enable PWM buffer operations */
}

static void e1767_configure_dvfs_pwm_tristate(const char *gname, int tristate)
{
	struct pinctrl_dev *pctl_dev = NULL;
	unsigned long config;
	int ret;

	pctl_dev = tegra_get_pinctrl_device_handle();
	if (!pctl_dev)
		return;

	config = TEGRA_PINCONF_PACK(TEGRA_PINCONF_PARAM_TRISTATE, tristate);
	ret = pinctrl_set_config_for_group_name(pctl_dev, gname, config);
	if (ret < 0)
		pr_err("%s(): ERROR: Not able to set %s to TRISTATE %d: %d\n",
			__func__, gname, tristate, ret);
}

static void e1767_suspend_dfll_bypass(void)
{
	e1767_configure_dvfs_pwm_tristate("dvfs_pwm_px0", TEGRA_PIN_ENABLE);
}

static void e1767_resume_dfll_bypass(void)
{
	e1767_configure_dvfs_pwm_tristate("dvfs_pwm_px0", TEGRA_PIN_DISABLE);
}
#endif

static struct tegra_cl_dvfs_cfg_param e1733_ardbeg_cl_dvfs_param = {
	.sample_rate = 12500,

	.force_mode = TEGRA_CL_DVFS_FORCE_FIXED,
	.cf = 10,
	.ci = 0,
	.cg = 2,

	.droop_cut_value = 0xF,
	.droop_restore_ramp = 0x0,
	.scale_out_ramp = 0x0,
};

/* E1733 volatge map. Fixed 10mv steps from VDD_MIN to 1400mv */
#ifdef CONFIG_ARCH_TEGRA_13x_SOC
#define E1733_CPU_VDD_MIN	650000
#else
#define E1733_CPU_VDD_MIN	700000
#endif
#define E1733_CPU_VDD_MAP_SIZE ((1400000 - E1733_CPU_VDD_MIN) / 10000 + 1)
static struct voltage_reg_map e1733_cpu_vdd_map[E1733_CPU_VDD_MAP_SIZE];
static inline void e1733_fill_reg_map(void)
{
        int i;
        for (i = 0; i < E1733_CPU_VDD_MAP_SIZE; i++) {
		e1733_cpu_vdd_map[i].reg_value = i + 0xa;
		e1733_cpu_vdd_map[i].reg_uV = E1733_CPU_VDD_MIN + 10000 * i;
        }
}

static struct tegra_cl_dvfs_platform_data e1733_cl_dvfs_data = {
	.dfll_clk_name = "dfll_cpu",
	.pmu_if = TEGRA_CL_DVFS_PMU_I2C,
	.u.pmu_i2c = {
		.fs_rate = 400000,
		.slave_addr = 0x80,
		.reg = 0x00,
	},
	.vdd_map = e1733_cpu_vdd_map,
	.vdd_map_size = E1733_CPU_VDD_MAP_SIZE,

	.cfg_param = &e1733_ardbeg_cl_dvfs_param,
};

static void __init ardbeg_tweak_E1767_dt(void)
{
	struct device_node *dn = NULL;
	struct property *pp = NULL;

	/*
	 *  Update E1735 DT for E1767 prototype. To be removed when
	 *  E1767 is productized with its own DT.
	 */
	dn = of_find_node_with_property(dn, "pwm-1wire-buffer");
	if (dn) {
		pp = of_find_property(dn, "pwm-1wire-buffer", NULL);
		if (pp)
			pp->name = "pwm-1wire-direct";
		of_node_put(dn);
	}
	if (!dn || !pp)
		WARN(1, "Failed update DT for PMU E1767 prototype\n");
}


static int __init ardbeg_cl_dvfs_init(struct board_info *pmu_board_info)
{
	u16 pmu_board_id = pmu_board_info->board_id;
	struct tegra_cl_dvfs_platform_data *data = NULL;

	if (pmu_board_id == BOARD_E1735) {
		bool e1767 = pmu_board_info->sku == E1735_EMULATE_E1767_SKU;
		struct device_node *dn = of_find_compatible_node(
			NULL, NULL, "nvidia,tegra124-dfll");
		/*
		 * Ardbeg platforms with E1735 PMIC module maybe used with
		 * different DT variants. Some of them include CL-DVFS data
		 * in DT, some - not. Check DT here, and continue with platform
		 * device registration only if DT DFLL node is not present.
		 */
		if (dn) {
			bool available = of_device_is_available(dn);
			of_node_put(dn);

			if (available) {
				if (e1767)
					ardbeg_tweak_E1767_dt();
				return 0;
			}
		}

		data = &e1735_cl_dvfs_data;

		data->u.pmu_pwm.pinctrl_dev = tegra_get_pinctrl_device_handle();
		if (!data->u.pmu_pwm.pinctrl_dev)
			return -EINVAL;

		data->u.pmu_pwm.pwm_pingroup =
				pinctrl_get_selector_from_group_name(
					data->u.pmu_pwm.pinctrl_dev,
					"dvfs_pwm_px0");
		if (data->u.pmu_pwm.pwm_pingroup < 0) {
			pr_err("%s: Tegra pin dvfs_pwm_px0 not found\n", __func__);
			return -EINVAL;
		}

		data->u.pmu_pwm.pwm_bus = e1767 ?
			TEGRA_CL_DVFS_PWM_1WIRE_DIRECT :
			TEGRA_CL_DVFS_PWM_1WIRE_BUFFER;

		if (data->u.pmu_pwm.dfll_bypass_dev) {
			platform_device_register(
				data->u.pmu_pwm.dfll_bypass_dev);
		} else {
			(void)e1735_dfll_bypass_dev;
		}
	}


	if (pmu_board_id == BOARD_E1733) {
		e1733_fill_reg_map();
		data = &e1733_cl_dvfs_data;
	}

	if (data) {
		data->flags = TEGRA_CL_DVFS_DYN_OUTPUT_CFG;
		tegra_cl_dvfs_device.dev.platform_data = data;
		platform_device_register(&tegra_cl_dvfs_device);
	}
	return 0;
}
#else
static inline int ardbeg_cl_dvfs_init(struct board_info *pmu_board_info)
{ return 0; }
#endif

int __init ardbeg_rail_alignment_init(void)
{
	struct board_info pmu_board_info;

	tegra_get_pmu_board_info(&pmu_board_info);

#ifdef CONFIG_ARCH_TEGRA_13x_SOC
	if (of_machine_is_compatible("nvidia,e2141"))
		tegra13x_vdd_cpu_align(E2141_CPU_VDD_STEP_UV,
				       E2141_CPU_VDD_MIN_UV);
#else
	if (pmu_board_info.board_id == BOARD_E1735)
		tegra12x_vdd_cpu_align(E1735_CPU_VDD_STEP_UV,
				       E1735_CPU_VDD_MIN_UV);
	else
		tegra12x_vdd_cpu_align(ARDBEG_DEFAULT_CVB_ALIGNMENT, 0);
#endif
	return 0;
}

int __init ardbeg_regulator_init(void)
{
	struct board_info pmu_board_info;

	tegra_get_pmu_board_info(&pmu_board_info);

	switch (pmu_board_info.board_id) {
	case BOARD_E1733:
	case BOARD_E1734:
	case BOARD_E1735:
	case BOARD_E1736:
	case BOARD_E1936:
	case BOARD_E1769:
	case BOARD_P1761:
	case BOARD_P1765:
		break;
	default:
		pr_warn("PMU board id 0x%04x is not supported\n",
			pmu_board_info.board_id);
		break;
	}

	ardbeg_cl_dvfs_init(&pmu_board_info);
	return 0;
}

int __init ardbeg_suspend_init(void)
{
	struct board_info pmu_board_info;

	tegra_get_pmu_board_info(&pmu_board_info);

	if (pmu_board_info.board_id == BOARD_E1735) {
		struct tegra_suspend_platform_data *data = &ardbeg_suspend_data;
		if (pmu_board_info.sku != E1735_EMULATE_E1767_SKU) {
			data->cpu_timer = 2000;
			data->crail_up_early = true;
#ifdef CONFIG_REGULATOR_TEGRA_DFLL_BYPASS
			data->suspend_dfll_bypass = e1735_suspend_dfll_bypass;
			data->resume_dfll_bypass = e1735_resume_dfll_bypass;
		} else {
			data->suspend_dfll_bypass = e1767_suspend_dfll_bypass;
			data->resume_dfll_bypass = e1767_resume_dfll_bypass;
#endif
		}
	}

	tegra_init_suspend(&ardbeg_suspend_data);
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

static struct tegra_thermtrip_pmic_data tpdata_palmas = {
	.reset_tegra = 1,
	.pmu_16bit_ops = 0,
	.controller_type = 0,
	.pmu_i2c_addr = 0x58,
	.i2c_controller_id = 4,
	.poweroff_reg_addr = 0xa0,
	.poweroff_reg_data = 0x0,
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

static struct tegra_thermtrip_pmic_data tpdata_max77620 = {
	.reset_tegra = 1,
	.pmu_16bit_ops = 0,
	.controller_type = 0,
	.pmu_i2c_addr = 0x3c,
	.i2c_controller_id = 4,
	.poweroff_reg_addr = 0x41,
	.poweroff_reg_data = 0x80,
};

static struct soctherm_therm ardbeg_therm_pop[THERM_SIZE] = {
	[THERM_CPU] = {
		.zone_enable = true,
		.passive_delay = 1000,
		.hotspot_offset = 6000,
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
	[THERM_GPU] = {
		.zone_enable = true,
		.passive_delay = 1000,
		.hotspot_offset = 6000,
		.num_trips = 3,
		.trips = {
			{
				.cdev_type = "tegra-shutdown",
				.trip_temp = 93000,
				.trip_type = THERMAL_TRIP_CRITICAL,
				.upper = THERMAL_NO_LIMIT,
				.lower = THERMAL_NO_LIMIT,
			},
			{
				.cdev_type = "tegra-heavy",
				.trip_temp = 91000,
				.trip_type = THERMAL_TRIP_HOT,
				.upper = THERMAL_NO_LIMIT,
				.lower = THERMAL_NO_LIMIT,
			},
			{
				.cdev_type = "gpu-balanced",
				.trip_temp = 81000,
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
				.trip_temp = 93000, /* = GPU shut */
				.trip_type = THERMAL_TRIP_CRITICAL,
				.upper = THERMAL_NO_LIMIT,
				.lower = THERMAL_NO_LIMIT,
			},
		},
		.tzp = &soctherm_tzp,
	},
	[THERM_PLL] = {
		.zone_enable = true,
		.num_trips = 1,
		.trips = {
			{
				.cdev_type = "tegra-dram",
				.trip_temp = 78000,
				.trip_type = THERMAL_TRIP_ACTIVE,
				.upper = 1,
				.lower = 1,
			},
		},
		.tzp = &soctherm_tzp,
	},
};

/*
 * @PSKIP_CONFIG_NOTE: For T132, throttling config of PSKIP is no longer
 * done in soctherm registers. These settings are now done via registers in
 * denver:ccroc module which are at a different register offset. More
 * importantly, there are _only_ three levels of throttling: 'low',
 * 'medium' and 'heavy' and are selected via the 'throttling_depth' field
 * in the throttle->devs[] section of the soctherm config. Since the depth
 * specification is per device, it is necessary to manually make sure the
 * depths specified alongwith a given level are the same across all devs,
 * otherwise it will overwrite a previously set depth with a different
 * depth. We will refer to this comment at each relevant location in the
 * config sections below.
 */
static struct soctherm_platform_data ardbeg_soctherm_data = {
	.oc_irq_base = TEGRA_SOC_OC_IRQ_BASE,
	.num_oc_irqs = TEGRA_SOC_OC_NUM_IRQ,
	.therm = {
		[THERM_CPU] = {
			.zone_enable = true,
			.passive_delay = 1000,
			.hotspot_offset = 6000,
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
					.cdev_type = "cpu-balanced",
					.trip_temp = 90000,
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
					.trip_temp = 90000,
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
					/* see @PSKIP_CONFIG_NOTE */
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

/* Only the diffs from ardbeg_soctherm_data structure */
static struct soctherm_platform_data t132ref_v1_soctherm_data = {
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

/* Only the diffs from ardbeg_soctherm_data structure */
static struct soctherm_platform_data t132ref_v2_soctherm_data = {
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
	},
};

static struct soctherm_throttle battery_oc_throttle_t13x = {
	.throt_mode = BRIEF,
	.polarity = SOCTHERM_ACTIVE_LOW,
	.priority = 50,
	.intr = true,
	.alarm_cnt_threshold = 15,
	.alarm_filter = 5100000,
	.devs = {
		[THROTTLE_DEV_CPU] = {
			.enable = true,
			.depth = 50,
			/* see @PSKIP_CONFIG_NOTE */
			.throttling_depth = "low_throttling",
		},
		[THROTTLE_DEV_GPU] = {
			.enable = true,
			.throttling_depth = "medium_throttling",
		},
	},
};

static struct soctherm_throttle battery_oc_throttle_t12x = {
	.throt_mode = BRIEF,
	.polarity = SOCTHERM_ACTIVE_LOW,
	.priority = 50,
	.intr = true,
	.alarm_cnt_threshold = 15,
	.alarm_filter = 5100000,
	.devs = {
		[THROTTLE_DEV_CPU] = {
			.enable = true,
			.depth = 50,
		},
		[THROTTLE_DEV_GPU] = {
			.enable = true,
			.throttling_depth = "medium_throttling",
		},
	},
};

static struct soctherm_throttle voltmon_throttle_t13x = {
	.throt_mode = BRIEF,
	.polarity = SOCTHERM_ACTIVE_LOW,
	.priority = 50,
	.intr = true,
	.alarm_cnt_threshold = 100,
	.alarm_filter = 5100000,
	.devs = {
		[THROTTLE_DEV_CPU] = {
			.enable = true,
			/* throttle depth 75% with 3.76us ramp rate */
			.dividend = 63,
			.divisor = 255,
			.duration = 0,
			.step = 0,
			/* see @PSKIP_CONFIG_NOTE */
			.throttling_depth = "medium_throttling",
		},
		[THROTTLE_DEV_GPU] = {
			.enable = true,
			.throttling_depth = "medium_throttling",
		},
	},
};

static struct soctherm_throttle voltmon_throttle_t12x = {
	.throt_mode = BRIEF,
	.polarity = SOCTHERM_ACTIVE_LOW,
	.priority = 50,
	.intr = true,
	.alarm_cnt_threshold = 100,
	.alarm_filter = 5100000,
	.devs = {
		[THROTTLE_DEV_CPU] = {
			.enable = true,
			/* throttle depth 75% with 3.76us ramp rate */
			.dividend = 63,
			.divisor = 255,
			.duration = 0,
			.step = 0,
		},
		[THROTTLE_DEV_GPU] = {
			.enable = true,
			.throttling_depth = "medium_throttling",
		},
	},
};

int __init ardbeg_soctherm_init(void)
{
	const int t12x_edp_temp_margin = 7000,
		t13x_cpu_edp_temp_margin = 5000,
		t13x_gpu_edp_temp_margin = 6000;
	int cpu_edp_temp_margin, gpu_edp_temp_margin;
	int cp_rev, ft_rev;
	struct board_info pmu_board_info;
	struct board_info board_info;
	enum soctherm_therm_id therm_cpu = THERM_CPU;

	tegra_get_board_info(&board_info);
	tegra_chip_id = tegra_get_chip_id();

	if (board_info.board_id == BOARD_E1923 ||
			board_info.board_id == BOARD_E1922) {
		memcpy(ardbeg_soctherm_data.therm,
				ardbeg_therm_pop, sizeof(ardbeg_therm_pop));
	}

	cp_rev = tegra_fuse_calib_base_get_cp(NULL, NULL);
	ft_rev = tegra_fuse_calib_base_get_ft(NULL, NULL);

	/* Bowmore and P1761 are T132 platforms */
	if (board_info.board_id == BOARD_E1971 ||
			board_info.board_id == BOARD_P1761 ||
			board_info.board_id == BOARD_P1765 ||
			board_info.board_id == BOARD_E2141 ||
			board_info.board_id == BOARD_E1991) {
		cpu_edp_temp_margin = t13x_cpu_edp_temp_margin;
		gpu_edp_temp_margin = t13x_gpu_edp_temp_margin;
		if (!cp_rev) {
			/* ATE rev is NEW: use v2 table */
			ardbeg_soctherm_data.therm[THERM_CPU] =
				t132ref_v2_soctherm_data.therm[THERM_CPU];
			ardbeg_soctherm_data.therm[THERM_GPU] =
				t132ref_v2_soctherm_data.therm[THERM_GPU];
		} else {
			/* ATE rev is Old or Mid: use PLLx sensor only */
			ardbeg_soctherm_data.therm[THERM_CPU] =
				t132ref_v1_soctherm_data.therm[THERM_CPU];
			ardbeg_soctherm_data.therm[THERM_PLL] =
				t132ref_v1_soctherm_data.therm[THERM_PLL];
			therm_cpu = THERM_PLL; /* override CPU with PLL zone */
		}
	} else {
		cpu_edp_temp_margin = t12x_edp_temp_margin;
		gpu_edp_temp_margin = t12x_edp_temp_margin;
	}

	/* do this only for supported CP,FT fuses */
	if ((cp_rev >= 0) && (ft_rev >= 0)) {
		tegra_platform_edp_init(
			ardbeg_soctherm_data.therm[therm_cpu].trips,
			&ardbeg_soctherm_data.therm[therm_cpu].num_trips,
			cpu_edp_temp_margin);
		tegra_platform_gpu_edp_init(
			ardbeg_soctherm_data.therm[THERM_GPU].trips,
			&ardbeg_soctherm_data.therm[THERM_GPU].num_trips,
			gpu_edp_temp_margin);
		tegra_add_cpu_vmax_trips(
			ardbeg_soctherm_data.therm[therm_cpu].trips,
			&ardbeg_soctherm_data.therm[therm_cpu].num_trips);
		tegra_add_tgpu_trips(
			ardbeg_soctherm_data.therm[THERM_GPU].trips,
			&ardbeg_soctherm_data.therm[THERM_GPU].num_trips);
		tegra_add_vc_trips(
			ardbeg_soctherm_data.therm[therm_cpu].trips,
			&ardbeg_soctherm_data.therm[therm_cpu].num_trips);
		tegra_add_core_vmax_trips(
			ardbeg_soctherm_data.therm[THERM_PLL].trips,
			&ardbeg_soctherm_data.therm[THERM_PLL].num_trips);
	}

	if (board_info.board_id == BOARD_P1761 ||
		board_info.board_id == BOARD_P2267 ||
		board_info.board_id == BOARD_P1765 ||
		board_info.board_id == BOARD_E1784 ||
		board_info.board_id == BOARD_E1971 ||
		board_info.board_id == BOARD_E2141 ||
		board_info.board_id == BOARD_E1991 ||
		board_info.board_id == BOARD_E1922) {
		tegra_add_cpu_vmin_trips(
			ardbeg_soctherm_data.therm[therm_cpu].trips,
			&ardbeg_soctherm_data.therm[therm_cpu].num_trips);
		tegra_add_gpu_vmin_trips(
			ardbeg_soctherm_data.therm[THERM_GPU].trips,
			&ardbeg_soctherm_data.therm[THERM_GPU].num_trips);
		tegra_add_core_vmin_trips(
			ardbeg_soctherm_data.therm[THERM_PLL].trips,
			&ardbeg_soctherm_data.therm[THERM_PLL].num_trips);
	}

	tegra_get_pmu_board_info(&pmu_board_info);

	if ((pmu_board_info.board_id == BOARD_E1733) ||
		(pmu_board_info.board_id == BOARD_E1734))
		ardbeg_soctherm_data.tshut_pmu_trip_data = &tpdata_as3722;
	else if (pmu_board_info.board_id == BOARD_E1735 ||
		 pmu_board_info.board_id == BOARD_E1736 ||
		 pmu_board_info.board_id == BOARD_E1769 ||
		 pmu_board_info.board_id == BOARD_P1761 ||
		 pmu_board_info.board_id == BOARD_P1765 ||
		 pmu_board_info.board_id == BOARD_E1936)
		ardbeg_soctherm_data.tshut_pmu_trip_data = &tpdata_palmas;
	else if (pmu_board_info.board_id == BOARD_E2174)
		ardbeg_soctherm_data.tshut_pmu_trip_data = &tpdata_max77620;
	else
		pr_warn("soctherm THERMTRIP not supported on PMU (BOARD_E%d)\n",
			pmu_board_info.board_id);

	/* Enable soc_therm OC throttling on selected platforms */
	switch (board_info.board_id) {
	case BOARD_E2141:
	case BOARD_E1971:
		memcpy(&ardbeg_soctherm_data.throttle[THROTTLE_OC4],
		       &battery_oc_throttle_t13x,
		       sizeof(battery_oc_throttle_t13x));
		break;
	case BOARD_P1761:
	case BOARD_P2267:
	case BOARD_E1936:
	case BOARD_P1765:
		if (tegra_chip_id == TEGRA_CHIPID_TEGRA13) {
			memcpy(&ardbeg_soctherm_data.throttle[THROTTLE_OC4],
				   &battery_oc_throttle_t13x,
				   sizeof(battery_oc_throttle_t13x));
			memcpy(&ardbeg_soctherm_data.throttle[THROTTLE_OC1],
				   &voltmon_throttle_t13x,
				   sizeof(voltmon_throttle_t13x));
		} else {
			memcpy(&ardbeg_soctherm_data.throttle[THROTTLE_OC4],
				   &battery_oc_throttle_t12x,
				   sizeof(battery_oc_throttle_t12x));
			memcpy(&ardbeg_soctherm_data.throttle[THROTTLE_OC1],
				   &voltmon_throttle_t12x,
				   sizeof(voltmon_throttle_t12x));
		}


		break;
	default:
		break;
	}

	return tegra_soctherm_init(&ardbeg_soctherm_data);
}
