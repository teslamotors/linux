// SPDX-License-Identifier: GPL-2.0-only

#define pr_fmt(fmt) "oem: " fmt

#include <linux/acpi.h>
#include <linux/bits.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/printk.h>

#include "acpi.h"

#define ACPI_SIG_OEM "OEM3"

static struct kobject *kobj;
static struct acpi_table_oem *oem;

static ssize_t state_show(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%02x\n", oem->state);
}
static DEVICE_ATTR_RO(state);

#define OEM_BIT(n) (((oem->state) & BIT(n)) >> (n))

#define OEM_ATTR(name, n) \
static ssize_t name##_show(struct device *dev, struct device_attribute *attr, \
				char *buf) \
{ \
	return snprintf(buf, PAGE_SIZE, "%lu\n", OEM_BIT(n)); \
} \
static DEVICE_ATTR_RO(name)

OEM_ATTR(factory_gated, 0);
OEM_ATTR(delivered, 1);

static struct attribute *attrs[] = {
	&dev_attr_state.attr,
	&dev_attr_factory_gated.attr,
	&dev_attr_delivered.attr,
	NULL,
};

static const struct attribute_group attr_group = {
	.attrs = attrs,
};

static int __init oem_init(void)
{
	acpi_status status;
	int err;

	status = acpi_get_table(ACPI_SIG_OEM, 1,
				(struct acpi_table_header **)&oem);
	if (ACPI_FAILURE(status) || oem->header.length < sizeof(*oem)) {
		pr_err(FW_BUG "failed to get ACPI table: %u\n", status);
		return -EINVAL;
	}

	pr_info("state = 0x%08x\n", oem->state);

	kobj = kobject_create_and_add("oem", firmware_kobj);
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

static void __exit oem_exit(void)
{
	sysfs_remove_group(kobj, &attr_group);
	kobject_put(kobj);
}

module_init(oem_init);
module_exit(oem_exit);

MODULE_DESCRIPTION("AMD OEM-specific fuses sysfs driver.");
MODULE_LICENSE("GPL v2");
