/*
 * PCIe host controller driver for Samsung TRAV SoCs
 *
 * Copyright (C) 2017 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Author: Pankaj Dubey <pankaj.dubey@samsung.com>
 * This file is based on driver/pci/dwc/pci-exynos.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/mfd/syscon.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/pci.h>
#include <linux/platform_device.h>
#include <linux/phy/phy.h>
#include <linux/regmap.h>
#include <linux/resource.h>
#include <linux/signal.h>
#include <linux/types.h>

#include "pcie-designware.h"

#define to_trav_pcie(x)	dev_get_drvdata((x)->dev)

/* PCIe ELBI registers */
#define PCIE_IRQ0_STS			0x000
#define PCIE_IRQ1_STS			0x004
#define PCIE_IRQ2_STS			0x008
#define PCIE_IRQ5_STS			0x00C
#define PCIE_IRQ0_EN			0x010
#define PCIE_IRQ1_EN			0x014
#define PCIE_IRQ2_EN			0x018
#define PCIE_IRQ5_EN			0x01C
#define IRQ_MSI_ENABLE			BIT(17)
#define PCIE_APP_LTSSM_ENABLE		0x054
#define PCIE_ELBI_LTSSM_ENABLE		0x1
#define PCIE_ELBI_LTSSM_DISABLE		0x0
#define PCIE_ELBI_CXPL_DEBUG_00_31	0x2C8
#define PCIE_ELBI_CXPL_DEBUG_32_63	0x2CC
#define PCIE_ELBI_SMLH_LINK_UP		BIT(4)
#define PCIE_TRAV_DEVICE_TYPE		0x080
#define DEVICE_TYPE_EP			0x0
#define DEVICE_TYPE_LEG_EP		0x1
#define DEVICE_TYPE_RC			0x4
#define PCIE_ELBI_SLV_AWMISC		0x828
#define PCIE_ELBI_SLV_ARMISC		0x820
#define PCIE_ELBI_SLV_DBI_ENABLE	BIT(21)
#define LTSSM_STATE_MASK		0x3f
#define LTSSM_STATE_L0			0x11

/* FSYS SYSREG Offsets */
#define FSYS0_PCIE_DBI_ADDR_CON		0x434
#define FSYS1_PCIE_DBI_ADDR_CON_0	0x50C
#define FSYS1_PCIE_DBI_ADDR_CON_1	0x510

int trav_pcie_dbi_addr_con[] = {
	FSYS1_PCIE_DBI_ADDR_CON_0,
	FSYS1_PCIE_DBI_ADDR_CON_1,
	FSYS0_PCIE_DBI_ADDR_CON
};

#define MAX_TRAV_PCI 3
#define MAX_ADDR_TYPES 2
int trav_pcie_dbi_val_con[MAX_TRAV_PCI][MAX_ADDR_TYPES] = {
	{0x6, 0x7}, {0x6, 0x7}, {0x36, 0x3f}
};

struct trav_pcie {
	struct dw_pcie			*pci;
	struct clk			*aux_clk;
	struct clk			*dbi_clk;
	struct clk			*mstr_clk;
	struct clk			*slv_clk;
	const struct trav_pcie_pdata	*pdata;

	void __iomem *elbi_base; /* DT 0th resource: PCIe CTRL */
	struct regmap *sysreg;

	enum dw_pcie_device_mode	mode;
	int link_id;

	/* For Generic PHY Framework */
	struct phy			*phy;
	bool				phy_initialized;
};

struct trav_pcie_res_ops {
	int (*get_mem_resources)(struct platform_device *pdev,
				 struct trav_pcie *trav_ctrl);
	int (*get_clk_resources)(struct platform_device *pdev,
				 struct trav_pcie *trav_ctrl);
	int (*init_clk_resources)(struct trav_pcie *trav_ctrl);
	void (*deinit_clk_resources)(struct trav_pcie *trav_ctrl);
};

struct trav_pcie_pdata {
	const struct dw_pcie_ops	*dwc_ops;
	struct dw_pcie_host_ops		*host_ops;
	const struct trav_pcie_res_ops	*res_ops;
	enum dw_pcie_device_mode	mode;
};

static void trav_pcie_writel(void __iomem *base, u32 val, u32 reg);

static int trav_pcie_get_rc_mem_resources(struct platform_device *pdev,
						struct trav_pcie *trav_ctrl)
{
	struct dw_pcie *pci = trav_ctrl->pci;
	struct device *dev = pci->dev;
	struct resource *res;

	/* External Local Bus interface(ELBI) Register */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "elbi");
	trav_ctrl->elbi_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(trav_ctrl->elbi_base)) {
		dev_err(dev, "failed to map elbi_base\n");
		return PTR_ERR(trav_ctrl->elbi_base);
	}

	/* Data Bus Interface(DBI) Register */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "dbi");
	pci->dbi_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(pci->dbi_base)) {
		dev_err(dev, "failed to map dbi_base\n");
		return PTR_ERR(pci->dbi_base);
	}

	/* fsys0 sysreg regmap handle */
	trav_ctrl->sysreg = syscon_regmap_lookup_by_phandle(dev->of_node,
			"samsung,fsys-sysreg");
	if (IS_ERR(trav_ctrl->sysreg)) {
		dev_err(dev, "fsys sysreg regmap lookup failed.\n");
		return PTR_ERR(trav_ctrl->sysreg);
	}

	return 0;
}

static int trav_pcie_get_ep_mem_resources(struct platform_device *pdev,
					  struct trav_pcie *trav_ctrl)
{
	struct dw_pcie_ep *ep;
	struct dw_pcie *pci = trav_ctrl->pci;
	struct device *dev = &pdev->dev;
	struct resource *res;

	ep = &pci->ep;

	/* External Local Bus interface(ELBI) Register */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "elbi");
	trav_ctrl->elbi_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(trav_ctrl->elbi_base)) {
		dev_err(dev, "failed to map elbi_base\n");
		return PTR_ERR(trav_ctrl->elbi_base);
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ep_dbics");
	pci->dbi_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(pci->dbi_base)) {
		dev_err(dev, "failed to map ep_dbics\n");
		return -ENOMEM;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ep_dbics2");
	pci->dbi_base2 = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(pci->dbi_base2)) {
		dev_err(dev, "failed to map ep_dbics2\n");
		return -ENOMEM;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "addr_space");
	if (!res)
		return -EINVAL;
	ep->phys_base = res->start;
	ep->addr_size = resource_size(res);

	/* fsys0/1 sysreg regmap handle */
	trav_ctrl->sysreg = syscon_regmap_lookup_by_phandle(dev->of_node,
			"samsung,fsys-sysreg");
	if (IS_ERR(trav_ctrl->sysreg)) {
		dev_err(dev, "fsys sysreg regmap lookup failed.\n");
		return PTR_ERR(trav_ctrl->sysreg);
	}

	return 0;
}

static int trav_pcie_get_clk_resources(struct platform_device *pdev,
				       struct trav_pcie *trav_ctrl)
{
	struct device *dev = &pdev->dev;

	trav_ctrl->aux_clk = devm_clk_get(dev, "aux_clk");
	if (IS_ERR(trav_ctrl->aux_clk)) {
		dev_err(dev, "couldn't get aux clock\n");
		return -EINVAL;
	}

	trav_ctrl->dbi_clk = devm_clk_get(dev, "dbi_clk");
	if (IS_ERR(trav_ctrl->dbi_clk)) {
		dev_info(dev, "couldn't get dbi clk\n");
		return -EINVAL;
	}

	trav_ctrl->slv_clk = devm_clk_get(dev, "slv_clk");
	if (IS_ERR(trav_ctrl->slv_clk)) {
		dev_err(dev, "couldn't get slave clock\n");
		return -EINVAL;
	}

	trav_ctrl->mstr_clk = devm_clk_get(dev, "mstr_clk");
	if (IS_ERR(trav_ctrl->mstr_clk)) {
		dev_info(dev, "couldn't get master clk\n");
		return -EINVAL;
	}

	return 0;
}

static int trav_pcie_init_clk_resources(struct trav_pcie *trav_ctrl)
{
	clk_prepare_enable(trav_ctrl->aux_clk);
	clk_prepare_enable(trav_ctrl->dbi_clk);
	clk_prepare_enable(trav_ctrl->mstr_clk);
	clk_prepare_enable(trav_ctrl->slv_clk);

	return 0;
}

static void trav_pcie_deinit_clk_resources(struct trav_pcie *trav_ctrl)
{
	clk_disable_unprepare(trav_ctrl->slv_clk);
	clk_disable_unprepare(trav_ctrl->mstr_clk);
	clk_disable_unprepare(trav_ctrl->dbi_clk);
	clk_disable_unprepare(trav_ctrl->aux_clk);
}

static const struct trav_pcie_res_ops trav_pcie_rc_res_ops = {
	.get_mem_resources	= trav_pcie_get_rc_mem_resources,
	.get_clk_resources	= trav_pcie_get_clk_resources,
	.init_clk_resources	= trav_pcie_init_clk_resources,
	.deinit_clk_resources	= trav_pcie_deinit_clk_resources,
};

static const struct trav_pcie_res_ops trav_pcie_ep_res_ops = {
	.get_mem_resources	= trav_pcie_get_ep_mem_resources,
	.get_clk_resources	= trav_pcie_get_clk_resources,
	.init_clk_resources	= trav_pcie_init_clk_resources,
	.deinit_clk_resources	= trav_pcie_deinit_clk_resources,
};

static void trav_pcie_writel(void __iomem *base, u32 val, u32 reg)
{
	writel(val, base + reg);
}

static u32 trav_pcie_readl(void __iomem *base, u32 reg)
{
	return readl(base + reg);
}

static void trav_pcie_sideband_dbi_w_mode(struct trav_pcie *trav_ctrl, bool on)
{
	u32 val;

	val = trav_pcie_readl(trav_ctrl->elbi_base, PCIE_ELBI_SLV_AWMISC);
	if (on)
		val |= PCIE_ELBI_SLV_DBI_ENABLE;
	else
		val &= ~PCIE_ELBI_SLV_DBI_ENABLE;
	trav_pcie_writel(trav_ctrl->elbi_base, val, PCIE_ELBI_SLV_AWMISC);
}

static void trav_pcie_sideband_dbi_r_mode(struct trav_pcie *trav_ctrl, bool on)
{
	u32 val;

	val = trav_pcie_readl(trav_ctrl->elbi_base, PCIE_ELBI_SLV_ARMISC);
	if (on)
		val |= PCIE_ELBI_SLV_DBI_ENABLE;
	else
		val &= ~PCIE_ELBI_SLV_DBI_ENABLE;
	trav_pcie_writel(trav_ctrl->elbi_base, val, PCIE_ELBI_SLV_ARMISC);
}

static void trav_pcie_stop_link(struct dw_pcie *pci)
{
	struct trav_pcie *trav_ctrl = to_trav_pcie(pci);
	u32 reg;

	reg = trav_pcie_readl(trav_ctrl->elbi_base, PCIE_ELBI_LTSSM_ENABLE);
	reg &= ~PCIE_APP_LTSSM_ENABLE;
	trav_pcie_writel(trav_ctrl->elbi_base, PCIE_ELBI_LTSSM_ENABLE,
			PCIE_APP_LTSSM_ENABLE);
}

static void trav_pcie_phy_exit(struct trav_pcie *trav_ctrl)
{
	if (trav_ctrl->phy_initialized) {
		phy_exit(trav_ctrl->phy);
		trav_ctrl->phy_initialized = false;
	}
}

static void trav_pcie_phy_init(struct trav_pcie *trav_ctrl)
{
	if (!trav_ctrl->phy_initialized) {
		phy_reset(trav_ctrl->phy);
		phy_init(trav_ctrl->phy);
		trav_ctrl->phy_initialized = true;
	}
}

static int trav_pcie_establish_link(struct dw_pcie *pci)
{
	struct trav_pcie *trav_ctrl = to_trav_pcie(pci);
	struct device *dev = pci->dev;
	struct dw_pcie_ep *ep;
	struct pci_epc *epc;
	unsigned long flags;

	if (dw_pcie_link_up(pci)) {
		dev_err(dev, "Link already up\n");
		return 0;
	}

	trav_pcie_phy_init(trav_ctrl);

	/* assert LTSSM enable */
	trav_pcie_writel(trav_ctrl->elbi_base, PCIE_ELBI_LTSSM_ENABLE,
			PCIE_APP_LTSSM_ENABLE);

	/* check if the link is up or not */
	if (!dw_pcie_wait_for_link(pci)) {
		dev_info(dev, "Link up done successfully\n");
		if (trav_ctrl->mode == DW_PCIE_EP_TYPE) {
			ep = &pci->ep;
			epc = ep->epc;
			spin_unlock_irqrestore(&epc->lock, flags);
			dw_pcie_ep_linkup(ep);
			spin_lock_irqsave(&epc->lock, flags);
		}
		return 0;
	}

	trav_pcie_phy_exit(trav_ctrl);
	return -ETIMEDOUT;
}

static void trav_pcie_msi_init(struct trav_pcie *trav_ctrl)
{
	struct dw_pcie *pci = trav_ctrl->pci;
	struct pcie_port *pp = &pci->pp;
	u32 val;

	dw_pcie_msi_init(pp);

	/* enable MSI interrupt */
	val = trav_pcie_readl(trav_ctrl->elbi_base, PCIE_IRQ2_EN);
	val |= IRQ_MSI_ENABLE;
	trav_pcie_writel(trav_ctrl->elbi_base, val, PCIE_IRQ2_EN);
}

static void trav_pcie_enable_interrupts(struct trav_pcie *trav_ctrl)
{
	if (IS_ENABLED(CONFIG_PCI_MSI))
		trav_pcie_msi_init(trav_ctrl);
}

static u32 trav_pcie_read_dbi(struct dw_pcie *pci, void __iomem *base,
				u32 reg, size_t size)
{
	struct trav_pcie *trav_ctrl = to_trav_pcie(pci);
	bool is_atu = false, is_dma = false;
	u32 val;

	if ((reg >> PCIE_REG_ADDR_TYPE_LOC) == ADDR_TYPE_ATU)
		is_atu = true;
	else if ((reg >> PCIE_REG_ADDR_TYPE_LOC) == ADDR_TYPE_DMA)
		is_dma = true;

	if (is_atu || is_dma) {
		reg = reg & 0xFFFF;
		regmap_write(trav_ctrl->sysreg,
				trav_pcie_dbi_addr_con[trav_ctrl->link_id],
				trav_pcie_dbi_val_con[trav_ctrl->link_id][is_dma]);
	}

	dw_pcie_read(base + reg, size, &val);

	if (is_atu || is_dma)
		regmap_write(trav_ctrl->sysreg,
				trav_pcie_dbi_addr_con[trav_ctrl->link_id],
				0x00);

	return val;
}

static void trav_pcie_write_dbi(struct dw_pcie *pci, void __iomem *base,
				u32 reg, size_t size, u32 val)
{
	struct trav_pcie *trav_ctrl = to_trav_pcie(pci);
	bool is_atu = false, is_dma = false;

	if ((reg >> PCIE_REG_ADDR_TYPE_LOC) == ADDR_TYPE_ATU)
		is_atu = true;
	else if ((reg >> PCIE_REG_ADDR_TYPE_LOC) == ADDR_TYPE_DMA)
		is_dma = true;

	if (is_atu || is_dma) {
		reg = reg & 0xFFFF;
		regmap_write(trav_ctrl->sysreg,
				trav_pcie_dbi_addr_con[trav_ctrl->link_id],
				trav_pcie_dbi_val_con[trav_ctrl->link_id][is_dma]);
	}

	dw_pcie_write(base + reg, size, val);

	if (is_atu || is_dma)
		regmap_write(trav_ctrl->sysreg,
				trav_pcie_dbi_addr_con[trav_ctrl->link_id],
				0x00);
}

static void trav_pcie_msi_handler(struct pcie_port *pp)
{
	int val;
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct trav_pcie *trav_ctrl = to_trav_pcie(pci);
	//pr_err("*");

	val = trav_pcie_readl(trav_ctrl->elbi_base, PCIE_IRQ2_STS);
	trav_pcie_writel(trav_ctrl->elbi_base, val, PCIE_IRQ2_STS);
}

static int trav_pcie_rd_own_conf(struct pcie_port *pp, int where, int size,
				u32 *val)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct trav_pcie *trav_ctrl = to_trav_pcie(pci);
	int ret;

	trav_pcie_sideband_dbi_r_mode(trav_ctrl, true);
	ret = dw_pcie_read(pci->dbi_base + where, size, val);
	trav_pcie_sideband_dbi_r_mode(trav_ctrl, false);
	return ret;
}

static int trav_pcie_wr_own_conf(struct pcie_port *pp, int where, int size,
				u32 val)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct trav_pcie *trav_ctrl = to_trav_pcie(pci);
	int ret;

	trav_pcie_sideband_dbi_w_mode(trav_ctrl, true);
	ret = dw_pcie_write(pci->dbi_base + where, size, val);
	trav_pcie_sideband_dbi_w_mode(trav_ctrl, false);
	return ret;
}

static int trav_pcie_link_up(struct dw_pcie *pci)
{
	struct trav_pcie *trav_ctrl = to_trav_pcie(pci);
	u32 val;

	val = trav_pcie_readl(trav_ctrl->elbi_base,
			PCIE_ELBI_CXPL_DEBUG_00_31);

	return (val & LTSSM_STATE_MASK) == LTSSM_STATE_L0;
}

static int trav_pcie_host_init(struct pcie_port *pp)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct trav_pcie *trav_ctrl = to_trav_pcie(pci);

	dw_pcie_writel_dbi(pci, PCIE_GEN3_RELATED_OFF,
				(PCIE_GEN3_EQ_PHASE_2_3 |
				 PCIE_GEN3_RXEQ_PH01_EN |
				 PCIE_GEN3_RXEQ_RGRDLESS_RXTS));

	dw_pcie_setup_rc(pp);

	/*
	 * Current PCIe fsys0 SI settings are not well tuned for GEN3 speed,
	 * when connected to 10G NIC using M.2 to PCIe extension cable.
	 * For now, limit that link to GEN2.
	 */
#define TRAV_PCIE_FSYS0_ID		2
#define PCIE_CONF_LINK_CTL2		0xA0
#define LINK_CTL2_SPEED_MASK 		0xF
#define LINK_CTL2_MAX_SPEED_GEN2	0x2
	if (trav_ctrl->link_id == TRAV_PCIE_FSYS0_ID) {
		u32 val;

		val = dw_pcie_readl_dbi(pci, PCIE_CONF_LINK_CTL2);
		val &= ~LINK_CTL2_SPEED_MASK;
		val |= LINK_CTL2_MAX_SPEED_GEN2;
		dw_pcie_writel_dbi(pci, PCIE_CONF_LINK_CTL2, val);
	}

	trav_pcie_establish_link(pci);
	trav_pcie_enable_interrupts(trav_ctrl);

	return 0;
}

static struct dw_pcie_host_ops trav_pcie_host_ops = {
	.rd_own_conf = trav_pcie_rd_own_conf,
	.wr_own_conf = trav_pcie_wr_own_conf,
	.host_init = trav_pcie_host_init,
	.msi_handler = trav_pcie_msi_handler,
};

static void trav_pcie_ep_init(struct dw_pcie_ep *ep)
{
	struct dw_pcie *pci = to_dw_pcie_from_ep(ep);
	/* Currently PCIe EP core is not setting iatu_unroll_enabled
	 * so let's handle it here. We need to find proper place to
	 * initialize this so that it can be used as for other EP
	 * controllers as well.
	 */
	pci->iatu_unroll_enabled = dw_pcie_iatu_unroll_enabled(pci);
}

static int trav_pcie_raise_irq(struct dw_pcie_ep *ep, u8 func_no,
				 enum pci_epc_irq_type type, u8 interrupt_num)
{
	struct dw_pcie *pci = to_dw_pcie_from_ep(ep);

	switch (type) {
	case PCI_EPC_IRQ_LEGACY:
		dev_err(pci->dev, "EP does not support legacy IRQs\n");
		return -EINVAL;
	case PCI_EPC_IRQ_MSI:
		return dw_pcie_ep_raise_msi_irq(ep, func_no, interrupt_num);
	default:
		dev_err(pci->dev, "UNKNOWN IRQ type\n");
	}

	return 0;
}

static void trav_pcie_ep_set_mask(struct dw_pcie_ep *ep, u32 reg, u32 mask)
{
	struct dw_pcie *pci = to_dw_pcie_from_ep(ep);
	struct trav_pcie *trav_ctrl = to_trav_pcie(pci);

	regmap_write(trav_ctrl->sysreg,
		     trav_pcie_dbi_addr_con[trav_ctrl->link_id], 0x32);

	dw_pcie_write(pci->dbi_base + reg, 4, mask);

	regmap_write(trav_ctrl->sysreg,
		     trav_pcie_dbi_addr_con[trav_ctrl->link_id], 0x00);
}

static struct dw_pcie_ep_ops trav_dw_pcie_ep_ops = {
	.ep_set_mask	= trav_pcie_ep_set_mask,
	.ep_init	= trav_pcie_ep_init,
	.raise_irq	= trav_pcie_raise_irq,
};

static int __init trav_add_pcie_ep(struct trav_pcie *trav_ctrl,
		struct platform_device *pdev)
{
	int ret;
	struct dw_pcie_ep *ep;
	struct device *dev = &pdev->dev;
	struct dw_pcie *pci = trav_ctrl->pci;

	ep = &pci->ep;
	ep->ops = &trav_dw_pcie_ep_ops;

	dw_pcie_writel_dbi(pci, PCIE_GEN3_RELATED_OFF,
				(PCIE_GEN3_EQ_PHASE_2_3 |
				 PCIE_GEN3_RXEQ_PH01_EN |
				 PCIE_GEN3_RXEQ_RGRDLESS_RXTS));

	ret = dw_pcie_ep_init(ep);
	if (ret) {
		dev_err(dev, "failed to initialize endpoint\n");
		return ret;
	}

	return 0;
}

static int __init trav_add_pcie_port(struct trav_pcie *trav_ctrl,
					struct platform_device *pdev)
{
	struct dw_pcie *pci = trav_ctrl->pci;
	struct pcie_port *pp = &pci->pp;
	struct device *dev = &pdev->dev;
	int ret;

	if (IS_ENABLED(CONFIG_PCI_MSI)) {
		pp->msi_irq = platform_get_irq_byname(pdev, "intr");
		if (!pp->msi_irq) {
			dev_err(dev, "failed to get msi irq\n");
			return -ENODEV;
		}
	}

	ret = dw_pcie_host_init(pp);
	if (ret) {
		dev_err(dev, "failed to initialize host\n");
		return ret;
	}

	return 0;
}

static const struct dw_pcie_ops trav_dw_pcie_ops = {
	.read_dbi	= trav_pcie_read_dbi,
	.write_dbi	= trav_pcie_write_dbi,
	.start_link	= trav_pcie_establish_link,
	.stop_link	= trav_pcie_stop_link,
	.link_up	= trav_pcie_link_up,
};

static int trav_pcie_probe(struct platform_device *pdev)
{
	int ret;
	struct dw_pcie *pci;
	struct pcie_port *pp;
	struct trav_pcie *trav_ctrl;
	enum dw_pcie_device_mode mode;
	struct device *dev = &pdev->dev;
	const struct trav_pcie_pdata *pdata;
	struct device_node *np = dev->of_node;

	trav_ctrl = devm_kzalloc(dev, sizeof(*trav_ctrl), GFP_KERNEL);
	if (!trav_ctrl)
		return -ENOMEM;

	pci = devm_kzalloc(dev, sizeof(*pci), GFP_KERNEL);
	if (!pci)
		return -ENOMEM;

	pdata = (const struct trav_pcie_pdata *)
		of_device_get_match_data(dev);
	mode = (enum dw_pcie_device_mode)pdata->mode;

	trav_ctrl->pci = pci;
	trav_ctrl->pdata = pdata;
	trav_ctrl->mode = mode;

	pci->dev = dev;
	pci->ops = pdata->dwc_ops;
	pci->dbi_base2 = NULL;
	pci->dbi_base = NULL;
	pp = &pci->pp;
	pp->ops = trav_ctrl->pdata->host_ops;

	if (mode == DW_PCIE_RC_TYPE)
		trav_ctrl->link_id = of_alias_get_id(np, "pcierc");
	else
		trav_ctrl->link_id = of_alias_get_id(np, "pcieep");

	trav_ctrl->phy = devm_of_phy_get(dev, np, NULL);
	if (IS_ERR(trav_ctrl->phy)) {
		if (PTR_ERR(trav_ctrl->phy) == -EPROBE_DEFER)
			return PTR_ERR(trav_ctrl->phy);
	}

	if (pdata->res_ops && pdata->res_ops->get_mem_resources) {
		ret = pdata->res_ops->get_mem_resources(pdev, trav_ctrl);
		if (ret)
			return ret;
	}

	if (pdata->res_ops && pdata->res_ops->get_clk_resources) {
		ret = pdata->res_ops->get_clk_resources(pdev, trav_ctrl);
		if (ret)
			return ret;
		ret = pdata->res_ops->init_clk_resources(trav_ctrl);
		if (ret)
			return ret;
	}

	platform_set_drvdata(pdev, trav_ctrl);

	ret = dma_set_mask_and_coherent(dev, DMA_BIT_MASK(36));
	if(ret)
		goto fail_dma_set;

	switch (mode) {
	case DW_PCIE_RC_TYPE:
		trav_pcie_writel(trav_ctrl->elbi_base, DEVICE_TYPE_RC,
				PCIE_TRAV_DEVICE_TYPE);
		ret = trav_add_pcie_port(trav_ctrl, pdev);
		if (ret < 0)
			goto fail_probe;
		break;
	case DW_PCIE_EP_TYPE:
		trav_pcie_writel(trav_ctrl->elbi_base, DEVICE_TYPE_EP,
				PCIE_TRAV_DEVICE_TYPE);

		ret = trav_add_pcie_ep(trav_ctrl, pdev);
		if (ret < 0)
			goto fail_gpio;

		trav_pcie_phy_init(trav_ctrl);
		break;
	default:
		dev_err(dev, "INVALID device type %d\n", mode);
		ret = -EINVAL;
		goto fail_gpio;
	}

	dev_info(dev, "Probe completed successfully\n");

	return 0;

fail_dma_set:
	dev_err(dev, "PCIe Failed to set 36 bit dma mask\n");
fail_probe:
	trav_pcie_phy_exit(trav_ctrl);
fail_gpio:
	if (pdata->res_ops && pdata->res_ops->deinit_clk_resources)
		pdata->res_ops->deinit_clk_resources(trav_ctrl);
	return ret;
}

static int __exit trav_pcie_remove(struct platform_device *pdev)
{
	struct trav_pcie *trav_ctrl = platform_get_drvdata(pdev);
	const struct trav_pcie_pdata *pdata = trav_ctrl->pdata;

	trav_pcie_phy_exit(trav_ctrl);

	if (pdata->res_ops && pdata->res_ops->deinit_clk_resources)
		pdata->res_ops->deinit_clk_resources(trav_ctrl);

	return 0;
}

static const struct trav_pcie_pdata trav_pcie_rc_pdata = {
	.dwc_ops	= &trav_dw_pcie_ops,
	.host_ops	= &trav_pcie_host_ops,
	.res_ops	= &trav_pcie_rc_res_ops,
	.mode		= DW_PCIE_RC_TYPE,
};

static const struct trav_pcie_pdata trav_pcie_ep_pdata = {
	.dwc_ops	= &trav_dw_pcie_ops,
	.host_ops	= &trav_pcie_host_ops,
	.res_ops	= &trav_pcie_ep_res_ops,
	.mode		= DW_PCIE_EP_TYPE,
};

static const struct of_device_id trav_pcie_of_match[] = {
	{
		.compatible = "samsung,trav-pcie",
		.data = (void *) &trav_pcie_rc_pdata,
	},
	{
		.compatible = "samsung,trav-pcie-ep",
		.data = (void *) &trav_pcie_ep_pdata,
	},
	{},
};

static int trav_pcie_suspend(struct device *dev)
{
	struct trav_pcie *trav_ctrl = dev_get_drvdata(dev);
	const struct trav_pcie_pdata *pdata = trav_ctrl->pdata;

	/* disable ltssm */
	trav_pcie_writel(trav_ctrl->elbi_base, PCIE_ELBI_LTSSM_DISABLE,
			 PCIE_APP_LTSSM_ENABLE);

	/* disable phy power */
	trav_pcie_phy_exit(trav_ctrl);

	/* gate mstr, slave, dbi clocks */
	if (pdata->res_ops && pdata->res_ops->deinit_clk_resources)
		pdata->res_ops->deinit_clk_resources(trav_ctrl);

	return 0;
}

static int trav_pcie_resume(struct device *dev)
{
	struct trav_pcie *trav_ctrl = dev_get_drvdata(dev);
	const struct trav_pcie_pdata *pdata = trav_ctrl->pdata;
	enum dw_pcie_device_mode mode;
	int ret;
	struct dw_pcie *pci = trav_ctrl->pci;
	struct pcie_port *pp = &pci->pp;

	/* enable mstr, slv, dbi clocks */
	if (pdata->res_ops && pdata->res_ops->init_clk_resources)
		pdata->res_ops->init_clk_resources(trav_ctrl);

	mode = (enum dw_pcie_device_mode)pdata->mode;
	switch (mode) {
	case DW_PCIE_RC_TYPE:
		trav_pcie_writel(trav_ctrl->elbi_base, DEVICE_TYPE_RC,
				 PCIE_TRAV_DEVICE_TYPE);

		ret = trav_pcie_host_init(pp);
		if (ret < 0)
			goto fail_resume;

		/* assert LTSSM enable */
		trav_pcie_writel(trav_ctrl->elbi_base, PCIE_ELBI_LTSSM_ENABLE,
				 PCIE_APP_LTSSM_ENABLE);

		break;
	case DW_PCIE_EP_TYPE:
		trav_pcie_writel(trav_ctrl->elbi_base, DEVICE_TYPE_EP,
				 PCIE_TRAV_DEVICE_TYPE);

		dw_pcie_writel_dbi(pci, PCIE_GEN3_RELATED_OFF,
				   (PCIE_GEN3_EQ_PHASE_2_3 |
				    PCIE_GEN3_RXEQ_PH01_EN |
				    PCIE_GEN3_RXEQ_RGRDLESS_RXTS));

		dw_pcie_setup(pci);
		break;
	default:
		dev_err(dev, "INVALID device type %d\n", mode);
		ret = -EINVAL;
		goto fail_mode;
	}

	return 0;

fail_resume:
	trav_pcie_phy_exit(trav_ctrl);
fail_mode:
	if (pdata->res_ops && pdata->res_ops->deinit_clk_resources)
		pdata->res_ops->deinit_clk_resources(trav_ctrl);
	return ret;
}

static const struct dev_pm_ops trav_pcie_pm_ops = {
	.suspend      = trav_pcie_suspend,
	.resume_noirq = trav_pcie_resume
};

static struct platform_driver trav_pcie_driver = {
	.probe	= trav_pcie_probe,
	.remove		= __exit_p(trav_pcie_remove),
	.driver = {
		.name	= "trav-pcie",
		.of_match_table = trav_pcie_of_match,
		.pm = &trav_pcie_pm_ops,
	},
};

static int __init trav_pcie_init(void)
{
	return platform_driver_register(&trav_pcie_driver);
}
module_init(trav_pcie_init);
