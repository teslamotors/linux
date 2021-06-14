// SPDX-License-Identifier: GPL-2.0-only

#define pr_fmt(fmt) "hsti: " fmt

#include <linux/acpi.h>
#include <linux/bits.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/printk.h>

#include "acpi.h"

#define ACPI_SIG_HSTI "OEM1"

static struct kobject *kobj;
static struct acpi_table_hsti *hsti;

static ssize_t state_show(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%08x\n", hsti->state);
}
static DEVICE_ATTR_RO(state);

#define HSTI_BIT(n) (((hsti->state) & BIT(n)) >> (n))

#define HSTI_ATTR(name, n) \
static ssize_t name##_show(struct device *dev, struct device_attribute *attr, \
				char *buf) \
{ \
	return snprintf(buf, PAGE_SIZE, "%lu\n", HSTI_BIT(n)); \
} \
static DEVICE_ATTR_RO(name)

HSTI_ATTR(psp_secure_en, 0);
HSTI_ATTR(platform_secure_boot_en, 1);
HSTI_ATTR(psp_debug_lock_on, 2);
HSTI_ATTR(psp_customer_key_lock_on, 3);
HSTI_ATTR(psp_anti_rollback_en, 7);
HSTI_ATTR(psp_rpmc_production_en, 8);
HSTI_ATTR(psp_rpmc_provision_success, 9);

static struct attribute *attrs[] = {
	&dev_attr_state.attr,
	&dev_attr_psp_secure_en.attr,
	&dev_attr_platform_secure_boot_en.attr,
	&dev_attr_psp_debug_lock_on.attr,
	&dev_attr_psp_customer_key_lock_on.attr,
	&dev_attr_psp_anti_rollback_en.attr,
	&dev_attr_psp_rpmc_production_en.attr,
	&dev_attr_psp_rpmc_provision_success.attr,
	NULL,
};

static const struct attribute_group attr_group = {
	.attrs = attrs,
};

static int __init hsti_init(void)
{
	acpi_status status;
	int err;

	status = acpi_get_table(ACPI_SIG_HSTI, 1,
				(struct acpi_table_header **)&hsti);
	if (ACPI_FAILURE(status) || hsti->header.length < sizeof(*hsti)) {
		pr_err(FW_BUG "failed to get ACPI table: %u\n", status);
		return -EINVAL;
	}

	pr_info("state = 0x%08x\n", hsti->state);

	kobj = kobject_create_and_add("hsti", firmware_kobj);
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

static void __exit hsti_exit(void)
{
	sysfs_remove_group(kobj, &attr_group);
	kobject_put(kobj);
}

module_init(hsti_init);
module_exit(hsti_exit);

MODULE_DESCRIPTION("AMD Hardware Security Test Interface sysfs driver.");
MODULE_LICENSE("GPL v2");
