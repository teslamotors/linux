/*
 * Intel USB Dual Role transceiver driver
 *
 * This driver is based on Cherrytrail OTG driver written by
 * Wu, Hao
 *
 * Copyright (C) 2017, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program;
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/usb.h>
#include <linux/usb/gadget.h>
#include <linux/usb/hcd.h>

#include <linux/usb/otg-fsm.h>
#include <linux/usb/otg.h>

#include <linux/extcon.h>

#include "../host/xhci.h"

struct intel_dr_phy {
	struct usb_phy phy;
	struct otg_fsm fsm;
	struct notifier_block nb;
	struct extcon_dev *edev;
	struct delayed_work mux_work;
};

static struct intel_dr_phy *intel_phy_dev;

static int intel_usb_mux_update(struct intel_dr_phy *intel_phy,
				enum phy_mode mode)
{
	struct usb_bus *host = intel_phy->phy.otg->host;
	struct usb_gadget *gadget = intel_phy->phy.otg->gadget;

	if (!host || !gadget || !gadget->dev.parent || !intel_phy->edev)
		return -ENODEV;

	/* PHY is shared between host and device controller. So both needs
	 * to be in D0 before making any PHY state transition.*/
	pm_runtime_get_sync(host->controller);
	pm_runtime_get_sync(gadget->dev.parent);

	if (mode == PHY_MODE_USB_HOST)
		extcon_set_state_sync(intel_phy->edev, EXTCON_USB_HOST, 1);
	else if (mode == PHY_MODE_USB_DEVICE)
		extcon_set_state_sync(intel_phy->edev, EXTCON_USB_HOST, 0);

	pm_runtime_put(gadget->dev.parent);
	pm_runtime_put(host->controller);

	return 0;
}

static int intel_dr_phy_start_host(struct otg_fsm *fsm, int on)
{
	struct intel_dr_phy *intel_phy;

	if (!fsm || !fsm->otg)
		return -ENODEV;

	intel_phy = container_of(fsm->otg->usb_phy, struct intel_dr_phy, phy);

	if (!fsm->otg->host)
		return -ENODEV;

	/* Just switch the mux to host path */
	return intel_usb_mux_update(intel_phy, PHY_MODE_USB_HOST);
}

static int intel_dr_phy_start_gadget(struct otg_fsm *fsm, int on)
{
	struct intel_dr_phy *intel_phy;

	if (!fsm || !fsm->otg)
		return -ENODEV;

	intel_phy = container_of(fsm->otg->usb_phy, struct intel_dr_phy, phy);

	if (!fsm->otg->gadget)
		return -ENODEV;

	/* Just switch the mux to device mode */
	return intel_usb_mux_update(intel_phy, PHY_MODE_USB_DEVICE);
}

static int intel_dr_phy_set_host(struct usb_otg *otg, struct usb_bus *host)
{
	struct intel_dr_phy *intel_phy;

	if (!otg || !host)
		return -ENODEV;

	intel_phy = container_of(otg->usb_phy, struct intel_dr_phy, phy);

	otg->host = host;

	intel_phy->fsm.a_bus_drop = 0;
	intel_phy->fsm.a_bus_req = 0;

	if (intel_phy->phy.otg->gadget)
		otg_statemachine(&intel_phy->fsm);

	return 0;
}

static int intel_dr_phy_set_peripheral(struct usb_otg *otg,
				  struct usb_gadget *gadget)
{
	struct intel_dr_phy *intel_phy;

	if (!otg || !gadget)
		return -ENODEV;

	intel_phy = container_of(otg->usb_phy, struct intel_dr_phy, phy);

	otg->gadget = gadget;

	intel_phy->fsm.b_bus_req = 1;

	/* if host is registered already then kick the state machine.
	 * Only trigger the mode switch once both host and device are
	 * registered */
	if (intel_phy->phy.otg->host)
		otg_statemachine(&intel_phy->fsm);

	return 0;
}

static int intel_dr_phy_handle_notification(struct notifier_block *nb,
				unsigned long event, void *data)
{
	struct usb_bus *host;
	struct usb_gadget *gadget;
	int state;

	if (!intel_phy_dev)
		return NOTIFY_BAD;

	host = intel_phy_dev->phy.otg->host;
	gadget = intel_phy_dev->phy.otg->gadget;

	switch (event) {
	case USB_EVENT_VBUS:
		if (intel_phy_dev->fsm.id)
			intel_phy_dev->fsm.b_sess_vld = 1;
		state = NOTIFY_OK;
		break;
	case USB_EVENT_ID:
		intel_phy_dev->fsm.id = 0;
		state = NOTIFY_OK;
		break;
	case USB_EVENT_NONE:
		if (intel_phy_dev->fsm.id == 0)
			intel_phy_dev->fsm.id = 1;
		else if (intel_phy_dev->fsm.b_sess_vld)
			intel_phy_dev->fsm.b_sess_vld = 0;
		else
			dev_err(intel_phy_dev->phy.dev, "USB_EVENT_NONE?\n");
			state = NOTIFY_OK;
		break;
	default:
		dev_info(intel_phy_dev->phy.dev, "unknown notification\n");
		state = NOTIFY_DONE;
		break;
	}

	/*
	 * Don't kick the state machine if host or device controller
	 * were never registered before. Just wait to kick it when
	 * set_host or set_peripheral.
	 * */

	if (host && gadget)
		otg_statemachine(&intel_phy_dev->fsm);

	return state;
}


static struct otg_fsm_ops intel_dr_phy_fsm_ops = {
	.start_host = intel_dr_phy_start_host,
	.start_gadget = intel_dr_phy_start_gadget,
};

static ssize_t show_mux_state(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct otg_fsm *fsm = &intel_phy_dev->fsm;

	if (fsm->id && fsm->a_vbus_vld)
		return sprintf(buf, "peripheral state\n");
	else if (!fsm->id && !fsm->a_vbus_vld)
		return sprintf(buf, "host state\n");
	else
		return sprintf(buf, "unknown state\n");

}

static ssize_t store_mux_state(struct device *_dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct otg_fsm *fsm;

	if (!intel_phy_dev)
		return -EINVAL;

	fsm = &intel_phy_dev->fsm;

	if (count != 2)
		return -EINVAL;

	switch (buf[0]) {
	case 'D':
	case 'd':
	case 'P':
	case 'p':
	case 'a':
	case 'A':
	case '1':
		dev_info(intel_phy_dev->phy.dev, "p: set PERIPHERAL mode\n");
		/* disable host mode */
		dev_info(intel_phy_dev->phy.dev, "ID = 1\n");
		atomic_notifier_call_chain(&intel_phy_dev->phy.notifier,
			USB_EVENT_NONE, NULL);
		/* enable device mode */
		dev_info(intel_phy_dev->phy.dev, "VBUS = 1\n");
		atomic_notifier_call_chain(&intel_phy_dev->phy.notifier,
			USB_EVENT_VBUS, NULL);
		return count;
	case 'H':
	case 'h':
	case 'b':
	case 'B':
	case '0':
		dev_info(intel_phy_dev->phy.dev, "h: set HOST mode\n");
		/* disable device mode */
		dev_info(intel_phy_dev->phy.dev, "VBUS = 0\n");
		atomic_notifier_call_chain(&intel_phy_dev->phy.notifier,
			USB_EVENT_NONE, NULL);
		/* enable host mode */
		dev_info(intel_phy_dev->phy.dev, "ID = 0\n");
		atomic_notifier_call_chain(&intel_phy_dev->phy.notifier,
			USB_EVENT_ID, NULL);
		return count;
	default:
		return -EINVAL;
	}

	return count;
}

static DEVICE_ATTR(mux_state, S_IRWXU|S_IRWXG, show_mux_state, store_mux_state);

static int intel_usb_mux_init(struct intel_dr_phy *intel_phy)
{
	if (!intel_phy)
		return -ENODEV;

	intel_phy->edev = extcon_get_extcon_dev("intel_usb_mux");
	if (!intel_phy->edev) {
		dev_err(intel_phy->phy.dev, "intel mux device not ready\n");
		return -ENODEV;
	}

	/* first switch the mux to host mode to force the host
	 * enumerate the port */
	extcon_set_state_sync(intel_phy->edev, EXTCON_USB_HOST, 1);
	msleep(10);
	extcon_set_state_sync(intel_phy->edev, EXTCON_USB_HOST, 0);

	return 0;
}

static void intel_usb_mux_delayed_init(struct work_struct *work)
{
	struct intel_dr_phy *intel_phy = container_of(work,
						       struct intel_dr_phy,
						       mux_work.work);
	static int retry_count = 0;

	if (intel_usb_mux_init(intel_phy) && retry_count < 10) {
		retry_count++;
		schedule_delayed_work(&intel_phy->mux_work, HZ);
	}
}

static int intel_dr_phy_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct intel_dr_phy *intel_phy;
	int err;

	intel_phy = devm_kzalloc(dev, sizeof(*intel_phy), GFP_KERNEL);
	if (!intel_phy)
		return -ENOMEM;

	intel_phy->phy.otg = devm_kzalloc(dev, sizeof(*intel_phy->phy.otg),
					GFP_KERNEL);
	if (!intel_phy->phy.otg)
		return -ENOMEM;

	INIT_DELAYED_WORK(&intel_phy->mux_work, intel_usb_mux_delayed_init);

	if (intel_usb_mux_init(intel_phy)) {
		dev_warn(dev, "mux is not available, so delayed mux_init");
		schedule_delayed_work(&intel_phy->mux_work, HZ);
	}

	/* initialize fsm ops */
	intel_phy->fsm.ops = &intel_dr_phy_fsm_ops;
	intel_phy->fsm.otg = intel_phy->phy.otg;
	/* default device mode */
	intel_phy->fsm.id = 1;
	intel_phy->fsm.b_sess_vld = 1;
	mutex_init(&intel_phy->fsm.lock);

	/* initalize the phy structure */
	intel_phy->phy.label = "intel_usb_dr_phy";
	intel_phy->phy.dev = &pdev->dev;
	intel_phy->phy.type = USB_PHY_TYPE_USB2;
	/* initalize otg structure */
	intel_phy->phy.otg->state = OTG_STATE_UNDEFINED;
	intel_phy->phy.otg->usb_phy = &intel_phy->phy;
	intel_phy->phy.otg->set_host = intel_dr_phy_set_host;
	intel_phy->phy.otg->set_peripheral = intel_dr_phy_set_peripheral;
	/* No support for ADP, HNP and SRP */
	intel_phy->phy.otg->start_hnp = NULL;
	intel_phy->phy.otg->start_srp = NULL;

	intel_phy_dev = intel_phy;

	err = usb_add_phy_dev(&intel_phy->phy);
	if (err) {
		dev_err(&pdev->dev, "can't register intel_dr_phy, err: %d\n",
			err);
		return err;
	}

	intel_phy->nb.notifier_call = intel_dr_phy_handle_notification;
	usb_register_notifier(&intel_phy->phy, &intel_phy->nb);

	platform_set_drvdata(pdev, intel_phy);

	err = device_create_file(&pdev->dev, &dev_attr_mux_state);
	if (err) {
		dev_err(&pdev->dev, "failed to create mux_status sysfs attribute\n");
		usb_remove_phy(&intel_phy->phy);
		return err;
	}

	otg_statemachine(&intel_phy->fsm);

	return 0;
}

static int intel_dr_phy_remove(struct platform_device *pdev)
{
	struct intel_dr_phy *intel_phy = platform_get_drvdata(pdev);

	usb_remove_phy(&intel_phy->phy);

	return 0;
}

static struct platform_driver intel_dr_phy_driver = {
	.probe		= intel_dr_phy_probe,
	.remove		= intel_dr_phy_remove,
	.driver		= {
		.name	= "intel_usb_dr_phy",
	},
};

struct platform_device *intel_dr_phy_device;

static int __init intel_dr_phy_init(void)
{
	int ret;
	ret = platform_driver_register(&intel_dr_phy_driver);
	if (ret)
		return ret;

	intel_dr_phy_device = platform_device_register_simple("intel_usb_dr_phy", 0, NULL, 0);
	if (IS_ERR(intel_dr_phy_device) || !platform_get_drvdata(intel_dr_phy_device)) {
		platform_driver_unregister(&intel_dr_phy_driver);
		return -ENODEV;
	}
	return 0;
}
rootfs_initcall(intel_dr_phy_init);

static void __exit intel_dr_phy_exit(void)
{
	platform_device_unregister(intel_dr_phy_device);
	platform_driver_unregister(&intel_dr_phy_driver);
}
module_exit(intel_dr_phy_exit);

MODULE_ALIAS("platform:intel_usb_dr_phy");
MODULE_DESCRIPTION("Intel USB Dual Role Transceiver driver");
MODULE_AUTHOR("Hao Wu");
MODULE_AUTHOR("Sathya Kuppuswamy <sathyanarayanan.kuppuswamy@linux.intel.com>");
MODULE_LICENSE("GPL");
