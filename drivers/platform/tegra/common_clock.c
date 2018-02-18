/*
 *
 * Copyright (C) 2010 Google, Inc.
 *
 * Author:
 *      Colin Cross <ccross@google.com>
 *
 * Copyright (c) 2010-2015, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/clk.h>

#include <linux/platform/tegra/clock.h>


static DEFINE_MUTEX(clock_list_lock);
static LIST_HEAD(clocks);

struct clk *tegra_get_clock_by_name(const char *name)
{
	struct clk_tegra *c;
	struct clk *ret = NULL;
	mutex_lock(&clock_list_lock);
	list_for_each_entry(c, &clocks, node) {
		if (strcmp(__clk_get_name(c->hw.clk), name) == 0) {
			ret = c->hw.clk;
			break;
		}
	}
	mutex_unlock(&clock_list_lock);
	return ret;
}

static int tegra_clk_init_one_from_table(struct tegra_clk_init_table *table)
{
	struct clk *c;
	struct clk *p;
	struct clk *parent;

	int ret = 0;

	c = tegra_get_clock_by_name(table->name);

	if (!c) {
		pr_warn("Unable to initialize clock %s\n",
			table->name);
		return -ENODEV;
	}

	parent = clk_get_parent(c);

	if (table->parent) {
		p = tegra_get_clock_by_name(table->parent);
		if (!p) {
			pr_warn("Unable to find parent %s of clock %s\n",
				table->parent, table->name);
			return -ENODEV;
		}

		if (parent != p) {
			ret = clk_set_parent(c, p);
			if (ret) {
				pr_warn("Unable to set parent %s of clock %s: %d\n",
					table->parent, table->name, ret);
				return -EINVAL;
			}
		}
	}

	if (table->rate && table->rate != clk_get_rate(c)) {
		ret = clk_set_rate(c, table->rate);
		if (ret) {
			pr_warn("Unable to set clock %s to rate %lu: %d\n",
				table->name, table->rate, ret);
			return -EINVAL;
		}
	}

	if (table->enabled) {
		ret = clk_prepare_enable(c);
		if (ret) {
			pr_warn("Unable to enable clock %s: %d\n",
				table->name, ret);
			return -EINVAL;
		}
	}

	return 0;
}

void tegra_clk_init_from_table(struct tegra_clk_init_table *table)
{
	for (; table->name; table++)
		tegra_clk_init_one_from_table(table);
}

#ifdef CONFIG_ARCH_TEGRA_18x_SOC
void tegra_periph_reset_deassert(struct clk *c)
{
}
EXPORT_SYMBOL(tegra_periph_reset_deassert);

void tegra_periph_reset_assert(struct clk *c)
{
}
EXPORT_SYMBOL(tegra_periph_reset_assert);
#else
void tegra_periph_reset_deassert(struct clk *c)
{
	struct clk_tegra *clk = to_clk_tegra(__clk_get_hw(c));
	if (WARN_ON(!clk->reset))
		return;
	clk->reset(__clk_get_hw(c), false);
}
EXPORT_SYMBOL(tegra_periph_reset_deassert);

void tegra_periph_reset_assert(struct clk *c)
{
	struct clk_tegra *clk = to_clk_tegra(__clk_get_hw(c));
	if (WARN_ON(!clk->reset))
		return;
	clk->reset(__clk_get_hw(c), true);
}
EXPORT_SYMBOL(tegra_periph_reset_assert);
#endif

/* Several extended clock configuration bits (e.g., clock routing, clock
 * phase control) are included in PLL and peripheral clock source
 * registers. */
int tegra_clk_cfg_ex(struct clk *c, enum tegra_clk_ex_param p, u32 setting)
{
	int ret = 0;
	struct clk_tegra *clk = to_clk_tegra(__clk_get_hw(c));

	if (!clk->clk_cfg_ex) {
		ret = -ENOSYS;
		goto out;
	}
	ret = clk->clk_cfg_ex(__clk_get_hw(c), p, setting);

out:
	return ret;
}
