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

#ifndef _TEGRA_PRODS_H
#define _TEGRA_PRODS_H

struct tegra_prod_list;

#define tegra_prod tegra_prod_list

int tegra_prod_set_list(void __iomem **base,
		struct tegra_prod_list *tegra_prod_list);

int tegra_prod_set_boot_init(void __iomem **base,
		struct tegra_prod_list *tegra_prod_list);

int tegra_prod_set_by_name(void __iomem **base, const char *name,
		struct tegra_prod_list *tegra_prod_list);

struct tegra_prod_list *tegra_prod_init(const struct device_node *np);

bool tegra_prod_by_name_supported(struct tegra_prod_list *tegra_prod_list,
				  const char *name);

struct tegra_prod_list *tegra_prod_get(struct device *dev, const char *name);

int tegra_prod_release(struct tegra_prod_list **tegra_prod_list);

/**
 * devm_tegra_prod_get(): Get the prod handle from the device.
 * @dev: Device handle on which prod setting nodes are available.
 *
 * Parse the prod-setting node of the dev->of_node and keep all prod
 * setting data in prod handle.
 * This handle is used for setting prod configurations.
 *
 * Returns valid prod_list handle on success or pointer to the error
 * when it failed.
 */
struct tegra_prod *devm_tegra_prod_get(struct device *dev);

/**
 * tegra_prod_get_from_node(): Get the prod handle from the node.
 * @np: Node pointer on which prod setting nodes are available.
 *
 * Parse the prod-setting node of the node pointer "np" and keep all prod
 * setting data in prod handle.
 * This handle is used for setting prod configurations.
 *
 * Returns valid prod_list handle on success or pointer to the error
 * when it failed.
 */
struct tegra_prod *tegra_prod_get_from_node(struct device_node *np);

/**
 * devm_tegra_prod_get_from_node(): Get the prod handle from the node.
 * @dev: Device handle.
 * @np: Node pointer on which prod setting nodes are available.
 *
 * Parse the prod-setting node of the node pointer "np" and keep all prod
 * setting data in prod handle.
 * This handle is used for setting prod configurations.
 *
 * Returns valid prod_list handle on success or pointer to the error
 * when it failed.
 * The allocated resource is released by driver core framework when device
 * is unbinded and so no need to call any release APIs for the tegra_prod
 * handle.
 */
struct tegra_prod *devm_tegra_prod_get_from_node(struct device *dev,
						 struct device_node *np);

/**
 * tegra_prod_put(): Put the allocated prod handle.
 * @tegra_prod: Tegra prod handle which was allocated by function
 *		devm_tegra_prod_get() or tegra_prod_get_from_node().
 *
 * Release the prod handle.
 *
 * Returns 0 on success or error number in negative when it failed.
 */
int tegra_prod_put(struct tegra_prod *tegra_prod);
#endif
