/*
* nvxxx_udc.c - Nvidia device mode implementation
*
* Copyright (c) 2013-2016, NVIDIA CORPORATION.  All rights reserved.
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

#define DEBUG
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/dma-mapping.h>
#include <linux/dmapool.h>
#include <linux/errno.h>
#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/usb/otg.h>
#include <linux/usb/hcd.h>
#include <linux/usb/tegra_usb_charger.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/version.h>
#include <linux/tegra-powergate.h>
#include <linux/regulator/consumer.h>
#include <linux/clk.h>
#include <linux/clk/tegra.h>
#include <linux/tegra-fuse.h>
#include <linux/extcon.h>
#if defined(CONFIG_ARCH_TEGRA_21x_SOC)
#include <mach/tegra_usb_pad_ctrl.h>
#endif
#include <linux/tegra_pm_domains.h>
#include <linux/wakelock.h>
#include "nvxxx.h"
#include "../../../arch/arm/mach-tegra/iomap.h"
#include <linux/tegra_prod.h>
#include <linux/phy/phy.h>
#include <soc/tegra/xusb.h>
#include <dt-bindings/usb/tegra_xhci.h>

static const char driver_name[] = "tegra-xudc";
static struct nv_udc_s *the_controller;

static void nvudc_remove_pci(struct pci_dev *pdev);

static int nvudc_gadget_start(struct usb_gadget *,
		struct usb_gadget_driver *);
static int nvudc_gadget_stop(struct usb_gadget *,
		struct usb_gadget_driver *);

static void build_ep0_status(struct nv_udc_ep *udc_ep_ptr, bool default_value,
		u32 status, struct nv_udc_request *udc_req_ptr);

static int ep_halt(struct nv_udc_ep *udc_ep_ptr, int halt);
static void setup_status_trb(struct nv_udc_s *nvudc,
	struct transfer_trb_s *p_trb, struct usb_request *usb_req, u8 pcs);

static void nvudc_handle_setup_pkt(struct nv_udc_s *nvudc,
		struct usb_ctrlrequest *setup_pkt, u16 seq_num);
#define NUM_EP_CX	32

#define TRB_MAX_BUFFER_SIZE		65536
#define NVUDC_CONTROL_EP_TD_RING_SIZE	8
#define NVUDC_BULK_EP_TD_RING_SIZE	64
#define NVUDC_ISOC_EP_TD_RING_SIZE	8
#define NVUDC_INT_EP_TD_RING_SIZE	8
#define EVENT_RING_SIZE			4096

#define PORTSC_MASK     0xFF15FFFF
#define PORTREGSEL_MASK	0xFFFFFFFC
#define POLL_TBRST_MAX_VAL	0xB0
#define PING_TBRST_VAL	0x6
#define LMPITP_TIMER_VAL	0x978

#define ENABLE_TM 0x10
#define ZIP	BIT(18)
#define ZIN	BIT(22)

static struct of_device_id tegra_xusba_pd[] = {
	{ .compatible = "nvidia,tegra186-xusba-pd", },
	{ .compatible = "nvidia,tegra210-xusba-pd", },
	{ .compatible = "nvidia,tegra132-xusba-pd", },
	{},
};

static struct of_device_id tegra_xusbb_pd[] = {
	{ .compatible = "nvidia,tegra186-xusbb-pd", },
	{ .compatible = "nvidia,tegra210-xusbb-pd", },
	{ .compatible = "nvidia,tegra132-xusbb-pd", },
	{},
};

static struct usb_endpoint_descriptor nvudc_ep0_desc = {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = 0,
	.bmAttributes = USB_ENDPOINT_XFER_CONTROL,
	.wMaxPacketSize = cpu_to_le16(64),
};

int debug_level = LEVEL_INFO;
module_param(debug_level, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(debug_level, "level 0~4");

static void usbep_struct_setup(struct nv_udc_s *nvudc, u32 index, u8 *name);

/* WAR: Disable u1 to improve link stability */
/* Please enable u1 for compliance tests */
static bool u1_enable;
module_param(u1_enable, bool, S_IRUGO|S_IWUSR);

static bool u2_enable = true;
module_param(u2_enable, bool, S_IRUGO|S_IWUSR);

/* Used as a WAR to disable LPM for HS and FS */
static bool disable_lpm;
module_param(disable_lpm, bool, S_IRUGO|S_IWUSR);

static bool war_poll_trbst_max = true;
module_param(war_poll_trbst_max, bool, S_IRUGO|S_IWUSR);

static bool enable_fe_infinite_ss_retry = true;
module_param(enable_fe_infinite_ss_retry, bool, S_IRUGO|S_IWUSR);

#ifdef PRIME_NOT_RCVD_WAR
/* With some hosts, after they reject the stream, they are not sending the
 * PRIME packet for the Device to know when to restart a stream. In that case
 * we retry the following count times before stopping and waiting for PRIME
 */
static unsigned int  max_stream_rejected_retry_count = 20;

/* When the stream is rejected by the Host, we wait for this much
 * time(in milliseconds) before we retry sending an ERDY. so in total
 * we retry for 400ms( 20 count x 20ms sleep each time) for the Host to
 * respond and then stop retrying and wait for the PRIME from Host
 */
static unsigned int stream_rejected_sleep_msecs = 20;
module_param(max_stream_rejected_retry_count, uint, S_IRUGO|S_IWUSR);
module_param(stream_rejected_sleep_msecs, uint, S_IRUGO|S_IWUSR);
#endif

#ifdef DEBUG
#define reg_dump(_dev, _base, _reg) \
	msg_dbg(_dev, "%s @%x = 0x%x\n", #_reg, _reg, ioread32(_base + _reg));
#else
#define reg_dump(_dev, _base, _reg)	do {} while (0)
#endif

struct xusb_usb2_otg_pad_config {
	u32 hs_curr_level;
	u32 hs_squelch_level;
	u32 term_range_adj;
	u32 hs_iref_cap;
	u32 hs_slew;
	u32 ls_rslew;
};

struct xudc_chip_id {
	char *name;
	u16 device_id;
};

struct tegra_xudc_soc_data {
	u16 device_id;
	const char * const *supply_names;
	unsigned int num_supplies;
};

#define DEFAULT_MIN_IRQ_INTERVAL	(1000) /* 1 ms */
/* disable interrupt moderation to boost the performance on silicon bringup */
static unsigned int min_irq_interval_us;
module_param(min_irq_interval_us, uint, S_IRUGO);
MODULE_PARM_DESC(min_irq_interval_us, "minimum irq interval in microseconds");

#ifdef CONFIG_TEGRA_GADGET_BOOST_CPU_FREQ
static unsigned int boost_cpu_freq = CONFIG_TEGRA_GADGET_BOOST_CPU_FREQ;
module_param(boost_cpu_freq, uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(boost_cpu_freq, "CPU frequency (in KHz) to boost");

#define BOOST_PERIOD		(msecs_to_jiffies(2*1000)) /* 2 seconds */
#define BOOST_TRIGGER_SIZE	(4096)
static void tegra_xudc_boost_cpu_freq_fn(struct work_struct *work)
{
	struct nv_udc_s *nvudc =
			container_of(work, struct nv_udc_s, boost_cpufreq_work);
	unsigned long delay = BOOST_PERIOD;
	s32 cpufreq_hz = boost_cpu_freq * 1000;

	mutex_lock(&nvudc->boost_cpufreq_lock);

	if (!nvudc->cpufreq_boosted) {
		dev_dbg(nvudc->dev, "boost cpu freq %d Hz\n", cpufreq_hz);
		pm_qos_update_request(&nvudc->boost_cpufreq_req, cpufreq_hz);
		nvudc->cpufreq_boosted = true;
	}

	if (!nvudc->restore_cpufreq_scheduled) {
		dev_dbg(nvudc->dev, "%s schedule restore work\n", __func__);
		schedule_delayed_work(&nvudc->restore_cpufreq_work, delay);
		nvudc->restore_cpufreq_scheduled = true;
	}

	nvudc->cpufreq_last_boosted = jiffies;

	mutex_unlock(&nvudc->boost_cpufreq_lock);
}

static void tegra_xudc_restore_cpu_freq_fn(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct nv_udc_s *nvudc =
		container_of(dwork, struct nv_udc_s, restore_cpufreq_work);
	unsigned long delay = BOOST_PERIOD;

	mutex_lock(&nvudc->boost_cpufreq_lock);

	if (time_is_after_jiffies(nvudc->cpufreq_last_boosted + delay)) {
		dev_dbg(nvudc->dev, "%s schedule restore work\n", __func__);
		schedule_delayed_work(&nvudc->restore_cpufreq_work, delay);
		goto done;
	}

	dev_dbg(nvudc->dev, "%s restore cpufreq\n", __func__);
	pm_qos_update_request(&nvudc->boost_cpufreq_req, PM_QOS_DEFAULT_VALUE);
	nvudc->cpufreq_boosted = false;
	nvudc->restore_cpufreq_scheduled = false;

done:
	mutex_unlock(&nvudc->boost_cpufreq_lock);
}

static void tegra_xudc_boost_cpu_init(struct nv_udc_s *nvudc)
{
	INIT_WORK(&nvudc->boost_cpufreq_work, tegra_xudc_boost_cpu_freq_fn);

	INIT_DELAYED_WORK(&nvudc->restore_cpufreq_work,
				tegra_xudc_restore_cpu_freq_fn);

	pm_qos_add_request(&nvudc->boost_cpufreq_req,
				PM_QOS_CPU_FREQ_MIN, PM_QOS_DEFAULT_VALUE);

	mutex_init(&nvudc->boost_cpufreq_lock);
}

static void tegra_xudc_boost_cpu_deinit(struct nv_udc_s *nvudc)
{
	cancel_work_sync(&nvudc->boost_cpufreq_work);
	cancel_delayed_work_sync(&nvudc->restore_cpufreq_work);

	pm_qos_remove_request(&nvudc->boost_cpufreq_req);
	mutex_destroy(&nvudc->boost_cpufreq_lock);
}

static bool tegra_xudc_boost_cpu_freq(struct nv_udc_s *nvudc)
{
	return schedule_work(&nvudc->boost_cpufreq_work);
}
#else
#define BOOST_TRIGGER_SIZE	(UINT_MAX)
static void tegra_xudc_boost_cpu_init(struct nv_udc_s *unused) {}
static void tegra_xudc_boost_cpu_deinit(struct nv_udc_s *unused) {}
static void tegra_xudc_boost_cpu_freq(struct nv_udc_s *unused) {}
#endif

static int nvudc_ep_disable(struct usb_ep *_ep);

static void __iomem *car_base;
static void __iomem *padctl_base;
static void fpga_hack_init(struct platform_device *pdev)
{
#if defined(CONFIG_ARCH_TEGRA_21x_SOC)
	car_base = devm_ioremap(&pdev->dev, 0x60006000, 0x1000);
	padctl_base = devm_ioremap(&pdev->dev, 0x7009f000, 0x1000);
#elif defined(CONFIG_ARCH_TEGRA_18x_SOC)
	car_base = devm_ioremap(&pdev->dev, 0x05000000, 0x1000000);
	padctl_base = devm_ioremap(&pdev->dev, 0x03520000, 0x1000);
#endif
	if (!car_base)
		dev_err(&pdev->dev, "failed to map CAR mmio\n");
	if (!padctl_base)
		dev_err(&pdev->dev, "failed to map PADCTL mmio\n");
}

static void fpga_hack_setup_car(struct nv_udc_s *nvudc)
{
	struct platform_device *pdev = nvudc->pdev.plat;
	struct device *dev = &pdev->dev;
#if defined(CONFIG_ARCH_TEGRA_21x_SOC)
	struct clk *c = clk_get_sys(NULL, "xusb_padctl");
#endif
	u32 val;

	if (!car_base) {
		dev_err(&pdev->dev, "not able to access CAR mmio\n");
		return;
	}
#if defined(CONFIG_ARCH_TEGRA_21x_SOC)
#define RST_DEVICES_U_0	(0xc)
#define HOST_RST	25

	tegra_periph_reset_deassert(c);

	reg_dump(dev, car_base, RST_DEVICES_U_0);
	val = ioread32(car_base + RST_DEVICES_U_0);
	val &= ~((1 << HOST_RST));
	iowrite32(val, car_base + RST_DEVICES_U_0);
	reg_dump(dev, car_base, RST_DEVICES_U_0);

#elif defined(CONFIG_ARCH_TEGRA_18x_SOC)
#define CLK_RST_CONTROLLER_RST_DEV_XUSB_0	(0x470000)
#define   SWR_XUSB_HOST_RST			(1 << 0)
#define   SWR_XUSB_DEV_RST			(1 << 1)
#define   SWR_XUSB_PADCTL_RST			(1 << 2)
#define   SWR_XUSB_SS_RST			(1 << 3)

	val = ioread32(car_base + CLK_RST_CONTROLLER_RST_DEV_XUSB_0);
	val &= ~(SWR_XUSB_HOST_RST | SWR_XUSB_DEV_RST | SWR_XUSB_PADCTL_RST |
			SWR_XUSB_SS_RST);
	iowrite32(val, car_base + CLK_RST_CONTROLLER_RST_DEV_XUSB_0);
	reg_dump(dev, car_base, CLK_RST_CONTROLLER_RST_DEV_XUSB_0);
#endif
}

static void
fpga_hack_setup_vbus_sense_and_termination(struct nv_udc_s *nvudc)
{
	struct platform_device *pdev = nvudc->pdev.plat;
	struct device *dev = &pdev->dev;
	u32 reg;

	if (XUDC_IS_T210(nvudc)) {
		reg = 0x00211040;
		iowrite32(reg, IO_ADDRESS(0x7009f000 + XUSB_VBUS));

		reg = 0x00215040;
		iowrite32(reg, IO_ADDRESS(0x7009f000 + XUSB_VBUS));

		reg = 0x3080;
		iowrite32(reg, nvudc->base + TERMINATION_2);

		reg = 0x1012E;
		iowrite32(reg, nvudc->base + TERMINATION_1);

		reg = 0x1000;
		iowrite32(reg, nvudc->base + TERMINATION_1);
	} else if (XUDC_IS_T186(nvudc)) {
		#define USB2_VBUS_ID		(0x360)
		reg_dump(dev, padctl_base, USB2_VBUS_ID);
		reg = 0x00211040; /* clear VBUS_OVERRIDE (bit[14]) */
		iowrite32(reg, padctl_base + USB2_VBUS_ID);

		reg = 0x00215040; /* set VBUS_OVERRIDE (bit[14]) */
		iowrite32(reg, padctl_base + USB2_VBUS_ID);
	}
}

#if defined(CONFIG_ARCH_TEGRA_18x_SOC)
static void nvudc_plat_phy_power_off(struct nv_udc_s *nvudc);
static int nvudc_plat_phy_power_on(struct nv_udc_s *nvudc);
#endif

static void tegra_xudc_notify_event(struct nv_udc_s *nvudc)
{
	enum usb_phy_events event = USB_EVENT_NONE;

	switch (nvudc->connect_type) {
	case CONNECT_TYPE_SDP:
	case CONNECT_TYPE_CDP:
		event = USB_EVENT_VBUS;
		break;
	case CONNECT_TYPE_APPLE_1000MA:
	case CONNECT_TYPE_APPLE_2000MA:
	case CONNECT_TYPE_APPLE_500MA:
	case CONNECT_TYPE_DCP:
	case CONNECT_TYPE_DCP_MAXIM:
	case CONNECT_TYPE_DCP_QC2:
	case CONNECT_TYPE_NON_STANDARD_CHARGER:
		event = USB_EVENT_CHARGER;
		break;
	default:
		event = USB_EVENT_NONE;
	}

	if (!IS_ERR_OR_NULL(nvudc->phy)) {
		spin_lock(&nvudc->phy->sync_lock);
		nvudc->phy->last_event = event;
		atomic_notifier_call_chain(&nvudc->phy->notifier, event, NULL);
		spin_unlock(&nvudc->phy->sync_lock);
	}
}

static int nvudc_set_port_state(struct usb_gadget *gadget, u8 pls)
{
	struct nv_udc_s *nvudc = container_of(gadget, struct nv_udc_s, gadget);
	u32 u_temp;
	unsigned long flags;

	if (!nvudc) {
		msg_err(the_controller->dev, "nvudc is NULL\n");
		return 0;
	}

	spin_lock_irqsave(&nvudc->lock, flags);

	if (nvudc->elpg_is_processing) {
		spin_unlock_irqrestore(&nvudc->lock, flags);
		return -ESHUTDOWN;
	}
	msg_dbg(nvudc->dev, "pls: %d speed:%s\n", pls,
		(nvudc->gadget.speed == USB_SPEED_SUPER) ? "super" :
		(nvudc->gadget.speed == USB_SPEED_HIGH) ? "high" :
		(nvudc->gadget.speed == USB_SPEED_FULL) ? "full" : "unknown");

	u_temp = ioread32(nvudc->mmio_reg_base + PORTSC);
	u_temp &= PORTSC_MASK;
	u_temp &= ~PORTSC_PLS(~0);
	u_temp |= PORTSC_PLS(pls);
	u_temp |= PORTSC_LWS;
	iowrite32(u_temp, nvudc->mmio_reg_base + PORTSC);
	spin_unlock_irqrestore(&nvudc->lock, flags);
	return 0;
}

static const char tegra210_name[] = "NVIDIA Tegra210";
static const char tegra186_name[] = "NVIDIA Tegra186";
static const char unknown_chip_name[] = "Unknown";
static const char *xudc_get_chip_name(struct nv_udc_s *nvudc)
{
	if (XUDC_IS_T210(nvudc))
		return tegra210_name;
	else if (XUDC_IS_T186(nvudc))
		return tegra186_name;
	else
		return unknown_chip_name;
}

static void init_chip(struct nv_udc_s *nvudc)
{
	if (XUDC_IS_T210(nvudc)) {
		/*
		 * init T210 specific features here
		 */
		disable_lpm = true;
	} else if (XUDC_IS_T186(nvudc)) {
		/*
		 * init T186 specific features here
		 */
		disable_lpm = true;
		u1_enable = true;
	}
}

/* must hold nvudc->lock */
static inline void vbus_detected(struct nv_udc_s *nvudc)
{
#if defined(CONFIG_ARCH_TEGRA_18x_SOC)
	int ret;
#endif

	if (nvudc->vbus_detected)
		return; /* nothing to do */

	msg_info(nvudc->dev, "%s: vbus on detected\n", __func__);

#if defined(CONFIG_ARCH_TEGRA_21x_SOC)
	xusb_enable_pad_protection(1);
	tegra_usb_pad_reg_update(XUSB_PADCTL_USB2_OTG_PAD_CTL_0(nvudc->hs_port),
		USB2_OTG_PD, 0);
	tegra_usb_pad_reg_update(XUSB_PADCTL_USB2_OTG_PAD_CTL_1(nvudc->hs_port),
		USB2_OTG_PD_DR, 0);
	tegra_usb_pad_reg_update(XUSB_PADCTL_USB2_VBUS_ID_0,
		USB2_VBUS_ID_0_VBUS_OVERRIDE, USB2_VBUS_ID_0_VBUS_OVERRIDE);
#elif defined(CONFIG_ARCH_TEGRA_18x_SOC)
	tegra_phy_xusb_utmi_pad_power_on(nvudc->usb2_phy);
	ret = tegra_phy_xusb_set_vbus_override(nvudc->usb2_phy);
	if (ret)
		dev_err(nvudc->dev, "set vbus override failed %d\n", ret);
#endif

	nvudc->vbus_detected = true;
}

/* must hold nvudc->lock */
static inline void vbus_not_detected(struct nv_udc_s *nvudc)
{
	u32 portsc;
#if defined(CONFIG_ARCH_TEGRA_18x_SOC)
	int ret;
#endif

	if (!nvudc->vbus_detected)
		return; /* nothing to do */

	msg_info(nvudc->dev, "%s: vbus off detected\n", __func__);

#if defined(CONFIG_ARCH_TEGRA_21x_SOC)
	tegra_usb_pad_reg_update(XUSB_PADCTL_USB2_VBUS_ID_0,
		USB2_VBUS_ID_0_VBUS_OVERRIDE, 0);

	tegra_usb_pad_reg_update(XUSB_PADCTL_USB2_OTG_PAD_CTL_0(nvudc->hs_port),
		USB2_OTG_PD, USB2_OTG_PD);
	tegra_usb_pad_reg_update(XUSB_PADCTL_USB2_OTG_PAD_CTL_1(nvudc->hs_port),
		USB2_OTG_PD_DR, USB2_OTG_PD_DR);
#elif defined(CONFIG_ARCH_TEGRA_18x_SOC)
	ret = tegra_phy_xusb_clear_vbus_override(nvudc->usb2_phy);
	if (ret)
		dev_err(nvudc->dev, "clear vbus override failed %d\n", ret);

	tegra_phy_xusb_utmi_pad_power_down(nvudc->usb2_phy);
#endif

	portsc = ioread32(nvudc->mmio_reg_base + PORTSC);

	/*
	 * WAR for disconnect in U2 or RESUME state.
	 *
	 * In RESUME state, when LTSSM starts to send U3
	 * wakeup LFPS, LTSSM(HW) is not looking at VBUS till
	 * the U3 handshake completes successfully or times out.
	 * This timeout is 10ms, and when this happens, device
	 * could not enter ELPG untill 10ms later.
	 */
	if (XUDC_IS_T210(nvudc) &&
		nvudc->gadget.speed == USB_SPEED_SUPER &&
		((portsc & PORTSC_PLS_MASK) == XDEV_RESUME ||
		((portsc & PORTSC_PLS_MASK) == XDEV_U2))) {
		u32 reg;
		/* set FRWE */
		reg = ioread32(nvudc->mmio_reg_base + PORTPM);
		reg |= PORTPM_FRWE;
		iowrite32(reg, nvudc->mmio_reg_base + PORTPM);

		/* direct U0 */
		reg = ioread32(nvudc->mmio_reg_base + PORTSC);
		reg &= ~PORTSC_PLS_MASK;
		reg |= XDEV_U0;
		reg |= PORTSC_LWS;
		iowrite32(reg, nvudc->mmio_reg_base + PORTSC);

		reg = ioread32(nvudc->mmio_reg_base + PORTSC);
		msg_dbg(nvudc->dev,
		"Directing link to U0, PORTSC: %08x => %08x\n", portsc, reg);
	}
	nvudc->vbus_detected = false;
}

static void tegra_xudc_current_work(struct work_struct *work)
{
	struct nv_udc_s *nvudc =
		container_of(work, struct nv_udc_s, current_work);

	if (nvudc->ucd == NULL)
		return;

	tegra_ucd_set_sdp_cdp_current(nvudc->ucd, nvudc->current_ma);
}

static void tegra_xudc_ucd_work(struct work_struct *work)
{
	struct nv_udc_s *nvudc =
		container_of(work, struct nv_udc_s, ucd_work);
	bool vbus_connected = 0;
	unsigned long flags;

	msg_entry(nvudc->dev);

	vbus_connected =
		extcon_get_cable_state(nvudc->vbus_extcon_dev, "USB");
	if (vbus_connected == nvudc->vbus_detected) {
		msg_exit(nvudc->dev);
		spin_lock_irqsave(&nvudc->lock, flags);
		nvudc->extcon_event_processing = false;
		spin_unlock_irqrestore(&nvudc->lock, flags);
		return;
	}

	if (vbus_connected) {
		if (nvudc->ucd != NULL)
			nvudc->connect_type =
				tegra_ucd_detect_cable_and_set_current(
						nvudc->ucd);

		if (nvudc->connect_type == CONNECT_TYPE_SDP)
			schedule_delayed_work(&nvudc->non_std_charger_work,
				msecs_to_jiffies(NON_STD_CHARGER_DET_TIME_MS));

		pm_runtime_get_sync(nvudc->dev);
		spin_lock_irqsave(&nvudc->lock, flags);
		vbus_detected(nvudc);
		nvudc->extcon_event_processing = false;
		spin_unlock_irqrestore(&nvudc->lock, flags);
	} else {
		cancel_delayed_work(&nvudc->non_std_charger_work);
		nvudc->current_ma = 0;
		if (nvudc->ucd != NULL) {
			tegra_ucd_set_charger_type(nvudc->ucd,
						CONNECT_TYPE_NONE);
			nvudc->connect_type = CONNECT_TYPE_NONE;
		}

		spin_lock_irqsave(&nvudc->lock, flags);
		vbus_not_detected(nvudc);
		nvudc->extcon_event_processing = false;
		pm_runtime_put_autosuspend(nvudc->dev);
		spin_unlock_irqrestore(&nvudc->lock, flags);
	}

	if (!IS_ERR_OR_NULL(nvudc->phy))
		tegra_xudc_notify_event(nvudc);
	msg_exit(nvudc->dev);
}

static void tegra_xudc_non_std_charger_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct nv_udc_s *nvudc =
		container_of(dwork, struct nv_udc_s, non_std_charger_work);

	if (nvudc->ucd == NULL)
		return;

	tegra_ucd_set_charger_type(nvudc->ucd,
			CONNECT_TYPE_NON_STANDARD_CHARGER);
}

static void tegra_xudc_plc_reset_war_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct nv_udc_s *nvudc =
		container_of(dwork, struct nv_udc_s, plc_reset_war_work);
	unsigned long flags;
#if defined(CONFIG_ARCH_TEGRA_18x_SOC)
	struct phy *usb2_phy = nvudc->usb2_phy;
	int ret;
#endif

	dev_info(nvudc->dev, "plc_reset_war_work\n");
	spin_lock_irqsave(&nvudc->lock, flags);
	if (nvudc->wait_for_csc) {
		u32 pls = ioread32(nvudc->mmio_reg_base + PORTSC);
		pls &= PORTSC_PLS_MASK;

		if (pls == XDEV_INACTIVE) {
			dev_info(nvudc->dev, "toggle vbus\n");
#if defined(CONFIG_ARCH_TEGRA_21x_SOC)
			tegra_usb_pad_reg_update(XUSB_PADCTL_USB2_VBUS_ID_0,
				USB2_VBUS_ID_0_VBUS_OVERRIDE, 0);

			tegra_usb_pad_reg_update(XUSB_PADCTL_USB2_VBUS_ID_0,
				USB2_VBUS_ID_0_VBUS_OVERRIDE,
				USB2_VBUS_ID_0_VBUS_OVERRIDE);
#elif defined(CONFIG_ARCH_TEGRA_18x_SOC)
			dev_warn(nvudc->dev, "VBUS_OVERRIDE toggle\n");
			ret = tegra_phy_xusb_clear_vbus_override(usb2_phy);
			if (ret)
				dev_err(nvudc->dev, "clear vbus override failed\n");
			ret = tegra_phy_xusb_set_vbus_override(usb2_phy);
			if (ret)
				dev_err(nvudc->dev, "set vbus override failed\n");
#endif
			nvudc->wait_for_csc = 0;
		}
	}
	spin_unlock_irqrestore(&nvudc->lock, flags);
}

static void tegra_xudc_port_reset_war_work(struct work_struct *work)
{
#if defined(CONFIG_ARCH_TEGRA_21x_SOC)

	struct delayed_work *dwork = to_delayed_work(work);
	struct nv_udc_s *nvudc =
		container_of(dwork, struct nv_udc_s, port_reset_war_work);
	unsigned long flags;

	dev_info(nvudc->dev, "port_reset_war_work\n");
	spin_lock_irqsave(&nvudc->lock, flags);
	if (nvudc->vbus_detected && nvudc->wait_for_sec_prc) {
		u32 val;
		u32 pls = ioread32(nvudc->mmio_reg_base + PORTSC);
		pls &= PORTSC_PLS_MASK;
		dev_info(nvudc->dev, "pls = %x\n", pls);

		val = tegra_usb_pad_reg_read(
		XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD_CTL0(nvudc->hs_port));

		dev_info(nvudc->dev, "battery_chrg_otgpad_ctl0 = %x\n", val);
		if ((pls == XDEV_DISABLED) && ((val & ZIN) || (val & ZIP))) {
			dev_info(nvudc->dev, "toggle vbus\n");
			/* PRC doesn't complete in 100ms, toggle the vbus */
			tegra_usb_pad_reg_update(XUSB_PADCTL_USB2_VBUS_ID_0,
				USB2_VBUS_ID_0_VBUS_OVERRIDE, 0);

			tegra_usb_pad_reg_update(XUSB_PADCTL_USB2_VBUS_ID_0,
				USB2_VBUS_ID_0_VBUS_OVERRIDE,
				USB2_VBUS_ID_0_VBUS_OVERRIDE);
			nvudc->wait_for_sec_prc = 0;
		}
	}

	spin_unlock_irqrestore(&nvudc->lock, flags);
#endif
}

static int extcon_notifications(struct notifier_block *nb,
				   unsigned long event, void *unused)
{
	unsigned long flags;
	struct nv_udc_s *nvudc =
			container_of(nb, struct nv_udc_s, vbus_extcon_nb);

	if (tegra_platform_is_fpga())
		return NOTIFY_DONE;

	msg_entry(nvudc->dev);

	spin_lock_irqsave(&nvudc->lock, flags);
	if (nvudc->is_suspended) {
		spin_unlock_irqrestore(&nvudc->lock, flags);
		msg_info(nvudc->dev, "system is suspending, ignore event\n");
		goto out;
	}
	nvudc->extcon_event_processing = true;
	spin_unlock_irqrestore(&nvudc->lock, flags);

	schedule_work(&nvudc->ucd_work);

out:
	msg_exit(nvudc->dev);

	return NOTIFY_DONE;
}

static void set_interrupt_moderation(struct nv_udc_s *nvudc, unsigned us)
{
	u32 reg;
	u32 imodi = us * 4; /* 1 us = 4 x 250 ns */

	if (imodi > 0xFFFF) {
		msg_warn(nvudc->dev, "invalid interrupt interval %u us\n", us);
		imodi = DEFAULT_MIN_IRQ_INTERVAL * 4;
	}

	reg = ioread32(nvudc->mmio_reg_base + RT_IMOD);
	reg &= ~RT_IMOD_IMODI(-1);
	reg |= RT_IMOD_IMODI(imodi);
	/* HW won't overwrite IMODC. Set IMODC to the new value also */
	reg &= ~RT_IMOD_IMODC(-1);
	reg |= RT_IMOD_IMODC(imodi);
	iowrite32(reg, nvudc->mmio_reg_base + RT_IMOD);
}

static void setup_link_trb(struct transfer_trb_s *link_trb,
					bool toggle, dma_addr_t next_trb)
{
	u32 dw = 0;

	link_trb->data_buf_ptr_lo = cpu_to_le32(lower_32_bits(next_trb));
	link_trb->data_buf_ptr_hi = cpu_to_le32(upper_32_bits(next_trb));

	link_trb->trb_dword2 = 0;

	XHCI_SETF_VAR(TRB_TYPE, dw, TRB_TYPE_LINK);
	if (toggle)
		XHCI_SETF_VAR(TRB_LINK_TOGGLE_CYCLE, dw, 1);
	else
		XHCI_SETF_VAR(TRB_LINK_TOGGLE_CYCLE, dw, 0);

	link_trb->trb_dword3 = cpu_to_le32(dw);
}

static dma_addr_t tran_trb_virt_to_dma(struct nv_udc_ep *udc_ep,
	struct transfer_trb_s *trb)
{
	unsigned long offset;
	int trb_idx;
	dma_addr_t dma_addr = 0;

	trb_idx = trb - udc_ep->tran_ring_ptr;
	if (unlikely(trb_idx < 0))
		return 0;

	offset = trb_idx * sizeof(*trb);
	if (unlikely(offset > udc_ep->tran_ring_info.len))
		return 0;
	dma_addr = udc_ep->tran_ring_info.dma + offset;
	return dma_addr;
}

static struct transfer_trb_s *tran_trb_dma_to_virt(struct nv_udc_ep *udc_ep,
	u64 address)
{
	unsigned long offset;
	struct transfer_trb_s *trb_virt;
	struct nv_udc_s *nvudc = udc_ep->nvudc;

	if (lower_32_bits(address) & 0xf) {
		msg_err(nvudc->dev, "transfer ring dma address incorrect\n");
		return NULL;
	}

	offset = address - udc_ep->tran_ring_info.dma;
	if (unlikely(offset > udc_ep->tran_ring_info.len)) {
		msg_err(nvudc->dev, "dma address isn't within transfer ring\n");
		msg_err(nvudc->dev, "dma address 0x%llx\n", address);
		msg_err(nvudc->dev, "ring is 0x%llx ~ 0x%llx\n",
			(u64) udc_ep->tran_ring_info.dma,
			(u64) (udc_ep->tran_ring_info.dma +
			udc_ep->tran_ring_info.len));
		return NULL;
	}
	offset = offset / sizeof(struct transfer_trb_s);
	trb_virt = udc_ep->tran_ring_ptr + offset;
	return trb_virt;
}

static dma_addr_t event_trb_virt_to_dma(struct nv_udc_s *nvudc,
		struct event_trb_s *event)
{
	dma_addr_t dma_addr = 0;
	unsigned long seg_offset;
	struct event_trb_s *evt_seg0_first_trb;
	struct event_trb_s *evt_seg1_first_trb;

	if (!nvudc || !event)
		return 0;

	evt_seg0_first_trb = (struct event_trb_s *)nvudc->event_ring0.vaddr;
	evt_seg1_first_trb = (struct event_trb_s *)nvudc->event_ring1.vaddr;

	if ((event >= evt_seg0_first_trb) &&
		(event <= nvudc->evt_seg0_last_trb)) {
		seg_offset = event - evt_seg0_first_trb;
		dma_addr = nvudc->event_ring0.dma + seg_offset * sizeof(*event);
	} else if ((event >= evt_seg1_first_trb)
			&& (event <= nvudc->evt_seg1_last_trb)) {
		seg_offset = event - evt_seg1_first_trb;
		dma_addr = nvudc->event_ring1.dma + seg_offset * sizeof(*event);
	}

	return dma_addr;
}

static void nvudc_epcx_setup(struct nv_udc_ep *udc_ep)
{
	struct nv_udc_s *nvudc = udc_ep->nvudc;
	struct device *dev = nvudc->dev;
	const struct usb_endpoint_descriptor *desc = udc_ep->desc;
	const struct usb_ss_ep_comp_descriptor *comp_desc = udc_ep->comp_desc;
	u8 DCI = udc_ep->DCI;
	struct ep_cx_s *epcx = (struct ep_cx_s *)nvudc->p_epcx + DCI;
	enum EP_TYPE_E ep_type;
	u16 esit_payload = 0;
	u16 maxburst = 0;
	u8 maxstreams = 0;
	u8 mult = 0;
	u16 maxsize;
	u32 dw;

	msg_dbg(dev, "nvudc->p_epcx %p, epcx %p\n", nvudc->p_epcx, epcx);
	msg_dbg(dev, "DCI %u, sizeof ep_cx %zu\n", DCI, sizeof(struct ep_cx_s));

	if (usb_endpoint_dir_out(desc))
		ep_type = usb_endpoint_type(desc);
	else
		ep_type = usb_endpoint_type(desc) + EP_TYPE_CNTRL;

	maxsize = usb_endpoint_maxp(desc) & 0x07ff; /* D[0:10] */

	if (nvudc->gadget.speed == USB_SPEED_SUPER) {
		maxburst = comp_desc->bMaxBurst;

		if (usb_endpoint_xfer_bulk(udc_ep->desc))
			maxstreams = comp_desc->bmAttributes & 0x1f;
		else if (usb_endpoint_xfer_isoc(udc_ep->desc))
			mult = comp_desc->bmAttributes & 0x3;

		if (usb_endpoint_xfer_int(udc_ep->desc) ||
					usb_endpoint_xfer_isoc(udc_ep->desc)) {
			esit_payload =
				le16_to_cpu(comp_desc->wBytesPerInterval);
		}
	} else if ((nvudc->gadget.speed == USB_SPEED_HIGH ||
		nvudc->gadget.speed == USB_SPEED_FULL) &&
			(usb_endpoint_xfer_int(udc_ep->desc) ||
				usb_endpoint_xfer_isoc(udc_ep->desc))) {
		if (nvudc->gadget.speed == USB_SPEED_HIGH)
			maxburst = (usb_endpoint_maxp(desc) >> 11) & 0x3;
		if (maxburst == 0x3) {
			msg_warn(dev, "invalid maxburst\n");
			maxburst = 0x2; /* really need ? */
		}
		esit_payload = (maxsize * (maxburst + 1));
	}

	/* fill ep_dw0 */
	dw = 0;
	XHCI_SETF_VAR(EP_CX_EP_STATE, dw, EP_STATE_RUNNING);
	XHCI_SETF_VAR(EP_CX_INTERVAL, dw, desc->bInterval);
	XHCI_SETF_VAR(EP_CX_MULT, dw, mult);
	if (maxstreams) {
		XHCI_SETF_VAR(EP_CX_MAX_PSTREAMS, dw, maxstreams);
		XHCI_SETF_VAR(EP_LINEAR_STRM_ARR, dw, 1);
	}
	epcx->ep_dw0 = cpu_to_le32(dw);

	/* fill ep_dw1 */
	dw = 0;
	XHCI_SETF_VAR(EP_CX_EP_TYPE, dw, ep_type);
	XHCI_SETF_VAR(EP_CX_CERR, dw, 3);
	XHCI_SETF_VAR(EP_CX_MAX_PACKET_SIZE, dw, maxsize);
	XHCI_SETF_VAR(EP_CX_MAX_BURST_SIZE, dw, maxburst);
	epcx->ep_dw1 = cpu_to_le32(dw);

	/* fill ep_dw2 */
	dw = lower_32_bits(udc_ep->tran_ring_info.dma) & EP_CX_TR_DQPT_LO_MASK;
	XHCI_SETF_VAR(EP_CX_DEQ_CYC_STATE, dw, udc_ep->pcs);
	epcx->ep_dw2 = cpu_to_le32(dw);

	/* fill ep_dw3 */
	dw = upper_32_bits(udc_ep->tran_ring_info.dma);
	epcx->ep_dw3 = cpu_to_le32(dw);

	/* fill ep_dw4 */
	dw = 0;
	/* Reasonable initial values of Average TRB Length for
	 * Control endpoints would be 8B,
	 * Interrupt endpoints would be 1KB,
	 * Bulk and Isoch endpoints would be 3KB.
	 * */
	XHCI_SETF_VAR(EP_CX_AVE_TRB_LEN, dw, 3000);
	XHCI_SETF_VAR(EP_CX_MAX_ESIT_PAYLOAD, dw, esit_payload);
	epcx->ep_dw4 = cpu_to_le32(dw);

	/* fill ep_dw5 */
	epcx->ep_dw5 = 0;

	/* fill ep_dw6 */
	dw = 0;
	XHCI_SETF_VAR(EP_CX_CERRCNT, dw, 3);
	epcx->ep_dw6 = cpu_to_le32(dw);
}

static int nvudc_ep_enable(struct usb_ep *ep,
		const struct usb_endpoint_descriptor *desc)
{
	struct nv_udc_ep *udc_ep;
	struct nv_udc_s *nvudc;
	struct device *dev;
	bool is_isoc_ep = false;
	u32 u_temp = 0;
	unsigned long flags;
	struct ep_cx_s *p_ep_cx;

	if  (!ep || !desc || (desc->bDescriptorType != USB_DT_ENDPOINT))
		return -EINVAL;

	udc_ep = container_of(ep, struct nv_udc_ep, usb_ep);
	nvudc = udc_ep->nvudc;

	if (!nvudc->driver)
		return -ESHUTDOWN;

	p_ep_cx = (struct ep_cx_s *) nvudc->p_epcx + udc_ep->DCI;
	if (XHCI_GETF(EP_CX_EP_STATE, p_ep_cx->ep_dw0) != EP_STATE_DISABLED)
		nvudc_ep_disable(ep);

	udc_ep->desc = desc;
	udc_ep->comp_desc = ep->comp_desc;
	dev = nvudc->dev;
	msg_entry(dev);

	spin_lock_irqsave(&nvudc->lock, flags);

	if (nvudc->elpg_is_processing) {
		spin_unlock_irqrestore(&nvudc->lock, flags);
		msg_exit(dev);
		return -ESHUTDOWN;
	}
	/* setup endpoint context for regular endpoint
	   the endpoint context for control endpoint has been
	   setted up in probe function */
	if (udc_ep->DCI) {
		u32 ring_size = 0;
		size_t ring_len;

		msg_dbg(dev, "ep_enable udc_ep->DCI = %d\n", udc_ep->DCI);
		if (usb_endpoint_xfer_isoc(desc)) {
			nvudc->g_isoc_eps++;
			if (nvudc->g_isoc_eps > 4) {
				msg_err(dev, "too many isoc eps %d\n",
						nvudc->g_isoc_eps);
				nvudc->g_isoc_eps--;
				spin_unlock_irqrestore(&nvudc->lock, flags);
				msg_exit(dev);
				return -EBUSY;
			}
			is_isoc_ep = true;
		}

		if (usb_endpoint_xfer_bulk(desc))
			ring_size =  NVUDC_BULK_EP_TD_RING_SIZE;
		else if (usb_endpoint_xfer_isoc(desc))
			ring_size = NVUDC_ISOC_EP_TD_RING_SIZE;
		else if (usb_endpoint_xfer_int(desc))
			ring_size = NVUDC_INT_EP_TD_RING_SIZE;
		ring_len = ring_size * sizeof(struct transfer_trb_s);

		/* setup transfer ring */
		if (!udc_ep->tran_ring_info.vaddr ||
			ring_len != udc_ep->tran_ring_info.len) {
			dma_addr_t dma;
			void *vaddr;

			if (udc_ep->tran_ring_info.vaddr) {
				dma_free_coherent(nvudc->dev,
						udc_ep->tran_ring_info.len,
						udc_ep->tran_ring_info.vaddr,
						udc_ep->tran_ring_info.dma);
			}

			vaddr = dma_alloc_coherent(nvudc->dev, ring_len,
					&dma, GFP_ATOMIC);
			if (!vaddr) {
				spin_unlock_irqrestore(&nvudc->lock, flags);
				msg_err(dev, "failed to allocate trb ring\n");
				msg_exit(dev);
				return -ENOMEM;
			}

			udc_ep->tran_ring_info.vaddr = vaddr;
			udc_ep->tran_ring_info.dma = dma;
			udc_ep->tran_ring_info.len = ring_len;
			udc_ep->first_trb = vaddr;
			udc_ep->link_trb = udc_ep->first_trb + ring_size - 1;
		}
		memset(udc_ep->first_trb, 0, udc_ep->tran_ring_info.len);

		setup_link_trb(udc_ep->link_trb, true,
					udc_ep->tran_ring_info.dma);

		if (usb_endpoint_xfer_bulk(desc))
			nvudc->act_bulk_ep |= BIT(udc_ep->DCI);

		udc_ep->enq_pt = udc_ep->first_trb;
		udc_ep->deq_pt = udc_ep->first_trb;
		udc_ep->pcs = 1;
		udc_ep->tran_ring_full = false;
		nvudc->num_enabled_eps++;
		nvudc_epcx_setup(udc_ep);
	}

	msg_dbg(dev, "num_enabled_eps = %d\n", nvudc->num_enabled_eps);

	if (nvudc->device_state == USB_STATE_ADDRESS) {
		u32 reg;
		reg = ioread32(nvudc->mmio_reg_base + CTRL);
		reg |= CTRL_RUN;
		iowrite32(reg, nvudc->mmio_reg_base + CTRL);

		msg_dbg(dev, "set run bit\n");

		nvudc->device_state = USB_STATE_CONFIGURED;
	}

	if (is_isoc_ep) {
		u32 u_temp;
		msg_dbg(dev, "is_isoc_ep is TRUE\n");

		/* pause all the active bulk endpoint */
		u_temp = ioread32(nvudc->mmio_reg_base + EP_PAUSE);
		u_temp |= nvudc->act_bulk_ep;

		iowrite32(u_temp, nvudc->mmio_reg_base + EP_PAUSE);

		poll_stchg(dev, "Pausing bulk eps", nvudc->act_bulk_ep);

		iowrite32(nvudc->act_bulk_ep, nvudc->mmio_reg_base + EP_STCHG);

		u_temp &= ~nvudc->act_bulk_ep;
		iowrite32(u_temp, nvudc->mmio_reg_base + EP_PAUSE);
		poll_stchg(dev, "Pausing bulk eps", u_temp);

		iowrite32(u_temp, nvudc->mmio_reg_base + EP_STCHG);
	}

	iowrite32(NV_BIT(udc_ep->DCI), nvudc->mmio_reg_base + EP_RELOAD);
	msg_dbg(dev, "load endpoint context = 0x%x\n",
			ioread32(nvudc->mmio_reg_base + EP_RELOAD));

	poll_reload(dev, "ep_enable()", NV_BIT(udc_ep->DCI));

	/* unpause ep */
	u_temp = ioread32(nvudc->mmio_reg_base + EP_PAUSE);
	u_temp &= ~NV_BIT(udc_ep->DCI);
	iowrite32(u_temp, nvudc->mmio_reg_base + EP_PAUSE);

	poll_stchg(dev, "EP PAUSED", NV_BIT(udc_ep->DCI));

	iowrite32(NV_BIT(udc_ep->DCI), nvudc->mmio_reg_base + EP_STCHG);
	msg_dbg(dev, "Clear Pause bits set by HW when reload context=0x%x\n",
		ioread32(nvudc->mmio_reg_base + EP_PAUSE));

	/* if EP was halted, clear the halt bit */
	u_temp = ioread32(nvudc->mmio_reg_base + EP_HALT);
	if (u_temp & NV_BIT(udc_ep->DCI)) {
		msg_dbg(dev, "EP was halted u_temp = 0x%x\n", u_temp);
		u_temp &= ~NV_BIT(udc_ep->DCI);
		iowrite32(u_temp, nvudc->mmio_reg_base + EP_HALT);

		poll_stchg(nvudc->dev, "EP halted", NV_BIT(udc_ep->DCI));
		iowrite32(NV_BIT(udc_ep->DCI), nvudc->mmio_reg_base + EP_STCHG);

		msg_dbg(dev, "clear state change register\n");
	}

	spin_unlock_irqrestore(&nvudc->lock, flags);
	msg_exit(dev);
	return 0;
}

/* Completes request.  Calls gadget completion handler
 * caller must have acquired spin lock.
 */
static void req_done(struct nv_udc_ep *udc_ep,
			struct nv_udc_request *udc_req, int status)
{
	struct nv_udc_s *nvudc = udc_ep->nvudc;


	if (likely(udc_req->usb_req.status == -EINPROGRESS))
		udc_req->usb_req.status = status;

	list_del_init(&udc_req->queue);
	if (udc_req->mapped) {
		dma_unmap_single(nvudc->dev,
				udc_req->usb_req.dma, udc_req->usb_req.length,
				(udc_ep->desc->bEndpointAddress & USB_DIR_IN)
				? DMA_TO_DEVICE
				: DMA_FROM_DEVICE);
#define DMA_ADDR_INVALID    (~(dma_addr_t)0)
		udc_req->usb_req.dma = DMA_ADDR_INVALID;
		udc_req->mapped = 0;
	}

	if (udc_req->usb_req.complete) {
		spin_unlock(&nvudc->lock);
		udc_req->usb_req.complete(&udc_ep->usb_ep, &udc_req->usb_req);
		spin_lock(&nvudc->lock);
	}
	msg_exit(nvudc->dev);
}

static void nuke(struct nv_udc_ep *udc_ep, int status)
{
	struct nv_udc_request *req = NULL;
	while (!list_empty(&udc_ep->queue)) {

		req = list_entry(udc_ep->queue.next,
				struct nv_udc_request,
				queue);

		req_done(udc_ep, req, status);
	}

}

static int nvudc_ep_disable(struct usb_ep *_ep)
{
	struct nv_udc_ep *udc_ep;
	struct nv_udc_s *nvudc;
	struct ep_cx_s *p_ep_cx;
	unsigned long flags;
	u32 u_temp, u_temp2;
	uint	ep_state;

	if (!_ep)
		return -EINVAL;

	udc_ep = container_of(_ep, struct nv_udc_ep, usb_ep);
	nvudc = udc_ep->nvudc;
	msg_entry(nvudc->dev);

	p_ep_cx = (struct ep_cx_s *)nvudc->p_epcx + udc_ep->DCI;

	ep_state = XHCI_GETF(EP_CX_EP_STATE, p_ep_cx->ep_dw0);
	if (!ep_state) {
		/* get here if ep is already disabled */
		return -EINVAL;
	}

	spin_lock_irqsave(&nvudc->lock, flags);

	if (nvudc->elpg_is_processing) {
		spin_unlock_irqrestore(&nvudc->lock, flags);
		msg_exit(nvudc->dev);
		return -ESHUTDOWN;
	}

	msg_dbg(nvudc->dev, "EPDCI = 0x%x\n", udc_ep->DCI);

	/* HW will pause the endpoint first while reload ep,
	 * do not need to write Pause register */
	XHCI_SETF_VAR(EP_CX_EP_STATE, p_ep_cx->ep_dw0, EP_STATE_DISABLED);

	msg_dbg(nvudc->dev, "reload eps\n");
	iowrite32(NV_BIT(udc_ep->DCI), nvudc->mmio_reg_base + EP_RELOAD);

	poll_reload(nvudc->dev, "ep_disable()", NV_BIT(udc_ep->DCI));

	/* clean up the request queue */
	nuke(udc_ep, -ESHUTDOWN);

	/* decrement ep counters */
	nvudc->num_enabled_eps--;
	if (usb_endpoint_xfer_isoc(udc_ep->desc))
		nvudc->g_isoc_eps--;

	/* release all the memory allocate for the endpoint */
	/* dma_free_coherent(nvudc->dev, nvudc->event_ring0.buff_len,
	 * nvudc->event_ring0.virt_addr, nvudc->event_ring0.dma_addr); */

	udc_ep->desc = NULL;

	/* clean up the endpoint context */
	memset(p_ep_cx, 0, sizeof(struct ep_cx_s));

	/* clean up the hw registers which used to track ep states*/
	/* EP_PAUSE register */
	u_temp = ioread32(nvudc->mmio_reg_base + EP_PAUSE);
	u_temp2 = 1 << udc_ep->DCI;
	if (u_temp & u_temp2) {
		u_temp &= ~u_temp2;
		iowrite32(u_temp, nvudc->mmio_reg_base + EP_PAUSE);
		poll_stchg(nvudc->dev, "nvudc_ep_disable", u_temp2);
		iowrite32(u_temp2, nvudc->mmio_reg_base + EP_STCHG);
		msg_dbg(nvudc->dev, "clear EP_PAUSE register\n");
	}

	/* EP_HALT register */
	u_temp = ioread32(nvudc->mmio_reg_base + EP_HALT);
	u_temp2 = 1 << udc_ep->DCI;
	if (u_temp & u_temp2) {
		u_temp &= ~u_temp2;
		iowrite32(u_temp, nvudc->mmio_reg_base + EP_HALT);
		poll_stchg(nvudc->dev, "nvudc_ep_disable", u_temp2);
		iowrite32(u_temp2, nvudc->mmio_reg_base + EP_STCHG);
		msg_dbg(nvudc->dev, "clear EP_HALT register\n");
	}

	/* EP_STOPPED register */
	u_temp = ioread32(nvudc->mmio_reg_base + EP_STOPPED);
	u_temp2 = 1 << udc_ep->DCI;
	if (u_temp & u_temp2) {
		iowrite32(u_temp2, nvudc->mmio_reg_base + EP_STOPPED);
		msg_dbg(nvudc->dev, "clear EP_STOPPED register\n");
	}

	msg_dbg(nvudc->dev, "num_enabled_eps = %d\n", nvudc->num_enabled_eps);

	/* If device state was changed to default by port
	reset, should not overwrite it again */
	if ((nvudc->num_enabled_eps == 0) &&
		(nvudc->device_state == USB_STATE_CONFIGURED)) {
		msg_dbg(nvudc->dev, "Device State USB_STATE_CONFIGURED\n");
		msg_dbg(nvudc->dev, "Set Device State to addressed\n");
		nvudc->device_state = USB_STATE_ADDRESS;
		u_temp = ioread32(nvudc->mmio_reg_base + CTRL);
		u_temp &= ~CTRL_RUN;
		iowrite32(u_temp, nvudc->mmio_reg_base + CTRL);

		msg_dbg(nvudc->dev, "clear RUN bit = 0x%x\n", u_temp);

		/*        nvudc_power_gate(); */
	}
	spin_unlock_irqrestore(&nvudc->lock, flags);
	msg_exit(nvudc->dev);
	return 0;
}

static struct usb_request *
nvudc_alloc_request(struct usb_ep *_ep, gfp_t gfp_flags)
{
	struct nv_udc_ep *udc_ep;
	struct nv_udc_s *nvudc;
	struct nv_udc_request *udc_req_ptr = NULL;

	udc_ep = container_of(_ep, struct nv_udc_ep, usb_ep);
	nvudc = udc_ep->nvudc;
	msg_entry(nvudc->dev);

	udc_req_ptr = kzalloc(sizeof(struct nv_udc_request), gfp_flags);

	msg_dbg(nvudc->dev, "udc_req_ptr = 0x%p\n", udc_req_ptr);

	if (!udc_req_ptr)
		return NULL;

	udc_req_ptr->usb_req.dma = DMA_ADDR_INVALID;
	INIT_LIST_HEAD(&udc_req_ptr->queue);

	msg_exit(nvudc->dev);
	return &udc_req_ptr->usb_req;
}

static void nvudc_free_request(struct usb_ep *_ep, struct usb_request *_req)
{
	struct nv_udc_ep *udc_ep;
	struct nv_udc_s *nvudc;
	struct nv_udc_request *udc_req_ptr = NULL;

	udc_ep = container_of(_ep, struct nv_udc_ep, usb_ep);
	nvudc = udc_ep->nvudc;
	msg_entry(nvudc->dev);

	if (!_ep || !_req)
		return;

	udc_req_ptr = container_of(_req, struct nv_udc_request, usb_req);
	kfree(udc_req_ptr);
	msg_exit(nvudc->dev);
}

/* num_trbs here is the size of the ring. */
u32 room_on_ring(struct nv_udc_s *nvudc, u32 num_trbs,
		struct transfer_trb_s *p_ring, struct transfer_trb_s *enq_pt,
		struct transfer_trb_s *dq_pt)
{
	u32 i = 0;

	msg_dbg(nvudc->dev, "room_on_ring enq_pt = 0x%p, dq_pt = 0x%p",
			enq_pt, dq_pt);
	if (enq_pt == dq_pt) {
		/* ring is empty */
		return num_trbs - 1;
	}

	while (enq_pt != dq_pt) {
		i++;

		enq_pt++;

		if (XHCI_GETF(TRB_TYPE, enq_pt->trb_dword3) == TRB_TYPE_LINK)
			enq_pt = p_ring;

		if (i > num_trbs)
			break;
	}

	msg_dbg(nvudc->dev, "room_on_ring 0x%x\n", i);
	return i-1;
}

void setup_status_trb(struct nv_udc_s *nvudc, struct transfer_trb_s *p_trb,
		struct usb_request *usb_req, u8 pcs)
{

	u32 u_temp, dir = 0;

	/* There are some cases where seutp_status_trb() is called with
	 * usb_req set to NULL.*/
	if (usb_req != NULL) {
		p_trb->data_buf_ptr_lo = lower_32_bits(usb_req->dma);
		p_trb->data_buf_ptr_hi = upper_32_bits(usb_req->dma);
	}

	msg_dbg(nvudc->dev, "data_buf_ptr_lo = 0x%x, data_buf_ptr_hi = 0x%x\n",
		p_trb->data_buf_ptr_lo, p_trb->data_buf_ptr_hi);

	p_trb->trb_dword2 = 0;

	u_temp = 0;
	XHCI_SETF_VAR(TRB_CYCLE_BIT, u_temp, pcs);
	XHCI_SETF_VAR(TRB_INTR_ON_COMPLETION, u_temp, 1);
	XHCI_SETF_VAR(TRB_TYPE, u_temp, TRB_TYPE_XFER_STATUS_STAGE);

	if (nvudc->setup_status == STATUS_STAGE_XFER)
		dir = 1;

	XHCI_SETF_VAR(DATA_STAGE_TRB_DIR, u_temp, dir);

	p_trb->trb_dword3 = u_temp;
	msg_dbg(nvudc->dev, "trb_dword2 = 0x%x, trb_dword3 = 0x%x\n",
			p_trb->trb_dword2, p_trb->trb_dword3);

}

void setup_datastage_trb(struct nv_udc_s *nvudc, struct transfer_trb_s *p_trb,
		struct usb_request *usb_req, u8 pcs, u32 num_trb,
		u32 transfer_length, u32 td_size, u8 IOC, u8 dir)
{

	u32 u_temp;
	p_trb->data_buf_ptr_lo = lower_32_bits(usb_req->dma);
	p_trb->data_buf_ptr_hi = upper_32_bits(usb_req->dma);

	msg_dbg(nvudc->dev, "data_buf_ptr_lo = 0x%x, data_buf_ptr_hi = 0x%x\n",
		p_trb->data_buf_ptr_lo, p_trb->data_buf_ptr_hi);

	/* TRB_Transfer_Length
	For USB_DIR_OUT, this field is the number of data bytes expected from
	xhc. For USB_DIR_IN, this field is the number of data bytes the device
	will send. */
	u_temp = 0;
	XHCI_SETF_VAR(TRB_TRANSFER_LEN, u_temp, transfer_length);
	XHCI_SETF_VAR(TRB_TD_SIZE, u_temp, td_size);
	p_trb->trb_dword2 = u_temp;

	u_temp = 0;
	XHCI_SETF_VAR(TRB_CYCLE_BIT, u_temp, pcs);
	XHCI_SETF_VAR(TRB_INTR_ON_SHORT_PKT, u_temp, 1);
	XHCI_SETF_VAR(TRB_INTR_ON_COMPLETION, u_temp, IOC);
	XHCI_SETF_VAR(TRB_TYPE, u_temp, TRB_TYPE_XFER_DATA_STAGE);
	XHCI_SETF_VAR(DATA_STAGE_TRB_DIR, u_temp, dir);
	p_trb->trb_dword3 = u_temp;

	msg_dbg(nvudc->dev, "trb_dword2 = 0x%x, trb_dword3 = 0x%x\n",
			p_trb->trb_dword2, p_trb->trb_dword3);

}


void setup_trb(struct nv_udc_s *nvudc, struct transfer_trb_s *p_trb,
		struct usb_request *usb_req, u32 xfer_len,
		dma_addr_t xfer_buf_addr, u8 td_size, u8 pcs,
		u8 trb_type, u8 short_pkt, u8 chain_bit,
		u8 intr_on_compl, bool b_setup_stage, u8 usb_dir,
		bool b_isoc, u8 tlb_pc, u16 frame_i_d, u8 SIA)
{
	u32 u_temp;
	p_trb->data_buf_ptr_lo = lower_32_bits(xfer_buf_addr);
	p_trb->data_buf_ptr_hi = upper_32_bits(xfer_buf_addr);

	msg_dbg(nvudc->dev, "data_buf_ptr_lo = 0x%x, data_buf_ptr_hi = 0x%x\n",
		p_trb->data_buf_ptr_lo, p_trb->data_buf_ptr_hi);

	u_temp = 0;
	XHCI_SETF_VAR(TRB_TRANSFER_LEN, u_temp, xfer_len);
	XHCI_SETF_VAR(TRB_TD_SIZE, u_temp, td_size);

	p_trb->trb_dword2 = u_temp;

	u_temp = 0;

	XHCI_SETF_VAR(TRB_CYCLE_BIT, u_temp, pcs);
	XHCI_SETF_VAR(TRB_INTR_ON_SHORT_PKT, u_temp, short_pkt);
	XHCI_SETF_VAR(TRB_CHAIN_BIT, u_temp, chain_bit);
	XHCI_SETF_VAR(TRB_INTR_ON_COMPLETION, u_temp, intr_on_compl);
	XHCI_SETF_VAR(TRB_TYPE, u_temp, trb_type);

	if (b_setup_stage)
		XHCI_SETF_VAR(DATA_STAGE_TRB_DIR, u_temp, usb_dir);

	if (usb_req->stream_id)
		XHCI_SETF_VAR(STREAM_TRB_STREAM_ID, u_temp, usb_req->stream_id);

	if (b_isoc) {
		XHCI_SETF_VAR(ISOC_TRB_TLBPC, u_temp, tlb_pc);
		XHCI_SETF_VAR(ISOC_TRB_FRAME_ID, u_temp, frame_i_d);
		XHCI_SETF_VAR(ISOC_TRB_SIA, u_temp, SIA);
	}

	p_trb->trb_dword3 = u_temp;
	msg_dbg(nvudc->dev, "trb_dword2 = 0x%.8x, trb_dword3 = 0x%.8x\n",
		p_trb->trb_dword2, p_trb->trb_dword3);
}

/*
 * TD and zlp for a transfer share the same usb_request. IOC is only set
 * once between them. The ways to set IOC for TD and zlp are different
 * between control and bulk/interrupt endpoint:
 * 1. Control endpoint: IOC of data stage TD is clear. IOC of zlp is set.
 *    Status stage TD is scheduled during IOC interrupt of zlp. This
 *    guarantees status stage TD scheduled after both of data stage
 *    TD and zlp.
 * 2. Bulk/interrupt endpoint: IOC of TD is set. IOC of zlp is clear.
 *    Actual transfer length is gotten and usb_request gets completed
 *    during IOC interrupt of TD. The completion of zlp is ignored.
 */
static void nvudc_queue_zlp_td(struct nv_udc_s *nvudc, struct nv_udc_ep *udc_ep)
{
	struct transfer_trb_s *enq_pt = udc_ep->enq_pt;
	u32 dw = 0;

	enq_pt->data_buf_ptr_lo = 0;
	enq_pt->data_buf_ptr_hi = 0;
	enq_pt->trb_dword2 = 0;

	XHCI_SETF_VAR(TRB_CYCLE_BIT, dw, udc_ep->pcs);
	if (usb_endpoint_xfer_control(udc_ep->desc)) {
		XHCI_SETF_VAR(TRB_TYPE, dw, TRB_TYPE_XFER_DATA_STAGE);
		XHCI_SETF_VAR(DATA_STAGE_TRB_DIR, dw, 1);
		XHCI_SETF_VAR(TRB_INTR_ON_COMPLETION, dw, 1);
	} else {
		XHCI_SETF_VAR(TRB_TYPE, dw, TRB_TYPE_XFER_NORMAL);
	}
	enq_pt->trb_dword3 = dw;
	msg_dbg(nvudc->dev, "trb_dword2 = 0x%x, trb_dword3 = 0x%x\n",
			enq_pt->trb_dword2, enq_pt->trb_dword3);

	enq_pt++;
	if (XHCI_GETF(TRB_TYPE, enq_pt->trb_dword3) ==
			TRB_TYPE_LINK) {
		if (XHCI_GETF(TRB_LINK_TOGGLE_CYCLE,
				enq_pt->trb_dword3)) {
			XHCI_SETF_VAR(TRB_CYCLE_BIT,
				enq_pt->trb_dword3,
				udc_ep->pcs);
			udc_ep->pcs ^= 0x1;
		}
		enq_pt = udc_ep->tran_ring_ptr;
	}
	udc_ep->enq_pt = enq_pt;
}

int nvudc_queue_trbs(struct nv_udc_ep *udc_ep_ptr,
		struct nv_udc_request *udc_req_ptr,  bool b_isoc,
		u32 xfer_ring_size,
		u32 num_trbs_needed, u64 buffer_length)
{
	struct nv_udc_s *nvudc = udc_ep_ptr->nvudc;
	struct transfer_trb_s *p_xfer_ring = udc_ep_ptr->tran_ring_ptr;
	u32 num_trbs_ava = 0;
	struct usb_request *usb_req = &udc_req_ptr->usb_req;
	u64 buff_len_temp = 0;
	u32 i, j = 1;
	struct transfer_trb_s *enq_pt = udc_ep_ptr->enq_pt;
	u8 td_size;
	u8 chain_bit = 1;
	u8 short_pkt = 0;
	u8 intr_on_compl = 0;
	u32 count;
	bool full_td = true;
	u32 intr_rate;
	dma_addr_t trb_buf_addr;
	bool need_zlp = false;

	msg_dbg(nvudc->dev, "nvudc_queue_trbs\n");

	if (!b_isoc) {
		if (udc_req_ptr->usb_req.zero == 1 &&
			udc_req_ptr->usb_req.length != 0 &&
			((udc_req_ptr->usb_req.length %
			  udc_ep_ptr->usb_ep.maxpacket) == 0)) {
			need_zlp = true;
		}
	}

	td_size = num_trbs_needed;

	num_trbs_ava = room_on_ring(nvudc, xfer_ring_size, p_xfer_ring,
			udc_ep_ptr->enq_pt, udc_ep_ptr->deq_pt);


	/* trb_buf_addr points to the addr of the buffer that we write in
	 * each TRB. If this function is called to complete the pending TRB
	 * transfers of a previous request, point it to the buffer that is
	 * not transferred, or else point it to the starting address of the
	 * buffer received in usb_request
	 */
	if (udc_req_ptr->trbs_needed) {
		/* Here udc_req_ptr->trbs_needed is used to indicate if we
		 * are completing a previous req
		 */
		trb_buf_addr = usb_req->dma +
			(usb_req->length - udc_req_ptr->buff_len_left);
	} else {
		trb_buf_addr = usb_req->dma;
	}


	if (num_trbs_ava >= num_trbs_needed + (need_zlp ? 1 : 0))
		count = num_trbs_needed;
	else {
		if (b_isoc) {
			struct nv_udc_request *udc_req_ptr_temp;
			u8 temp = 0;
			list_for_each_entry(udc_req_ptr_temp,
					&udc_ep_ptr->queue, queue) {
				temp++;
			}

			if (temp >= 2) {
				/*  we already scheduled two mfi in advance. */
				return 0;
			}
		}

		/* always keep one trb for zlp. */
		count = num_trbs_ava - (need_zlp ? 1 : 0);
		full_td = false;
		msg_dbg(nvudc->dev, "TRB Ring Full. Avail: 0x%x Req: 0x%x\n",
						num_trbs_ava, num_trbs_needed);
		udc_ep_ptr->tran_ring_full = true;
	}

	msg_dbg(nvudc->dev, "queue_trbs count = 0x%x\n", count);
	for (i = 0; i < count; i++) {
		if (buffer_length > TRB_MAX_BUFFER_SIZE)
			buff_len_temp = TRB_MAX_BUFFER_SIZE;
		else
			buff_len_temp = buffer_length;

		buffer_length -= buff_len_temp;

		if (usb_endpoint_dir_out(udc_ep_ptr->desc))
			short_pkt = 1;

		if (buffer_length == 0) {
			chain_bit = 0;
			intr_on_compl = 1;
			udc_req_ptr->all_trbs_queued = 1;
		}

#define BULK_EP_INTERRUPT_RATE      5
#define ISOC_EP_INTERRUPT_RATE      1
		if  (b_isoc)
			intr_rate = ISOC_EP_INTERRUPT_RATE;
		else
			intr_rate = BULK_EP_INTERRUPT_RATE;

		if  ((!full_td) && (j == intr_rate)) {
			intr_on_compl = 1;
			j = 0;
		}

		if (b_isoc) {
			setup_trb(nvudc, enq_pt, usb_req, buff_len_temp,
				trb_buf_addr, td_size-1, udc_ep_ptr->pcs,
				TRB_TYPE_XFER_DATA_ISOCH, short_pkt, chain_bit,
				intr_on_compl, 0, 0, 1, 0, 0, 1);
		} else {
			u8 pcs = udc_ep_ptr->pcs;
			if (udc_ep_ptr->comp_desc
				&& usb_ss_max_streams(udc_ep_ptr->comp_desc)) {
				setup_trb(nvudc, enq_pt, usb_req, buff_len_temp,
					trb_buf_addr, td_size-1,
					pcs, TRB_TYPE_XFER_STREAM, short_pkt,
					chain_bit, intr_on_compl, 0, 0, 0, 0,
					udc_req_ptr->usb_req.stream_id, 0);
			} else {
				setup_trb(nvudc, enq_pt, usb_req, buff_len_temp,
					trb_buf_addr, td_size-1,
					pcs, TRB_TYPE_XFER_NORMAL, short_pkt,
					chain_bit, intr_on_compl, 0, 0, 0, 0, 0,
					0);
		}
		}
		trb_buf_addr += buff_len_temp;
		td_size--;
		enq_pt++;
		j++;
		if (XHCI_GETF(TRB_TYPE, enq_pt->trb_dword3) == TRB_TYPE_LINK) {
			if (XHCI_GETF(TRB_LINK_TOGGLE_CYCLE,
					enq_pt->trb_dword3)) {

				XHCI_SETF_VAR(TRB_CYCLE_BIT,
					enq_pt->trb_dword3, udc_ep_ptr->pcs);
				udc_ep_ptr->pcs ^= 0x1;

				enq_pt = udc_ep_ptr->tran_ring_ptr;
			}
		}
	}

	if (!udc_req_ptr->trbs_needed)
		udc_req_ptr->first_trb = udc_ep_ptr->enq_pt;
	udc_ep_ptr->enq_pt = enq_pt;
	udc_req_ptr->buff_len_left = buffer_length;
	udc_req_ptr->trbs_needed = td_size;

	if (need_zlp && udc_req_ptr->buff_len_left == 0 &&
			!udc_ep_ptr->tran_ring_full)
		nvudc_queue_zlp_td(nvudc, udc_ep_ptr);

	if (udc_req_ptr->buff_len_left == 0) {
		/* It is actually last trb of a request plus 1 */
		if (udc_ep_ptr->enq_pt == udc_ep_ptr->tran_ring_ptr)
			udc_req_ptr->last_trb = udc_ep_ptr->link_trb - 1;
		else
			udc_req_ptr->last_trb = udc_ep_ptr->enq_pt - 1;
	}

	return 0;
}



int nvudc_queue_ctrl(struct nv_udc_ep *udc_ep_ptr,
		struct nv_udc_request *udc_req_ptr, u32 num_of_trbs_needed)
{
	struct nv_udc_s *nvudc = udc_ep_ptr->nvudc;
	struct ep_cx_s *p_ep_cx = nvudc->p_epcx;
	u8 ep_state = XHCI_GETF(EP_CX_EP_STATE, p_ep_cx->ep_dw0);
	struct transfer_trb_s *enq_pt = udc_ep_ptr->enq_pt;
	struct transfer_trb_s *dq_pt = udc_ep_ptr->deq_pt;
	struct usb_request *usb_req = &udc_req_ptr->usb_req;
	struct transfer_trb_s *p_trb;
	u32 transfer_length;
	u32 td_size = 0;
	u8 IOC;
	u8 dir = 0;

	msg_entry(nvudc->dev);
	msg_dbg(nvudc->dev, "num_of_trbs_needed = 0x%x\n", num_of_trbs_needed);

	/* Need to queue the request even ep is paused or halted */
	if (ep_state != EP_STATE_RUNNING) {
		msg_dbg(nvudc->dev, "EP State = 0x%x\n", ep_state);
		return -EINVAL;
	}

	if (list_empty(&udc_ep_ptr->queue)) {
		/* For control endpoint, we can handle one setup request at a
		 time. so if there are TD pending in the transfer ring.
		 wait for the sequence number error event. Then put the new
		 request to tranfer ring */
		if (enq_pt == dq_pt) {
			u32 u_temp = 0, i;
			bool need_zlp = false;

			msg_dbg(nvudc->dev, "Setup Data Stage TRBs\n");
			/* Transfer ring is empty
			   setup data stage TRBs */
			udc_req_ptr->first_trb = udc_ep_ptr->enq_pt;

			if (nvudc->setup_status ==  DATA_STAGE_XFER)
				dir = 1;
			else if (nvudc->setup_status == DATA_STAGE_RECV)
				dir = 0;
			else
				msg_dbg(nvudc->dev, "unexpected setup_status!%d\n"
						, nvudc->setup_status);

			if (udc_req_ptr->usb_req.zero == 1 &&
				udc_req_ptr->usb_req.length != 0 &&
				((udc_req_ptr->usb_req.length %
				  udc_ep_ptr->usb_ep.maxpacket) == 0))
				need_zlp = true;

			msg_dbg(nvudc->dev, "dir=%d, enq_pt=0x%p\n",
					dir, enq_pt);

			for (i = 0; i < num_of_trbs_needed; i++) {
				p_trb = enq_pt;
				if (i < (num_of_trbs_needed - 1)) {
					transfer_length = TRB_MAX_BUFFER_SIZE;
					IOC = 0;
				} else {
					u_temp = TRB_MAX_BUFFER_SIZE * i;
					transfer_length = (u32)usb_req->length
						- u_temp;
					IOC = 1;
				}

				msg_dbg(nvudc->dev,
					"tx_len = 0x%x, u_temp = 0x%x\n",
					transfer_length, u_temp);

				setup_datastage_trb(nvudc, p_trb, usb_req,
					udc_ep_ptr->pcs, i,
					transfer_length, td_size, IOC, dir);
				udc_req_ptr->all_trbs_queued = 1;
				enq_pt++;

				if (XHCI_GETF(TRB_TYPE, enq_pt->trb_dword3) ==
						TRB_TYPE_LINK) {
					if (XHCI_GETF(TRB_LINK_TOGGLE_CYCLE,
							enq_pt->trb_dword3)) {

						XHCI_SETF_VAR(TRB_CYCLE_BIT,
							enq_pt->trb_dword3,
							udc_ep_ptr->pcs);
						udc_ep_ptr->pcs ^= 0x1;
					}

					enq_pt = udc_ep_ptr->tran_ring_ptr;
				}
			}

			udc_ep_ptr->enq_pt = enq_pt;

			u_temp = 0;

			u_temp = DB_TARGET(0);


			u_temp |= DB_STREAMID(nvudc->ctrl_seq_num);

			msg_dbg(nvudc->dev, "DB register 0x%x\n",
					u_temp);

			iowrite32(u_temp, nvudc->mmio_reg_base + DB);
			udc_req_ptr->need_zlp = need_zlp;

			if (udc_ep_ptr->enq_pt == udc_ep_ptr->tran_ring_ptr)
				udc_req_ptr->last_trb = udc_ep_ptr->link_trb
								- 1;
			else
				udc_req_ptr->last_trb = udc_ep_ptr->enq_pt
								- 1;
		} else {
			/* we process one setup request at a time, so ring
			 * should already be empty.*/
			msg_dbg(nvudc->dev, "Eq = 0x%p != Dq = 0x%p\n",
				 enq_pt, dq_pt);
			/* Assert() */
		}
	} else {
		msg_dbg(nvudc->dev, "udc_ep_ptr->queue not empty\n");
		/* New setup packet came
		   Drop the this req.. */
		return -ECANCELED;
	}

	return 0;
}

int nvudc_build_td(struct nv_udc_ep *udc_ep_ptr,
		struct nv_udc_request *udc_req_ptr)
{
	int status = 0;
	struct nv_udc_s *nvudc = udc_ep_ptr->nvudc;
	u32 num_trbs_needed;
	u64 buffer_length;
	u32 u_temp;

	msg_entry(nvudc->dev);

	if (udc_req_ptr->trbs_needed) {
		/* If this is called to complete pending TRB transfers
		 * of previous Request
		 */
		buffer_length = udc_req_ptr->buff_len_left;
		num_trbs_needed = udc_req_ptr->trbs_needed;
	} else {
		buffer_length = (u64)udc_req_ptr->usb_req.length;
		num_trbs_needed = (u32)(buffer_length / TRB_MAX_BUFFER_SIZE);

		if ((buffer_length == 0) ||
			(buffer_length % TRB_MAX_BUFFER_SIZE))
			num_trbs_needed += 1;
	}

	msg_dbg(nvudc->dev, "buf_len = %ld, num_trb_needed = %d",
	(unsigned long)buffer_length, num_trbs_needed);

	if (usb_endpoint_xfer_control(udc_ep_ptr->desc))
		status = nvudc_queue_ctrl(udc_ep_ptr,
				 udc_req_ptr, num_trbs_needed);
	else if (usb_endpoint_xfer_isoc(udc_ep_ptr->desc)) {
		status = nvudc_queue_trbs(udc_ep_ptr, udc_req_ptr, 1,
				NVUDC_ISOC_EP_TD_RING_SIZE,
				num_trbs_needed, buffer_length);
		u_temp = udc_ep_ptr->DCI;
		u_temp = DB_TARGET(u_temp);
		msg_dbg(nvudc->dev, "DOORBELL = 0x%x\n", u_temp);
		iowrite32(u_temp, nvudc->mmio_reg_base + DB);

	} else if (usb_endpoint_xfer_bulk(udc_ep_ptr->desc)) {
		msg_dbg(nvudc->dev, "nvudc_queue_trbs\n");
		status = nvudc_queue_trbs(udc_ep_ptr, udc_req_ptr, 0,
				NVUDC_BULK_EP_TD_RING_SIZE,
				num_trbs_needed, buffer_length);
		u_temp = udc_ep_ptr->DCI;
		u_temp = DB_TARGET(u_temp);
		if (udc_ep_ptr->comp_desc &&
				usb_ss_max_streams(udc_ep_ptr->comp_desc)) {
			/* hold the doorbell if stream_rejected is set */
			if (nvudc->stream_rejected & NV_BIT(udc_ep_ptr->DCI))
				return status;
			u_temp |= DB_STREAMID(udc_req_ptr->usb_req.stream_id);
		}
		msg_dbg(nvudc->dev, "DOORBELL = 0x%x\n", u_temp);
		iowrite32(u_temp, nvudc->mmio_reg_base + DB);

	} else {
		/* interrupt endpoint */
		status = nvudc_queue_trbs(udc_ep_ptr, udc_req_ptr, 0,
				NVUDC_INT_EP_TD_RING_SIZE,
				num_trbs_needed, buffer_length);
		u_temp = udc_ep_ptr->DCI;
		u_temp = DB_TARGET(u_temp);
		msg_dbg(nvudc->dev, "DOORBELL = 0x%x\n", u_temp);
		iowrite32(u_temp, nvudc->mmio_reg_base + DB);
	}

	return status;
}

void clear_req_container(struct nv_udc_request *udc_req_ptr)
{
	udc_req_ptr->buff_len_left = 0;
	udc_req_ptr->trbs_needed = 0;
	udc_req_ptr->all_trbs_queued = 0;
	udc_req_ptr->first_trb = NULL;
	udc_req_ptr->last_trb = NULL;
	udc_req_ptr->short_pkt = 0;
	udc_req_ptr->need_zlp = false;
}

static int
nvudc_ep_queue(struct usb_ep *_ep, struct usb_request *_req, gfp_t gfp_flags)
{
	struct nv_udc_request *udc_req_ptr;
	struct nv_udc_ep *udc_ep_ptr;
	struct nv_udc_s *nvudc;
	int status;
	unsigned long flags;

	if (!_req || !_ep)
		return -EINVAL;

	udc_req_ptr = container_of(_req, struct nv_udc_request, usb_req);
	udc_ep_ptr = container_of(_ep, struct nv_udc_ep, usb_ep);

	if (!udc_req_ptr || !udc_ep_ptr)
		return -EINVAL;

	nvudc = udc_ep_ptr->nvudc;
	msg_entry(nvudc->dev);

	spin_lock_irqsave(&nvudc->lock, flags);

	if (nvudc->elpg_is_processing) {
		spin_unlock_irqrestore(&nvudc->lock, flags);
		msg_exit(nvudc->dev);
		return -ESHUTDOWN;
	}


	if (!udc_ep_ptr->tran_ring_ptr ||
		!udc_req_ptr->usb_req.complete ||
		!udc_req_ptr->usb_req.buf ||
		!list_empty(&udc_req_ptr->queue)) {
		spin_unlock_irqrestore(&nvudc->lock, flags);
		return -EINVAL;
	}
	msg_dbg(nvudc->dev, "EPDCI = 0x%x\n", udc_ep_ptr->DCI);

	if (!udc_ep_ptr->desc) {
		msg_dbg(nvudc->dev, "udc_ep_ptr->Desc is null\n");
		spin_unlock_irqrestore(&nvudc->lock, flags);
		return -EINVAL;
	}

	/* Clearing the Values of the NV_UDC_REQUEST container */
	clear_req_container(udc_req_ptr);
	udc_req_ptr->mapped = 0;

	if (usb_endpoint_xfer_control(udc_ep_ptr->desc) &&
				(_req->length == 0)) {
		nvudc->setup_status = STATUS_STAGE_XFER;
		status = -EINPROGRESS;
		if (udc_req_ptr) {
			msg_dbg(nvudc->dev,
				"udc_req_ptr = 0x%p\n", udc_req_ptr);

			build_ep0_status(&nvudc->udc_ep[0], false, status,
					udc_req_ptr);
		} else {
			msg_dbg(nvudc->dev, "udc_req_ptr = NULL\n");
			build_ep0_status(&nvudc->udc_ep[0], true, status, NULL);
		}
		msg_dbg(nvudc->dev,
			"act status request for control endpoint\n");
		spin_unlock_irqrestore(&nvudc->lock, flags);
		return 0;
	}

	/* request length is possible to be 0. Like SCSI blank command */
	msg_dbg(nvudc->dev, "request length=%d\n", _req->length);

	if (udc_req_ptr->usb_req.dma == DMA_ADDR_INVALID &&
		_req->length != 0) {
		udc_req_ptr->usb_req.dma =
			dma_map_single(nvudc->gadget.dev.parent,
					udc_req_ptr->usb_req.buf,
					udc_req_ptr->usb_req.length,
					usb_endpoint_dir_in(udc_ep_ptr->desc)
					? DMA_TO_DEVICE : DMA_FROM_DEVICE);
		udc_req_ptr->mapped = 1;
	}

	udc_req_ptr->usb_req.status = -EINPROGRESS;
	udc_req_ptr->usb_req.actual = 0;

	/* If the transfer ring for this particular end point is full,
	 * then simply queue the request and return
	 */
	if (udc_ep_ptr->tran_ring_full == true) {
		status = 0;
	} else {
		/* push the request to the transfer ring if possible. */
		status = nvudc_build_td(udc_ep_ptr, udc_req_ptr);
	}
	if (!status) {
		list_add_tail(&udc_req_ptr->queue, &udc_ep_ptr->queue);
	}
	spin_unlock_irqrestore(&nvudc->lock, flags);

	if (!status && (_req->length >= BOOST_TRIGGER_SIZE))
		tegra_xudc_boost_cpu_freq(nvudc);

	msg_exit(nvudc->dev);
	return status;
}

/* This function will go through the list of the USB requests for the
 * given endpoint and schedule any unscheduled trb's to the xfer ring
 */
void queue_pending_trbs(struct nv_udc_ep *nvudc_ep_ptr)
{
	struct nv_udc_s *nvudc = nvudc_ep_ptr->nvudc;
	struct nv_udc_request *udc_req_ptr;
	msg_entry(nvudc->dev);
	/* schedule  trbs till there arent any pending unscheduled ones
	 * or the ring is full again
	 */
	list_for_each_entry(udc_req_ptr, &nvudc_ep_ptr->queue, queue) {
		if (udc_req_ptr->all_trbs_queued == 0)
			nvudc_build_td(nvudc_ep_ptr, udc_req_ptr);

		if (nvudc_ep_ptr->tran_ring_full == true)
			break;
		}
	msg_exit(nvudc->dev);
}

u32 actual_data_xfered(struct nv_udc_ep *udc_ep, struct nv_udc_request *udc_req)
{
	struct nv_udc_s *nvudc = udc_ep->nvudc;
	struct ep_cx_s *p_ep_cx = nvudc->p_epcx + udc_ep->DCI;
	u16 data_offset = XHCI_GETF(EP_CX_DATA_OFFSET, p_ep_cx->ep_dw7);
	u8 num_trbs = XHCI_GETF(EP_CX_NUMTRBS, p_ep_cx->ep_dw7);
	u64 data_left = ((num_trbs+1) * TRB_MAX_BUFFER_SIZE) -
		data_offset;
	return udc_req->usb_req.length - data_left;
}

void squeeze_xfer_ring(struct nv_udc_ep *udc_ep_ptr,
		struct nv_udc_request *udc_req_ptr)
{
	struct transfer_trb_s *temp = udc_req_ptr->first_trb;
	struct nv_udc_request *next_req;

	while (temp != udc_ep_ptr->enq_pt) {
		temp->data_buf_ptr_lo = 0;
		temp->data_buf_ptr_hi = 0;
		temp->trb_dword2 = 0;
		temp->trb_dword3 = 0;

		temp++;

		if (XHCI_GETF(TRB_TYPE, temp->trb_dword3) == TRB_TYPE_LINK)
			temp = udc_ep_ptr->tran_ring_ptr;
	}

	/* Update the new enq_ptr starting from the deleted req */
	udc_ep_ptr->enq_pt = udc_req_ptr->first_trb;

	if (udc_ep_ptr->tran_ring_full == true)
		udc_ep_ptr->tran_ring_full = false;

	next_req = list_entry(udc_req_ptr->queue.next,
				struct nv_udc_request, queue);

	list_for_each_entry_from(next_req, &udc_ep_ptr->queue, queue) {
		next_req->usb_req.status = -EINPROGRESS;
		next_req->usb_req.actual = 0;

		/* clear the values of the nv_udc_request container */
		clear_req_container(next_req);

		if (udc_ep_ptr->tran_ring_full)
			break;
		else {
			/* push the request to the transfer ring */
			nvudc_build_td(udc_ep_ptr, next_req);
		}
	}
}

bool is_pointer_less_than(struct transfer_trb_s *a, struct transfer_trb_s *b,
	struct nv_udc_ep *udc_ep)
{
	if ((b > a) && ((udc_ep->enq_pt >= b) || (udc_ep->enq_pt < a)))
		return true;
	if ((b < a) && ((udc_ep->enq_pt >= b) && (udc_ep->enq_pt < a)))
		return true;
	return false;
}

static int
nvudc_ep_dequeue(struct usb_ep *_ep, struct usb_request *_req)
{
	struct nv_udc_ep *udc_ep;
	struct nv_udc_s *nvudc;
	u32 u_temp, u_temp2 = 0;
	struct nv_udc_request *udc_req;
	struct ep_cx_s *p_ep_cx;
	u8 ep_state;
	struct nv_udc_request *p_new_udc_req;
	u64 new_dq_pt;
	unsigned long flags;
	struct transfer_trb_s *pause_pt;
	u32 deq_pt_lo, deq_pt_hi;
	u64 dq_pt_addr;

	if (!_ep || !_req)
		return -EINVAL;

	udc_ep = container_of(_ep, struct nv_udc_ep, usb_ep);
	nvudc = udc_ep->nvudc;
	msg_entry(nvudc->dev);

	p_ep_cx = nvudc->p_epcx + udc_ep->DCI;
	ep_state = XHCI_GETF(EP_CX_EP_STATE, p_ep_cx->ep_dw0);

	msg_dbg(nvudc->dev, "EPDCI = 0x%x\n", udc_ep->DCI);

	spin_lock_irqsave(&nvudc->lock, flags);

	if (nvudc->elpg_is_processing) {
		spin_unlock_irqrestore(&nvudc->lock, flags);
		msg_exit(nvudc->dev);
		return -ESHUTDOWN;
	}

	if (ep_state == EP_STATE_RUNNING) {

		msg_dbg(nvudc->dev, "EP_STATE_RUNNING\n");
		/* stop the DMA from HW first */
		u_temp = ioread32(nvudc->mmio_reg_base + EP_PAUSE);
		u_temp2 = NV_BIT(udc_ep->DCI);
		u_temp |= u_temp2;

		iowrite32(u_temp, nvudc->mmio_reg_base + EP_PAUSE);
		msg_dbg(nvudc->dev, "pause endpoints 0x%x", u_temp);

		poll_stchg(nvudc->dev, "ep_dequeue()", u_temp2);

		msg_dbg(nvudc->dev, "STCHG = 0x%x\n",
			ioread32(nvudc->mmio_reg_base + EP_STCHG));

		iowrite32(u_temp2, nvudc->mmio_reg_base + EP_STCHG);
		msg_dbg(nvudc->dev, "clear the state change register\n");

		poll_ep_thread_active(nvudc->dev, "ep_dequeue()",
			NV_BIT(udc_ep->DCI));
	}


	list_for_each_entry(udc_req, &udc_ep->queue, queue) {
		if (&udc_req->usb_req == _req)
			break;
	}

	if (&udc_req->usb_req != _req) {
		msg_dbg(nvudc->dev,
			" did not find the request in request queue\n");
		spin_unlock_irqrestore(&nvudc->lock, flags);
		return -EINVAL;
	}
	/* Request hasn't been queued to transfer ring yet
	* dequeue it from sw queue only
	*/
	if (!udc_req->first_trb) {
		req_done(udc_ep, udc_req, -ECONNRESET);
		spin_unlock_irqrestore(&nvudc->lock, flags);
		return 0;
	}

	deq_pt_lo = p_ep_cx->ep_dw2 & EP_CX_TR_DQPT_LO_MASK;
	deq_pt_hi = p_ep_cx->ep_dw3;
	dq_pt_addr = (u64)deq_pt_lo + ((u64)deq_pt_hi << 32);
	pause_pt = tran_trb_dma_to_virt(udc_ep, dq_pt_addr);

	if (is_pointer_less_than(pause_pt, udc_req->first_trb, udc_ep)) {
		msg_dbg(nvudc->dev, "squeeze_xfer_ring\n");
		/* HW hasn't process the request yet */
		squeeze_xfer_ring(udc_ep, udc_req);
		req_done(udc_ep, udc_req, -ECONNRESET);
	} else if (udc_req->last_trb &&
		is_pointer_less_than(udc_req->last_trb, pause_pt, udc_ep)) {
		/* Request has been completed by HW
		* There must be transfer events pending in event ring, and
		* it will be processed later once interrupt context gets spin
		* lock.
		* Gadget driver free the request without checking the return
		* value of usb_ep_dequeue, so we have to complete the request
		* here and drop the transfer event later.
		*/
		dev_dbg(nvudc->dev,
			" Request has been complete by HW, reject request\n");
		req_done(udc_ep, udc_req, -ECONNRESET);
		spin_unlock_irqrestore(&nvudc->lock, flags);
		return -EINVAL;

	} else {
		/* Request has been partially completed by HW */

		msg_dbg(nvudc->dev,
			" Request has been partially completed by HW\n");
		udc_req->usb_req.actual = actual_data_xfered(udc_ep, udc_req);

		if (udc_req->queue.next == &udc_ep->queue) {
			/* If there is only one request in the queue which
			 * is being dequeued, we simply point the dequeue
			 * ptr to the enqueue ptr.
			 */
			new_dq_pt = tran_trb_virt_to_dma(udc_ep,
						udc_ep->enq_pt);
		} else {
			/* If there are more requests, then we point to the
			 * starting of the next requests buffer
			 */
			p_new_udc_req = list_entry(udc_req->queue.next,
					struct nv_udc_request, queue);
			if (p_new_udc_req && p_new_udc_req->first_trb) {
				new_dq_pt = tran_trb_virt_to_dma(udc_ep,
					p_new_udc_req->first_trb);
			} else {
				/* transfer ring was full, next request hasn't
				* been queued to transfer ring yet */
				 new_dq_pt = tran_trb_virt_to_dma(udc_ep,
						udc_ep->enq_pt);
			}
		}

		p_ep_cx->ep_dw2 = lower_32_bits(new_dq_pt)
						& EP_CX_TR_DQPT_LO_MASK;
		XHCI_SETF_VAR(EP_CX_DEQ_CYC_STATE, p_ep_cx->ep_dw2,
						   udc_ep->pcs);
		p_ep_cx->ep_dw3 = upper_32_bits(new_dq_pt);
		udc_ep->deq_pt = tran_trb_dma_to_virt(udc_ep, new_dq_pt);

		XHCI_SETF_VAR(EP_CX_EDTLA, p_ep_cx->ep_dw5, 0);
		XHCI_SETF_VAR(EP_CX_PARTIALTD, p_ep_cx->ep_dw5, 0);
		XHCI_SETF_VAR(EP_CX_DATA_OFFSET, p_ep_cx->ep_dw7, 0);

		msg_dbg(nvudc->dev, "complete requests\n");
		req_done(udc_ep, udc_req, -ECONNRESET);

		msg_dbg(nvudc->dev, "reload endpoint\n");
		iowrite32(NV_BIT(udc_ep->DCI),
			nvudc->mmio_reg_base + EP_RELOAD);
		poll_reload(nvudc->dev, "ep_dequeue()", NV_BIT(udc_ep->DCI));

		/* For big TD, we generated completion event every 5 TRBS.
		* So, we do not need to update sw dequeue pointer here.
		* Wait for interrupt context to update it.
		* Do not need to queue more trbs also.
		*/
	}

	/* clear PAUSE bit and reresume data transfer */
	if (ep_state == EP_STATE_RUNNING) {
		u_temp = ioread32(nvudc->mmio_reg_base + EP_PAUSE);
		u_temp &= ~(1 << udc_ep->DCI);
		iowrite32(u_temp, nvudc->mmio_reg_base + EP_PAUSE);

		poll_stchg(nvudc->dev, "EP PAUSED", (1 << udc_ep->DCI));

		iowrite32(NV_BIT(udc_ep->DCI), nvudc->mmio_reg_base + EP_STCHG);
	}

	spin_unlock_irqrestore(&nvudc->lock, flags);
	msg_exit(nvudc->dev);
	return 0;
}

/*
 * This function will set or clear ep halt state depending on value input.
 */
static int nvudc_ep_set_halt(struct usb_ep *_ep, int value)
{
	struct nv_udc_ep *udc_ep_ptr;
	struct nv_udc_s *nvudc;
	int status;
	unsigned long flags;

	if (!_ep)
		return -EINVAL;

	udc_ep_ptr = container_of(_ep, struct nv_udc_ep, usb_ep);
	nvudc = udc_ep_ptr->nvudc;
	msg_entry(nvudc->dev);

	if (value && usb_endpoint_dir_in(udc_ep_ptr->desc) &&
			!list_empty(&udc_ep_ptr->queue)) {
		msg_err(nvudc->dev, "list not empty\n");
		return -EAGAIN;
	}

	spin_lock_irqsave(&nvudc->lock, flags);

	if (nvudc->elpg_is_processing) {
		spin_unlock_irqrestore(&nvudc->lock, flags);
		msg_exit(nvudc->dev);
		return -ESHUTDOWN;
	}

	status = ep_halt(udc_ep_ptr, value);
	spin_unlock_irqrestore(&nvudc->lock, flags);

	msg_exit(nvudc->dev);
	return status;
}

static void set_ep_halt(struct nv_udc_s *nvudc, int ep_index, char *msg)
{
	u32 val, ep_bit = NV_BIT(ep_index);

	val = ioread32(nvudc->mmio_reg_base + EP_HALT);
	val |= ep_bit;
	iowrite32(val, nvudc->mmio_reg_base + EP_HALT);

	msg_info(nvudc->dev, "%s: 0x%x\n", msg, val);

	poll_stchg(nvudc->dev, msg, ep_bit);
	iowrite32(ep_bit, nvudc->mmio_reg_base + EP_STCHG);
}

static int ep_halt(struct nv_udc_ep *udc_ep_ptr, int halt)
{
	struct nv_udc_s *nvudc;
	u32 u_temp, u_temp2, u_pause;
	struct ep_cx_s *p_ep_cx;
	bool reset_seq_only = false;

	nvudc = udc_ep_ptr->nvudc;
	msg_entry(nvudc->dev);

	u_temp = ioread32(nvudc->mmio_reg_base + EP_HALT);
	u_temp2 = NV_BIT(udc_ep_ptr->DCI);

	if (!udc_ep_ptr->desc) {
		msg_err(nvudc->dev, "NULL desc\n");
		return -EINVAL;
	}

	if (udc_ep_ptr->desc->bmAttributes == USB_ENDPOINT_XFER_ISOC) {
		msg_err(nvudc->dev, "Isoc ep, halt not supported\n");
		return -EOPNOTSUPP;
	}

	p_ep_cx = nvudc->p_epcx + udc_ep_ptr->DCI;

	if (((u_temp >> udc_ep_ptr->DCI) & 1) == halt) {
		msg_dbg(nvudc->dev, "EP status already : %d, EP_STATE = %d\n",
		halt, XHCI_GETF(EP_CX_EP_STATE, p_ep_cx->ep_dw0));

		if (!halt) {
			/* For unhalt a not halted EP case, driver still need to
			clear sequecne number in EPCx. No unhalt EP again. */
			reset_seq_only = true;
		} else {
			/* Just return for halt a halted EP case.*/
			return 0;
		}
	}

	if (halt && !reset_seq_only) {
		/* setting ep to halt */
		u_temp |= u_temp2;

		msg_dbg(nvudc->dev, "HALT EP = 0x%x\n", u_temp2);
		msg_dbg(nvudc->dev, "XHCI_EP_HALT = 0x%x\n", u_temp);
		iowrite32(u_temp, nvudc->mmio_reg_base + EP_HALT);
		poll_stchg(nvudc->dev, "ep_sethalt()", u_temp2);

		msg_dbg(nvudc->dev, "EP_STCHG = 0x%x, u_temp2 = 0x%x\n",
			ioread32(nvudc->mmio_reg_base + EP_STCHG), u_temp2);
		iowrite32(u_temp2, nvudc->mmio_reg_base + EP_STCHG);
	} else {
		/* clearing ep halt state */
		msg_dbg(nvudc->dev, "Clear EP HALT = 0x%x\n", u_temp2);
		/* disable ep and reset sequence number */
		XHCI_SETF_VAR(
			EP_CX_EP_STATE, p_ep_cx->ep_dw0, EP_STATE_DISABLED);

		iowrite32(u_temp2, nvudc->mmio_reg_base + EP_RELOAD);
		poll_reload(nvudc->dev, "ep_clearhalt()", u_temp2);

		msg_dbg(nvudc->dev, "EP_RELOAD = 0x%x, u_temp2 = 0x%x\n",
		ioread32(nvudc->mmio_reg_base + EP_RELOAD), u_temp2);

		if (!reset_seq_only) {
			/* Clear halt for a halted EP.*/
			u_temp &= ~u_temp2;
			iowrite32(u_temp, nvudc->mmio_reg_base + EP_HALT);
			poll_stchg(nvudc->dev, "ep_clearhalt()", u_temp2);

			msg_dbg(nvudc->dev, "EP_STCHG = 0x%x, u_temp2 = 0x%x\n",
			ioread32(nvudc->mmio_reg_base + EP_STCHG), u_temp2);

			iowrite32(u_temp2, nvudc->mmio_reg_base + EP_STCHG);
		}

		/* set endpoint to running state */
		XHCI_SETF_VAR(
			EP_CX_EP_STATE, p_ep_cx->ep_dw0, EP_STATE_RUNNING);
		XHCI_SETF_VAR(EP_CX_SEQ_NUM, p_ep_cx->ep_dw5, 0);

		iowrite32(u_temp2, nvudc->mmio_reg_base + EP_RELOAD);
		poll_reload(nvudc->dev, "ep_clearhalt()", u_temp2);

		msg_dbg(nvudc->dev, "EP_RELOAD = 0x%x, u_temp2 = 0x%x\n",
			ioread32(nvudc->mmio_reg_base + EP_RELOAD), u_temp2);

		/* clear pause for the endpoint */
		u_pause = ioread32(nvudc->mmio_reg_base + EP_PAUSE);
		u_pause &= ~u_temp2;
		iowrite32(u_pause, nvudc->mmio_reg_base + EP_PAUSE);
		poll_stchg(nvudc->dev, "ep_clearhalt()", u_temp2);

		msg_dbg(nvudc->dev, "EP_STCHG = 0x%x, u_temp2 = 0x%x\n",
			ioread32(nvudc->mmio_reg_base + EP_STCHG), u_temp2);

		if (!list_empty(&udc_ep_ptr->queue)) {
			u_temp = udc_ep_ptr->DCI;
			u_temp = DB_TARGET(u_temp);
			iowrite32(u_temp, nvudc->mmio_reg_base + DB);
			msg_dbg(nvudc->dev, "DOORBELL = 0x%x\n", u_temp);
		}
	}

	msg_exit(nvudc->dev);
	return 0;
}

void doorbell_for_unpause(struct nv_udc_s *nvudc, u32 paused_bits)
{
	int i;
	u32 u_temp;
	struct nv_udc_ep *udc_ep_ptr;

	for (i = 0; i < NUM_EP_CX; i++) {
		if (!paused_bits)
			break;
		if (paused_bits & 1) {
			udc_ep_ptr = &nvudc->udc_ep[i];
			if (!list_empty(&udc_ep_ptr->queue)) {
				u_temp = udc_ep_ptr->DCI;
				u_temp = DB_TARGET(u_temp);
				iowrite32(u_temp, nvudc->mmio_reg_base + DB);
			}
		}
		paused_bits = paused_bits >> 1;
	}
}

static struct usb_ep_ops nvudc_ep_ops = {
	.enable = nvudc_ep_enable,
	.disable = nvudc_ep_disable,
	.alloc_request = nvudc_alloc_request,
	.free_request = nvudc_free_request,
	.queue = nvudc_ep_queue,
	.dequeue = nvudc_ep_dequeue,
	.set_halt = nvudc_ep_set_halt,
};

static int nvudc_gadget_get_frame(struct usb_gadget *gadget)
{
	u32 u_temp;
	struct nv_udc_s *nvudc = container_of(gadget, struct nv_udc_s, gadget);
	u_temp = ioread32(nvudc->mmio_reg_base + MFINDEX);
	u_temp = MFINDEX_FRAME(u_temp);
	return u_temp >> MFINDEX_FRAME_SHIFT;
}

static void nvudc_resume_state(struct nv_udc_s *nvudc, bool device_init)
{
	u32 ep_paused, u_temp, u_temp2;

	/* Unpausing the Endpoints */
	ep_paused = ioread32(nvudc->mmio_reg_base + EP_PAUSE);
	iowrite32(0, nvudc->mmio_reg_base + EP_PAUSE);
	poll_stchg(nvudc->dev, "Resume_state", ep_paused);
	iowrite32(ep_paused, nvudc->mmio_reg_base + EP_STCHG);

	u_temp = ioread32(nvudc->mmio_reg_base + PORTSC);

	if (XDEV_U0 != (u_temp & PORTSC_PLS_MASK)) {
		msg_info(nvudc->dev, "Directing link to U0\n");
		/* don't clear (write to 1) bits which we are not handling */
		u_temp2 = u_temp & PORTSC_MASK;
		u_temp2 &= ~PORTSC_PLS(-1);
		u_temp2 |= PORTSC_PLS(0);
		u_temp2 |= PORTSC_LWS;
		iowrite32(u_temp2, nvudc->mmio_reg_base + PORTSC);
	}

	if (nvudc->device_state == USB_STATE_SUSPENDED) {
		nvudc->device_state = nvudc->resume_state;
		nvudc->resume_state = 0;
	}

	if (device_init) {
		/* select HS/FS PI */
		u_temp = ioread32(nvudc->mmio_reg_base + CFG_DEV_FE);
		u_temp &= PORTREGSEL_MASK;
		u_temp |= CFG_DEV_FE_PORTREGSEL(2);
		iowrite32(u_temp, nvudc->mmio_reg_base + CFG_DEV_FE);

		/* set LWS = 1 and PLS = rx_detect */
		u_temp = ioread32(nvudc->mmio_reg_base + PORTSC);
		u_temp &= PORTSC_MASK;
		u_temp &= ~PORTSC_PLS(~0);
		u_temp |= PORTSC_PLS(5);
		u_temp |= PORTSC_LWS;
		iowrite32(u_temp, nvudc->mmio_reg_base + PORTSC);

		/* restore portregsel */
		u_temp = ioread32(nvudc->mmio_reg_base + CFG_DEV_FE);
		u_temp &= PORTREGSEL_MASK;
		u_temp |= CFG_DEV_FE_PORTREGSEL(0);
		iowrite32(u_temp, nvudc->mmio_reg_base + CFG_DEV_FE);
	}

	/* If the time difference between SW clearing the Pause bit
	*  and SW ringing doorbell was very little (less than 200ns),
	*  this could lead to the doorbell getting dropped and the
	*  corresponding transfer not completing.
	*  WAR: add 500ns delay before ringing the door bell
	*/
	ndelay(500);
	/* ring door bell for the paused endpoints */
	doorbell_for_unpause(nvudc, ep_paused);
}

static int nvudc_gadget_wakeup(struct usb_gadget *gadget)
{
	struct nv_udc_s *nvudc = container_of(gadget, struct nv_udc_s, gadget);
	u32 utemp, u_temp2;
	unsigned long flags;

	msg_entry(nvudc->dev);
	spin_lock_irqsave(&nvudc->lock, flags);

	utemp = ioread32(nvudc->mmio_reg_base + PORTPM);

	msg_info(nvudc->dev, "PORTPM = %08x, speed = %d\n", utemp,
		nvudc->gadget.speed);

	if (((nvudc->gadget.speed == USB_SPEED_FULL ||
		nvudc->gadget.speed == USB_SPEED_HIGH) &&
		(PORTPM_RWE & utemp)) ||
		((nvudc->gadget.speed == USB_SPEED_SUPER) &&
		(PORTPM_FRWE & utemp))) {

		nvudc_resume_state(nvudc, 0);

		/* write the function wake device notification register */
		if (nvudc->gadget.speed == USB_SPEED_SUPER) {
			u_temp2 = 0;
			u_temp2 |= DEVNOTIF_LO_TYPE(1);
			u_temp2 |= DEVNOTIF_LO_TRIG;
			iowrite32(0, nvudc->mmio_reg_base + DEVNOTIF_HI);
			iowrite32(u_temp2, nvudc->mmio_reg_base + DEVNOTIF_LO);
			msg_dbg(nvudc->dev, "Sent DEVNOTIF\n");
		}
	}
	spin_unlock_irqrestore(&nvudc->lock, flags);

	msg_exit(nvudc->dev);
	return 0;
}

static int nvudc_gadget_pullup(struct usb_gadget *gadget, int is_on)
{
	u32 temp;
	unsigned long flags;
	struct nv_udc_s *nvudc = container_of(gadget, struct nv_udc_s, gadget);
	msg_dbg(nvudc->dev, "pullup %x is_on %x vbus_detected %x",
		nvudc->pullup, is_on, nvudc->vbus_detected);

	pm_runtime_get_sync(nvudc->dev);
	spin_lock_irqsave(&nvudc->lock, flags);
	temp = ioread32(nvudc->mmio_reg_base + CTRL);
	if (is_on != nvudc->pullup) {
		if (is_on)
			temp |= CTRL_ENABLE;
		else
			temp &= ~CTRL_ENABLE;
		iowrite32(temp, nvudc->mmio_reg_base + CTRL);
		nvudc->pullup = is_on;
		nvudc->device_state = USB_STATE_DEFAULT;
	}
	spin_unlock_irqrestore(&nvudc->lock, flags);
	pm_runtime_put_sync(nvudc->dev);

	/* update vbus status */
	extcon_notifications(&nvudc->vbus_extcon_nb, 0, NULL);

	return 0;
}

static int nvudc_vbus_draw(struct usb_gadget *gadget, unsigned m_a)
{
	struct nv_udc_s *nvudc;

	nvudc = container_of(gadget, struct nv_udc_s, gadget);
	msg_dbg(nvudc->dev, "nvudc_vbus_draw m_a= 0x%x\n", m_a);

	if (nvudc->current_ma != m_a) {
		nvudc->current_ma = m_a;
		schedule_work(&nvudc->current_work);
	}

	return -ENOTSUPP;
}

/* defined in gadget.h */
static struct usb_gadget_ops nvudc_gadget_ops = {
	.get_frame = nvudc_gadget_get_frame,
	.wakeup = nvudc_gadget_wakeup,
	.pullup = nvudc_gadget_pullup,
	.udc_start = nvudc_gadget_start,
	.udc_stop = nvudc_gadget_stop,
	.vbus_draw = nvudc_vbus_draw,
	.set_port_state = nvudc_set_port_state,
};

static void nvudc_gadget_release(struct device *dev)
{
	return;
}

static void update_ep0_maxpacketsize(struct nv_udc_s *nvudc)
{
	u16 maxpacketsize = 0;
	struct nv_udc_ep *ep0 = &nvudc->udc_ep[0];
	struct ep_cx_s *p_epcx = nvudc->p_epcx;

	if (nvudc->gadget.speed == USB_SPEED_SUPER)
		maxpacketsize = 512;
	else
		maxpacketsize = 64;

	XHCI_SETF_VAR(EP_CX_MAX_PACKET_SIZE, p_epcx->ep_dw1, maxpacketsize);
	nvudc_ep0_desc.wMaxPacketSize = cpu_to_le16(maxpacketsize);
	ep0->usb_ep.maxpacket = maxpacketsize;
}

int init_ep0(struct nv_udc_s *nvudc)
{
	struct ep_cx_s *epcx = nvudc->p_epcx;
	struct nv_udc_ep *udc_ep = &nvudc->udc_ep[0];
	struct device *dev = nvudc->dev;
	u32 dw;

	/* setup transfer ring */
	if (!udc_ep->tran_ring_info.vaddr) {
		dma_addr_t dma;
		u32 ring_size = NVUDC_CONTROL_EP_TD_RING_SIZE;
		void *vaddr;
		size_t len;

		len = ring_size * sizeof(struct transfer_trb_s);
		vaddr = dma_alloc_coherent(nvudc->dev, len, &dma, GFP_KERNEL);
		if (!vaddr) {
			msg_err(dev, "failed to allocate trb ring\n");
			return -ENOMEM;
		}

		udc_ep->tran_ring_info.vaddr = vaddr;
		udc_ep->tran_ring_info.dma = dma;
		udc_ep->tran_ring_info.len = len;
		udc_ep->first_trb = vaddr;
		udc_ep->link_trb = udc_ep->first_trb + ring_size - 1;
	}

	memset(udc_ep->first_trb, 0, udc_ep->tran_ring_info.len);
	udc_ep->enq_pt = udc_ep->first_trb;
	udc_ep->deq_pt = udc_ep->first_trb;
	udc_ep->tran_ring_full = false;

	dw = 0;
	XHCI_SETF_VAR(EP_CX_EP_STATE, dw, EP_STATE_RUNNING);
	epcx->ep_dw0 = cpu_to_le32(dw);

	dw = 0;
	XHCI_SETF_VAR(EP_CX_EP_TYPE, dw, EP_TYPE_CNTRL);
	XHCI_SETF_VAR(EP_CX_CERR, dw, 3);
	XHCI_SETF_VAR(EP_CX_MAX_BURST_SIZE, dw, 0);
	XHCI_SETF_VAR(EP_CX_MAX_PACKET_SIZE, dw, 512);
	epcx->ep_dw1 = cpu_to_le32(dw);

	dw = lower_32_bits(udc_ep->tran_ring_info.dma) & EP_CX_TR_DQPT_LO_MASK;
	udc_ep->pcs = 1;
	XHCI_SETF_VAR(EP_CX_DEQ_CYC_STATE, dw, udc_ep->pcs);
	epcx->ep_dw2 = cpu_to_le32(dw);

	dw = upper_32_bits(udc_ep->tran_ring_info.dma);
	epcx->ep_dw3 = cpu_to_le32(dw);
	msg_dbg(dev, "EPTr_dq_pt_hi = 0x%x\n", dw);

	setup_link_trb(udc_ep->link_trb, true, udc_ep->tran_ring_info.dma);
	msg_dbg(dev, "Link Trb address = 0x%p\n", udc_ep->link_trb);

	dw = 0;
	XHCI_SETF_VAR(EP_CX_AVE_TRB_LEN, dw, 8);
	epcx->ep_dw4 = cpu_to_le32(dw);

	epcx->ep_dw5 = 0;

	dw = 0;
	XHCI_SETF_VAR(EP_CX_CERRCNT, epcx->ep_dw6, 3);
	epcx->ep_dw6 = cpu_to_le32(dw);

	epcx->ep_dw7 = 0;
	epcx->ep_dw8 = 0;
	epcx->ep_dw9 = 0;
	epcx->ep_dw11 = 0;
	epcx->ep_dw12 = 0;
	epcx->ep_dw13 = 0;
	epcx->ep_dw15 = 0;

	return 0;
}

int EP0_Start(struct nv_udc_s *nvudc)
{
	struct usb_ep *usb_ep;
	int i;

	usbep_struct_setup(nvudc, 0, NULL);

	nvudc->udc_ep[0].desc = &nvudc_ep0_desc;

	usb_ep = &nvudc->udc_ep[0].usb_ep;

	usb_ep->ops = &nvudc_ep_ops;

	/* FIXME: should not be part of ep0 setup */
	for (i = 1; i < NUM_EP_CX/2; i++) {
		u8 name[14];
		sprintf(name, "ep%dout", i);
		usbep_struct_setup(nvudc, i*2, name);
		sprintf(name, "ep%din", i);
		usbep_struct_setup(nvudc, i*2+1, name);
	}

	return 0;
}

void build_ep0_status(struct nv_udc_ep *udc_ep_ptr, bool default_value, u32
		status, struct nv_udc_request *udc_req_ptr)
{
	struct nv_udc_s *nvudc = udc_ep_ptr->nvudc;
	struct transfer_trb_s *enq_pt = udc_ep_ptr->enq_pt;
	u32 u_temp;

	if (default_value) {
		udc_req_ptr = nvudc->status_req;
		udc_req_ptr->usb_req.length = 0;
		udc_req_ptr->usb_req.status = status;
		udc_req_ptr->usb_req.actual = 0;
		udc_req_ptr->usb_req.complete = NULL;
	} else {
		udc_req_ptr->usb_req.status = status;
		udc_req_ptr->usb_req.actual = 0;
	}

	msg_dbg(nvudc->dev, "udc_req_ptr=0x%p, enq_pt=0x%p\n",
			udc_req_ptr, enq_pt);

	setup_status_trb(nvudc, enq_pt, &udc_req_ptr->usb_req, udc_ep_ptr->pcs);

	enq_pt++;

	/* check if we are at end of trb segment.  If so, update
	 * pcs and enq for next segment
	 */
	if (XHCI_GETF(TRB_TYPE, enq_pt->trb_dword3) == TRB_TYPE_LINK) {
		if (XHCI_GETF(TRB_LINK_TOGGLE_CYCLE, enq_pt->trb_dword3)) {
			XHCI_SETF_VAR(
				TRB_CYCLE_BIT,
				enq_pt->trb_dword3,
				udc_ep_ptr->pcs);
			udc_ep_ptr->pcs ^= 0x1;
		}
		enq_pt = udc_ep_ptr->tran_ring_ptr;
	}
	udc_ep_ptr->enq_pt = enq_pt;

	/* Note: for ep0, streamid field is also used for seqnum.*/
	u_temp = 0;
	u_temp |= DB_STREAMID(nvudc->ctrl_seq_num);

	msg_dbg(nvudc->dev, "DB register 0x%x\n", u_temp);

	iowrite32(u_temp, nvudc->mmio_reg_base + DB);

	list_add_tail(&udc_req_ptr->queue, &udc_ep_ptr->queue);
}

void ep0_req_complete(struct nv_udc_ep *udc_ep_ptr)
{
	struct nv_udc_s *nvudc = udc_ep_ptr->nvudc;
	switch (nvudc->setup_status) {
	case DATA_STAGE_XFER:
		nvudc->setup_status = STATUS_STAGE_RECV;
		build_ep0_status(udc_ep_ptr, true, -EINPROGRESS, NULL);
		break;
	case DATA_STAGE_RECV:
		nvudc->setup_status = STATUS_STAGE_XFER;
		build_ep0_status(udc_ep_ptr, true, -EINPROGRESS, NULL);
		break;
	default:
		if (nvudc->setup_fn_call_back)
			nvudc->setup_fn_call_back(nvudc);

		if (nvudc->set_tm & ENABLE_TM) {
			iowrite32(PORT_TM_CTRL(nvudc->set_tm),
				nvudc->mmio_reg_base + PORT_TM);
			nvudc->set_tm = 0;
		}

		nvudc->setup_status = WAIT_FOR_SETUP;
		break;
	}
}

#ifdef PRIME_NOT_RCVD_WAR
/* When the stream is rejected by Host, we call schedule_delayed_work
 * function from the IRQ handler which will make this function to be
 * executed by one of the kernel's already created worker threads. Since
 * this is executed in the Process context we can sleep here for some time
 * before retrying the stream
 */
static void retry_stream_rejected_work(struct work_struct *work)
{
	u32 u_temp;
	struct delayed_work *dwork = to_delayed_work(work);
	struct nv_udc_ep *udc_ep_ptr = container_of(dwork, struct nv_udc_ep,
							work);
	struct nv_udc_s *nvudc = udc_ep_ptr->nvudc;
	msleep(stream_rejected_sleep_msecs);

	u_temp = NV_BIT(udc_ep_ptr->DCI);
	poll_ep_stopped(nvudc->dev, "stream_rejected", u_temp);
	iowrite32(u_temp, nvudc->mmio_reg_base + EP_STOPPED);

	if (!list_empty(&udc_ep_ptr->queue)) {
		u_temp = udc_ep_ptr->DCI;
		u_temp = DB_TARGET(u_temp);
		iowrite32(u_temp, nvudc->mmio_reg_base + DB);
		msg_dbg(nvudc->dev, "DOORBELL STREAM = 0x%x\n", u_temp);
	}
}
#endif

void handle_cmpl_code_success(struct nv_udc_s *nvudc, struct event_trb_s *event,
	struct nv_udc_ep *udc_ep_ptr)
{
	u64 trb_pt;
	struct transfer_trb_s *p_trb;
	struct nv_udc_request *udc_req_ptr;
	u32 trb_transfer_length;

	msg_entry(nvudc->dev);
	trb_pt = (u64)event->trb_pointer_lo +
			((u64)(event->trb_pointer_hi) << 32);
	p_trb = tran_trb_dma_to_virt(udc_ep_ptr, trb_pt);
	msg_dbg(nvudc->dev, "trb_pt = %lx, p_trb = %p\n",
			(unsigned long)trb_pt, p_trb);

	if (!XHCI_GETF(TRB_CHAIN_BIT, p_trb->trb_dword3)) {
		/* chain bit is not set, which means it
		* is the end of a TD */
		msg_dbg(nvudc->dev, "end of TD\n");
		udc_req_ptr = list_entry(udc_ep_ptr->queue.next,
					struct nv_udc_request, queue);
		if (udc_req_ptr == NULL) {
			msg_info(nvudc->dev, "skip aborted requests\n");
			return;
		}

		msg_dbg(nvudc->dev, "udc_req_ptr = 0x%p\n", udc_req_ptr);

		trb_transfer_length = XHCI_GETF(EVE_TRB_TRAN_LEN,
					event->eve_trb_dword2);
		udc_req_ptr->usb_req.actual = udc_req_ptr->usb_req.length -
					trb_transfer_length;
		msg_dbg(nvudc->dev, "Actual data xfer = 0x%x, tx_len = 0x%x\n",
			udc_req_ptr->usb_req.actual, trb_transfer_length);

		if (udc_ep_ptr->desc &&
			usb_endpoint_xfer_control(udc_ep_ptr->desc) &&
			udc_req_ptr->need_zlp) {
			u32 dw;
			/* Only ctrl ep check need_zlp to schedule zlp for data
			 * stage TD here. Bulk/interrupt ep deal with zlp in
			 * other way. */
			nvudc_queue_zlp_td(nvudc, udc_ep_ptr);

			dw = DB_TARGET(0);
			dw |= DB_STREAMID(nvudc->ctrl_seq_num);
			msg_dbg(nvudc->dev, "DB register 0x%x for zlp\n", dw);
			iowrite32(dw, nvudc->mmio_reg_base + DB);
			udc_req_ptr->need_zlp = false;
			return;
		}

		req_done(udc_ep_ptr, udc_req_ptr, 0);

		if (!udc_ep_ptr->desc) {
			msg_dbg(nvudc->dev, "udc_ep_ptr->desc is NULL\n");
		} else {
			if (usb_endpoint_xfer_control(udc_ep_ptr->desc))
				ep0_req_complete(udc_ep_ptr);
		}
	}
}

void update_dequeue_pt(struct event_trb_s *event, struct nv_udc_ep *udc_ep)
{
	u32 deq_pt_lo = event->trb_pointer_lo;
	u32 deq_pt_hi = event->trb_pointer_hi;
	u64 dq_pt_addr = (u64)deq_pt_lo + ((u64)deq_pt_hi << 32);
	struct transfer_trb_s *deq_pt;

	deq_pt = tran_trb_dma_to_virt(udc_ep, dq_pt_addr);

	if (!deq_pt) {
		dma_addr_t dma = dq_pt_addr;
		/* TODO debug */
		pr_err("%s invalid dq_pt_addr 0x%llx\n", __func__, (u64) dma);
		return;
	}
	deq_pt++;

	if (XHCI_GETF(TRB_TYPE, deq_pt->trb_dword3) == TRB_TYPE_LINK)
		deq_pt = udc_ep->tran_ring_ptr;
	udc_ep->deq_pt = deq_pt;

}

void advance_dequeue_pt(struct nv_udc_ep *udc_ep)
{
	struct nv_udc_request *udc_req;
	if (!list_empty(&udc_ep->queue)) {
		udc_req = list_entry(udc_ep->queue.next,
				struct nv_udc_request,
				queue);

		if (udc_req->first_trb)
			udc_ep->deq_pt = udc_req->first_trb;
		else
			udc_ep->deq_pt = udc_ep->enq_pt;
	} else
		udc_ep->deq_pt = udc_ep->enq_pt;
}

bool is_request_dequeued(struct nv_udc_s *nvudc, struct nv_udc_ep *udc_ep,
		struct event_trb_s *event)
{
	struct nv_udc_request *udc_req;
	u32 trb_pt_lo = event->trb_pointer_lo;
	u32 trb_pt_hi = event->trb_pointer_hi;
	u64 trb_addr = (u64)trb_pt_lo + ((u64)trb_pt_hi << 32);
	struct transfer_trb_s *trb_pt;
	bool status = true;

	if (udc_ep->DCI == 0)
		return false;

	trb_pt = tran_trb_dma_to_virt(udc_ep, trb_addr);
	list_for_each_entry(udc_req, &udc_ep->queue, queue) {
		if ((trb_pt == udc_req->last_trb) ||
			(trb_pt == udc_req->first_trb)) {
			status = false;
			break;
		}

		if (is_pointer_less_than(trb_pt, udc_req->last_trb, udc_ep) &&
			is_pointer_less_than(udc_req->first_trb, trb_pt,
				udc_ep)) {
			status = false;
			break;
		}
	}

	return status;
}

int nvudc_handle_exfer_event(struct nv_udc_s *nvudc, struct event_trb_s *event)
{
	u8 ep_index = XHCI_GETF(EVE_TRB_ENDPOINT_ID, event->eve_trb_dword3);
	struct nv_udc_ep *udc_ep_ptr = &nvudc->udc_ep[ep_index];
	struct ep_cx_s *p_ep_cx = nvudc->p_epcx + ep_index;
	u16 comp_code;
	struct nv_udc_request *udc_req_ptr;
	bool trbs_dequeued = false;

	msg_entry(nvudc->dev);
	if (!udc_ep_ptr->tran_ring_ptr || XHCI_GETF(EP_CX_EP_STATE,
				p_ep_cx->ep_dw0) == EP_STATE_DISABLED)
		return -ENODEV;

	update_dequeue_pt(event, udc_ep_ptr);

	if (is_request_dequeued(nvudc, udc_ep_ptr, event)) {
		trbs_dequeued = true;
		dev_dbg(nvudc->dev, "WARNING: Drop the transfer event\n");
		goto queue_more_trbs;
	}

	comp_code = XHCI_GETF(EVE_TRB_COMPL_CODE, event->eve_trb_dword2);

	switch (comp_code) {
	case CMPL_CODE_SUCCESS:
	{
#ifdef PRIME_NOT_RCVD_WAR
		u32 u_temp;
		u_temp = NV_BIT(ep_index);
		/* When the stream is being rejected by the Host and then
		 * a valid packet is exchanged on this pipe, we clear the
		 * stream rejected condition and cancel any pending work that
		 * was scheduled to retry the stream
		 */
		if (nvudc->stream_rejected & u_temp) {
			nvudc->stream_rejected &= ~u_temp;
			cancel_delayed_work(&udc_ep_ptr->work);
			udc_ep_ptr->stream_rejected_retry_count = 0;
		}
#endif
		handle_cmpl_code_success(nvudc, event, udc_ep_ptr);
		trbs_dequeued = true;
		break;
	}
	case CMPL_CODE_SHORT_PKT:
	{
		u32 trb_transfer_length;

#ifdef PRIME_NOT_RCVD_WAR
		u32 u_temp;
		u_temp = NV_BIT(ep_index);
		/* When the stream is being rejected by the Host and then
		 * a valid packet is exchanged on this pipe, we clear the
		 * stream rejected condition and cancel any pending work that
		 * was scheduled to retry the stream
		 */
		if (nvudc->stream_rejected & u_temp) {
			nvudc->stream_rejected &= ~u_temp;
			cancel_delayed_work(&udc_ep_ptr->work);
			udc_ep_ptr->stream_rejected_retry_count = 0;
		}
#endif
		msg_dbg(nvudc->dev, "handle_exfer_event CMPL_CODE_SHORT_PKT\n");
		if (usb_endpoint_dir_out(udc_ep_ptr->desc)) {
			msg_dbg(nvudc->dev, "dir out EPIdx = 0x%x\n", ep_index);

			trb_transfer_length = XHCI_GETF(EVE_TRB_TRAN_LEN,
						event->eve_trb_dword2);
			udc_req_ptr = list_entry(udc_ep_ptr->queue.next,
						struct nv_udc_request, queue);

			udc_req_ptr->usb_req.actual =
				udc_req_ptr->usb_req.length -
				trb_transfer_length;
			msg_dbg(nvudc->dev, "Actual Data transfered = 0x%x\n",
					udc_req_ptr->usb_req.actual);
				req_done(udc_ep_ptr, udc_req_ptr, 0);
		} else
			msg_dbg(nvudc->dev, "ep dir in\n");
		trbs_dequeued = true;

		/* Advance the dequeue pointer to next TD */
		advance_dequeue_pt(udc_ep_ptr);

		break;
	}
	case CMPL_CODE_HOST_REJECTED:
	{
		u32 u_temp;
		msg_dbg(nvudc->dev, "CMPL_CODE_HOST_REJECTED\n");
		/* bug 1285731, 1376479*/
		/* PCD driver needs to block the ERDY until Prime Pipe
		Received. But Since currently the Host is not sending
		a PRIME packet when the stream is rejected, we are retrying
		the ERDY till some time before stopping and waiting for the
		PRIME*/
		u_temp = NV_BIT(ep_index);

#ifdef PRIME_NOT_RCVD_WAR
		if ((nvudc->stream_rejected & u_temp) &&
			(udc_ep_ptr->stream_rejected_retry_count
				>= max_stream_rejected_retry_count)) {

			/* We have tried retrying the stream for some time
			 * but the Host is not responding. So we stop
			 * retrying now and wait for the PRIME
			 */
			udc_ep_ptr->stream_rejected_retry_count = 0;
			msg_dbg(nvudc->dev, "Stream retry Limit reached\n");
		} else {
			nvudc->stream_rejected |= u_temp;
			udc_ep_ptr->stream_rejected_retry_count++;
			if (!schedule_delayed_work(&udc_ep_ptr->work, 0))
				msg_dbg(nvudc->dev,
				"Error occured in retrying stream\n");
		}
#else
		nvudc->stream_rejected |= u_temp;
#endif
		break;
	}
	case CMPL_CODE_PRIME_PIPE_RECEIVED:
	{
		u32 u_temp;
		msg_dbg(nvudc->dev, "CMPL_CODE_PRIME_PIPE_RECEIVED\n");
		u_temp = NV_BIT(ep_index);

		if (nvudc->stream_rejected & u_temp) {
			nvudc->stream_rejected &= ~u_temp;
#ifdef PRIME_NOT_RCVD_WAR
			udc_ep_ptr->stream_rejected_retry_count = 0;
			cancel_delayed_work(&udc_ep_ptr->work);
#endif
			/* When a stream is rejected for particular ep, its
			 * corresponding bit is set in the EP_STOPPED register
			 * The bit has to be cleared by writing 1 to it before
			 * sending an ERDY on the endpoint.
			 */
			poll_ep_stopped(nvudc->dev, "stream_rejected", u_temp);
			iowrite32(u_temp, nvudc->mmio_reg_base + EP_STOPPED);
		}

		/* re-ring the door bell if ERDY is pending */
		if (!list_empty(&udc_ep_ptr->queue)) {
			u_temp = udc_ep_ptr->DCI;
			u_temp = DB_TARGET(u_temp);
			iowrite32(u_temp, nvudc->mmio_reg_base + DB);
			msg_dbg(nvudc->dev, "DOORBELL STREAM = 0x%x\n", u_temp);
		}
			break;
	}
	case CMPL_CODE_BABBLE_DETECTED_ERR:
	{
		/* Race condition
		* When HW detects babble condition it generates a babble event
		* and flow controls the request from host, if at that time SW
		* was in proess of adding a new td to the transfer ring-
		* The doorbell for the new td could clear the flow control
		* condition and HW will attempt to resume transfer
		* by sending ERDY which is not desire.
		* Wait for STOPPED bit to be set by HW, so HW will stop
		* processing doorbells
		*/
		poll_ep_stopped(nvudc->dev, "cmpl_babble", NV_BIT(ep_index));
		iowrite32(NV_BIT(ep_index), nvudc->mmio_reg_base + EP_STOPPED);

		msg_dbg(nvudc->dev, "comp_babble_err\n");
		/* FALL THROUGH to halt endpoint */
	}
	case CMPL_CODE_STREAM_NUMP_ERROR:
	case CMPL_CODE_CTRL_DIR_ERR:
	case CMPL_CODE_INVALID_STREAM_TYPE_ERR:
	case CMPL_CODE_RING_UNDERRUN:
	case CMPL_CODE_RING_OVERRUN:
	case CMPL_CODE_ISOCH_BUFFER_OVERRUN:
	case CMPL_CODE_USB_TRANS_ERR:
	case CMPL_CODE_TRB_ERR:
	{
		msg_info(nvudc->dev, "comp_code = 0x%x\n", comp_code);
		set_ep_halt(nvudc, ep_index, "cmpl code error");
		break;
	}
	case CMPL_CODE_CTRL_SEQNUM_ERR:
	{
		u32 enq_idx = nvudc->ctrl_req_enq_idx;
		struct usb_ctrlrequest *setup_pkt;
		struct nv_setup_packet *nv_setup_pkt;
		u16 seq_num;

		msg_info(nvudc->dev, "CMPL_CODE_SEQNUM_ERR\n");

		/* skip seqnum err event until last one arrives. */
		if (udc_ep_ptr->deq_pt == udc_ep_ptr->enq_pt) {
			udc_req_ptr = list_entry(udc_ep_ptr->queue.next,
						struct nv_udc_request, queue);
			if (udc_req_ptr)
				req_done(udc_ep_ptr, udc_req_ptr, -EINVAL);

			/*drop all the queued setup packet, only
			* process the latest one.*/
			nvudc->setup_status = WAIT_FOR_SETUP;
			if (enq_idx) {
				nv_setup_pkt =
					&nvudc->ctrl_req_queue[enq_idx - 1];
				setup_pkt = &nv_setup_pkt->usbctrlreq;
				seq_num = nv_setup_pkt->seqnum;
				nvudc_handle_setup_pkt(nvudc, setup_pkt,
							seq_num);
				/* flash the queue after the latest
				 * setup pkt got handled.. */
				memset(nvudc->ctrl_req_queue, 0,
					sizeof(struct usb_ctrlrequest)
					* CTRL_REQ_QUEUE_DEPTH);
				nvudc->ctrl_req_enq_idx = 0;
			}
		} else {
			msg_info(nvudc->dev,
			"seqnum err skipped: deq_pt != enq_pt: 0x%p, 0x%p\n",
				udc_ep_ptr->deq_pt, udc_ep_ptr->enq_pt);
		}
		break;
	}
	case CMPL_CODE_STOPPED:
		/* stop transfer event for disconnect. */
		msg_dbg(nvudc->dev, "CMPL_CODE_STOPPED\n");
		msg_dbg(nvudc->dev, "EPDCI = 0x%x\n", udc_ep_ptr->DCI);
		udc_req_ptr = list_entry(udc_ep_ptr->queue.next,
				struct nv_udc_request,
				queue);
		if (udc_req_ptr)
			req_done(udc_ep_ptr, udc_req_ptr, -ECONNREFUSED);

		break;
	default:
		msg_dbg(nvudc->dev, "comp_code = 0x%x\n", comp_code);
		msg_dbg(nvudc->dev, "EPDCI = 0x%x\n", udc_ep_ptr->DCI);
		break;
	}

queue_more_trbs:
	/* If there are some trbs dequeued by HW and the ring
	 * was full before, then schedule any pending TRB's
	 */
	if ((trbs_dequeued == true) && (udc_ep_ptr->tran_ring_full == true)) {
		udc_ep_ptr->tran_ring_full = false;
		queue_pending_trbs(udc_ep_ptr);
	}
	msg_exit(nvudc->dev);
	return 0;
}

bool setfeaturesrequest(struct nv_udc_s *nvudc, u8 RequestType, u8 bRequest, u16
		value, u16 index, u16 length)
{
	int status = -EINPROGRESS;
	u8  DCI;
	struct nv_udc_ep *udc_ep_ptr;
	u32 u_temp;
	bool set_feat = 0;

	if (length != 0) {
		status = -EINVAL;
		goto set_feature_error;
	}

	if (nvudc->device_state == USB_STATE_DEFAULT) {
		status = -EINVAL;
		goto set_feature_error;
	}

	set_feat = (bRequest == USB_REQ_SET_FEATURE) ? 1 : 0;
	if ((RequestType & (USB_RECIP_MASK | USB_TYPE_MASK)) ==
			(USB_RECIP_ENDPOINT | USB_TYPE_STANDARD)) {
		msg_dbg(nvudc->dev, "Halt/Unhalt EP\n");
		if (nvudc->device_state == USB_STATE_ADDRESS) {
			if (index != 0) {
				status = -EINVAL;
				goto set_feature_error;
			}
		}

		DCI = (index & USB_ENDPOINT_NUMBER_MASK)*2 + ((index &
					USB_DIR_IN) ? 1 : 0);
		udc_ep_ptr = &nvudc->udc_ep[DCI];
		msg_dbg(nvudc->dev, "halt/Unhalt endpoint DCI = 0x%x\n", DCI);

		status = ep_halt(udc_ep_ptr,
				(bRequest == USB_REQ_SET_FEATURE) ? 1 : 0);
		if (status < 0)
			goto set_feature_error;
	} else if ((RequestType & (USB_RECIP_MASK | USB_TYPE_MASK)) ==
			(USB_RECIP_DEVICE | USB_TYPE_STANDARD)) {
		switch (value) {
		case USB_DEVICE_REMOTE_WAKEUP:
			/* REMOTE_WAKEUP selector is not used by USB3.0 */
			if ((nvudc->device_state < USB_STATE_DEFAULT) ||
				(nvudc->gadget.speed == USB_SPEED_SUPER)) {
				status = -EINVAL;
				goto set_feature_error;
			}
			msg_dbg(nvudc->dev, "%s_Feature RemoteWake\n",
				set_feat ? "Set" : "Clear");
			u_temp = ioread32(nvudc->mmio_reg_base + PORTPM);
			u_temp &= ~PORTPM_RWE;
			u_temp |= set_feat << PORTPM_RWE_SHIFT;
			iowrite32(u_temp, nvudc->mmio_reg_base + PORTPM);
			break;
		case USB_DEVICE_U1_ENABLE:
		case USB_DEVICE_U2_ENABLE:
		{
			if (nvudc->device_state != USB_STATE_CONFIGURED) {
				status = -EINVAL;
				goto set_feature_error;
			}

			if (index & 0xff) {
				status = -EINVAL;
				goto set_feature_error;
			}

			u_temp = ioread32(nvudc->mmio_reg_base
						+ PORTPM);

			if ((value == USB_DEVICE_U1_ENABLE) && u1_enable) {
					u_temp &= ~PORTPM_U1E;
					u_temp |= set_feat << PORTPM_U1E_SHIFT;
			}
			if ((value == USB_DEVICE_U2_ENABLE) && u2_enable) {
					u_temp &= ~PORTPM_U2E;
					u_temp |= set_feat << PORTPM_U2E_SHIFT;
			}

			iowrite32(u_temp, nvudc->mmio_reg_base + PORTPM);
			msg_dbg(nvudc->dev, "PORTPM = 0x%x", u_temp);
			break;

		}
		case USB_DEVICE_TEST_MODE:
		{
			u32 u_pattern;
			if (nvudc->gadget.speed > USB_SPEED_HIGH)
				goto set_feature_error;

			if (nvudc->device_state < USB_STATE_DEFAULT)
				goto set_feature_error;

			u_pattern = index >> 8;
			/* TESTMODE is only defined for high speed device */
			if (nvudc->gadget.speed == USB_SPEED_HIGH)
				nvudc->set_tm = PORT_TM_CTRL(u_pattern)
					| ENABLE_TM;
			if (u_pattern == TEST_MODE_OTG_HNP_REQD) {
				msg_dbg(nvudc->dev, "received OTG_HNP_REQD\n");
				nvudc->gadget.rcvd_otg_hnp_reqd = 1;
			}
			break;
		}
		case USB_DEVICE_B_HNP_ENABLE:
			if (set_feat) {
				nvudc->gadget.b_hnp_enable = 1;
			} else {
				/* b_hnp_enable is only cleared on a bus reset
				 * or at the end of a session. It cannot be
				 * cleared with a ClearFeature(b_hnp_enable)
				 * command.
				 */
				msg_dbg(nvudc->dev,
					"stall clear_feature(b_hnp_enable)\n");
				status = -EINVAL;
				goto set_feature_error;
			}
			break;
		case USB_DEVICE_A_HNP_SUPPORT:
			nvudc->gadget.a_hnp_support = 1;
			break;
		case USB_DEVICE_A_ALT_HNP_SUPPORT:
			nvudc->gadget.a_alt_hnp_support = 1;
			break;
		default:
			goto set_feature_error;
		}

	} else if ((RequestType & (USB_RECIP_MASK | USB_TYPE_MASK)) ==
			(USB_RECIP_INTERFACE | USB_TYPE_STANDARD)) {
		if (nvudc->device_state != USB_STATE_CONFIGURED) {
			status = -EINVAL;
			goto set_feature_error;
		}

		/* Suspend Option */
		if (value == USB_INTRF_FUNC_SUSPEND) {
			if (index & USB_INTR_FUNC_SUSPEND_OPT_MASK &
				USB_INTRF_FUNC_SUSPEND_LP) {
				u_temp = ioread32(nvudc->mmio_reg_base
						+ PORTPM);
				if (index & USB_INTRF_FUNC_SUSPEND_RW) {
					msg_dbg(nvudc->dev,
						"Enable Func Remote Wakeup\n");
					u_temp |= PORTPM_FRWE;
				} else {
					msg_dbg(nvudc->dev,
						"Disable Func RemoteWakeup\n");
					u_temp &= ~PORTPM_FRWE;
				}
				iowrite32(u_temp, nvudc->mmio_reg_base
						+ PORTPM);
				/* Do not need to return status stage here */
				/* Pass to composite gadget driver to process
				the request */
				return false;
			}
		}
	}

	nvudc->setup_status = STATUS_STAGE_XFER;
	build_ep0_status(&nvudc->udc_ep[0], true, status, NULL);
	return true;

set_feature_error:
	set_ep_halt(nvudc, 0, "set feature error");
	nvudc->setup_status = WAIT_FOR_SETUP;
	return true;
}

void getstatusrequest(struct nv_udc_s *nvudc, u8 RequestType, u16 value,
		u16 index, u16 length)
{
	u64 temp = 0;
	u32 temp2;
	u32 status = -EINPROGRESS;
	struct nv_udc_request *udc_req_ptr = nvudc->status_req;
	struct nv_udc_ep *udc_ep_ptr;

	if (!udc_req_ptr) {
		msg_exit(nvudc->dev);
		return;
	}

	if ((value) || (length > 2) || !length) {
		status = -EINVAL;
		goto get_status_error;
	}

	msg_dbg(nvudc->dev, "Get status request RequestType = 0x%x Index=%x\n",
			RequestType, index);
	if ((RequestType & USB_RECIP_MASK) == USB_RECIP_DEVICE) {
		msg_dbg(nvudc->dev, "Get status request Device request\n");
		if (index == USB_DEVICE_OTG_STATUS_SELECTOR) {
			/* set HOST_REQUEST_FLAG if otg_hnp_reqd received OR
			 * user on peripheral side requesting to become host.
			 */
			if (nvudc->gadget.rcvd_otg_hnp_reqd ||
				nvudc->gadget.request_hnp) {
				msg_dbg(nvudc->dev, "set HOST_REQUEST_FLAG\n");
				temp = HOST_REQUEST_FLAG;
				if (nvudc->gadget.request_hnp)
					nvudc->gadget.request_hnp = 0;
			}
		} else if (index) {
			status = -EINVAL;
			goto get_status_error;
		}

		temp2 = ioread32(nvudc->mmio_reg_base + PORTPM);

		if (nvudc->gadget.speed == USB_SPEED_HIGH ||
			nvudc->gadget.speed == USB_SPEED_FULL) {
			if (PORTPM_RWE & temp2)
				temp |= NV_BIT(USB_DEVICE_REMOTE_WAKEUP);
		}

		if (nvudc->gadget.speed == USB_SPEED_SUPER) {
			if (PORTPM_U1E & temp2)
				temp |= (u64)NV_BIT(USB_DEV_STAT_U1_ENABLED);
			if (PORTPM_U2E & temp2)
				temp |= (u64)NV_BIT(USB_DEV_STAT_U2_ENABLED);
		}
		msg_dbg(nvudc->dev, "Status = 0x%llx\n", temp);

	} else if ((RequestType & USB_RECIP_MASK) == USB_RECIP_INTERFACE) {

		msg_dbg(nvudc->dev, "Get status request Interface request\n");
		if (nvudc->gadget.speed == USB_SPEED_SUPER) {
			temp2 = ioread32(nvudc->mmio_reg_base + PORTPM);
			temp = USB_INTRF_STAT_FUNC_RW_CAP;
			if (PORTPM_FRWE & temp2)
				temp |= USB_INTRF_STAT_FUNC_RW;
			msg_dbg(nvudc->dev, "Status = 0x%llx\n", temp);
		} else
			temp = 0;
	} else if ((RequestType & USB_RECIP_MASK) == USB_RECIP_ENDPOINT) {

		u32 DCI = (index & USB_ENDPOINT_NUMBER_MASK)*2 + ((index &
					USB_DIR_IN) ? 1 : 0);
		msg_dbg(nvudc->dev,
		"Get status request endpoint request DCI = 0x%x\n", DCI);
		/* if device state is address state, index should be 0
		if device state is configured state, index should be an
		endpoint configured.*/

		if ((nvudc->device_state == USB_STATE_ADDRESS) && (DCI != 0)) {
			status = -EINVAL;
			goto get_status_error;
		}

		if (nvudc->device_state == USB_STATE_CONFIGURED) {
			struct ep_cx_s *p_ep_cx =
				(struct ep_cx_s *)nvudc->p_epcx + DCI;
			msg_dbg(nvudc->dev, "p_ep_cx->EPDWord0 = 0x%x\n",
				p_ep_cx->ep_dw0);
			if (XHCI_GETF(EP_CX_EP_STATE, p_ep_cx->ep_dw0) ==
					EP_STATE_DISABLED) {
				status = -EINVAL;
				goto get_status_error;
			}

			/* EP_STATE is not synced with SW and HW. Use EP_HALT
			 * and EP_PAUSE register to check the EP state */
			if (ioread32(nvudc->mmio_reg_base + EP_HALT)
				& (1 << DCI)) {
				temp = NV_BIT(USB_ENDPOINT_HALT);
				msg_dbg(nvudc->dev,
					"endpoint was halted = 0x%lx\n",
					(long unsigned int)temp);
			}

		}
	}

get_status_error:
	if (status != -EINPROGRESS)
		udc_req_ptr->usb_req.length = 0;
	else {
		udc_req_ptr->usb_req.buf = &nvudc->statusbuf;
		*(u16 *)udc_req_ptr->usb_req.buf = cpu_to_le64(temp);
		msg_dbg(nvudc->dev, "usb_req.buf = 0x%x\n",
				*((u16 *)udc_req_ptr->usb_req.buf));
		if (index == USB_DEVICE_OTG_STATUS_SELECTOR)
			udc_req_ptr->usb_req.length = 1;
		else
			udc_req_ptr->usb_req.length = 2;
	}
	udc_req_ptr->usb_req.status = status;
	udc_req_ptr->usb_req.actual = 0;
	udc_req_ptr->usb_req.complete = NULL;

	if (udc_req_ptr->usb_req.dma == DMA_ADDR_INVALID) {
		udc_req_ptr->usb_req.dma =
			dma_map_single(nvudc->gadget.dev.parent,
					udc_req_ptr->usb_req.buf,
					udc_req_ptr->usb_req.length,
					DMA_FROM_DEVICE);
		udc_req_ptr->mapped = 1;
	}


	udc_ep_ptr = &nvudc->udc_ep[0];

	nvudc->setup_status = DATA_STAGE_XFER;
	status = nvudc_build_td(udc_ep_ptr, udc_req_ptr);

	if (!status) {
		list_add_tail(&udc_req_ptr->queue, &udc_ep_ptr->queue);
	}
}

void nvudc_set_sel_cmpl(struct usb_ep *ep, struct usb_request *req)
{
	struct nv_udc_ep *udc_ep;
	struct nv_udc_s	 *nvudc;

	udc_ep = container_of(ep, struct nv_udc_ep, usb_ep);
	nvudc = udc_ep->nvudc;
	msg_entry(nvudc->dev);

	msg_dbg(nvudc->dev, "u1_sel_value = 0x%x, u2_sel_value = 0x%x\n",
			nvudc->sel_value.u1_sel_value,
			nvudc->sel_value.u2_sel_value);
	msg_exit(nvudc->dev);
}

void setselrequest(struct nv_udc_s *nvudc, u16 value, u16 index, u16 length,
		u64 data)
{
	int status = -EINPROGRESS;
	struct nv_udc_request *udc_req_ptr = nvudc->status_req;
	struct nv_udc_ep *udc_ep_ptr = &nvudc->udc_ep[0];

	if (!udc_req_ptr) {
		msg_exit(nvudc->dev);
		return;
	}

	if (nvudc->device_state == USB_STATE_DEFAULT)
		status = -EINVAL;

	if ((index != 0) || (value != 0) || (length != 6))
		status = -EINVAL;

	if (status != -EINPROGRESS) {
		/* ??? send status stage?
		   udc_req_ptr->usb_req.length = 0; */
	} else {
		udc_req_ptr->usb_req.length = length;
		(nvudc->sel_value).u2_pel_value = 0;
		(nvudc->sel_value).u2_sel_value = 0;
		(nvudc->sel_value).u1_pel_value = 0;
		(nvudc->sel_value).u1_sel_value = 0;
		udc_req_ptr->usb_req.buf = &nvudc->sel_value;
	}

	udc_req_ptr->usb_req.status = -EINPROGRESS;
	udc_req_ptr->usb_req.actual = 0;
	udc_req_ptr->usb_req.complete = nvudc_set_sel_cmpl;

	if (udc_req_ptr->usb_req.dma == DMA_ADDR_INVALID) {
		udc_req_ptr->usb_req.dma =
			dma_map_single(nvudc->gadget.dev.parent,
					udc_req_ptr->usb_req.buf,
					udc_req_ptr->usb_req.length,
					DMA_TO_DEVICE);
		udc_req_ptr->mapped = 1;
	}

	status = nvudc_build_td(udc_ep_ptr, udc_req_ptr);

	if (!status) {
		list_add_tail(&udc_req_ptr->queue, &udc_ep_ptr->queue);
	}
}

void set_isoch_delay(struct nv_udc_s *nvudc, u16 value, u16 index, u16 length,
		u16 seq_num)
{
	int status = -EINPROGRESS;
	if ((value > 65535) || (index != 0) || (length != 0))
		status = -EINVAL;

	nvudc->iso_delay = value;
	nvudc->setup_status = STATUS_STAGE_XFER;
	build_ep0_status(&nvudc->udc_ep[0], true, status, NULL);
}

void set_address_cmpl(struct nv_udc_s *nvudc)
{
	if ((nvudc->device_state == USB_STATE_DEFAULT) &&
				nvudc->dev_addr != 0) {
		nvudc->device_state = USB_STATE_ADDRESS;
		msg_dbg(nvudc->dev, "USB State Addressed\n");

	} else if (nvudc->device_state == USB_STATE_ADDRESS) {
		if (nvudc->dev_addr == 0)
			nvudc->device_state = USB_STATE_DEFAULT;
	}
}

void setaddressrequest(struct nv_udc_s *nvudc, u16 value, u16 index, u16 length,
		u16 seq_num)
{
	int status = -EINPROGRESS;
	struct ep_cx_s *p_epcx = nvudc->p_epcx;
	u32 reg;

	if ((value > 127) || (index != 0) || (length != 0)) {
		status = -EINVAL;
		goto set_address_error;
	}

	if (((nvudc->device_state == USB_STATE_DEFAULT) && value != 0) ||
			(nvudc->device_state == USB_STATE_ADDRESS)) {
		nvudc->dev_addr = value;
		reg = ioread32(nvudc->mmio_reg_base + CTRL);
		reg &= ~CTRL_DEVADDR(-1);
		reg |= CTRL_DEVADDR(nvudc->dev_addr);
		iowrite32(reg, nvudc->mmio_reg_base + CTRL);

		XHCI_SETF_VAR(
			EP_CX_DEVADDR, p_epcx->ep_dw11, nvudc->dev_addr);
	} else
		status = -EINVAL;


set_address_error:
	msg_dbg(nvudc->dev, "build_ep0_status for Address Device\n");

	nvudc->setup_status = STATUS_STAGE_XFER;
	nvudc->setup_fn_call_back = &set_address_cmpl;
	build_ep0_status(&nvudc->udc_ep[0], true, status, NULL);
}


void nvudc_handle_setup_pkt(struct nv_udc_s *nvudc,
		struct usb_ctrlrequest *setup_pkt, u16 seq_num)
{
	u16 wValue = setup_pkt->wValue;
	u16 wIndex = setup_pkt->wIndex;
	u16 wLength = setup_pkt->wLength;
	u64 wData = 0;
	u32 u_temp;

	nvudc->ctrl_seq_num = seq_num;

	/* clear halt for ep0 when new setup request coming. */
	u_temp = ioread32(nvudc->mmio_reg_base + EP_HALT);
	if (u_temp & 1) {
		msg_dbg(nvudc->dev,
			"EP0 is halted when new setup request coming\n");
		u_temp &= ~1;
		iowrite32(u_temp, nvudc->mmio_reg_base + EP_HALT);

		poll_stchg(nvudc->dev, "handle_setup_pkt():", 1);
		iowrite32(1, nvudc->mmio_reg_base + EP_STCHG);
	}

	msg_dbg(nvudc->dev,
		"bRequest=%d, wValue=0x%.4x, wIndex=%d, wLength=%d\n",
		setup_pkt->bRequest, wValue, wIndex, wLength);

	if (XUDC_IS_T210(nvudc)) {
		/* war for ctrl request with seq_num = 0xfffe or 0xffff */
		if (seq_num == 0xfffe || seq_num == 0xffff) {
			set_ep_halt(nvudc, 0,
				"war: ctrl seq_num = 0xfffe/0xffff");
			nvudc->setup_status = WAIT_FOR_SETUP;
			return;
		}
	}

	nvudc->setup_status = SETUP_PKT_PROCESS_IN_PROGRESS;
	nvudc->setup_fn_call_back = NULL;
	if ((setup_pkt->bRequestType & USB_TYPE_MASK) == USB_TYPE_STANDARD) {
		switch (setup_pkt->bRequest) {
		case USB_REQ_GET_STATUS:
			msg_dbg(nvudc->dev, "USB_REQ_GET_STATUS\n");
			if ((setup_pkt->bRequestType & (USB_DIR_IN |
							USB_TYPE_MASK))
				!= (USB_DIR_IN | USB_TYPE_STANDARD)) {
				nvudc->setup_status = WAIT_FOR_SETUP;
				return;
			}

			getstatusrequest(nvudc, setup_pkt->bRequestType,
					wValue, wIndex, wLength);
			return;
		case USB_REQ_SET_ADDRESS:
			msg_dbg(nvudc->dev, "USB_REQ_SET_ADDRESS\n");
			if (setup_pkt->bRequestType != (USB_DIR_OUT |
						USB_RECIP_DEVICE |
						USB_TYPE_STANDARD)) {
				nvudc->setup_status = WAIT_FOR_SETUP;
				return;
			}

			setaddressrequest(nvudc, wValue, wIndex, wLength,
					seq_num);
			return;
		case USB_REQ_SET_SEL:
			msg_dbg(nvudc->dev, "USB_REQ_SET_SEL\n");

			if (setup_pkt->bRequestType != (USB_DIR_OUT |
						USB_RECIP_DEVICE |
						USB_TYPE_STANDARD)) {
				nvudc->setup_status = WAIT_FOR_SETUP;
				return;
			}

			nvudc->setup_status = DATA_STAGE_RECV;
			setselrequest(nvudc, wValue, wIndex, wLength, wData);
			return;
		case USB_REQ_SET_ISOCH_DELAY:
			if (setup_pkt->bRequestType != (USB_DIR_OUT |
						USB_RECIP_DEVICE |
						USB_TYPE_STANDARD))
				break;
			msg_dbg(nvudc->dev, "USB_REQ_SET_ISOCH_DELAY\n");
			set_isoch_delay(nvudc, wValue, wIndex, wLength,
					seq_num);
			return;

		case USB_REQ_CLEAR_FEATURE:
		case USB_REQ_SET_FEATURE:
			msg_dbg(nvudc->dev, "USB_REQ_CLEAR/SET_FEATURE\n");

			/* Need composite gadget driver
			to process the function remote wakeup request */
			if (setfeaturesrequest(nvudc, setup_pkt->bRequestType,
						setup_pkt->bRequest,
					wValue, wIndex, wLength)) {
				/* Get here if request has been processed. */
				return;
			}
			break;
		case USB_REQ_SET_CONFIGURATION:
			/* In theory we need to clear RUN bit before
			status stage of deconfig request sent.
			But seeing problem if we do it before all the
			endpoints belonging to the configuration
			disabled. */
			/* FALL THROUGH */
		default:
			msg_dbg(nvudc->dev,
			 "USB_REQ default bRequest=%d, bRequestType=%d\n",
			 setup_pkt->bRequest, setup_pkt->bRequestType);

#ifndef HUI_XXX
			if (setup_pkt->bRequest == 0xFF)
				nvudc->dbg_cnt1 = 1;
#endif
			break;
		}
	}


	if (wLength) {
		/* data phase from gadget like GET_CONFIGURATION
		   call the setup routine of gadget driver.
		   remember the request direction. */
		nvudc->setup_status =  (setup_pkt->bRequestType & USB_DIR_IN) ?
			DATA_STAGE_XFER :  DATA_STAGE_RECV;
	}

	spin_unlock(&nvudc->lock);
	if (nvudc->driver->setup(&nvudc->gadget, setup_pkt) < 0) {
		spin_lock(&nvudc->lock);

		set_ep_halt(nvudc, 0, "setup request failed");
		nvudc->setup_status = WAIT_FOR_SETUP;
		return;
	}
	spin_lock(&nvudc->lock);
}

void nvudc_reset(struct nv_udc_s *nvudc)
{
	u32 i, u_temp;
	struct nv_udc_ep *udc_ep_ptr;
	struct ep_cx_s *p_ep_cx;
	dma_addr_t dqptaddr;
	nvudc->setup_status = WAIT_FOR_SETUP;
	/* Base on Figure 9-1, default USB_STATE is attached */
	nvudc->device_state = USB_STATE_ATTACHED;

	msg_entry(nvudc->dev);

	/* clear pause for all the endpoints */
	u_temp = ioread32(nvudc->mmio_reg_base + EP_PAUSE);
	iowrite32(0, nvudc->mmio_reg_base + EP_PAUSE);
	poll_stchg(nvudc->dev, "nvudc_reset", u_temp);
	iowrite32(u_temp, nvudc->mmio_reg_base + EP_STCHG);

	msg_dbg(nvudc->dev, "EPSTCHG2 = 0x%x\n",
			ioread32(nvudc->mmio_reg_base + EP_STCHG));

	for (i = 2; i < 32; i++) {
		udc_ep_ptr = &nvudc->udc_ep[i];
		p_ep_cx = nvudc->p_epcx + i;

		if (udc_ep_ptr->desc)
			nuke(udc_ep_ptr, -ESHUTDOWN);
		udc_ep_ptr->tran_ring_full = false;
	}

	/* Complete any reqs on EP0 queue */
	udc_ep_ptr = &nvudc->udc_ep[0];
	nuke(udc_ep_ptr, -ESHUTDOWN);

	p_ep_cx = nvudc->p_epcx;

	/* Reset the sequence number and dequeue pointer
	   So HW can flush the transfer ring. */
	XHCI_SETF_VAR(EP_CX_SEQ_NUM, p_ep_cx->ep_dw5, 0);

	dqptaddr = tran_trb_virt_to_dma(udc_ep_ptr, udc_ep_ptr->enq_pt);

	p_ep_cx->ep_dw2 = lower_32_bits(dqptaddr);
	XHCI_SETF_VAR(
		EP_CX_DEQ_CYC_STATE, p_ep_cx->ep_dw2, udc_ep_ptr->pcs);
	p_ep_cx->ep_dw3 = upper_32_bits(dqptaddr);

	udc_ep_ptr->deq_pt = udc_ep_ptr->enq_pt;

	/* reload EP0 */
	iowrite32(1, nvudc->mmio_reg_base + EP_RELOAD);
	poll_reload(nvudc->dev, "nvudc_reset()", 1);

	/* clear the PAUSE bit */
	u_temp = ioread32(nvudc->mmio_reg_base + EP_PAUSE);
	u_temp &= ~1;
	iowrite32(u_temp, nvudc->mmio_reg_base + EP_PAUSE);

	poll_stchg(nvudc->dev, "nvudc_reset()", 1);

	iowrite32(1, nvudc->mmio_reg_base + EP_STCHG);

	nvudc->ctrl_req_enq_idx = 0;
	memset(nvudc->ctrl_req_queue, 0,
			sizeof(struct nv_setup_packet) * CTRL_REQ_QUEUE_DEPTH);
	if (nvudc->driver) {
		msg_dbg(nvudc->dev, "calling disconnect\n");
		if (get_gadget_data(&nvudc->gadget) == NULL) {
			msg_dbg(nvudc->dev, "no cdev skipping disconnect\n");
		} else {
			spin_unlock(&nvudc->lock);
			nvudc->driver->disconnect(&nvudc->gadget);
			spin_lock(&nvudc->lock);
		}
	}
	msg_exit(nvudc->dev);
}

void dbg_print_rings(struct nv_udc_s *nvudc)
{
	u32 i;
	struct event_trb_s *temp_trb;
	msg_dbg(nvudc->dev, "Event Ring Segment 0\n");
	temp_trb = (struct event_trb_s *)nvudc->event_ring0.vaddr;
	for (i = 0; i < (EVENT_RING_SIZE + 5); i++) {
		msg_dbg(nvudc->dev, "0x%p: 0x%x, 0x%x, 0x%x, 0x%x\n",
				temp_trb, temp_trb->trb_pointer_lo,
				temp_trb->trb_pointer_hi,
				temp_trb->eve_trb_dword2,
				temp_trb->eve_trb_dword3);
		temp_trb = temp_trb + 1;
	}

	msg_dbg(nvudc->dev, "Event Ring Segment 1\n");
	temp_trb = (struct event_trb_s *)nvudc->event_ring1.vaddr;
	for (i = 0; i < (EVENT_RING_SIZE + 5); i++) {
		msg_dbg(nvudc->dev, "0x%p: 0x%x, 0x%x, 0x%x, 0x%x\n",
				temp_trb, temp_trb->trb_pointer_lo,
				temp_trb->trb_pointer_hi,
				temp_trb->eve_trb_dword2,
				temp_trb->eve_trb_dword3);
		temp_trb = temp_trb + 1;
	}
}

void dbg_print_ep_ctx(struct nv_udc_s *nvudc)
{
	u32 i, j;
	struct ep_cx_s *p_epcx_temp = nvudc->p_epcx;
	struct transfer_trb_s *temp_trb1;

	msg_dbg(nvudc->dev, "ENDPOINT CONTEXT\n");
	for (i = 0; i < 32; i++) {
		if (p_epcx_temp->ep_dw0) {
			msg_dbg(nvudc->dev, "endpoint %d\n", i);
			msg_dbg(nvudc->dev, "0x%x 0x%x 0x%x 0x%x\n",
					p_epcx_temp->ep_dw0,
					p_epcx_temp->ep_dw1,
					p_epcx_temp->ep_dw2,
					p_epcx_temp->ep_dw3);
			msg_dbg(nvudc->dev, "0x%x 0x%x 0x%x 0x%x\n",
					p_epcx_temp->ep_dw4,
					p_epcx_temp->ep_dw5,
					p_epcx_temp->ep_dw6,
					p_epcx_temp->ep_dw7);
			msg_dbg(nvudc->dev, "0x%x 0x%x 0x%x 0x%x\n",
					p_epcx_temp->ep_dw8,
					p_epcx_temp->ep_dw9,
					p_epcx_temp->ep_dw10,
					p_epcx_temp->ep_dw11);
			msg_dbg(nvudc->dev, "0x%x 0x%x 0x%x 0x%x\n",
					p_epcx_temp->ep_dw12,
					p_epcx_temp->ep_dw13,
					p_epcx_temp->ep_dw14,
					p_epcx_temp->ep_dw15);

		}
		p_epcx_temp++;
	}

	msg_dbg(nvudc->dev, "Transfer ring for each endpoint\n");
	for (i = 0; i < 32; i++) {
		struct nv_udc_ep *udc_ep = &nvudc->udc_ep[i];
		if (udc_ep->tran_ring_ptr) {
			struct nv_buffer_info_s *ring = &udc_ep->tran_ring_info;
			int ring_size =
				ring->len / sizeof(struct transfer_trb_s);

			msg_dbg(nvudc->dev, "endpoint DCI = %d, %p 0x%llx\n",
				nvudc->udc_ep[i].DCI,
				ring->vaddr, (u64) ring->dma);
			temp_trb1 =
			(struct transfer_trb_s *) udc_ep->tran_ring_ptr;
			for (j = 0; j < ring_size; j++) {
				msg_dbg(nvudc->dev,
					"0x%lx: 0x%x 0x%x 0x%x 0x%x\n",
						(unsigned long)temp_trb1,
						temp_trb1->data_buf_ptr_lo,
						temp_trb1->data_buf_ptr_hi,
						temp_trb1->trb_dword2,
						temp_trb1->trb_dword3);
				temp_trb1++;
			}
		}
	}
}

static ssize_t debug_store(struct device *_dev, struct device_attribute *attr,
	const char *buf, size_t size)
{
	struct nv_udc_s *nvudc = the_controller;

	if (sysfs_streq(buf, "show_evtr"))
		dbg_print_rings(nvudc);

	if (sysfs_streq(buf, "show_epc"))
		dbg_print_ep_ctx(nvudc);

	return size;
}
static DEVICE_ATTR(debug, S_IWUSR, NULL, debug_store);

static void portpm_config_war(struct nv_udc_s *nvudc)
{
	u32 portsc_value = ioread32(nvudc->mmio_reg_base + PORTSC);
	u32 reg_portpm;

	if (!u1_enable || !u2_enable) {
		reg_portpm = ioread32(nvudc->mmio_reg_base + PORTPM);
		if (!u1_enable)
			reg_portpm &= ~PORTPM_U1TIMEOUT(-1);
		if (!u2_enable)
			reg_portpm &= ~PORTPM_U2TIMEOUT(-1);
		iowrite32(reg_portpm, nvudc->mmio_reg_base + PORTPM);
	}

	if (((portsc_value >> PORTSC_PS_SHIFT) & PORTSC_PS_MASK)
			<= USB_SPEED_HIGH) {
		reg_portpm = ioread32(nvudc->mmio_reg_base + PORTPM);
		reg_portpm &= ~PORTPM_L1S(-1);

		if (disable_lpm)
			reg_portpm |= PORTPM_L1S(2);
		else
			reg_portpm |= PORTPM_L1S(1);

		iowrite32(reg_portpm, nvudc->mmio_reg_base + PORTPM);
		msg_dbg(nvudc->dev, "XHCI_PORTPM = %x\n",
			ioread32(nvudc->mmio_reg_base + PORTPM));
	}
}

bool nvudc_handle_port_status(struct nv_udc_s *nvudc)
{
	u32 u_temp, u_temp2;
	bool update_ptrs = true;

	u_temp = ioread32(nvudc->mmio_reg_base + PORTSC);
	msg_dbg(nvudc->dev, "PortSC= 0x%.8x\n", u_temp);

	if ((PORTSC_PRC & u_temp) &&
			(PORTSC_PR & u_temp)) {
		/* first port status change event for port reset*/
		msg_dbg(nvudc->dev, "PRC and PR both are set\n");
		/* clear PRC */
		u_temp2 = u_temp & PORTSC_MASK;
		u_temp2 |= PORTSC_PRC;
		u_temp2 |= PORTSC_PED;
		iowrite32(u_temp2, nvudc->mmio_reg_base + PORTSC);
#define TOGGLE_VBUS_WAIT_MS	100
		schedule_delayed_work(&nvudc->port_reset_war_work,
			msecs_to_jiffies(TOGGLE_VBUS_WAIT_MS));
		nvudc->wait_for_sec_prc = 1;
	}

	if ((PORTSC_PRC & u_temp) &&
			(!(PORTSC_PR & u_temp))) {
		msg_dbg(nvudc->dev, "PR completed. PRC is set, but PR is not\n");

		u_temp2 = (u_temp >> PORTSC_PS_SHIFT) & PORTSC_PS_MASK;
		if (u_temp2 <= USB_SPEED_FULL)
			nvudc->gadget.speed = USB_SPEED_FULL;
		else if (u_temp2 == USB_SPEED_HIGH)
			nvudc->gadget.speed = USB_SPEED_HIGH;
		else
			nvudc->gadget.speed = USB_SPEED_SUPER;
		msg_dbg(nvudc->dev, "gadget speed = 0x%x\n",
				nvudc->gadget.speed);

		nvudc_reset(nvudc);
		u_temp2 = u_temp & PORTSC_MASK;
		u_temp2 |= PORTSC_PRC;
		u_temp2 |= PORTSC_PED;
		iowrite32(u_temp2, nvudc->mmio_reg_base + PORTSC);
		nvudc->device_state = USB_STATE_DEFAULT;

		msg_dbg(nvudc->dev, "PORTSC = 0x%x\n",
				ioread32(nvudc->mmio_reg_base + PORTSC));

		portpm_config_war(nvudc);

		/* clear run change bit to enable door bell */
		u_temp2 = ioread32(nvudc->mmio_reg_base + ST);
		if (ST_RC & u_temp2)
			iowrite32(ST_RC, nvudc->mmio_reg_base + ST);

		cancel_delayed_work(&nvudc->port_reset_war_work);
		nvudc->wait_for_sec_prc = 0;
	}

	u_temp = ioread32(nvudc->mmio_reg_base + PORTHALT);
	if (PORTHALT_STCHG_REQ & u_temp) {

		msg_dbg(nvudc->dev, "STCHG_REQ is set, port_halt_reg=0x%.8x\n",
				u_temp);

#define PORTHALT_MASK   0x071F0003
		u_temp2 = u_temp & PORTHALT_MASK;
		u_temp2 |= PORTHALT_STCHG_REQ;

		if (PORTHALT_HALT_LTSSM & u_temp2) {
			u_temp2 &= ~PORTHALT_HALT_LTSSM;
			iowrite32(u_temp2, nvudc->mmio_reg_base + PORTHALT);
			msg_dbg(nvudc->dev,
				"STCHG_REQ_CLEAR, PORTSCHALT = 0x%x\n",
				(u32)ioread32(nvudc->mmio_reg_base + PORTHALT));
		}
	}

	u_temp = ioread32(nvudc->mmio_reg_base + PORTSC);
	if ((PORTSC_WRC & u_temp) && (PORTSC_WPR & u_temp)) {
		/* first port status change event for port reset*/
		msg_dbg(nvudc->dev, "WRC and WPR both are set\n");
		/* clear WRC */
		u_temp2 = u_temp & PORTSC_MASK;
		u_temp2 |= PORTSC_WRC;
		u_temp2 |= PORTSC_PED;
		iowrite32(u_temp2, nvudc->mmio_reg_base + PORTSC);

	}

	if ((PORTSC_WRC & u_temp) &&
			(!(PORTSC_WPR & u_temp))) {

		msg_dbg(nvudc->dev, "WRC is set, but WPR is not\n");
		nvudc_reset(nvudc);
		u_temp2 = u_temp & PORTSC_MASK;
		u_temp2 |= PORTSC_WRC;
		u_temp2 |= PORTSC_PED;
		iowrite32(u_temp2, nvudc->mmio_reg_base + PORTSC);
		nvudc->device_state = USB_STATE_DEFAULT;

		msg_dbg(nvudc->dev, "PORTSC = 0x%x\n",
				ioread32(nvudc->mmio_reg_base + PORTSC));

		portpm_config_war(nvudc);

		/* clear run change bit to enable door bell */
		u_temp2 = ioread32(nvudc->mmio_reg_base + ST);
		if (ST_RC & u_temp2) {
			msg_dbg(nvudc->dev, "clear RC\n");
			iowrite32(ST_RC, nvudc->mmio_reg_base + ST);
		}

	}

	if (PORTSC_CSC & u_temp) {

		msg_dbg(nvudc->dev, "CSC is set\n");
		if (PORTSC_CCS & u_temp) {

			msg_dbg(nvudc->dev, "CCS is set\n");
			u_temp2 = (u_temp >> PORTSC_PS_SHIFT) & PORTSC_PS_MASK;
			if (u_temp2 <= USB_SPEED_FULL)
				nvudc->gadget.speed = USB_SPEED_FULL;
			else if (u_temp2 == USB_SPEED_HIGH)
				nvudc->gadget.speed = USB_SPEED_HIGH;
			else
				nvudc->gadget.speed = USB_SPEED_SUPER;
			msg_dbg(nvudc->dev, "gadget speed = 0x%x\n",
					nvudc->gadget.speed);
			nvudc->device_state = USB_STATE_DEFAULT;
			/* complete any requests on ep0 queue */
			nuke(&nvudc->udc_ep[0], -ESHUTDOWN);
			nvudc->setup_status = WAIT_FOR_SETUP;
			update_ep0_maxpacketsize(nvudc);

			portpm_config_war(nvudc);
		} else {
			msg_dbg(nvudc->dev, "disconnect\n");
			nvudc_reset(nvudc);

			/* clear run change bit to enable door bell */
			u_temp2 = ioread32(nvudc->mmio_reg_base + ST);
			if (ST_RC & u_temp2)
				iowrite32(ST_RC, nvudc->mmio_reg_base + ST);
		}

		u_temp2 = ioread32(nvudc->mmio_reg_base + PORTSC);
		u_temp2 = u_temp2 & PORTSC_MASK;
		u_temp2 |= PORTSC_CSC;
		iowrite32(u_temp2, nvudc->mmio_reg_base + PORTSC);

		if (nvudc->wait_for_csc) {
			cancel_delayed_work(&nvudc->plc_reset_war_work);
			nvudc->wait_for_csc = 0;
		}
	}

	if (PORTSC_PLC & u_temp) {

		msg_dbg(nvudc->dev, "PLC is set PORTSC= 0x%x\n",
				u_temp);
		/* Clear PLC first */
		u_temp2 = ioread32(nvudc->mmio_reg_base + PORTSC);
		u_temp2 = u_temp2 & PORTSC_MASK;
		u_temp2 |= PORTSC_PLC;
		iowrite32(u_temp2, nvudc->mmio_reg_base + PORTSC);

		if ((u_temp2 & PORTSC_PLS_MASK) == XDEV_U3) {
#ifdef CONFIG_USB_OTG
			if (nvudc->phy)
				usb_phy_set_suspend(nvudc->phy, 1);
#endif
			msg_dbg(nvudc->dev, "PLS Suspend (U3)\n");
			nvudc->resume_state = nvudc->device_state;
			nvudc->device_state = USB_STATE_SUSPENDED;
			if (nvudc->driver->suspend) {
				spin_unlock(&nvudc->lock);
				nvudc->driver->suspend(&nvudc->gadget);
				spin_lock(&nvudc->lock);
			}
		} else if ((((u_temp2 & PORTSC_PLS_MASK) == XDEV_RESUME)
			&& (nvudc->gadget.speed == USB_SPEED_SUPER)) ||
			(((u_temp & PORTSC_PLS_MASK) == XDEV_U0)
			&& (nvudc->gadget.speed < USB_SPEED_SUPER))) {
#ifdef CONFIG_USB_OTG
			if (nvudc->phy)
				usb_phy_set_suspend(nvudc->phy, 0);
#endif
			msg_dbg(nvudc->dev, "PLS Resume\n");

			nvudc_resume_state(nvudc, 0);

			if (nvudc->driver->resume) {
				spin_unlock(&nvudc->lock);
				nvudc->driver->resume(&nvudc->gadget);
				spin_lock(&nvudc->lock);
			}
		} else if ((u_temp2 & PORTSC_PLS_MASK) == XDEV_INACTIVE) {
			schedule_delayed_work(&nvudc->plc_reset_war_work,
				msecs_to_jiffies(TOGGLE_VBUS_WAIT_MS));
			nvudc->wait_for_csc = 1;
		}
	}

	if (PORTSC_CEC & u_temp) {
		msg_dbg(nvudc->dev, "CEC is set PORTSC = 0x%x\n", u_temp);
		u_temp2 = ioread32(nvudc->mmio_reg_base + PORTSC);
		u_temp2 &= PORTSC_MASK;
		u_temp2 |= PORTSC_CEC;
		iowrite32(u_temp2, nvudc->mmio_reg_base + PORTSC);
	}

	return update_ptrs;
}

void process_event_ring(struct nv_udc_s *nvudc)
{
	struct event_trb_s *event;
	nvudc->num_evts_processed = 0;
	while (nvudc->evt_dq_pt) {
		event = (struct event_trb_s *)nvudc->evt_dq_pt;

		if (XHCI_GETF(EVE_TRB_CYCLE_BIT, event->eve_trb_dword3) !=
				nvudc->CCS)
			break;
		else {
			nvudc->num_evts_processed++;
			nvudc_handle_event(nvudc, event);

			if (event == nvudc->evt_seg0_last_trb)
				nvudc->evt_dq_pt = nvudc->event_ring1.vaddr;
			else if (event == nvudc->evt_seg1_last_trb) {
				msg_dbg(nvudc->dev,
					"evt_seg1_last_trb = 0x%p\n",
					nvudc->evt_seg1_last_trb);
				msg_dbg(nvudc->dev,
					"evt_dq_pt = 0x%p\n", nvudc->evt_dq_pt);
				nvudc->CCS = nvudc->CCS ? 0 : 1;
				nvudc->evt_dq_pt = nvudc->event_ring0.vaddr;
				msg_dbg(nvudc->dev,
				"wrap Event dq_pt to Event ring segment 0\n");
			} else
				nvudc->evt_dq_pt++;
		}
	}
}

void queue_setup_pkt(struct nv_udc_s *nvudc, struct usb_ctrlrequest *setup_pkt,
			u16 seq_num)
{
	msg_entry(nvudc->dev);
	if (nvudc->ctrl_req_enq_idx == CTRL_REQ_QUEUE_DEPTH) {
		msg_dbg(nvudc->dev, "ctrl request queque is full\n");
		return;
	}

	memcpy(&(nvudc->ctrl_req_queue[nvudc->ctrl_req_enq_idx].usbctrlreq),
			setup_pkt, sizeof(struct usb_ctrlrequest));
	nvudc->ctrl_req_queue[nvudc->ctrl_req_enq_idx].seqnum = seq_num;
	nvudc->ctrl_req_enq_idx++;
	msg_exit(nvudc->dev);
}

void nvudc_handle_event(struct nv_udc_s *nvudc, struct event_trb_s *event)
{

	msg_entry(nvudc->dev);

	switch (XHCI_GETF(EVE_TRB_TYPE, event->eve_trb_dword3)) {
	case TRB_TYPE_EVT_PORT_STATUS_CHANGE:
		nvudc_handle_port_status(nvudc);
		break;
	case TRB_TYPE_EVT_TRANSFER:
		nvudc_handle_exfer_event(nvudc, event);
		break;
	case TRB_TYPE_EVT_SETUP_PKT:
		{
			struct usb_ctrlrequest *setup_pkt;
			u16 seq_num;
			msg_dbg(nvudc->dev, "nvudc_handle_setup_pkt\n");

			cancel_delayed_work(&nvudc->non_std_charger_work);
			setup_pkt = (struct usb_ctrlrequest *)
					&event->trb_pointer_lo;

			seq_num = XHCI_GETF(EVE_TRB_SEQ_NUM,
					event->eve_trb_dword2);
			msg_dbg(nvudc->dev, "setup_pkt = 0x%p, seq_num = %d\n",
				setup_pkt, seq_num);
			if (nvudc->setup_status != WAIT_FOR_SETUP) {
				/*previous setup packet hasn't
				completed yet.save the new one in a
				software queue.*/
				queue_setup_pkt(nvudc, setup_pkt, seq_num);
				break;
			}
			nvudc_handle_setup_pkt(nvudc, setup_pkt, seq_num);

			break;
		}
	default:
		msg_dbg(nvudc->dev, "TRB_TYPE = 0x%x",
			XHCI_GETF(EVE_TRB_TYPE, event->eve_trb_dword3));
		break;
	}

	msg_exit(nvudc->dev);
}

static irqreturn_t nvudc_irq(int irq, void *_udc)
{
	struct nv_udc_s *nvudc = (struct nv_udc_s *)_udc;
	u32 u_temp;
	unsigned long flags;
	dma_addr_t erdp;
	u32 reg;

	u_temp = ioread32(nvudc->mmio_reg_base + ST);
	if (!(ST_IP & u_temp))
		return IRQ_NONE;

	msg_entry(nvudc->dev);
	msg_dbg(nvudc->dev, "Download_events nvudc->evt_dq_pt = 0x%p\n",
				nvudc->evt_dq_pt);

	spin_lock_irqsave(&nvudc->lock, flags);

	u_temp = 0;
	u_temp |= ST_IP;
	iowrite32(u_temp, nvudc->mmio_reg_base + ST);

	process_event_ring(nvudc);

	/* update dequeue pointer */
	erdp = event_trb_virt_to_dma(nvudc, nvudc->evt_dq_pt);
	reg =  upper_32_bits(erdp);
	iowrite32(reg, nvudc->mmio_reg_base + ERDPHI);
	reg = lower_32_bits(erdp);
	reg |= ERDPLO_EHB;
	iowrite32(reg, nvudc->mmio_reg_base + ERDPLO);

	spin_unlock_irqrestore(&nvudc->lock, flags);

	msg_exit(nvudc->dev);
	return IRQ_HANDLED;
}

#if defined(CONFIG_ARCH_TEGRA_21x_SOC)
static irqreturn_t nvudc_padctl_irq(int irq, void *data)
{
	struct nv_udc_s *nvudc = (struct nv_udc_s *) data;
	u32 reg, irq_for_dev;

	reg = tegra_usb_pad_reg_read(XUSB_PADCTL_ELPG_PROGRAM_0);
	reg &= XUSB_ALL_WAKE_EVENT;

	irq_for_dev = (reg & USB2_PORT0_WAKEUP_EVENT) |
			(reg & (SS_PORT_WAKEUP_EVENT(nvudc->ss_port)));

	if (irq_for_dev) {
		tegra_xhci_hs_wake_on_interrupts(TEGRA_XUSB_USB2_P0, false);
		if (nvudc->is_ss_port_active)
			tegra_xhci_ss_wake_on_interrupts((1 << nvudc->ss_port)
					, false);

		/* Check if still have pending padctl irq event*/
		if (!(reg ^ irq_for_dev)) {
			dev_dbg(nvudc->dev, "IRQ event for device controller only\n");
			return IRQ_HANDLED;
		}
	}
	dev_dbg(nvudc->dev, "IRQ event for host controller as well\n");
	return IRQ_NONE;
}
#endif

void usbep_struct_setup(struct nv_udc_s *nvudc, u32 index, u8 *name)
{
	struct nv_udc_ep *ep = &nvudc->udc_ep[index];
	ep->DCI = index;

	if (index) {
		strcpy(ep->name, name);
		ep->usb_ep.name = ep->name;
		usb_ep_set_maxpacket_limit(&ep->usb_ep, 1024);
		ep->usb_ep.max_streams = 16;
	} else {

		ep->usb_ep.name = "ep0";
		usb_ep_set_maxpacket_limit(&ep->usb_ep, 64);
	}
	msg_dbg(nvudc->dev, "ep = %p, ep name = %s maxpacket = 0x%x\n",
			ep, ep->name, ep->usb_ep.maxpacket);
	ep->usb_ep.ops = &nvudc_ep_ops;
	ep->nvudc = nvudc;
#ifdef PRIME_NOT_RCVD_WAR
	ep->stream_rejected_retry_count = 0;
	INIT_DELAYED_WORK(&ep->work, retry_stream_rejected_work);
#endif
	INIT_LIST_HEAD(&ep->queue);
	if (index)
		list_add_tail(&ep->usb_ep.ep_list, &nvudc->gadget.ep_list);
}

static int nvudc_gadget_start(struct usb_gadget *gadget,
		struct usb_gadget_driver *driver)
{
	struct nv_udc_s *nvudc = the_controller;
	unsigned long flags;
	int retval = -ENODEV;
	u32 u_temp = 0;

	if (!nvudc)
		return -ENODEV;

	msg_entry(nvudc->dev);
	if (!driver || (driver->max_speed < USB_SPEED_FULL)
			|| !driver->setup || !driver->disconnect)
		return -EINVAL;

	msg_dbg(nvudc->dev, "Sanity check. speed = 0x%x\n",
			driver->max_speed);

	msg_dbg(nvudc->dev, "nvudc->driver = %p, driver = %p\n",
			nvudc->driver, driver);
	if (nvudc->driver)
		return -EBUSY;
	msg_dbg(nvudc->dev, "nvudc->driver is not assgined.\n");

	pm_runtime_get_sync(nvudc->dev);
	spin_lock_irqsave(&nvudc->lock, flags);
	driver->driver.bus = NULL;
	nvudc->driver = driver;
	spin_unlock_irqrestore(&nvudc->lock, flags);
	msg_dbg(nvudc->dev, "bind\n");

	retval = device_create_file(nvudc->dev, &dev_attr_debug);
	if (retval)
		goto err_unbind;

	nvudc->device_state = USB_STATE_ATTACHED;
	nvudc->setup_status = WAIT_FOR_SETUP;

	set_interrupt_moderation(nvudc, min_irq_interval_us);

	u_temp = ioread32(nvudc->mmio_reg_base + CTRL);
	u_temp |= CTRL_IE;
	u_temp |= CTRL_LSE;
	iowrite32(u_temp, nvudc->mmio_reg_base + CTRL);

	u_temp = ioread32(nvudc->mmio_reg_base + PORTHALT);
	u_temp = u_temp & PORTHALT_MASK;
	u_temp |= PORTHALT_STCHG_INTR_EN;
	iowrite32(u_temp, nvudc->mmio_reg_base + PORTHALT);

	if (nvudc->pullup) {
		/* set ENABLE bit */
		u_temp = ioread32(nvudc->mmio_reg_base + CTRL);
		u_temp |= CTRL_ENABLE;
		iowrite32(u_temp, nvudc->mmio_reg_base + CTRL);
	}

	if (!IS_ERR_OR_NULL(nvudc->phy)) {
		msg_dbg(nvudc->dev, "otg_set_peripheral to nvudc->gadget\n");
		otg_set_peripheral(nvudc->phy->otg, &nvudc->gadget);
	}

	pm_runtime_put_sync(nvudc->dev);
	msg_exit(nvudc->dev);
	return 0;
err_unbind:
	nvudc->driver = NULL;
	pm_runtime_put(nvudc->dev);
	msg_exit(nvudc->dev);
	return retval;
}

static int nvudc_gadget_stop(struct usb_gadget *gadget,
		struct usb_gadget_driver *driver)
{
	u32 u_temp;
	struct nv_udc_s *nvudc = the_controller;
	unsigned long flags;

	if (!nvudc)
		return -ENODEV;

	msg_entry(nvudc->dev);

	if (driver && (driver != nvudc->driver || !driver->unbind))
		return -EINVAL;

	pm_runtime_get_sync(nvudc->dev);

	spin_lock_irqsave(&nvudc->lock, flags);
	u_temp = ioread32(nvudc->mmio_reg_base + CTRL);
	u_temp &= ~CTRL_IE;
	u_temp &= ~CTRL_ENABLE;
	iowrite32(u_temp, nvudc->mmio_reg_base + CTRL);

	nvudc_reset(nvudc);

	/* clear ENABLE bit */
	u_temp = ioread32(nvudc->mmio_reg_base + CTRL);
	u_temp &= ~CTRL_ENABLE;
	iowrite32(u_temp, nvudc->mmio_reg_base + CTRL);

	reset_data_struct(nvudc);

	spin_unlock_irqrestore(&nvudc->lock, flags);

	nvudc->driver = NULL;

	if (!IS_ERR_OR_NULL(nvudc->phy)) {
		msg_dbg(nvudc->dev, "otg_set_peripheral to NULL\n");
		otg_set_peripheral(nvudc->phy->otg, NULL);
	}

	device_remove_file(nvudc->dev, &dev_attr_debug);
	pm_runtime_put(nvudc->dev);

	return 0;
}

u32 init_hw_event_ring(struct nv_udc_s *nvudc)
{
	u32 u_temp, buff_length;
	int retval = 0;
	dma_addr_t mapping;

	msg_entry(nvudc->dev);

#define EVENT_RING_SEGMENT_TABLE_SIZE   2
	/* One event ring segment won't work on T210 HW */
	u_temp = 0;
	u_temp |= SPARAM_ERSTMAX(EVENT_RING_SEGMENT_TABLE_SIZE);
	iowrite32(u_temp, nvudc->mmio_reg_base + SPARAM);

	u_temp = 0;
	u_temp |= ERSTSZ_ERST0SZ(EVENT_RING_SIZE);
	u_temp |= ERSTSZ_ERST1SZ(EVENT_RING_SIZE);
	iowrite32(u_temp, nvudc->mmio_reg_base + ERSTSZ);

	buff_length = EVENT_RING_SIZE * sizeof(struct event_trb_s);
	if (!(nvudc->event_ring0).vaddr) {
		(nvudc->event_ring0).vaddr =
			dma_alloc_coherent(nvudc->dev, buff_length,
						&mapping, GFP_ATOMIC);

		if ((nvudc->event_ring0).vaddr == NULL) {
			retval = -ENOMEM;
			return retval;
		}
	} else
		mapping = nvudc->event_ring0.dma;

	(nvudc->event_ring0).len = buff_length;
	(nvudc->event_ring0).dma = mapping;

	u_temp = 0;
	u_temp = lower_32_bits(mapping);
	iowrite32(u_temp, nvudc->mmio_reg_base + ERST0BALO);

	u_temp = upper_32_bits(mapping);
	iowrite32(u_temp, nvudc->mmio_reg_base + ERST0BAHI);

	buff_length = EVENT_RING_SIZE * sizeof(struct event_trb_s);
	if (!nvudc->event_ring1.vaddr) {
		(nvudc->event_ring1).vaddr =
			dma_alloc_coherent(nvudc->dev, buff_length,
						&mapping, GFP_ATOMIC);
		if ((nvudc->event_ring1).vaddr == NULL) {
			retval = -ENOMEM;
			return retval;
		}
	} else
		mapping = (nvudc->event_ring1).dma;

	(nvudc->event_ring1).len = buff_length;
	(nvudc->event_ring1).dma = mapping;

	u_temp = 0;
	u_temp = lower_32_bits(mapping);
	iowrite32(u_temp, nvudc->mmio_reg_base + ERST1BALO);

	u_temp = upper_32_bits(mapping);
	iowrite32(u_temp, nvudc->mmio_reg_base + ERST1BAHI);

	return 0;
}

u32 reset_data_struct(struct nv_udc_s *nvudc)
{
	int retval = 0;
	u32 u_temp = 0, buff_length = 0;
	dma_addr_t mapping;

	msg_entry(nvudc->dev);
	u_temp = ioread32(nvudc->mmio_reg_base + CTRL);

	iowrite32(PORTSC_CHANGE_MASK, nvudc->mmio_reg_base + PORTSC);

	retval = init_hw_event_ring(nvudc);
	if (retval)
		return retval;

	buff_length = EVENT_RING_SIZE * sizeof(struct event_trb_s);
	memset((void *)nvudc->event_ring0.vaddr, 0, buff_length);

	nvudc->evt_dq_pt = (nvudc->event_ring0).vaddr;
	nvudc->CCS = 1;

	mapping = nvudc->event_ring0.dma;
	u_temp = lower_32_bits(mapping);
	iowrite32(u_temp, nvudc->mmio_reg_base + ERDPLO);

	u_temp |= EREPLO_ECS;
	iowrite32(u_temp, nvudc->mmio_reg_base + EREPLO);
	msg_dbg(nvudc->dev, "XHCI_EREPLO = 0x%x\n", u_temp);

	u_temp = upper_32_bits(mapping);
	iowrite32(u_temp, nvudc->mmio_reg_base + ERDPHI);
	iowrite32(u_temp, nvudc->mmio_reg_base + EREPHI);
	msg_dbg(nvudc->dev, "XHCI_EREPHI = 0x%x\n", u_temp);

	memset((void *)(nvudc->event_ring1.vaddr), 0, buff_length);

	nvudc->evt_seg0_last_trb =
		(struct event_trb_s *)(nvudc->event_ring0.vaddr)
		+ (EVENT_RING_SIZE - 1);
	nvudc->evt_seg1_last_trb =
		(struct event_trb_s *)(nvudc->event_ring1.vaddr)
		+ (EVENT_RING_SIZE - 1);

	buff_length = NUM_EP_CX * sizeof(struct ep_cx_s);

	if (!nvudc->ep_cx.vaddr) {
		(nvudc->ep_cx).vaddr =
			dma_alloc_coherent(nvudc->dev, buff_length,
						&mapping, GFP_ATOMIC);
		if  ((nvudc->ep_cx).vaddr == NULL) {
			retval = -ENOMEM;
			return retval;
		}
	} else {
		mapping = nvudc->ep_cx.dma;
	}

	nvudc->p_epcx = (nvudc->ep_cx).vaddr;
	(nvudc->ep_cx).len = buff_length;
	(nvudc->ep_cx).dma = mapping;

	msg_dbg(nvudc->dev, "allocate ep_cx = %p\n", nvudc->p_epcx);
	u_temp = lower_32_bits(mapping);
	iowrite32(u_temp, nvudc->mmio_reg_base + ECPLO);


	msg_dbg(nvudc->dev, "allocate ep_cx XHCI_ECPLO= 0x%x\n",
			u_temp);

	u_temp = upper_32_bits(mapping);
	iowrite32(u_temp, nvudc->mmio_reg_base + ECPHI);

	msg_dbg(nvudc->dev, "allocate ep_cx XHCI_EPHI= 0x%x\n", u_temp);

	retval = init_ep0(nvudc);
	if (retval)
		return retval;

	if (!nvudc->status_req) {
		nvudc->status_req =
		container_of(nvudc_alloc_request(&nvudc->udc_ep[0].usb_ep,
			GFP_ATOMIC), struct nv_udc_request,
			usb_req);
	}

	nvudc->act_bulk_ep = 0;
	nvudc->g_isoc_eps = 0;
	nvudc->ctrl_seq_num = 0;
	nvudc->dev_addr = 0;
	nvudc->gadget.b_hnp_enable = 0;
	nvudc->gadget.rcvd_otg_hnp_reqd = 0;

	if (tegra_platform_is_fpga())
		fpga_hack_setup_vbus_sense_and_termination(nvudc);

	/* select HS/FS PI */
	u_temp = ioread32(nvudc->mmio_reg_base + CFG_DEV_FE);
	u_temp &= PORTREGSEL_MASK;
	u_temp |= CFG_DEV_FE_PORTREGSEL(2);
	iowrite32(u_temp, nvudc->mmio_reg_base + CFG_DEV_FE);

	/* set LWS = 1 and PLS = rx_detect */
	u_temp = ioread32(nvudc->mmio_reg_base + PORTSC);
	u_temp &= PORTSC_MASK;
	u_temp &= ~PORTSC_PLS(~0);
	u_temp |= PORTSC_PLS(5);
	u_temp |= PORTSC_LWS;
	iowrite32(u_temp, nvudc->mmio_reg_base + PORTSC);

	/* select SS PI */
	u_temp = ioread32(nvudc->mmio_reg_base + CFG_DEV_FE);
	u_temp &= PORTREGSEL_MASK;
	u_temp |= CFG_DEV_FE_PORTREGSEL(1);
	iowrite32(u_temp, nvudc->mmio_reg_base + CFG_DEV_FE);

	/* set LWS = 1 and PLS = rx_detect */
	u_temp = ioread32(nvudc->mmio_reg_base + PORTSC);
	u_temp &= PORTSC_MASK;
	u_temp &= ~PORTSC_PLS(~0);
	u_temp |= PORTSC_PLS(5);
	u_temp |= PORTSC_LWS;
	iowrite32(u_temp, nvudc->mmio_reg_base + PORTSC);

	/* restore portregsel */
	u_temp = ioread32(nvudc->mmio_reg_base + CFG_DEV_FE);
	u_temp &= PORTREGSEL_MASK;
	u_temp |= CFG_DEV_FE_PORTREGSEL(0);
	iowrite32(u_temp, nvudc->mmio_reg_base + CFG_DEV_FE);

	if (enable_fe_infinite_ss_retry) {
		/* enable FE_INFINITE_SS_RETRY */
		u_temp = ioread32(nvudc->mmio_reg_base + CFG_DEV_FE);
		u_temp |= CFG_DEV_FE_INFINITE_SS_RETRY;
		iowrite32(u_temp, nvudc->mmio_reg_base + CFG_DEV_FE);
	}

	/* increase SSPI transaction timeout from 32us to 512us */
	u_temp = ioread32(nvudc->mmio_reg_base + CFG_DEV_SSPI_XFER);
	u_temp &= ~(CFG_DEV_SSPI_XFER_ACKTIMEOUT_MASK <<
		CFG_DEV_SSPI_XFER_ACKTIMEOUT_SHIFT);
	u_temp |= (0xf000 & CFG_DEV_SSPI_XFER_ACKTIMEOUT_MASK) <<
		CFG_DEV_SSPI_XFER_ACKTIMEOUT_SHIFT;
	iowrite32(u_temp, nvudc->mmio_reg_base + CFG_DEV_SSPI_XFER);

	return 0;
}

static int nvudc_pci_get_irq_resources(struct nv_udc_s *nvudc)
{
	struct pci_dev *pdev = nvudc->pdev.pci;

	if (request_irq(pdev->irq, nvudc_irq, IRQF_SHARED,
				driver_name, nvudc) != 0) {
		msg_err(nvudc->dev, "Can't get irq resource\n");
		return -EBUSY;
	}

	nvudc->irq = nvudc->pdev.pci->irq;
	return 0;
}

static int nvudc_pci_get_memory_resources(struct nv_udc_s *nvudc)
{
	struct pci_dev *pdev = nvudc->pdev.pci;
	int len;

	nvudc->mmio_phys_base = pci_resource_start(pdev, 0);
	len = pci_resource_len(pdev, 0);

	if (!request_mem_region(nvudc->mmio_phys_base, len, driver_name)) {
		msg_err(nvudc->dev, "Can't get memory resources\n");
		return -EBUSY;
	}

	nvudc->mmio_reg_base = ioremap_nocache(nvudc->mmio_phys_base, len);
	if (nvudc->mmio_reg_base == NULL) {
		msg_err(nvudc->dev, "memory mapping failed\n");
		return -EFAULT;
	}

	nvudc->mmio_phys_len = len;
	return 0;
}

static int nvudc_probe_pci(struct pci_dev *pdev, const struct pci_device_id
		*id)
{
	int retval;
	u32 i;
	struct nv_udc_s *nvudc_dev;

	nvudc_dev = kzalloc(sizeof(struct nv_udc_s), GFP_ATOMIC);
	if  (nvudc_dev == NULL) {
		retval = -ENOMEM;
		goto error;
	}

	/* this needs to be done before using msg_ macros */
	nvudc_dev->dev = &pdev->dev;
	msg_entry(nvudc_dev->dev);

	spin_lock_init(&nvudc_dev->lock);
	pci_set_drvdata(pdev, nvudc_dev);
	msg_dbg(nvudc_dev->dev, "pci_set_drvdata\n");

	/* Initialize back pointer in udc_ep[i]. This is used for accessing
	 * nvudc from gadget api calls */
	for (i = 0; i < 32; i++)
		nvudc_dev->udc_ep[i].nvudc = nvudc_dev;

	nvudc_dev->pdev.pci = pdev;
	dev_set_name(&nvudc_dev->gadget.dev, "gadget");
	nvudc_dev->gadget.dev.parent = &pdev->dev;
	nvudc_dev->gadget.dev.release = nvudc_gadget_release;
	nvudc_dev->gadget.ops = &nvudc_gadget_ops;
	nvudc_dev->gadget.ep0 = &nvudc_dev->udc_ep[0].usb_ep;
	INIT_LIST_HEAD(&nvudc_dev->gadget.ep_list);
	nvudc_dev->gadget.max_speed = USB_SPEED_SUPER;
	nvudc_dev->gadget.name = driver_name;

	dev_info(&pdev->dev, "%s PCI XUSB Device Mode Controller\n",
				xudc_get_chip_name(nvudc_dev));

	init_chip(nvudc_dev);

	if (pci_enable_device(pdev) < 0) {
		retval = -ENODEV;
		goto error;
	}
	nvudc_dev->enabled = 1;
	/* Default value for pullup is 1*/
	nvudc_dev->pullup = 1;

	msg_dbg(nvudc_dev->dev, "pci device enabled\n");

	retval = nvudc_pci_get_memory_resources(nvudc_dev);
	if (retval < 0)
		goto error;

	msg_dbg(nvudc_dev->dev, "ioremap_nocache nvudc->mmio_reg_base = 0x%p\n",
			nvudc_dev->mmio_reg_base);

	retval = reset_data_struct(nvudc_dev);
	if (retval)
		goto error;

	retval = EP0_Start(nvudc_dev);
	if (retval)
		goto error;

	retval = nvudc_pci_get_irq_resources(nvudc_dev);
	if (retval < 0)
		goto error;

	msg_dbg(nvudc_dev->dev, "request_irq, irq = 0x%x\n", nvudc_dev->irq);
	/* Setting the BusMaster bit so that the Device be able to do DMA
	 * which is also required for the MSI interrupts to be generated
	 */
	pci_set_master(pdev);

	retval = device_register(&nvudc_dev->gadget.dev);
	if (retval) {
		msg_err(nvudc_dev->dev, "Can't register device\n");
		goto error;
	}

	msg_dbg(nvudc_dev->dev, "device registered\n");
	nvudc_dev->registered = true;

	retval = usb_add_gadget_udc(&pdev->dev, &nvudc_dev->gadget);
	if (retval)
		goto error;

	msg_dbg(nvudc_dev->dev, "usb_add_gadget_udc\n");

	the_controller = nvudc_dev;

	return 0;
error:
	nvudc_remove_pci(pdev);
	if (nvudc_dev)
		msg_exit(nvudc_dev->dev);
	return retval;
}

void free_data_struct(struct nv_udc_s *nvudc)
{
	u32 i;
	struct nv_udc_ep *udc_ep_ptr;
	struct ep_cx_s *p_ep_cx;
	if (nvudc->event_ring0.vaddr) {
		dma_free_coherent(nvudc->dev,
				nvudc->event_ring0.len,
				nvudc->event_ring0.vaddr,
				nvudc->event_ring0.dma);
		nvudc->event_ring0.vaddr = 0;
		nvudc->event_ring0.dma = 0;
		nvudc->event_ring0.len = 0;
	}

	if (nvudc->event_ring1.vaddr) {
		dma_free_coherent(nvudc->dev,
				nvudc->event_ring1.len,
				nvudc->event_ring1.vaddr,
				nvudc->event_ring1.dma);
		nvudc->event_ring1.vaddr = 0;
		nvudc->event_ring1.dma = 0;
		nvudc->event_ring1.len = 0;

	}

	if (nvudc->p_epcx) {
		dma_free_coherent(nvudc->dev, nvudc->ep_cx.len,
				nvudc->ep_cx.vaddr, nvudc->ep_cx.dma);
		nvudc->ep_cx.vaddr = 0;
		nvudc->ep_cx.dma = 0;
		nvudc->ep_cx.len = 0;
	}

	for (i = 2; i < 32; i++) {
		udc_ep_ptr = &nvudc->udc_ep[i];
		p_ep_cx = nvudc->p_epcx + i;
		if (udc_ep_ptr->tran_ring_info.vaddr) {
			dma_free_coherent(nvudc->dev,
					udc_ep_ptr->tran_ring_info.len,
					udc_ep_ptr->tran_ring_info.vaddr,
					udc_ep_ptr->tran_ring_info.dma);
			udc_ep_ptr->tran_ring_info.vaddr = 0;
			udc_ep_ptr->tran_ring_info.dma = 0;
			udc_ep_ptr->tran_ring_info.len = 0;
		}
	}

	if (nvudc->status_req) {
		kfree(nvudc->status_req);
	}
	/*        otg_put_transceiver(nvudc->transceiver); */
}


static void nvudc_remove_pci(struct pci_dev *pdev)
{
	struct nv_udc_s *nvudc;

	nvudc = pci_get_drvdata(pdev);

	if (!nvudc)
		return;

	msg_entry(nvudc->dev);

	usb_del_gadget_udc(&nvudc->gadget);
	free_data_struct(nvudc);

	if (nvudc->mmio_phys_base) {
		release_mem_region(nvudc->mmio_phys_base,
				nvudc->mmio_phys_len);
		if (nvudc->mmio_reg_base) {
			iounmap(nvudc->mmio_reg_base);
			nvudc->mmio_reg_base = NULL;
		}
	}
	if (nvudc->irq)
		free_irq(pdev->irq, nvudc);

	if (nvudc->enabled)
		pci_disable_device(pdev);

	if (nvudc->registered)
		device_unregister(&nvudc->gadget.dev);
	pci_set_drvdata(pdev, NULL);

	msg_exit(nvudc->dev);
}

void save_mmio_reg(struct nv_udc_s *nvudc)
{
	/* Save Device Address, U2 Timeout, Port Link State and Port State
	*Event Ring enqueue pointer and PCS
	*/
	nvudc->mmio_reg.device_address = nvudc->dev_addr;
	nvudc->mmio_reg.portsc = ioread32(nvudc->mmio_reg_base + PORTSC);
	nvudc->mmio_reg.portpm = ioread32(nvudc->mmio_reg_base + PORTPM);
	nvudc->mmio_reg.ereplo = ioread32(nvudc->mmio_reg_base + EREPLO);
	nvudc->mmio_reg.erephi = ioread32(nvudc->mmio_reg_base + EREPHI);
	nvudc->mmio_reg.ctrl = ioread32(nvudc->mmio_reg_base + CTRL);
}

void restore_mmio_reg(struct nv_udc_s *nvudc)
{
	u32 reg;
	dma_addr_t dma;

	if (XUDC_IS_T210(nvudc)) {
		/* Enable clock gating */
		/* T210 WAR, Disable BLCG DFPCI/UFPCI/FE */
		reg = ioread32(nvudc->mmio_reg_base + BLCG);
		reg |= BLCG_ALL;
		reg &= ~(BLCG_DFPCI | BLCG_UFPCI | BLCG_FE |
			BLCG_COREPLL_PWRDN);
		iowrite32(reg, nvudc->mmio_reg_base + BLCG);
	} else if (XUDC_IS_T186(nvudc)) {
		/* T186 WAR, Disable BLCG COREPLL_PWRDN */
		reg = ioread32(nvudc->mmio_reg_base + BLCG);
		reg &= ~BLCG_COREPLL_PWRDN;
		iowrite32(reg, nvudc->mmio_reg_base + BLCG);
	}

	/* restore the event ring info */
	init_hw_event_ring(nvudc);

	iowrite32(nvudc->mmio_reg.ereplo, nvudc->mmio_reg_base + EREPLO);
	iowrite32(nvudc->mmio_reg.erephi, nvudc->mmio_reg_base + EREPHI);

	dma = event_trb_virt_to_dma(nvudc, nvudc->evt_dq_pt);
	iowrite32(lower_32_bits(dma), nvudc->mmio_reg_base + ERDPLO);
	iowrite32(upper_32_bits(dma), nvudc->mmio_reg_base + ERDPHI);

	/* restore endpoint contexts info */
	dma = nvudc->ep_cx.dma;
	reg = lower_32_bits(dma);
	iowrite32(reg, nvudc->mmio_reg_base + ECPLO);

	reg = upper_32_bits(dma);
	iowrite32(reg, nvudc->mmio_reg_base + ECPHI);

	/* restore port related info */
	iowrite32(nvudc->mmio_reg.portsc, nvudc->mmio_reg_base + PORTSC);

	iowrite32(nvudc->mmio_reg.portpm, nvudc->mmio_reg_base + PORTPM);

	if (enable_fe_infinite_ss_retry) {
		/* enable FE_INFINITE_SS_RETRY */
		reg = ioread32(nvudc->mmio_reg_base + CFG_DEV_FE);
		reg |= CFG_DEV_FE_INFINITE_SS_RETRY;
		iowrite32(reg, nvudc->mmio_reg_base + CFG_DEV_FE);
	}

	/* restore the control register
	* this has to be done at last, because ENABLE bit should not be set
	until event ring .. has setup
	*/
	iowrite32(nvudc->mmio_reg.ctrl, nvudc->mmio_reg_base + CTRL);

	/* restore TIMER for U3 exit */
	reg = ioread32(nvudc->base + SSPX_CORE_PADCTL4);
	reg &= ~(RXDAT_VLD_TIMEOUT_U3_MASK);
	reg |= RXDAT_VLD_TIMEOUT_U3;
	iowrite32(reg, nvudc->base + SSPX_CORE_PADCTL4);
}

static void nvudc_plat_clocks_deinit(struct nv_udc_s *nvudc)
{
	struct platform_device *pdev = nvudc->pdev.plat;
	struct device *dev = &pdev->dev;

	if (nvudc->pll_u_480M) {
		devm_clk_put(dev, nvudc->pll_u_480M);
		nvudc->pll_u_480M = NULL;
	}

	if (nvudc->dev_clk) {
		devm_clk_put(dev, nvudc->dev_clk);
		nvudc->dev_clk = NULL;
	}

	if (nvudc->ss_clk) {
		devm_clk_put(dev, nvudc->ss_clk);
		nvudc->ss_clk = NULL;
	}

	if (nvudc->pll_e) {
		devm_clk_put(dev, nvudc->pll_e);
		nvudc->pll_e = NULL;
	}
}

static int nvudc_plat_clocks_init(struct nv_udc_s *nvudc)
{
	struct platform_device *pdev = nvudc->pdev.plat;
	struct device *dev = &pdev->dev;
	int err;

	nvudc->pll_u_480M = devm_clk_get(dev, "pll_u_480M");
	if (IS_ERR(nvudc->pll_u_480M)) {
		err = PTR_ERR(nvudc->pll_u_480M);
		msg_err(dev, "failed to get pll_u_480M %d\n", err);
		nvudc->pll_u_480M = NULL;
		return err;
	}

	nvudc->dev_clk = devm_clk_get(dev, "dev");
	if (IS_ERR(nvudc->dev_clk)) {
		err = PTR_ERR(nvudc->dev_clk);
		msg_err(dev, "failed to get dev_clk %d\n", err);
		nvudc->dev_clk = NULL;
		return err;
	}

	nvudc->ss_clk = devm_clk_get(dev, "ss");
	if (IS_ERR(nvudc->ss_clk)) {
		err = PTR_ERR(nvudc->ss_clk);
		msg_err(dev, "failed to get ss_clk %d\n", err);
		nvudc->ss_clk = NULL;
		return err;
	}

	nvudc->pll_e = devm_clk_get(dev, "pll_e");
	if (IS_ERR(nvudc->pll_e)) {
		err = PTR_ERR(nvudc->pll_e);
		msg_err(dev, "failed to get pll_e %d\n", err);
		nvudc->pll_e = NULL;
		return err;
	}

	return 0;
}

static int nvudc_plat_clocks_enable(struct nv_udc_s *nvudc)
{
	struct platform_device *pdev = nvudc->pdev.plat;
	struct device *dev = &pdev->dev;
	int err;

	if (nvudc->pll_u_480M) {
		err = clk_prepare_enable(nvudc->pll_u_480M);
		if (err) {
			msg_err(dev, "failed to enable pll_u_480M %d\n", err);
			return err;
		}
	}

	if (nvudc->pll_e) {
		err = clk_prepare_enable(nvudc->pll_e);
		if (err) {
			msg_err(dev, "failed to enable pll_e %d\n", err);
			goto err_disable_pll_u_480M;
		}
	}
	return 0;

err_disable_pll_u_480M:
	if (nvudc->pll_u_480M)
		clk_disable_unprepare(nvudc->pll_u_480M);
	return err;
}

static void nvudc_plat_clocks_disable(struct nv_udc_s *nvudc)
{
	if (nvudc->pll_e)
		clk_disable_unprepare(nvudc->pll_e);
	if (nvudc->pll_u_480M)
		clk_disable_unprepare(nvudc->pll_u_480M);
}

static int nvudc_plat_regulators_init(struct nv_udc_s *nvudc)
{
	struct platform_device *pdev = nvudc->pdev.plat;
	struct device *dev = &pdev->dev;
	int err;
	int i;
	size_t size;

	size = nvudc->soc->num_supplies * sizeof(struct regulator_bulk_data);
	nvudc->supplies = devm_kzalloc(dev, size, GFP_ATOMIC);
	if (!nvudc->supplies) {
		msg_err(dev, "failed to alloc memory for regulators\n");
		return -ENOMEM;
	}

	for (i = 0; i < nvudc->soc->num_supplies; i++)
		nvudc->supplies[i].supply = nvudc->soc->supply_names[i];

	err = devm_regulator_bulk_get(dev, nvudc->soc->num_supplies,
				nvudc->supplies);
	if (err) {
		msg_err(dev, "failed to request regulators %d\n", err);
		return err;
	}

	err = regulator_bulk_enable(nvudc->soc->num_supplies, nvudc->supplies);
	if (err) {
		msg_err(dev, "failed to enable regulators %d\n", err);

		for (i = 0; i < nvudc->soc->num_supplies; i++) {
			if (nvudc->supplies[i].consumer)
				devm_regulator_put(nvudc->supplies[i].consumer);
		}
		return err;
	}

	return 0;
}

static void nvudc_plat_regulator_deinit(struct nv_udc_s *nvudc)
{
	struct platform_device *pdev = nvudc->pdev.plat;
	struct device *dev = &pdev->dev;
	int i;
	int err;

	err = regulator_bulk_disable(nvudc->soc->num_supplies, nvudc->supplies);
	if (err)
		msg_err(dev, "failed to disable regulators %d\n", err);

	for (i = 0; i < nvudc->soc->num_supplies; i++) {
		if (nvudc->supplies[i].consumer)
			devm_regulator_put(nvudc->supplies[i].consumer);
	}

	devm_kfree(dev, nvudc->supplies);
}

static int nvudc_plat_mmio_regs_init(struct nv_udc_s *nvudc)
{
	struct platform_device *pdev = nvudc->pdev.plat;
	struct device *dev = &pdev->dev;
	struct resource *res;
	resource_size_t size;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		msg_err(dev, "failed to get udc mmio resources\n");
		return -ENXIO;
	}
	msg_info(dev, "xudc mmio start %pa end %pa\n", &res->start, &res->end);
	nvudc->mmio_phys_base = res->start;

	nvudc->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(nvudc->base)) {
		msg_err(dev, "failed to request and map xudc mmio err=%ld\n",
				PTR_ERR(nvudc->base));
		return -EFAULT;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res) {
		msg_err(dev, "failed to get fpci mmio resources\n");
		return -ENXIO;
	}
	msg_info(dev, "fpci mmio start %pa end %pa\n", &res->start, &res->end);

	nvudc->fpci = devm_ioremap_resource(dev, res);
	if (IS_ERR(nvudc->fpci)) {
		msg_err(dev, "failed to request and map fpci mmio err=%ld\n",
				PTR_ERR(nvudc->fpci));
		return -EFAULT;
	}

	if (XUDC_IS_T210(nvudc)) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
		if (!res) {
			msg_err(dev, "failed to get ipfs mmio resources\n");
			return -ENXIO;
		}
		msg_info(dev, "ipfs mmio start %pa end %pa\n",
							&res->start, &res->end);
		nvudc->ipfs = devm_ioremap_resource(dev, res);
		if (IS_ERR(nvudc->ipfs)) {
			msg_err(dev, "failed to map ipfs mmio err=%ld\n",
					PTR_ERR(nvudc->ipfs));
			return -EFAULT;
		}

		res = platform_get_resource(pdev, IORESOURCE_MEM, 3);
		if (!res) {
			msg_err(dev, "failed to get padctl mmio resources\n");
			return -ENXIO;
		}
		msg_info(dev, "padctl mmio start %pa end %pa\n",
							&res->start, &res->end);
		size = resource_size(res);
		nvudc->padctl = devm_ioremap_nocache(dev, res->start, size);
		if (!nvudc->padctl) {
			msg_err(dev, "failed to map padctl mmio\n");
			return -EFAULT;
		}
	}

	nvudc->base_list[0] = nvudc->base;
	nvudc->base_list[1] = nvudc->fpci;
	nvudc->base_list[2] = nvudc->ipfs;
	nvudc->base_list[3] = nvudc->padctl;

	return 0;
}

static void nvudc_plat_fpci_ipfs_init(struct nv_udc_s *nvudc)
{
	struct platform_device *pdev = nvudc->pdev.plat;
	struct device *dev = &pdev->dev;
	u32 reg, addr;

	if (XUDC_IS_T210(nvudc)) {
		reg = ioread32(nvudc->ipfs + XUSB_DEV_CONFIGURATION);
		reg |= EN_FPCI;
		iowrite32(reg, nvudc->ipfs + XUSB_DEV_CONFIGURATION);
		reg_dump(dev, nvudc->ipfs, XUSB_DEV_CONFIGURATION);
		usleep_range(10, 30);
	}

	reg = (IO_SPACE_ENABLED | MEMORY_SPACE_ENABLED | BUS_MASTER_ENABLED);
	iowrite32(reg, nvudc->fpci + XUSB_DEV_CFG_1);
	reg_dump(dev, nvudc->fpci, XUSB_DEV_CFG_1);

	addr = lower_32_bits(nvudc->mmio_phys_base);
	reg = BASE_ADDRESS((addr >> 16));
	iowrite32(reg, nvudc->fpci + XUSB_DEV_CFG_4);
	reg_dump(dev, nvudc->fpci, XUSB_DEV_CFG_4);

	reg = upper_32_bits(nvudc->mmio_phys_base);
	iowrite32(reg, nvudc->fpci + XUSB_DEV_CFG_5);
	reg_dump(dev, nvudc->fpci, XUSB_DEV_CFG_5);

	usleep_range(100, 200);

	if (XUDC_IS_T210(nvudc)) {
		/* enable interrupt assertion */
		reg = ioread32(nvudc->ipfs + XUSB_DEV_INTR_MASK);
		reg |= IP_INT_MASK;
		writel(reg, nvudc->ipfs + XUSB_DEV_INTR_MASK);
		reg_dump(dev, nvudc->ipfs, XUSB_DEV_INTR_MASK);
	}

	reg = ioread32(nvudc->base + SSPX_CORE_PADCTL4);
	reg &= ~(RXDAT_VLD_TIMEOUT_U3_MASK);
	reg |= RXDAT_VLD_TIMEOUT_U3;
	iowrite32(reg, nvudc->base + SSPX_CORE_PADCTL4);
	reg_dump(dev, nvudc->base, SSPX_CORE_PADCTL4);

	/* Ping LFPS TBURST is greater than spec defined value of 200ns
	* Reduce ping tburst to 0x6 in order to make link layer tests pass
	*/
	reg = ioread32(nvudc->base + SSPX_CORE_CNT0);
	reg &= ~PING_TBRST_MASK;
	reg |= PING_TBRST(PING_TBRST_VAL);
	iowrite32(reg, nvudc->base + SSPX_CORE_CNT0);
	reg_dump(dev, nvudc->base, SSPX_CORE_CNT0);

	/* Increase tPortConfiguration timeout to 0x978 */
	reg = ioread32(nvudc->base + SSPX_CORE_CNT30);
	reg &= ~LMPITP_TIMER_MASK;
	reg |= LMPITP_TIMER(LMPITP_TIMER_VAL);
	iowrite32(reg, nvudc->base + SSPX_CORE_CNT30);
	reg_dump(dev, nvudc->base, SSPX_CORE_CNT30);

	/* WAR for TD 6.5 Polling LFPS Duration Test.
	* Test suite is violating polling lfps burst max of 1.4us
	* Sending 1.45us instead
	* program POLL_TBRST_MAX to 0xB0 in order to make the test pass
	*/
	if (war_poll_trbst_max) {
		reg = ioread32(nvudc->base + SSPX_CORE_CNT32);
		reg &= ~POLL_TBRST_MAX_MASK;
		reg |= POLL_TBRST_MAX(POLL_TBRST_MAX_VAL);
		iowrite32(reg, nvudc->base + SSPX_CORE_CNT32);
		reg_dump(dev, nvudc->base, SSPX_CORE_CNT32);
	}

	if (XUDC_IS_T186(nvudc)) {
		/* for HS link stability issue */
		#define XUSB_DEV_XHCI_HSFSPI_COUNT16		(0x19c)
		iowrite32(0x927c0, nvudc->base + XUSB_DEV_XHCI_HSFSPI_COUNT16);

		/* change INIT value of "NV_PROJ__XUSB_DEV_XHCI_HSFSPI_COUNT0"
		register from 0x12c to 0x3E8. This counter is used by xUSB
		device to respond to HS detection handshake after the
		detection of SE0 from host.*/
		#define XUSB_DEV_XHCI_HSFSPI_COUNT0		(0x100)
		iowrite32(0x3e8, nvudc->base + XUSB_DEV_XHCI_HSFSPI_COUNT0);
	}
}

static int tegra_xudc_exit_elpg(struct nv_udc_s *nvudc)
{
	int ret = 0;
	struct device *dev = nvudc->dev;
	unsigned long flags;
	int partition_id;

	mutex_lock(&nvudc->elpg_lock);

	if (!nvudc->is_elpg) {
		dev_warn(dev, "%s Not in ELPG\n", __func__);
		mutex_unlock(&nvudc->elpg_lock);
		return 0;
	}

	dev_dbg(dev, "Exit device controller ELPG\n");

	spin_lock_irqsave(&nvudc->lock, flags);
	nvudc->elpg_is_processing = true;
	spin_unlock_irqrestore(&nvudc->lock, flags);

	/* enable power rail */
	partition_id = tegra_pd_get_powergate_id(tegra_xusba_pd);
	if (partition_id < 0)
		return -EINVAL;
	ret = tegra_unpowergate_partition_with_clk_on(partition_id);
	if (ret) {
		dev_err(dev, "%s Fail to unpowergate XUSBA\n", __func__);
		mutex_unlock(&nvudc->elpg_lock);
		return ret;
	}

	partition_id = tegra_pd_get_powergate_id(tegra_xusbb_pd);
	if (partition_id < 0)
		return -EINVAL;
	ret = tegra_unpowergate_partition_with_clk_on(partition_id);
	if (ret) {
		dev_err(dev, "%s Fail to unpowergate XUSBB\n", __func__);
		mutex_unlock(&nvudc->elpg_lock);
		return ret;
	}

	spin_lock_irqsave(&nvudc->lock, flags);
	nvudc->elpg_is_processing = false;
	spin_unlock_irqrestore(&nvudc->lock, flags);

#if defined(CONFIG_ARCH_TEGRA_21x_SOC)
	/* disable wakeup interrupt */
	tegra_xhci_hs_wake_on_interrupts(TEGRA_XUSB_USB2_P0, false);
#elif defined(CONFIG_ARCH_TEGRA_18x_SOC)
	ret = nvudc_plat_phy_power_on(nvudc);
	if (ret)
		dev_err(dev, "phy power on failed %d\n", ret);
#endif

	/* register restore */
	nvudc_plat_fpci_ipfs_init(nvudc);
	set_interrupt_moderation(nvudc, min_irq_interval_us);
	restore_mmio_reg(nvudc);
	nvudc_resume_state(nvudc, 1);

	nvudc->is_elpg = false;

	mutex_unlock(&nvudc->elpg_lock);

	dev_info(dev, "Exit device controller ELPG done\n");
	return 0;
}

static int tegra_xudc_enter_elpg(struct nv_udc_s *nvudc)
{
	u32 reg;
	int ret;
	struct device *dev = nvudc->dev;
	unsigned long flags;
	int partition_id;

	mutex_lock(&nvudc->elpg_lock);

	if (nvudc->is_elpg) {
		dev_warn(dev, "%s Already in ELPG\n", __func__);
		mutex_unlock(&nvudc->elpg_lock);
		return 0;
	}

	dev_dbg(dev, "Enter device controller ELPG\n");

	/* do not support suspend if link is connected */
	reg = ioread32(nvudc->mmio_reg_base + PORTSC);
	if (PORTSC_CCS & reg) {
		pm_runtime_mark_last_busy(nvudc->dev);
		mutex_unlock(&nvudc->elpg_lock);
		return -EAGAIN;
	}

	nvudc->resume_state = nvudc->device_state;
	nvudc->device_state = USB_STATE_SUSPENDED;

	/* read and store registers */
	save_mmio_reg(nvudc);

#if defined(CONFIG_ARCH_TEGRA_21x_SOC)
	/* TODO : PMC sleepwalk */
	/* enable wakeup interrupt */
	tegra_xhci_hs_wake_on_interrupts(TEGRA_XUSB_USB2_P0, true);
#elif defined(CONFIG_ARCH_TEGRA_18x_SOC)
	nvudc_plat_phy_power_off(nvudc);
#endif

	spin_lock_irqsave(&nvudc->lock, flags);
	nvudc->elpg_is_processing = true;
	spin_unlock_irqrestore(&nvudc->lock, flags);

	/* disable partition power */
	partition_id = tegra_pd_get_powergate_id(tegra_xusba_pd);
	if (partition_id < 0)
		return -EINVAL;
	ret = tegra_powergate_partition_with_clk_off(partition_id);
	if (ret) {
		dev_err(dev, "%s Fail to powergate XUSBA\n", __func__);
		mutex_unlock(&nvudc->elpg_lock);
		return ret;
	}

	partition_id = tegra_pd_get_powergate_id(tegra_xusbb_pd);
	if (partition_id < 0)
		return -EINVAL;
	ret = tegra_powergate_partition_with_clk_off(partition_id);
	if (ret) {
		dev_err(dev, "%s Fail to powergate XUSBB\n", __func__);
		mutex_unlock(&nvudc->elpg_lock);
		return ret;
	}

	spin_lock_irqsave(&nvudc->lock, flags);
	nvudc->elpg_is_processing = false;
	spin_unlock_irqrestore(&nvudc->lock, flags);

	nvudc->is_elpg = true;

	mutex_unlock(&nvudc->elpg_lock);
	dev_info(dev, "Enter device controller ELPG done\n");
	return 0;
}

static int tegra_xudc_runtime_resume(struct device *dev)
{
	struct nv_udc_s *nvudc;
	struct platform_device *pdev = to_platform_device(dev);

	pr_debug("%s called\n", __func__);
	nvudc = platform_get_drvdata(pdev);
	return tegra_xudc_exit_elpg(nvudc);
}

static int tegra_xudc_runtime_suspend(struct device *dev)
{
	struct nv_udc_s *nvudc;
	struct platform_device *pdev = to_platform_device(dev);

	pr_debug("%s called\n", __func__);
	nvudc = platform_get_drvdata(pdev);
	return tegra_xudc_enter_elpg(nvudc);
}

static int nvudc_plat_irqs_init(struct nv_udc_s *nvudc)
{
	struct platform_device *pdev = nvudc->pdev.plat;
	struct device *dev = &pdev->dev;
	int err;

	/* TODO try "interrupt-names" in dts */
	err = platform_get_irq(pdev, 0);
	if (err < 0) {
		msg_err(dev, "failed to get irq resource 0\n");
		return -ENXIO;
	}
	nvudc->irq = err;
	msg_info(dev, "irq %u\n", nvudc->irq);

	err = devm_request_irq(dev, nvudc->irq, nvudc_irq, 0,
				dev_name(dev), nvudc);
	if (err < 0) {
		msg_err(dev, "failed to claim irq %d\n", err);
		return err;
	}

#if defined(CONFIG_ARCH_TEGRA_21x_SOC)
	err = platform_get_irq(pdev, 1);
	if (err < 0) {
		msg_err(dev, "failed to get irq resource 1\n");
		return -ENXIO;
	}
	nvudc->padctl_irq = err;
	msg_info(dev, "irq %u\n", nvudc->padctl_irq);

	err = devm_request_irq(dev, nvudc->padctl_irq, nvudc_padctl_irq
			, IRQF_SHARED | IRQF_TRIGGER_HIGH
			, dev_name(dev), nvudc);
	if (err < 0) {
		msg_err(dev, "failed to claim padctl_irq %d\n", err);
		return err;
	}
#endif

	return 0;
}

#define MASK_SHIFT(x , mask, shift) ((x & mask) << shift)

#define SET_PORT(nvudc, val, ss_node)	\
	(nvudc->bdata.ss_portmap |= \
	MASK_SHIFT(val, 0xF, ss_node * 4))


#define SET_LANE(nvudc, val, ss_node) \
	(nvudc->bdata.lane_owner |= (val << (ss_node * 4)))

static int nvudc_get_bdata(struct nv_udc_s *nvudc)
{
#if defined(CONFIG_ARCH_TEGRA_21x_SOC)
	struct platform_device *pdev = nvudc->pdev.plat;
	struct device_node *node = pdev->dev.of_node;
	struct device_node *padctl;
	u32 lane = 0, ss_portmap = 0, otg_portmap = 0;
	u32 ss_node = 0, index = 0, count = 0;

	/* Get common setting for padctl */
	padctl = of_parse_phandle(node, "nvidia,common_padctl", 0);

	count = of_property_count_u32(padctl, "nvidia,ss_ports");
	if (count < 4 || count % 4 != 0) {
		pr_err("nvidia,ss_ports not found or invalid count %d\n",
								count);
		return -ENODEV;
	}

	pr_debug("ss_portmap = %x, lane_owner = %x, otg_portmap = %x\n",
		nvudc->bdata.ss_portmap,
		nvudc->bdata.lane_owner,
		nvudc->bdata.otg_portmap);
	do {
		of_property_read_u32_index(padctl, "nvidia,ss_ports",
						index, &ss_node);
		if (ss_node != TEGRA_XHCI_UNUSED_PORT)
			index++;
		else {
			SET_PORT(nvudc, TEGRA_XHCI_UNUSED_PORT, index / 4);
			SET_LANE(nvudc, TEGRA_XHCI_UNUSED_LANE, index / 4);
			index += 4;
			continue;
		}
		of_property_read_u32_index(padctl, "nvidia,ss_ports",
						index, &ss_portmap);
		SET_PORT(nvudc, ss_portmap, ss_node);

		index++;
		of_property_read_u32_index(padctl, "nvidia,ss_ports",
						index, &lane);
		SET_LANE(nvudc, lane, ss_node);

		index++;
		of_property_read_u32_index(padctl, "nvidia,ss_ports",
						index, &otg_portmap);
		if (lane != TEGRA_XHCI_UNUSED_LANE)
			nvudc->bdata.otg_portmap |=
				MASK_SHIFT(otg_portmap, BIT(0), ss_node);
		if (ss_portmap != TEGRA_XHCI_UNUSED_PORT)
			nvudc->bdata.otg_portmap |= MASK_SHIFT(otg_portmap,
					BIT(0), (ss_portmap + XUSB_UTMI_INDEX));
		index++;

		pr_debug("index = %d, count = %d, ss_node = %d\n",
			index, count, ss_node);
		pr_debug("ss_portmap = %x, lane = %x, otg_portmap = %x\n",
			ss_portmap, lane, otg_portmap);
	} while (index < count);

	pr_debug("[%s]Value get from DT\n", __func__);
	pr_debug("nvidia,ss_portmap = %x\n", nvudc->bdata.ss_portmap);
	pr_debug("nvidia,lane_owner = %x\n", nvudc->bdata.lane_owner);
	pr_debug("nvidia,otg_portmap = %x\n", nvudc->bdata.otg_portmap);
#endif
	return 0;
}

#if defined(CONFIG_ARCH_TEGRA_21x_SOC)
static void t210_program_ss_pad(struct nv_udc_s *nvudc, int port)
{
	char prod_name[] = "prod_c_ssX";
	int err = 0;

	if (!nvudc->prod_list)
		goto safesettings;

	snprintf(prod_name, sizeof(prod_name), "prod_c_ss%d", port);
	err = tegra_prod_set_by_name((void __iomem **)&nvudc->base_list,
					prod_name, nvudc->prod_list);
	if (err) {
		msg_warn(nvudc->dev, "prod set failed\n");
		goto safesettings;
	}

	return;

safesettings:
	tegra_usb_pad_reg_update(UPHY_USB3_PAD_ECTL_1(port),
		TX_TERM_CTRL(~0), TX_TERM_CTRL(0x2));
	tegra_usb_pad_reg_update(UPHY_USB3_PAD_ECTL_2(port),
		RX_CTLE(~0), RX_CTLE(0xfc));
	tegra_usb_pad_reg_update(UPHY_USB3_PAD_ECTL_3(port),
		RX_DFE(~0), RX_DFE(0xc0077f1f));
	tegra_usb_pad_reg_update(UPHY_USB3_PAD_ECTL_4(port),
		RX_CDR_CTRL(~0), RX_CDR_CTRL(0x1c7));
	tegra_usb_pad_reg_update(UPHY_USB3_PAD_ECTL_6(port),
		RX_EQ_CTRL_H(~0), RX_EQ_CTRL_H(0xfcf01368));
	return;
}

static int nvudc_plat_pad_deinit(struct nv_udc_s *nvudc)
{
	xusb_utmi_pad_deinit(nvudc->hs_port);
	utmi_phy_pad_disable(nvudc->prod_list);
	utmi_phy_iddq_override(true);

	xusb_ss_pad_deinit(nvudc->ss_port);
	if (nvudc->is_ss_port_active)
		usb3_phy_pad_disable();

	return 0;
}

static int nvudc_plat_pad_init(struct nv_udc_s *nvudc)
{
	struct platform_device *pdev = nvudc->pdev.plat;
	int hs_port = 0, ss_port = 0;
	bool is_ss_port_enabled = true;
	u32 lane_owner_mask;
	u32 otg_portmap = nvudc->bdata.otg_portmap;

	/* VBUS_ID init */
	usb2_vbus_id_init();

	/* utmi pad init for pad 0 */
	hs_port = ffs(otg_portmap >> XUSB_UTMI_INDEX) - 1;
	if (hs_port > 7 || hs_port < 0) {
		msg_warn(nvudc->dev, "hs_port out of bound : %d\n", hs_port);
		return -EINVAL;
	}
	nvudc->hs_port = hs_port;

	xusb_utmi_pad_init(hs_port, PORT_CAP(hs_port, PORT_CAP_OTG), false);
	if (nvudc->vbus_detected)
		xusb_utmi_pad_driver_power(hs_port, true);

	if (!nvudc->prod_list)
		nvudc->prod_list = tegra_prod_init(pdev->dev.of_node);

	if (IS_ERR(nvudc->prod_list)) {
		msg_warn(nvudc->dev, "prod list init failed with error %ld\n",
			PTR_ERR(nvudc->prod_list));
		nvudc->prod_list = NULL;
	}

	utmi_phy_pad_enable(nvudc->prod_list);
	utmi_phy_iddq_override(false);

	if ((nvudc->bdata.ss_portmap & 0xf) == hs_port)
		ss_port = 0;
	else if ((nvudc->bdata.ss_portmap & 0xf0) == hs_port)
		ss_port = 1;
	else if ((nvudc->bdata.ss_portmap & 0xf00) == hs_port)
		ss_port = 2;
	else if ((nvudc->bdata.ss_portmap & 0xf000) == hs_port)
		ss_port = 3;

	lane_owner_mask = (0xf << (4 * ss_port));
	if ((nvudc->bdata.lane_owner & lane_owner_mask) == lane_owner_mask)
		is_ss_port_enabled = false;

	/* ss pad init for pad ss_port */
	xusb_ss_pad_init(ss_port, hs_port, XUSB_OTG_MODE);

	if (is_ss_port_enabled)
		t210_program_ss_pad(nvudc, ss_port);

	if ((tegra_usb_pad_reg_read(XUSB_PADCTL_USB2_VBUS_ID_0) &
			USB2_VBUS_ID_0_ID_OVERRIDE) ==
			USB2_VBUS_ID_0_ID_OVERRIDE_RID_FLOAT) {
		tegra_xhci_ss_wake_signal((1 << ss_port), false);
		tegra_xhci_ss_vcore((1 << ss_port), false);
	}

	/* ss pad phy enable */
	if (is_ss_port_enabled)
		usb3_phy_pad_enable(nvudc->bdata.lane_owner);

	nvudc->is_ss_port_active = is_ss_port_enabled;
	nvudc->ss_port = ss_port;
	return 0;
}
#elif defined(CONFIG_ARCH_TEGRA_18x_SOC)
static int nvudc_plat_phy_init(struct nv_udc_s *nvudc)
{
	struct device *dev = nvudc->dev;
	char *name;
	struct phy *phy;
	int ret;

	name = "usb3";
	phy = devm_phy_optional_get(nvudc->dev, name);
	if (IS_ERR(phy)) {
		ret = PTR_ERR(phy);
		if (ret != -EPROBE_DEFER)
			msg_err(dev, "devm_phy_get(%s) error %d\n", name, ret);
		return ret;
	}
	msg_dbg(dev, "devm_phy_get(%s) phy %p\n", name, phy);
	nvudc->usb3_phy = phy;

	name = "usb2";
	phy = devm_phy_optional_get(nvudc->dev, name);
	if (IS_ERR(phy)) {
		ret = PTR_ERR(phy);
		if (ret != -EPROBE_DEFER)
			msg_err(dev, "devm_phy_get(%s) error %d\n", name, ret);
		return ret;
	}
	msg_dbg(dev, "devm_phy_get(%s) phy %p\n", name, phy);
	nvudc->usb2_phy = phy;

	ret = phy_init(nvudc->usb2_phy);
	if (ret) {
		msg_err(dev, "usb2 phy_init() error %d\n", ret);
		return ret;
	}

	ret = phy_init(nvudc->usb3_phy);
	if (ret) {
		msg_err(dev, "usb3 phy_init() error %d\n", ret);
		return ret;
	}


	return 0;
}

static int nvudc_plat_phy_power_on(struct nv_udc_s *nvudc)
{
	int ret;

	ret = phy_power_on(nvudc->usb2_phy);
	if (ret)
		msg_err(nvudc->dev, "usb2 phy_power_on() error %d\n", ret);

	ret = phy_power_on(nvudc->usb3_phy);
	if (ret)
		msg_err(nvudc->dev, "usb3 phy_power_on() error %d\n", ret);


	return 0;
}

static void nvudc_plat_phy_power_off(struct nv_udc_s *nvudc)
{
	int ret;

	ret = phy_power_off(nvudc->usb2_phy);
	if (ret)
		msg_err(nvudc->dev, "usb2 phy_power_off() error %d\n", ret);

	ret = phy_power_off(nvudc->usb3_phy);
	if (ret)
		msg_err(nvudc->dev, "usb3 phy_power_off() error %d\n", ret);
}
#endif

static int nvudc_suspend_platform(struct device *dev)
{
	struct nv_udc_s *nvudc;
	struct platform_device *pdev = to_platform_device(dev);
	unsigned long flag;
	int err = 0;

	nvudc = platform_get_drvdata(pdev);

	/* During suspending, cable events may be in processing*/
	spin_lock_irqsave(&nvudc->lock, flag);
	if (nvudc->extcon_event_processing) {
		spin_unlock_irqrestore(&nvudc->lock, flag);
		msg_info(dev, "Cable Event is processing\n");
		return -EBUSY;
	}
	nvudc->is_suspended = true;
	spin_unlock_irqrestore(&nvudc->lock, flag);

	/*
	  * Force to enter ELPG if not being RPM_SUSPENDED status
	  * Later after resuming, status of cable would be restores.
	  */
	if (!pm_runtime_status_suspended(nvudc->dev)) {
		vbus_not_detected(nvudc);
		tegra_xudc_enter_elpg(nvudc);
	}

	clk_disable_unprepare(nvudc->pll_e);
	clk_disable_unprepare(nvudc->pll_u_480M);

#if defined(CONFIG_ARCH_TEGRA_21x_SOC)
	if (pex_usb_pad_pll_reset_assert())
		dev_err(dev, "error assert reset to pex pll\n");
#endif

	err = regulator_bulk_disable(nvudc->soc->num_supplies, nvudc->supplies);
	if (err)
		msg_err(dev, "failed to disable regulators %d\n", err);

#if defined(CONFIG_ARCH_TEGRA_21x_SOC)
	nvudc_plat_pad_deinit(nvudc);
#endif

	return 0;
}

static int nvudc_resume_platform(struct device *dev)
{
	struct nv_udc_s *nvudc;
	struct platform_device *pdev = to_platform_device(dev);
	int err = 0;
	unsigned long flags;

	nvudc = platform_get_drvdata(pdev);

	err = regulator_bulk_enable(nvudc->soc->num_supplies, nvudc->supplies);
	if (err)
		msg_err(dev, "failed to enable regulators %d\n", err);

#if defined(CONFIG_ARCH_TEGRA_21x_SOC)
	if (pex_usb_pad_pll_reset_deassert())
		dev_err(dev, "error deassert pex pll\n");
#endif

	clk_prepare_enable(nvudc->pll_e);
	clk_prepare_enable(nvudc->pll_u_480M);

#if defined(CONFIG_ARCH_TEGRA_21x_SOC)
	nvudc_plat_pad_init(nvudc);
#endif
	spin_lock_irqsave(&nvudc->lock, flags);
	nvudc->is_suspended = false;
	spin_unlock_irqrestore(&nvudc->lock, flags);

	if (!pm_runtime_status_suspended(nvudc->dev))  {
		tegra_xudc_exit_elpg(nvudc);
		vbus_detected(nvudc);
	}

	/* Update current status of cable */
	extcon_notifications(&nvudc->vbus_extcon_nb, 0, NULL);

	return 0;
}

static const char * const tegra210_xudc_supply_names[] = {
	"hvdd_usb",		/* TODO 3.3V */
	"avddio_usb",		/* TODO 1.05V */
	"avdd_pll_utmip",	/* TODO 1.8V */
};

static struct tegra_xudc_soc_data tegra210_xudc_soc_data = {
	.device_id = XUDC_DEVICE_ID_T210,
	.supply_names = tegra210_xudc_supply_names,
	.num_supplies = ARRAY_SIZE(tegra210_xudc_supply_names),
};

static const char * const tegra186_xudc_supply_names[] = {
	/* for USB2 pads */
	"avdd-usb",

	/* for PEX USB pads */
	"dvdd-pex",
	"hvdd-pex",

	/* for PEX PLL */
	"dvdd-pex-pll",
	"hvdd-pex-pll",
};

static struct tegra_xudc_soc_data tegra186_xudc_soc_data = {
	.device_id = XUDC_DEVICE_ID_T186,
	.supply_names = tegra186_xudc_supply_names,
	.num_supplies = ARRAY_SIZE(tegra186_xudc_supply_names),
};

static struct of_device_id tegra_xudc_of_match[] = {
	{.compatible = "nvidia,tegra210-xudc", &tegra210_xudc_soc_data},
	{.compatible = "nvidia,tegra186-xudc", &tegra186_xudc_soc_data},
	{},
};


static int tegra_xudc_plat_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	const struct of_device_id *match;
	struct nv_udc_s *nvudc;
	int i;
	int err;
	u32 val;
	int partition_id_xusba, partition_id_xusbb;

	if (!dev->dma_mask)
		dev->dma_mask = &dev->coherent_dma_mask;

	err = dma_set_mask(dev, DMA_BIT_MASK(64));
	if (err) {
		dev_warn(dev, "no suitable DMA available\n");
		return err;
	} else
		dma_set_coherent_mask(dev, DMA_BIT_MASK(64));

	nvudc = devm_kzalloc(dev, sizeof(*nvudc), GFP_ATOMIC);
	if (!nvudc) {
		dev_err(dev, "failed to allocate memory for nvudc\n");
		return -ENOMEM;
	}

	match = of_match_device(tegra_xudc_of_match, &pdev->dev);
	if (!match) {
		dev_err(dev, "Error: No device match found\n");
		return -ENODEV;
	}
	nvudc->soc = (struct tegra_xudc_soc_data *) match->data;

	INIT_WORK(&nvudc->ucd_work, tegra_xudc_ucd_work);
	INIT_WORK(&nvudc->current_work, tegra_xudc_current_work);
	INIT_DELAYED_WORK(&nvudc->non_std_charger_work,
					tegra_xudc_non_std_charger_work);
	INIT_DELAYED_WORK(&nvudc->port_reset_war_work,
					tegra_xudc_port_reset_war_work);
	INIT_DELAYED_WORK(&nvudc->plc_reset_war_work,
					tegra_xudc_plc_reset_war_work);

	nvudc->pdev.plat = pdev;
	nvudc->dev = dev;
	platform_set_drvdata(pdev, nvudc);

	if (XUDC_IS_T210(nvudc))
		nvudc_get_bdata(nvudc);

	if (tegra_platform_is_fpga()) {
		fpga_hack_init(pdev);
		fpga_hack_setup_car(nvudc);
	}

	err = nvudc_plat_regulators_init(nvudc);
	if (err) {
		dev_err(dev, "failed to init regulators\n");
		goto err_kfree_nvudc;
	}

#if defined(CONFIG_ARCH_TEGRA_21x_SOC)
	if (pex_usb_pad_pll_reset_deassert())
		dev_err(dev, "failed to deassert pex pll\n");
#endif

	err = nvudc_plat_clocks_init(nvudc);
	if (err) {
		dev_err(dev, "failed to init clocks\n");
		if (tegra_platform_is_fpga())
			dev_info(dev, "ignore clock failures for fpga\n");
		else
			goto err_regulators_deinit;
	}

	err = nvudc_plat_mmio_regs_init(nvudc);
	if (err) {
		dev_err(dev, "failed to init mmio regions\n");
		goto err_clocks_deinit;
	}

	partition_id_xusba = tegra_pd_get_powergate_id(tegra_xusba_pd);
	if (partition_id_xusba < 0)
		return -EINVAL;
	err = tegra_unpowergate_partition_with_clk_on(partition_id_xusba);
	if (err) {
		dev_err(dev, "failed to unpowergate XUSBA partition\n");
		if (tegra_platform_is_fpga())
			dev_info(dev, "ignore powergate failures for fpga\n");
		else
			goto err_clocks_deinit;
	}

	partition_id_xusbb = tegra_pd_get_powergate_id(tegra_xusbb_pd);
	if (partition_id_xusbb < 0)
		return -EINVAL;
	err = tegra_unpowergate_partition_with_clk_on(partition_id_xusbb);
	if (err) {
		dev_err(dev, "failed to unpowergate XUSBB partition\n");
		if (tegra_platform_is_fpga())
			dev_info(dev, "ignore powergate failures for fpga\n");
		else
			goto err_powergate_xusba;
	}

	err = nvudc_plat_clocks_enable(nvudc);
	if (err) {
		dev_err(dev, "failed to enable clocks\n");
		if (tegra_platform_is_fpga())
			dev_info(dev, "ignore clock failures for fpga\n");
		else
			goto err_powergate_xusbb;
	}

#if defined(CONFIG_ARCH_TEGRA_21x_SOC)
	err = nvudc_plat_pad_init(nvudc);
	if (err) {
		dev_err(dev, "failed to config pads\n");
		goto err_clocks_disable;
	}
#elif defined(CONFIG_ARCH_TEGRA_18x_SOC)
	err = nvudc_plat_phy_init(nvudc);
	if (err) {
		if (err != -EPROBE_DEFER)
			dev_err(dev, "failed to config pads\n");
		goto err_clocks_disable;
	}
	err = nvudc_plat_phy_power_on(nvudc);
	if (err)
		dev_err(dev, "phy power on failed %d\n", err);
#endif

	nvudc_plat_fpci_ipfs_init(nvudc);

	err = nvudc_plat_irqs_init(nvudc);
	if (err) {
		dev_err(dev, "failed to init irqs\n");
		goto err_clocks_disable;
	}

	spin_lock_init(&nvudc->lock);
	mutex_init(&nvudc->elpg_lock);

	val = ioread32(nvudc->fpci + XUSB_DEV_CFG_0);
	val = (val >> 16) & 0xffff;
	if (likely(val == nvudc->soc->device_id)) {
		dev_info(dev, "%s XUSB Device Mode Controller\n",
			xudc_get_chip_name(nvudc));
	} else {
		dev_err(dev, "Device ID doesn't match: hardware 0x%x DT 0x%x\n",
			val, nvudc->soc->device_id);
		err = -ENODEV;
		goto err_clocks_disable;
	}

	for (i = 0; i < 32; i++)
		nvudc->udc_ep[i].nvudc = nvudc;

	dev_set_name(&nvudc->gadget.dev, "gadget");
	nvudc->gadget.dev.parent = &pdev->dev;
	nvudc->gadget.dev.release = nvudc_gadget_release;
	nvudc->gadget.ops = &nvudc_gadget_ops;
	nvudc->gadget.ep0 = &nvudc->udc_ep[0].usb_ep;
	INIT_LIST_HEAD(&nvudc->gadget.ep_list);
	nvudc->gadget.name = driver_name;
	nvudc->gadget.max_speed = USB_SPEED_SUPER;

	init_chip(nvudc);

	nvudc->mmio_reg_base = nvudc->base; /* TODO support device context */

	err = reset_data_struct(nvudc);
	if (err) {
		dev_err(dev, "failed to reset_data_struct\n");
		goto err_clocks_disable;
	}

	err = EP0_Start(nvudc);
	if (err) {
		dev_err(dev, "failed to EP0_Start\n");
		goto err_clocks_disable;
	}

#if defined(CONFIG_ARCH_TEGRA_21x_SOC)
	if (nvudc->bdata.otg_portmap & (0xff << XUSB_UTMI_INDEX)) {
		nvudc->phy = usb_get_phy(USB_PHY_TYPE_USB3);
		if (IS_ERR_OR_NULL(nvudc->phy)) {
			dev_err(dev, "USB_PHY_TYPE_USB3 not found\n");
			nvudc->phy = NULL;
			goto err_clocks_disable;
		} else {
			nvudc->gadget.is_otg = 1;
		}
	}
#elif defined(CONFIG_ARCH_TEGRA_18x_SOC)
	/* TODO: T186 OTG */
#endif

	err = usb_add_gadget_udc(&pdev->dev, &nvudc->gadget);
	if (err) {
		dev_err(dev, "failed to usb_add_gadget_udc\n");
		goto err_clocks_disable;
	}

	the_controller = nvudc; /* TODO support device context */

	/* Enable runtime PM */
	pm_runtime_use_autosuspend(&pdev->dev);
	pm_runtime_set_autosuspend_delay(&pdev->dev, 2000);
	pm_runtime_set_active(&pdev->dev);
	pm_runtime_mark_last_busy(nvudc->dev);
	pm_runtime_enable(&pdev->dev);

	nvudc->is_suspended = false;
	nvudc->vbus_detected = false;
	nvudc->extcon_event_processing = false;
	nvudc->current_ma = USB_ANDROID_SUSPEND_CURRENT_MA;
	nvudc->ucd = tegra_usb_get_ucd();
	if (IS_ERR(nvudc->ucd)) {
		dev_info(dev, "charger detection handle not available\n");
		nvudc->ucd = NULL;
	}

	if (tegra_platform_is_silicon()) {
		/* TODO: support non-dt ?*/
		nvudc->vbus_extcon_dev =
			extcon_get_extcon_dev_by_cable(&pdev->dev, "vbus");

		if (IS_ERR_OR_NULL(nvudc->vbus_extcon_dev)) {
			err = PTR_ERR(nvudc->vbus_extcon_dev);
			pr_info("extcon_get_extcon_dev_by_cable failed %ld\n",
				PTR_ERR(nvudc->vbus_extcon_dev));
			if (!nvudc->vbus_extcon_dev)
				err = -ENODEV;
			goto err_del_gadget_udc;
		} else {
			nvudc->vbus_extcon_nb.notifier_call =
							extcon_notifications;
			extcon_register_notifier(nvudc->vbus_extcon_dev,
				&nvudc->vbus_extcon_nb);
		}
	} else if (tegra_platform_is_fpga())
		nvudc->vbus_detected = true;

	tegra_pd_add_device(&pdev->dev);
	tegra_xudc_boost_cpu_init(nvudc);

	return 0;

err_del_gadget_udc:
	usb_del_gadget_udc(&nvudc->gadget);
err_clocks_disable:
	nvudc_plat_clocks_disable(nvudc);
err_powergate_xusbb:
	tegra_powergate_partition_with_clk_off(partition_id_xusbb);
err_powergate_xusba:
	tegra_powergate_partition_with_clk_off(partition_id_xusba);
err_clocks_deinit:
	nvudc_plat_clocks_deinit(nvudc);
err_regulators_deinit:
	nvudc_plat_regulator_deinit(nvudc);
err_kfree_nvudc:
	devm_kfree(dev, nvudc);
	return err;
}

static int __exit tegra_xudc_plat_remove(struct platform_device *pdev)
{
	struct nv_udc_s *nvudc = platform_get_drvdata(pdev);
	struct device *dev = &pdev->dev;
	int partition_id_xusba, partition_id_xusbb;

	dev_info(dev, "%s nvudc %p\n", __func__, nvudc);

	/* TODO implement synchronization */
	if (nvudc) {
		tegra_xudc_boost_cpu_deinit(nvudc);
		tegra_usb_release_ucd(nvudc->ucd);
		cancel_work_sync(&nvudc->ucd_work);
		cancel_work_sync(&nvudc->current_work);
		cancel_delayed_work(&nvudc->non_std_charger_work);
		cancel_delayed_work(&nvudc->port_reset_war_work);
		cancel_delayed_work(&nvudc->plc_reset_war_work);
		usb_del_gadget_udc(&nvudc->gadget);
		free_data_struct(nvudc);
		nvudc_plat_clocks_disable(nvudc);
		partition_id_xusbb = tegra_pd_get_powergate_id(tegra_xusbb_pd);
		if (partition_id_xusbb < 0)
			return -EINVAL;

		partition_id_xusba = tegra_pd_get_powergate_id(tegra_xusba_pd);
		if (partition_id_xusba < 0)
			return -EINVAL;
		tegra_powergate_partition(partition_id_xusbb);
		tegra_powergate_partition(partition_id_xusba);
		nvudc_plat_clocks_deinit(nvudc);
#if defined(CONFIG_ARCH_TEGRA_21x_SOC)
		if (pex_usb_pad_pll_reset_assert())
			pr_err("Fail to assert pex pll\n");
#endif
		nvudc_plat_regulator_deinit(nvudc);
		if (nvudc->prod_list)
			tegra_prod_release(&nvudc->prod_list);
		extcon_unregister_notifier(nvudc->vbus_extcon_dev,
			&nvudc->vbus_extcon_nb);
		devm_kfree(dev, nvudc);
		platform_set_drvdata(pdev, NULL);
	}
	return 0;
}

static void tegra_xudc_plat_shutdown(struct platform_device *pdev)
{
	struct nv_udc_s *nvudc = platform_get_drvdata(pdev);
	struct device *dev = &pdev->dev;

	dev_info(dev, "%s nvudc %p\n", __func__, nvudc);

	tegra_xudc_boost_cpu_deinit(nvudc);
}

static const struct dev_pm_ops tegra_xudc_pm_ops = {
	.runtime_suspend = tegra_xudc_runtime_suspend,
	.runtime_resume = tegra_xudc_runtime_resume,
	.suspend = nvudc_suspend_platform,
	.resume = nvudc_resume_platform,
};

static struct platform_driver tegra_xudc_driver = {
	.probe = tegra_xudc_plat_probe,
	.remove = __exit_p(tegra_xudc_plat_remove),
	.shutdown = tegra_xudc_plat_shutdown,
	.driver = {
		.name = driver_name,
		.of_match_table = tegra_xudc_of_match,
		.owner = THIS_MODULE,
		.pm = &tegra_xudc_pm_ops,
	},
	/* TODO support PM */
};

static DEFINE_PCI_DEVICE_TABLE(nvpci_ids) = {
	{
		.class = ((PCI_CLASS_SERIAL_USB << 8) | 0xfe),
		.class_mask = ~0,
		.vendor = XUDC_VENDOR_ID,
		.device = XUDC_DEVICE_ID_T210,
		.subvendor = PCI_ANY_ID,
		.subdevice = PCI_ANY_ID,
	},
	{
		.class = ((PCI_CLASS_SERIAL_USB << 8) | 0xfe),
		.class_mask = ~0,
		.vendor = XUDC_VENDOR_ID,
		.device = XUDC_DEVICE_ID_T186,
		.subvendor = PCI_ANY_ID,
		.subdevice = PCI_ANY_ID,
	},
	{
		/* end: all zeroes */
	}
};
MODULE_DEVICE_TABLE(pci, nvpci_ids);

static struct pci_driver nvudc_driver_pci = {
	.name = driver_name,
	.id_table = nvpci_ids,
	.probe = nvudc_probe_pci,
	.remove = nvudc_remove_pci,
};

static int __init udc_init(void)
{
	int ret;

	ret = platform_driver_register(&tegra_xudc_driver);
	if (ret) {
		pr_err("%s failed to register platform driver %d\n",
				__func__, ret);
		return ret;
	}

	ret = pci_register_driver(&nvudc_driver_pci);
	if (ret) {
		pr_err("%s failed to register pci driver %d\n",
				__func__, ret);
		platform_driver_unregister(&tegra_xudc_driver);
		return ret;
	}

	return 0;
}

static void __exit udc_exit(void)
{
	pci_unregister_driver(&nvudc_driver_pci);
	platform_driver_unregister(&tegra_xudc_driver);
}

module_init(udc_init);
module_exit(udc_exit);

MODULE_DESCRIPTION("NV Usb3 Peripheral Controller");
MODULE_VERSION("0.0.1");
MODULE_AUTHOR("Hui Fu");
MODULE_LICENSE("GPL v2");
