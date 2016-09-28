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

#ifndef INTEL_IPU4_BUS_H
#define INTEL_IPU4_BUS_H

#include <linux/device.h>
#include <linux/irqreturn.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/pci.h>

#define INTEL_IPU4_BUS_NAME	INTEL_IPU4_NAME "-bus"

struct intel_ipu4_buttress_ctrl;
struct intel_ipu4_subsystem_trace_config;

struct intel_ipu4_bus_device {
	struct device dev;
	struct list_head list;
	void *pdata;
	struct intel_ipu4_bus_driver *adrv;
	struct device *iommu;
	struct intel_ipu4_device *isp;
	struct intel_ipu4_subsystem_trace_config *trace_cfg;
	struct intel_ipu4_buttress_ctrl *ctrl;
	u64 dma_mask;
	struct mutex resume_lock;
};

#define to_intel_ipu4_bus_device(_dev) \
	container_of(_dev, struct intel_ipu4_bus_device, dev)

struct intel_ipu4_bus_driver {
	struct device_driver	drv;
	char		wanted[20];
	int		(*probe)(struct intel_ipu4_bus_device *adev);
	void		(*remove)(struct intel_ipu4_bus_device *adev);
	irqreturn_t	(*isr)(struct intel_ipu4_bus_device *adev);
	irqreturn_t	(*isr_threaded)(struct intel_ipu4_bus_device *adev);
	bool		wake_isr_thread;
};

#define to_intel_ipu4_bus_driver(_drv) \
	container_of(_drv, struct intel_ipu4_bus_driver, drv)

struct intel_ipu4_bus_device *intel_ipu4_bus_add_device(
	struct pci_dev *pdev, struct device *parent, void *pdata,
	struct device *iommu, struct intel_ipu4_buttress_ctrl *ctrl, char *name,
	unsigned int nr);
void intel_ipu4_bus_del_devices(struct pci_dev *pdev);

int intel_ipu4_bus_register_driver(struct intel_ipu4_bus_driver *adrv);
int intel_ipu4_bus_unregister_driver(struct intel_ipu4_bus_driver *adrv);

int intel_ipu4_bus_register(void);
void intel_ipu4_bus_unregister(void);

int intel_ipu4_bus_set_iommu(struct iommu_ops *ops);

#define module_intel_ipu4_bus_driver(drv)			\
	module_driver(drv, intel_ipu4_bus_register_driver, \
		intel_ipu4_bus_unregister_driver)

#define intel_ipu4_bus_set_drvdata(adev, data)	\
	dev_set_drvdata(&(adev)->dev, data)
#define intel_ipu4_bus_get_drvdata(adev) dev_get_drvdata(&(adev)->dev)

int intel_ipu4_bus_flr_recovery(void);

#endif
