/*
 * Copyright (c) 2015, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/sched.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/export.h>
#include <linux/tegra-mce.h>
#include <linux/tegra-roc.h>

#define MCE_FLUSH_CTRL			0x0
#define MCE_FLUSH_ADDR_MATCH	0x4
#define MCE_FLUSH_ADDR_MASK		0x8

#define FLUSH_ADDR_MATCH_ALL	(BIT(29) - 1)
#define FLUSH_ADDR_MASK_NULL	0x0

#define FLUSH_CTRL_CLEANINV		0x0
#define FLUSH_CTRL_DONE_MASK	BIT(3)
#define FLUSH_CTRL_PEND_MASK	BIT(2)
#define FLUSH_CTRL_ABORT		BIT(1)
#define FLUSH_CTLR_TRIGGER		BIT(0)

#define FLUSH_POLL_DELAY		1	/* 1ms */

static void __iomem *flush_base;
static DEFINE_MUTEX(flush_lock);

/**
 * Flush cache of the whole CCPLEX.
 *
 * Needed before/after large DMA transfers with non-snooping devices.
 * This function will sleep if there is outstanding flush operation.
 *
 * Returns 0 if success.
 */
int tegra_roc_flush_cache_all(void)
{
	u32 ctrl;
	int ret = 0;

	if (!flush_base) {
		pr_err("tegra-roc-flush: driver is not ready yet.\n");
		return -EINVAL;
	}

	might_sleep();

	if (mutex_lock_interruptible(&flush_lock))
		return -ERESTARTSYS;

	/* Disable filtering */
	writel(FLUSH_ADDR_MATCH_ALL, flush_base + MCE_FLUSH_ADDR_MATCH);
	writel(FLUSH_ADDR_MASK_NULL, flush_base + MCE_FLUSH_ADDR_MASK);

	ctrl = readl(flush_base + MCE_FLUSH_CTRL);
	if (unlikely(ctrl & FLUSH_CTRL_PEND_MASK)) {
		pr_err("tegra-roc-flush: flush is pending.\n");
		ret = -EINVAL;
		goto unlock;
	}

	ctrl = FLUSH_CTRL_CLEANINV | FLUSH_CTLR_TRIGGER;
	writel(ctrl, flush_base + MCE_FLUSH_CTRL);

	for (;;) {
		if (signal_pending(current)) {
			ret = -ERESTARTSYS;
			goto unlock;
		}
		ctrl = readl(flush_base + MCE_FLUSH_CTRL);
		if (ctrl & FLUSH_CTRL_DONE_MASK)
			break;
		mdelay(FLUSH_POLL_DELAY);
	}

unlock:
	mutex_unlock(&flush_lock);
	return ret;
}
EXPORT_SYMBOL(tegra_roc_flush_cache_all);

static const struct of_device_id tegra_roc_flush_of_match[] = {
	{ .compatible = "nvidia,tegra186-roc-flush", },
	{},
};

static __init int tegra_roc_flush_init(void)
{
	struct device_node *np;

	np = of_find_compatible_node(NULL, NULL,
		tegra_roc_flush_of_match[0].compatible);
	if (!np) {
		pr_err("tegra-roc-flush: DT required.\n");
		return -EINVAL;
	}

	flush_base = of_iomap(np, 0);
	if (!flush_base) {
		pr_err("tegra-roc-flush: failed to map device.\n");
		return -EINVAL;
	}

	of_node_put(np);

	return 0;
}
early_initcall(tegra_roc_flush_init);
