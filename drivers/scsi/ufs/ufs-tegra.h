/*
 * Copyright (c) 2015-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * Authors:
 *      VenkataJagadish.p	<vjagadish@nvidia.com>
 *      Naveen Kumar Arepalli	<naveenk@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */


#ifndef _UFS_TEGRA_H
#define _UFS_TEGRA_H

#include <linux/io.h>
#include <linux/padctrl/padctrl.h>

#define NV_ADDRESS_MAP_MPHY_L0_BASE	0x02470000
#define NV_ADDRESS_MAP_MPHY_L1_BASE	0x02480000
#define NV_ADDRESS_MAP_UFSHC_AUX_BASE	0x02460000

/*
 * M-PHY Registers
 */

#define MPHY_TX_APB_TX_CG_OVR0_0	0x170
#define MPHY_TX_CLK_EN_SYMB	(1 << 1)
#define MPHY_TX_CLK_EN_SLOW	(1 << 3)
#define MPHY_TX_CLK_EN_FIXED	(1 << 4)
#define MPHY_TX_CLK_EN_3X	(1 << 5)

#define MPHY_TX_APB_TX_ATTRIBUTE_34_37_0	0x34
#define TX_ADVANCED_GRANULARITY		(0x8 << 16)
#define TX_ADVANCED_GRANULARITY_SETTINGS	(0x1 << 8)
#define MPHY_GO_BIT	1

#define MPHY_TX_APB_TX_VENDOR0_0	0x100


#define MPHY_RX_APB_CAPABILITY_88_8B_0		0x88
#define RX_HS_G1_SYNC_LENGTH_CAPABILITY(x)	(((x) & 0x3f) << 24)
#define RX_HS_SYNC_LENGTH			0xf

#define MPHY_RX_APB_CAPABILITY_94_97_0		0x94
#define RX_HS_G2_SYNC_LENGTH_CAPABILITY(x)	(((x) & 0x3f) << 0)
#define RX_HS_G3_SYNC_LENGTH_CAPABILITY(x)	(((x) & 0x3f) << 8)

#define MPHY_RX_APB_CAPABILITY_8C_8F_0		0x8c
#define RX_MIN_ACTIVATETIME_CAPABILITY(x)	(((x) & 0xf) << 24)
#define RX_MIN_ACTIVATETIME			0x5

#define	MPHY_RX_APB_CAPABILITY_98_9B_0		0x98
#define RX_ADVANCED_FINE_GRANULARITY(x)		(((x) & 0x1) << 0)
#define	RX_ADVANCED_GRANULARITY(x)		(((x) & 0x3) << 1)
#define	RX_ADVANCED_MIN_ACTIVATETIME(x)		(((x) & 0xf) << 16)
#define RX_ADVANCED_MIN_AT			0xa


#define MPHY_RX_APB_VENDOR2_0		0x184
#define MPHY_RX_APB_VENDOR2_0_RX_CAL_EN		(1 << 15)
#define MPHY_RX_APB_VENDOR2_0_RX_CAL_DONE	(1 << 19)


/* Unipro Vendor registers */

/*
+ * Vendor Specific Attributes
+ */

#define VS_DEBUGSAVECONFIGTIME		0xD0A0
#define VS_DEBUGSAVECONFIGTIME_TREF	0x6
#define SET_TREF(x)			(((x) & 0x7) << 2)

#define MPHY_ADDR_RANGE		0x1fc
#define UFS_AUX_ADDR_RANGE	0x18

/*UFS Clock Defines*/
#define UFSHC_CLK_FREQ		204000000
#define UFSDEV_CLK_FREQ		19200000

enum ufs_state {
	UFSHC_INIT,
	UFSHC_SUSPEND,
	UFSHC_RESUME,
};

/* vendor specific pre-defined parameters */

/*
 * HCLKFrequency in MHz.
 * HCLKDIV is used to generate 1usec tick signal used by Unipro.
 */
#define UFS_VNDR_HCLKDIV_1US_TICK	0xCC


/*UFS host controller vendor specific registers */
enum {
	REG_UFS_VNDR_HCLKDIV	= 0xFC,
};


/*
 * UFS AUX Registers
 */

#define UFSHC_AUX_UFSHC_STATUS_0	0x10
#define UFSHC_HIBERNATE_STATUS		(1 << 0)
#define UFSHC_AUX_UFSHC_DEV_CTRL_0	0x14
#define UFSHC_DEV_CLK_EN		(1 << 0)
#define UFSHC_DEV_RESET			(1 << 1)
#define UFSHC_AUX_UFSHC_SW_EN_CLK_SLCG_0	0x08
#define UFSHC_CLK_OVR_ON	(1 << 0)
#define UFSHC_HCLK_OVR_ON	(1 << 1)
#define UFSHC_LP_CLK_T_CLK_OVR_ON	(1 << 2)
#define UFSHC_CLK_T_CLK_OVR_ON		(1 << 3)
#define UFSHC_CG_SYS_CLK_OVR_ON		(1 << 4)
#define UFSHC_TX_SYMBOL_CLK_OVR_ON	(1 << 5)
#define UFSHC_RX_SYMBOLCLKSELECTED_CLK_OVR_ON		(1 << 6)
#define UFSHC_PCLK_OVR_ON		(1 << 7)

/*
 * MPHY Context save armphy_rx_apb registers
 */

static u16 mphy_rx_apb[] = {
0x080, /* MPHY_RX_APB_CAPABILITY_80_83_0 */
0x084, /* MPHY_RX_APB_CAPABILITY_84_87_0 */
0x088, /* MPHY_RX_APB_CAPABILITY_88_8B_0 */
0x08c, /* MPHY_RX_APB_CAPABILITY_8C_8F_0 */
0x090, /* MPHY_RX_APB_CAPABILITY_90_93_0 */
0x094, /* MPHY_RX_APB_CAPABILITY_94_97_0 */
0x098, /* MPHY_RX_APB_CAPABILITY_98_9B_0 */
0x0a0, /* MPHY_RX_APB_ATTRIBUTE_A0_A3_0  */
0x0a4, /* MPHY_RX_APB_ATTRIBUTE_A4_A7_0  */
0x0a8, /* MPHY_RX_APB_ATTRIBUTE_A8_AB_0  */
0x0d0, /* MPHY_RX_APB_MC_STATUS_D0_D3_0  */
0x0d4, /* MPHY_RX_APB_MC_STATUS_D4_D7_0  */
0x0d8, /* MPHY_RX_APB_MC_STATUS_D8_DB_0  */
0x0dc, /* MPHY_RX_APB_MC_STATUS_DC_DF_0  */
0x0e0, /* MPHY_RX_APB_MC_STATUS_E0_E3_0  */
0x0e4, /* MPHY_RX_APB_MC_STATUS_E4_E7_0  */
0x180, /* MPHY_RX_APB_VENDOR1_0          */
0x184, /* MPHY_RX_APB_VENDOR2_0          */
0x188, /* MPHY_RX_APB_VENDOR3_0          */
0x18c, /* MPHY_RX_APB_VENDOR4_0          */
0x190, /* MPHY_RX_APB_VENDOR5_0          */
0x194, /* MPHY_RX_APB_VENDOR6_0          */
0x198, /* MPHY_RX_APB_VENDOR7_0          */
0x19c, /* MPHY_RX_APB_VENDOR8_0          */
0x1a0, /* MPHY_RX_APB_VENDOR9_0          */
0x1a4, /* MPHY_RX_APB_VENDOR10_0         */
0x1a8, /* MPHY_RX_APB_VENDOR11_0         */
0x1ac, /* MPHY_RX_APB_VENDOR12_0         */
0x1b0, /* MPHY_RX_APB_VENDOR13_0         */
0x1b4, /* MPHY_RX_APB_VENDOR14_0         */
0x1b8, /* MPHY_RX_APB_VENDOR15_0         */
0x1bc, /* MPHY_RX_APB_VENDOR16_0         */
0x1c0, /* MPHY_RX_APB_VENDOR17_0         */
0x1c4, /* MPHY_RX_APB_VENDOR18_0         */
0x1c8, /* MPHY_RX_APB_VENDOR19_0         */
0x1cc, /* MPHY_RX_APB_VENDOR20_0         */
0x1d0, /* MPHY_RX_APB_VENDOR21_0         */
0x1d4, /* MPHY_RX_APB_VENDOR22_0         */
0x1d8, /* MPHY_RX_APB_VENDOR23_0         */
0x1dc, /* MPHY_RX_APB_VENDOR24_0         */
0x1e0, /* MPHY_RX_APB_VENDOR25_0         */
0x1e4, /* MPHY_RX_APB_VENDOR26_0         */
0x1e8, /* MPHY_RX_APB_VENDOR27_0         */
0x1ec, /* MPHY_RX_APB_VENDOR28_0         */
0x1f0, /* MPHY_RX_APB_VENDOR29_0         */
0x1f4, /* MPHY_RX_APB_VENDOR30_0         */
0x1f8, /* MPHY_RX_APB_VENDOR31_0         */
0x1fc  /* MPHY_RX_APB_VENDOR32_0         */
};

/*
 * MPHY Context save armphy_tx_apb registers
 */

static u16 mphy_tx_apb[] = {
0x000, /* MPHY_TX_APB_TX_CAPABILITY_00_03_0 */
0x004, /* MPHY_TX_APB_TX_CAPABILITY_04_07_0 */
0x008, /* MPHY_TX_APB_TX_CAPABILITY_08_0B_0 */
0x00c, /* MPHY_TX_APB_TX_CAPABILITY_0C_0F_0 */
0x010, /* MPHY_TX_APB_TX_CAPABILITY_10_13_0 */
0x020, /* MPHY_TX_APB_TX_ATTRIBUTE_20_23_0  */
0x024, /* MPHY_TX_APB_TX_ATTRIBUTE_24_27_0  */
0x028, /* MPHY_TX_APB_TX_ATTRIBUTE_28_2B_0  */
0x02c, /* MPHY_TX_APB_TX_ATTRIBUTE_2C_2F_0  */
0x030, /* MPHY_TX_APB_TX_ATTRIBUTE_30_33_0  */
0x034, /* MPHY_TX_APB_TX_ATTRIBUTE_34_37_0  */
0x038, /* MPHY_TX_APB_TX_ATTRIBUTE_38_3B_0  */
0x060, /* MPHY_TX_APB_MC_ATTRIBUTE_60_63_0  */
0x064, /* MPHY_TX_APB_MC_ATTRIBUTE_64_67_0  */
0x100, /* MPHY_TX_APB_TX_VENDOR0_0          */
0x104, /* MPHY_TX_APB_TX_VENDOR1_0          */
0x108, /* MPHY_TX_APB_TX_VENDOR2_0          */
0x10c, /* MPHY_TX_APB_TX_VENDOR3_0          */
0x110, /* MPHY_TX_APB_TX_VENDOR4_0          */
0x114, /* MPHY_TX_APB_TX_VENDOR5_0          */
0x118, /* MPHY_TX_APB_TX_VENDOR6_0          */
0x11c, /* MPHY_TX_APB_TX_VENDOR7_0          */
0x120, /* MPHY_TX_APB_PAD_TIMING0_0         */
0x124, /* MPHY_TX_APB_PAD_TIMING1_0         */
0x128, /* MPHY_TX_APB_PAD_TIMING2_0         */
0x12c, /* MPHY_TX_APB_PAD_TIMING3_0         */
0x130, /* MPHY_TX_APB_PAD_TIMING4_0         */
0x134, /* MPHY_TX_APB_PAD_TIMING5_0         */
0x138, /* MPHY_TX_APB_PAD_TIMING6_0         */
0x13c, /* MPHY_TX_APB_PAD_TIMING7_0         */
0x140, /* MPHY_TX_APB_PAD_TIMING8_0         */
0x144, /* MPHY_TX_APB_PAD_TIMING9_0         */
0x148, /* MPHY_TX_APB_PAD_TIMING10_0        */
0x14c, /* MPHY_TX_APB_TX_PAD_OVR_VAL0_0     */
0x150, /* MPHY_TX_APB_TX_PAD_OVR_CTRL0_0    */
0x154, /* MPHY_TX_APB_TX_OVR_CTRL0_0        */
0x158, /* MPHY_TX_APB_TX_OVR_VAL0_0         */
0x15c, /* MPHY_TX_APB_PAD_TIMER0_0          */
0x160, /* MPHY_TX_APB_TX_CLK_CTRL0_0        */
0x164, /* MPHY_TX_APB_TX_CLK_CTRL1_0        */
0x168, /* MPHY_TX_APB_TX_CLK_CTRL2_0        */
0x16c, /* MPHY_TX_APB_TX_CLK_CTRL3_0        */
0x170, /* MPHY_TX_APB_TX_CG_OVR0_0          */
0x174, /* MPHY_TX_APB_TX_CG_COUNTER0_0      */
0x178, /* MPHY_TX_APB_TX_PAD_OVR_VAL1_0     */
0x17c  /* MPHY_TX_APB_TX_PAD_OVR_CTRL1_0    */
};

struct ufs_tegra_host {
	struct ufs_hba *hba;
	bool is_lane_clks_enabled;
	bool x2config;
	bool enable_mphy_rx_calib;
	bool enable_hs_mode;
	u32 max_hs_gear;
	bool mask_fast_auto_mode;
	bool mask_hs_mode_b;
	u32 max_pwm_gear;
	enum ufs_state ufshc_state;
	void *mphy_context;
	void __iomem *mphy_l0_base;
	void __iomem *mphy_l1_base;
	void __iomem *ufs_aux_base;
	struct reset_control *ufs_rst;
	struct reset_control *ufs_axi_m_rst;
	struct reset_control *ufshc_lp_rst;
	struct reset_control *mphy_l0_rx_rst;
	struct reset_control *mphy_l0_tx_rst;
	struct reset_control *mphy_l1_rx_rst;
	struct reset_control *mphy_l1_tx_rst;
	struct reset_control *mphy_clk_ctl_rst;
	struct clk *mphy_core_pll_fixed;
	struct clk *mphy_l0_tx_symb;
	struct clk *mphy_tx_1mhz_ref;
	struct clk *mphy_l0_rx_ana;
	struct clk *mphy_l0_rx_symb;
	struct clk *mphy_l0_tx_ls_3xbit;
	struct clk *mphy_l0_rx_ls_bit;
	struct clk *mphy_l1_rx_ana;
	struct clk *ufshc_parent;
	struct clk *ufsdev_parent;
	struct clk *ufshc_clk;
	struct clk *ufsdev_ref_clk;
	struct regulator *vddio_ufs;
	struct regulator *vddio_ufs_ap;
	struct padctrl *ufs_padctrl;

};

extern struct ufs_hba_variant_ops ufs_hba_tegra_vops;

static inline u32 mphy_readl(void __iomem *mphy_base, u32 offset)
{
	u32 val;

	val = readl(mphy_base + offset);
	return val;
}

static inline void mphy_writel(void __iomem *mphy_base, u32 val, u32 offset)
{
	writel(val, mphy_base + offset);
}

static inline void mphy_update(void __iomem *mphy_base, u32 val,
								u32 offset)
{
	u32 update_val;

	update_val = mphy_readl(mphy_base, offset);
	update_val |= val;
	mphy_writel(mphy_base, update_val, offset);
}

static inline u32 ufs_aux_readl(void __iomem *ufs_aux_base, u32 offset)
{
	u32 val;

	val = readl(ufs_aux_base + offset);
	return val;
}

static inline void ufs_aux_writel(void __iomem *ufs_aux_base, u32 val,
								u32 offset)
{
	writel(val, ufs_aux_base + offset);
}

static inline void ufs_aux_update(void __iomem *ufs_aux_base, u32 val,
								u32 offset)
{
	u32 update_val;

	update_val = ufs_aux_readl(ufs_aux_base, offset);
	update_val |= val;
	ufs_aux_writel(ufs_aux_base, update_val, offset);
}

static inline void ufs_aux_clear_bits(void __iomem *ufs_aux_base, u32 val,
								u32 offset)
{
	u32 update_val;

	update_val = ufs_aux_readl(ufs_aux_base, offset);
	update_val &= ~val;
	ufs_aux_writel(ufs_aux_base, update_val, offset);
}

static void ufs_save_regs(void __iomem *reg_base, u32 **save_addr,
				u16 reg_array[], u32 no_of_regs)
{
	u32 regs;
	u32 *dest = *save_addr;

	for (regs = 0; regs < no_of_regs; ++regs, ++dest)
		*dest = readl(reg_base + (u32)reg_array[regs]);

	*save_addr = dest;
}

static void ufs_restore_regs(void __iomem *reg_base, u32 **save_addr,
				u16 reg_array[], u32 no_of_regs)
{
	u32 regs;
	u32 *src = *save_addr;

	for (regs = 0; regs < no_of_regs; ++regs, ++src)
		writel(*src, reg_base + (u32)reg_array[regs]);

	*save_addr = src;
}

#endif
