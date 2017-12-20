/* compat26/tegra/clock.c
 *
 * Copyright 2016 Codethink Ltd.
 */

#include <linux/kernel.h>
#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/clk-provider.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/reset.h>
#include <linux/slab.h>

#include "../include/compat26.h"
#include "../include/compat_clk.h"
#include "../include/mach/clk.h"

#include "../tegra26/clock.h"

#include "compat26_internal.h"

#if 1
#define __debug(arg...) do { } while(0)
#else
#define __debug(arg...) printk(KERN_INFO arg)
#endif

struct clk *tegra_get_clock_by_name(const char *name)
{
	struct clk *clk;

	clk = clk_get(NULL, name);
	if (IS_ERR_OR_NULL(clk)) {
		pr_err("%s: failed to find %s\n", __func__, name);
		clk = NULL;
	}

	if (clk)
		__debug("%s: clk %s => %p (%s)\n", __func__, name,
			clk, __clk_get_name(clk));

	/* ensure clock is prepared, often code isn't doing that */
	clk_prepare(clk);
	return clk;
}
EXPORT_SYMBOL(tegra_get_clock_by_name);

#ifdef CONFIG_ARCH_TEGRA_3x_SOC
int tegra_clk_cfg_ex(struct clk *c, enum tegra_clk_ex_param p, u32 setting)
{
	WARN_ON(1);
	return -1;
}
EXPORT_SYMBOL(tegra_clk_cfg_ex);
#endif

int tegra_is_clk_enabled(struct clk *c)
{
	return __clk_is_enabled(c) ? 1 : 0;
}
EXPORT_SYMBOL(tegra_is_clk_enabled);

/* 2.6 peripheral clocks which do not have a reset:
 * tegra2:	vi_sensor, csus, 3d, pcie, afi, pciex
 * tegra3:	vi_sensor, csus, 3d, 3d2
 */

int tegra26_compat_clk_enable(struct clk *clk)
{
	int rc;
	__debug("%s: clk %p(%s)\n", __func__, clk, __clk_get_name(clk));

	rc = clk_enable(clk);
	if (rc)
		pr_err("%s: failed to enable clk %p\n", __func__, clk);

	return rc;
}
EXPORT_SYMBOL(tegra26_compat_clk_enable);

#if 0
int tegra26_compat_clk_enable(struct clk *clk)
{
	struct reset_control *reset;
	int enabled = 0;
	int rc;

	__debug("%s: clk %p(%s)\n", __func__, clk, __clk_get_name(clk));

	reset = tegra_clk_to_reset(clk);
	if (!reset)
		pr_warn("%s: no reset for clk %p(%s)\n",
			__func__, clk,  __clk_get_name(clk));
	else
		enabled = tegra_is_clk_enabled(clk);

	rc = clk_enable(clk);
	if (rc) {
		pr_err("%s: failed to enable clk %p\n", __func__, clk);
		return rc;
	}

	/* todo - check if we can actually do this for all clocks */
	if (reset) {
		int rc = reset_control_status(reset) ;

		if (rc > 0) {
			__debug("%s: de-asserting reset for clk %p(%s)\n",
				__func__, clk,  __clk_get_name(clk));
			udelay(5);
			reset_control_deassert(reset);
		} else if (rc == 0) {
			__debug("%s: clock is already un-reset %p(%s)\n",
				__func__, clk,  __clk_get_name(clk));
		} else {
			WARN_ON(1);
		}
	}

	if (!reset && !enabled)
		pr_warn("%s: cannot unreset clk %p(%s)\n",
			__func__, clk,  __clk_get_name(clk));

	return 0;
}
EXPORT_SYMBOL(tegra26_compat_clk_enable);
#endif

void tegra26_compat_clk_disable(struct clk *clk)
{
	__debug("%s: clk %p(%s)\n", __func__, clk, __clk_get_name(clk));

	clk_disable(clk);
}
EXPORT_SYMBOL(tegra26_compat_clk_disable);

int tegra26_compat_clk_set_parent(struct clk *clk, struct clk *parent)
{
	__debug("%s: clk %p(%s) to parent %p(%s)\n",
		__func__, clk, __clk_get_name(clk),
		parent, __clk_get_name(parent));

	return clk_set_parent(clk, parent);
}
EXPORT_SYMBOL(tegra26_compat_clk_set_parent);

int tegra26_compat_clk_set_rate(struct clk *clk, unsigned long rate)
{
	__debug("%s: clk %p(%s) rate to %lu\n",
		__func__, clk, __clk_get_name(clk), rate);
	return clk_set_rate(clk, rate);
}
EXPORT_SYMBOL(tegra26_compat_clk_set_rate);

struct clk *tegra26_compat_clk_get(struct device *dev, const char *id)
{
	struct clk *clk = clk_get(dev, id);

	if (id != NULL && strcmp(id, "emc") == 0) {
		clk_put(clk);
		clk = clk_get(NULL, "emc");
	}

	__debug("%s: clk(%s,%s) => %p(%s)\n",
		__func__,
		dev ? dev_name(dev) : "null",
		id ? id: "null",
		clk,
		IS_ERR(clk) ? "error" : __clk_get_name(clk));
	return clk;
}
EXPORT_SYMBOL(tegra26_compat_clk_get);

/* AVP driver expects a clock, which is the child of SCLK
 * which does not have a gate, but does have a reset. The
 * driver uses this to unreset the AVP/COP units.
 *
 * The tegra clock code does not deal well with clocks with
 * no gate, so we add a simple clock implementation here.
 */

struct compat_reset_clk {
	struct clk_hw		hw;
	struct reset_control	*reset;
	unsigned int		enable_count;
};

#define to_rclk(__hw) container_of(__hw, struct compat_reset_clk, hw)

static int rclk_is_enabled(struct clk_hw *hw)
{
	struct compat_reset_clk *rclk = to_rclk(hw);

	return rclk->enable_count != 0;
}

static int rclk_enable(struct clk_hw *hw)
{
	struct compat_reset_clk *rclk = to_rclk(hw);
	int reset;

	if (rclk->enable_count == 0) {
		reset = reset_control_status(rclk->reset);
		if (reset < 0) {
			pr_err("%s: status error %d\n", __func__, reset);
			return reset;
		}

		if (reset == 1) {
			reset_control_deassert(rclk->reset);
			udelay(5);
		}
	}

	rclk->enable_count++;
	return 0;
}

static void rclk_disable(struct clk_hw *hw)
{
	struct compat_reset_clk *rclk = to_rclk(hw);
	if (rclk->enable_count)
		rclk->enable_count--;
}

static const struct clk_ops rclk_ops = {
	.is_enabled	= rclk_is_enabled,
	.enable		= rclk_enable,
	.disable	= rclk_disable,
};

static const char *rclk_parents[] = { "sclk" };

static struct clk_init_data rclk_init = {
	.name	= "cop",
	.flags	= 0,
	.ops	= &rclk_ops,
	.parent_names = rclk_parents,
	.num_parents	= ARRAY_SIZE(rclk_parents),
};

int tegra26_compat_avp_clk(struct device *dev)
{
	struct compat_reset_clk *rclk;
	struct clk_lookup *lookup;
	struct clk *clk;
	int ret;

	rclk = kzalloc(sizeof(*rclk), GFP_KERNEL);
	if (!rclk) {
		dev_err(dev, "no memory for clock\n");
		return -ENOMEM;
	}

	rclk->hw.init = &rclk_init;	/* hw.init is copied by clk_register() */

	rclk->reset = reset_control_get(dev, "cop");
	if (IS_ERR(rclk->reset)) {
		dev_err(dev, "cannot find reset\n");
		ret = -EINVAL;
		goto err1;
	}

	clk = clk_register(NULL, &rclk->hw);
	if (IS_ERR(clk)) {
		dev_err(dev, "cannot register clock\n");
		ret = PTR_ERR(clk);
		goto err2;
	}

	lookup = clkdev_alloc(clk, "cop", "tegra-avp");
	if (!lookup) {
		dev_err(dev, "cannot alloc clkdev lookup\n");
		ret = -ENOMEM;
		goto err3;
	}

	clkdev_add(lookup);

	tegra_compat_add_clk_reset(clk, rclk->reset);
	return 0;
err3:	
err2:
	reset_control_put(rclk->reset);
err1:
	kfree(rclk);
	return ret;
}
