/*
 * mods_clock.c - This file is part of NVIDIA MODS kernel driver.
 *
 * Copyright (c) 2011-2015, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA MODS kernel driver is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * NVIDIA MODS kernel driver is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NVIDIA MODS kernel driver.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include "mods_internal.h"
#include <linux/clk.h>
#include <linux/platform/tegra/clock.h>
#ifdef CONFIG_TEGRA_CLK_FRAMEWORK
	#include <mach/clk.h>
#elif defined(CONFIG_COMMON_CLK) && defined(CONFIG_OF_RESOLVE) && \
defined(CONFIG_OF_DYNAMIC)
	#include "mods_clock.h"
	#include <linux/clk-provider.h>
	#include <linux/clk-private.h>
	#include <linux/of_device.h>
	#include <linux/of.h>
	#include <linux/of_fdt.h>
	#include <linux/reset.h>
	#define ARBITRARY_MAX_CLK_FREQ  3500000000
#endif

static struct list_head mods_clock_handles;
static spinlock_t mods_clock_lock;
static u32 last_handle;

struct clock_entry {
	struct clk *pclk;
	u32 handle;
	struct list_head list;
};

#if defined(CONFIG_COMMON_CLK) && defined(CONFIG_OF_RESOLVE) && \
defined(CONFIG_OF_DYNAMIC)
static struct property clock_names_prop = {
	.name = "clock-names",
};

static struct property clocks_prop = {
	.name = "clocks",
};

static struct property reset_names_prop = {
	.name = "reset-names",
};

static struct property resets_prop = {
	.name = "resets",
};

static struct of_device_id tegra_car_of_match[] = {
	{ .compatible = "nvidia,tegra124-car", },
	{ .compatible = "nvidia,tegra210-car", },
	{ .compatible = "nvidia,tegra18x-car", },
	{},
};

static struct device_node *find_node(const char *compatible)
{
	struct device_node *np = NULL;
	struct of_device_id matches = {};

	strncpy(matches.compatible, compatible,
			sizeof(matches.compatible));

	np = of_find_matching_node(NULL, &matches);

	return np;
}

static void mods_update_node_properties(struct device_node *np,
		struct device_node *dup)
{
	struct property *prop;
	struct device_node *child;

	for_each_property_of_node(np, prop)
		of_add_property(dup, prop);

	for_each_child_of_node(np, child)
		child->parent = dup;
}

static void mods_attach_node_and_children(struct device_node *np)
{
	struct device_node *next, *dup, *child;

	dup = of_find_node_by_path(np->full_name);
	if (dup) {
		mods_debug_printk(DEBUG_CLOCK, "Duplicate 'mods' node found in "
				  "the device tree!!\n");
		mods_update_node_properties(np, dup);
		return;
	}

	child = np->child;
	np->child = NULL;
	np->sibling = NULL;
	of_attach_node(np);
	while (child) {
		next = child->sibling;
		mods_attach_node_and_children(child);
		child = next;
	}
}

static void mods_detach_node_and_children(struct device_node *np)
{
	while (np->child)
		mods_detach_node_and_children(np->child);
	of_detach_node(np);
}

/**
 * mods_attach_node_data - Reads, copies data from mods dtb node compiled
 * in the kernel image and attaches it to live dtb tree.
 * */
static void mods_attach_node_data(void)
{
	void *modsnode_data;
	struct device_node *modsnode_data_np, *np;
	int rc;
	/*
	 *  __dtb_mods_begin[] and __dtb_mods_end[] are magically
	 * created by cmd_dt_S_dtb in kernel/scripts/Makefile.lib
	 * */
	extern uint8_t __dtb_mods_begin[];
	extern uint8_t __dtb_mods_end[];
	const int size = __dtb_mods_end - __dtb_mods_begin;

	if (!size) {
		mods_error_printk("mods.dtb is not compiled in kernel image; "
				  "Can't create 'mods' node in device tree; "
				  "and can't init clocks\n");
		return;
	}

	modsnode_data = kmemdup(__dtb_mods_begin, size, GFP_KERNEL);
	if (!modsnode_data) {
		mods_error_printk("Failed to alloate memory for 'mods' node; "
				  "Can't init clocks\n");
		return;
	}

	/* unflatten the modsnode_data blob into device node structure */
	of_fdt_unflatten_tree(modsnode_data, &modsnode_data_np);
	if (!modsnode_data_np) {
		mods_error_printk("'mods' node can't be interpretted as a "
				  "device_node. Can't init clocks\n");
		return;
	}

	of_node_set_flag(modsnode_data_np, OF_DETACHED);
	rc = of_resolve_phandles(modsnode_data_np);
	if (rc) {
		mods_error_printk("Failed to resolve phandles (rc=%i); "
				  "Can't init clocks\n", rc);
		return;
	}

	/* attach the "modsnode" node to live tree */
	np = modsnode_data_np->child;
	while (np) {
		struct device_node *next = np->sibling;

		np->parent = of_allnodes;
		mods_attach_node_and_children(np);
		np = next;
	}
}

/* detach the mods node from the live dtb tree
 * when mods driver shuts down
 * */
static void mods_remove_node_data(void)
{
	struct property *prop;
	struct device_node *mods_np = find_node("nvidia,tegra-mods");

	for_each_property_of_node(mods_np, prop)
		of_remove_property(mods_np, prop);

	mods_detach_node_and_children(mods_np);
}
#endif  /* CONFIG_COMMON_CLOCK */

void mods_init_clock_api(void)
{
	spin_lock_init(&mods_clock_lock);
	INIT_LIST_HEAD(&mods_clock_handles);
	last_handle = 0;

#if defined(CONFIG_COMMON_CLK) && defined(CONFIG_OF_RESOLVE) && \
defined(CONFIG_OF_DYNAMIC)
	struct device_node *mods_np = 0;
	struct device_node *car_np = 0;
	void *clk_names = 0;
	void *clk_tuples = 0;
	int index, size, pos, num_clocks = 0;

	/* add mods node to kernel's live device tree*/
	mods_attach_node_data();

	mods_np = find_node("nvidia,tegra-mods");
	if (!mods_np || !of_device_is_available(mods_np)) {
		mods_error_printk("'mods' node not found in the device tree. "
				  "Can't init clocks\n");
		goto err;
	}

	car_np = of_find_matching_node(NULL, tegra_car_of_match);
	if (!car_np || !of_device_is_available(car_np)) {
		mods_error_printk("'tegra_car' node not found in device tree. "
				  "Can't init clocks\n");
		goto err;
	}

	num_clocks = sizeof(clock_list)/sizeof(clock_list[0]);
	/* update tegra_car phandle values in all clocks */
	for (index = 0; index < num_clocks; ++index) {
		clock_list[index].phandle = cpu_to_be32(car_np->phandle);
		clock_list[index].clk_uid = cpu_to_be32(clock_list[index].clk_uid);
		clock_list[index].rst_uid = cpu_to_be32(clock_list[index].rst_uid);
	}

	size = num_clocks * 2 * sizeof(u32);
	clk_tuples = kmalloc(size, GFP_ATOMIC);
	if (IS_ERR(clk_tuples))
		mods_error_printk("Not enough memory. Can't init clocks\n");
	memset(clk_tuples, 0, size);
	for (index = 0, pos = 0; index < num_clocks; ++index) {
		memmove(clk_tuples + pos, &clock_list[index].phandle,
			sizeof(u32));
		pos += sizeof(u32);
		memmove(clk_tuples + pos, &clock_list[index].clk_uid,
			sizeof(u32));
		pos += sizeof(u32);
	}

	clocks_prop.value = clk_tuples;
	clocks_prop.length = size;
	if (of_add_property(mods_np, &clocks_prop) < 0) {
		mods_error_printk("Couldn't add 'clocks' prop to 'mods' node "
				  "in device tree. Can't init clocks\n");
		goto err;
	}

	for (index = 0, size = 0; index < num_clocks; ++index)
		size += strlen(clock_list[index].name) + 1;

	clk_names = kmalloc(size, GFP_ATOMIC);
	if (IS_ERR(clk_names))
		mods_error_printk("Not enough memory. Can't init clocks\n");
	memset(clk_names, '\0', size);
	for (index = 0, pos = 0; index < num_clocks; ++index) {
		memmove(clk_names + pos, clock_list[index].name,
			strlen(clock_list[index].name) + 1);
		pos += strlen(clock_list[index].name) + 1;
	}

	clock_names_prop.value = clk_names;
	clock_names_prop.length = size;
	if (of_add_property(mods_np, &clock_names_prop) < 0) {
		mods_error_printk("Could'nt add 'clock-names' prop to 'mods' "
				  "node. Can't init clocks\n");
		goto err;
	}

#ifdef CONFIG_RESET_CONTROLLER
	void *rst_names;
	void *rst_tuples;
	u32 num_resets = 0;

	for (index = 0; index < num_clocks; ++index)
		if (clock_list[index].rst_uid != -1)
			++num_resets;

	size = num_resets * 2 * sizeof(u32);
	rst_tuples = kmalloc(size, GFP_ATOMIC);
	if (IS_ERR(rst_tuples))
		mods_error_printk("Not enough memory. Can't init reset devs\n");
	memset(rst_tuples, 0, size);
	for (index = 0, pos = 0; index < num_clocks; ++index) {
		if (clock_list[index].rst_uid != -1) {
			memmove(rst_tuples + pos, &clock_list[index].phandle,
				sizeof(u32));
			pos += sizeof(u32);
			memmove(rst_tuples + pos, &clock_list[index].rst_uid,
				sizeof(u32));
			pos += sizeof(u32);
		}
	}

	resets_prop.value = rst_tuples;
	resets_prop.length = size;
	if (of_add_property(mods_np, &resets_prop) < 0) {
		mods_error_printk("Couldn't add 'resets' prop to 'mods' node "
				  "in device tree. Can't init clocks\n");
		goto err;
	}

	for (index = 0, size = 0; index < num_clocks; ++index)
		if (clock_list[index].rst_uid != -1)
			size += strlen(clock_list[index].name) + 1;

	rst_names = kmalloc(size, GFP_ATOMIC);
	if (IS_ERR(rst_names))
		mods_error_printk("Not enough memory. Can't init reset devs\n");
	memset(rst_names, '\0', size);
	for (index = 0, pos = 0; index < num_clocks; ++index) {
		if (clock_list[index].rst_uid != -1) {
			memmove(rst_names + pos, clock_list[index].name,
				strlen(clock_list[index].name) + 1);
			pos += strlen(clock_list[index].name) + 1;
		}
	}

	reset_names_prop.value = rst_names;
	reset_names_prop.length = size;
	if (of_add_property(mods_np, &reset_names_prop) < 0) {
		mods_error_printk("Could'nt add 'reset-names' prop to 'mods' "
				  "node. Can't init clocks\n");
		goto err;
	}
#endif

err:
	of_node_put(mods_np);
	of_node_put(car_np);
	return;
#endif /* CONFIG_COMMON_CLOCK */
}

void mods_shutdown_clock_api(void)
{
	struct list_head *head = &mods_clock_handles;
	struct list_head *iter;
	struct list_head *tmp;

	spin_lock(&mods_clock_lock);

	list_for_each_safe(iter, tmp, head) {
		struct clock_entry *entry
			= list_entry(iter, struct clock_entry, list);
		list_del(iter);
		kfree(entry);
	}

#if defined(CONFIG_COMMON_CLK) && defined(CONFIG_OF_RESOLVE) && \
defined(CONFIG_OF_DYNAMIC)
	mods_remove_node_data();
#endif /* CONFIG_COMMON_CLOCK */

	spin_unlock(&mods_clock_lock);
}

static u32 mods_get_clock_handle(struct clk *pclk)
{
	struct list_head *head = &mods_clock_handles;
	struct list_head *iter;
	struct clock_entry *entry = 0;
	u32 handle = 0;

	spin_lock(&mods_clock_lock);

	list_for_each(iter, head) {
		struct clock_entry *cur
			= list_entry(iter, struct clock_entry, list);
		if (cur->pclk == pclk) {
			entry = cur;
			handle = cur->handle;
			break;
		}
	}

	if (!entry) {
		entry = kmalloc(sizeof(*entry), GFP_ATOMIC);
		if (!unlikely(!entry)) {
			entry->pclk = pclk;
			entry->handle = ++last_handle;
			handle = entry->handle;
			list_add(&entry->list, &mods_clock_handles);
		}
	}

	spin_unlock(&mods_clock_lock);

	return handle;
}

static struct clk *mods_get_clock(u32 handle)
{
	struct list_head *head = &mods_clock_handles;
	struct list_head *iter;
	struct clk *pclk = 0;

	spin_lock(&mods_clock_lock);

	list_for_each(iter, head) {
		struct clock_entry *entry
			= list_entry(iter, struct clock_entry, list);
		if (entry->handle == handle) {
			pclk = entry->pclk;
			break;
		}
	}

	spin_unlock(&mods_clock_lock);

	return pclk;
}

int esc_mods_get_clock_handle(struct file *pfile,
			      struct MODS_GET_CLOCK_HANDLE *p)
{
	struct clk *pclk = 0;
	int ret = -EINVAL;

	LOG_ENT();

#ifdef CONFIG_TEGRA_CLK_FRAMEWORK
	p->device_name[sizeof(p->device_name)-1] = 0;
	p->controller_name[sizeof(p->controller_name)-1] = 0;
	pclk = clk_get_sys(p->device_name, p->controller_name);

	if (IS_ERR(pclk)) {
		mods_error_printk("invalid clock specified: dev=%s, ctx=%s\n",
				  p->device_name, p->controller_name);
	} else {
		p->clock_handle = mods_get_clock_handle(pclk);
		ret = OK;
	}
#elif defined(CONFIG_COMMON_CLK) && defined(CONFIG_OF_RESOLVE) && \
defined(CONFIG_OF_DYNAMIC)
	struct device_node *mods_np = 0;
	struct property *pp = 0;
	u32 index, size = 0;

	mods_np = find_node("nvidia,tegra-mods");
	if (!mods_np || !of_device_is_available(mods_np)) {
		mods_error_printk("'mods' node not found in device tree\n");
		goto err;
	}
	pp = of_find_property(mods_np, "clock-names", NULL);
	if (IS_ERR(pp)) {
		mods_error_printk("'clock-names' prop not found in 'mods' node."
				  " Can't get clock handle for %s\n",
				  p->controller_name);
		goto err;
	}

	pclk = of_clk_get_by_name(mods_np, p->controller_name);

	if (IS_ERR(pclk))
		mods_error_printk("clk (%s) not found\n", p->controller_name);
	else {
		p->clock_handle = mods_get_clock_handle(pclk);
		size = sizeof(clock_list)/sizeof(clock_list[0]);
		for (index = 0; index < size; ++index)
			if (!strcmp(clock_list[index].name, p->controller_name))
				clock_list[index].mhandle = p->clock_handle;
		ret = OK;
	}
err:
	of_node_put(mods_np);
#endif
	LOG_EXT();
	return ret;
}

int esc_mods_set_clock_rate(struct file *pfile, struct MODS_CLOCK_RATE *p)
{
	struct clk *pclk = 0;
	int ret = -EINVAL;

	LOG_ENT();

	pclk = mods_get_clock(p->clock_handle);

	if (!pclk) {
		mods_error_printk("unrecognized clock handle: 0x%x\n",
				  p->clock_handle);
	} else {
		ret = clk_set_rate(pclk, p->clock_rate_hz);
		if (ret) {
			mods_error_printk(
				"unable to set rate %lluHz on clock 0x%x\n",
				p->clock_rate_hz, p->clock_handle);
		} else {
			mods_debug_printk(DEBUG_CLOCK,
				  "successfuly set rate %lluHz on clock 0x%x\n",
				  p->clock_rate_hz, p->clock_handle);
		}
	}

	LOG_EXT();
	return ret;
}

int esc_mods_get_clock_rate(struct file *pfile, struct MODS_CLOCK_RATE *p)
{
	struct clk *pclk = 0;
	int ret = -EINVAL;

	LOG_ENT();

	pclk = mods_get_clock(p->clock_handle);

	if (!pclk) {
		mods_error_printk("unrecognized clock handle: 0x%x\n",
				  p->clock_handle);
	} else {
		p->clock_rate_hz = clk_get_rate(pclk);
		mods_debug_printk(DEBUG_CLOCK, "clock 0x%x has rate %lluHz\n",
				  p->clock_handle, p->clock_rate_hz);
		ret = OK;
	}

	LOG_EXT();
	return ret;
}

int esc_mods_get_clock_max_rate(struct file *pfile, struct MODS_CLOCK_RATE *p)
{
	struct clk *pclk = 0;
	int ret = -EINVAL;

	LOG_ENT();
	pclk = mods_get_clock(p->clock_handle);

	if (!pclk) {
		mods_error_printk("unrecognized clock handle: 0x%x\n",
				  p->clock_handle);
#ifdef CONFIG_TEGRA_CLK_FRAMEWORK
	} else if (!pclk->ops || !pclk->ops->round_rate) {
		mods_error_printk(
			"unable to detect max rate for clock handle 0x%x\n",
			p->clock_handle);
	} else {
		long rate = pclk->ops->round_rate(pclk, pclk->max_rate);
		p->clock_rate_hz = rate < 0 ? pclk->max_rate
					    : (unsigned long)rate;
#elif defined(CONFIG_COMMON_CLK) && defined(CONFIG_OF_RESOLVE) && \
defined(CONFIG_OF_DYNAMIC)
	} else {
		long rate = clk_round_rate(pclk, ARBITRARY_MAX_CLK_FREQ);
		p->clock_rate_hz = rate < 0 ? ARBITRARY_MAX_CLK_FREQ
			: (unsigned long)rate;
#endif
		mods_debug_printk(DEBUG_CLOCK,
				  "clock 0x%x has max rate %lluHz\n",
				  p->clock_handle, p->clock_rate_hz);
		ret = OK;
	}

	LOG_EXT();
	return ret;
}

int esc_mods_set_clock_max_rate(struct file *pfile, struct MODS_CLOCK_RATE *p)
{
	struct clk *pclk = 0;
	int ret = -EINVAL;

	LOG_ENT();

	pclk = mods_get_clock(p->clock_handle);

	if (!pclk) {
		mods_error_printk("unrecognized clock handle: 0x%x\n",
				  p->clock_handle);
	} else {
#ifdef CONFIG_TEGRA_CLOCK_DEBUG_FUNC
		ret = tegra_clk_set_max(pclk, p->clock_rate_hz);
		if (ret) {
			mods_error_printk(
		"unable to override max clock rate %lluHz on clock 0x%x\n",
					  p->clock_rate_hz, p->clock_handle);
		} else {
			mods_debug_printk(DEBUG_CLOCK,
			  "successfuly set max rate %lluHz on clock 0x%x\n",
					  p->clock_rate_hz, p->clock_handle);
		}
#else
		mods_error_printk("unable to override max clock rate\n");
		mods_error_printk(
		"reconfigure kernel with CONFIG_TEGRA_CLOCK_DEBUG_FUNC=y\n");
		ret = -ENOSYS;
#endif
	}

	LOG_EXT();
	return ret;
}

int esc_mods_set_clock_parent(struct file *pfile, struct MODS_CLOCK_PARENT *p)
{
	struct clk *pclk = 0;
	struct clk *pparent = 0;
	int ret = -EINVAL;

	LOG_ENT();

	pclk = mods_get_clock(p->clock_handle);
	pparent = mods_get_clock(p->clock_parent_handle);

	if (!pclk) {
		mods_error_printk("unrecognized clock handle: 0x%x\n",
				  p->clock_handle);
	} else if (!pparent) {
		mods_error_printk("unrecognized parent clock handle: 0x%x\n",
				  p->clock_parent_handle);
	} else {
		ret = clk_set_parent(pclk, pparent);
		if (ret) {
			mods_error_printk(
			    "unable to make clock 0x%x parent of clock 0x%x\n",
			    p->clock_parent_handle, p->clock_handle);
		} else {
			mods_debug_printk(DEBUG_CLOCK,
			  "successfuly made clock 0x%x parent of clock 0x%x\n",
			  p->clock_parent_handle, p->clock_handle);
		}
	}

	LOG_EXT();
	return ret;
}

int esc_mods_get_clock_parent(struct file *pfile, struct MODS_CLOCK_PARENT *p)
{
	struct clk *pclk = 0;
	int ret = -EINVAL;

	LOG_ENT();

	pclk = mods_get_clock(p->clock_handle);

	if (!pclk) {
		mods_error_printk("unrecognized clock handle: 0x%x\n",
				  p->clock_handle);
	} else {
		struct clk *pparent = clk_get_parent(pclk);
		p->clock_parent_handle = mods_get_clock_handle(pparent);
		mods_debug_printk(DEBUG_CLOCK,
				  "clock 0x%x is parent of clock 0x%x\n",
				  p->clock_parent_handle, p->clock_handle);
		ret = OK;
	}

	LOG_EXT();
	return ret;
}

int esc_mods_enable_clock(struct file *pfile, struct MODS_CLOCK_HANDLE *p)
{
	struct clk *pclk = 0;
	int ret = -EINVAL;

	LOG_ENT();

	pclk = mods_get_clock(p->clock_handle);

	if (!pclk) {
		mods_error_printk("unrecognized clock handle: 0x%x\n",
				  p->clock_handle);
	} else {
#if defined(CONFIG_COMMON_CLK) && defined(CONFIG_OF_RESOLVE) && \
defined(CONFIG_OF_DYNAMIC)
		ret = clk_prepare(pclk);
		if (ret) {
			mods_error_printk("unable to prepare clock 0x%x before "
					  "enabling\n", p->clock_handle);
		}
#endif
		ret = clk_enable(pclk);
		if (ret) {
			mods_error_printk("failed to enable clock 0x%x\n",
					  p->clock_handle);
		} else {
			mods_debug_printk(DEBUG_CLOCK, "clock 0x%x enabled\n",
					  p->clock_handle);
		}
	}

	LOG_EXT();
	return ret;
}

int esc_mods_disable_clock(struct file *pfile, struct MODS_CLOCK_HANDLE *p)
{
	struct clk *pclk = 0;
	int ret = -EINVAL;

	LOG_ENT();

	pclk = mods_get_clock(p->clock_handle);

	if (!pclk) {
		mods_error_printk("unrecognized clock handle: 0x%x\n",
				  p->clock_handle);
	} else {
		clk_disable(pclk);
#if defined(CONFIG_COMMON_CLK) && defined(CONFIG_OF_RESOLVE) && \
defined(CONFIG_OF_DYNAMIC)
		clk_unprepare(pclk);
#endif
		mods_debug_printk(DEBUG_CLOCK, "clock 0x%x disabled\n",
				  p->clock_handle);
		ret = OK;
	}

	LOG_EXT();
	return ret;
}

int esc_mods_is_clock_enabled(struct file *pfile, struct MODS_CLOCK_ENABLED *p)
{
	struct clk *pclk = 0;
	int ret = -EINVAL;

	LOG_ENT();

	pclk = mods_get_clock(p->clock_handle);

	if (!pclk) {
		mods_error_printk("unrecognized clock handle: 0x%x\n",
				  p->clock_handle);
	} else {
#ifdef CONFIG_TEGRA_CLK_FRAMEWORK
		p->enable_count = pclk->refcnt;
#elif defined(CONFIG_COMMON_CLK) && defined(CONFIG_OF_RESOLVE) && \
defined(CONFIG_OF_DYNAMIC)
		p->enable_count = pclk->enable_count;
#endif
		mods_debug_printk(DEBUG_CLOCK, "clock 0x%x enable count is %u\n",
				  p->clock_handle, p->enable_count);
		ret = OK;
	}

	LOG_EXT();
	return ret;
}

int esc_mods_clock_reset_assert(struct file *pfile,
				struct MODS_CLOCK_HANDLE *p)
{
	struct clk *pclk = 0;
	int ret = -EINVAL;

	LOG_ENT();

	pclk = mods_get_clock(p->clock_handle);

	if (!pclk) {
		mods_error_printk("unrecognized clock handle: 0x%x\n",
				  p->clock_handle);
	} else {
#ifdef CONFIG_TEGRA_CLK_FRAMEWORK
		tegra_periph_reset_assert(pclk);
		mods_debug_printk(DEBUG_CLOCK, "clock 0x%x reset asserted\n",
				  p->clock_handle);
		ret = OK;
#elif defined(CONFIG_COMMON_CLK) && defined(CONFIG_OF_RESOLVE) && \
defined(CONFIG_OF_DYNAMIC) && defined(CONFIG_RESET_CONTROLLER)
		struct reset_control *prst = 0;
		struct device_node *mods_np = 0;
		struct property *pp = 0;
		u32 index, size = 0;

		mods_np = find_node("nvidia,tegra-mods");
		if (!mods_np || !of_device_is_available(mods_np)) {
			mods_error_printk("'mods' node not found in DTB\n");
			goto err;
		}
		pp = of_find_property(mods_np, "reset-names", NULL);
		if (IS_ERR(pp)) {
			mods_error_printk("'reset-names' prop not found in 'mods' node."
					  " Can't get handle for reset dev %s\n",
					  pclk->name);
			goto err;
		}

		size = sizeof(clock_list)/sizeof(clock_list[0]);
		for (index = 0; index < size; ++index)
			if (clock_list[index].mhandle == p->clock_handle)
				break;
		if (index == size) {
			for (index = 0; index < size; ++index) {
				if (!strcmp(clock_list[index].name, pclk->name)) {
					clock_list[index].mhandle =
						p->clock_handle;
					break;
				}
			}
		}

		prst = of_reset_control_get(mods_np, clock_list[index].name);
		if (IS_ERR(prst)) {
			mods_error_printk("reset device %s not found. Failed "
					  "to reset\n", clock_list[index].name);
			goto err;
		}
		ret = reset_control_assert(prst);
		if (ret) {
			mods_error_printk("failed to assert reset on '%s'\n",
					  clock_list[index].name);
		} else {
			mods_debug_printk(DEBUG_CLOCK, "asserted reset on '%s'",
					  clock_list[index].name);
		}

err:
		of_node_put(mods_np);
#endif
	}
	LOG_EXT();
	return ret;
}

int esc_mods_clock_reset_deassert(struct file *pfile,
				  struct MODS_CLOCK_HANDLE *p)
{
	struct clk *pclk = 0;
	int ret = -EINVAL;

	LOG_ENT();

	pclk = mods_get_clock(p->clock_handle);

	if (!pclk) {
		mods_error_printk("unrecognized clock handle: 0x%x\n",
				  p->clock_handle);
	} else {
#ifdef CONFIG_TEGRA_CLK_FRAMEWORK
		tegra_periph_reset_deassert(pclk);
		mods_debug_printk(DEBUG_CLOCK, "clock 0x%x reset deasserted\n",
				  p->clock_handle);
		ret = OK;
#elif defined(CONFIG_COMMON_CLK) && defined(CONFIG_OF_RESOLVE) && \
defined(CONFIG_OF_DYNAMIC) && defined(CONFIG_RESET_CONTROLLER)
		struct reset_control *prst = 0;
		struct device_node *mods_np = 0;
		struct property *pp = 0;
		u32 index, size = 0;

		mods_np = find_node("nvidia,tegra-mods");
		if (!mods_np || !of_device_is_available(mods_np)) {
			mods_error_printk("'mods' node not found in DTB\n");
			goto err;
		}
		pp = of_find_property(mods_np, "reset-names", NULL);
		if (IS_ERR(pp)) {
			mods_error_printk("'reset-names' prop not found in 'mods' node."
					  " Can't get handle for reset dev %s\n",
					  pclk->name);
			goto err;
		}

		size = sizeof(clock_list)/sizeof(clock_list[0]);
		for (index = 0; index < size; ++index)
			if (clock_list[index].mhandle == p->clock_handle)
				break;
		if (index == size) {
			for (index = 0; index < size; ++index) {
				if (!strcmp(clock_list[index].name, pclk->name)) {
					clock_list[index].mhandle =
						p->clock_handle;
					break;
				}
			}
		}

		prst = of_reset_control_get(mods_np, clock_list[index].name);
		if (IS_ERR(prst)) {
			mods_error_printk("reset device %s not found. Failed "
					  "to reset\n", clock_list[index].name);
			goto err;
		}
		ret = reset_control_deassert(prst);
		if (ret) {
			mods_error_printk("failed to assert reset on '%s'\n",
					  clock_list[index].name);
		} else {
			mods_debug_printk(DEBUG_CLOCK, "deasserted reset on '%s'",
					  clock_list[index].name);
		}

err:
		of_node_put(mods_np);
#endif
	}

	LOG_EXT();
	return ret;
}
