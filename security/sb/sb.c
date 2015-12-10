/*
 *
 * Intel Keystore Linux driver
 * Copyright (c) 2014, Intel Corporation.
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
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

#include <linux/user.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/pci.h>

#include <linux/xattr.h>
#include <linux/security.h>

#define SB_ENABLED		0x01	/* indicates sb is enabled */
#define SB_NOT_ENABLED		0x00	/* indicates sb is not enalbed */
#define PCI_DEVICE_ID_TXE	0x0F18	/* device-if for intel TXE */

/* global variable, secure boot status is retrieved and stored here */
static unsigned char sb_status;

/* global for /sys/kernel/security/sbstat */
static struct dentry *sb_stat_init;

/**
 * get_sb_stat - Function which returns the SB (Secure boot) status.
 *
 * Returns 1 on SB enabled systems else returns 0
 */
unsigned char get_sb_stat(void)
{
	/*
	 * Secure boot status is read once during the module init
	 * and stored in a static global variable. This function
	 * uses this info and returns accordingly.
	 */
	pr_info(KBUILD_MODNAME ": SecureBoot: %s [reg: %#x]\n",
		((sb_status & SB_ENABLED) ? "Enabled" : "Disabled"),
		sb_status);
	return sb_status & SB_ENABLED;
}
EXPORT_SYMBOL(get_sb_stat);

/**
 * sb_stat_rd - read() for /sys/kernel/security/sb-stat
 *
 * @filp: file pointer, not actually used
 * @buf: where to put the result
 * @count: maximum to send along
 * @ppos: where to start
 *
 * Returns number of bytes read or error code, as appropriate
 */
static ssize_t sb_stat_rd(struct file *filp, char __user *buf,
			  size_t count, loff_t *ppos)
{
	unsigned char temp[8] = {0};
	ssize_t rc;

	if (*ppos != 0)
		return 0;

	sprintf(temp, "%d", get_sb_stat());

	rc = simple_read_from_buffer(buf, count, ppos, temp, strlen(temp));

	return rc;
}

/**
 * fileop structure for /sys/kernel/security/sb-stat
 */
static const struct file_operations sbstat_ops = {
	.read		= sb_stat_rd,
	.write		= NULL,
};

/**
 * txe_probe - PCI device probe function
 *
 * @pdev:	pci_dev structure pointer for the probed device
 * @ent:	pci_device_identifier for the probed device
 *
 * Returns error if sys file creation fails
 */
int txe_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	int err = -1;

	pr_info(KBUILD_MODNAME ": txe_probe: vid: %#x, did: %#x\n",
		PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_TXE);

	/* read the secure-boot status and store it locally */
	if ((!pdev) || (0 != pci_read_config_byte(pdev, 0x50, &sb_status))) {
		pr_err(KBUILD_MODNAME ": TXE pci_read_config_byte: failed, so assuming SB failed/not enabled\n");
		sb_status = SB_NOT_ENABLED;
	}

	/*
	 * create a sysfs entry under /sys/kernel/security/sbstat.
	 * Only read operation is supported on this sysfsfile.
	 * Returns:
	 *  1 - SB enabled
	 *  0 - SB disabled
	 */
	sb_stat_init = securityfs_create_file("sb-stat", S_IRUSR | S_IRGRP,
					      NULL, NULL, &sbstat_ops);
	if (!sb_stat_init || IS_ERR(sb_stat_init)) {
		err = -EFAULT;
		pr_err(KBUILD_MODNAME ": TXE : failed to create sysfs entry for sbstat\n");
	}

	/* return -1, so that we do not claim the device and it can be used
	 * actual txe drivers.
	 */

	return err;
}

/**
 * txe_remove - PCI device remove function
 *
 * @dev:	pci_dev structure pointer for the probed device
 *
 * Returns void
 */
void txe_remove(struct pci_dev *dev)
{
	pr_info(KBUILD_MODNAME ": txe_remove : vid: %#x, did: %#x\n",
			PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_TXE);
}

/* pci device details */
static const struct pci_device_id txe_pci_tbl[] = {
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_TXE)},
	{0, }
};

static struct pci_driver txe_pci_drv = {
	.name = KBUILD_MODNAME,
	.id_table = txe_pci_tbl,
	.probe = txe_probe,
	.remove = txe_remove,
};

module_pci_driver(txe_pci_drv);

MODULE_AUTHOR("Intel Corporation");
MODULE_DESCRIPTION("TXE SecureBoot status");
MODULE_LICENSE("GPL v2");

