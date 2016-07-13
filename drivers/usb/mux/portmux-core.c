/**
 * intel_mux.c - USB Port Mux support
 *
 * Copyright (C) 2016 Intel Corporation
 *
 * Author: Lu Baolu <baolu.lu@linux.intel.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/slab.h>
#include <linux/notifier.h>
#include <linux/extcon.h>
#include <linux/err.h>
#include <linux/usb/portmux.h>

static int usb_mux_change_state(struct portmux_dev *pdev, int state)
{
	int ret;
	struct device *dev = &pdev->dev;

	dev_WARN_ONCE(dev,
		      !mutex_is_locked(&pdev->mux_mutex),
		      "mutex is unlocked\n");

	pdev->mux_state = state;

	if (pdev->mux_state)
		ret = pdev->desc->ops->cable_set_cb(pdev->dev.parent);
	else
		ret = pdev->desc->ops->cable_unset_cb(pdev->dev.parent);

	return ret;
}

static int usb_mux_notifier(struct notifier_block *nb,
			    unsigned long event, void *ptr)
{
	struct portmux_dev *pdev;
	int state;
	int ret = NOTIFY_DONE;

	pdev = container_of(nb, struct portmux_dev, nb);

	state = extcon_get_cable_state_(pdev->edev, EXTCON_USB_HOST);
	if (state < 0)
		return state;

	mutex_lock(&pdev->mux_mutex);
	ret = usb_mux_change_state(pdev, state);
	mutex_unlock(&pdev->mux_mutex);

	return ret;
}

static ssize_t state_show(struct device *dev,
			  struct device_attribute *attr,
			  char *buf)
{
	struct portmux_dev *pdev = dev_get_drvdata(dev);

	return sprintf(buf, "%s\n", pdev->mux_state ? "host" : "peripheral");
}

static ssize_t state_store(struct device *dev,
			   struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct portmux_dev *pdev = dev_get_drvdata(dev);
	int state;

	if (sysfs_streq(buf, "peripheral"))
		state = 0;
	else if (sysfs_streq(buf, "host"))
		state = 1;
	else
		return -EINVAL;

	mutex_lock(&pdev->mux_mutex);
	usb_mux_change_state(pdev, state);
	mutex_unlock(&pdev->mux_mutex);

	return count;
}
static DEVICE_ATTR_RW(state);

static ssize_t name_show(struct device *dev,
			 struct device_attribute *attr,
			 char *buf)
{
	struct portmux_dev *pdev = dev_get_drvdata(dev);

	return sprintf(buf, "%s\n", pdev->desc->name);
}
static DEVICE_ATTR_RO(name);

static struct attribute *portmux_attrs[] = {
	&dev_attr_state.attr,
	&dev_attr_name.attr,
	NULL,
};

static struct attribute_group portmux_attr_grp = {
	.attrs = portmux_attrs,
};

static const struct attribute_group *portmux_group[] = {
	&portmux_attr_grp,
	NULL,
};

static void portmux_release(struct device *dev)
{
	/* dummy release() */
}

/**
 * portmux_register - register a port mux
 * @dev: device the mux belongs to
 * @desc: the descriptor of this port mux
 *
 * Called by port mux drivers to register a mux.
 * Returns a valid pointer to struct portmux_dev on success
 * or an ERR_PTR() on error.
 */
struct portmux_dev *portmux_register(struct portmux_desc *desc)
{
	static atomic_t portmux_no = ATOMIC_INIT(-1);
	struct portmux_dev *pdev;
	struct extcon_dev *edev = NULL;
	struct device *dev;
	int ret;

	/* parameter sanity check */
	if (!desc || !desc->name || !desc->ops || !desc->dev ||
	    !desc->ops->cable_set_cb || !desc->ops->cable_unset_cb)
		return ERR_PTR(-EINVAL);

	dev = desc->dev;

	if (desc->extcon_name) {
		edev = extcon_get_extcon_dev(desc->extcon_name);
		if (IS_ERR_OR_NULL(edev))
			return ERR_PTR(-EPROBE_DEFER);
	}

	pdev = kzalloc(sizeof(*pdev), GFP_KERNEL);
	if (!pdev)
		return ERR_PTR(-ENOMEM);

	pdev->desc = desc;
	pdev->edev = edev;
	pdev->nb.notifier_call = usb_mux_notifier;
	mutex_init(&pdev->mux_mutex);

	pdev->dev.parent = dev;
	dev_set_name(&pdev->dev, "portmux.%lu",
		     (unsigned long)atomic_inc_return(&portmux_no));
	pdev->dev.groups = portmux_group;
	pdev->dev.release = portmux_release;
	ret = device_register(&pdev->dev);
	if (ret)
		goto cleanup_mem;

	dev_set_drvdata(&pdev->dev, pdev);

	if (edev) {
		ret = extcon_register_notifier(edev, EXTCON_USB_HOST,
					       &pdev->nb);
		if (ret < 0) {
			dev_err(dev, "failed to register extcon notifier\n");
			goto cleanup_dev;
		}
	}

	if (desc->initial_state == -1) {
		usb_mux_notifier(&pdev->nb, 0, NULL);
	} else {
		mutex_lock(&pdev->mux_mutex);
		ret = usb_mux_change_state(pdev, !!desc->initial_state);
		mutex_unlock(&pdev->mux_mutex);
	}

	return pdev;

cleanup_dev:
	device_unregister(&pdev->dev);
cleanup_mem:
	kfree(pdev);

	return ERR_PTR(ret);
}
EXPORT_SYMBOL_GPL(portmux_register);

/**
 * portmux_unregister - unregister a port mux
 * @pdev: the port mux device
 *
 * Called by port mux drivers to release a mux.
 */
void portmux_unregister(struct portmux_dev *pdev)
{
	if (pdev->edev)
		extcon_unregister_notifier(pdev->edev,
					   EXTCON_USB_HOST, &pdev->nb);
	device_unregister(&pdev->dev);
	kfree(pdev);
}
EXPORT_SYMBOL_GPL(portmux_unregister);

#ifdef CONFIG_PM_SLEEP
/**
 * portmux_complete - refresh port state during system resumes back
 * @pdev: the port mux device
 *
 * Called by port mux drivers to refresh port state during system
 * resumes back.
 */
void portmux_complete(struct portmux_dev *pdev)
{
	if (pdev->edev) {
		usb_mux_notifier(&pdev->nb, 0, NULL);
	} else {
		mutex_lock(&pdev->mux_mutex);
		usb_mux_change_state(pdev, pdev->mux_state);
		mutex_unlock(&pdev->mux_mutex);
	}
}
EXPORT_SYMBOL_GPL(portmux_complete);
#endif
