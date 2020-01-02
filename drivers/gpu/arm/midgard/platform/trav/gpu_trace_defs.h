/* drivers/gpu/arm/.../platform/gpu_treace_defs.h
 *
 * Copyright 2011 by S.LSI. Samsung Electronics Inc.
 * San#24, Nongseo-Dong, Giheung-Gu, Yongin, Korea
 *
 * Samsung SoC Mali-T Series DDK porting layer
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software FoundatIon.
 */

/**
 * @file gpu_trace_defs.h
 * DDK porting layer.
 */

#if 0 /* Dummy section to avoid breaking formatting */
int dummy_array[] = {
#endif

	/* MALI_SEC_INTEGRATION */
	KBASE_TRACE_CODE_MAKE_CODE(LSI_GPU_ON), /* gpu on */
	KBASE_TRACE_CODE_MAKE_CODE(LSI_GPU_OFF), /* gpu off */
	KBASE_TRACE_CODE_MAKE_CODE(LSI_SUSPEND), /* suspend */
	KBASE_TRACE_CODE_MAKE_CODE(LSI_RESUME), /* resume */
	KBASE_TRACE_CODE_MAKE_CODE(LSI_CLOCK_VALUE), /* clock */
	KBASE_TRACE_CODE_MAKE_CODE(LSI_TMU_VALUE), /* TMU LOCK info */
	KBASE_TRACE_CODE_MAKE_CODE(LSI_VOL_VALUE), /* voltage */
	KBASE_TRACE_CODE_MAKE_CODE(LSI_REGISTER_DUMP), /* CMU & PMU info */
	KBASE_TRACE_CODE_MAKE_CODE(LSI_CLOCK_ON), /* GPU CLOCK ON */
	KBASE_TRACE_CODE_MAKE_CODE(LSI_CLOCK_OFF), /* GPU CLOCK OFF*/

	KBASE_TRACE_CODE_MAKE_CODE(LSI_HWCNT_ON_DVFS), /* HWCNT ON DVFS */
	KBASE_TRACE_CODE_MAKE_CODE(LSI_HWCNT_OFF_DVFS), /* HWCNT OFF DVFS */
	KBASE_TRACE_CODE_MAKE_CODE(LSI_HWCNT_ON_GPR), /* HWCNT ON GPR */
	KBASE_TRACE_CODE_MAKE_CODE(LSI_HWCNT_OFF_GPR), /* HWCNT OFF GPR */
	KBASE_TRACE_CODE_MAKE_CODE(LSI_HWCNT_BT_ON), /* HWCNT BT ON */
	KBASE_TRACE_CODE_MAKE_CODE(LSI_HWCNT_BT_OFF), /* HWCNT BT OFF */
	KBASE_TRACE_CODE_MAKE_CODE(LSI_HWCNT_VSYNC_SKIP), /* HWCNT VSYNC SKIP */

	KBASE_TRACE_CODE_MAKE_CODE(LSI_CHECKSUM), /* CHECKSUM*/
	KBASE_TRACE_CODE_MAKE_CODE(LSI_GPU_MAX_LOCK), /* GPU MAX CLOCK LOCK */
	KBASE_TRACE_CODE_MAKE_CODE(LSI_GPU_MIN_LOCK), /* GPU MIN CLOCK LOCK */

	KBASE_TRACE_CODE_MAKE_CODE(LSI_GPU_SECURE), /* GPU Secure Rendering */
#if 0
};
#endif
