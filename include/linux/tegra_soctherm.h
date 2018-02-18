/*
 * include/linux/tegra_soctherm.h
 *
 * Copyright (c) 2011-2016, NVIDIA CORPORATION. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __TEGRA_SOCTHERM_H
#define __TEGRA_SOCTHERM_H

#include <linux/thermal.h>
#include <linux/platform_data/thermal_sensors.h>

/* This order must match the soc_therm HW register spec */
enum soctherm_sense {
	TSENSE_CPU0 = 0,
	TSENSE_CPU1,
	TSENSE_CPU2,
	TSENSE_CPU3,
	TSENSE_MEM0,
	TSENSE_MEM1,
	TSENSE_GPU,
	TSENSE_PLLX,
	TSENSE_SIZE,
};

/* This order must match the soc_therm HW register spec */
enum soctherm_therm_id {
	THERM_CPU = 0,
	THERM_GPU,
	THERM_MEM,
	THERM_PLL,
	THERM_SIZE,
	THERM_NONE,
};

enum soctherm_throttle_id {
	THROTTLE_LIGHT = 0,
	THROTTLE_HEAVY,
	THROTTLE_OC1,
	THROTTLE_OC2,
	THROTTLE_OC3,
	THROTTLE_OC4,
	THROTTLE_OC5, /* OC5 is reserved for WAR to Bug 1415030 */
	THROTTLE_SIZE,
};

enum soctherm_throttle_dev_id {
	THROTTLE_DEV_CPU = 0,
	THROTTLE_DEV_GPU,
	THROTTLE_DEV_SIZE,
	THROTTLE_DEV_NONE,
};

enum soctherm_oc_irq_id {
	TEGRA_SOC_OC_IRQ_1,
	TEGRA_SOC_OC_IRQ_2,
	TEGRA_SOC_OC_IRQ_3,
	TEGRA_SOC_OC_IRQ_4,
	TEGRA_SOC_OC_IRQ_5,
	TEGRA_SOC_OC_IRQ_MAX,
};

enum soctherm_polarity_id {
	SOCTHERM_ACTIVE_HIGH = 0,
	SOCTHERM_ACTIVE_LOW = 1,
};

struct soctherm_fuse_correction_war {
	/* both scaled *1000000 */
	int a;
	int b;
};

struct soctherm_sensor_common_params {
	u32 tall;
	u32 tiddq;
	u32 ten_count;
	u32 tsample;
	u32 tsamp_ate;
	u32 pdiv;
	u32 pdiv_ate;
};

struct soctherm_sensor_params {
	struct soctherm_sensor_common_params scp;
	struct soctherm_fuse_correction_war fuse_war[TSENSE_SIZE];
};

struct soctherm_therm {
	bool zone_enable;
	int passive_delay;
	int num_trips;
	int hotspot_offset;
	struct thermal_trip_info trips[THERMAL_MAX_TRIPS];
	struct thermal_zone_params *tzp;
	struct thermal_zone_device *tz;
	bool en_hw_pllx_offsetting;
	int pllx_offset_max;
	int pllx_offset_min;
};

struct soctherm_throttle_dev {
	bool enable;
	u8 depth; /* if this is non-zero, the values below are ignored */
	u8 dividend;
	u8 divisor;
	u16 duration;
	u8 step;
	char *throttling_depth;
};

enum throt_mode {
	DISABLED = 0,
	STICKY,
	BRIEF,
	RESERVED,
};

struct soctherm_throttle {
	u8 pgmask;
	u8 throt_mode;
	u8 polarity;
	u8 priority;
	u8 period;
	u32 alarm_cnt_threshold;
	u32 alarm_filter;
	bool intr;
	struct soctherm_throttle_dev devs[THROTTLE_DEV_SIZE];
};

struct soctherm_tsensor_pmu_data {
	u8 poweroff_reg_data;
	u8 poweroff_reg_addr;
	u8 reset_tegra;
	u8 controller_type;
	u8 i2c_controller_id;
	u8 pinmux;
	u8 pmu_16bit_ops;
	u8 pmu_i2c_addr;
};

/**
 * struct soctherm_platform_data - Board specific SOC_THERM info.
 * @thermal_irq_num:		IRQ number for soctherm thermal interrupt.
 * @edp_irq_num:		IRQ number for soctherm edp interrupt.
 * @oc_irq_base:		Base over-current IRQ number.
 * @num_oc_irqs:		Number of over-current IRQs.
 * @soctherm_clk_rate:		Clock rate for the SOC_THERM IP block.
 * @tsensor_clk_rate:		Clock rate for the thermal sensors.
 * @sensor_params:		Params for the thermal sensors.
 * @therm:			An array contanining the thermal zone settings.
 * @throttle:			specification for hardware throttle responses.
 * @tshut_pmu_trip_data:	PMU-specific thermal shutdown settings.
 */
struct soctherm_platform_data {
	int thermal_irq_num;
	int edp_irq_num;

	int oc_irq_base;
	int num_oc_irqs;

	u32 soctherm_clk_rate;
	u32 tsensor_clk_rate;

	struct soctherm_sensor_params sensor_params;
	struct soctherm_therm therm[THERM_SIZE];
	struct soctherm_throttle throttle[THROTTLE_SIZE];
	struct tegra_thermtrip_pmic_data *tshut_pmu_trip_data;
};

#ifdef CONFIG_TEGRA_SOCTHERM
int __init tegra_soctherm_init(struct soctherm_platform_data *data);
void tegra_soctherm_adjust_cpu_zone(bool high_voltage_range);
int tegra_soctherm_gpu_tsens_invalidate(bool control);
#else
static inline int tegra_soctherm_init(struct soctherm_platform_data *data)
{
	return 0;
}
static inline void tegra_soctherm_adjust_cpu_zone(bool high_voltage_range)
{ }
static int tegra_soctherm_gpu_tsens_invalidate(bool control)
{ return 0; }
#endif

#endif /* __TEGRA_SOCTHERM_H */
