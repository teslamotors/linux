/*
* tegra-xotg.h - Nvidia XOTG implementation
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
#include<linux/usb/otg.h>

/* variables set by the user space app */
struct xotg_app_request {
	int a_bus_drop;
	int a_bus_req;
	int b_bus_req;
	int b_srp_init;
};

/* parameters/internal variables/output defined in the OTG spec */
struct xotg_vars {
	/* B-device state machine parameters */
	/* input variables */
	int a_bus_resume;
	int a_bus_suspend;
	int a_conn;
	int b_se0_srp;
	int b_ssend_srp;
	int b_sess_vld;
	int power_up;

	/* internal variables */
	int b_srp_done;
	int b_hnp_en;

	/* A-device state machine parameters */
	/* input variables */
	int a_sess_vld;
	int a_srp_det;
	int a_vbus_vld;
	int b_conn;

	/* internal variables */
	int a_set_b_hnp_en;	/* will be set by the HCD/usbcore */
	int otg_test_device_enumerated;
	int start_tst_maint_timer;

	/* non-spec otg driver variables */
	int b_srp_initiated;

	/* Think we do not need the below */
	int b_bus_resume;
	int b_bus_suspend;
};

struct xotg_timers {
	struct timer_list a_wait_vrise_tmr;
	int a_wait_vrise_tmout;
	struct timer_list a_wait_vfall_tmr;
	int a_wait_vfall_tmout;
	struct timer_list a_wait_bcon_tmr;
	int a_wait_bcon_tmout;
	struct timer_list a_aidl_bdis_tmr;
	int a_aidl_bdis_tmout;
	struct timer_list a_bidl_adis_tmr;
	int a_bidl_adis_tmout;
	struct timer_list b_ase0_brst_tmr;
	int b_ase0_brst_tmout;

	/* driver timers */
	struct timer_list b_ssend_srp_tmr;
	int b_ssend_srp_tmout;

	/* timer used to wait for SRP response */
	struct timer_list b_srp_response_wait_tmr;
	int b_srp_response_wait_tmout;
	struct timer_list b_srp_done_tmr;
	int b_srp_done_tmout;

	struct timer_list a_tst_maint_tmr;
	int a_tst_maint_tmout;
	/* test timer */
	struct timer_list test_tmr;
};

/* nvidia otg controller Structure */
struct xotg {
	u8 hs_otg_port;
	u8 ss_otg_port;
	struct usb_phy phy;
	struct device *dev;
	struct platform_device *pdev;
	spinlock_t lock;
	spinlock_t vbus_lock;
	int nv_irq;
	int usb_irq;
	int id;

	/* vbus */
	struct regulator *usb_vbus_reg;
	struct work_struct vbus_work;
	bool vbus_enabled;

	/* extcon */
	struct extcon_dev *id_extcon_dev;
	struct notifier_block id_extcon_nb;
	bool id_grounded;
	bool device_connected;
	u32 usb2_id;

	/* role swap */
	struct xotg_app_request xotg_reqs;
	struct xotg_vars xotg_vars;
	struct xotg_timers xotg_timer_list;
	u32 test_timer_timeout;
	struct work_struct otg_work;
	struct workqueue_struct *otg_wq;
	bool vbus_en_started;
	bool vbus_dis_started;
	bool vbus_on;
	unsigned long vbus_on_jiffies;
};

/* timer constants, all in ms */
#define TA_VBUS_RISE			(500)	/* Table 4-1, OTG2.0 */
#define TA_WAIT_BCON			(9000)	/* Table 5-1, OTG2.0 */
#define TA_AIDL_BDIS			(250)	/* Table 5-1, OTG2.0 */
#define TA_BIDL_ADIS			(155)	/* Table 5-1, OTG2.0 */
#define TB_AIDL_BDIS			(15000)	/* Table 5-1, OTG2.0 */
#define TB_ASE0_BRST			(255)	/* Table 5-1, OTG2.0 */
#define TB_SE0_SRP			(1000)	/* Table 5-1, OTG2.0 */
#define TB_SSEND_SRP			(1600)	/* Table 5-1, OTG2.0 */
#define TB_SRP_FAIL			(5500)	/* Table 5-1, OTG2.0 */
#define TB_SRP_DONE			(100)
#define TA_WAIT_VFALL			(1000)	/* Table 4-1, OTG2.0 */
#define TA_TST_MAINT			(9900)
/* for communication with xhci driver */
#define XDEV_DISABLED	(0x4 << 5)
#define XDEV_RXDETECT	(0x5 << 5)

extern int xotg_debug_level;
#define LEVEL_DEBUG	4
#define LEVEL_INFO	3
#define LEVEL_WARNING	2
#define LEVEL_ERROR	1

#define xotg_dbg(dev, fmt, args...) { \
	if (xotg_debug_level >= LEVEL_DEBUG) \
		dev_info(dev, "%s():%d: " fmt, __func__ , __LINE__, ## args); \
	}
#define xotg_info(dev, fmt, args...) { \
	if (xotg_debug_level >= LEVEL_INFO) \
		dev_info(dev, "%s():%d: " fmt, __func__ , __LINE__, ## args); \
	}
#define xotg_err(dev, fmt, args...) { \
	if (xotg_debug_level >= LEVEL_ERROR) \
		dev_err(dev, fmt, ## args); \
	}
#define xotg_warn(dev, fmt, args...) { \
	if (xotg_debug_level >= LEVEL_WARNING) \
		dev_warn(dev, fmt, ## args); \
	}

#define xotg_method_entry(dev) xotg_dbg(dev, "enter\n")
#define xotg_method_exit(dev) xotg_dbg(dev, "exit\n")
