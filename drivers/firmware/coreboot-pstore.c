// SPDX-License-Identifier: GPL-2.0
// Driver to instantiate ramoops device passed in via coreboot table
//
// Copyright (C) 2021 Tesla, Inc.
// Based in part on chromeos_pstore.c and memconsole-coreboot.c

#include <linux/device.h>
#include <linux/acpi.h>
#include <linux/dmi.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pstore_ram.h>

#define DRV_NAME	"coreboot-pstore"
#define PFX		DRV_NAME ": "

/* FIXME: This header isn't really google-specific and should move */
#include "google/coreboot_table.h"

#define CB_TAG_RAM_OOPS	0x23

#define RAMOOPS_MIN_SIZE(pd)	\
	((pd).record_size + (pd).console_size + (pd).ftrace_size)

static struct ramoops_platform_data coreboot_ramoops_data = {
	/* Default if not overridden with pstore_dmi_table[] */
	.record_size	= 0x20000,
	.console_size	= 0x20000,
	.ftrace_size	= 0x20000,
	.pmsg_size	= 0x0,
	.dump_oops	= 1,
	.ecc_info	= { 0 },
};

static struct platform_device coreboot_ramoops = {
	.name = "ramoops",
	.dev = {
		.platform_data = &coreboot_ramoops_data,
	},
};

static int pstore_enable_infoz(const struct dmi_system_id *id)
{
	printk(KERN_INFO PFX "using tesla infoz pstore policy");
	coreboot_ramoops_data.record_size = 0x8000;
	coreboot_ramoops_data.console_size = 0xb0000;
	coreboot_ramoops_data.ftrace_size = 0x20000;
	coreboot_ramoops_data.pmsg_size = 0x20000;
	coreboot_ramoops_data.ecc_info.ecc_size = 16;

	return 0;
}

static const struct dmi_system_id pstore_dmi_table[] = {
	{
		.ident = "Tesla InfoZ",
		.matches = {
			DMI_MATCH(DMI_PRODUCT_FAMILY, "Tesla_InfoZ"),
		},
		.callback = pstore_enable_infoz,
	},
	{ }
};

static int coreboot_pstore_probe(struct coreboot_device *dev)
{
	if (dev->range.range_size < RAMOOPS_MIN_SIZE(coreboot_ramoops_data)) {
		dev_dbg(&(dev->dev), "RAM_OOPS region too small: %x < %lx\n",
			dev->range.range_size,
			RAMOOPS_MIN_SIZE(coreboot_ramoops_data));
		return -ENODEV;
	}

	dev_info(&(dev->dev),
		"coreboot pstore region found: addr=0x%llx, size=0x%x\n",
		dev->range.range_start, dev->range.range_size);

	coreboot_ramoops_data.mem_address = dev->range.range_start;
	coreboot_ramoops_data.mem_size = dev->range.range_size;

	if (dmi_check_system(pstore_dmi_table) <= 0) {
		dev_info(&(dev->dev),
		"no policy found for this platform, using defaults\n");
	}

	return platform_device_register(&coreboot_ramoops);
}

static int coreboot_pstore_remove(struct coreboot_device *dev)
{
	platform_device_unregister(&coreboot_ramoops);
	return 0;
}

static struct coreboot_driver coreboot_pstore_driver = {
	.probe = coreboot_pstore_probe,
	.remove = coreboot_pstore_remove,
	.drv = {
		.name = "pstore",
	},
	.tag = CB_TAG_RAM_OOPS,
};

module_coreboot_driver(coreboot_pstore_driver);

MODULE_DESCRIPTION("coreboot table pstore/ramoops module");
MODULE_AUTHOR("Tesla, Inc.");
MODULE_LICENSE("GPL v2");
