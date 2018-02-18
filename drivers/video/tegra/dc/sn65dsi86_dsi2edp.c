/*
 * drivers/video/tegra/dc/sn65dsi86_dsi2edp.c
 *
 * Copyright (c) 2013-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * Author:
 *	Bibek Basu <bbasu@nvidia.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */

#include <linux/i2c.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/regmap.h>
#include <linux/swab.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <mach/dc.h>
#include "dc_priv.h"
#include "sn65dsi86_dsi2edp.h"
#include "dsi.h"

static struct tegra_dc_dsi2edp_data *sn65dsi86_dsi2edp;
static struct i2c_client *sn65dsi86_i2c_client;

enum i2c_transfer_type {
	I2C_WRITE,
	I2C_READ,
};

static inline int sn65dsi86_reg_write(struct tegra_dc_dsi2edp_data *dsi2edp,
					unsigned int addr, unsigned int val)
{
	return regmap_write(dsi2edp->regmap, addr, val);
}

static inline void sn65dsi86_reg_read(struct tegra_dc_dsi2edp_data *dsi2edp,
					unsigned int addr, unsigned int *val)
{
	regmap_read(dsi2edp->regmap, addr, val);
}

static const struct regmap_config sn65dsi86_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
};

static int sn65dsi86_dsi2edp_init(struct tegra_dc_dsi_data *dsi)
{
	int err = 0;
	struct tegra_dc_dsi2edp_data *dsi2edp = sn65dsi86_dsi2edp;
	if (!sn65dsi86_dsi2edp)
		return -ENODEV;

	dsi2edp->dsi = dsi;
	dsi2edp->mode = &dsi->dc->mode;
	tegra_dsi_set_outdata(dsi, dsi2edp);

	if (dsi2edp->init.en_gpio) {
		err = gpio_request(dsi2edp->init.en_gpio, "dsi2dp");
		if (err < 0) {
			pr_err("err %d: dsi2dp GPIO request failed\n", err);
		} else {
			if (dsi2edp->init.en_gpio_flags & OF_GPIO_ACTIVE_LOW)
				gpio_direction_output(dsi2edp->init.en_gpio,
					0);
			else
				gpio_direction_output(dsi2edp->init.en_gpio,
					1);
		}
	}

	return 0;
}

static void sn65dsi86_dsi2edp_destroy(struct tegra_dc_dsi_data *dsi)
{
	struct tegra_dc_dsi2edp_data *dsi2edp =
				tegra_dsi_get_outdata(dsi);

	if (!dsi2edp)
		return;

	if (dsi2edp->init.en_gpio) {
		if (dsi2edp->init.en_gpio_flags & OF_GPIO_ACTIVE_LOW)
			gpio_set_value(dsi2edp->init.en_gpio, 1);
		else
			gpio_set_value(dsi2edp->init.en_gpio, 0);
	}
	sn65dsi86_dsi2edp = NULL;
	mutex_destroy(&dsi2edp->lock);
}


static void sn65dsi86_dsi2edp_enable(struct tegra_dc_dsi_data *dsi)
{
	struct tegra_dc_dsi2edp_data *dsi2edp = tegra_dsi_get_outdata(dsi);
	unsigned val = 0;
	unsigned retry = 0;
	int      hpd;

	if (dsi2edp && dsi2edp->dsi2edp_enabled)
		return;

	pr_info("SN65DSI86: %dx%d@%d %d-%d-%d/%d-%d-%d lanes:%d HS:%dKHz\n",
		dsi->dc->mode.h_active, dsi->dc->mode.v_active,
		dsi->info.refresh_rate, dsi->dc->mode.h_front_porch,
		dsi->dc->mode.h_sync_width, dsi->dc->mode.h_back_porch,
		dsi->dc->mode.v_front_porch, dsi->dc->mode.v_sync_width,
		dsi->dc->mode.v_back_porch, dsi->info.n_data_lanes,
		dsi->target_hs_clk_khz);

	mutex_lock(&dsi2edp->lock);

	/* reg.0x0a REFCLK */
	sn65dsi86_reg_write(dsi2edp, SN65DSI86_PLL_REFCLK_CFG,
			dsi2edp->init.pll_refclk_cfg);

	/* reg.0x10 DSI config */
	val = dsi->info.ganged_type ? (0 << 5) : (1 << 5);
	val |= (TEGRA_DSI_GANGED_SYMMETRIC_LEFT_RIGHT == dsi->info.ganged_type)
		? (1 << 7) : (0 << 7);
	val |= (4 - dsi->info.n_data_lanes / (dsi->info.ganged_type ? 2 : 1))
		<< 3;
	if (dsi->info.ganged_type)
		val |= (4 - dsi->info.n_data_lanes / 2) << 1;
	sn65dsi86_reg_write(dsi2edp, SN65DSI86_DSI_CFG1, val);

	/* reg.0x12-0x13 DSI CLK range */
	sn65dsi86_reg_write(dsi2edp, SN65DSI86_DSI_CHA_CLK_RANGE,
		dsi->target_hs_clk_khz / 5000);
	if (dsi->info.ganged_type)
		sn65dsi86_reg_write(dsi2edp, SN65DSI86_DSI_CHB_CLK_RANGE,
			dsi->target_hs_clk_khz / 5000);
	if (!(40000 <= dsi->target_hs_clk_khz
		&& dsi->target_hs_clk_khz < 755000))
		pr_warn("SN65DSI86: DC%d DSI HS clk %dKHz is out of range!\n",
			dsi->dc->ndev->id, dsi->target_hs_clk_khz);

	/* disable ASSR via TEST2 PULL UP */
	if (dsi2edp->init.disable_assr) {
		sn65dsi86_reg_write(dsi2edp, 0xFF, 0x07);
		sn65dsi86_reg_write(dsi2edp, 0x16, 0x01);
		sn65dsi86_reg_write(dsi2edp, 0xFF, 0x00);
	}
	/* reg.0x5a enhanced framing */
	sn65dsi86_reg_write(dsi2edp, SN65DSI86_FRAMING_CFG, (1 << 2));
	usleep_range(11000, 12000);  /* need a min 10mSec delay */
	/* reg.0x93 DP num of lanes & SSC */
	sn65dsi86_reg_write(dsi2edp, SN65DSI86_DP_SSC_CFG,
			dsi2edp->init.dp_ssc_cfg);
	usleep_range(11000, 12000);  /* need a min 10mSec delay */
	/* reg.0x94 L0mV HBR */
	sn65dsi86_reg_write(dsi2edp, SN65DSI86_DP_CFG, 0x80);
	usleep_range(11000, 12000);  /* need a min 10mSec delay */

	/* reg.0x0d enable DP PLL */
	sn65dsi86_reg_write(dsi2edp, SN65DSI86_PLL_EN, (1 << 0));
	retry = 0;
	do {
		usleep_range(2000, 2200);
		/* DP_PLL_LOCK */
		sn65dsi86_reg_read(dsi2edp, SN65DSI86_PLL_REFCLK_CFG, &val);
	} while (((val & (1 << 7)) == 0) && (retry++ < RETRY_PLL));
	if ((val & (1 << 7)) == 0)
		pr_err("SN65DSI86: DP_PLL not locked\n");
	usleep_range(1000, 1100);

	/* reg.0x95 POST2 0dB */
	sn65dsi86_reg_write(dsi2edp, SN65DSI86_TRAINING_CFG, 0x00);

	/* reg.0x5c HPD detection may take up to 100mSec */
	for (retry = 0; retry < RETRY_HPD; retry++) {
		sn65dsi86_reg_read(dsi2edp, SN65DSI86_REG_0x5c, &hpd);
		if (hpd & (1 << 4))  break;
		usleep_range(5000, 5500);
	}
	if (hpd & (1 << 0))  /* HPD disable makes it on */
		pr_info("SN65DSI86: DP HPD is not used\n");
	else
		pr_info("SN65DSI86: DP HPD%s detected\n",
			(hpd & (1 << 4)) ? "" : " not");
	hpd = (hpd & (1 << 4)) ? 1 : 0;

	/* reg.0x96 Semi-Auto Link-training
	   takes about 15~90mSec. some monitor takes up to 900mSec */
	if (hpd) {
		retry = 0;
		do {
			sn65dsi86_reg_write(dsi2edp,
				SN65DSI86_ML_TX_MODE, 0x0a);
			usleep_range(5000, 5500);
			sn65dsi86_reg_read(dsi2edp,
				SN65DSI86_ML_TX_MODE, &val);
		} while ((val != 0x1) && (retry++ < RETRY_LT));
		if (val != 0x1)
			pr_err("SN65DSI86: semi-auto link training failed\n");
		usleep_range(1000, 1100);
	}

	/* reg.0x20-0x21 ch.a h-active */
	sn65dsi86_reg_write(dsi2edp, SN65DSI86_VIDEO_CHA_LINE_LOW,
		(dsi->dc->mode.h_active / (dsi->info.ganged_type ? 2 : 1))
		& 0xff);
	sn65dsi86_reg_write(dsi2edp, SN65DSI86_VIDEO_CHA_LINE_HIGH,
		(dsi->dc->mode.h_active / (dsi->info.ganged_type ? 2 : 1))
		>> 8);
	/* reg.0x22-0x23 ch.b h-active */
	if (dsi->info.ganged_type) {
		sn65dsi86_reg_write(dsi2edp, SN65DSI86_VIDEO_CHB_LINE_LOW,
			(dsi->dc->mode.h_active / 2) & 0xff);
		sn65dsi86_reg_write(dsi2edp, SN65DSI86_VIDEO_CHB_LINE_HIGH,
			(dsi->dc->mode.h_active / 2) >> 8);
	}
	/* reg.0x24-0x25 v-active */
	sn65dsi86_reg_write(dsi2edp, SN65DSI86_CHA_VERT_DISP_SIZE_LOW,
			dsi->dc->mode.v_active & 0xff);
	sn65dsi86_reg_write(dsi2edp, SN65DSI86_CHA_VERT_DISP_SIZE_HIGH,
			dsi->dc->mode.v_active >> 8);
	/* reg.0x2c-0x2d h-sync-width */
	sn65dsi86_reg_write(dsi2edp, SN65DSI86_CHA_HSYNC_PULSE_WIDTH_LOW,
			dsi->dc->mode.h_sync_width & 0xff);
	sn65dsi86_reg_write(dsi2edp, SN65DSI86_CHA_HSYNC_PULSE_WIDTH_HIGH,
			((dsi->dc->mode.h_sync_width >> 8) & 0x7f)
			| (dsi2edp->init.negative_hsync ? (1 << 7) : 0));
	/* reg.0x30-0x31 v-sync-width */
	sn65dsi86_reg_write(dsi2edp, SN65DSI86_CHA_VSYNC_PULSE_WIDTH_LOW,
			dsi->dc->mode.v_sync_width & 0xff);
	sn65dsi86_reg_write(dsi2edp, SN65DSI86_CHA_VSYNC_PULSE_WIDTH_HIGH,
			((dsi->dc->mode.v_sync_width >> 8) & 0x7f)
			| (dsi2edp->init.negative_vsync ? (1 << 7) : 0));
	if (dsi->dc->mode.v_sync_width < 1)
		pr_warn("SN65DSI86: V-Sync-Width %d is not valid\n",
			dsi->dc->mode.v_sync_width);
	/* reg.0x34 h-back-porch */
	sn65dsi86_reg_write(dsi2edp, SN65DSI86_CHA_HORIZONTAL_BACK_PORCH,
			dsi->dc->mode.h_back_porch & 0xff);
	if (dsi->dc->mode.h_back_porch >> 8)
		pr_warn("SN65DSI86: H-Back-Porch %d is out of range\n",
			dsi->dc->mode.h_back_porch);
	/* reg.0x36 v-back-porch */
	sn65dsi86_reg_write(dsi2edp, SN65DSI86_CHA_VERTICAL_BACK_PORCH,
			dsi->dc->mode.v_back_porch & 0xff);
	if ((dsi->dc->mode.v_back_porch < 1)
		|| (dsi->dc->mode.v_back_porch >> 8))
		pr_warn("SN65DSI86: V-Back-Porch %d is out of range\n",
			dsi->dc->mode.v_back_porch);
	/* reg.0x38 h-front-porch */
	sn65dsi86_reg_write(dsi2edp, SN65DSI86_CHA_HORIZONTAL_FRONT_PORCH,
			dsi->dc->mode.h_front_porch & 0xff);
	if (dsi->dc->mode.h_front_porch >> 8)
		pr_warn("SN65DSI86: H-Front-Porch %d is out of range\n",
			dsi->dc->mode.h_front_porch);
	/* reg.0x3a v-front-porch */
	sn65dsi86_reg_write(dsi2edp, SN65DSI86_CHA_VERTICAL_FRONT_PORCH,
			dsi->dc->mode.v_front_porch & 0xff);
	if ((dsi->dc->mode.v_front_porch < 1)
		|| (dsi->dc->mode.v_front_porch >> 8))
		pr_warn("SN65DSI86: V-Front-Porch %d is out of range\n",
			dsi->dc->mode.v_front_porch);

	/* reg.0x5b DP-18BPP Enable */
	sn65dsi86_reg_write(dsi2edp, SN65DSI86_DP_18BPP_EN, 0x00);
	/* reg.0x3c COLOR BAR */
	sn65dsi86_reg_write(dsi2edp, SN65DSI86_COLOR_BAR_CFG,
		dsi2edp->init.enable_colorbar ? (1 << 4) : 0);
	/* reg.0x5a enable Vstream */
	sn65dsi86_reg_write(dsi2edp, SN65DSI86_FRAMING_CFG,
		(1 << 3) | (1 << 2));

	dsi2edp->dsi2edp_enabled = true;
	mutex_unlock(&dsi2edp->lock);
}

static void sn65dsi86_dsi2edp_disable(struct tegra_dc_dsi_data *dsi)
{
	struct tegra_dc_dsi2edp_data *dsi2edp = tegra_dsi_get_outdata(dsi);

	mutex_lock(&dsi2edp->lock);
	/* enhanced framing and Vstream disable */
	sn65dsi86_reg_write(dsi2edp, SN65DSI86_FRAMING_CFG, 0x04);
	dsi2edp->dsi2edp_enabled = false;
	mutex_unlock(&dsi2edp->lock);

}

#ifdef CONFIG_PM
static void sn65dsi86_dsi2edp_suspend(struct tegra_dc_dsi_data *dsi)
{
	struct tegra_dc_dsi2edp_data *dsi2edp = tegra_dsi_get_outdata(dsi);

	mutex_lock(&dsi2edp->lock);
	/* configure GPIO1 for suspend */
	sn65dsi86_reg_write(dsi2edp, SN65DSI86_GPIO_CTRL_CFG, 0x02);
	dsi2edp->dsi2edp_enabled = false;
	mutex_unlock(&dsi2edp->lock);

}

static void sn65dsi86_dsi2edp_resume(struct tegra_dc_dsi_data *dsi)
{
	struct tegra_dc_dsi2edp_data *dsi2edp = tegra_dsi_get_outdata(dsi);

	mutex_lock(&dsi2edp->lock);
	/* disable configure GPIO1 for suspend */
	sn65dsi86_reg_write(dsi2edp, SN65DSI86_GPIO_CTRL_CFG, 0x00);
	/* enhanced framing and Vstream enable */
	sn65dsi86_reg_write(dsi2edp, SN65DSI86_FRAMING_CFG, 0x0c);
	dsi2edp->dsi2edp_enabled = true;
	mutex_unlock(&dsi2edp->lock);
}
#endif

struct tegra_dsi_out_ops tegra_dsi2edp_ops = {
	.init = sn65dsi86_dsi2edp_init,
	.destroy = sn65dsi86_dsi2edp_destroy,
	.enable = sn65dsi86_dsi2edp_enable,
	.disable = sn65dsi86_dsi2edp_disable,
#ifdef CONFIG_PM
	.suspend = sn65dsi86_dsi2edp_suspend,
	.resume = sn65dsi86_dsi2edp_resume,
#endif
};

static int of_dsi2edp_parse_platform_data(struct i2c_client *client)
{
	struct device_node *np = client->dev.of_node;
	struct tegra_dc_dsi2edp_data *dsi2edp = sn65dsi86_dsi2edp;
	enum of_gpio_flags flags;
	int err = 0;
	u32 temp;

	if (!np) {
		dev_err(&client->dev, "dsi2edp: device node not defined\n");
		err = -EINVAL;
		goto err;
	}

	dsi2edp->init.en_gpio = of_get_named_gpio_flags(np,
			"ti,enable-gpio", 0, &flags);
	dsi2edp->init.en_gpio_flags = flags;
	if (!dsi2edp->init.en_gpio)
		dev_err(&client->dev, "dsi2edp: gpio number not provided\n");

	if (!of_property_read_u32(np, "ti,pll-refclk-cfg", &temp))
		dsi2edp->init.pll_refclk_cfg = temp;
	else
		dsi2edp->init.pll_refclk_cfg = 0x02;

	if (!of_property_read_u32(np, "ti,disable-assr", &temp))
		dsi2edp->init.disable_assr = temp;
	else
		dsi2edp->init.disable_assr = 0;

	if (!of_property_read_u32(np, "ti,dp-ssc-cfg", &temp))
		dsi2edp->init.dp_ssc_cfg = temp;
	else
		dsi2edp->init.dp_ssc_cfg = 0x20;

	if (!of_property_read_u32(np, "ti,negative-hsync", &temp))
		dsi2edp->init.negative_hsync = temp;
	else
		dsi2edp->init.negative_hsync = 0;

	if (!of_property_read_u32(np, "ti,negative-vsync", &temp))
		dsi2edp->init.negative_vsync = temp;
	else
		dsi2edp->init.negative_vsync = 0;

	if (!of_property_read_u32(np, "ti,enable-colorbar", &temp))
		dsi2edp->init.enable_colorbar = temp;
	else
		dsi2edp->init.enable_colorbar = 0;

	/* parameters below this will be obsolete */

	if (!of_property_read_u32(np, "ti,h-pulse-width-high", &temp))
		if (temp & (1 << 7))
			dsi2edp->init.negative_hsync = 1;

	if (!of_property_read_u32(np, "ti,v-pulse-width-high", &temp))
		if (temp & (1 << 7))
			dsi2edp->init.negative_vsync = 1;

err:
	return err;
}

static int sn65dsi86_i2c_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct device_node *np = client->dev.of_node;
	struct device_node *pri_pn = NULL;
	struct device_node *sec_pn = NULL;
	bool pri_bridge = 0;
	bool sec_bridge = 0;
	int err = 0;

	sn65dsi86_i2c_client = client;
	sn65dsi86_dsi2edp = devm_kzalloc(&client->dev,
					sizeof(*sn65dsi86_dsi2edp),
					GFP_KERNEL);
	if (!sn65dsi86_dsi2edp)
		return -ENOMEM;

	memset(sn65dsi86_dsi2edp, 0, sizeof(struct tegra_dc_dsi2edp_data));
	mutex_init(&sn65dsi86_dsi2edp->lock);
	sn65dsi86_dsi2edp->client_i2c = client;

	sn65dsi86_dsi2edp->regmap = devm_regmap_init_i2c(client,
						&sn65dsi86_regmap_config);
	if (IS_ERR(sn65dsi86_dsi2edp->regmap)) {
		err = PTR_ERR(sn65dsi86_dsi2edp->regmap);
		dev_err(&client->dev,
				"SN65DSI86: regmap init failed\n");
		return err;
	}

	err = of_dsi2edp_parse_platform_data(client);
	if (err)
		return err;

	if (np) {
		/* TODO. We don't want probe itself for
		 * panels which don't use bridge.
		 * Until this bridge device is registered as a
		 * sub device of /host1x/dsi/panel in device tree,
		 * do no operation in probe in case bridge is not used.
		 * The reason to prepare this step to check with
		 * dsi2edp-bridge property is to consider
		 * the case that probe contains any actual operation.
		 */
		pri_pn =
			tegra_primary_panel_get_dt_node(NULL);
		sec_pn =
			tegra_secondary_panel_get_dt_node(NULL);
		if (pri_pn) {
			pri_bridge =
				of_property_read_bool(pri_pn,
				"nvidia,dsi-edp-bridge");
			of_node_put(pri_pn);
		}
		if (sec_pn) {
			sec_bridge = of_property_read_bool(sec_pn,
				"nvidia,dsi-edp-bridge");
			of_node_put(sec_pn);
		};

		if (!pri_bridge && !sec_bridge)
			return 0;
	}

	return 0;
}

static int sn65dsi86_i2c_remove(struct i2c_client *client)
{
	sn65dsi86_i2c_client = NULL;

	return 0;
}

static const struct i2c_device_id sn65dsi86_id_table[] = {
	{"sn65dsi86_dsi2edp", 0},
	{},
};

static const struct of_device_id sn65dsi86_dt_match[] = {
	{ .compatible = "ti,sn65dsi86" },
	{ }
};

static struct i2c_driver sn65dsi86_i2c_drv = {
	.driver = {
		.name = "sn65dsi86_dsi2edp",
		.of_match_table = of_match_ptr(sn65dsi86_dt_match),
		.owner = THIS_MODULE,
	},
	.probe = sn65dsi86_i2c_probe,
	.remove = sn65dsi86_i2c_remove,
	.id_table = sn65dsi86_id_table,
};

static int __init sn65dsi86_i2c_client_init(void)
{
	int err = 0;

	err = i2c_add_driver(&sn65dsi86_i2c_drv);
	if (err)
		pr_err("SN65DSI86: Failed to add i2c client driver\n");

	return err;
}

static void __exit sn65dsi86_i2c_client_exit(void)
{
	i2c_del_driver(&sn65dsi86_i2c_drv);
}

subsys_initcall(sn65dsi86_i2c_client_init);
module_exit(sn65dsi86_i2c_client_exit);

MODULE_AUTHOR("Bibek Basu <bbasu@nvidia.com>");
MODULE_DESCRIPTION(" TI SN65DSI86 dsi bridge to edp");
MODULE_LICENSE("GPL");
