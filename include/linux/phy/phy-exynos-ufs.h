/*
 * phy-exynos-ufs.h - Header file for the UFS PHY of Exynos SoC
 *
 * Copyright (C) 2015 Samsung Electronics Co., Ltd.
 * Author: Seungwon Jeon <essuuj@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _PHY_EXYNOS_UFS_H_
#define _PHY_EXYNOS_UFS_H_

#include "phy.h"

/* PHY calibration point */
enum phy_cfg_tag {
	CFG_PRE_INIT	= 0,
	CFG_POST_INIT	= 1,
	CFG_PRE_PWR_HS	= 2,
	CFG_POST_PWR_HS	= 3,
	CFG_TAG_MAX,
};

/* description for PHY calibration */
enum {
	/* applicable to any */
	PWR_DESC_ANY	= 0,
	/* mode */
	PWR_DESC_PWM	= 1,
	PWR_DESC_HS	= 2,
	/* series */
	PWR_DESC_SER_A	= 1,
	PWR_DESC_SER_B	= 2,
	/* gear */
	PWR_DESC_G1	= 1,
	PWR_DESC_G2	= 2,
	PWR_DESC_G3	= 3,
	PWR_DESC_G4	= 4,
	PWR_DESC_G5	= 5,
	PWR_DESC_G6	= 6,
	PWR_DESC_G7	= 7,
	/* field mask */
	MD_MASK		= 0x3,
	SR_MASK		= 0x3,
	GR_MASK		= 0x7,
};

#define PWR_MODE(g, s, m)	((((g) & GR_MASK) << 4) |\
				 (((s) & SR_MASK) << 2) | ((m) & MD_MASK))
#define PWR_MODE_HS(g, s)	((((g) & GR_MASK) << 4) |\
				 (((s) & SR_MASK) << 2) | PWR_DESC_HS)
#define PWR_MODE_HS_G1_ANY	PWR_MODE_HS(PWR_DESC_G1, PWR_DESC_ANY)
#define PWR_MODE_HS_G1_SER_A	PWR_MODE_HS(PWR_DESC_G1, PWR_DESC_SER_A)
#define PWR_MODE_HS_G1_SER_B	PWR_MODE_HS(PWR_DESC_G1, PWR_DESC_SER_B)
#define PWR_MODE_HS_G2_ANY	PWR_MODE_HS(PWR_DESC_G2, PWR_DESC_ANY)
#define PWR_MODE_HS_G2_SER_A	PWR_MODE_HS(PWR_DESC_G2, PWR_DESC_SER_A)
#define PWR_MODE_HS_G2_SER_B	PWR_MODE_HS(PWR_DESC_G2, PWR_DESC_SER_B)
#define PWR_MODE_HS_G3_ANY	PWR_MODE_HS(PWR_DESC_G3, PWR_DESC_ANY)
#define PWR_MODE_HS_G3_SER_A	PWR_MODE_HS(PWR_DESC_G3, PWR_DESC_SER_A)
#define PWR_MODE_HS_G3_SER_B	PWR_MODE_HS(PWR_DESC_G3, PWR_DESC_SER_B)
#define PWR_MODE_HS_ANY		PWR_MODE(PWR_DESC_ANY,\
					 PWR_DESC_ANY, PWR_DESC_HS)
#define PWR_MODE_PWM_ANY	PWR_MODE(PWR_DESC_ANY,\
					 PWR_DESC_ANY, PWR_DESC_PWM)
#define PWR_MODE_ANY		PWR_MODE(PWR_DESC_ANY,\
					 PWR_DESC_ANY, PWR_DESC_ANY)
#define IS_PWR_MODE_HS(d)	(((d) & MD_MASK) == PWR_DESC_HS)
#define IS_PWR_MODE_PWM(d)	(((d) & MD_MASK) == PWR_DESC_PWM)
#define IS_PWR_MODE_ANY(d)	((d) == PWR_MODE_ANY)
#define IS_PWR_MODE_HS_ANY(d)	((d) == PWR_MODE_HS_ANY)
#define COMP_PWR_MODE(a, b)		((a) == (b))
#define COMP_PWR_MODE_GEAR(a, b)	((((a) >> 4) & GR_MASK) == \
					 (((b) >> 4) & GR_MASK))
#define COMP_PWR_MODE_SER(a, b)		((((a) >> 2) & SR_MASK) == \
					 (((b) >> 2) & SR_MASK))
#define COMP_PWR_MODE_MD(a, b)		(((a) & MD_MASK) == ((b) & MD_MASK))

int exynos_ufs_phy_calibrate(struct phy *phy, enum phy_cfg_tag tag, u8 pwr);
void exynos_ufs_phy_set_lane_cnt(struct phy *phy, u8 lane_cnt);
int exynos_ufs_phy_wait_for_lock_acq(struct phy *phy);

#endif /* _PHY_EXYNOS_UFS_H_ */
