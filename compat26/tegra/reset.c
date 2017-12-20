/* compat26/tegra/reset.c
 *
 * Copyright 2016 Codethink Ltd.
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>

#include "../include/compat26.h"
#include "../include/mach/clk.h"

#include "compat26_internal.h"

#define __debug(x...) do { } while(0)

/* we have a lock, the code shouldn't be called often but it is better
 * to be safe and lock the list.
 */
static DEFINE_SPINLOCK(clk_reset_lock);
static LIST_HEAD(clk_reset_list);

struct reset_map {
	struct list_head	list;
	struct clk		*clk;
	struct reset_control	*reset;
};

struct reset_control *tegra_clk_to_reset(struct clk *c)
{
	struct reset_map *map;

	if (IS_ERR_OR_NULL(c))
		return NULL;

	spin_lock(&clk_reset_lock);
	list_for_each_entry(map, &clk_reset_list, list) {
		if (map->clk == c || clk_is_match(map->clk, c)) {
			spin_unlock(&clk_reset_lock);
			return map->reset;
		}
	}

	spin_unlock(&clk_reset_lock);
	return NULL;
}
EXPORT_SYMBOL(tegra_clk_to_reset);

/* if we fail in our internal functions, raise a bug for the moment */
static struct reset_control *tegra_clk_to_reset1(struct clk *c)
{
	struct reset_control *reset = tegra_clk_to_reset(c);

	if (!reset) {
		pr_err("%s: failed to map %p (%s)\n", __func__, c,
		       !IS_ERR_OR_NULL(c) ? __clk_get_name(c) : "bad clock");
		BUG_ON(!reset);
	}

	return reset;
}

void tegra_compat_add_clk_reset(struct clk *c, struct reset_control *reset)
{
	struct reset_map *map;

	map = kcalloc(1, sizeof(struct reset_map), GFP_KERNEL);
	if (!map) {
		pr_err("%s: no memory for clock->reset\n", __func__);
		return;
	}

	map->clk = c;
	map->reset = reset;

	pr_debug("%s: map %p to %p\n", __func__, map->clk, map->reset);

	spin_lock(&clk_reset_lock);
	list_add(&map->list, &clk_reset_list);
	spin_unlock(&clk_reset_lock);
}

void tegra_periph_reset_deassert(struct clk *c)
{
	struct reset_control *reset = tegra_clk_to_reset1(c);

	if (reset) {
		__debug("%s: reset %s\n", __func__, __clk_get_name(c));
		reset_control_deassert(reset);
	}
}
EXPORT_SYMBOL(tegra_periph_reset_deassert);

void tegra_periph_reset_assert(struct clk *c)
{
	struct reset_control *reset = tegra_clk_to_reset1(c);

	if (reset) {
		__debug("%s: reset %s\n", __func__, __clk_get_name(c));
		reset_control_assert(reset);
	}
}
EXPORT_SYMBOL(tegra_periph_reset_assert);


static int tegra_reset_debugfs_dump(struct seq_file *s)
{
	struct reset_map *map;

	list_for_each_entry(map, &clk_reset_list, list) {
		seq_printf(s, "\t%s (%p) maps to %p\n",
			   __clk_get_name(map->clk), map->clk, map->reset);
	}
	return 0;
}

static int __init tegra_reset_debugfs(void)
{
	return tegra26_debugfs_add_ro("resets", tegra_reset_debugfs_dump);
}

device_initcall(tegra_reset_debugfs);
