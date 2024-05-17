// SPDX-License-Identifier: GPL-2.0-only
/*
 * Log short panic reason to BIOSRAM region (AMD platforms)
 *
 * Copyright (C) 2021 Tesla, Inc.
 */

#include <linux/dmi.h>
#include <linux/kernel.h>
#include <asm/io.h>
#include <linux/acpi.h>
#include <linux/init.h>
#include <linux/notifier.h>
#include <linux/kdebug.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>

#define DRV_NAME	"panic-biosram"
#define PFX		DRV_NAME ": "

/* Common for many AMD platforms */
#define BIOSRAM_MMIO_BASE	0xfed80500
#define BIOSRAM_MMIO_SIZE	256
#define BIOSRAM_KP_MSG_OFS	4
#define BIOSRAM_KP_MSG_SIZE	156

/* Magic marker at beginning of BIOSRAM region */
static uint8_t biosram_panic_magic[] = { 0xf0, 0x0f, 'K', 'P' };

/* Info about previous panic copied from BIOSRAM */
static bool panic_present;
static uint8_t panic_msg[BIOSRAM_KP_MSG_SIZE + 1];  // +1 to guarantee null termination

static void __iomem *biosram_base = NULL;

struct panic_save {
	unsigned long ip;
	unsigned long cs;
	int trapnr;		/* X86_TRAP_* */
	enum die_val dv;
	int saved;
};

static struct panic_save saved_info = { 0 };

static inline void biosram_write8(size_t idx, uint8_t val)
{
	iowrite8(val, (uint8_t __iomem *) biosram_base + idx);
}

static inline uint8_t biosram_read8(size_t idx)
{
	return ioread8((uint8_t __iomem *) biosram_base + idx);
}

static bool panic_detected(void)
{
	size_t i;
	for (i = 0; i < ARRAY_SIZE(biosram_panic_magic); i++) {
		if (biosram_read8(i) != biosram_panic_magic[i]) {
			return false;
		}
	}
	
	return true;
}

static inline void panic_msg_clear(void)
{
	biosram_write8(0, 0xaa);
}

static inline void panic_msg_copy(uint8_t* dst, size_t len)
{
	size_t i;

	for (i = 0; i < min((size_t)BIOSRAM_KP_MSG_SIZE, len-1); i++)
		dst[i] = biosram_read8(BIOSRAM_KP_MSG_OFS+i);

	dst[i++] = '\0';
}

static ssize_t panic_present_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", panic_present);
}

static ssize_t panic_msg_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	if (panic_present) {
		return snprintf(buf, sizeof(panic_msg)+1, "%s\n", panic_msg);
	}

	return 0;
}

static struct kobj_attribute panic_present_attr = __ATTR_RO(panic_present);
static struct kobj_attribute panic_msg_attr = __ATTR_RO(panic_msg);

static struct attribute *attrs[] = {
	&panic_present_attr.attr,
	&panic_msg_attr.attr,
	NULL,
};

static const struct attribute_group attr_group = {
	.attrs = attrs,
};


static int die_biosram_notifier(struct notifier_block *nb,
				unsigned long reason, void *arg)
{
	struct die_args *die = (struct die_args *) arg;

	/* Only save first meaningful die event */
	if (saved_info.saved == 0 && die->regs != NULL) {
		/* Save for later insertion into BIOSRAM message */
		saved_info.ip = die->regs->ip;
		saved_info.cs = die->regs->cs;
		saved_info.trapnr = die->trapnr;
		saved_info.dv = (enum die_val) reason;
		saved_info.saved = 1;
	}

	return NOTIFY_OK;
}

static int panic_biosram_notifier(struct notifier_block *nb,
				  unsigned long code, void *msg)
{
	size_t i;
	int n_written = 0;
	char s[BIOSRAM_KP_MSG_SIZE];

	/* Magic marker */
	for (i = 0; i < ARRAY_SIZE(biosram_panic_magic); i++)
		biosram_write8(i, biosram_panic_magic[i]);

	if (saved_info.saved) {
		snprintf(s, BIOSRAM_KP_MSG_SIZE,
			 "%s, RIP=%04x:%pS, die=%d, trap=%d",
			 msg ? (char *) msg : "Unknown cause",
			 (int) (saved_info.cs),
			 (void *) (saved_info.ip),
			 (int) (saved_info.dv),
			 saved_info.trapnr);
	} else {
		snprintf(s, BIOSRAM_KP_MSG_SIZE,
			 "%s", msg ? (char *) msg : "Unknown cause");
	}

	/* Save panic reason message */
	i = BIOSRAM_KP_MSG_OFS;
	while (n_written < (BIOSRAM_KP_MSG_SIZE-1) && s[n_written] != '\0')
		biosram_write8(i++, s[n_written++]);
	biosram_write8(i++, '\0');

	return 0;
}

static struct notifier_block die_biosram_nb = {
	.notifier_call = die_biosram_notifier,
};

static struct notifier_block panic_biosram_nb = {
	.notifier_call = panic_biosram_notifier,
};

static int panic_biosram_enable(const struct dmi_system_id *id)
{
	int ret;

	biosram_base = ioremap(BIOSRAM_MMIO_BASE, BIOSRAM_MMIO_SIZE);

	if (!biosram_base)
		return -ENOMEM;

	ret = register_die_notifier(&die_biosram_nb);
	if (ret != 0) {
		printk(KERN_ERR PFX "error %d registering die notifier\n", ret);
		goto err;
	}

	ret = atomic_notifier_chain_register(&panic_notifier_list,
					     &panic_biosram_nb);

	if (ret != 0) {
		printk(KERN_ERR PFX "error %d registering panic handler\n", ret);
		(void) unregister_die_notifier(&die_biosram_nb);
		goto err;
	}


	if (panic_detected()) {
		panic_present = true;

		panic_msg_copy(panic_msg, sizeof(panic_msg));

		panic_msg_clear();

		printk(KERN_INFO PFX "Kernel panic message detected: %s\n", panic_msg);
	}

	ret = sysfs_create_group(firmware_kobj, &attr_group);
	if (ret) {
		printk(KERN_ERR PFX "error %d creating sysfs group\n", ret);
	}

	printk(KERN_INFO PFX "successfully enabled");

	return 0;

err:
	iounmap(biosram_base);
	biosram_base = NULL;

	return ret;
}

static void panic_biosram_disable(void)
{
	if (biosram_base != NULL) {
		iounmap(biosram_base);
		(void) atomic_notifier_chain_unregister(&panic_notifier_list,
			&panic_biosram_nb);
		(void) unregister_die_notifier(&die_biosram_nb);
		sysfs_remove_group(firmware_kobj, &attr_group);
	}
}

static const struct dmi_system_id panic_biosram_dmi_table[] = {
	{
		.ident = "Tesla InfoZ",
		.matches = {
			DMI_MATCH(DMI_PRODUCT_FAMILY, "Tesla_InfoZ"),
		},
		.callback = panic_biosram_enable,
	},
	{ }
};

static int __init panic_biosram_init(void)
{
	dmi_check_system(panic_biosram_dmi_table);
	return 0;
}

static void __exit panic_biosram_exit(void)
{
	panic_biosram_disable();
}

module_init(panic_biosram_init);
module_exit(panic_biosram_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Tesla OpenSource <opensource@tesla.com>");
MODULE_DESCRIPTION("Log panic reason to BIOSRAM region");
