/*
 * Samsung TRAV SoC series USB DRD PHY driver
 *
 * Phy provider for USB 3.0 DRD controller on Trav SoC series
 *
 * Copyright (C) 2018 Samsung Electronics Co., Ltd.
 * Author: Pankaj Dubey <pankaj.dubey@samsung.com>
 *
 * This file is based on phy-exnos5-usbdrd.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/regulator/consumer.h>

/* Exynos USB PHY registers */
#define TRAV_FSEL_9MHZ6		0x0
#define TRAV_FSEL_10MHZ		0x1
#define TRAV_FSEL_12MHZ		0x2
#define TRAV_FSEL_19MHZ2	0x3
#define TRAV_FSEL_20MHZ		0x4
#define TRAV_FSEL_24MHZ		0x5
#define TRAV_FSEL_50MHZ		0x7

/* TRAV: USB 3.0 DRD PHY registers */
#define TRAV_DRD_LINK_PME			0x04
#define TRAV_DRD_LINKSYSTEM			0x08

#define LINKSYSTEM_FLADJ_MASK			(0x3f << 1)
#define LINKSYSTEM_FLADJ(_x)			((_x) << 1)
#define LINKSYSTEM_XHCI_VERSION_CONTROL		BIT(27)

#define TRAV_DRD_PHYUTMI			0x0c

#define PHYUTMI_OTGDISABLE			BIT(6)
#define PHYUTMI_FORCESUSPEND			BIT(1)
#define PHYUTMI_FORCESLEEP			BIT(0)

#define TRAV_DRD_PHYPIPE			0x10

#define TRAV_DRD_PHYCLKRST			0x14

#define PHYCLKRST_EN_UTMISUSPEND		BIT(31)

#define PHYCLKRST_SSC_EN			BIT(20)
#define PHYCLKRST_REF_SSP_EN			BIT(19)
#define PHYCLKRST_REF_CLKDIV2			BIT(18)

#define PHYCLKRST_MPLL_MULTIPLIER_MASK		(0x7f << 11)
#define PHYCLKRST_MPLL_MULTIPLIER_100MHZ_REF	(0x19 << 11)
#define PHYCLKRST_MPLL_MULTIPLIER_50M_REF	(0x32 << 11)
#define PHYCLKRST_MPLL_MULTIPLIER_24MHZ_REF	(0x68 << 11)
#define PHYCLKRST_MPLL_MULTIPLIER_20MHZ_REF	(0x7d << 11)
#define PHYCLKRST_MPLL_MULTIPLIER_19200KHZ_REF	(0x02 << 11)

#define PHYCLKRST_FSEL_MASK			(0x1f << 5)
#define PHYCLKRST_FSEL(_x)			((_x) << 5)
#define PHYCLKRST_FSEL_PAD_100MHZ		(0x27 << 5)
#define PHYCLKRST_FSEL_PAD_24MHZ		(0x2a << 5)
#define PHYCLKRST_FSEL_PAD_20MHZ		(0x31 << 5)
#define PHYCLKRST_FSEL_PAD_19_2MHZ		(0x38 << 5)

#define PHYCLKRST_REFCLKSEL_MASK		(0x03 << 2)
#define PHYCLKRST_REFCLKSEL_PAD_REFCLK		(0x2 << 2)
#define PHYCLKRST_REFCLKSEL_EXT_REFCLK		(0x3 << 2)

#define PHYCLKRST_PORTRESET			BIT(1)
#define PHYCLKRST_COMMONONN			BIT(0)

#define TRAV_DRD_PHYREG0			0x18
#define TRAV_DRD_PHYREG1			0x1c

#define TRAV_DRD_PHYPARAM0			0x20

#define PHYPARAM0_REF_USE_PAD			BIT(31)
#define PHYPARAM0_REF_LOSLEVEL_MASK		(0x1f << 26)
#define PHYPARAM0_REF_LOSLEVEL			(0x9 << 26)

#define TRAV_DRD_PHYPARAM1			0x24

#define PHYPARAM1_PCS_TXDEEMPH_MASK		(0x1f << 0)
#define PHYPARAM1_PCS_TXDEEMPH			(0x1c)

#define TRAV_DRD_PHYTERM			0x28

#define TRAV_DRD_PHYTEST			0x2c

#define PHYTEST_POWERDOWN_SSP			BIT(3)
#define PHYTEST_POWERDOWN_HSP			BIT(2)

#define TRAV_DRD_PHYADP			0x30

#define TRAV_DRD_PHYBATCHG		0x34

#define TRAV_DRD_PHYRESUME			0x38
#define	PHYRESUME_FORCE_QACT			BIT(9)

#define TRAV_DRD_LINKPORT			0x48

#define TRAV_HOST_PHYCTRL0			0x58
#define PHYCTRL0_PHY1_SW_RST			BIT(0)
#define PHYCTRL0_SIDDQ_PORT			BIT(6)

#define KHZ	1000
#define MHZ	(KHZ * KHZ)

enum trav_usbdrd_phy_id {
	TRAV_DRDPHY_UTMI,
	TRAV_DRDPHYS_NUM,
};

struct phy_usb_instance;
struct trav_usbdrd_phy;

struct trav_usbdrd_phy_config {
	u32 id;
	void (*phy_init)(struct trav_usbdrd_phy *phy_drd);
	unsigned int (*set_refclk)(struct phy_usb_instance *inst);
};

struct trav_usbdrd_phy_drvdata {
	const struct trav_usbdrd_phy_config *phy_cfg;
};

/**
 * struct trav_usbdrd_phy - driver data for USB 3.0 PHY
 * @dev: pointer to device instance of this platform device
 * @reg_phy: usb phy controller register memory base
 * @clk: phy clock for register access
 * @drv_data: pointer to SoC level driver data structure
 * @phys[]: array for 'TRAV_DRDPHYS_NUM' number of PHY
 *	    instances each with its 'phy' and 'phy_cfg'.
 * @extrefclk: frequency select settings when using 'separate
 *	       reference clocks' for SS and HS operations
 * @ref_clk: reference clock to PHY block from which PHY's
 *	     operational clocks are derived
 * vbus: VBUS regulator for phy
 * vbus_boost: Boost regulator for VBUS present on few Exynos boards
 */
struct trav_usbdrd_phy {
	struct device *dev;
	void __iomem *reg_phy;
	struct clk *clk;
	const struct trav_usbdrd_phy_drvdata *drv_data;
	struct phy_usb_instance {
		struct phy *phy;
		u32 index;
		const struct trav_usbdrd_phy_config *phy_cfg;
	} phys[TRAV_DRDPHYS_NUM];
	u32 extrefclk;
	struct clk *ref_clk;
	struct regulator *vbus;
	struct regulator *vbus_boost;
};

static inline
struct trav_usbdrd_phy *to_usbdrd_phy(struct phy_usb_instance *inst)
{
	return container_of((inst), struct trav_usbdrd_phy,
			    phys[(inst)->index]);
}

/*
 * trav_rate_to_clk() converts the supplied clock rate to the value that
 * can be written to the phy register.
 */
static unsigned int trav_rate_to_clk(unsigned long rate, u32 *reg)
{
	/* TRAV_FSEL_MASK */

	switch (rate) {
	case 9600 * KHZ:
		*reg = TRAV_FSEL_9MHZ6;
		break;
	case 10 * MHZ:
		*reg = TRAV_FSEL_10MHZ;
		break;
	case 12 * MHZ:
		*reg = TRAV_FSEL_12MHZ;
		break;
	case 19200 * KHZ:
		*reg = TRAV_FSEL_19MHZ2;
		break;
	case 20 * MHZ:
		*reg = TRAV_FSEL_20MHZ;
		break;
	case 24 * MHZ:
		*reg = TRAV_FSEL_24MHZ;
		break;
	case 50 * MHZ:
		*reg = TRAV_FSEL_50MHZ;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

/*
 * Sets the utmi phy's clk as EXTREFCLK (XXTI) which is internal clock
 * from clock core. Further sets the FSEL values for HighSpeed operations.
 */
static unsigned int
trav_usbdrd_utmi_set_refclk(struct phy_usb_instance *inst)
{
	u32 reg;
	struct trav_usbdrd_phy *phy_drd = to_usbdrd_phy(inst);

	/* restore any previous reference clock settings */
	reg = readl(phy_drd->reg_phy + TRAV_DRD_PHYCLKRST);

	reg &= ~PHYCLKRST_REFCLKSEL_MASK;
	reg |=	PHYCLKRST_REFCLKSEL_PAD_REFCLK;

	reg &= ~PHYCLKRST_FSEL_MASK |
		PHYCLKRST_MPLL_MULTIPLIER_MASK;

	reg |= PHYCLKRST_FSEL(phy_drd->extrefclk);

	return reg;
}

static void trav_usbdrd_utmi_init(struct trav_usbdrd_phy *phy_drd)
{
	u32 reg;

	reg = readl(phy_drd->reg_phy + TRAV_DRD_PHYPARAM0);
	/* Set Loss-of-Signal Detector sensitivity */
	reg &= ~PHYPARAM0_REF_LOSLEVEL_MASK;
	reg |=	PHYPARAM0_REF_LOSLEVEL;
	writel(reg, phy_drd->reg_phy + TRAV_DRD_PHYPARAM0);

	reg = readl(phy_drd->reg_phy + TRAV_DRD_PHYPARAM1);
	/* Set Tx De-Emphasis level */
	reg &= ~PHYPARAM1_PCS_TXDEEMPH_MASK;
	reg |=	PHYPARAM1_PCS_TXDEEMPH;
	writel(reg, phy_drd->reg_phy + TRAV_DRD_PHYPARAM1);

	/* UTMI Power Control */
	writel(PHYUTMI_OTGDISABLE, phy_drd->reg_phy + TRAV_DRD_PHYUTMI);

	reg = readl(phy_drd->reg_phy + TRAV_DRD_PHYTEST);
	reg &= ~PHYTEST_POWERDOWN_HSP;
	writel(reg, phy_drd->reg_phy + TRAV_DRD_PHYTEST);
}

static int trav_usbdrd_phy_init(struct phy *phy)
{
	int ret;
	u32 reg;
	struct phy_usb_instance *inst = phy_get_drvdata(phy);
	struct trav_usbdrd_phy *phy_drd = to_usbdrd_phy(inst);

	ret = clk_prepare_enable(phy_drd->clk);
	if (ret)
		return ret;

	reg = readl(phy_drd->reg_phy + TRAV_DRD_PHYRESUME);
	reg |= PHYRESUME_FORCE_QACT;
	writel(reg, phy_drd->reg_phy + TRAV_DRD_PHYRESUME);

	reg = readl(phy_drd->reg_phy + TRAV_HOST_PHYCTRL0);
	reg |= PHYCTRL0_PHY1_SW_RST;
	writel(reg, phy_drd->reg_phy + TRAV_HOST_PHYCTRL0);

	reg = readl(phy_drd->reg_phy + TRAV_HOST_PHYCTRL0);
	reg &= ~PHYCTRL0_SIDDQ_PORT;
	writel(reg, phy_drd->reg_phy + TRAV_HOST_PHYCTRL0);

	reg = readl(phy_drd->reg_phy + TRAV_HOST_PHYCTRL0);
	reg &= ~PHYCTRL0_PHY1_SW_RST;
	writel(reg, phy_drd->reg_phy + TRAV_HOST_PHYCTRL0);

	/*
	 * Setting the Frame length Adj value[6:1] to default 0x20
	 * See xHCI 1.0 spec, 5.2.4
	 */
	reg =	LINKSYSTEM_XHCI_VERSION_CONTROL |
		LINKSYSTEM_FLADJ(0x20);
	writel(reg, phy_drd->reg_phy + TRAV_DRD_LINKSYSTEM);

	reg = readl(phy_drd->reg_phy + TRAV_DRD_PHYPARAM0);
	/* Select PHY CLK source */
	reg &= ~PHYPARAM0_REF_USE_PAD;
	writel(reg, phy_drd->reg_phy + TRAV_DRD_PHYPARAM0);

	/* UTMI specific init */
	inst->phy_cfg->phy_init(phy_drd);

	/* reference clock settings */
	reg = inst->phy_cfg->set_refclk(inst);

		/* Digital power supply in normal operating mode */
		/* Enable ref clock for SS function */
	reg |= PHYCLKRST_REF_SSP_EN |
		/* Enable spread spectrum */
		PHYCLKRST_SSC_EN |
		/* Power down HS Bias and PLL blocks in suspend mode */
		PHYCLKRST_COMMONONN |
		/* Reset the port */
		PHYCLKRST_PORTRESET;

	writel(reg, phy_drd->reg_phy + TRAV_DRD_PHYCLKRST);

	udelay(10);

	reg &= ~PHYCLKRST_PORTRESET;
	writel(reg, phy_drd->reg_phy + TRAV_DRD_PHYCLKRST);

	clk_disable_unprepare(phy_drd->clk);

	return 0;
}

static int trav_usbdrd_phy_exit(struct phy *phy)
{
	int ret;
	u32 reg;
	struct phy_usb_instance *inst = phy_get_drvdata(phy);
	struct trav_usbdrd_phy *phy_drd = to_usbdrd_phy(inst);

	ret = clk_prepare_enable(phy_drd->clk);
	if (ret)
		return ret;

	reg =	PHYUTMI_OTGDISABLE |
		PHYUTMI_FORCESUSPEND |
		PHYUTMI_FORCESLEEP;
	writel(reg, phy_drd->reg_phy + TRAV_DRD_PHYUTMI);

	/* Resetting the PHYCLKRST enable bits to reduce leakage current */
	reg = readl(phy_drd->reg_phy + TRAV_DRD_PHYCLKRST);
	reg &= ~(PHYCLKRST_REF_SSP_EN |
		 PHYCLKRST_SSC_EN |
		 PHYCLKRST_COMMONONN);
	writel(reg, phy_drd->reg_phy + TRAV_DRD_PHYCLKRST);

	/* Control PHYTEST to remove leakage current */
	reg = readl(phy_drd->reg_phy + TRAV_DRD_PHYTEST);
	reg |=	PHYTEST_POWERDOWN_SSP |
		PHYTEST_POWERDOWN_HSP;
	writel(reg, phy_drd->reg_phy + TRAV_DRD_PHYTEST);

	clk_disable_unprepare(phy_drd->clk);

	return 0;
}

static int trav_usbdrd_phy_power_on(struct phy *phy)
{
	int ret;
	struct phy_usb_instance *inst = phy_get_drvdata(phy);
	struct trav_usbdrd_phy *phy_drd = to_usbdrd_phy(inst);

	dev_err(phy_drd->dev, "Request to power_on usbdrd_phy phy\n");

	clk_prepare_enable(phy_drd->ref_clk);

	/* Enable VBUS supply */
	if (phy_drd->vbus_boost) {
		ret = regulator_enable(phy_drd->vbus_boost);
		if (ret) {
			dev_err(phy_drd->dev,
				"Failed to enable VBUS boost supply\n");
			goto fail_vbus;
		}
	}

	if (phy_drd->vbus) {
		ret = regulator_enable(phy_drd->vbus);
		if (ret) {
			dev_err(phy_drd->dev, "Failed to enable VBUS supply\n");
			goto fail_vbus_boost;
		}
	}

	return 0;

fail_vbus_boost:
	if (phy_drd->vbus_boost)
		regulator_disable(phy_drd->vbus_boost);

fail_vbus:
	clk_disable_unprepare(phy_drd->ref_clk);

	return ret;
}

static int trav_usbdrd_phy_power_off(struct phy *phy)
{
	struct phy_usb_instance *inst = phy_get_drvdata(phy);
	struct trav_usbdrd_phy *phy_drd = to_usbdrd_phy(inst);

	dev_dbg(phy_drd->dev, "Request to power_off usbdrd_phy phy\n");

	/* Disable VBUS supply */
	if (phy_drd->vbus)
		regulator_disable(phy_drd->vbus);
	if (phy_drd->vbus_boost)
		regulator_disable(phy_drd->vbus_boost);

	clk_disable_unprepare(phy_drd->ref_clk);

	return 0;
}

static struct phy *trav_usbdrd_phy_xlate(struct device *dev,
					struct of_phandle_args *args)
{
	struct trav_usbdrd_phy *phy_drd = dev_get_drvdata(dev);

	if (WARN_ON(args->args[0] >= TRAV_DRDPHYS_NUM))
		return ERR_PTR(-ENODEV);

	return phy_drd->phys[args->args[0]].phy;
}

static const struct phy_ops trav_usbdrd_phy_ops = {
	.init		= trav_usbdrd_phy_init,
	.exit		= trav_usbdrd_phy_exit,
	.power_on	= trav_usbdrd_phy_power_on,
	.power_off	= trav_usbdrd_phy_power_off,
	.owner		= THIS_MODULE,
};

static int trav_usbdrd_phy_clk_handle(struct trav_usbdrd_phy *phy_drd)
{
	unsigned long ref_rate;
	int ret;

	phy_drd->clk = devm_clk_get(phy_drd->dev, "phy");
	if (IS_ERR(phy_drd->clk)) {
		dev_err(phy_drd->dev, "Failed to get phy clock\n");
		return PTR_ERR(phy_drd->clk);
	}

	phy_drd->ref_clk = devm_clk_get(phy_drd->dev, "ref");
	if (IS_ERR(phy_drd->ref_clk)) {
		dev_err(phy_drd->dev, "Failed to get phy reference clock\n");
		return PTR_ERR(phy_drd->ref_clk);
	}
	ref_rate = clk_get_rate(phy_drd->ref_clk);

	ret = trav_rate_to_clk(ref_rate, &phy_drd->extrefclk);
	if (ret) {
		dev_err(phy_drd->dev, "Clock rate (%ld) not supported\n",
			ref_rate);
		return ret;
	}

	return 0;
}

static const struct trav_usbdrd_phy_config phy_cfg_trav[] = {
	{
		.id		= TRAV_DRDPHY_UTMI,
		.phy_init	= trav_usbdrd_utmi_init,
		.set_refclk	= trav_usbdrd_utmi_set_refclk,
	},
};

static const struct trav_usbdrd_phy_drvdata trav_usbdrd_phy = {
	.phy_cfg		= phy_cfg_trav,
};

static const struct of_device_id trav_usbdrd_phy_of_match[] = {
	{
		.compatible = "samsung,trav-usbdrd-phy",
		.data = &trav_usbdrd_phy
	},
	{ },
};
MODULE_DEVICE_TABLE(of, trav_usbdrd_phy_of_match);

static int trav_usbdrd_phy_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct trav_usbdrd_phy *phy_drd;
	struct phy_provider *phy_provider;
	struct resource *res;
	const struct of_device_id *match;
	const struct trav_usbdrd_phy_drvdata *drv_data;
	int i, ret;

	phy_drd = devm_kzalloc(dev, sizeof(*phy_drd), GFP_KERNEL);
	if (!phy_drd)
		return -ENOMEM;

	dev_set_drvdata(dev, phy_drd);
	phy_drd->dev = dev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	phy_drd->reg_phy = devm_ioremap_resource(dev, res);
	if (IS_ERR(phy_drd->reg_phy))
		return PTR_ERR(phy_drd->reg_phy);

	match = of_match_node(trav_usbdrd_phy_of_match, pdev->dev.of_node);

	drv_data = match->data;
	phy_drd->drv_data = drv_data;

	ret = trav_usbdrd_phy_clk_handle(phy_drd);
	if (ret) {
		dev_err(dev, "Failed to initialize clocks\n");
		return ret;
	}

	/* Get Vbus regulators */
	phy_drd->vbus = devm_regulator_get(dev, "vbus");
	if (IS_ERR(phy_drd->vbus)) {
		ret = PTR_ERR(phy_drd->vbus);
		if (ret == -EPROBE_DEFER)
			return ret;

		dev_warn(dev, "Failed to get VBUS supply regulator\n");
		phy_drd->vbus = NULL;
	}

	phy_drd->vbus_boost = devm_regulator_get(dev, "vbus-boost");
	if (IS_ERR(phy_drd->vbus_boost)) {
		ret = PTR_ERR(phy_drd->vbus_boost);
		if (ret == -EPROBE_DEFER)
			return ret;

		dev_warn(dev, "Failed to get VBUS boost supply regulator\n");
		phy_drd->vbus_boost = NULL;
	}

	dev_vdbg(dev, "Creating usbdrd_phy phy\n");

	for (i = 0; i < TRAV_DRDPHYS_NUM; i++) {
		struct phy *phy = devm_phy_create(dev, NULL,
						  &trav_usbdrd_phy_ops);
		if (IS_ERR(phy)) {
			dev_err(dev, "Failed to create usbdrd_phy phy\n");
			return PTR_ERR(phy);
		}

		phy_drd->phys[i].phy = phy;
		phy_drd->phys[i].index = i;
		phy_drd->phys[i].phy_cfg = &drv_data->phy_cfg[i];
		phy_set_drvdata(phy, &phy_drd->phys[i]);
	}

	phy_provider = devm_of_phy_provider_register(dev,
						     trav_usbdrd_phy_xlate);
	if (IS_ERR(phy_provider)) {
		dev_err(phy_drd->dev, "Failed to register phy provider\n");
		return PTR_ERR(phy_provider);
	}

	return 0;
}

static struct platform_driver trav_usb3drd_phy = {
	.probe	= trav_usbdrd_phy_probe,
	.driver = {
		.of_match_table	= trav_usbdrd_phy_of_match,
		.name		= "trav_usb3drd_phy",
	}
};

module_platform_driver(trav_usb3drd_phy);
MODULE_DESCRIPTION("Samsung TRAV SoCs USB 3.0 DRD controller PHY driver");
MODULE_AUTHOR("Pankaj Dubey <pankaj.dubey@samsung.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:trav_usb3drd_phy");
