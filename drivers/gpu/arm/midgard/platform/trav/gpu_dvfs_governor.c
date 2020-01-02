/* drivers/gpu/arm/.../platform/gpu_dvfs_governor.c
 *
 * Copyright 2011 by S.LSI. Samsung Electronics Inc.
 * San#24, Nongseo-Dong, Giheung-Gu, Yongin, Korea
 *
 * Samsung SoC Mali-T Series DVFS driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software FoundatIon.
 */

/**
 * @file gpu_dvfs_governor.c
 * DVFS
 */

#include <mali_kbase.h>

#include "mali_kbase_platform.h"
#include "gpu_dvfs_handler.h"
#include "gpu_dvfs_governor.h"
#ifdef CONFIG_CPU_THERMAL_IPA
#include "gpu_ipa.h"
#endif /* CONFIG_CPU_THERMAL_IPA */

#ifdef CONFIG_MALI_DVFS
typedef void (*GET_NEXT_LEVEL)(struct exynos_context *platform, int utilization);
GET_NEXT_LEVEL gpu_dvfs_get_next_level;

static int gpu_dvfs_governor_default(struct exynos_context *platform, int utilization);
static int gpu_dvfs_governor_interactive(struct exynos_context *platform, int utilization);
static int gpu_dvfs_governor_static(struct exynos_context *platform, int utilization);
static int gpu_dvfs_governor_booster(struct exynos_context *platform, int utilization);

static gpu_dvfs_governor_info governor_info[G3D_MAX_GOVERNOR_NUM] = {
	{
		G3D_DVFS_GOVERNOR_DEFAULT,
		"Default",
		gpu_dvfs_governor_default,
		NULL
	},
	{
		G3D_DVFS_GOVERNOR_INTERACTIVE,
		"Interactive",
		gpu_dvfs_governor_interactive,
		NULL
	},
	{
		G3D_DVFS_GOVERNOR_STATIC,
		"Static",
		gpu_dvfs_governor_static,
		NULL
	},
	{
		G3D_DVFS_GOVERNOR_BOOSTER,
		"Booster",
		gpu_dvfs_governor_booster,
		NULL
	},
};

void gpu_dvfs_update_start_clk(int governor_type, int clk)
{
	governor_info[governor_type].start_clk = clk;
}

void gpu_dvfs_update_table(int governor_type, gpu_dvfs_info *table)
{
	governor_info[governor_type].table = table;
}

void gpu_dvfs_update_table_size(int governor_type, int size)
{
	governor_info[governor_type].table_size = size;
}

void *gpu_dvfs_get_governor_info(void)
{
	return &governor_info;
}

static int gpu_dvfs_governor_default(struct exynos_context *platform, int utilization)
{
	DVFS_ASSERT(platform);

	if ((platform->step > gpu_dvfs_get_level(platform->gpu_max_clock)) &&
			(utilization > platform->table[platform->step].max_threshold)) {
		platform->step--;
#ifdef MALI_SEC_HWCNT
		if ((!platform->hwcnt_bt_clk) && (platform->table[platform->step].clock > platform->gpu_max_clock_limit))
			platform->step = gpu_dvfs_get_level(platform->gpu_max_clock_limit);
#else
		if (platform->table[platform->step].clock > platform->gpu_max_clock_limit)
			platform->step = gpu_dvfs_get_level(platform->gpu_max_clock_limit);
#endif
		platform->down_requirement = platform->table[platform->step].down_staycount;
	} else if ((platform->step < gpu_dvfs_get_level(platform->gpu_min_clock)) && (utilization < platform->table[platform->step].min_threshold)) {
		platform->down_requirement--;
		if (platform->down_requirement == 0) {
			platform->step++;
			platform->down_requirement = platform->table[platform->step].down_staycount;
		}
	} else {
		platform->down_requirement = platform->table[platform->step].down_staycount;
	}
	DVFS_ASSERT((platform->step >= gpu_dvfs_get_level(platform->gpu_max_clock))
					&& (platform->step <= gpu_dvfs_get_level(platform->gpu_min_clock)));

	return 0;
}

static int gpu_dvfs_governor_interactive(struct exynos_context *platform, int utilization)
{
	DVFS_ASSERT(platform);

	if ((platform->step > gpu_dvfs_get_level(platform->gpu_max_clock))
			&& (utilization > platform->table[platform->step].max_threshold)) {
		int highspeed_level = gpu_dvfs_get_level(platform->interactive.highspeed_clock);
		if ((highspeed_level > 0) && (platform->step > highspeed_level)
				&& (utilization > platform->interactive.highspeed_load)) {
			if (platform->interactive.delay_count == platform->interactive.highspeed_delay) {
				platform->step = highspeed_level;
				platform->interactive.delay_count = 0;
			} else {
				platform->interactive.delay_count++;
			}
		} else {
			platform->step--;
			platform->interactive.delay_count = 0;
		}
#ifdef MALI_SEC_HWCNT
		if ((!platform->hwcnt_bt_clk) && (platform->table[platform->step].clock > platform->gpu_max_clock_limit))
			platform->step = gpu_dvfs_get_level(platform->gpu_max_clock_limit);
#else
		if (platform->table[platform->step].clock > platform->gpu_max_clock_limit)
			platform->step = gpu_dvfs_get_level(platform->gpu_max_clock_limit);
#endif
		platform->down_requirement = platform->table[platform->step].down_staycount;
	} else if ((platform->step < gpu_dvfs_get_level(platform->gpu_min_clock))
			&& (utilization < platform->table[platform->step].min_threshold)) {
		platform->interactive.delay_count = 0;
		platform->down_requirement--;
		if (platform->down_requirement == 0) {
			platform->step++;
			platform->down_requirement = platform->table[platform->step].down_staycount;
		}
	} else {
		platform->interactive.delay_count = 0;
		platform->down_requirement = platform->table[platform->step].down_staycount;
	}

	DVFS_ASSERT((platform->step >= gpu_dvfs_get_level(platform->gpu_max_clock))
					&& (platform->step <= gpu_dvfs_get_level(platform->gpu_min_clock)));

	return 0;
}

#define G3D_GOVERNOR_STATIC_PERIOD		10
static int gpu_dvfs_governor_static(struct exynos_context *platform, int utilization)
{
	static bool step_down = true;
	static int count;

	DVFS_ASSERT(platform);

	if (count == G3D_GOVERNOR_STATIC_PERIOD) {
		if (step_down) {
			if (platform->step > gpu_dvfs_get_level(platform->gpu_max_clock))
				platform->step--;
			if (((platform->max_lock > 0) && (platform->table[platform->step].clock == platform->max_lock))
					|| (platform->step == gpu_dvfs_get_level(platform->gpu_max_clock)))
				step_down = false;
		} else {
			if (platform->step < gpu_dvfs_get_level(platform->gpu_min_clock))
				platform->step++;
			if (((platform->min_lock > 0) && (platform->table[platform->step].clock == platform->min_lock))
					|| (platform->step == gpu_dvfs_get_level(platform->gpu_min_clock)))
				step_down = true;
		}

		count = 0;
	} else {
		count++;
	}

	return 0;
}

static int gpu_dvfs_governor_booster(struct exynos_context *platform, int utilization)
{
	static int weight;
	int cur_weight, booster_threshold, dvfs_table_lock;

	DVFS_ASSERT(platform);

	cur_weight = platform->cur_clock*utilization;
	/* booster_threshold = current clock * set the percentage of utilization */
	booster_threshold = platform->cur_clock * 50;

	dvfs_table_lock = gpu_dvfs_get_level(platform->gpu_max_clock);

	if ((platform->step >= dvfs_table_lock+2) &&
			((cur_weight - weight) > booster_threshold)) {
		platform->step -= 2;
		platform->down_requirement = platform->table[platform->step].down_staycount;
		GPU_LOG(DVFS_WARNING, DUMMY, 0u, 0u, "Booster Governor: G3D level 2 step\n");
	} else if ((platform->step > gpu_dvfs_get_level(platform->gpu_max_clock)) &&
			(utilization > platform->table[platform->step].max_threshold)) {
		platform->step--;
		platform->down_requirement = platform->table[platform->step].down_staycount;
	} else if ((platform->step < gpu_dvfs_get_level(platform->gpu_min_clock)) &&
			(utilization < platform->table[platform->step].min_threshold)) {
		platform->down_requirement--;
		if (platform->down_requirement == 0) {
			platform->step++;
			platform->down_requirement = platform->table[platform->step].down_staycount;
		}
	} else {
		platform->down_requirement = platform->table[platform->step].down_staycount;
	}

	DVFS_ASSERT((platform->step >= gpu_dvfs_get_level(platform->gpu_max_clock))
					&& (platform->step <= gpu_dvfs_get_level(platform->gpu_min_clock)));

	weight = cur_weight;

	return 0;
}

static int gpu_dvfs_decide_next_governor(struct exynos_context *platform)
{
	return 0;
}

void ipa_mali_dvfs_requested(unsigned int freq);
int gpu_dvfs_decide_next_freq(struct kbase_device *kbdev, int utilization)
{
	unsigned long flags;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;
	DVFS_ASSERT(platform);

	spin_lock_irqsave(&platform->gpu_dvfs_spinlock, flags);
	gpu_dvfs_decide_next_governor(platform);
	gpu_dvfs_get_next_level(platform, utilization);
	spin_unlock_irqrestore(&platform->gpu_dvfs_spinlock, flags);

#ifdef CONFIG_CPU_THERMAL_IPA
	ipa_mali_dvfs_requested(platform->table[platform->step].clock);
#endif /* CONFIG_CPU_THERMAL_IPA */

	return platform->table[platform->step].clock;
}

static int gpu_dvfs_update_asv_table(struct exynos_context *platform)
{
	int i, voltage, m_voltage, dvfs_table_size;
	gpu_dvfs_info *dvfs_table;

#ifdef CONFIG_PWRCAL
	struct dvfs_rate_volt g3d_rate_volt[48];
	struct dvfs_rate_volt g3d_m_rate_volt[48];
	int cal_table_size;
	int j;

	dvfs_table = platform->table;
	dvfs_table_size = platform->table_size;

	cal_table_size = cal_dfs_get_rate_asv_table(dvfs_g3d, g3d_rate_volt);
	if (!cal_table_size)
		GPU_LOG(DVFS_WARNING, DUMMY, 0u, 0u, "Failed get G3D ASV table\n");

	for (i = 0; i < dvfs_table_size; i++) {
		for (j = 0; j < cal_table_size; j++) {
			if (dvfs_table[i].clock * 1000 == g3d_rate_volt[j].rate) {
				voltage = g3d_rate_volt[j].volt;
				if (voltage > 0) {
					dvfs_table[i].voltage = g3d_rate_volt[j].volt;
					GPU_LOG(DVFS_WARNING, DUMMY, 0u, 0u, "G3D   %dKhz ASV is %duV\n", dvfs_table[i].clock*1000, dvfs_table[i].voltage);
				}
			}
		}
	}

	cal_table_size = cal_dfs_get_rate_asv_table(dvs_g3dm, g3d_m_rate_volt);
	if (!cal_table_size)
		GPU_LOG(DVFS_WARNING, DUMMY, 0u, 0u, "Failed get G3D_M ASV table\n");

	for (i = 0; i < dvfs_table_size; i++) {
		for (j = 0; j < cal_table_size; j++) {
			if (dvfs_table[i].clock * 1000 == g3d_m_rate_volt[j].rate) {
				m_voltage = g3d_m_rate_volt[j].volt;
				if (m_voltage > 0) {
					dvfs_table[i].g3dm_voltage = g3d_m_rate_volt[j].volt;
					GPU_LOG(DVFS_WARNING, DUMMY, 0u, 0u, "G3D_M %dKhz ASV is %duV\n", dvfs_table[i].clock*1000, dvfs_table[i].g3dm_voltage);
				}
			}
		}
	}
#else
	DVFS_ASSERT(platform);

	dvfs_table = platform->table;
	dvfs_table_size = platform->table_size;
	for (i = 0; i < dvfs_table_size; i++) {
		if (platform->dynamic_abb_status) {
			dvfs_table[i].asv_abb = get_match_abb(ID_G3D, dvfs_table[i].clock*1000);
			GPU_LOG(DVFS_WARNING, DUMMY, 0u, 0u, "DEVFREQ: %uKhz, ABB %u\n", dvfs_table[i].clock*1000, dvfs_table[i].asv_abb);
		}

		voltage = get_match_volt(ID_G3D, dvfs_table[i].clock*1000);
		if (voltage > 0)
			dvfs_table[i].voltage = voltage;
		GPU_LOG(DVFS_WARNING, DUMMY, 0u, 0u, "G3D %dKhz ASV is %duV\n", dvfs_table[i].clock*1000, dvfs_table[i].voltage);
	}
#endif
	return 0;
}
#endif /* CONFIG_MALI_DVFS */

int gpu_dvfs_governor_setting(struct exynos_context *platform, int governor_type)
{
#ifdef CONFIG_MALI_DVFS
	int i;
#endif /* CONFIG_MALI_DVFS */
	unsigned long flags;

	DVFS_ASSERT(platform);

	if ((governor_type < 0) || (governor_type >= G3D_MAX_GOVERNOR_NUM)) {
		GPU_LOG(DVFS_WARNING, DUMMY, 0u, 0u, "%s: invalid governor type (%d)\n", __func__, governor_type);
		return -1;
	}

	spin_lock_irqsave(&platform->gpu_dvfs_spinlock, flags);
#ifdef CONFIG_MALI_DVFS
	platform->table = governor_info[governor_type].table;
	platform->table_size = governor_info[governor_type].table_size;
	platform->step = gpu_dvfs_get_level(governor_info[governor_type].start_clk);
	gpu_dvfs_get_next_level = (GET_NEXT_LEVEL)(governor_info[governor_type].governor);

	platform->env_data.utilization = 80;
	platform->max_lock = 0;
	platform->min_lock = 0;

	for (i = 0; i < NUMBER_LOCK; i++) {
		platform->user_max_lock[i] = 0;
		platform->user_min_lock[i] = 0;
	}

	platform->down_requirement = 1;
	platform->governor_type = governor_type;

	gpu_dvfs_init_time_in_state();
#else /* CONFIG_MALI_DVFS */
	platform->table = (gpu_dvfs_info *)gpu_get_attrib_data(platform->attrib, GPU_GOVERNOR_TABLE_DEFAULT);
	platform->table_size = (u32)gpu_get_attrib_data(platform->attrib, GPU_GOVERNOR_TABLE_SIZE_DEFAULT);
	platform->step = gpu_dvfs_get_level(platform->gpu_dvfs_start_clock);
#endif /* CONFIG_MALI_DVFS */
	platform->cur_clock = platform->table[platform->step].clock;

	spin_unlock_irqrestore(&platform->gpu_dvfs_spinlock, flags);

	return 0;
}

int gpu_dvfs_governor_init(struct kbase_device *kbdev)
{
	int governor_type = G3D_DVFS_GOVERNOR_DEFAULT;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;

	DVFS_ASSERT(platform);

#ifdef CONFIG_MALI_DVFS
	governor_type = platform->governor_type;
#endif /* CONFIG_MALI_DVFS */
	if (gpu_dvfs_governor_setting(platform, governor_type) < 0) {
		GPU_LOG(DVFS_WARNING, DUMMY, 0u, 0u, "%s: fail to initialize governor\n", __func__);
		return -1;
	}

	//share table_size among governors, as every single governor has same table_size.
	platform->save_cpu_max_freq = kmalloc(sizeof(int) * platform->table_size, GFP_KERNEL);
#ifdef CONFIG_MALI_DVFS
	gpu_dvfs_update_asv_table(platform);
	gpu_dvfs_decide_max_clock(platform);
#endif /* CONFIG_MALI_DVFS */
#if defined(CONFIG_MALI_DVFS) && defined(CONFIG_CPU_THERMAL_IPA)
	gpu_ipa_dvfs_calc_norm_utilisation(kbdev);
#endif /* CONFIG_MALI_DVFS && CONFIG_CPU_THERMAL_IPA */

	return 0;
}
