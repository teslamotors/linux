/*
 * arch/arm/mach-tegra/nvdumper.c
 *
 * Copyright (c) 2011-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/errno.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/reboot.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include "../../../arch/arm/mach-tegra/board.h"
#include "nvdumper.h"
#include "nvdumper-footprint.h"

#ifdef CONFIG_TEGRA_USE_NCT
#include "../../../arch/arm/mach-tegra/include/mach/nct.h"
#endif

unsigned long nvdumper_reserved;

static void __init nvdumper_sysfs_init(void);
static void __exit nvdumper_sysfs_exit(void);

#define NVDUMPER_CLEAN      0xf000caf3U
#define NVDUMPER_DIRTY      0x2badfaceU
#define NVDUMPER_DIRTY_DUMP 0xdeadbeefU
#define NVDUMPER_WDT_DUMP   0x2badbeefU

#define RW_MODE (S_IWUSR | S_IRUGO)

static void __iomem *nvdumper_ptr;
static uint32_t nvdumper_last_reboot;

#ifdef CONFIG_ARCH_TEGRA_18x_SOC

#define NV_ADDRESS_MAP_PMC_IMPL_BASE              0x0c360000
#define NV_ADDRESS_MAP_PMC_IMPL_SIZE              0x400 /* map 1k */
#define PMC_IMPL_RAMDUMP_CTL_STATUS_0             0x0000010c
#define PMC_IMPL_RAMDUMP_CTL_STATUS_0_RAMDUMP_EN  0x1

static void enable_hw_ram_dump_ctrl(int enable)
{
	uint32_t value;
	void __iomem *ram_dump_ctrl = ioremap_nocache(
			NV_ADDRESS_MAP_PMC_IMPL_BASE,
			NV_ADDRESS_MAP_PMC_IMPL_SIZE);
	if (!ram_dump_ctrl) {
		pr_info("nvdumper: failed to ioremap memory at 0x%08x\n",
				NV_ADDRESS_MAP_PMC_IMPL_BASE);
		return;
	}

	value = ioread32(ram_dump_ctrl);

	value = value & ~PMC_IMPL_RAMDUMP_CTL_STATUS_0_RAMDUMP_EN;

	if (enable)
		value = value | PMC_IMPL_RAMDUMP_CTL_STATUS_0_RAMDUMP_EN;

	iowrite32(value, ram_dump_ctrl);

	iounmap(ram_dump_ctrl);
}
#endif

static uint32_t get_dirty_state(void)
{
	return ioread32(nvdumper_ptr);
}

static void set_dirty_state(uint32_t state)
{
	pr_info("nvdumper: set_dirty_state 0x%x\n", state);
	iowrite32(state, nvdumper_ptr);

#ifdef CONFIG_ARCH_TEGRA_18x_SOC
	if (state == NVDUMPER_DIRTY_DUMP || state == NVDUMPER_WDT_DUMP) {
		pr_info("nvdumper: preparing wdt\n");
		enable_hw_ram_dump_ctrl(1);
	} else {
		pr_info("nvdumper: cleaning up wdt\n");
		enable_hw_ram_dump_ctrl(0);
	}
#endif
}

static int nvdumper_reboot_cb(struct notifier_block *nb,
		unsigned long event, void *unused)
{
	pr_info("nvdumper: %s cleanly.\n",
		(event == SYS_RESTART) ? "rebooting" : "shutting down");
	set_dirty_state(NVDUMPER_CLEAN);
	return NOTIFY_DONE;
}

static struct notifier_block nvdumper_reboot_notifier = {
	.notifier_call = nvdumper_reboot_cb,
};

static int __init tegra_nvdumper_arg(char *options)
{
	char *p = options;

	nvdumper_reserved = memparse(p, &p);
	pr_info("nvdumper: nvdumper_reserved is first set to 0x%lx.\n",
		nvdumper_reserved);

	return 0;
}
early_param("nvdumper_reserved", tegra_nvdumper_arg);

static int __init nvdumper_init(void)
{
	int ret;

#ifdef CONFIG_TEGRA_USE_NCT
	union nct_item_type *item;
#endif
	if (!nvdumper_reserved)
		return -ENOTSUPP;
	nvdumper_ptr = ioremap_nocache(nvdumper_reserved,
			NVDUMPER_RESERVED_SIZE);
	if (!nvdumper_ptr) {
		pr_info("nvdumper: failed to ioremap memory at 0x%08lx\n",
				nvdumper_reserved);
		return -EIO;
	}
	ret = register_reboot_notifier(&nvdumper_reboot_notifier);
	if (ret)
		goto err_out1;

	ret = nvdumper_regdump_init();
	if (ret)
		goto err_out2;

	nvdumper_dbg_footprint_init();

	nvdumper_last_reboot = get_dirty_state();
	switch (nvdumper_last_reboot) {
	case NVDUMPER_CLEAN:
		pr_info("nvdumper: last reboot was clean\n");
		break;
	case NVDUMPER_DIRTY:
	case NVDUMPER_DIRTY_DUMP:
	case NVDUMPER_WDT_DUMP:
		pr_info("nvdumper: last reboot was dirty\n");
		break;
	default:
		pr_info("nvdumper: last reboot was unknown\n");
		break;
	}

	nvdumper_sysfs_init();

#ifdef CONFIG_TEGRA_USE_NCT
	item = kzalloc(sizeof(*item), GFP_KERNEL);
	if (!item) {
		pr_err("failed to allocate memory\n");
		goto err_out3;
	}

	ret = tegra_nct_read_item(NCT_ID_RAMDUMP, item);
	if (ret < 0) {
		pr_err("%s: NCT read failure\n", __func__);
		kfree(item);
		set_dirty_state(NVDUMPER_CLEAN);
		goto err_out0;
	}

	pr_info("%s: RAMDUMP flag(%d) from NCT\n",
			__func__, item->ramdump.flag);
	if (item->ramdump.flag == 1)
		set_dirty_state(NVDUMPER_DIRTY_DUMP);
	else if (item->ramdump.flag == 2)
		set_dirty_state(NVDUMPER_DIRTY);
	else if (item->ramdump.flag == 3)
		set_dirty_state(NVDUMPER_WDT_DUMP);
	else
		set_dirty_state(NVDUMPER_CLEAN);

	kfree(item);

	return 0;

err_out3:

#else
	set_dirty_state(NVDUMPER_DIRTY);
	return 0;
#endif

err_out2:
	unregister_reboot_notifier(&nvdumper_reboot_notifier);
err_out1:
	iounmap(nvdumper_ptr);

#ifdef CONFIG_TEGRA_USE_NCT /* avoid build error if NCT is not enabled*/
err_out0:
#endif

	return ret;

}

static void __exit nvdumper_exit(void)
{
	nvdumper_sysfs_exit();
	nvdumper_regdump_exit();
	nvdumper_dbg_footprint_exit();
	unregister_reboot_notifier(&nvdumper_reboot_notifier);
	set_dirty_state(NVDUMPER_CLEAN);
	iounmap(nvdumper_ptr);
}

static char *nvdumper_set_str = "dirty_dump";
static struct kobject *nvdumper_kobj;

static ssize_t nvdumper_set_show(struct kobject *kobj,
	struct kobj_attribute *attr,
	char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", nvdumper_set_str);
}

static ssize_t nvdumper_set_store(struct kobject *kobj,
	struct kobj_attribute *attr,
	const char *buf, size_t n)
{
	if (n < 1)
		return 0;

	snprintf(nvdumper_set_str, strlen("dirty_dump"), "%s", buf);
	nvdumper_set_str[n-1] = '\0';

	if (!strcmp(nvdumper_set_str, "clean"))
		set_dirty_state(NVDUMPER_CLEAN);
	else if (!strcmp(nvdumper_set_str, "dirty"))
		set_dirty_state(NVDUMPER_DIRTY);
	else if (!strcmp(nvdumper_set_str, "dirty_dump"))
		set_dirty_state(NVDUMPER_DIRTY_DUMP);
	else if (!strcmp(nvdumper_set_str, "wdt_dump"))
		set_dirty_state(NVDUMPER_WDT_DUMP);
	else
		strcpy(nvdumper_set_str, "unknown");

	pr_info("nvdumper_set was updated to %s\n", nvdumper_set_str);

	return n;
}

static ssize_t nvdumper_prev_show(struct kobject *kobj,
	struct kobj_attribute *attr,
	char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", nvdumper_set_str);
}

static const struct kobj_attribute nvdumper_attr[] = {
	__ATTR(nvdumper_set, 0644, nvdumper_set_show, nvdumper_set_store),
	__ATTR(nvdumper_prev, 0444, nvdumper_prev_show, NULL),
};

static void __init nvdumper_sysfs_init(void)
{
	int i, ret = 0;

	nvdumper_kobj = kobject_create_and_add("nvdumper", kernel_kobj);

	if (!nvdumper_kobj) {
		pr_err("unable to create nvdumper kernel object!\n");
		return;
	}

	/* create sysfs */
	for (i = 0; i < ARRAY_SIZE(nvdumper_attr); i++) {
		ret = sysfs_create_file(nvdumper_kobj, &nvdumper_attr[i].attr);
		if (ret)
			pr_err("failed to create %s\n",
				nvdumper_attr[i].attr.name);
	}

	switch (nvdumper_last_reboot) {
	case NVDUMPER_CLEAN:
		nvdumper_set_str = "clean\n";
		break;
	case NVDUMPER_DIRTY:
		nvdumper_set_str = "dirty\n";
		break;
	case NVDUMPER_DIRTY_DUMP:
		nvdumper_set_str = "dirty_dump\n";
		break;
	default:
		nvdumper_set_str = "dirty\n";
		break;
	}
}

static void __exit nvdumper_sysfs_exit(void)
{
	int i;

	if (!nvdumper_kobj)
		return;

	for (i = 0; i < ARRAY_SIZE(nvdumper_attr); i++)
		sysfs_remove_file(nvdumper_kobj, &nvdumper_attr[i].attr);
}

arch_initcall(nvdumper_init);
module_exit(nvdumper_exit);

MODULE_LICENSE("GPL");
