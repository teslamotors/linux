/*
 * drivers/platform/tegra/asim.c
 *
 * Copyright (C) 2010-2015 NVIDIA Corporation. All rights reserved.
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

#include <linux/printk.h>
#include <linux/io.h>
#include <linux/tegra-soc.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>

#include <mach/irqs.h>

#include "iomap.h"

#define DEFAULT_ASIM_FAKE_RPC_BASE	0x538f0000
#define ASIM_SHUTDOWN_REG_OFFSET	0xffc


static void __iomem *asim_shutdown_reg;

static struct of_device_id tegra_asim_of_match[] = {
	{ .compatible = "nvidia,tegra210-asim", },
	{ },
};

static int tegra_asim_probe(struct platform_device *pdev)
{
	if (pdev->dev.of_node) {
		const struct of_device_id *match;

		match = of_match_device(tegra_asim_of_match, &pdev->dev);
		if (match) {
			struct resource *res;

			res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
			asim_shutdown_reg =
				ioremap(
					(unsigned long)res->start +
					(unsigned long)ASIM_SHUTDOWN_REG_OFFSET,
					4);
		} else {
			asim_shutdown_reg = ioremap(
						DEFAULT_ASIM_FAKE_RPC_BASE +
						ASIM_SHUTDOWN_REG_OFFSET,
						4);
		}
	} else {
		asim_shutdown_reg = ioremap(DEFAULT_ASIM_FAKE_RPC_BASE +
						ASIM_SHUTDOWN_REG_OFFSET,
						4);
	}

	return 0;
}

static int tegra_asim_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver platform_driver = {
	.probe = tegra_asim_probe,
	.remove  = tegra_asim_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = "asim",
#ifdef CONFIG_OF
		.of_match_table = tegra_asim_of_match,
#endif
	},
};

static void asim_power_off(void)
{
	pr_err("ASIM Powering off the device\n");
	*(unsigned int *)(asim_shutdown_reg) = 1;
	while (1);
}

static int __init asim_init(void)
{
	if (tegra_cpu_is_asim())
		pm_power_off = asim_power_off;

	return platform_driver_register(&platform_driver);
}

static void __exit asim_exit(void)
{
        platform_driver_unregister(&platform_driver);
}

arch_initcall(asim_init);
module_exit(asim_exit);
