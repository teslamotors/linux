/*
 * xhci-tegra-T210-padreg.h - Nvidia xHCI host controller related data
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

/* xusb_padctl register offsets */

/* XUSB_PADCTL_USB2_PAD_MUX_0 0x4 */
#define USB2_OTG_PAD_PORT_MASK(x)		(0x3 << (2 * x))
#define USB2_OTG_PAD_PORT_OWNER_XUSB(x) (0x1 << (2 * x))
#define USB2_ULPI_PAD				(0x1 << 12)
#define USB2_ULPI_PAD_OWNER_XUSB	(0x1 << 12)
#define USB2_HSIC_PAD_PORT(p)	(1 << (14 + p * 1))

/* XUSB_PADCTL_USB2_PORT_CAP_0 0x8 */
#define USB2_PORT_CAP_MASK(x)	(0x3 << (4 * x))
#define USB2_PORT_CAP_HOST(x)	(0x1 << (4 * x))
#define USB2_PORT_CAP_OTG(x)	(0x3 << (4 * x))
#define USB2_ULPI_PORT_CAP		(0x1 << 24)

/* XUSB_PADCTL_IOPHY_USB3_PAD0_CTL_2_0 0x58 */
/* XUSB_PADCTL_IOPHY_USB3_PAD1_CTL_2_0 0x5c */
#define IOPHY_USB3_RXWANDER		(0xF << 4)
#define IOPHY_USB3_RXEQ			(0xFFFF << 8)
#define IOPHY_USB3_CDRCNTL		(0xFF << 24)

/* XUSB_PADCTL_USB2_BIAS_PAD_CTL_0_0 0xb8 */
#define USB2_BIAS_HS_SQUELCH_LEVEL	(0x3 << 0)
#define USB2_BIAS_HS_DISCON_LEVEL	(0x7 << 2)

/* XUSB_PADCTL_USB2_OC_MAP_0 0x10 */
#define OC_DETECTION_DISABLED 0x7
#define USB2_OC_MAP(x, val) ((val & 0x7) << (3 * x))

/* XUSB_PADCTL_USB2_OTG_PAD0_CTL_0_0 0xa0*/
/* XUSB_PADCTL_USB2_OTG_PAD1_CTL_0_0 0xa4*/
/* XUSB_PADCTL_USB2_OTG_PAD2_CTL_0_0 0xa8*/
#define USB2_OTG_HS_CURR_LVL	(0x3F << 0)
#define USB2_OTG_HS_SLEW		(0x3F << 6)
#define USB2_OTG_FS_SLEW		(0x3 << 12)
#define USB2_OTG_LS_RSLEW		(0x3 << 14)
#define USB2_OTG_LS_FSLEW		(0x3 << 16)
#define USB2_OTG_PD				(0x1 << 19)
#define USB2_OTG_PD2			(0x1 << 20)
#define USB2_OTG_PD_ZI			(0x1 << 21)

/* XUSB_PADCTL_USB2_OTG_PAD0_CTL_1_0 0xac*/
/* XUSB_PADCTL_USB2_OTG_PAD1_CTL_1_0 0xb0*/
/* XUSB_PADCTL_USB2_OTG_PAD2_CTL_1_0 0xb4*/
#define USB2_OTG_PD_CHRP_FORCE_POWERUP	(0x1 << 0)
#define USB2_OTG_PD_DISC_FORCE_POWERUP	(0x1 << 1)
#define USB2_OTG_PD_DR					(0x1 << 2)
#define USB2_OTG_TERM_RANGE_AD			(0xF << 3)
#define USB2_OTG_HS_IREF_CAP			(0x3 << 9)

/* XUSB_PADCTL_SS_PORT_MAP_0 0x14 */
#define SS_PORT_MAP(_p, val) \
	((val & 0x7) << (_p * 4))

/* XUSB_PADCTL_OC_DET_0 0x18 */
#define OC_DET_OC_DETECTED_VBUS_PAD0 (0x1 << 20)
#define OC_DET_OC_DETECTED_VBUS_PAD1 (0x1 << 21)
#define OC_DET_OC_DETECTED_VBUS_PAD2 (0x1 << 22)
#define OC_DET_OC_DETECTED_VBUS_PAD3 (0x0)

/* DFE/CTLE */
#if defined(CONFIG_ARCH_TEGRA_12x_SOC)
#define tegra_xhci_restore_ctle_context(hcd, port) \
	restore_ctle_context(hcd, port)
#define tegra_xhci_save_ctle_context(hcd, port) \
	save_ctle_context(hcd, port)
#else
/* T114 do not need to save/restore CTLE */
#define tegra_xhci_restore_ctle_context(hcd, port)	\
	do {} while (0)
#define tegra_xhci_save_ctle_context(hcd, port)	\
	do {} while (0)
#endif

/* XUSB_PADCTL_IOPHY_MISC_PAD_P0_CTL_6_0 0x98 */
/* XUSB_PADCTL_IOPHY_MISC_PAD_P1_CTL_6_0 0x9c */
#define MISC_OUT_SEL(x) ((x & 0xFF) << 16)
#define MISC_OUT_TAP_VAL(reg) ((reg & (0x1F << 24)) >> 24)
#define MISC_OUT_AMP_VAL(reg) ((reg & (0x7F << 24)) >> 24)
#define MISC_OUT_G_Z_VAL(reg) ((reg & (0x3F << 24)) >> 24)

/* XUSB_PADCTL_IOPHY_USB3_PAD0_CTL_4_0 0x68 */
/* XUSB_PADCTL_IOPHY_USB3_PAD0_CTL_4_0 0x6c */
#define DFE_CNTL_TAP_VAL(x) ((x & 0x1F) << 24)
#define DFE_CNTL_AMP_VAL(x) ((x & 0x7F) << 16)

/* XUSB_PADCTL_IOPHY_USB3_PAD0_CTL_2_0 0x58*/
/* XUSB_PADCTL_IOPHY_USB3_PAD1_CTL_2_0 0x5c*/
#define RX_EQ_G_VAL(x) ((x & 0x3F) << 8)
#define RX_EQ_Z_VAL(x) ((x & 0x3F) << 16)

/* XUSB_PADCTL_IOPHY_MISC_PAD_P0_CTL_2_0 0x78*/
/* XUSB_PADCTL_IOPHY_MISC_PAD_P1_CTL_2_0 0x7c*/
#define SPARE_IN(x) ((x & 0x3) << 28)

/* XUSB_PADCTL_IOPHY_MISC_PAD_P0_CTL_3_0 0x80*/
/* XUSB_PADCTL_IOPHY_MISC_PAD_P0_CTL_3_0 0x84*/
#define RX_IDLE_MODE			(1 << 18)
#define RX_IDLE_MODE_OVRD		(1 << 19)

/* XUSB_PADCTL_IOPHY_MISC_PAD_P0_CTL_5_0 0x90*/
/* XUSB_PADCTL_IOPHY_MISC_PAD_P1_CTL_5_0 0x94*/
#define RX_QEYE_EN				(1 << 8)

/* XUSB_PADCTL_USB2_BIAS_PAD_CTL_1_0 0xbc */
#define RCTRL(x)			(((x) & 0xffff) << 0)
#define TCTRL(x)			(((x) & 0xffff0000) >> 16)

/* XUSB_PADCTL_HSIC_PAD0_CTL_2_0 0xd0 */
/* XUSB_PADCTL_HSIC_PAD1_CTL_2_0 0xd4 */
#define RX_DATA_TRIM(x)		(((x) & 0xf) << 0)
#define RX_STROBE_TRIM(x)	(((x) & 0xf) << 4)
#define CALIOUT(x)			(((x) & 0xffff) << 16)

/* XUSB_PADCTL_HSIC_PAD0_CTL_0_0 0xc0 */
/* XUSB_PADCTL_HSIC_PAD1_CTL_0_0 0xc4 */
#define TX_RTUNEP(x)	(((x) & 0xf) << 0)
#define TX_RTUNEN(x)	(((x) & 0xf) << 4)
#define TX_SLEWP(x)		(((x) & 0xf) << 8)
#define TX_SLEWN(x)		(((x) & 0xf) << 12)
#define HSIC_OPT(x)		(((x) & 0xf) << 16)

/* XUSB_PADCTL_HSIC_PAD0_CTL_1_0 0xc8 */
/* XUSB_PADCTL_HSIC_PAD1_CTL_1_0 0xcc */
#define GET_HSIC_REG_OFFSET()	(padregs->usb2_hsic_padX_ctlY_0[pad][1])
#define AUTO_TERM_EN	(0x1 << 0)
#define HSIC_IDDQ		(0x1 << 1)
#define PD_TX			(0x1 << 2)
#define PD_TRX			(0x1 << 3)
#define PD_RX			(0x1 << 4)
#define HSIC_PD_ZI		(0x1 << 5)
#define LPBK			(0x1 << 6)
#define RPD_DATA		(0x1 << 7)
#define RPD_STROBE		(0x1 << 8)
#define RPU_DATA		(0x1 << 9)
#define RPU_STROBE		(0x1 << 10)

/* XUSB_PADCTL_HSIC_STRB_TRIM_CONTROL_0 0xe0*/
#define STRB_TRIM_VAL(x)			(((x) & 0x3f) << 0)
