/*
 * include/linux/tegra-pm.h
 *
 * Copyright (c) 2013-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
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

#ifndef _LINUX_TEGRA_PM_H_
#define _LINUX_TEGRA_PM_H_

/* Core state 0-9 */
#define TEGRA210_CPUIDLE_C4	4
#define TEGRA210_CPUIDLE_C7	7

/* Cluster states 10-19 */
#define TEGRA210_CPUIDLE_CC4	14
#define TEGRA210_CPUIDLE_CC6	16
#define TEGRA210_CPUIDLE_CC7	17

/* SoC states 20-29 */
#define TEGRA210_CPUIDLE_SC2	22
#define TEGRA210_CPUIDLE_SC3	23
#define TEGRA210_CPUIDLE_SC4	24
#define TEGRA210_CPUIDLE_SC7	27

#define TEGRA210_CLUSTER_SWITCH	31

#define TEGRA_PM_SUSPEND	0x0001
#define TEGRA_PM_RESUME		0x0002

enum tegra_suspend_mode {
	TEGRA_SUSPEND_NONE = 0,
	TEGRA_SUSPEND_LP2,	/* CPU voltage off */
	TEGRA_SUSPEND_LP1,	/* CPU voltage off, DRAM self-refresh */
	TEGRA_SUSPEND_LP0,      /* CPU + core voltage off, DRAM self-refresh */
	TEGRA_MAX_SUSPEND_MODE,
};

enum suspend_stage {
	TEGRA_SUSPEND_BEFORE_PERIPHERAL,
	TEGRA_SUSPEND_BEFORE_CPU,
};

enum resume_stage {
	TEGRA_RESUME_AFTER_PERIPHERAL,
	TEGRA_RESUME_AFTER_CPU,
};

struct tegra_suspend_platform_data {
	unsigned long cpu_timer;   /* CPU power good time in us,  LP2/LP1 */
	unsigned long cpu_off_timer;	/* CPU power off time us, LP2/LP1 */
	unsigned long core_timer;  /* core power good time in ticks,  LP0 */
	unsigned long core_off_timer;	/* core power off time ticks, LP0 */
	unsigned int cpu_suspend_freq; /* cpu suspend/resume frequency in Hz */
	bool corereq_high;         /* Core power request active-high */
	bool sysclkreq_high;       /* System clock request is active-high */
	bool sysclkreq_gpio;       /* if System clock request is set to gpio */
	bool combined_req;         /* if core & CPU power requests are combined */
	enum tegra_suspend_mode suspend_mode;
	unsigned long cpu_lp2_min_residency; /* Min LP2 state residency in us */
	void (*board_suspend)(int lp_state, enum suspend_stage stg);
	/* lp_state = 0 for LP0 state, 1 for LP1 state, 2 for LP2 state */
	void (*board_resume)(int lp_state, enum resume_stage stg);
	unsigned int cpu_resume_boost;	/* CPU frequency resume boost in kHz */
#ifdef CONFIG_TEGRA_LP1_LOW_COREVOLTAGE
	bool lp1_lowvolt_support;
	unsigned int i2c_base_addr;
	unsigned int pmuslave_addr;
	unsigned int core_reg_addr;
	unsigned int lp1_core_volt_low_cold;
	unsigned int lp1_core_volt_low;
	unsigned int lp1_core_volt_high;
#endif
	unsigned int lp1bb_core_volt_min;
	unsigned long lp1bb_emc_rate_min;
	unsigned long lp1bb_emc_rate_max;
#ifdef CONFIG_ARCH_TEGRA_HAS_SYMMETRIC_CPU_PWR_GATE
	unsigned long min_residency_vmin_fmin;
	unsigned long min_residency_ncpu_slow;
	unsigned long min_residency_ncpu_fast;
	unsigned long min_residency_crail;
	bool crail_up_early;
#endif
	unsigned long min_residency_mclk_stop;
	bool usb_vbus_internal_wake; /* support for internal vbus wake */
	bool usb_id_internal_wake; /* support for internal id wake */

	void (*suspend_dfll_bypass)(void);
	void (*resume_dfll_bypass)(void);
};

void __init tegra_init_suspend(struct tegra_suspend_platform_data *plat);

int tegra_suspend_dram(enum tegra_suspend_mode mode, unsigned int flags);

int tegra_register_pm_notifier(struct notifier_block *nb);
int tegra_unregister_pm_notifier(struct notifier_block *nb);
int tegra_pm_notifier_call_chain(unsigned int val);
int tegra_pm_prepare_sc7(void);
int tegra_pm_post_sc7(void);
void tegra_log_suspend_entry_time(void);
void tegra_log_resume_time(void);

#endif /* _LINUX_TEGRA_PM_H_ */
