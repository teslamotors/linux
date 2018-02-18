/*
 * Copyright (C) 2015-2016, NVIDIA Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/debugfs.h>
#include <linux/platform_device.h>
#include <linux/platform/tegra/clock.h>
#include <linux/irqchip/tegra-agic.h>
#include <linux/irq.h>
#include <linux/spinlock.h>

#include "dev.h"

#define ACTMON_DEV_CTRL				0x00
#define ACTMON_DEV_CTRL_ENB			(0x1 << 31)
#define ACTMON_DEV_CTRL_AT_END_ENB		(0x1 << 15)
#define ACTMON_DEV_CTRL_PERIODIC_ENB		(0x1 << 13)
#define ACTMON_DEV_CTRL_SAMPLE_PERIOD_VAL_SHIFT (0)
#define ACTMON_DEV_CTRL_SAMPLE_PERIOD_MASK	(0xff << 0)

#define ACTMON_DEV_COUNT			0x18

#define ACTMON_DEV_INTR_STATUS			0x20
#define ACTMON_DEV_INTR_AT_END			(0x1 << 27)

#define ACTMON_DEV_COUNT_WEGHT			0x24

#define ACTMON_DEV_SAMPLE_CTRL			0x28
#define ACTMON_DEV_SAMPLE_CTRL_TICK_65536	(0x1 << 2)
#define ACTMON_DEV_SAMPLE_CTRL_TICK_256		(0x0 << 1)

#define AMISC_ACTMON_0			0x54
#define AMISC_ACTMON_CNT_TARGET_ENABLE	(0x1 << 31)

#define ACTMON_REG_OFFSET			0x800
/* milli second divider as SAMPLE_TICK*/
#define SAMPLE_MS_DIVIDER			65536


struct adsp_cpustat {
	int irq;
	struct device *device;
	const char *dev_id;
	spinlock_t lock;
	struct clk *ape_clk;
	struct clk *adsp_clk;
	unsigned long ape_freq;
	unsigned long adsp_freq;
	u64 cur_usage;
	bool enable;
	u64 max_usage;
	void __iomem *base;
};

static struct adsp_cpustat cpustat;

static struct adsp_cpustat *cpumon;

static inline u32 actmon_readl(u32 offset)
{
	return __raw_readl(cpumon->base + offset);
}
static inline void actmon_writel(u32 val, u32 offset)
{
	__raw_writel(val, cpumon->base + offset);
}

static inline void actmon_wmb(void)
{
	wmb();
}

static irqreturn_t adsp_cpustat_isr(int irq, void *dev_id)
{
	u32 val;
	unsigned long period, flags;

	spin_lock_irqsave(&cpumon->lock, flags);
	val = actmon_readl(ACTMON_DEV_INTR_STATUS);
	actmon_writel(val, ACTMON_DEV_INTR_STATUS);

	if (val & ACTMON_DEV_INTR_AT_END) {
		period = (255 * SAMPLE_MS_DIVIDER) / cpumon->ape_freq;

		cpumon->cur_usage =
			((u64)actmon_readl(ACTMON_DEV_COUNT) * 100) / (period * cpumon->adsp_freq);
		if (cpumon->cur_usage > cpumon->max_usage)
			cpumon->max_usage = cpumon->cur_usage;
	}
	spin_unlock_irqrestore(&cpumon->lock, flags);

	return IRQ_HANDLED;
}

static void configure_actmon(void)
{
	u32 val;

	/* Set countb weight to 256 */
	actmon_writel(0x100, ACTMON_DEV_COUNT_WEGHT);

	/* Enable periodic sampling */
	val = actmon_readl(ACTMON_DEV_CTRL);
	val |= ACTMON_DEV_CTRL_PERIODIC_ENB;

	/* Set sampling period to max i,e, 255 ape clks */
	val &= ~ACTMON_DEV_CTRL_SAMPLE_PERIOD_MASK;
	val |= (0xFF <<
			ACTMON_DEV_CTRL_SAMPLE_PERIOD_VAL_SHIFT)
			& ACTMON_DEV_CTRL_SAMPLE_PERIOD_MASK;

	/* Enable the AT_END interrupt */
	val |= ACTMON_DEV_CTRL_AT_END_ENB;
	actmon_writel(val, ACTMON_DEV_CTRL);

	actmon_writel(ACTMON_DEV_SAMPLE_CTRL_TICK_65536,
			ACTMON_DEV_SAMPLE_CTRL);
	actmon_wmb();
}

static void adsp_cpustat_enable(void)
{
	u32 val;
	unsigned long flags;

	spin_lock_irqsave(&cpumon->lock, flags);

	val = actmon_readl(ACTMON_DEV_CTRL);
	val |= ACTMON_DEV_CTRL_ENB;
	actmon_writel(val, ACTMON_DEV_CTRL);
	actmon_wmb();

	enable_irq(cpumon->irq);
	spin_unlock_irqrestore(&cpumon->lock, flags);
}

static void adsp_cpustat_disable(void)
{
	u32 val;
	unsigned long flags;

	spin_lock_irqsave(&cpumon->lock, flags);
	disable_irq(cpumon->irq);

	val = actmon_readl(ACTMON_DEV_CTRL);
	val &= ~ACTMON_DEV_CTRL_ENB;
	actmon_writel(val, ACTMON_DEV_CTRL);
	actmon_writel(0xffffffff, ACTMON_DEV_INTR_STATUS);
	actmon_wmb();

	spin_unlock_irqrestore(&cpumon->lock, flags);
}

#define RW_MODE (S_IWUSR | S_IRUSR)
#define RO_MODE S_IRUSR

static int cur_usage_get(void *data, u64 *val)
{
	*val = cpumon->cur_usage;
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(cur_usage_fops, cur_usage_get, NULL, "%llu\n");

static int max_usage_get(void *data, u64 *val)
{
	*val = cpumon->max_usage;
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(max_usage_fops, max_usage_get, NULL, "%llu\n");

static int enable_set(void *data, u64 val)
{
	if (cpumon->enable == (bool)val)
		return 0;
	cpumon->enable = (bool)val;

	if (cpumon->enable)
		adsp_cpustat_enable();
	else
		adsp_cpustat_disable();
	return 0;
}

static int enable_get(void *data, u64 *val)
{
	*val = cpumon->enable;
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(enable_fops, enable_get, enable_set, "%llu\n");

static int cpustat_debugfs_init(struct nvadsp_drv_data *drv)
{
	int ret = -ENOMEM;
	struct dentry *d, *dir;

	if (!drv->adsp_debugfs_root)
		return ret;
	dir = debugfs_create_dir("adsp_cpustat", drv->adsp_debugfs_root);
	if (!dir)
		return ret;

	d = debugfs_create_file(
			"cur_usage", RO_MODE, dir, cpumon, &cur_usage_fops);
	if (!d)
		return ret;

	d = debugfs_create_file(
			"max_usage", RO_MODE, dir, cpumon, &max_usage_fops);
	if (!d)
		return ret;

	d = debugfs_create_file(
			"enable", RW_MODE, dir, cpumon, &enable_fops);
	if (!d)
		return ret;

	return 0;
}

int adsp_cpustat_init(struct platform_device *pdev)
{
	struct nvadsp_drv_data *drv = platform_get_drvdata(pdev);
	static void __iomem *amisc_base;
	u32 val;
	int ret = -EINVAL;

	if (drv->cpustat_initialized)
		return 0;

	cpumon = &cpustat;
	spin_lock_init(&cpumon->lock);
	cpumon->base = drv->base_regs[AMISC] + ACTMON_REG_OFFSET;
	amisc_base = drv->base_regs[AMISC];

	cpumon->ape_clk = clk_get_sys(NULL, "adsp.ape");
	if (IS_ERR_OR_NULL(cpumon->ape_clk)) {
		dev_err(cpumon->device, "Failed to find adsp.ape clk\n");
		ret = -EINVAL;
		goto err_ape_clk;
	}

	ret = clk_prepare_enable(cpumon->ape_clk);
	if (ret) {
		dev_err(cpumon->device, "Failed to enable ape clock\n");
		goto err_ape_enable;
	}
	cpumon->ape_freq = clk_get_rate(cpumon->ape_clk) / 1000;

	cpumon->adsp_clk = clk_get_sys(NULL, "adsp_cpu");
	if (IS_ERR_OR_NULL(cpumon->adsp_clk)) {
		dev_err(cpumon->device, "Failed to find adsp cpu clock\n");
		ret = -EINVAL;
		goto err_adsp_clk;
	}

	ret = clk_prepare_enable(cpumon->adsp_clk);
	if (ret) {
		dev_err(cpumon->device, "Failed to enable adsp cpu clock\n");
		goto err_adsp_enable;
	}
	cpumon->adsp_freq = clk_get_rate(cpumon->adsp_clk) / 1000;

	/* Enable AMISC_ACTMON */
	val = __raw_readl(amisc_base + AMISC_ACTMON_0);
	val |= AMISC_ACTMON_CNT_TARGET_ENABLE;
	__raw_writel(val, amisc_base + AMISC_ACTMON_0);

	/* Clear all interrupts */
	actmon_writel(0xffffffff, ACTMON_DEV_INTR_STATUS);

	/* One time configuration of actmon regs */
	configure_actmon();

	cpumon->irq = drv->agic_irqs[ACTMON_VIRQ];
	ret = request_irq(cpumon->irq, adsp_cpustat_isr,
			IRQ_TYPE_LEVEL_HIGH, "adsp_actmon", cpumon);
	if (ret) {
		dev_err(cpumon->device, "Failed irq %d request\n", cpumon->irq);
		goto err_irq;
	}

	cpustat_debugfs_init(drv);

	drv->cpustat_initialized = true;

	return 0;
err_irq:
	clk_disable_unprepare(cpumon->adsp_clk);
err_adsp_enable:
	clk_put(cpumon->adsp_clk);
err_adsp_clk:
	clk_disable_unprepare(cpumon->ape_clk);
err_ape_enable:
	clk_put(cpumon->ape_clk);
err_ape_clk:
	return ret;
}

int adsp_cpustat_exit(struct platform_device *pdev)
{
	status_t ret = 0;
	struct nvadsp_drv_data *drv = platform_get_drvdata(pdev);
	if (!drv->cpustat_initialized) {
		ret =  -EINVAL;
		goto end;
	}

	free_irq(cpumon->irq, cpumon);
	clk_disable_unprepare(cpumon->adsp_clk);
	clk_put(cpumon->adsp_clk);
	clk_put(cpumon->ape_clk);
	drv->cpustat_initialized = false;

end:
	return ret;
}
