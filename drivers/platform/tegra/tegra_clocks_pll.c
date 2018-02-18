/*
 * drivers/platform/tegra/tegra_clocks_pll.c
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/err.h>
#include <linux/clk.h>

#include <mach/hardware.h>

#include <linux/platform/tegra/clock.h>
#include "tegra_clocks_ops.h"

#define PLL_LOCKDET_DELAY 2	/* Lock detection safety delays */

/* PLL helper functions */
u32 pll_reg_idx_to_addr(struct clk *c, int idx)
{
	switch (idx) {
	case PLL_BASE_IDX:
		return c->reg;
	case PLL_MISC0_IDX:
		return c->reg + c->u.pll.misc0;
	case PLL_MISC1_IDX:
		return c->reg + c->u.pll.misc1;
	case PLL_MISC2_IDX:
		return c->reg + c->u.pll.misc2;
	case PLL_MISC3_IDX:
		return c->reg + c->u.pll.misc3;
	case PLL_MISC4_IDX:
		return c->reg + c->u.pll.misc4;
	case PLL_MISC5_IDX:
		return c->reg + c->u.pll.misc5;
	}
	BUG();
	return 0;
}

int tegra_pll_clk_wait_for_lock(struct clk *c, u32 lock_reg, u32 lock_bits)
{
#if USE_PLL_LOCK_BITS
	int i;
	u32 val = 0;

	for (i = 0; i < (c->u.pll.lock_delay / PLL_LOCKDET_DELAY + 1); i++) {
		udelay(PLL_LOCKDET_DELAY);
		val = clk_readl(lock_reg);
		if ((val & lock_bits) == lock_bits) {
			udelay(PLL_LOCKDET_DELAY);
			return 0;
		}
	}

	if (tegra_platform_is_silicon())
		pr_err("Timed out waiting for %s lock bit ([0x%x] = 0x%x)\n",
		       c->name, lock_reg, val);
	return -ETIMEDOUT;
#endif
	udelay(c->u.pll.lock_delay);
	return 0;
}

u16 pll_get_fixed_mdiv(struct clk *c, unsigned long input_rate)
{
	u16 mdiv = input_rate / c->u.pll.cf_min;
	if (c->u.pll.mdiv_default)
		mdiv = min(mdiv, c->u.pll.mdiv_default);
	return mdiv;
}

void pll_clk_verify_fixed_rate(struct clk *c)
{
	unsigned long rate = clk_get_rate_locked(c);

	/*
	 * If boot rate is not equal to expected fixed rate, print
	 * warning, but accept boot rate as new fixed rate, assuming
	 * that warning will be fixed.
	 */
	if (rate != c->u.pll.fixed_rate) {
		WARN(1, "%s: boot rate %lu != fixed rate %lu\n",
		       c->name, rate, c->u.pll.fixed_rate);
		c->u.pll.fixed_rate = rate;
	}
}

void pll_base_parse_cfg(struct clk *c, struct clk_pll_freq_table *cfg)
{
	struct clk_pll_controls *ctrl = c->u.pll.controls;
	struct clk_pll_div_layout *divs = c->u.pll.div_layout;
	u32 base = clk_readl(c->reg);

	cfg->m = (base & divs->mdiv_mask) >> divs->mdiv_shift;
	cfg->n = (base & divs->ndiv_mask) >> divs->ndiv_shift;
	cfg->p = (base & divs->pdiv_mask) >> divs->pdiv_shift;
	if (cfg->p > divs->pdiv_max) {
		WARN(1, "%s pdiv %u is above max %u\n",
		     c->name, cfg->p, divs->pdiv_max);
		cfg->p = divs->pdiv_max;
	}
	cfg->p = c->u.pll.vco_out ? 1 : divs->pdiv_to_p[cfg->p];

	cfg->sdm_data = 0;
	if (ctrl->sdm_en_mask) {
		u32 reg = pll_reg_idx_to_addr(c, ctrl->sdm_ctrl_reg_idx);
		if (ctrl->sdm_en_mask & clk_readl(reg)) {
			reg = pll_reg_idx_to_addr(c, divs->sdm_din_reg_idx);
			cfg->sdm_data = (clk_readl(reg) & divs->sdm_din_mask) >>
				divs->sdm_din_shift;
			cfg->sdm_data = SDIN_DIN_TO_DATA(cfg->sdm_data);
		}
	}
}

void pll_clk_set_gain(struct clk *c, struct clk_pll_freq_table *cfg)
{
	c->mul = cfg->n;
	c->div = cfg->m * cfg->p;

	if (cfg->sdm_data) {
		c->mul = c->mul*PLL_SDM_COEFF + PLL_SDM_COEFF/2 +
			SDIN_DATA_TO_DIN(cfg->sdm_data);
		c->div *= PLL_SDM_COEFF;
	}
}

u32 pll_base_set_div(struct clk *c, u32 m, u32 n, u32 pdiv, u32 val)
{
	struct clk_pll_div_layout *divs = c->u.pll.div_layout;

	val &= ~(divs->mdiv_mask | divs->ndiv_mask);
	val |= m << divs->mdiv_shift | n << divs->ndiv_shift;
	if (!c->u.pll.vco_out) {
		val &= ~divs->pdiv_mask;
		val |= pdiv << divs->pdiv_shift;
	}
	pll_writel_delay(val, c->reg);
	return val;
}

static void pll_sdm_set_din(struct clk *c, struct clk_pll_freq_table *cfg)
{
	u32 reg, val;
	struct clk_pll_controls *ctrl = c->u.pll.controls;
	struct clk_pll_div_layout *divs = c->u.pll.div_layout;

	if (!ctrl->sdm_en_mask)
		return;

	if (cfg->sdm_data) {
		reg = pll_reg_idx_to_addr(c, divs->sdm_din_reg_idx);
		val = clk_readl(reg) & (~divs->sdm_din_mask);
		val |= (SDIN_DATA_TO_DIN(cfg->sdm_data) <<
			divs->sdm_din_shift) & divs->sdm_din_mask;
		pll_writel_delay(val, reg);
	}

	reg = pll_reg_idx_to_addr(c, ctrl->sdm_ctrl_reg_idx);
	val = clk_readl(reg);
	if (!cfg->sdm_data != !(val & ctrl->sdm_en_mask)) {
		val ^= ctrl->sdm_en_mask;
		pll_writel_delay(val, reg);
	}
}

static void pll_clk_start_ss(struct clk *c)
{
	u32 reg, val;
	struct clk_pll_controls *ctrl = c->u.pll.controls;

	if (c->u.pll.defaults_set && ctrl->ssc_en_mask) {
		reg = pll_reg_idx_to_addr(c, ctrl->sdm_ctrl_reg_idx);
		val = clk_readl(reg) | ctrl->ssc_en_mask;
		pll_writel_delay(val, reg);

	}
}

static void pll_clk_stop_ss(struct clk *c)
{
	u32 reg, val;
	struct clk_pll_controls *ctrl = c->u.pll.controls;

	if (ctrl->ssc_en_mask) {
		reg = pll_reg_idx_to_addr(c, ctrl->sdm_ctrl_reg_idx);
		val = clk_readl(reg) & (~ctrl->ssc_en_mask);
		pll_writel_delay(val, reg);
	}
}

/*
 * Common configuration for PLLs with fixed input divider policy:
 * - always set fixed M-value based on the reference rate
 * - always set P-value value 1:1 for output rates above VCO minimum, and
 *   choose minimum necessary P-value for output rates below VCO minimum
 * - calculate N-value based on selected M and P
 * - calculate SDM_DIN fractional part
 */
static int pll_fixed_mdiv_cfg(struct clk *c, struct clk_pll_freq_table *cfg,
	unsigned long rate, unsigned long input_rate, u32 *pdiv)
{
	int p;
	unsigned long cf;

	if (!rate)
		return -EINVAL;

	if (!c->u.pll.vco_out) {
		p = DIV_ROUND_UP(c->u.pll.vco_min, rate);
		p = c->u.pll.round_p_to_pdiv(p, pdiv);
	} else {
		p = rate >= c->u.pll.vco_min ? 1 : -EINVAL;
	}
	if (IS_ERR_VALUE(p))
		return -EINVAL;

	cfg->m = pll_get_fixed_mdiv(c, input_rate);
	cfg->p = p;
	cfg->output_rate = rate * cfg->p;
	if (cfg->output_rate > c->u.pll.vco_max)
		cfg->output_rate = c->u.pll.vco_max;
	cf = input_rate / cfg->m;
	cfg->n = cfg->output_rate / cf;

	cfg->sdm_data = 0;
	if (c->u.pll.controls->sdm_en_mask) {
		unsigned long rem = cfg->output_rate - cf * cfg->n;
		/* If ssc is enabled SDM enabled as well, even for integer n */
		if (rem || c->u.pll.controls->ssc_en_mask) {
			u64 s = rem * PLL_SDM_COEFF;
			do_div(s, cf);
			s -= PLL_SDM_COEFF / 2;
			cfg->sdm_data = SDIN_DIN_TO_DATA(s);
		}
	}

	cfg->input_rate = input_rate;
	return 0;
}

int pll_clk_find_cfg(struct clk *c, struct clk_pll_freq_table *cfg,
	unsigned long rate, unsigned long input_rate, u32 *pdiv)
{
	const struct clk_pll_freq_table *sel;

	/* Check if the target rate is tabulated */
	for (sel = c->u.pll.freq_table; sel->input_rate != 0; sel++) {
		if (sel->input_rate == input_rate && sel->output_rate == rate) {
			if (!c->u.pll.vco_out) {
				int p = c->u.pll.round_p_to_pdiv(sel->p, pdiv);
				BUG_ON(IS_ERR_VALUE(p) || (p != sel->p));
			} else {
				BUG_ON(sel->p != 1);
			}

			if (c->u.pll.controls->dramp_en_mask)
				BUG_ON(sel->m !=
				       pll_get_fixed_mdiv(c, input_rate));
			BUG_ON(!c->u.pll.controls->sdm_en_mask &&
			       sel->sdm_data);
			BUG_ON(c->u.pll.controls->ssc_en_mask &&
			       !sel->sdm_data);
			*cfg = *sel;
			return 0;
		}
	}

	/* Configure out-of-table rate */
	if (pll_fixed_mdiv_cfg(c, cfg, rate, input_rate, pdiv)) {
		pr_err("%s: Failed to set %s out-of-table rate %lu\n",
		       __func__, c->name, rate);
		return -EINVAL;
	}
	return 0;
}

/*
 * Dynamic ramp queries. For "can_ramp" queries assume PLL will be enabled
 * (caller responsibility).
 */
static bool pll_is_dyn_ramp(struct clk *c, struct clk_pll_freq_table *old_cfg,
			    struct clk_pll_freq_table *new_cfg)
{
	return (c->state == ON) && c->u.pll.defaults_set && c->u.pll.dyn_ramp &&
		(new_cfg->m == old_cfg->m) && (new_cfg->p == old_cfg->p);
}

bool tegra_pll_can_ramp_to_rate(struct clk *c, unsigned long rate)
{
	struct clk_pll_freq_table cfg = { };
	struct clk_pll_freq_table old_cfg = { };
	unsigned long input_rate = clk_get_rate(c->parent);

	if (!c->u.pll.dyn_ramp || !c->u.pll.defaults_set)
		return false;

	if (pll_clk_find_cfg(c, &cfg, rate, input_rate, NULL))
		return false;

	pll_base_parse_cfg(c, &old_cfg);

	return (cfg.m == old_cfg.m) && (cfg.p == old_cfg.p);
}

bool tegra_pll_can_ramp_to_min(struct clk *c, unsigned long *min_rate)
{
	struct clk_pll_freq_table cfg = { };

	if (!c->u.pll.dyn_ramp || !c->u.pll.defaults_set)
		return false;

	pll_base_parse_cfg(c, &cfg);

	/* output rate after VCO ramped down to min with current M, P */
	*min_rate = DIV_ROUND_UP(c->u.pll.vco_min, cfg.p);
	return true;
}

bool tegra_pll_can_ramp_from_min(struct clk *c, unsigned long rate,
				 unsigned long *min_rate)
{
	struct clk_pll_freq_table cfg = { };
	unsigned long input_rate = clk_get_rate(c->parent);

	if (!c->u.pll.dyn_ramp || !c->u.pll.defaults_set)
		return false;

	if (pll_clk_find_cfg(c, &cfg, rate, input_rate, NULL))
		return false;

	/* output rate before VCO ramped up from min with target M, P */
	*min_rate = DIV_ROUND_UP(c->u.pll.vco_min, cfg.p);
	return true;
}

/* Common PLL ops */
int tegra_pll_clk_set_rate(struct clk *c, unsigned long rate)
{
	u32 val, pdiv = 0;
	struct clk_pll_freq_table cfg = { };
	struct clk_pll_freq_table old_cfg = { };
	struct clk_pll_controls *ctrl = c->u.pll.controls;
	unsigned long input_rate = clk_get_rate(c->parent);

	pr_debug("%s: %s %lu\n", __func__, c->name, rate);

	if (c->flags & PLL_FIXED) {
		if (rate != c->u.pll.fixed_rate) {
			pr_err("%s: can not change fixed rate %lu to %lu\n",
			       c->name, c->u.pll.fixed_rate, rate);
			return -EINVAL;
		}
		return 0;
	}

	if (pll_clk_find_cfg(c, &cfg, rate, input_rate, &pdiv))
		return -EINVAL;

	pll_base_parse_cfg(c, &old_cfg);

	if ((cfg.m == old_cfg.m) && (cfg.n == old_cfg.n) &&
	    (cfg.p == old_cfg.p) && (cfg.sdm_data == old_cfg.sdm_data) &&
	    c->u.pll.defaults_set)
		return 0;

	pll_clk_set_gain(c, &cfg);

	if (!c->u.pll.defaults_set && c->u.pll.set_defaults) {
		c->ops->disable(c);
		c->u.pll.set_defaults(c, input_rate);

		val = clk_readl(c->reg);
		val = pll_base_set_div(c, cfg.m, cfg.n, pdiv, val);
		pll_sdm_set_din(c, &cfg);

		if (c->state == ON)
			c->ops->enable(c);
		return 0;
	}

	if (pll_is_dyn_ramp(c, &old_cfg, &cfg)) {
		if (!c->u.pll.dyn_ramp(c, &cfg))
			return 0;
	}

	val = clk_readl(c->reg);
	if (c->state == ON) {
		pll_clk_stop_ss(c);
		val &= ~ctrl->enable_mask;
		pll_writel_delay(val, c->reg);
	}

	val = pll_base_set_div(c, cfg.m, cfg.n, pdiv, val);
	pll_sdm_set_din(c, &cfg);

	if (c->state == ON) {
		u32 reg = pll_reg_idx_to_addr(c, ctrl->lock_reg_idx);
		val |= ctrl->enable_mask;
		pll_writel_delay(val, c->reg);
		tegra_pll_clk_wait_for_lock(c, reg, ctrl->lock_mask);
		pll_clk_start_ss(c);
	}

	return 0;
}

int tegra_pll_clk_enable(struct clk *c)
{
	u32 val, reg;
	struct clk_pll_controls *ctrl = c->u.pll.controls;

	pr_debug("%s on clock %s\n", __func__, c->name);

	if (clk_readl(c->reg) & ctrl->enable_mask)
		return 0;	/* already enabled */

	if (ctrl->iddq_mask) {
		reg = pll_reg_idx_to_addr(c, ctrl->iddq_reg_idx);
		val = clk_readl(reg);
		val &= ~ctrl->iddq_mask;
		pll_writel_delay(val, reg);
		udelay(4); /* increase out of IDDQ delay to 5us */
	}

	if (ctrl->reset_mask) {
		reg = pll_reg_idx_to_addr(c, ctrl->reset_reg_idx);
		val = clk_readl(reg);
		val &= ~ctrl->reset_mask;
		pll_writel_delay(val, reg);
	}

	val = clk_readl(c->reg);
	val |= ctrl->enable_mask;
	pll_writel_delay(val, c->reg);

	reg = pll_reg_idx_to_addr(c, ctrl->lock_reg_idx);
	tegra_pll_clk_wait_for_lock(c, reg, ctrl->lock_mask);

	pll_clk_start_ss(c);

	if (val & ctrl->bypass_mask) {
		val &= ~ctrl->bypass_mask;
		pll_writel_delay(val, c->reg);
	}

	return 0;
}

void tegra_pll_clk_disable(struct clk *c)
{
	u32 val, reg;
	struct clk_pll_controls *ctrl = c->u.pll.controls;

	pr_debug("%s on clock %s\n", __func__, c->name);

	pll_clk_stop_ss(c);

	val = clk_readl(c->reg);
	val &= ~ctrl->enable_mask;
	pll_writel_delay(val, c->reg);

	if (ctrl->reset_mask) {
		reg = pll_reg_idx_to_addr(c, ctrl->reset_reg_idx);
		val = clk_readl(reg);
		val |= ctrl->reset_mask;
		pll_writel_delay(val, reg);
	}

	if (ctrl->iddq_mask) {
		reg = pll_reg_idx_to_addr(c, ctrl->iddq_reg_idx);
		val = clk_readl(reg);
		val |= ctrl->iddq_mask;
		pll_writel_delay(val, reg);
	}
}

void tegra_pll_clk_init(struct clk *c)
{
	u32 val;
	unsigned long vco_min;
	unsigned long input_rate = clk_get_rate(c->parent);
	unsigned long cf = input_rate / pll_get_fixed_mdiv(c, input_rate);

	struct clk_pll_freq_table cfg = { };
	struct clk_pll_controls *ctrl = c->u.pll.controls;
	struct clk_pll_div_layout *divs = c->u.pll.div_layout;
	BUG_ON(!ctrl || !divs);

	/*
	 * To avoid vco_min crossover by rounding:
	 * - clip vco_min to exact multiple of comparison frequency if PLL does
	 *   not support SDM fractional divider
	 * - limit increase in vco_min to SDM resolution if SDM is supported
	 */
	vco_min = DIV_ROUND_UP(c->u.pll.vco_min, cf) * cf;
	if (ctrl->sdm_en_mask) {
		c->u.pll.vco_min += DIV_ROUND_UP(cf, PLL_SDM_COEFF);
		vco_min = min(vco_min, c->u.pll.vco_min);
	}
	c->u.pll.vco_min = vco_min;

	if (!c->u.pll.vco_out)
		c->min_rate = DIV_ROUND_UP(c->u.pll.vco_min,
					   divs->pdiv_to_p[divs->pdiv_max]);
	else
		c->min_rate = vco_min;

	val = clk_readl(c->reg);
	c->state = (val & ctrl->enable_mask) ? ON : OFF;

	/*
	 * If PLL is enabled on boot, keep it as is, just check if boot-loader
	 * set correct PLL parameters - if not, parameters will be reset at the
	 * 1st opportunity of reconfiguring PLL.
	 */
	if (c->state == ON) {
		if (c->u.pll.set_defaults) /* check only since PLL is ON */
			c->u.pll.set_defaults(c, input_rate);
		pll_base_parse_cfg(c, &cfg);
		pll_clk_set_gain(c, &cfg);
		if (c->flags & PLL_FIXED)
			pll_clk_verify_fixed_rate(c);
		return;
	}

	/*
	 * Initialize PLL to default state: disabled, reference running;
	 * registers are loaded with default parameters; rate is preset
	 * (close) to 1/4 of minimum VCO rate for PLLs with internal only
	 * VCO, and to VCO minimum for PLLs with VCO exposed to the clock
	 * tree.
	 */
	if (c->u.pll.set_defaults)
		c->u.pll.set_defaults(c, input_rate);

	val = clk_readl(c->reg);
	val &= ~(PLL_BASE_BYPASS | PLL_BASE_ENABLE | PLL_BASE_REF_DISABLE);
	pll_writel_delay(val, c->reg);

	if (c->flags & PLL_FIXED) {
		c->flags &= ~PLL_FIXED;	/* temporarily to allow set rate once */
		c->ops->set_rate(c, c->u.pll.fixed_rate);
		c->flags |= PLL_FIXED;
		pll_clk_verify_fixed_rate(c);
	} else {
		vco_min = DIV_ROUND_UP(vco_min, cf) * cf;
		if (!c->u.pll.vco_out)
			c->ops->set_rate(c, vco_min / 4);
		else
			c->ops->set_rate(c, vco_min);
	}
}

/* Common PLL post-divider ops */
int tegra_pll_out_clk_set_rate(struct clk *c, unsigned long rate)
{
	int p;
	u32 val, pdiv;
	unsigned long vco_rate, flags;
	struct clk *pll = c->parent;
	struct clk_pll_div_layout *divs = pll->u.pll.div_layout;

	pr_debug("%s: %s %lu\n", __func__, c->name, rate);

	if (c->flags & PLL_FIXED) {
		if (rate != c->u.pll.fixed_rate) {
			pr_err("%s: can not change fixed rate %lu to %lu\n",
			       c->name, c->u.pll.fixed_rate, rate);
			return -EINVAL;
		}
		return 0;
	}

	if (!rate)
		return -EINVAL;

	clk_lock_save(pll, &flags);

	vco_rate = clk_get_rate_locked(pll);
	p = DIV_ROUND_UP(vco_rate, rate);
	p = pll->u.pll.round_p_to_pdiv(p, &pdiv);
	if (IS_ERR_VALUE(p)) {
		pr_err("%s: Failed to set %s rate %lu\n",
		       __func__, c->name, rate);
		clk_unlock_restore(pll, &flags);
		return -EINVAL;
	}

	val = clk_readl(pll->reg);
	val &= ~divs->pdiv_mask;
	val |= pdiv << divs->pdiv_shift;
	pll_writel_delay(val, pll->reg);

	c->div = p;
	c->mul = 1;

	clk_unlock_restore(pll, &flags);
	return 0;
}

int tegra_pll_out_clk_enable(struct clk *c)
{
	return 0;
}

void tegra_pll_out_clk_disable(struct clk *c)
{
}

void tegra_pll_out_clk_init(struct clk *c)
{
	u32 p, val;
	unsigned long vco_rate;
	struct clk *pll = c->parent;
	struct clk_pll_div_layout *divs = pll->u.pll.div_layout;

	c->state = c->parent->state;
	c->max_rate = pll->u.pll.vco_max;
	c->min_rate = pll->u.pll.vco_min;

	if (!c->ops->set_rate) {
		/* fixed ratio output */
		if (c->mul && c->div) {
			c->max_rate = c->max_rate * c->mul / c->div;
			c->min_rate = c->min_rate * c->mul / c->div;
		}
		return;
	}
	c->min_rate = DIV_ROUND_UP(c->min_rate,
				   divs->pdiv_to_p[divs->pdiv_max]);

	/* PLL is enabled on boot - just record state */
	if (pll->state == ON) {
		val = clk_readl(pll->reg);
		p = (val & divs->pdiv_mask) >> divs->pdiv_shift;
		if (p > divs->pdiv_max) {
			WARN(1, "%s pdiv %u is above max %u\n",
			     c->name, p, divs->pdiv_max);
			p = divs->pdiv_max;
		}
		p = divs->pdiv_to_p[p];
		c->div = p;
		c->mul = 1;
		if (c->flags & PLL_FIXED)
			pll_clk_verify_fixed_rate(c);
		return;
	}

	if (c->flags & PLL_FIXED) {
		c->flags &= ~PLL_FIXED;	/* temporarily to allow set rate once */
		c->ops->set_rate(c, c->u.pll.fixed_rate);
		c->flags |= PLL_FIXED;
		pll_clk_verify_fixed_rate(c);
	} else {
		/* PLL is disabled - set 1/4 of VCO rate */
		vco_rate = clk_get_rate(pll);
		c->ops->set_rate(c, vco_rate / 4);
	}
}

#ifdef CONFIG_PM_SLEEP
void tegra_pll_clk_resume_enable(struct clk *c)
{
	unsigned long rate = clk_get_rate_all_locked(c->parent);
	u32 val = clk_readl(c->reg);
	enum clk_state state = c->state;

	if (val & c->u.pll.controls->enable_mask)
		return;		/* already resumed */

	/* temporarily sync h/w and s/w states, final sync happens
	   in tegra_clk_resume later */
	c->state = OFF;
	if (c->u.pll.set_defaults)
		c->u.pll.set_defaults(c, rate);

	if (c->flags & PLL_FIXED) {
		c->flags &= ~PLL_FIXED;	/* temporarily to allow set rate once */
		c->ops->set_rate(c, c->u.pll.fixed_rate);
		c->flags |= PLL_FIXED;
	} else {
		rate = clk_get_rate_all_locked(c);
		c->ops->set_rate(c, rate);
	}
	c->ops->enable(c);
	c->state = state;
}

void tegra_pll_out_resume_enable(struct clk *c)
{
	struct clk *pll = c->parent;
	struct clk_pll_div_layout *divs = pll->u.pll.div_layout;
	u32 val = clk_readl(pll->reg);
	u32 pdiv;

	if (val & pll->u.pll.controls->enable_mask)
		return;		/* already resumed */

	/* Restore post divider */
	pll->u.pll.round_p_to_pdiv(c->div, &pdiv);
	val = clk_readl(pll->reg);
	val &= ~divs->pdiv_mask;
	val |= pdiv << divs->pdiv_shift;
	pll_writel_delay(val, pll->reg);

	/* Restore PLL feedback loop */
	tegra_pll_clk_resume_enable(pll);
}
#endif
