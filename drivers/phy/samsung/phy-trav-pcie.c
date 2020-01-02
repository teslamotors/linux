/*
 * Samsung TRAV SoC series PCIe PHY driver
 *
 * Phy provider for PCIe controller on Exynos SoC series
 *
 * Copyright (C) 2018 Samsung Electronics Co., Ltd.
 * Niyas Ahmed S T <niyas.ahmed@samsung.com>
 * Pankaj Dubey <pankaj.dubey@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/init.h>
#include <linux/mfd/syscon.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/phy/phy.h>
#include <linux/regmap.h>

/* TRAV PCIe PHY registers */
#define PHY_TRSV_CMN_REG03	0x00C
#define PHY_TRSV_CMN_REG05F	0x17C
#define PHY_TRSV_CMN_REG060	0x180
#define PHY_TRSV_CMN_REG062	0x188

#define PHY_AGG_BIF_RESET	0x0200
#define PHY_AGG_BIF_CLOCK	0x0208

#define PHY_TRSV_REG001_LN_N	0x404
#define PHY_TRSV_REG002_LN_N	0x408
#define PHY_TRSV_REG005_LN_N	0x414
#define PHY_TRSV_REG006_LN_N	0x418
#define PHY_TRSV_REG007_LN_N	0x41C
#define PHY_TRSV_REG009_LN_N	0x424
#define PHY_TRSV_REG00A_LN_N	0x428
#define PHY_TRSV_REG00C_LN_N	0x430
#define PHY_TRSV_REG012_LN_N	0x448
#define PHY_TRSV_REG013_LN_N	0x44C
#define PHY_TRSV_REG014_LN_N	0x450
#define PHY_TRSV_REG015_LN_N	0x454
#define PHY_TRSV_REG016_LN_N	0x458
#define PHY_TRSV_REG039_LN_N	0x4E4
#define PHY_TRSV_REG03B_LN_N	0x4EC
#define PHY_TRSV_REG03C_LN_N	0x4F0
#define PHY_TRSV_REG03E_LN_N	0x4F8
#define PHY_TRSV_REG03F_LN_N	0x4FC
#define PHY_TRSV_REG043_LN_N	0x50C
#define PHY_TRSV_REG044_LN_N	0x510
#define PHY_TRSV_REG048_LN_N	0x520
#define PHY_TRSV_REG049_LN_N	0x524
#define PHY_TRSV_REG04E_LN_N	0x538
#define PHY_TRSV_REG052_LN_N	0x548
#define PHY_TRSV_REG068_LN_N	0x5A0
#define PHY_TRSV_REG069_LN_N	0x5A4
#define PHY_TRSV_REG06B_LN_N	0x5AC
#define PHY_TRSV_REG07B_LN_N	0x5EC
#define PHY_TRSV_REG083_LN_N	0x60C
#define PHY_TRSV_REG086_LN_N	0x618
#define PHY_TRSV_REG087_LN_N	0x61C
#define PHY_TRSV_REG09D_LN_N	0x674
#define PHY_TRSV_REG09E_LN_N	0x678
#define PHY_TRSV_REG09F_LN_N	0x67C
#define PHY_TRSV_REG0CF_LN_N	0x738
#define PHY_TRSV_REG0FC_LN_N	0x7F0
#define PHY_TRSV_REG0FD_LN_N	0x7F4
/* TRAV PCIe PCS registers */
#define PCS_BRF_0		0x0004
#define PCS_BRF_1		0x0804
#define PCS_CLK			0x0180

/* TRAV SYS REG registers */
#define PHY_0_CON		0x042C
#define PHY_1_CON		0x0500

struct trav_pcie_phy_data {
	const struct phy_ops	*ops;
};

struct trav_pcie_phy {
	const struct trav_pcie_phy_data *drv_data;
	void __iomem *phy_base;
	void __iomem *pcs_base;
	struct regmap *sysreg;
	u32 phy_mode;
	int phy_id;
};

static void trav_phy_writel(void __iomem *base, u32 val, u32 offset)
{
	writel(val, base + offset);
}

static void trav_phy_ln_n_writel(void __iomem *base, u32 val, u32 offset)
{
	u32 i = 0;

	for (i = 0; i < 4; i++)
		writel(val, base + (offset + i * 0x400));
}

static void trav_pcie_phy0_init(struct phy *phy)
{
	struct trav_pcie_phy *phy_ctrl = phy_get_drvdata(phy);

	trav_phy_ln_n_writel(phy_ctrl->phy_base, 0x20, PHY_TRSV_REG07B_LN_N);
	trav_phy_ln_n_writel(phy_ctrl->phy_base, 0x00, PHY_TRSV_REG052_LN_N);
	trav_phy_writel(phy_ctrl->phy_base, 0x11, PHY_TRSV_CMN_REG05F);
	trav_phy_writel(phy_ctrl->phy_base, 0x23, PHY_TRSV_CMN_REG060);
	trav_phy_writel(phy_ctrl->phy_base, 0x0, PHY_TRSV_CMN_REG062);
	trav_phy_writel(phy_ctrl->phy_base, 0x15, PHY_TRSV_CMN_REG03);

	trav_phy_ln_n_writel(phy_ctrl->phy_base, 0x8, PHY_TRSV_REG0CF_LN_N);
	trav_phy_ln_n_writel(phy_ctrl->phy_base, 0xF, PHY_TRSV_REG039_LN_N);
	trav_phy_ln_n_writel(phy_ctrl->phy_base, 0x13, PHY_TRSV_REG03B_LN_N);
	trav_phy_ln_n_writel(phy_ctrl->phy_base, 0xF6, PHY_TRSV_REG03C_LN_N);
	trav_phy_ln_n_writel(phy_ctrl->phy_base, 0x57, PHY_TRSV_REG044_LN_N);
	trav_phy_ln_n_writel(phy_ctrl->phy_base, 0x10, PHY_TRSV_REG03E_LN_N);
	trav_phy_ln_n_writel(phy_ctrl->phy_base, 0x4, PHY_TRSV_REG03F_LN_N);
	trav_phy_ln_n_writel(phy_ctrl->phy_base, 0x11, PHY_TRSV_REG043_LN_N);
	trav_phy_ln_n_writel(phy_ctrl->phy_base, 0x6F, PHY_TRSV_REG049_LN_N);
	trav_phy_ln_n_writel(phy_ctrl->phy_base, 0x18, PHY_TRSV_REG04E_LN_N);
	trav_phy_ln_n_writel(phy_ctrl->phy_base, 0x1F, PHY_TRSV_REG068_LN_N);
	trav_phy_ln_n_writel(phy_ctrl->phy_base, 0xC, PHY_TRSV_REG069_LN_N);
	trav_phy_ln_n_writel(phy_ctrl->phy_base, 0x78, PHY_TRSV_REG06B_LN_N);
	trav_phy_ln_n_writel(phy_ctrl->phy_base, 0x8, PHY_TRSV_REG083_LN_N);
	trav_phy_ln_n_writel(phy_ctrl->phy_base, 0xFF, PHY_TRSV_REG086_LN_N);
	trav_phy_ln_n_writel(phy_ctrl->phy_base, 0x3C, PHY_TRSV_REG087_LN_N);
	trav_phy_ln_n_writel(phy_ctrl->phy_base, 0x7C, PHY_TRSV_REG09D_LN_N);
	trav_phy_ln_n_writel(phy_ctrl->phy_base, 0x33, PHY_TRSV_REG09E_LN_N);
	trav_phy_ln_n_writel(phy_ctrl->phy_base, 0x33, PHY_TRSV_REG09F_LN_N);
	trav_phy_ln_n_writel(phy_ctrl->phy_base, 0x3F, PHY_TRSV_REG001_LN_N);
	trav_phy_ln_n_writel(phy_ctrl->phy_base, 0x1C, PHY_TRSV_REG002_LN_N);
	trav_phy_ln_n_writel(phy_ctrl->phy_base, 0x2B, PHY_TRSV_REG005_LN_N);
	trav_phy_ln_n_writel(phy_ctrl->phy_base, 0x3, PHY_TRSV_REG006_LN_N);
	trav_phy_ln_n_writel(phy_ctrl->phy_base, 0x0C, PHY_TRSV_REG007_LN_N);
	trav_phy_ln_n_writel(phy_ctrl->phy_base, 0x10, PHY_TRSV_REG009_LN_N);
	trav_phy_ln_n_writel(phy_ctrl->phy_base, 0x1, PHY_TRSV_REG00A_LN_N);
	trav_phy_ln_n_writel(phy_ctrl->phy_base, 0x93, PHY_TRSV_REG00C_LN_N);
	trav_phy_ln_n_writel(phy_ctrl->phy_base, 0x1, PHY_TRSV_REG012_LN_N);
	trav_phy_ln_n_writel(phy_ctrl->phy_base, 0x0, PHY_TRSV_REG013_LN_N);
	trav_phy_ln_n_writel(phy_ctrl->phy_base, 0x70, PHY_TRSV_REG014_LN_N);
	trav_phy_ln_n_writel(phy_ctrl->phy_base, 0x0, PHY_TRSV_REG015_LN_N);
	trav_phy_ln_n_writel(phy_ctrl->phy_base, 0x70, PHY_TRSV_REG016_LN_N);
	trav_phy_ln_n_writel(phy_ctrl->phy_base, 0x80, PHY_TRSV_REG0FC_LN_N);
	trav_phy_ln_n_writel(phy_ctrl->phy_base, 0x0, PHY_TRSV_REG0FD_LN_N);

	regmap_update_bits(phy_ctrl->sysreg, PHY_0_CON, 0x100, (0x1 << 8));
}

static void trav_pcie_phy1_init(struct phy *phy)
{
	struct trav_pcie_phy *phy_ctrl = phy_get_drvdata(phy);

	trav_phy_ln_n_writel(phy_ctrl->phy_base, 0x20, PHY_TRSV_REG07B_LN_N);
	trav_phy_ln_n_writel(phy_ctrl->phy_base, 0x00, PHY_TRSV_REG052_LN_N);
	trav_phy_writel(phy_ctrl->phy_base, 0x11, PHY_TRSV_CMN_REG05F);
	trav_phy_writel(phy_ctrl->phy_base, 0x23, PHY_TRSV_CMN_REG060);
	trav_phy_writel(phy_ctrl->phy_base, 0x0, PHY_TRSV_CMN_REG062);
	trav_phy_writel(phy_ctrl->phy_base, 0x15, PHY_TRSV_CMN_REG03);

	trav_phy_ln_n_writel(phy_ctrl->phy_base, 0x8, PHY_TRSV_REG0CF_LN_N);
	trav_phy_ln_n_writel(phy_ctrl->phy_base, 0xF, PHY_TRSV_REG039_LN_N);
	trav_phy_ln_n_writel(phy_ctrl->phy_base, 0x13, PHY_TRSV_REG03B_LN_N);
	trav_phy_ln_n_writel(phy_ctrl->phy_base, 0xF6, PHY_TRSV_REG03C_LN_N);
	trav_phy_ln_n_writel(phy_ctrl->phy_base, 0x57, PHY_TRSV_REG044_LN_N);
	trav_phy_ln_n_writel(phy_ctrl->phy_base, 0x10, PHY_TRSV_REG03E_LN_N);
	trav_phy_ln_n_writel(phy_ctrl->phy_base, 0x4, PHY_TRSV_REG03F_LN_N);
	trav_phy_ln_n_writel(phy_ctrl->phy_base, 0x11, PHY_TRSV_REG043_LN_N);
	trav_phy_ln_n_writel(phy_ctrl->phy_base, 0x6F, PHY_TRSV_REG049_LN_N);
	trav_phy_ln_n_writel(phy_ctrl->phy_base, 0x18, PHY_TRSV_REG04E_LN_N);
	trav_phy_ln_n_writel(phy_ctrl->phy_base, 0x1F, PHY_TRSV_REG068_LN_N);
	trav_phy_ln_n_writel(phy_ctrl->phy_base, 0xC, PHY_TRSV_REG069_LN_N);
	trav_phy_ln_n_writel(phy_ctrl->phy_base, 0x78, PHY_TRSV_REG06B_LN_N);
	trav_phy_ln_n_writel(phy_ctrl->phy_base, 0x8, PHY_TRSV_REG083_LN_N);
	trav_phy_ln_n_writel(phy_ctrl->phy_base, 0xFF, PHY_TRSV_REG086_LN_N);
	trav_phy_ln_n_writel(phy_ctrl->phy_base, 0x3C, PHY_TRSV_REG087_LN_N);
	trav_phy_ln_n_writel(phy_ctrl->phy_base, 0x7C, PHY_TRSV_REG09D_LN_N);
	trav_phy_ln_n_writel(phy_ctrl->phy_base, 0x33, PHY_TRSV_REG09E_LN_N);
	trav_phy_ln_n_writel(phy_ctrl->phy_base, 0x33, PHY_TRSV_REG09F_LN_N);
	trav_phy_ln_n_writel(phy_ctrl->phy_base, 0x3F, PHY_TRSV_REG001_LN_N);
	trav_phy_ln_n_writel(phy_ctrl->phy_base, 0x1C, PHY_TRSV_REG002_LN_N);
	trav_phy_ln_n_writel(phy_ctrl->phy_base, 0x2B, PHY_TRSV_REG005_LN_N);
	trav_phy_ln_n_writel(phy_ctrl->phy_base, 0x3, PHY_TRSV_REG006_LN_N);
	trav_phy_ln_n_writel(phy_ctrl->phy_base, 0x0C, PHY_TRSV_REG007_LN_N);
	trav_phy_ln_n_writel(phy_ctrl->phy_base, 0x10, PHY_TRSV_REG009_LN_N);
	trav_phy_ln_n_writel(phy_ctrl->phy_base, 0x1, PHY_TRSV_REG00A_LN_N);
	trav_phy_ln_n_writel(phy_ctrl->phy_base, 0x93, PHY_TRSV_REG00C_LN_N);
	trav_phy_ln_n_writel(phy_ctrl->phy_base, 0x1, PHY_TRSV_REG012_LN_N);
	trav_phy_ln_n_writel(phy_ctrl->phy_base, 0x0, PHY_TRSV_REG013_LN_N);
	trav_phy_ln_n_writel(phy_ctrl->phy_base, 0x70, PHY_TRSV_REG014_LN_N);
	trav_phy_ln_n_writel(phy_ctrl->phy_base, 0x0, PHY_TRSV_REG015_LN_N);
	trav_phy_ln_n_writel(phy_ctrl->phy_base, 0x70, PHY_TRSV_REG016_LN_N);
	trav_phy_ln_n_writel(phy_ctrl->phy_base, 0x80, PHY_TRSV_REG0FC_LN_N);
	trav_phy_ln_n_writel(phy_ctrl->phy_base, 0x0, PHY_TRSV_REG0FD_LN_N);

	regmap_update_bits(phy_ctrl->sysreg, PHY_1_CON, 0x2, (0x1 << 1));
}

static int trav_pcie_phy_init(struct phy *phy)
{
	struct trav_pcie_phy *phy_ctrl = phy_get_drvdata(phy);

	if (phy_ctrl->phy_mode == 0) {
		trav_phy_writel(phy_ctrl->pcs_base, 0x00, PCS_BRF_0);
		trav_phy_writel(phy_ctrl->pcs_base, 0x00, PCS_BRF_1);
		trav_phy_writel(phy_ctrl->phy_base, 0x00, PHY_AGG_BIF_RESET);
		trav_phy_writel(phy_ctrl->phy_base, 0x00, PHY_AGG_BIF_CLOCK);
	} else if (phy_ctrl->phy_mode == 1) {
		trav_phy_writel(phy_ctrl->pcs_base, 0xF, PCS_BRF_0);
		trav_phy_writel(phy_ctrl->pcs_base, 0xF, PCS_BRF_1);
		trav_phy_writel(phy_ctrl->phy_base, 0x55, PHY_AGG_BIF_RESET);
		trav_phy_writel(phy_ctrl->phy_base, 0x00, PHY_AGG_BIF_CLOCK);
	} else if (phy_ctrl->phy_mode == 2) {
		trav_phy_writel(phy_ctrl->pcs_base, 0xC, PCS_BRF_0);
		trav_phy_writel(phy_ctrl->pcs_base, 0xC, PCS_BRF_1);
		trav_phy_writel(phy_ctrl->phy_base, 0x50, PHY_AGG_BIF_RESET);
		trav_phy_writel(phy_ctrl->phy_base, 0xA0, PHY_AGG_BIF_CLOCK);
	}

	if (phy_ctrl->phy_id == 0)
		trav_pcie_phy0_init(phy);
	else
		trav_pcie_phy1_init(phy);

	return 0;
}

static int trav_pcie_phy_reset(struct phy *phy)
{
	struct trav_pcie_phy *phy_ctrl = phy_get_drvdata(phy);

	if (phy_ctrl->phy_id == 1)
		regmap_update_bits(phy_ctrl->sysreg,
				PHY_1_CON, 0x8, (0x1 << 3));
	else
		regmap_update_bits(phy_ctrl->sysreg,
				PHY_0_CON, 0x200, (0x1 << 9));

	return 0;
}

static int trav_pcie_phy_exit(struct phy *phy)
{
	/* todo : phy power down sequence */
	return 0;
}

static const struct phy_ops trav_phy_ops = {
	.init  = trav_pcie_phy_init,
	.reset = trav_pcie_phy_reset,
	.exit  = trav_pcie_phy_exit,
	.owner = THIS_MODULE,
};

static const struct trav_pcie_phy_data trav_pcie_phy_data = {
	.ops		= &trav_phy_ops,
};

static const struct of_device_id trav_pcie_phy_match[] = {
	{
		.compatible = "samsung,trav-pcie-phy",
		.data = &trav_pcie_phy_data,
	},
	{},
};

static void trav_pcie_phy_update_phy_controller(struct trav_pcie_phy *trav_phy)
{
	trav_phy_writel(trav_phy->pcs_base, 0x1, PCS_CLK);

	if (trav_phy->phy_id == 0) {
		/* FSYS0 block PCIe PHY Controller */
		regmap_update_bits(trav_phy->sysreg, PHY_0_CON, 0x3FF,
				   0x0);
		regmap_update_bits(trav_phy->sysreg, PHY_0_CON, 0x10,
				(0x1 << 4));
		regmap_update_bits(trav_phy->sysreg, PHY_0_CON, 0x3,
				(0x2 << 0));
		regmap_update_bits(trav_phy->sysreg, PHY_0_CON, 0x200,
				(0x1 << 9));
	} else if (trav_phy->phy_id == 1) {
		/* FSYS1 block PCIe PHY Controller */
		regmap_update_bits(trav_phy->sysreg, PHY_1_CON, 0x1FF,
				   0x0);
		regmap_update_bits(trav_phy->sysreg, PHY_1_CON, 0x1,
				(0x1 << 0));
		regmap_update_bits(trav_phy->sysreg, PHY_1_CON, 0x30,
				(0x2 << 4));
		regmap_update_bits(trav_phy->sysreg, PHY_1_CON, 0x8,
				(0x1 << 3));
	}
}

static int trav_pcie_phy_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct trav_pcie_phy *trav_phy;
	struct phy *generic_phy;
	struct phy_provider *phy_provider;
	struct resource *res;
	const struct trav_pcie_phy_data *drv_data;

	drv_data = of_device_get_match_data(dev);
	if (!drv_data)
		return -ENODEV;

	trav_phy = devm_kzalloc(dev, sizeof(*trav_phy), GFP_KERNEL);
	if (!trav_phy)
		return -ENOMEM;

	if (of_property_read_u32(dev->of_node, "phy-mode",
				&trav_phy->phy_mode)) {
		dev_err(dev, "Failed selecting the phy-mode\n");
		return -EINVAL;
	}

	trav_phy->phy_id = of_alias_get_id(dev->of_node, "pciephy");

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	trav_phy->phy_base = devm_ioremap_resource(dev, res);
	if (IS_ERR(trav_phy->phy_base))
		return PTR_ERR(trav_phy->phy_base);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	trav_phy->pcs_base = devm_ioremap_resource(dev, res);
	if (IS_ERR(trav_phy->pcs_base))
		return PTR_ERR(trav_phy->pcs_base);

	/* sysreg regmap handle */
	trav_phy->sysreg = syscon_regmap_lookup_by_phandle(dev->of_node,
			"samsung,fsys-sysreg");
	if (IS_ERR(trav_phy->sysreg)) {
		dev_err(dev, "fsys sysreg regmap lookup failed.\n");
		return PTR_ERR(trav_phy->sysreg);
	}

	trav_phy->drv_data = drv_data;

	generic_phy = devm_phy_create(dev, dev->of_node, drv_data->ops);
	if (IS_ERR(generic_phy)) {
		dev_err(dev, "failed to create PHY\n");
		return PTR_ERR(generic_phy);
	}

	phy_set_drvdata(generic_phy, trav_phy);
	platform_set_drvdata(pdev, trav_phy);
	phy_provider = devm_of_phy_provider_register(dev, of_phy_simple_xlate);

	trav_pcie_phy_update_phy_controller(trav_phy);

	return PTR_ERR_OR_ZERO(phy_provider);
}

static int trav_pcie_phy_suspend(struct device *dev)
{
	return 0;
}

static int trav_pcie_phy_resume(struct device *dev)
{
	struct trav_pcie_phy *trav_phy;
	struct platform_device *pdev;

	pdev = to_platform_device(dev);
	trav_phy = platform_get_drvdata(pdev);
	trav_pcie_phy_update_phy_controller(trav_phy);
	return 0;
}

static const struct dev_pm_ops trav_pcie_phy_pm_ops = {
	.suspend      = trav_pcie_phy_suspend,
	.resume_noirq = trav_pcie_phy_resume
};

static struct platform_driver trav_pcie_phy_driver = {
	.probe = trav_pcie_phy_probe,
	.driver = {
		.of_match_table = trav_pcie_phy_match,
		.name = "trav_pcie_phy",
		.pm = &trav_pcie_phy_pm_ops,
	}
};

builtin_platform_driver(trav_pcie_phy_driver);
