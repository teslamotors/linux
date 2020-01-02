/* drivers/gpu/arm/.../platform/gpu_perf.h
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
 * @file gpu_perf.h
 * DVFS
 */

#ifndef __GPU_PERF_H
#define __GPU_PERF_H __FILE__

void exynos_gpu_perf_update(unsigned int *perfvalue);
int exynos_gpu_perf_reset(void);
int exynos_gpu_perf_init(void);
int exynos_gpu_perf_deinit(void);
#endif /* __GPU_PERF_H */
