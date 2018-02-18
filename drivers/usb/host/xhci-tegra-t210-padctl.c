/*
 * xhci-tegra-t210-padctl.c - Nvidia xHCI host padctl driver

 *
 * Copyright (c) 2013-2015, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/platform_device.h>
#include <mach/tegra_usb_pad_ctrl.h>
#include "xhci-tegra-legacy.h"
#include "xhci-tegra-t210-padreg.h"

/* UPHY_USB3_PADx_ECTL_1 */
#define TX_TERM_CTRL(x)	(((x) & 0x3) << 16)

/* UPHY_USB3_PADx_ECTL_2 */
#define RX_CTLE(x)		(((x) & 0xffff) << 0)

/* UPHY_USB3_PADx_ECTL_3 */
#define RX_DFE(x)		(((x) & 0xffffffff) << 0)

/* UPHY_USB3_PADx_ECTL_4 */
#define RX_CDR_CTRL(x)		(((x) & 0xffff) << 16)

/* UPHY_USB3_PADx_ECTL_6 */
#define RX_EQ_CTRL_H(x)	(((x) & 0xffffffff) << 0)

/* XUSB_PADCTL_USB2_PAD_MUX_0 0x4 */
#define USB2_PAD_MUX_0		(0x4)
#define  HSIC_PAD_PORT(x)	(1 << ((x) + 14))

/* XUSB_PADCTL_HSIC_PAD0_CTL_0_0 0x300 */
/* XUSB_PADCTL_HSIC_PAD1_CTL_0_0 0x320 */
#define HSIC_PAD_CTL_0(x)			((x == 0) ? (0x300) : (0x320))
#define  PD_TX_DATA0				(1 << 1)
#define  PD_TX_STROBE				(1 << 3)
#define  PD_TX					(PD_TX_DATA0 | PD_TX_STROBE)
#define  PD_RX_DATA0				(1 << 4)
#define  PD_RX_STROBE				(1 << 6)
#define  PD_RX					(PD_RX_DATA0 | PD_RX_STROBE)
#define  PD_ZI_DATA0				(1 << 7)
#define  PD_ZI_STROBE				(1 << 9)
#define  PD_ZI					(PD_ZI_DATA0 | PD_ZI_STROBE)
#define  RPD_DATA0				(0x1 << 13)
#define  RPD_DATA1				(0x1 << 14)
#define  RPD_DATA				(RPD_DATA0 | RPD_DATA1)
#define  RPD_STROBE				(0x1 << 15)
#define  RPU_DATA0				(0x1 << 16)
#define  RPU_DATA1				(0x1 << 17)
#define  RPU_DATA				(RPU_DATA0 | RPU_DATA1)
#define  RPU_STROBE				(0x1 << 18)

/* XUSB_PADCTL_HSIC_STRB_TRIM_CONTROL_0 0x344 */
#define HSIC_STRB_TRIM_CONTROL_0		(0x344)
#define  STRB_TRIM_VAL(x)			(((x) & 0x3F) << 0)

/* XUSB_PADCTL_UPHY_MISC_PAD_Px_CTL_1_0 */
/* XUSB_PADCTL_UPHY_MISC_PAD_S0_CTL_1_0 */
#define UPHY_MISC_PAD_AUX_RX_IDLE_MODE(x)		(((x) & 0x3) << 20)
#define UPHY_MISC_PAD_AUX_RX_TERM_EN			(1 << 18)
#define UPHY_MISC_PAD_AUX_RX_MODE_OVRD			(1 << 13)

#define BATTERY_CHRG_OTGPAD(x) (x * 0x40)
#define PADCTL_USB2_BATTERY_CHRG_OTGPAD_BASE 0x84
#define VREG_FIX18_OFFSET 0x6

void t210_program_utmi_pad(struct tegra_xhci_hcd *tegra, u8 port)
{
	xusb_utmi_pad_init(port, USB2_PORT_CAP_HOST(port)
		, tegra->bdata->uses_external_pmic);
}

void t210_program_ss_pad(struct tegra_xhci_hcd *tegra, u8 port, bool is_otg)
{
	xusb_ss_pad_init(port, GET_SS_PORTMAP(tegra->bdata->ss_portmap, port)
			, is_otg ? XUSB_OTG_MODE : XUSB_HOST_MODE);
}

int t210_hsic_pad_enable(struct tegra_xhci_hcd *tegra, u8 pad)
{
	struct device *dev = &tegra->pdev->dev;
	u32 mask, val;

	if (pad >= 2) {
		dev_err(dev, "%s invalid HSIC pad number %d\n", __func__, pad);
		return -EINVAL;
	}

	dev_dbg(dev, "%s pad %u\n", __func__, pad);

	/* keep HSIC in RESET */
	mask = RPD_DATA | RPD_STROBE | RPU_DATA | RPU_STROBE;
	mask |= PD_RX | PD_ZI | PD_TX;
	val = RPD_DATA | RPD_STROBE;
	tegra_usb_pad_reg_update(HSIC_PAD_CTL_0(pad), mask, val);

	hsic_trk_enable();

	tegra_usb_pad_reg_update(USB2_PAD_MUX_0,
		HSIC_PAD_PORT(pad), HSIC_PAD_PORT(pad)); /* hsic pad owner */

	reg_dump(dev, tegra->padctl_base, HSIC_PAD_CTL_0(pad));
	reg_dump(dev, tegra->padctl_base, HSIC_STRB_TRIM_CONTROL_0);
	reg_dump(dev, tegra->padctl_base, USB2_PAD_MUX_0);

	return 0;
}

int t210_hsic_pad_disable(struct tegra_xhci_hcd *tegra, unsigned pad)
{
	struct device *dev = &tegra->pdev->dev;

	if (pad >= 2) {
		dev_err(dev, "%s invalid HSIC pad number %d\n", __func__, pad);
		return -EINVAL;
	}

	dev_dbg(dev, "%s pad %u\n", __func__, pad);

	tegra_usb_pad_reg_update(USB2_PAD_MUX_0, HSIC_PAD_PORT(pad), 0);

	tegra_usb_pad_reg_update(HSIC_PAD_CTL_0(pad), PD_RX | PD_ZI | PD_TX, 0);

	reg_dump(dev, tegra->padctl_base, USB2_PAD_MUX_0);
	reg_dump(dev, tegra->padctl_base, HSIC_PAD_CTL_0(pad));

	return 0;
}

int t210_hsic_pad_pupd_set(struct tegra_xhci_hcd *tegra, unsigned pad,
	enum hsic_pad_pupd pupd)
{
	struct device *dev = &tegra->pdev->dev;
	u32 reg;
	u32 mask, val = 0;

	if (pad >= 2) {
		dev_err(dev, "%s invalid HSIC pad number %u\n", __func__, pad);
		return -EINVAL;
	}

	dev_dbg(dev, "%s pad %u pupd %d\n", __func__, pad, pupd);

	mask = (RPD_DATA | RPD_STROBE | RPU_DATA | RPU_STROBE);
	if (pupd == PUPD_IDLE) {
		val = (RPD_DATA | RPU_STROBE);
		reg |= (RPD_DATA | RPU_STROBE);
	} else if (pupd == PUPD_RESET) {
		val = (RPD_DATA | RPD_STROBE);
		reg |= (RPD_DATA | RPU_STROBE);
	} else if (pupd != PUPD_DISABLE) {
		dev_err(dev, "%s invalid pupd %d\n", __func__, pupd);
		return -EINVAL;
	}
	tegra_usb_pad_reg_update(HSIC_PAD_CTL_0(pad), mask, val);

	reg_dump(dev, tegra->padctl_base, HSIC_PAD_CTL_0(pad));
	return 0;
}

static void t210_lfps_detector(struct tegra_xhci_hcd *tegra,
						unsigned port, bool on)
{
	struct device *dev = &tegra->pdev->dev;
	u8 lane = (tegra->bdata->lane_owner >> (port * 4)) & 0xf;
	enum padctl_lane lane_enum = usb3_laneowner_to_lane_enum(lane);
	const struct tegra_xusb_padctl_regs *padregs = tegra->padregs;
	u32 misc_pad_ctl1;
	u32 mask, val;

	if ((lane == USB3_LANE_NOT_ENABLED) || (lane_enum < 0))
		return;

	if (lane_enum == SATA_S0)
		misc_pad_ctl1 = padregs->uphy_misc_pad_s0_ctlY_0[0];
	else
		misc_pad_ctl1 = padregs->uphy_misc_pad_pX_ctlY_0[lane_enum][0];

	mask = UPHY_MISC_PAD_AUX_RX_IDLE_MODE(~0) |
		UPHY_MISC_PAD_AUX_RX_TERM_EN | UPHY_MISC_PAD_AUX_RX_MODE_OVRD;

	if (!on) {
		val = UPHY_MISC_PAD_AUX_RX_IDLE_MODE(0x1) |
			UPHY_MISC_PAD_AUX_RX_TERM_EN |
			UPHY_MISC_PAD_AUX_RX_MODE_OVRD;
	} else
		val = 0;

	tegra_usb_pad_reg_update(misc_pad_ctl1, mask, val);

	reg_dump(dev, tegra->padctl_base, misc_pad_ctl1);
}

void t210_enable_lfps_detector(struct tegra_xhci_hcd *tegra, unsigned port)
{
	dev_dbg(&tegra->pdev->dev, "%s port %d\n", __func__, port);
	t210_lfps_detector(tegra, port, true);
}

void t210_disable_lfps_detector(struct tegra_xhci_hcd *tegra, unsigned port)
{
	dev_dbg(&tegra->pdev->dev, "%s port %d\n", __func__, port);
	t210_lfps_detector(tegra, port, false);
}

static void t210_battery_circuit(struct tegra_xhci_hcd *tegra,
						unsigned port, bool on)
{
	u32 mask;
	u32 reg_offset;

	mask = VREG_FIX18_OFFSET;
	reg_offset = (PADCTL_USB2_BATTERY_CHRG_OTGPAD_BASE +
		(BATTERY_CHRG_OTGPAD(port)));
	tegra_usb_pad_reg_update(reg_offset, mask, (on << VREG_FIX18_OFFSET));
}

void t210_disable_battery_circuit(struct tegra_xhci_hcd *tegra, unsigned port)
{
	dev_dbg(&tegra->pdev->dev, "%s port %d\n", __func__, port);
	t210_battery_circuit(tegra, port, false);
}

void t210_enable_battery_circuit(struct tegra_xhci_hcd *tegra, unsigned port)
{
	dev_dbg(&tegra->pdev->dev, "%s port %d\n", __func__, port);
	t210_battery_circuit(tegra, port, true);
}
