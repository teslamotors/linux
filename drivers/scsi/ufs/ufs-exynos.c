/*
 * UFS Host Controller driver for Exynos specific extensions
 *
 * Copyright (C) 2014-2015 Samsung Electronics Co., Ltd.
 * Author: Seungwon Jeon  <essuuj@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/clk.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/phy/phy-exynos-ufs.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>

#include "ufshcd.h"
#include "ufshcd-pltfrm.h"
#include "ufshci.h"
#include "unipro.h"

#include "ufs-exynos.h"

/*
 * Exynos's Vendor specific registers for UFSHCI
 */
#define HCI_TXPRDT_ENTRY_SIZE	0x00
#define PRDT_PREFECT_EN		BIT(31)
#define PRDT_SET_SIZE(x)	((x) & 0x1F)
#define HCI_RXPRDT_ENTRY_SIZE	0x04
#define HCI_1US_TO_CNT_VAL	0x0C
#define CNT_VAL_1US_MASK	0x3FF
#define HCI_UTRL_NEXUS_TYPE	0x40
#define HCI_UTMRL_NEXUS_TYPE	0x44
#define HCI_SW_RST		0x50
#define UFS_LINK_SW_RST		BIT(0)
#define UFS_UNIPRO_SW_RST	BIT(1)
#define UFS_SW_RST_MASK		(UFS_UNIPRO_SW_RST | UFS_LINK_SW_RST)
#define HCI_DATA_REORDER	0x60
#define HCI_UNIPRO_APB_CLK_CTRL	0x68
#define UNIPRO_APB_CLK(v, x)	(((v) & ~0xF) | ((x) & 0xF))
#define HCI_AXIDMA_RWDATA_BURST_LEN	0x6C
#define BURST_LEN(x)			((x) << 27 | (x))
#define WLU_EN				(1 << 31)
#define HCI_GPIO_OUT		0x70
#define HCI_ERR_EN_PA_LAYER	0x78
#define HCI_ERR_EN_DL_LAYER	0x7C
#define HCI_ERR_EN_N_LAYER	0x80
#define HCI_ERR_EN_T_LAYER	0x84
#define HCI_ERR_EN_DME_LAYER	0x88
#define HCI_UFSHCI_V2P1_CTRL	0X8C
#define IA_TICK_SEL				BIT(16)
#define HCI_CLKSTOP_CTRL	0xB0
#define REFCLKOUT_STOP		BIT(4)
#define REFCLK_STOP		BIT(2)
#define UNIPRO_MCLK_STOP	BIT(1)
#define UNIPRO_PCLK_STOP	BIT(0)
#define CLK_STOP_MASK		(REFCLKOUT_STOP |\
				 REFCLK_STOP |\
				 UNIPRO_MCLK_STOP |\
				 UNIPRO_PCLK_STOP)
#define HCI_MISC		0xB4
#define REFCLK_CTRL_EN		BIT(7)
#define UNIPRO_PCLK_CTRL_EN	BIT(6)
#define UNIPRO_MCLK_CTRL_EN	BIT(5)
#define HCI_CORECLK_CTRL_EN	BIT(4)
#define CLK_CTRL_EN_MASK	(REFCLK_CTRL_EN |\
				 UNIPRO_PCLK_CTRL_EN |\
				 UNIPRO_MCLK_CTRL_EN)
/* Device fatal error */
#define DFES_ERR_EN		BIT(31)
#define DFES_DEF_L2_ERRS	(UIC_DATA_LINK_LAYER_ERROR_RX_BUF_OVERFLOW |\
				 UIC_DATA_LINK_LAYER_ERROR_PA_INIT)
#define DFES_DEF_L3_ERRS	(UIC_NETWORK_LAYER_ERROR_UNSUPPORTED_HEADER_TYPE |\
				 UIC_NETWORK_LAYER_ERROR_BAD_DEVICEID_ENC |\
				 UIC_NETWORK_LAYER_ERROR_LHDR_TRAP_PACKET_DROPPING)
#define DFES_DEF_L4_ERRS	(UIC_TRANSPORT_LAYER_ERROR_UNSUPPORTED_HEADER_TYPE |\
				 UIC_TRANSPORT_LAYER_ERROR_UNKNOWN_CPORTID |\
				 UIC_TRANSPORT_LAYER_ERROR_NO_CONNECTION_RX |\
				 UIC_TRANSPORT_LAYER_ERROR_BAD_TC)

enum {
	UNIPRO_L1_5 = 0,/* PHY Adapter */
	UNIPRO_L2,	/* Data Link */
	UNIPRO_L3,	/* Network */
	UNIPRO_L4,	/* Transport */
	UNIPRO_DME,	/* DME */
};

/*
 * UNIPRO registers
 */
#define UNIPRO_COMP_VERSION			0x000
#define UNIPRO_DME_PWR_REQ			0x090
#define UNIPRO_DME_PWR_REQ_POWERMODE		0x094
#define UNIPRO_DME_PWR_REQ_LOCALL2TIMER0	0x098
#define UNIPRO_DME_PWR_REQ_LOCALL2TIMER1	0x09C
#define UNIPRO_DME_PWR_REQ_LOCALL2TIMER2	0x0A0
#define UNIPRO_DME_PWR_REQ_REMOTEL2TIMER0	0x0A4
#define UNIPRO_DME_PWR_REQ_REMOTEL2TIMER1	0x0A8
#define UNIPRO_DME_PWR_REQ_REMOTEL2TIMER2	0x0AC

/*
 * UFS Protector registers
 */
#define UFSPRSECURITY	0x010
#define NSSMU		BIT(14)
#define UFSPSBEGIN0	0x200
#define UFSPSEND0	0x204
#define UFSPSLUN0	0x208
#define UFSPSCTRL0	0x20C

/* UFS controller type */
#define	UFS_TYPE_EXYNOS7	1
#define	UFS_TYPE_TRAV		2

#define MAX_PRDT_ENTRY_SIZE_BIT         18
#define DEFAULT_PRDT_ENTRY_SIZE_BIT     12

static void exynos_ufs_auto_ctrl_hcc(struct exynos_ufs *ufs, bool en);
static void exynos_ufs_ctrl_clkstop(struct exynos_ufs *ufs, bool en);

static int ufs_host_index = 0;

/* Helper for UFS CAL interface */
static inline int ufs_init_cal(struct exynos_ufs *ufs, int idx,
					struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev = &pdev->dev;
	struct ufs_cal_param *p = NULL;

	p = devm_kzalloc(dev, sizeof(*p), GFP_KERNEL);
	if (!p) {
		dev_err(ufs->dev, "cannot allocate mem for cal param\n");
		return -ENOMEM;
	}
	ufs->cal_param = p;

	p->host = ufs;
	p->board = 0;
	if ((ret = ufs_cal_init(p, idx)) != UFS_CAL_NO_ERROR) {
		dev_err(ufs->dev, "ufs_init_cal = %d!!!\n", ret);
		return -EPERM;
	}

	return 0;
}

static inline int trav_ufs_pre_link(struct exynos_ufs *ufs)
{
	int ret = 0;
	struct ufs_cal_param *p = ufs->cal_param;

	p->mclk_rate = ufs->mclk_rate;
	p->target_lane = ufs->avail_ln_rx;
	p->available_lane = ufs->avail_ln_rx;

	if ((ret = ufs_cal_pre_link(p)) != UFS_CAL_NO_ERROR) {
		dev_err(ufs->dev, "ufs_pre_link = %d!!!\n", ret);
		return -EPERM;
	}

	return 0;
}

static inline int trav_ufs_post_link(struct exynos_ufs *ufs)
{
	int ret = 0;

	if ((ret = ufs_cal_post_link(ufs->cal_param)) != UFS_CAL_NO_ERROR) {
		dev_err(ufs->dev, "ufs_post_link = %d!!!\n", ret);
		return -EPERM;
	}

	return 0;
}

static inline int trav_ufs_pre_pwr_change(struct exynos_ufs *ufs,
				struct uic_pwr_mode *pmd)
{
	struct ufs_cal_param *p = ufs->cal_param;
	int ret = 0;

	p->pmd = pmd;
	p->target_lane = pmd->lane;
	if ((ret = ufs_cal_pre_pmc(p)) != UFS_CAL_NO_ERROR) {
		dev_err(ufs->dev, "ufs_pre_gear_change = %d!!!\n", ret);
		return -EPERM;
	}

	return 0;
}

static inline int trav_ufs_post_pwr_change(struct exynos_ufs *ufs,
						struct uic_pwr_mode *pmd)
{
	int ret = 0;

	if ((ret = ufs_cal_post_pmc(ufs->cal_param)) != UFS_CAL_NO_ERROR) {
		dev_err(ufs->dev, "ufs_post_gear_change = %d!!!\n", ret);
		return -EPERM;
	}

	return 0;
}

static inline int ufs_post_h8_enter(struct exynos_ufs *ufs)
{
	int ret = 0;

	if ((ret = ufs_cal_post_h8_enter(ufs->cal_param)) != UFS_CAL_NO_ERROR) {
		dev_err(ufs->dev, "ufs_post_h8_enter = %d!!!\n", ret);
		return -EPERM;
	}

	return 0;
}

static inline int ufs_pre_h8_exit(struct exynos_ufs *ufs)
{
	int ret = 0;

	if ((ret = ufs_cal_pre_h8_exit(ufs->cal_param)) != UFS_CAL_NO_ERROR) {
		dev_err(ufs->dev, "ufs_pre_h8_exit = %d!!!\n", ret);
		return -EPERM;
	}

	return 0;
}

/* Adaptor for UFS CAL */
void ufs_lld_dme_set(void *h, u32 addr, u32 val)
{
	ufshcd_dme_set(((struct exynos_ufs *)h)->hba, addr, val);
}

void ufs_lld_dme_get(void *h, u32 addr, u32 *val)
{
	ufshcd_dme_get(((struct exynos_ufs *)h)->hba, addr, val);
}

void ufs_lld_dme_peer_set(void *h, u32 addr, u32 val)
{
	ufshcd_dme_peer_set(((struct exynos_ufs *)h)->hba, addr, val);
}

void ufs_lld_pma_write(void *h, u32 val, u32 addr)
{
	phy_pma_writel((struct exynos_ufs *)h, val, addr);
}

u32 ufs_lld_pma_read(void *h, u32 addr)
{
	return phy_pma_readl((struct exynos_ufs *)h, addr);
}

void ufs_lld_unipro_write(void *h, u32 val, u32 addr)
{
	unipro_writel((struct exynos_ufs *)h, val, addr);
}

static inline void exynos_ufs_enable_auto_ctrl_hcc(struct exynos_ufs *ufs)
{
	exynos_ufs_auto_ctrl_hcc(ufs, true);
}

static inline void exynos_ufs_disable_auto_ctrl_hcc(struct exynos_ufs *ufs)
{
	exynos_ufs_auto_ctrl_hcc(ufs, false);
}

static inline void exynos_ufs_disable_auto_ctrl_hcc_save(
					struct exynos_ufs *ufs, u32 *val)
{
	*val = hci_readl(ufs, HCI_MISC);
	exynos_ufs_auto_ctrl_hcc(ufs, false);
}

static inline void exynos_ufs_auto_ctrl_hcc_restore(
					struct exynos_ufs *ufs, u32 *val)
{
	hci_writel(ufs, *val, HCI_MISC);
}

static inline void exynos_ufs_gate_clks(struct exynos_ufs *ufs)
{
	exynos_ufs_ctrl_clkstop(ufs, true);
}

static inline void exynos_ufs_ungate_clks(struct exynos_ufs *ufs)
{
	exynos_ufs_ctrl_clkstop(ufs, false);
}

static int exynos7_ufs_drv_init(struct device *dev, struct exynos_ufs *ufs)
{
	struct clk *child, *parent;

	child = devm_clk_get(dev, "ref_clk");
	if (IS_ERR(child)) {
		dev_err(dev, "failed to get ref_clk clock\n");
		return -EINVAL;
	}

	parent = devm_clk_get(dev, "ref_clk_parent");
	if (IS_ERR(parent)) {
		dev_err(dev, "failed to get ref_clk_parent clock\n");
		return -EINVAL;
	}
	return clk_set_parent(child, parent);
}

static int exynos7_ufs_pre_link(struct exynos_ufs *ufs)
{
	struct ufs_hba *hba = ufs->hba;
	u32 val = ufs->drv_data->uic_attr->pa_dbg_option_suite;
	unsigned int ovtm_offset = ufs->drv_data->ov_tm_offset;
	int i;

	exynos_ufs_enable_ov_tm(hba, ovtm_offset);
	for_each_ufs_tx_lane(ufs, i)
		ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x297, i), 0x17);
	for_each_ufs_rx_lane(ufs, i) {
		ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x362, i), 0xff);
		ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x363, i), 0x00);
	}
	exynos_ufs_disable_ov_tm(hba, ovtm_offset);

	ufshcd_dme_set(hba, UIC_ARG_MIB(PA_DBG_OPTION_SUITE_DYN), 0xf);
	ufshcd_dme_set(hba, UIC_ARG_MIB(PA_DBG_OPTION_SUITE_DYN), 0xf);
	for_each_ufs_tx_lane(ufs, i)
		ufshcd_dme_set(hba,
			UIC_ARG_MIB_SEL(TX_HIBERN8_CONTROL, i), 0x0);
	ufshcd_dme_set(hba, UIC_ARG_MIB(PA_DBG_TXPHY_CFGUPDT), 0x1);
	udelay(1);
	ufshcd_dme_set(hba, UIC_ARG_MIB(PA_DBG_OPTION_SUITE), val | (1 << 12));
	ufshcd_dme_set(hba, UIC_ARG_MIB(PA_DBG_SKIP_RESET_PHY), 0x1);
	ufshcd_dme_set(hba, UIC_ARG_MIB(PA_DBG_SKIP_LINE_RESET), 0x1);
	ufshcd_dme_set(hba, UIC_ARG_MIB(PA_DBG_LINE_RESET_REQ), 0x1);
	udelay(1600);
	ufshcd_dme_set(hba, UIC_ARG_MIB(PA_DBG_OPTION_SUITE), val);

	return 0;
}

static int exynos7_ufs_post_link(struct exynos_ufs *ufs)
{
	struct ufs_hba *hba = ufs->hba;
	unsigned int ovtm_offset = ufs->drv_data->ov_tm_offset;
	int i;

	exynos_ufs_enable_ov_tm(hba, ovtm_offset);
	for_each_ufs_tx_lane(ufs, i) {
		ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x28b, i), 0x83);
		ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x29a, i), 0x07);
		ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(0x277, i),
			TX_LINERESET_N(exynos_ufs_calc_time_cntr(ufs, 200000)));
	}
	exynos_ufs_disable_ov_tm(hba, ovtm_offset);

	exynos_ufs_enable_dbg_mode(hba);
	ufshcd_dme_set(hba, UIC_ARG_MIB(PA_SAVECONFIGTIME), 0xbb8);
	exynos_ufs_disable_dbg_mode(hba);

	return 0;
}

static int exynos7_ufs_pre_pwr_change(struct exynos_ufs *ufs,
						struct uic_pwr_mode *pwr)
{
	unipro_writel(ufs, 0x22, UNIPRO_DBG_FORCE_DME_CTRL_STATE);

	return 0;
}

static int exynos7_ufs_post_pwr_change(struct exynos_ufs *ufs,
						struct uic_pwr_mode *pwr)
{
	struct ufs_hba *hba = ufs->hba;

	ufshcd_dme_set(hba, UIC_ARG_MIB(PA_DBG_RXPHY_CFGUPDT), 0x1);

	if (pwr->lane == 1) {
		exynos_ufs_enable_dbg_mode(hba);
		ufshcd_dme_set(hba, UIC_ARG_MIB(PA_CONNECTEDTXDATALANES), 0x1);
		exynos_ufs_disable_dbg_mode(hba);
	}

	return 0;
}

/**
 * exynos_ufs_auto_ctrl_hcc - HCI core clock control by h/w
 * Control should be disabled in the below cases
 * - Before host controller S/W reset
 * - Access to UFS protector's register
 */
static void exynos_ufs_auto_ctrl_hcc(struct exynos_ufs *ufs, bool en)
{
	u32 misc = hci_readl(ufs, HCI_MISC);

	if (en)
		hci_writel(ufs, misc | HCI_CORECLK_CTRL_EN, HCI_MISC);
	else
		hci_writel(ufs, misc & ~HCI_CORECLK_CTRL_EN, HCI_MISC);
}

static void exynos_ufs_ctrl_clkstop(struct exynos_ufs *ufs, bool en)
{
	u32 ctrl = hci_readl(ufs, HCI_CLKSTOP_CTRL);
	u32 misc = hci_readl(ufs, HCI_MISC);

	if (en) {
		hci_writel(ufs, misc | CLK_CTRL_EN_MASK, HCI_MISC);
		hci_writel(ufs, ctrl | CLK_STOP_MASK, HCI_CLKSTOP_CTRL);
	} else {
		hci_writel(ufs, ctrl & ~CLK_STOP_MASK, HCI_CLKSTOP_CTRL);
		hci_writel(ufs, misc & ~CLK_CTRL_EN_MASK, HCI_MISC);
	}
}

static int exynos_ufs_get_clk_info(struct exynos_ufs *ufs)
{
	struct ufs_hba *hba = ufs->hba;
	struct list_head *head = &hba->clk_list_head;
	struct ufs_clk_info *clki;
	u32 pclk_rate;
	u32 f_min, f_max;
	u8 div = 0;
	int ret = 0;

	if (!head || list_empty(head))
		goto out;

	list_for_each_entry(clki, head, list) {
		if (!IS_ERR(clki->clk)) {
			if (!strcmp(clki->name, "core_clk"))
				ufs->clk_hci_core = clki->clk;
			else if (!strcmp(clki->name, "sclk_unipro_main"))
				ufs->clk_unipro_main = clki->clk;
		}
	}

	if (!ufs->clk_hci_core || !ufs->clk_unipro_main) {
		dev_err(hba->dev, "failed to get clk info\n");
		ret = -EINVAL;
		goto out;
	}

	ufs->mclk_rate = clk_get_rate(ufs->clk_unipro_main);
	pclk_rate = clk_get_rate(ufs->clk_hci_core);
	f_min = ufs->pclk_avail_min;
	f_max = ufs->pclk_avail_max;

	if (ufs->opts & EXYNOS_UFS_OPT_HAS_APB_CLK_CTRL) {
		do {
			pclk_rate /= (div + 1);

			if (pclk_rate <= f_max)
				break;
			div++;
		} while (pclk_rate >= f_min);
	}

	if (unlikely(pclk_rate < f_min || pclk_rate > f_max)) {
		dev_err(hba->dev, "not available pclk range %d\n", pclk_rate);
		ret = -EINVAL;
		goto out;
	}

	ufs->pclk_rate = pclk_rate;
	ufs->pclk_div = div;

out:
	return ret;
}

static void exynos_ufs_set_unipro_pclk_div(struct exynos_ufs *ufs)
{
	if (ufs->opts & EXYNOS_UFS_OPT_HAS_APB_CLK_CTRL) {
		u32 val;

		val = hci_readl(ufs, HCI_UNIPRO_APB_CLK_CTRL);
		hci_writel(ufs, UNIPRO_APB_CLK(val, ufs->pclk_div),
			   HCI_UNIPRO_APB_CLK_CTRL);
	}
}

static void exynos_ufs_calc_pwm_clk_div(struct exynos_ufs *ufs)
{
	struct ufs_hba *hba = ufs->hba;
	struct exynos_ufs_uic_attr *attr = ufs->drv_data->uic_attr;
	const unsigned int div = 30, mult = 20;
	const unsigned long pwm_min = 3 * 1000 * 1000;
	const unsigned long pwm_max = 9 * 1000 * 1000;
	const int divs[] = {32, 16, 8, 4};
	unsigned long clk = 0, _clk, clk_period;
	int i = 0, clk_idx = -1;

	clk_period = UNIPRO_PCLK_PERIOD(ufs);
	for (i = 0; i < ARRAY_SIZE(divs); i++) {
		_clk = NSEC_PER_SEC * mult / (clk_period * divs[i] * div);
		if (_clk >= pwm_min && _clk <= pwm_max) {
			if (_clk > clk) {
				clk_idx = i;
				clk = _clk;
			}
		}
	}

	if (clk_idx == -1) {
		ufshcd_dme_get(hba, UIC_ARG_MIB(CMN_PWM_CLK_CTRL), &clk_idx);
		dev_err(hba->dev,
			"failed to decide pwm clock divider, will not change\n");
	}

	attr->cmn_pwm_clk_ctrl = clk_idx & PWM_CLK_CTRL_MASK;
}

long exynos_ufs_calc_time_cntr(struct exynos_ufs *ufs, long period)
{
	const int precise = 10;
	long pclk_rate = ufs->pclk_rate;
	long clk_period, fraction;

	clk_period = UNIPRO_PCLK_PERIOD(ufs);
	fraction = ((NSEC_PER_SEC % pclk_rate) * precise) / pclk_rate;

	return (period * precise) / ((clk_period * precise) + fraction);
}

#if IS_ENABLED(CONFIG_PHY_EXYNOS_UFS)
static void exynos_ufs_config_sync_pattern_mask(struct exynos_ufs *ufs,
					struct uic_pwr_mode *pwr)
{
	struct ufs_hba *hba = ufs->hba;
	unsigned int ovtm_offset = ufs->drv_data->ov_tm_offset;
	u8 g = pwr->gear;
	u32 mask, sync_len;
	enum {
		SYNC_LEN_G1 = 80 * 1000, /* 80us */
		SYNC_LEN_G2 = 40 * 1000, /* 44us */
		SYNC_LEN_G3 = 20 * 1000, /* 20us */
	};
	int i;

	if (g == 1)
		sync_len = SYNC_LEN_G1;
	else if (g == 2)
		sync_len = SYNC_LEN_G2;
	else if (g == 3)
		sync_len = SYNC_LEN_G3;
	else
		return;

	mask = exynos_ufs_calc_time_cntr(ufs, sync_len);
	mask = (mask >> 8) & 0xff;

	exynos_ufs_enable_ov_tm(hba, ovtm_offset);

	for_each_ufs_rx_lane(ufs, i)
		ufshcd_dme_set(hba,
			UIC_ARG_MIB_SEL(RX_SYNC_MASK_LENGTH, i), mask);

	exynos_ufs_disable_ov_tm(hba, ovtm_offset);
}
#endif

static int exynos_ufs_pre_pwr_mode(struct ufs_hba *hba,
				struct ufs_pa_layer_attr *pwr_max,
				struct ufs_pa_layer_attr *final)
{
	struct exynos_ufs *ufs = ufshcd_get_variant(hba);
#if IS_ENABLED(CONFIG_PHY_EXYNOS_UFS)
	struct phy *generic_phy = ufs->phy;
#endif
	struct uic_pwr_mode *pwr_req = &ufs->pwr_req;
	struct uic_pwr_mode *pwr_act = &ufs->pwr_act;

	final->gear_rx
		= pwr_act->gear = min_t(u32, pwr_max->gear_rx, pwr_req->gear);
	final->gear_tx
		= pwr_act->gear = min_t(u32, pwr_max->gear_tx, pwr_req->gear);
	final->lane_rx
		= pwr_act->lane = min_t(u32, pwr_max->lane_rx, pwr_req->lane);
	final->lane_tx
		= pwr_act->lane = min_t(u32, pwr_max->lane_tx, pwr_req->lane);
	final->pwr_rx = pwr_act->mode = pwr_req->mode;
	final->pwr_tx = pwr_act->mode = pwr_req->mode;
	final->hs_rate = pwr_act->hs_series = pwr_req->hs_series;
#if 0
	/* save and configure l2 timer */
	pwr_act->l_l2_timer[0] = pwr_req->l_l2_timer[0];
	pwr_act->l_l2_timer[1] = pwr_req->l_l2_timer[1];
	pwr_act->l_l2_timer[2] = pwr_req->l_l2_timer[2];
	pwr_act->r_l2_timer[0] = pwr_req->r_l2_timer[0];
	pwr_act->r_l2_timer[1] = pwr_req->r_l2_timer[1];
	pwr_act->r_l2_timer[2] = pwr_req->r_l2_timer[2];

	ufshcd_dme_set(hba,
		UIC_ARG_MIB(DL_FC0PROTTIMEOUTVAL), pwr_act->l_l2_timer[0]);
	ufshcd_dme_set(hba,
		UIC_ARG_MIB(DL_TC0REPLAYTIMEOUTVAL), pwr_act->l_l2_timer[1]);
	ufshcd_dme_set(hba,
		UIC_ARG_MIB(DL_AFC0REQTIMEOUTVAL), pwr_act->l_l2_timer[2]);
	ufshcd_dme_set(hba,
		UIC_ARG_MIB(PA_PWRMODEUSERDATA0), pwr_act->r_l2_timer[0]);
	ufshcd_dme_set(hba,
		UIC_ARG_MIB(PA_PWRMODEUSERDATA1), pwr_act->r_l2_timer[1]);
	ufshcd_dme_set(hba,
		UIC_ARG_MIB(PA_PWRMODEUSERDATA2), pwr_act->r_l2_timer[2]);

	unipro_writel(ufs,
		pwr_act->l_l2_timer[0], UNIPRO_DME_PWR_REQ_LOCALL2TIMER0);
	unipro_writel(ufs,
		pwr_act->l_l2_timer[1], UNIPRO_DME_PWR_REQ_LOCALL2TIMER1);
	unipro_writel(ufs,
		pwr_act->l_l2_timer[2], UNIPRO_DME_PWR_REQ_LOCALL2TIMER2);
	unipro_writel(ufs,
		pwr_act->r_l2_timer[0], UNIPRO_DME_PWR_REQ_REMOTEL2TIMER0);
	unipro_writel(ufs,
		pwr_act->r_l2_timer[1], UNIPRO_DME_PWR_REQ_REMOTEL2TIMER1);
	unipro_writel(ufs,
		pwr_act->r_l2_timer[2], UNIPRO_DME_PWR_REQ_REMOTEL2TIMER2);
#endif
	if (ufs->drv_data->pre_pwr_change)
		ufs->drv_data->pre_pwr_change(ufs, pwr_act);

#if IS_ENABLED(CONFIG_PHY_EXYNOS_UFS)
	if (ufs->drv_data->ctrl_type == UFS_TYPE_EXYNOS7) {
		if (IS_UFS_PWR_MODE_HS(pwr_act->mode)) {
			exynos_ufs_config_sync_pattern_mask(ufs, pwr_act);
			switch (pwr_act->hs_series) {
			case PA_HS_MODE_A:
			case PA_HS_MODE_B:
				exynos_ufs_phy_calibrate(generic_phy,
					CFG_PRE_PWR_HS,
					PWR_MODE_HS(pwr_act->gear,
							pwr_act->hs_series));
				break;
			}
		}
	}
#endif
	return 0;
}

#define PWR_MODE_STR_LEN	64
static int exynos_ufs_post_pwr_mode(struct ufs_hba *hba,
				struct ufs_pa_layer_attr *pwr_max,
				struct ufs_pa_layer_attr *pwr_req)
{
	struct exynos_ufs *ufs = ufshcd_get_variant(hba);
#if IS_ENABLED(CONFIG_PHY_EXYNOS_UFS)
	struct phy *generic_phy = ufs->phy;
#endif
	struct uic_pwr_mode *pwr = &ufs->pwr_act;
	char pwr_str[PWR_MODE_STR_LEN] = "";
	int ret = 0;

	if (ufs->drv_data->post_pwr_change)
		ufs->drv_data->post_pwr_change(ufs, pwr);

	if (IS_UFS_PWR_MODE_HS(pwr->mode)) {
#if IS_ENABLED(CONFIG_PHY_EXYNOS_UFS)
		if (ufs->drv_data->ctrl_type == UFS_TYPE_EXYNOS7) {
			switch (pwr->hs_series) {
			case PA_HS_MODE_A:
			case PA_HS_MODE_B:
				exynos_ufs_phy_calibrate(generic_phy, CFG_POST_PWR_HS,
					PWR_MODE_HS(pwr->gear, pwr->hs_series));
				break;
			}

			ret = exynos_ufs_phy_wait_for_lock_acq(generic_phy);
		}
#endif
		snprintf(pwr_str, sizeof(pwr_str), "Fast%s series_%s G_%d L_%d",
			pwr->mode == FASTAUTO_MODE ? "_Auto" : "",
			pwr->hs_series == PA_HS_MODE_A ? "A" : "B",
			pwr->gear, pwr->lane);
	} else if (IS_UFS_PWR_MODE_PWM(pwr->mode)) {
		snprintf(pwr_str, sizeof(pwr_str), "Slow%s G_%d L_%d",
			pwr->mode == SLOWAUTO_MODE ? "_Auto" : "",
			pwr->gear, pwr->lane);
	}

	dev_info(hba->dev, "Power mode change %d : %s\n", ret, pwr_str);
	return ret;
}

static void exynos_ufs_specify_nexus_t_xfer_req(struct ufs_hba *hba,
						int tag, struct scsi_cmnd *cmd)
{
	struct exynos_ufs *ufs = ufshcd_get_variant(hba);
	u32 type;

	type =  hci_readl(ufs, HCI_UTRL_NEXUS_TYPE);

	if (cmd)
		hci_writel(ufs, type | (1 << tag), HCI_UTRL_NEXUS_TYPE);
	else
		hci_writel(ufs, type & ~(1 << tag), HCI_UTRL_NEXUS_TYPE);
}

static void exynos_ufs_specify_nexus_t_tm_req(struct ufs_hba *hba,
						int tag, u8 func)
{
	struct exynos_ufs *ufs = ufshcd_get_variant(hba);
	u32 type;

	type =  hci_readl(ufs, HCI_UTMRL_NEXUS_TYPE);

	switch (func) {
	case UFS_ABORT_TASK:
	case UFS_QUERY_TASK:
		hci_writel(ufs, type | (1 << tag), HCI_UTMRL_NEXUS_TYPE);
		break;
	case UFS_ABORT_TASK_SET:
	case UFS_CLEAR_TASK_SET:
	case UFS_LOGICAL_RESET:
	case UFS_QUERY_TASK_SET:
		hci_writel(ufs, type & ~(1 << tag), HCI_UTMRL_NEXUS_TYPE);
		break;
	}
}

static void exynos_ufs_phy_init(struct exynos_ufs *ufs)
{
	struct ufs_hba *hba = ufs->hba;
#if IS_ENABLED(CONFIG_PHY_EXYNOS_UFS)
	struct phy *generic_phy = ufs->phy;
#endif
	if (ufs->avail_ln_rx == 0 || ufs->avail_ln_tx == 0) {
		ufshcd_dme_get(hba, UIC_ARG_MIB(PA_AVAILRXDATALANES),
			&ufs->avail_ln_rx);
		ufshcd_dme_get(hba, UIC_ARG_MIB(PA_AVAILTXDATALANES),
			&ufs->avail_ln_tx);
		WARN(ufs->avail_ln_rx != ufs->avail_ln_tx,
			"available data lane is not equal(rx:%d, tx:%d)\n",
			ufs->avail_ln_rx, ufs->avail_ln_tx);
	}

#if IS_ENABLED(CONFIG_PHY_EXYNOS_UFS)
	if (ufs->drv_data->ctrl_type == UFS_TYPE_EXYNOS7) {
		exynos_ufs_phy_set_lane_cnt(generic_phy, ufs->avail_ln_rx);
		exynos_ufs_phy_calibrate(generic_phy, CFG_PRE_INIT, PWR_MODE_ANY);
	}
#endif
}

static void exynos_ufs_config_intr(struct exynos_ufs *ufs, u32 errs, u8 index)
{
	switch (index) {
	case UNIPRO_L1_5:
		hci_writel(ufs, DFES_ERR_EN | errs, HCI_ERR_EN_PA_LAYER);
		break;
	case UNIPRO_L2:
		hci_writel(ufs, DFES_ERR_EN | errs, HCI_ERR_EN_DL_LAYER);
		break;
	case UNIPRO_L3:
		hci_writel(ufs, DFES_ERR_EN | errs, HCI_ERR_EN_N_LAYER);
		break;
	case UNIPRO_L4:
		hci_writel(ufs, DFES_ERR_EN | errs, HCI_ERR_EN_T_LAYER);
		break;
	case UNIPRO_DME:
		hci_writel(ufs, DFES_ERR_EN | errs, HCI_ERR_EN_DME_LAYER);
		break;
	}
}

static int exynos_ufs_pre_link(struct ufs_hba *hba)
{
	struct exynos_ufs *ufs = ufshcd_get_variant(hba);

	/* hci */
	exynos_ufs_config_intr(ufs, DFES_DEF_L2_ERRS, UNIPRO_L2);
	exynos_ufs_config_intr(ufs, DFES_DEF_L3_ERRS, UNIPRO_L3);
	exynos_ufs_config_intr(ufs, DFES_DEF_L4_ERRS, UNIPRO_L4);
	exynos_ufs_set_unipro_pclk_div(ufs);

	exynos_ufs_enable_auto_ctrl_hcc(ufs);
	exynos_ufs_ungate_clks(ufs);

	/* m-phy */
	exynos_ufs_phy_init(ufs);

	if (ufs->drv_data->pre_link)
		ufs->drv_data->pre_link(ufs);

	return 0;
}

static void exynos_ufs_fit_aggr_timeout(struct exynos_ufs *ufs)
{
	u32 cnt_val;
	u32 nVal;

	/* IA_TICK_SEL : 1(1us_TO_CNT_VAL) */
	nVal = hci_readl(ufs, HCI_UFSHCI_V2P1_CTRL);
	nVal |= IA_TICK_SEL;
	hci_writel(ufs, nVal, HCI_UFSHCI_V2P1_CTRL);

	cnt_val = ufs->mclk_rate / 1000000 ;
	hci_writel(ufs, cnt_val & CNT_VAL_1US_MASK, HCI_1US_TO_CNT_VAL);
}

static int exynos_ufs_post_link(struct ufs_hba *hba)
{
	struct exynos_ufs *ufs = ufshcd_get_variant(hba);
#if IS_ENABLED(CONFIG_PHY_EXYNOS_UFS)
	struct phy *generic_phy = ufs->phy;
#endif
	u32 reg;

	exynos_ufs_fit_aggr_timeout(ufs);

	if (hba->ufs_prdt_entry_size_bit > MAX_PRDT_ENTRY_SIZE_BIT)
		hba->ufs_prdt_entry_size_bit = MAX_PRDT_ENTRY_SIZE_BIT;

	hci_writel(ufs, 0xa, HCI_DATA_REORDER);
	hci_writel(ufs, PRDT_PREFECT_EN | PRDT_SET_SIZE(hba->ufs_prdt_entry_size_bit),
			HCI_TXPRDT_ENTRY_SIZE);
	hci_writel(ufs, PRDT_SET_SIZE(hba->ufs_prdt_entry_size_bit), HCI_RXPRDT_ENTRY_SIZE);
	hci_writel(ufs, 0xFFFFFFFF, HCI_UTRL_NEXUS_TYPE);
	hci_writel(ufs, 0xFFFFFFFF, HCI_UTMRL_NEXUS_TYPE);

	reg = hci_readl(ufs, HCI_AXIDMA_RWDATA_BURST_LEN) &
					~BURST_LEN(0);
	hci_writel(ufs, WLU_EN | BURST_LEN(3),
					HCI_AXIDMA_RWDATA_BURST_LEN);

#if IS_ENABLED(CONFIG_PHY_EXYNOS_UFS)
	if (ufs->drv_data->ctrl_type == UFS_TYPE_EXYNOS7)
		exynos_ufs_phy_calibrate(generic_phy, CFG_POST_INIT, PWR_MODE_ANY);
#endif
	if (ufs->drv_data->post_link)
		ufs->drv_data->post_link(ufs);

	return 0;
}

static void exynos_ufs_specify_pwr_mode(struct device_node *np,
				struct exynos_ufs *ufs)
{
	struct uic_pwr_mode *pwr = &ufs->pwr_req;
	const char *str = NULL;

	if (!of_property_read_string(np, "ufs,pwr-attr-mode", &str)) {
		if (!strncmp(str, "FAST", sizeof("FAST")))
			pwr->mode = FAST_MODE;
		else if (!strncmp(str, "SLOW", sizeof("SLOW")))
			pwr->mode = SLOW_MODE;
		else if (!strncmp(str, "FAST_auto", sizeof("FAST_auto")))
			pwr->mode = FASTAUTO_MODE;
		else if (!strncmp(str, "SLOW_auto", sizeof("SLOW_auto")))
			pwr->mode = SLOWAUTO_MODE;
		else
			pwr->mode = FAST_MODE;
	} else {
		pwr->mode = FAST_MODE;
	}

	if (of_property_read_u32(np, "ufs,pwr-attr-lane", &pwr->lane))
		pwr->lane = 1;

	if (of_property_read_u32(np, "ufs,pwr-attr-gear", &pwr->gear))
		pwr->gear = 1;

	if (IS_UFS_PWR_MODE_HS(pwr->mode)) {
		if (!of_property_read_string(np,
					"ufs,pwr-attr-hs-series", &str)) {
			if (!strncmp(str, "HS_rate_b", sizeof("HS_rate_b")))
				pwr->hs_series = PA_HS_MODE_B;
			else if (!strncmp(str, "HS_rate_a",
					sizeof("HS_rate_a")))
				pwr->hs_series = PA_HS_MODE_A;
			else
				pwr->hs_series = PA_HS_MODE_A;
		} else {
			pwr->hs_series = PA_HS_MODE_A;
		}
	}

	if (of_property_read_u32_array(
		np, "ufs,pwr-local-l2-timer", pwr->l_l2_timer, 3)) {
		pwr->l_l2_timer[0] = FC0PROTTIMEOUTVAL;
		pwr->l_l2_timer[1] = TC0REPLAYTIMEOUTVAL;
		pwr->l_l2_timer[2] = AFC0REQTIMEOUTVAL;
	}

	if (of_property_read_u32_array(
		np, "ufs,pwr-remote-l2-timer", pwr->r_l2_timer, 3)) {
		pwr->r_l2_timer[0] = FC0PROTTIMEOUTVAL;
		pwr->r_l2_timer[1] = TC0REPLAYTIMEOUTVAL;
		pwr->r_l2_timer[2] = AFC0REQTIMEOUTVAL;
	}
}

static int exynos_ufs_parse_dt(struct device *dev, struct exynos_ufs *ufs)
{
	struct ufs_hba *hba = ufs->hba;
	struct device_node *np = dev->of_node;
	struct exynos_ufs_uic_attr *attr;
	u32 freq[2];
	int ret;

	if (ufs->drv_data && ufs->drv_data->uic_attr) {
		attr = ufs->drv_data->uic_attr;
	} else {
		dev_err(dev, "failed to get uic attributes\n");
		ret = -EINVAL;
		goto out;
	}

	ret = of_property_read_u32_array(np,
			"pclk-freq-avail-range", freq, ARRAY_SIZE(freq));
	if (!ret) {
		ufs->pclk_avail_min = freq[0];
		ufs->pclk_avail_max = freq[1];
	} else {
		dev_err(dev, "failed to get available pclk range\n");
		goto out;
	}

	if(of_property_read_u32(np, "ufs,ufs_addr_bus_width",
				       &ufs->ufs_addr_bus_wd)) {
		if (ufshcd_readl(hba, REG_CONTROLLER_CAPABILITIES)
				 & MASK_64_ADDRESSING_SUPPORT)
			ufs->ufs_addr_bus_wd = 64;
		else
			ufs->ufs_addr_bus_wd = 32;
	}

	if (of_property_read_u32(np, "ufs-prdt-size-bit",
					&hba->ufs_prdt_entry_size_bit)) {
		hba->ufs_prdt_entry_size_bit = DEFAULT_PRDT_ENTRY_SIZE_BIT;
	}

	exynos_ufs_specify_pwr_mode(np, ufs);

	if (!of_property_read_u32(np, "ufs-rx-adv-fine-gran-sup_en",
				&attr->rx_adv_fine_gran_sup_en)) {
		if (attr->rx_adv_fine_gran_sup_en == 0) {
			/* 100us step */
			if (of_property_read_u32(np,
					"ufs-rx-min-activate-time-cap",
					&attr->rx_min_actv_time_cap))
				dev_warn(dev,
					"ufs-rx-min-activate-time-cap is empty\n");

			if (of_property_read_u32(np,
					"ufs-rx-hibern8-time-cap",
					&attr->rx_hibern8_time_cap))
				dev_warn(dev,
					"ufs-rx-hibern8-time-cap is empty\n");
		} else if (attr->rx_adv_fine_gran_sup_en == 1) {
			/* fine granularity step */
			if (of_property_read_u32(np,
					"ufs-rx-adv-fine-gran-step",
					&attr->rx_adv_fine_gran_step))
				dev_warn(dev,
					"ufs-rx-adv-fine-gran-step is empty\n");

			if (of_property_read_u32(np,
					"ufs-rx-adv-min-activate-time-cap",
					&attr->rx_adv_min_actv_time_cap))
				dev_warn(dev,
					"ufs-rx-adv-min-activate-time-cap is empty\n");

			if (of_property_read_u32(np,
					"ufs-rx-adv-hibern8-time-cap",
					&attr->rx_adv_hibern8_time_cap))
				dev_warn(dev,
					"ufs-rx-adv-hibern8-time-cap is empty\n");
		} else {
			dev_warn(dev,
				"not supported val for ufs-rx-adv-fine-gran-sup_en %d\n",
				attr->rx_adv_fine_gran_sup_en);
		}
	} else {
		attr->rx_adv_fine_gran_sup_en = 0xf;
	}

	if (!of_property_read_u32(np,
				"ufs-pa-granularity", &attr->pa_granularity)) {
		if (of_property_read_u32(np,
				"ufs-pa-tacctivate", &attr->pa_tactivate))
			dev_warn(dev, "ufs-pa-tacctivate is empty\n");

		if (of_property_read_u32(np,
				"ufs-pa-hibern8time", &attr->pa_hibern8time))
			dev_warn(dev, "ufs-pa-hibern8time is empty\n");
	}

out:
	return ret;
}

static int exynos_ufs_init(struct ufs_hba *hba)
{
	struct device *dev = hba->dev;
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_ufs *ufs;
	struct exynos_ufs_phy *phy;
	struct resource *res;
	int ret;

	ufs = devm_kzalloc(dev, sizeof(*ufs), GFP_KERNEL);
	if (!ufs)
		return -ENOMEM;

	ufs->drv_data = (struct exynos_ufs_drv_data *)
		of_device_get_match_data(dev);

	ufs->hba = hba;

	phy = &ufs->ufs_phy;
	/* exynos-specific hci */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "vs_hci");
	ufs->reg_hci = devm_ioremap_resource(dev, res);
	if (!ufs->reg_hci) {
		dev_err(dev, "cannot ioremap for hci vendor register\n");
		return -ENOMEM;
	}

	/* unipro */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "unipro");
	ufs->reg_unipro = devm_ioremap_resource(dev, res);
	if (!ufs->reg_unipro) {
		dev_err(dev, "cannot ioremap for unipro register\n");
		return -ENOMEM;
	}

	/* ufs protector */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ufsp");
	ufs->reg_ufsp = devm_ioremap_resource(dev, res);
	if (!ufs->reg_ufsp) {
		dev_err(dev, "cannot ioremap for ufs protector register\n");
		return -ENOMEM;
	}

	/* ufs phy pma */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ufs_phy");
	phy->reg_pma = devm_ioremap_resource(dev, res);
	if (!phy->reg_pma) {
		dev_err(dev, "cannot ioremap for ufs phy pma register\n");
		return -ENOMEM;
	}

	/* This must be before calling exynos_ufs_populate_dt */
	ret = ufs_init_cal(ufs, ufs_host_index, pdev);
	if (ret)
		return ret;
	ret = exynos_ufs_parse_dt(dev, ufs);
	if (ret) {
		dev_err(dev, "failed to get dt info.\n");
		goto out;
	}

	ufs->opts = ufs->drv_data->opts |
		EXYNOS_UFS_OPT_SKIP_CONNECTION_ESTAB |
		EXYNOS_UFS_OPT_USE_SW_HIBERN8_TIMER;
	ufs->rx_sel_idx = PA_MAXDATALANES;
	if (ufs->opts & EXYNOS_UFS_OPT_BROKEN_RX_SEL_IDX)
		ufs->rx_sel_idx = 0;
	hba->priv = (void *)ufs;
	hba->quirks = ufs->drv_data->quirks;
	if (ufs->drv_data->drv_init) {
		ret = ufs->drv_data->drv_init(dev, ufs);
		if (ret) {
			dev_err(dev, "failed to init drv-data\n");
			goto out;
		}
	}

	ret = exynos_ufs_get_clk_info(ufs);
	if (ret)
		goto out;
	return 0;

out:
	return ret;
}

static int exynos_ufs_host_reset(struct ufs_hba *hba)
{
	struct exynos_ufs *ufs = ufshcd_get_variant(hba);
	unsigned long timeout = jiffies + msecs_to_jiffies(1);
	u32 val;
	int ret = 0;

	exynos_ufs_disable_auto_ctrl_hcc_save(ufs, &val);

	hci_writel(ufs, UFS_SW_RST_MASK, HCI_SW_RST);

	do {
		if (!(hci_readl(ufs, HCI_SW_RST) & UFS_SW_RST_MASK))
			goto out;
	} while (time_before(jiffies, timeout));

	dev_err(hba->dev, "timeout host sw-reset\n");
	ret = -ETIMEDOUT;

out:
	exynos_ufs_auto_ctrl_hcc_restore(ufs, &val);
	return ret;
}

static void exynos_ufs_dev_hw_reset(struct ufs_hba *hba)
{
	struct exynos_ufs *ufs = ufshcd_get_variant(hba);

	hci_writel(ufs, 0 << 0, HCI_GPIO_OUT);
	udelay(5);
	hci_writel(ufs, 1 << 0, HCI_GPIO_OUT);
}

static void exynos_ufs_pre_hibern8(struct ufs_hba *hba, u8 enter)
{
#if 0
	struct exynos_ufs *ufs = ufshcd_get_variant(hba);
	struct exynos_ufs_uic_attr *attr = ufs->drv_data->uic_attr;

	if (!enter) {
		if (ufs->opts & EXYNOS_UFS_OPT_BROKEN_AUTO_CLK_CTRL)
			exynos_ufs_disable_auto_ctrl_hcc(ufs);
		exynos_ufs_ungate_clks(ufs);

		if (ufs->opts & EXYNOS_UFS_OPT_USE_SW_HIBERN8_TIMER) {
			const unsigned int granularity_tbl[] = {
				1, 4, 8, 16, 32, 100
			};
			int h8_time = attr->pa_hibern8time *
				granularity_tbl[attr->pa_granularity - 1];
			unsigned long us;
			s64 delta;

			do {
				delta = h8_time - ktime_us_delta(ktime_get(),
							ufs->entry_hibern8_t);
				if (delta <= 0)
					break;

				us = min_t(s64, delta, USEC_PER_MSEC);
				if (us >= 10)
					usleep_range(us, us + 10);
			} while (1);
		}
	}
#endif
}

static void exynos_ufs_post_hibern8(struct ufs_hba *hba, u8 enter)
{
	struct exynos_ufs *ufs = ufshcd_get_variant(hba);

	if (!enter) {
		struct uic_pwr_mode *pwr = &ufs->pwr_act;
		u32 mode = 0;

		ufshcd_dme_get(hba, UIC_ARG_MIB(PA_PWRMODE), &mode);
		if (mode != (pwr->mode << 4 | pwr->mode)) {
			dev_warn(hba->dev, "%s: power mode change\n", __func__);
			hba->pwr_info.pwr_rx = (mode >> 4) & 0xf;
			hba->pwr_info.pwr_tx = mode & 0xf;
			ufshcd_config_pwr_mode(hba, &hba->max_pwr_info.info);
		}
	}
}

static int exynos_ufs_hce_enable_notify(struct ufs_hba *hba,
					enum ufs_notify_change_status status)
{
	struct exynos_ufs *ufs = ufshcd_get_variant(hba);
	int ret = 0;

	switch (status) {
	case PRE_CHANGE:
		ret = exynos_ufs_host_reset(hba);
		if (ret)
			return ret;
		exynos_ufs_dev_hw_reset(hba);
		break;
	case POST_CHANGE:
		exynos_ufs_calc_pwm_clk_div(ufs);
		if (!(ufs->opts & EXYNOS_UFS_OPT_BROKEN_AUTO_CLK_CTRL))
			exynos_ufs_enable_auto_ctrl_hcc(ufs);
		break;
	}

	return ret;
}

static int exynos_ufs_link_startup_notify(struct ufs_hba *hba,
					  enum ufs_notify_change_status status)
{
	int ret = 0;

	switch (status) {
	case PRE_CHANGE:
		ret = exynos_ufs_pre_link(hba);
		break;
	case POST_CHANGE:
		ret = exynos_ufs_post_link(hba);
		break;
	}

	return ret;
}

static int exynos_ufs_pwr_change_notify(struct ufs_hba *hba,
					enum ufs_notify_change_status status,
					struct ufs_pa_layer_attr *pwr_max,
					struct ufs_pa_layer_attr *pwr_req)
{
	int ret = 0;

	switch (status) {
	case PRE_CHANGE:
		ret = exynos_ufs_pre_pwr_mode(hba, pwr_max, pwr_req);
		break;
	case POST_CHANGE:
		ret = exynos_ufs_post_pwr_mode(hba, NULL, pwr_req);
		break;
	}

	return ret;
}

static void exynos_ufs_hibern8_notify(struct ufs_hba *hba,
				     enum uic_cmd_dme enter,
				     enum ufs_notify_change_status notify)
{
	switch ((u8)notify) {
	case PRE_CHANGE:
		exynos_ufs_pre_hibern8(hba, enter);
		break;
	case POST_CHANGE:
		exynos_ufs_post_hibern8(hba, enter);
		break;
	}
}

static int exynos_ufs_suspend(struct ufs_hba *hba, enum ufs_pm_op pm_op)
{
	return 0;
}

static int exynos_ufs_resume(struct ufs_hba *hba, enum ufs_pm_op pm_op)
{
	return 0;
}

static int exynos_ufs_set_dma_mask (struct ufs_hba *hba) {

	struct exynos_ufs *ufs = ufshcd_get_variant(hba);

	return	dma_set_mask_and_coherent(hba->dev,
				  DMA_BIT_MASK(ufs->ufs_addr_bus_wd));
}

static struct ufs_hba_variant_ops ufs_hba_exynos_ops = {
	.name				= "exynos_ufs",
	.init				= exynos_ufs_init,
	.hce_enable_notify		= exynos_ufs_hce_enable_notify,
	.link_startup_notify		= exynos_ufs_link_startup_notify,
	.pwr_change_notify		= exynos_ufs_pwr_change_notify,
	.specify_nexus_t_xfer_req	= exynos_ufs_specify_nexus_t_xfer_req,
	.specify_nexus_t_tm_req		= exynos_ufs_specify_nexus_t_tm_req,
	.hibern8_notify			= exynos_ufs_hibern8_notify,
	.suspend			= exynos_ufs_suspend,
	.resume				= exynos_ufs_resume,
	.set_dma_mask			= exynos_ufs_set_dma_mask,
};

static int exynos_ufs_probe(struct platform_device *pdev)
{
	int err;
	struct device *dev = &pdev->dev;

	err = ufshcd_pltfrm_init(pdev, &ufs_hba_exynos_ops);
	if (err)
		dev_err(dev, "ufshcd_pltfrm_init() failed %d\n", err);

	return err;
}

static int exynos_ufs_remove(struct platform_device *pdev)
{
	struct ufs_hba *hba =  platform_get_drvdata(pdev);

	pm_runtime_get_sync(&(pdev)->dev);
	ufshcd_remove(hba);
	return 0;
}

struct exynos_ufs_drv_data exynos_ufs_drvs = {

	.compatible		= "samsung,exynos7-ufs",
	.uic_attr		= &exynos7_uic_attr,
	.quirks			= UFSHCD_QUIRK_PRDT_BYTE_GRAN |
				  UFSHCI_QUIRK_BROKEN_REQ_LIST_CLR |
				  UFSHCI_QUIRK_BROKEN_HCE |
				  UFSHCI_QUIRK_SKIP_RESET_INTR_AGGR,
	.opts			= EXYNOS_UFS_OPT_HAS_APB_CLK_CTRL |
				  EXYNOS_UFS_OPT_BROKEN_AUTO_CLK_CTRL |
				  EXYNOS_UFS_OPT_BROKEN_RX_SEL_IDX,
	.drv_init		= exynos7_ufs_drv_init,
	.pre_link		= exynos7_ufs_pre_link,
	.post_link		= exynos7_ufs_post_link,
	.pre_pwr_change		= exynos7_ufs_pre_pwr_change,
	.post_pwr_change	= exynos7_ufs_post_pwr_change,
	.ctrl_type		= UFS_TYPE_EXYNOS7,
	.ov_tm_offset		= 0x9540, /*TODO: Move this to DT */
};

struct exynos_ufs_drv_data trav_ufs_drvs = {

	.compatible		= "turbo,trav-ufs",
	.uic_attr		= &trav_uic_attr,
	.quirks			= UFSHCD_QUIRK_PRDT_BYTE_GRAN |
				  UFSHCI_QUIRK_BROKEN_REQ_LIST_CLR |
				  UFSHCI_QUIRK_SKIP_RESET_INTR_AGGR |
				  UFSHCD_QUIRK_PRDT_ENTRY_MAPPING,
	.opts			= EXYNOS_UFS_OPT_HAS_APB_CLK_CTRL |
				  EXYNOS_UFS_OPT_BROKEN_AUTO_CLK_CTRL |
				  EXYNOS_UFS_OPT_BROKEN_RX_SEL_IDX,
	.pre_link		= trav_ufs_pre_link,
	.post_link		= trav_ufs_post_link,
	.pre_pwr_change		= trav_ufs_pre_pwr_change,
	.post_pwr_change	= trav_ufs_post_pwr_change,
	.ctrl_type		= UFS_TYPE_TRAV,
	.ov_tm_offset		= 0x200, /*TODO: Move this to DT */
};

static const struct of_device_id exynos_ufs_of_match[] = {
	{ .compatible = "samsung,exynos7-ufs",
	  .data	      = &exynos_ufs_drvs },
	{ .compatible = "turbo,trav-ufs",
	  .data	      = &trav_ufs_drvs },
	{},
};

static const struct dev_pm_ops exynos_ufs_pm_ops = {
	.suspend	= ufshcd_pltfrm_suspend,
	.resume		= ufshcd_pltfrm_resume,
	.runtime_suspend = ufshcd_pltfrm_runtime_suspend,
	.runtime_resume  = ufshcd_pltfrm_runtime_resume,
	.runtime_idle    = ufshcd_pltfrm_runtime_idle,
};

static struct platform_driver exynos_ufs_pltform = {
	.probe	= exynos_ufs_probe,
	.remove	= exynos_ufs_remove,
	.shutdown = ufshcd_pltfrm_shutdown,
	.driver	= {
		.name	= "exynos-ufshc",
		.pm	= &exynos_ufs_pm_ops,
		.of_match_table = of_match_ptr(exynos_ufs_of_match),
	},
};
module_platform_driver(exynos_ufs_pltform);
