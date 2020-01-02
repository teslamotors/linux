/*
 * UFS PHY driver for Samsung EXYNOS SoC
 *
 * Copyright (C) 2015 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#ifndef _PHY_EXYNOS7_UFS_H_
#define _PHY_EXYNOS7_UFS_H_

#include "phy-exynos-ufs.h"

#define EXYNOS7_EMBEDDED_COMBO_PHY_CTRL	0x720
#define EXYNOS7_EMBEDDED_COMBO_PHY_CTRL_MASK	0x1
#define EXYNOS7_EMBEDDED_COMBO_PHY_CTRL_EN	BIT(0)

#define TRAV_EMBEDDED_UFS_PHY_CTRL	0x724
#define TRAV_CARD_UFS_PHY_CTRL	0x728

/* Calibration for phy initialization */
static const struct exynos_ufs_phy_cfg exynos7_pre_init_cfg[] = {
	PHY_COMN_REG_CFG(0x00f, 0xfa, PWR_MODE_ANY),
	PHY_COMN_REG_CFG(0x010, 0x82, PWR_MODE_ANY),
	PHY_COMN_REG_CFG(0x011, 0x1e, PWR_MODE_ANY),
	PHY_COMN_REG_CFG(0x017, 0x84, PWR_MODE_ANY),
	PHY_TRSV_REG_CFG(0x035, 0x58, PWR_MODE_ANY),
	PHY_TRSV_REG_CFG(0x036, 0x32, PWR_MODE_ANY),
	PHY_TRSV_REG_CFG(0x037, 0x40, PWR_MODE_ANY),
	PHY_TRSV_REG_CFG(0x03b, 0x83, PWR_MODE_ANY),
	PHY_TRSV_REG_CFG(0x042, 0x88, PWR_MODE_ANY),
	PHY_TRSV_REG_CFG(0x043, 0xa6, PWR_MODE_ANY),
	PHY_TRSV_REG_CFG(0x048, 0x74, PWR_MODE_ANY),
	PHY_TRSV_REG_CFG(0x04c, 0x5b, PWR_MODE_ANY),
	PHY_TRSV_REG_CFG(0x04d, 0x83, PWR_MODE_ANY),
	PHY_TRSV_REG_CFG(0x05c, 0x14, PWR_MODE_ANY),
	END_UFS_PHY_CFG
};

static const struct exynos_ufs_phy_cfg exynos7_post_init_cfg[] = {
	END_UFS_PHY_CFG
};

/* Calibration for HS mode series A/B */
static const struct exynos_ufs_phy_cfg exynos7_pre_pwr_hs_cfg[] = {
	PHY_COMN_REG_CFG(0x00f, 0xfa, PWR_MODE_HS_ANY),
	PHY_COMN_REG_CFG(0x010, 0x82, PWR_MODE_HS_ANY),
	PHY_COMN_REG_CFG(0x011, 0x1e, PWR_MODE_HS_ANY),
	/* Setting order: 1st(0x16, 2nd(0x15) */
	PHY_COMN_REG_CFG(0x016, 0xff, PWR_MODE_HS_ANY),
	PHY_COMN_REG_CFG(0x015, 0x80, PWR_MODE_HS_ANY),
	PHY_COMN_REG_CFG(0x017, 0x94, PWR_MODE_HS_ANY),
	PHY_TRSV_REG_CFG(0x036, 0x32, PWR_MODE_HS_ANY),
	PHY_TRSV_REG_CFG(0x037, 0x43, PWR_MODE_HS_ANY),
	PHY_TRSV_REG_CFG(0x038, 0x3f, PWR_MODE_HS_ANY),
	PHY_TRSV_REG_CFG(0x042, 0x88, PWR_MODE_HS_G2_SER_A),
	PHY_TRSV_REG_CFG(0x042, 0xbb, PWR_MODE_HS_G2_SER_B),
	PHY_TRSV_REG_CFG(0x043, 0xa6, PWR_MODE_HS_ANY),
	PHY_TRSV_REG_CFG(0x048, 0x74, PWR_MODE_HS_ANY),
	PHY_TRSV_REG_CFG(0x034, 0x35, PWR_MODE_HS_G2_SER_A),
	PHY_TRSV_REG_CFG(0x034, 0x36, PWR_MODE_HS_G2_SER_B),
	PHY_TRSV_REG_CFG(0x035, 0x5b, PWR_MODE_HS_G2_SER_A),
	PHY_TRSV_REG_CFG(0x035, 0x5c, PWR_MODE_HS_G2_SER_B),
	END_UFS_PHY_CFG
};

/* Calibration for HS mode series A/B atfer PMC */
static const struct exynos_ufs_phy_cfg exynos7_post_pwr_hs_cfg[] = {
	PHY_COMN_REG_CFG(0x015, 0x00, PWR_MODE_HS_ANY),
	PHY_TRSV_REG_CFG(0x04d, 0x83, PWR_MODE_HS_ANY),
	END_UFS_PHY_CFG
};

static const struct exynos_ufs_phy_cfg *exynos7_ufs_phy_cfgs[CFG_TAG_MAX] = {
	[CFG_PRE_INIT]		= exynos7_pre_init_cfg,
	[CFG_POST_INIT]		= exynos7_post_init_cfg,
	[CFG_PRE_PWR_HS]	= exynos7_pre_pwr_hs_cfg,
	[CFG_POST_PWR_HS]	= exynos7_post_pwr_hs_cfg,
};

static struct exynos_ufs_phy_drvdata exynos7_ufs_phy = {
	.cfg = exynos7_ufs_phy_cfgs,
	.isol = {
		.offset = EXYNOS7_EMBEDDED_COMBO_PHY_CTRL,
		.mask = EXYNOS7_EMBEDDED_COMBO_PHY_CTRL_MASK,
		.en = EXYNOS7_EMBEDDED_COMBO_PHY_CTRL_EN,
	},
};

static struct exynos_ufs_phy_drvdata trav_emb_ufs_phy = {
	.isol = {
		.offset = TRAV_EMBEDDED_UFS_PHY_CTRL,
		.mask = EXYNOS7_EMBEDDED_COMBO_PHY_CTRL_MASK,
		.en = EXYNOS7_EMBEDDED_COMBO_PHY_CTRL_EN,
	},
};

static struct exynos_ufs_phy_drvdata trav_card_ufs_phy = {
	.isol = {
		.offset = TRAV_CARD_UFS_PHY_CTRL,
		.mask = EXYNOS7_EMBEDDED_COMBO_PHY_CTRL_MASK,
		.en = EXYNOS7_EMBEDDED_COMBO_PHY_CTRL_EN,
	},
};
#endif /* _PHY_EXYNOS7_UFS_H_ */
