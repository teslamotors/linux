/*
 * Sideband Interface driver for Apollo Lake
 * Copyright (c) 2015, Intel Corporation.
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
 *
 * The Sideband Interface is an access mechanism to communicate with multiple
 * devices on-board the SoC fabric over a PCI interface. This driver deals with
 * the Apollo Lake SoC.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/mutex.h>
#include <linux/pci.h>
#include <linux/debugfs.h>
#include <linux/capability.h>
#include <linux/platform_device.h>

#include <linux/platform_data/sbi_apl.h>

#define DRV_NAME "sbi_apl"
#define DRV_VERSION "1.0"

static struct sbi_platform_data *plat_data;

static u32 sbi_address(const struct sbi_apl_message *args)
{
	return (((u32)args->port_address) << 24 | args->register_offset);
}

static u16 sbi_status(const struct sbi_apl_message *args)
{
	return ((u16)args->opcode << 8) |
	args->posted << 7 |
	args->status << 1;
}

static u16 sbi_routing(const struct sbi_apl_message *args)
{
	return ((u16)args->byte_enable << 12) |
		args->base_address_register << 8 |
		args->function_id;
}

/* returns 0 on OK wait */
static int sbi_do_wait(struct pci_bus *sbi_pdev, unsigned int devfn, u16 *word)
{
	int ret;
	unsigned int timeout = 0x0FFFFFFF; /*2^28-1*/

	do {
		ret = pci_bus_read_config_word(
			sbi_pdev,
			devfn,
			SBI_STAT_OFFSET,
			word);
		if (ret)
			return ret;
		if (*word == (u16) -1) {
			pr_err("sbi_do_wait busy wait failed, P2SB read not allowed?");
			return 1;
		}
		timeout--;
	} while (timeout && (*word & SBI_STAT_BUSY_MASK));
	/* fail if time-out occurs */
	return timeout ? 0 : 1;
}

static int sbi_do_write(struct pci_bus *sbi_pdev, unsigned int devfn,
	struct sbi_apl_message *args)
{
	int ret = 0;
	u16 word;

	if (!sbi_pdev)
		return -ENODEV;
	if (!capable(CAP_SYS_RAWIO))
		return -EACCES;
	/* Is it busy? */
	if (sbi_do_wait(sbi_pdev, devfn, &word)) {
		pr_err(DRV_NAME " device has busy bit set %hx!\n", word);
		ret = -EAGAIN;
		goto err;
	};
	ret = pci_bus_write_config_dword(sbi_pdev, devfn,
		SBI_ADDR_OFFSET, sbi_address(args));
	if (ret)
		goto err;
	ret = pci_bus_write_config_dword(sbi_pdev, devfn,
		SBI_DATA_OFFSET, args->data);
	if (ret)
		goto err;
	ret = pci_bus_write_config_word(sbi_pdev, devfn,
		SBI_ROUT_OFFSET, sbi_routing(args));
	if (ret)
		goto err;
	ret = pci_bus_write_config_dword(sbi_pdev, devfn,
		SBI_EADD_OFFSET, args->extended_register_address);
	if (ret)
		goto err;
	ret = pci_bus_write_config_word(sbi_pdev, devfn,
		SBI_STAT_OFFSET, sbi_status(args) | SBI_STAT_BUSY_MASK);
	if (ret)
		goto err;

	/* Wait for busy loop */
	if (sbi_do_wait(sbi_pdev, devfn, &word)) {
		pr_err(DRV_NAME " device is not responding %hx!\n", word);
		ret = -EBUSY;
		goto err;
	};
	args->opcode = (word & 0xff00) >> 8;
	args->posted = (word & 0x0080) >> 7;
	args->status = (word & 0x0006) >> 1;

	ret = pci_bus_read_config_dword(sbi_pdev, devfn,
		SBI_DATA_OFFSET, &args->data);
	if (ret)
		goto err;
	pr_debug(DRV_NAME" read commit got %04x\n", args->data);
	return 0;
err:
	pr_err(DRV_NAME " PCI config access failed with %d\n", ret);
	return ret;
}

static int sbi_validate(const struct sbi_apl_message *args)
{
	if (args->posted > 0x01 ||
	args->status > 0x03 ||
	args->byte_enable > 0x0f ||
	args->base_address_register > 0x07)
		return -EINVAL;
	return 0;
}

/* hide/unhide P2SB PCI device
 * hide = 1 device will be hidden
 * hide = 0 device will be visible
 */
static void sbi_hide(struct pci_bus *bus, unsigned int devfn, int hide)
{
	pci_bus_write_config_byte(bus, devfn, 0xe1, hide);
}

/* we don't hide anything if it is somehow not hidden to begin with */
static int sbi_ishidden(struct pci_bus *bus, unsigned int devfn)
{
	u8 ret;

	pci_bus_read_config_byte(bus, devfn, 0xe1, &ret);
	return (ret) ? 1 : 0;
}

int sbi_apl_commit(struct sbi_apl_message *args)
{
	int ret;
	struct pci_bus *sbi_pdev;

	/* We're called before we're ready */
	if (!plat_data)
		return -EAGAIN;

	ret = sbi_validate(args);
	if (ret)
		return ret;

	sbi_pdev = pci_find_bus(0, plat_data->bus);
	if (!sbi_pdev)
		return -ENODEV;
	mutex_lock(plat_data->lock);
	sbi_hide(sbi_pdev, plat_data->p2sb, 0);
	ret = sbi_do_write(sbi_pdev, plat_data->p2sb, args);
	sbi_hide(sbi_pdev, plat_data->p2sb, 1);
	mutex_unlock(plat_data->lock);

	return ret;
}
EXPORT_SYMBOL(sbi_apl_commit);
#ifdef CONFIG_X86_INTEL_SBI_APL_DEBUG
static struct dentry *sbi_dbg;
static struct sbi_apl_message debug_args;

static void sbi_dbg_cleanup(void)
{
	debugfs_remove_recursive(sbi_dbg);
	sbi_dbg = NULL;
}

static int dbg_posted_set(void *data, u64 val)
{
	*(u8 *)data = !!val;
	return 0;
}

static int dbg_status_set(void *data, u64 val)
{
	*(u8 *)data = val & SBI_STAT_STATUS_MASK;
	return 0;
}

static int dbg_byte_enable_set(void *data, u64 val)
{
	*(u8 *)data = val & SBI_STAT_BYTE_ENABLE_MASK;
	return 0;
}

static int dbg_base_address_register_fops_set(void *data, u64 val)
{
	*(u8 *)data = val & SBI_STAT_BAR_MASK;
	return 0;
}

static int dbg_commit_set(void *data, u64 val)
{
	return sbi_apl_commit(&debug_args);
}

static int dbg_u8_get(void *data, u64 *val)
{
	*val = *(u8 *)data;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(posted_fops, dbg_u8_get, dbg_posted_set, "0x%01llx\n");
DEFINE_SIMPLE_ATTRIBUTE(status_fops, dbg_u8_get, dbg_status_set, "0x%01llx\n");
DEFINE_SIMPLE_ATTRIBUTE(byte_enable_fops, dbg_u8_get, dbg_byte_enable_set,
	"0x%01llx\n");
DEFINE_SIMPLE_ATTRIBUTE(base_address_register_fops, dbg_u8_get,
	dbg_base_address_register_fops_set, "0x%01llx\n");
DEFINE_SIMPLE_ATTRIBUTE(commit_fops, NULL, dbg_commit_set, "0x%01llx\n");

static void sbi_dbg_setup(void)
{
	struct dentry *d;

	if (!IS_ERR_OR_NULL(sbi_dbg))
		return;
	memset(&debug_args, 0, sizeof(debug_args));
	sbi_dbg = debugfs_create_dir("sbi_apl", NULL);
	if (IS_ERR_OR_NULL(sbi_dbg))
		return;

	/* port_address */
	d = debugfs_create_x8("port_address", 0660, sbi_dbg,
		&debug_args.port_address);
	if (!d)
		goto cleanup;

	/* register_offset */
	d = debugfs_create_x16("register_offset", 0660, sbi_dbg,
		&debug_args.register_offset);
	if (!d)
		goto cleanup;

	/* opcode */
	d = debugfs_create_x8("opcode", 0660, sbi_dbg, &debug_args.opcode);
	if (!d)
		goto cleanup;

	/* data */
	d = debugfs_create_x32("data", 0660, sbi_dbg, &debug_args.data);
	if (!d)
		goto cleanup;

	/* posted */
	d = debugfs_create_file("posted", 0660, sbi_dbg, &debug_args.posted,
		&posted_fops);
	if (!d)
		goto cleanup;

	/* status */
	d = debugfs_create_file("status", 0660, sbi_dbg, &debug_args.status,
		&status_fops);
	if (!d)
		goto cleanup;

	/* byte_enable */
	d = debugfs_create_file("byte_enable", 0660, sbi_dbg,
		&debug_args.byte_enable, &byte_enable_fops);
	if (!d)
		goto cleanup;

	/* base_address_register */
	d = debugfs_create_file("base_address_register",
		0660, sbi_dbg, &debug_args.base_address_register,
		&base_address_register_fops);
	if (!d)
		goto cleanup;

	/* function_id */
	d = debugfs_create_x8("function_id", 0660, sbi_dbg,
		&debug_args.function_id);
	if (!d)
		goto cleanup;

	/* extended_register_address */
	d = debugfs_create_x32("extended_register_address", 0660, sbi_dbg,
		&debug_args.extended_register_address);
	if (!d)
		goto cleanup;

	/* commit - initiate write to registers */
	d = debugfs_create_file("commit", 0220, sbi_dbg, NULL, &commit_fops);
	if (!d)
		goto cleanup;

	return;

cleanup:
	sbi_dbg_cleanup();
}

#else
static void sbi_dbg_setup(void)
{
}
static void sbi_dbg_cleanup(void)
{
}
#endif /* CONFIG_X86_INTEL_SBI_DEBUG */

static int sbi_apl_plat_probe(struct platform_device *dev)
{
	struct pci_bus *bus = NULL;
	struct sbi_platform_data *pdata;
	u32 id;
	int ret;

	if (!dev)
		return -ENODEV;

	pdata = dev_get_platdata(&dev->dev);

	pr_info("%s starting, bus %x, device %01x.%x", pdata->name, pdata->bus,
		PCI_SLOT(pdata->p2sb), PCI_FUNC(pdata->p2sb));
	bus = pci_find_bus(0, pdata->bus);
	if (!bus) {
		pr_warn("Cannot get P2SB bus!");
		return -ENODEV;
	}
	mutex_lock(pdata->lock);
	sbi_hide(bus, pdata->p2sb, 0);

	if (sbi_ishidden(bus, pdata->p2sb)) {
		pci_bus_read_config_dword(bus, pdata->p2sb, 0x00, &id);
		if (id != (u32)-1) {
			pr_warn("P2SB (0x%08x) might be there but disabled",
				id);
		} else {
			pr_err("Cannot unhideP2SB, is it really there?");
			ret = -ENODEV;
			goto out;
		}
	}
	sbi_hide(bus, pdata->p2sb, 1);
	mutex_unlock(pdata->lock);
	plat_data = pdata;
	sbi_dbg_setup();
	return 0;

out:
	mutex_unlock(pdata->lock);
	return ret;
}

static struct platform_driver sbi_plat_driver = {
	.probe		= sbi_apl_plat_probe,
	.driver = {
		.name = DRV_NAME,
	},
};

static void __exit sbi_apl_exit(void)
{
	sbi_dbg_cleanup();
	platform_driver_unregister(&sbi_plat_driver);
}

static int __init sbi_apl_init(void)
{
	pr_info("Apollo Lake Sideband Interface loading");
	return platform_driver_register(&sbi_plat_driver);
}

MODULE_DESCRIPTION("Apollo Lake Sideband Interface");
MODULE_VERSION(DRV_VERSION);
module_init(sbi_apl_init);
module_exit(sbi_apl_exit);
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" DRV_NAME);
