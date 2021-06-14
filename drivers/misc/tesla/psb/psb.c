// SPDX-License-Identifier: GPL-2.0-only

#define pr_fmt(fmt) "psb: " fmt

#include <linux/acpi.h>
#include <linux/bits.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/printk.h>

#include "acpi.h"

#define ACPI_SIG_PSB "OEM2"

static struct kobject *kobj;
static struct acpi_table_psb *psb;

static ssize_t config_show(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%08x\n", psb->config);
}
static DEVICE_ATTR_RO(config);

#define PSB_BITS(s, n) ((psb->config >> (s)) & (BIT(n) - 1))

#define PSB_ATTR(name, s, n) \
static ssize_t name##_show(struct device *dev, struct device_attribute *attr, \
				char *buf) \
{ \
	return snprintf(buf, PAGE_SIZE, "%lu\n", PSB_BITS(s, n)); \
} \
static DEVICE_ATTR_RO(name)

PSB_ATTR(platform_vendor_id, 0, 8);
PSB_ATTR(platform_model_id, 8, 4);
PSB_ATTR(root_key_select, 16, 4);
PSB_ATTR(platform_secure_boot_en, 24, 1);
PSB_ATTR(disable_bios_key_anti_rollback, 25, 1);
PSB_ATTR(disable_amd_key_usage, 26, 1);
PSB_ATTR(disable_secure_debug_unlock, 27, 1);
PSB_ATTR(customer_key_lock, 28, 1);

static ssize_t bios_key_rev_id_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%04x\n", psb->bios_key_rev_id);
}
static DEVICE_ATTR_RO(bios_key_rev_id);

static ssize_t root_key_hash_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	int i;
	size_t n = 0;

	for (i = 0; i < sizeof(psb->root_key_hash); i++)
		n += snprintf(buf + n, PAGE_SIZE - n, "%02x",
			psb->root_key_hash[i]);

	n += snprintf(buf + n, PAGE_SIZE - n, "\n");
	return n;
}
static DEVICE_ATTR_RO(root_key_hash);

static struct attribute *attrs[] = {
	&dev_attr_config.attr,
	&dev_attr_platform_vendor_id.attr,
	&dev_attr_platform_model_id.attr,
	&dev_attr_root_key_select.attr,
	&dev_attr_platform_secure_boot_en.attr,
	&dev_attr_disable_bios_key_anti_rollback.attr,
	&dev_attr_disable_amd_key_usage.attr,
	&dev_attr_disable_secure_debug_unlock.attr,
	&dev_attr_customer_key_lock.attr,
	&dev_attr_bios_key_rev_id.attr,
	&dev_attr_root_key_hash.attr,
	NULL,
};

static const struct attribute_group attr_group = {
	.attrs = attrs,
};

static int __init psb_init(void)
{
	acpi_status status;
	int err;

	status = acpi_get_table(ACPI_SIG_PSB, 1,
				(struct acpi_table_header **)&psb);
	if (ACPI_FAILURE(status) || psb->header.length < sizeof(*psb)) {
		pr_err(FW_BUG "failed to get ACPI table: %u\n", status);
		return -EINVAL;
	}

	pr_info("config = 0x%08x\n", psb->config);

	kobj = kobject_create_and_add("psb", firmware_kobj);
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

static void __exit psb_exit(void)
{
	sysfs_remove_group(kobj, &attr_group);
	kobject_put(kobj);
}

module_init(psb_init);
module_exit(psb_exit);

MODULE_DESCRIPTION("AMD Platform Secure Boot sysfs driver.");
MODULE_LICENSE("GPL v2");
