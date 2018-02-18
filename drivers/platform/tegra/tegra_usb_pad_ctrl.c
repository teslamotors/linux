/*
 * Copyright (c) 2013-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */


#include <linux/export.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/tegra-fuse.h>
#include <linux/clk/tegra.h>
#include <linux/tegra-powergate.h>
#include <linux/syscore_ops.h>
#include <linux/tegra_pm_domains.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/of_device.h>
#include <dt-bindings/usb/tegra_xhci.h>
#include <mach/tegra_usb_pad_ctrl.h>

#include "../../../arch/arm/mach-tegra/iomap.h"

static DEFINE_SPINLOCK(utmip_pad_lock);
static DEFINE_SPINLOCK(xusb_padctl_lock);
static DEFINE_SPINLOCK(xusb_padctl_reg_lock);
#ifdef CONFIG_ARCH_TEGRA_21x_SOC
static DEFINE_SPINLOCK(pcie_pad_lock);
static DEFINE_SPINLOCK(sata_pad_lock);
static DEFINE_SPINLOCK(hsic_pad_lock);
static int hsic_pad_count;
static struct of_device_id tegra_xusbb_pd[] = {
	{ .compatible = "nvidia,tegra186-xusbb-pd", },
	{ .compatible = "nvidia,tegra210-xusbb-pd", },
	{ .compatible = "nvidia,tegra132-xusbb-pd", },
	{},
};
static struct of_device_id tegra_xusbc_pd[] = {
	{ .compatible = "nvidia,tegra186-xusbc-pd", },
	{ .compatible = "nvidia,tegra210-xusbc-pd", },
	{ .compatible = "nvidia,tegra132-xusbc-pd", },
	{},
};
#endif
static int utmip_pad_count;
static struct clk *utmi_pad_clk;

#if !defined(CONFIG_ARCH_TEGRA_21x_SOC)
static u32 usb_lanes;
#endif

struct tegra_padctl_soc_data {
	char	* const *regulator_name;
	int	num_regulator;
};

struct tegra_padctl {
	struct platform_device *pdev;

	const struct tegra_padctl_soc_data *soc_data;

	struct regulator_bulk_data *regulator;

	struct clk *plle_clk;
	struct clk *plle_hw_clk;

	u32 lane_owner; /* used for XUSB lanes */
	u32 lane_map; /* used for PCIE lanes */
	bool enable_sata_port; /* used for SATA lane */

	int hs_curr_level_offset; /* deal with platform design deviation */
};
static struct tegra_padctl *padctl;

void tegra_xhci_release_otg_port(bool release)
{
	void __iomem *padctl_base = IO_ADDRESS(TEGRA_XUSB_PADCTL_BASE);
	u32 reg;
	unsigned long flags;

	if (tegra_platform_is_fpga())
		return;

	spin_lock_irqsave(&xusb_padctl_lock, flags);
	reg = readl(padctl_base + XUSB_PADCTL_USB2_PAD_MUX_0);

	reg &= ~PAD_PORT_MASK(0);
	if (release)
		reg |= PAD_PORT_SNPS(0);
	else
		reg |= PAD_PORT_XUSB(0);

	writel(reg, padctl_base + XUSB_PADCTL_USB2_PAD_MUX_0);
	spin_unlock_irqrestore(&xusb_padctl_lock, flags);
}
EXPORT_SYMBOL_GPL(tegra_xhci_release_otg_port);

void tegra_xhci_release_dev_port(bool release)
{
	void __iomem *padctl_base = IO_ADDRESS(TEGRA_XUSB_PADCTL_BASE);
	u32 reg;
	unsigned long flags;

	spin_lock_irqsave(&xusb_padctl_lock, flags);
	reg = readl(padctl_base + XUSB_PADCTL_USB2_PAD_MUX_0);

	reg &= ~USB2_OTG_PORT_CAP(0, ~0);
	if (release)
		reg |= USB2_OTG_PORT_CAP(0, XUSB_DEVICE_MODE);
	else
		reg |= USB2_OTG_PORT_CAP(0, XUSB_HOST_MODE);

	writel(reg, padctl_base + XUSB_PADCTL_USB2_PAD_MUX_0);
	spin_unlock_irqrestore(&xusb_padctl_lock, flags);
}
EXPORT_SYMBOL_GPL(tegra_xhci_release_dev_port);

static int tegra_padctl_hs_xmiter(int pad, bool force_disabled)
{
#ifdef CONFIG_ARCH_TEGRA_21x_SOC
	void __iomem *pad_base = IO_ADDRESS(TEGRA_XUSB_PADCTL_BASE);
	u32 reg;
	unsigned long flags;

	spin_lock_irqsave(&xusb_padctl_lock, flags);

	if (force_disabled) {
		/* assert PD2 */
		reg = readl(pad_base + XUSB_PADCTL_USB2_OTG_PAD_CTL_0(pad));
		reg |= (USB2_OTG_PD2 | USB2_PD2_OVRD_EN);
		writel(reg, pad_base + XUSB_PADCTL_USB2_OTG_PAD_CTL_0(pad));
	} else {
		/* remove pd2_ovrd_en, leave it under HW control */
		reg = readl(pad_base + XUSB_PADCTL_USB2_OTG_PAD_CTL_0(pad));
		reg &= ~(USB2_PD2_OVRD_EN);
		writel(reg, pad_base + XUSB_PADCTL_USB2_OTG_PAD_CTL_0(pad));
	}
	spin_unlock_irqrestore(&xusb_padctl_lock, flags);
#endif
	return 0;
}

int tegra_pd2_asserted(int pad)
{
	return tegra_padctl_hs_xmiter(pad, true);
}
EXPORT_SYMBOL_GPL(tegra_pd2_asserted);

int tegra_pd2_deasserted(int pad)
{
	return tegra_padctl_hs_xmiter(pad, false);
}
EXPORT_SYMBOL_GPL(tegra_pd2_deasserted);

void tegra_xhci_ss_wake_on_interrupts(u32 enabled_port, bool enable)
{
	void __iomem *pad_base = IO_ADDRESS(TEGRA_XUSB_PADCTL_BASE);
	u32 elpg_program0;
	unsigned long flags;

	spin_lock_irqsave(&xusb_padctl_lock, flags);
	/* clear any event */
	elpg_program0 = readl(pad_base + XUSB_PADCTL_ELPG_PROGRAM_0);
	if (enabled_port & TEGRA_XUSB_SS_P0)
		elpg_program0 |= SS_PORT0_WAKEUP_EVENT;
	if (enabled_port & TEGRA_XUSB_SS_P1)
		elpg_program0 |= SS_PORT1_WAKEUP_EVENT;
	if (enabled_port & TEGRA_XUSB_SS_P2)
		elpg_program0 |= SS_PORT2_WAKEUP_EVENT;
	if (enabled_port & TEGRA_XUSB_SS_P3)
		elpg_program0 |= SS_PORT3_WAKEUP_EVENT;

	writel(elpg_program0, pad_base + XUSB_PADCTL_ELPG_PROGRAM_0);

	/* enable ss wake interrupts */
	elpg_program0 = readl(pad_base + XUSB_PADCTL_ELPG_PROGRAM_0);
	if (enable) {
		/* enable interrupts */
		if (enabled_port & TEGRA_XUSB_SS_P0)
			elpg_program0 |= SS_PORT0_WAKE_INTERRUPT_ENABLE;
		if (enabled_port & TEGRA_XUSB_SS_P1)
			elpg_program0 |= SS_PORT1_WAKE_INTERRUPT_ENABLE;
		if (enabled_port & TEGRA_XUSB_SS_P2)
			elpg_program0 |= SS_PORT2_WAKE_INTERRUPT_ENABLE;
		if (enabled_port & TEGRA_XUSB_SS_P3)
			elpg_program0 |= SS_PORT3_WAKE_INTERRUPT_ENABLE;
	} else {
		/* disable interrupts */
		if (enabled_port & TEGRA_XUSB_SS_P0)
			elpg_program0 &= ~SS_PORT0_WAKE_INTERRUPT_ENABLE;
		if (enabled_port & TEGRA_XUSB_SS_P1)
			elpg_program0 &= ~SS_PORT1_WAKE_INTERRUPT_ENABLE;
		if (enabled_port & TEGRA_XUSB_SS_P2)
			elpg_program0 &= ~SS_PORT2_WAKE_INTERRUPT_ENABLE;
		if (enabled_port & TEGRA_XUSB_SS_P3)
			elpg_program0 &= ~SS_PORT3_WAKE_INTERRUPT_ENABLE;
	}
	writel(elpg_program0, pad_base + XUSB_PADCTL_ELPG_PROGRAM_0);
	spin_unlock_irqrestore(&xusb_padctl_lock, flags);
}
EXPORT_SYMBOL_GPL(tegra_xhci_ss_wake_on_interrupts);

void tegra_xhci_hs_wake_on_interrupts(u32 portmap, bool enable)
{
	void __iomem *pad_base = IO_ADDRESS(TEGRA_XUSB_PADCTL_BASE);
	u32 elpg_program0;
	unsigned long flags;

	spin_lock_irqsave(&xusb_padctl_lock, flags);
	elpg_program0 = readl(pad_base + XUSB_PADCTL_ELPG_PROGRAM_0);
	if (portmap & TEGRA_XUSB_USB2_P0)
		elpg_program0 |= USB2_PORT0_WAKEUP_EVENT;
	if (portmap & TEGRA_XUSB_USB2_P1)
		elpg_program0 |= USB2_PORT1_WAKEUP_EVENT;
	if (portmap & TEGRA_XUSB_USB2_P2)
		elpg_program0 |= USB2_PORT2_WAKEUP_EVENT;
	if (portmap & TEGRA_XUSB_USB2_P3)
		elpg_program0 |= USB2_PORT3_WAKEUP_EVENT;
	if (portmap & TEGRA_XUSB_HSIC_P0)
		elpg_program0 |= USB2_HSIC_PORT0_WAKEUP_EVENT;
	if (portmap & TEGRA_XUSB_HSIC_P1)
		elpg_program0 |= USB2_HSIC_PORT1_WAKEUP_EVENT;

	writel(elpg_program0, pad_base + XUSB_PADCTL_ELPG_PROGRAM_0);

	/* Enable the wake interrupts */
	elpg_program0 = readl(pad_base + XUSB_PADCTL_ELPG_PROGRAM_0);
	if (enable) {
		/* enable interrupts */
		if (portmap & TEGRA_XUSB_USB2_P0)
			elpg_program0 |= USB2_PORT0_WAKE_INTERRUPT_ENABLE;
		if (portmap & TEGRA_XUSB_USB2_P1)
			elpg_program0 |= USB2_PORT1_WAKE_INTERRUPT_ENABLE;
		if (portmap & TEGRA_XUSB_USB2_P2)
			elpg_program0 |= USB2_PORT2_WAKE_INTERRUPT_ENABLE;
		if (portmap & TEGRA_XUSB_USB2_P3)
			elpg_program0 |= USB2_PORT3_WAKE_INTERRUPT_ENABLE;
		if (portmap & TEGRA_XUSB_HSIC_P0)
			elpg_program0 |= USB2_HSIC_PORT0_WAKE_INTERRUPT_ENABLE;
		if (portmap & TEGRA_XUSB_HSIC_P1)
			elpg_program0 |= USB2_HSIC_PORT1_WAKE_INTERRUPT_ENABLE;
	} else {
		if (portmap & TEGRA_XUSB_USB2_P0)
			elpg_program0 &= ~USB2_PORT0_WAKE_INTERRUPT_ENABLE;
		if (portmap & TEGRA_XUSB_USB2_P1)
			elpg_program0 &= ~USB2_PORT1_WAKE_INTERRUPT_ENABLE;
		if (portmap & TEGRA_XUSB_USB2_P2)
			elpg_program0 &= ~USB2_PORT2_WAKE_INTERRUPT_ENABLE;
		if (portmap & TEGRA_XUSB_USB2_P3)
			elpg_program0 &= ~USB2_PORT3_WAKE_INTERRUPT_ENABLE;
		if (portmap & TEGRA_XUSB_HSIC_P0)
			elpg_program0 &= ~USB2_HSIC_PORT0_WAKE_INTERRUPT_ENABLE;
		if (portmap & TEGRA_XUSB_HSIC_P1)
			elpg_program0 &= ~USB2_HSIC_PORT1_WAKE_INTERRUPT_ENABLE;
	}
	writel(elpg_program0, pad_base + XUSB_PADCTL_ELPG_PROGRAM_0);
	spin_unlock_irqrestore(&xusb_padctl_lock, flags);
}
EXPORT_SYMBOL_GPL(tegra_xhci_hs_wake_on_interrupts);

void tegra_xhci_ss_wake_signal(u32 enabled_port, bool enable)
{
	void __iomem *pad_base = IO_ADDRESS(TEGRA_XUSB_PADCTL_BASE);
	u32 elpg_program0;
	unsigned long flags;

	/* DO NOT COMBINE BELOW 2 WRITES */

	spin_lock_irqsave(&xusb_padctl_lock, flags);
	/* Assert/Deassert clamp_en_early signals to SSP0/1 */
#ifdef CONFIG_ARCH_TEGRA_21x_SOC
	elpg_program0 = readl(pad_base + XUSB_PADCTL_ELPG_PROGRAM_1);
#else
	elpg_program0 = readl(pad_base + XUSB_PADCTL_ELPG_PROGRAM_0);
#endif
	if (enable) {
		if (enabled_port & TEGRA_XUSB_SS_P0)
			elpg_program0 |= SSP0_ELPG_CLAMP_EN_EARLY;
		if (enabled_port & TEGRA_XUSB_SS_P1)
			elpg_program0 |= SSP1_ELPG_CLAMP_EN_EARLY;
		if (enabled_port & TEGRA_XUSB_SS_P2)
			elpg_program0 |= SSP2_ELPG_CLAMP_EN_EARLY;
		if (enabled_port & TEGRA_XUSB_SS_P3)
			elpg_program0 |= SSP3_ELPG_CLAMP_EN_EARLY;
	} else {
		if (enabled_port & TEGRA_XUSB_SS_P0)
			elpg_program0 &= ~SSP0_ELPG_CLAMP_EN_EARLY;
		if (enabled_port & TEGRA_XUSB_SS_P1)
			elpg_program0 &= ~SSP1_ELPG_CLAMP_EN_EARLY;
		if (enabled_port & TEGRA_XUSB_SS_P2)
			elpg_program0 &= ~SSP2_ELPG_CLAMP_EN_EARLY;
		if (enabled_port & TEGRA_XUSB_SS_P3)
			elpg_program0 &= ~SSP3_ELPG_CLAMP_EN_EARLY;
	}
#ifdef CONFIG_ARCH_TEGRA_21x_SOC
	writel(elpg_program0, pad_base + XUSB_PADCTL_ELPG_PROGRAM_1);
#else
	writel(elpg_program0, pad_base + XUSB_PADCTL_ELPG_PROGRAM_0);
#endif
	/*
	 * Check the LP0 figure and leave gap bw writes to
	 * clamp_en_early and clamp_en
	 */
	udelay(100);

	/* Assert/Deassert clam_en signal */
#ifdef CONFIG_ARCH_TEGRA_21x_SOC
	elpg_program0 = readl(pad_base + XUSB_PADCTL_ELPG_PROGRAM_1);
#else
	elpg_program0 = readl(pad_base + XUSB_PADCTL_ELPG_PROGRAM_0);
#endif

	if (enable) {
		if (enabled_port & TEGRA_XUSB_SS_P0)
			elpg_program0 |= SSP0_ELPG_CLAMP_EN;
		if (enabled_port & TEGRA_XUSB_SS_P1)
			elpg_program0 |= SSP1_ELPG_CLAMP_EN;
		if (enabled_port & TEGRA_XUSB_SS_P2)
			elpg_program0 |= SSP2_ELPG_CLAMP_EN;
		if (enabled_port & TEGRA_XUSB_SS_P3)
			elpg_program0 |= SSP3_ELPG_CLAMP_EN;
	} else {
		if (enabled_port & TEGRA_XUSB_SS_P0)
			elpg_program0 &= ~SSP0_ELPG_CLAMP_EN;
		if (enabled_port & TEGRA_XUSB_SS_P1)
			elpg_program0 &= ~SSP1_ELPG_CLAMP_EN;
		if (enabled_port & TEGRA_XUSB_SS_P2)
			elpg_program0 &= ~SSP2_ELPG_CLAMP_EN;
		if (enabled_port & TEGRA_XUSB_SS_P3)
			elpg_program0 &= ~SSP3_ELPG_CLAMP_EN;
	}

#ifdef CONFIG_ARCH_TEGRA_21x_SOC
	writel(elpg_program0, pad_base + XUSB_PADCTL_ELPG_PROGRAM_1);
#else
	writel(elpg_program0, pad_base + XUSB_PADCTL_ELPG_PROGRAM_0);
#endif
	spin_unlock_irqrestore(&xusb_padctl_lock, flags);

	/* wait for 250us for the writes to propogate */
	if (enable)
		udelay(250);
}
EXPORT_SYMBOL_GPL(tegra_xhci_ss_wake_signal);

void tegra_xhci_ss_vcore(u32 enabled_port, bool enable)
{
	void __iomem *pad_base = IO_ADDRESS(TEGRA_XUSB_PADCTL_BASE);
	u32 elpg_program0;
	unsigned long flags;

	spin_lock_irqsave(&xusb_padctl_lock, flags);
	/* Assert vcore_off signal */
#ifdef CONFIG_ARCH_TEGRA_21x_SOC
	elpg_program0 = readl(pad_base + XUSB_PADCTL_ELPG_PROGRAM_1);
#else
	elpg_program0 = readl(pad_base + XUSB_PADCTL_ELPG_PROGRAM_0);
#endif

	if (enable) {
		if (enabled_port & TEGRA_XUSB_SS_P0)
			elpg_program0 |= SSP0_ELPG_VCORE_DOWN;
		if (enabled_port & TEGRA_XUSB_SS_P1)
			elpg_program0 |= SSP1_ELPG_VCORE_DOWN;
		if (enabled_port & TEGRA_XUSB_SS_P2)
			elpg_program0 |= SSP2_ELPG_VCORE_DOWN;
		if (enabled_port & TEGRA_XUSB_SS_P3)
			elpg_program0 |= SSP3_ELPG_VCORE_DOWN;
	} else {
		if (enabled_port & TEGRA_XUSB_SS_P0)
			elpg_program0 &= ~SSP0_ELPG_VCORE_DOWN;
		if (enabled_port & TEGRA_XUSB_SS_P1)
			elpg_program0 &= ~SSP1_ELPG_VCORE_DOWN;
		if (enabled_port & TEGRA_XUSB_SS_P2)
			elpg_program0 &= ~SSP2_ELPG_VCORE_DOWN;
		if (enabled_port & TEGRA_XUSB_SS_P3)
			elpg_program0 &= ~SSP3_ELPG_VCORE_DOWN;
	}
#ifdef CONFIG_ARCH_TEGRA_21x_SOC
	writel(elpg_program0, pad_base + XUSB_PADCTL_ELPG_PROGRAM_1);
#else
	writel(elpg_program0, pad_base + XUSB_PADCTL_ELPG_PROGRAM_0);
#endif
	spin_unlock_irqrestore(&xusb_padctl_lock, flags);
}
EXPORT_SYMBOL_GPL(tegra_xhci_ss_vcore);

static int pex_usb_pad_pll(bool assert)
{
#ifdef CONFIG_ARCH_TEGRA_21x_SOC
	static struct clk *pex_uphy;
	static int ref_count;
	unsigned long flags;
	void __iomem *clk_base = IO_ADDRESS(TEGRA_CLK_RESET_BASE);
	u32 val;

	if (!pex_uphy)
		pex_uphy = clk_get_sys(NULL, "pex_uphy");

	if (IS_ERR(pex_uphy)) {
		pr_err("Fail to get pex_uphy clk\n");
		return PTR_ERR(pex_uphy);
	}

	pr_debug("%s ref_count %d assert %d\n", __func__, ref_count, assert);

	spin_lock_irqsave(&pcie_pad_lock, flags);

	if (!assert) {
		if (ref_count == 0)
			tegra_periph_reset_deassert(pex_uphy);
		ref_count++;
	} else {
		ref_count--;
		if (ref_count == 0) {
			val = readl(clk_base +
				CLK_RST_CONTROLLER_XUSBIO_PLL_CFG0_0);
			if (XUSBIO_SEQ_ENABLE & val)
				pr_info("%s: pex uphy already enabled",
					__func__);
			else
				tegra_periph_reset_assert(pex_uphy);
		}
	}
	spin_unlock_irqrestore(&pcie_pad_lock, flags);

#endif
	return 0;
}

static int sata_usb_pad_pll(bool assert)
{
#ifdef CONFIG_ARCH_TEGRA_21x_SOC
	static struct clk *sata_uphy;
	static int ref_count;
	unsigned long flags;
	void __iomem *clk_base = IO_ADDRESS(TEGRA_CLK_RESET_BASE);
	u32 val;

	if (!sata_uphy)
		sata_uphy = clk_get_sys(NULL, "sata_uphy");

	if (IS_ERR(sata_uphy)) {
		pr_err("Fail to get sata_uphy clk\n");
		return PTR_ERR(sata_uphy);
	}

	pr_debug("%s ref_count %d assert %d\n", __func__, ref_count, assert);

	spin_lock_irqsave(&sata_pad_lock, flags);

	if (!assert) {
		if (ref_count == 0)
			tegra_periph_reset_deassert(sata_uphy);
		ref_count++;
	} else {
		ref_count--;
		if (ref_count == 0) {
			val = readl(clk_base +
				CLK_RST_CONTROLLER_SATA_PLL_CFG0_0);
			if (SATA_SEQ_ENABLE & val)
				pr_info("%s: sata uphy already enabled",
					__func__);
			else
				tegra_periph_reset_assert(sata_uphy);
		}
	}
	spin_unlock_irqrestore(&sata_pad_lock, flags);

#endif
	return 0;
}

int pex_usb_pad_pll_reset_assert(void)
{
	return pex_usb_pad_pll(true);
}
EXPORT_SYMBOL_GPL(pex_usb_pad_pll_reset_assert);

int pex_usb_pad_pll_reset_deassert(void)
{
	return pex_usb_pad_pll(false);
}
EXPORT_SYMBOL_GPL(pex_usb_pad_pll_reset_deassert);

int sata_usb_pad_pll_reset_assert(void)
{
	return sata_usb_pad_pll(true);
}
EXPORT_SYMBOL_GPL(sata_usb_pad_pll_reset_assert);

int sata_usb_pad_pll_reset_deassert(void)
{
	return sata_usb_pad_pll(false);
}
EXPORT_SYMBOL_GPL(sata_usb_pad_pll_reset_deassert);

#ifdef CONFIG_ARCH_TEGRA_21x_SOC
static bool tegra_xusb_partitions_powergated(void)
{
	int partition_id_xusbb, partition_id_xusbc;

	partition_id_xusbb = tegra_pd_get_powergate_id(tegra_xusbb_pd);
	if (partition_id_xusbb < 0)
		return -EINVAL;

	partition_id_xusbc = tegra_pd_get_powergate_id(tegra_xusbc_pd);
	if (partition_id_xusbc < 0)
		return -EINVAL;

	if (!tegra_powergate_is_powered(partition_id_xusbb)
			&& !tegra_powergate_is_powered(partition_id_xusbc))
		return true;
	return false;
}
#else
static bool tegra_xusb_partitions_powergated(void)
{
	return true;
}
#endif

int utmi_phy_iddq_override(bool set)
{
	unsigned long val, flags;
	void __iomem *clk_base = IO_ADDRESS(TEGRA_CLK_RESET_BASE);

	spin_lock_irqsave(&utmip_pad_lock, flags);
	val = readl(clk_base + UTMIPLL_HW_PWRDN_CFG0);
	if (set && !utmip_pad_count) {
		if (!tegra_xusb_partitions_powergated()) {
			pr_info("XUSB partitions are on, trying to assert IDDQ");
			goto out1;
		}
		if (val & UTMIPLL_LOCK) {
			pr_info("UTMIPLL is locked, trying to assert IDDQ");
			goto out1;
		}
		val |= UTMIPLL_HW_PWRDN_CFG0_IDDQ_OVERRIDE;
	} else if (!set && utmip_pad_count)
		val &= ~UTMIPLL_HW_PWRDN_CFG0_IDDQ_OVERRIDE;
	else
		goto out1;
	val |= UTMIPLL_HW_PWRDN_CFG0_IDDQ_SWCTL;
	writel(val, clk_base + UTMIPLL_HW_PWRDN_CFG0);

out1:
	spin_unlock_irqrestore(&utmip_pad_lock, flags);
	return 0;
}
EXPORT_SYMBOL_GPL(utmi_phy_iddq_override);

#ifndef CONFIG_ARCH_TEGRA_21x_SOC
static void utmi_phy_pad(bool enable)
{
	unsigned long val;
	int port, xhci_port_present = 0;
	void __iomem *pad_base =  IO_ADDRESS(TEGRA_USB_BASE);

	if (enable) {
		val = readl(pad_base + UTMIP_BIAS_CFG0);
		val &= ~(UTMIP_OTGPD | UTMIP_BIASPD);
		val |= UTMIP_HSSQUELCH_LEVEL(0x2) | UTMIP_HSDISCON_LEVEL(0x3) |
			UTMIP_HSDISCON_LEVEL_MSB;
		writel(val, pad_base + UTMIP_BIAS_CFG0);

#if defined(CONFIG_USB_XHCI_HCD)
	tegra_usb_pad_reg_update(XUSB_PADCTL_USB2_BIAS_PAD_CTL_0
			, PD_MASK , 0);
#endif
	} else {

		val = tegra_usb_pad_reg_read(XUSB_PADCTL_USB2_PAD_MUX_0);
		for (port = 0; port < XUSB_UTMI_COUNT; port++) {
			if ((val & PAD_PORT_MASK(port)) == PAD_PORT_XUSB(port))
				xhci_port_present = 1;
		}

		val = readl(pad_base + UTMIP_BIAS_CFG0);
		val |= UTMIP_OTGPD;
		val |= UTMIP_BIASPD;
#if defined(CONFIG_TEGRA_XHCI_ENABLE_CDP_PORT)
		if (xhci_port_present)	/* xhci holds atleast one utmi port */
			val &= ~UTMIP_BIASPD;
#endif
		val &= ~(UTMIP_HSSQUELCH_LEVEL(~0) | UTMIP_HSDISCON_LEVEL(~0) |
			UTMIP_HSDISCON_LEVEL_MSB);
		writel(val, pad_base + UTMIP_BIAS_CFG0);

#if defined(CONFIG_TEGRA_XHCI_ENABLE_CDP_PORT)
		tegra_usb_pad_reg_update(XUSB_PADCTL_USB2_BIAS_PAD_CTL_0
			, PD_MASK , 0);
#else
		tegra_usb_pad_reg_update(XUSB_PADCTL_USB2_BIAS_PAD_CTL_0
			, PD_MASK , PD_MASK);
#endif
	}
}

int utmi_phy_pad_enable(void)
{
	unsigned long flags;
	static struct clk *usb2_trk;

	if (!usb2_trk)
		usb2_trk = clk_get_sys(NULL, "usb2_trk");

	if (!utmi_pad_clk)
		utmi_pad_clk = clk_get_sys("utmip-pad", NULL);

	clk_enable(utmi_pad_clk);
	if (!IS_ERR_OR_NULL(usb2_trk))
		clk_enable(usb2_trk);

	spin_lock_irqsave(&utmip_pad_lock, flags);
	utmip_pad_count++;

	utmi_phy_pad(true);

	spin_unlock_irqrestore(&utmip_pad_lock, flags);

	clk_disable(utmi_pad_clk);
	if (!IS_ERR_OR_NULL(usb2_trk))
		clk_disable(usb2_trk);

	return 0;
}
EXPORT_SYMBOL_GPL(utmi_phy_pad_enable);

int utmi_phy_pad_disable(void)
{
	unsigned long flags;
	static struct clk *usb2_trk;

	if (!usb2_trk)
		usb2_trk = clk_get_sys(NULL, "usb2_trk");

	if (!utmi_pad_clk)
		utmi_pad_clk = clk_get_sys("utmip-pad", NULL);

	clk_enable(utmi_pad_clk);
	if (!IS_ERR_OR_NULL(usb2_trk))
		clk_enable(usb2_trk);
	spin_lock_irqsave(&utmip_pad_lock, flags);

	if (!utmip_pad_count) {
		pr_err("%s: utmip pad already powered off\n", __func__);
		goto out;
	}
	if (--utmip_pad_count == 0)
		utmi_phy_pad(false);
out:
	spin_unlock_irqrestore(&utmip_pad_lock, flags);
	clk_disable(utmi_pad_clk);
	if (!IS_ERR_OR_NULL(usb2_trk))
		clk_disable(usb2_trk);

	return 0;
}
EXPORT_SYMBOL_GPL(utmi_phy_pad_disable);

#else

static void utmi_phy_pad(struct tegra_prod_list *prod_list, bool enable)
{
	void __iomem *pad_base = IO_ADDRESS(TEGRA_XUSB_PADCTL_BASE);
	int val;

	if (IS_ERR_OR_NULL(prod_list))
		prod_list = NULL;

	if (tegra_platform_is_fpga())
		return;

	if (enable) {
		tegra_usb_pad_reg_update(XUSB_PADCTL_USB2_PAD_MUX_0,
			BIAS_PAD_MASK, BIAS_PAD_XUSB);
		tegra_usb_pad_reg_update(XUSB_PADCTL_USB2_BIAS_PAD_CTL_1,
			TRK_START_TIMER_MASK, TRK_START_TIMER);
		tegra_usb_pad_reg_update(XUSB_PADCTL_USB2_BIAS_PAD_CTL_1,
			TRK_DONE_RESET_TIMER_MASK, TRK_DONE_RESET_TIMER);

		if (prod_list) {
			/* DT node for prod_c_bias* uses index '0',
			 * TEGRA_XUSB_PADCTL_BASE is provided as
			 * base address to prod_set_by_name() API.
			 * Prod fw uses 'base + index' to compute the
			 * final base address.
			 * In case of other nodes prod_c_utmi* or prod_c_ss*
			 * entries index is '3', but base address
			 * passed to prod_set_by_name() is first element
			 * of array of addresses, hence it turns out to be
			 * base[3] which is final base adderess.
			 */

			const char *prod_name = "prod_c_bias";
			if (tegra_chip_get_revision() >= TEGRA_REVISION_A02)
				prod_name = "prod_c_bias_a02";
			val = tegra_prod_set_by_name(&pad_base, prod_name,
							prod_list);
			if (val < 0) {
				pr_err("%s(), failed with err:%d\n",
							__func__, val);
				goto safe_settings;
			}
		} else {
safe_settings:
			tegra_usb_pad_reg_update(
				XUSB_PADCTL_USB2_BIAS_PAD_CTL_0,
				HS_SQUELCH_LEVEL(~0), HS_SQUELCH_LEVEL(2));

			tegra_usb_pad_reg_update(
				XUSB_PADCTL_USB2_BIAS_PAD_CTL_0,
				HS_DISCON_LEVEL(~0), HS_DISCON_LEVEL(7));
		}

		tegra_usb_pad_reg_update(XUSB_PADCTL_USB2_BIAS_PAD_CTL_0,
			PD_MASK, 0);

		udelay(1);

		tegra_usb_pad_reg_update(XUSB_PADCTL_USB2_BIAS_PAD_CTL_1,
			PD_TRK_MASK, 0);

		/* for tracking complete */
		udelay(10);
	} else {

		tegra_usb_pad_reg_update(XUSB_PADCTL_USB2_PAD_MUX_0,
			BIAS_PAD_MASK, BIAS_PAD_XUSB);
		tegra_usb_pad_reg_update(XUSB_PADCTL_USB2_BIAS_PAD_CTL_0,
			PD_MASK, PD_MASK);
	}
}

int utmi_phy_pad_enable(struct tegra_prod_list *prod_list)
{
	unsigned long flags;
	static struct clk *usb2_trk;

	if (!usb2_trk)
		usb2_trk = clk_get_sys(NULL, "usb2_trk");

	if (!utmi_pad_clk)
		utmi_pad_clk = clk_get_sys("utmip-pad", NULL);

	clk_enable(utmi_pad_clk);
	if (!IS_ERR_OR_NULL(usb2_trk))
		clk_enable(usb2_trk);

	spin_lock_irqsave(&utmip_pad_lock, flags);
	utmip_pad_count++;

	utmi_phy_pad(prod_list, true);

	spin_unlock_irqrestore(&utmip_pad_lock, flags);

	clk_disable(utmi_pad_clk);
	if (!IS_ERR_OR_NULL(usb2_trk))
		clk_disable(usb2_trk);

	return 0;
}
EXPORT_SYMBOL_GPL(utmi_phy_pad_enable);

int utmi_phy_pad_disable(struct tegra_prod_list *prod_list)
{
	unsigned long flags;
	static struct clk *usb2_trk;

	if (!usb2_trk)
		usb2_trk = clk_get_sys(NULL, "usb2_trk");

	if (!utmi_pad_clk)
		utmi_pad_clk = clk_get_sys("utmip-pad", NULL);

	clk_enable(utmi_pad_clk);
	if (!IS_ERR_OR_NULL(usb2_trk))
		clk_enable(usb2_trk);
	spin_lock_irqsave(&utmip_pad_lock, flags);

	if (!utmip_pad_count) {
		pr_err("%s: utmip pad already powered off\n", __func__);
		goto out;
	}
	if (--utmip_pad_count == 0)
		utmi_phy_pad(prod_list, false);
out:
	spin_unlock_irqrestore(&utmip_pad_lock, flags);
	clk_disable(utmi_pad_clk);
	if (!IS_ERR_OR_NULL(usb2_trk))
		clk_disable(usb2_trk);

	return 0;
}
EXPORT_SYMBOL_GPL(utmi_phy_pad_disable);
#endif

int hsic_trk_enable(void)
{
#ifdef CONFIG_ARCH_TEGRA_21x_SOC
	unsigned long flags;

	/* TODO : need to enable HSIC_TRK clk */
	spin_lock_irqsave(&hsic_pad_lock, flags);

	hsic_pad_count++;
	tegra_usb_pad_reg_update(XUSB_PADCTL_USB2_PAD_MUX_0,
		HSIC_PAD_TRK(~0) , HSIC_PAD_TRK(HSIC_PAD_TRK_XUSB));

	tegra_usb_pad_reg_update(XUSB_PADCTL_HSIC_PAD_TRK_CTL_0,
		HSIC_TRK_START_TIMER(~0), HSIC_TRK_START_TIMER(0x1e));

	tegra_usb_pad_reg_update(XUSB_PADCTL_HSIC_PAD_TRK_CTL_0,
		HSIC_TRK_DONE_RESET_TIMER(~0) , HSIC_TRK_DONE_RESET_TIMER(0xa));

	udelay(1);

	tegra_usb_pad_reg_update(XUSB_PADCTL_HSIC_PAD_TRK_CTL_0,
		HSIC_PD_TRK(~0), HSIC_PD_TRK(0));

	spin_unlock_irqrestore(&hsic_pad_lock, flags);
	/* TODO : need to disable HSIC_TRK clk */
#endif
	return 0;
}
EXPORT_SYMBOL_GPL(hsic_trk_enable);


static void get_usb_calib_data(int pad, u32 *hs_curr_level_pad,
		u32 *term_range_adj, u32 *rpd_ctl, u32 *hs_iref_cap)
{
	u32 usb_calib0 = tegra_fuse_readl(FUSE_SKU_USB_CALIB_0);
	/*
	 * read from usb_calib0 and pass to driver
	 * set HS_CURR_LEVEL (PAD0)	= usb_calib0[5:0]
	 * set TERM_RANGE_ADJ		= usb_calib0[10:7]
	 * set HS_SQUELCH_LEVEL		= usb_calib0[12:11]
	 * set HS_IREF_CAP		= usb_calib0[14:13]
	 * set HS_CURR_LEVEL (PAD1)	= usb_calib0[20:15]
	 */
#ifdef CONFIG_ARCH_TEGRA_21x_SOC
	u32 usb_calib_ext = tegra_fuse_readl(FUSE_USB_CALIB_EXT_0);
	/* RPD_CTRL			= USB_CALIB_EXT[4:0] */

	pr_debug("usb_calib0 = 0x%08x\n", usb_calib0);
	pr_debug("usb_calib_ext = 0x%08x\n", usb_calib_ext);

	*hs_curr_level_pad = (usb_calib0 >>
		((!pad) ? 0 : ((6 * (pad + 1)) - 1))) & 0x3f;
	*term_range_adj = (usb_calib0 >> 7) & 0xf;
	*rpd_ctl = (usb_calib_ext >> 0) & 0x1f;
	*hs_iref_cap = 0;
#else
	pr_info("usb_calib0 = 0x%08x\n", usb_calib0);
	*hs_curr_level_pad = (usb_calib0 >>
			((!pad) ? 0 : (15 + 7 * (pad - 1)))) & 0x3f;
	*term_range_adj = (usb_calib0 >> 7) & 0xf;
	*hs_iref_cap = (usb_calib0 >> 13) & 0x3;
	*rpd_ctl = 0;
#endif
}

static u8 ss_pad_inited = 0x0;
static u8 utmi_pad_inited = 0x0;

void xusb_utmi_pad_deinit(int pad)
{
	unsigned long flags;
	spin_lock_irqsave(&xusb_padctl_lock, flags);
	utmi_pad_inited &= ~(1 << pad);
	spin_unlock_irqrestore(&xusb_padctl_lock, flags);
}
EXPORT_SYMBOL_GPL(xusb_utmi_pad_deinit);

void xusb_utmi_pad_init(int pad, u32 cap, bool external_pmic)
{
	unsigned long val, flags;
	u32 ctl0_offset, ctl1_offset, batry_chg_1;
	static u32 hs_curr_level_pad, term_range_adj;
	static u32 rpd_ctl, hs_iref_cap;
	void __iomem *pad_base = IO_ADDRESS(TEGRA_XUSB_PADCTL_BASE);

	spin_lock_irqsave(&xusb_padctl_lock, flags);
	/* Check if programmed */
	if (utmi_pad_inited & (1 << pad)) {
		pr_warn("Already init for utmi pad %d\n", pad);
		spin_unlock_irqrestore(&xusb_padctl_lock, flags);
		return;
	}

	/* program utmi pad */
	val = readl(pad_base + XUSB_PADCTL_USB2_PAD_MUX_0);
	val &= ~PAD_PORT_MASK(pad);
	val |= PAD_PORT_XUSB(pad);
	writel(val, pad_base + XUSB_PADCTL_USB2_PAD_MUX_0);

	val = readl(pad_base + XUSB_PADCTL_USB2_PORT_CAP_0);
	val &= ~USB2_OTG_PORT_CAP(pad, ~0);
	val |= cap;
	writel(val, pad_base + XUSB_PADCTL_USB2_PORT_CAP_0);

	val = readl(pad_base + XUSB_PADCTL_USB2_OC_MAP_0);
	val &= ~PORT_OC_PIN(pad, ~0);
	val |= PORT_OC_PIN(pad, OC_DISABLED);
	writel(val, pad_base + XUSB_PADCTL_USB2_OC_MAP_0);

	val = readl(pad_base + XUSB_PADCTL_VBUS_OC_MAP_0);
	val &= ~VBUS_OC_MAP(pad, ~0);
	val |= VBUS_OC_MAP(pad, OC_DISABLED);
	writel(val, pad_base + XUSB_PADCTL_VBUS_OC_MAP_0);
	/*
	* Modify only the bits which belongs to the port
	* and enable respective VBUS_PAD for the port
	*/
	if (external_pmic) {
		val = readl(pad_base + XUSB_PADCTL_OC_DET_0);
		val &= ~VBUS_EN_OC_MAP(pad, ~0);
		val |= VBUS_EN_OC_MAP(pad, OC_VBUS_PAD(pad));
		writel(val, pad_base + XUSB_PADCTL_OC_DET_0);
	}

	val = readl(pad_base + XUSB_PADCTL_USB2_OC_MAP_0);
	val &= ~PORT_OC_PIN(pad, ~0);
	val |= PORT_OC_PIN(pad, OC_VBUS_PAD(pad));
	writel(val, pad_base + XUSB_PADCTL_USB2_OC_MAP_0);

	ctl0_offset = XUSB_PADCTL_USB2_OTG_PAD_CTL_0(pad);
	ctl1_offset = XUSB_PADCTL_USB2_OTG_PAD_CTL_1(pad);

	get_usb_calib_data(pad, &hs_curr_level_pad,
			&term_range_adj, &rpd_ctl, &hs_iref_cap);

	if (padctl && padctl->hs_curr_level_offset) {
		int hs_current_level = (int) hs_curr_level_pad +
					padctl->hs_curr_level_offset;

		dev_dbg(&padctl->pdev->dev, "UTMI pad %d apply hs_curr_level_offset %d\n",
			pad, padctl->hs_curr_level_offset);

		if (hs_current_level < 0)
			hs_current_level = 0;
		if (hs_current_level > 0x3f)
			hs_current_level = 0x3f;

		hs_curr_level_pad = hs_current_level;
	}

	val = readl(pad_base + ctl0_offset);
	val &= ~(USB2_OTG_HS_CURR_LVL | USB2_OTG_PD_ZI);
	val |= hs_curr_level_pad | USB2_OTG_PD | USB2_OTG_PD2;
	writel(val, pad_base + ctl0_offset);

	val = readl(pad_base + ctl1_offset);
	val &= ~(USB2_OTG_TERM_RANGE_ADJ | RPD_CTRL
			| USB2_OTG_HS_IREF_CAP
			| USB2_OTG_PD_CHRP_FORCE_POWERUP
			| USB2_OTG_PD_DISC_FORCE_POWERUP);
	val |= (rpd_ctl << 26) |
		(term_range_adj << 3) |
		(hs_iref_cap << 9) | USB2_OTG_PD_DR;
	writel(val, pad_base + ctl1_offset);

	batry_chg_1 = XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD_CTL1(pad);

	val = readl(pad_base + batry_chg_1);
	val &= ~(VREG_FIX18 | VREG_LEV);
	if ((cap >> (4 * pad)) == XUSB_DEVICE_MODE)
		val |= VREG_LEV_EN;
	if ((cap >> (4 * pad)) == XUSB_HOST_MODE)
		val |= VREG_FIX18;
	writel(val, pad_base + batry_chg_1);

	utmi_pad_inited |= (1 << pad);
	spin_unlock_irqrestore(&xusb_padctl_lock, flags);

	/* Dump register value */
	pr_debug("[%s] UTMI pad %d, cap %x\n", __func__ , pad, cap);
	pr_debug("XUSB_PADCTL_USB2_PAD_MUX_0 = 0x%x\n"
			, readl(pad_base + XUSB_PADCTL_USB2_PAD_MUX_0));
	pr_debug("XUSB_PADCTL_USB2_PORT_CAP_0 = 0x%x\n"
			, readl(pad_base + XUSB_PADCTL_USB2_PORT_CAP_0));
	pr_debug("XUSB_PADCTL_USB2_OC_MAP_0 = 0x%x\n"
			, readl(pad_base + XUSB_PADCTL_USB2_OC_MAP_0));
	pr_debug("XUSB_PADCTL_VBUS_OC_MAP_0 = 0x%x\n"
			, readl(pad_base + XUSB_PADCTL_VBUS_OC_MAP_0));
	pr_debug("XUSB_PADCTL_USB2_OTG_PAD_CTL_0 (0x%x) = 0x%x\n"
			, ctl0_offset , readl(pad_base + ctl0_offset));
	pr_debug("XUSB_PADCTL_USB2_OTG_PAD_CTL_1 (0x%x) = 0x%x\n"
			, ctl1_offset, readl(pad_base + ctl1_offset));
	pr_debug("XUSB_PADCTL_USB2_BATTERY_CHRG_CTL1 (0x%x) = 0x%x\n"
			, batry_chg_1, readl(pad_base + batry_chg_1));
}
EXPORT_SYMBOL_GPL(xusb_utmi_pad_init);

void xusb_utmi_pad_driver_power(int pad, bool on)
{
	void __iomem *pad_base = IO_ADDRESS(TEGRA_XUSB_PADCTL_BASE);
	u32 ctl0_val, ctl1_val, ctl0_offset, ctl1_offset;
	unsigned long flags;

	ctl0_offset = XUSB_PADCTL_USB2_OTG_PAD_CTL_0(pad);
	ctl1_offset = XUSB_PADCTL_USB2_OTG_PAD_CTL_1(pad);

	spin_lock_irqsave(&xusb_padctl_lock, flags);

	ctl0_val = readl(pad_base + ctl0_offset);
	ctl1_val = readl(pad_base + ctl1_offset);

	if (on) {
		ctl0_val &= ~(USB2_OTG_PD | USB2_OTG_PD2);
		ctl1_val &= ~(USB2_OTG_PD_DR);
	} else {
		ctl0_val |= USB2_OTG_PD | USB2_OTG_PD2;
		ctl1_val |= USB2_OTG_PD_DR;
	}

	writel(ctl0_val, pad_base + ctl0_offset);
	writel(ctl1_val, pad_base + ctl1_offset);

	spin_unlock_irqrestore(&xusb_padctl_lock, flags);
}
EXPORT_SYMBOL_GPL(xusb_utmi_pad_driver_power);

void xusb_ss_pad_deinit(int pad)
{
	unsigned long flags;
	spin_lock_irqsave(&xusb_padctl_lock, flags);
	ss_pad_inited &= ~(1 << pad);
	spin_unlock_irqrestore(&xusb_padctl_lock, flags);
}
EXPORT_SYMBOL_GPL(xusb_ss_pad_deinit);

void xusb_ss_pad_init(int pad, int port_map, u32 cap)
{
	unsigned long val, flags;
	void __iomem *pad_base = IO_ADDRESS(TEGRA_XUSB_PADCTL_BASE);

	spin_lock_irqsave(&xusb_padctl_lock, flags);
	/* Check if programmed */
	if (ss_pad_inited & (1 << pad)) {
		pr_warn("Already init for ss pad %d\n", pad);
		spin_unlock_irqrestore(&xusb_padctl_lock, flags);
		return;
	}

	/* Program ss pad */
	val = readl(pad_base + XUSB_PADCTL_SS_PORT_MAP);
	val &= ~SS_PORT_MAP(pad, ~0);
	val |= SS_PORT_MAP(pad, port_map);
	writel(val, pad_base + XUSB_PADCTL_SS_PORT_MAP);

	val = readl(pad_base + XUSB_PADCTL_USB2_PORT_CAP_0);
	val &= ~USB2_OTG_PORT_CAP(port_map, ~0);
	val |= USB2_OTG_PORT_CAP(port_map, cap);
	writel(val, pad_base + XUSB_PADCTL_USB2_PORT_CAP_0);

	ss_pad_inited |= (1 << pad);
	spin_unlock_irqrestore(&xusb_padctl_lock, flags);
	pr_debug("[%s] ss pad %d\n", __func__ , pad);
	pr_debug("XUSB_PADCTL_SS_PORT_MAP = 0x%x\n"
			, readl(pad_base + XUSB_PADCTL_SS_PORT_MAP));

#ifdef CONFIG_ARCH_TEGRA_21x_SOC
	/* read and print xusb prod settings for the SS pad */
	val = readl(pad_base + XUSB_PADCTL_UPHY_USB3_ECTL_2_0(pad));
	val &= XUSB_PADCTL_UPHY_USB3_ECTL_2_0_RX_CTLE_MASK;
	pr_debug("xusb_prod port%d RX_CTLE = 0x%lx\n", pad, val);

	val = readl(pad_base + XUSB_PADCTL_UPHY_USB3_ECTL_3_0(pad));
	pr_debug("xusb_prod port%d RX_DFE = 0x%lx\n", pad, val);

	val = readl(pad_base + XUSB_PADCTL_UPHY_USB3_ECTL_4_0(pad));
	val &= XUSB_PADCTL_UPHY_USB3_ECTL_4_0_RX_CDR_CTRL_MASK;
	pr_debug("xusb_prod port%d RX_CDR_CTRL = 0x%lx\n", pad, val >> 16);

	val = readl(pad_base + XUSB_PADCTL_UPHY_USB3_ECTL_6_0(pad));
	pr_debug("xusb_prod port%d RX_EQ_CTRL_H = 0x%lx\n", pad, val);
#endif
}
EXPORT_SYMBOL_GPL(xusb_ss_pad_init);

void usb2_vbus_id_init(void)
{
	unsigned long val, flags;
	void __iomem *pad_base = IO_ADDRESS(TEGRA_XUSB_PADCTL_BASE);

	spin_lock_irqsave(&xusb_padctl_lock, flags);
	val = readl(pad_base + XUSB_PADCTL_USB2_VBUS_ID_0);
	val &= ~(VBUS_SOURCE_SELECT(~0) | ID_SOURCE_SELECT(~0));
	val |= (VBUS_SOURCE_SELECT(1) | ID_SOURCE_SELECT(1));
	writel(val, pad_base + XUSB_PADCTL_USB2_VBUS_ID_0);

	/* clear wrong status change */
	val = readl(pad_base + XUSB_PADCTL_USB2_VBUS_ID_0);
	writel(val, pad_base + XUSB_PADCTL_USB2_VBUS_ID_0);

	spin_unlock_irqrestore(&xusb_padctl_lock, flags);
	pr_debug("[%s] VBUS_ID\n", __func__);
	pr_debug("XUSB_PADCTL_USB2_VBUS_ID_0 = 0x%x\n"
			, readl(pad_base + XUSB_PADCTL_USB2_VBUS_ID_0));
}
EXPORT_SYMBOL_GPL(usb2_vbus_id_init);

#ifdef CONFIG_ARCH_TEGRA_21x_SOC
static void tegra_xusb_uphy_misc(bool ovrd, enum padctl_lane lane)
{
	unsigned long val;
	void __iomem *pad_base = IO_ADDRESS(TEGRA_XUSB_PADCTL_BASE);
	u16 misc_pad_ctl2_regs[] = {
		[PEX_P0] = XUSB_PADCTL_UPHY_MISC_PAD_P0_CTL2,
		[PEX_P1] = XUSB_PADCTL_UPHY_MISC_PAD_P1_CTL2,
		[PEX_P2] = XUSB_PADCTL_UPHY_MISC_PAD_P2_CTL2,
		[PEX_P3] = XUSB_PADCTL_UPHY_MISC_PAD_P3_CTL2,
		[PEX_P4] = XUSB_PADCTL_UPHY_MISC_PAD_P4_CTL2,
		[PEX_P5] = XUSB_PADCTL_UPHY_MISC_PAD_P5_CTL2,
		[PEX_P6] = XUSB_PADCTL_UPHY_MISC_PAD_P6_CTL2,
		[SATA_S0] = XUSB_PADCTL_UPHY_MISC_PAD_S0_CTL2,
	};

	if ((lane < LANE_MIN) || (lane > LANE_MAX)) {
		pr_warn("%s invalid lane number %d\n", __func__, lane);
		return;
	}

	pr_debug("%s lane %d override IDDQ %d\n", __func__, lane, ovrd);

	/* WAR: Override pad controls and keep UPHy under IDDQ and */
	/* SLEEP 3 state during lane ownership changes and powergating */
	val = readl(pad_base + misc_pad_ctl2_regs[lane]);
	if (ovrd) {
		/* register bit-fields are same across all lanes */
		val |= XUSB_PADCTL_UPHY_MISC_PAD_P0_CTL2_TX_IDDQ |
			XUSB_PADCTL_UPHY_MISC_PAD_P0_CTL2_RX_IDDQ |
			XUSB_PADCTL_UPHY_MISC_PAD_P0_CTL2_TX_IDDQ_OVRD |
			XUSB_PADCTL_UPHY_MISC_PAD_P0_CTL2_RX_IDDQ_OVRD |
			XUSB_PADCTL_UPHY_MISC_PAD_P0_CTL2_TX_SLEEP |
			XUSB_PADCTL_UPHY_MISC_PAD_P0_CTL2_RX_SLEEP |
			XUSB_PADCTL_UPHY_MISC_PAD_P0_CTL2_TX_PWR_OVRD |
			XUSB_PADCTL_UPHY_MISC_PAD_P0_CTL2_RX_PWR_OVRD;
		udelay(1);
	} else {
		val &= ~(XUSB_PADCTL_UPHY_MISC_PAD_P0_CTL2_TX_PWR_OVRD |
			XUSB_PADCTL_UPHY_MISC_PAD_P0_CTL2_RX_PWR_OVRD |
			XUSB_PADCTL_UPHY_MISC_PAD_P0_CTL2_TX_IDDQ_OVRD |
			XUSB_PADCTL_UPHY_MISC_PAD_P0_CTL2_RX_IDDQ_OVRD);
	}
	writel(val, pad_base + misc_pad_ctl2_regs[lane]);
}

static void usb3_pad_mux_set(u8 lane)
{
	unsigned long val;
	void __iomem *pad_base = IO_ADDRESS(TEGRA_XUSB_PADCTL_BASE);

	if (lane == USB3_LANE_NOT_ENABLED)
		return;
	val = readl(pad_base + XUSB_PADCTL_USB3_PAD_MUX_0);
	val &= ~PAD_MUX_PAD_LANE(lane, ~0);
	val |= PAD_MUX_PAD_LANE(lane, 1);
	writel(val, pad_base + XUSB_PADCTL_USB3_PAD_MUX_0);

}

static void usb3_lane_out_of_iddq(u8 lane)
{
	unsigned long val;
	void __iomem *pad_base = IO_ADDRESS(TEGRA_XUSB_PADCTL_BASE);

	if (lane == USB3_LANE_NOT_ENABLED)
		return;
	/* Bring enabled lane out of IDDQ */
	val = readl(pad_base + XUSB_PADCTL_USB3_PAD_MUX_0);
	val |= PAD_MUX_PAD_LANE_IDDQ(lane, 1);
	writel(val, pad_base + XUSB_PADCTL_USB3_PAD_MUX_0);
}

static void usb3_release_padmux_state_latch(void)
{
	unsigned long val;
	void __iomem *pad_base = IO_ADDRESS(TEGRA_XUSB_PADCTL_BASE);

	/* clear AUX_MUX_LP0 related bits in ELPG_PROGRAM_1 */
	udelay(1);

	val = readl(pad_base + XUSB_PADCTL_ELPG_PROGRAM_1);
	val &= ~XUSB_PADCTL_ELPG_PROGRAM_AUX_MUX_LP0_CLAMP_EN;
	writel(val, pad_base + XUSB_PADCTL_ELPG_PROGRAM_1);

	udelay(100);

	val &= ~XUSB_PADCTL_ELPG_PROGRAM_AUX_MUX_LP0_CLAMP_EN_EARLY;
	writel(val, pad_base + XUSB_PADCTL_ELPG_PROGRAM_1);

	udelay(100);

	val &= ~XUSB_PADCTL_ELPG_PROGRAM_AUX_MUX_LP0_VCORE_DOWN;
	writel(val, pad_base + XUSB_PADCTL_ELPG_PROGRAM_1);

}

static int tegra_xusb_padctl_phy_enable(void);
static void tegra_xusb_padctl_phy_disable(void);
static int usb3_phy_refcnt = 0x0;

void usb3_phy_pad_disable(void)
{
	unsigned long flags;
	spin_lock_irqsave(&xusb_padctl_lock, flags);

	pr_debug("%s usb3_phy_refcnt %d\n", __func__, usb3_phy_refcnt);

	usb3_phy_refcnt--;
	if (usb3_phy_refcnt == 0)
		tegra_xusb_padctl_phy_disable();

	spin_unlock_irqrestore(&xusb_padctl_lock, flags);
}

/*
 * Have below lane define used for each port
 * PCIE_LANEP6 0x6
 * PCIE_LANEP5 0x5
 * PCIE_LANEP4 0x4
 * PCIE_LANEP3 0x3
 * PCIE_LANEP2 0x2
 * PCIE_LANEP1 0x1
 * PCIE_LANEP0 0x0
 * SATA_LANE   0x8
 * NOT_SUPPORTED	0xF
 */
int usb3_phy_pad_enable(u32 lane_owner)
{
	unsigned long flags;
	int pad;
	void __iomem *pad_base = IO_ADDRESS(TEGRA_XUSB_PADCTL_BASE);

	pr_debug("%s usb3_phy_refcnt %d\n", __func__, usb3_phy_refcnt);
	spin_lock_irqsave(&xusb_padctl_lock, flags);

	if (usb3_phy_refcnt > 0)
		goto done;

	/* Program SATA pad phy */
	if ((lane_owner & 0xf000) == SATA_LANE)
		t210_sata_uphy_pll_init(true);

	tegra_xusb_padctl_phy_enable();

	/* Check PCIe/SATA pad phy programmed */
	pr_debug("[%s] PCIe pad/ SATA pad phy parameter\n", __func__);
	pr_debug("XUSB_PADCTL_UPHY_PLL_S0_CTL_1_0 = 0x%x\n"
			, readl(pad_base + XUSB_PADCTL_UPHY_PLL_S0_CTL_1_0));
	pr_debug("XUSB_PADCTL_UPHY_PLL_S0_CTL_2_0 = 0x%x\n"
			, readl(pad_base + XUSB_PADCTL_UPHY_PLL_S0_CTL_2_0));
	pr_debug("XUSB_PADCTL_UPHY_PLL_S0_CTL_4_0 = 0x%x\n"
			, readl(pad_base + XUSB_PADCTL_UPHY_PLL_S0_CTL_4_0));
	pr_debug("XUSB_PADCTL_UPHY_PLL_S0_CTL_5_0 = 0x%x\n"
			, readl(pad_base + XUSB_PADCTL_UPHY_PLL_S0_CTL_5_0));
	pr_debug("XUSB_PADCTL_UPHY_PLL_P0_CTL2_0 = 0x%x\n"
			, readl(pad_base + XUSB_PADCTL_UPHY_PLL_P0_CTL2_0));
	pr_debug("XUSB_PADCTL_UPHY_PLL_P0_CTL5_0 = 0x%x\n"
			, readl(pad_base + XUSB_PADCTL_UPHY_PLL_P0_CTL5_0));

	for (pad = 0; pad < SS_PAD_COUNT; pad++) {
		u8 lane = (lane_owner >> (pad * 4)) & 0xf;
		enum padctl_lane lane_enum = usb3_laneowner_to_lane_enum(lane);

		if (lane == USB3_LANE_NOT_ENABLED)
			continue;
		tegra_xusb_uphy_misc(true, lane_enum);
		usb3_pad_mux_set(lane);
		tegra_xusb_uphy_misc(false, lane_enum);
	}

	pr_debug("[%s] ss pad mux\n", __func__);
	pr_debug("XUSB_PADCTL_USB3_PAD_MUX_0 = 0x%x\n"
			, readl(pad_base + XUSB_PADCTL_USB3_PAD_MUX_0));

	/* Bring out of IDDQ */
	for (pad = 0; pad < SS_PAD_COUNT; pad++)
		usb3_lane_out_of_iddq((lane_owner >> (pad * 4)) & 0xf);

	pr_debug("ss lane exit IDDQ\n");
	pr_debug("XUSB_PADCTL_USB3_PAD_MUX_0 = 0x%x\n"
			, readl(pad_base + XUSB_PADCTL_USB3_PAD_MUX_0));

	pr_debug("ss release state latching\n");
	pr_debug("XUSB_PADCTL_ELPG_PROGRAM_1 = 0x%x\n"
			, readl(pad_base + XUSB_PADCTL_ELPG_PROGRAM_1));

done:
	usb3_phy_refcnt++;
	spin_unlock_irqrestore(&xusb_padctl_lock, flags);
	return 0;
}

#else
void usb3_phy_pad_disable(void) {}

int usb3_phy_pad_enable(u32 lane_owner)
{
	unsigned long val, flags;
	void __iomem *pad_base = IO_ADDRESS(TEGRA_XUSB_PADCTL_BASE);

	spin_lock_irqsave(&xusb_padctl_lock, flags);

	/* Program SATA pad phy */
	if (lane_owner & BIT(0)) {
		void __iomem *clk_base = IO_ADDRESS(TEGRA_CLK_RESET_BASE);

		val = readl(pad_base + XUSB_PADCTL_IOPHY_PLL_S0_CTL1_0);
		val &= ~XUSB_PADCTL_IOPHY_PLL_S0_CTL1_0_PLL0_REFCLK_NDIV_MASK;
		val |= XUSB_PADCTL_IOPHY_PLL_S0_CTL1_0_PLL0_REFCLK_NDIV;
		writel(val, pad_base + XUSB_PADCTL_IOPHY_PLL_S0_CTL1_0);

		val = readl(pad_base + XUSB_PADCTL_IOPHY_PLL_S0_CTL2_0);
		val &= ~(XUSB_PADCTL_IOPHY_PLL_S0_CTL2_0_XDIGCLK_SEL_MASK |
			XUSB_PADCTL_IOPHY_PLL_S0_CTL2_0_TXCLKREF_SEL |
			XUSB_PADCTL_IOPHY_PLL_S0_CTL2_0_TCLKOUT_EN |
			XUSB_PADCTL_IOPHY_PLL_S0_CTL2_0_PLL0_CP_CNTL_MASK |
			XUSB_PADCTL_IOPHY_PLL_S0_CTL2_0_PLL1_CP_CNTL_MASK);
		val |= XUSB_PADCTL_IOPHY_PLL_S0_CTL2_0_XDIGCLK_SEL |
			XUSB_PADCTL_IOPHY_PLL_S0_CTL2_0_TXCLKREF_SEL |
			XUSB_PADCTL_IOPHY_PLL_S0_CTL2_0_PLL0_CP_CNTL |
			XUSB_PADCTL_IOPHY_PLL_S0_CTL2_0_PLL1_CP_CNTL;

		writel(val, pad_base + XUSB_PADCTL_IOPHY_PLL_S0_CTL2_0);

		val = readl(pad_base + XUSB_PADCTL_IOPHY_PLL_S0_CTL3_0);
		val &= ~XUSB_PADCTL_IOPHY_PLL_S0_CTL3_0_RCAL_BYPASS;
		writel(val, pad_base + XUSB_PADCTL_IOPHY_PLL_S0_CTL3_0);

		/* Enable SATA PADPLL clocks */
		val = readl(clk_base + CLK_RST_CONTROLLER_SATA_PLL_CFG0_0);
		val &= ~SATA_PADPLL_RESET_SWCTL;
		val |= SATA_PADPLL_USE_LOCKDET | SATA_SEQ_START_STATE;
		writel(val, clk_base + CLK_RST_CONTROLLER_SATA_PLL_CFG0_0);

		udelay(1);

		val = readl(clk_base + CLK_RST_CONTROLLER_SATA_PLL_CFG0_0);
		val |= SATA_SEQ_ENABLE;
		writel(val, clk_base + CLK_RST_CONTROLLER_SATA_PLL_CFG0_0);
	}

	if ((lane_owner & BIT(1)) || (lane_owner & BIT(2))) {
		val = readl(pad_base + XUSB_PADCTL_IOPHY_PLL_P0_CTL2_0);
		val &= ~XUSB_PADCTL_IOPHY_PLL_P0_CTL2_PLL0_CP_CNTL_MASK;
		val |= XUSB_PADCTL_IOPHY_PLL_P0_CTL2_PLL0_CP_CNTL_VAL;
		writel(val, pad_base + XUSB_PADCTL_IOPHY_PLL_P0_CTL2_0);
	}

	/*
	 * program ownership of lanes owned by USB3 based on odmdata[28:30]
	 * odmdata[28] = 0 (SATA lane owner = SATA),
	 * odmdata[28] = 1 (SATA lane owner = USB3_SS port1)
	 * odmdata[29] = 0 (PCIe lane1 owner = PCIe),
	 * odmdata[29] = 1 (PCIe lane1 owner = USB3_SS port1)
	 * odmdata[30] = 0 (PCIe lane0 owner = PCIe),
	 * odmdata[30] = 1 (PCIe lane0 owner = USB3_SS port0)
	 * FIXME: Check any GPIO settings needed ?
	 */
	val = readl(pad_base + XUSB_PADCTL_USB3_PAD_MUX_0);
	/* USB3_SS port1 can either be mapped to SATA lane or PCIe lane1 */
	if (lane_owner & BIT(0)) {
		val &= ~XUSB_PADCTL_USB3_PAD_MUX_SATA_PAD_LANE0;
		val |= XUSB_PADCTL_USB3_PAD_MUX_SATA_PAD_LANE0_OWNER_USB3_SS;
	} else if (lane_owner & BIT(1)) {
		val &= ~XUSB_PADCTL_USB3_PAD_MUX_PCIE_PAD_LANE1;
		val |= XUSB_PADCTL_USB3_PAD_MUX_PCIE_PAD_LANE1_OWNER_USB3_SS;
	}
	/* USB_SS port0 is alwasy mapped to PCIe lane0 */
	if (lane_owner & BIT(2)) {
		val &= ~XUSB_PADCTL_USB3_PAD_MUX_PCIE_PAD_LANE0;
		val |= XUSB_PADCTL_USB3_PAD_MUX_PCIE_PAD_LANE0_OWNER_USB3_SS;
	}
	writel(val, pad_base + XUSB_PADCTL_USB3_PAD_MUX_0);

	/* Bring enabled lane out of IDDQ */
	val = readl(pad_base + XUSB_PADCTL_USB3_PAD_MUX_0);
	if (lane_owner & BIT(0))
		val |= XUSB_PADCTL_USB3_PAD_MUX_FORCE_SATA_PAD_IDDQ_DISABLE_MASK0;
	else if (lane_owner & BIT(1))
		val |= XUSB_PADCTL_USB3_PAD_MUX_FORCE_PCIE_PAD_IDDQ_DISABLE_MASK1;
	if (lane_owner & BIT(2))
		val |= XUSB_PADCTL_USB3_PAD_MUX_FORCE_PCIE_PAD_IDDQ_DISABLE_MASK0;
	writel(val, pad_base + XUSB_PADCTL_USB3_PAD_MUX_0);

	udelay(1);

	/* clear AUX_MUX_LP0 related bits in ELPG_PROGRAM */
	val = readl(pad_base + XUSB_PADCTL_ELPG_PROGRAM_0);
	val &= ~XUSB_PADCTL_ELPG_PROGRAM_AUX_MUX_LP0_CLAMP_EN;
	writel(val, pad_base + XUSB_PADCTL_ELPG_PROGRAM_0);

	udelay(100);

	val &= ~XUSB_PADCTL_ELPG_PROGRAM_AUX_MUX_LP0_CLAMP_EN_EARLY;
	writel(val, pad_base + XUSB_PADCTL_ELPG_PROGRAM_0);

	udelay(100);

	val &= ~XUSB_PADCTL_ELPG_PROGRAM_AUX_MUX_LP0_VCORE_DOWN;
	writel(val, pad_base + XUSB_PADCTL_ELPG_PROGRAM_0);

	usb_lanes = lane_owner;

	spin_unlock_irqrestore(&xusb_padctl_lock, flags);
	return 0;
}

#endif
EXPORT_SYMBOL_GPL(usb3_phy_pad_enable);
EXPORT_SYMBOL_GPL(usb3_phy_pad_disable);

void tegra_usb_pad_reg_update(u32 reg_offset, u32 mask, u32 val)
{
	void __iomem *pad_base = IO_ADDRESS(TEGRA_XUSB_PADCTL_BASE);
	unsigned long flags;
	u32 reg;

	spin_lock_irqsave(&xusb_padctl_reg_lock, flags);

	reg = readl(pad_base + reg_offset);
	reg &= ~mask;
	reg |= val;
	writel(reg, pad_base + reg_offset);

	spin_unlock_irqrestore(&xusb_padctl_reg_lock, flags);
}
EXPORT_SYMBOL_GPL(tegra_usb_pad_reg_update);

u32 tegra_usb_pad_reg_read(u32 reg_offset)
{
	void __iomem *pad_base = IO_ADDRESS(TEGRA_XUSB_PADCTL_BASE);
	unsigned long flags;
	u32 reg;

	spin_lock_irqsave(&xusb_padctl_reg_lock, flags);
	reg = readl(pad_base + reg_offset);
	spin_unlock_irqrestore(&xusb_padctl_reg_lock, flags);

	return reg;
}
EXPORT_SYMBOL_GPL(tegra_usb_pad_reg_read);

void tegra_usb_pad_reg_write(u32 reg_offset, u32 val)
{
	void __iomem *pad_base = IO_ADDRESS(TEGRA_XUSB_PADCTL_BASE);
	unsigned long flags;
	spin_lock_irqsave(&xusb_padctl_reg_lock, flags);
	writel(val, pad_base + reg_offset);
	spin_unlock_irqrestore(&xusb_padctl_reg_lock, flags);
}
EXPORT_SYMBOL_GPL(tegra_usb_pad_reg_write);

#ifdef CONFIG_ARCH_TEGRA_21x_SOC
int t210_sata_uphy_pll_init(bool sata_used_by_xusb)
{
	u32 val;
	int calib_timeout;
	u8 freq_ndiv;
	u8 txclkref_sel;
	void __iomem *clk_base = IO_ADDRESS(TEGRA_CLK_RESET_BASE);

	val = readl(clk_base + CLK_RST_CONTROLLER_SATA_PLL_CFG0_0);
	if (SATA_SEQ_ENABLE & val) {
		pr_info("%s: sata uphy already enabled\n", __func__);
		return 0;
	}

	pr_info("%s: init sata uphy pll\n", __func__);

	tegra_usb_pad_reg_update(XUSB_PADCTL_UPHY_PLL_S0_CTL2_0,
		S0_CTL2_PLL0_CAL_CTRL(~0),
		S0_CTL2_PLL0_CAL_CTRL(0x136));

	tegra_usb_pad_reg_update(XUSB_PADCTL_UPHY_PLL_S0_CTL5_0,
		S0_CTL5_PLL0_DCO_CTRL(~0),
		S0_CTL5_PLL0_DCO_CTRL(0x2a));

	tegra_usb_pad_reg_update(XUSB_PADCTL_UPHY_PLL_S0_CTL1_0,
		S0_CTL1_PLL0_PWR_OVRD, S0_CTL1_PLL0_PWR_OVRD);

	tegra_usb_pad_reg_update(XUSB_PADCTL_UPHY_PLL_S0_CTL2_0,
		S0_CTL2_PLL0_CAL_OVRD, S0_CTL2_PLL0_CAL_OVRD);

	tegra_usb_pad_reg_update(XUSB_PADCTL_UPHY_PLL_S0_CTL8_0,
		S0_CTL8_PLL0_RCAL_OVRD, S0_CTL8_PLL0_RCAL_OVRD);

	pr_info("%s %s uses SATA Lane\n", __func__,
			sata_used_by_xusb ? "XUSB" : "SATA");

	if (sata_used_by_xusb) {
		txclkref_sel = 0x2;
		freq_ndiv = 0x19;
	} else {
		txclkref_sel = 0x0;
		freq_ndiv = 0x1e;
	}

	tegra_usb_pad_reg_update(XUSB_PADCTL_UPHY_PLL_S0_CTL4_0,
		S0_PLL0_TXCLKREF_SEL(~0), S0_PLL0_TXCLKREF_SEL(txclkref_sel));

	tegra_usb_pad_reg_update(XUSB_PADCTL_UPHY_PLL_S0_CTL1_0,
		S0_PLL0_FREQ_NDIV(~0), S0_PLL0_FREQ_NDIV(freq_ndiv));

	tegra_usb_pad_reg_update(XUSB_PADCTL_UPHY_PLL_S0_CTL1_0,
		(S0_CTL1_PLL0_IDDQ | S0_CTL1_PLL0_SLEEP), 0);

	udelay(1);

	tegra_usb_pad_reg_update(XUSB_PADCTL_UPHY_PLL_S0_CTL2_0,
		S0_CTL2_PLL0_CAL_EN, S0_CTL2_PLL0_CAL_EN);

	calib_timeout = 20; /* 200 us in total */
	do {
		val = tegra_usb_pad_reg_read(XUSB_PADCTL_UPHY_PLL_S0_CTL2_0);
		udelay(10);
		if (--calib_timeout == 0) {
			pr_err("%s: timeout for CAL_DONE set\n", __func__);
			return -EBUSY;
		}
	} while (!(val & S0_CTL2_PLL0_CAL_DONE));

	tegra_usb_pad_reg_update(XUSB_PADCTL_UPHY_PLL_S0_CTL2_0,
		S0_CTL2_PLL0_CAL_EN, 0);

	calib_timeout = 20; /* 200 us in total */
	do {
		val = tegra_usb_pad_reg_read(XUSB_PADCTL_UPHY_PLL_S0_CTL2_0);
		udelay(10);
		if (--calib_timeout == 0) {
			pr_err("%s: timeout for CAL_DONE clear\n", __func__);
			return -EBUSY;
		}
	} while (val & S0_CTL2_PLL0_CAL_DONE);

	tegra_usb_pad_reg_update(XUSB_PADCTL_UPHY_PLL_S0_CTL1_0,
		S0_CTL1_PLL0_ENABLE, S0_CTL1_PLL0_ENABLE);

	calib_timeout = 20; /* 200 us in total */
	do {
		val = tegra_usb_pad_reg_read(XUSB_PADCTL_UPHY_PLL_S0_CTL1_0);
		udelay(10);
		if (--calib_timeout == 0) {
			pr_err("%s: timeout for LOCKDET set\n", __func__);
			return -EBUSY;
		}
	} while (!(val & S0_CTL1_PLL0_LOCKDET_STATUS));

	tegra_usb_pad_reg_update(XUSB_PADCTL_UPHY_PLL_S0_CTL8_0,
		S0_CTL8_PLL0_RCAL_EN, S0_CTL8_PLL0_RCAL_EN);

	tegra_usb_pad_reg_update(XUSB_PADCTL_UPHY_PLL_S0_CTL8_0,
		S0_CTL8_PLL0_RCAL_CLK_EN, S0_CTL8_PLL0_RCAL_CLK_EN);

	calib_timeout = 20; /* 200 us in total */
	do {
		val = tegra_usb_pad_reg_read(XUSB_PADCTL_UPHY_PLL_S0_CTL8_0);
		udelay(10);
		if (--calib_timeout == 0) {
			pr_err("%s: timeout for RCAL_DONE set\n", __func__);
			return -EBUSY;
		}
	} while (!(val & S0_CTL8_PLL0_RCAL_DONE));

	tegra_usb_pad_reg_update(XUSB_PADCTL_UPHY_PLL_S0_CTL8_0,
		S0_CTL8_PLL0_RCAL_EN, 0);

	calib_timeout = 20; /* 200 us in total */
	do {
		val = tegra_usb_pad_reg_read(XUSB_PADCTL_UPHY_PLL_S0_CTL8_0);
		udelay(10);
		if (--calib_timeout == 0) {
			pr_err("%s: timeout for RCAL_DONE clear\n", __func__);
			return -EBUSY;
		}
	} while (val & S0_CTL8_PLL0_RCAL_DONE);

	tegra_usb_pad_reg_update(XUSB_PADCTL_UPHY_PLL_S0_CTL8_0,
		S0_CTL8_PLL0_RCAL_CLK_EN, 0);

	/* Enable SATA PADPLL HW power sequencer */
	val = readl(clk_base + CLK_RST_CONTROLLER_SATA_PLL_CFG0_0);
	val &= ~SATA_PADPLL_RESET_SWCTL;
	val |= SATA_PADPLL_USE_LOCKDET | SATA_PADPLL_SLEEP_IDDQ;
	writel(val, clk_base + CLK_RST_CONTROLLER_SATA_PLL_CFG0_0);

	tegra_usb_pad_reg_update(XUSB_PADCTL_UPHY_PLL_S0_CTL1_0,
		S0_CTL1_PLL0_PWR_OVRD, 0);

	tegra_usb_pad_reg_update(XUSB_PADCTL_UPHY_PLL_S0_CTL2_0,
		S0_CTL2_PLL0_CAL_OVRD, 0);

	tegra_usb_pad_reg_update(XUSB_PADCTL_UPHY_PLL_S0_CTL8_0,
		S0_CTL8_PLL0_RCAL_OVRD, 0);

	udelay(1);
	val = readl(clk_base + CLK_RST_CONTROLLER_SATA_PLL_CFG0_0);
	val |= SATA_SEQ_ENABLE;
	writel(val, clk_base + CLK_RST_CONTROLLER_SATA_PLL_CFG0_0);

	return 0;
}
EXPORT_SYMBOL_GPL(t210_sata_uphy_pll_init);

static int pex_pll_refcnt;
static void tegra_xusb_padctl_phy_disable(void)
{
	pr_debug("%s pex_pll_refcnt %d\n", __func__, pex_pll_refcnt);
	pex_pll_refcnt--;
}

static int tegra_xusb_padctl_phy_enable(void)
{
	unsigned long val, timeout;
	void __iomem *pad_base = IO_ADDRESS(TEGRA_XUSB_PADCTL_BASE);
	void __iomem *clk_base = IO_ADDRESS(TEGRA_CLK_RESET_BASE);

	pr_debug("%s pex_pll_refcnt %d\n", __func__, pex_pll_refcnt);

	if (pex_pll_refcnt > 0)
		goto done; /* already done */

	val = readl(clk_base + CLK_RST_CONTROLLER_XUSBIO_PLL_CFG0_0);
	if (XUSBIO_SEQ_ENABLE & val) {
		pr_info("%s: pex uphy already enabled\n", __func__);
		goto done;
	}

	pr_info("%s: init pex uphy pll\n", __func__);

	/* Enable overrides to enable SW control over PLL */
	/* init UPHY, Set PWR/CAL/RCAL OVRD */
	val = readl(pad_base + XUSB_PADCTL_UPHY_PLL_P0_CTL2_0);
	val &= ~XUSB_PADCTL_UPHY_PLL_P0_CTL2_CAL_CTRL_MASK;
	val |= XUSB_PADCTL_UPHY_PLL_P0_CTL2_PLL0_CAL_CTRL;
	writel(val, pad_base + XUSB_PADCTL_UPHY_PLL_P0_CTL2_0);

	val = readl(pad_base + XUSB_PADCTL_UPHY_PLL_P0_CTL5_0);
	val &= ~XUSB_PADCTL_UPHY_PLL_P0_CTL5_DCO_CTRL_MASK;
	val |= XUSB_PADCTL_UPHY_PLL_P0_CTL5_PLL0_DCO_CTRL;
	writel(val, pad_base + XUSB_PADCTL_UPHY_PLL_P0_CTL5_0);

	val = readl(pad_base + XUSB_PADCTL_UPHY_PLL_P0_CTL1_0);
	val |= XUSB_PADCTL_UPHY_PLL_P0_CTL1_PLL0_PWR_OVRD;
	writel(val, pad_base + XUSB_PADCTL_UPHY_PLL_P0_CTL1_0);

	val = readl(pad_base + XUSB_PADCTL_UPHY_PLL_P0_CTL2_0);
	val |= XUSB_PADCTL_UPHY_PLL_P0_CTL2_PLL0_CAL_OVRD;
	writel(val, pad_base + XUSB_PADCTL_UPHY_PLL_P0_CTL2_0);

	val = readl(pad_base + XUSB_PADCTL_UPHY_PLL_P0_CTL8_0);
	val |= XUSB_PADCTL_UPHY_PLL_P0_CTL8_PLL0_RCAL_OVRD;
	writel(val, pad_base + XUSB_PADCTL_UPHY_PLL_P0_CTL8_0);

	/* Select REFCLK, TXCLKREF and Enable */
	val = readl(pad_base + XUSB_PADCTL_UPHY_PLL_P0_CTL4_0);
	val &= ~XUSB_PADCTL_UPHY_PLL_P0_CTL4_PLL0_REFCLK_SEL_MASK;
	val &= ~XUSB_PADCTL_UPHY_PLL_P0_CTL4_PLL0_TXCLKREF_SEL_MASK;
	val |= XUSB_PADCTL_UPHY_PLL_P0_CTL4_PLL0_TXCLKREF_SEL;
	val |= XUSB_PADCTL_UPHY_PLL_P0_CTL4_PLL0_TXCLKREF_EN;
	writel(val, pad_base + XUSB_PADCTL_UPHY_PLL_P0_CTL4_0);

	/* FREQ_MDIV & FREQ_NDIV programming for 500 MHz */
	val = readl(pad_base + XUSB_PADCTL_UPHY_PLL_P0_CTL1_0);
	val &= ~XUSB_PADCTL_UPHY_PLL_P0_CTL1_PLL0_FREQ_MDIV_MASK;
	val &= ~XUSB_PADCTL_UPHY_PLL_P0_CTL1_PLL0_FREQ_NDIV_MASK;
	val |= XUSB_PADCTL_UPHY_PLL_P0_CTL1_PLL0_FREQ_NDIV;
	writel(val, pad_base + XUSB_PADCTL_UPHY_PLL_P0_CTL1_0);

	/* Clear PLL0 iddq and sleep */
	val = readl(pad_base + XUSB_PADCTL_UPHY_PLL_P0_CTL1_0);
	val &= ~XUSB_PADCTL_UPHY_PLL_P0_CTL1_PLL0_IDDQ;
	writel(val, pad_base + XUSB_PADCTL_UPHY_PLL_P0_CTL1_0);
	val = readl(pad_base + XUSB_PADCTL_UPHY_PLL_P0_CTL1_0);
	val &= ~XUSB_PADCTL_UPHY_PLL_P0_CTL1_PLL0_SLEEP;
	writel(val, pad_base + XUSB_PADCTL_UPHY_PLL_P0_CTL1_0);
	udelay(1);

	val = readl(pad_base + XUSB_PADCTL_UPHY_PLL_P0_CTL4_0);
	val |= XUSB_PADCTL_UPHY_PLL_P0_CTL4_PLL0_REFCLKBUF_EN;
	writel(val, pad_base + XUSB_PADCTL_UPHY_PLL_P0_CTL4_0);

	/* Perform PLL calibration */
	/* Set PLL0_CAL_EN and wait for PLL0_CAL_DONE being set */
	val = readl(pad_base + XUSB_PADCTL_UPHY_PLL_P0_CTL2_0);
	val |= XUSB_PADCTL_UPHY_PLL_P0_CTL2_PLL0_CAL_EN;
	writel(val, pad_base + XUSB_PADCTL_UPHY_PLL_P0_CTL2_0);
	timeout = 20;
	do {
		val = readl(pad_base + XUSB_PADCTL_UPHY_PLL_P0_CTL2_0);
		udelay(10);
		if (--timeout == 0) {
			pr_err("PCIe error: timeout for PLL0_CAL_DONE set\n");
			return -EBUSY;
		}
	} while (!(val & XUSB_PADCTL_UPHY_PLL_P0_CTL2_PLL0_CAL_DONE));

	/* Clear PLL0_CAL_EN and wait for PLL0_CAL_DONE being cleared */
	val = readl(pad_base + XUSB_PADCTL_UPHY_PLL_P0_CTL2_0);
	val &= ~XUSB_PADCTL_UPHY_PLL_P0_CTL2_PLL0_CAL_EN;
	writel(val, pad_base + XUSB_PADCTL_UPHY_PLL_P0_CTL2_0);
	timeout = 20;
	do {
		val = readl(pad_base + XUSB_PADCTL_UPHY_PLL_P0_CTL2_0);
		udelay(10);
		if (--timeout == 0) {
			pr_err("PCIe error: timeout for PLL0_CAL_DONE cleared\n");
			return -EBUSY;
		}
	} while (val & XUSB_PADCTL_UPHY_PLL_P0_CTL2_PLL0_CAL_DONE);

	/* Set PLL0_ENABLE */
	val = readl(pad_base + XUSB_PADCTL_UPHY_PLL_P0_CTL1_0);
	val |= XUSB_PADCTL_UPHY_PLL_P0_CTL1_PLL0_ENABLE;
	writel(val, pad_base + XUSB_PADCTL_UPHY_PLL_P0_CTL1_0);

	/* Wait for the PLL to lock */
	timeout = 20;
	do {
		val = readl(pad_base + XUSB_PADCTL_UPHY_PLL_P0_CTL1_0);
		udelay(10);
		if (--timeout == 0) {
			pr_err("Tegra PCIe error: timeout waiting for PLL\n");
			return -EBUSY;
		}
	} while (!(val & XUSB_PADCTL_UPHY_PLL_P0_CTL1_PLL0_LOCKDET_STATUS));

	/* Perform register calibration */
	/* Set PLL0_RCAL_EN and wait for PLL0_RCAL_DONE being set */
	val = readl(pad_base + XUSB_PADCTL_UPHY_PLL_P0_CTL8_0);
	val |= XUSB_PADCTL_UPHY_PLL_P0_CTL8_PLL0_RCAL_EN;
	val |= XUSB_PADCTL_UPHY_PLL_P0_CTL8_PLL0_RCAL_CLK_EN;
	writel(val, pad_base + XUSB_PADCTL_UPHY_PLL_P0_CTL8_0);
	timeout = 10;
	do {
		val = readl(pad_base + XUSB_PADCTL_UPHY_PLL_P0_CTL8_0);
		udelay(1);
		if (--timeout == 0) {
			pr_err("PCIe error: timeout for PLL0_RCAL_DONE set\n");
			return -EBUSY;
		}
	} while (!(val & XUSB_PADCTL_UPHY_PLL_P0_CTL8_PLL0_RCAL_DONE));

	/* Clear PLL0_RCAL_EN and wait for PLL0_RCAL_DONE being cleared */
	val = readl(pad_base + XUSB_PADCTL_UPHY_PLL_P0_CTL8_0);
	val &= ~XUSB_PADCTL_UPHY_PLL_P0_CTL8_PLL0_RCAL_EN;
	writel(val, pad_base + XUSB_PADCTL_UPHY_PLL_P0_CTL8_0);
	timeout = 10;
	do {
		val = readl(pad_base + XUSB_PADCTL_UPHY_PLL_P0_CTL8_0);
		udelay(1);
		if (--timeout == 0) {
			pr_err("PCIe error: timeout for PLL0_RCAL_DONE cleared\n");
			return -EBUSY;
		}
	} while (val & XUSB_PADCTL_UPHY_PLL_P0_CTL8_PLL0_RCAL_DONE);

	val = readl(pad_base + XUSB_PADCTL_UPHY_PLL_P0_CTL8_0);
	val &= ~XUSB_PADCTL_UPHY_PLL_P0_CTL8_PLL0_RCAL_CLK_EN;
	writel(val, pad_base + XUSB_PADCTL_UPHY_PLL_P0_CTL8_0);

	/* Enable PEX PADPLL clocks */
	val = readl(clk_base + CLK_RST_CONTROLLER_XUSBIO_PLL_CFG0_0);
	val &= ~XUSBIO_CLK_ENABLE_SWCTL;
	val &= ~XUSBIO_PADPLL_RESET_SWCTL;
	val |= XUSBIO_PADPLL_USE_LOCKDET | XUSBIO_PADPLL_SLEEP_IDDQ;
	writel(val, clk_base + CLK_RST_CONTROLLER_XUSBIO_PLL_CFG0_0);

	val = readl(pad_base + XUSB_PADCTL_UPHY_PLL_P0_CTL1_0);
	val &= ~XUSB_PADCTL_UPHY_PLL_P0_CTL1_PLL0_PWR_OVRD;
	writel(val, pad_base + XUSB_PADCTL_UPHY_PLL_P0_CTL1_0);

	val = readl(pad_base + XUSB_PADCTL_UPHY_PLL_P0_CTL2_0);
	val &= ~XUSB_PADCTL_UPHY_PLL_P0_CTL2_PLL0_CAL_OVRD;
	writel(val, pad_base + XUSB_PADCTL_UPHY_PLL_P0_CTL2_0);

	val = readl(pad_base + XUSB_PADCTL_UPHY_PLL_P0_CTL8_0);
	val &= ~XUSB_PADCTL_UPHY_PLL_P0_CTL8_PLL0_RCAL_OVRD;
	writel(val, pad_base + XUSB_PADCTL_UPHY_PLL_P0_CTL8_0);

	udelay(1);
	val = readl(clk_base + CLK_RST_CONTROLLER_XUSBIO_PLL_CFG0_0);
	val |= XUSBIO_SEQ_ENABLE;
	writel(val, clk_base + CLK_RST_CONTROLLER_XUSBIO_PLL_CFG0_0);

done:
	pex_pll_refcnt++;
	return 0;
}
#else
static void tegra_xusb_padctl_phy_disable(void)
{

}
static int tegra_xusb_padctl_phy_enable(void)
{
	unsigned long val, timeout;
	void __iomem *pad_base = IO_ADDRESS(TEGRA_XUSB_PADCTL_BASE);

	/* set up PLL inputs in PLL_CTL1 */
	val = readl(pad_base + XUSB_PADCTL_IOPHY_PLL_P0_CTL1_0);
	val &= ~XUSB_PADCTL_IOPHY_PLL_P0_CTL1_REFCLK_SEL_MASK;
	val |= XUSB_PADCTL_IOPHY_PLL_P0_CTL1_REFCLK_SEL;
	writel(val, pad_base + XUSB_PADCTL_IOPHY_PLL_P0_CTL1_0);

	/* set TX ref sel to div5 in PLL_CTL2 */
	/* T124 is div5 while T30 is div10,beacuse CAR will divide 2 */
	/* in GEN1 mode in T124,and if div10,it will be 125MHZ */
	val = readl(pad_base + XUSB_PADCTL_IOPHY_PLL_P0_CTL2_0);
	val |= (XUSB_PADCTL_IOPHY_PLL_P0_CTL2_REFCLKBUF_EN |
		XUSB_PADCTL_IOPHY_PLL_P0_CTL2_TXCLKREF_EN |
		XUSB_PADCTL_IOPHY_PLL_P0_CTL2_TXCLKREF_SEL);
	writel(val, pad_base + XUSB_PADCTL_IOPHY_PLL_P0_CTL2_0);
#ifdef CONFIG_ARCH_TEGRA_13x_SOC
	/* recommended prod setting */
	val = readl(pad_base + XUSB_PADCTL_IOPHY_PLL_P0_CTL2_0);
	val &= ~XUSB_PADCTL_IOPHY_PLL_P0_CTL2_PLL0_CP_CNTL_MASK;
	val |= XUSB_PADCTL_IOPHY_PLL_P0_CTL2_PLL0_CP_CNTL_VAL;
	writel(val, pad_base + XUSB_PADCTL_IOPHY_PLL_P0_CTL2_0);
#endif
	/* take PLL out of reset */
	val = readl(pad_base + XUSB_PADCTL_IOPHY_PLL_P0_CTL1_0);
	val |= XUSB_PADCTL_IOPHY_PLL_P0_CTL1_PLL_RST_;
	writel(val, pad_base + XUSB_PADCTL_IOPHY_PLL_P0_CTL1_0);

	/* Wait for the PLL to lock */
	timeout = 300;
	do {
		val = readl(pad_base + XUSB_PADCTL_IOPHY_PLL_P0_CTL1_0);
		udelay(100);
		if (--timeout == 0) {
			pr_err("Tegra PCIe error: timeout waiting for PLL\n");
			return -EBUSY;
		}
	} while (!(val & XUSB_PADCTL_IOPHY_PLL_P0_CTL1_PLL0_LOCKDET));

	return 0;
}
#endif

#ifdef CONFIG_ARCH_TEGRA_21x_SOC
static void t210_padctl_force_sata_seq(bool force_off)
{
	u32 val;
	void __iomem *clk_base = IO_ADDRESS(TEGRA_CLK_RESET_BASE);

	val = readl(clk_base +
			CLK_RST_CONTROLLER_SATA_PLL_CFG0_0);

	if (force_off)
		val |= (SATA_SEQ_IN_SWCTL |
				SATA_SEQ_RESET_INPUT_VALUE |
				SATA_SEQ_LANE_PD_INPUT_VALUE |
				SATA_SEQ_PADPLL_PD_INPUT_VALUE);
	else
		val &= ~(SATA_SEQ_IN_SWCTL |
				SATA_SEQ_RESET_INPUT_VALUE |
				SATA_SEQ_LANE_PD_INPUT_VALUE |
				SATA_SEQ_PADPLL_PD_INPUT_VALUE);

	writel(val, clk_base +
			CLK_RST_CONTROLLER_SATA_PLL_CFG0_0);
}

static void t210_padctl_enable_sata_idle_detector(bool enable)
{
	if (enable)
		tegra_usb_pad_reg_update(
			XUSB_PADCTL_UPHY_MISC_PAD_S0_CTL_1_0,
			AUX_RX_TERM_EN | AUX_RX_MODE_OVRD |
				AUX_TX_IDDQ | AUX_TX_IDDQ_OVRD |
				AUX_RX_IDLE_EN,
			AUX_RX_IDLE_EN);
	else
		tegra_usb_pad_reg_update(
			XUSB_PADCTL_UPHY_MISC_PAD_S0_CTL_1_0,
			AUX_RX_TERM_EN | AUX_RX_MODE_OVRD |
				AUX_TX_IDDQ | AUX_TX_IDDQ_OVRD |
				AUX_RX_IDLE_EN,
			AUX_RX_TERM_EN | AUX_RX_MODE_OVRD |
				AUX_TX_IDDQ | AUX_TX_IDDQ_OVRD);
}
#endif

int tegra_padctl_init_sata_pad(void)
{
#ifdef CONFIG_ARCH_TEGRA_21x_SOC
	unsigned long flags;

	if (sata_usb_pad_pll_reset_deassert())
		pr_err("%s: fail to deassert sata uphy\n",
			__func__);

	spin_lock_irqsave(&xusb_padctl_lock, flags);
	if (t210_sata_uphy_pll_init(false))
		pr_err("%s: fail to init sata uphy\n",
			__func__);
	spin_unlock_irqrestore(&xusb_padctl_lock, flags);

	/* this is only to decrease refcnt */
	sata_usb_pad_pll_reset_assert();

	spin_lock_irqsave(&xusb_padctl_lock, flags);
	tegra_usb_pad_reg_update(
		XUSB_PADCTL_UPHY_MISC_PAD_S0_CTL_1_0,
		AUX_RX_MODE_OVRD | AUX_RX_IDLE_EN |
			AUX_RX_IDLE_TH(0x3),
		AUX_RX_MODE_OVRD | AUX_RX_IDLE_EN |
			AUX_RX_IDLE_TH(0x1));
	usb3_lane_out_of_iddq(SATA_S0);
	usb3_release_padmux_state_latch();
	udelay(200);
	tegra_usb_pad_reg_update(
		XUSB_PADCTL_UPHY_MISC_PAD_S0_CTL_4_0,
		RX_TERM_EN | RX_TERM_OVRD,
		RX_TERM_EN | RX_TERM_OVRD);
	t210_padctl_force_sata_seq(false);
	t210_padctl_enable_sata_idle_detector(true);
	spin_unlock_irqrestore(&xusb_padctl_lock, flags);
#endif

	return 0;
}
EXPORT_SYMBOL_GPL(tegra_padctl_init_sata_pad);

int tegra_padctl_enable_sata_pad(bool enable)
{
#ifdef CONFIG_ARCH_TEGRA_21x_SOC
	unsigned long flags;

	spin_lock_irqsave(&xusb_padctl_lock, flags);

	if (enable) {
		t210_padctl_enable_sata_idle_detector(true);
		t210_padctl_force_sata_seq(false);
		tegra_xusb_uphy_misc(false, SATA_S0);
	} else {
		t210_padctl_force_sata_seq(true);
		tegra_xusb_uphy_misc(true, SATA_S0);
		t210_padctl_enable_sata_idle_detector(false);
	}

	spin_unlock_irqrestore(&xusb_padctl_lock, flags);
#endif

	return 0;
}
EXPORT_SYMBOL_GPL(tegra_padctl_enable_sata_pad);

static int tegra_pcie_lane_iddq(bool enable, int lane_owner)
{
	unsigned long val;
	void __iomem *pad_base = IO_ADDRESS(TEGRA_XUSB_PADCTL_BASE);

#if !defined(CONFIG_ARCH_TEGRA_21x_SOC)
	/*
	 * USB 3.0 lanes and PCIe lanes are shared
	 *
	 * Honour the XUSB lane ownership information while modifying
	 * the controls for PCIe
	 */
	bool pcie_modify_lane0_iddq = (usb_lanes & BIT(2)) ? false : true;
	bool pcie_modify_lane1_iddq =
		(!(usb_lanes & BIT(0)) && (usb_lanes & BIT(1))) ? false : true;
#endif

	val = readl(pad_base + XUSB_PADCTL_USB3_PAD_MUX_0);
	switch (lane_owner) {
#ifdef CONFIG_ARCH_TEGRA_21x_SOC
	case PCIE_LANES_X4_X1:
	if (enable)
		val |=
		XUSB_PADCTL_USB3_PAD_MUX_FORCE_PCIE_PAD_IDDQ_DISABLE_MASK0;
	else
		val &=
		~XUSB_PADCTL_USB3_PAD_MUX_FORCE_PCIE_PAD_IDDQ_DISABLE_MASK0;
	case PCIE_LANES_X4_X0:
	if (enable)
		val |=
		(XUSB_PADCTL_USB3_PAD_MUX_FORCE_PCIE_PAD_IDDQ_DISABLE_MASK1 |
		XUSB_PADCTL_USB3_PAD_MUX_FORCE_PCIE_PAD_IDDQ_DISABLE_MASK2 |
		XUSB_PADCTL_USB3_PAD_MUX_FORCE_PCIE_PAD_IDDQ_DISABLE_MASK3 |
		XUSB_PADCTL_USB3_PAD_MUX_FORCE_PCIE_PAD_IDDQ_DISABLE_MASK4);
	else
		val &=
		~(XUSB_PADCTL_USB3_PAD_MUX_FORCE_PCIE_PAD_IDDQ_DISABLE_MASK1 |
		XUSB_PADCTL_USB3_PAD_MUX_FORCE_PCIE_PAD_IDDQ_DISABLE_MASK2 |
		XUSB_PADCTL_USB3_PAD_MUX_FORCE_PCIE_PAD_IDDQ_DISABLE_MASK3 |
		XUSB_PADCTL_USB3_PAD_MUX_FORCE_PCIE_PAD_IDDQ_DISABLE_MASK4);
		break;
	case PCIE_LANES_X2_X1:
	if (enable)
		val |=
		XUSB_PADCTL_USB3_PAD_MUX_FORCE_PCIE_PAD_IDDQ_DISABLE_MASK0;
	else
		val &=
		~XUSB_PADCTL_USB3_PAD_MUX_FORCE_PCIE_PAD_IDDQ_DISABLE_MASK0;
	case PCIE_LANES_X2_X0:
	if (enable)
		val |=
		(XUSB_PADCTL_USB3_PAD_MUX_FORCE_PCIE_PAD_IDDQ_DISABLE_MASK1 |
		XUSB_PADCTL_USB3_PAD_MUX_FORCE_PCIE_PAD_IDDQ_DISABLE_MASK2);
	else
		val &=
		~(XUSB_PADCTL_USB3_PAD_MUX_FORCE_PCIE_PAD_IDDQ_DISABLE_MASK1 |
		XUSB_PADCTL_USB3_PAD_MUX_FORCE_PCIE_PAD_IDDQ_DISABLE_MASK2);
		break;
	case PCIE_LANES_X0_X1:
	if (enable)
		val |=
		XUSB_PADCTL_USB3_PAD_MUX_FORCE_PCIE_PAD_IDDQ_DISABLE_MASK0;
	else
		val &=
		~XUSB_PADCTL_USB3_PAD_MUX_FORCE_PCIE_PAD_IDDQ_DISABLE_MASK0;
		break;
#else
	case PCIE_LANES_X4_X1:
	if (pcie_modify_lane0_iddq) {
		if (enable)
			val |=
			XUSB_PADCTL_USB3_PAD_MUX_FORCE_PCIE_PAD_IDDQ_DISABLE_MASK0;
		else
			val &=
			~XUSB_PADCTL_USB3_PAD_MUX_FORCE_PCIE_PAD_IDDQ_DISABLE_MASK0;
	}
	case PCIE_LANES_X4_X0:
	if (pcie_modify_lane1_iddq) {
		if (enable)
			val |=
			XUSB_PADCTL_USB3_PAD_MUX_FORCE_PCIE_PAD_IDDQ_DISABLE_MASK1;
		else
			val &=
			~XUSB_PADCTL_USB3_PAD_MUX_FORCE_PCIE_PAD_IDDQ_DISABLE_MASK1;
	}
	case PCIE_LANES_X2_X1:
	if (enable)
		val |=
		XUSB_PADCTL_USB3_PAD_MUX_FORCE_PCIE_PAD_IDDQ_DISABLE_MASK2;
	else
		val &=
		~XUSB_PADCTL_USB3_PAD_MUX_FORCE_PCIE_PAD_IDDQ_DISABLE_MASK2;
	case PCIE_LANES_X2_X0:
	if (enable)
		val |=
		(XUSB_PADCTL_USB3_PAD_MUX_FORCE_PCIE_PAD_IDDQ_DISABLE_MASK3 |
		XUSB_PADCTL_USB3_PAD_MUX_FORCE_PCIE_PAD_IDDQ_DISABLE_MASK4);
	else
		val &=
		~(XUSB_PADCTL_USB3_PAD_MUX_FORCE_PCIE_PAD_IDDQ_DISABLE_MASK3 |
		XUSB_PADCTL_USB3_PAD_MUX_FORCE_PCIE_PAD_IDDQ_DISABLE_MASK4);
		break;
	case PCIE_LANES_X0_X1:
	if (enable)
		val |=
		XUSB_PADCTL_USB3_PAD_MUX_FORCE_PCIE_PAD_IDDQ_DISABLE_MASK2;
	else
		val &=
		~XUSB_PADCTL_USB3_PAD_MUX_FORCE_PCIE_PAD_IDDQ_DISABLE_MASK2;
		break;
#endif
	default:
		pr_err("Tegra PCIe IDDQ error: wrong lane config\n");
		return -ENXIO;
	}
	writel(val, pad_base + XUSB_PADCTL_USB3_PAD_MUX_0);
	return 0;
}

#ifdef CONFIG_ARCH_TEGRA_21x_SOC
static void tegra_pcie_lane_aux_idle(bool ovdr, int lane)
{
	void __iomem *pad_base = IO_ADDRESS(TEGRA_XUSB_PADCTL_BASE);
	unsigned long val;
	u16 misc_pad_ctl1_regs[] = {
		[PEX_P0] = XUSB_PADCTL_UPHY_MISC_PAD_P0_CTL2,
		[PEX_P1] = XUSB_PADCTL_UPHY_MISC_PAD_P1_CTL2,
		[PEX_P2] = XUSB_PADCTL_UPHY_MISC_PAD_P2_CTL2,
		[PEX_P3] = XUSB_PADCTL_UPHY_MISC_PAD_P3_CTL2,
		[PEX_P4] = XUSB_PADCTL_UPHY_MISC_PAD_P4_CTL2,
	};

	if (!ovdr)
		return;
	/* reduce idle detect threshold for compliance purpose */
	val = readl(pad_base + misc_pad_ctl1_regs[lane]);
	val &= ~XUSB_PADCTL_UPHY_MISC_PAD_P0_CTL1_AUX_RX_IDLE_TH_MASK;
	val |= XUSB_PADCTL_UPHY_MISC_PAD_P0_CTL1_AUX_RX_IDLE_TH;
	writel(val, pad_base + misc_pad_ctl1_regs[lane]);
}

bool tegra_phy_get_lane_rdet(u8 lane_num)
{
	u32 data;
	void __iomem *pad_base = IO_ADDRESS(TEGRA_XUSB_PADCTL_BASE);

	switch (lane_num) {
	case 0:
		data = readl(pad_base + XUSB_PADCTL_UPHY_MISC_PAD_P0_CTL1);
		data = data &
			XUSB_PADCTL_UPHY_MISC_PAD_P0_CTL1_AUX_TX_RDET_STATUS;
		break;
	case 1:
		data = readl(pad_base + XUSB_PADCTL_UPHY_MISC_PAD_P1_CTL1);
		data = data &
			XUSB_PADCTL_UPHY_MISC_PAD_P1_CTL1_AUX_TX_RDET_STATUS;
		break;
	case 2:
		data = readl(pad_base + XUSB_PADCTL_UPHY_MISC_PAD_P2_CTL1);
		data = data &
			XUSB_PADCTL_UPHY_MISC_PAD_P2_CTL1_AUX_TX_RDET_STATUS;
		break;
	case 3:
		data = readl(pad_base + XUSB_PADCTL_UPHY_MISC_PAD_P3_CTL1);
		data = data &
			XUSB_PADCTL_UPHY_MISC_PAD_P3_CTL1_AUX_TX_RDET_STATUS;
		break;
	case 4:
		data = readl(pad_base + XUSB_PADCTL_UPHY_MISC_PAD_P4_CTL1);
		data = data &
			XUSB_PADCTL_UPHY_MISC_PAD_P4_CTL1_AUX_TX_RDET_STATUS;
		break;
	default:
		return 0;
	}
	return (!(!data));
}
EXPORT_SYMBOL_GPL(tegra_phy_get_lane_rdet);
#endif

static void tegra_pcie_lane_misc_pad_override(bool ovdr, int lane_owner)
{
#ifdef CONFIG_ARCH_TEGRA_21x_SOC
	switch (lane_owner) {
	case PCIE_LANES_X4_X1:
		tegra_xusb_uphy_misc(ovdr, PEX_P0);
		tegra_pcie_lane_aux_idle(ovdr, PEX_P0);
		/* fall through */
	case PCIE_LANES_X4_X0:
		tegra_xusb_uphy_misc(ovdr, PEX_P1);
		tegra_pcie_lane_aux_idle(ovdr, PEX_P1);
		tegra_xusb_uphy_misc(ovdr, PEX_P2);
		tegra_pcie_lane_aux_idle(ovdr, PEX_P2);
		tegra_xusb_uphy_misc(ovdr, PEX_P3);
		tegra_pcie_lane_aux_idle(ovdr, PEX_P3);
		tegra_xusb_uphy_misc(ovdr, PEX_P4);
		tegra_pcie_lane_aux_idle(ovdr, PEX_P4);
		break;
	case PCIE_LANES_X2_X1:
		tegra_xusb_uphy_misc(ovdr, PEX_P0);
		tegra_pcie_lane_aux_idle(ovdr, PEX_P0);
		/* fall through */
	case PCIE_LANES_X2_X0:
		tegra_xusb_uphy_misc(ovdr, PEX_P1);
		tegra_pcie_lane_aux_idle(ovdr, PEX_P1);
		tegra_xusb_uphy_misc(ovdr, PEX_P2);
		tegra_pcie_lane_aux_idle(ovdr, PEX_P2);
		break;
	case PCIE_LANES_X0_X1:
		tegra_xusb_uphy_misc(ovdr, PEX_P0);
		tegra_pcie_lane_aux_idle(ovdr, PEX_P0);
		break;
	}
#endif
}

int pcie_phy_pad_enable(bool enable, int lane_owner)
{
	unsigned long val, flags;
	void __iomem *pad_base = IO_ADDRESS(TEGRA_XUSB_PADCTL_BASE);
	int ret = 0;
#if !defined(CONFIG_ARCH_TEGRA_21x_SOC)
	bool pcie_modify_lane0_owner, pcie_modify_lane1_owner;
#endif

	spin_lock_irqsave(&xusb_padctl_lock, flags);

#if !defined(CONFIG_ARCH_TEGRA_21x_SOC)
	/*
	 * USB 3.0 lanes and PCIe lanes are shared
	 *
	 * Honour the XUSB lane ownership information while modifying
	 * the controls for PCIe
	 */
	pcie_modify_lane0_owner = (usb_lanes & BIT(2)) ? false : true;
	pcie_modify_lane1_owner =
		(!(usb_lanes & BIT(0)) && (usb_lanes & BIT(1))) ? false : true;
#endif

	if (enable) {
		ret = tegra_xusb_padctl_phy_enable();
		if (ret)
			goto exit;
	}
	ret = tegra_pcie_lane_iddq(enable, lane_owner);
	if (ret || !enable) {
		if (!enable) {
			tegra_xusb_padctl_phy_disable();
			tegra_pcie_lane_misc_pad_override(true, lane_owner);
		}
		goto exit;
	}

	/* clear AUX_MUX_LP0 related bits in ELPG_PROGRAM */
#ifndef CONFIG_ARCH_TEGRA_21x_SOC
	val = readl(pad_base + XUSB_PADCTL_ELPG_PROGRAM_0);
	val &= ~XUSB_PADCTL_ELPG_PROGRAM_AUX_MUX_LP0_CLAMP_EN;
	writel(val, pad_base + XUSB_PADCTL_ELPG_PROGRAM_0);
	udelay(1);
	val &= ~XUSB_PADCTL_ELPG_PROGRAM_AUX_MUX_LP0_CLAMP_EN_EARLY;
	writel(val, pad_base + XUSB_PADCTL_ELPG_PROGRAM_0);
	udelay(100);
	val &= ~XUSB_PADCTL_ELPG_PROGRAM_AUX_MUX_LP0_VCORE_DOWN;
	writel(val, pad_base + XUSB_PADCTL_ELPG_PROGRAM_0);
	udelay(100);
#endif
	tegra_pcie_lane_misc_pad_override(true, lane_owner);

	val = readl(pad_base + XUSB_PADCTL_USB3_PAD_MUX_0);
	switch (lane_owner) {
#ifdef CONFIG_ARCH_TEGRA_21x_SOC
	case PCIE_LANES_X4_X1:
		val &= ~XUSB_PADCTL_USB3_PAD_MUX_PCIE_PAD_LANE0;
	case PCIE_LANES_X4_X0:
		val |= (XUSB_PADCTL_USB3_PAD_MUX_PCIE_PAD_LANE1 |
			XUSB_PADCTL_USB3_PAD_MUX_PCIE_PAD_LANE2 |
			XUSB_PADCTL_USB3_PAD_MUX_PCIE_PAD_LANE3 |
			XUSB_PADCTL_USB3_PAD_MUX_PCIE_PAD_LANE4);
		break;
	case PCIE_LANES_X2_X1:
		val &= ~XUSB_PADCTL_USB3_PAD_MUX_PCIE_PAD_LANE0;
	case PCIE_LANES_X2_X0:
		val |= (XUSB_PADCTL_USB3_PAD_MUX_PCIE_PAD_LANE1 |
			XUSB_PADCTL_USB3_PAD_MUX_PCIE_PAD_LANE2);
		break;
	case PCIE_LANES_X0_X1:
		val &= ~XUSB_PADCTL_USB3_PAD_MUX_PCIE_PAD_LANE0;
		break;
#else
	case PCIE_LANES_X4_X1:
		if (pcie_modify_lane0_owner)
			val &= ~XUSB_PADCTL_USB3_PAD_MUX_PCIE_PAD_LANE0;
	case PCIE_LANES_X4_X0:
		if (pcie_modify_lane1_owner)
			val &= ~XUSB_PADCTL_USB3_PAD_MUX_PCIE_PAD_LANE1;
	case PCIE_LANES_X2_X1:
		val &= ~XUSB_PADCTL_USB3_PAD_MUX_PCIE_PAD_LANE2;
	case PCIE_LANES_X2_X0:
		val &= ~(XUSB_PADCTL_USB3_PAD_MUX_PCIE_PAD_LANE3 |
			XUSB_PADCTL_USB3_PAD_MUX_PCIE_PAD_LANE4);
		break;
	case PCIE_LANES_X0_X1:
		val &= ~XUSB_PADCTL_USB3_PAD_MUX_PCIE_PAD_LANE2;
		break;
#endif
	default:
		pr_err("Tegra PCIe error: wrong lane config\n");
		ret = -ENXIO;
		goto exit;
	}
	writel(val, pad_base + XUSB_PADCTL_USB3_PAD_MUX_0);
	tegra_pcie_lane_misc_pad_override(false, lane_owner);
exit:
	spin_unlock_irqrestore(&xusb_padctl_lock, flags);
	return ret;
}
EXPORT_SYMBOL_GPL(pcie_phy_pad_enable);

void xusb_enable_pad_protection(bool devmode)
{
	u32 otgpad_ctl0 = tegra_usb_pad_reg_read(
			XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD_CTL1(0));

	otgpad_ctl0 &= ~(VREG_FIX18 | VREG_LEV);

	otgpad_ctl0 |= devmode ? VREG_LEV_EN : VREG_FIX18;

	tegra_usb_pad_reg_write(
		XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD_CTL1(0), otgpad_ctl0);
}
EXPORT_SYMBOL_GPL(xusb_enable_pad_protection);

#ifdef CONFIG_ARCH_TEGRA_21x_SOC

static int
tegra_padctl_enable_regulator(struct platform_device *pdev)
{
	int err = 0;
	struct tegra_padctl *padctl;

	padctl = platform_get_drvdata(pdev);

	err = regulator_bulk_enable(padctl->soc_data->num_regulator,
		padctl->regulator);
	if (err)
		dev_err(&pdev->dev, "fail to enable regulator %d\n", err);

	return err;
}

static void
tegra_padctl_disable_regulator(struct platform_device *pdev)
{
	int err = 0;
	struct tegra_padctl *padctl;

	padctl = platform_get_drvdata(pdev);

	err = regulator_bulk_disable(padctl->soc_data->num_regulator,
		padctl->regulator);
	if (err)
		dev_err(&pdev->dev, "fail to disable regulator %d\n", err);
}

static int
tegra_padctl_enable_plle(struct platform_device *pdev)
{
	int err = 0;
	struct tegra_padctl *padctl;

	padctl = platform_get_drvdata(pdev);

	err = clk_enable(padctl->plle_clk);
	if (err)
		dev_err(&pdev->dev, "fail to enable plle clock\n");

	return err;
}

static void
tegra_padctl_disable_plle(struct platform_device *pdev)
{
	struct tegra_padctl *padctl;

	padctl = platform_get_drvdata(pdev);

	clk_disable(padctl->plle_clk);
}

static int
tegra_padctl_enable_uphy_pll(struct platform_device *pdev)
{
	int err = 0;
	struct tegra_padctl *padctl;

	padctl = platform_get_drvdata(pdev);

	err = pex_usb_pad_pll_reset_deassert();
	if (err) {
		dev_err(&pdev->dev, "fail to deassert pex pll\n");
		goto done;
	}

	err = sata_usb_pad_pll_reset_deassert();
	if (err) {
		dev_err(&pdev->dev, "fail to deassert sata pll\n");
		goto done;
	}

	if ((padctl->lane_owner & 0xf000) != SATA_LANE) {
		/* sata driver owns sata lane */
		t210_sata_uphy_pll_init(false);
		usb3_lane_out_of_iddq(SATA_S0);
		t210_padctl_enable_sata_idle_detector(false);
		t210_padctl_force_sata_seq(true);
	}

	usb3_phy_pad_enable(padctl->lane_owner);

	if (padctl->lane_map)
		pcie_phy_pad_enable(true, padctl->lane_map);

	usb3_release_padmux_state_latch();

done:
	return err;
}

static void
tegra_padctl_disable_uphy_pll(struct platform_device *pdev)
{
	struct tegra_padctl *padctl;

	padctl = platform_get_drvdata(pdev);

	usb3_phy_pad_disable();

	if (padctl->lane_map)
		pcie_phy_pad_enable(false, padctl->lane_map);

	/* this doesn't assert uphy pll if sequencers are enabled */
	if (pex_usb_pad_pll_reset_assert())
		dev_err(&pdev->dev, "fail to assert pex pll\n");
	if (sata_usb_pad_pll_reset_assert())
		dev_err(&pdev->dev, "fail to assert sata pll\n");
}

static int
tegra_padctl_enable_plle_hw(struct platform_device *pdev)
{
	int err = 0;
	struct tegra_padctl *padctl;

	padctl = platform_get_drvdata(pdev);

	err = clk_enable(padctl->plle_hw_clk);
	if (err)
		dev_err(&pdev->dev, "fail to enable plle_hw\n");

	return err;
}

static void
tegra_padctl_disable_plle_hw(struct platform_device *pdev)
{
	struct tegra_padctl *padctl;

	padctl = platform_get_drvdata(pdev);

	clk_disable(padctl->plle_hw_clk);
}

static int
tegra_padctl_uphy_init(struct platform_device *pdev)
{
	int err = 0;

	err = tegra_padctl_enable_plle(pdev);
	if (err)
		goto done;

	err = tegra_padctl_enable_uphy_pll(pdev);
	if (err)
		goto fail1;

	err = tegra_padctl_enable_plle_hw(pdev);
	if (err)
		goto fail2;

	goto done;

fail2:
	tegra_padctl_disable_uphy_pll(pdev);
fail1:
	tegra_padctl_disable_plle(pdev);
done:
	return err;
}

static void
tegra_padctl_uphy_deinit(struct platform_device *pdev)
{
	tegra_padctl_disable_plle_hw(pdev);
	tegra_padctl_disable_uphy_pll(pdev);
	tegra_padctl_disable_plle(pdev);
}

static int
tegra_padctl_get_regulator(struct platform_device *pdev)
{
	int err = 0;
	struct tegra_padctl *padctl;
	const struct tegra_padctl_soc_data *soc_data;
	size_t size;
	int i;

	padctl = platform_get_drvdata(pdev);
	soc_data = padctl->soc_data;

	size = soc_data->num_regulator *
		sizeof(struct regulator_bulk_data);
	padctl->regulator = devm_kzalloc(&pdev->dev, size, GFP_ATOMIC);
	if (IS_ERR(padctl->regulator)) {
		dev_err(&pdev->dev, "fail to alloc regulator\n");
		err = -ENOMEM;
		goto done;
	}

	for (i = 0; i < soc_data->num_regulator; i++)
		padctl->regulator[i].supply =
			soc_data->regulator_name[i];

	err = devm_regulator_bulk_get(&pdev->dev, soc_data->num_regulator,
		padctl->regulator);
	if (err) {
		dev_err(&pdev->dev, "fail to get regulator %d\n", err);
		goto done;
	}

done:
	return err;
}

static char * const tegra210_padctl_regulator_name[] = {
	"avdd_pll_uerefe", "hvdd_pex_pll_e",
	"dvdd_pex_pll", "hvddio_pex", "dvddio_pex",
	"hvdd_sata", "dvdd_sata_pll", "hvddio_sata", "dvddio_sata"
};

static const struct tegra_padctl_soc_data tegra210_padctl_data = {
	.regulator_name = tegra210_padctl_regulator_name,
	.num_regulator =
		ARRAY_SIZE(tegra210_padctl_regulator_name),
};

static struct of_device_id tegra_padctl_of_match[] = {
	{
		.compatible = "nvidia,tegra210-padctl",
		.data = &tegra210_padctl_data,
	},
	{ },
};

#define MASK_SHIFT(x , mask, shift) ((x & mask) << shift)
#define SET_LANE(padctl, lane, ss_node)	\
	(padctl->lane_owner |= \
	MASK_SHIFT(lane, 0xF, ss_node * 4))

static int tegra_padctl_parse_dt(struct platform_device *pdev,
					struct tegra_padctl *padctl)
{
	u32 lane_map = 0, ss_node = 0;
	u32 lane = 0, index = 0, count = 0;
	int ret;
	s32 v = 0;
	bool enable_sata_port;
	struct device_node *node = pdev->dev.of_node;

	count = of_property_count_u32(node, "nvidia,ss_ports");
	if (count < 4 || count % 4 != 0) {
		pr_err("nvidia,ss_ports not found or invalid count %d\n",
						count);
		return -ENODEV;
	}

	do {
		ret = of_property_read_u32_index(node, "nvidia,ss_ports",
						index, &ss_node);
		index += 2;
		ret = of_property_read_u32_index(node, "nvidia,ss_ports",
						index, &lane);
		if (ss_node != TEGRA_XHCI_UNUSED_PORT)
			SET_LANE(padctl, lane, ss_node);
		else
			SET_LANE(padctl, TEGRA_XHCI_UNUSED_LANE, ss_node);
		index += 2;
	} while (index < count);

	if (of_property_read_u32(node, "nvidia,lane-map",
		(u32 *) &lane_map))
		lane_map = 0;

	enable_sata_port = of_property_read_bool(node,
				"nvidia,enable-sata-port");

	if ((padctl->lane_owner == 0xffff) && (lane_map == 0)
				&& !enable_sata_port)
		return -ENODEV;

	if (of_property_read_s32(node, "nvidia,hs_curr_level_offset", &v) == 0) {
		dev_dbg(&pdev->dev, "HS current level offset %d\n", v);
		padctl->hs_curr_level_offset = v;
	}

	padctl->lane_map = lane_map;
	padctl->enable_sata_port = enable_sata_port;

	return 0;
}

static int
tegra_padctl_probe(struct platform_device *pdev)
{
	int err = 0;
	const struct of_device_id *match;
	struct clk *plle_clk;
	struct clk *plle_hw_clk;

	padctl = devm_kzalloc(&pdev->dev, sizeof(*padctl),
			GFP_KERNEL);
	if (IS_ERR(padctl)) {
		dev_err(&pdev->dev, "fail to alloc padctl struct\n");
		err = -ENOMEM;
		goto done;
	}
	padctl->pdev = pdev;
	platform_set_drvdata(pdev, padctl);

	err = tegra_padctl_parse_dt(pdev, padctl);
	if (err)
		return err;

	match = of_match_device(tegra_padctl_of_match, &pdev->dev);
	if (!match) {
		dev_err(&pdev->dev, "no device match\n");
		return -ENODEV;
	}
	padctl->soc_data = match->data;

	/* get regulators */
	err = tegra_padctl_get_regulator(pdev);
	if (err)
		goto done;

	/* get pll_e */
	plle_clk = devm_clk_get(&pdev->dev, "pll_e");
	if (IS_ERR(plle_clk)) {
		dev_err(&pdev->dev, "pll_e not found\n");
		err = -ENODEV;
		goto done;
	}
	padctl->plle_clk = plle_clk;

	/* get pll_e_hw */
	plle_hw_clk = clk_get_sys(NULL, "pll_e_hw");
	if (IS_ERR(plle_hw_clk)) {
		dev_err(&pdev->dev, "pll_e_hw not found\n");
		err = -ENODEV;
		goto done;
	}
	padctl->plle_hw_clk = plle_hw_clk;

	err = tegra_padctl_enable_regulator(pdev);
	if (err)
		goto done;

	err = tegra_padctl_uphy_init(pdev);

done:
	return err;
}

static int
tegra_padctl_remove(struct platform_device *pdev)
{
	tegra_padctl_uphy_deinit(pdev);
	tegra_padctl_disable_regulator(pdev);
	return 0;
}

static int
tegra_padctl_suspend_noirq(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);

	tegra_padctl_uphy_deinit(pdev);

	return 0;
}

static int
tegra_padctl_resume_noirq(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);

	return tegra_padctl_uphy_init(pdev);
}

static const struct dev_pm_ops tegra_padctl_pm_ops = {
	.suspend_noirq = tegra_padctl_suspend_noirq,
	.resume_noirq = tegra_padctl_resume_noirq,
};

static struct platform_driver tegra_padctl_driver = {
	.probe	= tegra_padctl_probe,
	.remove = tegra_padctl_remove,
	.driver	= {
		.name = "tegra-padctl",
		.of_match_table = tegra_padctl_of_match,
		.pm = &tegra_padctl_pm_ops,
	},
};

static int __init tegra_xusb_padctl_init(void)
{
	platform_driver_register(&tegra_padctl_driver);

	return 0;
}
fs_initcall(tegra_xusb_padctl_init);

#else

/* save restore below pad control register when cross LP0 */
struct xusb_padctl_restore_context {
	u32 padctl_usb3_pad_mux;
};
static struct xusb_padctl_restore_context context;

static int tegra_xusb_padctl_suspend(void)
{
	context.padctl_usb3_pad_mux = tegra_usb_pad_reg_read(
		XUSB_PADCTL_USB3_PAD_MUX_0);
	return 0;
}

static void tegra_xusb_padctl_resume(void)
{
	tegra_usb_pad_reg_write(XUSB_PADCTL_USB3_PAD_MUX_0,
		context.padctl_usb3_pad_mux);
}

static struct syscore_ops tegra_padctl_syscore_ops = {
	.suspend = tegra_xusb_padctl_suspend,
	.resume = tegra_xusb_padctl_resume,
	.save = tegra_xusb_padctl_suspend,
	.restore = tegra_xusb_padctl_resume,
};

static int __init tegra_xusb_padctl_init(void)
{
	register_syscore_ops(&tegra_padctl_syscore_ops);
	return 0;
}
late_initcall(tegra_xusb_padctl_init);

#endif
