/*
 * Copyright (c) 2016-2017, NVIDIA CORPORATION. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */
#include <linux/module.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/tegra-firmwares.h>

struct tegrafw_data {
	u32	flags;
	ssize_t	(*reader)(struct device *dev, char *data, size_t size);
	char	*saved_string;
	struct mutex lock;
};

void tegrafw_invalidate(struct device *fwdev)
{
	struct tegrafw_data *tfw = fwdev->platform_data;

	if (!tfw->reader) {
		dev_warn(fwdev, "attempt to invalidate when reader is NULL\n");
		return;
	}
	mutex_lock(&tfw->lock);
	if (tfw->saved_string)
		devm_kfree(fwdev, tfw->saved_string);
	tfw->saved_string = NULL;
	mutex_unlock(&tfw->lock);
}

static ssize_t version_show(struct device *fwdev,
			struct device_attribute *attr, char *buf)
{
	struct tegrafw_data *tfw = fwdev->platform_data;
	char *version_string;
	char reader_string[TFW_VERSION_MAX_SIZE];
	ssize_t r;

	mutex_lock(&tfw->lock);
	if (tfw->saved_string) {
		version_string = tfw->saved_string;
	} else {
		tfw->reader(fwdev->parent,
			reader_string, sizeof(reader_string));
		version_string = reader_string;
		if ((tfw->flags & TFW_DONT_CACHE) == 0) {
			if (tfw->saved_string)
				devm_kfree(fwdev, tfw->saved_string);
			tfw->saved_string = devm_kstrdup(fwdev,
				version_string, GFP_KERNEL);
		}
	}
	r = snprintf(buf, PAGE_SIZE, "%s: %s\n",
		dev_name(fwdev), version_string);
	mutex_unlock(&tfw->lock);
	return r;
}

static int versions_show_one(struct device *fwdev, void *buf)
{
	if (version_show(fwdev, NULL, buf + strlen(buf)) > 0)
		return 0;
	else
		return -ENOMEM;
}

static ssize_t versions_show(struct class *cls,
	struct class_attribute *attr, char *buf)
{
	buf[0] = '\0';
	class_for_each_device(cls, NULL, buf, versions_show_one);
	return strlen(buf);
}

static DEVICE_ATTR_RO(version);

static struct class_attribute tegrafw_class_attrs[] = {
	__ATTR(versions, S_IRUGO, versions_show, NULL),
	__ATTR_NULL,
};

static struct attribute *tegrafw_attrs[] = {
	&dev_attr_version.attr,
	NULL,
};

static struct attribute_group tegrafw_attr_group = {
	.attrs = tegrafw_attrs,
};

static const struct attribute_group *tegrafw_attr_groups[] = {
	&tegrafw_attr_group,
	NULL,
};

static void tegrafw_release(struct device *dev)
{
	kfree(dev);
}

static bool tegrafw_class_registered;

static struct class tegrafw_class = {
	.name = "tegra-firmware",
	.owner = THIS_MODULE,
	.class_attrs = tegrafw_class_attrs,
};


/**
 * tegrafw_register - register the firmware object
 *
 * @name: the firmware symbolic name, will be created under tegra-firmwares
 * @flags: only %TFW_DONT_CACHE is supported for now - always call @reader
 * @reader: function to print version info
 * @string: predefined string to return as a version
 *
 * Return value: error code if IS_ERR(), otherwise valid firmware object.
 *               Needs to be deallocated with call to tegrafw_unregister()
 */
struct device *tegrafw_register(const char *name,
			const u32 flags,
			ssize_t (*reader)(struct device *, char *, size_t),
			const char *string)
{
	struct device *fwdev = NULL;
	int err;
	struct tegrafw_data *tfw;

	if (!name) {
		pr_err("%s: attempt to register unnamed firmware",
			__func__);
		err = -EINVAL;
		goto out;
	}
	if ((flags & TFW_DONT_CACHE) && !reader) {
		pr_err("%s: don't cache and no reader?\n",
			name);
		err = -EINVAL;
		goto out;
	}
	if (reader && string)
		pr_info("%s: string will be used instead of calls to reader",
			name);

	fwdev = kzalloc(sizeof(*fwdev) + sizeof(*tfw), GFP_KERNEL);
	if (!fwdev) {
		err = -ENOMEM;
		goto out;
	}

	if (!tegrafw_class_registered) {
		class_register(&tegrafw_class);
		tegrafw_class_registered = true;
	}

	device_initialize(fwdev);
	dev_set_name(fwdev, "%s", name);
	tfw = (struct tegrafw_data *)(fwdev + 1);
	mutex_init(&tfw->lock);
	tfw->flags = flags;
	tfw->reader = reader;
	tfw->saved_string = string ?
		devm_kstrdup(fwdev, string, GFP_KERNEL) : NULL;
	fwdev->platform_data = tfw;
	fwdev->release = tegrafw_release;
	fwdev->class = &tegrafw_class;
	fwdev->groups = tegrafw_attr_groups;

	err = device_add(fwdev);
	if (err < 0)
		goto out;

	return fwdev;
out:
	put_device(fwdev);
	return ERR_PTR(err);
}
EXPORT_SYMBOL(tegrafw_register);

/**
 * tegrafw_unregister - release the firmware object
 *
 * @dev: device object returned by previous call to tegrafw_register()
 */
void tegrafw_unregister(struct device *fwdev)
{
	if (!IS_ERR_OR_NULL(fwdev))
		device_unregister(fwdev);
}
EXPORT_SYMBOL(tegrafw_unregister);

/*
 * devm_tegrafw_match - used by the devres framework to search devres
 *                      that matches the data
 * For tegrafw devres, resource holds the pointer to the data, so
 * simple *res = data will work.
 * Neither res nor *res should be NULL, in this case we return 0 to indicate
 * mismatch
 */
static int devm_tegrafw_match(struct device *dev, void *res, void *data)
{
	struct device **p = res;

	if (WARN_ON(!p || !*p))
		return 0;
	return *p == data;
}

static void devm_tegrafw_release(struct device *dev, void *res)
{
	tegrafw_unregister(*(struct device **)res);
}

/**
 * devm_tegrafw_regsiter - register "managed" firmware
 *
 * @dev: owner of the firmware
 * @name: symbolic name. If %NULL, device name will be used
 * @flags: only %TFW_DONT_CACHE is supported right now - always call @reader
 * @reader: function to print version info
 * @string: predefined string to return as a version
 *
 * Return: struct device or appropriate error code wrapped in ERR_PTR()
 */
struct device *devm_tegrafw_register(struct device *dev, const char *name,
			const u32 flags,
			ssize_t (*reader)(struct device *, char *, size_t),
			const char *string)
{
	struct device **ptr, *fwdev;

	if (dev == NULL)
		return tegrafw_register(name, flags, reader, string);

	ptr = devres_alloc(devm_tegrafw_release, sizeof(*ptr), GFP_KERNEL);
	if (!ptr)
		return ERR_PTR(-ENOMEM);

	fwdev = tegrafw_register(name ?: dev_name(dev), flags, reader, string);
	if (!IS_ERR(fwdev)) {
		*ptr = fwdev;
		devres_add(dev, ptr);
		/* set device parent */
		device_move(fwdev, dev, DPM_ORDER_NONE);
	} else {
		devres_free(ptr);
	}
	return fwdev;
}
EXPORT_SYMBOL(devm_tegrafw_register);

/**
 * devm_tegrafw_unregister - unregister "managed" firmware
 * @dev: owner of the firmware
 * @fwdev: object returned by devm_tegrafw_register()
 *
 * One usually does not need it, as unregistering will take place automatically
 */
void devm_tegrafw_unregister(struct device *dev, struct device *fwdev)
{
	WARN_ON(devres_release(dev, devm_tegrafw_release,
				devm_tegrafw_match, fwdev));
}
EXPORT_SYMBOL(devm_tegrafw_unregister);

struct device *devm_tegrafw_register_dt_string(struct device *dev,
						const char *name,
						const char *path,
						const char *property)
{
	struct device_node *dn;
	const char *data;

	dn = of_find_node_by_path(path);
	if (!dn)
		return ERR_PTR(-ENODEV);
	if (of_property_read_string(dn, property, &data))
		return ERR_PTR(-ENOENT);
	of_node_put(dn);
	return devm_tegrafw_register(dev, name, TFW_NORMAL, NULL, data);
}
EXPORT_SYMBOL(devm_tegrafw_register_dt_string);

static void tegrafw_exit(void)
{
	if (tegrafw_class_registered)
		class_unregister(&tegrafw_class);
}

module_exit(tegrafw_exit);
MODULE_AUTHOR("dmitry pervushin <dpervushin@nvidia.com>");
MODULE_DESCRIPTION("Tegra firmwares inventory");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:tegra-firmwares");
