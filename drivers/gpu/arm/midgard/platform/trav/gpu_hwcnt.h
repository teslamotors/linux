/* drivers/gpu/arm/.../platform/gpu_hwcnt.h
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
 * @file gpu_hwcnt.h
 * DVFS
 */
#ifdef MALI_SEC_HWCNT

#ifndef __GPU_HWCNT_H
#define __GPU_HWCNT_H __FILE__

#define MALI_SIZE_OF_HWCBLK 64

enum HWCNT_OFFSET {
	OFFSET_SHADER_20 = 20,
	OFFSET_SHADER_21 = 21,
	OFFSET_TRIPIPE_ACTIVE = 26,
	OFFSET_ARITH_WORDS = 27,
	OFFSET_LS_ISSUES = 32,
	OFFSET_TEX_ISSUES = 42,
};

#define LV1_SHIFT       20
#define LV2_BASE_MASK       0x3ff
#define LV2_PT_MASK     0xff000
#define LV2_SHIFT       12
#define LV1_DESC_MASK       0x3
#define LV2_DESC_MASK       0x2

#define HWC_MODE_UTILIZATION 0x80510010
#define HWC_MODE_GPR_EN 0x80510001
#define HWC_MODE_GPR_DIS 0x80510002

#define HWC_ACC_BUFFER_SIZE     4096		// bytes

extern mali_error kbase_instr_hwcnt_util_dump(struct kbase_device *kbdev);

mali_error exynos_gpu_hwcnt_update(void *dev);

bool hwcnt_check_conditions(struct kbase_device *kbdev);
void hwcnt_value_clear(struct kbase_device *kbdev);
void hwcnt_utilization_equation(struct kbase_device *kbdev);
mali_error hwcnt_get_utilization_resource(struct kbase_device *kbdev);
mali_error hwcnt_get_gpr_resource(struct kbase_device *kbdev, struct kbase_uk_hwcnt_gpr_dump *dump);
extern void hwcnt_accumulate_resource(struct kbase_device *kbdev);
mali_error hwcnt_dump(struct kbase_context *kctx);
void hwcnt_start(struct kbase_device *kbdev);
void hwcnt_stop(struct kbase_device *kbdev);
mali_error hwcnt_setup(struct kbase_context *kctx, struct kbase_uk_hwcnt_setup *setup);
void exynos_hwcnt_init(void *dev);
void exynos_hwcnt_remove(void *dev);
void hwcnt_prepare_suspend(void *dev);
void hwcnt_prepare_resume(void *dev);
void hwcnt_power_up(void *dev);
void hwcnt_power_down(void *dev);
#ifdef CONFIG_MALI_DVFS_USER
mali_error hwcnt_get_data(struct kbase_device *kbdev);
#endif
#endif /* __GPU_HWCNT_H */

#endif
