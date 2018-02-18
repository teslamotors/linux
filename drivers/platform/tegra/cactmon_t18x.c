/*
 * Copyright (C) 2016, NVIDIA Corporation. All rights reserved.
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
#include <linux/platform_device.h>
#include <linux/reset.h>
#include <linux/platform/tegra/emc_bwmgr.h>
#include <linux/platform/tegra/actmon_common.h>

/************ START OF REG DEFINITION **************/
/* Actmon common registers */
#define ACTMON_GLB_CTRL				0x00
/* ACTMON_MC_GLB_CTRL bit definitions */
#define ACTMON_GLB_CTRL_SAMPLE_PERIOD_VAL_SHIFT 0
#define ACTMON_GLB_CTRL_SAMPLE_PERIOD_MASK	(0xff << 0)
#define ACTMON_GLB_CTRL_SMPL_PRD_SOURCE_VAL_SHIFT 8
#define ACTMON_GLB_CTRL_SMPL_PRD_SOURCE_MASK (0x3 << 8)
#define ACTMON_GLB_CTRL_SMPL_PRD_TICK (0x2)
#define ACTMON_GLB_CTRL_SMPL_PRD_USEC (0x1)
#define ACTMON_GLB_CTRL_SMPL_PRD_MLSEC (0x0)
#define ACTMON_GLB_CTRL_SMPL_TICK_65536 (0x1 << 10)

#define ACTMON_GLB_INT_EN			0x04
/* ACTMON_MC_GLB_INT_ENABLE bit definitions */
#define ACTMON_GLB_INT_EN_MC_CPU (0x1 << 0)
#define ACTMON_GLB_INT_EN_MC_ALL (0x1 << 1)
#define ACTMON_GLB_INT_EN_TSA_CHAIN0 (0x1 << 2)
#define ACTMON_GLB_INT_EN_TSA_CHAIN1 (0x1 << 3)

#define ACTMON_GLB_INT_STATUS			0x08
#define ACTMON_GLB_INT_STATUS_MC_CPU (0x1 << 0)
#define ACTMON_GLB_INT_STATUS_MC_ALL (0x1 << 1)
#define ACTMON_GLB_INT_STATUS_TSA_CHAIN0 (0x1 << 2)
#define ACTMON_GLB_INT_STATUS_TSA_CHAIN1 (0x1 << 3)

/* Actmon device registers */
/* ACTMON_*_CTRL_0 offset */
#define ACTMON_DEV_CTRL				0x00
/* ACTMON_*_CTRL_0 bit definitions */
#define ACTMON_DEV_CTRL_ENB			(0x1 << 31)
#define ACTMON_DEV_CTRL_CUMULATIVE_ENB		(0x1 << 30)
#define ACTMON_DEV_CTRL_UP_WMARK_NUM_SHIFT	26
#define ACTMON_DEV_CTRL_UP_WMARK_NUM_MASK	(0x7 << 26)
#define ACTMON_DEV_CTRL_DOWN_WMARK_NUM_SHIFT	21
#define ACTMON_DEV_CTRL_DOWN_WMARK_NUM_MASK	(0x7 << 21)
#define ACTMON_DEV_CTRL_PERIODIC_ENB		(0x1 << 13)
#define ACTMON_DEV_CTRL_K_VAL_SHIFT		10
#define ACTMON_DEV_CTRL_K_VAL_MASK		(0x7 << 10)

/* ACTMON_*_INTR_ENABLE_0 */
#define ACTMON_DEV_INTR_ENB				0x04
/* ACTMON_*_INTR_ENABLE_0 bit definitions */
#define ACTMON_DEV_INTR_UP_WMARK_ENB		(0x1 << 31)
#define ACTMON_DEV_INTR_DOWN_WMARK_ENB		(0x1 << 30)
#define ACTMON_DEV_INTR_AVG_UP_WMARK_ENB	(0x1 << 29)
#define ACTMON_DEV_INTR_AVG_DOWN_WMARK_ENB (0x1 << 28)
#define ACTMON_DEV_INTR_AT_END_ENB (0x1 << 27)

/* ACTMON_*_INTR_STAUS_0 */
#define ACTMON_DEV_INTR_STATUS		0x08
/* ACTMON_*_INTR_STATUS_0 bit definitions */
#define ACTMON_DEV_INTR_UP_WMARK		(0x1 << 31)
#define ACTMON_DEV_INTR_DOWN_WMARK		(0x1 << 30)
#define ACTMON_DEV_INTR_AVG_DOWN_WMARK		(0x1 << 29)
#define ACTMON_DEV_INTR_AVG_UP_WMARK		(0x1 << 28)
#define ACTMON_DEV_INTR_AT_END			(0x1 << 27)

/* ACTMON_*_UPPER_WMARK_0 */
#define ACTMON_DEV_UP_WMARK			0x0c
/* ACTMON_*_LOWER_WMARK_0 */
#define ACTMON_DEV_DOWN_WMARK			0x10
/* ACTMON_*_AVG_UPPER_WMARK_0 */
#define ACTMON_DEV_AVG_UP_WMARK			0x14
/* ACTMON_*_AVG_LOWER_WMARK_0 */
#define ACTMON_DEV_AVG_DOWN_WMARK			0x18
/* ACTMON_*_AVG_INIT_AVG_0 */
#define ACTMON_DEV_INIT_AVG			0x1c
/* ACTMON_*_COUNT_0 */
#define ACTMON_DEV_COUNT			0x20
/* ACTMON_*_AVG_COUNT_0 */
#define ACTMON_DEV_AVG_COUNT			0x24
/* ACTMON_*_AVG_COUNT_WEIGHT_0 */
#define ACTMON_DEV_COUNT_WEGHT			0x28
/* ACTMON_*_CUMULATIVE_COUNT_0 */
#define ACTMON_DEV_CUMULATIVE_COUNT			0x2c
/************ END OF REG DEFINITION **************/

/******** start of actmon register operations **********/
static void set_prd_t18x(u32 val, void __iomem *base)
{
	__raw_writel(val, base + ACTMON_GLB_CTRL);
}

static void set_glb_intr(u32 val, void __iomem *base)
{
	__raw_writel(val, base + ACTMON_GLB_INT_EN);
}

static u32 get_glb_intr_st(void __iomem *base)
{
	return __raw_readl(base + ACTMON_GLB_INT_STATUS);
}
/******** end of actmon register operations **********/

/*********start of actmon device register operations***********/
static void set_init_avg(u32 val, void __iomem *base)
{
	__raw_writel(val, base + ACTMON_DEV_INIT_AVG);
}

static void set_avg_up_wm(u32 val, void __iomem *base)
{
	__raw_writel(val, base + ACTMON_DEV_AVG_UP_WMARK);
}

static void set_avg_dn_wm(u32 val, void __iomem *base)
{
	__raw_writel(val, base + ACTMON_DEV_AVG_DOWN_WMARK);
}

static void set_dev_up_wm(u32 val, void __iomem *base)
{
	__raw_writel(val, base + ACTMON_DEV_UP_WMARK);
}

static void set_dev_dn_wm(u32 val, void __iomem *base)
{
	__raw_writel(val, base + ACTMON_DEV_DOWN_WMARK);
}

static void set_cnt_wt(u32 val, void __iomem *base)
{
	__raw_writel(val, base + ACTMON_DEV_COUNT_WEGHT);
}

static void set_intr_st(u32 val, void __iomem *base)
{
	__raw_writel(val, base + ACTMON_DEV_INTR_STATUS);
}

static u32 get_intr_st(void __iomem *base)
{
	return __raw_readl(base + ACTMON_DEV_INTR_STATUS);
}

static void init_dev_cntrl(struct actmon_dev *dev, void __iomem *base)
{
	u32 val = 0;

	val |= ACTMON_DEV_CTRL_PERIODIC_ENB;
	val |= (((dev->avg_window_log2 - 1) << ACTMON_DEV_CTRL_K_VAL_SHIFT)
		& ACTMON_DEV_CTRL_K_VAL_MASK);
	val |= (((dev->down_wmark_window - 1) <<
		ACTMON_DEV_CTRL_DOWN_WMARK_NUM_SHIFT) &
		ACTMON_DEV_CTRL_DOWN_WMARK_NUM_MASK);
	val |=  (((dev->up_wmark_window - 1) <<
		ACTMON_DEV_CTRL_UP_WMARK_NUM_SHIFT) &
		ACTMON_DEV_CTRL_UP_WMARK_NUM_MASK);

	__raw_writel(val, base + ACTMON_DEV_CTRL);
}

static void enb_dev_intr_all(void __iomem *base)
{
	u32 val = 0;

	val |= (ACTMON_DEV_INTR_UP_WMARK_ENB |
			ACTMON_DEV_INTR_DOWN_WMARK_ENB |
			ACTMON_DEV_INTR_AVG_UP_WMARK_ENB |
			ACTMON_DEV_INTR_AVG_DOWN_WMARK_ENB);

	__raw_writel(val, base + ACTMON_DEV_INTR_ENB);
}

static void enb_dev_intr(u32 val, void __iomem *base)
{
	__raw_writel(val, base + ACTMON_DEV_INTR_ENB);
}

static u32 get_dev_intr(void __iomem *base)
{
	return __raw_readl(base + ACTMON_DEV_INTR_ENB);
}

static u32 get_avg_cnt(void __iomem *base)
{
	return __raw_readl(base + ACTMON_DEV_AVG_COUNT);
}

static u32 get_raw_cnt(void __iomem *base)
{
	return __raw_readl(base + ACTMON_DEV_COUNT);
}

static void enb_dev_wm(u32 *val)
{
	*val |= (ACTMON_DEV_INTR_UP_WMARK_ENB |
			ACTMON_DEV_INTR_DOWN_WMARK_ENB);
}

static void disb_dev_up_wm(u32 *val)
{
	*val &= ~ACTMON_DEV_INTR_UP_WMARK_ENB;
}

static void disb_dev_dn_wm(u32 *val)
{
	*val &= ~ACTMON_DEV_INTR_DOWN_WMARK_ENB;
}

/*********end of actmon device register operations***********/

static void actmon_dev_reg_ops_init(struct actmon_dev *adev)
{
	adev->ops.set_init_avg = set_init_avg;
	adev->ops.set_avg_up_wm = set_avg_up_wm;
	adev->ops.set_avg_dn_wm = set_avg_dn_wm;
	adev->ops.set_dev_up_wm = set_dev_up_wm;
	adev->ops.set_dev_dn_wm = set_dev_dn_wm;
	adev->ops.set_cnt_wt = set_cnt_wt;
	adev->ops.set_intr_st = set_intr_st;
	adev->ops.get_intr_st = get_intr_st;
	adev->ops.init_dev_cntrl = init_dev_cntrl;
	adev->ops.enb_dev_intr_all = enb_dev_intr_all;
	adev->ops.enb_dev_intr = enb_dev_intr;
	adev->ops.get_dev_intr_enb = get_dev_intr;
	adev->ops.get_avg_cnt = get_avg_cnt;
	adev->ops.get_raw_cnt = get_raw_cnt;
	adev->ops.enb_dev_wm = enb_dev_wm;
	adev->ops.disb_dev_up_wm = disb_dev_up_wm;
	adev->ops.disb_dev_dn_wm = disb_dev_dn_wm;
}

static unsigned long actmon_dev_get_rate(struct actmon_dev *adev)
{
	return tegra_bwmgr_get_emc_rate();
}

static unsigned long actmon_dev_post_change_rate(
	struct actmon_dev *adev, void *cclk)
{
	struct clk_notifier_data *clk_data = cclk;

	return clk_data->new_rate;
}
static void actmon_dev_set_rate(struct actmon_dev *adev,
		unsigned long freq)
{
	struct tegra_bwmgr_client *bwclnt = (struct tegra_bwmgr_client *)
			adev->clnt;

	tegra_bwmgr_set_emc(bwclnt, freq * 1000,
		TEGRA_BWMGR_SET_EMC_FLOOR);
}

static int cactmon_bwmgr_register_t18x(
	struct actmon_dev *adev, struct platform_device *pdev)
{
	struct tegra_bwmgr_client *bwclnt = (struct tegra_bwmgr_client *)
		adev->clnt;
	struct device *mon_dev = &pdev->dev;
	int ret = 0;

	bwclnt = tegra_bwmgr_register(TEGRA_BWMGR_CLIENT_MON);
	if (IS_ERR_OR_NULL(bwclnt)) {
		ret = -ENODEV;
		dev_err(mon_dev, "emc bw manager registration failed for %s\n",
			adev->dn->name);
		return ret;
	}

	adev->clnt = bwclnt;

	return ret;
}


static void cactmon_bwmgr_unregister_t18x(
	struct actmon_dev *adev, struct platform_device *pdev)
{
	struct tegra_bwmgr_client *bwclnt = (struct tegra_bwmgr_client *)
			adev->clnt;
	struct device *mon_dev = &pdev->dev;

	if (bwclnt) {
		dev_dbg(mon_dev, "unregistering BW manager for %s\n",
			adev->dn->name);
		tegra_bwmgr_unregister(bwclnt);
		adev->clnt = NULL;
	}
}

int actmon_dev_platform_register(struct actmon_dev *adev,
		struct platform_device *pdev)
{
	struct tegra_bwmgr_client *bwclnt;
	int ret = 0;

	ret = cactmon_bwmgr_register_t18x(adev, pdev);
	if (ret)
		goto end;

	bwclnt = (struct tegra_bwmgr_client *) adev->clnt;
	adev->dev_name = adev->dn->name;
	adev->max_freq = tegra_bwmgr_get_max_emc_rate();
	tegra_bwmgr_set_emc(bwclnt, adev->max_freq,
		TEGRA_BWMGR_SET_EMC_FLOOR);
	adev->max_freq /= 1000;
	actmon_dev_reg_ops_init(adev);
	adev->actmon_dev_set_rate = actmon_dev_set_rate;
	adev->actmon_dev_get_rate = actmon_dev_get_rate;
	if (adev->rate_change_nb.notifier_call) {
		ret = tegra_bwmgr_notifier_register(&adev->rate_change_nb);
		if (ret) {
			pr_err("Failed to register bw manager rate change notifier for %s\n",
				adev->dev_name);
			return ret;
		}
	}
	adev->actmon_dev_post_change_rate = actmon_dev_post_change_rate;

end:
	return ret;
}
EXPORT_SYMBOL_GPL(actmon_dev_platform_register);

static void actmon_reg_ops_init(struct platform_device *pdev)
{
	struct actmon_drv_data *d = platform_get_drvdata(pdev);

	d->ops.set_sample_prd = set_prd_t18x;
	d->ops.set_glb_intr = set_glb_intr;
	d->ops.get_glb_intr_st = get_glb_intr_st;
}

static void cactmon_free_resource(
	struct actmon_dev *adev, struct platform_device *pdev)
{
int ret;

	if (adev->rate_change_nb.notifier_call) {
		ret = tegra_bwmgr_notifier_unregister(&adev->rate_change_nb);
		if (ret) {
			pr_err("Failed to register bw manager rate change notifier for %s\n",
			adev->dev_name);
		}
	}

	cactmon_bwmgr_unregister_t18x(adev, pdev);
}

static int cactmon_reset_dinit_t18x(struct platform_device *pdev)
{
	struct actmon_drv_data *actmon = platform_get_drvdata(pdev);
	struct device *mon_dev = &pdev->dev;
	int ret = -EINVAL;

	if (actmon->actmon_rst) {
		ret = reset_control_assert(actmon->actmon_rst);
		if (ret)
			dev_err(mon_dev, "failed to assert actmon\n");
	}

	return ret;
}

static int cactmon_reset_init_t18x(struct platform_device *pdev)
{
	struct actmon_drv_data *actmon = platform_get_drvdata(pdev);
	struct device *mon_dev = &pdev->dev;
	int ret = 0;

	actmon->actmon_rst = devm_reset_control_get(mon_dev, "actmon_rst");
	if (IS_ERR(actmon->actmon_rst)) {
		ret = PTR_ERR(actmon->actmon_rst);
		dev_err(mon_dev, "can not get actmon reset%d\n", ret);
	}

	/* Make actmon out of reset */
	ret = reset_control_deassert(actmon->actmon_rst);
	if (ret)
		dev_err(mon_dev, "failed to deassert actmon\n");

	return ret;
}


static int cactmon_clk_disable_t18x(struct platform_device *pdev)
{
	struct actmon_drv_data *actmon = platform_get_drvdata(pdev);
	struct device *mon_dev = &pdev->dev;
	int ret = 0;

	if (actmon->actmon_clk) {
		clk_disable_unprepare(actmon->actmon_clk);
		devm_clk_put(mon_dev, actmon->actmon_clk);
		actmon->actmon_clk = NULL;
		dev_dbg(mon_dev, "actmon clocks disabled\n");
	}

	return ret;
}

static int cactmon_clk_enable_t18x(struct platform_device *pdev)
{
	struct actmon_drv_data *actmon = platform_get_drvdata(pdev);
	struct device *mon_dev = &pdev->dev;
	int ret = 0;

	actmon->actmon_clk = devm_clk_get(mon_dev, "actmon");
	if (IS_ERR_OR_NULL(actmon->actmon_clk)) {
		dev_err(mon_dev, "unable to find actmon clock\n");
		ret = PTR_ERR(actmon->actmon_clk);
		return ret;
	}

	ret = clk_prepare_enable(actmon->actmon_clk);
	if (ret) {
		dev_err(mon_dev, "unable to enable actmon clock\n");
		goto end;
	}

	actmon->freq = clk_get_rate(actmon->actmon_clk) / 1000;

	return 0;
end:
	devm_clk_put(mon_dev, actmon->actmon_clk);
	return ret;
}

int __init actmon_platform_register(struct platform_device *pdev)
{
	struct actmon_drv_data *d = platform_get_drvdata(pdev);

	d->clock_init = cactmon_clk_enable_t18x;
	d->clock_deinit = cactmon_clk_disable_t18x;
	d->reset_init = cactmon_reset_init_t18x;
	d->reset_deinit = cactmon_reset_dinit_t18x;
	d->dev_free_resource = cactmon_free_resource;
	actmon_reg_ops_init(pdev);
	return 0;
}
EXPORT_SYMBOL_GPL(actmon_platform_register);
