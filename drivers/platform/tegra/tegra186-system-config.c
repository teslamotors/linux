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

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <iomap.h>

struct tegra_system_config_info {
	struct device *dev;
	int n_mem_count;
	void __iomem **base;
	struct resource **mem_res;
};

static struct tegra_system_config_info *tegra186_system_config;

static int system_config_init_register(struct device *dev,
		struct tegra_system_config_info *tsconf)
{
	struct device_node *np = dev->of_node;
	struct device_node *child;
	unsigned long rval, reg;
	int count;
	u32 rindex, offs, mask, val;
	int i, ret;

	if (!np) {
		dev_err(dev, "System Config only supported form DT\n");
		return -ENODEV;
	}

	for_each_child_of_node(np, child) {
		if (!of_device_is_available(child))
			continue;

		count = of_property_count_u32_elems(child, "nvidia,reg-update");
		if ((count < 4) || (count % 4 != 0)) {
			dev_err(dev, "Node %s is not having proper data\n",
				child->full_name);
			continue;
		}

		dev_info(dev, "Initialising system config %s\n", child->name);
		count /= 4;
		for (i = 0; i < count; ++i) {
			int ind = i * 4;

			rindex = 0;
			offs = 0;
			mask = 0;
			val = 0;

			ret = of_property_read_u32_index(child,
					"nvidia,reg-update", ind, &rindex);
			if (!ret)
				ret = of_property_read_u32_index(child,
					"nvidia,reg-update", ind + 1, &offs);
			if (!ret)
				ret = of_property_read_u32_index(child,
					"nvidia,reg-update", ind + 2, &mask);
			if (!ret)
				ret = of_property_read_u32_index(child,
					"nvidia,reg-update", ind + 3, &val);

			if (ret < 0) {
				dev_err(dev, "Node %s failed on row %d\n",
					child->full_name, i);
				break;
			}

			rval = readl(tsconf->base[rindex] + offs);
			rval = (rval & ~mask) | (val & mask);
			writel(rval, tsconf->base[rindex] + offs);
			reg = (unsigned long)tsconf->mem_res[rindex]->start;
			dev_info(dev,
			   "Reg 0x%08lx updated, Mask:val = 0x%08x:0x%08x\n",
				reg + offs, mask, val);
		}

		if (ret < 0)
			continue;
	}
	return 0;
}

static int tegra_system_config_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct resource *r;
	void __iomem *base;
	struct tegra_system_config_info *tsystem_config;
	int i, n_mem_count;
	int ret;

	tsystem_config = devm_kzalloc(dev, sizeof(*tsystem_config), GFP_KERNEL);
	if (!tsystem_config)
		return -ENOMEM;

	for (n_mem_count = 0;; n_mem_count++) {
		r = platform_get_resource(pdev, IORESOURCE_MEM, n_mem_count);
		if (!r)
			break;
	}
	if (!n_mem_count) {
		dev_err(dev, "No Address to configure\n");
		return -ENODEV;
	}

	tsystem_config->mem_res = devm_kzalloc(dev, n_mem_count *
				sizeof(*tsystem_config->mem_res), GFP_KERNEL);
	if (!tsystem_config->mem_res)
		return -ENOMEM;

	tsystem_config->base = devm_kzalloc(dev, n_mem_count *
				sizeof(*tsystem_config->base), GFP_KERNEL);
	if (!tsystem_config->base)
		return -ENOMEM;

	tsystem_config->dev = dev;
	for (i = 0; i < n_mem_count; ++i) {
		r = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (!r) {
			dev_err(dev, "No IO memory resource\n");
			return -ENODEV;
		}

		base = devm_ioremap_resource(dev, r);
		if (IS_ERR(base)) {
			ret = PTR_ERR(base);
			dev_err(dev, "iomap failed register: %d\n", ret);
			return ret;
		}
		tsystem_config->mem_res[i] = r;
		tsystem_config->base[i] = base;
	}

	tegra186_system_config = tsystem_config;
	system_config_init_register(dev, tsystem_config);
	return 0;
}

static struct of_device_id tegra_system_config_of_match[] = {
	{ .compatible = "nvidia,tegra186-system-config", NULL },
	{ },
};

static struct platform_driver tegra_system_config_driver = {
	.driver	= {
		.name   = "tegra186-system-config",
		.owner  = THIS_MODULE,
		.of_match_table = tegra_system_config_of_match,
	},
	.probe	= tegra_system_config_probe,
};

static int __init tegra_system_config_init(void)
{
	return platform_driver_register(&tegra_system_config_driver);
}
postcore_initcall(tegra_system_config_init);
