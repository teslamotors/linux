/*
 * drivers/video/tegra/dc/lvds.c
 *
 * Copyright (c) 2012-2015, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/err.h>
#include <linux/kernel.h>

#include <mach/dc.h>
#include <linux/clk.h>

#include "lvds.h"
#include "dc_priv.h"
#include "edid.h"

static int tegra_dc_lvds_init(struct tegra_dc *dc)
{
	struct tegra_dc_lvds_data *lvds;
	int err;
	struct device_node *np = dc->ndev->dev.of_node;
	struct device_node *np_panel = NULL;
	bool virtual_edid = false;

	lvds = devm_kzalloc(&dc->ndev->dev, sizeof(*lvds), GFP_KERNEL);
	if (!lvds)
		return -ENOMEM;

	if (np) {
		np_panel = tegra_get_panel_node_out_type_check(dc,
			TEGRA_DC_OUT_LVDS);
		if (np_panel && of_device_is_available(np_panel)) {
			virtual_edid = of_property_read_bool(np_panel,
					"nvidia,edid");
			of_node_put(np_panel);
		} else {
			err = -EINVAL;
			of_node_put(np_panel);
			goto err_init;
		}
	}
	lvds->dc = dc;
	lvds->sor = tegra_dc_sor_init(dc, NULL);

	if (IS_ERR_OR_NULL(lvds->sor)) {
		err = PTR_ERR(lvds->sor);
		lvds->sor = NULL;
		goto err_init;
	}
	if (virtual_edid) {
		lvds->edid = tegra_edid_create(dc, tegra_dc_edid_blob);
		if (IS_ERR_OR_NULL(lvds->edid)) {
			dev_err(&dc->ndev->dev,
				"lvds: failed to create edid obj\n");
			err = PTR_ERR(lvds->edid);
			goto err_init;
		}
		tegra_dc_set_edid(dc, lvds->edid);
	} else if (dc->out->n_modes <= 0) {
		err = -EINVAL;
		goto err_init;
}
	tegra_dc_set_outdata(dc, lvds);
	return 0;
err_init:
	devm_kfree(&dc->ndev->dev, lvds);

	return err;
}


static void tegra_dc_lvds_destroy(struct tegra_dc *dc)
{
	struct tegra_dc_lvds_data *lvds = tegra_dc_get_outdata(dc);

	if (lvds->sor)
		tegra_dc_sor_destroy(lvds->sor);
	devm_kfree(&dc->ndev->dev, lvds);
	if (lvds->edid)
		tegra_edid_destroy(lvds->edid);
}

static int tegra_lvds_edid(struct tegra_dc_lvds_data *lvds)
{
	struct tegra_dc *dc = lvds->dc;
	struct fb_monspecs specs;
	int err;

	memset(&specs, 0 , sizeof(specs));

	err = tegra_edid_get_monspecs(lvds->edid, &specs);
	if (err < 0) {
		dev_err(&dc->ndev->dev,
			"lvds: Failed to get EDID data\n");
		goto fail;
	}

	/* set bpp if EDID provides primary color depth */
	dc->out->depth =
		dc->out->depth ? : specs.bpc ? specs.bpc * 3 : 18;
	dev_info(&dc->ndev->dev,
		"lvds: EDID: %d bpc panel, set to %d bpp\n",
		 specs.bpc, dc->out->depth);

	/* in mm */
	dc->out->h_size = dc->out->h_size ? : specs.max_x * 10;
	dc->out->v_size = dc->out->v_size ? : specs.max_y * 10;

	/*
	 * EDID specifies either the acutal screen sizes or
	 * the aspect ratios. The panel file can choose to
	 * trust the value as the actual sizes by leaving
	 * width/height to 0s
	 */
	dc->out->width = dc->out->width ? : dc->out->h_size;
	dc->out->height = dc->out->height ? : dc->out->v_size;

	if (!dc->out->modes)
		tegra_dc_set_fb_mode(dc, specs.modedb, false);

	tegra_dc_setup_clk(dc, dc->clk);
	kfree(specs.modedb);
	return 0;
fail:
	return err;
}

static void tegra_dc_lvds_enable(struct tegra_dc *dc)
{
	struct tegra_dc_lvds_data *lvds = tegra_dc_get_outdata(dc);

	if (lvds->edid && !lvds->edid->data)
		tegra_lvds_edid(lvds);

	tegra_dc_io_start(dc);
	tegra_sor_clk_enable(lvds->sor);
	/* Power on panel */
	tegra_sor_pad_cal_power(lvds->sor, true);
	tegra_dc_sor_set_internal_panel(lvds->sor, true);
	tegra_dc_sor_set_power_state(lvds->sor, 1);
	tegra_dc_sor_enable_lvds(lvds->sor, false, false);
	tegra_dc_io_end(dc);
}

static void tegra_dc_lvds_disable(struct tegra_dc *dc)
{
	struct tegra_dc_lvds_data *lvds = tegra_dc_get_outdata(dc);

	/* Power down SOR */
	tegra_dc_sor_disable(lvds->sor, true);
}


static void tegra_dc_lvds_suspend(struct tegra_dc *dc)
{
	struct tegra_dc_lvds_data *lvds = tegra_dc_get_outdata(dc);

	tegra_dc_lvds_disable(dc);
	lvds->suspended = true;
}


static void tegra_dc_lvds_resume(struct tegra_dc *dc)
{
	struct tegra_dc_lvds_data *lvds = tegra_dc_get_outdata(dc);

	if (!lvds->suspended)
		return;
	tegra_dc_lvds_enable(dc);
}

static long tegra_dc_lvds_setup_clk(struct tegra_dc *dc, struct clk *clk)
{
	struct tegra_dc_lvds_data *lvds = tegra_dc_get_outdata(dc);
	struct clk	*parent_clk;

	tegra_sor_setup_clk(lvds->sor, clk, true);

	parent_clk = clk_get_parent(clk);
	if (clk_get_parent(lvds->sor->sor_clk) != parent_clk)
		clk_set_parent(lvds->sor->sor_clk, parent_clk);

	return tegra_dc_pclk_round_rate(dc, lvds->sor->dc->mode.pclk);
}

struct tegra_dc_out_ops tegra_dc_lvds_ops = {
	.init	   = tegra_dc_lvds_init,
	.destroy   = tegra_dc_lvds_destroy,
	.enable	   = tegra_dc_lvds_enable,
	.disable   = tegra_dc_lvds_disable,
	.suspend   = tegra_dc_lvds_suspend,
	.resume	   = tegra_dc_lvds_resume,
	.setup_clk = tegra_dc_lvds_setup_clk,
};
