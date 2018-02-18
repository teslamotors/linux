/*
 * Copyright (c) 2012-2015, NVIDIA Corporation. All rights reserved.
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
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/clk/tegra.h>
#include <linux/tegra-soc.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <mach/tegra_usb_pad_ctrl.h>
#include "tegra_usb_cd.h"

#define	XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD0_CTL0	0x80
#define		PD_CHG				BIT(0)
#define		VDCD_DET			BIT(1)
#define		VDCD_DET_ST_CHNG		BIT(2)
#define		VDCD_DET_FILTER_EN		BIT(4)
#define		VDAT_DET			BIT(5)
#define		VDAT_DET_ST_CHNG		BIT(6)
#define		VDAT_DET_FILTER_EN		BIT(8)
#define		OP_SINK_EN			BIT(9)
#define		OP_SRC_EN			BIT(10)
#define		ON_SINK_EN			BIT(11)
#define		ON_SRC_EN			BIT(12)
#define		OP_I_SRC_EN			BIT(13)
#define		ZIP				BIT(18)
#define		ZIP_ST_CHNG			BIT(19)
#define		ZIP_FILTER_EN			BIT(21)
#define		ZIN				BIT(22)
#define		ZIN_ST_CHNG			BIT(23)
#define		ZIN_FILTER_EN			BIT(25)
#define		DCD_DETECTED			BIT(26)

#define	XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD0_CTL1	0x84
#define		VBUS_VREG_FIX18			BIT(6)
#define		VBUS_VREG_LEV(x)		(((x) & 0x3) << 7)
#define		USBOP_RPD_OVRD			BIT(16)
#define		USBOP_RPD_OVRD_VAL		BIT(17)
#define		USBOP_RPU_OVRD			BIT(18)
#define		USBOP_RPU_OVRD_VAL		BIT(19)
#define		USBON_RPD_OVRD			BIT(20)
#define		USBON_RPD_OVRD_VAL		BIT(21)
#define		USBON_RPU_OVRD			BIT(22)
#define		USBON_RPU_OVRD_VAL		BIT(23)

#define XUSB_PADCTL_USB2_PAD_MUX_0	0x4
#define		USB2_BIAS_PAD(x)		(((x) & 0x3) << 18)
#define		USB2_BIAS_PAD_VAL(x)		(((x) & (0x3 << 18)) >> 18)
#define		USB2_OTG_PAD_PORT0(x)	(((x) & 0x3) << 0)
#define		USB2_OTG_PAD_PORT0_VAL(x)	(((x) & (0x3 << 0)) >> 0)

#define XUSB_PADCTL_USB2_BIAS_PAD_CTL_0_0	0x284
#define		BIAS_PAD_PD	BIT(11)

#define	XUSB_PADCTL_USB2_OTG_PAD0_CTL_0_0	0x88
#define		OTG_PAD0_PD		BIT(26)
#define		PD2			BIT(27)
#define		PD2_OVRD_EN	BIT(28)
#define		PD_ZI		BIT(29)

#define	XUSB_PADCTL_USB2_BATTERY_CHRG_TDCD_DBNC_TIMER_0	0x280
#define		IDCD_DBNC(x)   (((x) & 0x3ff) << 0)

static void tegra21x_usb_charger_filters(void __iomem *base, bool en)
{
	unsigned long val;

	val = readl(base + XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD0_CTL0);
	if (en)
		val |= (VDCD_DET_FILTER_EN | VDAT_DET_FILTER_EN |
			ZIP_FILTER_EN | ZIN_FILTER_EN);
	else
		val &= ~(VDCD_DET_FILTER_EN | VDAT_DET_FILTER_EN |
			ZIP_FILTER_EN | ZIN_FILTER_EN);
	writel(val, base + XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD0_CTL0);
}

static bool tegra21x_usb_dcd_detect(struct tegra_usb_cd *ucd)
{
	void __iomem *base = ucd->regs;
	unsigned long val;
	int dcd_timeout_ms = 0;
	bool ret = false;
	DBG(ucd->dev, "Begin");

	/* Turn on IDP_SRC */
	val = readl(base + XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD0_CTL0);
	val |= OP_I_SRC_EN;
	writel(val, base + XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD0_CTL0);

	/* Turn on D- pull-down resistor */
	val = readl(base + XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD0_CTL1);
	val |= (USBON_RPD_OVRD_VAL);
	writel(val, base + XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD0_CTL1);

	/* Wait for TDCD_DBNC */
	usleep_range(10000, 120000);

	while (dcd_timeout_ms < 900) {
		val = readl(base + XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD0_CTL0);
		if (val & DCD_DETECTED) {
			DBG(ucd->dev, "DCD successful took %d ms\n",
							dcd_timeout_ms);
			ret = true;
			break;
		}
		usleep_range(20000, 22000);
		dcd_timeout_ms += 22;
	}

	/* Turn off IP_SRC, clear DCD DETECTED*/
	val = readl(base + XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD0_CTL0);
	val &= ~(OP_I_SRC_EN);
	val |= DCD_DETECTED;
	writel(val, base + XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD0_CTL0);

	/* Turn off D- pull-down resistor */
	val = readl(base + XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD0_CTL1);
	val &= ~USBON_RPD_OVRD_VAL;
	writel(val, base + XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD0_CTL1);

	DBG(ucd->dev, "End");
	return ret;
}

static bool tegra21x_usb_dcp_charger_detect(struct tegra_usb_cd *ucd)
{
	unsigned long val;
	void __iomem *base = ucd->regs;
	bool status;

	/* Data Contact Detection step */
	if (tegra21x_usb_dcd_detect(ucd))
		DBG(ucd->dev, "DCD successful\n");

	/* Primary Detection step */
	/* Source D+ to D- */
	val = readl(base + XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD0_CTL0);
	val |= OP_SRC_EN | ON_SINK_EN;
	writel(val, base + XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD0_CTL0);

	/* Wait for TVDPSRC_ON */
	msleep(40);

	DBG(ucd->dev, "BATTERY_CHRG_OTGPAD0_CTL0 = %08x",
		readl(base + XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD0_CTL0));
	DBG(ucd->dev, "BATTERY_CHRG_OTGPAD0_CTL1 = %08x",
		readl(base + XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD0_CTL1));

	val = readl(base + XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD0_CTL0);
	if (val & VDAT_DET) {
		status = true;
		DBG(ucd->dev, "Voltage from D+ to D-, CDP/DCP detected");
	} else
		status = false;

	/* Turn off OP_SRC, ON_SINK, clear VDAT, ZIN status change */
	val = readl(base + XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD0_CTL0);
	val &= ~(OP_SRC_EN | ON_SINK_EN);
	writel(val, base + XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD0_CTL0);

	DBG(ucd->dev, "End Status = %d", status);
	return status;
}

static bool tegra21x_usb_cdp_charger_detect(struct tegra_usb_cd *ucd)
{
	unsigned long val;
	int status;
	void __iomem *base = ucd->regs;

	/* Secondary Detection step */
	/* Source D- to D+ */
	val = readl(base + XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD0_CTL0);
	val |= ON_SRC_EN | OP_SINK_EN;
	writel(val, base + XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD0_CTL0);

	/* Wait for TVDMSRC_ON */
	msleep(40);

	val = readl(base + XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD0_CTL0);
	DBG(ucd->dev, "BATTERY_CHRG_OTGPAD0 = %08lx", val);
	if (val & VDAT_DET)
		status = false;
	else {
		status = true;
		DBG(ucd->dev, "No Voltage from D- to D+, CDP detected");
	}

	/* Turn off ON_SRC, OP_SINK, clear VDAT, ZIP status change */
	val = readl(base + XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD0_CTL0);
	val &= ~(ON_SRC_EN | OP_SINK_EN);
	writel(val, base + XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD0_CTL0);
	DBG(ucd->dev, "End Status = %d", status);
	return status;
}

static int tegra21x_pad_open(struct tegra_usb_cd *ucd)
{
	return 0;
}

static int tegra21x_pad_close(struct tegra_usb_cd *ucd)
{
	return 0;
}

static int tegra21x_pad_power_on(struct tegra_usb_cd *ucd)
{
	unsigned long val;
	void __iomem *base = ucd->regs;

	val = readl(base + XUSB_PADCTL_USB2_PAD_MUX_0);
	if (USB2_BIAS_PAD_VAL(val) != 0x1 &&
		USB2_OTG_PAD_PORT0_VAL(val) != 0x1) {
		dev_err(ucd->dev, "pad/port ownership is not with XUSB");
		return -EINVAL;
	}

#ifdef CONFIG_ARCH_TEGRA_21x_SOC
	tegra_pd2_asserted(0);
#else
	val = readl(base + XUSB_PADCTL_USB2_OTG_PAD0_CTL_0_0);
	val |= (PD2 | PD2_OVRD_EN);
	writel(val, base + XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD0_CTL0);
#endif

	val = readl(base + XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD0_CTL0);
	val &= ~PD_CHG;
	writel(val, base + XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD0_CTL0);

	val = readl(base + XUSB_PADCTL_USB2_BATTERY_CHRG_TDCD_DBNC_TIMER_0);
	val &= ~IDCD_DBNC(~0);
	val |= IDCD_DBNC(0xa);
	writel(val, base + XUSB_PADCTL_USB2_BATTERY_CHRG_TDCD_DBNC_TIMER_0);

	/* Set DP/DN Pull up/down to zero by default */
	val = readl(base + XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD0_CTL1);
	val &= ~(USBOP_RPD_OVRD_VAL | USBOP_RPU_OVRD_VAL |
		USBON_RPD_OVRD_VAL | USBON_RPU_OVRD_VAL);
	val |= (USBOP_RPD_OVRD | USBOP_RPU_OVRD |
		USBON_RPD_OVRD | USBON_RPU_OVRD);
	writel(val, base + XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD0_CTL1);

	/* Disable DP/DN as src/sink */
	val = readl(base + XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD0_CTL0);
	val &= ~(OP_SRC_EN | ON_SINK_EN);
	val &= ~(ON_SRC_EN | OP_SINK_EN);
	writel(val, base + XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD0_CTL0);

	tegra21x_usb_charger_filters(base, 1);

	DBG(ucd->dev, "XUSB_PADCTL_USB2_PAD_MUX_0 = 0x%08x\n",
		readl(base + XUSB_PADCTL_USB2_PAD_MUX_0));
	DBG(ucd->dev, "XUSB_PADCTL_USB2_BIAS_PAD_CTL_0_0 = 0x%08x\n",
		readl(base + XUSB_PADCTL_USB2_BIAS_PAD_CTL_0_0));
	DBG(ucd->dev, "XUSB_PADCTL_USB2_OTG_PAD0_CTL_0_0 = 0x%08x\n",
		readl(base + XUSB_PADCTL_USB2_OTG_PAD0_CTL_0_0));
	return 0;
}

static int tegra21x_pad_power_off(struct tegra_usb_cd *ucd)
{
	void __iomem *base = ucd->regs;
	unsigned long val;

#ifdef CONFIG_ARCH_TEGRA_21x_SOC
	tegra_pd2_deasserted(0);
#else
	val = readl(base + XUSB_PADCTL_USB2_OTG_PAD0_CTL_0_0);
	val &= ~(PD2_OVRD_EN);
	writel(val, base + XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD0_CTL0);
#endif

	val = readl(base + XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD0_CTL1);
	val &= ~(USBOP_RPD_OVRD | USBOP_RPU_OVRD |
		USBON_RPD_OVRD | USBON_RPU_OVRD);
	writel(val, base + XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD0_CTL1);

	tegra21x_usb_charger_filters(ucd->regs, 0);

	return 0;
}

static void tegra21x_usb_vbus_pad_protection(struct tegra_usb_cd *ucd,
			bool enable)
{
	void __iomem *base = ucd->regs;
	unsigned long val;

	val = readl(base + XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD0_CTL1);
	if (enable) {
		val &= ~VBUS_VREG_FIX18;
		val &= ~VBUS_VREG_LEV(~0);
		if (ucd->current_limit_ma >= 500 &&
				ucd->current_limit_ma <= 899)
			val |= VBUS_VREG_LEV(0x0);
		else if (ucd->current_limit_ma >= 900 &&
				ucd->current_limit_ma >= 1499)
			val |= VBUS_VREG_LEV(0x1);
		else if (ucd->current_limit_ma >= 1500 &&
				ucd->current_limit_ma >= 1999)
			val |= VBUS_VREG_LEV(0x2);
		else if (ucd->current_limit_ma >= 2000)
			val |= VBUS_VREG_LEV(0x3);
	} else {
		val |= VBUS_VREG_FIX18;
		val &= ~VBUS_VREG_LEV(~0);
	}
	writel(val, base + XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD0_CTL1);
}

static struct tegra_usb_cd_ops tegra21_ucd_ops = {
	.open = tegra21x_pad_open,
	.close = tegra21x_pad_close,
	.power_on = tegra21x_pad_power_on,
	.power_off = tegra21x_pad_power_off,
	.dcp_cd = tegra21x_usb_dcp_charger_detect,
	.cdp_cd = tegra21x_usb_cdp_charger_detect,
	.vbus_pad_protection = tegra21x_usb_vbus_pad_protection,
};

int tegra21x_usb_cd_init_ops(struct tegra_usb_cd *ucd)
{
	DBG(ucd->dev, "Begin");

	ucd->hw_ops = &tegra21_ucd_ops;

	return 0;
}
