// SPDX-License-Identifier: GPL-2.0
/* Copyright (C) 2019, Intel, co. */

#include <linux/printk.h>
#include <linux/pci.h>
#include <asm/pci-direct.h>
#include <linux/manuf_mode.h>

#define INTEL_MANUF_MODE BIT(4)
/**
 * manuf_mode() check if the device is in manufacturing mode
 * Return:
 * * MANUF_MODE_UNDEF: if manufacturing mode cannot be determined
 * * MANUF_MODE_ENABLED: if manufacturing mode is enabled
 * * MANUF_MODE_SEALED: 0 if device is sealed. (production)
 */

unsigned int manuf_mode(void)
{
	static u32 fwsts1 = 0;
	u16 vendor, devid;
	u8 slot = 0x0f;

	if (fwsts1)
		goto check;

	WARN_ON(fwsts1 == 0xFFFFFFFF);

	/* FIXME: not all MEI devices are on 0x0f */
	vendor = read_pci_config_16(0, slot, 0, PCI_VENDOR_ID);
	devid = read_pci_config_16(0, slot, 0, PCI_DEVICE_ID);
	if (vendor != 0x8086 && devid != 0x5A9A)
		return MANUF_MODE_UNDEF;

	fwsts1 = read_pci_config(0, slot, 0, 0x40);
	pr_debug("FWSTS = 0x%0X MANUF=0x%08lX\n",
		 fwsts1, fwsts1 & INTEL_MANUF_MODE);
check:
	if (fwsts1 & INTEL_MANUF_MODE) {
		pr_notice_once("Device is in manufacturing mode\n");
		return MANUF_MODE_ENABLED;
	}

	pr_notice_once("Device is in production mode\n");

	return MANUF_MODE_SEALED;
}
