/* drivers/gpu/arm/.../platform/gpu_perf.c
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
 * @file gpu_perf.c
 * DVFS
 */

#include <linux/io.h>
#include <linux/math64.h>
#include "gpu_perf.h"

void __iomem *g3d0_perfmon_regs;
void __iomem *g3d1_perfmon_regs;

static void g3d0_perfmon_regs_reset(void)
{
	__raw_writel(0x0, g3d0_perfmon_regs+0x0004);
	__raw_writel(0x6, g3d0_perfmon_regs+0x0004);
	__raw_writel(0x8000000F, g3d0_perfmon_regs+0x0008);
	__raw_writel(0x2, g3d0_perfmon_regs+0x0200);
	__raw_writel(0x0, g3d0_perfmon_regs+0x0204);
	__raw_writel(0x0, g3d0_perfmon_regs+0x0208);
	__raw_writel(0x24, g3d0_perfmon_regs+0x020C);

	__raw_writel(0x1, g3d0_perfmon_regs+0x0004);
}

static unsigned int g3d0_perfmon_regs_update(void)
{
	unsigned long long a;
	unsigned long long b;

	__raw_writel(0x0, g3d0_perfmon_regs+0x0004);
	a = (((unsigned long long)__raw_readl(g3d0_perfmon_regs+0x0044))&0xff)<<32;
	a += (unsigned long long)__raw_readl(g3d0_perfmon_regs+0x0040);
	b = (unsigned long long)__raw_readl(g3d0_perfmon_regs+0x0034);

	if (b == 0)
		return 0;

	return (unsigned int)div_u64(a, b);
}

static void g3d1_perfmon_regs_reset(void)
{
	__raw_writel(0x0, g3d1_perfmon_regs+0x0004);
	__raw_writel(0x6, g3d1_perfmon_regs+0x0004);
	__raw_writel(0x8000000F, g3d1_perfmon_regs+0x0008);
	__raw_writel(0x2, g3d1_perfmon_regs+0x0200);
	__raw_writel(0x0, g3d1_perfmon_regs+0x0204);
	__raw_writel(0x0, g3d1_perfmon_regs+0x0208);
	__raw_writel(0x24, g3d1_perfmon_regs+0x020C);

	__raw_writel(0x1, g3d1_perfmon_regs+0x0004);
}

static unsigned int g3d1_perfmon_regs_update(void)
{
	unsigned long long a;
	unsigned long long b;

	__raw_writel(0x0, g3d1_perfmon_regs+0x0004);
	a = (((unsigned long long)__raw_readl(g3d1_perfmon_regs+0x0044))&0xff)<<32;
	a += (unsigned long long)__raw_readl(g3d1_perfmon_regs+0x0040);
	b = (unsigned long long)__raw_readl(g3d1_perfmon_regs+0x0034);

	if (b == 0)
		return 0;

	return (unsigned int)div_u64(a, b);
}

void exynos_gpu_perf_update(unsigned int *perfvalue)
{
	unsigned int perfvalue0 = 0, perfvalue1 = 0;

	perfvalue0 = g3d0_perfmon_regs_update();
	perfvalue1 = g3d1_perfmon_regs_update();
	if (perfvalue0 > perfvalue1)
		*perfvalue = perfvalue0;
	else
		*perfvalue = perfvalue1;

}

int exynos_gpu_perf_reset(void)
{
	g3d0_perfmon_regs_reset();
	g3d1_perfmon_regs_reset();

	return 0;
}

int exynos_gpu_perf_init(void)
{
	g3d0_perfmon_regs = ioremap(0x14A10000, SZ_32);
	g3d1_perfmon_regs = ioremap(0x14A30000, SZ_32);

	return 0;
}

int exynos_gpu_perf_deinit(void)
{
	return 0;

}
