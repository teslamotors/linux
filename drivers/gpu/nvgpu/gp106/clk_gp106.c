/*
 * GP106 Clocks
 *
 * Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
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
#include "hw_trim_gp106.h"
#include "clk_gp106.h"
#include "clk/clk_arb.h"

#define gk20a_dbg_clk(fmt, arg...) \
	gk20a_dbg(gpu_dbg_clk, fmt, ##arg)

#ifdef CONFIG_DEBUG_FS
static int clk_gp106_debugfs_init(struct gk20a *g);
#endif

#define NUM_NAMEMAPS	4
#define XTAL4X_KHZ 108000


static u32 gp106_get_rate_cntr(struct gk20a *g, struct namemap_cfg *);
static u16 gp106_clk_get_rate(struct gk20a *g, u32 api_domain);
static u32 gp106_crystal_clk_hz(struct gk20a *g)
{
	return (XTAL4X_KHZ * 1000);
}

static u16 gp106_clk_get_rate(struct gk20a *g, u32 api_domain)
{
	struct clk_gk20a *clk = &g->clk;
	u32 freq_khz;
	u32 i;
	struct namemap_cfg *c = NULL;

	for (i = 0; i < clk->namemap_num; i++) {
		if (api_domain == clk->namemap_xlat_table[i]) {
			c = &clk->clk_namemap[i];
			break;
		}
	}

	if (!c)
		return 0;

	freq_khz = c->is_counter ? c->scale * gp106_get_rate_cntr(g, c) :
		0; /* TODO: PLL read */

	/* Convert to MHZ */
	return (u16) (freq_khz/1000);
}

static int gp106_init_clk_support(struct gk20a *g) {
	struct clk_gk20a *clk = &g->clk;
	u32 err = 0;

	gk20a_dbg_fn("");

	mutex_init(&clk->clk_mutex);

	clk->clk_namemap = (struct namemap_cfg *)
		kzalloc(sizeof(struct namemap_cfg) * NUM_NAMEMAPS, GFP_KERNEL);

	if (!clk->clk_namemap)
		return -ENOMEM;

	clk->namemap_xlat_table = kcalloc(NUM_NAMEMAPS, sizeof(u32),
		GFP_KERNEL);

	if (!clk->namemap_xlat_table) {
		kfree(clk->clk_namemap);
		return -ENOMEM;
	}

	clk->clk_namemap[0] = (struct namemap_cfg) {
		.namemap = CLK_NAMEMAP_INDEX_GPC2CLK,
		.is_enable = 1,
		.is_counter = 1,
		.g = g,
		.cntr.reg_ctrl_addr = trim_gpc_bcast_clk_cntr_ncgpcclk_cfg_r(),
		.cntr.reg_ctrl_idx  =
			trim_gpc_bcast_clk_cntr_ncgpcclk_cfg_source_gpc2clk_f(),
		.cntr.reg_cntr_addr = trim_gpc_bcast_clk_cntr_ncgpcclk_cnt_r(),
		.name = "gpc2clk",
		.scale = 1
	};
	clk->namemap_xlat_table[0] = CTRL_CLK_DOMAIN_GPC2CLK;
	clk->clk_namemap[1] = (struct namemap_cfg) {
		.namemap = CLK_NAMEMAP_INDEX_SYS2CLK,
		.is_enable = 1,
		.is_counter = 1,
		.g = g,
		.cntr.reg_ctrl_addr = trim_sys_clk_cntr_ncsyspll_cfg_r(),
		.cntr.reg_ctrl_idx  = trim_sys_clk_cntr_ncsyspll_cfg_source_sys2clk_f(),
		.cntr.reg_cntr_addr = trim_sys_clk_cntr_ncsyspll_cnt_r(),
		.name = "sys2clk",
		.scale = 1
	};
	clk->namemap_xlat_table[1] = CTRL_CLK_DOMAIN_SYS2CLK;
	clk->clk_namemap[2] = (struct namemap_cfg) {
		.namemap = CLK_NAMEMAP_INDEX_XBAR2CLK,
		.is_enable = 1,
		.is_counter = 1,
		.g = g,
		.cntr.reg_ctrl_addr = trim_sys_clk_cntr_ncltcpll_cfg_r(),
		.cntr.reg_ctrl_idx  = trim_sys_clk_cntr_ncltcpll_cfg_source_xbar2clk_f(),
		.cntr.reg_cntr_addr = trim_sys_clk_cntr_ncltcpll_cnt_r(),
		.name = "xbar2clk",
		.scale = 1
	};
	clk->namemap_xlat_table[2] = CTRL_CLK_DOMAIN_XBAR2CLK;
	clk->clk_namemap[3] = (struct namemap_cfg) {
		.namemap = CLK_NAMEMAP_INDEX_DRAMCLK,
		.is_enable = 1,
		.is_counter = 1,
		.g = g,
		.cntr.reg_ctrl_addr = trim_fbpa_bcast_clk_cntr_ncltcclk_cfg_r(),
		.cntr.reg_ctrl_idx  =
			trim_fbpa_bcast_clk_cntr_ncltcclk_cfg_source_dramdiv4_rec_clk1_f(),
		.cntr.reg_cntr_addr = trim_fbpa_bcast_clk_cntr_ncltcclk_cnt_r(),
		.name = "dramdiv2_rec_clk1",
		.scale = 2
	};
	clk->namemap_xlat_table[3] = CTRL_CLK_DOMAIN_MCLK;

	clk->namemap_num = NUM_NAMEMAPS;

	clk->g = g;

#ifdef CONFIG_DEBUG_FS
	if (!clk->debugfs_set) {
		if (!clk_gp106_debugfs_init(g))
			clk->debugfs_set = true;
	}
#endif
	return err;
}

static u32 gp106_get_rate_cntr(struct gk20a *g, struct namemap_cfg *c) {
	u32 save_reg;
	u32 retries;
	u32 cntr = 0;

	struct clk_gk20a *clk = &g->clk;

	if (!c || !c->cntr.reg_ctrl_addr || !c->cntr.reg_cntr_addr)
		return 0;

	mutex_lock(&clk->clk_mutex);

	/* Save the register */
	save_reg = gk20a_readl(g, c->cntr.reg_ctrl_addr);

	/* Disable and reset the current clock */
	gk20a_writel(g, c->cntr.reg_ctrl_addr,
				 trim_gpc_bcast_clk_cntr_ncgpcclk_cfg_reset_asserted_f() |
				 trim_gpc_bcast_clk_cntr_ncgpcclk_cfg_enable_deasserted_f());

	/* Force wb() */
	gk20a_readl(g, c->cntr.reg_ctrl_addr);

	/* Wait for reset to happen */
	retries = CLK_DEFAULT_CNTRL_SETTLE_RETRIES;
	do {
		udelay(CLK_DEFAULT_CNTRL_SETTLE_USECS);
	} while ((--retries) && (cntr = gk20a_readl(g, c->cntr.reg_cntr_addr)));

	if (!retries) {
		gk20a_err(dev_from_gk20a(g),
             "unable to settle counter reset, bailing");
		goto read_err;
	}
	/* Program counter */
	gk20a_writel(g, c->cntr.reg_ctrl_addr,
					trim_gpc_bcast_clk_cntr_ncgpcclk_cfg_reset_deasserted_f()          |
				 	trim_gpc_bcast_clk_cntr_ncgpcclk_cfg_enable_asserted_f()           |
				 	trim_gpc_bcast_clk_cntr_ncgpcclk_cfg_write_en_asserted_f()         |
				 	trim_gpc_bcast_clk_cntr_ncgpcclk_cfg_write_en_asserted_f()         |
				 	trim_gpc_bcast_clk_cntr_ncgpcclk_cfg_write_en_asserted_f()         |
				 	trim_gpc_bcast_clk_cntr_ncgpcclk_cfg_noofipclks_f(XTAL_CNTR_CLKS)  |
					c->cntr.reg_ctrl_idx);
	gk20a_readl(g, c->cntr.reg_ctrl_addr);

	udelay(XTAL_CNTR_DELAY);

	cntr = XTAL_SCALE_TO_KHZ * gk20a_readl(g, c->cntr.reg_cntr_addr);

read_err:
	/* reset and restore control register */
	gk20a_writel(g, c->cntr.reg_ctrl_addr,
				 trim_gpc_bcast_clk_cntr_ncgpcclk_cfg_reset_asserted_f() |
				 trim_gpc_bcast_clk_cntr_ncgpcclk_cfg_enable_deasserted_f());
	gk20a_readl(g, c->cntr.reg_ctrl_addr);
	gk20a_writel(g, c->cntr.reg_ctrl_addr, save_reg);
	gk20a_readl(g, c->cntr.reg_ctrl_addr);
	mutex_unlock(&clk->clk_mutex);

	return cntr;

}

#ifdef CONFIG_DEBUG_FS
static int gp106_get_rate_show(void *data , u64 *val) {
	struct namemap_cfg *c = (struct namemap_cfg *) data;
	struct gk20a *g = c->g;

	*val = c->is_counter ? gp106_get_rate_cntr(g, c) : 0 /* TODO PLL read */;
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(get_rate_fops, gp106_get_rate_show, NULL, "%llu\n");


static int clk_gp106_debugfs_init(struct gk20a *g) {
	struct gk20a_platform *platform = dev_get_drvdata(g->dev);

	struct dentry *gpu_root = platform->debugfs;
	struct dentry *clocks_root;
	struct dentry *d;
	int i;

	if (NULL == (clocks_root = debugfs_create_dir("clocks", gpu_root)))
		return -ENOMEM;

	gk20a_dbg(gpu_dbg_info, "g=%p", g);

	for (i = 0; i < g->clk.namemap_num; i++) {
		if (g->clk.clk_namemap[i].is_enable) {
			d = debugfs_create_file(
				g->clk.clk_namemap[i].name,
				S_IRUGO,
				clocks_root,
				&g->clk.clk_namemap[i],
				&get_rate_fops);
			if (!d)
				goto err_out;
		}
	}
	return 0;

err_out:
	pr_err("%s: Failed to make debugfs node\n", __func__);
	debugfs_remove_recursive(clocks_root);
	return -ENOMEM;
}
#endif /* CONFIG_DEBUG_FS */

void gp106_init_clk_ops(struct gpu_ops *gops) {
	gops->clk.init_clk_support = gp106_init_clk_support;
	gops->clk.get_crystal_clk_hz = gp106_crystal_clk_hz;
	gops->clk.get_rate = gp106_clk_get_rate;
}

