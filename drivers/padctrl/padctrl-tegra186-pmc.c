/*
 * Copyright (c) 2015-2016, NVIDIA CORPORATION.  All rights reserved.
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
#include <dt-bindings/padctrl/tegra186-pads.h>


#define PMC_DDR_PWR	0x38
#define PMC_E_18V_PWR	0x3C
#define PMC_E_33V_PWR	0x40
#define PMC_IO_DPD_REQ	0x74
#define PMC_IO_DPD2_REQ	0x7C

struct tegra186_pmc_pads {
	const char *pad_name;
	int pad_id;
	int pad_reg;
	int lo_volt;
	int hi_volt;
	int bit_position;
	int io_dpd_bit_pos;
	int io_dpd_reg_off;
	bool dynamic_pad_voltage;
};

struct tegra186_pmc_padcontrl {
	struct padctrl_dev *pad_dev;
};

#define TEGRA_186_PADS(_name, _id, _reg, _bit, _lvolt, _hvolt, _dpd_off, _dpd_bit)	\
{								\
	.pad_name = _name,					\
	.pad_id = TEGRA_IO_PAD_GROUP_##_id,			\
	.pad_reg = _reg,					\
	.lo_volt = _lvolt * 1000,				\
	.hi_volt = _hvolt * 1000,				\
	.bit_position = _bit,					\
	.io_dpd_reg_off = _dpd_off,				\
	.io_dpd_bit_pos = _dpd_bit,				\
}

static struct tegra186_pmc_pads tegra186_pads[] = {
	TEGRA_186_PADS("audio", AUDIO, PMC_IO_DPD_REQ, -1, -1, -1, 0, 17),
	TEGRA_186_PADS("audio-hv", AUDIO_HV, PMC_E_33V_PWR, 1, 1800, 3300, 1, 29),
	TEGRA_186_PADS("cam", CAM, PMC_IO_DPD2_REQ, -1, -1, -1, 1, 6),
	TEGRA_186_PADS("conn", CONN, PMC_IO_DPD2_REQ, -1, -1, -1, 1, 28),
	TEGRA_186_PADS("csia", CSIA, PMC_IO_DPD_REQ, -1, -1, -1, 0, 0),
	TEGRA_186_PADS("csib", CSIB, PMC_IO_DPD_REQ, -1, -1, -1, 0, 1),
	TEGRA_186_PADS("csic", CSIC, PMC_IO_DPD2_REQ, -1, -1, -1, 1, 11),
	TEGRA_186_PADS("csid", CSID, PMC_IO_DPD2_REQ, -1, -1, -1, 1, 12),
	TEGRA_186_PADS("csie", CSIE, PMC_IO_DPD2_REQ, -1, -1, -1, 1, 13),
	TEGRA_186_PADS("csif", CSIF, PMC_IO_DPD2_REQ, -1, -1, -1, 1, 14),
	TEGRA_186_PADS("dbg", DBG, PMC_E_18V_PWR, 4, 1200, 1800, 0, 25),
	TEGRA_186_PADS("dmic-hv", DMIC_HV, PMC_E_33V_PWR, 2, 1800, 3300, 1, 20),
	TEGRA_186_PADS("dsi", DSI, PMC_IO_DPD_REQ, -1, -1, -1, 0, 2),
	TEGRA_186_PADS("dsib", DSIB, PMC_IO_DPD2_REQ, -1, -1, -1, 1, 8),
	TEGRA_186_PADS("dsic", DSIC, PMC_IO_DPD2_REQ, -1, -1, -1, 1, 9),
	TEGRA_186_PADS("dsid", DSID, PMC_IO_DPD2_REQ, -1, -1, -1, 1, 10),
	TEGRA_186_PADS("edp", EDP, PMC_IO_DPD2_REQ, -1, -1, -1, 1, 21),
	TEGRA_186_PADS("hdmi-dp0", HDMI_DP0, PMC_IO_DPD_REQ, -1, -1, -1, 0, 28),
	TEGRA_186_PADS("hdmi-dp1", HDMI_DP1, PMC_IO_DPD_REQ, -1, -1, -1, 0, 29),
	TEGRA_186_PADS("hsic", HSIC, PMC_IO_DPD_REQ, -1, -1, -1, 0, 19),
	TEGRA_186_PADS("mipi-bias", MIPI_BIAS, PMC_IO_DPD_REQ, -1, -1, -1, 0, 3),
	TEGRA_186_PADS("pex-clk-bias", PEX_CLK_BIAS, PMC_IO_DPD_REQ, -1, -1, -1, 0, 4),
	TEGRA_186_PADS("pex-clk1", PEX_CLK1, PMC_IO_DPD_REQ, -1, -1, -1, 0, 7),
	TEGRA_186_PADS("pex-clk2", PEX_CLK2, PMC_IO_DPD_REQ, -1, -1, -1, 0, 6),
	TEGRA_186_PADS("pex-clk3", PEX_CLK3, PMC_IO_DPD_REQ, -1, -1, -1, 0, 5),
	TEGRA_186_PADS("pex-ctrl", PEX_CTRL, PMC_IO_DPD2_REQ, -1, -1, -1, 1, 0),
	TEGRA_186_PADS("sdmmc1-hv", SDMMC1_HV, PMC_E_33V_PWR, 4, 1800, 3300, 1, 23),
	TEGRA_186_PADS("sdmmc2-hv", SDMMC2_HV, PMC_E_33V_PWR, 5, 1800, 3300, 1, 2),
	TEGRA_186_PADS("sdmmc3-hv", SDMMC3_HV, PMC_E_33V_PWR, 6, 1800, 3300, 1, 24),
	TEGRA_186_PADS("sdmmc4", SDMMC4, PMC_IO_DPD2_REQ, -1, -1, -1, 1, 4),
	TEGRA_186_PADS("spi", SPI, PMC_E_18V_PWR, 5, 1200, 1800, 1, 15),
	TEGRA_186_PADS("uart", UART, PMC_IO_DPD_REQ, -1, -1, -1, 0, 14),
	TEGRA_186_PADS("usb-bias", USB_BIAS, PMC_IO_DPD_REQ, -1, -1, -1, 0, 12),
	TEGRA_186_PADS("usb0", USB0, PMC_IO_DPD_REQ, -1, -1, -1, 0, 9),
	TEGRA_186_PADS("usb1", USB1, PMC_IO_DPD_REQ, -1, -1, -1, 0, 10),
	TEGRA_186_PADS("usb2", USB2, PMC_IO_DPD_REQ, -1, -1, -1, 0, 11),
	TEGRA_186_PADS("ufs", UFS, PMC_E_18V_PWR, 1, 1200, 1800, 1, 17),
	TEGRA_186_PADS("ao-hv", AO_HV, PMC_E_33V_PWR, 0, 1800, 3300, -1, -1),
	TEGRA_186_PADS("ddr-dvi", DDR_DVI, PMC_DDR_PWR, 0, 1200, 1800, -1, -1),
	TEGRA_186_PADS("ddr-gmi", DDR_GMI, PMC_DDR_PWR, 1, 1200, 1800, -1, -1),
	TEGRA_186_PADS("ddr-sdmmc2", DDR_SDMMC2, PMC_DDR_PWR, 2, 1200, 1800, -1, -1),
	TEGRA_186_PADS("ddr-spi", DDR_SPI, PMC_DDR_PWR, 3, 1200, 1800, -1, -1),
};

static int tegra186_pmc_padctrl_set_voltage(struct padctrl_dev *pad_dev,
		int pad_id, u32 voltage)
{
	u32 offset;
	unsigned int pad_mask;
	u32 curr_volt;
	int val;
	int i;

	if ((voltage != 1200000) && (voltage != 1800000)
			&& (voltage != 3300000)) {
		pr_err("Pad voltage %u is not valid\n", voltage);
		return -EINVAL;
	}

	for (i = 0; i < ARRAY_SIZE(tegra186_pads); ++i) {
		if (tegra186_pads[i].pad_id == pad_id)
			break;
	}

	if (i == ARRAY_SIZE(tegra186_pads)) {
		pr_err("Pad ID %d does not support for dynamic voltage\n",
			pad_id);
		return -EINVAL;
	}

	if (!tegra186_pads[i].dynamic_pad_voltage) {
		offset = BIT(tegra186_pads[i].bit_position);
		pad_mask = tegra_pmc_pad_voltage_get(tegra186_pads[i].pad_reg);
		curr_volt = (pad_mask & offset) ? tegra186_pads[i].hi_volt :
					tegra186_pads[i].lo_volt;
		if (voltage == curr_volt)
			return 0;

		pr_err("Pad %s: Dynamic pad voltage is not supported\n",
			tegra186_pads[i].pad_name);

		return -EINVAL;
	}

	if ((voltage < tegra186_pads[i].lo_volt) ||
		(voltage > tegra186_pads[i].hi_volt)) {
		pr_err("Voltage %d is not supported for pad %s\n",
			voltage, tegra186_pads[i].pad_name);
		return -EINVAL;
	}

	offset = BIT(tegra186_pads[i].bit_position);
	val = (voltage == tegra186_pads[i].hi_volt) ? offset : 0;
	tegra_pmc_pad_voltage_update(tegra186_pads[i].pad_reg, offset, val);
	if ((pad_id == TEGRA_IO_PAD_GROUP_SDMMC1_HV) ||
		(pad_id == TEGRA_IO_PAD_GROUP_SDMMC2_HV) ||
		(pad_id == TEGRA_IO_PAD_GROUP_SDMMC3_HV)) {
		pad_mask = tegra_pmc_pad_voltage_get(tegra186_pads[i].pad_reg);
		pr_info("pad_id %d:  PMC_IMPL_E_33V_PWR_0 = [0x%x]"
				, pad_id, pad_mask );
	}
	udelay(100);
	return 0;
}

static int tegra186_pmc_padctl_get_voltage(struct padctrl_dev *pad_dev,
		int pad_id, u32 *voltage)
{
	unsigned int pad_mask;
	u32 offset;
	int i;

	for (i = 0; i < ARRAY_SIZE(tegra186_pads); ++i) {
		if (tegra186_pads[i].pad_id == pad_id)
			break;
	}

	if (i == ARRAY_SIZE(tegra186_pads))
		return -EINVAL;

	offset = BIT(tegra186_pads[i].bit_position);
	pad_mask = tegra_pmc_pad_voltage_get(tegra186_pads[i].pad_reg);
	*voltage = (pad_mask & offset) ? tegra186_pads[i].hi_volt :
						tegra186_pads[i].lo_volt;
	return 0;
}

static int tegra186_pmc_padctrl_set_power(struct padctrl_dev *pad_dev,
		int pad_id, u32 enable)
{
	u32 bit, reg;
	int i;
	int ret;

	for (i = 0; i < ARRAY_SIZE(tegra186_pads); ++i) {
		if (tegra186_pads[i].pad_id == pad_id)
			break;
	}

	if (i == ARRAY_SIZE(tegra186_pads))
		return -EINVAL;

	if (tegra186_pads[i].io_dpd_bit_pos < 0)
		return -EINVAL;

	bit = tegra186_pads[i].io_dpd_bit_pos;
	reg = tegra186_pads[i].io_dpd_reg_off;
	if (enable)
		ret = tegra186_pmc_io_dpd_disable(reg, bit);
	else
		ret = tegra186_pmc_io_dpd_enable(reg, bit);
	return ret;
}

static int tegra186_pmc_padctrl_power_enable(struct padctrl_dev *pad_dev,
		int pad_id)
{
	return tegra186_pmc_padctrl_set_power(pad_dev, pad_id, 1);
}

static int tegra186_pmc_padctrl_power_disable(struct padctrl_dev *pad_dev,
		int pad_id)
{
	return tegra186_pmc_padctrl_set_power(pad_dev, pad_id, 0);
}

static struct padctrl_ops tegra186_pmc_padctrl_ops = {
	.set_voltage = &tegra186_pmc_padctrl_set_voltage,
	.get_voltage = &tegra186_pmc_padctl_get_voltage,
	.power_enable = &tegra186_pmc_padctrl_power_enable,
	.power_disable = &tegra186_pmc_padctrl_power_disable,
};

static struct padctrl_desc tegra186_pmc_padctrl_desc = {
	.name = "tegra-pmc-padctrl",
	.ops = &tegra186_pmc_padctrl_ops,
};

static int tegra186_pmc_parse_io_pad_init(struct device_node *np,
		struct padctrl_dev *pad_dev)
{
	struct device_node *pad_np, *child;
	u32 pval;
	int pad_id;
	const char *pad_name, *name;
	int n_config;
	u32 *volt_configs, *iodpd_configs;
	int i, index, vcount;
	bool dyn_pad_volt, dpd_en, dpd_dis, pad_en, pad_dis, io_dpd_en, io_dpd_dis;
	int pindex, dpd_count;
	int ret;

	pad_np = of_get_child_by_name(np, "io-pad-defaults");
	if (!pad_np)
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

		for (i = 0; i < ARRAY_SIZE(tegra186_pads); ++i) {
			if (strcmp(name, tegra186_pads[i].pad_name))
				continue;
			ret = of_property_read_u32(child,
				"nvidia,io-pad-init-voltage", &pval);
			if (!ret) {
				volt_configs[vcount] = i;
				volt_configs[vcount + 1] = pval;
				vcount += 2;
			}
			tegra186_pads[i].dynamic_pad_voltage =
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
		pad_id = tegra186_pads[pindex].pad_id;
		pad_name = tegra186_pads[pindex].pad_name;

		dyn_pad_volt = tegra186_pads[pindex].dynamic_pad_voltage;
		tegra186_pads[pindex].dynamic_pad_voltage = true;
		ret = tegra186_pmc_padctrl_set_voltage(pad_dev,
				pad_id, volt_configs[index + 1]);
		if (ret < 0) {
			pr_warn("PMC: IO pad %s voltage config failed: %d\n",
					pad_name, ret);
			WARN_ON(1);
		} else {
			pr_info("PMC: IO pad %s voltage is %d\n",
					pad_name, volt_configs[index + 1]);
		}
		tegra186_pads[pindex].dynamic_pad_voltage = dyn_pad_volt;
	}

	for (i = 0; i < dpd_count / 2; ++i) {
		index = i * 2;
		pad_id = tegra186_pads[iodpd_configs[index]].pad_id;
		pad_name = tegra186_pads[iodpd_configs[index]].pad_name;

		ret = tegra186_pmc_padctrl_set_power(pad_dev,
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

int tegra186_pmc_padctrl_init(struct device *dev, struct device_node *np)
{
	struct tegra186_pmc_padcontrl *pmc_padctrl;
	struct padctrl_config config = { };
	int ret;

	pmc_padctrl = devm_kzalloc(dev, sizeof(*pmc_padctrl), GFP_KERNEL);
	if (!pmc_padctrl) {
		pr_err("Mem allocation for pmc_padctrl failed\n");
		return -ENOMEM;
	}

	config.of_node = dev->of_node ? dev->of_node : np;
	pmc_padctrl->pad_dev = padctrl_register(dev, &tegra186_pmc_padctrl_desc,
					&config);
	if (IS_ERR(pmc_padctrl->pad_dev)) {
		ret = PTR_ERR(pmc_padctrl->pad_dev);
		pr_err("T186 padctrl driver init failed: %d\n", ret);
		devm_kfree(dev, pmc_padctrl);
		return ret;
	}
	padctrl_set_drvdata(pmc_padctrl->pad_dev, pmc_padctrl);
	tegra186_pmc_parse_io_pad_init(config.of_node,
				pmc_padctrl->pad_dev);

	pr_info("T186 pmc padctrl driver initialized\n");
	return 0;
}

#ifdef CONFIG_DEBUG_FS
#include <linux/debugfs.h>
#include <linux/seq_file.h>

static int dbg_io_pad_voltage(struct seq_file *s, void *unused)
{
	unsigned int pad_mask;
	u32 offset;
	int i;
	unsigned long voltage;

	for (i = 0; i < ARRAY_SIZE(tegra186_pads); ++i) {
		if (tegra186_pads[i].bit_position < 0)
			continue;
		offset = BIT(tegra186_pads[i].bit_position);
		pad_mask = tegra_pmc_pad_voltage_get(tegra186_pads[i].pad_reg);
		voltage = (pad_mask & offset) ? tegra186_pads[i].hi_volt :
						tegra186_pads[i].lo_volt;
		seq_printf(s, "PMC: IO pad %s voltage %lu\n",
			tegra186_pads[i].pad_name, voltage);
	}

	return 0;
}

static int dbg_io_pad_dpd(struct seq_file *s, void *unused)
{
	int i;
	int enable;
	char *estr;
	int bit, reg;

	for (i = 0; i < ARRAY_SIZE(tegra186_pads); ++i) {
		if (tegra186_pads[i].io_dpd_bit_pos < 0)
			continue;

		bit = tegra186_pads[i].io_dpd_bit_pos;
		reg = tegra186_pads[i].io_dpd_reg_off;
		enable = tegra186_pmc_io_dpd_get_status(reg, bit);
		estr = (enable) ? "enable" : "disable";
		seq_printf(s, "PMC: IO pad DPD %s - %s\n",
			tegra186_pads[i].pad_name, estr);
	}

	return 0;
}

#define DEBUG_IO_PAD(_f)					\
static int dbg_io_pad_open_##_f(struct inode *inode, struct file *file) \
{								\
	return single_open(file, dbg_##_f, &inode->i_private);	\
}								\
static const struct file_operations debug_fops_##_f = {		\
	.open		= dbg_io_pad_open_##_f,			\
	.read		= seq_read,				\
	.llseek		= seq_lseek,				\
	.release	= single_release,			\
}								\

DEBUG_IO_PAD(io_pad_voltage);
DEBUG_IO_PAD(io_pad_dpd);

static int __init tegra_io_pad_debuginit(void)
{
	(void)debugfs_create_file("tegra_io_pad_voltage", S_IRUGO,
				NULL, NULL, &debug_fops_io_pad_voltage);

	(void)debugfs_create_file("tegra_io_pad_dpd", S_IRUGO,
				NULL, NULL, &debug_fops_io_pad_dpd);
	return 0;
}
late_initcall(tegra_io_pad_debuginit);

#endif
