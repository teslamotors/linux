/*
 * arch/arm/mach-tegra/include/mach/xusb.h
 *
 * Copyright (c) 2013-2015 NVIDIA Corporation. All rights reserved.
 *
 * Author:
 *	Ajay Gupta <ajayg@nvidia.com>
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
#ifndef _XUSB_H
#define _XUSB_H

#include <linux/types.h>

/*
 * BIT0 - BIT7 : SS ports
 * BIT8 - BIT15 : USB2 UTMI ports
 * BIT16 - BIT23 : HSIC ports
 * BIT24 - BIT31 : ULPI ports
 */
#define XUSB_SS_INDEX	(0)
#define TEGRA_XUSB_SS_P0	(1 << 0)
#define TEGRA_XUSB_SS_P1	(1 << 1)
#define TEGRA_XUSB_SS_P2	(1 << 2)
#define TEGRA_XUSB_SS_P3	(1 << 3)

#define XUSB_UTMI_INDEX	(8)
#define XUSB_UTMI_COUNT (3)
#define TEGRA_XUSB_USB2_P0	BIT(XUSB_UTMI_INDEX)
#define TEGRA_XUSB_USB2_P1	BIT(XUSB_UTMI_INDEX + 1)
#define TEGRA_XUSB_USB2_P2	BIT(XUSB_UTMI_INDEX + 2)
#define TEGRA_XUSB_USB2_P3	BIT(XUSB_UTMI_INDEX + 3)

#define XUSB_HSIC_INDEX	(16)
#define XUSB_HSIC_COUNT	(2)
#define TEGRA_XUSB_HSIC_P0	BIT(XUSB_HSIC_INDEX)
#define TEGRA_XUSB_HSIC_P1	BIT(XUSB_HSIC_INDEX + 1)

#define TEGRA_XUSB_ULPI_P0	(1 << 24)
#define XUSB_SS_PORT_COUNT	(2)
#define TEGRA_XUSB_SS_PORT_MAP_USB2_P0 (0x0)
#define TEGRA_XUSB_SS_PORT_MAP_USB2_P1 (0x1)
#define TEGRA_XUSB_SS_PORT_MAP_USB2_P2 (0x2)
#define TEGRA_XUSB_SS_PORT_MAP	(0xf0)
#define TEGRA_XUSB_ULPI_PORT_CAP_MASTER	(0x0)
#define TEGRA_XUSB_ULPI_PORT_CAP_PHY	(0x1)
#define TEGRA_XUSB_UTMIP_PMC_PORT0	(0x0)
#define TEGRA_XUSB_UTMIP_PMC_PORT1	(0x1)
#define TEGRA_XUSB_UTMIP_PMC_PORT2	(0x2)
#define TEGRA_XUSB_UTMIP_PMC_PORT3	(0x3)

struct tegra_xusb_regulator_name {
	const char *s3p3v;
	const char *s1p8v;
	const char *vddio_hsic;
	const char *s1p05v;
	char **utmi_vbuses;
};

/* Ensure dt compatiblity when changing order */
struct tegra_xusb_hsic_config {
	u8 rx_strobe_trim;
	u8 rx_data_trim;
	u8 tx_rtune_n;
	u8 tx_rtune_p;
	u8 tx_slew_n;
	u8 tx_slew_p;
	u8 auto_term_en;
	u8 strb_trim_val;
	u8 pretend_connect;
};

enum vbus_en_type {
	VBUS_FIXED = 0, /* VBUS enabled by GPIO, without PADCTL OC detection */
	VBUS_FIXED_OC,  /* VBUS enabled by GPIO, with PADCTL detection */
	VBUS_EN_OC,     /* VBUS enabled by XUSB PADCTL */
};

enum vbus_en_pin {
	VBUS_EN0 = 0,
	VBUS_EN1,
	VBUS_EN2,
};

struct usb_vbus_en_oc {
	enum vbus_en_type type;
	enum vbus_en_pin pin;
};

struct tegra_xusb_board_data {
	u32	portmap;
	/*
	 * SS0 or SS1 port may be mapped either to USB2_P0 or USB2_P1
	 * ss_portmap[0:3] = SS0 map, ss_portmap[4:7] = SS1 map
	 */
	u32	ss_portmap;
	u32	otg_portmap;
	u32	ulpicap;
	u32	lane_owner;
	bool uses_external_pmic;
	bool gpio_controls_muxed_ss_lanes;
	u32 gpio_ss1_sata;
	u32 portcap;
	struct tegra_xusb_hsic_config hsic[XUSB_HSIC_COUNT];
	const char *firmware_file_dt;
	struct usb_vbus_en_oc *vbus_en_oc;
	u32	hsic_power_on_at_boot;
};

struct tegra_xusb_platform_data {
	u32 portmap;
	u8 lane_owner;
	bool pretend_connect_0;
};

struct tegra_xusb_chip_calib {
	u32 hs_squelch_level;
};

struct tegra_xusb_padctl_regs {
	u16 boot_media_0;
	u16 usb2_pad_mux_0;
	u16 usb2_port_cap_0;
	u16 snps_oc_map_0;
	u16 usb2_oc_map_0;
	u16 ss_port_map_0;
	u16 vbus_oc_map;
	u16 oc_det_0;
	u16 elpg_program_0;
	u16 elpg_program_1;
	u16 uphy_cfg_stb_0;
	u16 usb2_bchrg_otgpadX_ctlY_0[4][2];
	u16 usb2_bchrg_bias_pad_0;
	u16 usb2_bchrg_tdcd_dbnc_timer_0;
	u16 iophy_pll_p0_ctlY_0[4];
	u16 iophy_usb3_padX_ctlY_0[2][4];
	u16 iophy_misc_pad_pX_ctlY_0[5][6];
	u16 usb2_otg_padX_ctlY_0[4][2];
	u16 usb2_bias_pad_ctlY_0[2];
	u16 usb2_hsic_padX_ctlY_0[2][3];
	u16 ulpi_link_trim_ctl0;
	u16 ulpi_null_clk_trim_ctl0;
	u16 hsic_strb_trim_ctl0;
	u16 wake_ctl0;
	u16 pm_spare0;
	u16 usb3_pad_mux_0;
	u16 iophy_pll_s0_ctlY_0[4];
	u16 iophy_misc_pad_s0_ctlY_0[6];
	u16 hsic_pad_trk_ctl_0;
	u16 uphy_pll_p0_ctlY_0[11];
	u16 uphy_misc_pad_pX_ctlY_0[7][9];
	u16 uphy_pll_s0_ctlY_0[11];
	u16 uphy_misc_pad_s0_ctlY_0[9];
	u16 uphy_usb3_padX_ectlY_0[4][6];
	u16 uphy_usb3_padX_ctl_0[4];
	u16 usb2_vbus_id_0;
};

struct tegra_xusb_soc_config {
	struct tegra_xusb_board_data *bdata;
	/*
	 * BIT[0:3] = PMC port # for USB2_P0
	 * BIT[4:7] = PMC port # for USB2_P1
	 * BIT[8:11] = PMC port # for USB2_P2
	 */
	u32 pmc_portmap;
	/* chip specific */
	unsigned long quirks;
	u32 utmi_pad_count;
	u32 ss_pad_count;
	struct tegra_xusb_regulator_name supply;
	const char *default_firmware_file;
	const struct tegra_xusb_padctl_regs *padctl_offsets;
	void (*check_lane_owner_by_pad) (int pad, u32 lane_owner);

	/* applicable to T210 */
	u32 tx_term_ctrl;
	u32 rx_ctle;
	u32 rx_dfe;
	u32 rx_cdr_ctrl;
	u32 rx_eq_ctrl_h;

	/* applicable to T114/T124/T132 */
	u32 rx_wander;
	u32 rx_eq;
	u32 cdr_cntl;
	u32 dfe_cntl;
	u32 hs_slew;
	u32 hs_disc_lvl;
	u32 spare_in;
	u32 ls_rslew_pad[];
};

#define TEGRA_XUSB_USE_HS_SRC_CLOCK2 BIT(0)

#endif /* _XUSB_H */
