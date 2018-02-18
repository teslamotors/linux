/*
 * Copyright (c) 2011-2015, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/platform_data/tegra_usb.h>
#include <linux/workqueue.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/usb.h>
#include <linux/err.h>
#include <linux/pm_runtime.h>
#include <linux/suspend.h>
#include <linux/slab.h>
#include <linux/wakelock.h>
#include <linux/pm_qos.h>
#include <linux/regulator/consumer.h>
#include <linux/sysedp.h>
#include <linux/platform_data/tegra_usb_modem_power.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <asm/segment.h>
#include <asm/uaccess.h>
#include <linux/platform_data/modem_thermal.h>
#include <linux/platform_data/sysedp_modem.h>
#include <linux/system-wakeup.h>

#if defined(CONFIG_ARCH_TEGRA_21x_SOC)
#include <linux/of_platform.h>
#else
#include <linux/dma-mapping.h>
#include "../../../arch/arm/mach-tegra/iomap.h"
#endif

#define BOOST_CPU_FREQ_MIN	1200000
#define BOOST_CPU_FREQ_TIMEOUT	5000

#define WAKELOCK_TIMEOUT_FOR_USB_ENUM		(HZ * 10)
#define WAKELOCK_TIMEOUT_FOR_REMOTE_WAKE	(HZ)

#define MAX_MODEM_EDP_STATES 10

/* default autosuspend delay in ms */
#define DEFAULT_AUTOSUSPEND		2000

#define XHCI_HSIC_POWER "/sys/bus/platform/devices/tegra-xhci/hsic0_power"

static int hsic_power(bool on)
{
	struct file *fp = NULL;
	mm_segment_t oldfs;
	loff_t offset = 0;

	oldfs = get_fs();
	set_fs(get_ds());
	fp = filp_open(XHCI_HSIC_POWER, O_RDWR, S_IRWXU);
	if (IS_ERR(fp)) {
		pr_err("%s: error opening %s, error:%ld\n",
				__func__, XHCI_HSIC_POWER, PTR_ERR(fp));
		return PTR_ERR(fp);
	}

	if (on)
		vfs_write(fp, "1", 1, &offset);
	else
		vfs_write(fp, "0", 1, &offset);

	vfs_fsync(fp, 0);
	set_fs(oldfs);
	filp_close(fp, NULL);
	return 0;
}

#ifndef CONFIG_ARCH_TEGRA_21x_SOC
static u64 tegra_ehci_dmamask = DMA_BIT_MASK(64);

static struct resource tegra_usb2_resources[] = {
	[0] = {
		.start  = TEGRA_USB2_BASE,
		.end    = TEGRA_USB2_BASE + TEGRA_USB2_SIZE - 1,
		.flags  = IORESOURCE_MEM,
	},
	[1] = {
		.start  = INT_USB2,
		.end    = INT_USB2,
		.flags  = IORESOURCE_IRQ,
	},
};

static struct platform_device tegra_ehci2_device = {
	.name   = "tegra-ehci",
	.id     = 1,
	.dev    = {
		.dma_mask       = &tegra_ehci_dmamask,
		.coherent_dma_mask = DMA_BIT_MASK(64),
	},
	.resource = tegra_usb2_resources,
	.num_resources = ARRAY_SIZE(tegra_usb2_resources),
};

static struct tegra_usb_platform_data tegra_ehci2_hsic_baseband_pdata = {
	.port_otg = false,
	.has_hostpc = true,
	.unaligned_dma_buf_supported = false,
	.phy_intf = TEGRA_USB_PHY_INTF_HSIC,
	.op_mode = TEGRA_USB_OPMODE_HOST,
	.u_data.host = {
		.hot_plug = false,
		.remote_wakeup_supported = true,
		.power_off_on_suspend = true,
		.skip_resume = true,
	},
};
#endif

struct tegra_usb_modem {
	struct tegra_usb_modem_power_platform_data *pdata;
	struct platform_device *pdev;
	struct regulator *regulator; /* modem power regulator */
	unsigned int wake_cnt;	/* remote wakeup counter */
	unsigned int wake_irq;	/* remote wakeup irq */
	bool wake_irq_wakeable; /* LP0 wakeable */
	unsigned int boot_irq;	/* modem boot irq */
	bool boot_irq_wakeable; /* LP0 wakeable */
	struct mutex lock;
	struct wake_lock wake_lock;	/* modem wake lock */
	unsigned int vid;	/* modem vendor id */
	unsigned int pid;	/* modem product id */
	struct usb_device *xusb_roothub;	/* XUSB roothub device */
	struct usb_device *udev;	/* modem usb device */
	struct usb_device *parent;	/* parent device */
	struct usb_interface *intf;	/* first modem usb interface */
	struct workqueue_struct *wq;	/* modem workqueue */
	struct delayed_work recovery_work;	/* modem recovery work */
	struct delayed_work enable_nvhsic_work; /* enable xhci hsic work */
	bool nvhsic_work_queued;	/* if hsic power work is queued*/
	struct work_struct host_load_work;	/* usb host load work */
	struct work_struct host_unload_work;	/* usb host unload work */
	struct pm_qos_request cpu_boost_req; /* min CPU freq request */
	struct work_struct cpu_boost_work;	/* CPU freq boost work */
	struct delayed_work cpu_unboost_work;	/* CPU freq unboost work */
	const struct tegra_modem_operations *ops;	/* modem operations */
	unsigned int capability;	/* modem capability */
	int system_suspend;	/* system suspend flag */
	struct notifier_block pm_notifier;	/* pm event notifier */
	struct notifier_block usb_notifier;	/* usb event notifier */
	int sysfs_file_created;
	struct platform_device *hc;	/* USB host controller */
	struct mutex hc_lock;
	enum { EHCI_HSIC = 0, XHCI_HSIC, XHCI_UTMI } phy_type;
	struct platform_device *modem_thermal_pdev;
	int pre_boost_gpio;		/* control regulator output voltage */
	int modem_state_file_created;	/* modem_state sysfs created */
	enum { AIRPLANE = 0, RAT_3G_LTE, RAT_2G} modem_power_state;
	struct mutex modem_state_lock;
	struct platform_device *modem_edp_pdev;
};


/* supported modems */
static const struct usb_device_id modem_list[] = {
	{USB_DEVICE(0x1983, 0x0310),	/* Icera 450 rev1 */
	 .driver_info = TEGRA_MODEM_AUTOSUSPEND | TEGRA_MODEM_CPU_BOOST,
	 },
	{USB_DEVICE(0x1983, 0x0321),	/* Icera 450 rev2 */
	 .driver_info = TEGRA_MODEM_AUTOSUSPEND | TEGRA_MODEM_CPU_BOOST,
	 },
	{USB_DEVICE(0x1983, 0x0327),	/* Icera 450 5AE */
	 .driver_info = TEGRA_MODEM_AUTOSUSPEND | TEGRA_MODEM_CPU_BOOST,
	 },
	{USB_DEVICE(0x1983, 0x0427),	/* Icera 500 5AN */
	 .driver_info = TEGRA_MODEM_AUTOSUSPEND | TEGRA_MODEM_CPU_BOOST,
	 },
	{USB_DEVICE(0x1983, 0x1005),	/* Icera 500 5AN (BSD) */
	 .driver_info = TEGRA_MODEM_CPU_BOOST,
	/* .driver_info = TEGRA_MODEM_AUTOSUSPEND, */
	 },
	{USB_DEVICE(0x1983, 0x1007),	/* Icera 500 Bruce */
	 .driver_info = TEGRA_USB_HOST_RELOAD|TEGRA_MODEM_AUTOSUSPEND,
	 },
	{}
};

static ssize_t load_unload_usb_host(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count);

static void cpu_freq_unboost(struct work_struct *ws)
{
	struct tegra_usb_modem *modem = container_of(ws, struct tegra_usb_modem,
						     cpu_unboost_work.work);

	pm_qos_update_request(&modem->cpu_boost_req, PM_QOS_DEFAULT_VALUE);
}

static void cpu_freq_boost(struct work_struct *ws)
{
	struct tegra_usb_modem *modem = container_of(ws, struct tegra_usb_modem,
						     cpu_boost_work);

	cancel_delayed_work_sync(&modem->cpu_unboost_work);
	pm_qos_update_request(&modem->cpu_boost_req, BOOST_CPU_FREQ_MIN);
	queue_delayed_work(modem->wq, &modem->cpu_unboost_work,
			      msecs_to_jiffies(BOOST_CPU_FREQ_TIMEOUT));
}

static irqreturn_t tegra_usb_modem_wake_thread(int irq, void *data)
{
	struct tegra_usb_modem *modem = (struct tegra_usb_modem *)data;

	mutex_lock(&modem->lock);
	if (modem->udev && modem->udev->state != USB_STATE_NOTATTACHED) {
		dev_info(&modem->pdev->dev, "remote wake (%u)\n",
			 ++(modem->wake_cnt));

		if (!modem->system_suspend) {
			usb_lock_device(modem->udev);
			if (usb_autopm_get_interface(modem->intf) == 0)
				usb_autopm_put_interface_async(modem->intf);
			usb_unlock_device(modem->udev);
		}
	}
	mutex_unlock(&modem->lock);

	return IRQ_HANDLED;
}

static irqreturn_t tegra_usb_modem_boot_thread(int irq, void *data)
{
	struct tegra_usb_modem *modem = (struct tegra_usb_modem *)data;
	int v = gpio_get_value(modem->pdata->boot_gpio);

	dev_info(&modem->pdev->dev, "MDM_COLDBOOT %s\n", v ? "high" : "low");
	if (modem->capability & TEGRA_USB_HOST_RELOAD)
		if (!work_pending(&modem->host_load_work) &&
		    !work_pending(&modem->host_unload_work))
			queue_work(modem->wq, v ? &modem->host_load_work :
				   &modem->host_unload_work);

	/* hold wait lock to complete the enumeration */
	wake_lock_timeout(&modem->wake_lock, WAKELOCK_TIMEOUT_FOR_USB_ENUM);

	/* boost CPU freq for timing requirements on single flash platforms */
	if ((modem->capability & TEGRA_MODEM_CPU_BOOST) &&
	     !work_pending(&modem->cpu_boost_work))
		queue_work(modem->wq, &modem->cpu_boost_work);

	/* USB disconnect maybe on going... */
	mutex_lock(&modem->lock);
	if (modem->udev && modem->udev->state != USB_STATE_NOTATTACHED)
		pr_warn("Device is not disconnected!\n");
	mutex_unlock(&modem->lock);

	return IRQ_HANDLED;
}

static void tegra_usb_modem_recovery(struct work_struct *ws)
{
	struct tegra_usb_modem *modem = container_of(ws, struct tegra_usb_modem,
						     recovery_work.work);

	mutex_lock(&modem->lock);
	if (!modem->udev) {	/* assume modem crashed */
		if (modem->ops && modem->ops->reset)
			modem->ops->reset();
	}
	mutex_unlock(&modem->lock);
}

static void enable_nvhsic_power(struct work_struct *ws)
{
	struct tegra_usb_modem *modem = container_of(ws, struct tegra_usb_modem,
					enable_nvhsic_work.work);
	int ret;

	pr_info("Enabing XHCI HSIC\n");
	mutex_lock(&modem->hc_lock);
	ret = hsic_power(1);
	if (ret) {
		pr_err("Failed to Enable XHCI HSIC power: %d\n", ret);
		modem->hc = NULL;
		goto error;
	}
	modem->hc = (struct platform_device *)1;
error:
	modem->nvhsic_work_queued = false;
	mutex_unlock(&modem->hc_lock);
}

static void tegra_usb_host_load(struct work_struct *ws)
{
	struct tegra_usb_modem *modem = container_of(ws, struct tegra_usb_modem,
						     host_load_work);
	load_unload_usb_host(&modem->pdev->dev, NULL, "1", 1);
}

static void tegra_usb_host_unload(struct work_struct *ws)
{
	struct tegra_usb_modem *modem = container_of(ws, struct tegra_usb_modem,
						     host_unload_work);
	load_unload_usb_host(&modem->pdev->dev, NULL, "0", 1);
}

static void modem_device_add_handler(struct tegra_usb_modem *modem,
			       struct usb_device *udev)
{
	const struct usb_device_descriptor *desc = &udev->descriptor;
	struct usb_interface *intf = usb_ifnum_to_if(udev, 0);
	const struct usb_device_id *id = NULL;

	if (intf) {
		/* only look for specific modems if modem_list is provided in
		   platform data. Otherwise, look for the modems in the default
		   supported modem list */
		if (modem->pdata->modem_list)
			id = usb_match_id(intf, modem->pdata->modem_list);
		else
			id = usb_match_id(intf, modem_list);
	}

	if (id) {
		/* hold wakelock to ensure ril has enough time to restart */
		wake_lock_timeout(&modem->wake_lock,
				  WAKELOCK_TIMEOUT_FOR_USB_ENUM);

		/* enable XUSB ELPG (autosuspend) */
		if (modem->phy_type == XHCI_HSIC)
			pm_runtime_put(&modem->xusb_roothub->dev);

		pr_info("Add device %d <%s %s>\n", udev->devnum,
			udev->manufacturer, udev->product);

		mutex_lock(&modem->lock);
		modem->udev = udev;
		modem->parent = udev->parent;
		modem->intf = intf;
		modem->vid = desc->idVendor;
		modem->pid = desc->idProduct;
		modem->wake_cnt = 0;
		modem->capability = id->driver_info;
		mutex_unlock(&modem->lock);

		pr_info("persist_enabled: %u\n", udev->persist_enabled);

#ifdef CONFIG_PM
		if (modem->capability & TEGRA_MODEM_AUTOSUSPEND) {
			pm_runtime_set_autosuspend_delay(&udev->dev,
					modem->pdata->autosuspend_delay);
			usb_enable_autosuspend(udev);
			pr_info("enable autosuspend for %s %s\n",
				udev->manufacturer, udev->product);
		}

		/* allow the device to wake up the system */
		if (udev->actconfig->desc.bmAttributes &
		    USB_CONFIG_ATT_WAKEUP)
			device_set_wakeup_enable(&udev->dev, true);
#endif
	}
}

static void modem_device_remove_handler(struct tegra_usb_modem *modem,
				  struct usb_device *udev)
{
	const struct usb_device_descriptor *desc = &udev->descriptor;

	if (desc->idVendor == modem->vid && desc->idProduct == modem->pid) {
		pr_info("Remove device %d <%s %s>\n", udev->devnum,
			udev->manufacturer, udev->product);

		/* enable XUSB ELPG (autosuspend) */
		if (modem->phy_type == XHCI_HSIC)
			pm_runtime_put(&modem->xusb_roothub->dev);

		mutex_lock(&modem->lock);
		modem->udev = NULL;
		modem->intf = NULL;
		modem->vid = 0;
		mutex_unlock(&modem->lock);

		if (modem->capability & TEGRA_MODEM_RECOVERY)
			queue_delayed_work(modem->wq,
					   &modem->recovery_work, HZ * 10);
	}
}


static void xusb_usb2_roothub_add_handler(struct tegra_usb_modem *modem,
		struct usb_device *udev)
{
	const struct usb_device_descriptor *desc = &udev->descriptor;

	if (modem->phy_type == XHCI_HSIC &&
		desc->idVendor == 0x1d6b && desc->idProduct == 0x2 &&
		!strcmp(udev->serial, "tegra-xhci")) {
		pr_info("Add device %d <%s %s>\n", udev->devnum,
			udev->manufacturer, udev->product);
		mutex_lock(&modem->lock);
		modem->xusb_roothub = udev;
		mutex_unlock(&modem->lock);
		/* HSIC isn't enabled yet, enable it in 1 sec */
		if (!modem->hc) {
			modem->nvhsic_work_queued = true;
			/* disable ELPG */
			pm_runtime_get_sync(&modem->xusb_roothub->dev);
			queue_delayed_work(modem->wq,
					&modem->enable_nvhsic_work, HZ);
		}
	}
}

static void xusb_usb2_roothub_remove_handler(struct tegra_usb_modem *modem,
		struct usb_device *udev)
{
	const struct usb_device_descriptor *desc = &udev->descriptor;

	if (modem->phy_type == XHCI_HSIC &&
		desc->idVendor == 0x1d6b && desc->idProduct == 0x2 &&
		!strcmp(udev->serial, "tegra-xhci")) {
		pr_info("Remove device %d <%s %s>\n", udev->devnum,
			udev->manufacturer, udev->product);
		mutex_lock(&modem->lock);
		modem->xusb_roothub = NULL;
		mutex_unlock(&modem->lock);
	}
}

static int mdm_usb_notifier(struct notifier_block *notifier,
			    unsigned long usb_event, void *udev)
{
	struct tegra_usb_modem *modem =
	    container_of(notifier, struct tegra_usb_modem, usb_notifier);

	switch (usb_event) {
	case USB_DEVICE_ADD:
		modem_device_add_handler(modem, udev);
		xusb_usb2_roothub_add_handler(modem, udev);
		break;
	case USB_DEVICE_REMOVE:
		modem_device_remove_handler(modem, udev);
		xusb_usb2_roothub_remove_handler(modem, udev);
		break;
	}
	return NOTIFY_OK;
}

static int mdm_pm_notifier(struct notifier_block *notifier,
			   unsigned long pm_event, void *unused)
{
	struct tegra_usb_modem *modem =
	    container_of(notifier, struct tegra_usb_modem, pm_notifier);

	mutex_lock(&modem->lock);
	if (!modem->udev) {
		mutex_unlock(&modem->lock);
		return NOTIFY_DONE;
	}

	pr_info("%s: event %ld\n", __func__, pm_event);
	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		if (wake_lock_active(&modem->wake_lock)) {
			pr_warn("%s: wakelock was active, aborting suspend\n",
				__func__);
			mutex_unlock(&modem->lock);
			return NOTIFY_STOP;
		}

		modem->system_suspend = 1;
		mutex_unlock(&modem->lock);
		return NOTIFY_OK;
	case PM_POST_SUSPEND:
		modem->system_suspend = 0;
		mutex_unlock(&modem->lock);
		return NOTIFY_OK;
	}

	mutex_unlock(&modem->lock);
	return NOTIFY_DONE;
}

static int mdm_request_irq(struct tegra_usb_modem *modem,
			   irq_handler_t thread_fn,
			   unsigned int irq_gpio,
			   unsigned long irq_flags,
			   const char *label,
			   unsigned int *irq,
			   bool *is_wakeable)
{
	int ret;

	/* enable IRQ for GPIO */
	*irq = gpio_to_irq(irq_gpio);

	/* request threaded irq for GPIO */
	ret = request_threaded_irq(*irq, NULL, thread_fn, irq_flags, label,
				   modem);
	if (ret) {
		*irq = 0;
		return ret;
	}

	ret = enable_irq_wake(*irq);
	*is_wakeable = (ret) ? false : true;

	return 0;
}

/* load USB host controller */
static struct platform_device *tegra_usb_host_register(
				const struct tegra_usb_modem *modem)
{
	struct platform_device *pdev;

#if defined(CONFIG_ARCH_TEGRA_21x_SOC)
	struct property *status_prop;
#else
	const struct platform_device *hc_device =
	    modem->pdata->tegra_ehci_device;
	int val;
#endif

#if defined(CONFIG_ARCH_TEGRA_21x_SOC)
	/* we're registering EHCI through DT here */

	/* If ehci node is unavailable, we're unable to register it later.
	 * So make it available by setting status to "okay" */
	if (!of_device_is_available(modem->pdata->ehci_node)) {
		pr_info("%s(): enable ehci node\n", __func__);
		status_prop = of_find_property(modem->pdata->ehci_node,
						"status", NULL);
		if (!status_prop) {
			pr_err("%s(): error getting status in ehci node\n",
				__func__);
			return NULL;
		}
		strcpy((char *)status_prop->value, "okay");
	}

	/* we have to hard-code EHCI device name here for now */
	pdev = of_platform_device_create(modem->pdata->ehci_node,
					"tegra-ehci.1", NULL);
	if (!pdev) {
		pr_info("%s: error registering EHCI\n", __func__);
		return NULL;
	}

	return pdev;
#else

	pdev = platform_device_alloc(hc_device->name, hc_device->id);
	if (!pdev)
		return NULL;

	val = platform_device_add_resources(pdev, hc_device->resource,
					    hc_device->num_resources);
	if (val)
		goto error;

	pdev->dev.dma_mask = hc_device->dev.dma_mask;
	pdev->dev.coherent_dma_mask = hc_device->dev.coherent_dma_mask;

	val = platform_device_add_data(pdev, modem->pdata->tegra_ehci_pdata,
					sizeof(struct tegra_usb_platform_data));
	if (val)
		goto error;

	val = platform_device_add(pdev);
	if (val)
		goto error;

	return pdev;

error:
	pr_err("%s: err %d\n", __func__, val);
	platform_device_put(pdev);
	return NULL;
#endif
}

/* unload USB host controller */
static void tegra_usb_host_unregister(const struct tegra_usb_modem *modem)
{
	struct platform_device *pdev = modem->hc;

	if (unlikely(modem->pdata->tegra_ehci_device))
		platform_device_unregister(pdev);
	else {
		of_device_unregister(pdev);
#if defined(CONFIG_ARCH_TEGRA_21x_SOC)
		of_node_clear_flag(modem->pdata->ehci_node,
				   OF_POPULATED);
#endif
	}
}

static ssize_t show_usb_host(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	struct tegra_usb_modem *modem = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", (modem->hc) ? 1 : 0);
}

static ssize_t load_unload_usb_host(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct tegra_usb_modem *modem = dev_get_drvdata(dev);
	int host, ret;

	if (sscanf(buf, "%d", &host) != 1 || host < 0 || host > 1)
		return -EINVAL;


	mutex_lock(&modem->hc_lock);
	switch (modem->phy_type) {
	case XHCI_HSIC:
		dev_info(&modem->pdev->dev, "using XHCI_HSIC\n");
		if (host) {
			if (modem->xusb_roothub &&
					!modem->nvhsic_work_queued) {
				modem->nvhsic_work_queued = true;
				/* disable ELPG */
				pm_runtime_get_sync(&modem->xusb_roothub->dev);
				/* enable HSIC power now */
				queue_delayed_work(modem->wq,
					&modem->enable_nvhsic_work, 0);
			}
		} else {
			if (modem->xusb_roothub) {
				pr_info("Disable XHCI HSIC\n");
				/* disable ELPG */
				pm_runtime_get_sync(&modem->xusb_roothub->dev);
				ret = hsic_power(0);
				modem->hc = NULL;
			} else
				pr_warn("xusb roothub not available!\n");
		}
		break;
	case EHCI_HSIC:
		dev_info(&modem->pdev->dev, "using EHCI_HSIC\n");
		pr_info("%s EHCI\n", host ? "Load" : "Unload");
		if (host && !modem->hc) {
			modem->hc = tegra_usb_host_register(modem);
		} else if (!host && modem->hc) {
			tegra_usb_host_unregister(modem);
			modem->hc = NULL;
		}
		break;
	case XHCI_UTMI:
		dev_info(&modem->pdev->dev, "using XHCI_UTMI\n");
		modem->hc = host ? (struct platform_device *)1 : NULL;
		break;
	default:
		pr_warn("%s: wrong phy_type:%d\n", __func__, modem->phy_type);
	}
	mutex_unlock(&modem->hc_lock);

	return count;
}

static DEVICE_ATTR(load_host, S_IRUSR | S_IWUSR, show_usb_host,
		   load_unload_usb_host);

/* Export a new sysfs for the user land (RIL) to notify modem state
 * Airplane mode = 0
 * 3G/LTE mode = 1
 * 2G mode = 2
*/
static ssize_t show_modem_state(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct tegra_usb_modem *modem = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", modem->modem_power_state);
}

static ssize_t set_modem_state(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct tegra_usb_modem *modem = dev_get_drvdata(dev);
	int modem_state_value;

	if (sscanf(buf, "%d", &modem_state_value) != 1)
		return -EINVAL;

	mutex_lock(&modem->modem_state_lock);
	switch (modem_state_value) {
	case AIRPLANE:
		modem->modem_power_state = modem_state_value;

		/* Release BYPASS */
		if (regulator_allow_bypass(modem->regulator, false))
			dev_warn(dev,
			"failed to set modem regulator in non bypass mode\n");

		/* auto PFM mode*/
		if (regulator_set_mode(modem->regulator, REGULATOR_MODE_NORMAL))
			dev_warn(dev,
			"failed to set modem regulator in normal mode\n");
		break;
	case RAT_2G:
		modem->modem_power_state = modem_state_value;

		/* Release BYPASS */
		if (regulator_allow_bypass(modem->regulator, true))
			dev_warn(dev,
			"failed to set modem regulator in bypass mode\n");

		/* forced PWM  mode*/
		if (regulator_set_mode(modem->regulator, REGULATOR_MODE_FAST))
			dev_warn(dev,
			"failed to set modem regulator in fast mode\n");
		break;
	case RAT_3G_LTE:
		modem->modem_power_state = modem_state_value;

		/* Release BYPASS */
		if (regulator_allow_bypass(modem->regulator, true))
			dev_warn(dev,
			"failed to set modem regulator in bypass mode\n");

		/* auto PFM mode*/
		if (regulator_set_mode(modem->regulator, REGULATOR_MODE_NORMAL))
			dev_warn(dev,
			"failed to set modem regulator in normal mode\n");
		break;
	default:
		dev_warn(dev, "%s: wrong modem power state:%d\n", __func__,
		modem_state_value);
	}
	mutex_unlock(&modem->modem_state_lock);

	return count;
}

static DEVICE_ATTR(modem_state, S_IRUSR | S_IWUSR, show_modem_state,
			set_modem_state);

static struct modem_thermal_platform_data thermdata = {
	.num_zones = 0,
};

static struct platform_device modem_thermal_device = {
	.name = "modem-thermal",
	.id = -1,
	.num_resources = 0,
	.dev = {
		.platform_data = NULL,
	},
};

static struct modem_edp_platform_data edpdata = {
	.mdm_power_report = -1,
	.consumer_name = "icemdm",
};

static struct platform_device modem_edp_device = {
	.name = "sysedp_modem",
	.id = -1,
	.num_resources = 0,
	.dev = {
		.platform_data = NULL,
	},
};

static int mdm_init(struct tegra_usb_modem *modem, struct platform_device *pdev)
{
	struct tegra_usb_modem_power_platform_data *pdata =
	    pdev->dev.platform_data;
	int ret = 0;

	modem->pdata = pdata;
	modem->pdev = pdev;

#ifndef CONFIG_ARCH_TEGRA_21x_SOC
	/* WAR to load statically defined ehci device/pdata */
	pdata->tegra_ehci_device = &tegra_ehci2_device;
	pdata->tegra_ehci_pdata = &tegra_ehci2_hsic_baseband_pdata;
#endif

	pdata->autosuspend_delay = DEFAULT_AUTOSUSPEND;

	/* get modem operations from platform data */
	modem->ops = (const struct tegra_modem_operations *)pdata->ops;

	/* modem init */
	if (modem->ops && modem->ops->init) {
		ret = modem->ops->init();
		if (ret)
			goto error;
	}

	mutex_init(&(modem->lock));
	mutex_init(&modem->hc_lock);
	wake_lock_init(&modem->wake_lock, WAKE_LOCK_SUSPEND, "mdm_lock");

	/* if wake gpio is not specified we rely on native usb remote wake */
	if (gpio_is_valid(pdata->wake_gpio)) {
		/* request remote wakeup irq from platform data */
		ret = mdm_request_irq(modem,
				      tegra_usb_modem_wake_thread,
				      pdata->wake_gpio,
				      pdata->wake_irq_flags,
				      "mdm_wake",
				      &modem->wake_irq,
				      &modem->wake_irq_wakeable);
		if (ret) {
			dev_err(&pdev->dev, "request wake irq error\n");
			goto error;
		}
	}

	if (gpio_is_valid(pdata->boot_gpio)) {
		/* request boot irq from platform data */
		ret = mdm_request_irq(modem,
				      tegra_usb_modem_boot_thread,
				      pdata->boot_gpio,
				      pdata->boot_irq_flags,
				      "mdm_boot",
				      &modem->boot_irq,
				      &modem->boot_irq_wakeable);
		if (ret) {
			dev_err(&pdev->dev, "request boot irq error\n");
			goto error;
		}
	} else
		dev_warn(&pdev->dev, "boot irq not specified\n");

	/* create sysfs node to load/unload host controller */
	ret = device_create_file(&pdev->dev, &dev_attr_load_host);
	if (ret) {
		dev_err(&pdev->dev, "can't create sysfs file\n");
		goto error;
	}
	modem->sysfs_file_created = 1;
	modem->capability = TEGRA_USB_HOST_RELOAD;
	if (pdev->id >= 0)
		dev_set_name(&pdev->dev, "MDM%d", pdev->id);
	else
		dev_set_name(&pdev->dev, "MDM");

	/* create sysfs node for RIL to report modem power state */
	/* only if a regulator has been requested          */
	modem->modem_state_file_created = 0;
	if (pdata->regulator_name) {
		ret = device_create_file(&pdev->dev, &dev_attr_modem_state);
		if (ret) {
			dev_err(&pdev->dev,
			"can't create modem state sysfs file\n");
			goto error;
		}
		modem->modem_state_file_created = 1;
		mutex_init(&modem->modem_state_lock);
	}

	/* create work queue platform_driver_registe */
	modem->wq = create_workqueue("tegra_usb_mdm_queue");
	INIT_DELAYED_WORK(&modem->recovery_work, tegra_usb_modem_recovery);
	INIT_DELAYED_WORK(&modem->enable_nvhsic_work, enable_nvhsic_power);
	INIT_WORK(&modem->host_load_work, tegra_usb_host_load);
	INIT_WORK(&modem->host_unload_work, tegra_usb_host_unload);
	INIT_WORK(&modem->cpu_boost_work, cpu_freq_boost);
	INIT_DELAYED_WORK(&modem->cpu_unboost_work, cpu_freq_unboost);

	pm_qos_add_request(&modem->cpu_boost_req, PM_QOS_CPU_FREQ_MIN,
			   PM_QOS_DEFAULT_VALUE);

	modem->pm_notifier.notifier_call = mdm_pm_notifier;
	modem->usb_notifier.notifier_call = mdm_usb_notifier;

	usb_register_notify(&modem->usb_notifier);
	register_pm_notifier(&modem->pm_notifier);

	if (pdata->num_temp_sensors) {
		thermdata.num_zones = pdata->num_temp_sensors;
		modem_thermal_device.dev.platform_data = &thermdata;
		modem->modem_thermal_pdev = &modem_thermal_device;
		platform_device_register(modem->modem_thermal_pdev);
	}

	if (pdata->mdm_power_report_gpio) {
		edpdata.mdm_power_report = pdata->mdm_power_report_gpio;
		modem_edp_device.dev.platform_data = &edpdata;
		modem->modem_edp_pdev = &modem_edp_device;
		platform_device_register(modem->modem_edp_pdev);
	}

	/* start modem */
	if (modem->ops && modem->ops->start)
		modem->ops->start();

	return ret;
error:
	if (modem->sysfs_file_created)
		device_remove_file(&pdev->dev, &dev_attr_load_host);

	if (modem->wake_irq)
		free_irq(modem->wake_irq, modem);

	if (modem->boot_irq)
		free_irq(modem->boot_irq, modem);

	if (modem->modem_state_file_created)
		device_remove_file(&pdev->dev, &dev_attr_modem_state);

	return ret;
}

static int tegra_usb_modem_parse_dt(struct tegra_usb_modem *modem,
		struct platform_device *pdev)
{
	struct tegra_usb_modem_power_platform_data pdata;
	struct device_node *node = pdev->dev.of_node;
	struct device_node *phy_node;
	const unsigned int *prop;
	int gpio;
	int ret;
	const char *node_status;
#if defined(CONFIG_ARCH_TEGRA_21x_SOC)
	struct device_node *ehci_node = NULL;
#endif

	if (!node) {
		dev_err(&pdev->dev, "Missing device tree node\n");
		return -EINVAL;
	}

	memset(&pdata, 0, sizeof(pdata));

	/* turn on modem regulator if required */
	pdata.regulator_name = of_get_property(node, "nvidia,regulator", NULL);
	if (pdata.regulator_name) {
		modem->regulator = devm_regulator_get(&pdev->dev,
				pdata.regulator_name);
		if (IS_ERR(modem->regulator)) {
			dev_err(&pdev->dev, "failed to get regulator %s\n",
				pdata.regulator_name);
			return PTR_ERR(modem->regulator);
		}
		ret = regulator_enable(modem->regulator);
		if (ret) {
			dev_err(&pdev->dev, "failed to enable regulator %s\n",
				pdata.regulator_name);
			return ret;
		}

		dev_info(&pdev->dev, "set modem regulator:%s\n",
		pdata.regulator_name);

		/* Enable regulator bypass */
		if (regulator_allow_bypass(modem->regulator, true))
			dev_warn(&pdev->dev,
			"failed to set modem regulator in bypass mode\n");
		else
			dev_info(&pdev->dev,
			"set modem regulator in bypass mode");

		/* Enable autoPFM */
		if (regulator_set_mode(modem->regulator, REGULATOR_MODE_NORMAL))
			dev_warn(&pdev->dev,
			"failed to set modem regulator in normal mode\n");
		else
			dev_info(&pdev->dev,
			"set modem regulator in normal mode");

		/* Set modem_power_state */
		modem->modem_power_state = RAT_3G_LTE;
	}

	/* determine phy type */
	modem->phy_type = -1;
	ret = of_property_read_u32(node, "nvidia,phy-type", &modem->phy_type);
	if (0 == ret) {
		dev_info(&pdev->dev,
			"set phy type with property 'nvidia,phy-type'\n");
	} else {
		dev_info(&pdev->dev,
			"set phy type with child node 'nvidia,phy-*hci-*'\n");
		for_each_child_of_node(node, phy_node) {
			ret = of_property_read_string(phy_node,
							"status", &node_status);
			if (ret != 0) {
				dev_err(&pdev->dev,
					"DT property '%s/status' read fail!\n",
					phy_node->full_name);
				goto error;
			}

			if (strcmp(node_status, "okay") == 0) {
				if (strcmp(phy_node->name,
						"nvidia,phy-ehci-hsic") == 0)
					modem->phy_type = EHCI_HSIC;
				else if (strcmp(phy_node->name,
						"nvidia,phy-xhci-hsic") == 0)
					modem->phy_type = XHCI_HSIC;
				else if (strcmp(phy_node->name,
						"nvidia,phy-xhci-utmi") == 0)
					modem->phy_type = XHCI_UTMI;
				else {
					dev_err(&pdev->dev,
						"Unrecognized phy type node!!\n");
					ret = -EINVAL;
					goto error;
				}
			}
		}
	}
	if (-1 == modem->phy_type) {
		dev_err(&pdev->dev,
			"Unable to set phy type!!\n");
		ret = -EINVAL;
		goto error;
	}

#if defined(CONFIG_ARCH_TEGRA_21x_SOC)
	/* get EHCI device/pdata handle */
	if (modem->phy_type == EHCI_HSIC) {
		ehci_node = of_parse_phandle(node, "nvidia,ehci-device", 0);
		if (!ehci_node) {
			dev_err(&pdev->dev, "can't find nvidia,ehci-device\n");
			return -EINVAL;
		}
		/* set ehci device/pdata */
		pdata.ehci_node = ehci_node;
	}
#endif

	prop = of_get_property(node, "nvidia,num-temp-sensors", NULL);
	if (prop)
		pdata.num_temp_sensors = be32_to_cpup(prop);

	/* Configure input GPIOs */
	gpio = of_get_named_gpio(node, "nvidia,wake-gpio", 0);
	if (gpio == -EPROBE_DEFER) {
		ret = -EPROBE_DEFER;
		goto error;
	}
	pdata.wake_gpio = gpio_is_valid(gpio) ? gpio : -1;
	dev_info(&pdev->dev, "set MDM_WAKE_AP gpio:%d\n", pdata.wake_gpio);
	if (gpio_is_valid(gpio)) {
		ret = devm_gpio_request(&pdev->dev, gpio, "MDM_WAKE_AP");
		if (ret) {
			dev_err(&pdev->dev, "request gpio %d failed\n", gpio);
			goto error;
		}
		gpio_direction_input(gpio);
	}

	gpio = of_get_named_gpio(node, "nvidia,boot-gpio", 0);
	if (gpio == -EPROBE_DEFER) {
		ret = -EPROBE_DEFER;
		goto error;
	}
	pdata.boot_gpio = gpio_is_valid(gpio) ? gpio : -1;
	dev_info(&pdev->dev, "set MDM_COLDBOOT gpio:%d\n", pdata.boot_gpio);
	if (gpio_is_valid(gpio)) {
		ret = devm_gpio_request(&pdev->dev, gpio, "MDM_COLDBOOT");
		if (ret) {
			dev_err(&pdev->dev, "request gpio %d failed\n", gpio);
			goto error;
		}
		gpio_direction_input(gpio);
	}

	gpio = of_get_named_gpio(node, "nvidia,mdm-power-report-gpio", 0);
	if (gpio == -EPROBE_DEFER) {
		ret = -EPROBE_DEFER;
		goto error;
	}
	pdata.mdm_power_report_gpio = gpio_is_valid(gpio) ? gpio : -1;
	dev_info(&pdev->dev, "set MDM_PWR_REPORT gpio:%d\n",
		pdata.mdm_power_report_gpio);

	gpio = of_get_named_gpio(node, "nvidia,pre-boost-gpio", 0);
	if (gpio == -EPROBE_DEFER) {
		ret = -EPROBE_DEFER;
		goto error;
	}
	if (gpio_is_valid(gpio)) {
		dev_info(&pdev->dev, "set pre boost gpio (%d) to 1\n", gpio);
		ret = devm_gpio_request(&pdev->dev, gpio, "MODEM PREBOOST");
		if (ret) {
			dev_err(&pdev->dev, "request gpio %d failed\n", gpio);
			goto error;
		}
		gpio_direction_output(gpio, 1);
		modem->pre_boost_gpio = gpio;
	} else
		modem->pre_boost_gpio = -1;

	/* set GPIO IRQ flags */
	pdata.wake_irq_flags = pdata.boot_irq_flags =
		pdata.mdm_power_irq_flags =
		IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_ONESHOT;

	/* initialize necessary output GPIO and start modem here */
	gpio = of_get_named_gpio(node, "nvidia,mdm-en-gpio", 0);
	if (gpio == -EPROBE_DEFER) {
		ret = -EPROBE_DEFER;
		goto error;
	}
	if (gpio_is_valid(gpio)) {
		dev_info(&pdev->dev, "set MODEM EN (%d) to 1\n", gpio);
		ret = devm_gpio_request(&pdev->dev, gpio, "MODEM EN");
		if (ret) {
			dev_err(&pdev->dev, "request gpio %d failed\n", gpio);
			goto error;
		}
		gpio_direction_output(gpio, 1);
	}

	gpio = of_get_named_gpio(node, "nvidia,reset-gpio", 0);
	if (gpio == -EPROBE_DEFER) {
		ret = -EPROBE_DEFER;
		goto error;
	}
	if (gpio_is_valid(gpio)) {
		ret = devm_gpio_request(&pdev->dev, gpio, "MODEM RESET");
		if (ret) {
			dev_err(&pdev->dev, "request gpio %d failed\n", gpio);
			goto error;
		}
		/* boot modem now */
		/* Modem requires at least 10ms between MDM_EN assertion
		and release of the reset. 20ms is the min value of msleep */
		msleep(20);
		/* Release modem reset to start boot */
		dev_info(&pdev->dev, "set MODEM RESET (%d) to 1\n", gpio);
		gpio_direction_output(gpio, 1);
		gpio_export(gpio, false);

		/* also create a dedicated sysfs for the modem-reset gpio */
		gpio_export_link(&pdev->dev, "modem_reset", gpio);
	}

	/* add platform data to pdev */
	platform_device_add_data(pdev, &pdata, sizeof(pdata));
	return 0;

error:
	if (modem->regulator)
		regulator_disable(modem->regulator);
	return ret;
}

static int tegra_usb_modem_probe(struct platform_device *pdev)
{
	struct tegra_usb_modem *modem;
	int ret = 0;

	modem = kzalloc(sizeof(struct tegra_usb_modem), GFP_KERNEL);
	if (!modem) {
		dev_dbg(&pdev->dev, "failed to allocate memory\n");
		return -ENOMEM;
	}

	/* initialize from device tree */
	ret = tegra_usb_modem_parse_dt(modem, pdev);
	if (ret) {
		dev_err(&pdev->dev, "device tree parsing error\n");
		kfree(modem);
		return ret;
	}

	ret = mdm_init(modem, pdev);
	if (ret) {
		kfree(modem);
		return ret;
	}

	dev_set_drvdata(&pdev->dev, modem);

	return ret;
}

static int __exit tegra_usb_modem_remove(struct platform_device *pdev)
{
	struct tegra_usb_modem *modem = platform_get_drvdata(pdev);

	unregister_pm_notifier(&modem->pm_notifier);
	usb_unregister_notify(&modem->usb_notifier);

	if (modem->modem_edp_pdev)
		platform_device_unregister(modem->modem_edp_pdev);

	if (modem->wake_irq)
		free_irq(modem->wake_irq, modem);

	if (modem->boot_irq)
		free_irq(modem->boot_irq, modem);

	if (modem->sysfs_file_created)
		device_remove_file(&pdev->dev, &dev_attr_load_host);

	if (modem->modem_state_file_created)
		device_remove_file(&pdev->dev, &dev_attr_modem_state);

	if (!IS_ERR(modem->regulator))
		regulator_put(modem->regulator);

	cancel_delayed_work_sync(&modem->recovery_work);
	cancel_delayed_work_sync(&modem->enable_nvhsic_work);
	cancel_work_sync(&modem->host_load_work);
	cancel_work_sync(&modem->host_unload_work);
	cancel_work_sync(&modem->cpu_boost_work);
	cancel_delayed_work_sync(&modem->cpu_unboost_work);
	destroy_workqueue(modem->wq);

	pm_qos_remove_request(&modem->cpu_boost_req);

	kfree(modem);
	return 0;
}

static void tegra_usb_modem_shutdown(struct platform_device *pdev)
{
	struct tegra_usb_modem *modem = platform_get_drvdata(pdev);

	if (modem->ops && modem->ops->stop)
		modem->ops->stop();
}

#ifdef CONFIG_PM
static int tegra_usb_modem_suspend(struct platform_device *pdev,
				   pm_message_t state)
{
	struct tegra_usb_modem *modem = platform_get_drvdata(pdev);
	int ret = 0;

	/* send L3 hint to modem */
	if (modem->ops && modem->ops->suspend)
		modem->ops->suspend();

	if (modem->wake_irq) {
		ret = enable_irq_wake(modem->wake_irq);
		if (ret) {
			pr_info("%s, wake irq=%d, error=%d\n",
				__func__, modem->wake_irq, ret);
			goto fail;
		}
	}

	if (modem->boot_irq) {
		ret = enable_irq_wake(modem->boot_irq);
		if (ret)
			pr_info("%s, wake irq=%d, error=%d\n",
				__func__, modem->boot_irq, ret);
	}

	if (gpio_is_valid(modem->pre_boost_gpio))
		gpio_set_value(modem->pre_boost_gpio, 0);
fail:
	return ret;
}

static int tegra_usb_modem_resume(struct platform_device *pdev)
{
	struct tegra_usb_modem *modem = platform_get_drvdata(pdev);
	int ret = 0;

	if (gpio_is_valid(modem->pre_boost_gpio))
		gpio_set_value(modem->pre_boost_gpio, 1);

	if (modem->boot_irq) {
		ret = disable_irq_wake(modem->boot_irq);
		if (ret)
			pr_err("Failed to disable modem boot_irq\n");
	}

	if (modem->wake_irq) {
		ret = disable_irq_wake(modem->wake_irq);
		if (ret)
			pr_err("Failed to disable modem wake_irq\n");
	} else if (get_wakeup_reason_irq() == INT_USB2) {
		pr_info("%s: remote wakeup from USB. Hold a timed wakelock\n",
				__func__);
		wake_lock_timeout(&modem->wake_lock,
				WAKELOCK_TIMEOUT_FOR_REMOTE_WAKE);
	}

	/* send L3->L0 hint to modem */
	if (modem->ops && modem->ops->resume)
		modem->ops->resume();
	return 0;
}
#endif

static const struct of_device_id tegra_usb_modem_match[] = {
	{ .compatible = "nvidia,icera-i500", },
	{},
};
MODULE_DEVICE_TABLE(of, tegra_usb_modem_match);

static struct platform_driver tegra_usb_modem_power_driver = {
	.driver = {
		   .name = "tegra_usb_modem_power",
		   .owner = THIS_MODULE,
		   .of_match_table = of_match_ptr(tegra_usb_modem_match),
		   },
	.probe = tegra_usb_modem_probe,
	.remove = __exit_p(tegra_usb_modem_remove),
	.shutdown = tegra_usb_modem_shutdown,
#ifdef CONFIG_PM
	.suspend = tegra_usb_modem_suspend,
	.resume = tegra_usb_modem_resume,
#endif
};

static int __init tegra_usb_modem_power_init(void)
{
	return platform_driver_register(&tegra_usb_modem_power_driver);
}

module_init(tegra_usb_modem_power_init);

static void __exit tegra_usb_modem_power_exit(void)
{
	platform_driver_unregister(&tegra_usb_modem_power_driver);
}

module_exit(tegra_usb_modem_power_exit);

MODULE_DESCRIPTION("Tegra usb modem power management driver");
MODULE_LICENSE("GPL");
