/*
 * drivers/video/tegra/dc/null_or.c
 *
 * Copyright (c) 2015, NVIDIA CORPORATION, All rights reserved.
 * Author: Aron Wong <awong@nvidia.com>
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

#include <linux/kernel.h>
#include <mach/dc.h>
#include <linux/of_irq.h> /*for INT_DISPLAY_GENERAL, INT_DPAUX*/

#include "dc_reg.h"
#include "dc_priv.h"
#include "null_or.h"

#include "../../../../arch/arm/mach-tegra/iomap.h"

#define DRIVER_NAME "null_or"

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
#ifdef CONFIG_TEGRA_NVDISPLAY
	{
		.name   = "split_dsia_regs",
		.start  = TEGRA_DSI_BASE,
		.end    = TEGRA_DSI_BASE + TEGRA_DSI_SIZE - 1,
		.flags  = IORESOURCE_MEM,
	},
	{
		.name   = "split_dsib_regs",
		.start  = TEGRA_DSIB_BASE,
		.end    = TEGRA_DSIB_BASE + TEGRA_DSIB_SIZE - 1,
		.flags  = IORESOURCE_MEM,
	},
	{
		.name   = "split_dsic_regs",
		.start  = TEGRA_DSIC_BASE,
		.end    = TEGRA_DSIC_BASE + TEGRA_DSIC_SIZE - 1,
		.flags  = IORESOURCE_MEM,
	},
	{
		.name   = "split_disd_regs",
		.start  = TEGRA_DSID_BASE,
		.end    = TEGRA_DSID_BASE + TEGRA_DSID_SIZE - 1,
		.flags  = IORESOURCE_MEM,
	},
	{
		.name   = "dsi_pad_reg",
		.start  = TEGRA_DSI_PADCTL_BASE,
		.end    = TEGRA_DSI_PADCTL_BASE + TEGRA_DSI_PADCTL_SIZE - 1,
		.flags  = IORESOURCE_MEM,
	},
#endif
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


static int tegra_dc_null_init(struct tegra_dc *dc)
{
	dev_dbg(&dc->ndev->dev, ":" DRIVER_NAME ":%s()\n", __func__);
	if (dc->out->flags & TEGRA_DC_OUT_ONE_SHOT_MODE) {
		dev_warn(&dc->ndev->dev, ":" DRIVER_NAME
			": TEGRA_DC_OUT_ONE_SHOT_MODE not supported.\n");
		/* suppress the one-shot flag */
		dc->out->flags &= ~TEGRA_DC_OUT_ONE_SHOT_MODE;
	}
	return 0;
}

static void tegra_dc_null_destroy(struct tegra_dc *dc)
{
	dev_dbg(&dc->ndev->dev, ":" DRIVER_NAME ":%s()\n", __func__);
}

static void tegra_dc_null_enable(struct tegra_dc *dc)
{
	dev_dbg(&dc->ndev->dev, ":" DRIVER_NAME ":%s()\n", __func__);
	tegra_dc_writel(dc, DISP_CTRL_MODE_C_DISPLAY, DC_CMD_DISPLAY_COMMAND);
	tegra_dc_writel(dc, GENERAL_UPDATE, DC_CMD_STATE_CONTROL);
	tegra_dc_writel(dc, GENERAL_ACT_REQ, DC_CMD_STATE_CONTROL);
	dc->connected = true;
}

static void tegra_dc_null_disable(struct tegra_dc *dc)
{
	dev_dbg(&dc->ndev->dev, ":" DRIVER_NAME ":%s()\n", __func__);
	tegra_dc_writel(dc, DISP_CTRL_MODE_STOP, DC_CMD_DISPLAY_COMMAND);
	tegra_dc_writel(dc, GENERAL_UPDATE, DC_CMD_STATE_CONTROL);
	tegra_dc_writel(dc, GENERAL_ACT_REQ, DC_CMD_STATE_CONTROL);
}

/* used by tegra_dc_probe() to detect connection(HPD) status at boot */
/* always return true as null output equivalent to internal panel    */
static bool tegra_dc_null_detect(struct tegra_dc *dc)
{
	dev_dbg(&dc->ndev->dev, ":" DRIVER_NAME ":%s()\n", __func__);
	return true;
}

static void tegra_dc_null_hold_host(struct tegra_dc *dc)
{
	dev_dbg(&dc->ndev->dev, ":" DRIVER_NAME ":%s()\n", __func__);
}

static void tegra_dc_null_release_host(struct tegra_dc *dc)
{
	dev_dbg(&dc->ndev->dev, ":" DRIVER_NAME ":%s()\n", __func__);
}

static void tegra_dc_null_idle(struct tegra_dc *dc)
{
	dev_dbg(&dc->ndev->dev, ":" DRIVER_NAME ":%s()\n", __func__);
}

#ifdef CONFIG_PM
static void tegra_dc_null_suspend(struct tegra_dc *dc)
{
	dev_dbg(&dc->ndev->dev, ":" DRIVER_NAME ":%s()\n", __func__);
}

static void tegra_dc_null_resume(struct tegra_dc *dc)
{
	dev_dbg(&dc->ndev->dev, ":" DRIVER_NAME ":%s()\n", __func__);
}
#endif

/* test if mode is supported */
static bool tegra_dc_null_mode_filter(const struct tegra_dc *dc,
	struct fb_videomode *mode)
{
	dev_dbg(&dc->ndev->dev, ":" DRIVER_NAME ":%s()\n", __func__);
	/* TODO: check the constraints of DC to avoid setting illegal modes */
	return true;
}

/* setup pixel clock for the current mode, return mode's adjusted pclk. */
static long tegra_dc_null_setup_clk(struct tegra_dc *dc, struct clk *clk)
{
	static char *pllds[] =
#ifdef CONFIG_TEGRA_NVDISPLAY
		{"pll_d", "plld2", "plld3"};
#else
		{"pll_d_out0", "pll_d2"};
#endif
	const char *clk_name = pllds[dc->ndev->id];
	struct clk *parent_clk =
#ifdef CONFIG_TEGRA_NVDISPLAY
		tegra_disp_clk_get(&dc->ndev->dev, clk_name);
#else
		clk_get_sys(NULL, clk_name);
#endif
	struct clk *base_clk;
	long rate;

	if ((clk == NULL) || (parent_clk == NULL) || tegra_platform_is_linsim())
		return 0;

	if (dc->out != NULL)
		dc->out->parent_clk = clk_name;
	else
		return 0;

	/* configure the clock rate to something low before we switch to it. */
	if (clk != dc->clk) {
		base_clk = clk_get_parent(parent_clk);
		rate = 100800000;
		clk_set_rate(base_clk, dc->mode.pclk);
		if (clk_get_parent(clk) != parent_clk)
			clk_set_parent(clk, parent_clk);
		clk_set_rate(clk, dc->mode.pclk / 4);
	}

	rate = dc->mode.pclk;
	if (WARN(rate > 600000000, "pixel clock too big"))
		rate = dc->mode.pclk;
	else if (rate >= 300000000)
		rate = dc->mode.pclk;
	else if (rate >= 150000000)
		rate = dc->mode.pclk * 2;
	else
		rate = dc->mode.pclk * 4;

	if (rate != clk_get_rate(parent_clk))
		clk_set_rate(parent_clk, rate);

	if (clk_get_parent(clk) != parent_clk)
		clk_set_parent(clk, parent_clk);

	rate = tegra_dc_pclk_round_rate(dc, dc->mode.pclk);
	return rate;
}

static bool tegra_dc_null_osidle(struct tegra_dc *dc)
{
	/* TODO: figure out how to NC_HOST_TRIG
	   when in DISP_CTRL_MODE_NC_DISPLAY */
	if (dc->out->flags & TEGRA_DC_OUT_ONE_SHOT_MODE)
		return true;
	else
		return false;
}

static void tegra_dc_null_modeset_notifier(struct tegra_dc *dc)
{
	dev_dbg(&dc->ndev->dev, ":" DRIVER_NAME ":%s()\n", __func__);
}

struct tegra_dc_out_ops tegra_dc_null_ops = {
	.init	   = tegra_dc_null_init,
	.destroy   = tegra_dc_null_destroy,
	.enable	   = tegra_dc_null_enable,
	.disable   = tegra_dc_null_disable,
	.detect    = tegra_dc_null_detect,
	.hold = tegra_dc_null_hold_host,
	.release = tegra_dc_null_release_host,
	.idle = tegra_dc_null_idle,
#ifdef CONFIG_PM
	.suspend = tegra_dc_null_suspend,
	.resume = tegra_dc_null_resume,
#endif
	.setup_clk = tegra_dc_null_setup_clk,
	.mode_filter = tegra_dc_null_mode_filter,
	.osidle = tegra_dc_null_osidle,
	.modeset_notifier = tegra_dc_null_modeset_notifier,
	/* TODO: partial_update */
};

static int tegra_dc_add_fakedisp_resources(struct platform_device *ndev)
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
			} else
				pr_info("Error - First variable is not fbmem\n");
		}
	}
	ndev->resource = all_disp1_resources;
	ndev->num_resources = ARRAY_SIZE(all_disp1_resources);

	return 0;
}

int tegra_dc_init_null_or(struct tegra_dc *dc)
{
	struct tegra_dc_out *dc_out = dc->out;

	/* Set the needed resources */
	tegra_dc_add_fakedisp_resources(dc->ndev);

	dc_out->align = TEGRA_DC_ALIGN_MSB,
	dc_out->order = TEGRA_DC_ORDER_RED_BLUE,
	dc_out->flags = TEGRA_DC_OUT_CONTINUOUS_MODE;
	dc_out->out_pins = NULL;
	dc_out->n_out_pins = 0;
	dc_out->depth = 24;
	dc_out->enable = NULL;
	dc_out->disable = NULL;
	dc_out->postsuspend = NULL;
	dc_out->hotplug_gpio = -1;
	dc_out->postpoweron = NULL;
	dc_out->parent_clk = NULL;

	return 0;
}

