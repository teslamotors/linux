/* drivers/gpu/arm/.../platform/gpu_pmqos.c
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
 * @file gpu_pmqos.c
 * DVFS
 */

#include <mali_kbase.h>

#include <linux/pm_qos.h>
#include "mali_kbase_platform.h"
#include "gpu_dvfs_handler.h"

struct pm_qos_request exynos5_g3d_mif_min_qos;
struct pm_qos_request exynos5_g3d_mif_max_qos;
struct pm_qos_request exynos5_g3d_int_qos;
struct pm_qos_request exynos5_g3d_cpu_cluster0_min_qos;
struct pm_qos_request exynos5_g3d_cpu_cluster1_max_qos;
struct pm_qos_request exynos5_g3d_cpu_cluster1_min_qos;

#ifdef CONFIG_MALI_DVFS_USER
struct pm_qos_request proactive_mif_min_qos;
struct pm_qos_request proactive_int_min_qos;
struct pm_qos_request proactive_apollo_min_qos;
struct pm_qos_request proactive_atlas_min_qos;
#endif

int gpu_pm_qos_command(struct exynos_context *platform, gpu_pmqos_state state)
{
	DVFS_ASSERT(platform);

	if (!platform->devfreq_status)
		return 0;

#ifdef CONFIG_MALI_DVFS
#ifdef CONFIG_PM_DEVFREQ
	int idx;
	switch (state) {
	case GPU_CONTROL_PM_QOS_INIT:
		pm_qos_add_request(&exynos5_g3d_mif_min_qos, PM_QOS_BUS_THROUGHPUT, 0);
		if (platform->pmqos_mif_max_clock)
			pm_qos_add_request(&exynos5_g3d_mif_max_qos, PM_QOS_BUS_THROUGHPUT_MAX, PM_QOS_BUS_THROUGHPUT_MAX_DEFAULT_VALUE);
		if (!platform->pmqos_int_disable)
			pm_qos_add_request(&exynos5_g3d_int_qos, PM_QOS_DEVICE_THROUGHPUT, 0);
		pm_qos_add_request(&exynos5_g3d_cpu_cluster0_min_qos, PM_QOS_CLUSTER0_FREQ_MIN, 0);
		pm_qos_add_request(&exynos5_g3d_cpu_cluster1_max_qos, PM_QOS_CLUSTER1_FREQ_MAX, PM_QOS_CLUSTER1_FREQ_MAX_DEFAULT_VALUE);
		if (platform->boost_egl_min_lock)
			pm_qos_add_request(&exynos5_g3d_cpu_cluster1_min_qos, PM_QOS_CLUSTER1_FREQ_MIN, 0);
		for(idx=0; idx<platform->table_size; idx++)
			platform->save_cpu_max_freq[idx] = platform->table[idx].cpu_max_freq;
		break;
	case GPU_CONTROL_PM_QOS_DEINIT:
		pm_qos_remove_request(&exynos5_g3d_mif_min_qos);
		if (platform->pmqos_mif_max_clock)
			pm_qos_remove_request(&exynos5_g3d_mif_max_qos);
		if (!platform->pmqos_int_disable)
			pm_qos_remove_request(&exynos5_g3d_int_qos);
		pm_qos_remove_request(&exynos5_g3d_cpu_cluster0_min_qos);
		pm_qos_remove_request(&exynos5_g3d_cpu_cluster1_max_qos);
		if (platform->boost_egl_min_lock)
			pm_qos_remove_request(&exynos5_g3d_cpu_cluster1_min_qos);
		break;
	case GPU_CONTROL_PM_QOS_SET:
		KBASE_DEBUG_ASSERT(platform->step >= 0);
		if (platform->perf_gathering_status) {
			gpu_mif_pmqos(platform, platform->table[platform->step].mem_freq);
		} else {
			pm_qos_update_request(&exynos5_g3d_mif_min_qos, platform->table[platform->step].mem_freq);
			if (platform->pmqos_mif_max_clock &&
				(platform->table[platform->step].clock >= platform->pmqos_mif_max_clock_base))
				pm_qos_update_request(&exynos5_g3d_mif_max_qos, platform->pmqos_mif_max_clock);
		}
		if (!platform->pmqos_int_disable)
			pm_qos_update_request(&exynos5_g3d_int_qos, platform->table[platform->step].int_freq);
		pm_qos_update_request(&exynos5_g3d_cpu_cluster0_min_qos, platform->table[platform->step].cpu_freq);
		if (!platform->boost_is_enabled) {
			if (platform->sustainable.low_power_cluster1_clock > 0 &&
				platform->sustainable.sustainable_gpu_clock == platform->table[platform->step].clock)
				pm_qos_update_request(&exynos5_g3d_cpu_cluster1_max_qos, platform->sustainable.low_power_cluster1_clock);
			else
				pm_qos_update_request(&exynos5_g3d_cpu_cluster1_max_qos, platform->table[platform->step].cpu_max_freq);
		}
		break;
	case GPU_CONTROL_PM_QOS_RESET:
		pm_qos_update_request(&exynos5_g3d_mif_min_qos, 0);
		if (platform->pmqos_mif_max_clock)
			pm_qos_update_request(&exynos5_g3d_mif_max_qos, PM_QOS_BUS_THROUGHPUT_MAX_DEFAULT_VALUE);
		if (!platform->pmqos_int_disable)
			pm_qos_update_request(&exynos5_g3d_int_qos, 0);
		pm_qos_update_request(&exynos5_g3d_cpu_cluster0_min_qos, 0);
		pm_qos_update_request(&exynos5_g3d_cpu_cluster1_max_qos, PM_QOS_CLUSTER1_FREQ_MAX_DEFAULT_VALUE);
		break;
	case GPU_CONTROL_PM_QOS_EGL_SET:
		//pm_qos_update_request(&exynos5_g3d_cpu_cluster1_min_qos, platform->boost_egl_min_lock);
		pm_qos_update_request_timeout(&exynos5_g3d_cpu_cluster1_min_qos, platform->boost_egl_min_lock, 30000);
		for(idx=0; idx<platform->table_size; idx++)
			platform->table[idx].cpu_max_freq = PM_QOS_CLUSTER1_FREQ_MAX_DEFAULT_VALUE;
		break;
	case GPU_CONTROL_PM_QOS_EGL_RESET:
		//pm_qos_update_request(&exynos5_g3d_cpu_cluster1_min_qos, 0);
		for(idx=0; idx<platform->table_size; idx++)
			platform->table[idx].cpu_max_freq = platform->save_cpu_max_freq[idx];
		break;
	default:
		break;
	}
#endif
#endif

	return 0;
}

int gpu_sustainable_pmqos(struct exynos_context *platform, int clock)
{
	static int full_util_count = 0;
	static int threshold_maxlock = 20;

	DVFS_ASSERT(platform);
	platform->sustainable.low_power_cluster1_clock = 0;

	if(!platform->devfreq_status)
		return 0;

	if (clock != platform->sustainable.sustainable_gpu_clock)
	{
		full_util_count = 0;
		threshold_maxlock = platform->sustainable.threshold;
		return 0;
	}

	if (platform->env_data.utilization == 100) {
		full_util_count++;

		if (full_util_count > threshold_maxlock) {
			platform->sustainable.low_power_cluster1_clock = platform->sustainable.low_power_cluster1_maxlock;
		}
	}
	else {
		full_util_count = 0;
		threshold_maxlock *= 2;
	}

	return 0;
}


int gpu_mif_pmqos(struct exynos_context *platform, int mem_freq)
{
	static int prev_freq;
	DVFS_ASSERT(platform);

	if(!platform->devfreq_status)
		return 0;
	if(prev_freq != mem_freq)
		pm_qos_update_request(&exynos5_g3d_mif_min_qos, mem_freq);

	prev_freq = mem_freq;

	return 0;
}

#ifdef CONFIG_MALI_DVFS_USER
int proactive_pm_qos_command(struct exynos_context *platform, gpu_pmqos_state state)
{
	DVFS_ASSERT(platform);

	if (!platform->devfreq_status)
		return 0;

	switch (state) {
		case GPU_CONTROL_PM_QOS_INIT:
			pm_qos_add_request(&proactive_mif_min_qos, PM_QOS_BUS_THROUGHPUT, 0);
			pm_qos_add_request(&proactive_apollo_min_qos, PM_QOS_CLUSTER0_FREQ_MIN, 0);
			pm_qos_add_request(&proactive_atlas_min_qos, PM_QOS_CLUSTER1_FREQ_MIN, 0);
			if (!platform->pmqos_int_disable)
				pm_qos_add_request(&proactive_int_min_qos, PM_QOS_DEVICE_THROUGHPUT, 0);

#ifdef CONFIG_PWRCAL
			update_cal_table();
#endif
			break;
		case GPU_CONTROL_PM_QOS_DEINIT:
			pm_qos_remove_request(&proactive_mif_min_qos);
			pm_qos_remove_request(&proactive_apollo_min_qos);
			pm_qos_remove_request(&proactive_atlas_min_qos);
			if (!platform->pmqos_int_disable)
				pm_qos_remove_request(&proactive_int_min_qos);
			break;
		case GPU_CONTROL_PM_QOS_RESET:
			pm_qos_update_request(&proactive_mif_min_qos, 0);
			pm_qos_update_request(&proactive_apollo_min_qos, 0);
			pm_qos_update_request(&proactive_atlas_min_qos, 0);
		default:
			break;
	}

	return 0;
}

int gpu_mif_min_pmqos(struct exynos_context *platform, int mif_step)
{
	DVFS_ASSERT(platform);

	if(!platform->devfreq_status)
		return 0;

	pm_qos_update_request_timeout(&proactive_mif_min_qos, platform->mif_table[mif_step], 30000);

	return 0;
}

int gpu_int_min_pmqos(struct exynos_context *platform, int int_step)
{
	DVFS_ASSERT(platform);

	if(!platform->devfreq_status)
		return 0;

	pm_qos_update_request_timeout(&proactive_int_min_qos, platform->int_table[int_step], 30000);

	return 0;
}

int gpu_apollo_min_pmqos(struct exynos_context *platform, int apollo_step)
{
	DVFS_ASSERT(platform);

	if(!platform->devfreq_status)
		return 0;

	pm_qos_update_request_timeout(&proactive_apollo_min_qos, platform->apollo_table[apollo_step], 30000);

	return 0;
}

int gpu_atlas_min_pmqos(struct exynos_context *platform, int atlas_step)
{
	DVFS_ASSERT(platform);

	if(!platform->devfreq_status)
		return 0;

	pm_qos_update_request_timeout(&proactive_atlas_min_qos, platform->atlas_table[atlas_step], 30000);

	return 0;
}
#endif
