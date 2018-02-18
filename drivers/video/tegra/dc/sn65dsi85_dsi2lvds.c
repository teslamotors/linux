/*
 * drivers/video/tegra/dc/sn65dsi85_dsi2lvds.c
 *
 * Copyright (c) 2013-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * Author:
 *	Tow Wang <toww@nvidia.com>
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
#include <linux/uaccess.h>	/*copy_from_user*/
#include <linux/debugfs.h>
#include <mach/dc.h>
#include "dc_priv.h"
#include "sn65dsi85_dsi2lvds.h"
#include "dsi.h"

/* TODO: support multiple instances */
static struct tegra_dc_dsi2lvds_data *sn65dsi85_dsi2lvds;
static struct i2c_client *sn65dsi85_i2c_client;

enum i2c_transfer_type {
	I2C_WRITE,
	I2C_READ,
};

static inline int sn65dsi85_reg_write(struct tegra_dc_dsi2lvds_data *dsi2lvds,
					unsigned int addr, unsigned int val)
{
	return regmap_write(dsi2lvds->regmap, addr, val);
}

static inline int sn65dsi85_reg_read(struct tegra_dc_dsi2lvds_data *dsi2lvds,
					unsigned int addr, unsigned int *val)
{
	return regmap_read(dsi2lvds->regmap, addr, val);
}

static const struct regmap_config sn65dsi85_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
};

static void sn65dsi85_dsi2lvds_en_gpio(
	struct tegra_dc_dsi2lvds_data *dsi2lvds,
	bool enable)
{
	if (!gpio_is_valid(dsi2lvds->en_gpio))
		return;

	if (enable) {
		gpio_direction_output(dsi2lvds->en_gpio,
			!(dsi2lvds->en_gpio_flags & OF_GPIO_ACTIVE_LOW));

		/* Wait >1ms for internal regulator to stabilize */
		usleep_range(2000, 3000);
	} else
		gpio_direction_output(dsi2lvds->en_gpio,
			dsi2lvds->en_gpio_flags & OF_GPIO_ACTIVE_LOW);

}

static int sn65dsi85_dsi2lvds_init(struct tegra_dc_dsi_data *dsi)
{
	int err = 0;
	struct tegra_dc_dsi2lvds_data *dsi2lvds = sn65dsi85_dsi2lvds;

	if (!sn65dsi85_dsi2lvds)
		return -ENODEV;

	dsi2lvds->dsi = dsi;
	tegra_dsi_set_outdata(dsi, dsi2lvds);

	if (gpio_is_valid(dsi2lvds->en_gpio)) {
		/* Request and set direction, but keep
		   disabled until enable() */
		err = devm_gpio_request_one(&sn65dsi85_i2c_client->dev,
			dsi2lvds->en_gpio,
			dsi2lvds->en_gpio_flags & OF_GPIO_ACTIVE_LOW,
			"dsi2lvds");
		if (err)
			pr_err("err %d: dsi2lvds GPIO request failed\n", err);
	} else
		pr_err("err %d: dsi2lvds GPIO is invalid\n", err);

	return 0;
}

static void sn65dsi85_dsi2lvds_destroy(struct tegra_dc_dsi_data *dsi)
{
	struct tegra_dc_dsi2lvds_data *dsi2lvds =
				tegra_dsi_get_outdata(dsi);

	if (!dsi2lvds)
		return;

	sn65dsi85_dsi2lvds_en_gpio(dsi2lvds, false);
	devm_gpio_free(&sn65dsi85_i2c_client->dev, dsi2lvds->en_gpio);
	devm_kfree(&sn65dsi85_i2c_client->dev, sn65dsi85_dsi2lvds);
	sn65dsi85_dsi2lvds = NULL;
	mutex_destroy(&dsi2lvds->lock);
}

/* Excerpt from SN65DSI85 spec, section 9.3:
   * init seq2: Assert the EN terminal
   * init seq3: Wait for 1 ms
   * init seq4: Initialize all CSR registers

   Before this function is called, DSI lanes must be in LP11.
   Enable DSI video after this function is called.
 */
static void sn65dsi85_dsi2lvds_enable(struct tegra_dc_dsi_data *dsi)
{
	struct tegra_dc_dsi2lvds_data *dsi2lvds = tegra_dsi_get_outdata(dsi);

	if (dsi2lvds && dsi2lvds->dsi2lvds_enabled)
		return;

	mutex_lock(&dsi2lvds->lock);
	sn65dsi85_dsi2lvds_en_gpio(dsi2lvds, true);
	sn65dsi85_reg_write(dsi2lvds, SN65DSI85_PLL_REFCLK_CFG,
			dsi2lvds->pll_refclk_cfg);
	sn65dsi85_reg_write(dsi2lvds, SN65DSI85_DIVIDER_MULTIPLIER, 0x10);
		/* LVDS clk = DSI HS clk / 3 */
	sn65dsi85_reg_write(dsi2lvds, SN65DSI85_DSI_CFG1,
			dsi2lvds->dsi_cfg1);

	sn65dsi85_reg_write(dsi2lvds, SN65DSI85_DSI_CHA_CLK_RANGE,
			dsi2lvds->dsi_cha_clk_range);
	sn65dsi85_reg_write(dsi2lvds, SN65DSI85_DSI_CHB_CLK_RANGE,
			dsi2lvds->dsi_chb_clk_range);
	sn65dsi85_reg_write(dsi2lvds, SN65DSI85_LVDS_FORMAT, dsi2lvds->lvds_format);
	/* CHA_ACTIVE_LINE_LENGTH */
	sn65dsi85_reg_write(dsi2lvds, SN65DSI85_VIDEO_CHA_LINE_LOW,
			dsi2lvds->video_cha_line_low);
	sn65dsi85_reg_write(dsi2lvds, SN65DSI85_VIDEO_CHA_LINE_HIGH,
			dsi2lvds->video_cha_line_high);
	/* CHB_ACTIVE_LINE_LENGTH */
	if (dsi2lvds->video_chb_line_low >= 0)
		sn65dsi85_reg_write(dsi2lvds, SN65DSI85_VIDEO_CHB_LINE_LOW,
				dsi2lvds->video_chb_line_low);
	if (dsi2lvds->video_chb_line_high)
		sn65dsi85_reg_write(dsi2lvds, SN65DSI85_VIDEO_CHB_LINE_HIGH,
				dsi2lvds->video_chb_line_high);
	/* CHA_VERTICAL_DISPLAY_SIZE */
	sn65dsi85_reg_write(dsi2lvds, SN65DSI85_CHA_VERT_DISP_SIZE_LOW,
			dsi2lvds->cha_vert_disp_size_low);
	sn65dsi85_reg_write(dsi2lvds, SN65DSI85_CHA_VERT_DISP_SIZE_HIGH,
			dsi2lvds->cha_vert_disp_size_high);
	sn65dsi85_reg_write(dsi2lvds, SN65DSI85_CHA_SYNC_DELAY_LOW, 0x20);
	sn65dsi85_reg_write(dsi2lvds, SN65DSI85_CHA_SYNC_DELAY_HIGH, 0x00);
	/* CHA_HSYNC_PULSE_WIDTH */
	sn65dsi85_reg_write(dsi2lvds, SN65DSI85_CHA_HSYNC_PULSE_WIDTH_LOW,
			dsi2lvds->h_pulse_width_low);
	sn65dsi85_reg_write(dsi2lvds, SN65DSI85_CHA_HSYNC_PULSE_WIDTH_HIGH,
			dsi2lvds->h_pulse_width_high);
	/* CHA_VSYNC_PULSE_WIDTH */
	sn65dsi85_reg_write(dsi2lvds, SN65DSI85_CHA_VSYNC_PULSE_WIDTH_LOW,
			dsi2lvds->v_pulse_width_low);
	sn65dsi85_reg_write(dsi2lvds, SN65DSI85_CHA_VSYNC_PULSE_WIDTH_HIGH,
			dsi2lvds->v_pulse_width_high);
	/* CHA_HORIZONTAL_BACK_PORCH */
	sn65dsi85_reg_write(dsi2lvds, SN65DSI85_CHA_HORIZONTAL_BACK_PORCH,
			dsi2lvds->h_back_porch);
	/* CHA_VERTICAL_BACK_PORCH */
	sn65dsi85_reg_write(dsi2lvds, SN65DSI85_CHA_VERTICAL_BACK_PORCH,
			dsi2lvds->v_back_porch);
	/* CHA_HORIZONTAL_FRONT_PORCH */
	sn65dsi85_reg_write(dsi2lvds, SN65DSI85_CHA_HORIZONTAL_FRONT_PORCH,
			dsi2lvds->h_front_porch);
	/* CHA_VERTICAL_FRONT_PORCH */
	sn65dsi85_reg_write(dsi2lvds, SN65DSI85_CHA_VERTICAL_FRONT_PORCH,
			dsi2lvds->v_front_porch);
	/* COLOR BAR for channel A only */
	/* sn65dsi85_reg_write(dsi2lvds, SN65DSI85_COLOR_BAR_CFG, 0x10);*/
	dsi2lvds->dsi2lvds_enabled = true;
	mutex_unlock(&dsi2lvds->lock);
}

/* Excerpt from SN65DSI85 spec, section 9.3:
   * init seq6: Set the PLL_EN bit
   * init seq7: Wait >3ms
   * init seq8: Set the SOFT_RESET bit

   enable() must be called and DSI video must be enabled before this
   function is called.
 */
static void sn65dsi85_dsi2lvds_postpoweron(struct tegra_dc_dsi_data *dsi)
{
	struct tegra_dc_dsi2lvds_data *dsi2lvds = tegra_dsi_get_outdata(dsi);
	unsigned val = 0;
	unsigned retry = 0;

	if (NULL == dsi2lvds)
		return;

	WARN_ON(!dsi2lvds->dsi2lvds_enabled);

	mutex_lock(&dsi2lvds->lock);
	/* Enable PLL after all CSR registers are programmed */
	do {
		/* PLL ENABLE */
		sn65dsi85_reg_write(dsi2lvds, SN65DSI85_PLL_EN, 0x01);
		usleep_range(10000, 12000);
		/* PLL_EN_STAT */
		sn65dsi85_reg_read(dsi2lvds, SN65DSI85_PLL_REFCLK_CFG, &val);
	} while (((val & 0x80) == 0) && (retry++ < RETRYLOOP));
	if ((val & 0xFF) >> 7 == 0)
		pr_err("sn65dsi85_dsi2lvds: PLL not locked\n");
	usleep_range(10000, 12000);

	/* Must soft reset after CSR registers are updated */
	sn65dsi85_reg_write(dsi2lvds, SN65DSI85_SOFT_RESET, 0x01);
	usleep_range(10000, 12000);

	mutex_unlock(&dsi2lvds->lock);
}

static void sn65dsi85_dsi2lvds_disable(struct tegra_dc_dsi_data *dsi)
{
	struct tegra_dc_dsi2lvds_data *dsi2lvds = tegra_dsi_get_outdata(dsi);

	mutex_lock(&dsi2lvds->lock);
	/* disable internal PLL */
	sn65dsi85_reg_write(dsi2lvds, SN65DSI85_PLL_EN, 0);
	sn65dsi85_dsi2lvds_en_gpio(dsi2lvds, false);
	dsi2lvds->dsi2lvds_enabled = false;
	mutex_unlock(&dsi2lvds->lock);
}

#ifdef CONFIG_PM
static void sn65dsi85_dsi2lvds_suspend(struct tegra_dc_dsi_data *dsi)
{
	struct tegra_dc_dsi2lvds_data *dsi2lvds = tegra_dsi_get_outdata(dsi);

	mutex_lock(&dsi2lvds->lock);
	sn65dsi85_reg_write(dsi2lvds, SN65DSI85_PLL_EN, 0);
	mutex_unlock(&dsi2lvds->lock);

}

static void sn65dsi85_dsi2lvds_resume(struct tegra_dc_dsi_data *dsi)
{
	struct tegra_dc_dsi2lvds_data *dsi2lvds = tegra_dsi_get_outdata(dsi);

	mutex_lock(&dsi2lvds->lock);
	sn65dsi85_dsi2lvds_postpoweron(dsi2lvds->dsi);
	mutex_unlock(&dsi2lvds->lock);
}
#endif

struct tegra_dsi_out_ops tegra_dsi2lvds_ops = {
	.init = sn65dsi85_dsi2lvds_init,
	.destroy = sn65dsi85_dsi2lvds_destroy,
	.enable = sn65dsi85_dsi2lvds_enable,
	.postpoweron = sn65dsi85_dsi2lvds_postpoweron,
	.disable = sn65dsi85_dsi2lvds_disable,
#ifdef CONFIG_PM
	.suspend = sn65dsi85_dsi2lvds_suspend,
	.resume = sn65dsi85_dsi2lvds_resume,
#endif
};

#ifdef CONFIG_DEBUG_FS
static int sn65dsi85_dsi2lvds_regs_show(struct seq_file *s, void *unused)
{
	struct tegra_dc_dsi2lvds_data *dsi2lvds = s->private;
	unsigned int val;

	mutex_lock(&dsi2lvds->lock);

	sn65dsi85_reg_read(dsi2lvds, SN65DSI85_PLL_REFCLK_CFG, &val);
	seq_printf(s, "%40s%#6x%#6x\n", "SN65DSI85_PLL_REFCLK_CFG",
			SN65DSI85_PLL_REFCLK_CFG, val);
	sn65dsi85_reg_read(dsi2lvds, SN65DSI85_DSI_CFG1, &val);
	seq_printf(s, "%40s%#6x%#6x\n", "SN65DSI85_DSI_CFG1",
			SN65DSI85_DSI_CFG1, val);
	sn65dsi85_reg_read(dsi2lvds, SN65DSI85_DSI_CHA_CLK_RANGE, &val);
	seq_printf(s, "%40s%#6x%#6x\n", "SN65DSI85_DSI_CHA_CLK_RANGE",
			SN65DSI85_DSI_CHA_CLK_RANGE, val);
	sn65dsi85_reg_read(dsi2lvds, SN65DSI85_DSI_CHB_CLK_RANGE, &val);
	seq_printf(s, "%40s%#6x%#6x\n", "SN65DSI85_DSI_CHB_CLK_RANGE",
			SN65DSI85_DSI_CHB_CLK_RANGE, val);
	sn65dsi85_reg_read(dsi2lvds, SN65DSI85_VIDEO_CHA_LINE_LOW, &val);
	seq_printf(s, "%40s%#6x%#6x\n", "SN65DSI85_VIDEO_CHA_LINE_LOW",
			SN65DSI85_VIDEO_CHA_LINE_LOW, val);
	sn65dsi85_reg_read(dsi2lvds, SN65DSI85_VIDEO_CHA_LINE_HIGH, &val);
	seq_printf(s, "%40s%#6x%#6x\n", "SN65DSI85_VIDEO_CHA_LINE_HIGH",
			SN65DSI85_VIDEO_CHA_LINE_HIGH, val);
	sn65dsi85_reg_read(dsi2lvds, SN65DSI85_VIDEO_CHB_LINE_LOW, &val);
	seq_printf(s, "%40s%#6x%#6x\n", "SN65DSI85_VIDEO_CHB_LINE_LOW",
			SN65DSI85_VIDEO_CHB_LINE_LOW, val);
	sn65dsi85_reg_read(dsi2lvds, SN65DSI85_VIDEO_CHB_LINE_HIGH, &val);
	seq_printf(s, "%40s%#6x%#6x\n", "SN65DSI85_VIDEO_CHB_LINE_HIGH",
			SN65DSI85_VIDEO_CHB_LINE_HIGH, val);
	sn65dsi85_reg_read(dsi2lvds, SN65DSI85_CHA_VERT_DISP_SIZE_LOW, &val);
	seq_printf(s, "%40s%#6x%#6x\n", "SN65DSI85_CHA_VERT_DISP_SIZE_LOW",
			SN65DSI85_CHA_VERT_DISP_SIZE_LOW, val);
	sn65dsi85_reg_read(dsi2lvds, SN65DSI85_CHA_VERT_DISP_SIZE_HIGH, &val);
	seq_printf(s, "%40s%#6x%#6x\n", "SN65DSI85_CHA_VERT_DISP_SIZE_HIGH",
			SN65DSI85_CHA_VERT_DISP_SIZE_HIGH, val);
	sn65dsi85_reg_read(dsi2lvds, SN65DSI85_CHA_HSYNC_PULSE_WIDTH_LOW, &val);
	seq_printf(s, "%40s%#6x%#6x\n", "SN65DSI85_CHA_HSYNC_PULSE_WIDTH_LOW",
			SN65DSI85_CHA_HSYNC_PULSE_WIDTH_LOW, val);
	sn65dsi85_reg_read(dsi2lvds, SN65DSI85_CHA_HSYNC_PULSE_WIDTH_HIGH, &val);
	seq_printf(s, "%40s%#6x%#6x\n", "SN65DSI85_CHA_HSYNC_PULSE_WIDTH_HIGH",
			SN65DSI85_CHA_HSYNC_PULSE_WIDTH_HIGH, val);
	sn65dsi85_reg_read(dsi2lvds, SN65DSI85_CHA_VSYNC_PULSE_WIDTH_LOW, &val);
	seq_printf(s, "%40s%#6x%#6x\n", "SN65DSI85_CHA_VSYNC_PULSE_WIDTH_LOW",
			SN65DSI85_CHA_VSYNC_PULSE_WIDTH_LOW, val);
	sn65dsi85_reg_read(dsi2lvds, SN65DSI85_CHA_VSYNC_PULSE_WIDTH_HIGH, &val);
	seq_printf(s, "%40s%#6x%#6x\n", "SN65DSI85_CHA_VSYNC_PULSE_WIDTH_HIGH",
			SN65DSI85_CHA_VSYNC_PULSE_WIDTH_HIGH, val);
	sn65dsi85_reg_read(dsi2lvds, SN65DSI85_CHA_HORIZONTAL_BACK_PORCH, &val);
	seq_printf(s, "%40s%#6x%#6x\n", "SN65DSI85_CHA_HORIZONTAL_BACK_PORCH",
			SN65DSI85_CHA_HORIZONTAL_BACK_PORCH, val);
	sn65dsi85_reg_read(dsi2lvds, SN65DSI85_CHA_VERTICAL_BACK_PORCH, &val);
	seq_printf(s, "%40s%#6x%#6x\n", "SN65DSI85_CHA_VERTICAL_BACK_PORCH",
			SN65DSI85_CHA_VERTICAL_BACK_PORCH, val);
	sn65dsi85_reg_read(dsi2lvds, SN65DSI85_CHA_HORIZONTAL_FRONT_PORCH, &val);
	seq_printf(s, "%40s%#6x%#6x\n", "SN65DSI85_CHA_HORIZONTAL_FRONT_PORCH",
			SN65DSI85_CHA_HORIZONTAL_FRONT_PORCH, val);
	sn65dsi85_reg_read(dsi2lvds, SN65DSI85_CHA_VERTICAL_FRONT_PORCH, &val);
	seq_printf(s, "%40s%#6x%#6x\n", "SN65DSI85_CHA_VERTICAL_FRONT_PORCH",
			SN65DSI85_CHA_VERTICAL_FRONT_PORCH, val);
	sn65dsi85_reg_read(dsi2lvds, SN65DSI85_COLOR_BAR_CFG, &val);
	seq_printf(s, "%40s%#6x%#6x\n", "SN65DSI85_COLOR_BAR_CFG",
			SN65DSI85_COLOR_BAR_CFG, val);
	sn65dsi85_reg_read(dsi2lvds, SN65DSI85_CHA_IRQ_STATUS, &val);
	seq_printf(s, "%40s%#6x%#6x\n", "SN65DSI85_CHA_IRQ_STATUS",
			SN65DSI85_CHA_IRQ_STATUS, val);
	sn65dsi85_reg_read(dsi2lvds, SN65DSI85_CHB_IRQ_STATUS, &val);
	seq_printf(s, "%40s%#6x%#6x\n", "SN65DSI85_CHB_IRQ_STATUS",
			SN65DSI85_CHB_IRQ_STATUS, val);
	mutex_unlock(&dsi2lvds->lock);
	return 0;
}


static int sn65dsi85_dsi2lvds_regs_open(struct inode *inode, struct file *file)
{
	return single_open(file, sn65dsi85_dsi2lvds_regs_show, inode->i_private);
}

static const struct file_operations regs_fops = {
	.open		= sn65dsi85_dsi2lvds_regs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static void sn65dsi85_dsi2lvds_panel_remove_debugfs(
		struct tegra_dc_dsi2lvds_data *dsi2lvds)
{
	/* debugfs_remove_recursive(NULL) is safe */
	debugfs_remove_recursive(dsi2lvds->debugdir);
	dsi2lvds->debugdir = NULL;
}

static int sn65dsi85_dsi2lvds_panel_create_debugfs(
		struct tegra_dc_dsi2lvds_data *dsi2lvds)
{
	struct dentry *pEntry;
	int	ret = 0;

	dsi2lvds->debugdir = debugfs_create_dir("sn65dsi85_dsi2lvds", NULL);
	pr_info("%s: dsi2lvds->debugdir = %p\n", __func__, dsi2lvds->debugdir);
	if (!dsi2lvds->debugdir) {
		ret = ENOTDIR;
		goto err;
	}

	pEntry = debugfs_create_file("regs", S_IRUGO | S_IWUSR,
		dsi2lvds->debugdir, dsi2lvds, &regs_fops);
	pr_info("%s: debugfs_create_file returned %p\n", __func__, pEntry);
	if (NULL == pEntry) {
		ret = ENOENT;
		goto err;
	}
	return ret;

err:
	sn65dsi85_dsi2lvds_panel_remove_debugfs(dsi2lvds);
	dev_err(&dsi2lvds->client_i2c->dev, "Failed to create debugfs\n");
	return ret;
}
#else
static void sn65dsi85_dsi2lvds_panel_create_debugfs(
		struct tegra_dc_dsi2lvds_data *dsi2lvds) { }
static void sn65dsi85_dsi2lvds_panel_remove_debugfs(
		struct tegra_dc_dsi2lvds_data *dsi2lvds) { }
#endif

static int of_dsi2lvds_parse_platform_data(struct i2c_client *client)
{
	struct device_node *np = client->dev.of_node;
	struct tegra_dc_dsi2lvds_data *dsi2lvds = sn65dsi85_dsi2lvds;
	enum of_gpio_flags flags;
	int err = 0;
	u32 temp;

	if (!np) {
		dev_err(&client->dev, "dsi2lvds: device node not defined\n");
		err = -EINVAL;
		goto err;
	}

	dsi2lvds->en_gpio = of_get_named_gpio_flags(np,
			"ti,enable-gpio", 0, &flags);
	dsi2lvds->en_gpio_flags = flags;
	if (dsi2lvds->en_gpio < 0)
		dev_err(&client->dev, "dsi2lvds: gpio number not provided\n");
	else if (!gpio_is_valid(dsi2lvds->en_gpio))
		dev_err(&client->dev, "dsi2lvds: en gpio is invalid\n");

	if (!of_property_read_u32(np, "ti,pll-refclk-cfg", &temp))
		dsi2lvds->pll_refclk_cfg = temp;
	else
		dsi2lvds->pll_refclk_cfg = 0x0B;
			/* 137.5 < LVDS CLK < 154 MHz, HS_CLK_SRC=D-PHY */

	if (!of_property_read_u32(np, "ti,dsi-cfg1", &temp))
		dsi2lvds->dsi_cfg1 = temp;
	else
		dsi2lvds->dsi_cfg1 = 0x26;	/* Single 4 DSI lanes */

	if (!of_property_read_u32(np, "ti,dsi-cha-clk-range", &temp))
		dsi2lvds->dsi_cha_clk_range = temp;
	else
		dsi2lvds->dsi_cha_clk_range = 0x59;		/* 445.5 / 5 = 89.1 */

	if (!of_property_read_u32(np, "ti,dsi-chb-clk-range", &temp))
		dsi2lvds->dsi_chb_clk_range = temp;
	else
		dsi2lvds->dsi_chb_clk_range = 0x59;
			/* 445.5 / 5 = 89.1, do not use reserved values */

	if (!of_property_read_u32(np, "ti,lvds-format", &temp))
		dsi2lvds->lvds_format = temp;
	else
		dsi2lvds->lvds_format = 0x7F;
			/*HS and VS neg. pol., only ch. A, enable lane 4 on both ch.*/
	/*
	 * These registers are left at their reset values
	 * #define  SN65DSI85_LVDS_VOLTAGE			0x19
	 * #define  SN65DSI85_LVDS_TERMINAL		0x1A
	 */

	if (!of_property_read_u32(np, "ti,video-cha-line-low", &temp))
		dsi2lvds->video_cha_line_low = temp;
	else
		dsi2lvds->video_cha_line_low = 0x80;

	if (!of_property_read_u32(np, "ti,video-cha-line-high", &temp))
		dsi2lvds->video_cha_line_high = temp;
	else
		dsi2lvds->video_cha_line_high = 0x07;

	if (!of_property_read_u32(np, "ti,video-chb-line-low", &temp))
		dsi2lvds->video_chb_line_low = temp;
	else
		dsi2lvds->video_chb_line_low = 0x00;

	if (!of_property_read_u32(np, "ti,video-chb-line-high", &temp))
		dsi2lvds->video_chb_line_high = temp;
	else
		dsi2lvds->video_chb_line_high = 0x00;

	if (!of_property_read_u32(np, "ti,cha-vert-disp-size-low", &temp))
		dsi2lvds->cha_vert_disp_size_low = temp;
	else
		dsi2lvds->cha_vert_disp_size_low = 0x38;

	if (!of_property_read_u32(np, "ti,cha-vert-disp-size-high", &temp))
		dsi2lvds->cha_vert_disp_size_high = temp;
	else
		dsi2lvds->cha_vert_disp_size_high = 0x04;

	if (!of_property_read_u32(np, "ti,h-pulse-width-low", &temp))
		dsi2lvds->h_pulse_width_low = temp;
	else
		dsi2lvds->h_pulse_width_low = 0x10;

	if (!of_property_read_u32(np, "ti,h-pulse-width-high", &temp))
		dsi2lvds->h_pulse_width_high = temp;
	else
		dsi2lvds->h_pulse_width_high = 0x00;	/* Set bits 3:0 only */

	if (!of_property_read_u32(np, "ti,v-pulse-width-low", &temp))
		dsi2lvds->v_pulse_width_low = temp;
	else
		dsi2lvds->v_pulse_width_low = 0x0e;

	if (!of_property_read_u32(np, "ti,v-pulse-width-high", &temp))
		dsi2lvds->v_pulse_width_high = temp;
	else
		dsi2lvds->v_pulse_width_high = 0x00;	/* Set bits 3:0 only */

	if (!of_property_read_u32(np, "ti,h-back-porch", &temp))
		dsi2lvds->h_back_porch = temp;
	else
		dsi2lvds->h_back_porch = 0x98;

	if (!of_property_read_u32(np, "ti,v-back-porch", &temp))
		dsi2lvds->v_back_porch = temp;
	else
		dsi2lvds->v_back_porch = 0x13;

	if (!of_property_read_u32(np, "ti,h-front-porch", &temp))
		dsi2lvds->h_front_porch = temp;
	else
		dsi2lvds->h_front_porch = 0x10;

	if (!of_property_read_u32(np, "ti,v-front-porch", &temp))
		dsi2lvds->v_front_porch = temp;
	else
		dsi2lvds->v_front_porch = 0x03;

err:
	return err;
}

static int sn65dsi85_i2c_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	int err = 0;

	sn65dsi85_i2c_client = client;
	sn65dsi85_dsi2lvds = devm_kzalloc(&client->dev,
					sizeof(*sn65dsi85_dsi2lvds),
					GFP_KERNEL);
	if (NULL == sn65dsi85_dsi2lvds)
		return -ENOMEM;

	memset(sn65dsi85_dsi2lvds, 0, sizeof(struct tegra_dc_dsi2lvds_data));
	mutex_init(&sn65dsi85_dsi2lvds->lock);
	sn65dsi85_dsi2lvds->client_i2c = client;

	sn65dsi85_dsi2lvds->regmap = devm_regmap_init_i2c(client,
						&sn65dsi85_regmap_config);
	if (IS_ERR(sn65dsi85_dsi2lvds->regmap)) {
		err = PTR_ERR(sn65dsi85_dsi2lvds->regmap);
		dev_err(&client->dev,
				"sn65dsi85_dsi2lvds: regmap init failed\n");
		return err;
	}

	err = of_dsi2lvds_parse_platform_data(client);
	if (err)
		return err;

	sn65dsi85_dsi2lvds_panel_create_debugfs(sn65dsi85_dsi2lvds);

	pr_info("%s: returning with err=%d\n", __func__, err);
	return 0;
}

static int sn65dsi85_i2c_remove(struct i2c_client *client)
{
	sn65dsi85_dsi2lvds_panel_remove_debugfs(sn65dsi85_dsi2lvds);
	sn65dsi85_i2c_client = NULL;

	return 0;
}

static const struct i2c_device_id sn65dsi85_id_table[] = {
	{"sn65dsi85_dsi2lvds", 0},
	{},
};

static const struct of_device_id sn65dsi85_dt_match[] = {
	{ .compatible = "ti,sn65dsi85" },
	{ }
};

static struct i2c_driver sn65dsi85_i2c_drv = {
	.driver = {
		.name = "ti,sn65dsi85",
		.of_match_table = of_match_ptr(sn65dsi85_dt_match),
		.owner = THIS_MODULE,
	},
	.probe = sn65dsi85_i2c_probe,
	.remove = sn65dsi85_i2c_remove,
	.id_table = sn65dsi85_id_table,
};

static int __init sn65dsi85_i2c_client_init(void)
{
	int err = 0;

	err = i2c_add_driver(&sn65dsi85_i2c_drv);
	if (err)
		pr_err("sn65dsi85_dsi2lvds: Failed to add i2c client driver\n");

	return err;
}

static void __exit sn65dsi85_i2c_client_exit(void)
{
	i2c_del_driver(&sn65dsi85_i2c_drv);
}

subsys_initcall(sn65dsi85_i2c_client_init);
module_exit(sn65dsi85_i2c_client_exit);

MODULE_AUTHOR("Tow Wang <toww@nvidia.com>");
MODULE_DESCRIPTION(" TI SN65DSI85 dsi bridge to lvds");
MODULE_LICENSE("GPL");
