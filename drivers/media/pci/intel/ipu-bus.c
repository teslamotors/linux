// SPDX-License_Identifier: GPL-2.0
// Copyright (C) 2013 - 2018 Intel Corporation

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

#include "ipu.h"
#include "ipu-platform.h"
#include "ipu-dma.h"
#include "ipu-mmu.h"

#ifdef CONFIG_PM
static struct bus_type ipu_bus;

static int bus_pm_suspend_child_dev(struct device *dev, void *p)
{
	struct ipu_bus_device *adev = to_ipu_bus_device(dev);
	struct device *parent = (struct device *)p;

	if (!ipu_bus_get_drvdata(adev))
		return 0;	/* Device not attached to any driver yet */

	if (dev->parent != parent || adev->ctrl)
		return 0;

	return pm_generic_runtime_suspend(dev);
}

static int bus_pm_runtime_suspend(struct device *dev)
{
	struct ipu_bus_device *adev = to_ipu_bus_device(dev);
	int rval;

	if (!adev->ctrl) {
		dev_dbg(dev, "has no buttress control info, bailing out\n");
		return 0;
	}

	rval = bus_for_each_dev(&ipu_bus, NULL, dev, bus_pm_suspend_child_dev);
	if (rval) {
		dev_err(dev, "failed to suspend child device\n");
		return rval;
	}

	rval = pm_generic_runtime_suspend(dev);
	if (rval)
		return rval;

	rval = ipu_buttress_power(dev, adev->ctrl, false);
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
	struct ipu_bus_device *adev = to_ipu_bus_device(dev);
	struct device *parent = (struct device *)p;
	int r;

	if (!ipu_bus_get_drvdata(adev))
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
	struct ipu_bus_device *adev = to_ipu_bus_device(dev);
	int rval;

	if (!adev->ctrl) {
		dev_dbg(dev, "has no buttress control info, bailing out\n");
		return 0;
	}

	rval = ipu_buttress_power(dev, adev->ctrl, true);
	dev_dbg(dev, "%s: buttress power up %d\n", __func__, rval);
	if (rval)
		return rval;

	rval = pm_generic_runtime_resume(dev);
	dev_dbg(dev, "%s: resume %d\n", __func__, rval);
	if (rval)
		goto out_err;

	/*
	 * It needs to be ensured that IPU child devices' resume/suspend are
	 * called only when the child devices' power is turned on/off by the
	 * parent device here. Therefore, children's suspend/resume are called
	 * from here, because that is the only way to guarantee it.
	 */
	rval = bus_for_each_dev(&ipu_bus, NULL, dev, bus_pm_resume_child_dev);
	if (rval) {
		dev_err(dev, "failed to resume child device - reset it\n");

		rval = pm_generic_runtime_suspend(dev);
		dev_dbg(dev, "%s: suspend %d\n", __func__, rval);

		rval = ipu_buttress_power(dev, adev->ctrl, false);
		dev_dbg(dev, "%s: buttress power down %d\n", __func__, rval);
		if (rval)
			return rval;

		usleep_range(1000, 1100);

		rval = ipu_buttress_power(dev, adev->ctrl, true);
		dev_dbg(dev, "%s: buttress power up %d\n", __func__, rval);
		if (rval)
			return rval;

		rval = pm_generic_runtime_resume(dev);
		dev_dbg(dev, "%s: re-resume %d\n", __func__, rval);
		if (rval)
			goto out_err;

		rval = bus_for_each_dev(&ipu_bus, NULL, dev,
					bus_pm_resume_child_dev);

		if (rval) {
			dev_err(dev, "resume retry failed\n");
			goto out_err;
		}
	}

	return 0;

out_err:
	if (adev->ctrl)
		ipu_buttress_power(dev, adev->ctrl, false);

	return -EBUSY;
}

static const struct dev_pm_ops ipu_bus_pm_ops = {
	.runtime_suspend = bus_pm_runtime_suspend,
	.runtime_resume = bus_pm_runtime_resume,
};

#define IPU_BUS_PM_OPS	(&ipu_bus_pm_ops)
#else
#define IPU_BUS_PM_OPS	NULL
#endif

static int ipu_bus_match(struct device *dev, struct device_driver *drv)
{
	struct ipu_bus_driver *adrv = to_ipu_bus_driver(drv);

	dev_dbg(dev, "bus match: \"%s\" --- \"%s\"\n", dev_name(dev),
		adrv->wanted);

	return !strncmp(dev_name(dev), adrv->wanted, strlen(adrv->wanted));
}

static struct ipu_dma_mapping *alloc_dma_mapping(struct device *dev)
{
	struct ipu_dma_mapping *dmap;

	dmap = kzalloc(sizeof(*dmap), GFP_KERNEL);
	if (!dmap)
		return NULL;

	dmap->domain = iommu_domain_alloc(dev->bus);
	if (!dmap->domain) {
		kfree(dmap);
		return NULL;
	}
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 0, 0)
	init_iova_domain(&dmap->iovad, dma_get_mask(dev) >> PAGE_SHIFT);
#elif LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0)
	init_iova_domain(&dmap->iovad, SZ_4K, 1,
			 dma_get_mask(dev) >> PAGE_SHIFT);
#else
	init_iova_domain(&dmap->iovad, SZ_4K, 1);
#endif

	kref_init(&dmap->ref);

	pr_debug("alloc mapping\n");

	iova_cache_get();

	return dmap;
}

static void free_dma_mapping(void *ptr)
{
	struct ipu_mmu *mmu = ptr;
	struct ipu_dma_mapping *dmap = mmu->dmap;

	iommu_domain_free(dmap->domain);
	mmu->set_mapping(mmu, NULL);
	iova_cache_put();
	put_iova_domain(&dmap->iovad);
	kfree(dmap);
}

static struct iommu_group *ipu_bus_get_group(struct device *dev)
{
	struct device *aiommu = to_ipu_bus_device(dev)->iommu;
	struct ipu_mmu *mmu = dev_get_drvdata(aiommu);
	struct iommu_group *group;
	struct ipu_dma_mapping *dmap;

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
	 * develoment environments.
	 */
	pm_runtime_get_sync(aiommu);
	mmu->set_mapping(mmu, dmap);
	pm_runtime_put_sync(aiommu);

	return group;
}

static int ipu_bus_probe(struct device *dev)
{
	struct ipu_bus_device *adev = to_ipu_bus_device(dev);
	struct ipu_bus_driver *adrv = to_ipu_bus_driver(dev->driver);
	struct iommu_group *group = NULL;
	int rval;

	dev_dbg(dev, "bus probe dev %s\n", dev_name(dev));

	if (adev->iommu) {
		dev_dbg(dev, "iommu %s\n", dev_name(adev->iommu));

		group = ipu_bus_get_group(dev);
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
	ipu_bus_set_drvdata(adev, NULL);
	adev->adrv = NULL;
	iommu_group_remove_device(dev);
	iommu_group_put(group);
	return rval;
}

static int ipu_bus_remove(struct device *dev)
{
	struct ipu_bus_device *adev = to_ipu_bus_device(dev);
	struct ipu_bus_driver *adrv = to_ipu_bus_driver(dev->driver);

	if (adrv->remove)
		adrv->remove(adev);

	if (adev->iommu)
		iommu_group_remove_device(dev);

	return 0;
}

static struct bus_type ipu_bus = {
	.name = IPU_BUS_NAME,
	.match = ipu_bus_match,
	.probe = ipu_bus_probe,
	.remove = ipu_bus_remove,
	.pm = IPU_BUS_PM_OPS,
};

static struct mutex ipu_bus_mutex;

static void ipu_bus_release(struct device *dev)
{
	struct ipu_bus_device *adev = to_ipu_bus_device(dev);

	kfree(adev);
}

struct ipu_bus_device *ipu_bus_add_device(struct pci_dev *pdev,
					  struct device *parent, void *pdata,
					  struct device *iommu,
					  struct ipu_buttress_ctrl *ctrl,
					  char *name, unsigned int nr)
{
	struct ipu_bus_device *adev;
	struct ipu_device *isp = pci_get_drvdata(pdev);
	int rval;

	adev = kzalloc(sizeof(*adev), GFP_KERNEL);
	if (!adev)
		return ERR_PTR(-ENOMEM);

	adev->dev.parent = parent;
	adev->dev.bus = &ipu_bus;
	adev->dev.release = ipu_bus_release;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 13, 16)
	adev->dev.dma_ops = &ipu_dma_ops;
#else
	adev->dev.archdata.dma_ops = &ipu_dma_ops;
#endif
	adev->dma_mask = DMA_BIT_MASK(isp->secure_mode ?
				      IPU_MMU_ADDRESS_BITS :
				      IPU_MMU_ADDRESS_BITS_NON_SECURE);
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

	mutex_lock(&ipu_bus_mutex);
	list_add(&adev->list, &isp->devices);
	mutex_unlock(&ipu_bus_mutex);

	return adev;
}

void ipu_bus_del_devices(struct pci_dev *pdev)
{
	struct ipu_device *isp = pci_get_drvdata(pdev);
	struct ipu_bus_device *adev, *save;

	mutex_lock(&ipu_bus_mutex);

	list_for_each_entry_safe(adev, save, &isp->devices, list) {
		list_del(&adev->list);
		device_unregister(&adev->dev);
	}

	mutex_unlock(&ipu_bus_mutex);
}

int ipu_bus_register_driver(struct ipu_bus_driver *adrv)
{
	adrv->drv.bus = &ipu_bus;
	return driver_register(&adrv->drv);
}
EXPORT_SYMBOL(ipu_bus_register_driver);

int ipu_bus_unregister_driver(struct ipu_bus_driver *adrv)
{
	driver_unregister(&adrv->drv);
	return 0;
}
EXPORT_SYMBOL(ipu_bus_unregister_driver);

int ipu_bus_register(void)
{
	mutex_init(&ipu_bus_mutex);
	return bus_register(&ipu_bus);
}
EXPORT_SYMBOL(ipu_bus_register);

void ipu_bus_unregister(void)
{
	mutex_destroy(&ipu_bus_mutex);
	return bus_unregister(&ipu_bus);
}
EXPORT_SYMBOL(ipu_bus_unregister);

int ipu_bus_set_iommu(struct iommu_ops *ops)
{
	if (iommu_present(&ipu_bus))
		return 0;

	return bus_set_iommu(&ipu_bus, ops);
}
EXPORT_SYMBOL(ipu_bus_set_iommu);

static int flr_rpm_recovery(struct device *dev, void *p)
{
	dev_dbg(dev, "FLR recovery call\n");
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

int ipu_bus_flr_recovery(void)
{
	bus_for_each_dev(&ipu_bus, NULL, NULL, flr_rpm_recovery);
	return 0;
}
EXPORT_SYMBOL(ipu_bus_flr_recovery);
