/* drivers/gpu/arm/.../platform/gpu_ipa.c
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
 * @file gpu_ipa.c
 * DVFS
 */

#include <mali_kbase.h>
#include "mali_kbase_platform.h"

#include "gpu_ipa.h"
#include "gpu_control.h"
#include "gpu_dvfs_handler.h"

#define CREATE_TRACE_POINTS
#include "mali_power.h"
#undef  CREATE_TRACE_POINTS

extern struct kbase_device *pkbdev;

#ifdef CONFIG_MALI_DVFS
static void gpu_ipa_trace_utilisation(struct kbase_device *kbdev)
{
	int utilisation;
	int norm_utilisation;
	int freq_for_norm;

	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;
	if (!platform) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: platform context (0x%p) is not initialized\n", __func__, platform);
		return ;
	}

	/* Can expand this to only trace when utilisation changed, to avoid too
	 * much trace output and losing the part we're interested in */

	utilisation = platform->env_data.utilization;
	norm_utilisation = platform->norm_utilisation;
	freq_for_norm = platform->freq_for_normalisation;

	trace_mali_utilization_stats(utilisation, norm_utilisation, freq_for_norm);
}

static unsigned int gpu_ipa_dvfs_max_allowed_freq(struct kbase_device *kbdev)
{
	gpu_dvfs_info *dvfs_max_info;
	int max_thermal_step = -1;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;
	int max_step;

	if (!platform) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: platform context (0x%p) is not initialized\n", __func__, platform);
		return 0xffffffff;
	}

	max_step = gpu_dvfs_get_level(platform->gpu_max_clock);

	/* Account for Throttling Lock */
#ifdef CONFIG_EXYNOS_THERMAL
	max_thermal_step = gpu_dvfs_get_level(platform->gpu_max_clock);
#endif /* CONFIG_EXYNOS_THERMAL */
	if (max_thermal_step <= gpu_dvfs_get_level(platform->gpu_min_clock) && max_thermal_step > max_step)
		max_step = max_thermal_step;

	/* NOTE: This is the absolute maximum, not taking into account any tmu
	 * throttling */
	dvfs_max_info = &(platform->table[max_step]);
	return dvfs_max_info->clock;
}

void gpu_ipa_dvfs_calc_norm_utilisation(struct kbase_device *kbdev)
{
	int cur_freq;
	unsigned int cur_vol;
	int max_freq;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;
	int cur_utilisation;
	gpu_dvfs_info *dvfs_cur_info;

	if (!platform) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: platform context (0x%p) is not initialized\n", __func__, platform);
		return ;
	}

	/* TODO:
	 * - Other callers of kbase_platform_dvfs_set_level()
	 */

	/* Get Current Op point */
	/* This is before mali_dvfs_event_proc queued, so the dvfs 'step' is taken before we change frequency */
	cur_utilisation = platform->env_data.utilization;
	dvfs_cur_info = &(platform->table[platform->step]); /* dvfs_status under spinlock */
	cur_freq = (int)dvfs_cur_info->clock;

	cur_vol = dvfs_cur_info->voltage/10000;
	/* Get Max Op point */
	max_freq = gpu_ipa_dvfs_max_allowed_freq(kbdev);

	/* Calculate */
	platform->norm_utilisation = (cur_utilisation * cur_freq)/max_freq;
	/* Store what frequency was used for normalization */
	platform->freq_for_normalisation = cur_freq;
	platform->power = div_u64((u64)platform->ipa_power_coeff_gpu * cur_freq * cur_vol * cur_vol, 1000000);
	/* adding an extra 0 for division in order to compensate for GPU coefficient unit change */

	gpu_ipa_trace_utilisation(kbdev);
}

int gpu_ipa_dvfs_get_norm_utilisation(struct kbase_device *kbdev)
{
	unsigned long flags;
	int norm_utilisation = 0;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;
	if (!platform) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: platform context (0x%p) is not initialized\n", __func__, platform);
		return -1;
	}

	spin_lock_irqsave(&platform->gpu_dvfs_spinlock, flags);
	norm_utilisation = platform->norm_utilisation;
	spin_unlock_irqrestore(&platform->gpu_dvfs_spinlock, flags);

	return norm_utilisation;
}
#endif /* CONFIG_MALI_DVFS */

int kbase_platform_dvfs_freq_to_power(int freq)
{
#ifdef CONFIG_MALI_DVFS
	int level;
	unsigned int vol;
	unsigned long flags;
	unsigned long long power;
	struct kbase_device *kbdev = pkbdev;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;

	if (!platform) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: platform context (0x%p) is not initialized\n", __func__, platform);
		return -1;
	}

	if (0 == freq) {
		spin_lock_irqsave(&platform->gpu_dvfs_spinlock, flags);
		power = platform->power;
		spin_unlock_irqrestore(&platform->gpu_dvfs_spinlock, flags);
	} else {
		for (level = gpu_dvfs_get_level(platform->gpu_max_clock); level <= gpu_dvfs_get_level(platform->gpu_min_clock); level++)
			if (platform->table[level].clock == freq)
				break;

		if (level <= gpu_dvfs_get_level(platform->gpu_min_clock)) {
			vol = platform->table[level].voltage / 10000;
			power = div_u64((u64)platform->ipa_power_coeff_gpu * freq * vol * vol, 1000000);
		} else {
			power = 0;
		}
	}

	return (int)power;
#else
	return 0;
#endif /* CONFIG_MALI_DVFS */
}

int kbase_platform_dvfs_power_to_freq(int power)
{
#ifdef CONFIG_MALI_DVFS
	int level, freq;
	unsigned int vol;
	u64 _power;
	struct kbase_device *kbdev = pkbdev;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;

	if (!platform) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: platform context (0x%p) is not initialized\n", __func__, platform);
		return -1;
	}

	for (level = gpu_dvfs_get_level(platform->gpu_min_clock); level >= gpu_dvfs_get_level(platform->gpu_max_clock); level--) {
		vol = platform->table[level].voltage / 10000;
		freq = platform->table[level].clock;
		_power = div_u64((u64)platform->ipa_power_coeff_gpu * freq * vol * vol, 1000000);
		if ((int)_power >= power)
			break;
	}

	return platform->table[level].clock;
#else
	return 0;
#endif /* CONFIG_MALI_DVFS */
}

/**
 * Get a number of statsistics under the same lock, so they are all 'in sync'
 */
void gpu_ipa_dvfs_get_utilisation_stats(struct mali_debug_utilisation_stats *stats)
{
#ifdef CONFIG_MALI_DVFS
	unsigned long flags;
	struct kbase_device *kbdev = pkbdev;
	struct exynos_context *platform = (struct exynos_context *)kbdev->platform_context;
	if (!platform) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: platform context (0x%p) is not initialized\n", __func__, platform);
		return ;
	}

	spin_lock_irqsave(&platform->gpu_dvfs_spinlock, flags);
	stats->s.utilisation = platform->env_data.utilization;
	stats->s.norm_utilisation = platform->norm_utilisation;
	stats->s.freq_for_norm =platform->freq_for_normalisation;
	stats->time_busy = platform->time_busy;
	stats->time_idle = platform->time_idle;
	stats->time_tick = platform->time_tick;
	spin_unlock_irqrestore(&platform->gpu_dvfs_spinlock, flags);
#endif /* CONFIG_MALI_DVFS */
}

int gpu_ipa_dvfs_max_lock(int clock)
{
#ifdef CONFIG_MALI_DVFS
	struct kbase_device *kbdev = pkbdev;
	struct exynos_context *platform = (struct exynos_context *)kbdev->platform_context;

	if (!platform) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: platform context (0x%p) is not initialized\n", __func__, platform);
		return -ENODEV;
	}

	gpu_dvfs_clock_lock(GPU_DVFS_MAX_LOCK, IPA_LOCK, clock);
#endif /* CONFIG_MALI_DVFS */
	return 0;
}

int gpu_ipa_dvfs_max_unlock(void)
{
#ifdef CONFIG_MALI_DVFS
	struct kbase_device *kbdev = pkbdev;
	struct exynos_context *platform = (struct exynos_context *)kbdev->platform_context;

	if (!platform) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: platform context (0x%p) is not initialized\n", __func__, platform);
		return -ENODEV;
	}

	gpu_dvfs_clock_lock(GPU_DVFS_MAX_UNLOCK, IPA_LOCK, 0);
#endif /* CONFIG_MALI_DVFS */
	return 0;
}

int get_ipa_dvfs_max_freq(void)
{
	struct kbase_device *kbdev = pkbdev;
	struct exynos_context *platform = (struct exynos_context *)kbdev->platform_context;

	if (!platform) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: platform context (0x%p) is not initialized\n", __func__, platform);
		return -ENODEV;
	}

	return platform->gpu_max_clock;
}
