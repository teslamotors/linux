/*
 * kernel/drivers/video/tegra/dc/fake_panel.c
 *
 * Copyright (c) 2014-2016, NVIDIA CORPORATION. All rights reserved.
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

#include <generated/mach-types.h>
#include "fake_panel.h"

#define DSI_PANEL_RESET		1

#define DC_CTRL_MODE	TEGRA_DC_OUT_CONTINUOUS_MODE

#ifndef CONFIG_TEGRA_HDMI_PRIMARY
static struct resource all_disp1_resources[] = {
	{
		/* keep fbmem as first variable in array for
		 * easy replacement
		 */
		.name	= "fbmem",
		.start	= 0,
		.end	= 0,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "irq",
#ifndef CONFIG_TEGRA_NVDISPLAY
		.start	= INT_DISPLAY_GENERAL,
		.end	= INT_DISPLAY_GENERAL,
#endif
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name	= "regs",
		.start	= TEGRA_DISPLAY_BASE,
		.end	= TEGRA_DISPLAY_BASE + TEGRA_DISPLAY_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "ganged_dsia_regs",
		.start	= TEGRA_DSI_BASE,
		.end	= TEGRA_DSI_BASE + TEGRA_DSI_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "ganged_dsib_regs",
		.start	= TEGRA_DSIB_BASE,
		.end	= TEGRA_DSIB_BASE + TEGRA_DSIB_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		/* init with dispa reg base*/
		.name	= "dsi_regs",
		.start	= TEGRA_DSI_BASE,
		.end	= TEGRA_DSI_BASE + TEGRA_DSI_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "mipi_cal",
		.start	= TEGRA_MIPI_CAL_BASE,
		.end	= TEGRA_MIPI_CAL_BASE + TEGRA_MIPI_CAL_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name   = "sor0",
		.start  = TEGRA_SOR_BASE,
		.end    = TEGRA_SOR_BASE + TEGRA_SOR_SIZE - 1,
		.flags  = IORESOURCE_MEM,
	},
	{
		.name   = "sor1",
		.start  = TEGRA_SOR1_BASE,
		.end    = TEGRA_SOR1_BASE + TEGRA_SOR1_SIZE - 1,
		.flags  = IORESOURCE_MEM,
	},
	{
		.name   = "dpaux",
		.start  = TEGRA_DPAUX_BASE,
		.end    = TEGRA_DPAUX_BASE + TEGRA_DPAUX_SIZE - 1,
		.flags  = IORESOURCE_MEM,
	},
	{
		.name	= "irq_dp",
#ifndef CONFIG_TEGRA_NVDISPLAY
		.start	= INT_DPAUX,
		.end	= INT_DPAUX,
#endif
		.flags	= IORESOURCE_IRQ,
	},

};
#endif

static int tegra_dc_add_fakedisp_resources(struct platform_device *ndev,
						long dc_outtype)
{
	/* Copy the existing fbmem resources locally
	 * and replace the existing resource pointer
	 * with local array
	 */
	struct resource *resources  = ndev->resource;
	int i;
	for (i = 0; i < ndev->num_resources; i++) {
		struct resource *r = &resources[i];
		if (resource_type(r) == IORESOURCE_MEM &&
			!strcmp(r->name, "fbmem")) {
			if (!strcmp(all_disp1_resources[0].name, "fbmem")) {
				all_disp1_resources[0].flags = r->flags;
				all_disp1_resources[0].start = r->start;
				all_disp1_resources[0].end  = r->end;
			}
		}
	}
	ndev->resource = all_disp1_resources;
	ndev->num_resources = ARRAY_SIZE(all_disp1_resources);

	for (i = 0; i < ndev->num_resources; i++) {
		struct resource *r = &all_disp1_resources[i];
		if (resource_type(r) == IORESOURCE_MEM &&
			(!strcmp(r->name, "dsi_regs"))) {
			if (dc_outtype == TEGRA_DC_OUT_FAKE_DSIB) {
				r->start = TEGRA_DSIB_BASE;
				r->end = TEGRA_DSIB_BASE +
						TEGRA_DSIB_SIZE - 1;
			} else if (dc_outtype == TEGRA_DC_OUT_FAKE_DSIA) {
				r->start = TEGRA_DSI_BASE;
				r->end = TEGRA_DSI_BASE + TEGRA_DSI_SIZE - 1;
			}
		}
	}
	return 0;
}

static struct tegra_dsi_out dsi_fake_panel_pdata = {
	.controller_vs = DSI_VS_1,
	.n_data_lanes = 4,

	.video_data_type = TEGRA_DSI_VIDEO_TYPE_VIDEO_MODE,
	.video_burst_mode = TEGRA_DSI_VIDEO_NONE_BURST_MODE,
	.refresh_rate = 60,

	.pixel_format = TEGRA_DSI_PIXEL_FORMAT_24BIT_P,
	.virtual_channel = TEGRA_DSI_VIRTUAL_CHANNEL_0,

	.panel_reset = DSI_PANEL_RESET,
	.power_saving_suspend = true,
	.video_clock_mode = TEGRA_DSI_VIDEO_CLOCK_TX_ONLY,

	.ulpm_not_supported = true,

};

static struct tegra_dc_mode dsi_fake_panel_modes[] = {
	{
#ifdef CONFIG_TEGRA_NVDISPLAY
		.pclk = 193224000, /* @60Hz*/
		.h_ref_to_sync = 1,
		.v_ref_to_sync = 11,
		.h_sync_width = 1,
		.v_sync_width = 1,
		.h_back_porch = 20,
		.v_back_porch = 7,
		.h_active = 1200,
		.v_active = 1920,
		.h_front_porch = 107,
		.v_front_porch = 497,
#else
		.pclk = 154700000, /* @60Hz*/
		.h_ref_to_sync = 4,
		.v_ref_to_sync = 1,
		.h_sync_width = 16,
		.v_sync_width = 2,
		.h_back_porch = 32,
		.v_back_porch = 16,
		.h_active = 1920,
		.v_active = 1200,
		.h_front_porch = 120,
		.v_front_porch = 17,
#endif
	},
};

static struct tegra_dc_mode dp_fake_panel_modes[] = {
	{
		.pclk = 148500000, /* @60Hz*/
		.h_ref_to_sync = 1,
		.v_ref_to_sync = 1,
		.h_sync_width = 44,
		.v_sync_width = 5,
		.h_back_porch = 148,
		.v_back_porch = 36,
		.h_active = 1920,
		.v_active = 1080,
		.h_front_porch = 88,
		.v_front_porch = 4,
	},
};

int tegra_dc_init_fake_panel_link_cfg(struct tegra_dc_dp_link_config *cfg)
{
	/*
	 * Currently fake the values for testing - same as eDp
	 * will need to add a method to update as needed
	 */
	if (!cfg->is_valid) {
		cfg->max_lane_count = 4;
		cfg->tps3_supported = false;
		cfg->support_enhanced_framing = true;
		cfg->downspread = true;
		cfg->support_fast_lt = true;
		cfg->aux_rd_interval = 0;
		cfg->alt_scramber_reset_cap = true;
		cfg->only_enhanced_framing = true;
		cfg->edp_cap = true;
		cfg->max_link_bw = 20;
		cfg->scramble_ena = 0;
		cfg->lt_data_valid = 0;
	}
	return 0;
}

int tegra_dc_init_fakedp_panel(struct tegra_dc *dc)
{
	struct tegra_dc_out *dc_out = dc->out;

	/* Set the needed resources */
	tegra_dc_add_fakedisp_resources(dc->ndev, TEGRA_DC_OUT_FAKE_DP);

	dc_out->align = TEGRA_DC_ALIGN_MSB,
	dc_out->order = TEGRA_DC_ORDER_RED_BLUE,
	dc_out->flags = TEGRA_DC_OUT_CONTINUOUS_MODE;
	dc_out->out_pins = NULL;
	dc_out->n_out_pins = 0;
	dc_out->depth = 18;
#if defined(CONFIG_TEGRA_NVDISPLAY)
	dc_out->parent_clk = "plld2";
#else
	dc_out->parent_clk = "pll_d_out0";
#endif
	dc_out->modes = dp_fake_panel_modes;
	dc_out->n_modes = ARRAY_SIZE(dp_fake_panel_modes);

	dc_out->enable = NULL;
	dc_out->disable = NULL;
	dc_out->postsuspend = NULL;
	dc_out->hotplug_gpio = -1;
	dc_out->postpoweron = NULL;

	return 0;
}

static int tegra_dc_reset_fakedsi_panel(struct tegra_dc *dc, long dc_outtype)
{
	struct tegra_dc_out *dc_out = dc->out;
	if (dc_outtype == TEGRA_DC_OUT_FAKE_DSI_GANGED) {
		dc_out->dsi->ganged_type = TEGRA_DSI_GANGED_SYMMETRIC_EVEN_ODD;
		dc_out->dsi->even_odd_split_width = 1;
		dc_out->dsi->dsi_instance = DSI_INSTANCE_0;
		dc_out->dsi->n_data_lanes = 8;
	} else if (dc_outtype == TEGRA_DC_OUT_FAKE_DSIB) {
		dc_out->dsi->ganged_type = 0;
		dc_out->dsi->dsi_instance = DSI_INSTANCE_1;
		dc_out->dsi->n_data_lanes = 4;
	} else if (dc_outtype == TEGRA_DC_OUT_FAKE_DSIA) {
		dc_out->dsi->ganged_type = 0;
		dc_out->dsi->dsi_instance = DSI_INSTANCE_0;
		dc_out->dsi->n_data_lanes = 4;
	}

	return 0;
}

int tegra_dc_init_fakedsi_panel(struct tegra_dc *dc, long dc_outtype)
{
	struct tegra_dc_out *dc_out = dc->out;
	/* Set the needed resources */

	tegra_dc_add_fakedisp_resources(dc->ndev, dc_outtype);

	dc_out->dsi = &dsi_fake_panel_pdata;
#if defined(CONFIG_TEGRA_NVDISPLAY)
	dc_out->parent_clk = "pllp_display";
#else
	dc_out->parent_clk = "pll_d_out0";
#endif
	dc_out->modes = dsi_fake_panel_modes;
	dc_out->n_modes = ARRAY_SIZE(dsi_fake_panel_modes);
	dc_out->enable = NULL;
	dc_out->postpoweron = NULL;
	dc_out->disable = NULL;
	dc_out->postsuspend	= NULL;
	dc_out->width = 217;
	dc_out->height = 135;
	dc_out->flags = DC_CTRL_MODE;
	tegra_dc_reset_fakedsi_panel(dc, dc_outtype);

	if (!dc->out->sd_settings)
		return -EINVAL;
	dc->out->sd_settings->enable = 1;
	dc->out->sd_settings->enable_int = 1;

	return 0;
}


int tegra_dc_destroy_dsi_resources(struct tegra_dc *dc, long dc_outtype)
{
	struct tegra_dc_dsi_data *dsi = tegra_dc_get_outdata(dc);
	int i = 0;
	struct resource *res;
	struct resource dsi_res;
#ifdef CONFIG_TEGRA_NVDISPLAY
	char *ganged_reg_name[4] = {"ganged_dsia_regs", "ganged_dsib_regs",
					NULL, NULL};
#else
	char *ganged_reg_name[2] = {"ganged_dsia_regs", "ganged_dsib_regs"};
#endif
	struct device_node *np = dc->ndev->dev.of_node;
#ifdef CONFIG_OF
	struct device_node *np_dsi =
		of_find_node_by_path(DSI_NODE);
#else
	struct device_node *np_dsi = NULL;
#endif

	if (!dsi) {
		dev_err(&dc->ndev->dev, " dsi: allocation deleted\n");
		of_node_put(np_dsi);
		return -ENOMEM;
	}

	dsi->max_instances = dc->out->dsi->ganged_type ? MAX_DSI_INSTANCE : 1;

	mutex_lock(&dsi->lock);
	tegra_dc_io_start(dc);
	for (i = 0; i < dsi->max_instances; i++) {
		if (np) {
			if (np_dsi && of_device_is_available(np_dsi)) {
				if (!dc->out->dsi->ganged_type)
					of_address_to_resource(np_dsi,
						dc->out->dsi->dsi_instance,
						&dsi_res);
				else /* ganged type */
					of_address_to_resource(np_dsi,
						i, &dsi_res);
				res = &dsi_res;
			} else {
				return -EINVAL;
			}
		} else {
			res = platform_get_resource_byname(dc->ndev,
				IORESOURCE_MEM,
				dc->out->dsi->ganged_type ?
				ganged_reg_name[i] :
				ganged_reg_name[dsi->info.dsi_instance]);
		}
		if (!res) {
			dev_err(&dc->ndev->dev, "dsi: no mem resource\n");
			return -ENOENT;
		}

		if (dsi->base[i])
			iounmap(dsi->base[i]);
		if (dsi->base_res[i])
			release_mem_region(res->start, resource_size(res));
		dsi->base[i] = NULL;
		dsi->base_res[i] = NULL;
	}

	if (dsi->avdd_dsi_csi) {
		regulator_put(dsi->avdd_dsi_csi);
		dsi->avdd_dsi_csi = NULL;
	}
#ifndef COMMON_MIPICAL_SUPPORTED
	if (dsi->mipi_cal)
		tegra_mipi_cal_destroy(dc);
#endif

#if defined (CONFIG_ARCH_TEGRA_18x_SOC)
	if (dsi->pad_ctrl)
		tegra_dsi_padctrl_shutdown(dc);
#endif

	tegra_dc_io_end(dc);
	mutex_unlock(&dsi->lock);

	return 0;
}


int tegra_dc_reinit_dsi_resources(struct tegra_dc *dc, long dc_outtype)
{
	struct resource *res;
	struct resource dsi_res;

	int err = 0, i;
#ifdef CONFIG_TEGRA_NVDISPLAY
	char *ganged_reg_name[4] = {"ganged_dsia_regs", "ganged_dsib_regs",
					NULL, NULL};
#else
	char *ganged_reg_name[4] = {"ganged_dsia_regs", "ganged_dsib_regs"};
#endif

	struct device_node *np = dc->ndev->dev.of_node;
#ifdef CONFIG_OF
	struct device_node *np_dsi =
		of_find_node_by_path(DSI_NODE);
#else
	struct device_node *np_dsi = NULL;
#endif
	struct tegra_dc_dsi_data *dsi = tegra_dc_get_outdata(dc);

	if (!dsi) {
		dev_err(&dc->ndev->dev, " dsi: allocation deleted\n");
		of_node_put(np_dsi);
		return -ENOMEM;
	}

	/* Since all fake DSI share the same DSI pointer, need to reset here */
	/* to avoid misconfigurations when switching between fake DSI types */
	tegra_dc_reset_fakedsi_panel(dc, dc_outtype);

	dsi->max_instances = dc->out->dsi->ganged_type ? MAX_DSI_INSTANCE : 1;

	for (i = 0; i < dsi->max_instances; i++) {
		if (np) {
			if (np_dsi && of_device_is_available(np_dsi)) {
				if (!dc->out->dsi->ganged_type)
					of_address_to_resource(np_dsi,
						dc->out->dsi->dsi_instance,
						&dsi_res);
				else /* ganged type */
					of_address_to_resource(np_dsi,
						i, &dsi_res);
				res = &dsi_res;
			} else {
				err = -EINVAL;
				goto err_release_regs;
			}
		} else {
			res = platform_get_resource_byname(dc->ndev,
				IORESOURCE_MEM,
				dc->out->dsi->ganged_type ?
				ganged_reg_name[i] :
				ganged_reg_name[dsi->info.dsi_instance]);
		}
		if (!res) {
			dev_err(&dc->ndev->dev, "dsi: no mem resource\n");
			err = -ENOENT;
			goto err_release_regs;
		}

		dsi->base_res[i] = request_mem_region(res->start,
				resource_size(res), dc->ndev->name);
		if (!dsi->base_res[i]) {
			dev_err(&dc->ndev->dev,
				"dsi: request_mem_region failed\n");
			err = -EBUSY;
			goto err_release_regs;
		}

		dsi->base[i] = ioremap(res->start, resource_size(res));
		if (!dsi->base[i]) {
			dev_err(&dc->ndev->dev,
				"dsi: registers can't be mapped\n");
			err = -EBUSY;
			goto err_release_regs;
		}

	}

	dsi->avdd_dsi_csi =  regulator_get(&dc->ndev->dev, "avdd_dsi_csi");
	if (IS_ERR_OR_NULL(dsi->avdd_dsi_csi)) {
		dev_err(&dc->ndev->dev, "dsi: avdd_dsi_csi reg get failed\n");
		err = -ENODEV;
		goto err_release_regs;
	}
#ifndef COMMON_MIPICAL_SUPPORTED
	dsi->mipi_cal = tegra_mipi_cal_init_sw(dc);
	if (IS_ERR(dsi->mipi_cal)) {
		dev_err(&dc->ndev->dev, "dsi: mipi_cal sw init failed\n");
		err = PTR_ERR(dsi->mipi_cal);
		goto err_release_regs;
	}
#endif
#if defined (CONFIG_ARCH_TEGRA_18x_SOC)
	dsi->pad_ctrl = tegra_dsi_padctrl_init(dc);
	if (IS_ERR(dsi->pad_ctrl)) {
		dev_err(&dc->ndev->dev, "dsi: Padctrl sw init failed\n");
		err = PTR_ERR(dsi->pad_ctrl);
		goto err_mipical_dest;
	}
	dsi->info.enable_hs_clock_on_lp_cmd_mode = true;
#endif
	/* Need to always reinitialize clocks to ensure proper functionality */
	tegra_dsi_init_clock_param(dc);
	of_node_put(np_dsi);
	return 0;

#if defined (CONFIG_ARCH_TEGRA_18x_SOC)
err_mipical_dest:
#ifndef COMMON_MIPICAL_SUPPORTED
	if(dsi->mipi_cal)
		tegra_mipi_cal_destroy(dc);
#endif
#endif
err_release_regs:
	if (dsi->avdd_dsi_csi)
		regulator_put(dsi->avdd_dsi_csi);

	for (i = 0; i < dsi->max_instances; i++) {
		if (dsi->base_res[i])
			release_resource(dsi->base_res[i]);
	}
	of_node_put(np_dsi);
	return err;
}

