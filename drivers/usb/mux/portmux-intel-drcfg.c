/**
 * intel-mux-drcfg.c - Driver for Intel USB mux via register
 *
 * Copyright (C) 2016 Intel Corporation
 * Author: Heikki Krogerus <heikki.krogerus@linux.intel.com>
 * Author: Lu Baolu <baolu.lu@linux.intel.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/property.h>
#include <linux/platform_device.h>
#include <linux/usb.h>
#include <linux/usb/portmux.h>

#define INTEL_MUX_CFG0		0x00
#define INTEL_MUX_CFG1		0x04
#define CFG0_SW_IDPIN		BIT(20)
#define CFG0_SW_IDPIN_EN	BIT(21)
#define CFG0_SW_VBUS_VALID	BIT(24)
#define CFG1_MODE		BIT(29)

struct intel_mux_drcfg {
	struct portmux_desc desc;
	struct device *dev;
	void __iomem *regs;
	struct portmux_dev *pdev;
};

static int usb0_init_state = -1;
module_param(usb0_init_state, int, S_IRUGO);
MODULE_PARM_DESC(usb0_init_state, "APL USB port 0 init state 0:device, 1:host");

static int usb0_auto_role = -1;
module_param(usb0_auto_role, int, S_IRUGO);
MODULE_PARM_DESC(usb0_auto_role, "APL USB port 0 automatic role switching");

static inline int intel_mux_drcfg_switch(struct device *dev, bool host)
{
	u32 data;
	struct intel_mux_drcfg *mux;

	mux = dev_get_drvdata(dev);

	if (usb0_auto_role == 1) {
		dev_info(dev, "USB port 0 mux is in automatic role mode.\n");
		return 0;
	}

	/* Check and set mux to SW controlled mode */
	data = readl(mux->regs + INTEL_MUX_CFG0);
	if (!(data & CFG0_SW_IDPIN_EN)) {
		data |= CFG0_SW_IDPIN_EN;
		writel(data, mux->regs + INTEL_MUX_CFG0);
		dev_dbg(dev, "Set SW_IDPIN_EN bit\n");
	}

	/*
	 * Configure CFG0 to switch the mux and VBUS_VALID bit is
	 * required for device mode.
	 */
	data = readl(mux->regs + INTEL_MUX_CFG0);
	if (host)
		data &= ~(CFG0_SW_IDPIN | CFG0_SW_VBUS_VALID);
	else
		data |= (CFG0_SW_IDPIN | CFG0_SW_VBUS_VALID);
	writel(data, mux->regs + INTEL_MUX_CFG0);

	return 0;
}

static int intel_mux_drcfg_cable_set(struct device *dev)
{
	dev_dbg(dev, "drcfg mux switch to HOST\n");

	return intel_mux_drcfg_switch(dev, true);
}

static int intel_mux_drcfg_cable_unset(struct device *dev)
{
	dev_dbg(dev, "drcfg mux switch to DEVICE\n");

	return intel_mux_drcfg_switch(dev, false);
}

static const struct portmux_ops drcfg_ops = {
	.cable_set_cb = intel_mux_drcfg_cable_set,
	.cable_unset_cb = intel_mux_drcfg_cable_unset,
};

static int intel_mux_drcfg_probe(struct platform_device *pdev)
{
	struct intel_mux_drcfg *mux;
	struct device *dev = &pdev->dev;
	const char *extcon_name = NULL;
	u64 start, size;
	int ret;

	mux = devm_kzalloc(dev, sizeof(*mux), GFP_KERNEL);
	if (!mux)
		return -ENOMEM;

	ret = device_property_read_u64(dev, "reg-start", &start);
	ret |= device_property_read_u64(dev, "reg-size", &size);
	if (ret)
		return -ENODEV;

	ret = device_property_read_string(dev, "extcon-name", &extcon_name);
	if (!ret)
		mux->desc.extcon_name = extcon_name;

	mux->regs = devm_ioremap_nocache(dev, start, size);
	if (!mux->regs)
		return -ENOMEM;

	mux->desc.dev = dev;
	mux->desc.name = "intel-mux-drcfg";
	mux->desc.ops = &drcfg_ops;
	mux->desc.initial_state = (usb0_init_state == -1) ?
			!!(readl(mux->regs + INTEL_MUX_CFG1) & CFG1_MODE) :
			(usb0_init_state ? 1 : 0);
	dev_set_drvdata(dev, mux);
	mux->pdev = portmux_register(&mux->desc);

	if (usb0_auto_role == 1)
		writel(0x800, mux->regs + INTEL_MUX_CFG0);

	return PTR_ERR_OR_ZERO(mux->pdev);
}

static int intel_mux_drcfg_remove(struct platform_device *pdev)
{
	struct intel_mux_drcfg *mux;

	mux = platform_get_drvdata(pdev);
	portmux_unregister(mux->pdev);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
/*
 * In case a micro A cable was plugged in while device was sleeping,
 * we missed the interrupt. We need to poll usb id state when waking
 * the driver to detect the missed event.
 * We use 'complete' callback to give time to all extcon listeners to
 * resume before we send new events.
 */
static void intel_mux_drcfg_complete(struct device *dev)
{
	struct intel_mux_drcfg *mux;

	mux = dev_get_drvdata(dev);
	portmux_complete(mux->pdev);
}

static const struct dev_pm_ops intel_mux_drcfg_pm_ops = {
	.complete = intel_mux_drcfg_complete,
};
#endif

static const struct platform_device_id intel_mux_drcfg_platform_ids[] = {
	{ .name = "intel-mux-drcfg", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(platform, intel_mux_drcfg_platform_ids);

static struct platform_driver intel_mux_drcfg_driver = {
	.probe		= intel_mux_drcfg_probe,
	.remove		= intel_mux_drcfg_remove,
	.driver		= {
		.name	= "intel-mux-drcfg",
#ifdef CONFIG_PM_SLEEP
		.pm	= &intel_mux_drcfg_pm_ops,
#endif
	},
	.id_table = intel_mux_drcfg_platform_ids,
};

module_platform_driver(intel_mux_drcfg_driver);

MODULE_AUTHOR("Heikki Krogerus <heikki.krogerus@linux.intel.com>");
MODULE_AUTHOR("Lu Baolu <baolu.lu@linux.intel.com>");
MODULE_DESCRIPTION("Intel USB drcfg mux driver");
MODULE_LICENSE("GPL v2");
