/*
 * Copyright (c) 2013-2016, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef _TEGRA_USB_PAD_CTRL_INTERFACE_H_
#define _TEGRA_USB_PAD_CTRL_INTERFACE_H_

#include <mach/xusb.h>
#include <linux/tegra_prod.h>

#define UTMIPLL_HW_PWRDN_CFG0			0x52c
#define   UTMIPLL_LOCK				(1<<31)
#define   UTMIPLL_HW_PWRDN_CFG0_IDDQ_OVERRIDE  (1<<1)
#define   UTMIPLL_HW_PWRDN_CFG0_IDDQ_SWCTL     (1<<0)

#define UTMIP_BIAS_CFG0		0x80c
#define UTMIP_OTGPD			(1 << 11)
#define UTMIP_BIASPD			(1 << 10)
#define UTMIP_HSSQUELCH_LEVEL(x)	(((x) & 0x3) << 0)
#define UTMIP_HSDISCON_LEVEL(x)	(((x) & 0x3) << 2)
#define UTMIP_HSDISCON_LEVEL_MSB	(1 << 24)

/* Encode lane width of each RP in nibbles starting with RP0 at lowest */
#define PCIE_LANES_X4_X1		0x14
#define PCIE_LANES_X4_X0		0x04
#define PCIE_LANES_X2_X1		0x12
#define PCIE_LANES_X2_X0		0x02
#define PCIE_LANES_X0_X1		0x10

enum padctl_lane {
	PEX_P0 = 0,
	PEX_P1,
	PEX_P2,
	PEX_P3,
	PEX_P4,
	PEX_P5,
	PEX_P6,
	SATA_S0,
	LANE_MIN = PEX_P0,
	LANE_MAX = SATA_S0
};

static inline enum padctl_lane usb3_laneowner_to_lane_enum(u8 laneowner)
{
	if (laneowner == 0x0)
		return PEX_P0;
	else if (laneowner == 0x1)
		return PEX_P1;
	else if (laneowner == 0x2)
		return PEX_P2;
	else if (laneowner == 0x3)
		return PEX_P3;
	else if (laneowner == 0x4)
		return PEX_P4;
	else if (laneowner == 0x5)
		return PEX_P5;
	else if (laneowner == 0x6)
		return PEX_P6;
	else if (laneowner == 0x8)
		return SATA_S0;
	else
		return -1; /* unknown */
}

/* PCIe/SATA pad phy */
#ifdef CONFIG_ARCH_TEGRA_21x_SOC
#define SS_PAD_COUNT	4
#define USB3_LANE_NOT_ENABLED	0xF
#define SATA_LANE	(0x8 << 12)

#define XUSB_PADCTL_UPHY_PLL_P0_CTL1_0	0x360
#define XUSB_PADCTL_UPHY_PLL_P0_CTL1_PLL0_IDDQ	(1 << 0)
#define XUSB_PADCTL_UPHY_PLL_P0_CTL1_PLL0_SLEEP	(0x3 << 1)
#define XUSB_PADCTL_UPHY_PLL_P0_CTL1_PLL0_ENABLE	(1 << 3)
#define XUSB_PADCTL_UPHY_PLL_P0_CTL1_PLL0_PWR_OVRD	(1 << 4)
#define XUSB_PADCTL_UPHY_PLL_P0_CTL1_PLL0_LOCKDET_STATUS	(1 << 15)
#define XUSB_PADCTL_UPHY_PLL_P0_CTL1_PLL0_FREQ_MDIV_MASK	(0x3 << 16)
#define XUSB_PADCTL_UPHY_PLL_P0_CTL1_PLL0_FREQ_NDIV	(25 << 20)
#define XUSB_PADCTL_UPHY_PLL_P0_CTL1_PLL0_FREQ_NDIV_MASK	(0xFF << 20)

#define XUSB_PADCTL_UPHY_PLL_P0_CTL2_0	0x364
#define XUSB_PADCTL_UPHY_PLL_P0_CTL2_PLL0_CAL_EN	(1 << 0)
#define XUSB_PADCTL_UPHY_PLL_P0_CTL2_PLL0_CAL_DONE	(1 << 1)
#define XUSB_PADCTL_UPHY_PLL_P0_CTL2_PLL0_CAL_OVRD	(1 << 2)
#define XUSB_PADCTL_UPHY_PLL_P0_CTL2_CAL_CTRL_MASK	(0xFFFFFF << 4)
#define XUSB_PADCTL_UPHY_PLL_P0_CTL2_PLL0_CAL_CTRL	(0x136 << 4)

#define XUSB_PADCTL_UPHY_PLL_P0_CTL4_0	0x36C
#define XUSB_PADCTL_UPHY_PLL_P0_CTL4_PLL0_REFCLK_SEL_MASK	(0xF << 4)
#define XUSB_PADCTL_UPHY_PLL_P0_CTL4_PLL0_REFCLKBUF_EN	(1 << 8)
#define XUSB_PADCTL_UPHY_PLL_P0_CTL4_PLL0_TXCLKREF_SEL	(0x2 << 12)
#define XUSB_PADCTL_UPHY_PLL_P0_CTL4_PLL0_TXCLKREF_SEL_MASK	(0x3 << 12)
#define XUSB_PADCTL_UPHY_PLL_P0_CTL4_PLL0_TXCLKREF_EN	(1 << 15)

#define XUSB_PADCTL_UPHY_PLL_P0_CTL5_0	0x370
#define XUSB_PADCTL_UPHY_PLL_P0_CTL5_DCO_CTRL_MASK	(0xFF << 16)
#define XUSB_PADCTL_UPHY_PLL_P0_CTL5_PLL0_DCO_CTRL	(0x2a << 16)

#define XUSB_PADCTL_UPHY_PLL_P0_CTL8_0	0x37C
#define XUSB_PADCTL_UPHY_PLL_P0_CTL8_PLL0_RCAL_EN	(1 << 12)
#define XUSB_PADCTL_UPHY_PLL_P0_CTL8_PLL0_RCAL_CLK_EN	(1 << 13)
#define XUSB_PADCTL_UPHY_PLL_P0_CTL8_PLL0_RCAL_OVRD	(1 << 15)
#define XUSB_PADCTL_UPHY_PLL_P0_CTL8_PLL0_RCAL_DONE	(1 << 31)

#define XUSB_PADCTL_UPHY_PLL_S0_CTL1_0			0x860
#define S0_CTL1_PLL0_IDDQ				(1 << 0)
#define S0_CTL1_PLL0_SLEEP				(0x3 << 1)
#define S0_CTL1_PLL0_ENABLE				(1 << 3)
#define S0_CTL1_PLL0_PWR_OVRD				(1 << 4)
#define S0_CTL1_PLL0_LOCKDET_STATUS			(1 << 15)
#define S0_PLL0_FREQ_NDIV(x)				(((x) & 0xFF) << 20)

#define XUSB_PADCTL_UPHY_PLL_S0_CTL2_0			(0x864)
#define S0_CTL2_PLL0_CAL_EN				(1 << 0)
#define S0_CTL2_PLL0_CAL_DONE				(1 << 1)
#define S0_CTL2_PLL0_CAL_OVRD				(1 << 2)
#define S0_CTL2_PLL0_CAL_CTRL(x)			(((x) & 0xFFFFFF) << 4)

#define XUSB_PADCTL_UPHY_PLL_S0_CTL4_0			(0x86c)
#define S0_PLL0_TXCLKREF_SEL(x)			(((x) & 0x3) << 12)

#define XUSB_PADCTL_UPHY_PLL_S0_CTL5_0			(0x870)
#define S0_CTL5_PLL0_DCO_CTRL(x)			(((x) & 0xFF) << 16)

#define XUSB_PADCTL_UPHY_PLL_S0_CTL8_0			(0x87C)
#define S0_CTL8_PLL0_RCAL_EN				(1 << 12)
#define S0_CTL8_PLL0_RCAL_CLK_EN			(1 << 13)
#define S0_CTL8_PLL0_RCAL_OVRD				(1 << 15)
#define S0_CTL8_PLL0_RCAL_DONE				(1 << 31)

#define XUSB_PADCTL_UPHY_MISC_PAD_P0_CTL1	0x460
#define XUSB_PADCTL_UPHY_MISC_PAD_P0_CTL1_AUX_RX_IDLE_TH_MASK	(0x3 << 24)
#define XUSB_PADCTL_UPHY_MISC_PAD_P0_CTL1_AUX_RX_IDLE_TH	(1 << 24)
#define XUSB_PADCTL_UPHY_MISC_PAD_P0_CTL1_AUX_TX_RDET_STATUS	(1 << 7)

#define XUSB_PADCTL_UPHY_MISC_PAD_P1_CTL1	0x4A0
#define XUSB_PADCTL_UPHY_MISC_PAD_P1_CTL1_AUX_TX_RDET_STATUS	(1 << 7)

#define XUSB_PADCTL_UPHY_MISC_PAD_P2_CTL1	0x4E0
#define XUSB_PADCTL_UPHY_MISC_PAD_P2_CTL1_AUX_TX_RDET_STATUS	(1 << 7)

#define XUSB_PADCTL_UPHY_MISC_PAD_P3_CTL1	0x520
#define XUSB_PADCTL_UPHY_MISC_PAD_P3_CTL1_AUX_TX_RDET_STATUS	(1 << 7)

#define XUSB_PADCTL_UPHY_MISC_PAD_P4_CTL1	0x560
#define XUSB_PADCTL_UPHY_MISC_PAD_P4_CTL1_AUX_TX_RDET_STATUS	(1 << 7)

#define XUSB_PADCTL_UPHY_MISC_PAD_P0_CTL2	0x464
#define XUSB_PADCTL_UPHY_MISC_PAD_P0_CTL2_TX_IDDQ	(1 << 0)
#define XUSB_PADCTL_UPHY_MISC_PAD_P0_CTL2_RX_IDDQ	(1 << 8)
#define XUSB_PADCTL_UPHY_MISC_PAD_P0_CTL2_TX_IDDQ_OVRD	(1 << 1)
#define XUSB_PADCTL_UPHY_MISC_PAD_P0_CTL2_RX_IDDQ_OVRD	(1 << 9)
#define XUSB_PADCTL_UPHY_MISC_PAD_P0_CTL2_TX_SLEEP	(3 << 4)
#define XUSB_PADCTL_UPHY_MISC_PAD_P0_CTL2_RX_SLEEP	(3 << 12)
#define XUSB_PADCTL_UPHY_MISC_PAD_P0_CTL2_TX_PWR_OVRD	(1 << 24)
#define XUSB_PADCTL_UPHY_MISC_PAD_P0_CTL2_RX_PWR_OVRD	(1 << 25)

#define XUSB_PADCTL_UPHY_MISC_PAD_P1_CTL2	0x4A4
#define XUSB_PADCTL_UPHY_MISC_PAD_P2_CTL2	0x4E4
#define XUSB_PADCTL_UPHY_MISC_PAD_P3_CTL2	0x524
#define XUSB_PADCTL_UPHY_MISC_PAD_P4_CTL2	0x564
#define XUSB_PADCTL_UPHY_MISC_PAD_P5_CTL2	0x5A4
#define XUSB_PADCTL_UPHY_MISC_PAD_P6_CTL2	0x5E4
#define XUSB_PADCTL_UPHY_MISC_PAD_S0_CTL2	0x964

#define XUSB_PADCTL_UPHY_PLL_S0_CTL_1_0 0x860
#define XUSB_PADCTL_UPHY_PLL_S0_CTL1_MDIV_MASK	(0x3 << 16)
#define XUSB_PADCTL_UPHY_PLL_S0_CTL1_NDIV_MASK	(0xFF << 20)
#define XUSB_PADCTL_UPHY_PLL_S0_CTL1_PLL0_FREQ_MDIV (0x0 << 16)
#define XUSB_PADCTL_UPHY_PLL_S0_CTL1_PLL0_FREQ_NDIV (0x19 << 20)

#define XUSB_PADCTL_UPHY_PLL_S0_CTL_2_0 0x864
#define XUSB_PADCTL_UPHY_PLL_S0_CTL2_CAL_CTRL_MASK	(0xFFFFFF << 4)
#define XUSB_PADCTL_UPHY_PLL_S0_CTL2_PLL0_CAL_CTRL	(0x136 << 4)

#define XUSB_PADCTL_UPHY_PLL_S0_CTL_4_0 0x86c
#define XUSB_PADCTL_UPHY_PLL_S0_CTL4_TXCLKREF_SEL_MASK	(0x3 << 12)
#define XUSB_PADCTL_UPHY_PLL_S0_CTL4_TXCLKREF_EN_MASK	(0x1 << 15)
#define XUSB_PADCTL_UPHY_PLL_S0_CTL4_PLL0_TXCLKREF_SEL	(0x2 << 12)
#define XUSB_PADCTL_UPHY_PLL_S0_CTL4_PLL0_TXCLKREF_EN	(0x1 << 15)

#define XUSB_PADCTL_UPHY_PLL_S0_CTL_5_0 0x870
#define XUSB_PADCTL_UPHY_PLL_S0_CTL5_DCO_CTRL_MASK	(0xFF << 16)
#define XUSB_PADCTL_UPHY_PLL_S0_CTL5_PLL0_DCO_CTRL	(0x2a << 16)

#define XUSB_PADCTL_UPHY_MISC_PAD_S0_CTL_1_0	0x960
#define AUX_TX_IDDQ				(1 << 0)
#define AUX_TX_IDDQ_OVRD			(1 << 1)
#define AUX_RX_MODE_OVRD			(1 << 13)
#define AUX_RX_TERM_EN				(1 << 18)
#define AUX_RX_IDLE_EN				(1 << 22)
#define AUX_RX_IDLE_TH(x)			(((x) & 0x3) << 24)

#define XUSB_PADCTL_UPHY_MISC_PAD_S0_CTL_4_0	0x96c
#define RX_TERM_EN				(1 << 21)
#define RX_TERM_OVRD				(1 << 23)

#else
#define SATA_LANE	(0x1)

/* xusb padctl regs for pad programming of t124 usb3 */
#define XUSB_PADCTL_IOPHY_PLL_S0_CTL1_0			0x138
#define XUSB_PADCTL_IOPHY_PLL_S0_CTL1_0_PLL0_REFCLK_NDIV_MASK	(0x3 << 20)
#define XUSB_PADCTL_IOPHY_PLL_S0_CTL1_0_PLL0_REFCLK_NDIV	(0x2 << 20)

#define XUSB_PADCTL_IOPHY_PLL_S0_CTL2_0			0x13c
#define XUSB_PADCTL_IOPHY_PLL_S0_CTL2_0_XDIGCLK_SEL_MASK	(0x7 << 0)
#define XUSB_PADCTL_IOPHY_PLL_S0_CTL2_0_XDIGCLK_SEL		(0x7 << 0)
#define XUSB_PADCTL_IOPHY_PLL_S0_CTL2_0_TXCLKREF_SEL		(1 << 4)
#define XUSB_PADCTL_IOPHY_PLL_S0_CTL2_0_TCLKOUT_EN		(1 << 12)
#define XUSB_PADCTL_IOPHY_PLL_S0_CTL2_0_PLL0_CP_CNTL_MASK	(0xF << 16)
#define XUSB_PADCTL_IOPHY_PLL_S0_CTL2_0_PLL0_CP_CNTL		(0x8 << 16)
#define XUSB_PADCTL_IOPHY_PLL_S0_CTL2_0_PLL1_CP_CNTL_MASK	(0xF << 20)
#define XUSB_PADCTL_IOPHY_PLL_S0_CTL2_0_PLL1_CP_CNTL		(0x8 << 20)

#define XUSB_PADCTL_IOPHY_PLL_S0_CTL3_0			0x140
#define XUSB_PADCTL_IOPHY_PLL_S0_CTL3_0_RCAL_BYPASS		(1 << 7)

/* xusb padctl regs for pad programming of pcie */
#define XUSB_PADCTL_IOPHY_PLL_P0_CTL1_0	0x40
#define XUSB_PADCTL_IOPHY_PLL_P0_CTL1_REFCLK_SEL_MASK	(0xF << 12)
#define XUSB_PADCTL_IOPHY_PLL_P0_CTL1_REFCLK_SEL	(0x0 << 12)
#define XUSB_PADCTL_IOPHY_PLL_P0_CTL1_PLL_RST_		(1 << 1)
#define XUSB_PADCTL_IOPHY_PLL_P0_CTL1_PLL0_LOCKDET		(1 << 19)

#define XUSB_PADCTL_IOPHY_PLL_P0_CTL2_0	0x44
#define XUSB_PADCTL_IOPHY_PLL_P0_CTL2_TXCLKREF_SEL	(1 << 4)
#define XUSB_PADCTL_IOPHY_PLL_P0_CTL2_TXCLKREF_EN	(1 << 5)
#define XUSB_PADCTL_IOPHY_PLL_P0_CTL2_REFCLKBUF_EN	(1 << 6)
#define XUSB_PADCTL_IOPHY_PLL_P0_CTL2_PLL0_CP_CNTL_MASK	(0xF << 16)
#define XUSB_PADCTL_IOPHY_PLL_P0_CTL2_PLL0_CP_CNTL_VAL	(0x5 << 16)

#endif

/* PADCTL ELPG_PROGRAM */
#ifdef CONFIG_ARCH_TEGRA_21x_SOC

#define XUSB_PADCTL_ELPG_PROGRAM_0		0x20
#define XUSB_PADCTL_ELPG_PROGRAM_1		0x24
#define SSP0_ELPG_CLAMP_EN			(1 << 0)
#define SSP0_ELPG_CLAMP_EN_EARLY	(1 << 1)
#define SSP0_ELPG_VCORE_DOWN		(1 << 2)
#define SSP1_ELPG_CLAMP_EN			(1 << 3)
#define SSP1_ELPG_CLAMP_EN_EARLY	(1 << 4)
#define SSP1_ELPG_VCORE_DOWN		(1 << 5)
#define SSP2_ELPG_CLAMP_EN			(1 << 6)
#define SSP2_ELPG_CLAMP_EN_EARLY	(1 << 7)
#define SSP2_ELPG_VCORE_DOWN		(1 << 8)
#define SSP3_ELPG_CLAMP_EN			(1 << 9)
#define SSP3_ELPG_CLAMP_EN_EARLY	(1 << 10)
#define SSP3_ELPG_VCORE_DOWN		(1 << 11)
#define XUSB_PADCTL_ELPG_PROGRAM_AUX_MUX_LP0_CLAMP_EN	(1 << 29)
#define XUSB_PADCTL_ELPG_PROGRAM_AUX_MUX_LP0_CLAMP_EN_EARLY	(1 << 30)
#define XUSB_PADCTL_ELPG_PROGRAM_AUX_MUX_LP0_VCORE_DOWN	(1 << 31)

#define USB2_PORT3_WAKE_INTERRUPT_ENABLE	(1 << 3)
#define SS_PORT0_WAKE_INTERRUPT_ENABLE	(1 << 14)
#define SS_PORT1_WAKE_INTERRUPT_ENABLE	(1 << 15)
#define SS_PORT2_WAKE_INTERRUPT_ENABLE	(1 << 16)
#define SS_PORT3_WAKE_INTERRUPT_ENABLE	(1 << 17)

#define SS_PORT0_WAKEUP_EVENT		(1 << 21)
#define SS_PORT1_WAKEUP_EVENT		(1 << 22)
#define SS_PORT2_WAKEUP_EVENT		(1 << 23)
#define SS_PORT3_WAKEUP_EVENT		(1 << 24)
#define SS_PORT_WAKEUP_EVENT(p)		(1 << (21 + p))

#define USB2_PORT0_WAKEUP_EVENT		(1 << 7)
#define USB2_PORT1_WAKEUP_EVENT		(1 << 8)
#define USB2_PORT2_WAKEUP_EVENT		(1 << 9)
#define USB2_PORT3_WAKEUP_EVENT		(1 << 10)
#define USB2_HSIC_PORT0_WAKEUP_EVENT	(1 << 30)
#define USB2_HSIC_PORT1_WAKEUP_EVENT	(1 << 31)
#define USB2_HSIC_PORT0_WAKE_INTERRUPT_ENABLE	(1 << 28)
#define USB2_HSIC_PORT1_WAKE_INTERRUPT_ENABLE	(1 << 29)

#define XUSB_ALL_WAKE_EVENT	\
		(USB2_PORT0_WAKEUP_EVENT | USB2_PORT1_WAKEUP_EVENT |	\
		USB2_PORT2_WAKEUP_EVENT | USB2_PORT3_WAKEUP_EVENT |	\
		SS_PORT0_WAKEUP_EVENT | SS_PORT1_WAKEUP_EVENT |	\
		SS_PORT2_WAKEUP_EVENT | SS_PORT3_WAKEUP_EVENT |	\
		USB2_HSIC_PORT0_WAKEUP_EVENT)

#else
#define XUSB_PADCTL_ELPG_PROGRAM_0		0x1c
#define XUSB_PADCTL_ELPG_PROGRAM_AUX_MUX_LP0_CLAMP_EN	(1 << 24)
#define XUSB_PADCTL_ELPG_PROGRAM_AUX_MUX_LP0_CLAMP_EN_EARLY	(1 << 25)
#define XUSB_PADCTL_ELPG_PROGRAM_AUX_MUX_LP0_VCORE_DOWN	(1 << 26)

#define SSP0_ELPG_CLAMP_EN		(1 << 16)
#define SSP0_ELPG_CLAMP_EN_EARLY	(1 << 17)
#define SSP0_ELPG_VCORE_DOWN		(1 << 18)
#define SSP1_ELPG_CLAMP_EN		(1 << 20)
#define SSP1_ELPG_CLAMP_EN_EARLY	(1 << 21)
#define SSP1_ELPG_VCORE_DOWN		(1 << 22)
#define SSP2_ELPG_CLAMP_EN			0x0
#define SSP2_ELPG_CLAMP_EN_EARLY	0x0
#define SSP2_ELPG_VCORE_DOWN		0x0
#define SSP3_ELPG_CLAMP_EN			0x0
#define SSP3_ELPG_CLAMP_EN_EARLY	0x0
#define SSP3_ELPG_VCORE_DOWN		0x0

#define USB2_PORT3_WAKE_INTERRUPT_ENABLE	0x0
#define SS_PORT0_WAKE_INTERRUPT_ENABLE	(1 << 6)
#define SS_PORT1_WAKE_INTERRUPT_ENABLE	(1 << 7)
#define SS_PORT2_WAKE_INTERRUPT_ENABLE	0x0
#define SS_PORT3_WAKE_INTERRUPT_ENABLE	0x0

#define SS_PORT0_WAKEUP_EVENT		(1 << 14)
#define SS_PORT1_WAKEUP_EVENT		(1 << 15)
#define SS_PORT2_WAKEUP_EVENT		0x0
#define SS_PORT3_WAKEUP_EVENT		0x0

#define USB2_PORT0_WAKEUP_EVENT		(1 << 8)
#define USB2_PORT1_WAKEUP_EVENT		(1 << 9)
#define USB2_PORT2_WAKEUP_EVENT		(1 << 10)
#define USB2_PORT3_WAKEUP_EVENT		0x0
#define USB2_HSIC_PORT0_WAKEUP_EVENT	(1 << 11)
#define USB2_HSIC_PORT1_WAKEUP_EVENT	(1 << 12)
#define USB2_HSIC_PORT0_WAKE_INTERRUPT_ENABLE	(1 << 3)
#define USB2_HSIC_PORT1_WAKE_INTERRUPT_ENABLE	(1 << 4)

#define XUSB_ALL_WAKE_EVENT	\
		(USB2_PORT0_WAKEUP_EVENT | USB2_PORT1_WAKEUP_EVENT |	\
		USB2_PORT2_WAKEUP_EVENT |	\
		SS_PORT0_WAKEUP_EVENT | SS_PORT1_WAKEUP_EVENT |	\
		SS_PORT2_WAKEUP_EVENT |	\
		USB2_HSIC_PORT0_WAKEUP_EVENT)
#endif

/* PADCTL register offset (Shared T124/T210/T114)*/
#define USB2_PORT0_WAKE_INTERRUPT_ENABLE	(1 << 0)
#define USB2_PORT1_WAKE_INTERRUPT_ENABLE	(1 << 1)
#define USB2_PORT2_WAKE_INTERRUPT_ENABLE	(1 << 2)


#define XUSB_PADCTL_USB2_PAD_MUX_0		0x4
#define PAD_PORT_MASK(_p)	(0x3 << (_p * 2))
#define PAD_PORT_SNPS(_p)	(0x0 << (_p * 2))
#define PAD_PORT_XUSB(_p)	(0x1 << (_p * 2))
#define XUSB_OTG_MODE		3
#define XUSB_DEVICE_MODE	2
#define XUSB_HOST_MODE		1

#define XUSB_PADCTL_USB2_PORT_CAP_0		0x8
#define USB2_OTG_PORT_CAP(_p, val)	((val & 0x3) << (_p * 4))
#define USB2_PORT_CAP_REVERSE_ID(x) (0x1 << ((4 * (x + 1)) - 1))

#define CLK_RST_PLLU_HW_PWRDN_CFG0_0	0x530
#define PLLU_CLK_ENABLE_OVERRIDE_VALUE	(1 << 3)
#define PLLU_SEQ_IN_SWCTL				(1 << 4)

/* PAD MUX */
#ifdef CONFIG_ARCH_TEGRA_21x_SOC
/* XUSB_PADCTL_USB2_PAD_MUX_0 */
#define BIAS_PAD_MASK	(0x3 << 18)
#define BIAS_PAD_XUSB	(0x1 << 18)
#define HSIC_PAD_TRK(x)	(((x) & 0x3) << 16)
#define  HSIC_PAD_TRK_SNPS	(0)
#define  HSIC_PAD_TRK_XUSB	(1)

#define XUSB_PADCTL_USB3_PAD_MUX_0		0x28
#define XUSB_PADCTL_USB3_PAD_MUX_FORCE_PCIE_PAD_IDDQ_DISABLE_MASK0	(1 << 1)
#define XUSB_PADCTL_USB3_PAD_MUX_FORCE_PCIE_PAD_IDDQ_DISABLE_MASK1	(1 << 2)
#define XUSB_PADCTL_USB3_PAD_MUX_FORCE_PCIE_PAD_IDDQ_DISABLE_MASK2	(1 << 3)
#define XUSB_PADCTL_USB3_PAD_MUX_FORCE_PCIE_PAD_IDDQ_DISABLE_MASK3	(1 << 4)
#define XUSB_PADCTL_USB3_PAD_MUX_FORCE_PCIE_PAD_IDDQ_DISABLE_MASK4	(1 << 5)
#define XUSB_PADCTL_USB3_PAD_MUX_FORCE_PCIE_PAD_IDDQ_DISABLE_MASK5	(1 << 6)
#define XUSB_PADCTL_USB3_PAD_MUX_FORCE_PCIE_PAD_IDDQ_DISABLE_MASK6	(1 << 7)
#define XUSB_PADCTL_USB3_PAD_MUX_FORCE_SATA_PAD_IDDQ_DISABLE_MASK0	(1 << 8)
#define XUSB_PADCTL_USB3_PAD_MUX_PCIE_PAD_LANE0	(0x3 << 12)
#define XUSB_PADCTL_USB3_PAD_MUX_PCIE_PAD_LANE1	(0x3 << 14)
#define XUSB_PADCTL_USB3_PAD_MUX_PCIE_PAD_LANE2	(0x3 << 16)
#define XUSB_PADCTL_USB3_PAD_MUX_PCIE_PAD_LANE3	(0x3 << 18)
#define XUSB_PADCTL_USB3_PAD_MUX_PCIE_PAD_LANE4	(0x3 << 20)
#define XUSB_PADCTL_USB3_PAD_MUX_PCIE_PAD_LANE5	(0x3 << 22)
#define XUSB_PADCTL_USB3_PAD_MUX_PCIE_PAD_LANE6	(0x3 << 24)
#define XUSB_PADCTL_USB3_PAD_MUX_SATA_PAD_LANE0	(0x3 << 30)
#define XUSB_PADCTL_USB3_PAD_MUX_SATA_PAD_LANE0_OWNER_USB3_SS	(0x1 << 30)
#define XUSB_PADCTL_USB3_PAD_MUX_PCIE_PAD_LANE0_OWNER_USB3_SS	(0x1 << 12)
#define XUSB_PADCTL_USB3_PAD_MUX_PCIE_PAD_LANE3_OWNER_USB3_SS	(0x1 << 18)
#define XUSB_PADCTL_USB3_PAD_MUX_PCIE_PAD_LANE4_OWNER_USB3_SS	(0x1 << 20)
#define XUSB_PADCTL_USB3_PAD_MUX_PCIE_PAD_LANE5_OWNER_USB3_SS	(0x1 << 22)
#define XUSB_PADCTL_USB3_PAD_MUX_PCIE_PAD_LANE6_OWNER_USB3_SS	(0x1 << 24)
#define PAD_MUX_PAD_LANE(_lane, val)	\
	((_lane == 8) ? ((val & 0x3) << 30) :	\
	((val & 0x3) << (12 + _lane * 2)))
#define PAD_MUX_PAD_LANE_IDDQ(_lane, val)	\
	((_lane == 8) ? ((val & 0x1) << 8) :	\
	((val & 0x1) << (_lane + 1)))

#define XUSB_PADCTL_USB2_BIAS_PAD_CTL_0		0x284
#define HS_SQUELCH_LEVEL(x)		((x & 0x7) << 0)
#define HS_DISCON_LEVEL(x)				(((x) & 0x7) << 3)
#define XUSB_PADCTL_USB2_BIAS_PAD_CTL_1		0x288
#define PD_MASK			(0x1 << 11)
#define PD				(0x0 << 11)
#define PD_TRK_MASK		(0x1 << 26)
#define PD_TRK			(0x0 << 26)
#define TRK_START_TIMER_MASK		(0x7F << 12)
#define TRK_START_TIMER				(0x1E << 12)
#define TRK_DONE_RESET_TIMER_MASK	(0x7F << 19)
#define TRK_DONE_RESET_TIMER		(0xA << 19)
#define GET_PCTRL(x)	((x & 0xfc0) >> 6)
#define GET_TCTRL(x)	((x & 0x3f) >> 0)



#define XUSB_PADCTL_HSIC_PAD_TRK_CTL_0		0x340
#define HSIC_TRK_START_TIMER(x)		(((x) & 0x7F) << 5)
#define HSIC_TRK_DONE_RESET_TIMER(x)		(((x) & 0x7F) << 12)
#define HSIC_PD_TRK(x)				(((x) & 0x1) << 19)

#define XUSB_PADCTL_HSIC_PAD1_CTL_0_0	0x320
#define PAD1_PD_TX_DATA0		(1 << 1)
#define PAD1_PD_TX_DATA1		(1 << 2)
#define PAD1_PD_TX_STROBE	(1 << 3)
#define PAD1_PD_TX_MASK	\
	(PAD1_PD_TX_DATA0 | PAD1_PD_TX_DATA1 | PAD1_PD_TX_STROBE)


#define XUSB_PADCTL_VBUS_OC_MAP_0	0x18
#define VBUS_OC_MAP(_p, val)	((val & 0xf) << ((_p * 5) + 1))
#define VBUS_ENABLE(x)	  (1 << ((x)*5))
#define OC_DISABLE		(0xf)

#define XUSB_PADCTL_OC_DET_0	0x1c
#define VBUS_EN_OC_MAP(x, v)	0x0
#define SET_OC_DETECTED(x)			(1 << (x))
#define OC_DETECTED(x)				(1 << (8 + (x)))
#define OC_DETECTED_VBUS_PAD(x)		(1 << (12 + (x)))
#define OC_DETECTED_VBUS_PAD_MASK	(0xf << 12)
#define OC_DETECTED_INTR_ENABLE(x)	(1 << (20 + (x)))
#define OC_DETECTED_INTR_ENABLE_VBUS_PAD(x)	(1 << (24 + (x)))

#define XUSB_PADCTL_USB2_OTG_PAD_CTL_0(_p)	(0x88 + _p * 0x40)
#define USB2_OTG_HS_CURR_LVL	(0x3F << 0)
#define USB2_OTG_PD		(0x1 << 26)
#define USB2_OTG_PD2	(0x1 << 27)
#define USB2_PD2_OVRD_EN (0x1 << 28)
#define USB2_OTG_PD_ZI	(0x1 << 29)

#define XUSB_PADCTL_USB2_OTG_PAD_CTL_1(_p)	(0x8c + _p * 0x40)
#define USB2_OTG_TERM_RANGE_ADJ	(0xF << 3)
#define USB2_OTG_PD_DR			(0x1 << 2)
#define USB2_OTG_PD_DISC_FORCE_POWERUP	(0x1 << 1)
#define USB2_OTG_PD_CHRP_FORCE_POWERUP	(0x1 << 0)
#define RPD_CTRL				(0x1f << 26)
#define GET_RPD_CTRL(x)			((x & 0x7c000000) >> 26)
#define USB2_OTG_HS_IREF_CAP			0x0


#define XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD_CTL0(_p)	\
	(0x80 + _p * 0x40)

#define XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD_CTL1(_p)	\
	(0x84 + _p * 0x40)
#define VREG_FIX18	(1 << 6)
#define VREG_LEV	(0x3 << 7)
#define VREG_LEV_EN	(0x1 << 7)

#define XUSB_PADCTL_USB2_OC_MAP_0	0x10
#define PORT_OC_PIN(_p, val)	((val & 0xf) << (_p * 4))
#define OC_VBUS_PAD(p)	(p + 4)
#define OC_DISABLED	0xf

#define XUSB_PADCTL_SS_PORT_MAP	0x14
#define SS_PORT_MAP(_p, val) \
	((val & 0x7) << (_p * 5))

#define XUSB_PADCTL_UPHY_USB3_ECTL_2_0(p) (0xa64 + p * 0x40)
#define XUSB_PADCTL_UPHY_USB3_ECTL_2_0_RX_CTLE_MASK 0xffff
#define XUSB_PADCTL_UPHY_USB3_ECTL_3_0(p) (0xa68 + p * 0x40)
#define XUSB_PADCTL_UPHY_USB3_ECTL_4_0(p) (0xa6c + p * 0x40)
#define XUSB_PADCTL_UPHY_USB3_ECTL_4_0_RX_CDR_CTRL_MASK (0xffff << 16)
#define XUSB_PADCTL_UPHY_USB3_ECTL_6_0(p) (0xa74 + p * 0x40)

#define XUSB_PADCTL_USB2_VBUS_ID_0	0xc60
#define VBUS_SOURCE_SELECT(val)	((val & 0x3) << 12)
#define ID_SOURCE_SELECT(val)	((val & 0x3) << 16)
#define USB2_VBUS_ID_0_VBUS_OVERRIDE    (1 << 14)
#define IDDIG_CHNG_INTR_EN		(1 << 11)
#define USB2_VBUS_ID_0_ID_SRC_OVERRIDE		(0x1 << 16)
#define USB2_VBUS_ID_0_ID_OVERRIDE		(0xf << 18)
#define USB2_VBUS_ID_0_ID_OVERRIDE_RID_FLOAT	(0x8 << 18)
#define USB2_VBUS_ID_0_ID_OVERRIDE_RID_GND	(0x0 << 18)
#define USB2_VBUS_ID_0_VBUS_SESS_VLD_STS	(0x1 << 0)
#define USB2_VBUS_ID_0_VBUS_SESS_VLD_STS_CHG	(0x1 << 1)
#define USB2_VBUS_ID_0_VBUS_SESS_VLD_CHG_INT_EN	(0x1 << 2)
#define USB2_VBUS_ID_0_VBUS_VLD_STS		(0x1 << 3)
#define USB2_VBUS_ID_0_VBUS_VLD_STS_CHG		(0x1 << 4)
#define USB2_VBUS_ID_0_VBUS_VLD_CHG_INT_EN	(0x1 << 5)
#define USB2_VBUS_ID_0_IDDIG_STS		(0x1 << 6)
#define USB2_VBUS_ID_0_IDDIGA_STS		(0x1 << 7)
#define USB2_VBUS_ID_0_IDDIGB_STS		(0x1 << 8)
#define USB2_VBUS_ID_0_IDDIGC_STS		(0x1 << 9)
#define USB2_VBUS_ID_0_RID_MASK			(0xf << 6)
#define USB2_VBUS_ID_0_RID_FLOAT		USB2_VBUS_ID_0_IDDIG_STS
#define USB2_VBUS_ID_0_RID_A			USB2_VBUS_ID_0_IDDIGA_STS
#define USB2_VBUS_ID_0_RID_B			USB2_VBUS_ID_0_IDDIGB_STS
#define USB2_VBUS_ID_0_RID_C			USB2_VBUS_ID_0_IDDIGC_STS
#define USB2_VBUS_ID_0_RID_GND			(0x0 << 6)
#define USB2_VBUS_ID_0_IDDIG_STS_CHG		(0x1 << 10)
#define USB2_VBUS_ID_0_IDDIG_CHG_INT_EN		(0x1 << 11)
#define USB2_VBUS_ID_0_VBUS_SRC_SELECT		(0x3 << 12)
#define USB2_VBUS_ID_0_VBUS_SRC_OVERRIDE	(0x1 << 12)
#define USB2_VBUS_ID_0_VBUS_WKUP_OVERRIDE	(0x1 << 15)
#define USB2_VBUS_ID_0_ID_SRC_SELECT		(0x3 << 16)
#define USB2_VBUS_ID_0_VBUS_WKUP_STS		(0x1 << 22)
#define USB2_VBUS_ID_0_VBUS_WKUP_STS_CHG	(0x1 << 23)
#define USB2_VBUS_ID_0_VBUS_WKUP_CHG_INT_EN	(0x1 << 24)
#define USB2_VBUS_ID_0_INTR_STS_CHG_MASK (USB2_VBUS_ID_0_VBUS_VLD_STS_CHG | \
					USB2_VBUS_ID_0_VBUS_SESS_VLD_STS_CHG | \
					USB2_VBUS_ID_0_VBUS_WKUP_STS_CHG)

#define XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD_CTL0_0(x) (0x80 + 0x40 * x)
#define USB2_BATTERY_CHRG_OTGPAD_GENERATE_SRP		(1 << 31)
#define USB2_BATTERY_CHRG_OTGPAD_SRP_INTR_EN		(1 << 30)
#define USB2_BATTERY_CHRG_OTGPAD_SRP_DETECTED		(1 << 29)
#define USB2_BATTERY_CHRG_OTGPAD_SRP_DETECT_EN		(1 << 28)
#define USB2_BATTERY_CHRG_OTGPAD_DCD_DETECTED		(1 << 26)
#define USB2_BATTERY_CHRG_OTGPAD_INTR_STS_CHG_MASK (\
			USB2_BATTERY_CHRG_OTGPAD_SRP_DETECTED | \
			USB2_BATTERY_CHRG_OTGPAD_DCD_DETECTED)

#else
#define XUSB_PADCTL_USB3_PAD_MUX_0		0x134
#define XUSB_PADCTL_USB3_PAD_MUX_FORCE_PCIE_PAD_IDDQ_DISABLE_MASK0	(1 << 1)
#define XUSB_PADCTL_USB3_PAD_MUX_FORCE_PCIE_PAD_IDDQ_DISABLE_MASK1	(1 << 2)
#define XUSB_PADCTL_USB3_PAD_MUX_FORCE_PCIE_PAD_IDDQ_DISABLE_MASK2	(1 << 3)
#define XUSB_PADCTL_USB3_PAD_MUX_FORCE_PCIE_PAD_IDDQ_DISABLE_MASK3	(1 << 4)
#define XUSB_PADCTL_USB3_PAD_MUX_FORCE_PCIE_PAD_IDDQ_DISABLE_MASK4	(1 << 5)
#define XUSB_PADCTL_USB3_PAD_MUX_FORCE_SATA_PAD_IDDQ_DISABLE_MASK0	(1 << 6)
#define XUSB_PADCTL_USB3_PAD_MUX_PCIE_PAD_LANE0	(0x3 << 16)
#define XUSB_PADCTL_USB3_PAD_MUX_PCIE_PAD_LANE1	(0x3 << 18)
#define XUSB_PADCTL_USB3_PAD_MUX_PCIE_PAD_LANE2	(0x3 << 20)
#define XUSB_PADCTL_USB3_PAD_MUX_PCIE_PAD_LANE3	(0x3 << 22)
#define XUSB_PADCTL_USB3_PAD_MUX_PCIE_PAD_LANE4	(0x3 << 24)
#define XUSB_PADCTL_USB3_PAD_MUX_SATA_PAD_LANE0	(0x3 << 26)
#define XUSB_PADCTL_USB3_PAD_MUX_SATA_PAD_LANE0_OWNER_USB3_SS	(0x1 << 26)
#define XUSB_PADCTL_USB3_PAD_MUX_PCIE_PAD_LANE0_OWNER_USB3_SS	(0x1 << 16)
#define XUSB_PADCTL_USB3_PAD_MUX_PCIE_PAD_LANE1_OWNER_USB3_SS	(0x1 << 18)

#ifdef CONFIG_ARCH_TEGRA_11x_SOC
#define XUSB_PADCTL_USB2_BIAS_PAD_CTL_0		0xa0
#else
#define XUSB_PADCTL_USB2_BIAS_PAD_CTL_0		0xb8
#endif
#define PD_MASK		(0x1 << 12)

#define XUSB_PADCTL_VBUS_OC_MAP_0	0x0
#define VBUS_OC_MAP(_p, val)	0x0
#define VBUS_ENABLE(x)	  0x0
#define OC_DISABLE		0x0

#define XUSB_PADCTL_OC_DET_0	0x18
#define VBUS_EN_OC_MAP(x, v) \
	(((x) == 2) ? \
	(((v) & 0x7) << 5) : (((v) & 0x7) << (10 + 3 * (x))))
#define SET_OC_DETECTED(x)			(1 << (x))
#define OC_DETECTED(x)				(1 << (16 + (x)))
#define OC_DETECTED_VBUS_PAD(x)		(1 << (20 + (x)))
#define OC_DETECTED_VBUS_PAD_MASK	(0x7 << 20)
#define OC_DETECTED_INTR_ENABLE(x)	(1 << (24 + (x)))
#define OC_DETECTED_INTR_ENABLE_VBUS_PAD(x)	(1 << (28 + (x)))

#define XUSB_PADCTL_USB2_OTG_PAD_CTL_0(_p)	(0xa0 + _p * 0x4)
#define USB2_OTG_HS_CURR_LVL	(0x3F << 0)
#define USB2_OTG_PD				(0x1 << 19)
#define USB2_OTG_PD2			(0x1 << 20)
#define USB2_OTG_PD_ZI			(0x1 << 21)

#define XUSB_PADCTL_USB2_OTG_PAD_CTL_1(_p)	(0xac + _p * 0x4)
#define USB2_OTG_PD_DR					(0x1 << 2)
#define USB2_OTG_TERM_RANGE_ADJ			(0xF << 3)
#define USB2_OTG_HS_IREF_CAP			(0x3 << 9)
#define USB2_OTG_PD_CHRP_FORCE_POWERUP	(0x1 << 0)
#define USB2_OTG_PD_DISC_FORCE_POWERUP	(0x1 << 1)
#define RPD_CTRL						0x0

#define XUSB_PADCTL_USB2_OC_MAP_0	0x10
#define PORT_OC_PIN(_p, val)	((val & 0xf) << (_p * 3))
#define OC_VBUS_PAD(p)	(p + 4)
#define OC_DISABLED	0xf

#define XUSB_PADCTL_SS_PORT_MAP	0x14
#define SS_PORT_MAP(_p, val) \
	((val & 0x7) << (_p * 4))

#define XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD_CTL1(_p)	0x0
#define VREG_FIX18	0x0
#define VREG_LEV	0x0
#define VREG_LEV_EN	0x0
#define RPD_CTRL	0x0

#define XUSB_PADCTL_USB2_VBUS_ID_0	0x0
#define VBUS_SOURCE_SELECT(val)	0x0
#define ID_SOURCE_SELECT(val)	0x0
#define IDDIG_CHNG_INTR_EN		0x0

#define HSIC_PAD_TRK	0x0
#define TRK_START_TIMER_MASK		0x0
#define TRK_DONE_RESET_TIMER_MASK	0x0

#define XUSB_PADCTL_HSIC_PAD_TRK_CTL_0	0x0
#define HSIC_TRK_START_TIMER_MASK		0x0
#define HSIC_TRK_DONE_RESET_TIMER_MASK	0x0
#define HSIC_PD_TRK_MASK	0x0

#define XUSB_PADCTL_HSIC_PAD1_CTL_0_0	0x0
#define PAD1_PD_TX_MASK	0x0
#endif
/* SATA PADPLL */
#define CLK_RST_CONTROLLER_SATA_PLL_CFG0_0	0x490
#define SATA_PADPLL_RESET_SWCTL			(1 << 0)
#define SATA_PADPLL_USE_LOCKDET			(1 << 2)
#define SATA_SEQ_IN_SWCTL			(1 << 4)
#define SATA_SEQ_RESET_INPUT_VALUE		(1 << 5)
#define SATA_SEQ_LANE_PD_INPUT_VALUE		(1 << 6)
#define SATA_SEQ_PADPLL_PD_INPUT_VALUE		(1 << 7)
#define SATA_PADPLL_SLEEP_IDDQ			(1 << 13)
#define SATA_SEQ_ENABLE				(1 << 24)
#define SATA_SEQ_START_STATE			(1 << 25)

#define CLK_RST_CONTROLLER_XUSBIO_PLL_CFG0_0		0x51C
#define XUSBIO_PADPLL_RESET_SWCTL			(1 << 0)
#define XUSBIO_CLK_ENABLE_SWCTL			(1 << 2)
#define XUSBIO_PADPLL_USE_LOCKDET			(1 << 6)
#define XUSBIO_PADPLL_SLEEP_IDDQ			(1 << 13)
#define XUSBIO_SEQ_ENABLE				(1 << 24)

void tegra_xhci_release_otg_port(bool release);
void tegra_xhci_release_dev_port(bool release);
void tegra_xhci_ss_wake_on_interrupts(u32 portmap, bool enable);
void tegra_xhci_hs_wake_on_interrupts(u32 portmap, bool enable);
void tegra_xhci_ss_wake_signal(u32 portmap, bool enable);
void tegra_xhci_ss_vcore(u32 portmap, bool enable);

#ifndef CONFIG_ARCH_TEGRA_21x_SOC
int utmi_phy_pad_disable(void);
int utmi_phy_pad_enable(void);
#else
int utmi_phy_pad_disable(struct tegra_prod_list *prod_list);
int utmi_phy_pad_enable(struct tegra_prod_list *prod_list);
#endif
int usb3_phy_pad_enable(u32 lane_owner);
int pcie_phy_pad_enable(bool enable, int lane_owner);
#ifdef CONFIG_ARCH_TEGRA_21x_SOC
bool tegra_phy_get_lane_rdet(u8 lane_num);
#endif

int utmi_phy_iddq_override(bool set);
void tegra_usb_pad_reg_update(u32 reg_offset, u32 mask, u32 val);
u32 tegra_usb_pad_reg_read(u32 reg_offset);
void tegra_usb_pad_reg_write(u32 reg_offset, u32 val);

void xusb_utmi_pad_init(int pad, u32 cap, bool external_pmic);
void xusb_ss_pad_init(int pad, int port_map, u32 cap);
void usb2_vbus_id_init(void);
int hsic_trk_enable(void);

int pex_usb_pad_pll_reset_assert(void);
int pex_usb_pad_pll_reset_deassert(void);
int sata_usb_pad_pll_reset_assert(void);
int sata_usb_pad_pll_reset_deassert(void);
int t210_sata_uphy_pll_init(bool sata_used_by_xusb);

int tegra_pd2_asserted(int pad);
int tegra_pd2_deasserted(int pad);

void xusb_utmi_pad_deinit(int pad);
void xusb_ss_pad_deinit(int pad);
void usb3_phy_pad_disable(void);
void xusb_enable_pad_protection(bool);

void xusb_utmi_pad_driver_power(int port, bool on);

int tegra_padctl_init_sata_pad(void);
int tegra_padctl_enable_sata_pad(bool enable);

#endif
