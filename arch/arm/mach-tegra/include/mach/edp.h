/*
 * arch/arm/mach-tegra/include/mach/edp.h
 *
 * Copyright (c) 2011-2014, NVIDIA CORPORATION. All Rights Reserved.
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

#ifndef __MACH_EDP_H
#define __MACH_EDP_H

#ifdef CONFIG_ARCH_TEGRA_2x_SOC
#error "tegra2x: no support"
#endif

#ifdef CONFIG_ARCH_TEGRA_3x_SOC
#error "tegra3x: no support"
#endif

#ifdef CONFIG_ARCH_TEGRA_11x_SOC
#error "tegra11x: no support"
#endif

#ifdef CONFIG_ARCH_TEGRA_14x_SOC
#error "tegra14x: no support"
#endif

#include <linux/debugfs.h>
#include <linux/edp.h>
#include <linux/thermal.h>
#include <linux/platform_data/tegra_edp.h>
#include <linux/platform_data/thermal_sensors.h>
#include <linux/tegra_ppm.h>

struct tegra_edp_limits {
	int temperature;
	unsigned int freq_limits[4];
};

struct tegra_edp_voltage_temp_constraint {
	int temperature;
	unsigned int voltage_limit_mv;
};

struct tegra_edp_maximum_current_constraint {
	unsigned int max_cur;
	unsigned int max_temp;
	unsigned int max_freq[4]; /* KHz */
};


struct tegra_edp_common_powermodel_params {
	unsigned int temp_scaled;

	unsigned int dyn_scaled;
	int dyn_consts_n[4];	 /* pre-multiplied by 'scaled */

	unsigned int consts_scaled;
	int leakage_consts_n[4];	 /* pre-multiplied by 'scaled */

	unsigned int ijk_scaled;
	int leakage_consts_ijk[4][4][4]; /* pre-multiplied by 'scaled */
	unsigned int leakage_min;	 /* minimum leakage current */
};

struct tegra_edp_cpu_powermodel_params {
	int cpu_speedo_id;

	struct tegra_ppm_params common;

	unsigned int safety_cap[4];
};

struct tegra_edp_freq_voltage_table {
	unsigned int freq;
	int voltage_mv;
};

enum tegra_core_edp_profiles {
	CORE_EDP_PROFILE_FAVOR_EMC = 0,
	CORE_EDP_PROFILE_BALANCED,
	CORE_EDP_PROFILE_FAVOR_GPU,

	CORE_EDP_PROFILES_NUM,
};

struct tegra_core_edp_limits {
	int sku;
	struct clk **cap_clocks;
	int cap_clocks_num;
	int *temperatures;
	int temperature_ranges;
	int core_modules_states;
	unsigned long *cap_rates_scpu_on;
	unsigned long *cap_rates_scpu_off;
};

bool tegra_is_edp_reg_idle_supported(void);
bool tegra_is_cpu_edp_supported(void);

#ifdef CONFIG_TEGRA_EDP_LIMITS
int tegra_get_edp_max_thermal_index(void);
unsigned int tegra_get_edp_max_freq(int, int);
unsigned int tegra_get_reg_idle_freq(int);
void tegra_recalculate_cpu_edp_limits(void);
void tegra_get_cpu_edp_limits(const struct tegra_edp_limits **limits,
			      int *size);
unsigned int tegra_get_sysedp_max_freq(int cpupwr, int online_cpus,
				       int cpu_mode);

/* defined in cpu-tegra.c */
void tegra_edp_update_max_cpus(void);

#else /* CONFIG_TEGRA_EDP_LIMITS */
static inline void tegra_recalculate_cpu_edp_limits(void)
{}
static inline void tegra_get_cpu_edp_limits(struct tegra_edp_limits **limits,
					    int *size)
{}
static inline void tegra_edp_update_max_cpus(void)
{}
#endif

#ifdef CONFIG_TEGRA_CORE_EDP_LIMITS
void tegra_init_core_edp_limits(unsigned int regulator_ma);
int tegra_core_edp_debugfs_init(struct dentry *edp_dir);
int tegra_core_edp_cpu_state_update(bool scpu_state);
struct tegra_cooling_device *tegra_core_edp_get_cdev(void);
#else
static inline void tegra_init_core_edp_limits(unsigned int regulator_ma)
{}
static inline int tegra_core_edp_debugfs_init(struct dentry *edp_dir)
{ return 0; }
static inline int tegra_core_edp_cpu_state_update(bool scpu_state)
{ return 0; }
static inline struct tegra_cooling_device *tegra_core_edp_get_cdev(void)
{ return NULL; }
#endif

#ifdef CONFIG_SYSEDP_FRAMEWORK
struct tegra_sysedp_corecap *tegra_get_sysedp_corecap(unsigned int *sz);
#else
static inline struct tegra_sysedp_corecap *tegra_get_sysedp_corecap
(unsigned int *sz) { return NULL; }
#endif

#endif	/* __MACH_EDP_H */
