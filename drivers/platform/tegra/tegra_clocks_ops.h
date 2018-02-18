/*
 * drivers/platform/tegra/tegra_clocks_ops.h
 *
 * Copyright (c) 2014-2015 NVIDIA CORPORATION. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#ifndef _TEGRA_CLOCKS_OPS_H
#define _TEGRA_CLOCKS_OPS_H

#include <linux/io.h>
#include <linux/delay.h>

#include "iomap.h"

static void __iomem *reg_clk_base = IO_ADDRESS(TEGRA_CLK_RESET_BASE);

#define clk_writel(value, reg) \
	__raw_writel(value, reg_clk_base + (reg))
#define clk_readl(reg) \
	__raw_readl(reg_clk_base + (reg))

static inline void clk_writel_delay(u32 value, u32 reg)
{
	__raw_writel((value), reg_clk_base + (reg));
	__raw_readl(reg_clk_base + (reg));
	dsb(sy);
	udelay(2);
}

static inline void pll_writel_delay(u32 value, u32 reg)
{
	__raw_writel((value), reg_clk_base + (reg));
	__raw_readl(reg_clk_base + (reg));
	dsb(sy);
	udelay(1);
}

/* PLL base common controls */
#define PLL_BASE_BYPASS			(1<<31)
#define PLL_BASE_ENABLE			(1<<30)
#define PLL_BASE_REF_DISABLE		(1<<29)

/*
 * SDM fractional divisor is 16-bit 2's complement signed number within
 * (-2^12 ... 2^12-1) range. Represented in PLL data structure as unsigned
 * 16-bit value, with "0" divisor mapped to 0xFFFF. Data "0" is used to
 * indicate that SDM is disabled.
 *
 * Effective ndiv value when SDM is enabled: ndiv + 1/2 + sdm_din/2^13
 */
#define PLL_SDM_COEFF		(1 << 13)
#define SDIN_DIN_TO_DATA(din)	((u16)((din) ? : 0xFFFFU))
#define SDIN_DATA_TO_DIN(dat)	(((dat) == 0xFFFFU) ? 0 : (s16)dat)

#define PLL_MISC_CHK_DEFAULT(c, misc_num, default_val, mask)		       \
do {									       \
	u32 boot_val = clk_readl((c)->reg + (c)->u.pll.misc##misc_num);	       \
	boot_val &= (mask);						       \
	default_val &= (mask);						       \
	if (boot_val != (default_val)) {				       \
		pr_warn("%s boot misc" #misc_num " 0x%x : expected 0x%x\n",    \
			(c)->name, boot_val, (default_val));		       \
		pr_warn(" (comparison mask = 0x%x)\n", mask);		       \
			(c)->u.pll.defaults_set = false;		       \
	}								       \
} while (0)

u32 pll_reg_idx_to_addr(struct clk *c, int idx);
u16 pll_get_fixed_mdiv(struct clk *c, unsigned long input_rate);
u32 pll_base_set_div(struct clk *c, u32 m, u32 n, u32 pdiv, u32 val);
void pll_base_parse_cfg(struct clk *c, struct clk_pll_freq_table *cfg);
void pll_clk_set_gain(struct clk *c, struct clk_pll_freq_table *cfg);
void pll_clk_verify_fixed_rate(struct clk *c);
int pll_clk_find_cfg(struct clk *c, struct clk_pll_freq_table *cfg,
	unsigned long rate, unsigned long input_rate, u32 *pdiv);

bool tegra_pll_can_ramp_to_rate(struct clk *c, unsigned long rate);
bool tegra_pll_can_ramp_to_min(struct clk *c, unsigned long *min_rate);
bool tegra_pll_can_ramp_from_min(struct clk *c, unsigned long rate,
				 unsigned long *min_rate);

void tegra_pll_clk_init(struct clk *c);
int tegra_pll_clk_enable(struct clk *c);
void tegra_pll_clk_disable(struct clk *c);
int tegra_pll_clk_set_rate(struct clk *c, unsigned long rate);
int tegra_pll_clk_wait_for_lock(struct clk *c, u32 lock_reg, u32 lock_bits);

void tegra_pll_out_clk_init(struct clk *c);
int tegra_pll_out_clk_enable(struct clk *c);
void tegra_pll_out_clk_disable(struct clk *c);
int tegra_pll_out_clk_set_rate(struct clk *c, unsigned long rate);

#ifdef CONFIG_PM_SLEEP
void tegra_pll_clk_resume_enable(struct clk *c);
void tegra_pll_out_resume_enable(struct clk *c);
#endif

#endif
