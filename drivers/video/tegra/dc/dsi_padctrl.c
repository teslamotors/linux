/*
 * drivers/video/tegra/dc/dsi_padctrl.c
 *
 * Copyright (c) 2015-2016, NVIDIA CORPORATION, All rights reserved.
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
#include <linux/io.h>
#include <linux/of_address.h>
#include <linux/reset.h>
#include <mach/dc.h>

#include "dsi_padctrl_regs.h"
#include "dc_priv_defs.h"
#include "dc_priv.h"
#include "dsi.h"

#include <linux/tegra_prod.h>

#define DSI_PADCTRL_INSTANCE	4
#define DSI_MAX_INSTANCES	4

static int dsi_padctrl_pwr_down_regs[DSI_MAX_INSTANCES] = {
	DSI_PADCTRL_A_LANES_PWR_DOWN,
	DSI_PADCTRL_B_LANES_PWR_DOWN,
	DSI_PADCTRL_C_LANES_PWR_DOWN,
	DSI_PADCTRL_D_LANES_PWR_DOWN,
};

static int dsi_padctrl_pull_down_regs[DSI_MAX_INSTANCES] = {
	DSI_PADCTRL_A_PULL_DOWN,
	DSI_PADCTRL_B_PULL_DOWN,
	DSI_PADCTRL_C_PULL_DOWN,
	DSI_PADCTRL_D_PULL_DOWN,
};

#define DSI_A_IO_LANES_ACTIVE	0x03
#define DSI_B_IO_LANES_ACTIVE	0x0C
#define DSI_C_IO_LANES_ACTIVE	0x30
#define DSI_D_IO_LANES_ACTIVE	0xC0
#define DSI_ALL_LANES_ACTIVE	0xFF

#define DSI_A_CLK_ACTIVE	0x1
#define DSI_B_CLK_ACTIVE	0x2
#define DSI_C_CLK_ACTIVE	0x4
#define DSI_D_CLK_ACTIVE	0x8
#define DSI_ALL_CLKS_ACTIVE	0xF

static inline void tegra_dsi_padctrl_write(struct tegra_dsi_padctrl *dsi_padctrl,
	int val, int reg)
{
	writel(val, dsi_padctrl->base_addr + (reg * 4));
}

static inline unsigned long tegra_dsi_padctrl_read(struct tegra_dsi_padctrl *dsi_padctrl,
	int reg)
{
	return readl(dsi_padctrl->base_addr + (reg * 4));
}

static void tegra_dsi_padctrl_reset(struct tegra_dsi_padctrl *dsi_padctrl)
{
	if (!dsi_padctrl || !dsi_padctrl->reset) {
		pr_err("dsi padctl: Invalid dsi padctrl reset handle\n");
		return;
	}

	reset_control_reset(dsi_padctrl->reset);
}

void tegra_dsi_padctrl_disable(struct tegra_dsi_padctrl *dsi_padctrl)
{
	int val, i;

	if (!dsi_padctrl->dsi_pads_enabled)
		return;

	/* Enable all pwr downs for all controllers */
	val = 0;
	for (i = 0; i < ARRAY_SIZE(dsi_padctrl_pwr_down_regs); i++) {
		val = tegra_dsi_padctrl_read(dsi_padctrl,
			dsi_padctrl_pwr_down_regs[i]);
		val |= (DSI_PADCTRL_PWR_DOWN_PD_CLK_EN |
			DSI_PADCTRL_PWR_DOWN_PD_IO_0_EN |
			DSI_PADCTRL_PWR_DOWN_PD_IO_1_EN);
		tegra_dsi_padctrl_write(dsi_padctrl, val,
			dsi_padctrl_pwr_down_regs[i]);
	}

	/* Enable all pull downs for all controllers */
	val = 0;
	for (i = 0; i < ARRAY_SIZE(dsi_padctrl_pull_down_regs); i++) {
		val = tegra_dsi_padctrl_read(dsi_padctrl,
			dsi_padctrl_pull_down_regs[i]);
		val |= (DSI_PADCTRL_E_PULL_DWN_PD_CLK_EN |
			DSI_PADCTRL_E_PULL_DWN_PD_IO_0_EN |
			DSI_PADCTRL_E_PULL_DWN_PD_IO_1_EN);
		tegra_dsi_padctrl_write(dsi_padctrl, val,
			dsi_padctrl_pull_down_regs[i]);
	}

	/* Enable PDVCLAMP in global pad controls */
	val = tegra_dsi_padctrl_read(dsi_padctrl, DSI_PADCTRL_GLOBAL_CNTRLS);
	val |=  DSI_PADCTRL_PDVCLAMP_AB | DSI_PADCTRL_PDVCLAMP_CD;
	tegra_dsi_padctrl_write(dsi_padctrl, val, DSI_PADCTRL_GLOBAL_CNTRLS);

	dsi_padctrl->dsi_pads_enabled = false;
}

void tegra_dsi_padctrl_enable(struct tegra_dsi_padctrl *dsi_padctrl)
{
	int val, err;
	u8 i;

	if (dsi_padctrl->dsi_pads_enabled)
		return;

	/* Disable PDVCLAMP in global pad controls */
	val = tegra_dsi_padctrl_read(dsi_padctrl, DSI_PADCTRL_GLOBAL_CNTRLS);
	val &=  ~(DSI_PADCTRL_PDVCLAMP_AB | DSI_PADCTRL_PDVCLAMP_CD);
	tegra_dsi_padctrl_write(dsi_padctrl, val, DSI_PADCTRL_GLOBAL_CNTRLS);

	if (!dsi_padctrl->prod_settings_updated && dsi_padctrl->prod_list) {
		err = tegra_prod_set_by_name(&dsi_padctrl->base_addr,
			"dsi-padctrl-prod", dsi_padctrl->prod_list);
		if (err)
			pr_err("dsi padctl:prod settings failed%d\n", err);
		else
			dsi_padctrl->prod_settings_updated = true;
	}

	/* Clear pwr and pull downs for required data and clock lanes */
	for (i = 0; i < ARRAY_SIZE(dsi_padctrl_pwr_down_regs); i++) {
		val = tegra_dsi_padctrl_read(dsi_padctrl,
			dsi_padctrl_pwr_down_regs[i]);
		val &= ~dsi_padctrl->pwr_dwn_mask[i];
		tegra_dsi_padctrl_write(dsi_padctrl, val,
			dsi_padctrl_pwr_down_regs[i]);

		val = tegra_dsi_padctrl_read(dsi_padctrl,
			dsi_padctrl_pull_down_regs[i]);
		val &= ~(DSI_PADCTRL_E_PULL_DWN_PD_CLK_EN |
				DSI_PADCTRL_E_PULL_DWN_PD_IO_0_EN |
				DSI_PADCTRL_E_PULL_DWN_PD_IO_1_EN);
		tegra_dsi_padctrl_write(dsi_padctrl, val,
			dsi_padctrl_pull_down_regs[i]);
	}

	dsi_padctrl->dsi_pads_enabled = true;
}

/*
 * Enable dsi pads based on the number of lanes to be used and the DSI
 * controller instance. Power down unused clock lanes when using DSI
 * ganged mode.
 */
static void tegra_dsi_padctrl_setup_pwr_down_mask(struct tegra_dc_dsi_data *dsi,
	struct tegra_dsi_padctrl *dsi_padctrl)
{
	u8 i;
	u8 dsi_act_data_lane_mask = DSI_ALL_LANES_ACTIVE;
	u8 dsi_act_clk_lane_mask = DSI_ALL_CLKS_ACTIVE;

	/* Check for the num of lanes to be enabled */
	if (dsi->info.n_data_lanes == 8) {
		/*
		 * When all 8 lanes are used, DSI A,B,C,D IO pad power downs
		 * are cleared. If split link feature is enabled with all 4
		 * DSI controllers, all clock lane power downs are cleared.
		 * For ganged mode, only DSI A,C clock lane power downs are
		 * cleared.
		 */
		dsi_act_data_lane_mask &= DSI_ALL_LANES_ACTIVE;
		if (dsi->info.ganged_type)
			dsi_act_clk_lane_mask &= (DSI_A_CLK_ACTIVE |
				DSI_C_CLK_ACTIVE);
		else if (dsi->info.split_link_type ==
				TEGRA_DSI_SPLIT_LINK_A_B_C_D)
			dsi_act_clk_lane_mask &= DSI_ALL_CLKS_ACTIVE;
	} else if (dsi->info.n_data_lanes == 4) {
		/*
		 * When 4 data lanes are used, clear power downs for DSI A,B or
		 * DSI C,D pads based on the dsi instance or Split link
		 * configuration. Power downs need to be cleared for
		 * corresponding clock lanes.
		 */
		if ((dsi->info.split_link_type == TEGRA_DSI_SPLIT_LINK_A_B) ||
				!dsi->info.dsi_instance) {
			dsi_act_data_lane_mask &= (DSI_A_IO_LANES_ACTIVE |
				DSI_B_IO_LANES_ACTIVE);
			dsi_act_clk_lane_mask &= DSI_A_CLK_ACTIVE;
		} else {
			dsi_act_data_lane_mask &= (DSI_C_IO_LANES_ACTIVE |
				DSI_D_IO_LANES_ACTIVE);
			dsi_act_clk_lane_mask &= DSI_C_CLK_ACTIVE;
		}
	}

	/*
	 * Fill up active data and clock lanes. pwr_dwn_mask consists of 3 bits
	 * BIT(0) indicates whether clock lane is active or not.
	 * BIT(1) and BIT(2) indicate whether IO LANE0 and LANE1 are active.
	 */
	for (i = 0; i < DSI_MAX_INSTANCES; i++)
		dsi_padctrl->pwr_dwn_mask[i] =
			(((dsi_act_data_lane_mask >> (2 * i)) & 0x3) << 1) |
			((dsi_act_clk_lane_mask >> i) & 0x1);
}

struct tegra_dsi_padctrl *tegra_dsi_padctrl_init(struct tegra_dc *dc)
{
	struct tegra_dc_dsi_data *dsi;
	struct tegra_dsi_padctrl *dsi_padctrl;
	struct resource *res;
	struct resource padctrl_res;
	struct device_node *np_dsi = of_find_node_by_path(DSI_NODE);
	int err;

	/* Padctrl module doesn't exist on fpga */
	if (tegra_platform_is_linsim() || tegra_platform_is_fpga()) {
		return NULL;
	}

	dsi = tegra_dc_get_outdata(dc);
	dsi_padctrl = devm_kzalloc(&dc->ndev->dev, sizeof(struct tegra_dsi_padctrl),
		GFP_KERNEL);
        if (!dsi_padctrl) {
                dev_err(&dc->ndev->dev, "dsi padctl: memory allocation failed\n");
                err = -ENOMEM;
		goto fail;
        }

	/*
	 * DSI pad control module is listed in dt immediately after DSI
	 * instances. Use DSI_PADCTRL_INSTANCE to get the resource for
	 * dsi pad control module.
	 */
	if (np_dsi && of_device_is_available(np_dsi)) {
		err = of_address_to_resource(np_dsi, DSI_PADCTRL_INSTANCE,
			&padctrl_res);
		if (err) {
			dev_err(&dc->ndev->dev, "dsi padctl: no mem res\n");
			goto fail;
		}
	}

	res = &padctrl_res;
	dsi_padctrl->base_res = request_mem_region(res->start,
		resource_size(res), dc->ndev->name);
	if (!dsi_padctrl->base_res) {
		dev_err(&dc->ndev->dev,
			"dsi patctl: request_mem_region failed\n");
		err = -EBUSY;
		goto fail;
	}

	dsi_padctrl->base_addr = ioremap(res->start, resource_size(res));
	if (!dsi_padctrl->base_addr) {
		dev_err(&dc->ndev->dev,
			"dsi patctl: Failed to map registers\n");
		err = -EINVAL;
		goto fail;
	}

	if (tegra_bpmp_running()) {
		dsi_padctrl->reset = of_reset_control_get(np_dsi, "dsi_padctrl");
		if (IS_ERR_OR_NULL(dsi_padctrl->reset)) {
			dev_err(&dc->ndev->dev, "dsi padctl: Failed to get reset\n");
			err = PTR_ERR(dsi_padctrl->reset);
			goto fail;
		}

		/* Reset dsi padctrl module */
		tegra_dsi_padctrl_reset(dsi_padctrl);
	}

	dsi_padctrl->prod_list = devm_tegra_prod_get_from_node(&dc->ndev->dev,
							       np_dsi);
	if (IS_ERR(dsi_padctrl->prod_list)) {
		dev_err(&dc->ndev->dev, "dsi padctl:prod list init failed%ld\n",
			PTR_ERR(dsi_padctrl->prod_list));
		dsi_padctrl->prod_list = NULL;
	}

	/* Set up active data and clock lanes mask */
	tegra_dsi_padctrl_setup_pwr_down_mask(dsi, dsi_padctrl);

	return dsi_padctrl;
fail:
	dev_err(&dc->ndev->dev, "dsi pactrl init failed %d\n", err);
	return ERR_PTR(err);
}

void tegra_dsi_padctrl_shutdown(struct tegra_dc *dc)
{
	struct tegra_dc_dsi_data *dsi = tegra_dc_get_outdata(dc);
	struct tegra_dsi_padctrl *dsi_padctrl;

	dsi_padctrl = dsi->pad_ctrl;

	/* Power down all DSI pads */
	tegra_dsi_padctrl_disable(dsi_padctrl);

	if (dsi_padctrl->prod_list) {
		dsi_padctrl->prod_list = NULL;
		dsi_padctrl->prod_settings_updated = false;
	}

	iounmap(dsi_padctrl->base_addr);
	release_mem_region(dsi_padctrl->base_res->start,
		resource_size(dsi_padctrl->base_res));
}
