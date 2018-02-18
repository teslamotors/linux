/*
* tegra-xotg.c - Nvidia XOTG implementation
*
* Copyright (c) 2013-2015, NVIDIA CORPORATION.  All rights reserved.
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
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/usb.h>
#include <linux/device.h>
#include <linux/usb/gadget.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/extcon.h>
#include <asm/unaligned.h>
#include <mach/tegra_usb_pad_ctrl.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include "linux/usb/hcd.h"
#include "linux/usb/otg.h"
#include "tegra-xotg.h"

#define MAX_USB_PORTS 8

int xotg_debug_level = LEVEL_INFO;
module_param(xotg_debug_level, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(xotg_debug_level, "level 0~4");

static bool session_supported;
module_param(session_supported, bool, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(session_supported, "session supported");

static const char driver_name[] = "tegra-xotg";
static void xotg_work(struct work_struct *work);

static void xotg_print_status(struct xotg *xotg)
{
	struct xotg_app_request *xreqs = &xotg->xotg_reqs;
	struct xotg_vars *xvars = &xotg->xotg_vars;

	xotg_info(xotg->dev, "current OTG state is %s\n",
	usb_otg_state_string(xotg->phy.state));

	xotg_info(xotg->dev, "id=%d\n", xotg->id);
	xotg_info(xotg->dev, "default_a=%d\n", xotg->phy.otg->default_a);
	xotg_info(xotg->dev, "xotg_app_req a_bus_req=%d\n", xreqs->a_bus_req);
	xotg_info(xotg->dev, "xotg_app_req b_bus_req=%d\n", xreqs->b_bus_req);
	xotg_info(xotg->dev, "xotg_app_req a_bus_drop=%d\n", xreqs->a_bus_drop);
	xotg_info(xotg->dev, "xotg_app_req b_srp_init=%d\n", xreqs->b_srp_init);

	xotg_info(xotg->dev, "xotg_vars: a_bus_suspend=%d\n",
			xvars->a_bus_suspend);
	xotg_info(xotg->dev, "xotg_vars: a_bus_resume=%d\n",
			xvars->a_bus_resume);
	xotg_info(xotg->dev, "xotg_vars: a_conn=%d\n", xvars->a_conn);
	xotg_info(xotg->dev, "xotg_vars: b_se0_srp=%d\n", xvars->b_se0_srp);
	xotg_info(xotg->dev, "xotg_vars: b_ssend_srp=%d\n", xvars->b_ssend_srp);
	xotg_info(xotg->dev, "xotg_vars: b_sess_vld=%d\n", xvars->b_sess_vld);
	xotg_info(xotg->dev, "xotg_vars: power_up=%d\n", xvars->power_up);
	xotg_info(xotg->dev, "xotg_vars: b_srp_done=%d\n", xvars->b_srp_done);
	xotg_info(xotg->dev, "xotg_vars: b_hnp_en=%d\n", xvars->b_hnp_en);

	xotg_info(xotg->dev, "xotg_vars: a_sess_vld=%d\n", xvars->a_sess_vld);
	xotg_info(xotg->dev, "xotg_vars: a_srp_det=%d\n", xvars->a_srp_det);
	xotg_info(xotg->dev, "xotg_vars: a_vbus_vld=%d\n", xvars->a_vbus_vld);
	xotg_info(xotg->dev, "xotg_vars: b_conn=%d\n", xvars->b_conn);
	xotg_info(xotg->dev, "xotg_vars: a_set_b_hnp_en=%d\n",
			xvars->a_set_b_hnp_en);
	xotg_info(xotg->dev, "xotg_vars: b_srp_initiated=%d\n",
			xvars->b_srp_initiated);
	xotg_info(xotg->dev, "xotg_vars: b_bus_resume=%d\n",
			xvars->b_bus_resume);
	xotg_info(xotg->dev, "xotg_vars: b_bus_suspend=%d\n",
			xvars->b_bus_suspend);

	xotg_info(xotg->dev, "a_wait_vrise_tmout = %d\n",
			xotg->xotg_timer_list.a_wait_vrise_tmout);
	xotg_info(xotg->dev, "a_wait_vfall_tmout = %d\n",
			xotg->xotg_timer_list.a_wait_vfall_tmout);
	xotg_info(xotg->dev, "a_wait_bcon_tmout = %d\n",
			xotg->xotg_timer_list.a_wait_bcon_tmout);
	xotg_info(xotg->dev, "a_aidl_bdis_tmout = %d\n",
			xotg->xotg_timer_list.a_aidl_bdis_tmout);
	xotg_info(xotg->dev, "a_bidl_adis_tmout = %d\n",
			xotg->xotg_timer_list.a_bidl_adis_tmout);
	xotg_info(xotg->dev, "b_ase0_brst_tmout = %d\n",
			xotg->xotg_timer_list.b_ase0_brst_tmout);
	xotg_info(xotg->dev, "b_ssend_srp_tmout = %d\n",
			xotg->xotg_timer_list.b_ssend_srp_tmout);
	xotg_info(xotg->dev, "b_srp_response_wait_tmout = %d\n",
			xotg->xotg_timer_list.b_srp_response_wait_tmout);
	xotg_info(xotg->dev, "b_srp_done_tmout = %d\n",
			xotg->xotg_timer_list.b_srp_done_tmout);

	if (xotg->phy.otg && xotg->phy.otg->gadget) {
		xotg_info(xotg->dev, "gadget: rcvd_otg_hnp_reqd=%d\n",
			xotg->phy.otg->gadget->rcvd_otg_hnp_reqd);
		xotg_info(xotg->dev, "gadget: b_hnp_enable=%d\n",
			xotg->phy.otg->gadget->b_hnp_enable);
	}
	if (xotg->phy.otg && xotg->phy.otg->host) {
		xotg_info(xotg->dev, "host: is_b_host=%d\n",
			xotg->phy.otg->host->is_b_host);
		xotg_info(xotg->dev, "host: b_hnp_enable=%d\n",
			xotg->phy.otg->host->b_hnp_enable);
	}
}

static void xotg_roothub_resume(struct xotg *xotg)
{
	struct usb_phy *phy = &xotg->phy;
	struct usb_otg *otg = phy->otg;
	struct usb_hcd *shared_hcd, *main_hcd;

	if (otg->xhcihost) {
		shared_hcd = bus_to_hcd(otg->xhcihost);
		/* let host go in elpg */
		usb_hcd_resume_root_hub(shared_hcd);
	}
	if (otg->host) {
		main_hcd = bus_to_hcd(otg->host);
		/* let host go in elpg */
		usb_hcd_resume_root_hub(main_hcd);
	}
}
static void xotg_notify_event(struct xotg *xotg, int event)
{
	spin_lock(&xotg->phy.sync_lock);

	xotg->phy.last_event = event;
	atomic_notifier_call_chain(&xotg->phy.notifier, event, NULL);

	spin_unlock(&xotg->phy.sync_lock);
}

static ssize_t debug_store(struct device *_dev, struct device_attribute *attr,
	const char *buf, size_t size)
{
	struct xotg *xotg = dev_get_drvdata(_dev);

	if (sysfs_streq(buf, "uptimeout")) {
		xotg->test_timer_timeout += 30000;
		xotg_info(xotg->dev, "test_timer_timeout = %d sec\n",
			xotg->test_timer_timeout / 1000);
	}

	if (sysfs_streq(buf, "dntimeout")) {
		xotg->test_timer_timeout -= 30000;
		xotg_info(xotg->dev, "test_timer_timeout = %d sec\n",
			xotg->test_timer_timeout / 1000);
	}

	if (sysfs_streq(buf, "settimer")) {
		xotg_info(xotg->dev, "test timer set for %d sec\n",
			xotg->test_timer_timeout / 1000);
		mod_timer(&xotg->xotg_timer_list.test_tmr,
			jiffies + msecs_to_jiffies(xotg->test_timer_timeout));
	}

	if (sysfs_streq(buf, "status"))
		xotg_print_status(xotg);

	if (sysfs_streq(buf, "enable_host")) {
		xotg_info(xotg->dev, "AppleOTG: setting up host mode\n");
		xotg->id_grounded = true;
		tegra_usb_pad_reg_update(XUSB_PADCTL_USB2_VBUS_ID_0,
			USB2_VBUS_ID_0_VBUS_OVERRIDE, 0);
		tegra_usb_pad_reg_update(XUSB_PADCTL_USB2_VBUS_ID_0,
			USB2_VBUS_ID_0_ID_OVERRIDE,
			USB2_VBUS_ID_0_ID_OVERRIDE_RID_GND);
		/* pad protection for host mode */
		xusb_enable_pad_protection(0);
	}
	if (sysfs_streq(buf, "enable_device")) {
		xotg_info(xotg->dev, "AppleOTG: setting up device mode\n");
		xotg->id_grounded = false;
		tegra_usb_pad_reg_update(XUSB_PADCTL_USB2_VBUS_ID_0,
			USB2_VBUS_ID_0_ID_OVERRIDE,
			USB2_VBUS_ID_0_ID_OVERRIDE_RID_FLOAT);
		tegra_usb_pad_reg_update(XUSB_PADCTL_USB2_VBUS_ID_0,
			USB2_VBUS_ID_0_VBUS_OVERRIDE,
			USB2_VBUS_ID_0_VBUS_OVERRIDE);
		/* pad protection for device mode */
		xusb_enable_pad_protection(1);
	}
	if (sysfs_streq(buf, "hnp"))
		xotg->phy.otg->gadget->request_hnp = 1;

	return size;
}
static DEVICE_ATTR(debug, S_IWUSR, NULL, debug_store);

static int extcon_id_notifications(struct notifier_block *nb,
				   unsigned long event, void *unused)
{
	struct xotg *xotg =
			container_of(nb, struct xotg, id_extcon_nb);
	unsigned long flags;

	spin_lock_irqsave(&xotg->lock, flags);

	if (extcon_get_cable_state(xotg->id_extcon_dev, "USB-Host")) {
		if (xotg->vbus_on_jiffies == 0)
			xotg->vbus_on_jiffies = jiffies;
		xotg_info(xotg->dev, "USB_ID pin grounded\n");
		xotg->id_grounded = true;
	} else {
		xotg_info(xotg->dev, "USB_ID pin floating\n");
		xotg->id_grounded = false;
	}

	if (xotg->id_grounded)
		xotg_notify_event(xotg, USB_EVENT_ID);
	else
		xotg_notify_event(xotg, USB_EVENT_ID_FLOAT);

	spin_unlock_irqrestore(&xotg->lock, flags);
	return NOTIFY_DONE;
}

/* allocate a timer_list structure and populate the values */
static int xotg_alloc_timer(struct xotg *xotg,
	struct timer_list *temp_timer_list,
	void (*xotg_timer_comp)(unsigned long), unsigned long expires,
	unsigned long data)
{
	xotg_dbg(xotg->dev, "expires = %lx, msecs_to_jiffies=%lx\n",
		expires, msecs_to_jiffies(expires));

	init_timer(temp_timer_list);

	temp_timer_list->function = xotg_timer_comp;
	temp_timer_list->expires = msecs_to_jiffies(expires);
	temp_timer_list->data = data;

	return 0;
}

static void xotg_test_timer_comp(unsigned long data)
{
	struct xotg *xotg = (struct xotg *)data;

	xotg_info(xotg->dev, "test timer expired after %d sec\n",
		xotg->test_timer_timeout/1000);
}

/*
 * SRP wait timer: Time within which the B-device needs to get a
 * response from the A-device after B has initiated the SRP
 */
static void b_srp_response_wait_timer(unsigned long data)
{
	struct xotg *xotg = (struct xotg *)data;

	xotg_dbg(xotg->dev, "b_srp_response_wait timer expired\n");
	/* just set the timeout to indicate we got no response
	 * from A-device
	 */
	xotg->xotg_timer_list.b_srp_response_wait_tmout = 1;

	queue_work(xotg->otg_wq, &xotg->otg_work);
}

static void xotg_timer_comp(unsigned long timeout)
{
	struct usb_phy *phy = usb_get_phy(USB_PHY_TYPE_USB3);
	struct xotg *xotg = container_of(phy, struct xotg, phy);

	xotg_dbg(xotg->dev, "timer expired\n");
	*(int *)timeout = 1;
	queue_work(xotg->otg_wq, &xotg->otg_work);
}

static void b_srp_done_timer(unsigned long data)
{
	struct xotg *xotg = (struct xotg *)data;
	struct usb_phy *phy = &xotg->phy;

	xotg_dbg(xotg->dev, "b_srp_done timer expired\n");
	if (phy->state == OTG_STATE_B_SRP_INIT) {
		xotg->xotg_vars.b_srp_done = 1;
		xotg->xotg_vars.b_srp_initiated = 0;
	}
	queue_work(xotg->otg_wq, &xotg->otg_work);
}

static int xotg_init_timers(struct xotg *xotg)
{
	int status = 0;

	/* vrise timer */
	status = xotg_alloc_timer(xotg, &xotg->xotg_timer_list.a_wait_vrise_tmr,
			&xotg_timer_comp, TA_VBUS_RISE,
			(unsigned long)&xotg->xotg_timer_list.
			a_wait_vrise_tmout);
	if (status) {
		xotg_err(xotg->dev, "error in init a_wait_vrise_tmr list\n");
		return status;
	}

	/* vfall timer */
	status = xotg_alloc_timer(xotg, &xotg->xotg_timer_list.a_wait_vfall_tmr,
			&xotg_timer_comp, TA_WAIT_VFALL,
			(unsigned long)&xotg->xotg_timer_list.
			a_wait_vfall_tmout);
	if (status) {
		xotg_err(xotg->dev, "error in init list a_wait_vfall_tmr\n");
		return status;
	}

	/* bcon timer */
	status = xotg_alloc_timer(xotg, &xotg->xotg_timer_list.a_wait_bcon_tmr,
			&xotg_timer_comp, TA_WAIT_BCON,
			(unsigned long)&xotg->xotg_timer_list.
			a_wait_bcon_tmout);
	if (status) {
		xotg_err(xotg->dev, "error in init list a_wait_bcon_tmr\n");
		return status;
	}

	/* tst_maint timer */
	status = xotg_alloc_timer(xotg, &xotg->xotg_timer_list.a_tst_maint_tmr,
			&xotg_timer_comp, TA_TST_MAINT,
			(unsigned long)&xotg->xotg_timer_list.
			a_tst_maint_tmout);
	if (status) {
		xotg_err(xotg->dev, "error in init list a_tst_maint_tmr\n");
		return status;
	}

	/* bdis_timer */
	status = xotg_alloc_timer(xotg, &xotg->xotg_timer_list.a_aidl_bdis_tmr,
			&xotg_timer_comp, TA_AIDL_BDIS,
			(unsigned long)
			&xotg->xotg_timer_list.a_aidl_bdis_tmout);
	if (status) {
		xotg_err(xotg->dev, "error in init list a_aidl_bdis_tmr\n");
		return status;
	}

	/* adis_timer */
	status = xotg_alloc_timer(xotg, &xotg->xotg_timer_list.a_bidl_adis_tmr,
			&xotg_timer_comp, TA_BIDL_ADIS,
			(unsigned long)&xotg->xotg_timer_list.
			a_bidl_adis_tmout);
	if (status) {
		xotg_err(xotg->dev, "error in init list a_bidl_adis_tmr\n");
		return status;
	}

	/* ase0_timer */
	status = xotg_alloc_timer(xotg, &xotg->xotg_timer_list.b_ase0_brst_tmr,
			&xotg_timer_comp, TB_ASE0_BRST,
			(unsigned long)&xotg->xotg_timer_list.
			b_ase0_brst_tmout);
	if (status) {
		xotg_err(xotg->dev, "error in init list b_ase0_brst_tmr\n");
		return status;
	}

	/* b_ssend_srp timer */
	status = xotg_alloc_timer(xotg, &xotg->xotg_timer_list.b_ssend_srp_tmr,
			&xotg_timer_comp, TB_SSEND_SRP,
			(unsigned long)&xotg->xotg_timer_list.
			b_ssend_srp_tmout);
	if (status) {
		xotg_err(xotg->dev, "error in init list b_ssend_srp\n");
		return status;
	}

	/* b_srp_response_wait_tmr */
	status = xotg_alloc_timer(xotg, &xotg->xotg_timer_list.
			b_srp_response_wait_tmr,
			&b_srp_response_wait_timer,
			TB_SRP_FAIL,
			(unsigned long)xotg);
	if (status) {
		xotg_err(xotg->dev,
			"error in init list b_srp_response_wait_timer\n");
		return status;
	}

	/* b_srp_done_tmr */
	status = xotg_alloc_timer(xotg, &xotg->xotg_timer_list.b_srp_done_tmr,
			&b_srp_done_timer,
			TB_SRP_DONE,
			(unsigned long)xotg);
	if (status) {
		xotg_err(xotg->dev, "error in init list b_srp_done_timer\n");
		return status;
	}

	/* test_tmr */
	status = xotg_alloc_timer(xotg, &xotg->xotg_timer_list.test_tmr,
			&xotg_test_timer_comp,
			1000,
			(unsigned long)xotg);
	if (status) {
		xotg_err(xotg->dev, "error initializing list test_timer\n");
		return status;
	}
	return status;
}

/* function to de-activate timers */
static void xotg_deinit_timers(struct xotg *xotg)
{
	del_timer_sync(&xotg->xotg_timer_list.a_wait_vrise_tmr);
	del_timer_sync(&xotg->xotg_timer_list.a_wait_vfall_tmr);
	del_timer_sync(&xotg->xotg_timer_list.a_wait_bcon_tmr);
	del_timer_sync(&xotg->xotg_timer_list.a_aidl_bdis_tmr);
	del_timer_sync(&xotg->xotg_timer_list.a_bidl_adis_tmr);
	del_timer_sync(&xotg->xotg_timer_list.b_ase0_brst_tmr);
	del_timer_sync(&xotg->xotg_timer_list.b_ssend_srp_tmr);
	del_timer_sync(&xotg->xotg_timer_list.b_srp_response_wait_tmr);
	del_timer_sync(&xotg->xotg_timer_list.b_srp_done_tmr);
	del_timer_sync(&xotg->xotg_timer_list.test_tmr);
}

/* this assigns the otg port role reverse of what the ID value suggests */
static void xotg_set_reverse_id(struct xotg *xotg, bool set)
{
	u8 port = xotg->hs_otg_port;
	u32 reg = tegra_usb_pad_reg_read(XUSB_PADCTL_USB2_PORT_CAP_0);

	xotg_dbg(xotg->dev, "port=%d, set=%d reg=0x%x\n", port, set, reg);

	if (set)
		reg |= USB2_PORT_CAP_REVERSE_ID(port);
	else
		reg &= ~USB2_PORT_CAP_REVERSE_ID(port);
	tegra_usb_pad_reg_write(XUSB_PADCTL_USB2_PORT_CAP_0, reg);

	return;
}

static int xotg_enable_vbus(struct xotg *xotg)
{
	int ret = 0;
	unsigned long flags;

	if (!IS_ERR_OR_NULL(xotg->usb_vbus_reg)) {
		xotg_dbg(xotg->dev, "enabling vbus\n");
		if (!xotg->vbus_enabled) {
			ret = regulator_enable(xotg->usb_vbus_reg);
			if (ret < 0) {
				xotg_err(xotg->dev,
				"vbus0 enable failed. ret=%d\n", ret);
			} else {
				spin_lock_irqsave(&xotg->vbus_lock, flags);
				xotg->vbus_enabled = true;
				xotg->vbus_en_started = false;
				if (xotg->vbus_on_jiffies) {
					xotg_info(xotg->dev,
					"id gnd to vbus on :%d ms",
					jiffies_to_msecs(jiffies -
						xotg->vbus_on_jiffies));
					xotg->vbus_on_jiffies = 0;
				}
				spin_unlock_irqrestore(&xotg->vbus_lock, flags);
			}
		}
	} else {
		ret = -ENODEV;
	}

	return ret;
}

static int xotg_disable_vbus(struct xotg *xotg)
{
	int ret = 0;
	unsigned long flags;

	if (!IS_ERR_OR_NULL(xotg->usb_vbus_reg)) {
		xotg_dbg(xotg->dev, "disabling vbus\n");
		if (xotg->vbus_enabled) {
			ret = regulator_disable(xotg->usb_vbus_reg);
			if (ret < 0) {
				xotg_err(xotg->dev,
				"vbus0 disable failed. ret=%d\n", ret);
			} else {
				spin_lock_irqsave(&xotg->vbus_lock, flags);
				xotg->vbus_enabled = false;
				xotg->vbus_dis_started = false;
				spin_unlock_irqrestore(&xotg->vbus_lock, flags);
			}
		}
	} else {
		ret = -ENODEV;
	}

	return ret;
}

static void nv_vbus_work(struct work_struct *work)
{
	unsigned long flags;
	struct xotg *xotg = container_of(work, struct xotg, vbus_work);

	spin_lock_irqsave(&xotg->vbus_lock, flags);
	if (xotg->vbus_on) {
		spin_unlock_irqrestore(&xotg->vbus_lock, flags);
		xotg_enable_vbus(xotg);
	} else {
		spin_unlock_irqrestore(&xotg->vbus_lock, flags);
		xotg_disable_vbus(xotg);
	}
}

static void xotg_drive_vbus(struct xotg *xotg, bool start)
{
	unsigned long flags;
	xotg_dbg(xotg->dev, "start=%d vbus_enabled=%d\n",
		start, xotg->vbus_enabled);

	spin_lock_irqsave(&xotg->vbus_lock, flags);
	if (start == xotg->vbus_enabled && !xotg->vbus_en_started
			&& !xotg->vbus_dis_started) {
		spin_unlock_irqrestore(&xotg->vbus_lock, flags);
		xotg_warn(xotg->dev, "vbus already %s\n", start ? "on" : "off");
		return;
	}
	if (start)
		xotg->vbus_en_started = true;
	else
		xotg->vbus_dis_started = true;

	xotg->vbus_on = start;
	spin_unlock_irqrestore(&xotg->vbus_lock, flags);

	queue_work(xotg->otg_wq, &xotg->vbus_work);
}

static void xotg_enable_srp_detect(struct xotg *xotg, bool enable)
{
	u8 port = xotg->hs_otg_port;
	u32 reg = tegra_usb_pad_reg_read(
		XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD_CTL0_0(port));

	xotg_dbg(xotg->dev, "port=%d, en=%d, reg=%x\n", port, enable, reg);

	if (enable) {
		reg |= USB2_BATTERY_CHRG_OTGPAD_SRP_DETECT_EN |
				USB2_BATTERY_CHRG_OTGPAD_SRP_INTR_EN;
	} else {
		reg &= ~(USB2_BATTERY_CHRG_OTGPAD_SRP_DETECT_EN |
				USB2_BATTERY_CHRG_OTGPAD_SRP_INTR_EN);
	}
	tegra_usb_pad_reg_write(
		XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD_CTL0_0(port), reg);
}

static int xotg_generate_srp(struct xotg *xotg)
{
	u8 port = xotg->hs_otg_port;
	u32 reg = tegra_usb_pad_reg_read(
		XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD_CTL0_0(port));

	xotg_dbg(xotg->dev, "port=%d\n", port);

	reg |= USB2_BATTERY_CHRG_OTGPAD_GENERATE_SRP;
	tegra_usb_pad_reg_write(
		XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD_CTL0_0(port), reg);

	return 0;
}

static const pm_message_t xotg_suspend_state = {
	/* defined in include/linux/pm.h
	 * PM_EVENT_ON = 0x0 , no change
	 * PM_EVENT_FREEZE = 0x1 , system going for hibernate
	 * PM_EVENT_SUSPEND = 0x2
	 * PM_EVENT_RESUME = 0x10
	 */
	.event = 0x1,
};

static int xotg_enable_gadget(struct xotg *xotg, bool start)
{
	struct usb_phy *phy = &xotg->phy;
	struct usb_otg *otg = phy->otg;
	struct device *dev;

	if (!otg->gadget || !otg->gadget->dev.parent) {
		xotg_err(xotg->dev, "otg->gadget or gadget->dev.parent null\n");
		return 1;
	}

	xotg_dbg(xotg->dev, "gadget %s\n", start ? "on" : "off");

	dev = otg->gadget->dev.parent;

	if (start && dev->driver->resume) {
		xotg_dbg(xotg->dev, "driver->resume\n");
		dev->driver->resume(dev);
	} else if (!start && dev->driver->suspend) {
		xotg_dbg(xotg->dev, "driver->suspend\n");
		dev->driver->suspend(dev, xotg_suspend_state);
	} else {
		xotg_warn(xotg->dev,
			"driver->%s not defined but %s vbus override\n",
			start ? "resume" : "suspend",
			start ? "setting" : "clearing");

		return 1;
	}

	return 0;
}

static bool xotg_suspend_host(struct xotg *xotg, int port_state)
{
	struct usb_phy *phy = &xotg->phy;
	struct usb_otg *otg = phy->otg;
	struct usb_hcd *hcd;

	if (!otg->host) {
		xotg_err(xotg->dev, "host driver not loaded yet\n");
		return 1;
	}
	hcd = bus_to_hcd(otg->host);

	xotg_dbg(xotg->dev, "setting port to port_state = %d\n", port_state);

	/* issue roothub command to set the linkstate */
	hcd->driver->hub_control(hcd, SetPortFeature, USB_PORT_FEAT_LINK_STATE,
			(port_state << 3) | (xotg->hs_otg_port + 1), NULL, 0);

	if (port_state == XDEV_DISABLED && otg->xhcihost) {
		struct usb_hcd *shared_hcd = bus_to_hcd(otg->xhcihost);
		shared_hcd->driver->hub_control(shared_hcd, SetPortFeature,
			USB_PORT_FEAT_LINK_STATE, (port_state << 3) |
			(xotg->ss_otg_port + 1)/* wIndex=1 SS P0 */, NULL, 0);
	}
	return 0;
}

/* will be called from the HCD during its initialization */
static int xotg_set_host(struct usb_otg *otg, struct usb_bus *host)
{
	struct xotg *xotg;

	if (!otg)
		return -ENODEV;

	if (host) {
		xotg = container_of(otg->phy, struct xotg, phy);
		xotg_info(xotg->dev, "host = 0x%p\n", host);

		otg->host = host;
		/* set the hs_otg_port here for the usbcore/HCD to be able
		 * to access it by setting it in the usb_bus structure
		 */
		otg->host->otg_port = xotg->hs_otg_port + 1;

		xotg->xotg_reqs.a_bus_req = 1;
		xotg->xotg_reqs.a_bus_drop = 0;

		extcon_id_notifications(&xotg->id_extcon_nb, 0, NULL);
	} else {
		otg->host = NULL;
	}
	return 0;
}

/* will be called from the HCD during its USB3 shared_hcd initialization */
static int xotg_set_xhci_host(struct usb_otg *otg, struct usb_bus *host)
{
	struct xotg *xotg;

	if (!otg)
		return -ENODEV;

	if (host) {
		xotg = container_of(otg->phy, struct xotg, phy);
		xotg_info(xotg->dev, "xhcihost = 0x%p\n", host);

		otg->xhcihost = host;
		/* set the otg_port here for the usbcore/HCD to be able
		 * to access it by setting it in the usb_bus structure
		 */
		otg->xhcihost->otg_port = xotg->ss_otg_port + 1;
	} else {
		otg->xhcihost = NULL;
	}
	return 0;
}

/* called by HCD when it has set the PORT_STATE of the OTG port = SUSPEND */
static int xotg_start_hnp(struct usb_otg *otg)
{
	struct xotg *xotg;

	if (!otg /*|| xotg->phy.state != OTG_STATE_B_IDLE*/)
		return 1;

	xotg = container_of(otg->phy, struct xotg, phy);

	xotg_dbg(xotg->dev, "start HNP, state=%s\n",
			usb_otg_state_string(xotg->phy.state));

	xotg_dbg(xotg->dev, "b_conn = %d, b_hnp_enable=%d\n",
		xotg->xotg_vars.b_conn,
		xotg->phy.otg->host->b_hnp_enable);

	xotg->xotg_vars.b_conn = 0;
	xotg->phy.otg->host->b_hnp_enable = 1;
	queue_work(xotg->otg_wq, &xotg->otg_work);
	return 0;
}

/* must be called by the PCD */
static int xotg_start_srp(struct usb_otg *otg)
{
	struct xotg *xotg;

	if (!otg)
		return 1;

	xotg = container_of(otg->phy, struct xotg, phy);

	xotg_dbg(xotg->dev, "start SRP\n");

	if (xotg->phy.state != OTG_STATE_B_IDLE)
		return 1;

	xotg->xotg_reqs.b_bus_req = 1;
	xotg_info(xotg->dev, "state %s -> a_suspend\n",
		usb_otg_state_string(xotg->phy.state));
	xotg->phy.state = OTG_STATE_A_SUSPEND;
	queue_work(xotg->otg_wq, &xotg->otg_work);
	return 0;
}

/*
 * main state machine which will be called from
 * 1. isr to switch to gadget or host mode.
 * 2. called from ioctl routine as part of user trigger
 * 3. during driver startup/initialization
 */
static void xotg_work(struct work_struct *work)
{
	struct xotg *xotg = container_of(work, struct xotg, otg_work);
	enum usb_otg_state from_state;
	struct usb_gadget *gadget = xotg->phy.otg->gadget;
	unsigned long flags;
	int state_changed = 0;
	u32 temp;

	spin_lock_irqsave(&xotg->lock, flags);

	from_state = xotg->phy.state;

	if (!gadget && (xotg->phy.state == OTG_STATE_B_PERIPHERAL)) {
		xotg_err(xotg->dev, "gadget driver not loaded yet ?\n");
		spin_unlock_irqrestore(&xotg->lock, flags);
		return;
	}

	xotg_dbg(xotg->dev, "switching from %s\n",
		usb_otg_state_string(from_state));

	switch (from_state) {
	case OTG_STATE_UNDEFINED:
	{
		/* TODO: do we need to read the id again assuming a
		 * little delay between scheduling of this
		 * function and execution of this function ?
		 */
		if (xotg->id) {
			xotg_info(xotg->dev, "state undefiend -> b_idle\n");
			/* set state */
			xotg->phy.state = OTG_STATE_B_IDLE;
			xotg->phy.otg->default_a = 0;

			/* do actions
			 * a) drv_vbus off
			 * b) loc_conn off : Automatic when (ID = 1) ?
			 * c) loc_sof off: Automatic when (ID = 1) ?
			 * or PORT_SUSPEND, PORT_RESUME is to be doen ?
			 * d) adp_sns or adp_prb
			 * disable vbus: we are b-device now
			 * vbus will already be switched off
			 */

			/* start the b_ssend_srp timer and note
			 * currently b_sess_vld = 0,
			 * if the timer expires before b_sess_vld
			 * becomes 1, then switch to b_srp_init state
			 * if it becomes one before the timer expires
			 * then delete this timer immediately and switch
			 * to b_peripheral (intr for v_sess_vld)
			 * We have to wait min of TB_SSEND_SRP (1.5s)
			 * and TB_SE0_SRP (1s) so timer set for 1.5s
			 */
			xotg_dbg(xotg->dev,
				"starting TB_SSEND_SRP(1.6s) timer\n");
			xotg->xotg_timer_list.b_ssend_srp_tmout = 0;
			mod_timer(&xotg->xotg_timer_list.b_ssend_srp_tmr,
				jiffies + msecs_to_jiffies(TB_SSEND_SRP));
		} else {
			xotg_info(xotg->dev, "state undefined -> a_idle\n");
			/* set state */
			xotg->phy.state = OTG_STATE_A_IDLE;

			/* do actions
			 * a) drv_vbus off
			 * b) loc_conn off : Automatic when (ID = 0) ?
			 * c) loc_sof off: Automatic when (ID = 0) ?
			 * or PORT_SUSPEND, PORT_RESUME is to be doen ?
			 * d) adp_prb
			 */
			xotg->phy.otg->default_a = 1;
			/* we are A-device, but don't start the VBUS
			 * just yet.
			 * Now state machine will get called from
			 * set_host() or from any OTG interrupt.
			 * TODO: check if to resume host and start vbus
			 * which will eventually cause interrupt which
			 * will move statemachine
			 */
		}
	}
	break;
	/* triggered by user ioctl or timeout */
	case OTG_STATE_B_IDLE:
	{
		temp = tegra_usb_pad_reg_read(
			XUSB_PADCTL_USB2_VBUS_ID_0);
		xotg->xotg_vars.b_sess_vld = temp &
			USB2_VBUS_ID_0_VBUS_SESS_VLD_STS;

		xotg_dbg(xotg->dev, "id=%d, default_a=%d, b_bus_req=%d\n",
			xotg->id, xotg->phy.otg->default_a,
			xotg->xotg_reqs.b_bus_req);

		xotg_dbg(xotg->dev, "b_ssend_srp_tmout=%d, b_sess_vld=%d\n",
			xotg->xotg_timer_list.b_ssend_srp_tmout,
			xotg->xotg_vars.b_sess_vld);

		if (!xotg->id) {
			xotg_info(xotg->dev, "state b_idle -> a_idle\n");
			state_changed = 1;
			xotg->xotg_vars.a_srp_det = 0;
			xotg->phy.state = OTG_STATE_A_IDLE;
			xotg->phy.otg->default_a = 1;
			xotg->xotg_reqs.a_bus_req = 1;

			if (!xotg->xotg_timer_list.b_ssend_srp_tmout) {
				xotg_dbg(xotg->dev,
					"del_timer b_ssend_srp_tmr\n");
				del_timer_sync(
					&xotg->xotg_timer_list.b_ssend_srp_tmr);
			} else if (xotg->xotg_timer_list.b_ssend_srp_tmout) {
				xotg->xotg_timer_list.b_ssend_srp_tmout = 0;
			}
			spin_unlock_irqrestore(&xotg->lock, flags);
			xotg_work(&xotg->otg_work);
			return;
		} else if (xotg->xotg_vars.b_sess_vld) {
			xotg_info(xotg->dev, "state b_idle -> b_peripheral\n");
			state_changed = 1;
			/* b_idle to b_peripheral transition requires
			 * VBUS_SESS_VLD to be set. (7.2.1)
			 */
			xotg->phy.state = OTG_STATE_B_PERIPHERAL;
			/* no need to stop vbus, no need to clear
			 * reverse id. its already stopped and cleared
			 * if we are a B-device. essentially do nothing
			 * but set the host port to rxdetect
			 */

			/* A host has responded to the SRP request by enabling
			 * the VBUS before TB_SRP_FAIL. Delete the timer.
			 */
			if (!xotg->xotg_timer_list.b_srp_response_wait_tmout) {
				xotg_dbg(xotg->dev,
					"del_timer b_srp_response_wait_tmr\n");
				del_timer_sync(&xotg->xotg_timer_list.
						b_srp_response_wait_tmr);
			}
		} else if (xotg->xotg_reqs.b_bus_req &&
			xotg->xotg_timer_list.b_ssend_srp_tmout) {
			/* FIXME: b_bus_req (only?) or power_up also */
			/* ensure VBUS is not valid first.if not just
			 * stay in the same state
			 */
			xotg->xotg_timer_list.b_ssend_srp_tmout = 0;
			if (!xotg->xotg_vars.b_sess_vld) {
				xotg_info(xotg->dev,
					"state b_idle -> b_srp_init\n");
				state_changed = 1;
				xotg->phy.state = OTG_STATE_B_SRP_INIT;

				/* do actions of the next state */
				if (!xotg_generate_srp(xotg)) {
					/* b_srp_done: TRUE when
					 * B-device has completed
					 * inititating SRP. Need to wait
					 * for the response though ?
					 */
					xotg->xotg_vars.b_srp_initiated = 1;
					xotg_dbg(xotg->dev,
					"starting TB_SRP_DONE(100ms) timer\n");
					xotg->xotg_timer_list.
						b_srp_done_tmout = 0;
					mod_timer(&xotg->xotg_timer_list.
					b_srp_done_tmr, jiffies +
					msecs_to_jiffies(TB_SRP_DONE));
				}
				/* After generating SRP pulse, start a
				 * timer to get the response back from
				 * the Host side(VBUS SESS VLD)
				 * starttimer(srp_fail_timer)
				 */
				xotg_dbg(xotg->dev,
				"starting TB_SRP_FAIL(5.5s) timer\n");
				xotg->xotg_timer_list.
					b_srp_response_wait_tmout = 0;
				mod_timer(&xotg->xotg_timer_list.
				b_srp_response_wait_tmr, jiffies +
					msecs_to_jiffies(TB_SRP_FAIL));
			}
		} else if (xotg->xotg_timer_list.b_srp_response_wait_tmout) {
			xotg->xotg_timer_list.b_srp_response_wait_tmout = 0;
			/* inform the user above that A-device
			 * has not responded
			 */
			xotg_warn(xotg->dev,
				"A-host not responding to SRP request\n");
		}
		if (state_changed && !xotg->xotg_timer_list.b_ssend_srp_tmout) {
			/* do stuff to cleanup the B_IDLE state */
			xotg_dbg(xotg->dev, "del_timer b_ssend_srp_tmr\n");
			del_timer_sync(&xotg->xotg_timer_list.b_ssend_srp_tmr);
		} else if (xotg->xotg_timer_list.b_ssend_srp_tmout) {
			xotg->xotg_timer_list.b_ssend_srp_tmout = 0;
		}
	}
	break;
	case OTG_STATE_B_SRP_INIT:
	{
		xotg_dbg(xotg->dev, "id=%d, b_srp_response_wait_tmout=%d\n",
			xotg->id,
			xotg->xotg_timer_list.b_srp_response_wait_tmout);
		xotg_dbg(xotg->dev, "b_srp_done=%d, default_a=%d\n",
			xotg->xotg_vars.b_srp_done, xotg->phy.otg->default_a);

		/*
		 * if micro-A is attached, go back to B_IDLE state and
		 * from there we go back to A_IDLE state
		 */
		if (!xotg->id || xotg->xotg_vars.b_srp_done) {
			xotg_info(xotg->dev, "state b_srp_init -> b_idle\n");
			xotg->xotg_vars.b_srp_done = 0;
			state_changed = 1;
			/* reset the b_srp_done flag back to 0 when
			 * changing from SRP_INIT to B_IDLE state
			 */
			xotg->phy.state = OTG_STATE_B_IDLE;
		} else if (!xotg->xotg_vars.b_srp_done &&
			xotg->xotg_timer_list.b_srp_response_wait_tmout) {
			xotg_info(xotg->dev, "state b_srp_init -> b_idle\n");
			xotg->xotg_timer_list.b_srp_response_wait_tmout = 0;
			/* inform the user above that A-device
			 * has not responded
			 */
			xotg_dbg(xotg->dev,
			"A-host not responding to SRP request\n");
			/* FIXME state has to be changed to B_IDLE */
			xotg->phy.state = OTG_STATE_B_IDLE;
		}
		/* We delete the timer only when the state is changed due
		 * to Micro-A being attached. We should not delete the timer
		 * when state is changed due to b_srp_done
		 */
		if (state_changed && !xotg->id &&
			!xotg->xotg_timer_list.b_srp_response_wait_tmout) {
			xotg_dbg(xotg->dev,
				"del_timer b_srp_response_wait_tmr\n");
			del_timer_sync(&xotg->xotg_timer_list.
				b_srp_response_wait_tmr);
		} else if (xotg->xotg_timer_list.b_srp_response_wait_tmout) {
			xotg->xotg_timer_list.b_srp_response_wait_tmout = 0;
		}
	}
	break;
	case OTG_STATE_B_PERIPHERAL:
	{
		temp = tegra_usb_pad_reg_read(XUSB_PADCTL_USB2_VBUS_ID_0);
		xotg->xotg_vars.b_sess_vld = temp &
			USB2_VBUS_ID_0_VBUS_SESS_VLD_STS;

		xotg_dbg(xotg->dev, "id=%d, b_sess_vld=%d, b_bus_req=%d\n",
			xotg->id, xotg->xotg_vars.b_sess_vld,
			xotg->xotg_reqs.b_bus_req);
		xotg_dbg(xotg->dev, "rcvd_otg_hnp_reqd=%d, b_hnp_enable=%d\n",
			xotg->phy.otg->gadget->rcvd_otg_hnp_reqd,
			xotg->phy.otg->gadget->b_hnp_enable);
		xotg_dbg(xotg->dev, "b_bus_suspend=%d\n",
			xotg->xotg_vars.b_bus_suspend);

		if (!xotg->id || !xotg->xotg_vars.b_sess_vld) {
			xotg_info(xotg->dev, "state b_peripheral -> b_idle\n");
			state_changed = 1;
			xotg->phy.state = OTG_STATE_B_IDLE;
			xotg->xotg_vars.b_bus_suspend = 0;
			xotg->xotg_vars.a_bus_suspend = 0;
			xotg->phy.otg->gadget->rcvd_otg_hnp_reqd = 0;
			/* TODO need to run b_ssend_srp_tmr if id = 1 */
			xotg_dbg(xotg->dev,
				"starting TB_SSEND_SRP(1.6s) timer\n");
			xotg->xotg_timer_list.b_ssend_srp_tmout = 0;
			mod_timer(&xotg->xotg_timer_list.b_ssend_srp_tmr,
				jiffies + msecs_to_jiffies(TB_SSEND_SRP));
		} else if ((xotg->xotg_reqs.b_bus_req ||
			xotg->phy.otg->gadget->rcvd_otg_hnp_reqd) &&
			xotg->phy.otg->gadget->b_hnp_enable &&
			xotg->xotg_vars.b_bus_suspend) {
			xotg_info(xotg->dev,
				"state b_peripheral -> b_wait_acon\n");
			xotg->xotg_vars.b_bus_suspend = 0;
			/* U: b_bus_req, NBP: b_hnp_enable,
			 * NBP: a_bus_suspend
			 */
			state_changed = 1;

			if (xotg_enable_gadget(xotg, 0) == 1) {
				/* should not happen */
				xotg_err(xotg->dev,
				"xotg_enable_gadget(xotg, 0) failed\n");
			}

			xotg_set_reverse_id(xotg, true);

			if (gadget && gadget->ops)
				gadget->ops->set_port_state(gadget, 0x5);

			/* a_bus_suspend will be set in the otg phy by
			 * the udc driver when it gets a port status
			 * change event because of the A-side bus/port
			 * going into U3/suspend. It needs to set it and
			 * immediately trigger a call to the current
			 * state machine
			 * FIXME: host driver to be started?
			 */
			xotg->phy.otg->host->is_b_host = 1;
			xotg->phy.state = OTG_STATE_B_WAIT_ACON;
			xotg_dbg(xotg->dev,
				"starting TB_ASE0_BRST(155ms) timer\n");
			xotg->xotg_timer_list.b_ase0_brst_tmout = 0;
			mod_timer(&xotg->xotg_timer_list.b_ase0_brst_tmr,
				jiffies + msecs_to_jiffies(TB_ASE0_BRST));
		}
	}
	break;
	case OTG_STATE_B_WAIT_ACON:
	{
		temp = tegra_usb_pad_reg_read(XUSB_PADCTL_USB2_VBUS_ID_0);
		xotg->xotg_vars.b_sess_vld = temp &
			USB2_VBUS_ID_0_VBUS_SESS_VLD_STS;

		xotg_dbg(xotg->dev, "id=%d, a_conn=%d, b_sess_vld=%d\n",
			xotg->id, xotg->xotg_vars.a_conn,
			xotg->xotg_vars.b_sess_vld);
		xotg_dbg(xotg->dev, "a_bus_resume=%d, b_ase0_brst_tmout=%d\n",
			xotg->xotg_vars.a_bus_resume,
			xotg->xotg_timer_list.b_ase0_brst_tmout);

		if (xotg->xotg_vars.a_conn) {
			/* a_conn is set by PCD when connect to A is
			 * detected and cleared when disconnect to A is
			 * detected
			 */
			xotg_info(xotg->dev, "state b_wait_acon -> b_host\n");
			state_changed = 1;
			xotg->phy.state = OTG_STATE_B_HOST;

			/* PCD sets the a_conn when B-device detects
			 * connection from A-device
			 * Luke : Check if reverse id setting/clearing
			 * works when port is in U3 ? earlier this was
			 * the proposal, but later it changed to
			 * "port needs to be disabled ?"
			 * TODO: check if below U3 is good enough or
			 * DISABLED is required
			 */
			usb_bus_start_enum(xotg->phy.otg->host,
					xotg->hs_otg_port);
		} else if (!xotg->id || !xotg->xotg_vars.b_sess_vld) {
			xotg_info(xotg->dev, "state b_wait_acon -> b_idle\n");
			state_changed = 1;
			xotg->phy.state = OTG_STATE_B_IDLE;
			xotg->phy.otg->host->is_b_host = 0;

			/* start the b_ssend_srp timer
			 * note currently b_sess_vld = 0,
			 */

			xotg_set_reverse_id(xotg, false);

			if (xotg_enable_gadget(xotg, 1) == 1) {
				/* should not happen */
				xotg_err(xotg->dev,
				"xotg_enable_gadget(xotg, 1) failed\n");
			}

			xotg_dbg(xotg->dev,
				"starting TB_SSEND_SRP(1.6s) timer\n");
			xotg->xotg_timer_list.b_ssend_srp_tmout = 0;
			mod_timer(&xotg->xotg_timer_list.b_ssend_srp_tmr,
				jiffies + msecs_to_jiffies(TB_SSEND_SRP));
		} else if (xotg->xotg_vars.a_bus_resume ||
			xotg->xotg_timer_list.b_ase0_brst_tmout) {
			xotg_info(xotg->dev,
				"state b_wait_acon -> b_peripheral\n");
			if (xotg->xotg_timer_list.b_ase0_brst_tmout) {
				xotg_warn(xotg->dev,
					"No Repsonse from A-Device\n");
				xotg->xotg_timer_list.b_ase0_brst_tmout = 0;

			}

			xotg_set_reverse_id(xotg, false);
			if (xotg_enable_gadget(xotg, 1) == 1) {
				/* should not happen */
				xotg_err(xotg->dev,
				"xotg_enable_gadget(xotg, 0) failed\n");
			}


			/* this case is when A is signalling a resume
			 * before it saw B lower its D+ resistor
			 */
			state_changed = 1;
			xotg->phy.state = OTG_STATE_B_PERIPHERAL;

			/* do actions
			 * no action required here apart from deleting
			 * timer. just get back to where you came from
			 */
		}
		if (state_changed && !xotg->xotg_timer_list.b_ase0_brst_tmout) {
			xotg_dbg(xotg->dev, "del_timer b_ase0_brst_tmr\n");
			del_timer_sync(&xotg->xotg_timer_list.b_ase0_brst_tmr);
		} else if (xotg->xotg_timer_list.b_ase0_brst_tmout) {
			xotg->xotg_timer_list.b_ase0_brst_tmout = 0;
		}
	}
	break;
	case OTG_STATE_B_HOST:
	{
		temp = tegra_usb_pad_reg_read(XUSB_PADCTL_USB2_VBUS_ID_0);
		xotg->xotg_vars.b_sess_vld = temp &
			USB2_VBUS_ID_0_VBUS_SESS_VLD_STS;

		if (!xotg->xotg_reqs.b_bus_req || !xotg->xotg_vars.a_conn) {
			xotg_info(xotg->dev, "state b_host -> b_peripheral\n");
			state_changed = 1;
			xotg->phy.state = OTG_STATE_B_PERIPHERAL;

			xotg->phy.otg->gadget->b_hnp_enable = 0;
			xotg->xotg_vars.b_bus_suspend = 0;
			xotg->phy.otg->host->is_b_host = 0;
			/* do actions of the next state,
			 * transitioning from host to peripheral
			 * 1. disable the host port
			 * 2. clear the reverse id
			 * 3. set the host port back to rxdetect.
			 * 4. start/resume the gadget driver
			 * FIXME: see if host port to be disabled?
			 */
			xotg_suspend_host(xotg, XDEV_DISABLED);
			xotg_set_reverse_id(xotg, false);
			xotg_suspend_host(xotg, XDEV_RXDETECT);
			if (!xotg_enable_gadget(xotg, 1) == 1) {
				xotg_err(xotg->dev,
				"xotg_enable_gadget(xotg, 1) failed\n");
			}
		}
	}
	break;
	case OTG_STATE_A_IDLE:
	{
		xotg_dbg(xotg->dev, "id=%d, a_bus_drop=%d, a_bus_req=%d\n",
			xotg->id, xotg->xotg_reqs.a_bus_drop,
			xotg->xotg_reqs.a_bus_req);
		xotg_dbg(xotg->dev, "a_srp_det=%d\n",
			xotg->xotg_vars.a_srp_det);

		if (xotg->id) {
			xotg_info(xotg->dev, "state a_idle -> b_idle\n");
			xotg->phy.state = OTG_STATE_B_IDLE;
			xotg->phy.otg->default_a = 0;
			/* do actions of the next state
			 * disable vbus
			 */
			xotg_drive_vbus(xotg, 0);
			xotg_dbg(xotg->dev,
				"starting TB_SSEND_SRP(1.6s) timer\n");
			xotg->xotg_timer_list.b_ssend_srp_tmout = 0;
			mod_timer(&xotg->xotg_timer_list.b_ssend_srp_tmr,
				jiffies + msecs_to_jiffies(TB_SSEND_SRP));

			/* ID floating caused host ELPG exit if hcd was in
			 * ELPG so make sure host enters to ELPG again.
			 */
			xotg_roothub_resume(xotg);
		} else if (!xotg->xotg_reqs.a_bus_drop &&
				(xotg->xotg_reqs.a_bus_req ||
					xotg->xotg_vars.a_srp_det)) {
			/* drive vbus */
			xotg_info(xotg->dev, "state a_idle -> a_wait_vrise\n");
			xotg->phy.state = OTG_STATE_A_WAIT_VRISE;
			if (xotg->xotg_vars.a_srp_det)
				xotg->xotg_vars.a_srp_det = 0;

			xotg_drive_vbus(xotg, 1);

			/* wait for T_A_VBUS_RISE time */
			xotg_dbg(xotg->dev,
				"starting TA_VBUS_RISE(100ms) timer\n");
			xotg->xotg_timer_list.a_wait_vrise_tmout = 0;
			mod_timer(&xotg->xotg_timer_list.
				a_wait_vrise_tmr, jiffies +
				msecs_to_jiffies(TA_VBUS_RISE));
		}
	}
	break;
	case OTG_STATE_A_WAIT_VRISE:
	{
		xotg->xotg_vars.a_vbus_vld = xotg->vbus_enabled;

		xotg_dbg(xotg->dev, "a_vbus_vld=%d, id=%d, a_bus_drop=%d\n",
			xotg->xotg_vars.a_vbus_vld, xotg->id,
			xotg->xotg_reqs.a_bus_drop);
		xotg_dbg(xotg->dev, "a_wait_vrise_tmout=%d\n",
			xotg->xotg_timer_list.a_wait_vrise_tmout);

		if (xotg->id || (xotg->xotg_timer_list.a_wait_vrise_tmout &&
				!xotg->xotg_vars.a_vbus_vld)
					|| xotg->xotg_reqs.a_bus_drop) {
			xotg_info(xotg->dev,
				"state a_wait_vrise -> a_wait_vfall\n");
			if (xotg->xotg_timer_list.a_wait_vrise_tmout)
				xotg->xotg_timer_list.a_wait_vrise_tmout = 0;
			state_changed = 1;
			xotg->phy.state = OTG_STATE_A_WAIT_VFALL;

			/* set a_srp_det to 0. OTG2 spec 7.4.1.7 */
			xotg->xotg_vars.a_srp_det = 0;

			/* stop drive vbus */
			xotg_drive_vbus(xotg, 0);

			xotg_dbg(xotg->dev,
				"starting TA_WAIT_VFALL(1sec) timer\n");
			xotg->xotg_timer_list.a_wait_vfall_tmout = 0;
			mod_timer(&xotg->xotg_timer_list.a_wait_vfall_tmr,
				jiffies + msecs_to_jiffies(TA_WAIT_VFALL));
		} else if (xotg->xotg_vars.a_vbus_vld) {
			xotg_info(xotg->dev,
				"state a_wait_vrise -> a_wait_bcon\n");
			state_changed = 1;
			xotg->phy.state = OTG_STATE_A_WAIT_BCON;

			/* Put the host port in RxDetect state so that
			 * B-device signalling connection will be
			 * detected
			 */

			/* start the timer */
			xotg_dbg(xotg->dev,
				"starting TA_WAIT_BCON(9sec) timer\n");
			xotg->xotg_timer_list.a_wait_bcon_tmout = 0;
			mod_timer(&xotg->xotg_timer_list.a_wait_bcon_tmr,
				jiffies + msecs_to_jiffies(TA_WAIT_BCON));
		}
		if (state_changed &&
			!xotg->xotg_timer_list.a_wait_vrise_tmout) {
			xotg_dbg(xotg->dev, "del_timer a_wait_vrise_tmr\n");
			del_timer_sync(&xotg->xotg_timer_list.a_wait_vrise_tmr);
		} else if (xotg->xotg_timer_list.a_wait_vrise_tmout) {
			xotg->xotg_timer_list.a_wait_vrise_tmout = 0;
		}
	}
	break;
	case OTG_STATE_A_WAIT_VFALL:
	{
		xotg_dbg(xotg->dev, "id=%d, a_wait_vfall_tmout=%d\n",
			xotg->id, xotg->xotg_timer_list.a_wait_vfall_tmout);

		if (xotg->xotg_vars.otg_test_device_enumerated)
			xotg->xotg_vars.otg_test_device_enumerated = 0;

		if (xotg->id) {
			xotg_info(xotg->dev, "state a_wait_vfall -> b_idle\n");
			state_changed = 1;
			xotg->phy.state = OTG_STATE_B_IDLE;
			xotg->phy.otg->default_a = 0;
			/* do actions of the next state
			 * disable vbus
			 */
			xotg_dbg(xotg->dev,
				"starting TB_SSEND_SRP(1.6s) timer\n");
			xotg->xotg_timer_list.b_ssend_srp_tmout = 0;
			mod_timer(&xotg->xotg_timer_list.b_ssend_srp_tmr,
				jiffies + msecs_to_jiffies(TB_SSEND_SRP));
		} else if (xotg->xotg_timer_list.a_wait_vfall_tmout) {
			xotg_info(xotg->dev, "state a_wait_vfall -> a_idle\n");
			xotg->xotg_timer_list.a_wait_vfall_tmout = 0;
			state_changed = 1;
			xotg->phy.state = OTG_STATE_A_IDLE;

			/* do actions
			 * enable the srp detect
			 */
			xotg_enable_srp_detect(xotg, true);

			xotg->phy.otg->default_a = 1;
			if (!session_supported)
				xotg_drive_vbus(xotg, 1);
		}
		/* ID ground caused host ELPG exit if hcd was in
		 * ELPG so make sure host enters to ELPG again.
		 * Also enable vbus if session is not suppported
		 */
		xotg_roothub_resume(xotg);
		if (state_changed &&
			!xotg->xotg_timer_list.a_wait_vrise_tmout) {
			/* delete the timer to wait for vfall */
			xotg_dbg(xotg->dev, "del_timer a_wait_vfall_tmr\n");
			del_timer_sync(&xotg->xotg_timer_list.a_wait_vfall_tmr);
		} else if (xotg->xotg_timer_list.a_wait_vrise_tmout) {
			xotg->xotg_timer_list.a_wait_vrise_tmout = 0;
		}
	}
	break;
	case OTG_STATE_A_WAIT_BCON:
	{
		xotg->xotg_vars.a_vbus_vld = xotg->vbus_enabled;

		xotg_dbg(xotg->dev, "id=%d, a_bus_drop=%d, b_conn=%d\n",
			xotg->id, xotg->xotg_reqs.a_bus_drop,
			xotg->xotg_vars.b_conn);
		xotg_dbg(xotg->dev, "a_wait_bcon_tmout=%d\n",
			xotg->xotg_timer_list.a_wait_bcon_tmout);

		if (xotg->id || xotg->xotg_reqs.a_bus_drop ||
			xotg->xotg_timer_list.a_wait_bcon_tmout) {
			xotg_info(xotg->dev,
				"state a_wait_bcon -> a_wait_vfall\n");
			if (xotg->xotg_timer_list.a_wait_bcon_tmout) {
				xotg->xotg_timer_list.a_wait_bcon_tmout = 0;
				xotg_warn(xotg->dev,
					"No Response from Device\n");
			}
			state_changed = 1;
			xotg->phy.state = OTG_STATE_A_WAIT_VFALL;

			/* do actions */
			xotg_drive_vbus(xotg, 0);
			xotg_dbg(xotg->dev,
				"starting TA_WAIT_VFALL(1sec) timer\n");
			xotg->xotg_timer_list.a_wait_vfall_tmout = 0;
			mod_timer(&xotg->xotg_timer_list.a_wait_vfall_tmr,
				jiffies + msecs_to_jiffies(TA_WAIT_VFALL));
		} else if (xotg->xotg_vars.b_conn) {
			/* will be set by the HCD whenever it receives a
			 * port status change event
			 */
			xotg_info(xotg->dev, "state a_wait_bcon -> a_host\n");
			state_changed = 1;
			xotg->phy.state = OTG_STATE_A_HOST;
			xotg->xotg_reqs.a_bus_req = 1;
			xotg->xotg_vars.a_bus_suspend = 0;
			xotg->xotg_vars.a_srp_det = 0;

			/* hcd to pick up this port since port state is
			 * rxdetect. either suspend the gadget driver or
			 * set the device port to disabled
			 */
			if (!xotg_enable_gadget(xotg, 0) == 1) {
				/* should not happen */
				xotg_dbg(xotg->dev,
				"xotg_enable_gadget(xotg, 0) failed\n");
			}
			/* below 3 steps will complete the role
			 * assignment/swap process
			 * FIXME PCD already in suspend
			 */
			if (gadget && gadget->ops)
				gadget->ops->set_port_state(gadget, 0x4);
			xotg_set_reverse_id(xotg, false);
			if (gadget && gadget->ops)
				gadget->ops->set_port_state(gadget, 0x5);
		}
		if (state_changed && !xotg->xotg_timer_list.a_wait_bcon_tmout) {
			xotg_dbg(xotg->dev, "del_timer a_wait_bcon_tmr\n");
			del_timer_sync(&xotg->xotg_timer_list.a_wait_bcon_tmr);
		} else if (xotg->xotg_timer_list.a_wait_bcon_tmout) {
			xotg->xotg_timer_list.a_wait_bcon_tmout = 0;
		}
	}
	break;
	case OTG_STATE_A_HOST:
	{
		xotg->xotg_vars.a_vbus_vld = xotg->vbus_enabled;

		xotg_dbg(xotg->dev, "id=%d, b_conn=%d, a_bus_drop=%d\n",
			xotg->id, xotg->xotg_vars.b_conn,
			xotg->xotg_reqs.a_bus_drop);
		xotg_dbg(xotg->dev, "a_bus_req=%d\n",
			xotg->xotg_reqs.a_bus_req);

		/* b_conn = 0: when HCD detects B has disconnected */
		if (xotg->id || xotg->xotg_reqs.a_bus_drop) {
			xotg_info(xotg->dev, "state a_host -> a_wait_vfall\n");
			xotg->phy.state = OTG_STATE_A_WAIT_VFALL;
			state_changed = 1;
			/* do actions */
			xotg_drive_vbus(xotg, 0);
			xotg_dbg(xotg->dev,
				"starting TA_WAIT_VFALL(1sec) timer\n");
			xotg->xotg_timer_list.a_wait_vfall_tmout = 0;
			mod_timer(&xotg->xotg_timer_list.a_wait_vfall_tmr,
				jiffies + msecs_to_jiffies(TA_WAIT_VFALL));
		} else if (!xotg->id && !xotg->xotg_vars.b_conn
			/*&& !xotg->xotg_vars.a_bus_suspend*/) {
			xotg_info(xotg->dev, "state a_host -> a_wait_bcon\n");
			xotg->phy.state = OTG_STATE_A_WAIT_BCON;
			state_changed = 1;

			if (xotg->xotg_vars.otg_test_device_enumerated)
				xotg->xotg_vars.otg_test_device_enumerated = 0;

			/* do actions */
			xotg_dbg(xotg->dev,
			"starting TA_WAIT_BCON(9sec) timer\n");
			xotg->xotg_timer_list.a_wait_bcon_tmout = 0;
			mod_timer(&xotg->xotg_timer_list.a_wait_bcon_tmr,
				jiffies + msecs_to_jiffies(TA_WAIT_BCON));
		} else if (!xotg->xotg_reqs.a_bus_req) {
			xotg_info(xotg->dev, "state a_host -> a_suspend\n");
			/* TODO: a_bus_drop = !a_bus_req. So, will this
			 * else-if get executed anytime at all ?
			 */
			xotg->phy.state = OTG_STATE_A_SUSPEND;
			state_changed = 1;

			/* start a timer to keep track of B-device
			 * signalling a disconnect. Once b_hnp_enable is
			 * enabled for B-device the disconnect signal
			 * from B-device  will indicate to A-host that
			 * role swapping needs to be done.
			 */
			if (xotg->phy.otg->host->b_hnp_enable) {
				xotg_dbg(xotg->dev,
				"starting TA_AIDL_BDIS(200ms) timer\n");
				xotg->xotg_timer_list.a_aidl_bdis_tmout = 0;
				mod_timer(&xotg->xotg_timer_list.
					a_aidl_bdis_tmr, jiffies +
					msecs_to_jiffies(TA_AIDL_BDIS));
			}
		}  else if (xotg->xotg_timer_list.a_tst_maint_tmout) {
			xotg_dbg(xotg->dev,
				"TST_MAINT timeout. Driving Vbus Off\n");
			xotg->xotg_timer_list.a_tst_maint_tmout = 0;
			if (session_supported)
				xotg_drive_vbus(xotg, 0);
		} else if (xotg->xotg_vars.start_tst_maint_timer) {
			xotg_dbg(xotg->dev,
				"Starting the TST_MAINT timer\n");
			xotg->xotg_vars.start_tst_maint_timer = 0;
			xotg->xotg_timer_list.a_tst_maint_tmout = 0;
			mod_timer(&xotg->xotg_timer_list.a_tst_maint_tmr,
				jiffies + msecs_to_jiffies(TA_TST_MAINT));
		}
		if (state_changed &&
		   (xotg->phy.state != OTG_STATE_A_SUSPEND) &&
		   (!xotg->xotg_timer_list.a_tst_maint_tmout)) {
			xotg_dbg(xotg->dev, "del_timer a_tst_maint_tmr");
			del_timer_sync(&xotg->xotg_timer_list.a_tst_maint_tmr);
		}

	}
	break;
	/* triggered by following:
	 * 1. b_conn = 0 and a_set_b_hnp_en = 0
	 * 2. a_vbus_vld = 0
	 * 3. a_aidl_bdis_tmout
	 * 4. b_conn = 0 and a_set_b_hnp_en = 1
	 */
	case OTG_STATE_A_SUSPEND:
	{
		xotg->xotg_vars.a_vbus_vld = xotg->vbus_enabled;

		xotg_dbg(xotg->dev, "id=%d, a_bus_drop=%d, b_hnp_enable=%d\n",
			xotg->id, xotg->xotg_reqs.a_bus_drop,
			xotg->phy.otg->host->b_hnp_enable);
		xotg_dbg(xotg->dev, "a_aidl_bdis_tmout=%d, b_conn=%d\n",
			xotg->xotg_timer_list.a_aidl_bdis_tmout,
			xotg->xotg_vars.b_conn);

		if (xotg->id || xotg->xotg_reqs.a_bus_drop ||
			xotg->xotg_timer_list.a_aidl_bdis_tmout) {
			xotg_info(xotg->dev,
				"state a_suspend -> a_wait_vfall\n");
			state_changed = 1;
			xotg->phy.state = OTG_STATE_A_WAIT_VFALL;

			xotg->xotg_timer_list.a_aidl_bdis_tmout = 0;

			/* stop drive vbus */
			xotg_drive_vbus(xotg, 0);
			xotg_dbg(xotg->dev,
				"starting TA_WAIT_VFALL(1sec) timer\n");
			xotg->xotg_timer_list.a_wait_vfall_tmout = 0;
			mod_timer(&xotg->xotg_timer_list.a_wait_vfall_tmr,
				jiffies + msecs_to_jiffies(TA_WAIT_VFALL));
		} else if (xotg->xotg_reqs.a_bus_req) {
			xotg_info(xotg->dev, "state a_suspend -> a_host\n");
			state_changed = 1;
			xotg->phy.state = OTG_STATE_A_HOST;
			/* VBUS is already enabled */
		} else if (!xotg->xotg_vars.b_conn &&
				xotg->phy.otg->host->b_hnp_enable) {
			/* b_conn is cleared by the HCD since it sees a
			 * disconnect from B-device on the other side.
			 * This is an indication of a role swap
			 */
			xotg_info(xotg->dev,
				"state a_suspend -> a_peripheral\n");
			state_changed = 1;
			xotg->phy.state = OTG_STATE_A_PERIPHERAL;
			/* A-host is going to be A-peripheral as part of
			 * HNP/RSP so set reverse id bit and start the
			 * gadget below defined in xhci-hub.c
			 */

			if (xotg_suspend_host(xotg, XDEV_DISABLED) == 1) {
				xotg_err(xotg->dev,
					"xotg_suspend_host(D)failed\n");
			}
			/* set reverse id */
			xotg_set_reverse_id(xotg, true);

			if (xotg_suspend_host(xotg, XDEV_RXDETECT) == 1) {
				xotg_err(xotg->dev,
					"xotg_suspend_host(R)failed\n");
			}

			tegra_usb_pad_reg_update(XUSB_PADCTL_USB2_VBUS_ID_0,
				USB2_VBUS_ID_0_VBUS_OVERRIDE,
				USB2_VBUS_ID_0_VBUS_OVERRIDE);

			if (!xotg_enable_gadget(xotg, 1) == 1) {
				xotg_err(xotg->dev,
				"xotg_enable_gadget(xotg, 1) failed\n");
			}

		} else if (!xotg->xotg_vars.b_conn &&
				!xotg->phy.otg->host->b_hnp_enable) {
			xotg_info(xotg->dev,
				"state a_suspend -> a_wait_bcon\n");
			state_changed = 1;
			xotg->phy.state = OTG_STATE_A_WAIT_BCON;
			xotg->xotg_reqs.a_bus_req = 1;
			xotg->xotg_vars.a_bus_suspend = 0;

			/* do actions
			 * Put the host port in RxDetect state so that
			 * B-device signalling connection is detected
			 */
			xotg_suspend_host(xotg, XDEV_RXDETECT);
			xotg_dbg(xotg->dev,
				"starting TA_WAIT_BCON(9sec) timer\n");
			xotg->xotg_timer_list.a_wait_bcon_tmout = 0;
			mod_timer(&xotg->xotg_timer_list.a_wait_bcon_tmr,
				jiffies + msecs_to_jiffies(4000));
		} else if (xotg->xotg_timer_list.a_tst_maint_tmout) {
			xotg_dbg(xotg->dev,
				"TST_MAINT timeout. Drive VBUS OFF\n");
			xotg->xotg_timer_list.a_tst_maint_tmout = 0;
			if (session_supported)
				xotg_drive_vbus(xotg, 0);
		}

		if (state_changed &&
		   !xotg->xotg_timer_list.a_tst_maint_tmout) {
			xotg_dbg(xotg->dev,
				"del_timer a_tst_maint_tmr %p\n",
				&xotg->xotg_timer_list.a_tst_maint_tmr);
			del_timer_sync(
				&xotg->xotg_timer_list.a_tst_maint_tmr);
		}

		if (state_changed &&
			xotg->phy.otg->host->b_hnp_enable &&
			!xotg->xotg_timer_list.a_aidl_bdis_tmout) {
			xotg_dbg(xotg->dev, "del_timer a_aidl_bdis_tmr %p\n",
				&xotg->xotg_timer_list.a_aidl_bdis_tmr);
			del_timer_sync(&xotg->xotg_timer_list.a_aidl_bdis_tmr);
		} else if (xotg->xotg_timer_list.a_aidl_bdis_tmout) {
			xotg->xotg_timer_list.a_aidl_bdis_tmout = 0;
		}
	}
	break;
	case OTG_STATE_A_PERIPHERAL:
	{
		xotg->xotg_vars.a_vbus_vld = xotg->vbus_enabled;

		if (xotg->id || xotg->xotg_reqs.a_bus_drop) {
			xotg_info(xotg->dev,
				"state a_peripheral -> a_wait_vfall\n");
			state_changed = 1;
			xotg->phy.state = OTG_STATE_A_WAIT_VFALL;
			xotg_set_reverse_id(xotg, false);
			tegra_usb_pad_reg_update(XUSB_PADCTL_USB2_VBUS_ID_0,
				USB2_VBUS_ID_0_VBUS_OVERRIDE, 0);
			/* stop drive vbus */
			xotg_drive_vbus(xotg, 0);
			/* enable the srp detect */
			xotg_enable_srp_detect(xotg, true);
			xotg_dbg(xotg->dev,
				"starting TA_WAIT_VFALL(1sec) timer\n");
			xotg->xotg_timer_list.a_wait_vfall_tmout = 0;
			mod_timer(&xotg->xotg_timer_list.a_wait_vfall_tmr,
				jiffies + msecs_to_jiffies(TA_WAIT_VFALL));
		} else if (xotg->xotg_timer_list.a_bidl_adis_tmout) {
			xotg_info(xotg->dev,
				"state a_peripheral -> a_wait_bcon\n");
			xotg->xotg_timer_list.a_bidl_adis_tmout = 0;
			state_changed = 1;
			xotg->phy.state = OTG_STATE_A_WAIT_BCON;

			if (xotg->xotg_vars.otg_test_device_enumerated)
				xotg->xotg_vars.otg_test_device_enumerated = 0;

			xotg_set_reverse_id(xotg, false);

			tegra_usb_pad_reg_update(XUSB_PADCTL_USB2_VBUS_ID_0,
				USB2_VBUS_ID_0_VBUS_OVERRIDE, 0);


			xotg_dbg(xotg->dev,
				"starting TA_WAIT_BCON(9sec) timer\n");
			xotg->xotg_timer_list.a_wait_bcon_tmout = 0;
			mod_timer(&xotg->xotg_timer_list.a_wait_bcon_tmr,
				jiffies + msecs_to_jiffies(TA_WAIT_BCON));
		} else if (xotg->xotg_vars.b_bus_suspend) {
			xotg_dbg(xotg->dev,
				"starting TA_BIDL_ADIS(155ms) timer\n");
			xotg->xotg_vars.b_bus_suspend = 0;
			xotg->xotg_timer_list.a_bidl_adis_tmout = 0;
			mod_timer(&xotg->xotg_timer_list.
				a_bidl_adis_tmr, jiffies +
				msecs_to_jiffies(TA_BIDL_ADIS));
		}
		if (state_changed && !xotg->xotg_timer_list.a_bidl_adis_tmout) {
			xotg_dbg(xotg->dev, "del_timer a_bidl_adis_tmr\n");
			del_timer_sync(&xotg->xotg_timer_list.a_bidl_adis_tmr);
		} else if (xotg->xotg_timer_list.a_bidl_adis_tmout) {
			xotg->xotg_timer_list.a_bidl_adis_tmout = 0;
		}
	}
	break;
	default:
		break;
	}
	spin_unlock_irqrestore(&xotg->lock, flags);
}

static int xotg_notify_connect(struct usb_phy *phy, enum usb_device_speed speed)
{
	struct xotg *xotg = container_of(phy, struct xotg, phy);
	unsigned long flags;

	xotg_dbg(xotg->dev, "speed = 0x%d\n", speed);
	spin_lock_irqsave(&xotg->lock, flags);
	if (speed != USB_SPEED_UNKNOWN)
		xotg->device_connected = true;
	if (xotg->phy.otg->default_a) {
		/* A-host has detected B-peripheral's connection
		 * (notified by HCD) or
		 * A-peripheral has detected B-Host's connection
		 * (notified by PCD)
		 */
		xotg_dbg(xotg->dev, "b_conn=1\n");
		xotg->xotg_vars.b_conn = 1;
	} else {
		/* B-peripheral has detected A-host's connection
		 * (notified by PCD) or
		 * B-Host has detected A-peripheral's connection
		 * (notified by HCD)
		 */
		xotg_dbg(xotg->dev, "a_conn=1\n");
		xotg->xotg_vars.a_conn = 1;
	}
	spin_unlock_irqrestore(&xotg->lock, flags);

	queue_work(xotg->otg_wq, &xotg->otg_work);
	return 0;
}

static int xotg_set_suspend(struct usb_phy *phy, int suspend)
{
	struct xotg *xotg = container_of(phy, struct xotg, phy);
	unsigned long flags;

	xotg_dbg(xotg->dev, "suspend = %d, state=%s\n", suspend,
		usb_otg_state_string(phy->state));

	spin_lock_irqsave(&xotg->lock, flags);
	if (suspend) {
		switch (phy->state) {
		case OTG_STATE_B_PERIPHERAL:
		case OTG_STATE_A_PERIPHERAL:
			xotg_dbg(xotg->dev, "B_PERIPHERAL set b_bus_susp\n");
			xotg->xotg_vars.b_bus_suspend = 1;
			break;
		case OTG_STATE_A_HOST:
			xotg_dbg(xotg->dev, "A_HOST\n");
			if (xotg->phy.otg->default_a) {
				xotg->xotg_reqs.a_bus_req = 0;
				xotg->xotg_vars.a_bus_suspend = 1;
				xotg->xotg_vars.a_bus_resume = 0;
				xotg_dbg(xotg->dev, "default_a=1 [add]\n");
			} else {
				xotg_dbg(xotg->dev, "default_a=0 [delete]\n");
				xotg->xotg_vars.a_bus_suspend = 1;
				xotg->xotg_vars.a_bus_resume = 0;
			}
			spin_unlock_irqrestore(&xotg->lock, flags);
			xotg_work(&xotg->otg_work);
			return 0;
		default:
			xotg_warn(xotg->dev, "suspend: default.\n");
			break;
		}
	} else {
		switch (phy->state) {
		default:
			xotg_warn(xotg->dev, "resume: default.\n");
			break;
		}
	}
	spin_unlock_irqrestore(&xotg->lock, flags);
	queue_work(xotg->otg_wq, &xotg->otg_work);
	return 0;
}

static int xotg_set_vbus(struct usb_phy *phy, int on)
{
	struct xotg *xotg = container_of(phy, struct xotg, phy);
	unsigned long flags;

	spin_lock_irqsave(&xotg->lock, flags);
	if (on) {
		tegra_usb_pad_reg_update(XUSB_PADCTL_USB2_VBUS_ID_0,
			USB2_VBUS_ID_0_ID_OVERRIDE,
			USB2_VBUS_ID_0_ID_OVERRIDE_RID_GND);

		/* pad protection for host mode */
		xusb_enable_pad_protection(0);

		/* set PP */
		xotg_notify_event(xotg, USB_EVENT_HANDLE_OTG_PP);
		xotg->phy.otg->default_a = 1;
		xotg->id = 0;
		queue_work(xotg->otg_wq, &xotg->otg_work);
	} else {
		/* If the device is an OTG test device like PET, then
		 * always move the state machine without waiting for the
		 * disconnect as the PET can just remove the ID with out
		 * disconnecting D+
		 */
		if (xotg->xotg_vars.otg_test_device_enumerated ||
		   !xotg->device_connected) {
			tegra_usb_pad_reg_update(XUSB_PADCTL_USB2_VBUS_ID_0,
				USB2_VBUS_ID_0_ID_OVERRIDE,
				USB2_VBUS_ID_0_ID_OVERRIDE_RID_FLOAT);
			/* pad protection for device mode */
			xusb_enable_pad_protection(1);
			xotg->phy.otg->default_a = 0;
			xotg->id = 1;
			queue_work(xotg->otg_wq, &xotg->otg_work);
		}
	}
	spin_unlock_irqrestore(&xotg->lock, flags);

	return 0;
}

static int xotg_notify_disconnect(struct usb_phy *phy,
		enum usb_device_speed speed)
{
	struct xotg *xotg = container_of(phy, struct xotg, phy);
	unsigned long flags;

	if (speed != USB_SPEED_UNKNOWN)
		return 0;

	xotg_dbg(xotg->dev, "speed = 0x%d\n", speed);
	spin_lock_irqsave(&xotg->lock, flags);
	xotg->device_connected = false;
	if (xotg->phy.otg->default_a) {
		/* A-host has detected B-peripheral's
		 * disconnection (notified by HCD) or
		 * A-peripheral has detected B-Host's
		 * disconnection (notified by PCD)
		 */
		xotg_dbg(xotg->dev, "b_conn=0\n");
		xotg->xotg_vars.b_conn = 0;
	} else {
		/* B-peripheral has detected A-host's
		 * disconnection (notified by PCD) or
		 * B-Host has detected A-peripheral's
		 * disconnection (notified by HCD)
		 */
		xotg_dbg(xotg->dev, "a_conn=0\n");
		xotg->xotg_vars.a_conn = 0;
	}
	if (!xotg->id_grounded) {
		spin_unlock_irqrestore(&xotg->lock, flags);
		xotg_set_vbus(phy, 0);
	} else {
		spin_unlock_irqrestore(&xotg->lock, flags);
		queue_work(xotg->otg_wq, &xotg->otg_work);
	}

	return 0;
}

static int xotg_notify_otg_test_device(struct usb_phy *phy)
{
	struct xotg *xotg = container_of(phy, struct xotg, phy);

	xotg->xotg_vars.otg_test_device_enumerated = 1;
	xotg->xotg_vars.start_tst_maint_timer = 1;
	xotg_work(&xotg->otg_work);
	return 0;
}

/* will be called from the PCD */
static int xotg_set_peripheral(struct usb_otg *otg, struct usb_gadget *gadget)
{
	struct xotg *xotg;

	if (!otg || !gadget)
		return -1;

	xotg = container_of(otg->phy, struct xotg, phy);
	xotg_info(xotg->dev, "usb_gadget = 0x%p\n", gadget);

	otg->gadget = gadget;
	xotg->xotg_reqs.b_bus_req = 1;

	return 0;
}

static int xotg_set_power(struct usb_phy *phy, unsigned ma)
{
	return 0;
}

static void xotg_get_otg_port_num(struct xotg *xotg)
{
	struct device_node *node = NULL;
	struct device_node *padctl = NULL;
	int otg_portmap, ret, port;

	node = xotg->dev->of_node;
	if (node != NULL) {
		padctl = of_parse_phandle(node, "nvidia,common_padctl", 0);
		ret = of_property_read_u32(padctl, "nvidia,otg_portmap",
							&otg_portmap);
		if (ret < 0) {
			xotg_err(xotg->dev,
				"Fail to get otg_portmap, ret (%d)\n", ret);
		} else {
			/* otg_portmap - bit-field indicating which OTG ports
			 * are controlled by XUDC/XHCI
			 * bit[0-7]  : SS ports
			 * bit[8-15] : HS ports
			 * T210 has only 4 ports [0-3] for SS and [8-11] for HS.
			 * We will search for all possible 8 positions incase
			 * future chips have more than 4 ports.
			 */
			for (port = 0; port < MAX_USB_PORTS; port++) {
				if (BIT(port) & otg_portmap) {
					xotg->ss_otg_port = port;
					break;
				}
			}

			for (port = 0; port < MAX_USB_PORTS; port++) {
				if (BIT(XUSB_UTMI_INDEX + port) & otg_portmap) {
					xotg->hs_otg_port = port;
					break;
				}
			}
		}
	}
}

static void xotg_struct_init(struct xotg *xotg)
{
	/* initialize the spinlock */
	spin_lock_init(&xotg->lock);
	spin_lock_init(&xotg->vbus_lock);

	xotg->phy.label = "Nvidia USB XOTG PHY";
	xotg->phy.set_power = xotg_set_power;
	xotg->phy.set_suspend = xotg_set_suspend;
	xotg->phy.set_vbus = xotg_set_vbus;
	xotg->phy.notify_connect = xotg_notify_connect;
	xotg->phy.notify_disconnect = xotg_notify_disconnect;
	xotg->phy.notify_otg_test_device = xotg_notify_otg_test_device;

	xotg->phy.otg->phy = &xotg->phy;
	xotg->phy.otg->set_host = xotg_set_host;
	xotg->phy.otg->set_xhci_host = xotg_set_xhci_host;
	xotg->phy.otg->set_peripheral = xotg_set_peripheral;
	xotg->phy.otg->start_hnp = xotg_start_hnp;
	xotg->phy.otg->start_srp = xotg_start_srp;

	/* initial states */
	xotg_dbg(xotg->dev, "UNDEFINED\n");
	xotg->phy.state = OTG_STATE_UNDEFINED;

	xotg->device_connected = false;

	/* app reqs to -1 */
	memset(&xotg->xotg_reqs, -1, sizeof(struct xotg_app_request));

	/* also, set the a_bus_drop and a_bus_req to default values so that
	 * if uA cable is connected, A-device can transition from A_IDLE to
	 * A_WAIT_VRISE.
	 */
	xotg->xotg_reqs.a_bus_drop = 0;
	xotg->xotg_reqs.a_bus_req = 1;

	/* nv vars to -1 */
	memset(&xotg->xotg_vars, 0, sizeof(struct xotg_vars));

	/* set default_a to 0 */
	xotg->phy.otg->default_a = 0;
}

/* start the OTG controller */
static int xotg_start(struct xotg *xotg)
{
	u32 reg;

	/* enable interrupts */
	reg = tegra_usb_pad_reg_read(XUSB_PADCTL_USB2_VBUS_ID_0);
	reg |= USB2_VBUS_ID_0_VBUS_VLD_CHG_INT_EN;
	reg |= USB2_VBUS_ID_0_VBUS_SESS_VLD_CHG_INT_EN;
	tegra_usb_pad_reg_write(XUSB_PADCTL_USB2_VBUS_ID_0, reg);

	/* TODO: vbus and ID interrupt can be enabled when needed */

	/* get the RID value from IDDIG/IDDIG_A/IDDIG_B/IDDIG_C bits */
	reg = tegra_usb_pad_reg_read(XUSB_PADCTL_USB2_VBUS_ID_0);
	reg &= USB2_VBUS_ID_0_RID_MASK;

	if (reg == USB2_VBUS_ID_0_RID_FLOAT
		|| reg == USB2_VBUS_ID_0_RID_B
		|| reg == USB2_VBUS_ID_0_RID_C) {
		xotg->id = 1;
	} else if (reg == USB2_VBUS_ID_0_RID_A
		|| reg == USB2_VBUS_ID_0_RID_GND) {
		/* micro-A end attached */
		xotg->id = 0;
	}
	xotg->phy.state = OTG_STATE_UNDEFINED;

	queue_work(xotg->otg_wq, &xotg->otg_work);
	return 0;
}

static irqreturn_t xotg_phy_wake_isr(int irq, void *data)
{
	struct xotg *xotg = (struct xotg *)data;

	xotg_dbg(xotg->dev, "irq %d", irq);
	return IRQ_HANDLED;
}

/* xotg interrupt handler */
static irqreturn_t xotg_irq(int irq, void *data)
{
	struct xotg *xotg = (struct xotg *)data;
	unsigned long flags;
	u32 vbus_id_reg, otgpad_ctl0, rid_status;

	spin_lock_irqsave(&xotg->lock, flags);

	/* handle the ID CHANGE interrupt */
	vbus_id_reg = tegra_usb_pad_reg_read(XUSB_PADCTL_USB2_VBUS_ID_0);

	xotg_dbg(xotg->dev, "vbus_id_reg: 0x%x\n", vbus_id_reg);

	/* handle the SRP detection logic */
	otgpad_ctl0 = tegra_usb_pad_reg_read(
			XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD_CTL0_0(
			xotg->hs_otg_port));

	xotg_dbg(xotg->dev, "otgpad_ctl0: 0x%x\n", otgpad_ctl0);
	if (!(vbus_id_reg & USB2_VBUS_ID_0_INTR_STS_CHG_MASK) &&
		!(otgpad_ctl0 & USB2_BATTERY_CHRG_OTGPAD_INTR_STS_CHG_MASK)) {
		xotg_dbg(xotg->dev, "padctl interrupt is not for xotg %x\n",
			vbus_id_reg);
		spin_unlock_irqrestore(&xotg->lock, flags);
		return IRQ_NONE;
	}

	/* int generated only when id value changes */
	if (vbus_id_reg & USB2_VBUS_ID_0_IDDIG_STS_CHG) {
		xotg_dbg(xotg->dev, "IDDIG STS CHG\n");
		/* get the RID value from the ID bits */
		rid_status = vbus_id_reg & USB2_VBUS_ID_0_RID_MASK;

		if (rid_status == USB2_VBUS_ID_0_RID_GND ||
			rid_status == USB2_VBUS_ID_0_RID_A) {
			xotg_dbg(xotg->dev, "A_IDLE\n");
			xotg->phy.otg->default_a = 1;
			xotg->id = 0;
		} else if (rid_status == USB2_VBUS_ID_0_RID_B ||
			rid_status == USB2_VBUS_ID_0_RID_FLOAT) {
			xotg_dbg(xotg->dev, "B_IDLE\n");
			xotg->phy.otg->default_a = 0;
			xotg->id = 1;
		}
	}

	if (vbus_id_reg & USB2_VBUS_ID_0_VBUS_SESS_VLD_STS_CHG) {
		xotg_dbg(xotg->dev, "VBUS SESS VLD STS CHG\n");
		if (vbus_id_reg & USB2_VBUS_ID_0_VBUS_SESS_VLD_STS)
			xotg->xotg_vars.b_sess_vld = 1;
		else
			xotg->xotg_vars.b_sess_vld = 0;
	}

	if (vbus_id_reg & USB2_VBUS_ID_0_VBUS_VLD_STS_CHG) {
		xotg_dbg(xotg->dev, "VBUS VLD STS CHG\n");
		if (vbus_id_reg & USB2_VBUS_ID_0_VBUS_VLD_STS)
			xotg->xotg_vars.a_vbus_vld = 1;
		else
			xotg->xotg_vars.a_vbus_vld = 0;
	}

	if (otgpad_ctl0 & USB2_BATTERY_CHRG_OTGPAD_SRP_DETECTED) {
		xotg_dbg(xotg->dev, "SRP DETECTED\n");
		xotg_enable_srp_detect(xotg, false);
		xotg->xotg_vars.a_srp_det = 1;
		xotg->xotg_reqs.a_bus_drop = 0;
		xotg->xotg_vars.a_bus_suspend = 0;
	}

	/* write to clear the status */
	tegra_usb_pad_reg_write(XUSB_PADCTL_USB2_VBUS_ID_0, vbus_id_reg);

	queue_work(xotg->otg_wq, &xotg->otg_work);
	spin_unlock_irqrestore(&xotg->lock, flags);
	return IRQ_HANDLED;
}

static int xotg_probe(struct platform_device *pdev)
{
	struct xotg *xotg;
	int status;
	struct resource *res;

	/* allocate space for nvidia XOTG device */
	xotg = kzalloc(sizeof(struct xotg), GFP_KERNEL);
	if (!xotg) {
		xotg_err(&pdev->dev, "memory alloc failed for xotg\n");
		return -ENOMEM;
	}

	/* allocate memory for usb_otg structure */
	xotg->phy.otg = kzalloc(sizeof(struct usb_otg), GFP_KERNEL);
	if (!xotg->phy.otg) {
		xotg_err(xotg->dev, "memory alloc failed for usb_otg\n");
		status = -ENOMEM;
		goto error1;
	}

	/* initialize the otg structure */
	xotg_struct_init(xotg);
	xotg->phy.dev = &pdev->dev;
	xotg->dev = &pdev->dev;
	xotg->pdev = pdev;
	platform_set_drvdata(pdev, xotg);
	xotg->phy.type = USB_PHY_TYPE_UNDEFINED;
	dev_set_drvdata(&pdev->dev, xotg);
	xotg_get_otg_port_num(xotg);
	/* store the otg phy */
	status = usb_add_phy(&xotg->phy, USB_PHY_TYPE_USB3);
	if (status) {
		xotg_err(xotg->dev, "usb_add_phy failed\n");
		goto error2;
	}

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		xotg_err(xotg->dev, "irq resource 0 doesn't exist\n");
		goto error3;
	}
	xotg->nv_irq = res->start;

	/* register shared padctl IRQ for otg */
	status = devm_request_irq(&pdev->dev, xotg->nv_irq, xotg_irq,
			IRQF_SHARED | IRQF_TRIGGER_HIGH,
			"tegra_xotg_irq", xotg);
	if (status != 0) {
		xotg_err(xotg->dev, "padctl irq register fail %d\n", status);
		goto error3;
	}

	/* register phy wake isr */
	res = platform_get_resource(pdev, IORESOURCE_IRQ, 1);
	if (!res) {
		xotg_err(xotg->dev, "irq resource 1 doesn't exist\n");
		goto error4;
	}
	xotg->usb_irq = res->start;

	status = devm_request_irq(&pdev->dev, xotg->usb_irq, xotg_phy_wake_isr,
			IRQF_SHARED | IRQF_TRIGGER_HIGH,
			"xotg_phy_wake_isr", xotg);
	if (status != 0) {
		xotg_err(xotg->dev, "usb_irq register fail %d\n", status);
		goto error4;
	}

	xotg->test_timer_timeout = 60000;
	xotg->vbus_on = false;
	xotg->vbus_enabled = false;
	INIT_WORK(&xotg->vbus_work, nv_vbus_work);

	/* workqueue to handle the state transitions of A-device/B-device
	 * as defined in the OTG spec.
	 */
	xotg->otg_wq = alloc_workqueue("OTG WQ\n", WQ_HIGHPRI|WQ_UNBOUND, 0);
	if (!xotg->otg_wq) {
		xotg_err(xotg->dev, "Work Queue Allocation Failed\n");
		goto error5;
	}
	INIT_WORK(&xotg->otg_work, xotg_work);

	/* initialize timers */
	status = xotg_init_timers(xotg);
	if (status) {
		xotg_err(xotg->dev, "timer init failed\n");
		goto error6;
	}

	/* regulator for usb_vbus, to be moved to OTG driver */
	xotg->usb_vbus_reg = devm_regulator_get(&pdev->dev, "usb_vbus");
	if (IS_ERR_OR_NULL(xotg->usb_vbus_reg)) {
		xotg_err(xotg->dev, "usb_vbus regulator not found: %ld\n",
			PTR_ERR(xotg->usb_vbus_reg));
		goto error7;
	}

	/* start OTG */
	status = xotg_start(xotg);
	if (status) {
		xotg_err(xotg->dev, "xotg_start failed\n");
		goto error8;
	}

	/* extcon for id pin */
	xotg->id_extcon_dev =
		extcon_get_extcon_dev_by_cable(&pdev->dev, "id");
	if (IS_ERR(xotg->id_extcon_dev))
		status = -ENODEV;

	xotg->id_extcon_nb.notifier_call = extcon_id_notifications;
	extcon_register_notifier(xotg->id_extcon_dev,
						&xotg->id_extcon_nb);

	status = device_create_file(xotg->dev, &dev_attr_debug);
	if (status) {
		xotg_err(xotg->dev, "failed to device_create_file\n");
		goto error9;
	}

	return status;

error9:
	extcon_unregister_notifier(xotg->id_extcon_dev,
		&xotg->id_extcon_nb);
error8:
	devm_regulator_put(xotg->usb_vbus_reg);
error7:
	xotg_deinit_timers(xotg);
error6:
	destroy_workqueue(xotg->otg_wq);
error5:
	devm_free_irq(&pdev->dev, xotg->usb_irq, xotg);
error4:
	devm_free_irq(&pdev->dev, xotg->nv_irq, xotg);
error3:
	usb_remove_phy(&xotg->phy);
error2:
	kfree(xotg->phy.otg);
error1:
	kfree(xotg);
	return status;
}

static int __exit xotg_remove(struct platform_device *pdev)
{
	struct xotg *xotg = platform_get_drvdata(pdev);

	device_remove_file(xotg->dev, &dev_attr_debug);
	extcon_unregister_notifier(xotg->id_extcon_dev,
		&xotg->id_extcon_nb);

	device_remove_file(&pdev->dev, &dev_attr_debug);
	devm_regulator_put(xotg->usb_vbus_reg);
	xotg_deinit_timers(xotg);
	destroy_workqueue(xotg->otg_wq);
	devm_free_irq(&pdev->dev, xotg->nv_irq, xotg);
	devm_free_irq(&pdev->dev, xotg->usb_irq, xotg);
	usb_remove_phy(&xotg->phy);

	kfree(xotg->phy.otg);
	kfree(xotg);

	return 0;
}

static struct of_device_id tegra_xotg_of_match[] = {
	{.compatible = "nvidia,tegra210-xotg",},
	{},
};

static int xotg_resume_platform(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct xotg *xotg = platform_get_drvdata(pdev);
	u32 reg;
	unsigned long flags;

	xotg_dbg(xotg->dev, "state: %s\n",
		usb_otg_state_string(xotg->phy.state));

	disable_irq_wake(xotg->usb_irq);
	spin_lock_irqsave(&xotg->lock, flags);
	/* restore USB2_ID register except when xudc driver updated it
	 * due to lp0 wake by connecting device mode cable
	 */
	reg = tegra_usb_pad_reg_read(XUSB_PADCTL_USB2_VBUS_ID_0);
	if ((reg & USB2_VBUS_ID_0_ID_OVERRIDE_RID_FLOAT) &&
		(reg & USB2_VBUS_ID_0_VBUS_OVERRIDE)) {
		reg |= USB2_VBUS_ID_0_VBUS_VLD_CHG_INT_EN;
		reg |= USB2_VBUS_ID_0_VBUS_SESS_VLD_CHG_INT_EN;
		tegra_usb_pad_reg_write(XUSB_PADCTL_USB2_VBUS_ID_0, reg);
	} else {
		/* if lp0 wake is by host otg cabel disconnect then
		 * make sure not to overrite ID_OVERRIDE it.
		 */
		if (((xotg->usb2_id & USB2_VBUS_ID_0_ID_OVERRIDE) ==
			USB2_VBUS_ID_0_ID_OVERRIDE_RID_GND) &&
			!xotg->id_grounded) {
			xotg_warn(xotg->dev, "restore without ID_GND\n");
			xotg->usb2_id &= ~USB2_VBUS_ID_0_ID_OVERRIDE;
			xotg->usb2_id |= USB2_VBUS_ID_0_ID_OVERRIDE_RID_FLOAT;
		}
		tegra_usb_pad_reg_write(XUSB_PADCTL_USB2_VBUS_ID_0,
			xotg->usb2_id);
	}
	spin_unlock_irqrestore(&xotg->lock, flags);
	return 0;
}

static int xotg_suspend_platform(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct xotg *xotg = platform_get_drvdata(pdev);
	int ret = 0;
	unsigned long flags;

	xotg_dbg(xotg->dev, "%s\n", __func__);

	spin_lock_irqsave(&xotg->lock, flags);
	/* save USB2_ID register */
	xotg->usb2_id = tegra_usb_pad_reg_read(XUSB_PADCTL_USB2_VBUS_ID_0);
	spin_unlock_irqrestore(&xotg->lock, flags);

	/* enable_irq_wake for otg port wakes */
	ret = enable_irq_wake(xotg->usb_irq);
	if (ret < 0) {
		xotg_err(xotg->dev,
		"Couldn't enable otg port wakeup, irq=%d, error=%d\n",
		xotg->usb_irq, ret);
	}

	return ret;
}

static const struct dev_pm_ops tegra_xotg_pm_ops = {
	.suspend = xotg_suspend_platform,
	.resume = xotg_resume_platform,
};

static struct platform_driver xotg_driver = {
	.probe = xotg_probe,
	.remove = __exit_p(xotg_remove),
	.driver = {
		.name = driver_name,
		.owner = THIS_MODULE,
		.of_match_table = tegra_xotg_of_match,
		.pm = &tegra_xotg_pm_ops,
	},
};

static int __init xotg_init(void)
{
	return platform_driver_register(&xotg_driver);
}
fs_initcall(xotg_init);

static void __exit xotg_exit(void)
{
	platform_driver_unregister(&xotg_driver);
}
module_exit(xotg_exit);

MODULE_DESCRIPTION("Nvidia USB XOTG Driver");
MODULE_VERSION("Rev. 1.00");
MODULE_AUTHOR("Ajay Gupta");
MODULE_LICENSE("GPL");
