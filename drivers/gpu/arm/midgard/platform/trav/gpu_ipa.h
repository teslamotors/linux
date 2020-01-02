/* drivers/gpu/arm/.../platform/gpu_ipa.h
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
 * @file gpu_ipa.h
 * DVFS
 */

#ifndef _GPU_IPA_H_
#define _GPU_IPA_H_

struct mali_utilisation_stats {
	int utilisation;
	int norm_utilisation;
	int freq_for_norm;
};

struct mali_debug_utilisation_stats {
	struct mali_utilisation_stats s;
	u32 time_busy;
	u32 time_idle;
	int time_tick;
};

int gpu_ipa_dvfs_get_norm_utilisation(struct kbase_device *kbdev);
void gpu_ipa_dvfs_get_utilisation_stats(struct mali_debug_utilisation_stats *stats);
void gpu_ipa_dvfs_calc_norm_utilisation(struct kbase_device *kbdev);
int gpu_ipa_dvfs_max_lock(int clock);
int gpu_ipa_dvfs_max_unlock(void);
int get_ipa_dvfs_max_freq(void);

#endif /* _GPU_IPA_H_ */
