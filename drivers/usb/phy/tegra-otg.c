/*
 * drivers/usb/otg/tegra-otg.c
 *
 * OTG transceiver driver for Tegra UTMI phy
 *
 * Copyright (C) 2010-2015 NVIDIA CORPORATION. All rights reserved.
 * Copyright (C) 2010 Google, Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
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

#include <linux/usb.h>
#include <linux/usb/otg.h>
#include <linux/usb/gadget.h>
#include <linux/platform_device.h>
#include <linux/platform_data/tegra_usb.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/export.h>
#include <linux/pm_runtime.h>
#include <linux/extcon.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/usb/hcd.h>
#include <linux/tegra_pm_domains.h>
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/debugfs.h>

#include <mach/tegra_usb_pad_ctrl.h>
#include "../../../arch/arm/mach-tegra/iomap.h"
#include "../../../arch/arm/mach-tegra/board.h"

#define USB_PHY_WAKEUP		0x408
#define  USB_ID_INT_EN		(1 << 0)
#define  USB_ID_INT_STATUS	(1 << 1)
#define  USB_ID_STATUS		(1 << 2)
#define  USB_ID_PIN_WAKEUP_EN	(1 << 6)
#define  USB_VBUS_WAKEUP_EN	(1 << 30)
#define  USB_VBUS_INT_EN	(1 << 8)
#define  USB_VBUS_INT_STATUS	(1 << 9)
#define  USB_VBUS_STATUS	(1 << 10)
#define  USB_ID_SW_EN		(1 << 3)
#define  USB_ID_SW_VALUE	(1 << 4)
#define  USB_INT_EN		(USB_VBUS_INT_EN | USB_ID_INT_EN | \
						USB_VBUS_WAKEUP_EN | USB_ID_PIN_WAKEUP_EN)
#define USB_VBUS_INT_STS_MASK	(0x7 << 8)
#define USB_ID_INT_STS_MASK	(0x7 << 0)

#if defined(CONFIG_ARM64)
#define UTMI1_PORT_OWNER_XUSB   0x1
#endif

#ifdef OTG_DEBUG
#define DBG(stuff...)	pr_info("tegra-otg: " stuff)
#else
#define DBG(stuff...)	do {} while (0)
#endif

#define YCABLE_CHARGING_CURRENT_UA 1200000u
#define ACA_RID_A_CURRENT_UA 2000000u

enum tegra_otg_aca_state {
	RID_NONE,
	RID_A,
	RID_B,
	RID_C,
};

struct tegra_otg {
	struct platform_device *pdev;
	struct device dev;
	struct tegra_usb_otg_data *pdata;
	struct device_node *ehci_node;
	struct usb_phy phy;
	unsigned long int_status;
	unsigned long int_mask;
	spinlock_t lock;
	struct mutex irq_work_mutex;
	void __iomem *regs;
	struct clk *clk;
	int irq;
	struct work_struct work;
	struct regulator *vbus_reg;
	struct regulator *vbus_bat_reg;
	unsigned int intr_reg_data;
	bool support_y_cable;
	bool support_aca_nv_cable;
	bool y_cable_conn;
	bool rid_a_conn;
	bool turn_off_vbus_on_lp0;
	bool clk_enabled;
	bool interrupt_mode;
	bool suspended;
	bool support_pmu_vbus;
	bool support_usb_id;
	bool support_pmu_id;
	bool support_pmu_rid;
	bool support_hp_trans;
	enum tegra_usb_id_detection id_det_type;
	struct extcon_dev *id_extcon_dev;
	struct extcon_dev *vbus_extcon_dev;
	struct extcon_dev *aca_nv_extcon_dev;
	struct extcon_dev *y_extcon_dev;
	struct extcon_dev *edev;
	struct notifier_block otg_aca_nv_nb;
	struct notifier_block otg_y_nb;
	struct extcon_cable *id_extcon_cable;
	struct extcon_cable *vbus_extcon_cable;
	struct extcon_cable *aca_nv_extcon_cable;
	struct extcon_cable *y_extcon_cable;
	struct extcon_specific_cable_nb id_extcon_obj;
	struct extcon_specific_cable_nb vbus_extcon_obj;
	struct extcon_specific_cable_nb aca_nv_extcon_obj;
	struct extcon_specific_cable_nb y_extcon_obj;
	struct tegra_otg_cables *rid_a;
	struct tegra_otg_cables *rid_b;
	struct tegra_otg_cables *rid_c;
	enum tegra_otg_aca_state aca_state;
};

struct tegra_otg_soc_data {
	struct platform_device *ehci_device;
	struct tegra_usb_platform_data *ehci_pdata;
};

struct tegra_otg_cables {
	struct extcon_dev *edev;
	struct extcon_cable *ecable;
	struct notifier_block nb;
	struct extcon_specific_cable_nb extcon_obj;
};

static u64 tegra_ehci_dmamask = DMA_BIT_MASK(64);
static struct tegra_otg *tegra_clone;
static struct notifier_block otg_vbus_nb;
static struct notifier_block otg_id_nb;
static struct wakeup_source *otg_work_wl;

enum tegra_connect_type {
	CONNECT_TYPE_Y_CABLE,
	CONNECT_TYPE_RID_A,
};

static inline unsigned long otg_readl(struct tegra_otg *tegra,
				      unsigned int offset)
{
	return readl(tegra->regs + offset);
}

static inline void otg_writel(struct tegra_otg *tegra, unsigned long val,
			      unsigned int offset)
{
	writel(val, tegra->regs + offset);
}

static char *const tegra_otg_extcon_cable[] = {
	[CONNECT_TYPE_Y_CABLE] = "Y-cable",
	[CONNECT_TYPE_RID_A] = "ACA RID-A",
	NULL,
};

static void tegra_otg_set_extcon_state(struct tegra_otg *tegra)
{
	const char **cables;
	struct extcon_dev *edev;

	if (tegra->edev == NULL || tegra->edev->supported_cable == NULL)
		return;

	edev = tegra->edev;
	cables = tegra->edev->supported_cable;

	if (tegra->y_cable_conn)
		extcon_set_cable_state(edev, cables[CONNECT_TYPE_Y_CABLE],
				true);
	else if (tegra->rid_a_conn)
		extcon_set_cable_state(edev, cables[CONNECT_TYPE_RID_A],
				true);
	else
		extcon_set_state(edev, 0x0);
}

static void otg_notifications_of(struct tegra_otg *tegra)
{
	unsigned long val;
	int index;
	bool vbus = false, id = false;

	if (tegra->support_pmu_vbus) {
		index = tegra->vbus_extcon_cable->cable_index;
		if (extcon_get_cable_state_(tegra->vbus_extcon_dev, index))
			vbus = true;
	}

	if (tegra->support_aca_nv_cable) {
		index = tegra->aca_nv_extcon_cable->cable_index;
		if (extcon_get_cable_state_(tegra->aca_nv_extcon_dev, index))
			vbus = true;
	}

	if (tegra->support_y_cable) {
		index = tegra->y_extcon_cable->cable_index;
		if (extcon_get_cable_state_(tegra->y_extcon_dev, index)) {
			vbus = true;
			id = true;
		}
	}

	if (tegra->support_pmu_id) {
		index = tegra->id_extcon_cable->cable_index;
		if (extcon_get_cable_state_(tegra->id_extcon_dev, index))
			id = true;
	}

	if (tegra->support_pmu_rid) {
		if (extcon_get_cable_state_(tegra->rid_a->edev,
			       tegra->rid_a->ecable->cable_index)) {
			tegra->aca_state = RID_A;
			id = true;
		} else if (extcon_get_cable_state_(tegra->rid_b->edev,
			       tegra->rid_b->ecable->cable_index)) {
			tegra->aca_state = RID_B;
			vbus = true;
		} else if (extcon_get_cable_state_(tegra->rid_c->edev,
			       tegra->rid_c->ecable->cable_index)) {
			tegra->aca_state = RID_C;
			vbus = true;
		} else {
			tegra->aca_state = RID_NONE;
		}
	}

	if (tegra->support_pmu_vbus || tegra->support_aca_nv_cable ||
			tegra->support_y_cable) {
		if (vbus)
			tegra->int_status |= USB_VBUS_STATUS;
		else
			tegra->int_status &= ~USB_VBUS_STATUS;
	}

	if (tegra->support_pmu_id || tegra->support_y_cable) {
		if (id) {
			tegra->int_status &= ~USB_ID_STATUS;
			tegra->int_status |= USB_ID_INT_EN;

			val = otg_readl(tegra, USB_PHY_WAKEUP);
			val |= USB_ID_SW_EN;
			val &= ~USB_ID_SW_VALUE;
			otg_writel(tegra, val, USB_PHY_WAKEUP);
		 } else {
			tegra->int_status |= USB_ID_STATUS;
			val = otg_readl(tegra, USB_PHY_WAKEUP);
			val |= USB_ID_SW_VALUE;
			otg_writel(tegra, val, USB_PHY_WAKEUP);
		}
	}
}

static void otg_notifications_pdata(struct tegra_otg *tegra)
{
	unsigned long val;

	if (tegra->support_pmu_vbus) {
		if (extcon_get_cable_state(tegra->vbus_extcon_dev, "USB"))
			tegra->int_status |= USB_VBUS_STATUS ;
		else
			tegra->int_status &= ~USB_VBUS_STATUS;
	}

	if (tegra->support_pmu_id) {
		if (extcon_get_cable_state(tegra->id_extcon_dev, "USB-Host")) {
			tegra->int_status &= ~USB_ID_STATUS;
			tegra->int_status |= USB_ID_INT_EN;

			val = otg_readl(tegra, USB_PHY_WAKEUP);
			val |= USB_ID_SW_EN;
			val &= ~USB_ID_SW_VALUE;
			otg_writel(tegra, val, USB_PHY_WAKEUP);
		 } else {
			tegra->int_status |= USB_ID_STATUS;
			val = otg_readl(tegra, USB_PHY_WAKEUP);
			val |= USB_ID_SW_VALUE;
			otg_writel(tegra, val, USB_PHY_WAKEUP);
		}
	}
}

static int otg_notifications(struct notifier_block *nb,
				   unsigned long event, void *unused)
{
	struct tegra_otg *tegra = tegra_clone;
	unsigned long flags;
	DBG("%s(%d) Begin\n", __func__, __LINE__);

	spin_lock_irqsave(&tegra->lock, flags);
	if (tegra->dev.of_node)
		otg_notifications_of(tegra);
	else
		otg_notifications_pdata(tegra);
	spin_unlock_irqrestore(&tegra->lock, flags);

	DBG("%s(%d) tegra->int_status = 0x%lx\n", __func__,
				__LINE__, tegra->int_status);

	if (!tegra->suspended)
		schedule_work(&tegra->work);

	DBG("%s(%d) End\n", __func__, __LINE__);
	return NOTIFY_DONE;
}

static const char *tegra_state_name(enum usb_otg_state state)
{
	switch (state) {
		case OTG_STATE_A_HOST:
			return "HOST";
		case OTG_STATE_B_PERIPHERAL:
			return "PERIPHERAL";
		case OTG_STATE_A_SUSPEND:
			return "SUSPEND";
		case OTG_STATE_UNDEFINED:
			return "UNDEFINED";
		default:
			return "INVALID";
	}
}

static unsigned long enable_interrupt(struct tegra_otg *tegra, bool en)
{
	unsigned long val;

	pm_runtime_get_sync(tegra->phy.dev);
	clk_prepare_enable(tegra->clk);
	val = otg_readl(tegra, USB_PHY_WAKEUP);
	if (en) {
		/* Enable ID interrupt if detection is through USB controller */
		if (tegra->support_usb_id) {
			val |= USB_ID_INT_EN | USB_ID_PIN_WAKEUP_EN;
			tegra->int_mask |= USB_ID_INT_STS_MASK;
		}

		/* Enable vbus interrupt if cable is not detected through PMU */
		if (!tegra->support_pmu_vbus) {
			val |= USB_VBUS_INT_EN | USB_VBUS_WAKEUP_EN;
			tegra->int_mask |= USB_VBUS_INT_STS_MASK;
		}
	}
	else
		val &= ~USB_INT_EN;

	otg_writel(tegra, val, USB_PHY_WAKEUP);
	/* Add delay to make sure register is updated */
	udelay(1);
	clk_disable_unprepare(tegra->clk);
	pm_runtime_mark_last_busy(tegra->phy.dev);
	pm_runtime_put_autosuspend(tegra->phy.dev);

	DBG("%s(%d) interrupt mask = 0x%lx\n", __func__, __LINE__,
							tegra->int_mask);
	return val;
}

static void tegra_otg_set_current(struct regulator *vbus_bat_reg, int max_uA)
{
	if (vbus_bat_reg == NULL)
		return ;

	regulator_set_current_limit(vbus_bat_reg, 0, max_uA);
}

static void tegra_otg_vbus_enable(struct tegra_otg *tegra, int on)
{
	static int vbus_enable = 1;

	if (tegra->vbus_reg == NULL)
		return;

	if (tegra->support_pmu_rid &&
		extcon_get_cable_state_(tegra->rid_a->edev,
			       tegra->rid_a->ecable->cable_index))
		return;

	if (on && vbus_enable && !regulator_enable(tegra->vbus_reg))
		vbus_enable = 0;
	else if (!on && !vbus_enable && !regulator_disable(tegra->vbus_reg))
		vbus_enable = 1;
}

static void tegra_start_host(struct tegra_otg *tegra)
{
	struct tegra_usb_otg_data *pdata = tegra->pdata;
	struct platform_device *pdev = NULL;
	struct device_node *ehci_node = tegra->ehci_node;
	struct property *status_prop;
	int val;

	DBG("%s(%d) Begin\n", __func__, __LINE__);

	if (tegra->pdev)
		return;

	if (pdata->is_xhci) {
		tegra_xhci_release_otg_port(false);
		return;
	}

	/* if this node is unavailable, we're unable to register it later */
	if (ehci_node) {
		if (!of_device_is_available(ehci_node)) {
			pr_info("%s(): enable ehci node\n", __func__);
			status_prop = of_find_property(ehci_node,
			"status", NULL);
			if (!status_prop) {
				pr_err("%s(): error getting status property\n",
					__func__);
				return;
			}
			strcpy((char *)status_prop->value, "okay");
		}
		/* we have to hard-code EHCI device name here for now */
		if (ehci_node) {
			pdev = of_platform_device_create(ehci_node,
							"tegra-ehci.0", NULL);
			if (!pdev) {
				pr_info("%s: error registering EHCI\n",
								__func__);
				return;
			}
		}
	}

	/* prepare device structure for registering host */
	if (!pdev) {
		struct platform_device *ehci_device = pdata->ehci_device;
		pdev = platform_device_alloc(ehci_device->name,
							ehci_device->id);
		if (!pdev)
			return;

		val = platform_device_add_resources(pdev,
			ehci_device->resource, ehci_device->num_resources);
		if (val)
			goto error;

		pdev->dev.dma_mask = ehci_device->dev.dma_mask;
		pdev->dev.coherent_dma_mask =
					ehci_device->dev.coherent_dma_mask;

		val = platform_device_add_data(pdev, pdata->ehci_pdata,
				sizeof(struct tegra_usb_platform_data));
		if (val)
			goto error;

		val = platform_device_add(pdev);
		if (val) {
			pr_err("%s: platform_device_add failed\n", __func__);
			goto error;
		}
	}

	tegra->pdev = pdev;

	DBG("%s(%d) End\n", __func__, __LINE__);
	return;

error:
	BUG_ON("failed to add the host controller device\n");
	platform_device_del(pdev);
	tegra->pdev = NULL;
}

static void tegra_stop_host(struct tegra_otg *tegra)
{
	struct platform_device *pdev = tegra->pdev;

	DBG("%s(%d) Begin\n", __func__, __LINE__);

	if (tegra->pdata->is_xhci) {
		tegra_xhci_release_otg_port(true);
		return;
	}
	if (pdev) {
		/* unregister host from otg */
		if (!tegra->ehci_node)
			platform_device_unregister(pdev);
		else {
			of_device_unregister(pdev);
			of_node_clear_flag(tegra->ehci_node, OF_POPULATED);
			of_node_put(tegra->ehci_node);
		}
		tegra->pdev = NULL;
	}

	DBG("%s(%d) End\n", __func__, __LINE__);
}

static void tegra_otg_notify_event(struct tegra_otg *tegra, int event)
{
	tegra->phy.last_event = event;
	atomic_notifier_call_chain(&tegra->phy.notifier, event, tegra->phy.otg->gadget);
}

static int tegra_otg_start_host(struct tegra_otg *tegra, int on)
{
	if (on) {
		if (tegra->support_y_cable &&
				(tegra->int_status & USB_VBUS_STATUS)) {
			DBG("%s(%d) set current %dmA\n", __func__, __LINE__,
					YCABLE_CHARGING_CURRENT_UA/1000);
			tegra_otg_set_current(tegra->vbus_bat_reg,
					YCABLE_CHARGING_CURRENT_UA);
			tegra->y_cable_conn = true;
			tegra_otg_set_extcon_state(tegra);
		} else if (tegra->support_pmu_rid &&
				tegra->aca_state == RID_A) {
			tegra_otg_set_current(tegra->vbus_bat_reg,
					ACA_RID_A_CURRENT_UA);
			tegra->rid_a_conn = true;
			tegra_otg_set_extcon_state(tegra);
		} else {
			tegra_otg_vbus_enable(tegra, 1);
		}
		tegra_start_host(tegra);
		tegra_otg_notify_event(tegra, USB_EVENT_ID);
	} else {
		tegra_stop_host(tegra);
		tegra_otg_notify_event(tegra, USB_EVENT_NONE);
		tegra_otg_vbus_enable(tegra, 0);
		if (tegra->support_y_cable && tegra->y_cable_conn) {
			tegra_otg_set_current(tegra->vbus_bat_reg, 0);
			tegra->y_cable_conn = false;
			tegra_otg_set_extcon_state(tegra);
		} else if (tegra->support_pmu_rid &&
			       tegra->rid_a_conn) {
			tegra_otg_set_current(tegra->vbus_bat_reg, 0);
			tegra->rid_a_conn = false;
			tegra_otg_set_extcon_state(tegra);
		}
	}
	return 0;
}

static int tegra_otg_start_gadget(struct tegra_otg *tegra, int on)
{
	struct usb_otg *otg = tegra->phy.otg;

	if (on) {
		pm_runtime_get_sync(tegra->phy.dev);
		usb_gadget_vbus_connect(otg->gadget);
	} else {
		usb_gadget_vbus_disconnect(otg->gadget);
		pm_runtime_put_sync(tegra->phy.dev);
	}
	return 0;
}

static void tegra_change_otg_state(struct tegra_otg *tegra,
				enum usb_otg_state to)
{
	struct usb_otg *otg = tegra->phy.otg;
	enum usb_otg_state from = otg->phy->state;

	if (!tegra->interrupt_mode){
		DBG("OTG: Vbus detection is disabled");
		return;
	}

	DBG("%s(%d) requested otg state %s-->%s\n", __func__,
		__LINE__, tegra_state_name(from), tegra_state_name(to));

	if (to != OTG_STATE_UNDEFINED && from != to) {
		otg->phy->state = to;
		pr_info("otg state changed: %s --> %s\n", tegra_state_name(from), tegra_state_name(to));

		if (from == OTG_STATE_A_SUSPEND) {
			if (to == OTG_STATE_B_PERIPHERAL && otg->gadget)
				tegra_otg_start_gadget(tegra, 1);
			else if (to == OTG_STATE_A_HOST)
				tegra_otg_start_host(tegra, 1);
		} else if (from == OTG_STATE_A_HOST) {
			if (to == OTG_STATE_A_SUSPEND)
				tegra_otg_start_host(tegra, 0);
			else if (to == OTG_STATE_B_PERIPHERAL && otg->gadget
					&& tegra->support_hp_trans) {
				tegra_otg_start_host(tegra, 0);
				tegra_otg_start_gadget(tegra, 1);
			}
		} else if (from == OTG_STATE_B_PERIPHERAL && otg->gadget) {
			if (to == OTG_STATE_A_SUSPEND)
				tegra_otg_start_gadget(tegra, 0);
			else if (to == OTG_STATE_A_HOST
					&& tegra->support_hp_trans) {
				tegra_otg_start_gadget(tegra, 0);
				tegra_otg_start_host(tegra, 1);
			}
		}
	}

	if (tegra->support_y_cable && tegra->y_cable_conn &&
			from == to && from == OTG_STATE_A_HOST) {
		if (!(tegra->int_status & USB_VBUS_STATUS)) {
			DBG("%s(%d) Charger disconnect\n", __func__, __LINE__);
			tegra_otg_set_current(tegra->vbus_bat_reg, 0);
			tegra_otg_vbus_enable(tegra, 1);
			tegra->y_cable_conn = false;
			tegra_otg_set_extcon_state(tegra);
		}
	}

	if (tegra->support_pmu_rid &&
		from == to && from == OTG_STATE_B_PERIPHERAL)
		tegra_otg_start_gadget(tegra, 1);
}

static void irq_work(struct work_struct *work)
{
	struct tegra_otg *tegra =
		container_of(work, struct tegra_otg, work);
	struct usb_otg *otg = tegra->phy.otg;
	enum usb_otg_state from;
	enum usb_otg_state to = OTG_STATE_UNDEFINED;
	unsigned long flags;
	unsigned long status;

	__pm_stay_awake(otg_work_wl);
	msleep(50);

	mutex_lock(&tegra->irq_work_mutex);

	spin_lock_irqsave(&tegra->lock, flags);
	from = otg->phy->state;
	status = tegra->int_status;

	/* Debug prints */
	DBG("%s(%d) status = 0x%lx\n", __func__, __LINE__, status);
	if ((status & USB_ID_INT_STATUS) &&
			(status & USB_VBUS_INT_STATUS))
		DBG("%s(%d) got vbus & id interrupt\n", __func__, __LINE__);
	else {
		if (status & USB_ID_INT_STATUS)
			DBG("%s(%d) got id interrupt\n", __func__, __LINE__);
		if (status & USB_VBUS_INT_STATUS)
			DBG("%s(%d) got vbus interrupt\n", __func__, __LINE__);
	}

	if (!(status & USB_ID_STATUS) && (status & USB_ID_INT_EN))
		to = OTG_STATE_A_HOST;
	else if (status & USB_VBUS_STATUS &&
			(from != OTG_STATE_A_HOST || tegra->support_hp_trans))
		to = OTG_STATE_B_PERIPHERAL;
	else
		to = OTG_STATE_A_SUSPEND;

	spin_unlock_irqrestore(&tegra->lock, flags);
	tegra_change_otg_state(tegra, to);
	mutex_unlock(&tegra->irq_work_mutex);
	__pm_relax(otg_work_wl);
}

static irqreturn_t tegra_otg_irq(int irq, void *data)
{
	struct tegra_otg *tegra = data;
	unsigned long flags;
	unsigned long val;
	struct usb_hcd *hcd = bus_to_hcd(tegra->phy.otg->host);
	enum usb_otg_state state = tegra->phy.state;

	spin_lock_irqsave(&tegra->lock, flags);
	val = otg_readl(tegra, USB_PHY_WAKEUP);
	DBG("%s(%d) interrupt val = 0x%lx\n", __func__, __LINE__, val);

	if (val & (USB_VBUS_INT_EN | USB_ID_INT_EN)) {
		DBG("%s(%d) PHY_WAKEUP = 0x%lx\n", __func__, __LINE__, val);
		otg_writel(tegra, val, USB_PHY_WAKEUP);
		if ((val & USB_ID_INT_STATUS) || (val & USB_VBUS_INT_STATUS)) {
			tegra->int_status &= ~tegra->int_mask;
			tegra->int_status |= val & tegra->int_mask;
			schedule_work(&tegra->work);
		}
	}

	/* Re-acquire wakelock to service the device connected */
	/* Abort the suspend */

	if (state == OTG_STATE_A_HOST) {
		if (hcd && hcd->state == HC_STATE_SUSPENDED)
			tegra_otg_notify_event(tegra, USB_EVENT_ID);
	}
	spin_unlock_irqrestore(&tegra->lock, flags);

	return IRQ_HANDLED;
}


static int tegra_otg_set_peripheral(struct usb_otg *otg,
				struct usb_gadget *gadget)
{
	struct tegra_otg *tegra;
	unsigned long val;
	DBG("%s(%d) BEGIN\n", __func__, __LINE__);

	tegra = (struct tegra_otg *)container_of(otg->phy, struct tegra_otg, phy);
	otg->gadget = gadget;

	val = enable_interrupt(tegra, true);
	tegra->suspended = false;

	if (((val & USB_ID_STATUS) || tegra->support_pmu_id) &&
		 (val & USB_VBUS_STATUS) && !tegra->support_pmu_vbus)
		val |= USB_VBUS_INT_STATUS;
	else if (!(val & USB_ID_STATUS)) {
		if (tegra->support_usb_id)
			val |= USB_ID_INT_STATUS;
		else
			val &= ~USB_ID_INT_STATUS;
	}
	else
		val &= ~(USB_ID_INT_STATUS | USB_VBUS_INT_STATUS);

	if ((val & USB_ID_INT_STATUS) || (val & USB_VBUS_INT_STATUS)) {
		tegra->int_status = val;
		schedule_work(&tegra->work);
	}

	if (tegra->support_pmu_vbus || tegra->support_pmu_id ||
				tegra->support_aca_nv_cable)
		otg_notifications(NULL, 0, NULL);

	DBG("%s(%d) END\n", __func__, __LINE__);
	return 0;
}

static int tegra_otg_set_host(struct usb_otg *otg, struct usb_bus *host)
{
	struct tegra_otg *tegra = container_of(otg->phy, struct tegra_otg, phy);
	unsigned long val;
	DBG("%s(%d) BEGIN\n", __func__, __LINE__);

	otg->host = host;

	pm_runtime_get_sync(tegra->phy.dev);
	clk_prepare_enable(tegra->clk);
	val = otg_readl(tegra, USB_PHY_WAKEUP);
	val &= ~(USB_VBUS_INT_STATUS | USB_ID_INT_STATUS);
	otg_writel(tegra, val, USB_PHY_WAKEUP);
	if (tegra->support_usb_id) {
		val = otg_readl(tegra, USB_PHY_WAKEUP);
		val |= (USB_ID_INT_EN | USB_ID_PIN_WAKEUP_EN);
		otg_writel(tegra, val, USB_PHY_WAKEUP);
	}
	clk_disable_unprepare(tegra->clk);
	pm_runtime_mark_last_busy(tegra->phy.dev);
	pm_runtime_put_autosuspend(tegra->phy.dev);

	DBG("%s(%d) END\n", __func__, __LINE__);
	return 0;
}

static ssize_t show_host_en(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct tegra_otg *tegra = platform_get_drvdata(pdev);
	struct usb_otg *otg = tegra->phy.otg;

	*buf = ((!tegra->interrupt_mode) &&
			(otg->phy->state == OTG_STATE_A_HOST)) ? '1' : '0';

	strcat(buf, "\n");
	return strlen(buf);
}

static ssize_t store_host_en(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct tegra_otg *tegra = platform_get_drvdata(pdev);
	int host;

	if (sscanf(buf, "%d", &host) != 1 || host < 0 || host > 1)
		return -EINVAL;

	if (host) {
		enable_interrupt(tegra, false);
		tegra->interrupt_mode = true;
		tegra_change_otg_state(tegra, OTG_STATE_A_SUSPEND);
		tegra_change_otg_state(tegra, OTG_STATE_A_HOST);
		tegra->interrupt_mode = false;
	} else {
		tegra->interrupt_mode = true;
		tegra_change_otg_state(tegra, OTG_STATE_A_SUSPEND);
		enable_interrupt(tegra, true);
	}

	return count;
}

static DEVICE_ATTR(enable_host, 0644, show_host_en, store_host_en);

static ssize_t show_device_en(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct tegra_otg *tegra = platform_get_drvdata(pdev);
	struct usb_otg *otg = tegra->phy.otg;

	*buf = ((!tegra->interrupt_mode) &&
			otg->phy->state == OTG_STATE_B_PERIPHERAL) ? '1' : '0';

	strcat(buf, "\n");
	return strlen(buf);
}

static ssize_t store_device_en(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct tegra_otg *tegra = platform_get_drvdata(pdev);
	unsigned long device;

	if (sscanf(buf, "%lu", &device) != 1 || device < 0 || device > 1)
		return -EINVAL;

	if (device) {
		enable_interrupt(tegra, false);
		tegra->interrupt_mode = true;
		tegra_change_otg_state(tegra, OTG_STATE_A_SUSPEND);
		tegra_change_otg_state(tegra, OTG_STATE_B_PERIPHERAL);
		tegra->interrupt_mode = false;
	} else {
		tegra->interrupt_mode = true;
		tegra_change_otg_state(tegra, OTG_STATE_A_SUSPEND);
		enable_interrupt(tegra, true);
	}

	return count;
}

static DEVICE_ATTR(enable_device, 0644, show_device_en, store_device_en);

#ifdef CONFIG_DEBUG_FS
static struct dentry *tegra_otg_debugfs_root;
static int tegra_otg_pm_set(void *data, u64 val)
{
	struct tegra_otg *tegra = (struct tegra_otg *)data;

	if (!tegra)
		return -EINVAL;

	if (val)
		pm_runtime_get_sync(tegra->phy.dev);
	else
		pm_runtime_put_sync(tegra->phy.dev);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(tegra_otg_pm_fops,
		NULL,
		tegra_otg_pm_set, "%llu\n");

static int tegra_otg_debug_init(struct tegra_otg *tegra)
{
	tegra_otg_debugfs_root = debugfs_create_dir("tegra-otg", NULL);

	if (!tegra_otg_debugfs_root)
		return -ENOMEM;

	if (!debugfs_create_file("get_rtpm", 0644, tegra_otg_debugfs_root,
				(void *)tegra, &tegra_otg_pm_fops))
		goto err_out;

	return 0;

err_out:
	debugfs_remove_recursive(tegra_otg_debugfs_root);
	return -ENOMEM;
}

static void tegra_otg_debug_exit(void)
{
	debugfs_remove_recursive(tegra_otg_debugfs_root);
}
#else
static inline int tegra_otg_debug_init(struct tegra_otg *tegra)
{
	return 0;
}

static void tegra_otg_debug_exit(void)
{
}
#endif

static int tegra_otg_set_power(struct usb_phy *phy, unsigned mA)
{
	return 0;
}

static int tegra_otg_set_suspend(struct usb_phy *phy, int suspend)
{
	return 0;
}

static void tegra_otg_set_id_detection_type(struct tegra_otg *tegra)
{
	switch (tegra->id_det_type) {
	case TEGRA_USB_ID:
		tegra->support_usb_id = true;
		break;
	case TEGRA_USB_PMU_ID:
		tegra->support_pmu_id = true;
		break;
	case TEGRA_USB_VIRTUAL_ID:
		tegra->support_usb_id = false;
		break;
	default:
		pr_info("otg usb id detection type is unknown\n");
		break;
	}
}

static struct tegra_usb_otg_data *tegra_otg_dt_parse_pdata(
	struct platform_device *pdev, struct tegra_otg_soc_data *soc_data,
	struct tegra_otg *tegra)
{
	struct tegra_usb_otg_data *pdata;
	struct device_node *np = pdev->dev.of_node;
#ifdef CONFIG_ARM64
	int usb_port_owner_info = tegra_get_usb_port_owner_info();
#endif

	if (!np)
		return NULL;

#if (defined(CONFIG_ARCH_TEGRA_21x_SOC) || defined(CONFIG_ARCH_TEGRA_12x_SOC))\
	&& !defined(CONFIG_ARCH_TEGRA_13x_SOC)
	/* get EHCI device/pdata handle */
	if (!tegra->ehci_node) {
		tegra->ehci_node = of_parse_phandle(np, "nvidia,hc-device", 0);
		if (!tegra->ehci_node) {
			dev_err(&pdev->dev, "can't find nvidia,ehci\n");
			return NULL;
		}
	}
#endif
	pdata = devm_kzalloc(&pdev->dev, sizeof(struct tegra_usb_otg_data),
			GFP_KERNEL);
	if (!pdata) {
		dev_err(&pdev->dev, "Can't allocate platform data\n");
		return ERR_PTR(-ENOMEM);
	}
	pdata->is_xhci = of_property_read_bool(np, "nvidia,enable-xhci-host");
#ifdef CONFIG_ARM64
	if (!(usb_port_owner_info & UTMI1_PORT_OWNER_XUSB))
		pdata->is_xhci = false;
#endif

	if (!tegra->ehci_node) {
		pdata->ehci_device = soc_data->ehci_device;
		pdata->ehci_pdata = soc_data->ehci_pdata;
		pdata->ehci_pdata->u_data.host.support_y_cable =
		   of_property_read_bool(np, "nvidia,enable-y-cable-detection");
		pdata->ehci_pdata->support_pmu_vbus = of_property_read_bool(np,
					"nvidia,enable-pmu-vbus-detection");
		pdata->ehci_pdata->u_data.host.turn_off_vbus_on_lp0 =
		of_property_read_bool(np, "nvidia,turn-off-vbus-in-lp0");
		of_property_read_u32(np, "nvidia,id-detection-type",
			&pdata->ehci_pdata->id_det_type);
		pdata->ehci_pdata->vbus_extcon_dev_name = NULL;
		pdata->ehci_pdata->id_extcon_dev_name = NULL;
	}

	return pdata;
}

static struct resource tegra_usb_resources[] = {
	[0] = {
		.start  = TEGRA_USB_BASE,
		.end    = TEGRA_USB_BASE + TEGRA_USB_SIZE - 1,
		.flags  = IORESOURCE_MEM,
	},
	[1] = {
		.start  = INT_USB,
		.end    = INT_USB,
		.flags  = IORESOURCE_IRQ,
	},
};

static struct platform_device tegra_ehci_device = {
	.name   = "tegra-ehci",
	.id     = 0,
	.dev    = {
		.dma_mask       = &tegra_ehci_dmamask,
		.coherent_dma_mask = DMA_BIT_MASK(64),
	},
	.resource = tegra_usb_resources,
	.num_resources = ARRAY_SIZE(tegra_usb_resources),
};

static struct tegra_usb_platform_data tegra_ehci_utmi_pdata = {
	.port_otg = true,
	.has_hostpc = true,
	.unaligned_dma_buf_supported = false,
	.phy_intf = TEGRA_USB_PHY_INTF_UTMI,
	.op_mode = TEGRA_USB_OPMODE_HOST,
	.u_data.host = {
		.hot_plug = false,
		.remote_wakeup_supported = true,
		.power_off_on_suspend = true,
	},
	.u_cfg.utmi = {
		.hssync_start_delay = 0,
		.elastic_limit = 16,
		.idle_wait_delay = 17,
		.term_range_adj = 6,
		.xcvr_setup = 15,
		.xcvr_lsfslew = 0,
		.xcvr_lsrslew = 3,
		.xcvr_setup_offset = 0,
		.xcvr_use_fuses = 1,
		.vbus_oc_map = 0x4,
		.xcvr_hsslew_lsb = 2,
	},
};

static struct tegra_otg_soc_data tegra_soc_data = {
	.ehci_device = &tegra_ehci_device,
	.ehci_pdata = &tegra_ehci_utmi_pdata,
};

static struct of_device_id tegra_otg_of_match[] = {
	{.compatible = "nvidia,tegra132-otg", .data = &tegra_soc_data, },
	{.compatible = "nvidia,tegra124-otg" },
};
MODULE_DEVICE_TABLE(of, tegra_otg_of_match);

static int tegra_otg_conf(struct platform_device *pdev)
{
	struct tegra_usb_otg_data *pdata;
	struct tegra_otg *tegra;
	struct tegra_otg_soc_data *soc_data;
	const struct of_device_id *match;
	int err = 0;

	tegra = devm_kzalloc(&pdev->dev, sizeof(struct tegra_otg), GFP_KERNEL);
	if (!tegra) {
		dev_err(&pdev->dev, "unable to allocate tegra_otg\n");
		return -ENOMEM;
	}

	if (pdev->dev.of_node) {
		match = of_match_device(of_match_ptr(tegra_otg_of_match),
				&pdev->dev);
		if (!match) {
			dev_err(&pdev->dev, "Error: No device match found\n");
			return -ENODEV;
		}

		soc_data = (struct tegra_otg_soc_data *)match->data;
		pdata = tegra_otg_dt_parse_pdata(pdev, soc_data, tegra);
		tegra->support_aca_nv_cable =
				of_property_read_bool(pdev->dev.of_node,
					"nvidia,enable-aca-nv-charger-detection");
		tegra->support_pmu_rid =
				of_property_read_bool(pdev->dev.of_node,
					"nvidia,enable-aca-rid-detection");
		tegra->support_hp_trans =
				of_property_read_bool(pdev->dev.of_node,
				"nvidia,enable-host-peripheral-transitions");
	} else {
		pdata = dev_get_platdata(&pdev->dev);
	}

	if (!pdata) {
		dev_err(&pdev->dev, "unable to get platform data\n");
		return -ENODEV;
	}

	tegra->dev = pdev->dev;
	tegra->phy.otg = devm_kzalloc(&pdev->dev, sizeof(struct usb_otg), GFP_KERNEL);
	if (!tegra->phy.otg) {
		dev_err(&pdev->dev, "unable to allocate otg\n");
		return -ENOMEM;
	}

	tegra->vbus_reg = regulator_get(&pdev->dev, "usb_vbus");
	if (IS_ERR_OR_NULL(tegra->vbus_reg)) {
		pr_err("failed to get regulator usb_vbus: %ld\n",
			PTR_ERR(tegra->vbus_reg));
		tegra->vbus_reg = NULL;
	}

	spin_lock_init(&tegra->lock);
	mutex_init(&tegra->irq_work_mutex);

	INIT_WORK(&tegra->work, irq_work);

	platform_set_drvdata(pdev, tegra);
	tegra_clone = tegra;

	tegra->interrupt_mode = true;
	tegra->suspended = true;
	tegra->y_cable_conn = false;
	tegra->pdata = pdata;

	if (!tegra->ehci_node) {
		tegra->turn_off_vbus_on_lp0 =
			pdata->ehci_pdata->u_data.host.turn_off_vbus_on_lp0;
		tegra->support_y_cable =
				pdata->ehci_pdata->u_data.host.support_y_cable;
		tegra->support_pmu_vbus = pdata->ehci_pdata->support_pmu_vbus;
		tegra->id_det_type = pdata->ehci_pdata->id_det_type;
	} else {
		tegra->turn_off_vbus_on_lp0 = of_property_read_bool(
			pdev->dev.of_node, "nvidia,turn-off-vbus-in-lp0");
		tegra->support_y_cable = of_property_read_bool(
			pdev->dev.of_node, "nvidia,enable-y-cable-detection");
		tegra->support_pmu_vbus = of_property_read_bool(
			pdev->dev.of_node, "nvidia,enable-pmu-vbus-detection");
		of_property_read_u32(pdev->dev.of_node,
			"nvidia,id-detection-type", &tegra->id_det_type);
	}
	tegra_otg_set_id_detection_type(tegra);

	tegra->phy.dev = &pdev->dev;
	tegra->phy.label = "tegra-otg";
	tegra->phy.set_suspend = tegra_otg_set_suspend;
	tegra->phy.set_power = tegra_otg_set_power;
	tegra->phy.state = OTG_STATE_A_SUSPEND;
	tegra->phy.otg->phy = &tegra->phy;
	tegra->phy.otg->set_host = tegra_otg_set_host;
	tegra->phy.otg->set_peripheral = tegra_otg_set_peripheral;

	if (tegra->support_pmu_vbus) {
		if (pdev->dev.of_node) {
			tegra->vbus_extcon_cable = extcon_get_extcon_cable(
						&pdev->dev, "vbus");
			if (IS_ERR(tegra->vbus_extcon_cable)) {
				err = -EPROBE_DEFER;
				dev_err(&pdev->dev, "Cannot get vbus extcon cable\n");
				goto err_vbus_extcon;
			}
			tegra->vbus_extcon_dev = tegra->vbus_extcon_cable->edev;
			otg_vbus_nb.notifier_call = otg_notifications;
			extcon_register_cable_interest(&tegra->vbus_extcon_obj,
					tegra->vbus_extcon_cable, &otg_vbus_nb);
		} else {
			if (!pdata->ehci_pdata->vbus_extcon_dev_name) {
				dev_err(&pdev->dev,
					"Missing vbus extcon dev name\n");
				err = -EINVAL;
				goto err_vbus_extcon;
			}
			tegra->vbus_extcon_dev = extcon_get_extcon_dev(pdata->
					ehci_pdata->vbus_extcon_dev_name);
			if (!tegra->vbus_extcon_dev) {
				err = -ENODEV;
				dev_err(&pdev->dev,
					"Cannot get vbus extcon dev\n");
				goto err_vbus_extcon;
			}
			otg_vbus_nb.notifier_call = otg_notifications;
			extcon_register_notifier(tegra->vbus_extcon_dev,
								&otg_vbus_nb);
		}
	}

	if (tegra->support_pmu_id) {
		if (pdev->dev.of_node) {
			tegra->id_extcon_cable = extcon_get_extcon_cable(
						&pdev->dev, "id");
			if (IS_ERR(tegra->id_extcon_cable)) {
				err = -EPROBE_DEFER;
				dev_err(&pdev->dev, "Cannot get id extcon cable\n");
				goto err_id_extcon;
			}
			tegra->id_extcon_dev = tegra->id_extcon_cable->edev;
			otg_id_nb.notifier_call = otg_notifications;
			extcon_register_cable_interest(&tegra->id_extcon_obj,
					tegra->id_extcon_cable, &otg_id_nb);
		} else {
			if (!pdata->ehci_pdata->id_extcon_dev_name) {
				dev_err(&pdev->dev,
					"Missing id extcon dev name\n");
				err = -EINVAL;
				goto err_id_extcon;
			}
			tegra->id_extcon_dev = extcon_get_extcon_dev(pdata->
					ehci_pdata->id_extcon_dev_name);
			if (!tegra->id_extcon_dev) {
				err = -ENODEV;
				dev_err(&pdev->dev, "Cannot get id extcon dev\n");
				goto err_id_extcon;
			}
			otg_id_nb.notifier_call = otg_notifications;
			extcon_register_notifier(tegra->id_extcon_dev, &otg_id_nb);
		}
	}

	if (tegra->support_pmu_rid && pdev->dev.of_node) {
		tegra->rid_a = devm_kzalloc(&pdev->dev,
				sizeof(struct tegra_otg_cables), GFP_KERNEL);
		tegra->rid_a->ecable = extcon_get_extcon_cable(
					&pdev->dev, "aca-ra");
		if (IS_ERR(tegra->rid_a->ecable)) {
			err = -EPROBE_DEFER;
			dev_err(&pdev->dev, "Cannot get rid-a extcon cable");
			goto err_rid_a_extcon_cable;
		}
		tegra->rid_a->edev = tegra->rid_a->ecable->edev;
		tegra->rid_a->nb.notifier_call = otg_notifications;
		extcon_register_cable_interest(&tegra->rid_a->extcon_obj,
				tegra->rid_a->ecable, &tegra->rid_a->nb);

		tegra->rid_b = devm_kzalloc(&pdev->dev,
				sizeof(struct tegra_otg_cables), GFP_KERNEL);
		tegra->rid_b->ecable = extcon_get_extcon_cable(
					&pdev->dev, "aca-rb");
		if (IS_ERR(tegra->rid_b->ecable)) {
			err = -EPROBE_DEFER;
			dev_err(&pdev->dev, "Cannot get rid-b extcon cable");
			goto err_rid_b_extcon_cable;
		}
		tegra->rid_b->edev = tegra->rid_b->ecable->edev;
		tegra->rid_b->nb.notifier_call = otg_notifications;
		extcon_register_cable_interest(&tegra->rid_b->extcon_obj,
				tegra->rid_b->ecable, &tegra->rid_b->nb);

		tegra->rid_c = devm_kzalloc(&pdev->dev,
				sizeof(struct tegra_otg_cables), GFP_KERNEL);
		tegra->rid_c->ecable = extcon_get_extcon_cable(
					&pdev->dev, "aca-rc");
		if (IS_ERR(tegra->rid_c->ecable)) {
			err = -EPROBE_DEFER;
			dev_err(&pdev->dev, "Cannot get rid-c extcon cable");
			goto err_rid_c_extcon_cable;
		}
		tegra->rid_c->edev = tegra->rid_c->ecable->edev;
		tegra->rid_c->nb.notifier_call = otg_notifications;
		extcon_register_cable_interest(&tegra->rid_c->extcon_obj,
				tegra->rid_c->ecable, &tegra->rid_c->nb);
	}

	if (tegra->support_aca_nv_cable) {
		tegra->aca_nv_extcon_cable = extcon_get_extcon_cable(&pdev->dev,
							"aca-nv");
		if (IS_ERR(tegra->aca_nv_extcon_cable)) {
			err = -EPROBE_DEFER;
			dev_err(&pdev->dev, "Cannot get aca-nv extcon cable\n");
			goto err_aca_nv_extcon_cable;
		}
		tegra->aca_nv_extcon_dev = tegra->aca_nv_extcon_cable->edev;
		tegra->otg_aca_nv_nb.notifier_call = otg_notifications;
		extcon_register_cable_interest(&tegra->aca_nv_extcon_obj,
			tegra->aca_nv_extcon_cable, &tegra->otg_aca_nv_nb);
	}

	if (tegra->support_y_cable) {
		tegra->y_extcon_cable = extcon_get_extcon_cable(&pdev->dev,
							"y-cable");
		if (IS_ERR(tegra->y_extcon_cable)) {
			dev_err(&pdev->dev, "Cannot get y cable extcon cable\n");
		} else {
			tegra->y_extcon_dev = tegra->y_extcon_cable->edev;
			tegra->otg_y_nb.notifier_call = otg_notifications;
			extcon_register_cable_interest(&tegra->y_extcon_obj,
				tegra->y_extcon_cable, &tegra->otg_y_nb);
		}
	}

	err = usb_add_phy(&tegra->phy, USB_PHY_TYPE_USB2);
	if (err) {
		dev_err(&pdev->dev, "usb_set_transceiver failed\n");
		goto err_set_trans;
	}

	return 0;
err_set_trans:
err_rid_a_extcon_cable:
err_rid_b_extcon_cable:
err_rid_c_extcon_cable:
err_aca_nv_extcon_cable:
	if (tegra->support_pmu_id && !pdev->dev.of_node)
		extcon_unregister_notifier(tegra->id_extcon_dev,
						&otg_id_nb);
err_id_extcon:
	if (tegra->support_pmu_vbus && !pdev->dev.of_node)
		extcon_unregister_notifier(tegra->vbus_extcon_dev,
						&otg_vbus_nb);
err_vbus_extcon:
	if (tegra->vbus_reg)
		regulator_put(tegra->vbus_reg);
	return err;
}

static int tegra_otg_start(struct platform_device *pdev)
{
	struct usb_phy *otg_trans = usb_get_phy(USB_PHY_TYPE_USB2);
	struct tegra_otg *tegra;
	struct resource *res;
	int err;

	tegra = container_of(otg_trans, struct tegra_otg, phy);
	tegra->clk = clk_get(&pdev->dev, NULL);
	if (IS_ERR(tegra->clk)) {
		dev_err(&pdev->dev, "Failed to get otg clock\n");
		err = PTR_ERR(tegra->clk);
		goto err_clk;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "Failed to get I/O memory\n");
		err = -ENXIO;
		goto err_io;
	}

	tegra->regs = ioremap(res->start, resource_size(res));
	if (!tegra->regs) {
		err = -ENOMEM;
		goto err_io;
	}

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		dev_err(&pdev->dev, "Failed to get IRQ\n");
		err = -ENXIO;
		goto err_irq;
	}

	tegra->irq = res->start;
	err = devm_request_threaded_irq(&pdev->dev, tegra->irq, tegra_otg_irq,
				   NULL,
				   IRQF_SHARED | IRQF_TRIGGER_HIGH,
				   "tegra-otg", tegra);
	if (err) {
		dev_err(&pdev->dev, "Failed to register IRQ\n");
		goto err_irq;
	}

	if (tegra->support_y_cable) {
		tegra->vbus_bat_reg = regulator_get(&pdev->dev, "usb_bat_chg");
		if (IS_ERR_OR_NULL(tegra->vbus_bat_reg)) {
			pr_err("failed to get regulator usb_bat_chg: %ld\n",
					PTR_ERR(tegra->vbus_bat_reg));
			tegra->vbus_bat_reg = NULL;
		}

		tegra->edev = kzalloc(sizeof(struct extcon_dev), GFP_KERNEL);
		if (!tegra->edev) {
			dev_err(&pdev->dev, "failed to allocate memory for extcon\n");
			err = -ENOMEM;
			goto err_irq;
		}
		tegra->edev->name = "tegra-otg";
		tegra->edev->supported_cable =
				(const char **) tegra_otg_extcon_cable;
		tegra->edev->dev.parent = &pdev->dev;
		err = extcon_dev_register(tegra->edev);
		if (err) {
			dev_err(&pdev->dev, "failed to register extcon device\n");
			kfree(tegra->edev);
			tegra->edev = NULL;
		}
	}

	return 0;
err_irq:
	iounmap(tegra->regs);
err_io:
	clk_put(tegra->clk);
err_clk:
	if (tegra->vbus_reg)
		regulator_put(tegra->vbus_reg);
	if (tegra->support_y_cable && tegra->vbus_bat_reg)
		regulator_put(tegra->vbus_bat_reg);

	return err;
}

static const struct of_device_id otg_tegra_device_match[] = {
	{.compatible = "nvidia,tegra132-otg" },
	{},
};

static int tegra_otg_probe(struct platform_device *pdev)
{
	struct tegra_otg *tegra;
	int err = 0;
	int ret;

	err = tegra_otg_conf(pdev);
	if (err) {
		dev_err(&pdev->dev, "otg configuration failed\n");
		goto err;
	}

	err = tegra_otg_start(pdev);
	if (err) {
		dev_err(&pdev->dev, "otg start failed\n");
		goto err;
	}

	tegra = tegra_clone;
	if (!tegra->support_usb_id && !tegra->support_pmu_id) {
		err = device_create_file(&pdev->dev, &dev_attr_enable_host);
		if (err) {
			dev_warn(&pdev->dev,
				"Can't register host-sysfs attribute\n");
			goto err;
		}
		err = device_create_file(&pdev->dev, &dev_attr_enable_device);
		if (err) {
			dev_warn(&pdev->dev,
				"Can't register device-sysfs attribute\n");
			device_remove_file(&pdev->dev, &dev_attr_enable_host);
			goto err;
		}
	}

	ret = genpd_dev_pm_add(otg_tegra_device_match, &pdev->dev);
	if (ret)
		pr_err("Could not add %s to power domain using device tree\n",
					  dev_name(&pdev->dev));

	tegra_pd_add_device(tegra->phy.dev);
	pm_runtime_use_autosuspend(tegra->phy.dev);
	pm_runtime_set_autosuspend_delay(tegra->phy.dev, 100);
	pm_runtime_enable(tegra->phy.dev);

	tegra_otg_debug_init(tegra);

	dev_info(&pdev->dev, "otg transceiver registered\n");
err:
	return err;
}

static int __exit tegra_otg_remove(struct platform_device *pdev)
{
	struct tegra_otg *tegra = platform_get_drvdata(pdev);

	if (tegra->vbus_reg)
		regulator_put(tegra->vbus_reg);

	if (tegra->support_y_cable && tegra->vbus_bat_reg)
		regulator_put(tegra->vbus_bat_reg);

	if (tegra->support_pmu_id && !pdev->dev.of_node)
		extcon_unregister_notifier(tegra->id_extcon_dev, &otg_id_nb);

	if (tegra->support_pmu_vbus && !pdev->dev.of_node)
		extcon_unregister_notifier(tegra->vbus_extcon_dev,
							&otg_vbus_nb);

	if (tegra->edev != NULL) {
		extcon_dev_unregister(tegra->edev);
		kfree(tegra->edev);
	}

	tegra_otg_debug_exit();
	pm_runtime_disable(tegra->phy.dev);
	usb_remove_phy(&tegra->phy);
	iounmap(tegra->regs);
	clk_put(tegra->clk);
	platform_set_drvdata(pdev, NULL);
	mutex_destroy(&tegra->irq_work_mutex);

	return 0;
}

#ifdef CONFIG_PM
static int tegra_otg_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct tegra_otg *tegra = platform_get_drvdata(pdev);
	enum usb_otg_state from = tegra->phy.state;
	unsigned int val;
	int err = 0;

	flush_work(&tegra->work);

	mutex_lock(&tegra->irq_work_mutex);
	DBG("%s(%d) BEGIN state : %s\n", __func__, __LINE__,
				tegra_state_name(tegra->phy.state));

	pm_runtime_get_sync(dev);
	clk_prepare_enable(tegra->clk);
	val = otg_readl(tegra, USB_PHY_WAKEUP);
	val &= ~(USB_ID_INT_EN | USB_VBUS_INT_EN);
	otg_writel(tegra, val, USB_PHY_WAKEUP);
	clk_disable_unprepare(tegra->clk);
	pm_runtime_put_sync(dev);

	/* suspend peripheral mode, host mode is taken care by host driver */
	if (from == OTG_STATE_B_PERIPHERAL)
		tegra_change_otg_state(tegra, OTG_STATE_A_SUSPEND);

	if (from == OTG_STATE_A_HOST && tegra->turn_off_vbus_on_lp0)
		tegra_otg_vbus_enable(tegra, 0);

	if (tegra->irq) {
		err = enable_irq_wake(tegra->irq);
		if (err < 0) {
			dev_err(&pdev->dev,
			"Couldn't enable USB otg mode wakeup,"
			"irq=%d, error=%d\n", tegra->irq, err);
			goto fail;
		}
	}

fail:
	tegra->suspended = true;
	DBG("%s(%d) END\n", __func__, __LINE__);
	mutex_unlock(&tegra->irq_work_mutex);
	return err;
}

static void tegra_otg_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct tegra_otg *tegra = platform_get_drvdata(pdev);
	int val;
	unsigned long flags;
	int err = 0;

	DBG("%s(%d) BEGIN\n", __func__, __LINE__);

	mutex_lock(&tegra->irq_work_mutex);
	if (!tegra->suspended) {
		mutex_unlock(&tegra->irq_work_mutex);
		return ;
	}
	if (tegra->irq) {
		err = disable_irq_wake(tegra->irq);
		if (err < 0)
			dev_err(&pdev->dev,
			"Couldn't disable USB otg mode wakeup,"
			"irq=%d, error=%d\n", tegra->irq, err);
	}

	/* Detect cable status after LP0 for all detection types */

	if (tegra->support_usb_id || !tegra->support_pmu_vbus) {
		/* Clear pending interrupts  */
		pm_runtime_get_sync(dev);
		clk_prepare_enable(tegra->clk);
		val = otg_readl(tegra, USB_PHY_WAKEUP);
		otg_writel(tegra, val, USB_PHY_WAKEUP);
		DBG("%s(%d) PHY WAKEUP : 0x%x\n", __func__, __LINE__, val);
		clk_disable_unprepare(tegra->clk);
		pm_runtime_mark_last_busy(dev);
		pm_runtime_put_autosuspend(dev);

		spin_lock_irqsave(&tegra->lock, flags);
		if (tegra->support_usb_id)
			val |= USB_ID_INT_EN | USB_ID_PIN_WAKEUP_EN;
		if (!tegra->support_pmu_vbus)
			val |= USB_VBUS_INT_EN | USB_VBUS_WAKEUP_EN;
		tegra->int_status = val;
		spin_unlock_irqrestore(&tegra->lock, flags);
	}

	if (tegra->support_pmu_vbus || tegra->support_pmu_id
				|| tegra->support_aca_nv_cable)
		otg_notifications(NULL, 0, NULL);

	if (tegra->turn_off_vbus_on_lp0 &&
		!(tegra->int_status & USB_ID_STATUS)) {
		/* Handle Y-cable */
		if (tegra->support_y_cable &&
				(tegra->int_status & USB_VBUS_STATUS)) {
			tegra_otg_set_current(tegra->vbus_bat_reg,
				YCABLE_CHARGING_CURRENT_UA);
			tegra->y_cable_conn = true;
			tegra_otg_set_extcon_state(tegra);
		} else {
			tegra_otg_vbus_enable(tegra, 1);
		}
	}

	/* Hold wakelock for display wake up */
	if (tegra->int_status & USB_VBUS_STATUS)
		tegra_otg_notify_event(tegra, USB_EVENT_VBUS);

	/* Call work to set appropriate state */
	schedule_work(&tegra->work);
	enable_interrupt(tegra, true);

	tegra->suspended = false;

	DBG("%s(%d) END\n", __func__, __LINE__);
	mutex_unlock(&tegra->irq_work_mutex);
}

static const struct dev_pm_ops tegra_otg_pm_ops = {
	.complete = tegra_otg_resume,
	.suspend = tegra_otg_suspend,
};
#endif

static struct platform_driver tegra_otg_driver = {
	.driver = {
		.name  = "tegra-otg",
#ifdef CONFIG_PM
		.pm    = &tegra_otg_pm_ops,
#endif
		.of_match_table = of_match_ptr(tegra_otg_of_match),
	},
	.remove  = __exit_p(tegra_otg_remove),
	.probe   = tegra_otg_probe,
};

static int __init tegra_otg_init(void)
{
	otg_work_wl = wakeup_source_register("otg_work_wakelock");

	return platform_driver_register(&tegra_otg_driver);
}
fs_initcall(tegra_otg_init);

static void __exit tegra_otg_exit(void)
{
	platform_driver_unregister(&tegra_otg_driver);
}
module_exit(tegra_otg_exit);
