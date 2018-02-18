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
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/clk/tegra.h>
#include <linux/interrupt.h>
#include <linux/suspend.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <mach/irqs.h>
#include <linux/platform/tegra/cpu-tegra.h>
#include <linux/platform/tegra/clock.h>
#include <linux/tegra_pm_domains.h>
#include <linux/platform_device.h>
#include <linux/reset.h>
#include <linux/platform/tegra/actmon_common.h>
#include "iomap.h"

#define aclk(x)	((struct clk *) x)

/************ START OF REG DEFINITION **************/
#define ACTMON_GLB_STATUS			0x00
#define ACTMON_GLB_PERIOD_CTRL			0x04

#define ACTMON_DEV_CTRL				0x00
#define ACTMON_DEV_CTRL_ENB			(0x1 << 31)
#define ACTMON_DEV_CTRL_UP_WMARK_ENB		(0x1 << 30)
#define ACTMON_DEV_CTRL_DOWN_WMARK_ENB		(0x1 << 29)
#define ACTMON_DEV_CTRL_UP_WMARK_NUM_SHIFT	26
#define ACTMON_DEV_CTRL_UP_WMARK_NUM_MASK	(0x7 <<	26)
#define ACTMON_DEV_CTRL_DOWN_WMARK_NUM_SHIFT	23
#define ACTMON_DEV_CTRL_DOWN_WMARK_NUM_MASK	(0x7 <<	23)
#define ACTMON_DEV_CTRL_AVG_UP_WMARK_ENB	(0x1 << 21)
#define ACTMON_DEV_CTRL_AVG_DOWN_WMARK_ENB	(0x1 << 20)
#define ACTMON_DEV_CTRL_PERIODIC_ENB		(0x1 << 18)
#define ACTMON_DEV_CTRL_K_VAL_SHIFT		10
#define ACTMON_DEV_CTRL_K_VAL_MASK		(0x7 << 10)

#define ACTMON_DEV_UP_WMARK			0x04
#define ACTMON_DEV_DOWN_WMARK			0x08
#define ACTMON_DEV_INIT_AVG			0x0c
#define ACTMON_DEV_AVG_UP_WMARK			0x10
#define ACTMON_DEV_AVG_DOWN_WMARK			0x14

#define ACTMON_DEV_COUNT_WEGHT			0x18
#define ACTMON_DEV_COUNT			0x1c
#define ACTMON_DEV_AVG_COUNT			0x20

#define ACTMON_DEV_INTR_STATUS			0x24
#define ACTMON_DEV_INTR_UP_WMARK		(0x1 << 31)
#define ACTMON_DEV_INTR_DOWN_WMARK		(0x1 << 30)
#define ACTMON_DEV_INTR_AVG_DOWN_WMARK		(0x1 << 25)
#define ACTMON_DEV_INTR_AVG_UP_WMARK		(0x1 << 24)

/************ END OF REG DEFINITION **************/

/******** actmon device register operations start **********/
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
	u32 val = __raw_readl(base + ACTMON_DEV_CTRL);

	val |= (ACTMON_DEV_CTRL_UP_WMARK_ENB |
			ACTMON_DEV_CTRL_DOWN_WMARK_ENB |
			ACTMON_DEV_CTRL_AVG_UP_WMARK_ENB |
			ACTMON_DEV_CTRL_AVG_DOWN_WMARK_ENB);

	__raw_writel(val, base + ACTMON_DEV_CTRL);
}

static void enb_dev_intr(u32 val, void __iomem *base)
{
	u32 old_val = __raw_readl(base + ACTMON_DEV_CTRL);

	old_val |= val;
	__raw_writel(old_val, base + ACTMON_DEV_CTRL);
}

static u32 get_dev_intr(void __iomem *base)
{
	return __raw_readl(base + ACTMON_DEV_CTRL);
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
	*val |= (ACTMON_DEV_CTRL_UP_WMARK_ENB |
			ACTMON_DEV_CTRL_DOWN_WMARK_ENB);
}

static void disb_dev_up_wm(u32 *val)
{
	*val &= ~ACTMON_DEV_CTRL_UP_WMARK_ENB;
}

static void disb_dev_dn_wm(u32 *val)
{
	*val &= ~ACTMON_DEV_CTRL_DOWN_WMARK_ENB;
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

static void actmon_dev_clk_enable
		(struct actmon_dev *adev)
{
	tegra_clk_prepare_enable(aclk(adev->clnt));
}

static void actmon_dev_set_rate(struct actmon_dev *adev,
		unsigned long freq)
{
	clk_set_rate(aclk(adev->clnt), freq * 1000);
}

static unsigned long actmon_dev_get_rate(struct actmon_dev *adev)
{
	return clk_get_rate(aclk(adev->clnt));
}

int actmon_dev_platform_register(struct actmon_dev *adev,
		struct platform_device *pdev)
{
	struct clk *prnt;
	int ret = 0;

	adev->clnt = (struct clk *) clk_get_sys(adev->dev_name, adev->con_id);
	if (IS_ERR(adev->clnt)) {
		pr_err("Failed to find %s.%s clock\n", adev->dev_name,
			adev->con_id);
		return -ENODEV;
	}

	adev->dev_name = adev->dn->name;
	adev->max_freq = clk_round_rate(aclk(adev->clnt), ULONG_MAX);
	clk_set_rate(adev->clnt, adev->max_freq);
	adev->max_freq /= 1000;
	actmon_dev_reg_ops_init(adev);
	adev->actmon_dev_set_rate = actmon_dev_set_rate;
	adev->actmon_dev_get_rate = actmon_dev_get_rate;
	adev->actmon_dev_clk_enable = actmon_dev_clk_enable;

	prnt = clk_get_parent(aclk(adev->clnt));
	BUG_ON(!prnt);

	if (adev->rate_change_nb.notifier_call) {
		ret = tegra_register_clk_rate_notifier(prnt,
				&adev->rate_change_nb);
		if (ret) {
			pr_err("Failed to register %s rate change notifier for %s\n",
				prnt->name, adev->dev_name);
			return ret;
		}
	}

	return ret;
}
EXPORT_SYMBOL_GPL(actmon_dev_platform_register);

/******** actmon register operations start **********/
static void set_prd_t21x(u32 val, void __iomem *base)
{
	__raw_writel(val, base + ACTMON_GLB_PERIOD_CTRL);
}


static u32 get_glb_intr_st(void __iomem *base)
{
	return __raw_readl(base + ACTMON_GLB_STATUS);
}

/********* actmon register operations end ***********/

static void actmon_reg_ops_init(struct platform_device *pdev)
{
	struct actmon_drv_data *d = platform_get_drvdata(pdev);

	d->ops.set_sample_prd = set_prd_t21x;
	d->ops.set_glb_intr = NULL;
	d->ops.get_glb_intr_st = get_glb_intr_st;
}

static void cactmon_free_resource(
	struct actmon_dev *adev, struct platform_device *pdev)
{
	struct clk *prnt;

	prnt = clk_get_parent(aclk(adev->clnt));
	tegra_unregister_clk_rate_notifier(prnt, &adev->rate_change_nb);
}

static int cactmon_reset_deinit_t21x(struct platform_device *pdev)
{
	struct actmon_drv_data *actmon = platform_get_drvdata(pdev);

	tegra_periph_reset_assert(actmon->actmon_clk);

	return 0;
}

static int cactmon_reset_init_t21x(struct platform_device *pdev)
{
	struct actmon_drv_data *actmon = platform_get_drvdata(pdev);

	/* Reset ACTMON */
	tegra_periph_reset_assert(actmon->actmon_clk);
	udelay(10);
	tegra_periph_reset_deassert(actmon->actmon_clk);
	return 0;
}

static int cactmon_clk_disable_t21x(struct platform_device *pdev)
{
	struct actmon_drv_data *actmon = platform_get_drvdata(pdev);
	struct device *mon_dev = &pdev->dev;
	int ret = 0;

	if (actmon->actmon_clk) {
		tegra_clk_disable_unprepare(actmon->actmon_clk);
		actmon->actmon_clk = NULL;
		dev_dbg(mon_dev, "actmon clocks disabled\n");
	}

	return ret;
}

static int cactmon_clk_enable_t21x(struct platform_device *pdev)
{
	struct actmon_drv_data *actmon = platform_get_drvdata(pdev);
	struct device *mon_dev = &pdev->dev;
	int ret = 0;

	actmon->actmon_clk = tegra_get_clock_by_name("actmon");
	if (IS_ERR_OR_NULL(actmon->actmon_clk)) {
		dev_err(mon_dev, "unable to find actmon clock\n");
		ret = PTR_ERR(actmon->actmon_clk);
		return ret;
	}

	ret = tegra_clk_prepare_enable(actmon->actmon_clk);
	if (ret) {
		pr_err("%s: Failed to enable actmon clock\n", __func__);
		return ret;
	}
	actmon->freq = clk_get_rate(actmon->actmon_clk) / 1000;

	return ret;
}

int __init actmon_platform_register(struct platform_device *pdev)
{
	struct actmon_drv_data *d = platform_get_drvdata(pdev);

	d->clock_init = cactmon_clk_enable_t21x;
	d->clock_deinit = cactmon_clk_disable_t21x;
	d->reset_init = cactmon_reset_init_t21x;
	d->reset_deinit = cactmon_reset_deinit_t21x;
	d->dev_free_resource = cactmon_free_resource;
	actmon_reg_ops_init(pdev);
	return 0;
}
EXPORT_SYMBOL_GPL(actmon_platform_register);
