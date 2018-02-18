/*
 * arch/arm/mach-tegra/board-loki-power.c
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
#include <linux/platform_device.h>
#include <linux/resource.h>
#include <linux/io.h>
#include <linux/regulator/machine.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/regulator/tegra-dfll-bypass-regulator.h>
#include <linux/power/power_supply_extcon.h>
#include <linux/tegra-fuse.h>
#include <linux/tegra-pmc.h>
#include <linux/pinctrl/pinconf-tegra.h>
#include <linux/gpio.h>
#include <linux/system-wakeup.h>
#include <linux/syscore_ops.h>
#include <linux/delay.h>
#include <linux/tegra-pmc.h>

#include <mach/irqs.h>
#include <mach/edp.h>

#include <linux/pid_thermal_gov.h>
#include <linux/tegra_soctherm.h>

#include <asm/mach-types.h>

#include "pm.h"
#include "board.h"
#include "tegra-board-id.h"
#include "board-common.h"
#include "board-loki.h"
#include "board-pmu-defines.h"
#include "devices.h"
#include "iomap.h"
#include "tegra-board-id.h"
#include <linux/platform/tegra/dvfs.h>
#include <linux/platform/tegra/tegra_cl_dvfs.h>

void tegra13x_vdd_cpu_align(int step_uv, int offset_uv);

static void loki_reset_gamepad(void)
{
	int ret = 0;
	int gpio_gamepad_rst = TEGRA_GPIO_PQ7;

	struct board_info bi;
	tegra_get_board_info(&bi);

	if (bi.board_id == BOARD_E2548 ||
		(bi.board_id == BOARD_P2530 && bi.fab == 0xA0))
		gpio_gamepad_rst = TEGRA_GPIO_PK0;

	ret = gpio_request(gpio_gamepad_rst, "GAMEPAD_RST");
	if (ret < 0) {
		pr_err("%s: gpio_request failed %d\n", __func__, ret);
		return;
	}

	ret = gpio_direction_output(gpio_gamepad_rst, 1);
	if (ret < 0) {
		gpio_free(gpio_gamepad_rst);
		pr_err("%s: gpio_direction_output failed %d\n", __func__, ret);
		return;
	}

	gpio_set_value(gpio_gamepad_rst, 0);
	udelay(20);
	gpio_set_value(gpio_gamepad_rst, 1);
	gpio_free(gpio_gamepad_rst);

	pr_info("%s: done\n", __func__);

	return;
}

static void loki_board_resume(void)
{
	int wakeup_irq = get_wakeup_reason_irq();

	/* if wake up by LID open, reset gamepad uC */
	if (gpio_to_irq(TEGRA_GPIO_PS0) == wakeup_irq)
		loki_reset_gamepad();

	return;
}

static struct syscore_ops loki_pm_syscore_ops = {
	.resume = loki_board_resume,
};

static struct tegra_suspend_platform_data loki_suspend_data = {
	.cpu_timer      = 3500,
	.cpu_off_timer  = 300,
	.suspend_mode   = TEGRA_SUSPEND_LP0,
	.core_timer     = 0x157e,
	.core_off_timer = 2000,
	.corereq_high   = true,
	.sysclkreq_high = true,
	.cpu_lp2_min_residency = 1000,
	.min_residency_crail = 20000,
};

int __init loki_suspend_init(void)
{
	struct board_info bi;
	tegra_get_board_info(&bi);

	/* reduce cpu_power_good_time for loki ffd fab a3 or higher */
	if (bi.board_id == BOARD_P2530 && bi.fab >= 0xa3)
		loki_suspend_data.cpu_timer = 500;

	tegra_init_suspend(&loki_suspend_data);

	register_syscore_ops(&loki_pm_syscore_ops);

	return 0;
}

static struct power_supply_extcon_plat_data extcon_pdata = {
	.extcon_name = "tegra-udc",
	.y_cable_extcon_name = "tegra-otg",
};

static struct platform_device power_supply_extcon_device = {
	.name	= "power-supply-extcon",
	.id	= -1,
	.dev	= {
		.platform_data = &extcon_pdata,
	},
};

/************************ LOKI CL-DVFS DATA *********************/
#define LOKI_CPU_VDD_MAP_SIZE		33
#define LOKI_CPU_VDD_MIN_UV		703000
#define LOKI_CPU_VDD_STEP_UV		19200
#define LOKI_CPU_VDD_STEP_US		80
#define LOKI_CPU_VDD_BOOT_UV		1000000

#ifdef CONFIG_ARCH_TEGRA_HAS_CL_DVFS
/* loki board parameters for cpu dfll */
static struct tegra_cl_dvfs_cfg_param loki_cl_dvfs_param = {
	.sample_rate = 50000,

	.force_mode = TEGRA_CL_DVFS_FORCE_FIXED,
	.cf = 10,
	.ci = 0,
	.cg = 2,

	.droop_cut_value = 0xF,
	.droop_restore_ramp = 0x0,
	.scale_out_ramp = 0x0,
};

/* loki dfll bypass device for legacy dvfs control */
static struct regulator_consumer_supply loki_dfll_bypass_consumers[] = {
	REGULATOR_SUPPLY("vdd_cpu", NULL),
};
DFLL_BYPASS(loki,
	    LOKI_CPU_VDD_MIN_UV, LOKI_CPU_VDD_STEP_UV, LOKI_CPU_VDD_BOOT_UV,
	    LOKI_CPU_VDD_MAP_SIZE, LOKI_CPU_VDD_STEP_US, -1);

static struct tegra_cl_dvfs_platform_data loki_cl_dvfs_data = {
	.dfll_clk_name = "dfll_cpu",
	.pmu_if = TEGRA_CL_DVFS_PMU_PWM,
	.u.pmu_pwm = {
		.pwm_rate = 12750000,
		.min_uV = LOKI_CPU_VDD_MIN_UV,
		.step_uV = LOKI_CPU_VDD_STEP_UV,
		.pwm_bus = TEGRA_CL_DVFS_PWM_1WIRE_BUFFER,
		.out_gpio = TEGRA_GPIO_PU6,
		.out_enable_high = false,
#ifdef CONFIG_REGULATOR_TEGRA_DFLL_BYPASS
		.dfll_bypass_dev = &loki_dfll_bypass_dev,
#endif
	},

	.cfg_param = &loki_cl_dvfs_param,
};

static void loki_suspend_dfll_bypass(void)
{
/* tristate external PWM buffer */
	if (gpio_is_valid(loki_cl_dvfs_data.u.pmu_pwm.out_gpio))
		__gpio_set_value(loki_cl_dvfs_data.u.pmu_pwm.out_gpio, 1);
}

static void loki_resume_dfll_bypass(void)
{
/* enable PWM buffer operations */
	if (gpio_is_valid(loki_cl_dvfs_data.u.pmu_pwm.out_gpio))
		__gpio_set_value(loki_cl_dvfs_data.u.pmu_pwm.out_gpio, 0);
}
static int __init loki_cl_dvfs_init(void)
{
	struct tegra_cl_dvfs_platform_data *data = NULL;
	struct board_info bi;

	tegra_get_board_info(&bi);
	if (bi.board_id == BOARD_P2530 && bi.fab >= 0xc0) {
		loki_cl_dvfs_data.u.pmu_pwm.out_gpio =  -1;
		loki_cl_dvfs_data.u.pmu_pwm.pwm_bus =
			TEGRA_CL_DVFS_PWM_1WIRE_DIRECT;
	}

	{
		data = &loki_cl_dvfs_data;

		data->u.pmu_pwm.pinctrl_dev = tegra_get_pinctrl_device_handle();
		if (!data->u.pmu_pwm.pinctrl_dev) {
			pr_err("%s: Tegra pincontrol driver not found\n",
				__func__);
			return -EINVAL;
		}

		data->u.pmu_pwm.pwm_pingroup =
			pinctrl_get_selector_from_group_name(
				data->u.pmu_pwm.pinctrl_dev, "dvfs_pwm_px0");
		if (data->u.pmu_pwm.pwm_pingroup < 0) {
			pr_err("%s: Tegra pin dvfs_pwm_px0 not found\n",
				__func__);
			return -EINVAL;
		}

		if (data->u.pmu_pwm.dfll_bypass_dev) {
			/* this has to be exact to 1uV level from table */
			loki_suspend_data.suspend_dfll_bypass =
				loki_suspend_dfll_bypass;
			loki_suspend_data.resume_dfll_bypass =
				loki_resume_dfll_bypass;
			platform_device_register(
				data->u.pmu_pwm.dfll_bypass_dev);
		} else {
			(void)loki_dfll_bypass_dev;
		}
	}

	data->flags = TEGRA_CL_DVFS_DYN_OUTPUT_CFG;
	tegra_cl_dvfs_device.dev.platform_data = data;
	platform_device_register(&tegra_cl_dvfs_device);
	return 0;
}
#else
static inline int loki_cl_dvfs_init()
{ return 0; }
#endif

int __init loki_rail_alignment_init(void)
{
#ifdef CONFIG_ARCH_TEGRA_13x_SOC
	tegra13x_vdd_cpu_align(LOKI_CPU_VDD_STEP_UV,
			LOKI_CPU_VDD_MIN_UV);
#else
	tegra12x_vdd_cpu_align(LOKI_CPU_VDD_STEP_UV,
			LOKI_CPU_VDD_MIN_UV);
#endif
	return 0;
}

int __init loki_regulator_init(void)
{
	platform_device_register(&power_supply_extcon_device);

	loki_cl_dvfs_init();
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

static struct soctherm_platform_data loki_soctherm_data = {
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
					.trip_temp = 98000,
					.trip_type = THERMAL_TRIP_HOT,
					.upper = THERMAL_NO_LIMIT,
					.lower = THERMAL_NO_LIMIT,
				},
				{
					.cdev_type = "tegra-balanced",
					.trip_temp = 88000,
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
					.trip_temp = 98000,
					.trip_type = THERMAL_TRIP_CRITICAL,
					.upper = THERMAL_NO_LIMIT,
					.lower = THERMAL_NO_LIMIT,
				},
				{
					.cdev_type = "tegra-heavy",
					.trip_temp = 95000,
					.trip_type = THERMAL_TRIP_HOT,
					.upper = THERMAL_NO_LIMIT,
					.lower = THERMAL_NO_LIMIT,
				},
				{
					.cdev_type = "tegra-balanced",
					.trip_temp = 85000,
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
					.trip_temp = 103000, /* = GPU shut */
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
					.enable = true,
					.throttling_depth = "heavy_throttling",
				},
			},
		},
		[THROTTLE_OC2] = {
			.throt_mode = BRIEF,
			.polarity = 1,
			.intr = false,
			.devs = {
				[THROTTLE_DEV_CPU] = {
					.enable = true,
					.depth = 30,
				},
				[THROTTLE_DEV_GPU] = {
					.enable = true,
					.throttling_depth = "heavy_throttling",
				},
			},
		},
	},
	.tshut_pmu_trip_data = &tpdata_palmas,
};

int __init loki_soctherm_init(void)
{
	/* do this only for supported CP,FT fuses */
	if ((tegra_fuse_calib_base_get_cp(NULL, NULL) >= 0) &&
	    (tegra_fuse_calib_base_get_ft(NULL, NULL) >= 0)) {
		tegra_platform_edp_init(
			loki_soctherm_data.therm[THERM_CPU].trips,
			&loki_soctherm_data.therm[THERM_CPU].num_trips,
			8000); /* edp temperature margin */
		tegra_platform_gpu_edp_init(
			loki_soctherm_data.therm[THERM_GPU].trips,
			&loki_soctherm_data.therm[THERM_GPU].num_trips,
			8000);
		tegra_add_cpu_vmax_trips(
			loki_soctherm_data.therm[THERM_CPU].trips,
			&loki_soctherm_data.therm[THERM_CPU].num_trips);
		tegra_add_tgpu_trips(
			loki_soctherm_data.therm[THERM_GPU].trips,
			&loki_soctherm_data.therm[THERM_GPU].num_trips);
		tegra_add_vc_trips(
			loki_soctherm_data.therm[THERM_CPU].trips,
			&loki_soctherm_data.therm[THERM_CPU].num_trips);
		tegra_add_core_vmax_trips(
			loki_soctherm_data.therm[THERM_PLL].trips,
			&loki_soctherm_data.therm[THERM_PLL].num_trips);
		tegra_add_cpu_vmin_trips(
			loki_soctherm_data.therm[THERM_CPU].trips,
			&loki_soctherm_data.therm[THERM_CPU].num_trips);
		tegra_add_gpu_vmin_trips(
			loki_soctherm_data.therm[THERM_GPU].trips,
			&loki_soctherm_data.therm[THERM_GPU].num_trips);
		/*PLL soctherm is being used for SOC vmin*/
		tegra_add_core_vmin_trips(
			loki_soctherm_data.therm[THERM_PLL].trips,
			&loki_soctherm_data.therm[THERM_PLL].num_trips);
	}

	return tegra_soctherm_init(&loki_soctherm_data);
}
