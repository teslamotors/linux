/*
 * Copyright (c) 2011 - 2016, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef CLK_GK20A_H
#define CLK_GK20A_H

#include <linux/mutex.h>

#define GPUFREQ_TABLE_END     ~(u32)1
enum {
	/* only one PLL for gk20a */
	GK20A_GPC_PLL = 0,
};

enum gpc_pll_mode {
	GPC_PLL_MODE_F = 0,	/* fixed frequency mode a.k.a legacy mode */
	GPC_PLL_MODE_DVFS,	/* DVFS mode a.k.a NA mode */
};

struct na_dvfs {
	u32 n_int;
	u32 sdm_din;
	int dfs_coeff;
	int dfs_det_max;
	int dfs_ext_cal;
	int uv_cal;
	int mv;
};

struct pll {
	u32 id;
	u32 clk_in;	/* KHz */
	u32 M;
	u32 N;
	u32 PL;
	u32 freq;	/* KHz */
	bool enabled;
	enum gpc_pll_mode mode;
	struct na_dvfs dvfs;
};

struct pll_parms {
	u32 min_freq, max_freq;	/* KHz */
	u32 min_vco, max_vco;	/* KHz */
	u32 min_u,   max_u;	/* KHz */
	u32 min_M,   max_M;
	u32 min_N,   max_N;
	u32 min_PL,  max_PL;
	/* NA mode parameters*/
	int coeff_slope, coeff_offs; /* coeff = slope * V + offs */
	int uvdet_slope, uvdet_offs; /* uV = slope * det + offs */
	u32 vco_ctrl;
	/*
	 * Timing parameters in us. Lock timeout is applied to locking in fixed
	 * frequency mode and to dynamic ramp in any mode; does not affect lock
	 * latency, since lock/ramp done status bit is polled. NA mode lock and
	 * and IDDQ exit delays set the time of the respective opertaions with
	 * no status polling.
	 */
	u32 lock_timeout;
	u32 na_lock_delay;
	u32 iddq_exit_delay;
};

struct namemap_cfg;

struct clk_gk20a {
	struct gk20a *g;
	struct clk *tegra_clk;
	struct pll gpc_pll;
	struct pll gpc_pll_last;
	struct mutex clk_mutex;
	struct namemap_cfg *clk_namemap;
	u32 namemap_num;
	u32 *namemap_xlat_table;
	bool sw_ready;
	bool clk_hw_on;
	bool debugfs_set;
};

struct gpu_ops;
#ifdef CONFIG_TEGRA_CLK_FRAMEWORK
void gk20a_init_clk_ops(struct gpu_ops *gops);
#else
static inline void gk20a_init_clk_ops(struct gpu_ops *gops) {}
#endif

/* APIs used for both GK20A and GM20B */
unsigned long gk20a_clk_get_rate(struct gk20a *g);
int gk20a_clk_set_rate(struct gk20a *g, unsigned long rate);
long gk20a_clk_round_rate(struct gk20a *g, unsigned long rate);
struct clk *gk20a_clk_get(struct gk20a *g);

#define KHZ 1000
#define MHZ 1000000

static inline unsigned long rate_gpc2clk_to_gpu(unsigned long rate)
{
	/* convert the kHz gpc2clk frequency to Hz gpcpll frequency */
	return (rate * KHZ) / 2;
}
static inline unsigned long rate_gpu_to_gpc2clk(unsigned long rate)
{
	/* convert the Hz gpcpll frequency to kHz gpc2clk frequency */
	return (rate * 2) / KHZ;
}

#endif /* CLK_GK20A_H */
