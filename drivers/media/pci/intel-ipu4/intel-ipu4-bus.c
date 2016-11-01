/*
 * Copyright (c) 2013--2016 Intel Corporation.
 *
 * Bus implementation based on the bt8xx driver.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/iommu.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/pci.h>
#include <linux/pm_runtime.h>
#include <linux/sizes.h>

#include "intel-ipu4.h"
#include "intel-ipu4-dma.h"
#include "intel-ipu4-mmu.h"

#ifdef CONFIG_PM
static struct bus_type intel_ipu4_bus;

static int bus_pm_suspend_child_dev(struct device *dev, void *p)
{
	struct intel_ipu4_bus_device *adev = to_intel_ipu4_bus_device(dev);
	struct device *parent = (struct device *)p;

	if (!intel_ipu4_bus_get_drvdata(adev))
		return 0;	/* Device not attached to any driver yet */

	if (dev->parent != parent || adev->ctrl)
		return 0;

	return pm_generic_runtime_suspend(dev);
}

static int bus_pm_runtime_suspend(struct device *dev)
{
	struct intel_ipu4_bus_device *adev = to_intel_ipu4_bus_device(dev);
	int rval;

	if (!adev->ctrl) {
		dev_dbg(dev, "has no buttress control info, bailing out\n");
		return 0;
	}

	rval = bus_for_each_dev(&intel_ipu4_bus, NULL, dev,
				bus_pm_suspend_child_dev);
	if (rval) {
		dev_err(dev, "failed to suspend child device\n");
		return rval;
	}

	rval = pm_generic_runtime_suspend(dev);
	if (rval)
		return rval;

	rval = intel_ipu4_buttress_power(dev, adev->ctrl, false);
	dev_dbg(dev, "%s: buttress power down %d\n", __func__, rval);
	if (!rval)
		return 0;

	dev_err(dev, "power down failed!\n");

	/* Powering down failed, attempt to resume device now */
	rval = pm_generic_runtime_resume(dev);
	if (!rval)
		return -EBUSY;

	return -EIO;
}

static int bus_pm_resume_child_dev(struct device *dev, void *p)
{
	struct intel_ipu4_bus_device *adev = to_intel_ipu4_bus_device(dev);
	struct device *parent = (struct device *)p;
	int r;

	if (!intel_ipu4_bus_get_drvdata(adev))
		return 0;	/* Device not attached to any driver yet */

	if (dev->parent != parent || adev->ctrl)
		return 0;

	mutex_lock(&adev->resume_lock);
	r = pm_generic_runtime_resume(dev);
	mutex_unlock(&adev->resume_lock);
	return r;
}

static int bus_pm_runtime_resume(struct device *dev)
{
	struct intel_ipu4_bus_device *adev = to_intel_ipu4_bus_device(dev);
	int rval;

	if (!adev->ctrl) {
		dev_dbg(dev, "has no buttress control info, bailing out\n");
		return 0;
	}

	rval = intel_ipu4_buttress_power(dev, adev->ctrl, true);
	dev_dbg(dev, "%s: buttress power up %d\n", __func__, rval);
	if (rval)
		return rval;

	rval = pm_generic_runtime_resume(dev);
	dev_dbg(dev, "%s: resume %d\n", __func__, rval);
	if (rval)
		goto out_err;

	/*
	 * It needs to be ensured that IPU4 child devices' resume/suspend are
	 * called only when the child devices' power is turned on/off by the
	 * parent device here. Therefore, children's suspend/resume are called
	 * from here, because that is the only way to guarantee it.
	 */
	rval = bus_for_each_dev(&intel_ipu4_bus, NULL, dev,
				bus_pm_resume_child_dev);
	if (rval) {
		dev_err(dev, "failed to resume child device - reset it\n");

		rval = pm_generic_runtime_suspend(dev);
		dev_dbg(dev, "%s: suspend %d\n", __func__, rval);

		rval = intel_ipu4_buttress_power(dev, adev->ctrl, false);
		dev_dbg(dev, "%s: buttress power down %d\n", __func__, rval);
		if (rval)
			return rval;

		usleep_range(1000, 1100);

		rval = intel_ipu4_buttress_power(dev, adev->ctrl, true);
		dev_dbg(dev, "%s: buttress power up %d\n", __func__, rval);
		if (rval)
			return rval;

		rval = pm_generic_runtime_resume(dev);
		dev_dbg(dev, "%s: re-resume %d\n", __func__, rval);
		if (rval)
			goto out_err;

		rval = bus_for_each_dev(&intel_ipu4_bus, NULL, dev,
					bus_pm_resume_child_dev);

		if (rval) {
			dev_err(dev, "resume retry failed\n");
			goto out_err;
		}
	}

	return 0;

out_err:
	if (adev->ctrl)
		intel_ipu4_buttress_power(dev, adev->ctrl, false);

	return -EBUSY;
}

const struct dev_pm_ops intel_ipu4_bus_pm_ops = {
	.runtime_suspend = bus_pm_runtime_suspend,
	.runtime_resume = bus_pm_runtime_resume,
};

#define INTEL_IPU4_BUS_PM_OPS	(&intel_ipu4_bus_pm_ops)
#else
#define INTEL_IPU4_BUS_PM_OPS	NULL
#endif

static int intel_ipu4_bus_match(struct device *dev, struct device_driver *drv)
{
	struct intel_ipu4_bus_driver *adrv = to_intel_ipu4_bus_driver(drv);

	dev_dbg(dev, "bus match: \"%s\" --- \"%s\"\n", dev_name(dev),
		adrv->wanted);

	return !strncmp(dev_name(dev), adrv->wanted, strlen(adrv->wanted));
}

static struct intel_ipu4_dma_mapping *alloc_dma_mapping(struct device *dev)
{
	struct intel_ipu4_dma_mapping *dmap;

	dmap = kzalloc(sizeof(*dmap), GFP_KERNEL);
	if (!dmap)
		return NULL;

	dmap->domain = iommu_domain_alloc(dev->bus);
	if (!dmap->domain) {
		kfree(dmap);
		return NULL;
	}

	init_iova_domain(&dmap->iovad, SZ_4K, 1,
			 dma_get_mask(dev) >> PAGE_SHIFT);

	kref_init(&dmap->ref);

	pr_debug("alloc mapping\n");

	iova_cache_get();

	return dmap;
}

static void free_dma_mapping(void *ptr)
{
	struct intel_ipu4_mmu *mmu = ptr;
	struct intel_ipu4_dma_mapping *dmap = mmu->dmap;

	iommu_domain_free(dmap->domain);
	mmu->set_mapping(mmu, NULL);
	iova_cache_put();
	put_iova_domain(&dmap->iovad);
	kfree(dmap);
}

static struct iommu_group *intel_ipu4_bus_get_group(struct device *dev)
{
	struct device *aiommu = to_intel_ipu4_bus_device(dev)->iommu;
	struct intel_ipu4_mmu *mmu = dev_get_drvdata(aiommu);
	struct iommu_group *group;
	struct intel_ipu4_dma_mapping *dmap;

	if (!mmu) {
		dev_err(dev, "%s: no iommu available\n", __func__);
		return NULL;
	}

	group = iommu_group_get(dev);
	if (group)
		return group;

	group = iommu_group_alloc();
	if (!group) {
		dev_err(dev, "%s: can't alloc iommu group\n", __func__);
		return NULL;
	}

	dmap = alloc_dma_mapping(dev);
	if (!dmap) {
		dev_err(dev, "%s: can't alloc dma mapping\n", __func__);
		iommu_group_put(group);
		return NULL;
	}

	iommu_group_set_iommudata(group, mmu, free_dma_mapping);

	/*
	 * Turn mmu on and off synchronously. Otherwise it may still be on
	 * at psys / isys probing phase and that may cause problems on
	 * develoment environments. Mostly required in FPGA
	 * environments but doesn't really hurt in real SOC.
	 */
	pm_runtime_get_sync(aiommu);
	mmu->set_mapping(mmu, dmap);
	pm_runtime_put_sync(aiommu);

	return group;
}

static int intel_ipu4_bus_probe(struct device *dev)
{
	struct intel_ipu4_bus_device *adev = to_intel_ipu4_bus_device(dev);
	struct intel_ipu4_bus_driver *adrv =
		to_intel_ipu4_bus_driver(dev->driver);
	struct iommu_group *group = NULL;
	int rval;

	dev_dbg(dev, "bus probe dev %s\n", dev_name(dev));

	if (adev->iommu) {
		dev_dbg(dev, "iommu %s\n", dev_name(adev->iommu));

		group = intel_ipu4_bus_get_group(dev);
		if (!group)
			return -EPROBE_DEFER;

		rval = iommu_group_add_device(group, dev);
		if (rval)
			goto out_err;
	}

	adev->adrv = adrv;
	if (adrv->probe) {
		rval = adrv->probe(adev);
		if (!rval) {
			/*
			 * If the device power, after probe, is enabled
			 * (from the parent device), its resume needs to
			 * be called to initialize the device properly.
			 */
			if (!adev->ctrl &&
			    !pm_runtime_status_suspended(dev->parent)) {
				mutex_lock(&adev->resume_lock);
				pm_generic_runtime_resume(dev);
				mutex_unlock(&adev->resume_lock);
			}
		}
	} else {
		rval = -ENODEV;
	}

	if (rval)
		goto out_err;

	return 0;

out_err:
	intel_ipu4_bus_set_drvdata(adev, NULL);
	adev->adrv = NULL;
	iommu_group_remove_device(dev);
	iommu_group_put(group);
	return rval;
}

static int intel_ipu4_bus_remove(struct device *dev)
{
	struct intel_ipu4_bus_device *adev = to_intel_ipu4_bus_device(dev);
	struct intel_ipu4_bus_driver *adrv =
		to_intel_ipu4_bus_driver(dev->driver);

	if (adrv->remove)
		adrv->remove(adev);

	if (adev->iommu)
		iommu_group_remove_device(dev);

	return 0;
}

static struct bus_type intel_ipu4_bus = {
	.name = INTEL_IPU4_BUS_NAME,
	.match = intel_ipu4_bus_match,
	.probe = intel_ipu4_bus_probe,
	.remove = intel_ipu4_bus_remove,
	.pm = INTEL_IPU4_BUS_PM_OPS,
};

static struct mutex intel_ipu4_bus_mutex;

static void intel_ipu4_bus_release(struct device *dev)
{
	struct intel_ipu4_bus_device *adev = to_intel_ipu4_bus_device(dev);

	kfree(adev);
}

struct intel_ipu4_bus_device *intel_ipu4_bus_add_device(
	struct pci_dev *pdev, struct device *parent, void *pdata,
	struct device *iommu, struct intel_ipu4_buttress_ctrl *ctrl, char *name,
	unsigned int nr)
{
	struct intel_ipu4_bus_device *adev;
	struct intel_ipu4_device *isp = pci_get_drvdata(pdev);
	int rval;

	adev = kzalloc(sizeof(*adev), GFP_KERNEL);
	if (!adev)
		return ERR_PTR(-ENOMEM);

	adev->dev.parent = parent;
	adev->dev.bus = &intel_ipu4_bus;
	adev->dev.release = intel_ipu4_bus_release;
	adev->dev.archdata.dma_ops = &intel_ipu4_dma_ops;
	adev->dma_mask = DMA_BIT_MASK(isp->secure_mode ?
				      INTEL_IPU4_MMU_ADDRESS_BITS :
				      INTEL_IPU4_MMU_ADDRESS_BITS_NON_SECURE);
	adev->dev.dma_mask = &adev->dma_mask;
	adev->iommu = iommu;
	adev->ctrl = ctrl;
	adev->pdata = pdata;
	adev->isp = isp;
	mutex_init(&adev->resume_lock);
	dev_set_name(&adev->dev, "%s%d", name, nr);

	rval = device_register(&adev->dev);
	if (rval) {
		put_device(&adev->dev);
		return ERR_PTR(rval);
	}

	pm_runtime_allow(&adev->dev);
	pm_runtime_enable(&adev->dev);

	mutex_lock(&intel_ipu4_bus_mutex);
	list_add(&adev->list, &isp->devices);
	mutex_unlock(&intel_ipu4_bus_mutex);

	return adev;
}

void intel_ipu4_bus_del_devices(struct pci_dev *pdev)
{
	struct intel_ipu4_device *isp = pci_get_drvdata(pdev);
	struct intel_ipu4_bus_device *adev, *save;

	mutex_lock(&intel_ipu4_bus_mutex);

	list_for_each_entry_safe(adev, save, &isp->devices, list) {
		list_del(&adev->list);
		device_unregister(&adev->dev);
	}

	mutex_unlock(&intel_ipu4_bus_mutex);
}

int intel_ipu4_bus_register_driver(struct intel_ipu4_bus_driver *adrv)
{
	adrv->drv.bus = &intel_ipu4_bus;
	return driver_register(&adrv->drv);
}
EXPORT_SYMBOL(intel_ipu4_bus_register_driver);

int intel_ipu4_bus_unregister_driver(struct intel_ipu4_bus_driver *adrv)
{
	driver_unregister(&adrv->drv);
	return 0;
}
EXPORT_SYMBOL(intel_ipu4_bus_unregister_driver);

int intel_ipu4_bus_register(void)
{
	mutex_init(&intel_ipu4_bus_mutex);
	return bus_register(&intel_ipu4_bus);
}
EXPORT_SYMBOL(intel_ipu4_bus_register);

void intel_ipu4_bus_unregister(void)
{
	mutex_destroy(&intel_ipu4_bus_mutex);
	return bus_unregister(&intel_ipu4_bus);
}
EXPORT_SYMBOL(intel_ipu4_bus_unregister);

int intel_ipu4_bus_set_iommu(struct iommu_ops *ops)
{
	if (iommu_present(&intel_ipu4_bus))
		return 0;

	return bus_set_iommu(&intel_ipu4_bus, ops);
}
EXPORT_SYMBOL(intel_ipu4_bus_set_iommu);

static int flr_rpm_recovery(struct device *dev, void *p)
{
	dev_dbg(dev, "FLR recovery call");
	/*
	 * We are not necessarily going through device from child to
	 * parent. runtime PM refuses to change state for parent if the child
	 * is still active. At FLR (full reset for whole IPU) that doesn't
	 * matter. Everything has been power gated by HW during the FLR cycle
	 * and we are just cleaning up SW state. Thus, ignore child during
	 * set_suspended.
	 */
	pm_suspend_ignore_children(dev, true);
	pm_runtime_set_suspended(dev);
	pm_suspend_ignore_children(dev, false);

	return 0;
}

int intel_ipu4_bus_flr_recovery(void)
{
	bus_for_each_dev(&intel_ipu4_bus, NULL, NULL,
			 flr_rpm_recovery);
	return 0;
}
EXPORT_SYMBOL(intel_ipu4_bus_flr_recovery);
