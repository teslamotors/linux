/**
 * extcon-intel-usb.c - Driver for Intel USB mux
 *
 * Copyright (C) 2015 Intel Corporation
 * Author: Heikki Krogerus <heikki.krogerus@linux.intel.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/slab.h>
#include <linux/extcon.h>

#include <linux/extcon/intel_usb_mux.h>

#include "extcon.h"

#define INTEL_MUX_CFG0		0x00
#define INTEL_MUX_CFG1		0x04

#define CFG0_SW_DRD_MODE_MASK	0x3
#define CFG0_SW_DRD_DYN		0
#define CFG0_SW_DRD_STATIC_HOST	1
#define CFG0_SW_DRD_STATIC_DEV	2
#define CFG0_SW_SYNC_SS_AND_HS	BIT(2)
#define CFG0_SW_SWITCH_EN	BIT(16)
#define CFG0_SW_IDPIN		BIT(20)
#define CFG0_SW_IDPIN_EN	BIT(21)
#define CFG0_SW_VBUS_VALID	BIT(24)

#define CFG1_MODE		BIT(29)

struct intel_usb_mux {
	struct notifier_block nb;
	struct extcon_dev edev;
	void __iomem *regs;
};

static const int intel_mux_cable[] = {
	EXTCON_USB_HOST,
	EXTCON_NONE,
};

static int intel_usb_mux_notifier(struct notifier_block *nb,
				  unsigned long old, void *ptr)
{
	struct intel_usb_mux *mux = container_of(nb, struct intel_usb_mux, nb);
	u32 val;

	if (mux->edev.state)
		val = CFG0_SW_IDPIN_EN | CFG0_SW_DRD_STATIC_HOST | CFG0_SW_SWITCH_EN;
	else
		val = CFG0_SW_IDPIN_EN | CFG0_SW_IDPIN | CFG0_SW_VBUS_VALID |
		      CFG0_SW_DRD_STATIC_DEV | CFG0_SW_SWITCH_EN ;

	writel(val, mux->regs);
	return NOTIFY_OK;
}

struct intel_usb_mux *intel_usb_mux_register(struct device *dev,
					     struct resource *r)
{
	struct intel_usb_mux *mux;
	int ret;
	u32 val;

	mux = kzalloc(sizeof(*mux), GFP_KERNEL);
	if (!mux)
		return ERR_PTR(-ENOMEM);

	mux->regs = ioremap_nocache(r->start, resource_size(r));
	if (!mux->regs) {
		kfree(mux);
		return ERR_PTR(-ENOMEM);
	}

	val = CFG0_SW_IDPIN_EN | CFG0_SW_IDPIN | CFG0_SW_VBUS_VALID |
			CFG0_SW_DRD_STATIC_DEV | CFG0_SW_SWITCH_EN;
	writel(val, mux->regs);

	mux->edev.dev.parent = dev;
	mux->edev.supported_cable = intel_mux_cable;

	ret = extcon_dev_register(&mux->edev);
	if (ret)
		goto err;

	mux->edev.name = "intel_usb_mux";
	mux->edev.state = !!(readl(mux->regs + INTEL_MUX_CFG1) & CFG1_MODE);

	/* An external source needs to tell us what to do */
	mux->nb.notifier_call = intel_usb_mux_notifier;
	ret = extcon_register_notifier(&mux->edev, EXTCON_USB_HOST, &mux->nb);
	if (ret) {
		dev_err(&mux->edev.dev, "failed to register notifier\n");
		extcon_dev_unregister(&mux->edev);
		goto err;
	}
	return mux;
err:
	iounmap(mux->regs);
	kfree(mux);
	return ERR_PTR(ret);
}
EXPORT_SYMBOL_GPL(intel_usb_mux_register);

void intel_usb_mux_unregister(struct intel_usb_mux *mux)
{
	extcon_unregister_notifier(&mux->edev, EXTCON_USB_HOST, &mux->nb);
	extcon_dev_unregister(&mux->edev);
	iounmap(mux->regs);
	kfree(mux);
}
EXPORT_SYMBOL_GPL(intel_usb_mux_unregister);
