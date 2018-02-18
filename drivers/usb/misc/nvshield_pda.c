/*
 * USB PDA Driver for NVIDIA Shield
 *
 * Copyright (c) 2014-2015, NVIDIA Corporation. All Rights Reserved.
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
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/usb.h>
#include <linux/slab.h>
#include <linux/of.h>

#define DRIVER_AUTHOR "Bodla Rakesh Babu, rbodla@nvidia.com"
#define DRIVER_DESC "NVIDIA Shield USB PDA Driver"

#define VID 0x0955
#define PID 0x7208 /* Need to modify when actual PID for PDA is allocated */

#define USB_CTRL_TIMEOUT	2000

static const struct usb_device_id nvshieldpda_table[] = {
	{ USB_DEVICE(VID, PID) },
	{}
};
MODULE_DEVICE_TABLE(usb, nvshield_table);

struct nvshield_product {
	const char *name;
	const char *dt_name;
	u16 id;
	u16 max_voltage_mv;
};

struct nvshield_pda {
	struct usb_device *udev;
	struct device *dev;
	struct nvshield_product *nvshield_list;
	struct nvshield_product nvshield;
	int num_nvshield_products;
};

static struct nvshield_product nvshield_list[] = {
	{
		.name = "Unknown",
		.dt_name = NULL,
		.id = 0,
		.max_voltage_mv = 5000,
	},
	{
		.name = "Thor",
		.dt_name = NULL,
		.id = 1,
		.max_voltage_mv = 5000,
	},
	{
		.name = "ST8",
		.dt_name = "nvidia,tn8",
		.id = 2,
		.max_voltage_mv = 12000,
	},
	{
		.name = "Loki-E",
		.dt_name = "nvidia,loki-e",
		.id = 3,
		.max_voltage_mv = 13000,
	},
	{
		.name = "Green Arrow",
		.dt_name = "nvidia,green-arrow",
		.id = 4,
		.max_voltage_mv = 12000,
	},
	{
		.name = "Hawkeye",
		.dt_name = "nvidia,he2290",
		.id = 5,
		.max_voltage_mv = 12000,
	},
};

static int send_command(struct nvshield_pda *pda)
{
	int retval = 0;

	retval = usb_control_msg(pda->udev, usb_sndctrlpipe(pda->udev, 0),
			0x00,
			USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_ENDPOINT,
			cpu_to_le16(pda->nvshield.id),
			cpu_to_le16(pda->nvshield.max_voltage_mv),
			NULL,
			0,
			USB_CTRL_TIMEOUT);

	return retval;
}

static void nvshieldpda_configure(struct nvshield_pda *pda)
{
	int i, idx = 0;

	for (i = 0; i < pda->num_nvshield_products; i++)
		if (pda->nvshield_list[i].dt_name &&
		of_machine_is_compatible(pda->nvshield_list[i].dt_name)) {
			idx = i;
			break;
		}

	pda->nvshield = pda->nvshield_list[idx];

	dev_info(pda->dev, "Connected to %s Shield id-%d, Max voltage-%d\n",
			pda->nvshield.name, pda->nvshield.id,
			pda->nvshield.max_voltage_mv);
}

static ssize_t show_pdastate(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct usb_interface *intf = to_usb_interface(dev);
	struct nvshield_pda *pda = usb_get_intfdata(intf);

	return sprintf(buf, "Product - %s, Shield ID - %d, Max Voltage - %d\n",
			pda->nvshield.name, pda->nvshield.id,
			pda->nvshield.max_voltage_mv);
}

static DEVICE_ATTR(pdastate, S_IRUGO, show_pdastate, NULL);

static int nvshieldpda_probe(struct usb_interface *interface,
		const struct usb_device_id *id)
{
	struct usb_device *udev = interface_to_usbdev(interface);
	struct nvshield_pda *pda;
	int retval = -ENOMEM;

	pda = kzalloc(sizeof(struct nvshield_pda), GFP_KERNEL);
	if (pda == NULL) {
		dev_err(&interface->dev, "out of memory\n");
		goto error_mem;
	}

	pda->udev = usb_get_dev(udev);
	pda->dev = &interface->dev;
	pda->nvshield_list = nvshield_list;
	pda->num_nvshield_products = ARRAY_SIZE(nvshield_list);

	usb_set_intfdata(interface, pda);

	retval = device_create_file(&interface->dev, &dev_attr_pdastate);
	if (retval)
		goto error_dev_create;
	dev_info(&interface->dev, "Nvidia Shield PDA attached\n");

	nvshieldpda_configure(pda);
	send_command(pda);
	return 0;

error_dev_create:
	usb_set_intfdata(interface, NULL);
	usb_put_dev(pda->udev);
	kfree(pda);
error_mem:
	return retval;
}

static void nvshieldpda_disconnect(struct usb_interface *interface)
{
	struct nvshield_pda *dev;

	dev = usb_get_intfdata(interface);

	device_remove_file(&interface->dev, &dev_attr_pdastate);

	usb_set_intfdata(interface, NULL);
	usb_put_dev(dev->udev);
	kfree(dev);
	dev_info(&interface->dev, "Nvidia Shield PDA detached\n");
}

static int nvshieldpda_suspend(struct usb_interface *interface,
		pm_message_t notused)
{
	dev_info(&interface->dev, "Nvidia Shield PDA suspended\n");
	return 0;
}

static int nvshieldpda_resume(struct usb_interface *interface)
{
	dev_info(&interface->dev, "Nvidia Shield PDA resumed\n");
	return 0;
}

static struct usb_driver nvshieldpda_driver = {
	.name =	"nvshieldpda",
	.probe =	nvshieldpda_probe,
	.disconnect =	nvshieldpda_disconnect,
	.suspend =	nvshieldpda_suspend,
	.resume =	nvshieldpda_resume,
	.id_table =	nvshieldpda_table,
};

module_usb_driver(nvshieldpda_driver);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");
