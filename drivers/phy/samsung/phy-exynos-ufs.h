/*
 * UFS PHY driver for Samsung EXYNOS SoC
 *
 * Copyright (C) 2015 Samsung Electronics Co., Ltd.
 * Author: Seungwon Jeon <essuuj@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#ifndef _PHY_EXYNOS_UFS_
#define _PHY_EXYNOS_UFS_

#define PHY_COMN_BLK	1
#define PHY_TRSV_BLK	2
#define END_UFS_PHY_CFG { 0 }
#define PHY_TRSV_CH_OFFSET	0x30
#define PHY_APB_ADDR(off)	((off) << 2)

#define PHY_COMN_REG_CFG(o, v, d) {	\
	.off_0 = PHY_APB_ADDR((o)),	\
	.off_1 = 0,		\
	.val = (v),		\
	.desc = (d),		\
	.id = PHY_COMN_BLK,	\
}

#define PHY_TRSV_REG_CFG(o, v, d) {	\
	.off_0 = PHY_APB_ADDR((o)),	\
	.off_1 = PHY_APB_ADDR((o) + PHY_TRSV_CH_OFFSET),	\
	.val = (v),		\
	.desc = (d),		\
	.id = PHY_TRSV_BLK,	\
}

/* UFS PHY registers */
#define PHY_PLL_LOCK_STATUS	0x1e
#define PHY_CDR_LOCK_STATUS	0x5e

#define PHY_PLL_LOCK_BIT	BIT(5)
#define PHY_CDR_LOCK_BIT	BIT(4)

struct exynos_ufs_phy_cfg {
	u32 off_0;
	u32 off_1;
	u32 val;
	u8 desc;
	u8 id;
};

struct exynos_ufs_phy_drvdata {
	const struct exynos_ufs_phy_cfg **cfg;
	struct pmu_isol {
		u32 offset;
		u32 mask;
		u32 en;
	} isol;
};

struct exynos_ufs_phy {
	struct device *dev;
	void __iomem *reg_pma;
	struct regmap *reg_pmu;
	const struct exynos_ufs_phy_drvdata *drvdata;
	struct exynos_ufs_phy_cfg **cfg;
	const struct pmu_isol *isol;
	u8 lane_cnt;
};

static inline struct exynos_ufs_phy *get_exynos_ufs_phy(struct phy *phy)
{
	return (struct exynos_ufs_phy *)phy_get_drvdata(phy);
}

static inline void exynos_ufs_phy_ctrl_isol(
		struct exynos_ufs_phy *phy, u32 isol)
{
	regmap_update_bits(phy->reg_pmu, phy->isol->offset,
			phy->isol->mask, isol ? 0 : phy->isol->en);
}

#include "phy-exynos7-ufs.h"

#endif /* _PHY_EXYNOS_UFS_ */
