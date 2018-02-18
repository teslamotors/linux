/*
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/io.h>
#include <linux/tegra_prod.h>
#include <linux/kmemleak.h>

#define PROD_TUPLE_NUM (sizeof(struct prod_tuple)/sizeof(u32))

/* tegra_prod_list: Tegra Prod list for the given submodule
 * @n_prod_cells: Number of prod setting cells.
 * @mask_ones:  Mask value type. if it is true than value applied for those
 *		bits whose mask bits are 1s. If it false then value applies
 *		to those bits whose mask bits are 0.
 */
struct tegra_prod_list {
	struct tegra_prod_config *prod_config;
	int num; /* number of tegra_prod*/
	int n_prod_cells;
	bool mask_ones;
};

struct prod_tuple {
	u32 index; /* Address base index */
	u32 addr;  /* offset address*/
	u32 mask;  /* mask */
	u32 val;   /* value */
};

struct tegra_prod_config {
	const char *name;
	struct prod_tuple *prod_tuple;
	int count; /* number of prod_tuple*/
	bool boot_init;
};

static int tegra_prod_get_child_tupple_count(const struct device_node *np,
		int n_tupple)
{
	struct device_node *child;
	int count;
	int total_tupple = 0;

	count = of_property_count_u32(np, "prod");
	if (count > 0) {
		if ((count < n_tupple) || (count % n_tupple != 0)) {
			pr_err("Node %s: Not found proper setting in %s\n",
				np->name, np->name);
			return -EINVAL;
		}
		total_tupple = count / n_tupple;
	}

	for_each_child_of_node(np, child) {
		/* Check whether child is enabled or not */
		if (!of_device_is_available(child))
			continue;

		count = of_property_count_u32(child, "prod");
		if (count < 0) {
			pr_err("Node %s: prod prop not found\n", child->name);
			continue;
		}

		if ((count < n_tupple) || (count % n_tupple != 0)) {
			pr_err("Node %s: Not found proper setting in %s\n",
				child->name, np->name);
			return -EINVAL;
		}
		total_tupple += count / n_tupple;
	}
	return total_tupple;
}

static int tegra_prod_read_prod_data(const struct device_node *np,
		struct prod_tuple *p_tuple, int n_tupple)
{
	u32 pval;
	int count;
	int t_count;
	int cnt;
	int index;
	int ret;

	if (!of_device_is_available(np))
		return 0;

	count = of_property_count_u32(np, "prod");
	if (count <= 0) {
		pr_debug("Node %s: prod prop not found\n", np->name);
		return 0;
	}

	t_count = count / n_tupple;
	for (cnt = 0; cnt < t_count; cnt++, p_tuple++) {
		index = cnt * n_tupple;

		if (n_tupple == 4) {
			ret = of_property_read_u32_index(np, "prod", index,
						&pval);
			if (ret) {
				pr_err("Node %s: Failed to parse index\n",
					np->name);
				return -EINVAL;
			}
			p_tuple->index = pval;
			index++;
		} else {
			p_tuple->index = 0;
		}

		ret = of_property_read_u32_index(np, "prod", index, &pval);
		if (ret) {
			pr_err("Node %s: Failed to parse address\n", np->name);
			return -EINVAL;
		}
		p_tuple->addr = pval;
		index++;

		ret = of_property_read_u32_index(np, "prod", index, &pval);
		if (ret) {
			pr_err("Node %s: Failed to parse mask\n", np->name);
			return -EINVAL;
		}
		p_tuple->mask = pval;
		index++;

		ret = of_property_read_u32_index(np, "prod", index, &pval);
		if (ret) {
			pr_err("Node %s: Failed to parse value\n", np->name);
			return -EINVAL;
		}
		p_tuple->val = pval;
	}

	return t_count;
}

static int tegra_prod_read_node_tupple(const struct device_node *np,
		struct tegra_prod_config *t_prod, int n_tupple)
{
	int ret = 0;
	int sindex;
	struct prod_tuple *p_tuple;
	struct device_node *child;

	p_tuple = (struct prod_tuple *)&t_prod->prod_tuple[0];

	ret = tegra_prod_read_prod_data(np, p_tuple, n_tupple);
	if (ret < 0)
		return -EINVAL;

	sindex = ret;
	p_tuple += ret;

	for_each_child_of_node(np, child) {
		ret = tegra_prod_read_prod_data(child, p_tuple, n_tupple);
		if (ret < 0)
			return -EINVAL;
		sindex += ret;
		p_tuple += ret;
	}
	return sindex;
}

/**
 * tegra_prod_parse_dt - Read the prod setting form Device tree.
 * @np:			device node from which the property value is to be read.
 * @np_prod:		Prod setting node.
 * @tegra_prod_list:	The list of tegra prods.
 *
 * Read the prod setting form DT according the prod name in tegra prod list.
 * prod tuple will be allocated dynamically according to the tuple number of
 * each prod in DT.
 *
 * Returns 0 on success.
 */

static int tegra_prod_parse_dt(const struct device_node *np,
		const struct device_node *np_prod,
		struct tegra_prod_list *tegra_prod_list)
{
	struct device_node *child;
	struct tegra_prod_config *t_prod;
	struct prod_tuple *p_tuple;
	int n_child, j;
	int n_tupple = 3;
	int ret;
	int count;
	u32 pval;

	if (!tegra_prod_list || !tegra_prod_list->prod_config) {
		pr_err("Node %s: Invalid tegra prods list.\n", np->name);
		return -EINVAL;
	};

	ret = of_property_read_u32(np_prod, "#prod-cells", &pval);
	if (!ret)
		n_tupple = pval;
	if ((n_tupple != 3) && (n_tupple != 4)) {
		pr_err("Node %s: Prod cells not supported\n", np->name);
		return -EINVAL;
	}
	tegra_prod_list->n_prod_cells = n_tupple;

	tegra_prod_list->mask_ones = of_property_read_bool(np_prod,
							   "mask-one-style");

	n_child = 0;
	for_each_child_of_node(np_prod, child) {
		/* Check whether child is enabled or not */
		if (!of_device_is_available(child))
			continue;

		t_prod = &tegra_prod_list->prod_config[n_child];
		t_prod->name = child->name;

		count = tegra_prod_get_child_tupple_count(child, n_tupple);
		if (count < 0) {
			pr_err("Node %s: Child has not proper setting\n",
				child->name);
			ret = -EINVAL;
			goto err_parsing;
		}

		if (!count) {
			pr_err("Node %s: prod prop not found\n", child->name);
			ret = -EINVAL;
			goto err_parsing;
		}

		t_prod->count = count;

		t_prod->prod_tuple = kzalloc(sizeof(*p_tuple) * t_prod->count,
						GFP_KERNEL);
		if (!t_prod->prod_tuple) {
			ret = -ENOMEM;
			goto err_parsing;
		}
		kmemleak_not_leak(t_prod->prod_tuple);

		t_prod->boot_init = of_property_read_bool(child,
						"nvidia,prod-boot-init");

		ret = tegra_prod_read_node_tupple(child, t_prod, n_tupple);
		if (ret < 0) {
			pr_err("Node %s: Reading prod setting failed: %d\n",
				child->name, ret);
			ret = -EINVAL;
			goto err_parsing;
		}
		if (t_prod->count != ret) {
			pr_err("Node %s: prod read failed: exp %d read %d\n",
				child->name, t_prod->count, ret);
			ret = -EINVAL;
			goto err_parsing;
		}
		n_child++;
	}

	tegra_prod_list->num = n_child;
	of_node_put(child);
	return 0;

err_parsing:
	of_node_put(child);
	for (j = 0; j <= n_child; j++) {
		t_prod = (struct tegra_prod_config *)&tegra_prod_list->prod_config[j];
		kfree(t_prod->prod_tuple);
	}
	return ret;
}

/**
 * tegra_prod_set_tuple - Only set a tuple.
 * @base:		base address of the register.
 * @prod_tuple:		the tuple to set.
 *
 * Returns 0 on success.
 */
static int tegra_prod_set_tuple(void __iomem **base,
				struct prod_tuple *prod_tuple,
				bool mask_ones)
{
	u32 reg;

	if (!prod_tuple)
		return -EINVAL;

	reg = readl(base[prod_tuple->index] + prod_tuple->addr);
	if (mask_ones)
		reg = ((reg & ~prod_tuple->mask) |
			(prod_tuple->val & prod_tuple->mask));
	else
		reg = ((reg & prod_tuple->mask) |
			(prod_tuple->val & ~prod_tuple->mask));

	writel(reg, base[prod_tuple->index] + prod_tuple->addr);

	return 0;
}

/**
 * tegra_prod_set - Set one prod setting.
 * @base:		base address of the register.
 * @tegra_prod:		the prod setting to set.
 *
 * Set all the tuples in one tegra_prod.
 * Returns 0 on success.
 */
static int tegra_prod_set(void __iomem **base,
			  struct tegra_prod_config *tegra_prod,
			  bool mask_ones)
{
	int i;
	int ret;

	if (!tegra_prod)
		return -EINVAL;

	for (i = 0; i < tegra_prod->count; i++) {
		ret = tegra_prod_set_tuple(base, &tegra_prod->prod_tuple[i],
					   mask_ones);
		if (ret)
			return ret;
	}

	return 0;
}

/**
 * tegra_prod_set_list - Set all the prod settings of the list in sequence.
 * @base:		base address of the register.
 * @tegra_prod_list:	the list of tegra prods.
 *
 * Returns 0 on success.
 */
int tegra_prod_set_list(void __iomem **base,
		struct tegra_prod_list *tegra_prod_list)
{
	int i;
	int ret;

	if (!tegra_prod_list)
		return -EINVAL;

	for (i = 0; i < tegra_prod_list->num; i++) {
		ret = tegra_prod_set(base, &tegra_prod_list->prod_config[i],
				     tegra_prod_list->mask_ones);
		if (ret)
			return ret;
	}

	return 0;
}
EXPORT_SYMBOL(tegra_prod_set_list);

/**
 * tegra_prod_set_boot_init - Set all the prod settings of the list in sequence
 *			Which are needed for boot initialisation.
 * @base:		base address of the register.
 * @tegra_prod_list:	the list of tegra prods.
 *
 * Returns 0 on success.
 */
int tegra_prod_set_boot_init(void __iomem **base,
		struct tegra_prod_list *tegra_prod_list)
{
	int i;
	int ret;

	if (!tegra_prod_list)
		return -EINVAL;

	for (i = 0; i < tegra_prod_list->num; i++) {
		if (!tegra_prod_list->prod_config[i].boot_init)
			continue;
		ret = tegra_prod_set(base, &tegra_prod_list->prod_config[i],
				     tegra_prod_list->mask_ones);
		if (ret)
			return ret;
	}

	return 0;
}
EXPORT_SYMBOL(tegra_prod_set_boot_init);

/**
 * tegra_prod_set_by_name - Set the prod setting according the name.
 * @base:		base address of the register.
 * @name:		the name of tegra prod need to set.
 * @tegra_prod_list:	the list of tegra prods.
 *
 * Find the tegra prod in the list according to the name. Then set
 * that tegra prod.
 *
 * Returns 0 on success.
 */
int tegra_prod_set_by_name(void __iomem **base, const char *name,
		struct tegra_prod_list *tegra_prod_list)
{
	int i;
	struct tegra_prod_config *t_prod;

	if (!tegra_prod_list)
		return -EINVAL;

	for (i = 0; i < tegra_prod_list->num; i++) {
		t_prod = &tegra_prod_list->prod_config[i];
		if (!t_prod)
			return -EINVAL;
		if (!strcmp(t_prod->name, name))
			return tegra_prod_set(base, t_prod,
					      tegra_prod_list->mask_ones);
	}

	return -ENODEV;
}
EXPORT_SYMBOL(tegra_prod_set_by_name);

bool tegra_prod_by_name_supported(struct tegra_prod_list *tegra_prod_list,
				  const char *name)
{
	int i;
	struct tegra_prod_config *t_prod;

	if (!tegra_prod_list)
		return false;

	for (i = 0; i < tegra_prod_list->num; i++) {
		t_prod = &tegra_prod_list->prod_config[i];
		if (!t_prod)
			break;

		if (!strcmp(t_prod->name, name))
			return true;
	}

	return false;
}
EXPORT_SYMBOL(tegra_prod_by_name_supported);

/**
 * tegra_prod_init - Init tegra prod list.
 * @np		device node from which the property value is to be read.
 *
 * Query all the prod settings under DT node & Init the tegra prod list
 * automatically.
 *
 * Returns 0 on success, -EINVAL for wrong prod number, -ENOMEM if faild
 * to allocate memory for tegra prod list.
 */
struct tegra_prod_list *tegra_prod_init(const struct device_node *np)
{
	struct tegra_prod_list *tegra_prod_list;
	struct device_node *np_prod;
	int prod_num = 0;
	int ret;

	np_prod = of_get_child_by_name(np, "prod-settings");
	if (!np_prod)
		return ERR_PTR(-ENODEV);

	/* Check whether child is enabled or not */
	if (!of_device_is_available(np_prod)) {
		pr_err("Node %s: Node is not enabled\n", np_prod->name);
		return ERR_PTR(-ENODEV);
	}

	prod_num = of_get_child_count(np_prod);
	if (prod_num <= 0) {
		pr_err("Node %s: No child node for prod settings\n",
			np_prod->name);
		return  ERR_PTR(-ENODEV);
	}

	tegra_prod_list = kzalloc(sizeof(*tegra_prod_list), GFP_KERNEL);
	if (!tegra_prod_list)
		return  ERR_PTR(-ENOMEM);

	kmemleak_not_leak(tegra_prod_list);

	tegra_prod_list->prod_config = kzalloc(prod_num *
				sizeof(struct tegra_prod_config), GFP_KERNEL);
	if (!tegra_prod_list->prod_config) {
		ret = -ENOMEM;
		goto err_prod_alloc;
	}
	kmemleak_not_leak(tegra_prod_list->prod_config);
	tegra_prod_list->num = prod_num;

	ret = tegra_prod_parse_dt(np, np_prod, tegra_prod_list);
	if (ret) {
		pr_err("Node %s: Faild to read the Prod Setting.\n", np->name);
		goto err_get;
	}

	of_node_put(np_prod);
	return tegra_prod_list;

err_get:
	kfree(tegra_prod_list->prod_config);
err_prod_alloc:
	kfree(tegra_prod_list);
	return ERR_PTR(ret);
}
EXPORT_SYMBOL(tegra_prod_init);

/**
 * tegra_prod_get - Get the prod list for tegra.
 * @dev: Device for which prod settings are required.
 * @name: Name of the prod settings, if NULL then complete list of that device.
 */
struct tegra_prod_list *tegra_prod_get(struct device *dev, const char *name)
{
	if (!dev || !dev->of_node) {
		pr_err("Device does not have node pointer\n");
		return ERR_PTR(-EINVAL);
	}

	return tegra_prod_init(dev->of_node);
}
EXPORT_SYMBOL(tegra_prod_get);

/**
 * tegra_prod_release - Release all the resources.
 * @tegra_prod_list:	the list of tegra prods.
 *
 * Release all the resources of tegra_prod_list.
 * Returns 0 on success.
 */
int tegra_prod_release(struct tegra_prod_list **tegra_prod_list)
{
	int i;
	struct tegra_prod_config *t_prod;
	struct tegra_prod_list *tp_list;

	if (!tegra_prod_list)
		return -EINVAL;

	tp_list = *tegra_prod_list;
	if (tp_list) {
		if (tp_list->prod_config) {
			for (i = 0; i < tp_list->num; i++) {
				t_prod = (struct tegra_prod_config *)
					&tp_list->prod_config[i];
				if (t_prod)
					kfree(t_prod->prod_tuple);
			}
			kfree(tp_list->prod_config);
		}
		kfree(tp_list);
	}

	*tegra_prod_list = NULL;
	return 0;
}
EXPORT_SYMBOL(tegra_prod_release);

static void devm_tegra_prod_release(struct device *dev, void *res)

{
	struct tegra_prod *prod_list = *(struct tegra_prod **)res;

	tegra_prod_release(&prod_list);
}

struct tegra_prod *devm_tegra_prod_get(struct device *dev)
{
	struct tegra_prod **ptr, *prod_list;

	ptr = devres_alloc(devm_tegra_prod_release, sizeof(*ptr), GFP_KERNEL);
	if (!ptr)
		return ERR_PTR(-ENOMEM);

	prod_list = tegra_prod_get(dev, NULL);
	if (IS_ERR(prod_list)) {
		devres_free(ptr);
		return prod_list;
	}

	*ptr = prod_list;
	devres_add(dev, ptr);

	return prod_list;
}
EXPORT_SYMBOL(devm_tegra_prod_get);

struct tegra_prod *tegra_prod_get_from_node(struct device_node *np)
{
	return tegra_prod_init(np);
}
EXPORT_SYMBOL(tegra_prod_get_from_node);

struct tegra_prod *devm_tegra_prod_get_from_node(struct device *dev,
						 struct device_node *np)
{
	struct tegra_prod **ptr, *prod_list;

	ptr = devres_alloc(devm_tegra_prod_release, sizeof(*ptr), GFP_KERNEL);
	if (!ptr)
		return ERR_PTR(-ENOMEM);

	prod_list = tegra_prod_init(np);
	if (IS_ERR(prod_list)) {
		devres_free(ptr);
		return prod_list;
	}

	*ptr = prod_list;
	devres_add(dev, ptr);

	return prod_list;
}
EXPORT_SYMBOL(devm_tegra_prod_get_from_node);

int tegra_prod_put(struct tegra_prod *tegra_prod)
{
	return tegra_prod_release(&tegra_prod);
}
EXPORT_SYMBOL(tegra_prod_put);
