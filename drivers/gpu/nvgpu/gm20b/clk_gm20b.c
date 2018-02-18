/*
 * GM20B Clocks
 *
 * Copyright (c) 2014-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/clk.h>
#include <linux/delay.h>	/* for mdelay */
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/clk/tegra.h>
#include <linux/tegra-fuse.h>

#include "gk20a/gk20a.h"
#include "hw_trim_gm20b.h"
#include "hw_timer_gm20b.h"
#include "hw_therm_gm20b.h"
#include "hw_fuse_gm20b.h"
#include "clk_gm20b.h"

#define gk20a_dbg_clk(fmt, arg...) \
	gk20a_dbg(gpu_dbg_clk, fmt, ##arg)

#define DFS_DET_RANGE	6	/* -2^6 ... 2^6-1 */
#define SDM_DIN_RANGE	12	/* -2^12 ... 2^12-1 */
#define DFS_TESTOUT_DET	BIT(0)
#define DFS_EXT_CAL_EN	BIT(9)
#define DFS_EXT_STROBE	BIT(16)

#define BOOT_GPU_UV	1000000	/* gpu rail boot voltage 1.0V */
#define ADC_SLOPE_UV	10000	/* default ADC detection slope 10mV */

#define DVFS_SAFE_MARGIN	10	/* 10% */
static unsigned long dvfs_safe_max_freq;

static struct pll_parms gpc_pll_params = {
	128000,  2600000,	/* freq */
	1300000, 2600000,	/* vco */
	12000,   38400,		/* u */
	1, 255,			/* M */
	8, 255,			/* N */
	1, 31,			/* PL */
	-165230, 214007,	/* DFS_COEFF */
	0, 0,			/* ADC char coeff - to be read from fuses */
	0x7 << 3,		/* vco control in NA mode */
	500,			/* Locking and ramping timeout */
	40,			/* Lock delay in NA mode */
	5,			/* IDDQ mode exit delay */
};

#ifdef CONFIG_DEBUG_FS
static int clk_gm20b_debugfs_init(struct gk20a *g);
#endif
static void clk_setup_slide(struct gk20a *g, u32 clk_u);

#define DUMP_REG(addr_func) \
do {									\
	addr = trim_sys_##addr_func##_r();				\
	data = gk20a_readl(g, addr);					\
	pr_info(#addr_func "[0x%x] = 0x%x\n", addr, data);		\
} while (0)

static void dump_gpc_pll(struct gk20a *g, struct pll *gpll, u32 last_cfg)
{
	u32 addr, data;

	pr_info("**** GPCPLL DUMP ****");
	pr_info("gpcpll s/w M=%u N=%u P=%u\n", gpll->M, gpll->N, gpll->PL);
	pr_info("gpcpll_cfg_last = 0x%x\n", last_cfg);
	DUMP_REG(gpcpll_cfg);
	DUMP_REG(gpcpll_coeff);
	DUMP_REG(sel_vco);
	pr_info("\n");
}

/* 1:1 match between post divider settings and divisor value */
static inline u32 pl_to_div(u32 pl)
{
	return pl;
}

static inline u32 div_to_pl(u32 div)
{
	return div;
}

#define PLDIV_GLITCHLESS 1

#if PLDIV_GLITCHLESS
/*
 * Post divider tarnsition is glitchless only if there is common "1" in binary
 * representation of old and new settings.
 */
static u32 get_interim_pldiv(u32 old_pl, u32 new_pl)
{
	u32 pl;

	if (old_pl & new_pl)
		return 0;

	pl = old_pl | BIT(ffs(new_pl) - 1);	/* pl never 0 */
	new_pl |= BIT(ffs(old_pl) - 1);

	return min(pl, new_pl);
}
#endif

/* Calculate and update M/N/PL as well as pll->freq
    ref_clk_f = clk_in_f;
    u_f = ref_clk_f / M;
    vco_f = u_f * N = ref_clk_f * N / M;
    PLL output = gpc2clk = target clock frequency = vco_f / pl_to_pdiv(PL);
    gpcclk = gpc2clk / 2; */
static int clk_config_pll(struct clk_gk20a *clk, struct pll *pll,
	struct pll_parms *pll_params, u32 *target_freq, bool best_fit)
{
	u32 min_vco_f, max_vco_f;
	u32 best_M, best_N;
	u32 low_PL, high_PL, best_PL;
	u32 m, n, n2;
	u32 target_vco_f, vco_f;
	u32 ref_clk_f, target_clk_f, u_f;
	u32 delta, lwv, best_delta = ~0;
	u32 pl;

	BUG_ON(target_freq == NULL);

	gk20a_dbg_fn("request target freq %d MHz", *target_freq);

	ref_clk_f = pll->clk_in;
	target_clk_f = *target_freq;
	max_vco_f = pll_params->max_vco;
	min_vco_f = pll_params->min_vco;
	best_M = pll_params->max_M;
	best_N = pll_params->min_N;
	best_PL = pll_params->min_PL;

	target_vco_f = target_clk_f + target_clk_f / 50;
	if (max_vco_f < target_vco_f)
		max_vco_f = target_vco_f;

	/* Set PL search boundaries. */
	high_PL = div_to_pl((max_vco_f + target_vco_f - 1) / target_vco_f);
	high_PL = min(high_PL, pll_params->max_PL);
	high_PL = max(high_PL, pll_params->min_PL);

	low_PL = div_to_pl(min_vco_f / target_vco_f);
	low_PL = min(low_PL, pll_params->max_PL);
	low_PL = max(low_PL, pll_params->min_PL);

	gk20a_dbg_info("low_PL %d(div%d), high_PL %d(div%d)",
			low_PL, pl_to_div(low_PL), high_PL, pl_to_div(high_PL));

	for (pl = low_PL; pl <= high_PL; pl++) {
		target_vco_f = target_clk_f * pl_to_div(pl);

		for (m = pll_params->min_M; m <= pll_params->max_M; m++) {
			u_f = ref_clk_f / m;

			if (u_f < pll_params->min_u)
				break;
			if (u_f > pll_params->max_u)
				continue;

			n = (target_vco_f * m) / ref_clk_f;
			n2 = ((target_vco_f * m) + (ref_clk_f - 1)) / ref_clk_f;

			if (n > pll_params->max_N)
				break;

			for (; n <= n2; n++) {
				if (n < pll_params->min_N)
					continue;
				if (n > pll_params->max_N)
					break;

				vco_f = ref_clk_f * n / m;

				if (vco_f >= min_vco_f && vco_f <= max_vco_f) {
					lwv = (vco_f + (pl_to_div(pl) / 2))
						/ pl_to_div(pl);
					delta = abs(lwv - target_clk_f);

					if (delta < best_delta) {
						best_delta = delta;
						best_M = m;
						best_N = n;
						best_PL = pl;

						if (best_delta == 0 ||
						    /* 0.45% for non best fit */
						    (!best_fit && (vco_f / best_delta > 218))) {
							goto found_match;
						}

						gk20a_dbg_info("delta %d @ M %d, N %d, PL %d",
							delta, m, n, pl);
					}
				}
			}
		}
	}

found_match:
	BUG_ON(best_delta == ~0);

	if (best_fit && best_delta != 0)
		gk20a_dbg_clk("no best match for target @ %dMHz on gpc_pll",
			target_clk_f);

	pll->M = best_M;
	pll->N = best_N;
	pll->PL = best_PL;

	/* save current frequency */
	pll->freq = ref_clk_f * pll->N / (pll->M * pl_to_div(pll->PL));

	*target_freq = pll->freq;

	gk20a_dbg_clk("actual target freq %d kHz, M %d, N %d, PL %d(div%d)",
		*target_freq, pll->M, pll->N, pll->PL, pl_to_div(pll->PL));

	gk20a_dbg_fn("done");

	return 0;
}

/* GPCPLL NA/DVFS mode methods */

/*
 * Read ADC characteristic parmeters from fuses.
 * Determine clibration settings.
 */
static int clk_config_calibration_params(struct gk20a *g)
{
	int slope, offs;
	struct pll_parms *p = &gpc_pll_params;

	if (!tegra_fuse_calib_gpcpll_get_adc(&slope, &offs)) {
		p->uvdet_slope = slope;
		p->uvdet_offs = offs;
	}

	if (!p->uvdet_slope || !p->uvdet_offs) {
		/*
		 * If ADC conversion slope/offset parameters are not fused
		 * (non-production config), report error, but allow to use
		 * boot internal calibration with default slope.
		 */
		gk20a_err(dev_from_gk20a(g), "ADC coeff are not fused\n");
		return -EINVAL;
	}
	return 0;
}

/*
 * Determine DFS_COEFF for the requested voltage. Always select external
 * calibration override equal to the voltage, and set maximum detection
 * limit "0" (to make sure that PLL output remains under F/V curve when
 * voltage increases).
 */
static void clk_config_dvfs_detection(int mv, struct na_dvfs *d)
{
	u32 coeff, coeff_max;
	struct pll_parms *p = &gpc_pll_params;

	coeff_max = trim_sys_gpcpll_dvfs0_dfs_coeff_v(
		trim_sys_gpcpll_dvfs0_dfs_coeff_m());
	coeff = DIV_ROUND_CLOSEST(mv * p->coeff_slope, 1000) + p->coeff_offs;
	coeff = DIV_ROUND_CLOSEST(coeff, 1000);
	coeff = min(coeff, coeff_max);
	d->dfs_coeff = coeff;

	d->dfs_ext_cal = DIV_ROUND_CLOSEST(mv * 1000 - p->uvdet_offs,
					   p->uvdet_slope);
	BUG_ON(abs(d->dfs_ext_cal) >= (1 << DFS_DET_RANGE));
	d->uv_cal = p->uvdet_offs + d->dfs_ext_cal * p->uvdet_slope;
	d->dfs_det_max = 0;
}

/*
 * Solve equation for integer and fractional part of the effective NDIV:
 *
 * n_eff = n_int + 1/2 + SDM_DIN / 2^(SDM_DIN_RANGE + 1) +
 * DVFS_COEFF * DVFS_DET_DELTA / 2^DFS_DET_RANGE
 *
 * The SDM_DIN LSB is finally shifted out, since it is not accessible by s/w.
 */
static void clk_config_dvfs_ndiv(int mv, u32 n_eff, struct na_dvfs *d)
{
	int n, det_delta;
	u32 rem, rem_range;
	struct pll_parms *p = &gpc_pll_params;

	det_delta = DIV_ROUND_CLOSEST(mv * 1000 - p->uvdet_offs,
				      p->uvdet_slope);
	det_delta -= d->dfs_ext_cal;
	det_delta = min(det_delta, d->dfs_det_max);
	det_delta = det_delta * d->dfs_coeff;

	n = (int)(n_eff << DFS_DET_RANGE) - det_delta;
	BUG_ON((n < 0) || (n > (p->max_N << DFS_DET_RANGE)));
	d->n_int = ((u32)n) >> DFS_DET_RANGE;

	rem = ((u32)n) & ((1 << DFS_DET_RANGE) - 1);
	rem_range = SDM_DIN_RANGE + 1 - DFS_DET_RANGE;
	d->sdm_din = (rem << rem_range) - (1 << SDM_DIN_RANGE);
	d->sdm_din = (d->sdm_din >> BITS_PER_BYTE) & 0xff;
}

/* Voltage dependent configuration */
static void clk_config_dvfs(struct gk20a *g, struct pll *gpll)
{
	struct na_dvfs *d = &gpll->dvfs;

	d->mv = tegra_dvfs_predict_mv_at_hz_cur_tfloor(
			clk_get_parent(g->clk.tegra_clk),
			rate_gpc2clk_to_gpu(gpll->freq));
	clk_config_dvfs_detection(d->mv, d);
	clk_config_dvfs_ndiv(d->mv, gpll->N, d);
}

/* Update DVFS detection settings in flight */
static void clk_set_dfs_coeff(struct gk20a *g, u32 dfs_coeff)
{
	u32 data = gk20a_readl(g, trim_gpc_bcast_gpcpll_dvfs2_r());
	data |= DFS_EXT_STROBE;
	gk20a_writel(g, trim_gpc_bcast_gpcpll_dvfs2_r(), data);

	data = gk20a_readl(g, trim_sys_gpcpll_dvfs0_r());
	data = set_field(data, trim_sys_gpcpll_dvfs0_dfs_coeff_m(),
		trim_sys_gpcpll_dvfs0_dfs_coeff_f(dfs_coeff));
	gk20a_writel(g, trim_sys_gpcpll_dvfs0_r(), data);

	data = gk20a_readl(g, trim_gpc_bcast_gpcpll_dvfs2_r());
	udelay(1);
	data &= ~DFS_EXT_STROBE;
	gk20a_writel(g, trim_gpc_bcast_gpcpll_dvfs2_r(), data);
}

static void __maybe_unused clk_set_dfs_det_max(struct gk20a *g, u32 dfs_det_max)
{
	u32 data = gk20a_readl(g, trim_gpc_bcast_gpcpll_dvfs2_r());
	data |= DFS_EXT_STROBE;
	gk20a_writel(g, trim_gpc_bcast_gpcpll_dvfs2_r(), data);

	data = gk20a_readl(g, trim_sys_gpcpll_dvfs0_r());
	data = set_field(data, trim_sys_gpcpll_dvfs0_dfs_det_max_m(),
		trim_sys_gpcpll_dvfs0_dfs_det_max_f(dfs_det_max));
	gk20a_writel(g, trim_sys_gpcpll_dvfs0_r(), data);

	data = gk20a_readl(g, trim_gpc_bcast_gpcpll_dvfs2_r());
	udelay(1);
	data &= ~DFS_EXT_STROBE;
	gk20a_writel(g, trim_gpc_bcast_gpcpll_dvfs2_r(), data);
}

static void clk_set_dfs_ext_cal(struct gk20a *g, u32 dfs_det_cal)
{
	u32 data;

	data = gk20a_readl(g, trim_gpc_bcast_gpcpll_dvfs2_r());
	data &= ~(BIT(DFS_DET_RANGE + 1) - 1);
	data |= dfs_det_cal;
	gk20a_writel(g, trim_gpc_bcast_gpcpll_dvfs2_r(), data);

	data = gk20a_readl(g, trim_sys_gpcpll_dvfs1_r());
	udelay(1);
	if (~trim_sys_gpcpll_dvfs1_dfs_ctrl_v(data) & DFS_EXT_CAL_EN) {
		data = set_field(data, trim_sys_gpcpll_dvfs1_dfs_ctrl_m(),
				 trim_sys_gpcpll_dvfs1_dfs_ctrl_f(
					 DFS_EXT_CAL_EN | DFS_TESTOUT_DET));
		gk20a_writel(g, trim_sys_gpcpll_dvfs1_r(), data);
	}
}

static void clk_setup_dvfs_detection(struct gk20a *g, struct pll *gpll)
{
	struct na_dvfs *d = &gpll->dvfs;

	u32 data = gk20a_readl(g, trim_gpc_bcast_gpcpll_dvfs2_r());
	data |= DFS_EXT_STROBE;
	gk20a_writel(g, trim_gpc_bcast_gpcpll_dvfs2_r(), data);

	data = gk20a_readl(g, trim_sys_gpcpll_dvfs0_r());
	data = set_field(data, trim_sys_gpcpll_dvfs0_dfs_coeff_m(),
		trim_sys_gpcpll_dvfs0_dfs_coeff_f(d->dfs_coeff));
	data = set_field(data, trim_sys_gpcpll_dvfs0_dfs_det_max_m(),
		trim_sys_gpcpll_dvfs0_dfs_det_max_f(d->dfs_det_max));
	gk20a_writel(g, trim_sys_gpcpll_dvfs0_r(), data);

	data = gk20a_readl(g, trim_gpc_bcast_gpcpll_dvfs2_r());
	udelay(1);
	data &= ~DFS_EXT_STROBE;
	gk20a_writel(g, trim_gpc_bcast_gpcpll_dvfs2_r(), data);

	clk_set_dfs_ext_cal(g, d->dfs_ext_cal);
}

/* Enable NA/DVFS mode */
static int clk_enbale_pll_dvfs(struct gk20a *g)
{
	u32 data;
	int delay = gpc_pll_params.iddq_exit_delay; /* iddq & calib delay */
	struct pll_parms *p = &gpc_pll_params;
	bool calibrated = p->uvdet_slope && p->uvdet_offs;

	/* Enable NA DVFS */
	data = gk20a_readl(g, trim_sys_gpcpll_dvfs1_r());
	data |= trim_sys_gpcpll_dvfs1_en_dfs_m();
	gk20a_writel(g, trim_sys_gpcpll_dvfs1_r(), data);

	/* Set VCO_CTRL */
	if (p->vco_ctrl) {
		data = gk20a_readl(g, trim_sys_gpcpll_cfg3_r());
		data = set_field(data, trim_sys_gpcpll_cfg3_vco_ctrl_m(),
				 trim_sys_gpcpll_cfg3_vco_ctrl_f(p->vco_ctrl));
		gk20a_writel(g, trim_sys_gpcpll_cfg3_r(), data);
	}

	/*
	 * If calibration parameters are known (either from fuses, or from
	 * internal calibration on boot) - use them. Internal calibration is
	 * started anyway; it will complete, but results will not be used.
	 */
	if (calibrated) {
		data = gk20a_readl(g, trim_sys_gpcpll_dvfs1_r());
		data |= trim_sys_gpcpll_dvfs1_en_dfs_cal_m();
		gk20a_writel(g, trim_sys_gpcpll_dvfs1_r(), data);
	}

	/* Exit IDDQ mode */
	data = gk20a_readl(g, trim_sys_gpcpll_cfg_r());
	data = set_field(data, trim_sys_gpcpll_cfg_iddq_m(),
			 trim_sys_gpcpll_cfg_iddq_power_on_v());
	gk20a_writel(g, trim_sys_gpcpll_cfg_r(), data);
	gk20a_readl(g, trim_sys_gpcpll_cfg_r());
	udelay(delay);

	/*
	 * Dynamic ramp setup based on update rate, which in DVFS mode on GM20b
	 * is always 38.4 MHz, the same as reference clock rate.
	 */
	clk_setup_slide(g, g->clk.gpc_pll.clk_in);

	if (calibrated)
		return 0;

	/*
	 * If calibration parameters are not fused, start internal calibration,
	 * wait for completion, and use results along with default slope to
	 * calculate ADC offset during boot.
	 */
	data = gk20a_readl(g, trim_sys_gpcpll_dvfs1_r());
	data |= trim_sys_gpcpll_dvfs1_en_dfs_cal_m();
	gk20a_writel(g, trim_sys_gpcpll_dvfs1_r(), data);

	/* Wait for internal calibration done (spec < 2us). */
	do {
		data = gk20a_readl(g, trim_sys_gpcpll_dvfs1_r());
		if (trim_sys_gpcpll_dvfs1_dfs_cal_done_v(data))
			break;
		udelay(1);
		delay--;
	} while (delay > 0);

	if (delay <= 0) {
		gk20a_err(dev_from_gk20a(g), "GPCPLL calibration timeout");
		return -ETIMEDOUT;
	}

	data = gk20a_readl(g, trim_sys_gpcpll_cfg3_r());
	data = trim_sys_gpcpll_cfg3_dfs_testout_v(data);
	p->uvdet_offs = BOOT_GPU_UV - data * ADC_SLOPE_UV;
	p->uvdet_slope = ADC_SLOPE_UV;
	return 0;
}

/* GPCPLL slide methods */
static void clk_setup_slide(struct gk20a *g, u32 clk_u)
{
	u32 data, step_a, step_b;

	switch (clk_u) {
	case 12000:
	case 12800:
	case 13000:			/* only on FPGA */
		step_a = 0x2B;
		step_b = 0x0B;
		break;
	case 19200:
		step_a = 0x12;
		step_b = 0x08;
		break;
	case 38400:
		step_a = 0x04;
		step_b = 0x05;
		break;
	default:
		gk20a_err(dev_from_gk20a(g), "Unexpected reference rate %u kHz",
			  clk_u);
		BUG();
	}

	/* setup */
	data = gk20a_readl(g, trim_sys_gpcpll_cfg2_r());
	data = set_field(data, trim_sys_gpcpll_cfg2_pll_stepa_m(),
			trim_sys_gpcpll_cfg2_pll_stepa_f(step_a));
	gk20a_writel(g, trim_sys_gpcpll_cfg2_r(), data);
	data = gk20a_readl(g, trim_sys_gpcpll_cfg3_r());
	data = set_field(data, trim_sys_gpcpll_cfg3_pll_stepb_m(),
			trim_sys_gpcpll_cfg3_pll_stepb_f(step_b));
	gk20a_writel(g, trim_sys_gpcpll_cfg3_r(), data);
}

static int clk_slide_gpc_pll(struct gk20a *g, struct pll *gpll)
{
	u32 data, coeff;
	u32 nold, sdm_old;
	int ramp_timeout = gpc_pll_params.lock_timeout;

	/* get old coefficients */
	coeff = gk20a_readl(g, trim_sys_gpcpll_coeff_r());
	nold = trim_sys_gpcpll_coeff_ndiv_v(coeff);

	/* do nothing if NDIV is same */
	if (gpll->mode == GPC_PLL_MODE_DVFS) {
		/* in DVFS mode check both integer and fraction */
		coeff = gk20a_readl(g, trim_sys_gpcpll_cfg2_r());
		sdm_old = trim_sys_gpcpll_cfg2_sdm_din_v(coeff);
		if ((gpll->dvfs.n_int == nold) &&
		    (gpll->dvfs.sdm_din == sdm_old))
			return 0;
	} else {
		if (gpll->N == nold)
			return 0;

		/* dynamic ramp setup based on update rate */
		clk_setup_slide(g, gpll->clk_in / gpll->M);
	}

	/* pll slowdown mode */
	data = gk20a_readl(g, trim_sys_gpcpll_ndiv_slowdown_r());
	data = set_field(data,
			trim_sys_gpcpll_ndiv_slowdown_slowdown_using_pll_m(),
			trim_sys_gpcpll_ndiv_slowdown_slowdown_using_pll_yes_f());
	gk20a_writel(g, trim_sys_gpcpll_ndiv_slowdown_r(), data);

	/* new ndiv ready for ramp */
	if (gpll->mode == GPC_PLL_MODE_DVFS) {
		/* in DVFS mode SDM is updated via "new" field */
		coeff = gk20a_readl(g, trim_sys_gpcpll_cfg2_r());
		coeff = set_field(coeff, trim_sys_gpcpll_cfg2_sdm_din_new_m(),
			trim_sys_gpcpll_cfg2_sdm_din_new_f(gpll->dvfs.sdm_din));
		gk20a_writel(g, trim_sys_gpcpll_cfg2_r(), coeff);

		coeff = gk20a_readl(g, trim_sys_gpcpll_coeff_r());
		coeff = set_field(coeff, trim_sys_gpcpll_coeff_ndiv_m(),
			trim_sys_gpcpll_coeff_ndiv_f(gpll->dvfs.n_int));
		udelay(1);
		gk20a_writel(g, trim_sys_gpcpll_coeff_r(), coeff);
	} else {
		coeff = gk20a_readl(g, trim_sys_gpcpll_coeff_r());
		coeff = set_field(coeff, trim_sys_gpcpll_coeff_ndiv_m(),
				trim_sys_gpcpll_coeff_ndiv_f(gpll->N));
		udelay(1);
		gk20a_writel(g, trim_sys_gpcpll_coeff_r(), coeff);
	}

	/* dynamic ramp to new ndiv */
	data = gk20a_readl(g, trim_sys_gpcpll_ndiv_slowdown_r());
	data = set_field(data,
			trim_sys_gpcpll_ndiv_slowdown_en_dynramp_m(),
			trim_sys_gpcpll_ndiv_slowdown_en_dynramp_yes_f());
	udelay(1);
	gk20a_writel(g, trim_sys_gpcpll_ndiv_slowdown_r(), data);

	do {
		udelay(1);
		ramp_timeout--;
		data = gk20a_readl(
			g, trim_gpc_bcast_gpcpll_ndiv_slowdown_debug_r());
		if (trim_gpc_bcast_gpcpll_ndiv_slowdown_debug_pll_dynramp_done_synced_v(data))
			break;
	} while (ramp_timeout > 0);

	if ((gpll->mode == GPC_PLL_MODE_DVFS) && (ramp_timeout > 0)) {
		/* in DVFS mode complete SDM update */
		coeff = gk20a_readl(g, trim_sys_gpcpll_cfg2_r());
		coeff = set_field(coeff, trim_sys_gpcpll_cfg2_sdm_din_m(),
			trim_sys_gpcpll_cfg2_sdm_din_f(gpll->dvfs.sdm_din));
		gk20a_writel(g, trim_sys_gpcpll_cfg2_r(), coeff);
	}

	/* exit slowdown mode */
	data = gk20a_readl(g, trim_sys_gpcpll_ndiv_slowdown_r());
	data = set_field(data,
			trim_sys_gpcpll_ndiv_slowdown_slowdown_using_pll_m(),
			trim_sys_gpcpll_ndiv_slowdown_slowdown_using_pll_no_f());
	data = set_field(data,
			trim_sys_gpcpll_ndiv_slowdown_en_dynramp_m(),
			trim_sys_gpcpll_ndiv_slowdown_en_dynramp_no_f());
	gk20a_writel(g, trim_sys_gpcpll_ndiv_slowdown_r(), data);
	gk20a_readl(g, trim_sys_gpcpll_ndiv_slowdown_r());

	if (ramp_timeout <= 0) {
		gk20a_err(dev_from_gk20a(g), "gpcpll dynamic ramp timeout");
		return -ETIMEDOUT;
	}
	return 0;
}

/* GPCPLL bypass methods */
static int clk_change_pldiv_under_bypass(struct gk20a *g, struct pll *gpll)
{
	u32 data, coeff;

	/* put PLL in bypass before programming it */
	data = gk20a_readl(g, trim_sys_sel_vco_r());
	data = set_field(data, trim_sys_sel_vco_gpc2clk_out_m(),
		trim_sys_sel_vco_gpc2clk_out_bypass_f());
	gk20a_writel(g, trim_sys_sel_vco_r(), data);

	/* change PLDIV */
	coeff = gk20a_readl(g, trim_sys_gpcpll_coeff_r());
	udelay(1);
	coeff = set_field(coeff, trim_sys_gpcpll_coeff_pldiv_m(),
			  trim_sys_gpcpll_coeff_pldiv_f(gpll->PL));
	gk20a_writel(g, trim_sys_gpcpll_coeff_r(), coeff);

	/* put PLL back on vco */
	data = gk20a_readl(g, trim_sys_sel_vco_r());
	udelay(1);
	data = set_field(data, trim_sys_sel_vco_gpc2clk_out_m(),
		trim_sys_sel_vco_gpc2clk_out_vco_f());
	gk20a_writel(g, trim_sys_sel_vco_r(), data);

	return 0;
}

static int clk_lock_gpc_pll_under_bypass(struct gk20a *g, struct pll *gpll)
{
	u32 data, cfg, coeff, timeout;

	/* put PLL in bypass before programming it */
	data = gk20a_readl(g, trim_sys_sel_vco_r());
	data = set_field(data, trim_sys_sel_vco_gpc2clk_out_m(),
		trim_sys_sel_vco_gpc2clk_out_bypass_f());
	gk20a_writel(g, trim_sys_sel_vco_r(), data);

	cfg = gk20a_readl(g, trim_sys_gpcpll_cfg_r());
	udelay(1);
	if (trim_sys_gpcpll_cfg_iddq_v(cfg)) {
		/* get out from IDDQ (1st power up) */
		cfg = set_field(cfg, trim_sys_gpcpll_cfg_iddq_m(),
				trim_sys_gpcpll_cfg_iddq_power_on_v());
		gk20a_writel(g, trim_sys_gpcpll_cfg_r(), cfg);
		gk20a_readl(g, trim_sys_gpcpll_cfg_r());
		udelay(gpc_pll_params.iddq_exit_delay);
	} else {
		/* clear SYNC_MODE before disabling PLL */
		cfg = set_field(cfg, trim_sys_gpcpll_cfg_sync_mode_m(),
				trim_sys_gpcpll_cfg_sync_mode_disable_f());
		gk20a_writel(g, trim_sys_gpcpll_cfg_r(), cfg);
		gk20a_readl(g, trim_sys_gpcpll_cfg_r());

		/* disable running PLL before changing coefficients */
		cfg = set_field(cfg, trim_sys_gpcpll_cfg_enable_m(),
				trim_sys_gpcpll_cfg_enable_no_f());
		gk20a_writel(g, trim_sys_gpcpll_cfg_r(), cfg);
		gk20a_readl(g, trim_sys_gpcpll_cfg_r());
	}

	/* change coefficients */
	if (gpll->mode == GPC_PLL_MODE_DVFS) {
		clk_setup_dvfs_detection(g, gpll);

		coeff = gk20a_readl(g, trim_sys_gpcpll_cfg2_r());
		coeff = set_field(coeff, trim_sys_gpcpll_cfg2_sdm_din_m(),
			trim_sys_gpcpll_cfg2_sdm_din_f(gpll->dvfs.sdm_din));
		gk20a_writel(g, trim_sys_gpcpll_cfg2_r(), coeff);

		coeff = trim_sys_gpcpll_coeff_mdiv_f(gpll->M) |
			trim_sys_gpcpll_coeff_ndiv_f(gpll->dvfs.n_int) |
			trim_sys_gpcpll_coeff_pldiv_f(gpll->PL);
		gk20a_writel(g, trim_sys_gpcpll_coeff_r(), coeff);
	} else {
		coeff = trim_sys_gpcpll_coeff_mdiv_f(gpll->M) |
			trim_sys_gpcpll_coeff_ndiv_f(gpll->N) |
			trim_sys_gpcpll_coeff_pldiv_f(gpll->PL);
		gk20a_writel(g, trim_sys_gpcpll_coeff_r(), coeff);
	}

	/* enable PLL after changing coefficients */
	cfg = gk20a_readl(g, trim_sys_gpcpll_cfg_r());
	cfg = set_field(cfg, trim_sys_gpcpll_cfg_enable_m(),
			trim_sys_gpcpll_cfg_enable_yes_f());
	gk20a_writel(g, trim_sys_gpcpll_cfg_r(), cfg);

	/* just delay in DVFS mode (lock cannot be used) */
	if (gpll->mode == GPC_PLL_MODE_DVFS) {
		gk20a_readl(g, trim_sys_gpcpll_cfg_r());
		udelay(gpc_pll_params.na_lock_delay);
		gk20a_dbg_clk("NA config_pll under bypass: %u (%u) kHz %d mV",
			      gpll->freq, gpll->freq / 2,
			      (trim_sys_gpcpll_cfg3_dfs_testout_v(
				      gk20a_readl(g, trim_sys_gpcpll_cfg3_r()))
			       * gpc_pll_params.uvdet_slope
			       + gpc_pll_params.uvdet_offs) / 1000);
		goto pll_locked;
	}

	/* lock pll */
	cfg = gk20a_readl(g, trim_sys_gpcpll_cfg_r());
	if (cfg & trim_sys_gpcpll_cfg_enb_lckdet_power_off_f()){
		cfg = set_field(cfg, trim_sys_gpcpll_cfg_enb_lckdet_m(),
			trim_sys_gpcpll_cfg_enb_lckdet_power_on_f());
		gk20a_writel(g, trim_sys_gpcpll_cfg_r(), cfg);
		cfg = gk20a_readl(g, trim_sys_gpcpll_cfg_r());
	}

	/* wait pll lock */
	timeout = gpc_pll_params.lock_timeout + 1;
	do {
		udelay(1);
		cfg = gk20a_readl(g, trim_sys_gpcpll_cfg_r());
		if (cfg & trim_sys_gpcpll_cfg_pll_lock_true_f())
			goto pll_locked;
	} while (--timeout > 0);

	/* PLL is messed up. What can we do here? */
	dump_gpc_pll(g, gpll, cfg);
	BUG();
	return -EBUSY;

pll_locked:
	gk20a_dbg_clk("locked config_pll under bypass r=0x%x v=0x%x",
		trim_sys_gpcpll_cfg_r(), cfg);

	/* set SYNC_MODE for glitchless switch out of bypass */
	cfg = set_field(cfg, trim_sys_gpcpll_cfg_sync_mode_m(),
			trim_sys_gpcpll_cfg_sync_mode_enable_f());
	gk20a_writel(g, trim_sys_gpcpll_cfg_r(), cfg);
	gk20a_readl(g, trim_sys_gpcpll_cfg_r());

	/* put PLL back on vco */
	data = gk20a_readl(g, trim_sys_sel_vco_r());
	data = set_field(data, trim_sys_sel_vco_gpc2clk_out_m(),
		trim_sys_sel_vco_gpc2clk_out_vco_f());
	gk20a_writel(g, trim_sys_sel_vco_r(), data);

	return 0;
}

/*
 *  Change GPCPLL frequency:
 *  - in legacy (non-DVFS) mode
 *  - in DVFS mode at constant DVFS detection settings, matching current/lower
 *    voltage; the same procedure can be used in this case, since maximum DVFS
 *    detection limit makes sure that PLL output remains under F/V curve when
 *    voltage increases arbitrary.
 */
static int clk_program_gpc_pll(struct gk20a *g, struct pll *gpll_new,
			int allow_slide)
{
	u32 cfg, coeff, data;
	bool can_slide, pldiv_only;
	struct pll gpll;

	gk20a_dbg_fn("");

	if (!tegra_platform_is_silicon())
		return 0;

	/* get old coefficients */
	coeff = gk20a_readl(g, trim_sys_gpcpll_coeff_r());
	gpll.M = trim_sys_gpcpll_coeff_mdiv_v(coeff);
	gpll.N = trim_sys_gpcpll_coeff_ndiv_v(coeff);
	gpll.PL = trim_sys_gpcpll_coeff_pldiv_v(coeff);
	gpll.clk_in = gpll_new->clk_in;

	/* combine target dvfs with old coefficients */
	gpll.dvfs = gpll_new->dvfs;
	gpll.mode = gpll_new->mode;

	/* do NDIV slide if there is no change in M and PL */
	cfg = gk20a_readl(g, trim_sys_gpcpll_cfg_r());
	can_slide = allow_slide && trim_sys_gpcpll_cfg_enable_v(cfg);

	if (can_slide && (gpll_new->M == gpll.M) && (gpll_new->PL == gpll.PL))
		return clk_slide_gpc_pll(g, gpll_new);

	/* slide down to NDIV_LO */
	if (can_slide) {
		int ret;
		gpll.N = DIV_ROUND_UP(gpll.M * gpc_pll_params.min_vco,
				      gpll.clk_in);
		if (gpll.mode == GPC_PLL_MODE_DVFS)
			clk_config_dvfs_ndiv(gpll.dvfs.mv, gpll.N, &gpll.dvfs);
		ret = clk_slide_gpc_pll(g, &gpll);
		if (ret)
			return ret;
	}
	pldiv_only = can_slide && (gpll_new->M == gpll.M);

	/*
	 *  Split FO-to-bypass jump in halfs by setting out divider 1:2.
	 *  (needed even if PLDIV_GLITCHLESS is set, since 1:1 <=> 1:2 direct
	 *  transition is not really glitch-less - see get_interim_pldiv
	 *  function header).
	 */
	if ((gpll_new->PL < 2) || (gpll.PL < 2)) {
		data = gk20a_readl(g, trim_sys_gpc2clk_out_r());
		data = set_field(data, trim_sys_gpc2clk_out_vcodiv_m(),
			trim_sys_gpc2clk_out_vcodiv_f(2));
		gk20a_writel(g, trim_sys_gpc2clk_out_r(), data);
		/* Intentional 2nd write to assure linear divider operation */
		gk20a_writel(g, trim_sys_gpc2clk_out_r(), data);
		gk20a_readl(g, trim_sys_gpc2clk_out_r());
		udelay(2);
	}

#if PLDIV_GLITCHLESS
	coeff = gk20a_readl(g, trim_sys_gpcpll_coeff_r());
	if (pldiv_only) {
		/* Insert interim PLDIV state if necessary */
		u32 interim_pl = get_interim_pldiv(gpll_new->PL, gpll.PL);
		if (interim_pl) {
			coeff = set_field(coeff,
				trim_sys_gpcpll_coeff_pldiv_m(),
				trim_sys_gpcpll_coeff_pldiv_f(interim_pl));
			gk20a_writel(g, trim_sys_gpcpll_coeff_r(), coeff);
			coeff = gk20a_readl(g, trim_sys_gpcpll_coeff_r());
		}
		goto set_pldiv;	/* path A: no need to bypass */
	}

	/* path B: bypass if either M changes or PLL is disabled */
#endif
	/*
	 * Program and lock pll under bypass. On exit PLL is out of bypass,
	 * enabled, and locked. VCO is at vco_min if sliding is allowed.
	 * Otherwise it is at VCO target (and therefore last slide call below
	 * is effectively NOP). PL is set to target. Output divider is engaged
	 * at 1:2 if either entry, or exit PL setting is 1:1.
	 */
	gpll = *gpll_new;
	if (allow_slide) {
		gpll.N = DIV_ROUND_UP(gpll_new->M * gpc_pll_params.min_vco,
				      gpll_new->clk_in);
		if (gpll.mode == GPC_PLL_MODE_DVFS)
			clk_config_dvfs_ndiv(gpll.dvfs.mv, gpll.N, &gpll.dvfs);
	}
	if (pldiv_only)
		clk_change_pldiv_under_bypass(g, &gpll);
	else
		clk_lock_gpc_pll_under_bypass(g, &gpll);

#if PLDIV_GLITCHLESS
	coeff = gk20a_readl(g, trim_sys_gpcpll_coeff_r());

set_pldiv:
	/* coeff must be current from either path A or B */
	if (trim_sys_gpcpll_coeff_pldiv_v(coeff) != gpll_new->PL) {
		coeff = set_field(coeff, trim_sys_gpcpll_coeff_pldiv_m(),
			trim_sys_gpcpll_coeff_pldiv_f(gpll_new->PL));
		gk20a_writel(g, trim_sys_gpcpll_coeff_r(), coeff);
	}
#endif
	/* restore out divider 1:1 */
	data = gk20a_readl(g, trim_sys_gpc2clk_out_r());
	if ((data & trim_sys_gpc2clk_out_vcodiv_m()) !=
	    trim_sys_gpc2clk_out_vcodiv_by1_f()) {
		data = set_field(data, trim_sys_gpc2clk_out_vcodiv_m(),
				 trim_sys_gpc2clk_out_vcodiv_by1_f());
		udelay(2);
		gk20a_writel(g, trim_sys_gpc2clk_out_r(), data);
		/* Intentional 2nd write to assure linear divider operation */
		gk20a_writel(g, trim_sys_gpc2clk_out_r(), data);
		gk20a_readl(g, trim_sys_gpc2clk_out_r());
	}

	/* slide up to target NDIV */
	return clk_slide_gpc_pll(g, gpll_new);
}

/* Find GPCPLL config safe at DVFS coefficient = 0, matching target frequency */
static void clk_config_pll_safe_dvfs(struct gk20a *g, struct pll *gpll)
{
	u32 nsafe, nmin;

	if (gpll->freq > dvfs_safe_max_freq)
		gpll->freq = gpll->freq * (100 - DVFS_SAFE_MARGIN) / 100;

	nmin = DIV_ROUND_UP(gpll->M * gpc_pll_params.min_vco, gpll->clk_in);
	nsafe = gpll->M * gpll->freq / gpll->clk_in;

	/*
	 * If safe frequency is above VCOmin, it can be used in safe PLL config
	 * as is. Since safe frequency is below both old and new frequencies,
	 * in this case all three configurations have same post divider 1:1, and
	 * direct old=>safe=>new n-sliding will be used for transitions.
	 *
	 * Otherwise, if safe frequency is below VCO min, post-divider in safe
	 * configuration (and possibly in old and/or new configurations) is
	 * above 1:1, and each old=>safe and safe=>new transitions includes
	 * sliding to/from VCOmin, as well as divider changes. To avoid extra
	 * dynamic ramps from VCOmin during old=>safe transition and to VCOmin
	 * during safe=>new transition, select nmin as safe NDIV, and set safe
	 * post divider to assure PLL output is below safe frequency
	 */
	if (nsafe < nmin) {
		gpll->PL = DIV_ROUND_UP(nmin * gpll->clk_in,
					gpll->M * gpll->freq);
		nsafe = nmin;
	}
	gpll->N = nsafe;
	clk_config_dvfs_ndiv(gpll->dvfs.mv, gpll->N, &gpll->dvfs);

	gk20a_dbg_clk("safe freq %d kHz, M %d, N %d, PL %d(div%d), mV(cal) %d(%d), DC %d",
		gpll->freq, gpll->M, gpll->N, gpll->PL, pl_to_div(gpll->PL),
		gpll->dvfs.mv, gpll->dvfs.uv_cal / 1000, gpll->dvfs.dfs_coeff);
}

/* Change GPCPLL frequency and DVFS detection settings in DVFS mode */
static int clk_program_na_gpc_pll(struct gk20a *g, struct pll *gpll_new,
				  int allow_slide)
{
	int ret;
	struct pll gpll_safe;
	struct pll *gpll_old = &g->clk.gpc_pll_last;

	BUG_ON(gpll_new->M != 1);	/* the only MDIV in NA mode  */
	clk_config_dvfs(g, gpll_new);

	/*
	 * In cases below no intermediate steps in PLL DVFS configuration are
	 * necessary because either
	 * - PLL DVFS will be configured under bypass directly to target, or
	 * - voltage is not changing, so DVFS detection settings are the same
	 */
	if (!allow_slide || !gpll_new->enabled ||
	    (gpll_old->dvfs.mv == gpll_new->dvfs.mv))
		return clk_program_gpc_pll(g, gpll_new, allow_slide);

	/*
	 * Interim step for changing DVFS detection settings: low enough
	 * frequency to be safe at at DVFS coeff = 0.
	 *
	 * 1. If voltage is increasing:
	 * - safe frequency target matches the lowest - old - frequency
	 * - DVFS settings are still old
	 * - Voltage already increased to new level by tegra DVFS, but maximum
	 *    detection limit assures PLL output remains under F/V curve
	 *
	 * 2. If voltage is decreasing:
	 * - safe frequency target matches the lowest - new - frequency
	 * - DVFS settings are still old
	 * - Voltage is also old, it will be lowered by tegra DVFS afterwards
	 *
	 * Interim step can be skipped if old frequency is below safe minimum,
	 * i.e., it is low enough to be safe at any voltage in operating range
	 * with zero DVFS coefficient.
	 */
	if (gpll_old->freq > dvfs_safe_max_freq) {
		if (gpll_old->dvfs.mv < gpll_new->dvfs.mv) {
			gpll_safe = *gpll_old;
			gpll_safe.dvfs.mv = gpll_new->dvfs.mv;
		} else {
			gpll_safe = *gpll_new;
			gpll_safe.dvfs = gpll_old->dvfs;
		}
		clk_config_pll_safe_dvfs(g, &gpll_safe);

		ret = clk_program_gpc_pll(g, &gpll_safe, 1);
		if (ret) {
			gk20a_err(dev_from_gk20a(g), "Safe dvfs program fail\n");
			return ret;
		}
	}

	/*
	 * DVFS detection settings transition:
	 * - Set DVFS coefficient zero (safe, since already at frequency safe
	 *   at DVFS coeff = 0 for the lowest of the old/new end-points)
	 * - Set calibration level to new voltage (safe, since DVFS coeff = 0)
	 * - Set DVFS coefficient to match new voltage (safe, since already at
	 *   frequency safe at DVFS coeff = 0 for the lowest of the old/new
	 *   end-points.
	 */
	clk_set_dfs_coeff(g, 0);
	clk_set_dfs_ext_cal(g, gpll_new->dvfs.dfs_ext_cal);
	clk_set_dfs_coeff(g, gpll_new->dvfs.dfs_coeff);

	gk20a_dbg_clk("config_pll  %d kHz, M %d, N %d, PL %d(div%d), mV(cal) %d(%d), DC %d",
		gpll_new->freq, gpll_new->M, gpll_new->N, gpll_new->PL,
		pl_to_div(gpll_new->PL),
		max(gpll_new->dvfs.mv, gpll_old->dvfs.mv),
		gpll_new->dvfs.uv_cal / 1000, gpll_new->dvfs.dfs_coeff);

	/* Finally set target rate (with DVFS detection settings already new) */
	return clk_program_gpc_pll(g, gpll_new, 1);
}

static int clk_disable_gpcpll(struct gk20a *g, int allow_slide)
{
	u32 cfg, coeff;
	struct clk_gk20a *clk = &g->clk;
	struct pll gpll = clk->gpc_pll;

	/* slide to VCO min */
	cfg = gk20a_readl(g, trim_sys_gpcpll_cfg_r());
	if (allow_slide && trim_sys_gpcpll_cfg_enable_v(cfg)) {
		coeff = gk20a_readl(g, trim_sys_gpcpll_coeff_r());
		gpll.M = trim_sys_gpcpll_coeff_mdiv_v(coeff);
		gpll.N = DIV_ROUND_UP(gpll.M * gpc_pll_params.min_vco,
				      gpll.clk_in);
		if (gpll.mode == GPC_PLL_MODE_DVFS)
			clk_config_dvfs_ndiv(gpll.dvfs.mv, gpll.N, &gpll.dvfs);
		clk_slide_gpc_pll(g, &gpll);
	}

	/* put PLL in bypass before disabling it */
	cfg = gk20a_readl(g, trim_sys_sel_vco_r());
	cfg = set_field(cfg, trim_sys_sel_vco_gpc2clk_out_m(),
			trim_sys_sel_vco_gpc2clk_out_bypass_f());
	gk20a_writel(g, trim_sys_sel_vco_r(), cfg);

	/* clear SYNC_MODE before disabling PLL */
	cfg = gk20a_readl(g, trim_sys_gpcpll_cfg_r());
	cfg = set_field(cfg, trim_sys_gpcpll_cfg_sync_mode_m(),
			trim_sys_gpcpll_cfg_sync_mode_disable_f());
	gk20a_writel(g, trim_sys_gpcpll_cfg_r(), cfg);

	/* disable PLL */
	cfg = gk20a_readl(g, trim_sys_gpcpll_cfg_r());
	cfg = set_field(cfg, trim_sys_gpcpll_cfg_enable_m(),
			trim_sys_gpcpll_cfg_enable_no_f());
	gk20a_writel(g, trim_sys_gpcpll_cfg_r(), cfg);
	gk20a_readl(g, trim_sys_gpcpll_cfg_r());

	clk->gpc_pll.enabled = false;
	clk->gpc_pll_last.enabled = false;
	return 0;
}

static int gm20b_init_clk_reset_enable_hw(struct gk20a *g)
{
	gk20a_dbg_fn("");
	return 0;
}

static int gm20b_init_clk_setup_sw(struct gk20a *g)
{
	struct clk_gk20a *clk = &g->clk;
	unsigned long safe_rate;
	struct clk *ref;

	gk20a_dbg_fn("");

	if (clk->sw_ready) {
		gk20a_dbg_fn("skip init");
		return 0;
	}

	if (!gk20a_clk_get(g))
		return -EINVAL;

	/*
	 * On Tegra GPU clock exposed to frequency governor is a shared user on
	 * GPCPLL bus (gbus). The latter can be accessed as GPU clock parent.
	 * Respectively the grandparent is PLL reference clock.
	 */
	ref = clk_get_parent(clk_get_parent(clk->tegra_clk));
	if (IS_ERR(ref)) {
		gk20a_err(dev_from_gk20a(g),
			"failed to get GPCPLL reference clock");
		return -EINVAL;
	}

	clk->gpc_pll.id = GK20A_GPC_PLL;
	clk->gpc_pll.clk_in = clk_get_rate(ref) / KHZ;

	safe_rate = tegra_dvfs_get_fmax_at_vmin_safe_t(
		clk_get_parent(clk->tegra_clk));
	safe_rate = safe_rate * (100 - DVFS_SAFE_MARGIN) / 100;
	dvfs_safe_max_freq = rate_gpu_to_gpc2clk(safe_rate);
	clk->gpc_pll.PL = (dvfs_safe_max_freq == 0) ? 0 :
		DIV_ROUND_UP(gpc_pll_params.min_vco, dvfs_safe_max_freq);

	/* Initial freq: low enough to be safe at Vmin (default 1/3 VCO min) */
	clk->gpc_pll.M = 1;
	clk->gpc_pll.N = DIV_ROUND_UP(gpc_pll_params.min_vco,
				clk->gpc_pll.clk_in);
	clk->gpc_pll.PL = max(clk->gpc_pll.PL, 3U);
	clk->gpc_pll.freq = clk->gpc_pll.clk_in * clk->gpc_pll.N;
	clk->gpc_pll.freq /= pl_to_div(clk->gpc_pll.PL);

	 /*
	  * All production parts should have ADC fuses burnt. Therefore, check
	  * ADC fuses always, regardless of whether NA mode is selected; and if
	  * NA mode is indeed selected, and part can support it, switch to NA
	  * mode even when ADC calibration is not fused; less accurate s/w
	  * self-calibration will be used for those parts.
	  */
	clk_config_calibration_params(g);
#ifdef CONFIG_TEGRA_USE_NA_GPCPLL
	if (tegra_fuse_can_use_na_gpcpll()) {
		/* NA mode is supported only at max update rate 38.4 MHz */
		BUG_ON(clk->gpc_pll.clk_in != gpc_pll_params.max_u);
		clk->gpc_pll.mode = GPC_PLL_MODE_DVFS;
		gpc_pll_params.min_u = gpc_pll_params.max_u;
	}
#endif

	mutex_init(&clk->clk_mutex);

	clk->sw_ready = true;

	gk20a_dbg_fn("done");
	dev_info(dev_from_gk20a(g), "GPCPLL initial settings:%s M=%u, N=%u, P=%u",
		clk->gpc_pll.mode == GPC_PLL_MODE_DVFS ? " NA mode," : "",
		clk->gpc_pll.M, clk->gpc_pll.N, clk->gpc_pll.PL);
	return 0;
}

static int gm20b_init_clk_setup_hw(struct gk20a *g)
{
	u32 data;

	gk20a_dbg_fn("");

	/* LDIV: Div4 mode (required); both  bypass and vco ratios 1:1 */
	data = gk20a_readl(g, trim_sys_gpc2clk_out_r());
	data = set_field(data,
			trim_sys_gpc2clk_out_sdiv14_m() |
			trim_sys_gpc2clk_out_vcodiv_m() |
			trim_sys_gpc2clk_out_bypdiv_m(),
			trim_sys_gpc2clk_out_sdiv14_indiv4_mode_f() |
			trim_sys_gpc2clk_out_vcodiv_by1_f() |
			trim_sys_gpc2clk_out_bypdiv_f(0));
	gk20a_writel(g, trim_sys_gpc2clk_out_r(), data);

	/*
	 * Clear global bypass control; PLL is still under bypass, since SEL_VCO
	 * is cleared by default.
	 */
	data = gk20a_readl(g, trim_sys_bypassctrl_r());
	data = set_field(data, trim_sys_bypassctrl_gpcpll_m(),
			 trim_sys_bypassctrl_gpcpll_vco_f());
	gk20a_writel(g, trim_sys_bypassctrl_r(), data);

	/* If not fused, set RAM SVOP PDP data 0x2, and enable fuse override */
	data = gk20a_readl(g, fuse_ctrl_opt_ram_svop_pdp_r());
	if (!fuse_ctrl_opt_ram_svop_pdp_data_v(data)) {
		data = set_field(data, fuse_ctrl_opt_ram_svop_pdp_data_m(),
			 fuse_ctrl_opt_ram_svop_pdp_data_f(0x2));
		gk20a_writel(g, fuse_ctrl_opt_ram_svop_pdp_r(), data);
		data = gk20a_readl(g, fuse_ctrl_opt_ram_svop_pdp_override_r());
		data = set_field(data,
			fuse_ctrl_opt_ram_svop_pdp_override_data_m(),
			fuse_ctrl_opt_ram_svop_pdp_override_data_yes_f());
		gk20a_writel(g, fuse_ctrl_opt_ram_svop_pdp_override_r(), data);
	}

	/* Disable idle slow down */
	data = gk20a_readl(g, therm_clk_slowdown_r(0));
	data = set_field(data, therm_clk_slowdown_idle_factor_m(),
			 therm_clk_slowdown_idle_factor_disabled_f());
	gk20a_writel(g, therm_clk_slowdown_r(0), data);
	gk20a_readl(g, therm_clk_slowdown_r(0));

	if (g->clk.gpc_pll.mode == GPC_PLL_MODE_DVFS)
		return clk_enbale_pll_dvfs(g);

	return 0;
}

static int set_pll_target(struct gk20a *g, u32 freq, u32 old_freq)
{
	struct clk_gk20a *clk = &g->clk;

	if (freq > gpc_pll_params.max_freq)
		freq = gpc_pll_params.max_freq;
	else if (freq < gpc_pll_params.min_freq)
		freq = gpc_pll_params.min_freq;

	if (freq != old_freq) {
		/* gpc_pll.freq is changed to new value here */
		if (clk_config_pll(clk, &clk->gpc_pll, &gpc_pll_params,
				   &freq, true)) {
			gk20a_err(dev_from_gk20a(g),
				   "failed to set pll target for %d", freq);
			return -EINVAL;
		}
	}
	return 0;
}

static int set_pll_freq(struct gk20a *g, int allow_slide)
{
	struct clk_gk20a *clk = &g->clk;
	int err = 0;

	gk20a_dbg_fn("last freq: %dMHz, target freq %dMHz",
		     clk->gpc_pll_last.freq, clk->gpc_pll.freq);

	/* If programming with dynamic sliding failed, re-try under bypass */
	if (clk->gpc_pll.mode == GPC_PLL_MODE_DVFS) {
		err = clk_program_na_gpc_pll(g, &clk->gpc_pll, allow_slide);
		if (err && allow_slide)
			err = clk_program_na_gpc_pll(g, &clk->gpc_pll, 0);
	} else {
		err = clk_program_gpc_pll(g, &clk->gpc_pll, allow_slide);
		if (err && allow_slide)
			err = clk_program_gpc_pll(g, &clk->gpc_pll, 0);
	}

	if (!err) {
		clk->gpc_pll.enabled = true;
		clk->gpc_pll_last = clk->gpc_pll;
		return 0;
	}

	/*
	 * Just report error but not restore PLL since dvfs could already change
	 * voltage even when programming failed.
	 */
	gk20a_err(dev_from_gk20a(g), "failed to set pll to %d",
		  clk->gpc_pll.freq);
	return err;
}

static int gm20b_clk_export_set_rate(void *data, unsigned long *rate)
{
	u32 old_freq;
	int ret = -ENODATA;
	struct gk20a *g = data;
	struct clk_gk20a *clk = &g->clk;

	if (rate) {
		mutex_lock(&clk->clk_mutex);
		old_freq = clk->gpc_pll.freq;
		ret = set_pll_target(g, rate_gpu_to_gpc2clk(*rate), old_freq);
		if (!ret && clk->gpc_pll.enabled && clk->clk_hw_on)
			ret = set_pll_freq(g, 1);
		if (!ret)
			*rate = rate_gpc2clk_to_gpu(clk->gpc_pll.freq);
		mutex_unlock(&clk->clk_mutex);
	}
	return ret;
}

static int gm20b_clk_export_enable(void *data)
{
	int ret = 0;
	struct gk20a *g = data;
	struct clk_gk20a *clk = &g->clk;

	mutex_lock(&clk->clk_mutex);
	if (!clk->gpc_pll.enabled && clk->clk_hw_on)
		ret = set_pll_freq(g, 1);
	mutex_unlock(&clk->clk_mutex);
	return ret;
}

static void gm20b_clk_export_disable(void *data)
{
	struct gk20a *g = data;
	struct clk_gk20a *clk = &g->clk;

	mutex_lock(&clk->clk_mutex);
	if (clk->gpc_pll.enabled && clk->clk_hw_on)
		clk_disable_gpcpll(g, 1);
	mutex_unlock(&clk->clk_mutex);
}

static void gm20b_clk_export_init(void *data, unsigned long *rate, bool *state)
{
	struct gk20a *g = data;
	struct clk_gk20a *clk = &g->clk;

	mutex_lock(&clk->clk_mutex);
	if (state)
		*state = clk->gpc_pll.enabled;
	if (rate)
		*rate = rate_gpc2clk_to_gpu(clk->gpc_pll.freq);
	mutex_unlock(&clk->clk_mutex);
}

static struct tegra_clk_export_ops gm20b_clk_export_ops = {
	.init = gm20b_clk_export_init,
	.enable = gm20b_clk_export_enable,
	.disable = gm20b_clk_export_disable,
	.set_rate = gm20b_clk_export_set_rate,
};

static int gm20b_clk_register_export_ops(struct gk20a *g)
{
	int ret;
	struct clk *c;

	if (gm20b_clk_export_ops.data)
		return 0;

	gm20b_clk_export_ops.data = (void *)g;
	c = g->clk.tegra_clk;
	if (!c || !clk_get_parent(c))
		return -ENOSYS;

	ret = tegra_clk_register_export_ops(clk_get_parent(c),
					    &gm20b_clk_export_ops);

	return ret;
}

static int gm20b_init_clk_support(struct gk20a *g)
{
	struct clk_gk20a *clk = &g->clk;
	u32 err;

	gk20a_dbg_fn("");

	clk->g = g;

	err = gm20b_init_clk_reset_enable_hw(g);
	if (err)
		return err;

	err = gm20b_init_clk_setup_sw(g);
	if (err)
		return err;

	mutex_lock(&clk->clk_mutex);
	clk->clk_hw_on = true;

	err = gm20b_init_clk_setup_hw(g);
	mutex_unlock(&clk->clk_mutex);
	if (err)
		return err;

	err = gm20b_clk_register_export_ops(g);
	if (err)
		return err;

	/* FIXME: this effectively prevents host level clock gating */
	err = clk_enable(g->clk.tegra_clk);
	if (err)
		return err;

	/* The prev call may not enable PLL if gbus is unbalanced - force it */
	mutex_lock(&clk->clk_mutex);
	if (!clk->gpc_pll.enabled)
		err = set_pll_freq(g, 1);
	mutex_unlock(&clk->clk_mutex);
	if (err)
		return err;

#ifdef CONFIG_DEBUG_FS
	if (!clk->debugfs_set) {
		if (!clk_gm20b_debugfs_init(g))
			clk->debugfs_set = true;
	}
#endif
	return err;
}

static int gm20b_suspend_clk_support(struct gk20a *g)
{
	int ret = 0;

	clk_disable(g->clk.tegra_clk);

	/* The prev call may not disable PLL if gbus is unbalanced - force it */
	mutex_lock(&g->clk.clk_mutex);
	if (g->clk.gpc_pll.enabled)
		ret = clk_disable_gpcpll(g, 1);
	g->clk.clk_hw_on = false;
	mutex_unlock(&g->clk.clk_mutex);
	return ret;
}

void gm20b_init_clk_ops(struct gpu_ops *gops)
{
	gops->clk.init_clk_support = gm20b_init_clk_support;
	gops->clk.suspend_clk_support = gm20b_suspend_clk_support;
}

#ifdef CONFIG_DEBUG_FS

static int rate_get(void *data, u64 *val)
{
	struct gk20a *g = (struct gk20a *)data;
	*val = (u64)gk20a_clk_get_rate(g);
	return 0;
}
static int rate_set(void *data, u64 val)
{
	struct gk20a *g = (struct gk20a *)data;
	return gk20a_clk_set_rate(g, (u32)val);
}
DEFINE_SIMPLE_ATTRIBUTE(rate_fops, rate_get, rate_set, "%llu\n");

static int pll_reg_show(struct seq_file *s, void *data)
{
	struct gk20a *g = s->private;
	u32 reg, m, n, pl, f;

	mutex_lock(&g->clk.clk_mutex);
	if (!g->clk.clk_hw_on) {
		seq_printf(s, "%s powered down - no access to registers\n",
			   dev_name(dev_from_gk20a(g)));
		mutex_unlock(&g->clk.clk_mutex);
		return 0;
	}

	reg = gk20a_readl(g, trim_sys_bypassctrl_r());
	seq_printf(s, "bypassctrl = %s, ", reg ? "bypass" : "vco");
	reg = gk20a_readl(g, trim_sys_sel_vco_r());
	seq_printf(s, "sel_vco = %s, ", reg ? "vco" : "bypass");

	reg = gk20a_readl(g, trim_sys_gpcpll_cfg_r());
	seq_printf(s, "cfg  = 0x%x : %s : %s : %s\n", reg,
		trim_sys_gpcpll_cfg_enable_v(reg) ? "enabled" : "disabled",
		trim_sys_gpcpll_cfg_pll_lock_v(reg) ? "locked" : "unlocked",
		trim_sys_gpcpll_cfg_sync_mode_v(reg) ? "sync_on" : "sync_off");

	reg = gk20a_readl(g, trim_sys_gpcpll_coeff_r());
	m = trim_sys_gpcpll_coeff_mdiv_v(reg);
	n = trim_sys_gpcpll_coeff_ndiv_v(reg);
	pl = trim_sys_gpcpll_coeff_pldiv_v(reg);
	f = g->clk.gpc_pll.clk_in * n / (m * pl_to_div(pl));
	seq_printf(s, "coef = 0x%x : m = %u : n = %u : pl = %u", reg, m, n, pl);
	seq_printf(s, " : pll_f(gpu_f) = %u(%u) kHz\n", f, f/2);
	mutex_unlock(&g->clk.clk_mutex);
	return 0;
}

static int pll_reg_open(struct inode *inode, struct file *file)
{
	return single_open(file, pll_reg_show, inode->i_private);
}

static const struct file_operations pll_reg_fops = {
	.open		= pll_reg_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int pll_reg_raw_show(struct seq_file *s, void *data)
{
	struct gk20a *g = s->private;
	u32 reg;

	mutex_lock(&g->clk.clk_mutex);
	if (!g->clk.clk_hw_on) {
		seq_printf(s, "%s powered down - no access to registers\n",
			   dev_name(dev_from_gk20a(g)));
		mutex_unlock(&g->clk.clk_mutex);
		return 0;
	}

	seq_puts(s, "GPCPLL REGISTERS:\n");
	for (reg = trim_sys_gpcpll_cfg_r(); reg <= trim_sys_gpcpll_dvfs2_r();
	      reg += sizeof(u32))
		seq_printf(s, "[0x%02x] = 0x%08x\n", reg, gk20a_readl(g, reg));

	seq_puts(s, "\nGPC CLK OUT REGISTERS:\n");

	reg = trim_sys_sel_vco_r();
	seq_printf(s, "[0x%02x] = 0x%08x\n", reg, gk20a_readl(g, reg));
	reg = trim_sys_gpc2clk_out_r();
	seq_printf(s, "[0x%02x] = 0x%08x\n", reg, gk20a_readl(g, reg));
	reg = trim_sys_bypassctrl_r();
	seq_printf(s, "[0x%02x] = 0x%08x\n", reg, gk20a_readl(g, reg));

	mutex_unlock(&g->clk.clk_mutex);
	return 0;
}

static int pll_reg_raw_open(struct inode *inode, struct file *file)
{
	return single_open(file, pll_reg_raw_show, inode->i_private);
}

static ssize_t pll_reg_raw_write(struct file *file,
	const char __user *userbuf, size_t count, loff_t *ppos)
{
	struct gk20a *g = file->f_path.dentry->d_inode->i_private;
	char buf[80];
	u32 reg, val;

	if (sizeof(buf) <= count)
		return -EINVAL;

	if (copy_from_user(buf, userbuf, count))
		return -EFAULT;

	/* terminate buffer and trim - white spaces may be appended
	 *  at the end when invoked from shell command line */
	buf[count] = '\0';
	strim(buf);

	if (sscanf(buf, "[0x%x] = 0x%x", &reg, &val) != 2)
		return -EINVAL;

	if (((reg < trim_sys_gpcpll_cfg_r()) ||
	    (reg > trim_sys_gpcpll_dvfs2_r())) &&
	    (reg != trim_sys_sel_vco_r()) &&
	    (reg != trim_sys_gpc2clk_out_r()) &&
	    (reg != trim_sys_bypassctrl_r()))
		return -EPERM;

	mutex_lock(&g->clk.clk_mutex);
	if (!g->clk.clk_hw_on) {
		mutex_unlock(&g->clk.clk_mutex);
		return -EBUSY;
	}
	gk20a_writel(g, reg, val);
	mutex_unlock(&g->clk.clk_mutex);
	return count;
}

static const struct file_operations pll_reg_raw_fops = {
	.open		= pll_reg_raw_open,
	.read		= seq_read,
	.write		= pll_reg_raw_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int monitor_get(void *data, u64 *val)
{
	struct gk20a *g = (struct gk20a *)data;
	struct clk_gk20a *clk = &g->clk;
	u32 clk_slowdown, clk_slowdown_save;
	int err;

	u32 ncycle = 800; /* count GPCCLK for ncycle of clkin */
	u64 freq = clk->gpc_pll.clk_in;
	u32 count1, count2;

	err = gk20a_busy(g->dev);
	if (err)
		return err;

	mutex_lock(&g->clk.clk_mutex);

	/* Disable clock slowdown during measurements */
	clk_slowdown_save = gk20a_readl(g, therm_clk_slowdown_r(0));
	clk_slowdown = set_field(clk_slowdown_save,
				 therm_clk_slowdown_idle_factor_m(),
				 therm_clk_slowdown_idle_factor_disabled_f());
	gk20a_writel(g, therm_clk_slowdown_r(0), clk_slowdown);
	gk20a_readl(g, therm_clk_slowdown_r(0));

	gk20a_writel(g, trim_gpc_clk_cntr_ncgpcclk_cfg_r(0),
		     trim_gpc_clk_cntr_ncgpcclk_cfg_reset_asserted_f());
	gk20a_writel(g, trim_gpc_clk_cntr_ncgpcclk_cfg_r(0),
		     trim_gpc_clk_cntr_ncgpcclk_cfg_enable_asserted_f() |
		     trim_gpc_clk_cntr_ncgpcclk_cfg_write_en_asserted_f() |
		     trim_gpc_clk_cntr_ncgpcclk_cfg_noofipclks_f(ncycle));
	/* start */

	/* It should take less than 25us to finish 800 cycle of 38.4MHz.
	   But longer than 100us delay is required here. */
	gk20a_readl(g, trim_gpc_clk_cntr_ncgpcclk_cfg_r(0));
	udelay(200);

	count1 = gk20a_readl(g, trim_gpc_clk_cntr_ncgpcclk_cnt_r(0));
	udelay(100);
	count2 = gk20a_readl(g, trim_gpc_clk_cntr_ncgpcclk_cnt_r(0));
	freq *= trim_gpc_clk_cntr_ncgpcclk_cnt_value_v(count2);
	do_div(freq, ncycle);
	*val = freq;

	/* Restore clock slowdown */
	gk20a_writel(g, therm_clk_slowdown_r(0), clk_slowdown_save);
	mutex_unlock(&g->clk.clk_mutex);

	gk20a_idle(g->dev);

	if (count1 != count2)
		return -EBUSY;
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(monitor_fops, monitor_get, NULL, "%llu\n");

static int voltage_get(void *data, u64 *val)
{
	struct gk20a *g = (struct gk20a *)data;
	struct clk_gk20a *clk = &g->clk;
	u32 det_out;
	int err;

	if (clk->gpc_pll.mode != GPC_PLL_MODE_DVFS)
		return -ENOSYS;

	err = gk20a_busy(g->dev);
	if (err)
		return err;

	mutex_lock(&g->clk.clk_mutex);

	det_out = gk20a_readl(g, trim_sys_gpcpll_cfg3_r());
	det_out = trim_sys_gpcpll_cfg3_dfs_testout_v(det_out);
	*val = (det_out * gpc_pll_params.uvdet_slope +
		gpc_pll_params.uvdet_offs) / 1000;

	mutex_unlock(&g->clk.clk_mutex);

	gk20a_idle(g->dev);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(voltage_fops, voltage_get, NULL, "%llu\n");

static int pll_param_show(struct seq_file *s, void *data)
{
	seq_printf(s, "ADC offs = %d uV, ADC slope = %d uV, VCO ctrl = 0x%x\n",
		   gpc_pll_params.uvdet_offs, gpc_pll_params.uvdet_slope,
		   gpc_pll_params.vco_ctrl);
	return 0;
}

static int pll_param_open(struct inode *inode, struct file *file)
{
	return single_open(file, pll_param_show, inode->i_private);
}

static const struct file_operations pll_param_fops = {
	.open		= pll_param_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int clk_gm20b_debugfs_init(struct gk20a *g)
{
	struct dentry *d;
	struct gk20a_platform *platform = dev_get_drvdata(g->dev);

	d = debugfs_create_file(
		"rate", S_IRUGO|S_IWUSR, platform->debugfs, g, &rate_fops);
	if (!d)
		goto err_out;

	d = debugfs_create_file(
		"pll_reg", S_IRUGO, platform->debugfs, g, &pll_reg_fops);
	if (!d)
		goto err_out;

	d = debugfs_create_file("pll_reg_raw",
		S_IRUGO, platform->debugfs, g, &pll_reg_raw_fops);
	if (!d)
		goto err_out;

	d = debugfs_create_file(
		"monitor", S_IRUGO, platform->debugfs, g, &monitor_fops);
	if (!d)
		goto err_out;

	d = debugfs_create_file(
		"voltage", S_IRUGO, platform->debugfs, g, &voltage_fops);
	if (!d)
		goto err_out;

	d = debugfs_create_file(
		"pll_param", S_IRUGO, platform->debugfs, g, &pll_param_fops);
	if (!d)
		goto err_out;

	d = debugfs_create_u32("pll_na_mode", S_IRUGO, platform->debugfs,
			       (u32 *)&g->clk.gpc_pll.mode);
	if (!d)
		goto err_out;

	d = debugfs_create_u32("fmax2x_at_vmin_safe_t", S_IRUGO,
			       platform->debugfs, (u32 *)&dvfs_safe_max_freq);
	if (!d)
		goto err_out;

	return 0;

err_out:
	pr_err("%s: Failed to make debugfs node\n", __func__);
	debugfs_remove_recursive(platform->debugfs);
	return -ENOMEM;
}

#endif /* CONFIG_DEBUG_FS */
