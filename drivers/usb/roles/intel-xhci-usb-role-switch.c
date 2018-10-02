// SPDX-License-Identifier: GPL-2.0+
/*
 * Intel XHCI (Cherry Trail, Broxton and others) USB OTG role switch driver
 *
 * Copyright (c) 2016-2017 Hans de Goede <hdegoede@redhat.com>
 *
 * Loosely based on android x86 kernel code which is:
 *
 * Copyright (C) 2014 Intel Corp.
 *
 * Author: Wu, Hao
 */

#include <linux/acpi.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/usb/role.h>

/* register definition */
#define DUAL_ROLE_CFG0			0x68
#define SW_VBUS_VALID			BIT(24)
#define SW_IDPIN_EN			BIT(21)
#define SW_IDPIN			BIT(20)

#define DUAL_ROLE_CFG1			0x6c
#define HOST_MODE			BIT(29)

#define DUAL_ROLE_CFG1_POLL_TIMEOUT	1000

#define DRV_NAME			"intel_xhci_usb_sw"

struct intel_xhci_usb_data {
	struct usb_role_switch *role_sw;
	void __iomem *base;
};

static const struct acpi_device_id typec_port_devices[] = {
	{ "PNP0CA0", 0 }, /* UCSI */
	{ "INT33FE", 0 }, /* CHT USB Type-C PHY */
	{ },
};

static acpi_status intel_xhci_userspace_ctrl(acpi_handle handle, u32 level,
					     void *ctx, void **ret)
{
	struct acpi_device *adev;

	if (acpi_bus_get_device(handle, &adev))
		return AE_OK;

	/* Don't allow userspace-control with Type-C ports */
	if (!acpi_match_device_ids(adev, typec_port_devices))
		return AE_ACCESS;

	return AE_OK;
}

static int default_role;
module_param(default_role, int, 0444);
MODULE_PARM_DESC(default_role, "USB OTG port default role 0:default 1:host 2:device");

static int intel_xhci_usb_set_role(struct device *dev, enum usb_role role)
{
	struct intel_xhci_usb_data *data = dev_get_drvdata(dev);
	unsigned long timeout;
	acpi_status status;
	u32 glk, val;

	/*
	 * On many CHT devices ACPI event (_AEI) handlers read / modify /
	 * write the cfg0 register, just like we do. Take the ACPI lock
	 * to avoid us racing with the AML code.
	 */
	status = acpi_acquire_global_lock(ACPI_WAIT_FOREVER, &glk);
	if (ACPI_FAILURE(status) && status != AE_NOT_CONFIGURED) {
		dev_err(dev, "Error could not acquire lock\n");
		return -EIO;
	}

	/* Set idpin value as requested */
	val = readl(data->base + DUAL_ROLE_CFG0);
	switch (role) {
	case USB_ROLE_NONE:
		val |= SW_IDPIN;
		val &= ~SW_VBUS_VALID;
		break;
	case USB_ROLE_HOST:
		val &= ~SW_IDPIN;
		val &= ~SW_VBUS_VALID;
		break;
	case USB_ROLE_DEVICE:
		val |= SW_IDPIN;
		val |= SW_VBUS_VALID;
		break;
	}
	val |= SW_IDPIN_EN;

	writel(val, data->base + DUAL_ROLE_CFG0);

	acpi_release_global_lock(glk);

	/* In most case it takes about 600ms to finish mode switching */
	timeout = jiffies + msecs_to_jiffies(DUAL_ROLE_CFG1_POLL_TIMEOUT);

	/* Polling on CFG1 register to confirm mode switch.*/
	do {
		val = readl(data->base + DUAL_ROLE_CFG1);
		if (!!(val & HOST_MODE) == (role == USB_ROLE_HOST))
			return 0;

		/* Interval for polling is set to about 5 - 10 ms */
		usleep_range(5000, 10000);
	} while (time_before(jiffies, timeout));

	dev_warn(dev, "Timeout waiting for role-switch\n");
	return -ETIMEDOUT;
}

static enum usb_role intel_xhci_usb_get_role(struct device *dev)
{
	struct intel_xhci_usb_data *data = dev_get_drvdata(dev);
	enum usb_role role;
	u32 val;

	val = readl(data->base + DUAL_ROLE_CFG0);

	if (!(val & SW_IDPIN))
		role = USB_ROLE_HOST;
	else if (val & SW_VBUS_VALID)
		role = USB_ROLE_DEVICE;
	else
		role = USB_ROLE_NONE;

	return role;
}

static struct usb_role_switch_desc sw_desc = {
	.set = intel_xhci_usb_set_role,
	.get = intel_xhci_usb_get_role,
};

static int intel_xhci_usb_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct intel_xhci_usb_data *data;
	struct resource *res;
	acpi_status status;

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	data->base = devm_ioremap_nocache(dev, res->start, resource_size(res));
	if (IS_ERR(data->base))
		return PTR_ERR(data->base);

	status = acpi_walk_namespace(ACPI_TYPE_DEVICE, ACPI_ROOT_OBJECT,
				     ACPI_UINT32_MAX, intel_xhci_userspace_ctrl,
				     NULL, NULL, NULL);
	if (ACPI_SUCCESS(status))
		sw_desc.allow_userspace_control = true;

	platform_set_drvdata(pdev, data);

	data->role_sw = usb_role_switch_register(dev, &sw_desc);
	if (IS_ERR(data->role_sw))
		return PTR_ERR(data->role_sw);

	intel_xhci_usb_set_role(dev, USB_ROLE_HOST);
	msleep(10);
	intel_xhci_usb_set_role(dev, USB_ROLE_DEVICE);

	/* Override USB OTG port default role. */
	switch (default_role) {
	case USB_ROLE_HOST:
		intel_xhci_usb_set_role(dev, USB_ROLE_HOST);
		break;
	case USB_ROLE_DEVICE:
		intel_xhci_usb_set_role(dev, USB_ROLE_DEVICE);
		break;
	}

	return 0;
}

static int intel_xhci_usb_remove(struct platform_device *pdev)
{
	struct intel_xhci_usb_data *data = platform_get_drvdata(pdev);

	usb_role_switch_unregister(data->role_sw);
	return 0;
}

static const struct platform_device_id intel_xhci_usb_table[] = {
	{ .name = DRV_NAME },
	{}
};
MODULE_DEVICE_TABLE(platform, intel_xhci_usb_table);

static struct platform_driver intel_xhci_usb_driver = {
	.driver = {
		.name = DRV_NAME,
	},
	.id_table = intel_xhci_usb_table,
	.probe = intel_xhci_usb_probe,
	.remove = intel_xhci_usb_remove,
};

module_platform_driver(intel_xhci_usb_driver);

MODULE_AUTHOR("Hans de Goede <hdegoede@redhat.com>");
MODULE_DESCRIPTION("Intel XHCI USB role switch driver");
MODULE_LICENSE("GPL");
