/*
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
 */

#include <linux/padctrl/padctrl.h>
#include <linux/tegra-pmc.h>
#include <linux/delay.h>
#include <linux/slab.h>

/* IO PAD group */
#define TEGRA_IO_PAD_GROUP_AUDIO	0
#define TEGRA_IO_PAD_GROUP_AUDIO_HV	1
#define TEGRA_IO_PAD_GROUP_CAM		2
#define TEGRA_IO_PAD_GROUP_CSIA		3
#define TEGRA_IO_PAD_GROUP_CSIB		4
#define TEGRA_IO_PAD_GROUP_CSIC		5
#define TEGRA_IO_PAD_GROUP_CSID		6
#define TEGRA_IO_PAD_GROUP_CSIE		7
#define TEGRA_IO_PAD_GROUP_CSIF		8
#define TEGRA_IO_PAD_GROUP_DBG		9
#define TEGRA_IO_PAD_GROUP_DEBUG_NONAO	10
#define TEGRA_IO_PAD_GROUP_DMIC		11
#define TEGRA_IO_PAD_GROUP_DP		12
#define TEGRA_IO_PAD_GROUP_DSI		13
#define TEGRA_IO_PAD_GROUP_DSIB		14
#define TEGRA_IO_PAD_GROUP_DSIC		15
#define TEGRA_IO_PAD_GROUP_DSID		16
#define TEGRA_IO_PAD_GROUP_EMMC		17
#define TEGRA_IO_PAD_GROUP_EMMC2	18
#define TEGRA_IO_PAD_GROUP_GPIO		19
#define TEGRA_IO_PAD_GROUP_HDMI		20
#define TEGRA_IO_PAD_GROUP_HSIC		21
#define TEGRA_IO_PAD_GROUP_LVDS		22
#define TEGRA_IO_PAD_GROUP_MIPI_BIAS	23
#define TEGRA_IO_PAD_GROUP_PEX_BIAS	24
#define TEGRA_IO_PAD_GROUP_PEX_CLK1	25
#define TEGRA_IO_PAD_GROUP_PEX_CLK2	26
#define TEGRA_IO_PAD_GROUP_PEX_CTRL	27
#define TEGRA_IO_PAD_GROUP_SDMMC1	28
#define TEGRA_IO_PAD_GROUP_SDMMC3	29
#define TEGRA_IO_PAD_GROUP_SPI		30
#define TEGRA_IO_PAD_GROUP_SPI_HV	31
#define TEGRA_IO_PAD_GROUP_UART		32
#define TEGRA_IO_PAD_GROUP_USB_BIAS	33
#define TEGRA_IO_PAD_GROUP_USB0		34
#define TEGRA_IO_PAD_GROUP_USB1		35
#define TEGRA_IO_PAD_GROUP_USB2		36
#define TEGRA_IO_PAD_GROUP_USB3		37
#define TEGRA_IO_PAD_GROUP_SYS		38
#define TEGRA_IO_PAD_GROUP_BB		39
#define TEGRA_IO_PAD_GROUP_HV		40


struct tegra210_pmc_padcontrl {
	struct padctrl_dev *pad_dev;
};

struct tegra210_pmc_pads_info {
	const char *pad_name;
	int pad_id;
	int io_dpd_bit_pos;
	int io_dpd_reg_off;
	int io_volt_bit_pos;
	int io_volt_reg_off;
	bool dynamic_pad_voltage;
};

#define TEGRA_210_PAD_INFO(_name, _id, _dpd_off, _dpd_bit, _vbit) \
{								\
	.pad_name = _name,					\
	.pad_id = TEGRA_IO_PAD_GROUP_##_id,			\
	.io_dpd_reg_off = _dpd_off,				\
	.io_dpd_bit_pos = _dpd_bit,				\
	.io_volt_bit_pos = _vbit,				\
	.io_volt_reg_off = 0,					\
}

static struct tegra210_pmc_pads_info tegra210_pads_info[] = {
	TEGRA_210_PAD_INFO("audio", AUDIO, 0, 17, 5),
	TEGRA_210_PAD_INFO("audio-hv", AUDIO_HV, 1, 29, 18),
	TEGRA_210_PAD_INFO("cam", CAM, 1, 4, 10),
	TEGRA_210_PAD_INFO("csia", CSIA, 0, 0, -1),
	TEGRA_210_PAD_INFO("csib", CSIB, 0, 1, -1),
	TEGRA_210_PAD_INFO("csic", CSIC, 1, 10, -1),
	TEGRA_210_PAD_INFO("csid", CSID, 1, 11, -1),
	TEGRA_210_PAD_INFO("csie", CSIE, 1, 12, -1),
	TEGRA_210_PAD_INFO("csif", CSIF, 1, 13, -1),
	TEGRA_210_PAD_INFO("dbg", DBG, 0, 25, 19),
	TEGRA_210_PAD_INFO("debug-nonao", DEBUG_NONAO, 0, 26, -1),
	TEGRA_210_PAD_INFO("dmic", DMIC, 1, 18, 20),
	TEGRA_210_PAD_INFO("dp", DP, 1, 19, -1),
	TEGRA_210_PAD_INFO("dsi", DSI, 0, 2, -1),
	TEGRA_210_PAD_INFO("dsib", DSIB, 1, 7, -1),
	TEGRA_210_PAD_INFO("dsic", DSIC, 1, 8, -1),
	TEGRA_210_PAD_INFO("dsid", DSID, 1, 9, -1),
	TEGRA_210_PAD_INFO("emmc", EMMC, 1, 3, -1),
	TEGRA_210_PAD_INFO("emmc2", EMMC2, 1, 5, -1),
	TEGRA_210_PAD_INFO("gpio", GPIO, 0, 27, 21),
	TEGRA_210_PAD_INFO("hdmi", HDMI, 0, 28, -1),
	TEGRA_210_PAD_INFO("hsic", HSIC, 0, 19, -1),
	TEGRA_210_PAD_INFO("lvds", LVDS, 1, 25, -1),
	TEGRA_210_PAD_INFO("mipi-bias", MIPI_BIAS, 0, 3, -1),
	TEGRA_210_PAD_INFO("pex-bias", PEX_BIAS, 0, 4, -1),
	TEGRA_210_PAD_INFO("pex-clk1", PEX_CLK1, 0, 5, -1),
	TEGRA_210_PAD_INFO("pex-clk2", PEX_CLK2, 0, 6, -1),
	TEGRA_210_PAD_INFO("pex-ctrl", PEX_CTRL, 1, 0, 11),
	TEGRA_210_PAD_INFO("sdmmc1", SDMMC1, 1, 1, 12),
	TEGRA_210_PAD_INFO("sdmmc3", SDMMC3, 1, 2, 13),
	TEGRA_210_PAD_INFO("spi", SPI, 1, 14, 22),
	TEGRA_210_PAD_INFO("spi-hv", SPI_HV, 1, 15, 23),
	TEGRA_210_PAD_INFO("uart", UART, 0, 14, 2),
	TEGRA_210_PAD_INFO("usb-bias", USB_BIAS, 0, 12, -1),
	TEGRA_210_PAD_INFO("usb0", USB0, 0, 9, -1),
	TEGRA_210_PAD_INFO("usb1", USB1, 0, 10, -1),
	TEGRA_210_PAD_INFO("usb2", USB2, 0, 11, -1),
	TEGRA_210_PAD_INFO("usb3", USB3, 0, 18, -1),
	TEGRA_210_PAD_INFO("sys", SYS, 0, -1, 0),
	TEGRA_210_PAD_INFO("bb", BB, 0, -1, 3),
	TEGRA_210_PAD_INFO("hv", HV, 0, -1, -1),
};

static int tegra210_pmc_padctrl_set_voltage(struct padctrl_dev *pad_dev,
		int pad_id, u32 voltage)
{
	u32 offset, curr_volt;
	unsigned int pad_mask;
	int val;
	int i;

	if ((voltage != 1800000) && (voltage != 3300000)) {
		pr_err("Pad voltage %u is not valid\n", voltage);
		return -EINVAL;
	}

	for (i = 0; i < ARRAY_SIZE(tegra210_pads_info); ++i) {
		if (tegra210_pads_info[i].pad_id == pad_id)
			break;
	}

	if (i == ARRAY_SIZE(tegra210_pads_info))
		return -EINVAL;

	if (tegra210_pads_info[i].io_volt_bit_pos < 0)
			return -EINVAL;;

	offset = BIT(tegra210_pads_info[i].io_volt_bit_pos);

	if (!tegra210_pads_info[i].dynamic_pad_voltage) {
		pad_mask = tegra_pmc_pwr_detect_get(offset);
		curr_volt = (pad_mask & offset) ? 3300000UL : 1800000UL;
		if (voltage == curr_volt)
			return 0;
		pr_err("Pad %s: Dynamic pad voltage is not supported\n",
			tegra210_pads_info[i].pad_name);

		return -EINVAL;
	}

	val = (voltage == 3300000) ? offset : 0;
	tegra_pmc_pwr_detect_update(offset, val);
	udelay(100);
	return 0;
}

static int tegra210_pmc_padctl_get_voltage(struct padctrl_dev *pad_dev,
		int pad_id, u32 *voltage)
{
	unsigned int pwrdet_mask;
	u32 offset;
	int i;

	for (i = 0; i < ARRAY_SIZE(tegra210_pads_info); ++i) {
		if (tegra210_pads_info[i].pad_id == pad_id)
			break;
	}

	if (i == ARRAY_SIZE(tegra210_pads_info))
		return -EINVAL;

	if (tegra210_pads_info[i].io_volt_bit_pos < 0)
			return -EINVAL;;

	offset = BIT(tegra210_pads_info[i].io_volt_bit_pos);
	pwrdet_mask = tegra_pmc_pwr_detect_get(offset);
	if (pwrdet_mask & offset)
		*voltage = 3300000UL;
	else
		*voltage = 1800000UL;

	return 0;
}

static int tegra210_pmc_padctrl_set_power(struct padctrl_dev *pad_dev,
		int pad_id, u32 enable)
{
	u32 bit, reg;
	int i;
	int ret;

	for (i = 0; i < ARRAY_SIZE(tegra210_pads_info); ++i) {
		if (tegra210_pads_info[i].pad_id == pad_id)
			break;
	}

	if (i == ARRAY_SIZE(tegra210_pads_info))
		return -EINVAL;

	if (tegra210_pads_info[i].io_dpd_bit_pos < 0)
		return -EINVAL;

	bit = tegra210_pads_info[i].io_dpd_bit_pos;
	reg = tegra210_pads_info[i].io_dpd_reg_off;
	if (enable)
		ret = tegra_pmc_io_dpd_disable(reg, bit);
	else
		ret = tegra_pmc_io_dpd_enable(reg, bit);
	return ret;
}

static int tegra210_pmc_padctrl_power_enable(struct padctrl_dev *pad_dev,
		int pad_id)
{
	return tegra210_pmc_padctrl_set_power(pad_dev, pad_id, 1);
}

static int tegra210_pmc_padctrl_power_disable(struct padctrl_dev *pad_dev,
		int pad_id)
{
	return tegra210_pmc_padctrl_set_power(pad_dev, pad_id, 0);
}

static struct padctrl_ops tegra210_pmc_padctrl_ops = {
	.set_voltage = &tegra210_pmc_padctrl_set_voltage,
	.get_voltage = &tegra210_pmc_padctl_get_voltage,
	.power_enable = &tegra210_pmc_padctrl_power_enable,
	.power_disable = &tegra210_pmc_padctrl_power_disable,
};

static struct padctrl_desc tegra210_pmc_padctrl_desc = {
	.name = "tegra-pmc-padctrl",
	.ops = &tegra210_pmc_padctrl_ops,
};

static int tegra210_pmc_parse_io_pad_init(struct device_node *np,
		struct padctrl_dev *pad_dev)
{
	struct device_node *pad_np, *child;
	u32 pval;
	int pad_id;
	const char *pad_name, *name;
	bool dpd_en, dpd_dis, pad_en, pad_dis, io_dpd_en, io_dpd_dis;
	bool dyn_pad_volt;
	int n_config;
	u32 *volt_configs, *iodpd_configs;
	int i, index, vcount, dpd_count, pindex;
	int ret;

	pad_np = of_get_child_by_name(np, "io-pad-defaults");
	if (!pad_np)
		return 0;

	/* Ignore the nodes if disabled */
	ret = of_device_is_available(pad_np);
	if (!ret)
		return 0;

	pr_info("PMC: configuring io pad defaults\n");
	n_config = of_get_child_count(pad_np);
	if (!n_config)
		return 0;
	n_config *= 2;

	volt_configs = kzalloc(n_config * sizeof(*volt_configs), GFP_KERNEL);
	if (!volt_configs)
		return -ENOMEM;

	iodpd_configs = kzalloc(n_config * sizeof(*iodpd_configs), GFP_KERNEL);
	if (!iodpd_configs) {
		kfree(volt_configs);
		return -ENOMEM;
	}

	vcount = 0;
	dpd_count = 0;
	for_each_child_of_node(pad_np, child) {
		/* Ignore the nodes if disabled */
		ret = of_device_is_available(child);
		if (!ret)
			continue;
		name = of_get_property(child, "nvidia,pad-name", NULL);
		if (!name)
			name = child->name;

		for (i = 0; i < ARRAY_SIZE(tegra210_pads_info); ++i) {
			if (strcmp(name, tegra210_pads_info[i].pad_name))
				continue;
			ret = of_property_read_u32(child,
					"nvidia,io-pad-init-voltage", &pval);
			if (!ret) {
				volt_configs[vcount] = i;
				volt_configs[vcount + 1] = pval;
				vcount += 2;
			}

			tegra210_pads_info[i].dynamic_pad_voltage =
				of_property_read_bool(child,
					"nvidia,enable-dynamic-pad-voltage");

			dpd_en = of_property_read_bool(child,
						"nvidia,deep-power-down-enable");
			dpd_dis = of_property_read_bool(child,
						"nvidia,deep-power-down-disable");
			pad_en = of_property_read_bool(child,
						"nvidia,io-pad-power-enable");
			pad_dis = of_property_read_bool(child,
						"nvidia,io-pad-power-disable");

			io_dpd_en = dpd_en | pad_dis;
			io_dpd_dis = dpd_dis | pad_en;

			if ((dpd_en && pad_en)	|| (dpd_dis && pad_dis) ||
					(io_dpd_en & io_dpd_dis)) {
				pr_err("PMC: Conflict on io-pad %s config\n",
					name);
				continue;
			}
			if (io_dpd_en || io_dpd_dis) {
				iodpd_configs[dpd_count] = i;
				iodpd_configs[dpd_count + 1] = !!io_dpd_dis;
				dpd_count += 2;
			}
		}
	}

	for (i = 0; i < vcount/2; ++i) {
		index = i * 2;
		if (!volt_configs[index + 1])
			continue;
		pindex = volt_configs[index];
		pad_id = tegra210_pads_info[volt_configs[index]].pad_id;
		pad_name = tegra210_pads_info[volt_configs[index]].pad_name;

		dyn_pad_volt = tegra210_pads_info[pindex].dynamic_pad_voltage;
		tegra210_pads_info[pindex].dynamic_pad_voltage = true;
		ret = tegra210_pmc_padctrl_set_voltage(pad_dev,
				pad_id, volt_configs[index + 1]);
		if (ret < 0) {
			pr_warn("PMC: IO pad %s voltage config failed: %d\n",
				pad_name, ret);
			WARN_ON(1);
		} else {
			pr_info("PMC: IO pad %s voltage is %d\n",
				pad_name, volt_configs[index + 1]);
		}
		tegra210_pads_info[pindex].dynamic_pad_voltage = dyn_pad_volt;
	}

	for (i = 0; i < dpd_count / 2; ++i) {
		index = i * 2;
		pad_id = tegra210_pads_info[iodpd_configs[index]].pad_id;
		pad_name = tegra210_pads_info[iodpd_configs[index]].pad_name;

		ret = tegra210_pmc_padctrl_set_power(pad_dev,
				pad_id, iodpd_configs[index + 1]);
		if (ret < 0) {
			pr_warn("PMC: IO pad %s power config failed: %d\n",
			     pad_name, ret);
			WARN_ON(1);
		} else {
			pr_info("PMC: IO pad %s power is %s\n",
				pad_name, (iodpd_configs[index + 1]) ?
				"enable" : "disable");
		}
	}

	kfree(volt_configs);
	kfree(iodpd_configs);
	return 0;
}

int tegra210_pmc_padctrl_init(struct device *dev, struct device_node *np)
{
	struct tegra210_pmc_padcontrl *pmc_padctrl;
	struct padctrl_config config = { };
	int ret;

	pmc_padctrl = kzalloc(sizeof(*pmc_padctrl), GFP_KERNEL);
	if (!pmc_padctrl) {
		pr_err("Mem allocation for pmc_padctrl failed\n");
		return -ENOMEM;
	}

	config.of_node = (dev && dev->of_node) ? dev->of_node : np;
	pmc_padctrl->pad_dev = padctrl_register(dev, &tegra210_pmc_padctrl_desc,
					&config);
	if (IS_ERR(pmc_padctrl->pad_dev)) {
		ret = PTR_ERR(pmc_padctrl->pad_dev);
		pr_err("T210 padctrl driver init failed: %d\n", ret);
		kfree(pmc_padctrl);
		return ret;
	}
	padctrl_set_drvdata(pmc_padctrl->pad_dev, pmc_padctrl);

	/* Clear all DPD */
	tegra_pmc_io_dpd_clear();

	tegra210_pmc_parse_io_pad_init(config.of_node,
		pmc_padctrl->pad_dev);

	pr_info("T210 pmc padctrl driver initialized\n");
	return 0;
}

#ifdef CONFIG_DEBUG_FS

#include <linux/debugfs.h>
#include <linux/seq_file.h>

static int dbg_io_pad_show(struct seq_file *s, void *unused)
{
	unsigned int pwrdet_mask;
	u32 offset;
	int i;
	unsigned long voltage;

	for (i = 0; i < ARRAY_SIZE(tegra210_pads_info); ++i) {
		if (tegra210_pads_info[i].io_volt_bit_pos < 0)
			continue;

		offset = BIT(tegra210_pads_info[i].io_volt_bit_pos);
		pwrdet_mask = tegra_pmc_pwr_detect_get(offset);
		if (pwrdet_mask & offset)
			voltage = 3300000UL;
		else
			voltage = 1800000UL;
		seq_printf(s, "PMC: IO pad %s voltage %lu\n",
			tegra210_pads_info[i].pad_name, voltage);
	}

	return 0;
}

static int dbg_io_pad_dpd(struct seq_file *s, void *unused)
{
	int i;
	int enable;
	char *estr;
	int bit, reg;

	for (i = 0; i < ARRAY_SIZE(tegra210_pads_info); ++i) {
		if (tegra210_pads_info[i].io_dpd_bit_pos < 0)
			continue;

		bit = tegra210_pads_info[i].io_dpd_bit_pos;
		reg = tegra210_pads_info[i].io_dpd_reg_off;
		enable = tegra_pmc_io_dpd_get_status(reg, bit);
		estr = (enable) ? "enable" : "disable";
		seq_printf(s, "PMC: IO pad DPD %s - %s\n",
			tegra210_pads_info[i].pad_name, estr);
	}

	return 0;
}

#define DEBUG_IO_PAD(_f)						\
static int dbg_io_pad_open_##_f(struct inode *inode, struct file *file)	\
{									\
	return single_open(file, _f, &inode->i_private);		\
}									\
static const struct file_operations debug_fops_##_f = {			\
	.open	   = dbg_io_pad_open_##_f,				\
	.read	   = seq_read,						\
	.llseek	 = seq_lseek,						\
	.release	= single_release,				\
}									\

DEBUG_IO_PAD(dbg_io_pad_show);
DEBUG_IO_PAD(dbg_io_pad_dpd);

static int __init tegra_io_pad_debuginit(void)
{
	(void)debugfs_create_file("tegra_io_pad", S_IRUGO,
				NULL, NULL, &debug_fops_dbg_io_pad_show);
	(void)debugfs_create_file("tegra_io_dpd", S_IRUGO,
				NULL, NULL, &debug_fops_dbg_io_pad_dpd);
	return 0;
}
late_initcall(tegra_io_pad_debuginit);
#endif
