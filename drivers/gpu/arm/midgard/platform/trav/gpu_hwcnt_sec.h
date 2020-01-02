/* drivers/gpu/arm/.../platform/gpu_hwcnt_sec.h
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
 * @file gpu_hwcnt_sec.h
 * DVFS
 */
#ifndef __GPU_HWCNT_SEC_H
#define __GPU_HWCNT_SEC_H __FILE__

#define MALI_SIZE_OF_HWCBLK 64

#define HWC_MODE_GPR_EN 0x80510001
#define HWC_MODE_GPR_DIS 0x80510002

enum HWCNT_OFFSET {
	OFFSET_SHADER_20 = 20,
	OFFSET_SHADER_21 = 21,
	OFFSET_TRIPIPE_ACTIVE = 26,
	OFFSET_ARITH_WORDS = 27,
	OFFSET_LS_ISSUES = 32,
	OFFSET_TEX_ISSUES = 42,
};

void dvfs_hwcnt_attach(void *dev);
void dvfs_hwcnt_update(void *dev);
void dvfs_hwcnt_detach(void *dev);
void dvfs_hwcnt_enable(void *dev);
void dvfs_hwcnt_disable(void *dev);
void dvfs_hwcnt_force_start(void *dev);
void dvfs_hwcnt_force_stop(void *dev);
void dvfs_hwcnt_gpr_enable(struct kbase_device *kbdev, bool flag);
void dvfs_hwcnt_get_gpr_resource(struct kbase_device *kbdev, struct kbase_uk_hwcnt_gpr_dump *dump);
void dvfs_hwcnt_get_resource(struct kbase_device *kbdev);
void dvfs_hwcnt_clear_tripipe(struct kbase_device *kbdev);
void dvfs_hwcnt_utilization_equation(struct kbase_device *kbdev);

#endif
