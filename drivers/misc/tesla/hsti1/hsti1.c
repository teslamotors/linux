// SPDX-License-Identifier: GPL-2.0-only

#define pr_fmt(fmt) "hsti1: " fmt

#include <linux/acpi.h>
#include <linux/bits.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/printk.h>

#include "acpi.h"

#define ACPI_SIG_HSTI1 "OEM4"

static struct kobject *kobj;
static struct acpi_table_hsti1 *hsti1;

static ssize_t state_show(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%08x\n", hsti1->state);
}
static DEVICE_ATTR_RO(state);

#define HSTI1_BITS(s, n) ((hsti1->state >> (s)) & (BIT(n) - 1))

#define HSTI1_ATTR(name, s, n) \
static ssize_t name##_show(struct device *dev, struct device_attribute *attr, \
				char *buf) \
{ \
	return snprintf(buf, PAGE_SIZE, "%lu\n", HSTI1_BITS(s, n)); \
} \
static DEVICE_ATTR_RO(name)

HSTI1_ATTR(bios_key_revid_update_req, 11, 1);
HSTI1_ATTR(spl_revid_update_req, 12, 1);
HSTI1_ATTR(spl_status, 13, 3);

static struct attribute *attrs[] = {
	&dev_attr_state.attr,
	&dev_attr_bios_key_revid_update_req.attr,
	&dev_attr_spl_revid_update_req.attr,
	&dev_attr_spl_status.attr,
	NULL,
};

static const struct attribute_group attr_group = {
	.attrs = attrs,
};

static int __init hsti1_init(void)
{
	acpi_status status;
	int err;

	status = acpi_get_table(ACPI_SIG_HSTI1, 1,
				(struct acpi_table_header **)&hsti1);
	if (ACPI_FAILURE(status) || hsti1->header.length < sizeof(*hsti1)) {
		pr_err(FW_BUG "failed to get ACPI table: %u\n", status);
		return -EINVAL;
	}

	pr_info("state = 0x%08x\n", hsti1->state);

	kobj = kobject_create_and_add("hsti1", firmware_kobj);
	if (!kobj) {
		pr_err("failed to create kobject\n");
		return -ENOMEM;
	}

	err = sysfs_create_group(kobj, &attr_group);
	if (err) {
		pr_err("failed to create sysfs group: %d\n", err);
		kobject_put(kobj);
	}

	return err;
}

static void __exit hsti1_exit(void)
{
	sysfs_remove_group(kobj, &attr_group);
	kobject_put(kobj);
}

module_init(hsti1_init);
module_exit(hsti1_exit);

MODULE_AUTHOR("Tesla OpenSource <opensource@tesla.com>");
MODULE_DESCRIPTION("AMD Hardware Security Test Interface 1 sysfs driver.");
MODULE_LICENSE("GPL v2");
