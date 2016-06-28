/*
 * pinctrl-apl-dev.c: APL pinctrl GPIO Platform Device
 *
 * (C) Copyright 2015 Intel Corporation
 * Author: Kean Ho, Chew (kean.ho.chew@intel.com)
 * Modified: Tan, Jui Nee (jui.nee.tan@intel.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/bitops.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>
#include <linux/pci.h>
#include <linux/pinctrl/pinctrl-apl.h>

/* PCI Memory Base Access */
#define PCI_DEVICE_ID_INTEL_APL_PCU	0x5A92
#define NO_REGISTER_SETTINGS	(BIT(0) | BIT(1) | BIT(2))

/* Offsets */
#define NORTH_OFFSET		0xC5
#define NORTHWEST_OFFSET		0xC4
#define WEST_OFFSET		0xC7
#define SOUTHWEST_OFFSET		0xC0

#define NORTH_END		(90 * 0x8)
#define NORTHWEST_END		(77 * 0x8)
#define WEST_END		(47 * 0x8)
#define SOUTHWEST_END		(43 * 0x8)

static struct apl_pinctrl_port apl_gpio_north_platform_data = {
	.unique_id = "1",
};

static struct resource apl_gpio_north_resources[] = {
	{
		.start	= 0x0,
		.end	= 0x0,
		.flags	= IORESOURCE_MEM,
		.name	= "io-memory",
	},
	{
		.start	= 14,
		.end	= 14,
		.flags	= IORESOURCE_IRQ,
		.name	= "irq",
	}
};

static struct apl_pinctrl_port apl_gpio_northwest_platform_data = {
	.unique_id = "2",
};

static struct resource apl_gpio_northwest_resources[] = {
	{
		.start	= 0x0,
		.end	= 0x0,
		.flags	= IORESOURCE_MEM,
		.name	= "io-memory",
	},
	{
		.start	= 14,
		.end	= 14,
		.flags	= IORESOURCE_IRQ,
		.name	= "irq",
	}
};

static struct apl_pinctrl_port apl_gpio_west_platform_data = {
	.unique_id = "3",
};

static struct resource apl_gpio_west_resources[] = {
	{
		.start	= 0x0,
		.end	= 0x0,
		.flags	= IORESOURCE_MEM,
		.name	= "io-memory",
	},
	{
		.start	= 14,
		.end	= 14,
		.flags	= IORESOURCE_IRQ,
		.name	= "irq",
	}
};

static struct apl_pinctrl_port apl_gpio_southwest_platform_data = {
	.unique_id = "4",
};

static struct resource apl_gpio_southwest_resources[] = {
	{
		.start	= 0x0,
		.end	= 0x0,
		.flags	= IORESOURCE_MEM,
		.name	= "io-memory",
	},
	{
		.start	= 14,
		.end	= 14,
		.flags	= IORESOURCE_IRQ,
		.name	= "irq",
	}
};

static struct platform_device apl_gpio_north_device = {
	.name			= "apl_gpio",
	.id			= 0,
	.num_resources		= ARRAY_SIZE(apl_gpio_north_resources),
	.resource		= apl_gpio_north_resources,
	.dev			= {
		.platform_data	= &apl_gpio_north_platform_data,
	}
};

static struct platform_device apl_gpio_northwest_device = {
	.name			= "apl_gpio",
	.id			= 1,
	.num_resources		= ARRAY_SIZE(apl_gpio_northwest_resources),
	.resource		= apl_gpio_northwest_resources,
	.dev			= {
		.platform_data	= &apl_gpio_northwest_platform_data,
	}
};

static struct platform_device apl_gpio_west_device = {
	.name			= "apl_gpio",
	.id			= 2,
	.num_resources		= ARRAY_SIZE(apl_gpio_west_resources),
	.resource		= apl_gpio_west_resources,
	.dev			= {
		.platform_data	= &apl_gpio_west_platform_data,
	}
};

static struct platform_device apl_gpio_southwest_device = {
	.name			= "apl_gpio",
	.id			= 3,
	.num_resources		= ARRAY_SIZE(apl_gpio_southwest_resources),
	.resource		= apl_gpio_southwest_resources,
	.dev			= {
		.platform_data	= &apl_gpio_southwest_platform_data,
	}
};

static struct platform_device *devices[] __initdata = {
	&apl_gpio_north_device,
	&apl_gpio_northwest_device,
	&apl_gpio_west_device,
	&apl_gpio_southwest_device,
};

static int __init get_pci_memory_init(void)
{
	u32 io_base_add;
	struct pci_dev *pci_dev;

	pci_dev = pci_get_device(PCI_VENDOR_ID_INTEL,
				PCI_DEVICE_ID_INTEL_APL_PCU,
				NULL);

	if (pci_dev == NULL)
		return -EFAULT;

	pci_read_config_dword(pci_dev, 0x10, &io_base_add);
	io_base_add &= ~NO_REGISTER_SETTINGS;
	apl_gpio_north_resources[0].start =
		io_base_add + (NORTH_OFFSET << 16);
	apl_gpio_north_resources[0].end =
		io_base_add + (NORTH_OFFSET << 16) + NORTH_END;
	apl_gpio_northwest_resources[0].start =
		io_base_add + (NORTHWEST_OFFSET << 16);
	apl_gpio_northwest_resources[0].end =
		io_base_add + (NORTHWEST_OFFSET << 16) + NORTHWEST_END;
	apl_gpio_west_resources[0].start =
		io_base_add + (WEST_OFFSET << 16);
	apl_gpio_west_resources[0].end =
		io_base_add + (WEST_OFFSET << 16) + WEST_END;
	apl_gpio_southwest_resources[0].start =
		io_base_add + (SOUTHWEST_OFFSET << 16);
	apl_gpio_southwest_resources[0].end =
		io_base_add + (SOUTHWEST_OFFSET << 16) + SOUTHWEST_END;
	return 0;
};
rootfs_initcall(get_pci_memory_init);

static int __init apl_gpio_device_init(void)
{
	return platform_add_devices(devices, ARRAY_SIZE(devices));
};
device_initcall(apl_gpio_device_init);
