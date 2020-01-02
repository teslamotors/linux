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
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/phy/phy.h>
#include <linux/phy/phy-exynos-ufs.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

#include "phy-exynos-ufs.h"

#define for_each_phy_lane(phy, i) \
	for (i = 0; i < (phy)->lane_cnt; i++)
#define for_each_phy_cfg(cfg) \
	for (; (cfg)->id; (cfg)++)

#define PHY_DEF_LANE_CNT	1

static void exynos_ufs_phy_config(struct exynos_ufs_phy *phy,
			const struct exynos_ufs_phy_cfg *cfg, u8 lane)
{
	enum {LANE_0, LANE_1}; /* lane index */

	switch (lane) {
	case LANE_0:
		writel(cfg->val, (phy)->reg_pma + cfg->off_0);
		break;
	case LANE_1:
		if (cfg->id == PHY_TRSV_BLK)
			writel(cfg->val, (phy)->reg_pma + cfg->off_1);
		break;
	}
}

static bool match_cfg_to_pwr_mode(u8 desc, u8 required_pwr)
{
	if (IS_PWR_MODE_ANY(desc))
		return true;

	if (IS_PWR_MODE_HS(required_pwr) && IS_PWR_MODE_HS_ANY(desc))
		return true;

	if (COMP_PWR_MODE(required_pwr, desc))
		return true;

	if (COMP_PWR_MODE_MD(required_pwr, desc) &&
	    COMP_PWR_MODE_GEAR(required_pwr, desc) &&
	    COMP_PWR_MODE_SER(required_pwr, desc))
		return true;

	return false;
}

int exynos_ufs_phy_calibrate(struct phy *phy,
				    enum phy_cfg_tag tag, u8 pwr)
{
	struct exynos_ufs_phy *ufs_phy = get_exynos_ufs_phy(phy);
	struct exynos_ufs_phy_cfg **cfgs = ufs_phy->cfg;
	const struct exynos_ufs_phy_cfg *cfg;
	int i;

	if (unlikely(tag < CFG_PRE_INIT || tag >= CFG_TAG_MAX)) {
		dev_err(ufs_phy->dev, "invalid phy config index %d\n", tag);
		return -EINVAL;
	}

	cfg = cfgs[tag];
	if (!cfg)
		goto out;

	for_each_phy_cfg(cfg) {
		for_each_phy_lane(ufs_phy, i) {
			if (match_cfg_to_pwr_mode(cfg->desc, pwr))
				exynos_ufs_phy_config(ufs_phy, cfg, i);
		}
	}

out:
	return 0;
}

void exynos_ufs_phy_set_lane_cnt(struct phy *phy, u8 lane_cnt)
{
	struct exynos_ufs_phy *ufs_phy = get_exynos_ufs_phy(phy);

	ufs_phy->lane_cnt = lane_cnt;
}

int exynos_ufs_phy_wait_for_lock_acq(struct phy *phy)
{
	struct exynos_ufs_phy *ufs_phy = get_exynos_ufs_phy(phy);
	const unsigned int timeout_us = 100000;
	const unsigned int sleep_us = 10;
	u32 val;
	int err;

	err = readl_poll_timeout(
			ufs_phy->reg_pma + PHY_APB_ADDR(PHY_PLL_LOCK_STATUS),
			val, (val & PHY_PLL_LOCK_BIT), sleep_us, timeout_us);
	if (err) {
		dev_err(ufs_phy->dev,
			"failed to get phy pll lock acquisition %d\n", err);
		goto out;
	}

	err = readl_poll_timeout(
			ufs_phy->reg_pma + PHY_APB_ADDR(PHY_CDR_LOCK_STATUS),
			val, (val & PHY_CDR_LOCK_BIT), sleep_us, timeout_us);
	if (err) {
		dev_err(ufs_phy->dev,
			"failed to get phy cdr lock acquisition %d\n", err);
		goto out;
	}

out:
	return err;
}

static int exynos_ufs_phy_power_on(struct phy *phy)
{
	struct exynos_ufs_phy *_phy = get_exynos_ufs_phy(phy);

	exynos_ufs_phy_ctrl_isol(_phy, false);
	return 0;
}

static int exynos_ufs_phy_power_off(struct phy *phy)
{
	struct exynos_ufs_phy *_phy = get_exynos_ufs_phy(phy);

	exynos_ufs_phy_ctrl_isol(_phy, true);
	return 0;
}

static struct phy_ops exynos_ufs_phy_ops = {
	.power_on	= exynos_ufs_phy_power_on,
	.power_off	= exynos_ufs_phy_power_off,
}
;
static const struct of_device_id exynos_ufs_phy_match[];

static int exynos_ufs_phy_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct resource *res;
	const struct of_device_id *match;
	struct exynos_ufs_phy *phy;
	struct phy *gen_phy;
	struct phy_provider *phy_provider;
	const struct exynos_ufs_phy_drvdata *drvdata;
	int err = 0;

	match = of_match_node(exynos_ufs_phy_match, dev->of_node);
	if (!match) {
		err = -EINVAL;
		dev_err(dev, "failed to get match_node\n");
		goto out;
	}

	phy = devm_kzalloc(dev, sizeof(*phy), GFP_KERNEL);
	if (!phy) {
		err = -ENOMEM;
		goto out;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "phy-pma");
	phy->reg_pma = devm_ioremap_resource(dev, res);
	if (IS_ERR(phy->reg_pma)) {
		err = PTR_ERR(phy->reg_pma);
		goto out;
	}

	phy->reg_pmu = syscon_regmap_lookup_by_phandle(
				dev->of_node, "samsung,pmu-syscon");
	if (IS_ERR(phy->reg_pmu)) {
		err = PTR_ERR(phy->reg_pmu);
		dev_err(dev, "failed syscon remap for pmu\n");
		goto out;
	}

	gen_phy = devm_phy_create(dev, NULL, &exynos_ufs_phy_ops);
	if (IS_ERR(gen_phy)) {
		err = PTR_ERR(gen_phy);
		dev_err(dev, "failed to create PHY for ufs-phy\n");
		goto out;
	}

	drvdata = match->data;
	phy->dev = dev;
	phy->drvdata = drvdata;
	phy->cfg = (struct exynos_ufs_phy_cfg **)drvdata->cfg;
	phy->isol = &drvdata->isol;
	phy->lane_cnt = PHY_DEF_LANE_CNT;

	phy_set_drvdata(gen_phy, phy);

	phy_provider = devm_of_phy_provider_register(dev, of_phy_simple_xlate);
	if (IS_ERR(phy_provider)) {
		err = PTR_ERR(phy_provider);
		dev_err(dev, "failed to register phy-provider\n");
		goto out;
	}
out:
	return err;
}

static const struct of_device_id exynos_ufs_phy_match[] = {
	{
		.compatible = "samsung,exynos7-ufs-phy",
		.data = &exynos7_ufs_phy,
	},{
		.compatible = "turbo,trav-emb-ufs-phy",
		.data = &trav_emb_ufs_phy,
	},{
		.compatible = "turbo,trav-card-ufs-phy",
		.data = &trav_card_ufs_phy,
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_ufs_phy_match);

static struct platform_driver exynos_ufs_phy_driver = {
	.probe  = exynos_ufs_phy_probe,
	.driver = {
		.name = "exynos-ufs-phy",
		.of_match_table = exynos_ufs_phy_match,
	},
};
module_platform_driver(exynos_ufs_phy_driver);
MODULE_DESCRIPTION("EXYNOS SoC UFS PHY Driver");
MODULE_AUTHOR("Seungwon Jeon <essuuj@gmail.com>");
MODULE_LICENSE("GPL v2");
