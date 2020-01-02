/* drivers/gpu/arm/.../platform/mali_kbase_platform_uku.h
 *
 * Copyright 2011 by S.LSI. Samsung Electronics Inc.
 * San#24, Nongseo-Dong, Giheung-Gu, Yongin, Korea
 *
 * Samsung SoC Mali-T Series platform-dependent codes
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software FoundatIon.
 */

/**
 * @file mali_kbase_platform_uku.h
 * Platform-dependent ioctl parameter
 */

#ifndef _KBASE_PLATFORM_UKU_H_
#define _KBASE_PLATFORM_UKU_H_

#include "mali_uk.h"
#include "mali_malisw.h"
#include "mali_base_kernel.h"

#ifdef MALI_SEC_HWCNT
struct kbase_uk_hwcnt_gpr_dump {
	union uk_header header;
	u32 shader_20;
	u32 shader_21;
};
#endif

/* MALI_SEC_INTEGRATION */
#define TMU_INDEX_MAX 10
struct kbase_uk_tmu_skip {
	union uk_header header;
	/* IN */
	u32 temperature[TMU_INDEX_MAX];
	u32 skip_count[TMU_INDEX_MAX];
	u32 num_ratiometer;
	/* OUT */
};

/* MALI_SEC_INTEGRATION */
struct kbase_uk_vsync_skip {
	union uk_header header;
	/* IN */
	u32 skip_count;
	/* OUT */
};

struct kbase_uk_custom_command {
	union uk_header header;
	u32       enabled;
	u32       padding;
	/* MALI_SEC_SECURE_RENDERING */
	u64       flags;
};

#endif
