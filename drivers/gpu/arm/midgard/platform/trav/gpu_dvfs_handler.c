/* drivers/gpu/arm/.../platform/gpu_dvfs_handler.c
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
 * @file gpu_dvfs_handler.c
 * DVFS
 */

#include <mali_kbase.h>

#include "mali_kbase_platform.h"
#include "gpu_control.h"
#include "gpu_dvfs_handler.h"
#include "gpu_dvfs_governor.h"

extern struct kbase_device *pkbdev;

#ifdef CONFIG_MALI_DVFS
int kbase_platform_dvfs_event(struct kbase_device *kbdev, u32 utilisation)
{
	struct exynos_context *platform;

	int mif , i;
	int *mif_min_table;
	int table_size;

	platform = (struct exynos_context *) kbdev->platform_context;
	mif_min_table = get_mif_table(&table_size);

	DVFS_ASSERT(platform);

	if (!platform->perf_gathering_status) {
		mutex_lock(&platform->gpu_dvfs_handler_lock);
		if (gpu_control_is_power_on(kbdev)) {
			int clk = 0;
			gpu_dvfs_calculate_env_data(kbdev);
			clk = gpu_dvfs_decide_next_freq(kbdev, platform->env_data.utilization);
			gpu_set_target_clk_vol(clk, true);
		}
		mutex_unlock(&platform->gpu_dvfs_handler_lock);
		return 0;
	}
	else {
		mutex_lock(&platform->gpu_dvfs_handler_lock);
		gpu_dvfs_calculate_env_data_ppmu(kbdev);
		mif = platform->table[platform->step].mem_freq;
		if (platform->env_data.perf < 300)
		{
			for (i = 0 ; i < table_size; i++)
			{
				if (mif == mif_min_table[i])
					break;
			}
			if (i > 0)
				mif = mif_min_table[i-1];
		}

		if (gpu_control_is_power_on(kbdev)) {
			int clk = 0;
			gpu_dvfs_calculate_env_data(kbdev);
			clk = gpu_dvfs_decide_next_freq(kbdev, platform->env_data.utilization);
			gpu_set_target_clk_vol(clk, true);
			gpu_mif_pmqos(platform, mif);
		}
		mutex_unlock(&platform->gpu_dvfs_handler_lock);
	}

	GPU_LOG(DVFS_DEBUG, DUMMY, 0u, 0u, "dvfs hanlder is called\n");

	return 0;
}

int gpu_dvfs_handler_init(struct kbase_device *kbdev)
{
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;

	DVFS_ASSERT(platform);

	if (!platform->dvfs_status)
		platform->dvfs_status = true;

#ifdef CONFIG_MALI_DVFS_USER
	platform->mif_min_step = -1;
	platform->int_min_step = -1;
	platform->apollo_min_step = -1;
	platform->atlas_min_step = -1;
	proactive_pm_qos_command(platform, GPU_CONTROL_PM_QOS_INIT);
#endif
	gpu_pm_qos_command(platform, GPU_CONTROL_PM_QOS_INIT);

	gpu_set_target_clk_vol(platform->table[platform->step].clock, false);

	GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "dvfs handler initialized\n");
	return 0;
}

int gpu_dvfs_handler_deinit(struct kbase_device *kbdev)
{
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;

	DVFS_ASSERT(platform);

	if (platform->dvfs_status)
		platform->dvfs_status = false;

	gpu_pm_qos_command(platform, GPU_CONTROL_PM_QOS_DEINIT);
#ifdef CONFIG_MALI_DVFS_USER
	proactive_pm_qos_command(platform, GPU_CONTROL_PM_QOS_DEINIT);
#endif

	GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "dvfs handler de-initialized\n");
	return 0;
}
#else
#define gpu_dvfs_event_proc(q) do { } while (0)
int kbase_platform_dvfs_event(struct kbase_device *kbdev, u32 utilisation)
{
	return 0;
}
#endif /* CONFIG_MALI_DVFS */
