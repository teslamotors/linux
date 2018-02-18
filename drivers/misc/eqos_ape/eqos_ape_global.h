/*
 * eqos_ape_global.h
 *
 * A header file for AMISC
 *
 * Copyright (C) 2016 NVIDIA Corporation. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __EQOS_APE_GLOBAL_H__
#define __EQOS_APE_GLOBAL_H__

#include <linux/platform_device.h>
#include <linux/clk.h>
#include <mach/clk.h>

#define AMISC_IDLE_0				             0x54

#define AMISC_APE_TSC_CTRL_0_0			         0xc0
#define AMISC_APE_TSC_CTRL_NMODULE_0_0_MASK(x)   (x << 16)
#define AMISC_APE_TSC_CTRL_NFRACT_0_0_MASK(x)    (x)

#define AMISC_APE_TSC_CTRL_1_0			         0xc4
#define AMISC_APE_TSC_CTRL_NINT_1_0_MASK(x)      (x)

#define AMISC_APE_TSC_CTRL_2_0			         0xc8
#define AMISC_APE_TSC_CTRL_2_0_DIGITAL		     0x3b9aca00
#define AMISC_APE_TSC_CTRL_2_0_BINARY		     0x80000000

#define AMISC_APE_TSC_CTRL_3_0			         0xcc
#define AMISC_APE_TSC_CTRL_3_0_TRIGGER		     (1 << 0)
#define AMISC_APE_TSC_CTRL_3_0_RESET		     (1 << 1)
#define AMISC_APE_TSC_CTRL_3_0_COPY		         (1 << 2)
#define AMISC_APE_TSC_CTRL_3_0_ENABLE		     (1 << 31)
#define AMISC_APE_TSC_CTRL_3_0_DISABLE           (0 << 31)

#define AMISC_APE_RT_TSC_NS_0			         0xd0
#define AMISC_APE_RT_TSC_SEC_0			         0xd4
#define AMISC_APE_SNAP_TSC_NS_0			         0xd8
#define AMISC_APE_SNAP_TSC_SEC_0		         0xdc
#define AMISC_EAVB_SNAP_TSC_NS_0		         0xe0
#define AMISC_EAVB_SNAP_TSC_SEC_0		         0xe4

enum {
	AMISC_IDLE,
	AMISC_EAVB,
	AMISC_MAX_REG,
};

struct eqos_drvdata {
	void __iomem **base_regs;
	struct platform_device *pdev;
	int first_sync;
	u64 ape_sec_snap;
	u64 eavb_sec_snap;
	u64 ape_ns_snap;
	u64 eavb_ns_snap;
	struct clk *pll_a_clk;
	struct clk *pll_a_out0_clk;
	struct clk *ape_clk;
	struct clk *ape_clk_parent;
	int pll_a_clk_rate;
};


void amisc_clk_init(void);
void amisc_clk_deinit(void);
u32 amisc_readl(u32 reg);
void amisc_writel(u32 val, u32 reg);
void amisc_idle_enable(void);
void amisc_idle_disable(void);
int amisc_ape_get_rate(void);
void amisc_ape_set_rate(int rate);
int amisc_plla_get_rate(void);
void amisc_plla_set_rate(int rate);

#endif /* __EQOS_APE_GLOBAL_H__ */
