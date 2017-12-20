/* drivers/misc/sysfs-platdev.c
 *
 * Compatibility for old platform-device sysfs from OF devices.
 *
 * Copyright 2016 Codethink Ltd.
 *	Ben Dooks <ben.dooks@codethink.co.uk>
 *	Edward Cragg <edward.cragg@codethink.co.uk>
 *
 * Licensed under GPL v2.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/of.h>

#include "../base/base.h"

static struct platform_device dummy_pdev = {
	.name	= "dummy_pdev",
	.id	= -1,
};

static int sysfs_platdev_add(struct device *dev)
{
	struct device_node *of = dev_of_node(dev);
	struct bus_type *bus = dev->bus;
	struct kobject *kobj_plat;
	const char *name = NULL;
	int ret;

	ret = of_property_read_string(of, "linux,sysfs-compat-name", &name);
	if (ret) {
		dev_dbg(dev, "no compatibility info, skipping\n");
		return 0;
	}

	if (!dummy_pdev.dev.kobj.parent) {
		dev_err(dev, "no platform device registered\n");
		return -EINVAL;
	}

	dev_dbg(dev, "creating symlink for '%s'\n", name);
	kobj_plat = dummy_pdev.dev.kobj.parent;
	ret = sysfs_create_link(kobj_plat, &dev->kobj, name);
	if (ret)
		return ret;

	if (dev->class) {
		dev_dbg(dev, "%s: creating class symlink\n", __func__);
		ret = sysfs_create_link(&dev->class->p->subsys.kobj,
					&dev->kobj, name);
		if (ret)
			goto err1;
	}

	if (bus) {
		dev_dbg(dev, "%s: creating bus symlink\n", __func__);
		ret = sysfs_create_link(&bus->p->devices_kset->kobj,
					&dev->kobj, name);
		if (ret)
			goto err2;
	}

	dev_info(dev, "created link for '%s'\n", name);
	return 0;

err2:
	if (dev->class)
		sysfs_remove_link(&dev->class->p->subsys.kobj, name);
err1:
	sysfs_remove_link(kobj_plat, name);
	return ret;
}

static int sysfs_platdev_delete(struct device *dev)
{
	struct device_node *of = dev_of_node(dev);
	struct kobject *kobj_plat;
	const char *name = NULL;
	int ret;

	ret = of_property_read_string(of, "linux,sysfs-compat-name", &name);
	if (ret) {
		dev_dbg(dev, "no compatibility info, skipping\n");
		return 0;
	}

	kobj_plat = dummy_pdev.dev.kobj.parent;
	sysfs_remove_link(kobj_plat, name);

	if (dev->class)
		sysfs_remove_link(&dev->class->p->subsys.kobj, name);

	if (dev->bus)
		sysfs_remove_link(&dev->bus->p->devices_kset->kobj, name);

	dev_info(dev, "removed link for '%s'\n", name);
	return 0;
}

static int sysfs_platdev_notifier(struct notifier_block *nb,
				  unsigned long action, void *data)
{
	struct device *dev = data;
	int rc = 0;

	dev_dbg(dev, "%s: action %lu\n", __func__, action);

	if (!dev_of_node(dev)) {
		dev_dbg(dev, "device has no of-node");
		return 0;
	}

	switch (action) {
	case BUS_NOTIFY_REMOVED_DEVICE:
		rc = sysfs_platdev_delete(dev);
		if (rc)
			dev_err(dev, "failed to add remove link (%d)\n", rc);
		break;
	case BUS_NOTIFY_ADD_DEVICE:
		rc = sysfs_platdev_add(dev);
		if (rc)
			dev_err(dev, "failed to add sysfs link (%d)\n", rc);
		break;
	}

	return rc;
}

static struct notifier_block sysfs_platdev_nb = {
	.notifier_call = sysfs_platdev_notifier,
};

static int __init sysfs_platdev_init(void)
{
	int ret;

	ret = platform_device_register(&dummy_pdev);
	if (ret) {
		pr_err("%s: failed to register device (%d)\n", __func__, ret);
		return -EINVAL;
	}

	bus_register_notifier(&platform_bus_type, &sysfs_platdev_nb);
	return 0;
}

static void __exit sysfs_platdev_exit(void)
{
	platform_device_unregister(&dummy_pdev);
	bus_unregister_notifier(&platform_bus_type, &sysfs_platdev_nb);
}

postcore_initcall(sysfs_platdev_init);
module_exit(sysfs_platdev_exit);

