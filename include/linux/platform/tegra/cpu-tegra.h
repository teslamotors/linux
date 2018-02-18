/*
 * linux/platform/tegra/cpu-tegra.h
 *
 * Copyright (c) 2011-2015, NVIDIA Corporation. All rights reserved.
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

#ifndef __MACH_TEGRA_CPU_TEGRA_H
#define __MACH_TEGRA_CPU_TEGRA_H

#include <linux/fs.h>
#include <linux/tegra_throttle.h>

unsigned int tegra_getspeed(unsigned int cpu);
int tegra_update_cpu_speed(unsigned long rate);
int tegra_cpu_set_speed_cap(unsigned int *speed_cap);
int tegra_cpu_set_speed_cap_locked(unsigned int *speed_cap);
void tegra_cpu_set_volt_cap(unsigned int cap);

#if defined(CONFIG_TEGRA_CPUQUIET) && defined(CONFIG_TEGRA_CLUSTER_CONTROL)
int tegra_auto_hotplug_init(struct mutex *cpulock);
void tegra_auto_hotplug_exit(void);
void tegra_auto_hotplug_governor(unsigned int cpu_freq, bool suspend);
#else
static inline int tegra_auto_hotplug_init(struct mutex *cpu_lock)
{ return 0; }
static inline void tegra_auto_hotplug_exit(void)
{ }
static inline void tegra_auto_hotplug_governor(unsigned int cpu_freq,
					       bool suspend)
{ }
#endif

#ifdef CONFIG_CPU_FREQ
struct cpufreq_frequency_table;

/**
  * freq_table: List of frequencies allowed for cpu.
  * throttle_lowest_index: Lowest throttling frequency index.
  * throttle_highest_index: Highest throttling frequency index.
  * suspend_index: Suspend frequency index.
  * preserve_across_suspend: Preserve cpu frequency even after suspend
  *			     for restoring during resume.
  */
struct tegra_cpufreq_table_data {
	struct cpufreq_frequency_table *freq_table;
	int throttle_lowest_index;
	int throttle_highest_index;
	int suspend_index;
	bool preserve_across_suspend;
};
struct tegra_cpufreq_table_data *tegra_cpufreq_table_get(void);
unsigned long tegra_emc_to_cpu_ratio(unsigned long cpu_rate);
#ifdef CONFIG_ARCH_TEGRA_2x_SOC
static inline int tegra_update_mselect_rate(unsigned long cpu_rate)
{ return 0; }
#else
int tegra_update_mselect_rate(unsigned long cpu_rate);
#endif
#if defined(CONFIG_ARCH_TEGRA_13x_SOC) || defined(CONFIG_ARCH_TEGRA_21x_SOC)
unsigned long tegra_emc_cpu_limit(unsigned long cpu_rate);
#else
static inline unsigned long tegra_emc_cpu_limit(unsigned long cpu_rate)
{ return 0; }
#endif
int tegra_suspended_target(unsigned int target_freq);
#else
static inline unsigned long tegra_emc_to_cpu_ratio(unsigned long cpu_rate)
{ return 0; }
static inline int tegra_suspended_target(unsigned int target_freq)
{ return -ENOSYS; }
#endif

#if defined(CONFIG_CPU_FREQ) && defined(CONFIG_TEGRA_EDP_LIMITS)
int tegra_update_cpu_edp_limits(void);
int tegra_cpu_reg_mode_force_normal(bool force);
#else
static inline int tegra_update_cpu_edp_limits(void)
{ return 0; }
static inline int tegra_cpu_reg_mode_force_normal(bool force)
{ return 0; }
#endif

#ifdef CONFIG_TEGRA_CPU_VOLT_CAP
struct tegra_cooling_device *tegra_vc_get_cdev(void);
#else
static inline struct tegra_cooling_device *tegra_vc_get_cdev(void)
{ return NULL; }
#endif

#ifdef CONFIG_TEGRA_HMP_CLUSTER_CONTROL
unsigned long lp_to_virtual_gfreq(unsigned long lp_freq);
int tegra_cpu_volt_cap_apply(int *cap_idx, int new_idx, int level);
#else
static inline unsigned long lp_to_virtual_gfreq(unsigned long freq)
{ return freq; }
static inline int tegra_cpu_volt_cap_apply(int *cap_idx, int new_idx, int level)
{ return -ENOSYS; }
#endif

#endif /* __MACH_TEGRA_CPU_TEGRA_H */
