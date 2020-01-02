/* drivers/gpu/arm/.../platform/gpu_dvfs_governor.h
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
 * @file gpu_dvfs_governor.h
 * DVFS
 */

#ifndef _GPU_DVFS_GOVERNOR_H_
#define _GPU_DVFS_GOVERNOR_H_

typedef enum {
	G3D_DVFS_GOVERNOR_DEFAULT = 0,
	G3D_DVFS_GOVERNOR_INTERACTIVE,
	G3D_DVFS_GOVERNOR_STATIC,
	G3D_DVFS_GOVERNOR_BOOSTER,
	G3D_MAX_GOVERNOR_NUM,
} gpu_governor_type;

void gpu_dvfs_update_start_clk(int governor_type, int clk);
void gpu_dvfs_update_table(int governor_type, gpu_dvfs_info *table);
void gpu_dvfs_update_table_size(int governor_type, int size);
void *gpu_dvfs_get_governor_info(void);
int gpu_dvfs_decide_next_freq(struct kbase_device *kbdev, int utilization);
int gpu_dvfs_governor_setting(struct exynos_context *platform, int governor_type);
int gpu_dvfs_governor_init(struct kbase_device *kbdev);

#endif /* _GPU_DVFS_GOVERNOR_H_ */
