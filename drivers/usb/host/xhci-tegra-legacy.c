/*
 * xhci-tegra-legacy.c - Nvidia xHCI host controller driver
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

#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/ioport.h>
#include <linux/dma-mapping.h>
#include <linux/irq.h>
#include <linux/regulator/consumer.h>
#include <linux/platform_data/tegra_usb.h>
#include <linux/uaccess.h>
#include <linux/circ_buf.h>
#include <linux/vmalloc.h>
#include <linux/debugfs.h>
#include <linux/kthread.h>
#include <linux/gpio.h>
#include <linux/usb/otg.h>
#include <linux/clk/tegra.h>
#include <linux/tegra-powergate.h>
#include <linux/firmware.h>
#include <linux/pm_runtime.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/tegra-fuse.h>
#include <linux/tegra_pm_domains.h>
#include <linux/tegra_prod.h>
#include <linux/tegra-soc.h>

#include <dt-bindings/usb/tegra_xhci.h>
#include <mach/tegra_usb_pad_ctrl.h>
#include <mach/tegra_usb_pmc.h>
#include <mach/xusb.h>

#include <linux/platform/tegra/mc.h>

#include "xhci-tegra-legacy.h"
#include "xhci.h"
#include "../../../arch/arm/mach-tegra/iomap.h"

#if defined(CONFIG_ARCH_TEGRA_21x_SOC)
#include "xhci-tegra-t210-padreg.h"
#else
#include "xhci-tegra-t124-padreg.h"
#endif

/* macros */
#define FW_IOCTL_LOG_DEQUEUE_LOW	(4)
#define FW_IOCTL_LOG_DEQUEUE_HIGH	(5)
#define FW_IOCTL_DATA_SHIFT		(0)
#define FW_IOCTL_DATA_MASK		(0x00ffffff)
#define FW_IOCTL_TYPE_SHIFT		(24)
#define FW_IOCTL_TYPE_MASK		(0xff000000)
#define FW_LOG_SIZE			((int) sizeof(struct log_entry))
#define FW_LOG_COUNT			(4096)
#define FW_LOG_RING_SIZE		(FW_LOG_SIZE * FW_LOG_COUNT)
#define FW_LOG_PAYLOAD_SIZE		(27)
#define DRIVER				(0x01)
#define CIRC_BUF_SIZE			(4 * (1 << 20))	/* 4MB */
#define FW_LOG_THREAD_RELAX		(msecs_to_jiffies(100))

/* tegra_xhci_firmware_log.flags bits */
#define FW_LOG_CONTEXT_VALID		(0)
#define FW_LOG_FILE_OPENED		(1)

#define PAGE_SELECT_MASK			0xFFFFFE00
#define PAGE_SELECT_SHIFT			9
#define PAGE_OFFSET_MASK			0x000001FF
#define CSB_PAGE_SELECT(_addr)						\
	({								\
		typecheck(u32, _addr);					\
		((_addr & PAGE_SELECT_MASK) >> PAGE_SELECT_SHIFT);	\
	})
#define CSB_PAGE_OFFSET(_addr)						\
	({								\
		typecheck(u32, _addr);					\
		(_addr & PAGE_OFFSET_MASK);				\
	})

#define PMC_PORTMAP_MASK(map, pad)	(((map) >> 4*(pad)) & 0xF)

#define PMC_USB_DEBOUNCE_DEL_0			0xec
#define   UTMIP_LINE_DEB_CNT(x)		(((x) & 0xf) << 16)
#define   UTMIP_LINE_DEB_CNT_MASK		(0xf << 16)

#define PMC_UTMIP_UHSIC_SLEEP_CFG_0		0x1fc

/* XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPADX_CTLY_0 register */
#define PD_CHG					(1 << 0)
#define ON_SRC_EN				(1 << 12)
/* XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPADX_CTLY_0 register */
#define PD2						(1 << 20)

/* Production setting types */
#define XUSB_PROD_PREFIX_UTMI	"prod_c_utmi"
#define XUSB_PROD_PREFIX_HSIC	"prod_c_hsic"
#define XUSB_PROD_PREFIX_SS	"prod_c_ss"
#define XUSB_PROD_PREFIX_SATA	"prod_c_sata"

static struct of_device_id tegra_xusba_pd[] = {
	{ .compatible = "nvidia,tegra210-xusba-pd", },
	{ .compatible = "nvidia,tegra132-xusba-pd", },
	{},
};

static struct of_device_id tegra_xusbc_pd[] = {
	{ .compatible = "nvidia,tegra210-xusbc-pd", },
	{ .compatible = "nvidia,tegra132-xusbc-pd", },
	{},
};

/* private data types */
/* command requests from the firmware */
enum MBOX_CMD_TYPE {
	MBOX_CMD_MSG_ENABLED = 1,
	MBOX_CMD_INC_FALC_CLOCK,
	MBOX_CMD_DEC_FALC_CLOCK,
	MBOX_CMD_INC_SSPI_CLOCK,
	MBOX_CMD_DEC_SSPI_CLOCK, /* 5 */
	MBOX_CMD_SET_BW,
	MBOX_CMD_SET_SS_PWR_GATING,
	MBOX_CMD_SET_SS_PWR_UNGATING, /* 8 */
	MBOX_CMD_SAVE_DFE_CTLE_CTX,
	MBOX_CMD_AIRPLANE_MODE_ENABLED, /* unused */
	MBOX_CMD_AIRPLANE_MODE_DISABLED, /* 11, unused */
	MBOX_CMD_STAR_HSIC_IDLE,
	MBOX_CMD_STOP_HSIC_IDLE,
	MBOX_CMD_DBC_WAKE_STACK, /* unused */
	MBOX_CMD_HSIC_PRETEND_CONNECT,
	MBOX_CMD_RESET_SSPI,
	MBOX_CMD_DISABLE_SS_LFPS_DETECTION,
	MBOX_CMD_ENABLE_SS_LFPS_DETECTION,

	/* needs to be the last cmd */
	MBOX_CMD_MAX,

	/* resp msg to ack above commands */
	MBOX_CMD_ACK = 128,
	MBOX_CMD_NACK
};

struct log_entry {
	u32 sequence_no;
	u8 data[FW_LOG_PAYLOAD_SIZE];
	u8 owner;
};

enum build_info_log {
	LOG_NONE = 0,
	LOG_MEMORY
};

/* Usb3 Firmware Cfg Table */
#define	FW_MAJOR_VERSION(x)	(((x) >> 24) & 0xff)
#define	FW_MINOR_VERSION(x)	(((x) >> 16) & 0xff)
#define	FW_LOG_TYPE_DMA_SYS_MEM	(0x1)
struct cfgtbl {
	u32 boot_loadaddr_in_imem;
	u32 boot_codedfi_offset;
	u32 boot_codetag;
	u32 boot_codesize;

	/* Physical memory reserved by Bootloader/BIOS */
	u32 phys_memaddr;
	u16 reqphys_memsize;
	u16 alloc_phys_memsize;

	/* .rodata section */
	u32 rodata_img_offset;
	u32 rodata_section_start;
	u32 rodata_section_end;
	u32 main_fnaddr;

	u32 fwimg_cksum;
	u32 fwimg_created_time;

	/* Fields that get filled by linker during linking phase
	 * or initialized in the FW code.
	 */
	u32 imem_resident_start;
	u32 imem_resident_end;
	u32 idirect_start;
	u32 idirect_end;
	u32 l2_imem_start;
	u32 l2_imem_end;
	u32 version_id;
	u8 init_ddirect;
	u8 reserved[3];
	u32 phys_addr_log_buffer;
	u32 total_log_entries;
	u32 dequeue_ptr;

	/*	Below two dummy variables are used to replace
	 *	L2IMemSymTabOffsetInDFI and L2IMemSymTabSize in order to
	 *	retain the size of struct _CFG_TBL used by other AP/Module.
	 */
	u32 dummy_var1;
	u32 dummy_var2;

	/* fwimg_len */
	u32 fwimg_len;
	u8 magic[8];
	u32 ss_low_power_entry_timeout;
	u8 num_hsic_port;
	u8 ss_portmap;
	u8 build_log:4;
	u8 build_type:4;
	u8 padding[137]; /* padding bytes to makeup 256-bytes cfgtbl */
};
static int tegra_xhci_probe2(struct tegra_xhci_hcd *tegra);
static int tegra_xhci_remove(struct platform_device *pdev);
static void set_port_cdp(struct tegra_xhci_hcd *tegra, bool enable, int pad);
static void init_filesystem_firmware_done(const struct firmware *fw,
					void *context);
static int get_host_controlled_ports(struct tegra_xhci_hcd *tegra);
static struct work_struct tegra_xhci_reinit_work;
static void xhci_reinit_work(struct work_struct *work);
static bool reinit_started;
static struct tegra_usb_pmc_data *pmc_data;
static struct tegra_usb_pmc_data pmc_hsic_data[XUSB_HSIC_COUNT];
static void save_ctle_context(struct tegra_xhci_hcd *tegra,
	u8 port)  __attribute__ ((unused));

static bool en_hcd_reinit;
module_param(en_hcd_reinit, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(en_hcd_reinit, "Enable hcd reinit when hc died");

static char *firmware_file = "";
#define FIRMWARE_FILE_HELP	\
	"used to specify firmware file of Tegra XHCI host controller. "

module_param(firmware_file, charp, S_IRUGO);
MODULE_PARM_DESC(firmware_file, FIRMWARE_FILE_HELP);

/* functions */
#ifdef CONFIG_TEGRA_EHCI_BOOST_CPU_FREQ
static unsigned int boost_cpu_freq = CONFIG_TEGRA_EHCI_BOOST_CPU_FREQ;
module_param(boost_cpu_freq, uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(boost_cpu_freq, "CPU frequency (in KHz) to boost");

#define BOOST_PERIOD		(msecs_to_jiffies(2*1000)) /* 2 seconds */
#define BOOST_TRIGGER		16384 /* 16KB */
static void tegra_xusb_boost_cpu_freq_fn(struct work_struct *work)
{
	struct tegra_xhci_hcd *tegra = container_of(work,
					struct tegra_xhci_hcd,
					boost_cpufreq_work);
	unsigned long delay = BOOST_PERIOD;
	s32 cpufreq_hz = boost_cpu_freq * 1000;

	mutex_lock(&tegra->boost_cpufreq_lock);

	if (!tegra->cpufreq_boosted) {
		xhci_dbg(tegra->xhci, "boost cpu freq %d Hz\n", cpufreq_hz);
		pm_qos_update_request(&tegra->boost_cpufreq_req, cpufreq_hz);
		pm_qos_update_request(&tegra->boost_cpuon_req,
					PM_QOS_MAX_ONLINE_CPUS_DEFAULT_VALUE);
		tegra->cpufreq_boosted = true;
	}

	if (!tegra->restore_cpufreq_scheduled) {
		xhci_dbg(tegra->xhci, "%s schedule restore work\n", __func__);
		schedule_delayed_work(&tegra->restore_cpufreq_work, delay);
		tegra->restore_cpufreq_scheduled = true;
	}

	tegra->cpufreq_last_boosted = jiffies;

	mutex_unlock(&tegra->boost_cpufreq_lock);
}

static void tegra_xusb_restore_cpu_freq_fn(struct work_struct *work)
{
	struct tegra_xhci_hcd *tegra = container_of(work,
					struct tegra_xhci_hcd,
					restore_cpufreq_work.work);
	unsigned long delay = BOOST_PERIOD;

	mutex_lock(&tegra->boost_cpufreq_lock);

	if (time_is_after_jiffies(tegra->cpufreq_last_boosted + delay)) {
		xhci_dbg(tegra->xhci, "%s schedule restore work\n", __func__);
		schedule_delayed_work(&tegra->restore_cpufreq_work, delay);
		goto done;
	}

	xhci_dbg(tegra->xhci, "%s restore cpufreq\n", __func__);
	pm_qos_update_request(&tegra->boost_cpufreq_req, PM_QOS_DEFAULT_VALUE);
	pm_qos_update_request(&tegra->boost_cpuon_req, PM_QOS_DEFAULT_VALUE);
	tegra->cpufreq_boosted = false;
	tegra->restore_cpufreq_scheduled = false;

done:
	mutex_unlock(&tegra->boost_cpufreq_lock);
}

static void tegra_xusb_boost_cpu_init(struct tegra_xhci_hcd *tegra)
{
	INIT_WORK(&tegra->boost_cpufreq_work, tegra_xusb_boost_cpu_freq_fn);

	INIT_DELAYED_WORK(&tegra->restore_cpufreq_work,
				tegra_xusb_restore_cpu_freq_fn);

	pm_qos_add_request(&tegra->boost_cpufreq_req,
				PM_QOS_CPU_FREQ_MIN, PM_QOS_DEFAULT_VALUE);

	pm_qos_add_request(&tegra->boost_cpuon_req,
				PM_QOS_MIN_ONLINE_CPUS, PM_QOS_DEFAULT_VALUE);

	mutex_init(&tegra->boost_cpufreq_lock);
}

static void tegra_xusb_boost_cpu_deinit(struct tegra_xhci_hcd *tegra)
{
	if (!pm_qos_request_active(&tegra->boost_cpufreq_req)) {
		pr_warn("deinit call when cpu boost not initialized\n");
		return;
	}
	cancel_work_sync(&tegra->boost_cpufreq_work);
	cancel_delayed_work_sync(&tegra->restore_cpufreq_work);

	pm_qos_remove_request(&tegra->boost_cpufreq_req);
	pm_qos_remove_request(&tegra->boost_cpuon_req);
	mutex_destroy(&tegra->boost_cpufreq_lock);
}

static bool tegra_xusb_boost_cpu_freq(struct tegra_xhci_hcd *tegra)
{
	if (tegra_dvfs_is_cpu_rail_connected_to_regulators())
		return schedule_work(&tegra->boost_cpufreq_work);
	else
		return false;
}
#else
static void tegra_xusb_boost_cpu_init(struct tegra_xhci_hcd *unused) {}
static void tegra_xusb_boost_cpu_deinit(struct tegra_xhci_hcd *unused) {}
static void tegra_xusb_boost_cpu_freq(struct tegra_xhci_hcd *unused) {}
#endif
static inline struct tegra_xhci_hcd *hcd_to_tegra_xhci(struct usb_hcd *hcd)
{
	return (struct tegra_xhci_hcd *) dev_get_drvdata(hcd->self.controller);
}

static inline void must_have_sync_lock(struct tegra_xhci_hcd *tegra)
{
#if defined(CONFIG_DEBUG_MUTEXES) || defined(CONFIG_SMP)
#if defined(CONFIG_PREEMPT_RT_FULL)
	WARN_ON(tegra->sync_lock.lock.owner != current);
#else
	WARN_ON(tegra->sync_lock.owner != current);
#endif
#endif
}

#define for_each_ss_pad(_pad, pad_count)	\
	for (_pad = 0; _pad < pad_count; _pad++)

#define for_each_enabled_ss_pad(_pad, _tegra_xhci_hcd)		\
	for (_pad = find_next_enabled_ss_pad(_tegra_xhci_hcd, 0);	\
		(_pad < (_tegra_xhci_hcd->soc_config->ss_pad_count))	\
		&& (_pad >= 0);	\
		_pad = find_next_enabled_ss_pad(_tegra_xhci_hcd, _pad + 1))

#define for_each_enabled_utmi_pad(_pad, _tegra_xhci_hcd)		\
	for (_pad = find_next_enabled_utmi_pad(_tegra_xhci_hcd, 0);	\
		(_pad < (_tegra_xhci_hcd->soc_config->utmi_pad_count))	\
		&& (_pad >= 0);	\
		_pad = find_next_enabled_utmi_pad(_tegra_xhci_hcd, _pad + 1))

#define for_each_enabled_utmi_pad_with_otg(_pad, _tegra_xhci_hcd)	\
	for (_pad = find_next_enabled_utmi_pad_with_otg(_tegra_xhci_hcd, 0); \
		(_pad < (_tegra_xhci_hcd->soc_config->utmi_pad_count))	\
		&& (_pad >= 0);	\
		_pad = \
		find_next_enabled_utmi_pad_with_otg(_tegra_xhci_hcd, _pad + 1))

#define for_each_enabled_hsic_pad(_pad, _tegra_xhci_hcd)		\
	for (_pad = find_next_enabled_hsic_pad(_tegra_xhci_hcd, 0);	\
	    (_pad < XUSB_HSIC_COUNT) && (_pad >= 0);			\
	    _pad = find_next_enabled_hsic_pad(_tegra_xhci_hcd, _pad + 1))

static inline int find_next_enabled_pad(struct tegra_xhci_hcd *tegra,
						int start, int last)
{
	unsigned long portmap = tegra->bdata->portmap;

	return find_next_bit(&portmap, last , start);
}

static inline int find_next_enabled_pad_with_otg(struct tegra_xhci_hcd *tegra,
						int start, int last)
{
	unsigned long portmap = tegra->bdata->portmap |
				tegra->bdata->otg_portmap;
	return find_next_bit(&portmap, last , start);
}

static inline int find_next_enabled_hsic_pad(struct tegra_xhci_hcd *tegra,
						int curr_pad)
{
	int start = XUSB_HSIC_INDEX + curr_pad;
	int last = XUSB_HSIC_INDEX + XUSB_HSIC_COUNT;

	if ((curr_pad < 0) || (curr_pad >= XUSB_HSIC_COUNT))
		return -1;

	return find_next_enabled_pad(tegra, start, last) - XUSB_HSIC_INDEX;
}
static inline int find_next_enabled_utmi_pad(struct tegra_xhci_hcd *tegra,
				int curr_pad)
{
	int utmi_pads = tegra->soc_config->utmi_pad_count;
	int start = XUSB_UTMI_INDEX + curr_pad;
	int last = XUSB_UTMI_INDEX + utmi_pads;

	if ((curr_pad < 0) || (curr_pad >= utmi_pads))
		return -1;
	return find_next_enabled_pad(tegra, start, last) - XUSB_UTMI_INDEX;
}

static inline int find_next_enabled_utmi_pad_with_otg(
	struct tegra_xhci_hcd *tegra, int curr_pad)
{
	int utmi_pads = tegra->soc_config->utmi_pad_count;
	int start = XUSB_UTMI_INDEX + curr_pad;
	int last = XUSB_UTMI_INDEX + utmi_pads;

	if ((curr_pad < 0) || (curr_pad >= utmi_pads))
		return -1;
	return find_next_enabled_pad_with_otg(tegra, start, last) -
					XUSB_UTMI_INDEX;
}

static inline int find_next_enabled_ss_pad(struct tegra_xhci_hcd *tegra,
						int curr_pad)
{
	int ss_pads = tegra->soc_config->ss_pad_count;
	int start = XUSB_SS_INDEX + curr_pad;
	int last = XUSB_SS_INDEX + ss_pads;

	if ((curr_pad < 0) || (curr_pad >= ss_pads))
		return -1;

	return find_next_enabled_pad(tegra, start, last) - XUSB_SS_INDEX;
}

static void tegra_xhci_setup_gpio_for_ss_lane(struct tegra_xhci_hcd *tegra)
{
	int err = 0;

	if (!tegra->bdata->gpio_controls_muxed_ss_lanes)
		return;

	if (tegra->bdata->lane_owner & BIT(0)) {
		/* USB3_SS port1 is using SATA lane so set (MUX_SATA)
		 * GPIO P11 to '0'
		 */
		err = gpio_request((tegra->bdata->gpio_ss1_sata & 0xffff),
			"gpio_ss1_sata");
		if (err < 0)
			pr_err("%s: gpio_ss1_sata gpio_request failed %d\n",
				__func__, err);
		err = gpio_direction_output((tegra->bdata->gpio_ss1_sata
			& 0xffff), 1);
		if (err < 0)
			pr_err("%s: gpio_ss1_sata gpio_direction failed %d\n",
				__func__, err);
		__gpio_set_value((tegra->bdata->gpio_ss1_sata & 0xffff),
				((tegra->bdata->gpio_ss1_sata >> 16)));
	}
}

static u32 xhci_read_portsc(struct xhci_hcd *xhci, unsigned int port)
{
	int num_ports = HCS_MAX_PORTS(xhci->hcs_params1);
	__le32 __iomem *addr;

	if (port >= num_ports) {
		xhci_err(xhci, "%s invalid port %u\n", __func__, port);
		return -1;
	}

	addr = &xhci->op_regs->port_status_base + (NUM_PORT_REGS * port);
	return xhci_readl(xhci, addr);
}

static void debug_print_portsc(struct xhci_hcd *xhci)
{
	__le32 __iomem *addr = &xhci->op_regs->port_status_base;
	u32 reg;
	int i;
	int ports;

	ports = HCS_MAX_PORTS(xhci->hcs_params1);
	for (i = 0; i < ports; i++) {
		reg = xhci_read_portsc(xhci, i);
		xhci_dbg(xhci, "@%p port %d status reg = 0x%x\n",
				addr, i, (unsigned int) reg);
		addr += NUM_PORT_REGS;
	}
}
static void tegra_xhci_war_for_tctrl_rctrl(struct tegra_xhci_hcd *tegra);

static bool is_otg_host(struct tegra_xhci_hcd *tegra)
{
	if (!tegra->transceiver)
		return true;
	else if (tegra->transceiver->state == OTG_STATE_A_HOST)
		return true;
	else
		return false;
}

static int get_usb2_port_speed(struct tegra_xhci_hcd *tegra, u8 port)
{
	struct xhci_hcd *xhci = tegra->xhci;
	u32 portsc;

	portsc = xhci_readl(xhci, xhci->usb2_ports[port]);
	if (DEV_FULLSPEED(portsc))
		return USB_PMC_PORT_SPEED_FULL;
	else if (DEV_HIGHSPEED(portsc))
		return USB_PMC_PORT_SPEED_HIGH;
	else if (DEV_LOWSPEED(portsc))
		return USB_PMC_PORT_SPEED_LOW;
	else if (DEV_SUPERSPEED(portsc))
		return USB_PMC_PORT_SPEED_SUPER;
	else
		return USB_PMC_PORT_SPEED_UNKNOWN;
}

static void pmc_init(struct tegra_xhci_hcd *tegra)
{
	struct tegra_usb_pmc_data *pmc;
	struct device *dev = &tegra->pdev->dev;
	int pad, utmi_pad_count;

	utmi_pad_count = tegra->soc_config->utmi_pad_count;

	pmc_data = kzalloc(sizeof(struct tegra_usb_pmc_data) *
			utmi_pad_count, GFP_KERNEL);

	for (pad = 0; pad < utmi_pad_count; pad++) {
		if ((BIT(XUSB_UTMI_INDEX + pad) & tegra->bdata->portmap) ||
			(pad == tegra->hs_otg_portnum)) {
			dev_dbg(dev, "%s utmi pad %d\n", __func__, pad);
			pmc = &pmc_data[pad];
			if (tegra->soc_config->pmc_portmap) {
				pmc->instance = PMC_PORTMAP_MASK(
						tegra->soc_config->pmc_portmap,
						pad);
			} else {
				pmc->instance = pad;
			}
			pmc->phy_type = TEGRA_USB_PHY_INTF_UTMI;
			pmc->port_speed = USB_PMC_PORT_SPEED_UNKNOWN;
			pmc->controller_type = TEGRA_USB_3_0;
			tegra_usb_pmc_init(pmc);
		}
	}

	for_each_enabled_hsic_pad(pad, tegra) {
		dev_dbg(dev, "%s hsic pad %d\n", __func__, pad);
		pmc = &pmc_hsic_data[pad];
		pmc->instance = pad + 1;
		pmc->phy_type = TEGRA_USB_PHY_INTF_HSIC;
		pmc->port_speed = USB_PMC_PORT_SPEED_HIGH;
		pmc->controller_type = TEGRA_USB_3_0;
		tegra_usb_pmc_init(pmc);
	}
}

static void pmc_setup_wake_detect(struct tegra_xhci_hcd *tegra)
{
	struct tegra_usb_pmc_data *pmc;
	struct device *dev = &tegra->pdev->dev;
	u32 portsc;
	int port;
	int pad, utmi_pads;

	for_each_enabled_hsic_pad(pad, tegra) {
		dev_dbg(dev, "%s hsic pad %d\n", __func__, pad);

		pmc = &pmc_hsic_data[pad];
		port = hsic_pad_to_port(pad);
		portsc = xhci_read_portsc(tegra->xhci, port);
		dev_dbg(dev, "%s hsic pad %d portsc 0x%x\n",
			__func__, pad, portsc);

		if (((int) portsc != -1) && (portsc & PORT_CONNECT))
			pmc->pmc_ops->setup_pmc_wake_detect(pmc);
	}

	utmi_pads = tegra->soc_config->utmi_pad_count;

	for (pad = 0; pad < utmi_pads; pad++) {
		if (BIT(XUSB_UTMI_INDEX + pad) & tegra->bdata->portmap) {
			dev_dbg(dev, "%s utmi pad %d\n", __func__, pad);
			pmc = &pmc_data[pad];
			pmc->port_speed = get_usb2_port_speed(tegra, pad);
			if (pad == 0) {
				if (is_otg_host(tegra))
					pmc->pmc_ops->setup_pmc_wake_detect(
									pmc);
			} else
				pmc->pmc_ops->setup_pmc_wake_detect(pmc);
		}
	}
	if (tegra->otg_port_owned) {
		pad = tegra->hs_otg_portnum;
		pmc = &pmc_data[pad];
		pmc->port_speed = get_usb2_port_speed(tegra, pad);
		pmc->pmc_ops->setup_pmc_wake_detect(pmc);
	}
}

static void pmc_disable_bus_ctrl(struct tegra_xhci_hcd *tegra)
{
	struct tegra_usb_pmc_data *pmc;
	struct device *dev = &tegra->pdev->dev;
	int pad, utmi_pads;

	for_each_enabled_hsic_pad(pad, tegra) {
		dev_dbg(dev, "%s hsic pad %d\n", __func__, pad);

		pmc = &pmc_hsic_data[pad];
		pmc->pmc_ops->disable_pmc_bus_ctrl(pmc, 0);
	}

	utmi_pads = tegra->soc_config->utmi_pad_count;

	for (pad = 0; pad < utmi_pads; pad++) {
		if (BIT(XUSB_UTMI_INDEX + pad) & tegra->bdata->portmap) {
			dev_dbg(dev, "%s utmi pad %d\n", __func__, pad);
			pmc = &pmc_data[pad];
			pmc->pmc_ops->disable_pmc_bus_ctrl(pmc, 0);
		}
	}
	if (tegra->otg_port_owned || tegra->otg_port_ownership_changed) {
		pad = tegra->hs_otg_portnum;
		pmc = &pmc_data[pad];
		pmc->pmc_ops->disable_pmc_bus_ctrl(pmc, 0);
	}
}

static u32 csb_read(struct tegra_xhci_hcd *tegra, u32 addr)
{
	void __iomem *fpci_base = tegra->fpci_base;
	struct platform_device *pdev = tegra->pdev;
	u32 input_addr;
	u32 data;
	u32 csb_page_select;

	/* to select the appropriate CSB page to write to */
	csb_page_select = CSB_PAGE_SELECT(addr);

	dev_vdbg(&pdev->dev, "csb_read: csb_page_select= 0x%08x\n",
			csb_page_select);

	iowrite32(csb_page_select, fpci_base + XUSB_CFG_ARU_C11_CSBRANGE);

	/* selects the appropriate offset in the page to read from */
	input_addr = CSB_PAGE_OFFSET(addr);
	data = ioread32(fpci_base + XUSB_CFG_CSB_BASE_ADDR + input_addr);

	dev_vdbg(&pdev->dev, "csb_read: input_addr = 0x%08x data = 0x%08x\n",
			input_addr, data);
	return data;
}

static void csb_write(struct tegra_xhci_hcd *tegra, u32 addr, u32 data)
{
	void __iomem *fpci_base = tegra->fpci_base;
	struct platform_device *pdev = tegra->pdev;
	u32 input_addr;
	u32 csb_page_select;

	/* to select the appropriate CSB page to write to */
	csb_page_select = CSB_PAGE_SELECT(addr);

	dev_vdbg(&pdev->dev, "csb_write:csb_page_selectx = 0x%08x\n",
			csb_page_select);

	iowrite32(csb_page_select, fpci_base + XUSB_CFG_ARU_C11_CSBRANGE);

	/* selects the appropriate offset in the page to write to */
	input_addr = CSB_PAGE_OFFSET(addr);
	iowrite32(data, fpci_base + XUSB_CFG_CSB_BASE_ADDR + input_addr);

	dev_vdbg(&pdev->dev, "csb_write: input_addr = 0x%08x data = %0x08x\n",
			input_addr, data);
}

static int fw_message_send(struct tegra_xhci_hcd *tegra,
	enum MBOX_CMD_TYPE type, u32 data)
{
	struct device *dev = &tegra->pdev->dev;
	void __iomem *base = tegra->fpci_base;
	unsigned long target;
	u32 reg;

	dev_dbg(dev, "%s type %d data 0x%x\n", __func__, type, data);

	mutex_lock(&tegra->mbox_lock);

	target = jiffies + msecs_to_jiffies(20);
	/* wait mailbox to become idle, timeout in 20ms */
	while (((reg = readl(base + XUSB_CFG_ARU_MBOX_OWNER)) != 0) &&
		time_is_after_jiffies(target)) {
		mutex_unlock(&tegra->mbox_lock);
		usleep_range(100, 200);
		mutex_lock(&tegra->mbox_lock);
	}

	if (reg != 0) {
		dev_err(dev, "%s mailbox is still busy\n", __func__);
		goto timeout;
	}

	target = jiffies + msecs_to_jiffies(10);
	/* acquire mailbox , timeout in 10ms */
	writel(MBOX_OWNER_SW, base + XUSB_CFG_ARU_MBOX_OWNER);
	while (((reg = readl(base + XUSB_CFG_ARU_MBOX_OWNER)) != MBOX_OWNER_SW)
		&& time_is_after_jiffies(target)) {
		mutex_unlock(&tegra->mbox_lock);
		usleep_range(100, 200);
		mutex_lock(&tegra->mbox_lock);
		writel(MBOX_OWNER_SW, base + XUSB_CFG_ARU_MBOX_OWNER);
	}

	if (reg != MBOX_OWNER_SW) {
		dev_err(dev, "%s acquire mailbox timeout\n", __func__);
		goto timeout;
	}

	reg = CMD_TYPE(type) | CMD_DATA(data);
	writel(reg, base + XUSB_CFG_ARU_MBOX_DATA_IN);

	reg = readl(tegra->fpci_base + XUSB_CFG_ARU_MBOX_CMD);
	reg |= MBOX_INT_EN | MBOX_FALC_INT_EN;
	writel(reg, tegra->fpci_base + XUSB_CFG_ARU_MBOX_CMD);

	mutex_unlock(&tegra->mbox_lock);
	return 0;

timeout:
	reg_dump(dev, base, XUSB_CFG_ARU_MBOX_CMD);
	reg_dump(dev, base, XUSB_CFG_ARU_MBOX_DATA_IN);
	reg_dump(dev, base, XUSB_CFG_ARU_MBOX_DATA_OUT);
	reg_dump(dev, base, XUSB_CFG_ARU_MBOX_OWNER);
	mutex_unlock(&tegra->mbox_lock);
	return -ETIMEDOUT;
}

/**
 * ack_fw_message_send_sync - send FW message and block until receiving FW ACK
 *	This function will block until FW ack is received.
 * @return	0 if FW returns MBOX_CMD_ACK (success)
 *		-EINVAL if FW returns MBOX_CMD_NACK (failure)
 *		-EPIPE if FW returns a mbox type other than ACK or NACK
 *		-ETIMEOUT if either sending or waiting for FW ack times out
 *		-ERESTARTSYS if the wait has been interrupted by a signal
 */
static int ack_fw_message_send_sync(struct tegra_xhci_hcd *tegra,
	enum MBOX_CMD_TYPE type, u32 data)
{
	int ret = 0;

	mutex_lock(&tegra->mbox_lock_ack);
	tegra->fw_ack = 0;

	/* send mbox message */
	ret = fw_message_send(tegra, type, data);
	if (ret)
		goto out;

	/* wait for FW ACK with 20ms timeout */
	ret = wait_event_interruptible_timeout(tegra->fw_ack_wq,
			tegra->fw_ack, msecs_to_jiffies(20));
	if (ret == 0) {
		dev_warn(&tegra->pdev->dev, "%s: timeout waiting for FW msg\n",
				__func__);
		ret = -ETIMEDOUT;
		goto out;
	} else if (ret == -ERESTARTSYS) {
		dev_warn(&tegra->pdev->dev, "%s: interrupted when waiting\n",
				__func__);
		goto out;
	}

	/* we have got FW ACK here, check what FW returns */
	dev_dbg(&tegra->pdev->dev, "%s: FW ack type:%u\n",
			__func__, tegra->fw_ack);
	if (tegra->fw_ack == MBOX_CMD_ACK)
		ret = 0;
	else if (tegra->fw_ack == MBOX_CMD_NACK)
		ret = -EINVAL;
	else
		ret = -EPIPE; /* violation in mailbox protocol */
out:
	mutex_unlock(&tegra->mbox_lock_ack);
	return ret;
}

/**
 * fw_log_next - find next log entry in a tegra_xhci_firmware_log context.
 *	This function takes care of wrapping. That means when current log entry
 *	is the last one, it returns with the first one.
 *
 * @param log	The tegra_xhci_firmware_log context.
 * @param this	The current log entry.
 * @return	The log entry which is next to the current one.
 */
static inline struct log_entry *fw_log_next(
		struct tegra_xhci_firmware_log *log, struct log_entry *this)
{
	struct log_entry *first = (struct log_entry *) log->virt_addr;
	struct log_entry *last = first + FW_LOG_COUNT - 1;

	WARN((this < first) || (this > last), "%s: invalid input\n", __func__);

	return (this == last) ? first : (this + 1);
}

/**
 * fw_log_update_dequeue_pointer - update dequeue pointer to both firmware and
 *	tegra_xhci_firmware_log.dequeue.
 *
 * @param log	The tegra_xhci_firmware_log context.
 * @param n	Counts of log entries to fast-forward.
 */
static inline void fw_log_update_deq_pointer(
		struct tegra_xhci_firmware_log *log, int n)
{
	struct tegra_xhci_hcd *tegra =
			container_of(log, struct tegra_xhci_hcd, log);
	struct device *dev = &tegra->pdev->dev;
	struct log_entry *deq = tegra->log.dequeue;
	dma_addr_t physical_addr;
	u32 reg;

	dev_vdbg(dev, "curr 0x%p fast-forward %d entries\n", deq, n);
	while (n-- > 0)
		deq = fw_log_next(log, deq);

	tegra->log.dequeue = deq;
	physical_addr = tegra->log.phys_addr +
			((u8 *)deq - (u8 *)tegra->log.virt_addr);

	/* update dequeue pointer to firmware */
	reg = (FW_IOCTL_LOG_DEQUEUE_LOW << FW_IOCTL_TYPE_SHIFT);
	reg |= (physical_addr & 0xffff); /* lower 16-bits */
	iowrite32(reg, tegra->fpci_base + XUSB_CFG_ARU_FW_SCRATCH);

	reg = (FW_IOCTL_LOG_DEQUEUE_HIGH << FW_IOCTL_TYPE_SHIFT);
	reg |= ((physical_addr >> 16) & 0xffff); /* higher 16-bits */
	iowrite32(reg, tegra->fpci_base + XUSB_CFG_ARU_FW_SCRATCH);

	dev_vdbg(dev, "new 0x%p physical addr 0x%x\n", deq, (u32)physical_addr);
}

static inline bool circ_buffer_full(struct circ_buf *circ)
{
	int space = CIRC_SPACE(circ->head, circ->tail, CIRC_BUF_SIZE);

	return (space <= FW_LOG_SIZE);
}

static inline bool fw_log_available(struct tegra_xhci_hcd *tegra)
{
	return (tegra->log.dequeue->owner == DRIVER);
}

/**
 * fw_log_wait_empty_timeout - wait firmware log thread to clean up shared
 *	log buffer.
 * @param tegra:	tegra_xhci_hcd context
 * @param msec:		timeout value in millisecond
 * @return true:	shared log buffer is empty,
 *	   false:	shared log buffer isn't empty.
 */
static inline bool fw_log_wait_empty_timeout(struct tegra_xhci_hcd *tegra,
		unsigned timeout)
{
	unsigned long target = jiffies + msecs_to_jiffies(timeout);
	bool ret;

	mutex_lock(&tegra->log.mutex);

	while (fw_log_available(tegra) && time_is_after_jiffies(target)) {
		mutex_unlock(&tegra->log.mutex);
		usleep_range(1000, 2000);
		mutex_lock(&tegra->log.mutex);
	}

	ret = fw_log_available(tegra);
	mutex_unlock(&tegra->log.mutex);

	return ret;
}

/**
 * fw_log_copy - copy firmware log from device's buffer to driver's circular
 *	buffer.
 * @param tegra	tegra_xhci_hcd context
 * @return true,	We still have firmware log in device's buffer to copy.
 *			This function returned due the driver's circular buffer
 *			is full. Caller should invoke this function again as
 *			soon as there is space in driver's circular buffer.
 *	   false,	Device's buffer is empty.
 */
static inline bool fw_log_copy(struct tegra_xhci_hcd *tegra)
{
	struct device *dev = &tegra->pdev->dev;
	struct circ_buf *circ = &tegra->log.circ;
	int head, tail;
	int buffer_len, copy_len;
	struct log_entry *entry;
	struct log_entry *first = tegra->log.virt_addr;

	while (fw_log_available(tegra)) {

		/* calculate maximum contiguous driver buffer length */
		head = circ->head;
		tail = ACCESS_ONCE(circ->tail);
		buffer_len = CIRC_SPACE_TO_END(head, tail, CIRC_BUF_SIZE);
		/* round down to FW_LOG_SIZE */
		buffer_len -= (buffer_len % FW_LOG_SIZE);
		if (!buffer_len)
			return true; /* log available but no space left */

		/* calculate maximum contiguous log copy length */
		entry = tegra->log.dequeue;
		copy_len = 0;
		do {
			if (tegra->log.seq != entry->sequence_no) {
				dev_warn(dev,
				"%s: discontinuous seq no, expect %u get %u\n",
				__func__, tegra->log.seq, entry->sequence_no);
			}
			tegra->log.seq = entry->sequence_no + 1;

			copy_len += FW_LOG_SIZE;
			buffer_len -= FW_LOG_SIZE;
			if (!buffer_len)
				break; /* no space left */
			entry = fw_log_next(&tegra->log, entry);
		} while ((entry->owner == DRIVER) && (entry != first));

		memcpy(&circ->buf[head], tegra->log.dequeue, copy_len);
		memset(tegra->log.dequeue, 0, copy_len);
		circ->head = (circ->head + copy_len) & (CIRC_BUF_SIZE - 1);

		mb(); /* make sure controller sees it */

		fw_log_update_deq_pointer(&tegra->log, copy_len/FW_LOG_SIZE);

		dev_vdbg(dev, "copied %d entries, new dequeue 0x%p\n",
				copy_len/FW_LOG_SIZE, tegra->log.dequeue);
		wake_up_interruptible(&tegra->log.read_wait);
	}

	return false;
}

static int fw_log_thread(void *data)
{
	struct tegra_xhci_hcd *tegra = data;
	struct device *dev = &tegra->pdev->dev;
	struct circ_buf *circ = &tegra->log.circ;
	bool logs_left;

	dev_dbg(dev, "start firmware log thread\n");

	do {
		mutex_lock(&tegra->log.mutex);
		if (circ_buffer_full(circ)) {
			mutex_unlock(&tegra->log.mutex);
			dev_info(dev, "%s: circ buffer full\n", __func__);
			wait_event_interruptible(tegra->log.write_wait,
			    kthread_should_stop() || !circ_buffer_full(circ));
			mutex_lock(&tegra->log.mutex);
		}

		logs_left = fw_log_copy(tegra);
		mutex_unlock(&tegra->log.mutex);

		/* relax if no logs left  */
		if (!logs_left)
			wait_event_interruptible_timeout(tegra->log.intr_wait,
				fw_log_available(tegra), FW_LOG_THREAD_RELAX);
	} while (!kthread_should_stop());

	dev_dbg(dev, "stop firmware log thread\n");
	return 0;
}

static inline bool circ_buffer_empty(struct circ_buf *circ)
{
	return (CIRC_CNT(circ->head, circ->tail, CIRC_BUF_SIZE) == 0);
}

static ssize_t fw_log_file_read(struct file *file, char __user *buf,
		size_t count, loff_t *offp)
{
	struct tegra_xhci_hcd *tegra = file->private_data;
	struct platform_device *pdev = tegra->pdev;
	struct circ_buf *circ = &tegra->log.circ;
	int head, tail;
	size_t n = 0;
	int s;

	mutex_lock(&tegra->log.mutex);

	while (circ_buffer_empty(circ)) {
		mutex_unlock(&tegra->log.mutex);
		if (file->f_flags & O_NONBLOCK)
			return -EAGAIN; /* non-blocking read */

		dev_dbg(&pdev->dev, "%s: nothing to read\n", __func__);

		if (wait_event_interruptible(tegra->log.read_wait,
				!circ_buffer_empty(circ)))
			return -ERESTARTSYS;

		if (mutex_lock_interruptible(&tegra->log.mutex))
			return -ERESTARTSYS;
	}

	while (count > 0) {
		head = ACCESS_ONCE(circ->head);
		tail = circ->tail;
		s = min_t(int, count,
				CIRC_CNT_TO_END(head, tail, CIRC_BUF_SIZE));

		if (s > 0) {
			if (copy_to_user(&buf[n], &circ->buf[tail], s)) {
				dev_warn(&pdev->dev, "copy_to_user failed\n");
				mutex_unlock(&tegra->log.mutex);
				return -EFAULT;
			}
			circ->tail = (circ->tail + s) & (CIRC_BUF_SIZE - 1);

			count -= s;
			n += s;
		} else
			break;
	}

	mutex_unlock(&tegra->log.mutex);

	wake_up_interruptible(&tegra->log.write_wait);

	dev_dbg(&pdev->dev, "%s: %zu bytes\n", __func__, n);

	return n;
}

static int fw_log_file_open(struct inode *inode, struct file *file)
{
	struct tegra_xhci_hcd *tegra;

	file->private_data = inode->i_private;
	tegra = file->private_data;

	if (test_and_set_bit(FW_LOG_FILE_OPENED, &tegra->log.flags)) {
		dev_info(&tegra->pdev->dev, "%s: already opened\n", __func__);
		return -EBUSY;
	}

	return 0;
}

static int fw_log_file_close(struct inode *inode, struct file *file)
{
	struct tegra_xhci_hcd *tegra = file->private_data;

	clear_bit(FW_LOG_FILE_OPENED, &tegra->log.flags);

	return 0;
}

static const struct file_operations firmware_log_fops = {
		.open		= fw_log_file_open,
		.release	= fw_log_file_close,
		.read		= fw_log_file_read,
		.owner		= THIS_MODULE,
};

static int fw_log_init(struct tegra_xhci_hcd *tegra)
{
	struct platform_device *pdev = tegra->pdev;
	int rc = 0;

	/* allocate buffer to be shared between driver and firmware */
	tegra->log.virt_addr = dma_alloc_writecombine(&pdev->dev,
			FW_LOG_RING_SIZE, &tegra->log.phys_addr, GFP_KERNEL);

	if (!tegra->log.virt_addr) {
		dev_err(&pdev->dev, "dma_alloc_writecombine() size %d failed\n",
				FW_LOG_RING_SIZE);
		return -ENOMEM;
	}

	dev_info(&pdev->dev,
		"%d bytes log buffer physical 0x%u virtual 0x%p\n",
		FW_LOG_RING_SIZE, (u32)tegra->log.phys_addr,
		tegra->log.virt_addr);

	memset(tegra->log.virt_addr, 0, FW_LOG_RING_SIZE);
	tegra->log.dequeue = tegra->log.virt_addr;

	tegra->log.circ.buf = vmalloc(CIRC_BUF_SIZE);
	if (!tegra->log.circ.buf) {
		rc = -ENOMEM;
		goto error_free_dma;
	}

	tegra->log.circ.head = 0;
	tegra->log.circ.tail = 0;

	init_waitqueue_head(&tegra->log.read_wait);
	init_waitqueue_head(&tegra->log.write_wait);
	init_waitqueue_head(&tegra->log.intr_wait);

	mutex_init(&tegra->log.mutex);

	tegra->log.path = debugfs_create_dir("tegra_xhci", NULL);
	if (IS_ERR_OR_NULL(tegra->log.path)) {
		dev_warn(&pdev->dev, "debugfs_create_dir() failed\n");
		rc = -ENOMEM;
		goto error_free_mem;
	}

	tegra->log.log_file = debugfs_create_file("firmware_log",
			S_IRUGO, tegra->log.path, tegra, &firmware_log_fops);
	if ((!tegra->log.log_file) ||
			(tegra->log.log_file == ERR_PTR(-ENODEV))) {
		dev_warn(&pdev->dev, "debugfs_create_file() failed\n");
		rc = -ENOMEM;
		goto error_remove_debugfs_path;
	}

	tegra->log.thread = kthread_run(fw_log_thread, tegra, "xusb-fw-log");
	if (IS_ERR(tegra->log.thread)) {
		dev_warn(&pdev->dev, "kthread_run() failed\n");
		rc = -ENOMEM;
		goto error_remove_debugfs_file;
	}

	set_bit(FW_LOG_CONTEXT_VALID, &tegra->log.flags);
	return rc;

error_remove_debugfs_file:
	debugfs_remove(tegra->log.log_file);
error_remove_debugfs_path:
	debugfs_remove(tegra->log.path);
error_free_mem:
	vfree(tegra->log.circ.buf);
error_free_dma:
	dma_free_writecombine(&pdev->dev, FW_LOG_RING_SIZE,
			tegra->log.virt_addr, tegra->log.phys_addr);
	memset(&tegra->log, 0, sizeof(tegra->log));
	return rc;
}

static void fw_log_deinit(struct tegra_xhci_hcd *tegra)
{
	struct platform_device *pdev = tegra->pdev;

	if (test_and_clear_bit(FW_LOG_CONTEXT_VALID, &tegra->log.flags)) {

		debugfs_remove(tegra->log.log_file);
		debugfs_remove(tegra->log.path);

		wake_up_interruptible(&tegra->log.read_wait);
		wake_up_interruptible(&tegra->log.write_wait);
		kthread_stop(tegra->log.thread);

		mutex_lock(&tegra->log.mutex);
		dma_free_writecombine(&pdev->dev, FW_LOG_RING_SIZE,
			tegra->log.virt_addr, tegra->log.phys_addr);
		vfree(tegra->log.circ.buf);
		tegra->log.circ.head = tegra->log.circ.tail = 0;
		mutex_unlock(&tegra->log.mutex);

		mutex_destroy(&tegra->log.mutex);
	}
}

/* hsic pad operations */
/*
 * HSIC pads need VDDIO_HSIC power rail turned on to be functional. There is
 * only one VDDIO_HSIC power rail shared by all HSIC pads.
 */
static int hsic_power_rail_enable(struct tegra_xhci_hcd *tegra)
{
	struct device *dev = &tegra->pdev->dev;
	const struct tegra_xusb_regulator_name *supply =
				&tegra->soc_config->supply;
	int ret;

	if (tegra->vddio_hsic_reg)
		goto done;

	tegra->vddio_hsic_reg = devm_regulator_get(dev, supply->vddio_hsic);
	if (IS_ERR(tegra->vddio_hsic_reg)) {
		dev_err(dev, "%s get vddio_hsic failed\n", __func__);
		ret = PTR_ERR(tegra->vddio_hsic_reg);
		goto get_failed;
	}

	dev_dbg(dev, "%s regulator_enable vddio_hsic\n", __func__);
	ret = regulator_enable(tegra->vddio_hsic_reg);
	if (ret < 0) {
		dev_err(dev, "%s enable vddio_hsic failed\n", __func__);
		goto enable_failed;
	}

done:
	tegra->vddio_hsic_refcnt++;
	WARN(tegra->vddio_hsic_refcnt > XUSB_HSIC_COUNT,
			"vddio_hsic_refcnt exceeds\n");
	return 0;

enable_failed:
	devm_regulator_put(tegra->vddio_hsic_reg);
get_failed:
	tegra->vddio_hsic_reg = NULL;
	return ret;
}

static int hsic_power_rail_disable(struct tegra_xhci_hcd *tegra)
{
	struct device *dev = &tegra->pdev->dev;
	int ret;

	WARN_ON(!tegra->vddio_hsic_reg || !tegra->vddio_hsic_refcnt);

	tegra->vddio_hsic_refcnt--;
	if (tegra->vddio_hsic_refcnt)
		return 0;

	dev_dbg(dev, "%s regulator_disable vddio_hsic\n", __func__);
	ret = regulator_disable(tegra->vddio_hsic_reg);
	if (ret < 0) {
		dev_err(dev, "%s disable vddio_hsic failed\n", __func__);
		tegra->vddio_hsic_refcnt++;
		return ret;
	}

	devm_regulator_put(tegra->vddio_hsic_reg);
	tegra->vddio_hsic_reg = NULL;

	return 0;
}

static int hsic_pad_enable(struct tegra_xhci_hcd *tegra, unsigned pad)
{
#if defined(CONFIG_ARCH_TEGRA_21x_SOC)
	if (XUSB_IS_T210(tegra))
		return t210_hsic_pad_enable(tegra, pad);
#else
	struct device *dev = &tegra->pdev->dev;
	void __iomem *base = tegra->padctl_base;
	const struct tegra_xusb_padctl_regs *padregs = tegra->padregs;
	u32 reg;

	if (pad >= XUSB_HSIC_COUNT) {
		dev_err(dev, "%s invalid HSIC pad number %d\n", __func__, pad);
		return -EINVAL;
	}

	dev_dbg(dev, "%s pad %u\n", __func__, pad);

	reg = padctl_readl(tegra, GET_HSIC_REG_OFFSET());
	reg &= ~(RPD_DATA | RPD_STROBE | RPU_DATA | RPU_STROBE);
	reg |= (RPD_DATA | RPU_STROBE); /* keep HSIC in IDLE */
	reg &= ~(PD_RX | HSIC_PD_ZI | PD_TRX | PD_TX);
	padctl_writel(tegra, reg, GET_HSIC_REG_OFFSET());

	/* FIXME: May have better way to handle tracking circuit on HSIC */
	if (XUSB_DEVICE_ID_T210 == tegra->device_id) {
		hsic_trk_enable();
	} else {
		/* Wait for 25 us */
		usleep_range(25, 50);

		/* Power down tracking circuit */
		reg = padctl_readl(tegra
				, padregs->usb2_hsic_padX_ctlY_0[pad][1]);
		reg |= PD_TRX;
		padctl_writel(tegra, reg
				, padregs->usb2_hsic_padX_ctlY_0[pad][1]);
	}

	reg = padctl_readl(tegra, padregs->usb2_pad_mux_0);
	reg |= USB2_HSIC_PAD_PORT(pad);
	padctl_writel(tegra, reg, padregs->usb2_pad_mux_0);

	reg_dump(dev, base, padregs->usb2_hsic_padX_ctlY_0[pad][0]);
	reg_dump(dev, base, padregs->usb2_hsic_padX_ctlY_0[pad][1]);
	reg_dump(dev, base, padregs->usb2_hsic_padX_ctlY_0[pad][2]);
	reg_dump(dev, base, padregs->hsic_strb_trim_ctl0);
	reg_dump(dev, base, padregs->usb2_pad_mux_0);
#endif
	return 0;
}

static void hsic_pad_pretend_connect(struct tegra_xhci_hcd *tegra)
{
	struct device *dev = &tegra->pdev->dev;
	struct tegra_xusb_hsic_config *hsic;
	struct usb_device *hs_root_hub = tegra->xhci->main_hcd->self.root_hub;
	int pad;
	u32 portsc;
	int port;
	int enabled_pads = 0;
	unsigned long wait_ports = 0;
	unsigned long target;

	for_each_enabled_hsic_pad(pad, tegra) {
		hsic = &tegra->bdata->hsic[pad];
		if (hsic->pretend_connect)
			enabled_pads++;
	}

	if (enabled_pads == 0) {
		dev_dbg(dev, "%s no hsic pretend_connect enabled\n", __func__);
		return;
	}

	usb_disable_autosuspend(hs_root_hub);

	for_each_enabled_hsic_pad(pad, tegra) {
		hsic = &tegra->bdata->hsic[pad];
		if (!hsic->pretend_connect)
			continue;

		port = hsic_pad_to_port(pad);
		portsc = xhci_read_portsc(tegra->xhci, port);
		dev_dbg(dev, "%s pad %u portsc 0x%x\n", __func__, pad, portsc);

		if (!(portsc & PORT_CONNECT)) {
			/* firmware wants 1-based port index */
			fw_message_send(tegra,
				MBOX_CMD_HSIC_PRETEND_CONNECT, BIT(port + 1));
		}

		set_bit(port, &wait_ports);
	}

	/* wait till port reaches U0 */
	target = jiffies + msecs_to_jiffies(500);
	do {
		for_each_set_bit(port, &wait_ports, BITS_PER_LONG) {
			portsc = xhci_read_portsc(tegra->xhci, port);
			pad = port_to_hsic_pad(port);
			dev_dbg(dev, "%s pad %u portsc 0x%x\n", __func__,
				pad, portsc);
			if ((PORT_PLS_MASK & portsc) == XDEV_U0)
				clear_bit(port, &wait_ports);
		}

		if (wait_ports)
			usleep_range(1000, 5000);
	} while (wait_ports && time_is_after_jiffies(target));

	if (wait_ports)
		dev_warn(dev, "%s HSIC pad(s) didn't reach U0.\n", __func__);

	usb_enable_autosuspend(hs_root_hub);
}

static int hsic_pad_disable(struct tegra_xhci_hcd *tegra, unsigned pad)
{
#if defined(CONFIG_ARCH_TEGRA_21x_SOC)
	if (XUSB_IS_T210(tegra))
		return t210_hsic_pad_disable(tegra, pad);
#else
	struct device *dev = &tegra->pdev->dev;
	void __iomem *base = tegra->padctl_base;
	const struct tegra_xusb_padctl_regs *padregs = tegra->padregs;
	u32 reg;

	if (pad >= XUSB_HSIC_COUNT) {
		dev_err(dev, "%s invalid HSIC pad number %d\n", __func__, pad);
		return -EINVAL;
	}

	dev_dbg(dev, "%s pad %u\n", __func__, pad);

	reg = padctl_readl(tegra, padregs->usb2_pad_mux_0);
	reg &= ~USB2_HSIC_PAD_PORT(pad);
	padctl_writel(tegra, reg, padregs->usb2_pad_mux_0);

	reg = padctl_readl(tegra, padregs->usb2_hsic_padX_ctlY_0[pad][1]);
	reg |= (PD_RX | HSIC_PD_ZI | PD_TRX | PD_TX);
	padctl_writel(tegra, reg, padregs->usb2_hsic_padX_ctlY_0[pad][1]);

	reg_dump(dev, base, padregs->usb2_hsic_padX_ctlY_0[pad][0]);
	reg_dump(dev, base, padregs->usb2_hsic_padX_ctlY_0[pad][1]);
	reg_dump(dev, base, padregs->usb2_hsic_padX_ctlY_0[pad][2]);
	reg_dump(dev, base, padregs->usb2_pad_mux_0);
#endif
	return 0;
}

static int hsic_pad_pupd_set(struct tegra_xhci_hcd *tegra, unsigned pad,
	enum hsic_pad_pupd pupd)
{
#if defined(CONFIG_ARCH_TEGRA_21x_SOC)
	if (XUSB_IS_T210(tegra))
		return t210_hsic_pad_pupd_set(tegra, pad, pupd);
#else
	struct device *dev = &tegra->pdev->dev;
	const struct tegra_xusb_padctl_regs *padregs = tegra->padregs;
	u32 reg;

	if (pad >= XUSB_HSIC_COUNT) {
		dev_err(dev, "%s invalid HSIC pad number %u\n", __func__, pad);
		return -EINVAL;
	}

	dev_dbg(dev, "%s pad %u pupd %d\n", __func__, pad, pupd);

	reg = padctl_readl(tegra, padregs->usb2_hsic_padX_ctlY_0[pad][1]);
	reg &= ~(RPD_DATA | RPD_STROBE | RPU_DATA | RPU_STROBE);

	if (pupd == PUPD_IDLE)
		reg |= (RPD_DATA | RPU_STROBE);
	else if (pupd == PUPD_RESET)
		reg |= (RPD_DATA | RPD_STROBE);
	else if (pupd != PUPD_DISABLE) {
		dev_err(dev, "%s invalid pupd %d\n", __func__, pupd);
		return -EINVAL;
	}

	padctl_writel(tegra, reg, padregs->usb2_hsic_padX_ctlY_0[pad][1]);

	reg_dump(dev, tegra->padctl_base
		, padregs->usb2_hsic_padX_ctlY_0[pad][1]);
#endif
	return 0;
}

static void tegra_xhci_debug_read_pads(struct tegra_xhci_hcd *tegra)
{
	const struct tegra_xusb_padctl_regs *padregs = tegra->padregs;
	struct xhci_hcd *xhci = tegra->xhci;
	u32 reg;

	xhci_info(xhci, "============ PADCTL VALUES START =================\n");
	reg = padctl_readl(tegra, padregs->usb2_pad_mux_0);
	xhci_info(xhci, " PAD MUX = %x\n", reg);
	reg = padctl_readl(tegra, padregs->usb2_port_cap_0);
	xhci_info(xhci, " PORT CAP = %x\n", reg);
	reg = padctl_readl(tegra, padregs->snps_oc_map_0);
	xhci_info(xhci, " SNPS OC MAP = %x\n", reg);
	reg = padctl_readl(tegra, padregs->usb2_oc_map_0);
	xhci_info(xhci, " USB2 OC MAP = %x\n", reg);
	reg = padctl_readl(tegra, padregs->ss_port_map_0);
	xhci_info(xhci, " SS PORT MAP = %x\n", reg);
	reg = padctl_readl(tegra, padregs->oc_det_0);
	xhci_info(xhci, " OC DET 0= %x\n", reg);
	reg = padctl_readl(tegra, padregs->iophy_usb3_padX_ctlY_0[0][1]);
	xhci_info(xhci, " iophy_usb3_pad0_ctl2_0= %x\n", reg);
	reg = padctl_readl(tegra, padregs->iophy_usb3_padX_ctlY_0[1][1]);
	xhci_info(xhci, " iophy_usb3_pad1_ctl2_0= %x\n", reg);
	reg = padctl_readl(tegra, padregs->usb2_otg_padX_ctlY_0[0][0]);
	xhci_info(xhci, " usb2_otg_pad0_ctl0_0= %x\n", reg);
	reg = padctl_readl(tegra, padregs->usb2_otg_padX_ctlY_0[1][0]);
	xhci_info(xhci, " usb2_otg_pad1_ctl0_0= %x\n", reg);
	reg = padctl_readl(tegra, padregs->usb2_otg_padX_ctlY_0[0][1]);
	xhci_info(xhci, " usb2_otg_pad0_ctl1_0= %x\n", reg);
	reg = padctl_readl(tegra, padregs->usb2_otg_padX_ctlY_0[1][1]);
	xhci_info(xhci, " usb2_otg_pad1_ctl1_0= %x\n", reg);
	reg = padctl_readl(tegra, padregs->usb2_bias_pad_ctlY_0[0]);
	xhci_info(xhci, " usb2_bias_pad_ctl0_0= %x\n", reg);
	reg = padctl_readl(tegra, padregs->usb2_hsic_padX_ctlY_0[0][0]);
	xhci_info(xhci, " usb2_hsic_pad0_ctl0_0= %x\n", reg);
	reg = padctl_readl(tegra, padregs->usb2_hsic_padX_ctlY_0[1][0]);
	xhci_info(xhci, " usb2_hsic_pad1_ctl0_0= %x\n", reg);
	xhci_info(xhci, "============ PADCTL VALUES END=================\n");
}

static void tegra_xhci_cfg(struct tegra_xhci_hcd *tegra)
{
	u32 reg;

	reg = readl(tegra->ipfs_base + IPFS_XUSB_HOST_CONFIGURATION_0);
	reg |= IPFS_EN_FPCI;
	writel(reg, tegra->ipfs_base + IPFS_XUSB_HOST_CONFIGURATION_0);
	udelay(10);

	/* Program Bar0 Space */
	reg = readl(tegra->fpci_base + XUSB_CFG_4);
	reg |= tegra->host_phy_base;
	writel(reg, tegra->fpci_base + XUSB_CFG_4);
	usleep_range(100, 200);

	/* Enable Bus Master */
	reg = readl(tegra->fpci_base + XUSB_CFG_1);
	reg |= 0x7;
	writel(reg, tegra->fpci_base + XUSB_CFG_1);

	/* Set intr mask to enable intr assertion */
	reg = readl(tegra->ipfs_base + IPFS_XUSB_HOST_INTR_MASK_0);
	reg |= IPFS_IP_INT_MASK;
	writel(reg, tegra->ipfs_base + IPFS_XUSB_HOST_INTR_MASK_0);

	/* Set hysteris to 0x80 */
	writel(0x80, tegra->ipfs_base + IPFS_XUSB_HOST_CLKGATE_HYSTERESIS_0);
}

static int tegra_xusb_regulator_init(struct tegra_xhci_hcd *tegra,
		struct platform_device *pdev)
{
	const struct tegra_xusb_regulator_name *supply =
				&tegra->soc_config->supply;
	int err = 0;
	int utmi_pads = 0, pad;

	tegra->xusb_s3p3v_reg =
			devm_regulator_get(&pdev->dev, supply->s3p3v);
	if (IS_ERR(tegra->xusb_s3p3v_reg)) {
		dev_err(&pdev->dev, "3p3v: regulator not found: %ld."
			, PTR_ERR(tegra->xusb_s3p3v_reg));
		err = PTR_ERR(tegra->xusb_s3p3v_reg);
	} else {
		err = regulator_enable(tegra->xusb_s3p3v_reg);
		if (err < 0) {
			dev_err(&pdev->dev,
				"3p3v: regulator enable failed:%d\n", err);
		}
	}
	if (err)
		goto err_null_regulator;

	tegra->xusb_s1p8v_reg =
		devm_regulator_get(&pdev->dev, supply->s1p8v);
	if (IS_ERR(tegra->xusb_s1p8v_reg)) {
		dev_err(&pdev->dev, "1p8v regulator not found: %ld."
			, PTR_ERR(tegra->xusb_s1p8v_reg));
		err = PTR_ERR(tegra->xusb_s1p8v_reg);
	} else {
		err = regulator_enable(tegra->xusb_s1p8v_reg);
		if (err < 0) {
			dev_err(&pdev->dev,
			"1p8v: regulator enable failed:%d\n", err);
		}
	}
	if (err)
		goto err_disable_s3p3v_reg;

	tegra->xusb_s1p05v_reg =
			devm_regulator_get(&pdev->dev, supply->s1p05v);
	if (IS_ERR(tegra->xusb_s1p05v_reg)) {
		dev_err(&pdev->dev, "1p05v: regulator not found: %ld."
			, PTR_ERR(tegra->xusb_s1p05v_reg));
		err = PTR_ERR(tegra->xusb_s1p05v_reg);
	} else {
		err = regulator_enable(tegra->xusb_s1p05v_reg);
		if (err < 0) {
			dev_err(&pdev->dev,
			"1p05v: regulator enable failed:%d\n", err);
		}
	}
	if (err)
		goto err_disable_s1p8v_reg;


	/* enable utmi vbuses */
	utmi_pads = tegra->soc_config->utmi_pad_count;
	tegra->xusb_utmi_vbus_regs = devm_kzalloc(&pdev->dev,
			sizeof(*tegra->xusb_utmi_vbus_regs) *
			utmi_pads, GFP_KERNEL);
	for_each_enabled_utmi_pad(pad, tegra) {
		struct usb_vbus_en_oc *en_oc = &tegra->bdata->vbus_en_oc[pad];
		struct regulator *reg;

		reg = devm_regulator_get(&pdev->dev,
						supply->utmi_vbuses[pad]);
		if (IS_ERR(reg)) {
			dev_err(&pdev->dev,
			"%s regulator not found: %ld.",
			supply->utmi_vbuses[pad], PTR_ERR(reg));
			err = PTR_ERR(reg);
		} else {
			if ((en_oc->type == VBUS_FIXED)
					|| (en_oc->type == VBUS_FIXED_OC)) {
				err = regulator_enable(reg);
				if (err < 0) {
					dev_err(&pdev->dev,
					"%s: regulator enable failed: %d\n",
					supply->utmi_vbuses[pad], err);
				}
			}
		}

		if (err)
			goto err_disable_s1p05v_reg;
		tegra->xusb_utmi_vbus_regs[pad] = reg;
	}

	return 0;

err_disable_s1p05v_reg:
	regulator_disable(tegra->xusb_s1p05v_reg);
err_disable_s1p8v_reg:
	regulator_disable(tegra->xusb_s1p8v_reg);
err_disable_s3p3v_reg:
	regulator_disable(tegra->xusb_s3p3v_reg);
err_null_regulator:
	for (pad = 0; pad < utmi_pads; pad++) {
		if (tegra->xusb_utmi_vbus_regs[pad])
			regulator_disable(tegra->xusb_utmi_vbus_regs[pad]);
		tegra->xusb_utmi_vbus_regs[pad] = NULL;
	}
	tegra->xusb_s1p05v_reg = NULL;
	tegra->xusb_s3p3v_reg = NULL;
	tegra->xusb_s1p8v_reg = NULL;
	return err;
}

static void tegra_xusb_regulator_deinit(struct tegra_xhci_hcd *tegra)
{
	int pad;

	for_each_enabled_utmi_pad(pad, tegra) {
		struct regulator *reg = tegra->xusb_utmi_vbus_regs[pad];
		struct usb_vbus_en_oc *en_oc = &tegra->bdata->vbus_en_oc[pad];

		if ((en_oc->type == VBUS_FIXED)
				|| (en_oc->type == VBUS_FIXED_OC))
			regulator_disable(reg);
		tegra->xusb_utmi_vbus_regs[pad] = NULL;
	}

	regulator_disable(tegra->xusb_s1p05v_reg);
	regulator_disable(tegra->xusb_s1p8v_reg);
	regulator_disable(tegra->xusb_s3p3v_reg);

	tegra->xusb_s1p05v_reg = NULL;
	tegra->xusb_s1p8v_reg = NULL;
	tegra->xusb_s3p3v_reg = NULL;
}

/*
 * We need to enable only plle_clk as pllu_clk, utmip_clk and plle_re_vco_clk
 * are under hardware control
 */
static int tegra_usb2_clocks_init(struct tegra_xhci_hcd *tegra)
{
	struct platform_device *pdev = tegra->pdev;
	int err = 0;

	tegra->plle_clk = devm_clk_get(&pdev->dev, "pll_e");
	if (IS_ERR(tegra->plle_clk)) {
		dev_err(&pdev->dev, "%s: Failed to get plle clock\n", __func__);
		err = PTR_ERR(tegra->plle_clk);
		return err;
	}
	err = clk_enable(tegra->plle_clk);
	if (err) {
		dev_err(&pdev->dev, "%s: could not enable plle clock\n",
			__func__);
		return err;
	}

	return err;
}

static void tegra_usb2_clocks_deinit(struct tegra_xhci_hcd *tegra)
{
	clk_disable(tegra->plle_clk);
	tegra->plle_clk = NULL;
}

static int tegra_xusb_partitions_clk_init(struct tegra_xhci_hcd *tegra)
{
	struct platform_device *pdev = tegra->pdev;
	int err = 0;

	tegra->emc_clk = devm_clk_get(&pdev->dev, "emc");
	if (IS_ERR(tegra->emc_clk)) {
		dev_err(&pdev->dev, "Failed to get xusb.emc clock\n");
		return PTR_ERR(tegra->emc_clk);
	}

	if (tegra->soc_config->quirks & TEGRA_XUSB_USE_HS_SRC_CLOCK2) {
		tegra->pll_re_vco_clk = devm_clk_get(&pdev->dev, "pll_re_vco");
		if (IS_ERR(tegra->pll_re_vco_clk)) {
			dev_err(&pdev->dev, "Failed to get refPLLE clock\n");
			err = PTR_ERR(tegra->pll_re_vco_clk);
			goto get_pll_re_vco_clk_failed;
		}
	}

	/* get the clock handle of 120MHz clock source */
	tegra->pll_u_480M = devm_clk_get(&pdev->dev, "pll_u_480M");
	if (IS_ERR(tegra->pll_u_480M)) {
		dev_err(&pdev->dev, "Failed to get pll_u_480M clk handle\n");
		err = PTR_ERR(tegra->pll_u_480M);
		goto get_pll_u_480M_failed;
	}

	/* get the clock handle of 12MHz clock source */
	tegra->clk_m = devm_clk_get(&pdev->dev, "clk_m");
	if (IS_ERR(tegra->clk_m)) {
		dev_err(&pdev->dev, "Failed to get clk_m clk handle\n");
		err = PTR_ERR(tegra->clk_m);
		goto clk_get_clk_m_failed;
	}

	tegra->ss_src_clk = devm_clk_get(&pdev->dev, "ss_src");
	if (IS_ERR(tegra->ss_src_clk)) {
		dev_err(&pdev->dev, "Failed to get SSPI clk\n");
		err = PTR_ERR(tegra->ss_src_clk);
		tegra->ss_src_clk = NULL;
		goto get_ss_src_clk_failed;
	}

	tegra->host_clk = devm_clk_get(&pdev->dev, "host");
	if (IS_ERR(tegra->host_clk)) {
		dev_err(&pdev->dev, "Failed to get host partition clk\n");
		err = PTR_ERR(tegra->host_clk);
		tegra->host_clk = NULL;
		goto get_host_clk_failed;
	}

	tegra->ss_clk = devm_clk_get(&pdev->dev, "ss");
	if (IS_ERR(tegra->ss_clk)) {
		dev_err(&pdev->dev, "Failed to get ss partition clk\n");
		err = PTR_ERR(tegra->ss_clk);
		tegra->ss_clk = NULL;
		goto get_ss_clk_failed;
	}

	return 0;

get_ss_clk_failed:
	tegra->host_clk = NULL;

get_host_clk_failed:
	tegra->ss_src_clk = NULL;

get_ss_src_clk_failed:
	tegra->clk_m = NULL;

clk_get_clk_m_failed:
	tegra->pll_u_480M = NULL;

get_pll_u_480M_failed:
	if (tegra->soc_config->quirks & TEGRA_XUSB_USE_HS_SRC_CLOCK2)
		tegra->pll_re_vco_clk = NULL;

get_pll_re_vco_clk_failed:
	tegra->emc_clk = NULL;

	return err;
}

static void tegra_xusb_partitions_clk_deinit(struct tegra_xhci_hcd *tegra)
{
	if (tegra->clock_enable_done) {
		clk_disable(tegra->emc_clk);
		if (tegra->soc_config->quirks & TEGRA_XUSB_USE_HS_SRC_CLOCK2)
			clk_disable(tegra->pll_re_vco_clk);
		tegra->clock_enable_done = false;
	}
	tegra->ss_clk = NULL;
	tegra->host_clk = NULL;
	tegra->ss_src_clk = NULL;
	tegra->clk_m = NULL;
	tegra->pll_u_480M = NULL;
	tegra->emc_clk = NULL;
	if (tegra->soc_config->quirks & TEGRA_XUSB_USE_HS_SRC_CLOCK2)
		tegra->pll_re_vco_clk = NULL;
}

static void tegra_xhci_rx_idle_mode_override(struct tegra_xhci_hcd *tegra,
	bool enable)
{
	const struct tegra_xusb_padctl_regs *padregs = tegra->padregs;
	u32 reg;

	/* Issue is only applicable for T114 */
	if (XUSB_DEVICE_ID_T114 != tegra->device_id)
		return;

	if (tegra->bdata->portmap & TEGRA_XUSB_SS_P0) {
		reg = padctl_readl(tegra,
			padregs->iophy_misc_pad_pX_ctlY_0[0][2]);
		if (enable) {
			reg &= ~RX_IDLE_MODE;
			reg |= RX_IDLE_MODE_OVRD;
		} else {
			reg |= RX_IDLE_MODE;
			reg &= ~RX_IDLE_MODE_OVRD;
		}
		padctl_writel(tegra, reg,
			padregs->iophy_misc_pad_pX_ctlY_0[0][2]);
	}

	if (tegra->bdata->portmap & TEGRA_XUSB_SS_P1) {
		reg = padctl_readl(tegra,
			padregs->iophy_misc_pad_pX_ctlY_0[1][2]);
		if (enable) {
			reg &= ~RX_IDLE_MODE;
			reg |= RX_IDLE_MODE_OVRD;
		} else {
			reg |= RX_IDLE_MODE;
			reg &= ~RX_IDLE_MODE_OVRD;
		}
		padctl_writel(tegra, reg,
			padregs->iophy_misc_pad_pX_ctlY_0[1][2]);

		/* SATA lane also if USB3_SS port1 mapped to it */
		if (XUSB_DEVICE_ID_T114 != tegra->device_id &&
				tegra->bdata->lane_owner & BIT(0)) {
			reg = padctl_readl(tegra,
				padregs->iophy_misc_pad_s0_ctlY_0[2]);
			if (enable) {
				reg &= ~RX_IDLE_MODE;
				reg |= RX_IDLE_MODE_OVRD;
			} else {
				reg |= RX_IDLE_MODE;
				reg &= ~RX_IDLE_MODE_OVRD;
			}
			padctl_writel(tegra, reg,
				padregs->iophy_misc_pad_s0_ctlY_0[2]);
		}
	}
}

/* Enable ss clk, host clk, falcon clk,
 * fs clk, dev clk, plle and refplle
 */

static int
tegra_xusb_request_clk_rate(struct tegra_xhci_hcd *tegra,
		struct clk *clk_handle, u32 rate, u32 *sw_resp)
{
	int ret = 0;
	enum MBOX_CMD_TYPE cmd_ack = MBOX_CMD_ACK;
	int fw_req_rate = rate, cur_rate;

	/* Do not handle clock change as needed for HS disconnect issue */
	if (tegra->soc_config->quirks & TEGRA_XUSB_USE_HS_SRC_CLOCK2) {
		*sw_resp = CMD_DATA(fw_req_rate) | CMD_TYPE(MBOX_CMD_ACK);
		return ret;
	}

	/* frequency request from firmware is in KHz.
	 * Convert it to MHz
	 */

	/* get current rate of clock */
	cur_rate = clk_get_rate(clk_handle);
	cur_rate /= 1000;

	if (fw_req_rate == cur_rate) {
		cmd_ack = MBOX_CMD_ACK;

	} else {

		if (clk_handle == tegra->ss_src_clk && fw_req_rate == 12000) {
			/* Change SS clock source to CLK_M at 12MHz */
			clk_set_parent(clk_handle, tegra->clk_m);
			clk_set_rate(clk_handle, fw_req_rate * 1000);

			/* save leakage power when SS freq is being decreased */
			tegra_xhci_rx_idle_mode_override(tegra, true);
		} else if (clk_handle == tegra->ss_src_clk &&
				fw_req_rate == 120000) {
			/* Change SS clock source to HSIC_480 at 120MHz */
			clk_set_rate(clk_handle,  3000 * 1000);
			clk_set_parent(clk_handle, tegra->pll_u_480M);

			/* clear ovrd bits when SS freq is being increased */
			tegra_xhci_rx_idle_mode_override(tegra, false);
		}

		cur_rate = (clk_get_rate(clk_handle) / 1000);

		if (cur_rate != fw_req_rate) {
			xhci_err(tegra->xhci, "cur_rate=%d, fw_req_rate=%d\n",
				cur_rate, fw_req_rate);
			cmd_ack = MBOX_CMD_NACK;
		}
	}
	*sw_resp = CMD_DATA(cur_rate) | CMD_TYPE(cmd_ack);
	return ret;
}

static void tegra_xusb_set_bw(struct tegra_xhci_hcd *tegra, unsigned int bw)
{
/*	FIXME: below api does not exist. change when available
	unsigned int freq_khz;

	freq_khz = tegra_emc_bw_to_freq_req(bw);
	clk_set_rate(tegra->emc_clk, freq_khz * 1000);*/
}

static void tegra_xhci_save_dfe_context(struct tegra_xhci_hcd *tegra,
	u8 port)
{
	struct xhci_hcd *xhci = tegra->xhci;
	const struct tegra_xusb_padctl_regs *padregs = tegra->padregs;
	u32 offset;
	u32 reg;
	int ss_pads = tegra->soc_config->ss_pad_count;

	if (port > (ss_pads - 1)) {
		pr_err("%s invalid SS port number %u\n", __func__, port);
		return;
	}

	xhci_info(xhci, "saving restore DFE context for port %d\n", port);

	/* if port1 is mapped to SATA lane then read from SATA register */
	if (port == 1 && XUSB_DEVICE_ID_T114 != tegra->device_id &&
			tegra->bdata->lane_owner & BIT(0))
		offset = padregs->iophy_misc_pad_s0_ctlY_0[5];
	else
		offset = padregs->iophy_misc_pad_pX_ctlY_0[port][5];

	/*
	 * Value set to IOPHY_MISC_PAD_x_CTL_6 where x P0/P1/S0/ is from,
	 * T114 refer PG USB3_FW_Programming_Guide_Host.doc section 14.3.10
	 * T124 refer PG T124_USB3_FW_Programming_Guide_Host.doc section 14.3.10
	 */
	reg = padctl_readl(tegra, offset);
	reg &= ~MISC_OUT_SEL(~0);
	reg |= MISC_OUT_SEL(0x32);
	padctl_writel(tegra, reg, offset);

	reg = padctl_readl(tegra, offset);
	tegra->sregs.tap1_val[port] = MISC_OUT_TAP_VAL(reg);

	reg = padctl_readl(tegra, offset);
	reg &= ~MISC_OUT_SEL(~0);
	reg |= MISC_OUT_SEL(0x33);
	padctl_writel(tegra, reg, offset);

	reg = padctl_readl(tegra, offset);
	tegra->sregs.amp_val[port] = MISC_OUT_AMP_VAL(reg);

	reg = padctl_readl(tegra, padregs->iophy_usb3_padX_ctlY_0[port][3]);
	reg &= ~DFE_CNTL_TAP_VAL(~0);
	reg |= DFE_CNTL_TAP_VAL(tegra->sregs.tap1_val[port]);
	padctl_writel(tegra, reg, padregs->iophy_usb3_padX_ctlY_0[port][3]);

	reg = padctl_readl(tegra, padregs->iophy_usb3_padX_ctlY_0[port][3]);
	reg &= ~DFE_CNTL_AMP_VAL(~0);
	reg |= DFE_CNTL_AMP_VAL(tegra->sregs.amp_val[port]);
	padctl_writel(tegra, reg, padregs->iophy_usb3_padX_ctlY_0[port][3]);

	tegra->dfe_ctx_saved = (1 << port);
}

static void save_ctle_context(struct tegra_xhci_hcd *tegra,
	u8 port)
{
	struct xhci_hcd *xhci = tegra->xhci;
	const struct tegra_xusb_padctl_regs *padregs = tegra->padregs;
	u32 offset;
	u32 reg;
	int ss_pads = tegra->soc_config->ss_pad_count;

	if (port > (ss_pads - 1)) {
		pr_err("%s invalid SS port number %u\n", __func__, port);
		return;
	}

	xhci_info(xhci, "saving restore CTLE context for port %d\n", port);

	/* if port1 is mapped to SATA lane then read from SATA register */
	if (port == 1 && XUSB_DEVICE_ID_T114 != tegra->device_id &&
			tegra->bdata->lane_owner & BIT(0))
		offset = padregs->iophy_misc_pad_s0_ctlY_0[5];
	else
		offset = padregs->iophy_misc_pad_pX_ctlY_0[port][5];

	/*
	 * Value set to IOPHY_MISC_PAD_x_CTL_6 where x P0/P1/S0/ is from,
	 * T114 refer PG USB3_FW_Programming_Guide_Host.doc section 14.3.10
	 * T124 refer PG T124_USB3_FW_Programming_Guide_Host.doc section 14.3.10
	 */
	reg = padctl_readl(tegra, offset);
	reg &= ~MISC_OUT_SEL(~0);
	reg |= MISC_OUT_SEL(0xa1);
	padctl_writel(tegra, reg, offset);

	reg = padctl_readl(tegra, offset);
	reg &= ~MISC_OUT_SEL(~0);
	reg |= MISC_OUT_SEL(0x21);
	padctl_writel(tegra, reg, offset);

	reg = padctl_readl(tegra, offset);
	tegra->sregs.ctle_g_val[port] = MISC_OUT_G_Z_VAL(reg);

	reg = padctl_readl(tegra, offset);
	reg &= ~MISC_OUT_SEL(~0);
	reg |= MISC_OUT_SEL(0x48);
	padctl_writel(tegra, reg, offset);

	reg = padctl_readl(tegra, offset);
	tegra->sregs.ctle_z_val[port] = MISC_OUT_G_Z_VAL(reg);

	reg = padctl_readl(tegra, padregs->iophy_usb3_padX_ctlY_0[port][1]);
	reg &= ~RX_EQ_Z_VAL(~0);
	reg |= RX_EQ_Z_VAL(tegra->sregs.ctle_z_val[port]);
	padctl_writel(tegra, reg, padregs->iophy_usb3_padX_ctlY_0[port][1]);

	reg = padctl_readl(tegra, padregs->iophy_usb3_padX_ctlY_0[port][1]);
	reg &= ~RX_EQ_G_VAL(~0);
	reg |= RX_EQ_G_VAL(tegra->sregs.ctle_g_val[port]);
	padctl_writel(tegra, reg, padregs->iophy_usb3_padX_ctlY_0[port][1]);

	tegra->ctle_ctx_saved = (1 << port);
}

#if defined(CONFIG_ARCH_TEGRA_12x_SOC) || defined(CONFIG_ARCH_TEGRA_13x_SOC)
static void tegra_xhci_restore_dfe_context(struct tegra_xhci_hcd *tegra,
	u8 port)
{
	struct xhci_hcd *xhci = tegra->xhci;
	u32 reg;

	/* don't restore if not saved */
	if (!(tegra->dfe_ctx_saved && (1 << port)))
		return;

	xhci_info(xhci, "restoring dfe context of port %d\n", port);

	/* restore dfe_cntl for the port */
	reg = padctl_readl(tegra
			, tegra->padregs->iophy_usb3_padX_ctlY_0[port][3]);
	reg &= ~(DFE_CNTL_AMP_VAL(~0) |
			DFE_CNTL_TAP_VAL(~0));
	reg |= DFE_CNTL_AMP_VAL(tegra->sregs.amp_val[port]) |
		DFE_CNTL_TAP_VAL(tegra->sregs.tap1_val[port]);
	padctl_writel(tegra, reg
			, tegra->padregs->iophy_usb3_padX_ctlY_0[port][3]);
}

static void restore_ctle_context(struct tegra_xhci_hcd *tegra,
	u8 port)
{
	struct xhci_hcd *xhci = tegra->xhci;
	u32 reg;

	/* don't restore if not saved */
	if (!(tegra->ctle_ctx_saved && (1 << port)))
		return;

	xhci_info(xhci, "restoring CTLE context of port %d\n", port);

	/* restore ctle for the port */
	reg = padctl_readl(tegra
			, tegra->padregs->iophy_usb3_padX_ctlY_0[port][1]);
	reg &= ~(RX_EQ_Z_VAL(~0) |
			RX_EQ_G_VAL(~0));
	reg |= (RX_EQ_Z_VAL(tegra->sregs.ctle_z_val[port]) |
		RX_EQ_G_VAL(tegra->sregs.ctle_g_val[port]));
	padctl_writel(tegra, reg
			, tegra->padregs->iophy_usb3_padX_ctlY_0[port][1]);
}
#endif

static void padctl_enable_usb_vbus(struct tegra_xhci_hcd *tegra, int pad)
{
	const struct tegra_xusb_padctl_regs *padregs = tegra->padregs;
	struct usb_vbus_en_oc *en_oc = &tegra->bdata->vbus_en_oc[pad];
	struct xhci_hcd *xhci = tegra->xhci;
	void __iomem *clk_base = IO_ADDRESS(TEGRA_CLK_RESET_BASE);
	struct regulator *vbus_regulator;
	const struct tegra_xusb_regulator_name *supply =
				&tegra->soc_config->supply;
	unsigned long flags;
	u32 reg;
	int err;

	vbus_regulator = tegra->xusb_utmi_vbus_regs[pad];

	spin_lock_irqsave(&tegra->lock, flags);

	/* WAR: need to disable VBUS_ENABLEx_OC_MAP before enable VBUS */
	reg = padctl_readl(tegra, padregs->oc_det_0);
	xhci_dbg(xhci, "%s: pad %d OC_DET_0 0x%x\n", __func__, pad, reg);
	reg &= ~OC_DETECTED_VBUS_PAD_MASK;
	padctl_writel(tegra, reg, padregs->oc_det_0);

	reg = padctl_readl(tegra, padregs->vbus_oc_map);
	reg &= ~VBUS_OC_MAP(en_oc->pin, ~0);
	reg |= VBUS_OC_MAP(en_oc->pin, OC_DISABLE);
	padctl_writel(tegra, reg, padregs->vbus_oc_map);

	/* WAR: disable PLLU power down,
	 * so HW can propagate the OCA transition
	 */
	reg = readl(clk_base + CLK_RST_PLLU_HW_PWRDN_CFG0_0);
	reg |= (PLLU_CLK_ENABLE_OVERRIDE_VALUE | PLLU_SEQ_IN_SWCTL);
	writel(reg, clk_base + CLK_RST_PLLU_HW_PWRDN_CFG0_0);

	/* clear false OC_DETECTED_VBUS_PADx */
	reg = padctl_readl(tegra, padregs->oc_det_0);
	reg &= ~OC_DETECTED_VBUS_PAD_MASK;
	reg |= OC_DETECTED_VBUS_PAD(en_oc->pin);
	padctl_writel(tegra, reg, padregs->oc_det_0);

	udelay(100);

	/* WAR: enable PLLU power down */
	reg = readl(clk_base + CLK_RST_PLLU_HW_PWRDN_CFG0_0);
	reg &= ~(PLLU_CLK_ENABLE_OVERRIDE_VALUE | PLLU_SEQ_IN_SWCTL);
	writel(reg, clk_base + CLK_RST_PLLU_HW_PWRDN_CFG0_0);

	/* Enable VBUS */
	reg = padctl_readl(tegra, padregs->oc_det_0);
	reg &= ~OC_DETECTED_VBUS_PAD_MASK;
	padctl_writel(tegra, reg, padregs->oc_det_0);

	reg = padctl_readl(tegra, padregs->vbus_oc_map);
	reg |= VBUS_ENABLE(en_oc->pin);
	padctl_writel(tegra, reg, padregs->vbus_oc_map);

	spin_unlock_irqrestore(&tegra->lock, flags);

	err = regulator_enable(vbus_regulator);
	if (err < 0) {
		xhci_err(xhci, "%s: regulator enable failed: %d\n",
					supply->utmi_vbuses[pad], err);
	}

	/* vbus has been supplied to device */

	/* WAR: A finite time (> 10ms) for OC detection pin to be pulled-up */
	msleep(20);

	/* WAR: Check and clear if there is any stray OC */
	reg = padctl_readl(tegra, padregs->oc_det_0);
	if (reg & OC_DETECTED_VBUS_PAD(en_oc->pin)) {
		xhci_dbg(xhci, "%s: clear stray OC OC_DET_0 0x%x\n",
				__func__, reg);

		err = regulator_disable(vbus_regulator);
		if (err < 0) {
			xhci_err(xhci, "%s: regulator disable failed: %d\n",
					supply->utmi_vbuses[pad], err);
		}

		spin_lock_irqsave(&tegra->lock, flags);

		reg &= ~OC_DETECTED_VBUS_PAD_MASK;
		reg |= OC_DETECTED_VBUS_PAD(en_oc->pin);
		padctl_writel(tegra, reg, padregs->oc_det_0);

		/* Enable VBUS back after clearing stray OC */
		reg = padctl_readl(tegra, padregs->oc_det_0);
		reg &= ~OC_DETECTED_VBUS_PAD_MASK;
		padctl_writel(tegra, reg, padregs->oc_det_0);

		reg = padctl_readl(tegra, padregs->vbus_oc_map);
		reg |= VBUS_ENABLE(en_oc->pin);
		padctl_writel(tegra, reg, padregs->vbus_oc_map);

		spin_unlock_irqrestore(&tegra->lock, flags);

		err = regulator_enable(vbus_regulator);
		if (err < 0) {
			xhci_err(xhci, "%s: regulator enable failed: %d\n",
						supply->utmi_vbuses[pad], err);
		}
	}

	spin_lock_irqsave(&tegra->lock, flags);

	/* Change the OC_MAP source and enable OC interrupt */
	reg = padctl_readl(tegra, padregs->usb2_oc_map_0);
	reg &= ~PORT_OC_PIN(pad, ~0);
	reg |= PORT_OC_PIN(pad, OC_VBUS_PAD(en_oc->pin));
	padctl_writel(tegra, reg, padregs->usb2_oc_map_0);

	reg = padctl_readl(tegra, padregs->oc_det_0);
	reg &= ~OC_DETECTED_VBUS_PAD_MASK;
	reg |= OC_DETECTED_INTR_ENABLE_VBUS_PAD(en_oc->pin);
	padctl_writel(tegra, reg, padregs->oc_det_0);

	reg = padctl_readl(tegra, padregs->vbus_oc_map);
	reg &= ~VBUS_OC_MAP(en_oc->pin, ~0);
	reg |= VBUS_OC_MAP(en_oc->pin, OC_VBUS_PAD(en_oc->pin));
	padctl_writel(tegra, reg, padregs->vbus_oc_map);

	spin_unlock_irqrestore(&tegra->lock, flags);

}

static void tegra_xhci_program_ulpi_pad(struct tegra_xhci_hcd *tegra,
	u8 port)
{
	const struct tegra_xusb_padctl_regs *padregs = tegra->padregs;
	u32 reg;

	reg = padctl_readl(tegra, padregs->usb2_pad_mux_0);
	reg &= ~USB2_ULPI_PAD;
	reg |= USB2_ULPI_PAD_OWNER_XUSB;
	padctl_writel(tegra, reg, padregs->usb2_pad_mux_0);

	reg = padctl_readl(tegra, padregs->usb2_port_cap_0);
	reg &= ~USB2_ULPI_PORT_CAP;
	reg |= (tegra->bdata->ulpicap << 24);
	padctl_writel(tegra, reg, padregs->usb2_port_cap_0);
	/* FIXME: Program below when more details available
	 * XUSB_PADCTL_ULPI_LINK_TRIM_CONTROL_0
	 * XUSB_PADCTL_ULPI_NULL_CLK_TRIM_CONTROL_0
	 */
}

static void tegra_xhci_program_utmip_power_lp0_exit(
	struct tegra_xhci_hcd *tegra, u8 port)
{
	u8 hs_pls = (tegra->sregs.hs_pls >> (4 * port)) & 0xf;

	if (hs_pls == ARU_CONTEXT_HS_PLS_SUSPEND ||
		hs_pls == ARU_CONTEXT_HS_PLS_FS_MODE)
		xusb_utmi_pad_driver_power(port, true);
	else
		xusb_utmi_pad_driver_power(port, false);
}

static void tegra_xhci_program_utmip_pad(struct tegra_xhci_hcd *tegra,
	u8 port)
{
#if defined(CONFIG_ARCH_TEGRA_21x_SOC)
	if (XUSB_IS_T210(tegra))
		t210_program_utmi_pad(tegra, port);
#else
	xusb_utmi_pad_init(port, USB2_PORT_CAP_HOST(port)
			, tegra->bdata->uses_external_pmic);
#endif

	if (tegra->lp0_exit)
		tegra_xhci_program_utmip_power_lp0_exit(tegra, port);

	/*Release OTG port if not in host mode*/
	if ((port == 0) && !is_otg_host(tegra))
		tegra_xhci_release_otg_port(true);
}

static inline bool xusb_use_sata_lane(struct tegra_xhci_hcd *tegra)
{
	bool ret = false;

	if (XUSB_DEVICE_ID_T124 == tegra->device_id)
		ret = ((tegra->bdata->portmap & TEGRA_XUSB_SS_P1)
				&& (tegra->bdata->lane_owner & SATA_LANE));
	if (XUSB_DEVICE_ID_T210 == tegra->device_id)
		ret = ((tegra->bdata->portmap & TEGRA_XUSB_SS_P3)
			&& ((tegra->bdata->lane_owner & 0xf000) == SATA_LANE));

	return ret;
}

#ifndef CONFIG_ARCH_TEGRA_21x_SOC
static void tegra_xhci_program_ss_pad(struct tegra_xhci_hcd *tegra,
	u8 port)
{
	/* We have host/device/otg driver to program
	 * Move to common API to reduce duplicate program
	 */
	xusb_ss_pad_init(port
		, GET_SS_PORTMAP(tegra->bdata->ss_portmap, port)
		, XUSB_HOST_MODE);

	tegra_xhci_restore_dfe_context(tegra, port);
	tegra_xhci_restore_ctle_context(tegra, port);
}
#endif

/* This function assigns the USB ports to the controllers,
 * then programs the port capabilities and pad parameters
 * of ports assigned to XUSB after booted to OS.
 */
static void
tegra_xhci_padctl_portmap_and_caps(struct tegra_xhci_hcd *tegra)
{
	const struct tegra_xusb_padctl_regs *padregs = tegra->padregs;
	u32 reg = 0;
	unsigned pad;
	u32 ss_pads;
	char prod_name[15];
	u32 host_ports = get_host_controlled_ports(tegra);

	if (tegra->prod_list)
		tegra_prod_set_by_name(&tegra->base_list[0], "prod",
				tegra->prod_list);

	reg = padctl_readl(tegra, padregs->usb2_bias_pad_ctlY_0[0]);
	reg &= ~(USB2_BIAS_HS_SQUELCH_LEVEL);
	reg |= tegra->cdata->hs_squelch_level;
	padctl_writel(tegra, reg, padregs->usb2_bias_pad_ctlY_0[0]);

	for_each_enabled_utmi_pad(pad, tegra) {
		sprintf(prod_name, XUSB_PROD_PREFIX_UTMI "%d", pad);
		tegra_prod_set_by_name(&tegra->base_list[0], prod_name,
					tegra->prod_list);
		tegra_xhci_program_utmip_pad(tegra, pad);
	}

	if (tegra->otg_port_owned && tegra->lp0_exit) {
		usb2_vbus_id_init();
		xusb_utmi_pad_init(0, USB2_PORT_CAP_OTG(0), false);
		tegra_xhci_program_utmip_power_lp0_exit(tegra,
				tegra->hs_otg_portnum);
	}

	for_each_enabled_hsic_pad(pad, tegra) {
		sprintf(prod_name, XUSB_PROD_PREFIX_HSIC "%d", pad);
		tegra_prod_set_by_name(&tegra->base_list[0], prod_name,
					tegra->prod_list);
		hsic_pad_enable(tegra, pad);
	}

	if (tegra->bdata->portmap & TEGRA_XUSB_ULPI_P0)
		tegra_xhci_program_ulpi_pad(tegra, 0);

	if (xusb_use_sata_lane(tegra)) {
		sprintf(prod_name, XUSB_PROD_PREFIX_SATA "0");
		tegra_prod_set_by_name(&tegra->base_list[0], prod_name,
					tegra->prod_list);
	}
	ss_pads = tegra->soc_config->ss_pad_count;
	for_each_ss_pad(pad, ss_pads) {
		if (host_ports & (1 << pad)) {
#if defined(CONFIG_ARCH_TEGRA_21x_SOC)
			bool is_otg_port =
				(tegra->bdata->otg_portmap & (1 << pad)) != 0;
#endif
			tegra->soc_config->check_lane_owner_by_pad(pad
					, tegra->bdata->lane_owner);

			sprintf(prod_name, XUSB_PROD_PREFIX_SS "%d", pad);
			tegra_prod_set_by_name(&tegra->base_list[0],
						prod_name,
						tegra->prod_list);
#if defined(CONFIG_ARCH_TEGRA_21x_SOC)
			t210_program_ss_pad(tegra, pad, is_otg_port);
#else
			tegra_xhci_program_ss_pad(tegra, pad);
#endif
		} else {
			reg = padctl_readl(tegra
				, padregs->iophy_misc_pad_pX_ctlY_0[pad][2]);
			reg &= ~RX_IDLE_MODE;
			reg |= RX_IDLE_MODE_OVRD;
			padctl_writel(tegra, reg
				, padregs->iophy_misc_pad_pX_ctlY_0[pad][2]);

			/* If USB3_SS port1 mapped to SATA lane but unused */
			if (XUSB_DEVICE_ID_T124 == tegra->device_id &&
					tegra->bdata->lane_owner & BIT(0)) {
				reg = padctl_readl(tegra,
					padregs->iophy_misc_pad_s0_ctlY_0[2]);
				reg &= ~RX_IDLE_MODE;
				reg |= RX_IDLE_MODE_OVRD;
				padctl_writel(tegra, reg,
					padregs->iophy_misc_pad_s0_ctlY_0[2]);
			}
		}
	}

	if (XUSB_DEVICE_ID_T114 != tegra->device_id) {
		tegra_xhci_setup_gpio_for_ss_lane(tegra);
		usb3_phy_pad_enable(tegra->bdata->lane_owner);
	}
}

/* This function read XUSB registers and stores in device context */
static void
tegra_xhci_save_xusb_ctx(struct tegra_xhci_hcd *tegra)
{

	/* a. Save the IPFS registers */
	tegra->sregs.msi_bar_sz =
		readl(tegra->ipfs_base + IPFS_XUSB_HOST_MSI_BAR_SZ_0);

	tegra->sregs.msi_axi_barst =
		readl(tegra->ipfs_base + IPFS_XUSB_HOST_MSI_AXI_BAR_ST_0);

	tegra->sregs.msi_fpci_barst =
		readl(tegra->ipfs_base + IPFS_XUSB_HOST_FPCI_BAR_ST_0);

	tegra->sregs.msi_vec0 =
		readl(tegra->ipfs_base + IPFS_XUSB_HOST_MSI_VEC0_0);

	tegra->sregs.msi_en_vec0 =
		readl(tegra->ipfs_base + IPFS_XUSB_HOST_MSI_EN_VEC0_0);

	tegra->sregs.fpci_error_masks =
		readl(tegra->ipfs_base + IPFS_XUSB_HOST_FPCI_ERROR_MASKS_0);

	tegra->sregs.intr_mask =
		readl(tegra->ipfs_base + IPFS_XUSB_HOST_INTR_MASK_0);

	tegra->sregs.ipfs_intr_enable =
		readl(tegra->ipfs_base + IPFS_XUSB_HOST_IPFS_INTR_ENABLE_0);

	tegra->sregs.ufpci_config =
		readl(tegra->ipfs_base + IPFS_XUSB_HOST_UFPCI_CONFIG_0);

	tegra->sregs.clkgate_hysteresis =
		readl(tegra->ipfs_base + IPFS_XUSB_HOST_CLKGATE_HYSTERESIS_0);

	tegra->sregs.xusb_host_mccif_fifo_cntrl =
		readl(tegra->ipfs_base + IPFS_XUSB_HOST_MCCIF_FIFOCTRL_0);

	/* b. Save the CFG registers */

	tegra->sregs.hs_pls =
		readl(tegra->fpci_base + XUSB_CFG_ARU_CONTEXT_HS_PLS);

	tegra->sregs.fs_pls =
		readl(tegra->fpci_base + XUSB_CFG_ARU_CONTEXT_FS_PLS);

	tegra->sregs.hs_fs_speed =
		readl(tegra->fpci_base + XUSB_CFG_ARU_CONTEXT_HSFS_SPEED);

	tegra->sregs.hs_fs_pp =
		readl(tegra->fpci_base + XUSB_CFG_ARU_CONTEXT_HSFS_PP);

	tegra->sregs.cfg_aru =
		readl(tegra->fpci_base + XUSB_CFG_ARU_CONTEXT);

	tegra->sregs.cfg_order =
		readl(tegra->fpci_base + XUSB_CFG_FPCICFG);

	tegra->sregs.cfg_fladj =
		readl(tegra->fpci_base + XUSB_CFG_24);

	tegra->sregs.cfg_sid =
		readl(tegra->fpci_base + XUSB_CFG_16);
}

static void tegra_xhci_handle_otg_port_change(struct tegra_xhci_hcd *tegra)
{
	struct xhci_hcd *xhci = tegra->xhci;
	struct platform_device *pdev = tegra->pdev;

	dev_info(&pdev->dev, "otg port pp %s\n",
			tegra->otg_port_owned ? "on" : "off");

	if (tegra->otg_port_owned)
		schedule_work(&tegra->reset_otg_sspi_work);
	else
		xhci_hub_control(xhci->shared_hcd, ClearPortFeature,
			USB_PORT_FEAT_POWER,
			tegra->hs_otg_portnum + 1, NULL, 0);
}

/* This function restores XUSB registers from device context */
static void
tegra_xhci_restore_ctx(struct tegra_xhci_hcd *tegra)
{
	/* Restore Cfg registers */
	writel(tegra->sregs.hs_pls,
		tegra->fpci_base + XUSB_CFG_ARU_CONTEXT_HS_PLS);

	writel(tegra->sregs.fs_pls,
		tegra->fpci_base + XUSB_CFG_ARU_CONTEXT_FS_PLS);

	writel(tegra->sregs.hs_fs_speed,
		tegra->fpci_base + XUSB_CFG_ARU_CONTEXT_HSFS_SPEED);

	writel(tegra->sregs.hs_fs_pp,
		tegra->fpci_base + XUSB_CFG_ARU_CONTEXT_HSFS_PP);

	writel(tegra->sregs.cfg_aru,
		tegra->fpci_base + XUSB_CFG_ARU_CONTEXT);

	writel(tegra->sregs.cfg_order,
		tegra->fpci_base + XUSB_CFG_FPCICFG);

	writel(tegra->sregs.cfg_fladj,
		tegra->fpci_base + XUSB_CFG_24);

	writel(tegra->sregs.cfg_sid,
		tegra->fpci_base + XUSB_CFG_16);

	/* Restore IPFS registers */

	writel(tegra->sregs.msi_bar_sz,
		tegra->ipfs_base + IPFS_XUSB_HOST_MSI_BAR_SZ_0);

	writel(tegra->sregs.msi_axi_barst,
		tegra->ipfs_base + IPFS_XUSB_HOST_MSI_AXI_BAR_ST_0);

	writel(tegra->sregs.msi_fpci_barst,
		tegra->ipfs_base + IPFS_XUSB_HOST_FPCI_BAR_ST_0);

	writel(tegra->sregs.msi_vec0,
		tegra->ipfs_base + IPFS_XUSB_HOST_MSI_VEC0_0);

	writel(tegra->sregs.msi_en_vec0,
		tegra->ipfs_base + IPFS_XUSB_HOST_MSI_EN_VEC0_0);

	writel(tegra->sregs.fpci_error_masks,
		tegra->ipfs_base + IPFS_XUSB_HOST_FPCI_ERROR_MASKS_0);

	writel(tegra->sregs.intr_mask,
		tegra->ipfs_base + IPFS_XUSB_HOST_INTR_MASK_0);

	writel(tegra->sregs.ipfs_intr_enable,
		tegra->ipfs_base + IPFS_XUSB_HOST_IPFS_INTR_ENABLE_0);

	writel(tegra->sregs.ufpci_config,
		tegra->fpci_base + IPFS_XUSB_HOST_UFPCI_CONFIG_0);

	writel(tegra->sregs.clkgate_hysteresis,
		tegra->ipfs_base + IPFS_XUSB_HOST_CLKGATE_HYSTERESIS_0);

	writel(tegra->sregs.xusb_host_mccif_fifo_cntrl,
		tegra->ipfs_base + IPFS_XUSB_HOST_MCCIF_FIFOCTRL_0);
}

static void tegra_xhci_enable_fw_message(struct tegra_xhci_hcd *tegra)
{
	fw_message_send(tegra, MBOX_CMD_MSG_ENABLED, 0 /* no data needed */);
}

static int load_firmware(struct tegra_xhci_hcd *tegra, bool reset_aru)
{
	struct platform_device *pdev = tegra->pdev;
	struct cfgtbl *cfg_tbl = (struct cfgtbl *) tegra->firmware.data;
	u32 phys_addr_lo;
	u32 reg;
	u16 nblocks;
	time_t fw_time;
	struct tm fw_tm;
	u8 hc_caplength;
	u32 usbsts, count = 0xff;
	struct xhci_cap_regs __iomem *cap_regs;
	struct xhci_op_regs __iomem *op_regs;
	int pad, ss_pads;
	unsigned long delay;

	/* Program SS port map config */
	ss_pads = tegra->soc_config->ss_pad_count;
	cfg_tbl->ss_portmap = 0x0;
	cfg_tbl->ss_portmap |=
		(tegra->bdata->portmap & ((1 << ss_pads) - 1));

	cfg_tbl->num_hsic_port = 0;
	for_each_enabled_hsic_pad(pad, tegra)
		cfg_tbl->num_hsic_port++;

	dev_info(&pdev->dev, "num_hsic_port %d\n", cfg_tbl->num_hsic_port);

	/* First thing, reset the ARU. By the time we get to
	 * loading boot code below, reset would be complete.
	 * alternatively we can busy wait on rst pending bit.
	 */
	/* Don't reset during ELPG/LP0 exit path */
	if (reset_aru) {
		iowrite32(0x1, tegra->fpci_base + XUSB_CFG_ARU_RST);
		usleep_range(1000, 2000);
	}
	if (csb_read(tegra, XUSB_CSB_MP_ILOAD_BASE_LO) != 0) {
		dev_err(&pdev->dev, "Firmware already loaded, Falcon state 0x%x\n",
				csb_read(tegra, XUSB_FALC_CPUCTL));
		return 0;
	}

	/* update the phys_log_buffer and total_entries here */
	if (test_bit(FW_LOG_CONTEXT_VALID, &tegra->log.flags)) {
		cfg_tbl->phys_addr_log_buffer = tegra->log.phys_addr;
		cfg_tbl->total_log_entries = FW_LOG_COUNT;
	}

	phys_addr_lo = tegra->firmware.dma;
	phys_addr_lo += sizeof(struct cfgtbl);

	/* Program the size of DFI into ILOAD_ATTR */
	csb_write(tegra, XUSB_CSB_MP_ILOAD_ATTR, tegra->firmware.size);

	/* Boot code of the firmware reads the ILOAD_BASE_LO register
	 * to get to the start of the dfi in system memory.
	 */
	csb_write(tegra, XUSB_CSB_MP_ILOAD_BASE_LO, phys_addr_lo);

	/* Program the ILOAD_BASE_HI with a value of MSB 32 bits */
	csb_write(tegra, XUSB_CSB_MP_ILOAD_BASE_HI, 0);

	/* Set BOOTPATH to 1 in APMAP Register. Bit 31 is APMAP_BOOTMAP */
	csb_write(tegra, XUSB_CSB_MP_APMAP, APMAP_BOOTPATH);

	/* Invalidate L2IMEM. */
	csb_write(tegra, XUSB_CSB_MP_L2IMEMOP_TRIG, L2IMEM_INVALIDATE_ALL);

	/* Initiate fetch of Bootcode from system memory into L2IMEM.
	 * Program BootCode location and size in system memory.
	 */
	reg = ((cfg_tbl->boot_codetag / IMEM_BLOCK_SIZE) &
			L2IMEMOP_SIZE_SRC_OFFSET_MASK)
			<< L2IMEMOP_SIZE_SRC_OFFSET_SHIFT;
	reg |= ((cfg_tbl->boot_codesize / IMEM_BLOCK_SIZE) &
			L2IMEMOP_SIZE_SRC_COUNT_MASK)
			<< L2IMEMOP_SIZE_SRC_COUNT_SHIFT;
	csb_write(tegra, XUSB_CSB_MP_L2IMEMOP_SIZE, reg);

	/* Trigger L2IMEM Load operation. */
	csb_write(tegra, XUSB_CSB_MP_L2IMEMOP_TRIG, L2IMEM_LOAD_LOCKED_RESULT);

	/* Setup Falcon Auto-fill */
	nblocks = (cfg_tbl->boot_codesize / IMEM_BLOCK_SIZE);
	if ((cfg_tbl->boot_codesize % IMEM_BLOCK_SIZE) != 0)
		nblocks += 1;
	csb_write(tegra, XUSB_FALC_IMFILLCTL, nblocks);

	reg = (cfg_tbl->boot_codetag / IMEM_BLOCK_SIZE) & IMFILLRNG_TAG_MASK;
	reg |= (((cfg_tbl->boot_codetag + cfg_tbl->boot_codesize)
			/IMEM_BLOCK_SIZE) - 1) << IMFILLRNG1_TAG_HI_SHIFT;
	csb_write(tegra, XUSB_FALC_IMFILLRNG1, reg);

	csb_write(tegra, XUSB_FALC_DMACTL, 0);

	/* wait for RESULT_VLD to get set */
	delay = jiffies + msecs_to_jiffies(10);
	do {
		usleep_range(50, 60);
		usbsts = csb_read(tegra, XUSB_CSB_MEMPOOL_L2IMEMOP_RESULT);
	} while (!(usbsts & XUSB_CSB_MEMPOOL_L2IMEMOP_RESULT_VLD) &&
		time_is_after_jiffies(delay));

	if (time_is_before_jiffies(delay) &&
		!(usbsts & XUSB_CSB_MEMPOOL_L2IMEMOP_RESULT_VLD)) {
		dev_err(&pdev->dev, "DMA controller not ready 0x08%x\n",
			csb_read(tegra, XUSB_CSB_MEMPOOL_L2IMEMOP_RESULT));
		return -EFAULT;
	}

	csb_write(tegra, XUSB_FALC_BOOTVEC, cfg_tbl->boot_codetag);

	/* Start Falcon CPU */
	csb_write(tegra, XUSB_FALC_CPUCTL, CPUCTL_STARTCPU);
	usleep_range(1000, 2000);

	fw_time = cfg_tbl->fwimg_created_time;
	time_to_tm(fw_time, 0, &fw_tm);
	dev_info(&pdev->dev,
		"Firmware timestamp: %ld-%02d-%02d %02d:%02d:%02d UTC, Version: %02x.%02x %s, Falcon state 0x%x\n",
		fw_tm.tm_year + 1900,
		fw_tm.tm_mon + 1, fw_tm.tm_mday, fw_tm.tm_hour,
		fw_tm.tm_min, fw_tm.tm_sec,
		FW_MAJOR_VERSION(cfg_tbl->version_id),
		FW_MINOR_VERSION(cfg_tbl->version_id),
		(cfg_tbl->build_log == FW_LOG_TYPE_DMA_SYS_MEM) ?
			"debug" : "release",
		csb_read(tegra, XUSB_FALC_CPUCTL));

	/* return fail if firmware status is not good */
	if (csb_read(tegra, XUSB_FALC_CPUCTL) == XUSB_FALC_STATE_HALTED)
		return -EFAULT;

	cap_regs = IO_ADDRESS(tegra->host_phy_base);
	hc_caplength = HC_LENGTH(ioread32(&cap_regs->hc_capbase));
	op_regs = IO_ADDRESS(tegra->host_phy_base + hc_caplength);

	/* wait for USBSTS_CNR to get set */
	do {
		usbsts = ioread32(&op_regs->status);
	} while ((usbsts & STS_CNR) && count--);

	if (!count && (usbsts & STS_CNR)) {
		dev_err(&pdev->dev, "Controller not ready\n");
		return -EFAULT;
	}

	return 0;
}

static void tegra_xhci_release_port_ownership(struct tegra_xhci_hcd *tegra,
	bool release)
{
	const struct tegra_xusb_padctl_regs *padregs = tegra->padregs;
	u32 reg;

	/* Issue is only applicable for T114 */
	if (XUSB_DEVICE_ID_T114 != tegra->device_id)
		return;

	reg = padctl_readl(tegra, padregs->usb2_pad_mux_0);
	reg &= ~(USB2_OTG_PAD_PORT_MASK(0) | USB2_OTG_PAD_PORT_MASK(1) |
			USB2_OTG_PAD_PORT_MASK(2));

	if (!release) {
		if (tegra->bdata->portmap & TEGRA_XUSB_USB2_P0)
			if (is_otg_host(tegra))
				reg |= USB2_OTG_PAD_PORT_OWNER_XUSB(0);
		if (tegra->bdata->portmap & TEGRA_XUSB_USB2_P1)
			reg |= USB2_OTG_PAD_PORT_OWNER_XUSB(1);
		if (tegra->bdata->portmap & TEGRA_XUSB_USB2_P2)
			reg |= USB2_OTG_PAD_PORT_OWNER_XUSB(2);
	}

	padctl_writel(tegra, reg, padregs->usb2_pad_mux_0);
}

static int get_host_controlled_ports(struct tegra_xhci_hcd *tegra)
{
	int enabled_ports = 0;

	enabled_ports = tegra->bdata->portmap;

	if (tegra->otg_port_owned)
		enabled_ports |= tegra->bdata->otg_portmap;

	return enabled_ports;
}

static int get_wake_sources_for_host_controlled_ports(int enabled_ports)
{
	int wake_events = 0;

	if (enabled_ports & TEGRA_XUSB_USB2_P0)
		wake_events |= USB2_PORT0_WAKEUP_EVENT;
	if (enabled_ports & TEGRA_XUSB_USB2_P1)
		wake_events |= USB2_PORT1_WAKEUP_EVENT;
	if (enabled_ports & TEGRA_XUSB_USB2_P2)
		wake_events |= USB2_PORT2_WAKEUP_EVENT;
	if (enabled_ports & TEGRA_XUSB_USB2_P3)
		wake_events |= USB2_PORT3_WAKEUP_EVENT;

	if (enabled_ports & TEGRA_XUSB_SS_P0)
		wake_events |= SS_PORT0_WAKEUP_EVENT;
	if (enabled_ports & TEGRA_XUSB_SS_P1)
		wake_events |= SS_PORT1_WAKEUP_EVENT;
	if (enabled_ports & TEGRA_XUSB_SS_P2)
		wake_events |= SS_PORT2_WAKEUP_EVENT;
	if (enabled_ports & TEGRA_XUSB_SS_P3)
		wake_events |= SS_PORT3_WAKEUP_EVENT;

	if (enabled_ports & TEGRA_XUSB_HSIC_P0)
		wake_events |= USB2_HSIC_PORT0_WAKEUP_EVENT;
	if (enabled_ports & TEGRA_XUSB_HSIC_P1)
		wake_events |= USB2_HSIC_PORT1_WAKEUP_EVENT;

	return wake_events;
}

/* SS ELPG Entry initiated by fw */
static int tegra_xhci_ss_elpg_entry(struct tegra_xhci_hcd *tegra)
{
	struct xhci_hcd *xhci = tegra->xhci;
	u32 ret = 0;
	u32 host_ports;
	int partition_id;

	must_have_sync_lock(tegra);

	/* update maximum BW requirement to 0 */
	tegra_xusb_set_bw(tegra, 0);

	/* This is SS partition ELPG entry
	 * STEP 0: firmware will set WOC WOD bits in PVTPORTSC2 regs.
	 */

	/* Step 0: Acquire mbox and send PWRGATE msg to firmware
	 * only if it is sw initiated one
	 */

	/* STEP 1: xHCI firmware and xHCIPEP driver communicates
	 * SuperSpeed partition ELPG entry via mailbox protocol
	 */

	/* STEP 2: xHCI PEP driver and XUSB device mode driver
	 * enable the XUSB wakeup interrupts for the SuperSpeed
	 * and USB2.0 ports assigned to host.Section 4.1 Step 3
	 */
	host_ports = get_host_controlled_ports(tegra);
	tegra_xhci_ss_wake_on_interrupts(host_ports, true);

	/* STEP 3: xHCI PEP driver initiates the signal sequence
	 * to enable the XUSB SSwake detection logic for the
	 * SuperSpeed ports assigned to host.Section 4.1 Step 4
	 */
	tegra_xhci_ss_wake_signal(host_ports, true);

	/* STEP 4: System Power Management driver disables the
	 * XUSB SuperSpeed partition power rails.
	 */
	debug_print_portsc(xhci);

	/* tegra_powergate_partition also does partition reset assert */
	partition_id = tegra_pd_get_powergate_id(tegra_xusba_pd);
	if (partition_id < 0)
		return -EINVAL;
	ret = tegra_powergate_partition_with_clk_off(partition_id);
	if (ret) {
		xhci_err(xhci, "%s: could not powergate xusba partition\n",
				__func__);
		/* TODO: error recovery? */
	}
	tegra->ss_pwr_gated = true;

	/* STEP 5: xHCI PEP driver initiates the signal sequence
	 * to enable the XUSB SSwake detection logic for the
	 * SuperSpeed ports assigned to host.Section 4.1 Step 7
	 */
	tegra_xhci_ss_vcore(host_ports, true);

	return ret;
}

/* Host ELPG Entry */
static int tegra_xhci_host_elpg_entry(struct tegra_xhci_hcd *tegra)
{
	struct xhci_hcd *xhci = tegra->xhci;
	int host_ports = get_host_controlled_ports(tegra);
	u32 ret = 0;
	int partition_id;

	must_have_sync_lock(tegra);

	/* If ss is already powergated skip ss ctx save stuff */
	if (tegra->ss_pwr_gated) {
		xhci_info(xhci, "%s: SS partition is already powergated\n",
			__func__);
	} else {
		ret = tegra_xhci_ss_elpg_entry(tegra);
		if (ret) {
			xhci_err(xhci, "%s: ss_elpg_entry failed %d\n",
				__func__, ret);
			return ret;
		}
	}

	/* 1. IS INTR PENDING INT_PENDING=1 ? */

	/* STEP 1.1: Do a context save of XUSB and IPFS registers */
	tegra_xhci_save_xusb_ctx(tegra);

	/* calculate rctrl_val and tctrl_val */
	tegra_xhci_war_for_tctrl_rctrl(tegra);

	pmc_setup_wake_detect(tegra);

	tegra_xhci_hs_wake_on_interrupts(host_ports, true);
	xhci_dbg(xhci, "%s: PMC_UTMIP_UHSIC_SLEEP_CFG_0 = %x\n", __func__,
		tegra_usb_pmc_reg_read(PMC_UTMIP_UHSIC_SLEEP_CFG_0));

	/* tegra_powergate_partition also does partition reset assert */
	partition_id = tegra_pd_get_powergate_id(tegra_xusbc_pd);
	if (partition_id < 0)
		return -EINVAL;
	ret = tegra_powergate_partition_with_clk_off(partition_id);
	if (ret) {
		xhci_err(xhci, "%s: could not unpowergate xusbc partition %d\n",
			__func__, ret);
		/* TODO: error handling? */
		return ret;
	}
	tegra->host_pwr_gated = true;

	if (tegra->soc_config->quirks & TEGRA_XUSB_USE_HS_SRC_CLOCK2)
		clk_disable(tegra->pll_re_vco_clk);
	clk_disable(tegra->emc_clk);
	/* set port ownership to SNPS */
	tegra_xhci_release_port_ownership(tegra, true);

#ifdef CONFIG_ARCH_TEGRA_21x_SOC
	utmi_phy_pad_disable(tegra->prod_list);
	utmi_phy_iddq_override(true);
#endif

	xhci_dbg(xhci, "%s: PMC_UTMIP_UHSIC_SLEEP_CFG_0 = %x\n", __func__,
		tegra_usb_pmc_reg_read(PMC_UTMIP_UHSIC_SLEEP_CFG_0));

	xhci_info(xhci, "%s: elpg_entry: completed\n", __func__);
	xhci_dbg(xhci, "%s: HOST POWER STATUS = %d\n",
		__func__, tegra_powergate_is_powered(partition_id));
	return ret;
}

/* SS ELPG Exit triggered by PADCTL irq */
/**
 * tegra_xhci_ss_partition_elpg_exit - bring XUSBA partition out from elpg
 *
 * This function must be called with tegra->sync_lock acquired.
 *
 * @tegra: xhci controller context
 * @return 0 for success, or error numbers
 */
static int tegra_xhci_ss_partition_elpg_exit(struct tegra_xhci_hcd *tegra)
{
	struct xhci_hcd *xhci = tegra->xhci;
	int ret = 0;
	int host_ports = get_host_controlled_ports(tegra);
	int partition_id;

	must_have_sync_lock(tegra);

	/* if we are exiting elpg due to no longer owning otg port, then
	 * need to disable wake detect logic
	 */
	if (!tegra->otg_port_owned && tegra->otg_port_ownership_changed)
		host_ports |= tegra->bdata->otg_portmap;

	if (tegra->ss_pwr_gated && (tegra->ss_wake_event ||
			tegra->hs_wake_event || tegra->host_resume_req)) {

		/*
		 * PWR_UNGATE SS partition. XUSBA
		 * tegra_unpowergate_partition also does partition reset
		 * deassert
		 */
		partition_id = tegra_pd_get_powergate_id(tegra_xusba_pd);
		if (partition_id < 0)
			return -EINVAL;
		ret = tegra_unpowergate_partition_with_clk_on(partition_id);
		if (ret) {
			xhci_err(xhci,
			"%s: could not unpowergate xusba partition %d\n",
			__func__, ret);
			goto out;
		}
		if (tegra->ss_wake_event)
			tegra->ss_wake_event = false;

	} else {
		xhci_info(xhci, "%s: ss already power gated\n",
			__func__);
		return ret;
	}

	/* Step 4: Disable ss wake detection logic */
	tegra_xhci_ss_wake_on_interrupts(host_ports, false);

	/* Step 4.1: Disable ss wake detection logic */
	tegra_xhci_ss_vcore(host_ports, false);

	/* wait 150us */
	usleep_range(150, 200);

	/* Step 4.2: Disable ss wake detection logic */
	tegra_xhci_ss_wake_signal(host_ports, false);

	xhci_dbg(xhci, "%s: SS ELPG EXIT. ALL DONE\n", __func__);
	tegra->ss_pwr_gated = false;
out:
	return ret;
}

static void ss_partition_elpg_exit_work(struct work_struct *work)
{
	struct tegra_xhci_hcd *tegra = container_of(work, struct tegra_xhci_hcd,
		ss_elpg_exit_work);

	mutex_lock(&tegra->sync_lock);
	tegra_xhci_ss_partition_elpg_exit(tegra);
	mutex_unlock(&tegra->sync_lock);
}

/* read pmc WAKE2_STATUS register to know if SS port caused remote wake */
static void update_remote_wakeup_ports(struct tegra_xhci_hcd *tegra)
{
	struct xhci_hcd *xhci = tegra->xhci;
	u32 wake2_status;
	int port;

#define PMC_WAKE2_STATUS	0x168
#define PADCTL_WAKE		(1 << (58 - 32)) /* PADCTL is WAKE#58 */

	wake2_status = tegra_usb_pmc_reg_read(PMC_WAKE2_STATUS);

	if (wake2_status & PADCTL_WAKE) {
		/* FIXME: This is customized for Dalmore, find a generic way */
		set_bit(0, &tegra->usb3_rh_remote_wakeup_ports);
		/* clear wake status */
		tegra_usb_pmc_reg_write(PMC_WAKE2_STATUS, PADCTL_WAKE);
	}

	/* set all usb2 ports with RESUME link state as wakup ports  */
	for (port = 0; port < xhci->num_usb2_ports; port++) {
		u32 portsc = xhci_readl(xhci, xhci->usb2_ports[port]);

		if ((portsc & PORT_PLS_MASK) == XDEV_RESUME)
			set_bit(port, &tegra->usb2_rh_remote_wakeup_ports);
	}

	xhci_dbg(xhci, "%s: usb2 roothub remote_wakeup_ports 0x%lx\n",
			__func__, tegra->usb2_rh_remote_wakeup_ports);
	xhci_dbg(xhci, "%s: usb3 roothub remote_wakeup_ports 0x%lx\n",
			__func__, tegra->usb3_rh_remote_wakeup_ports);
}

static void wait_remote_wakeup_ports(struct usb_hcd *hcd)
{
	struct xhci_hcd *xhci = hcd_to_xhci(hcd);
	struct tegra_xhci_hcd *tegra = hcd_to_tegra_xhci(hcd);
	int port, num_ports;
	unsigned long *remote_wakeup_ports;
	u32 portsc;
	__le32 __iomem	**port_array;
	unsigned char *rh;
	unsigned int retry = 64;
	struct xhci_bus_state *bus_state;

	bus_state = &xhci->bus_state[hcd_index(hcd)];

	if (hcd == xhci->shared_hcd) {
		port_array = xhci->usb3_ports;
		num_ports = xhci->num_usb3_ports;
		remote_wakeup_ports = &tegra->usb3_rh_remote_wakeup_ports;
		rh = "usb3 roothub";
	} else {
		port_array = xhci->usb2_ports;
		num_ports = xhci->num_usb2_ports;
		remote_wakeup_ports = &tegra->usb2_rh_remote_wakeup_ports;
		rh = "usb2 roothub";
	}

	while (*remote_wakeup_ports && retry--) {
		for_each_set_bit(port, remote_wakeup_ports, num_ports) {
			bool can_continue;

			portsc = xhci_readl(xhci, port_array[port]);

			if (!(portsc & PORT_CONNECT)) {
				/* nothing to do if already disconnected */
				clear_bit(port, remote_wakeup_ports);
				continue;
			}

			if (hcd == xhci->shared_hcd) {
				can_continue =
					(portsc & PORT_PLS_MASK) == XDEV_U0;
			} else {
				unsigned long flags;

				spin_lock_irqsave(&xhci->lock, flags);
				can_continue =
				test_bit(port, &bus_state->resuming_ports);
				spin_unlock_irqrestore(&xhci->lock, flags);
			}

			if (can_continue)
				clear_bit(port, remote_wakeup_ports);
			else
				xhci_dbg(xhci, "%s: %s port %d status 0x%x\n",
					__func__, rh, port, portsc);
		}

		if (*remote_wakeup_ports)
			msleep(20); /* give some time, irq will direct U0 */
	}

	xhci_dbg(xhci, "%s: %s remote_wakeup_ports 0x%lx\n", __func__, rh,
			*remote_wakeup_ports);
}

static void tegra_xhci_war_for_tctrl_rctrl(struct tegra_xhci_hcd *tegra)
{
	const struct tegra_xusb_padctl_regs *padregs = tegra->padregs;
	u32 reg, utmip_rctrl_val, utmip_tctrl_val, pad_mux, portmux, portowner;

	portmux = USB2_OTG_PAD_PORT_MASK(0) | USB2_OTG_PAD_PORT_MASK(1);
	portowner = USB2_OTG_PAD_PORT_OWNER_XUSB(0) |
			USB2_OTG_PAD_PORT_OWNER_XUSB(1);

	/* Only applicable on T114/T124 */
	if ((XUSB_DEVICE_ID_T114 != tegra->device_id) &&
		(XUSB_DEVICE_ID_T124 != tegra->device_id))
		return;

	if (XUSB_DEVICE_ID_T114 != tegra->device_id) {
		portmux |= USB2_OTG_PAD_PORT_MASK(2);
		portowner |= USB2_OTG_PAD_PORT_OWNER_XUSB(2);
	}

	/* Use xusb padctl space only when xusb owns all UTMIP port */
	pad_mux = padctl_readl(tegra, padregs->usb2_pad_mux_0);
	if ((pad_mux & portmux) == portowner) {
		/* XUSB_PADCTL_USB2_BIAS_PAD_CTL_0_0::PD = 0 and
		 * XUSB_PADCTL_USB2_BIAS_PAD_CTL_0_0::PD_TRK = 0
		 */
		reg = padctl_readl(tegra, padregs->usb2_bias_pad_ctlY_0[0]);
		reg &= ~((1 << 12) | (1 << 13));
		padctl_writel(tegra, reg, padregs->usb2_bias_pad_ctlY_0[0]);

		/* wait 20us */
		usleep_range(20, 30);

		/* Read XUSB_PADCTL:: XUSB_PADCTL_USB2_BIAS_PAD_CTL_1_0
		 * :: TCTRL and RCTRL
		 */
		reg = padctl_readl(tegra, padregs->usb2_bias_pad_ctlY_0[1]);
		utmip_rctrl_val = RCTRL(reg);
		utmip_tctrl_val = TCTRL(reg);

		/*
		 * tctrl_val = 0x1f - (16 - ffz(utmip_tctrl_val)
		 * rctrl_val = 0x1f - (16 - ffz(utmip_rctrl_val)
		 */
		utmip_rctrl_val = 0xf + ffz(utmip_rctrl_val);
		utmip_tctrl_val = 0xf + ffz(utmip_tctrl_val);
		utmi_phy_update_trking_data(utmip_tctrl_val, utmip_rctrl_val);
		xhci_dbg(tegra->xhci, "rctrl_val = 0x%x, tctrl_val = 0x%x\n",
					utmip_rctrl_val, utmip_tctrl_val);

		/* XUSB_PADCTL_USB2_BIAS_PAD_CTL_0_0::PD = 1 and
		 * XUSB_PADCTL_USB2_BIAS_PAD_CTL_0_0::PD_TRK = 1
		 */
		reg = padctl_readl(tegra, padregs->usb2_bias_pad_ctlY_0[0]);
		reg |= (1 << 13);
		padctl_writel(tegra, reg, padregs->usb2_bias_pad_ctlY_0[0]);

		/* Program these values into PMC regiseter and program the
		 * PMC override.
		 */
		reg = PMC_TCTRL_VAL(utmip_tctrl_val) |
				PMC_RCTRL_VAL(utmip_rctrl_val);
		tegra_usb_pmc_reg_update(PMC_UTMIP_TERM_PAD_CFG,
					0xffffffff, reg);
		reg = UTMIP_RCTRL_USE_PMC_P2 | UTMIP_TCTRL_USE_PMC_P2;
		tegra_usb_pmc_reg_update(PMC_SLEEP_CFG(2), reg, reg);
	} else {
		/* Use common PMC API to use SNPS register space */
		utmi_phy_set_snps_trking_data();
	}
}

/* Called when exiting elpg */
static void tegra_init_otg_port(struct tegra_xhci_hcd *tegra)
{
	if (XUSB_DEVICE_ID_T210 != tegra->device_id)
		return;

	/* perform reset_sspi WAR if we were in otg_host mode with
	 * only otg cable connected during lp0 entry and now we are in
	 * lp0 exit path by SS device connect behind otg cable.
	 * WAR is required as usb2_id register gets default value
	 * (ID_OVRD == FLOAT) soon after lp0 exit.
	 */
	if (tegra->lp0_exit && tegra->transceiver &&
		(tegra->transceiver->state == OTG_STATE_A_WAIT_BCON) &&
		tegra->otg_port_owned &&
		!tegra->otg_port_ownership_changed)
		tegra_xhci_handle_otg_port_change(tegra);

	if (!tegra->otg_port_ownership_changed)
		/* Nop if we just got ownership */
		return;

	if (!(tegra->bdata->otg_portmap & 0xff))
		/* Nop if no ss otg ports */
		return;

	if (tegra->transceiver && tegra->transceiver->set_vbus)
		tegra->transceiver->set_vbus(tegra->transceiver,
		tegra->otg_port_owned ? 1 : 0);

	tegra->otg_port_ownership_changed = false;
}
/* Host ELPG Exit triggered by PADCTL irq */
/**
 * tegra_xhci_host_partition_elpg_exit - bring XUSBC partition out from elpg
 *
 * This function must be called with tegra->sync_lock acquired.
 *
 * @tegra: xhci controller context
 * @return 0 for success, or error numbers
 */
static int
tegra_xhci_host_partition_elpg_exit(struct tegra_xhci_hcd *tegra)
{
	const struct tegra_xusb_padctl_regs *padregs = tegra->padregs;
	struct xhci_hcd *xhci = tegra->xhci;
	int ret = 0;
	int pad;
	int partition_id;
	u32 usbcmd;

	must_have_sync_lock(tegra);

	/* let system call resume() routine first if in lp0 */
	if (!tegra->hc_in_elpg || tegra->system_in_lp0)
		return 0;

#ifdef CONFIG_ARCH_TEGRA_21x_SOC
	utmi_phy_pad_enable(tegra->prod_list);
	utmi_phy_iddq_override(false);
#endif

	clk_enable(tegra->emc_clk);
	if (tegra->soc_config->quirks & TEGRA_XUSB_USE_HS_SRC_CLOCK2)
		clk_enable(tegra->pll_re_vco_clk);

	if (tegra->lp0_exit) {
		u32 reg, oc_bits = 0;

		/* Issue is only applicable for T114 */
		if (XUSB_DEVICE_ID_T114 == tegra->device_id)
			tegra_xhci_war_for_tctrl_rctrl(tegra);
		tegra_xhci_padctl_portmap_and_caps(tegra);

		for_each_enabled_utmi_pad(pad, tegra) {
			if (tegra->bdata->vbus_en_oc[pad].type == VBUS_EN_OC)
				padctl_enable_usb_vbus(tegra, pad);
			else {
				if (tegra->bdata->portmap &
						BIT(XUSB_UTMI_INDEX + pad))
					oc_bits |= OC_DETECTED_VBUS_PAD(
					tegra->bdata->vbus_en_oc[pad].pin);
			}
		}
		reg = padctl_readl(tegra, padregs->oc_det_0);
		xhci_dbg(xhci, "%s: oc_det_0=0x%x\n", __func__, reg);
		if (reg & oc_bits) {
			xhci_info(xhci, "Over current detected. Clearing...\n");
			padctl_writel(tegra, reg, padregs->oc_det_0);

			usleep_range(100, 200);

			reg = padctl_readl(tegra, padregs->oc_det_0);
			if (reg & oc_bits)
				xhci_info(xhci, "Over current still present\n");
		}
	}

	partition_id = tegra_pd_get_powergate_id(tegra_xusbc_pd);
	if (partition_id < 0)
		return -EINVAL;
	/* Clear FLUSH_ENABLE of MC client */
	tegra_powergate_mc_flush_done(partition_id);

	/* set port ownership back to xusb */
	tegra_xhci_release_port_ownership(tegra, false);

	/*
	 * PWR_UNGATE Host partition. XUSBC
	 * tegra_unpowergate_partition also does partition reset deassert
	 */
	ret = tegra_unpowergate_partition_with_clk_on(partition_id);
	if (ret) {
		xhci_err(xhci, "%s: could not unpowergate xusbc partition %d\n",
			__func__, ret);
		goto out;
	}

	/* Step 6.1: IPFS and XUSB BAR initialization */
	tegra_xhci_cfg(tegra);

	/* Step 6.2: IPFS and XUSB related restore */
	tegra_xhci_restore_ctx(tegra);

	/* Step 8: xhci spec related ctx restore
	 * will be done in xhci_resume().Do it here.
	 */

	tegra_xhci_ss_partition_elpg_exit(tegra);

	/* Change SS clock source to HSIC_480 and set ss_src_clk at 120MHz */
	if (clk_get_rate(tegra->ss_src_clk) == 12000000) {
		clk_set_rate(tegra->ss_src_clk,  3000 * 1000);
		clk_set_parent(tegra->ss_src_clk, tegra->pll_u_480M);
	}

	/* clear ovrd bits */
	tegra_xhci_rx_idle_mode_override(tegra, false);

	/* Load firmware */
	xhci_dbg(xhci, "%s: elpg_exit: loading firmware from pmc.\nss (p1=0x%x, p2=0x%x, p3=0x%x), hs (p1=0x%x, p2=0x%x, p3=0x%x),\nfs (p1=0x%x, p2=0x%x, p3=0x%x)\n",
			__func__,
			csb_read(tegra, XUSB_FALC_SS_PVTPORTSC1),
			csb_read(tegra, XUSB_FALC_SS_PVTPORTSC2),
			csb_read(tegra, XUSB_FALC_SS_PVTPORTSC3),
			csb_read(tegra, XUSB_FALC_HS_PVTPORTSC1),
			csb_read(tegra, XUSB_FALC_HS_PVTPORTSC2),
			csb_read(tegra, XUSB_FALC_HS_PVTPORTSC3),
			csb_read(tegra, XUSB_FALC_FS_PVTPORTSC1),
			csb_read(tegra, XUSB_FALC_FS_PVTPORTSC2),
			csb_read(tegra, XUSB_FALC_FS_PVTPORTSC3));
	debug_print_portsc(xhci);

	tegra_xhci_enable_fw_message(tegra);
	ret = load_firmware(tegra, false /* EPLG exit, do not reset ARU */);
	if (ret < 0) {
		xhci_err(xhci, "%s: failed to load firmware %d\n",
			__func__, ret);
		goto out;
	}
	for_each_enabled_hsic_pad(pad, tegra)
		hsic_pad_pupd_set(tegra, pad, PUPD_IDLE);

	pmc_disable_bus_ctrl(tegra);

	tegra->hc_in_elpg = false;
	ret = xhci_resume(tegra->xhci, 0);
	if (ret) {
		xhci_err(xhci, "%s: could not resume right %d\n",
				__func__, ret);
		goto out;
	}

	usbcmd = xhci_readl(xhci, &xhci->op_regs->command);
	usbcmd |= CMD_EIE;
	xhci_writel(xhci, usbcmd, &xhci->op_regs->command);

	update_remote_wakeup_ports(tegra);

	if (tegra->hs_wake_event)
		tegra->hs_wake_event = false;

	if (tegra->host_resume_req)
		tegra->host_resume_req = false;

	xhci_info(xhci, "elpg_exit: completed: lp0/elpg time=%d msec\n",
		jiffies_to_msecs(jiffies - tegra->last_jiffies));

	tegra->host_pwr_gated = false;

	tegra_init_otg_port(tegra);
out:
	return ret;
}

static void tegra_xotg_vbus_work(struct work_struct *work)
{
	struct tegra_xhci_hcd *tegra = container_of(work, struct tegra_xhci_hcd,
		xotg_vbus_work);

	if (tegra->transceiver && tegra->transceiver->set_vbus)
		tegra->transceiver->set_vbus(tegra->transceiver,
				tegra->otg_port_owned ? 1 : 0);
}

static void host_partition_elpg_exit_work(struct work_struct *work)
{
	struct tegra_xhci_hcd *tegra = container_of(work, struct tegra_xhci_hcd,
		host_elpg_exit_work);

	mutex_lock(&tegra->sync_lock);
	tegra_xhci_host_partition_elpg_exit(tegra);
	mutex_unlock(&tegra->sync_lock);
}

/* Mailbox handling function. This function handles requests
 * from firmware and communicates with clock and powergating
 * module to alter clock rates and to power gate/ungate xusb
 * partitions.
 *
 * Following is the structure of mailbox messages.
 * bit 31:28 - msg type
 * bits 27:0 - mbox data
 * FIXME:  Check if we can just call clock functions like below
 * or should we schedule it for calling later ?
 */

static void
tegra_xhci_process_mbox_message(struct work_struct *work)
{
	u32 sw_resp = 0, cmd, data_in, fw_msg;
	int ret = 0;
	struct tegra_xhci_hcd *tegra = container_of(work, struct tegra_xhci_hcd,
					mbox_work);
	struct xhci_hcd *xhci = tegra->xhci;
	int pad, port;
	unsigned long ports;
	enum MBOX_CMD_TYPE response;

	mutex_lock(&tegra->mbox_lock);

	/* get the mbox message from firmware */
	fw_msg = readl(tegra->fpci_base + XUSB_CFG_ARU_MBOX_DATA_OUT);

	/* TODO: check data_in for the corresponding SW-initiated mbox */
	data_in = readl(tegra->fpci_base + XUSB_CFG_ARU_MBOX_DATA_IN);
	dev_dbg(&tegra->pdev->dev, "%s data_in 0x%x\n", __func__, data_in);

	/* get cmd type and cmd data */
	tegra->cmd_type = (fw_msg >> CMD_TYPE_SHIFT) & CMD_TYPE_MASK;
	tegra->cmd_data = (fw_msg >> CMD_DATA_SHIFT) & CMD_DATA_MASK;

	/* decode the message and make appropriate requests to
	 * clock or powergating module.
	 */

	switch (tegra->cmd_type) {
	case MBOX_CMD_INC_FALC_CLOCK:
	case MBOX_CMD_DEC_FALC_CLOCK:
		ret = tegra_xusb_request_clk_rate(
				tegra,
				tegra->falc_clk,
				tegra->cmd_data,
				&sw_resp);
		if (ret)
			xhci_err(xhci, "%s: could not set required falc rate\n",
				__func__);
		goto send_sw_response;
	case MBOX_CMD_INC_SSPI_CLOCK:
	case MBOX_CMD_DEC_SSPI_CLOCK:
		if (XUSB_IS_T210(tegra)) {
			/*
			 * TODO: temporarily skip SSPI clock changing for T210.
			 * Hardware group will provide proper sequence.
			 */
			pr_info("%s: ignore SSPI clock request.\n", __func__);
			sw_resp = CMD_DATA(tegra->cmd_data) |
						CMD_TYPE(MBOX_CMD_ACK);
			goto send_sw_response;

		}

		ret = tegra_xusb_request_clk_rate(
				tegra,
				tegra->ss_src_clk,
				tegra->cmd_data,
				&sw_resp);
		if (ret)
			xhci_err(xhci, "%s: could not set required ss rate.\n",
				__func__);
		goto send_sw_response;

	case MBOX_CMD_SET_BW:
		/* fw sends BW request in MByte/sec */
		mutex_lock(&tegra->sync_lock);
		tegra_xusb_set_bw(tegra, tegra->cmd_data << 10);
		mutex_unlock(&tegra->sync_lock);
		break;

	case MBOX_CMD_SAVE_DFE_CTLE_CTX:
		tegra_xhci_save_dfe_context(tegra, tegra->cmd_data);
		tegra_xhci_save_ctle_context(tegra, tegra->cmd_data);
		sw_resp = CMD_DATA(tegra->cmd_data) | CMD_TYPE(MBOX_CMD_ACK);
		goto send_sw_response;

	case MBOX_CMD_STAR_HSIC_IDLE:
		ports = tegra->cmd_data;
		for_each_set_bit(port, &ports, BITS_PER_LONG) {
			pad = port_to_hsic_pad(port - 1);
			mutex_lock(&tegra->sync_lock);
			ret = hsic_pad_pupd_set(tegra, pad, PUPD_IDLE);
			mutex_unlock(&tegra->sync_lock);
			if (ret)
				break;
		}

		sw_resp = CMD_DATA(tegra->cmd_data);
		if (!ret)
			sw_resp |= CMD_TYPE(MBOX_CMD_ACK);
		else
			sw_resp |= CMD_TYPE(MBOX_CMD_NACK);

		goto send_sw_response;

	case MBOX_CMD_STOP_HSIC_IDLE:
		ports = tegra->cmd_data;
		for_each_set_bit(port, &ports, BITS_PER_LONG) {
			pad = port_to_hsic_pad(port - 1);
			mutex_lock(&tegra->sync_lock);
			ret = hsic_pad_pupd_set(tegra, pad, PUPD_DISABLE);
			mutex_unlock(&tegra->sync_lock);
			if (ret)
				break;
		}

		sw_resp = CMD_DATA(tegra->cmd_data);
		if (!ret)
			sw_resp |= CMD_TYPE(MBOX_CMD_ACK);
		else
			sw_resp |= CMD_TYPE(MBOX_CMD_NACK);
		goto send_sw_response;

#if defined(CONFIG_ARCH_TEGRA_21x_SOC)
	case MBOX_CMD_DISABLE_SS_LFPS_DETECTION:
		ports = tegra->cmd_data;
		for_each_set_bit(port, &ports, BITS_PER_LONG) {
			t210_disable_lfps_detector(tegra, port - 1);

			/*
			 * Add this delay to increase stability of
			 * directing U3.
			 */
			usleep_range(500, 1000);
		}
		sw_resp = CMD_DATA(tegra->cmd_data);
		sw_resp |= CMD_TYPE(MBOX_CMD_ACK);
		goto send_sw_response;

	case MBOX_CMD_ENABLE_SS_LFPS_DETECTION:
		ports = tegra->cmd_data;
		for_each_set_bit(port, &ports, BITS_PER_LONG) {
			t210_enable_lfps_detector(tegra, port - 1);
		}
		sw_resp = CMD_DATA(tegra->cmd_data);
		sw_resp |= CMD_TYPE(MBOX_CMD_ACK);
		goto send_sw_response;
#endif

	case MBOX_CMD_ACK:
	case MBOX_CMD_NACK:
		if (tegra->cmd_type == MBOX_CMD_ACK)
			xhci_dbg(xhci, "%s firmware responds ACK\n", __func__);
		else
			xhci_warn(xhci, "%s firmware responds NACK\n",
					__func__);

		/* inform processes that needs FW ACK */
		tegra->fw_ack = tegra->cmd_type;
		wake_up_interruptible(&tegra->fw_ack_wq);
		break;
	default:
		xhci_err(xhci, "%s: invalid cmdtype %d\n",
				__func__, tegra->cmd_type);
	}

	/* clear MBOX_SMI_INT_EN bit */
	cmd = readl(tegra->fpci_base + XUSB_CFG_ARU_MBOX_CMD);
	cmd &= ~MBOX_SMI_INT_EN;
	writel(cmd, tegra->fpci_base + XUSB_CFG_ARU_MBOX_CMD);

	/* clear mailbox ownership */
	writel(0, tegra->fpci_base + XUSB_CFG_ARU_MBOX_OWNER);

	mutex_unlock(&tegra->mbox_lock);
	return;

send_sw_response:
	response = (sw_resp >> CMD_TYPE_SHIFT) & CMD_TYPE_MASK;
	if (response == MBOX_CMD_NACK)
		xhci_warn(xhci, "%s respond fw message 0x%x with NACK\n",
			__func__, fw_msg);
	else if (response == MBOX_CMD_ACK)
		xhci_dbg(xhci, "%s respond fw message 0x%x with ACK\n",
			__func__, fw_msg);
	else
		xhci_err(xhci, "%s respond fw message 0x%x with %d\n",
		__func__, fw_msg, response);

	writel(sw_resp, tegra->fpci_base + XUSB_CFG_ARU_MBOX_DATA_IN);
	cmd = readl(tegra->fpci_base + XUSB_CFG_ARU_MBOX_CMD);
	cmd |= MBOX_INT_EN | MBOX_FALC_INT_EN;
	writel(cmd, tegra->fpci_base + XUSB_CFG_ARU_MBOX_CMD);

	mutex_unlock(&tegra->mbox_lock);
}

static irqreturn_t pmc_usb_phy_wake_isr(int irq, void *data)
{
	struct tegra_xhci_hcd *tegra = (struct tegra_xhci_hcd *) data;
	struct xhci_hcd *xhci = tegra->xhci;

	xhci_dbg(xhci, "%s irq %d", __func__, irq);
	return IRQ_HANDLED;
}

static void tegra_xhci_handle_oc_condition(struct work_struct *work)
{
	struct tegra_xhci_hcd *tegra = container_of(work, struct tegra_xhci_hcd,
			oc_handling_work);
	struct xhci_hcd *xhci = tegra->xhci;
	struct tegra_xusb_board_data *bdata = tegra->bdata;
	const struct tegra_xusb_padctl_regs *padregs = tegra->padregs;
	const struct tegra_xusb_regulator_name *supply =
					&tegra->soc_config->supply;
	int pad, err;
	u32 reg;

	mutex_lock(&tegra->sync_lock);
	reg = padctl_readl(tegra, padregs->oc_det_0);
	for_each_enabled_utmi_pad(pad, tegra) {
		if (reg & OC_DETECTED_VBUS_PAD(bdata->vbus_en_oc[pad].pin)) {
			err = regulator_disable(
					tegra->xusb_utmi_vbus_regs[pad]);
			if (err < 0) {
				xhci_err(xhci, "%s: regulator disable failed: %d\n",
						supply->utmi_vbuses[pad], err);
			}

			padctl_enable_usb_vbus(tegra, pad);
		}
	}
	mutex_unlock(&tegra->sync_lock);
}

static irqreturn_t tegra_xhci_padctl_irq(int irq, void *ptrdev)
{
	struct tegra_xhci_hcd *tegra = (struct tegra_xhci_hcd *) ptrdev;
	struct xhci_hcd *xhci = tegra->xhci;
	const struct tegra_xusb_padctl_regs *padregs = tegra->padregs;
	u32 elpg_program0 = 0, oc_det = 0;
	int pad;
	bool schedule_oc_work = false;
	int host_ports = get_host_controlled_ports(tegra);
	struct tegra_xusb_board_data *bdata = tegra->bdata;

	spin_lock(&tegra->lock);

	tegra->last_jiffies = jiffies;

	oc_det = padctl_readl(tegra, padregs->oc_det_0);
	xhci_dbg(xhci, "%s: OC_DET_0 0x%x\n", __func__, oc_det);
	for_each_enabled_utmi_pad(pad, tegra) {
		if (oc_det & OC_DETECTED_VBUS_PAD(bdata->vbus_en_oc[pad].pin)) {
			xhci_dbg(xhci, "%s: OC detected pad %d\n",
						__func__, pad);
			oc_det &= ~OC_DETECTED_VBUS_PAD_MASK;
			oc_det &= ~OC_DETECTED_INTR_ENABLE_VBUS_PAD(
						bdata->vbus_en_oc[pad].pin);
			schedule_oc_work = true;
		}
	}

	if (schedule_oc_work) {
		padctl_writel(tegra, oc_det, padregs->oc_det_0);
		oc_det = padctl_readl(tegra, padregs->oc_det_0);
		xhci_dbg(xhci, "%s: OC_DET_0 scheduled 0x%x\n",
						__func__, oc_det);
		schedule_work(&tegra->oc_handling_work);
		spin_unlock(&tegra->lock);
		return IRQ_HANDLED;
	}

	/* Check the intr cause. Could be  USB2 or HSIC or SS wake events */
	elpg_program0 = tegra_usb_pad_reg_read(padregs->elpg_program_0);

	/* filter out all wake events */
	elpg_program0 &= get_wake_sources_for_host_controlled_ports(host_ports);

	/* Clear the interrupt cause if it's for host.
	 * We already read the intr status.
	 */
	if (elpg_program0) {
		tegra_xhci_ss_wake_on_interrupts(host_ports, false);
		tegra_xhci_hs_wake_on_interrupts(host_ports, false);
	} else {
		xhci_info(xhci, "padctl interrupt is not for xhci\n");
		spin_unlock(&tegra->lock);
		return IRQ_NONE;
	}

	xhci_dbg(xhci, "%s: elpg_program0 = %x\n", __func__, elpg_program0);
	xhci_dbg(xhci, "%s: PMC REGISTER = %x\n", __func__,
		tegra_usb_pmc_reg_read(PMC_UTMIP_UHSIC_SLEEP_CFG_0));
	xhci_dbg(xhci, "%s: OC_DET Register = %x\n",
		__func__, readl(tegra->padctl_base + padregs->oc_det_0));
	xhci_dbg(xhci, "%s: usb2_bchrg_otgpad0_ctl0_0 Register = %x\n",
		__func__,
		padctl_readl(tegra, padregs->usb2_bchrg_otgpadX_ctlY_0[0][0]));
	xhci_dbg(xhci, "%s: usb2_bchrg_otgpad1_ctl0_0 Register = %x\n",
		__func__,
		padctl_readl(tegra, padregs->usb2_bchrg_otgpadX_ctlY_0[1][0]));
	xhci_dbg(xhci, "%s: usb2_bchrg_bias_pad_0 Register = %x\n",
		__func__,
		padctl_readl(tegra, padregs->usb2_bchrg_bias_pad_0));

	if (elpg_program0 & (SS_PORT0_WAKEUP_EVENT | SS_PORT1_WAKEUP_EVENT
			| SS_PORT2_WAKEUP_EVENT | SS_PORT3_WAKEUP_EVENT))
		tegra->ss_wake_event = true;
	else if (elpg_program0 & (USB2_PORT0_WAKEUP_EVENT |
			USB2_PORT1_WAKEUP_EVENT |
			USB2_PORT2_WAKEUP_EVENT |
			USB2_PORT3_WAKEUP_EVENT |
			USB2_HSIC_PORT0_WAKEUP_EVENT |
			USB2_HSIC_PORT1_WAKEUP_EVENT))
		tegra->hs_wake_event = true;

	if (tegra->ss_wake_event || tegra->hs_wake_event) {
		xhci_dbg(xhci, "[%s] schedule host_elpg_exit_work\n",
			__func__);
		schedule_work(&tegra->host_elpg_exit_work);
	}
	spin_unlock(&tegra->lock);
	return IRQ_HANDLED;
}

static irqreturn_t tegra_xhci_smi_irq(int irq, void *ptrdev)
{
	struct tegra_xhci_hcd *tegra = (struct tegra_xhci_hcd *) ptrdev;
	u32 temp;

	spin_lock(&tegra->lock);

	/* clear the mbox intr status 1st thing. Other
	 * bits are W1C bits, so just write to SMI bit.
	 */

	temp = readl(tegra->fpci_base + XUSB_CFG_ARU_SMI_INTR);
	writel(temp, tegra->fpci_base + XUSB_CFG_ARU_SMI_INTR);

	xhci_dbg(tegra->xhci, "SMI INTR status 0x%x\n", temp);
	if (temp & SMI_INTR_STATUS_FW_REINIT)
		xhci_err(tegra->xhci, "Firmware reinit.\n");
	if (temp & SMI_INTR_STATUS_MBOX)
		queue_work(tegra->mbox_wq, &tegra->mbox_work);

	spin_unlock(&tegra->lock);
	return IRQ_HANDLED;
}

static void tegra_xhci_plat_quirks(struct device *dev, struct xhci_hcd *xhci)
{
	/*
	 * As of now platform drivers don't provide MSI support so we ensure
	 * here that the generic code does not try to make a pci_dev from our
	 * dev struct in order to setup MSI
	 */
	xhci->quirks |= XHCI_PLAT;
	xhci->quirks &= ~XHCI_SPURIOUS_REBOOT;
	xhci->quirks |= XHCI_INTEL_HOST;
}

/* called during probe() after chip reset completes */
static int xhci_plat_setup(struct usb_hcd *hcd)
{
	return xhci_gen_setup(hcd, tegra_xhci_plat_quirks);
}

static int tegra_xhci_request_mem_region(struct platform_device *pdev,
	int num, void __iomem **region)
{
	struct resource	*res;
	void __iomem *mem;

	res = platform_get_resource(pdev, IORESOURCE_MEM, num);
	if (!res) {
		dev_err(&pdev->dev, "memory resource %d doesn't exist\n", num);
		return -ENODEV;
	}

	mem = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(mem)) {
		dev_err(&pdev->dev, "failed to ioremap for %d. Error %ld\n",
				num, PTR_ERR(mem));
		return -EFAULT;
	}
	*region = mem;

	return 0;
}

static int tegra_xhci_request_irq(struct platform_device *pdev,
	int num, irq_handler_t handler, unsigned long irqflags,
	const char *devname, int *irq_no)
{
	int ret;
	struct tegra_xhci_hcd *tegra = platform_get_drvdata(pdev);
	struct resource	*res;

	res = platform_get_resource(pdev, IORESOURCE_IRQ, num);
	if (!res) {
		dev_err(&pdev->dev, "irq resource %d doesn't exist\n", num);
		return -ENODEV;
	}

	ret = devm_request_irq(&pdev->dev, res->start, handler, irqflags,
			devname, tegra);
	if (ret != 0) {
		dev_err(&pdev->dev,
			"failed to request_irq for %s (irq %d), error = %d\n",
			devname, (int)res->start, ret);
		return ret;
	}
	*irq_no = res->start;

	return 0;
}

#ifdef CONFIG_PM
static int tegra_xhci_bus_suspend(struct usb_hcd *hcd)
{
	struct tegra_xhci_hcd *tegra = hcd_to_tegra_xhci(hcd);
	struct xhci_hcd *xhci = hcd_to_xhci(hcd);
	int err = 0;
	u32 host_ports = get_host_controlled_ports(tegra);
	unsigned long flags;
	int num_ports = HCS_MAX_PORTS(xhci->hcs_params1);
	u32 usbcmd;
	int i;
	u32 portsc;

	mutex_lock(&tegra->sync_lock);

	if (!tegra->init_done) {
		xhci_warn(xhci, "%s: xhci probe not done\n",
				__func__);
		mutex_unlock(&tegra->sync_lock);
		return -EBUSY;
	}

	if (xhci->shared_hcd == hcd) {
		tegra->usb3_rh_suspend = true;
		xhci_dbg(xhci, "%s: usb3 root hub\n", __func__);
	} else if (xhci->main_hcd == hcd) {
		tegra->usb2_rh_suspend = true;
		xhci_dbg(xhci, "%s: usb2 root hub\n", __func__);
	}

	WARN_ON(tegra->hc_in_elpg);

	/* suspend xhci bus. This will also set remote mask */
	err = xhci_bus_suspend(hcd);
	if (err) {
		xhci_err(xhci, "%s: xhci_bus_suspend failed %d\n",
				__func__, err);
		goto xhci_bus_suspend_failed;
	}

	if (!(tegra->usb2_rh_suspend && tegra->usb3_rh_suspend))
		goto done; /* one of the root hubs is still working */

	spin_lock_irqsave(&xhci->lock, flags);
	usbcmd = xhci_readl(xhci, &xhci->op_regs->command);
	usbcmd &= ~CMD_EIE;
	xhci_writel(xhci, usbcmd, &xhci->op_regs->command);
	for (i = 0; i < num_ports; i++) {
		portsc = xhci_read_portsc(xhci, i);
		if ((portsc & PORT_PE) && (portsc & PORT_PLS_MASK) != XDEV_U3) {
			usbcmd = xhci_readl(xhci, &xhci->op_regs->command);
			usbcmd |= CMD_EIE;
			xhci_writel(xhci, usbcmd, &xhci->op_regs->command);

			spin_unlock_irqrestore(&xhci->lock, flags);

			xhci_info(xhci, "%s: port not in suspended state\n",
					__func__);
			err = -EBUSY;
			goto xhci_bus_suspend_failed;
		}
	}
	spin_unlock_irqrestore(&xhci->lock, flags);

	spin_lock_irqsave(&tegra->lock, flags);
	tegra->hc_in_elpg = true;
	spin_unlock_irqrestore(&tegra->lock, flags);

	WARN_ON(tegra->ss_pwr_gated && tegra->host_pwr_gated);

	/* save xhci spec ctx. Already done by xhci_suspend */
	/* FIXME: do we need wake interrupts? */
	err = xhci_suspend(tegra->xhci, true);
	if (err) {
		xhci_err(xhci, "%s: xhci_suspend failed %d\n", __func__, err);
		goto xhci_suspend_failed;
	}

	/* Powergate host. Include ss power gate if not already done */
	err = tegra_xhci_host_elpg_entry(tegra);
	if (err) {
		xhci_err(xhci, "%s: unable to perform elpg entry %d\n",
				__func__, err);
		goto tegra_xhci_host_elpg_entry_failed;
	}

	/* At this point,ensure ss/hs intr enables are always on */
	tegra_xhci_ss_wake_on_interrupts(host_ports, true);
	tegra_xhci_hs_wake_on_interrupts(host_ports, true);

	/* In ELPG, firmware log context is gone. Rewind shared log buffer. */
	if (test_bit(FW_LOG_CONTEXT_VALID, &tegra->log.flags)) {
		if (fw_log_wait_empty_timeout(tegra, 100))
			xhci_warn(xhci, "%s still has logs\n", __func__);
		tegra->log.dequeue = tegra->log.virt_addr;
		tegra->log.seq = 0;
	}

done:
	/* pads are disabled only if usb2 root hub in xusb is idle */
	/* pads will actually be disabled only when all usb2 ports are idle */
#ifndef CONFIG_ARCH_TEGRA_21x_SOC
	if (xhci->main_hcd == hcd) {
		utmi_phy_pad_disable();
		utmi_phy_iddq_override(true);
	}
#endif
	mutex_unlock(&tegra->sync_lock);
	return 0;

tegra_xhci_host_elpg_entry_failed:

xhci_suspend_failed:
	tegra->hc_in_elpg = false;
xhci_bus_suspend_failed:
	if (xhci->shared_hcd == hcd)
		tegra->usb3_rh_suspend = false;
	else if (xhci->main_hcd == hcd)
		tegra->usb2_rh_suspend = false;

	mutex_unlock(&tegra->sync_lock);
	return err;
}

/* First, USB2HCD and then USB3HCD resume will be called */
static int tegra_xhci_bus_resume(struct usb_hcd *hcd)
{
	struct tegra_xhci_hcd *tegra = hcd_to_tegra_xhci(hcd);
	struct xhci_hcd *xhci = hcd_to_xhci(hcd);
	int usb2_utmi_port_start, usb2_utmi_port_end;
	int err = 0, pad, port;
	u32 reg;

	mutex_lock(&tegra->sync_lock);

	tegra->host_resume_req = true;

	if (xhci->shared_hcd == hcd)
		xhci_dbg(xhci, "%s: usb3 root hub\n", __func__);
	else if (xhci->main_hcd == hcd)
		xhci_dbg(xhci, "%s: usb2 root hub\n", __func__);

	/* pads are disabled only if usb2 root hub in xusb is idle */
	/* pads will actually be disabled only when all usb2 ports are idle */
#ifndef CONFIG_ARCH_TEGRA_21x_SOC
	if (xhci->main_hcd == hcd && tegra->usb2_rh_suspend) {
		utmi_phy_pad_enable();
		utmi_phy_iddq_override(false);
	}
#endif
	if (tegra->usb2_rh_suspend && tegra->usb3_rh_suspend) {
		if (tegra->ss_pwr_gated && tegra->host_pwr_gated)
			tegra_xhci_host_partition_elpg_exit(tegra);

		if (tegra->lp0_exit) {
			usb2_utmi_port_start = XUSB_SS_PORT_COUNT;
			usb2_utmi_port_end = XUSB_SS_PORT_COUNT
						+ XUSB_UTMI_COUNT - 1;

			for (port = usb2_utmi_port_start;
					port <= usb2_utmi_port_end; port++) {
				reg = xhci_read_portsc(xhci, port);
				if (!(reg & PORT_CONNECT)) {
					pad = port - XUSB_SS_PORT_COUNT;
					set_port_cdp(tegra, true, pad);
				}
			}
			tegra->lp0_exit = false;
		}
	}

	 /* handle remote wakeup before resuming bus */
	wait_remote_wakeup_ports(hcd);

	err = xhci_bus_resume(hcd);
	if (err) {
		xhci_err(xhci, "%s: xhci_bus_resume failed %d\n",
				__func__, err);
		goto xhci_bus_resume_failed;
	}

	if (xhci->shared_hcd == hcd)
		tegra->usb3_rh_suspend = false;
	else if (xhci->main_hcd == hcd)
		tegra->usb2_rh_suspend = false;

	mutex_unlock(&tegra->sync_lock);
	return 0;

xhci_bus_resume_failed:
	/* TODO: reverse elpg? */
	mutex_unlock(&tegra->sync_lock);
	return err;
}
#endif

#ifdef CONFIG_TEGRA_XHCI_ENABLE_CDP_PORT
static void set_port_cdp(struct tegra_xhci_hcd *tegra, bool enable, int pad)
{
	const struct tegra_xusb_padctl_regs *padregs = tegra->padregs;
	u32 __iomem bchrg_otgpad_reg =
				padregs->usb2_bchrg_otgpadX_ctlY_0[pad][0];
	u32 __iomem otg_pad_reg = padregs->usb2_otg_padX_ctlY_0[pad][0];
	long val;

	if (enable) {
		val = tegra_usb_pad_reg_read(bchrg_otgpad_reg);
		val &= ~(PD_CHG);
		tegra_usb_pad_reg_write(bchrg_otgpad_reg, val);
		val = tegra_usb_pad_reg_read(otg_pad_reg);
		val |= (PD2);
		tegra_usb_pad_reg_write(otg_pad_reg, val);
		val = tegra_usb_pad_reg_read(bchrg_otgpad_reg);
		val |= (ON_SRC_EN);
		tegra_usb_pad_reg_write(bchrg_otgpad_reg, val);
	} else {
		val = tegra_usb_pad_reg_read(bchrg_otgpad_reg);
		val |= (PD_CHG);
		tegra_usb_pad_reg_write(bchrg_otgpad_reg, val);
		val = tegra_usb_pad_reg_read(otg_pad_reg);
		val &= ~(PD2);
		tegra_usb_pad_reg_write(otg_pad_reg, val);
		val = tegra_usb_pad_reg_read(bchrg_otgpad_reg);
		val &= ~(ON_SRC_EN);
		tegra_usb_pad_reg_write(bchrg_otgpad_reg, val);
	}
}

void tegra_xhci_free_dev(struct usb_hcd *hcd, struct usb_device *udev)
{
	struct tegra_xhci_hcd *tegra = hcd_to_tegra_xhci(hcd);
	int pad;

	pad = udev->portnum - 1;
	xhci_free_dev(hcd, udev);
	set_port_cdp(tegra, true, pad);
}

int tegra_xhci_alloc_dev(struct usb_hcd *hcd, struct usb_device *udev)
{
	struct xhci_hcd *xhci = hcd_to_xhci(hcd);
	struct tegra_xhci_hcd *tegra = hcd_to_tegra_xhci(hcd);
	int usb2_utmi_port_start, usb2_utmi_port_end;
	int port; int pad;
	u32 reg;

	usb2_utmi_port_start = XUSB_SS_PORT_COUNT;
	usb2_utmi_port_end = XUSB_SS_PORT_COUNT + XUSB_UTMI_COUNT - 1;

	for (port = usb2_utmi_port_start; port <= usb2_utmi_port_end; port++) {
		reg = xhci_read_portsc(xhci, port);
		pad = port - XUSB_SS_PORT_COUNT;
		if (reg & PORT_CONNECT)
			set_port_cdp(tegra, false, pad);
	}

	return xhci_alloc_dev(hcd, udev);
}

#else
static void set_port_cdp(struct tegra_xhci_hcd *tegra, bool enable, int pad)
{
}

static void tegra_xhci_free_dev(struct usb_hcd *hcd, struct usb_device *udev)
{
		xhci_free_dev(hcd, udev);
}

static int tegra_xhci_alloc_dev(struct usb_hcd *hcd, struct usb_device *udev)
{
	return xhci_alloc_dev(hcd, udev);
}
#endif

static irqreturn_t tegra_xhci_irq(struct usb_hcd *hcd)
{
	struct tegra_xhci_hcd *tegra = hcd_to_tegra_xhci(hcd);
	struct xhci_hcd *xhci = hcd_to_xhci(hcd);
	irqreturn_t iret = IRQ_HANDLED;
	u32 status;

	spin_lock(&tegra->lock);
	if (tegra->hc_in_elpg) {
		spin_lock(&xhci->lock);
		if (HCD_HW_ACCESSIBLE(hcd)) {
			status = xhci_readl(xhci, &xhci->op_regs->status);
			status |= STS_EINT;
			xhci_writel(xhci, status, &xhci->op_regs->status);
		}
		xhci_dbg(xhci, "%s: schedule host_elpg_exit_work\n",
				__func__);
		schedule_work(&tegra->host_elpg_exit_work);
		spin_unlock(&xhci->lock);
	} else
		iret = xhci_irq(hcd);
	spin_unlock(&tegra->lock);

	if (test_bit(FW_LOG_CONTEXT_VALID, &tegra->log.flags))
		wake_up_interruptible(&tegra->log.intr_wait);

	return iret;
}

static int tegra_xhci_urb_enqueue(struct usb_hcd *hcd, struct urb *urb,
			gfp_t mem_flags)
{
	int xfertype;
	struct tegra_xhci_hcd *tegra = hcd_to_tegra_xhci(hcd);

	xfertype = usb_endpoint_type(&urb->ep->desc);
	switch (xfertype) {
	case USB_ENDPOINT_XFER_ISOC:
	case USB_ENDPOINT_XFER_BULK:
		if (urb->transfer_buffer_length > tegra->boost_cpu_trigger)
			tegra_xusb_boost_cpu_freq(tegra);
		break;
	case USB_ENDPOINT_XFER_INT:
	case USB_ENDPOINT_XFER_CONTROL:
	default:
		/* Do nothing special here */
		break;
	}
	return xhci_urb_enqueue(hcd, urb, mem_flags);
}

static int tegra_xhci_hub_control(struct usb_hcd *hcd, u16 type_req,
		u16 value, u16 index, char *buf, u16 length)
{
	int ret;
	int port = (index & 0xff) - 1;

	/* power on before port resume */
	if (hcd->speed == HCD_USB2) {
		if ((type_req == ClearPortFeature) &&
			(value == USB_PORT_FEAT_SUSPEND))
			xusb_utmi_pad_driver_power(port, true);
	}

	ret = xhci_hub_control(hcd, type_req, value, index, buf, length);

	if ((hcd->speed == HCD_USB2) && (ret == 0)) {
		/* power off after port suspend */
		if ((type_req == SetPortFeature) &&
			(value == USB_PORT_FEAT_SUSPEND))
			/* We dont suspend the PAD while HNP role swap happens
			 * on the OTG port
			 */
			if (!((hcd->self.otg_port == (port + 1)) &&
			    (hcd->self.b_hnp_enable ||
			    hcd->self.otg_quick_hnp))) {
				xusb_utmi_pad_driver_power(port, false);
			}

		/* power on/off after CSC clear for connect/disconnect event */
		if ((type_req == ClearPortFeature) &&
			(value == USB_PORT_FEAT_C_CONNECTION)) {
			struct xhci_hcd *xhci = hcd_to_xhci(hcd);
			u32 portsc = xhci_readl(xhci, xhci->usb2_ports[port]);

			if (portsc & PORT_CONNECT)
				xusb_utmi_pad_driver_power(port, true);
			else {
				/* We dont suspend the PAD while HNP
				 * role swap happens on the OTG port
				 */
				if (!((hcd->self.otg_port == (port + 1))
					&& (hcd->self.b_hnp_enable ||
					hcd->self.otg_quick_hnp)))
					xusb_utmi_pad_driver_power(
							port, false);
			}
		}
	}

	return ret;
}

static int tegra_xhci_hub_status_data(struct usb_hcd *hcd, char *buf)
{
	if (hcd->speed == HCD_USB2) {
		struct xhci_hcd *xhci = hcd_to_xhci(hcd);
		struct tegra_xhci_hcd *tegra = hcd_to_tegra_xhci(hcd);
		int port;

		for_each_enabled_utmi_pad_with_otg(port, tegra) {
			u32 portsc = xhci_readl(xhci, xhci->usb2_ports[port]);

			if (portsc == 0xffffffff)
				break;

			/* power on for remote wakeup event */
			if ((portsc & PORT_PLS_MASK) == XDEV_RESUME)
				xusb_utmi_pad_driver_power(port, true);
		}
	}

	return xhci_hub_status_data(hcd, buf);
}

static int tegra_xhci_update_hub_device(struct usb_hcd *hcd,
		struct usb_device *hdev, struct usb_tt *tt, gfp_t mem_flags)
{
	/* Disable LPM SUPPORT for SS hubs connected to roothub
	   This is to avoid the Firmware exception seen on host controller */
	if (hdev->speed == USB_SPEED_SUPER
				&& (hdev->parent == hdev->bus->root_hub))
		hdev->lpm_capable = 0;

	return xhci_update_hub_device(hcd, hdev, tt, mem_flags);
}

static void tegra_xhci_reset_otg_sspi_work(struct work_struct *work)
{
	struct tegra_xhci_hcd *tegra = container_of(work, struct tegra_xhci_hcd,
			reset_otg_sspi_work);
	struct xhci_hcd *xhci = tegra->xhci;

	dev_dbg(&tegra->pdev->dev, "%s\n", __func__);
	/* set PP=0 */
	xhci_hub_control(xhci->shared_hcd, ClearPortFeature,
		USB_PORT_FEAT_POWER,
		tegra->ss_otg_portnum + 1, NULL, 0);

	/* reset OTG port SSPI */
	ack_fw_message_send_sync(tegra, MBOX_CMD_RESET_SSPI,
			tegra->ss_otg_portnum + 1);

	/* set PP=1 */
	xhci_hub_control(xhci->shared_hcd, SetPortFeature,
		USB_PORT_FEAT_POWER,
		tegra->ss_otg_portnum + 1, NULL, 0);
}

static int tegra_xhci_hcd_reinit(struct usb_hcd *hcd)
{
	if (en_hcd_reinit) {
#ifdef CONFIG_USB_OTG_WAKELOCK
		/* FIXME: Add 3.0 phy support to otg code.
		 * xhci-ring changed. pull hcd reinit code and uncomment below
		 */
		/* otgwl_acquire_temp_lock(); */
#endif
		INIT_WORK(&tegra_xhci_reinit_work, xhci_reinit_work);
		schedule_work(&tegra_xhci_reinit_work);
	} else {
		pr_info("%s: hcd_reinit is disabled\n", __func__);
	}
	return 0;
}

#ifdef CONFIG_NV_GAMEPAD_RESET
static void tegra_loki_gamepad_reset(void)
{
	struct device_node *np = of_find_node_by_name(NULL, "gamepad-reset");

	if (np) {
		if (of_device_is_available(np))
			gamepad_reset_war();
	}
}
#endif

static const struct hc_driver tegra_plat_xhci_driver = {
	.description =		"tegra-xhci",
	.product_desc =		"Nvidia xHCI Host Controller",
	.hcd_priv_size =	sizeof(struct xhci_hcd *),

	/*
	 * generic hardware linkage
	 */
	.irq =			tegra_xhci_irq,
	.flags =		HCD_MEMORY | HCD_USB3 | HCD_SHARED,

	/*
	 * basic lifecycle operations
	 */
	.reset =		xhci_plat_setup,
	.start =		xhci_run,
	.stop =			xhci_stop,
	.shutdown =		xhci_shutdown,

	/*
	 * managing i/o requests and associated device resources
	 */
	.urb_enqueue =		tegra_xhci_urb_enqueue,
	.urb_dequeue =		xhci_urb_dequeue,
	.alloc_dev =		tegra_xhci_alloc_dev,
	.free_dev =		tegra_xhci_free_dev,
	.alloc_streams =	xhci_alloc_streams,
	.free_streams =		xhci_free_streams,
	.add_endpoint =		xhci_add_endpoint,
	.drop_endpoint =	xhci_drop_endpoint,
	.endpoint_reset =	xhci_endpoint_reset,
	.check_bandwidth =	xhci_check_bandwidth,
	.reset_bandwidth =	xhci_reset_bandwidth,
	.address_device =	xhci_address_device,
	.update_hub_device =	tegra_xhci_update_hub_device,
	.reset_device =		xhci_discover_or_reset_device,

	/*
	 * scheduling support
	 */
	.get_frame_number =	xhci_get_frame,

	/* Root hub support */
	.hub_control =		tegra_xhci_hub_control,
	.hub_status_data =	tegra_xhci_hub_status_data,

#ifdef CONFIG_PM
	.bus_suspend =		tegra_xhci_bus_suspend,
	.bus_resume =		tegra_xhci_bus_resume,
#endif

	.enable_usb3_lpm_timeout =	xhci_enable_usb3_lpm_timeout,
	.disable_usb3_lpm_timeout =	xhci_disable_usb3_lpm_timeout,
	.hcd_reinit =	tegra_xhci_hcd_reinit,
};

#ifdef CONFIG_PM
static int
tegra_xhci_suspend(struct platform_device *pdev,
						pm_message_t state)
{
	struct tegra_xhci_hcd *tegra = platform_get_drvdata(pdev);
	struct xhci_hcd *xhci = tegra->xhci;
	int ret = 0;
	int pad = 0;

	mutex_lock(&tegra->sync_lock);

	if (!tegra->init_done) {
		xhci_warn(xhci, "%s: xhci probe not done\n",
				__func__);
		mutex_unlock(&tegra->sync_lock);
		return -EBUSY;
	}
	if (!tegra->hc_in_elpg) {
		xhci_warn(xhci, "%s: lp0 suspend entry while elpg not done\n",
				__func__);
		mutex_unlock(&tegra->sync_lock);
		return -EBUSY;
	}

	/* enable_irq_wake for ss ports */
	ret = enable_irq_wake(tegra->padctl_irq);
	if (ret < 0) {
		xhci_err(xhci,
		"%s: Couldn't enable USB host mode wakeup, irq=%d, error=%d\n",
		__func__, tegra->padctl_irq, ret);
	}

	/* enable_irq_wake for utmip/uhisc wakes */
	ret = enable_irq_wake(tegra->usb3_irq);
	if (ret < 0) {
		xhci_err(xhci,
		"%s: Couldn't enable utmip/uhsic wakeup, irq=%d, error=%d\n",
		__func__, tegra->usb3_irq, ret);
	}

	/* enable_irq_wake for utmip/uhisc wakes */
	ret = enable_irq_wake(tegra->usb2_irq);
	if (ret < 0) {
		xhci_err(xhci,
		"%s: Couldn't enable utmip/uhsic wakeup, irq=%d, error=%d\n",
		__func__, tegra->usb2_irq, ret);
	}

	if (pex_usb_pad_pll_reset_assert())
		dev_err(&pdev->dev, "error assert pex pll\n");

	if (xusb_use_sata_lane(tegra)) {
		if (sata_usb_pad_pll_reset_assert())
			dev_err(&pdev->dev, "error assert sata pll\n");
	}

	regulator_disable(tegra->xusb_s1p8v_reg);
	regulator_disable(tegra->xusb_s1p05v_reg);
	tegra_usb2_clocks_deinit(tegra);

	for_each_enabled_utmi_pad(pad, tegra)
		xusb_utmi_pad_deinit(pad);

	for_each_ss_pad(pad, tegra->soc_config->ss_pad_count) {
		if (tegra->bdata->portmap & (1 << pad))
			xusb_ss_pad_deinit(pad);
	}

	if (XUSB_DEVICE_ID_T114 != tegra->device_id)
		usb3_phy_pad_disable();

	tegra->system_in_lp0 = true;

	mutex_unlock(&tegra->sync_lock);

	return ret;
}

static int
tegra_xhci_resume(struct platform_device *pdev)
{
	struct tegra_xhci_hcd *tegra = platform_get_drvdata(pdev);
	struct xhci_hcd *xhci = tegra->xhci;
	int ret;

	dev_dbg(&pdev->dev, "%s\n", __func__);

	mutex_lock(&tegra->sync_lock);

	if (!tegra->init_done) {
		pr_warn("%s: tegra xhci probe not done\n",
				__func__);
		mutex_unlock(&tegra->sync_lock);
		return -EBUSY;
	}

	tegra->last_jiffies = jiffies;

	disable_irq_wake(tegra->padctl_irq);
	disable_irq_wake(tegra->usb3_irq);
	disable_irq_wake(tegra->usb2_irq);
	tegra->lp0_exit = true;

	ret = regulator_enable(tegra->xusb_s1p05v_reg);
	if (ret)
		xhci_warn(xhci, "enable 1.05V regulator failed %d\n", ret);
	ret = regulator_enable(tegra->xusb_s1p8v_reg);
	if (ret)
		xhci_warn(xhci, "enable 1.8V regulator failed %d\n", ret);
	tegra_usb2_clocks_init(tegra);

	if (pex_usb_pad_pll_reset_deassert())
		dev_err(&pdev->dev, "error deassert pex pll\n");

	if (xusb_use_sata_lane(tegra)) {
		if (sata_usb_pad_pll_reset_deassert())
			dev_err(&pdev->dev, "error deassert sata pll\n");
	}

	tegra->system_in_lp0 = false;

	mutex_unlock(&tegra->sync_lock);

	return 0;
}
#endif

static int init_filesystem_firmware(struct tegra_xhci_hcd *tegra)
{
	struct platform_device *pdev = tegra->pdev;
	int ret;

	if (!strcmp(firmware_file, "")) {
		if (tegra->bdata->firmware_file_dt)
			firmware_file = kasprintf(GFP_KERNEL, "%s",
					tegra->bdata->firmware_file_dt);
		else
			firmware_file = kasprintf(GFP_KERNEL, "%s",
				tegra->soc_config->default_firmware_file);
	}

	ret = request_firmware_nowait(THIS_MODULE, true, firmware_file,
		&pdev->dev, GFP_KERNEL, tegra, init_filesystem_firmware_done);
	if (ret < 0) {
		dev_err(&pdev->dev, "request_firmware failed %d\n", ret);
		return ret;
	}

	return ret;
}

static void init_filesystem_firmware_done(const struct firmware *fw,
					void *context)
{
	struct tegra_xhci_hcd *tegra = context;
	struct platform_device *pdev = tegra->pdev;
	struct cfgtbl *fw_cfgtbl;
	size_t fw_size;
	void *fw_data;
	dma_addr_t fw_dma;
	int ret;

	mutex_lock(&tegra->sync_lock);

	if (fw == NULL) {
		dev_err(&pdev->dev,
			"failed to init firmware from filesystem: %s\n",
			firmware_file);
		goto err_firmware_done;
	}

	fw_cfgtbl = (struct cfgtbl *) fw->data;
	fw_size = fw_cfgtbl->fwimg_len;
	dev_info(&pdev->dev, "Firmware File: %s (%zu Bytes)\n",
			firmware_file, fw_size);

	if (fw_cfgtbl->build_log == LOG_MEMORY)
		fw_log_init(tegra);

	fw_data = dma_alloc_coherent(&pdev->dev, fw_size,
			&fw_dma, GFP_KERNEL);
	if (!fw_data) {
		dev_err(&pdev->dev, "%s: dma_alloc_coherent failed\n",
			__func__);
		goto err_firmware_done;
	}

	memcpy(fw_data, fw->data, fw_size);
	dev_info(&pdev->dev,
		"Firmware DMA Memory: dma 0x%p mapped 0x%p (%zu Bytes)\n",
		(void *)(uintptr_t) fw_dma, fw_data, fw_size);

	/* all set and ready to go */
	tegra->firmware.data = fw_data;
	tegra->firmware.dma = fw_dma;
	tegra->firmware.size = fw_size;

	ret = tegra_xhci_probe2(tegra);
	if (ret < 0) {
		dev_err(&pdev->dev, "%s: failed to probe: %d\n", __func__, ret);
		goto err_firmware_done;
	}

	release_firmware(fw);
	mutex_unlock(&tegra->sync_lock);
	return;

err_firmware_done:
	release_firmware(fw);
	mutex_unlock(&tegra->sync_lock);
	device_release_driver(&pdev->dev);
}

static void deinit_filesystem_firmware(struct tegra_xhci_hcd *tegra)
{
	struct platform_device *pdev = tegra->pdev;

	if (tegra->firmware.data) {
		dma_free_coherent(&pdev->dev, tegra->firmware.size,
			tegra->firmware.data, tegra->firmware.dma);
	}

	csb_write(tegra, XUSB_CSB_MP_ILOAD_BASE_LO, 0);
	memset(&tegra->firmware, 0, sizeof(tegra->firmware));
}
static int init_firmware(struct tegra_xhci_hcd *tegra)
{
	return init_filesystem_firmware(tegra);
}

static void deinit_firmware(struct tegra_xhci_hcd *tegra)
{
	return deinit_filesystem_firmware(tegra);
}

static int tegra_enable_xusb_clk(struct tegra_xhci_hcd *tegra,
		struct platform_device *pdev)
{
	int err = 0;

	if (tegra->soc_config->quirks & TEGRA_XUSB_USE_HS_SRC_CLOCK2) {
		err = clk_enable(tegra->pll_re_vco_clk);
		if (err) {
			dev_err(&pdev->dev, "Failed to enable refPLLE clk\n");
			return err;
		}
	}

	clk_set_rate(tegra->emc_clk, 0);
	err = clk_enable(tegra->emc_clk);
	if (err) {
		dev_err(&pdev->dev, "Failed to enable xusb.emc clk\n");
		goto enable_emc_clk_failed;
	}

	tegra->clock_enable_done = true;

	return 0;

enable_emc_clk_failed:
	if (tegra->soc_config->quirks & TEGRA_XUSB_USE_HS_SRC_CLOCK2)
		clk_disable(tegra->pll_re_vco_clk);
	return err;
}

static const struct tegra_xusb_padctl_regs tegra114_padctl_offsets = {
	.boot_media_0			= 0x0,
	.usb2_pad_mux_0			= 0x4,
	.usb2_port_cap_0		= 0x8,
	.snps_oc_map_0			= 0xc,
	.usb2_oc_map_0			= 0x10,
	.ss_port_map_0			= 0x14,
	.vbus_oc_map		= PADCTL_REG_NONE,
	.oc_det_0			= 0x18,
	.elpg_program_0			= 0x1c,
	.usb2_bchrg_otgpadX_ctlY_0	= {
		{0x20, PADCTL_REG_NONE},
		{0x24, PADCTL_REG_NONE},
		{PADCTL_REG_NONE, PADCTL_REG_NONE}
	},
	.usb2_bchrg_bias_pad_0		= 0x28,
	.usb2_bchrg_tdcd_dbnc_timer_0	= 0x2c,
	.iophy_pll_p0_ctlY_0		= {0x30, 0x34, 0x38, 0x3c},
	.iophy_usb3_padX_ctlY_0		= {
		{0x40, 0x48, 0x50, 0x58},
		{0x44, 0x4c, 0x54, 0x5c}
	},
	.iophy_misc_pad_pX_ctlY_0	= {
		{0x60, 0x68, 0x70, 0x78, 0x80, 0x88},
		{0x64, 0x6c, 0x74, 0x7c, 0x84, 0x8c},
		{PADCTL_REG_NONE, PADCTL_REG_NONE, PADCTL_REG_NONE,
		 PADCTL_REG_NONE, PADCTL_REG_NONE},
		{PADCTL_REG_NONE, PADCTL_REG_NONE, PADCTL_REG_NONE,
		 PADCTL_REG_NONE, PADCTL_REG_NONE},
		{PADCTL_REG_NONE, PADCTL_REG_NONE, PADCTL_REG_NONE,
		 PADCTL_REG_NONE, PADCTL_REG_NONE}
	},
	.usb2_otg_padX_ctlY_0		= {
		{0x90, 0x98},
		{0x94, 0x9c},
		{PADCTL_REG_NONE, PADCTL_REG_NONE}
	},
	.usb2_bias_pad_ctlY_0		= {0xa0, 0xa4},
	.usb2_hsic_padX_ctlY_0		= {
		{0xa8, 0xb0, 0xb8},
		{0xac, 0xb4, 0xbc}
	},
	.ulpi_link_trim_ctl0		= 0xc0,
	.ulpi_null_clk_trim_ctl0	= 0xc4,
	.hsic_strb_trim_ctl0		= 0xc8,
	.wake_ctl0			= 0xcc,
	.pm_spare0			= 0xd0,
	.usb3_pad_mux_0			= PADCTL_REG_NONE,
	.iophy_pll_s0_ctlY_0		= {PADCTL_REG_NONE, PADCTL_REG_NONE,
					   PADCTL_REG_NONE, PADCTL_REG_NONE},
	.iophy_misc_pad_s0_ctlY_0	= {PADCTL_REG_NONE, PADCTL_REG_NONE,
					   PADCTL_REG_NONE, PADCTL_REG_NONE,
					   PADCTL_REG_NONE, PADCTL_REG_NONE},
	.hsic_pad_trk_ctl_0 = PADCTL_REG_NONE,
};

static const struct tegra_xusb_padctl_regs tegra124_padctl_offsets = {
	.boot_media_0			= 0x0,
	.usb2_pad_mux_0			= 0x4,
	.usb2_port_cap_0		= 0x8,
	.snps_oc_map_0			= 0xc,
	.usb2_oc_map_0			= 0x10,
	.ss_port_map_0			= 0x14,
	.vbus_oc_map		= PADCTL_REG_NONE,
	.oc_det_0			= 0x18,
	.elpg_program_0			= 0x1c,
	.usb2_bchrg_otgpadX_ctlY_0	= {
		{0x20, 0x24},
		{0x28, 0x2c},
		{0x30, 0x34}
	},
	.usb2_bchrg_bias_pad_0		= 0x38,
	.usb2_bchrg_tdcd_dbnc_timer_0	= 0x3c,
	.iophy_pll_p0_ctlY_0		= {0x40, 0x44, 0x48, 0x4c},
	.iophy_usb3_padX_ctlY_0		= {
		{0x50, 0x58, 0x60, 0x68},
		{0x54, 0x5c, 0x64, 0x6c}
	},
	.iophy_misc_pad_pX_ctlY_0	= {
		{0x70, 0x78, 0x80, 0x88, 0x90, 0x98},
		{0x74, 0x7c, 0x84, 0x8c, 0x94, 0x9c},
		{0xec, 0xf8, 0x104, 0x110, 0x11c, 0x128},
		{0xf0, 0xfc, 0x108, 0x114, 0x120, 0x12c},
		{0xf4, 0x100, 0x10c, 0x118, 0x124, 0x130}
	},
	.usb2_otg_padX_ctlY_0		= {
		{0xa0, 0xac},
		{0xa4, 0xb0},
		{0xa8, 0xb4}
	},
	.usb2_bias_pad_ctlY_0		= {0xb8, 0xbc},
	.usb2_hsic_padX_ctlY_0		= {
		{0xc0, 0xc8, 0xd0},
		{0xc4, 0xcc, 0xd4}
	},
	.ulpi_link_trim_ctl0		= 0xd8,
	.ulpi_null_clk_trim_ctl0	= 0xdc,
	.hsic_strb_trim_ctl0		= 0xe0,
	.wake_ctl0			= 0xe4,
	.pm_spare0			= 0xe8,
	.usb3_pad_mux_0			= 0x134,
	.iophy_pll_s0_ctlY_0		= {0x138, 0x13c, 0x140, 0x144},
	.iophy_misc_pad_s0_ctlY_0	= {0x148, 0x14c, 0x150, 0x154,
					   0x158, 0x15c},
	.hsic_pad_trk_ctl_0 = PADCTL_REG_NONE,
};

static const struct tegra_xusb_padctl_regs tegra210_padctl_offsets = {
	.boot_media_0		= 0x0,
	.usb2_pad_mux_0		= 0x4,
	.usb2_port_cap_0	= 0x8,
	.snps_oc_map_0		= 0xc,
	.usb2_oc_map_0		= 0x10,
	.ss_port_map_0		= 0x14,
	.vbus_oc_map		= 0x18,
	.oc_det_0			= 0x1c,
	.elpg_program_0		= 0x20,
	.elpg_program_1		= 0x24,
	.usb3_pad_mux_0		= 0x28,
	.wake_ctl0			= 0x2c,
	.pm_spare0			= 0x30,
	.uphy_cfg_stb_0		= 0x34,
	.usb2_bchrg_otgpadX_ctlY_0	= {
		{0x80, 0x84},
		{0xc0, 0xc4},
		{0x100, 0x104},
		{0x140, 0x144},
	},
	.usb2_otg_padX_ctlY_0	= {
		{0x88, 0x8c},
		{0xc8, 0xcc},
		{0x108, 0x10c},
		{0x148, 0x14c},
	},
	.usb2_bchrg_tdcd_dbnc_timer_0	= 0x280,
	.usb2_bias_pad_ctlY_0		= {0x284, 0x288},
	.usb2_hsic_padX_ctlY_0		= {
		{0x300, 0x304, 0x308},
		{0x320, 0x324, 0x328},
	},
	.hsic_pad_trk_ctl_0 = 0x340,
	.hsic_strb_trim_ctl0	= 0x344,
	.uphy_pll_p0_ctlY_0	= {0x360, 0x364, 0x368, 0x36c, 0x370,
			0x374, 0x378, 0x37c, 0x380, 0x384, 0x388},
	.uphy_misc_pad_pX_ctlY_0	= {
		{0x460, 0x464, 0x468, 0x46c, 0x470, 0x474, 0x478, 0x47c, 0x480},
		{0x4a0, 0x4a4, 0x4a8, 0x4ac, 0x4b0, 0x4b4, 0x4b8, 0x4bc, 0x4c0},
		{0x4e0, 0x4e4, 0x4e8, 0x4ec, 0x4f0, 0x4f4, 0x4f8, 0x4fc, 0x500},
		{0x520, 0x524, 0x528, 0x52c, 0x530, 0x534, 0x538, 0x53c, 0x540},
		{0x560, 0x564, 0x568, 0x56c, 0x570, 0x574, 0x578, 0x57c, 0x580},
		{0x5a0, 0x5a4, 0x5a8, 0x5ac, 0x5b0, 0x5b4, 0x5b8, 0x5bc, 0x5c0},
		{0x5e0, 0x5e4, 0x5e8, 0x5ec, 0x5f0, 0x5f4, 0x5f8, 0x5fc, 0x600},
	},
	.uphy_pll_s0_ctlY_0 = {0x860, 0x864, 0x868, 0x86c, 0x870,
			0x874, 0x878, 0x87c, 0x880, 0x884, 0x888},
	.uphy_misc_pad_s0_ctlY_0	= {0x960, 0x964, 0x968, 0x96c, 0x970,
			0x974, 0x978, 0x97c, 0x980},
	.uphy_usb3_padX_ectlY_0 = {
		{0xa60, 0xa64, 0xa68, 0xa6c, 0xa70, 0xa74},
		{0xaa0, 0xaa4, 0xaa8, 0xaac, 0xab0, 0xab4},
		{0xae0, 0xae4, 0xae8, 0xaec, 0xaf0, 0xaf4},
		{0xb20, 0xb24, 0xb28, 0xb2c, 0xb30, 0xb34},
	},
	.uphy_usb3_padX_ctl_0 = {0xa78, 0xab8, 0xaf8, 0xb38},
	.usb2_vbus_id_0 = 0xc60,
	.usb2_bchrg_bias_pad_0		= PADCTL_REG_NONE,
	.iophy_usb3_padX_ctlY_0		= {
		{PADCTL_REG_NONE, PADCTL_REG_NONE
		, PADCTL_REG_NONE, PADCTL_REG_NONE},
		{PADCTL_REG_NONE, PADCTL_REG_NONE
		, PADCTL_REG_NONE, PADCTL_REG_NONE}
	},
	.iophy_pll_p0_ctlY_0		= {
		PADCTL_REG_NONE, PADCTL_REG_NONE
		, PADCTL_REG_NONE, PADCTL_REG_NONE},
	.iophy_misc_pad_pX_ctlY_0	= {
		{PADCTL_REG_NONE, PADCTL_REG_NONE, PADCTL_REG_NONE
		, PADCTL_REG_NONE, PADCTL_REG_NONE, PADCTL_REG_NONE},
		{PADCTL_REG_NONE, PADCTL_REG_NONE, PADCTL_REG_NONE
		, PADCTL_REG_NONE, PADCTL_REG_NONE, PADCTL_REG_NONE},
		{PADCTL_REG_NONE, PADCTL_REG_NONE, PADCTL_REG_NONE
		, PADCTL_REG_NONE, PADCTL_REG_NONE, PADCTL_REG_NONE},
		{PADCTL_REG_NONE, PADCTL_REG_NONE, PADCTL_REG_NONE
		, PADCTL_REG_NONE, PADCTL_REG_NONE, PADCTL_REG_NONE},
		{PADCTL_REG_NONE, PADCTL_REG_NONE, PADCTL_REG_NONE
		, PADCTL_REG_NONE, PADCTL_REG_NONE, PADCTL_REG_NONE}
	},
	.iophy_pll_s0_ctlY_0	= {
		PADCTL_REG_NONE, PADCTL_REG_NONE
		, PADCTL_REG_NONE, PADCTL_REG_NONE},
	.iophy_misc_pad_s0_ctlY_0	= {PADCTL_REG_NONE, PADCTL_REG_NONE
			, PADCTL_REG_NONE, PADCTL_REG_NONE
			, PADCTL_REG_NONE, PADCTL_REG_NONE},
	.ulpi_link_trim_ctl0		= PADCTL_REG_NONE,
	.ulpi_null_clk_trim_ctl0	= PADCTL_REG_NONE,
};

/* FIXME: using notifier to transfer control to host from suspend
 * for otg port when xhci is in elpg. Find  better alternative
 */
static int tegra_xhci_otg_notify(struct notifier_block *nb,
				   unsigned long event, void *unused)
{
	struct tegra_xhci_hcd *tegra = container_of(nb,
					struct tegra_xhci_hcd, otgnb);
	struct platform_device *pdev = tegra->pdev;

	dev_info(&pdev->dev, "received otg event %lu\n", event);

	if (event == USB_EVENT_ID || event == USB_EVENT_ID_FLOAT) {
		tegra->otg_port_owned = (event == USB_EVENT_ID) ? true : false;
		if (tegra->hc_in_elpg) {
			dev_info(&pdev->dev, "elpg exit by USB_ID=%s\n",
				tegra->otg_port_owned ? "ground" : "float");
			tegra->host_resume_req = true;
			tegra->otg_port_ownership_changed = true;
			schedule_work(&tegra->host_elpg_exit_work);
		} else {
			schedule_work(&tegra->xotg_vbus_work);
		}
	} else if (event == USB_EVENT_HANDLE_OTG_PP) {
		tegra_xhci_handle_otg_port_change(tegra);
	}

	return NOTIFY_OK;
}

static int utmi_pad_read_property(struct usb_vbus_en_oc *vbus_en_oc,
		struct device_node *np)
{
	int err;
	char name[17];

	snprintf(name, sizeof(name), "nvidia,vbus_type");
	err = of_property_read_u32(np, name, (u32 *) &vbus_en_oc->type);

	if (err < 0)
		return err;

	err = of_property_read_u32(np, "nvidia,vbus_en_pin", &vbus_en_oc->pin);

	return err;
}

static void tegra_xusb_parse_subnode(struct tegra_xhci_hcd *tegra,
		struct device_node *parent)
{
	struct tegra_xusb_board_data *bdata = tegra->bdata;
	struct device_node *np;
	char name[10];
	int pad = 0;
	int err;

	tegra->bdata->vbus_en_oc = devm_kzalloc(&tegra->pdev->dev,
				tegra->soc_config->utmi_pad_count *
				sizeof(struct usb_vbus_en_oc), GFP_KERNEL);
	for_each_enabled_utmi_pad(pad, tegra) {

		snprintf(name, sizeof(name), "utmi_pad%d", pad);
		np = of_get_child_by_name(parent, name);

		if (!np) {
			pr_debug("Do not find child node %s\n", name);
			continue;
		}

		err = utmi_pad_read_property(&bdata->vbus_en_oc[pad], np);
		if (err < 0)
			pr_err("Fail to parse node %s\n", name);

		pr_debug("set pad %d, type(%d), control pin(%d)\n",
				pad, tegra->bdata->vbus_en_oc[pad].type,
				tegra->bdata->vbus_en_oc[pad].pin);
	}
}

#define MASK_SHIFT(x , mask, shift) ((x & mask) << shift)

#define SET_PORT(bdata, val, ss_node)	\
	(bdata->ss_portmap |= \
	MASK_SHIFT(val, 0xF, ss_node * 4))


#define SET_LANE(bdata, val, ss_node) \
	(bdata->lane_owner |= (val << (ss_node * 4)))

static void tegra_xusb_parse_portmap(struct tegra_xusb_board_data *bdata,
					struct device_node *node)
{
	u32 lane = 0, ss_portmap = 0, otg_portmap = 0;
	u32 ss_node = 0, index = 0, count = 0;

	count = of_property_count_u32(node, "nvidia,ss_ports");
	if (count < 4 || count % 4 != 0) {
		pr_err("nvidia,ss_ports not found or invalid count %d\n",
								count);
		return;
	}

	pr_debug("ss_portmap = %x, lane_owner = %x, otg_portmap = %x\n",
		bdata->ss_portmap, bdata->lane_owner, bdata->otg_portmap);
	do {
		of_property_read_u32_index(node, "nvidia,ss_ports",
						index, &ss_node);
		if (ss_node != TEGRA_XHCI_UNUSED_PORT)
			index++;
		else {
			SET_PORT(bdata, TEGRA_XHCI_UNUSED_PORT, index / 4);
			SET_LANE(bdata, TEGRA_XHCI_UNUSED_LANE, index / 4);
			index += 4;
			continue;
		}
		of_property_read_u32_index(node, "nvidia,ss_ports",
						index, &ss_portmap);
		SET_PORT(bdata, ss_portmap, ss_node);
		index++;

		of_property_read_u32_index(node, "nvidia,ss_ports",
						index, &lane);
		SET_LANE(bdata, lane, ss_node);

		index++;
		of_property_read_u32_index(node, "nvidia,ss_ports",
						index, &otg_portmap);
		if (lane != TEGRA_XHCI_UNUSED_LANE)
			bdata->otg_portmap |=
				MASK_SHIFT(otg_portmap, BIT(0), ss_node);
		if (ss_portmap != TEGRA_XHCI_UNUSED_PORT)
			bdata->otg_portmap |= MASK_SHIFT(otg_portmap, BIT(0),
						(ss_portmap + XUSB_UTMI_INDEX));
		index++;

		pr_debug("index = %d, count = %d, ss_node = %d\n",
			index, count, ss_node);
		pr_debug("ss_portmap = %x, lane = %x, otg_portmap = %x\n",
			ss_portmap, lane, otg_portmap);
	} while (index < count);
	return;
}

static void tegra_xusb_read_board_data(struct tegra_xhci_hcd *tegra)
{
	struct tegra_xusb_board_data *bdata = tegra->bdata;
	struct device_node *node = tegra->pdev->dev.of_node;
	struct device_node *padctl;
	int ret;

	bdata->uses_external_pmic = of_property_read_bool(node,
					"nvidia,uses_external_pmic");
	bdata->gpio_controls_muxed_ss_lanes = of_property_read_bool(node,
					"nvidia,gpio_controls_muxed_ss_lanes");
	ret = of_property_read_u32(node, "nvidia,gpio_ss1_sata",
					&bdata->gpio_ss1_sata);
	ret = of_property_read_u32(node, "nvidia,portmap",
					&bdata->portmap);
	ret = of_property_read_u32(node, "nvidia,ss_portmap",
					(u32 *) &bdata->ss_portmap);
	ret = of_property_read_u32(node, "nvidia,lane_owner",
					(u32 *) &bdata->lane_owner);
	ret = of_property_read_u32(node, "nvidia,ulpicap",
					(u32 *) &bdata->ulpicap);
	ret = of_property_read_u8_array(node, "nvidia,hsic0",
					(u8 *) &bdata->hsic[0],
					sizeof(bdata->hsic[0]));

	ret = of_property_read_u8_array(node, "nvidia,hsic1",
					(u8 *) &bdata->hsic[1],
					sizeof(bdata->hsic[0]));
	ret = of_property_read_string(node, "nvidia,firmware_file",
					&bdata->firmware_file_dt);
	ret = of_property_read_u32(node, "nvidia,boost_cpu_trigger",
					&tegra->boost_cpu_trigger);

	ret = of_property_read_u32(node, "nvidia,hsic_power_on_at_boot",
					(u32 *) &bdata->hsic_power_on_at_boot);

	/* For T210, retrieve common padctl config from xusb_pad_ctl node */
	padctl = of_parse_phandle(node, "nvidia,common_padctl", 0);

	tegra_xusb_parse_portmap(bdata, padctl);

	pr_debug("[%s]Value get from DT\n", __func__);
	pr_debug("nvidia,portmap = %x\n", bdata->portmap);
	pr_debug("nvidia,ss_portmap = %x\n", bdata->ss_portmap);
	pr_debug("nvidia,lane_owner = %x\n", bdata->lane_owner);
	pr_debug("nvidia,otg_portmap = %x\n", bdata->otg_portmap);

	tegra_xusb_parse_subnode(tegra, node);

	/* TODO: Add error conditions check */
}

/* FIXME: Should have a better way to handle */
static void tegra_xusb_read_calib_data(struct tegra_xhci_hcd *tegra)
{
	u32 usb_calib0;
	struct tegra_xusb_chip_calib *cdata = tegra->cdata;

	/* Put utmi pad program to padctl.c */
	if (XUSB_DEVICE_ID_T210 == tegra->device_id)
		return;

	usb_calib0 = tegra_fuse_readl(FUSE_SKU_USB_CALIB_0);

	pr_info("tegra_xusb_read_usb_calib: usb_calib0 = 0x%08x\n", usb_calib0);
	/*
	 * read from usb_calib0 and pass to driver
	 * set HS_CURR_LEVEL (PAD0)	= usb_calib0[5:0]
	 * set TERM_RANGE_ADJ		= usb_calib0[10:7]
	 * set HS_SQUELCH_LEVEL		= usb_calib0[12:11]
	 * set HS_IREF_CAP		= usb_calib0[14:13]
	 * set HS_CURR_LEVEL (PAD1)	= usb_calib0[20:15]
	*/
	cdata->hs_squelch_level = (usb_calib0 >> 11) & 0x3;
	pr_debug("hs_squelch_level = 0x%x\n", cdata->hs_squelch_level);

}

static void t114_chk_lane_owner_by_pad(int pad, u32 lane_owner)
{
	pr_err("Lane owner for SS Pad not supported on T114\n");
}

static void t124_chk_lane_owner_by_pad(int pad, u32 lane_owner)
{
	u32 lane = NOT_SUPPORTED;

	if (pad == 0) {
		lane = (lane_owner & 0x4);
		if (!lane)
			pr_err("Lane owner for SS Pad 0 setting is incorrect\n");
	}
	if (pad == 1) {
		if (lane_owner & 0x1)
			lane = (lane_owner & 0x1);
		else if (lane_owner & 0x2)
			lane = (lane_owner & 0x2);
		else
			pr_err("Lane owner for SS Pad 1 setting is incorrect\n");
	}
}

static void t210_chk_lane_owner_by_pad(int pad, u32 lane_owner)
{
	u32 lane = (lane_owner >> (pad * 4)) & 0xf;

	pr_debug("%s, pad %d, lane %d\n", __func__ , pad, lane);

	if (lane == 0xf)
		return;

	switch (pad) {
	case 0:
		if (lane != 6)
			pr_err("Lane owner for SS Pad 0 setting is incorrect\n");
		break;
	case 1:
		if (lane != 5)
			pr_err("Lane owner for SS Pad 1 setting is incorrect\n");
		break;
	case 2:
		if ((lane != 3) && (lane != 0))
			pr_err("Lane owner for SS Pad 2 setting is incorrect\n");
		break;
	case 3:
		if ((lane != 4) && (lane != 8))
			pr_err("Lane owner for SS Pad 3 setting is incorrect\n");
		break;
	}

}
static char *vbus[] = {"usb_vbus0", "usb_vbus1", "usb_vbus2",};
static const struct tegra_xusb_soc_config tegra114_soc_config = {
	.pmc_portmap = (TEGRA_XUSB_UTMIP_PMC_PORT0 << 0) |
			(TEGRA_XUSB_UTMIP_PMC_PORT2 << 4),
	.quirks = TEGRA_XUSB_USE_HS_SRC_CLOCK2,
	.rx_wander = (0x3 << 4),
	.rx_eq = (0x3928 << 8),
	.cdr_cntl = (0x26 << 24),
	.dfe_cntl = 0x002008EE,
	.hs_slew = (0xE << 6),
	.ls_rslew_pad = {(0x3 << 14), (0x0 << 14)},
	.hs_disc_lvl = (0x7 << 2),
	.spare_in = 0x0,
	.supply = {
		.utmi_vbuses = vbus,
		.s3p3v = "hvdd_usb",
		.s1p8v = "avdd_usb_pll",
		.vddio_hsic = "vddio_hsic",
		.s1p05v = "avddio_usb",
	},
	.default_firmware_file = "tegra_xusb_firmware",
	.utmi_pad_count = 2,
	.ss_pad_count = 1,
	.padctl_offsets = &tegra114_padctl_offsets,
	.check_lane_owner_by_pad = t114_chk_lane_owner_by_pad,
};

static const struct tegra_xusb_soc_config tegra124_soc_config = {
	.pmc_portmap = (TEGRA_XUSB_UTMIP_PMC_PORT0 << 0) |
			(TEGRA_XUSB_UTMIP_PMC_PORT1 << 4) |
			(TEGRA_XUSB_UTMIP_PMC_PORT2 << 8),
	.supply = {
		.utmi_vbuses = vbus,
		.s3p3v = "hvdd_usb",
		.s1p8v = "avdd_pll_utmip",
		.vddio_hsic = "vddio_hsic",
		.s1p05v = "avddio_usb",
	},
	.default_firmware_file = "tegra12x_xusb_firmware",
	.utmi_pad_count = 3,
	.ss_pad_count = 2,
	.padctl_offsets = &tegra124_padctl_offsets,
	.check_lane_owner_by_pad = t124_chk_lane_owner_by_pad,
};

static const struct tegra_xusb_soc_config tegra132_soc_config = {
	.pmc_portmap = (TEGRA_XUSB_UTMIP_PMC_PORT0 << 0) |
			(TEGRA_XUSB_UTMIP_PMC_PORT1 << 4) |
			(TEGRA_XUSB_UTMIP_PMC_PORT2 << 8),
	.supply = {
		.utmi_vbuses = vbus,
		.s3p3v = "hvdd_usb",
		.s1p8v = "avdd_pll_utmip",
		.vddio_hsic = "vddio_hsic",
		.s1p05v = "avddio_usb",
	},
	.default_firmware_file = "tegra13x_xusb_firmware",
	.utmi_pad_count = 3,
	.ss_pad_count = 2,
	.padctl_offsets = &tegra124_padctl_offsets,
	.check_lane_owner_by_pad = t124_chk_lane_owner_by_pad,
};

static char *t210_vbus[] = {"usb_vbus0", "usb_vbus1", "usb_vbus2", "usb_vbus3"};
static const struct tegra_xusb_soc_config tegra210_soc_config = {
	.pmc_portmap = (TEGRA_XUSB_UTMIP_PMC_PORT0 << 0) |
			(TEGRA_XUSB_UTMIP_PMC_PORT1 << 4) |
			(TEGRA_XUSB_UTMIP_PMC_PORT2 << 8) |
			(TEGRA_XUSB_UTMIP_PMC_PORT3 << 12),
	.supply = {
		.utmi_vbuses = t210_vbus,
		.s3p3v = "hvdd_usb",
		.s1p8v = "avdd_pll_utmip",
		.vddio_hsic = "vddio_hsic",
		.s1p05v = "avddio_usb",
	},
	.default_firmware_file = "tegra21x_xusb_firmware",
	.utmi_pad_count = 4,
	.ss_pad_count = 4,
	.padctl_offsets = &tegra210_padctl_offsets,
	.check_lane_owner_by_pad = t210_chk_lane_owner_by_pad,

	.tx_term_ctrl = 0x2,
	.rx_ctle = 0xfc,
	.rx_dfe = 0xc0077f1f,
	.rx_cdr_ctrl = 0x1c7,
	.rx_eq_ctrl_h = 0xfcf01368,
};

static struct of_device_id tegra_xhci_of_match[] = {
	{ .compatible = "nvidia,tegra114-xhci", .data = &tegra114_soc_config },
	{ .compatible = "nvidia,tegra124-xhci", .data = &tegra124_soc_config },
	{ .compatible = "nvidia,tegra132-xhci", .data = &tegra132_soc_config },
	{ .compatible = "nvidia,tegra210-xhci", .data = &tegra210_soc_config },
	{ },
};

static ssize_t hsic_power_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct tegra_xhci_hcd *tegra = platform_get_drvdata(pdev);
	int pad;

	for_each_enabled_hsic_pad(pad, tegra) {
		if (&tegra->hsic_power_attr[pad] == attr)
			return sprintf(buf, "%d\n", pad);
	}

	return sprintf(buf, "-1\n");
}

static ssize_t hsic_power_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t n)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct tegra_xhci_hcd *tegra = platform_get_drvdata(pdev);
	enum MBOX_CMD_TYPE msg;
	unsigned int on;
	int pad, port;
	int ret;

	if (kstrtouint(buf, 10, &on))
		return -EINVAL;

	if (on)
		msg = MBOX_CMD_AIRPLANE_MODE_DISABLED;
	else
		msg = MBOX_CMD_AIRPLANE_MODE_ENABLED;

	for_each_enabled_hsic_pad(pad, tegra) {
		port = hsic_pad_to_port(pad);
		if (&tegra->hsic_power_attr[pad] == attr) {
			hsic_pad_pupd_set(tegra, pad, PUPD_IDLE);
			ret = fw_message_send(tegra, msg, BIT(port + 1));
		}
	}

	return n;
}

static void hsic_power_remove_file(struct tegra_xhci_hcd *tegra)
{
	struct device *dev = &tegra->pdev->dev;
	int p;

	for_each_enabled_hsic_pad(p, tegra) {
		if (tegra->hsic_power_attr[p].attr.name) {
			device_remove_file(dev, &tegra->hsic_power_attr[p]);
			kzfree(tegra->hsic_power_attr[p].attr.name);
		}
	}

}

static int hsic_power_create_file(struct tegra_xhci_hcd *tegra)
{
	struct device *dev = &tegra->pdev->dev;
	int p;
	int err;
	char *power_attr;

	for_each_enabled_hsic_pad(p, tegra) {
		power_attr = kasprintf(GFP_KERNEL, "hsic%d_power", p);
		if (!power_attr)
			return -ENOMEM;

		tegra->hsic_power_attr[p].attr.name = power_attr;
		power_attr = NULL;
		tegra->hsic_power_attr[p].show = hsic_power_show;
		tegra->hsic_power_attr[p].store = hsic_power_store;
		tegra->hsic_power_attr[p].attr.mode = (S_IRUGO | S_IWUSR);
		sysfs_attr_init(&tegra->hsic_power_attr[p].attr);

		err = device_create_file(dev, &tegra->hsic_power_attr[p]);
		if (err) {
			kzfree(tegra->hsic_power_attr[p].attr.name);
			tegra->hsic_power_attr[p].attr.name = NULL;
			return err;
		}
	}

	return 0;
}

#define DEV_RST	31
static void xusb_tegra_program_registers(void)
{
	struct clk *c = clk_get_sys(NULL, "xusb_padctl");
	u32 val;

	/* Clear XUSB_PADCTL_RST D14 */
	tegra_periph_reset_deassert(c);

	/* Also dessert dev_rst for FPGA */
	val = readl(IO_ADDRESS(0x6000600c));
	val &= ~(1 << DEV_RST);
	writel(val, IO_ADDRESS(0x6000600c));

}

/* TODO: we have to refine error handling in tegra_xhci_probe() */
static int tegra_xhci_probe(struct platform_device *pdev)
{
	struct tegra_xhci_hcd *tegra;
	int ret;
	const struct tegra_xusb_soc_config *soc_config;
	const struct of_device_id *match;
	unsigned pad;

	if (tegra_platform_is_fpga())
		xusb_tegra_program_registers();

	BUILD_BUG_ON(sizeof(struct cfgtbl) != 256);

	if (usb_disabled())
		return -ENODEV;

	tegra = devm_kzalloc(&pdev->dev, sizeof(*tegra), GFP_KERNEL);
	if (!tegra)
		return -ENOMEM;

	mutex_init(&tegra->sync_lock);
	spin_lock_init(&tegra->lock);
	mutex_init(&tegra->mbox_lock);
	mutex_init(&tegra->mbox_lock_ack);

	tegra->init_done = false;

	tegra->bdata = devm_kzalloc(&pdev->dev, sizeof(
					struct tegra_xusb_board_data),
					GFP_KERNEL);
	if (!tegra->bdata) {
		dev_err(&pdev->dev, "memory alloc failed\n");
		return -ENOMEM;
	}
	tegra->cdata = devm_kzalloc(&pdev->dev, sizeof(
					struct tegra_xusb_chip_calib),
					GFP_KERNEL);
	if (!tegra->cdata) {
		dev_err(&pdev->dev, "memory alloc failed\n");
		return -ENOMEM;
	}

	match = of_match_device(tegra_xhci_of_match, &pdev->dev);
	if (!match) {
		dev_err(&pdev->dev, "Error: No device match found\n");
		return -ENODEV;
	}
	soc_config = match->data;

	/* Right now device-tree probed devices don't get dma_mask set.
	 * Since shared usb code relies on it, set it here for now.
	 * Once we have dma capability bindings this can go away.
	 */
	tegra->tegra_xusb_dmamask = DMA_BIT_MASK(64);
	pdev->dev.dma_mask = &tegra->tegra_xusb_dmamask;

	tegra->pdev = pdev;
	tegra->soc_config = soc_config;
	tegra_xusb_read_calib_data(tegra);
	tegra_xusb_read_board_data(tegra);
	tegra->pdata = dev_get_platdata(&pdev->dev);
	if (tegra->pdata) {
		tegra->bdata->portmap = tegra->pdata->portmap;
		tegra->bdata->hsic[0].pretend_connect =
				tegra->pdata->pretend_connect_0;
		tegra->bdata->lane_owner = tegra->pdata->lane_owner;
	}

	for_each_enabled_hsic_pad(pad, tegra) {
		if (tegra->bdata->hsic_power_on_at_boot & (1 << pad))
			tegra->bdata->hsic[pad].pretend_connect = true;
	}

	if (!tegra->bdata->portmap) {
		pr_info("%s doesn't have any port enabled\n", __func__);
		return -ENODEV;
	}

	tegra->ss_pwr_gated = false;
	tegra->host_pwr_gated = false;
	tegra->hc_in_elpg = false;
	tegra->hs_wake_event = false;
	tegra->host_resume_req = false;
	tegra->lp0_exit = false;
	if (!tegra->boost_cpu_trigger)
		tegra->boost_cpu_trigger = BOOST_TRIGGER;
	/* request resource padctl base address */
	ret = tegra_xhci_request_mem_region(pdev, 3, &tegra->padctl_base);
	if (ret) {
		dev_err(&pdev->dev, "failed to map padctl\n");
		return ret;
	}

	/* request resource fpci base address */
	ret = tegra_xhci_request_mem_region(pdev, 1, &tegra->fpci_base);
	if (ret) {
		dev_err(&pdev->dev, "failed to map fpci\n");
		return ret;
	}

	/* request resource ipfs base address */
	ret = tegra_xhci_request_mem_region(pdev, 2, &tegra->ipfs_base);
	if (ret) {
		dev_err(&pdev->dev, "failed to map ipfs\n");
		return ret;
	}

	tegra->base_list[1] = tegra->fpci_base;
	tegra->base_list[2] = tegra->ipfs_base;
	tegra->base_list[3] = tegra->padctl_base;

	ret = init_firmware(tegra);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to init firmware\n");
		ret = -ENODEV;
		goto err_deinit_firmware_log;
	}

	return 0;

err_deinit_firmware_log:
	fw_log_deinit(tegra);

	return ret;
}

static int tegra_xhci_probe2(struct tegra_xhci_hcd *tegra)
{
	struct platform_device *pdev = tegra->pdev;
	const struct hc_driver *driver;
	int ret;
	u32 val;
	unsigned pad;
	struct resource	*res;
	int irq;
	struct xhci_hcd	*xhci;
	struct usb_hcd	*hcd;
	u32 portsc;
#if defined(CONFIG_ARCH_TEGRA_21x_SOC)
	u32 port;
#endif
	int partition_id_xusba, partition_id_xusbc;

	for (pad = 0; pad < tegra->soc_config->utmi_pad_count; pad++) {
		if (BIT(XUSB_UTMI_INDEX + pad) & tegra->bdata->otg_portmap) {
			tegra->hs_otg_portnum = pad;
			break;
		}
	}

	for (pad = 0; pad < tegra->soc_config->ss_pad_count; pad++) {
		if (BIT(pad) & tegra->bdata->otg_portmap) {
			tegra->ss_otg_portnum = pad;
			break;
		}
	}

	for (pad = 0; pad < XUSB_UTMI_COUNT; pad++)
		set_port_cdp(tegra, true, pad);

#if defined(CONFIG_ARCH_TEGRA_21x_SOC)
	/* By default disable the BATTERY_CHRG_OTGPAD for all ports */
	for (port = 0; port <= XUSB_UTMI_COUNT; port++)
		t210_enable_battery_circuit(tegra, port);
#endif

	/* Enable power rails to the PAD,VBUS
	 * and pull-up voltage.Initialize the regulators
	 */
	ret = tegra_xusb_regulator_init(tegra, pdev);
	if (ret) {
		dev_err(&pdev->dev, "failed to initialize xusb regulator\n");
		if (ret == -ENODEV) {
			ret = -EPROBE_DEFER;
			dev_err(&pdev->dev, "Retry at a later stage\n");
		}
		goto err_put_otg_transceiver;
	}

	/* Enable UTMIP, PLLU and PLLE */
	ret = tegra_usb2_clocks_init(tegra);
	if (ret) {
		dev_err(&pdev->dev, "error initializing usb2 clocks\n");
		goto err_deinit_tegra_xusb_regulator;
	}

	partition_id_xusba = tegra_pd_get_powergate_id(tegra_xusba_pd);
	if (partition_id_xusba < 0)
		return -EINVAL;

	partition_id_xusbc = tegra_pd_get_powergate_id(tegra_xusbc_pd);
	if (partition_id_xusbc < 0)
		return -EINVAL;
	/* tegra_unpowergate_partition also does partition reset deassert */
	ret = tegra_unpowergate_partition_with_clk_on(partition_id_xusba);
	if (ret)
		dev_err(&pdev->dev, "could not unpowergate xusba partition\n");

	/* tegra_unpowergate_partition also does partition reset deassert */
	ret = tegra_unpowergate_partition_with_clk_on(partition_id_xusbc);
	if (ret)
		dev_err(&pdev->dev, "could not unpowergate xusbc partition\n");

	ret = tegra_xusb_partitions_clk_init(tegra);
	if (ret) {
		dev_err(&pdev->dev,
			"failed to initialize xusb partitions clocks\n");
		goto err_deinit_usb2_clocks;
	}

	ret = tegra_enable_xusb_clk(tegra, pdev);
	if (ret) {
		dev_err(&pdev->dev, "could not enable partition clock\n");
		goto err_deinit_xusb_partition_clk;
	}

	/* reset the pointer back to NULL. driver uses it */
	/* platform_set_drvdata(pdev, NULL); */

	/* request resource host base address */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "mem resource host doesn't exist\n");
		ret = -ENODEV;
		goto err_deinit_xusb_partition_clk;
	}
	tegra->host_phy_base = res->start;
	tegra->host_phy_size = resource_size(res);

	tegra->host_phy_virt_base = devm_ioremap(&pdev->dev,
				res->start, resource_size(res));
	if (!tegra->host_phy_virt_base) {
		dev_err(&pdev->dev, "error mapping host phy memory\n");
		ret = -ENOMEM;
		goto err_deinit_xusb_partition_clk;
	}
	/* Setup IPFS access and BAR0 space */
	tegra_xhci_cfg(tegra);

	val = readl(tegra->fpci_base + XUSB_CFG_0);
	tegra->device_id = (val >> 16) & 0xffff;

	dev_info(&pdev->dev, "XUSB device id = 0x%x (%s)\n", tegra->device_id,
		XUSB_IS_T114(tegra) ? "T114" : XUSB_IS_T124(tegra) ? "T124" :
		XUSB_IS_T210(tegra) ? "T210" : "UNKNOWN");

	if ((tegra->bdata->otg_portmap & (0xff << XUSB_UTMI_INDEX)) ||
		(tegra->bdata->portmap & TEGRA_XUSB_USB2_P0)) {
		if (XUSB_DEVICE_ID_T124 == tegra->device_id)
			tegra->transceiver = usb_get_phy(USB_PHY_TYPE_USB2);
		else if (XUSB_DEVICE_ID_T210 == tegra->device_id)
			tegra->transceiver = usb_get_phy(USB_PHY_TYPE_USB3);
		if (IS_ERR_OR_NULL(tegra->transceiver)) {
			dev_err(&pdev->dev, "failed to get usb phy\n");
			tegra->transceiver = NULL;
		}
	}
	tegra->padregs = tegra->soc_config->padctl_offsets;

	tegra->base_list[0] = tegra->host_phy_virt_base;
	tegra->prod_list = tegra_prod_init(tegra->pdev->dev.of_node);
	if (IS_ERR(tegra->prod_list)) {
		dev_err(&pdev->dev, "Prod settings list not initialized\n");
		tegra->prod_list = NULL;
	}

	if (pex_usb_pad_pll_reset_deassert())
		dev_err(&pdev->dev, "error deassert pex pll\n");

	if (xusb_use_sata_lane(tegra)) {
		if (sata_usb_pad_pll_reset_deassert())
			dev_err(&pdev->dev, "error deassert sata pll\n");
	}

	/* calculate rctrl_val and tctrl_val once at boot time */
	/* Issue is only applicable for T114 */
	if (XUSB_DEVICE_ID_T114 == tegra->device_id)
		tegra_xhci_war_for_tctrl_rctrl(tegra);

	for_each_enabled_hsic_pad(pad, tegra)
		hsic_power_rail_enable(tegra);

	/* Program the XUSB pads to take ownership of ports */
	tegra_xhci_padctl_portmap_and_caps(tegra);

	/* Enable Vbus of host ports */
	for_each_enabled_utmi_pad(pad, tegra) {
		if (tegra->bdata->vbus_en_oc[pad].type == VBUS_EN_OC)
			padctl_enable_usb_vbus(tegra, pad);
	}

	/* Release XUSB wake logic state latching */
	tegra_xhci_ss_wake_signal(tegra->bdata->portmap, false);
	tegra_xhci_ss_vcore(tegra->bdata->portmap, false);

	/* Perform USB2.0 pad tracking */
#ifdef CONFIG_ARCH_TEGRA_21x_SOC
	utmi_phy_pad_enable(tegra->prod_list);
#else
	utmi_phy_pad_enable();
#endif
	utmi_phy_iddq_override(false);

	platform_set_drvdata(pdev, tegra);

	ret = load_firmware(tegra, false /* do reset ARU */);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to load firmware\n");
		return -ENODEV;
	}
	pmc_init(tegra);

	device_init_wakeup(&pdev->dev, 1);
	driver = &tegra_plat_xhci_driver;

	hcd = usb_create_hcd(driver, &pdev->dev, dev_name(&pdev->dev));
	if (!hcd) {
		dev_err(&pdev->dev, "failed to create usb2 hcd\n");
		return -ENOMEM;
	}

	/* request resource host base address */
	ret = tegra_xhci_request_mem_region(pdev, 0, &hcd->regs);
	if (ret) {
		dev_err(&pdev->dev, "failed to map host\n");
		goto err_put_usb2_hcd;
	}
	hcd->rsrc_start = tegra->host_phy_base;
	hcd->rsrc_len = tegra->host_phy_size;

	/* Register interrupt handler for HOST */
	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		dev_err(&pdev->dev, "irq resource host doesn't exist\n");
		ret = -ENODEV;
		goto err_put_usb2_hcd;
	}

	if (!IS_ERR_OR_NULL(tegra->transceiver))
		hcd->usb_phy = tegra->transceiver;

	platform_set_drvdata(pdev, tegra);
	irq = res->start;
	ret = usb_add_hcd(hcd, irq, IRQF_SHARED);
	if (ret) {
		dev_err(&pdev->dev, "failed to add usb2hcd, error = %d\n", ret);
		goto err_put_usb2_hcd;
	}

	xhci = hcd_to_xhci(hcd);
	tegra->xhci = xhci;

	if (!IS_ERR_OR_NULL(tegra->transceiver))
		xhci->phy = tegra->transceiver;

	xhci->shared_hcd = usb_create_shared_hcd(driver, &pdev->dev,
						dev_name(&pdev->dev), hcd);
	if (!xhci->shared_hcd) {
		dev_err(&pdev->dev, "failed to create usb3 hcd\n");
		ret = -ENOMEM;
		goto err_remove_usb2_hcd;
	}
	if (!IS_ERR_OR_NULL(tegra->transceiver))
		xhci->shared_hcd->usb_phy = tegra->transceiver;

	/*
	 * Set the xHCI pointer before xhci_plat_setup() (aka hcd_driver.reset)
	 * is called by usb_add_hcd().
	 */
	*((struct xhci_hcd **) xhci->shared_hcd->hcd_priv) = xhci;

	if (!IS_ERR_OR_NULL(tegra->transceiver))
		xhci->shared_hcd->usb_phy = tegra->transceiver;

	ret = usb_add_hcd(xhci->shared_hcd, irq, IRQF_SHARED);
	if (ret) {
		dev_err(&pdev->dev, "failed to add usb3hcd, error = %d\n", ret);
		goto err_put_usb3_hcd;
	}

	if (is_tegra_hypervisor_mode()) {
		pm_runtime_disable(&hcd_to_bus(hcd)->root_hub->dev);
		pm_runtime_disable(
			&hcd_to_bus(xhci->shared_hcd)->root_hub->dev);
	}

	device_init_wakeup(&hcd->self.root_hub->dev, 1);
	device_init_wakeup(&xhci->shared_hcd->self.root_hub->dev, 1);

	/* do mailbox related initializations */
	tegra->mbox_owner = 0xffff;
	INIT_WORK(&tegra->mbox_work, tegra_xhci_process_mbox_message);
	tegra->mbox_wq = alloc_ordered_workqueue("mbox_wq", 0);
	if (IS_ERR_OR_NULL(tegra->mbox_wq)) {
		dev_err(&pdev->dev, "failed to alloc workqueue\n");
		ret = -ENOMEM;
		goto err_remove_usb3_hcd;
	}

	/* do ss partition elpg exit related initialization */
	INIT_WORK(&tegra->ss_elpg_exit_work, ss_partition_elpg_exit_work);

	/* do host partition elpg exit related initialization */
	INIT_WORK(&tegra->host_elpg_exit_work, host_partition_elpg_exit_work);

	INIT_WORK(&tegra->xotg_vbus_work, tegra_xotg_vbus_work);

	/* do oc handling work */
	INIT_WORK(&tegra->oc_handling_work, tegra_xhci_handle_oc_condition);

	/* do otg port reset sspi work initialization */
	INIT_WORK(&tegra->reset_otg_sspi_work, tegra_xhci_reset_otg_sspi_work);

	/* FW mailbox ACK wait queue initialization */
	init_waitqueue_head(&tegra->fw_ack_wq);

	/* Init pm qos for cpu boost */
	tegra_xusb_boost_cpu_init(tegra);

	/* Register interrupt handler for SMI line to handle mailbox
	 * interrupt from firmware
	 */

	ret = tegra_xhci_request_irq(pdev, 1, tegra_xhci_smi_irq,
			IRQF_SHARED, "tegra_xhci_mbox_irq", &tegra->smi_irq);
	if (ret != 0)
		goto err_remove_usb3_hcd;

	/* Register interrupt handler for PADCTRL line to
	 * handle wake on connect irqs interrupt from
	 * firmware
	 */
	ret = tegra_xhci_request_irq(pdev, 2, tegra_xhci_padctl_irq,
			IRQF_SHARED | IRQF_TRIGGER_HIGH,
			"tegra_xhci_padctl_irq", &tegra->padctl_irq);
	if (ret != 0)
		goto err_remove_usb3_hcd;

	/* Register interrupt wake handler for USB2 */
	ret = tegra_xhci_request_irq(pdev, 4, pmc_usb_phy_wake_isr,
		IRQF_SHARED | IRQF_TRIGGER_HIGH, "pmc_usb_phy_wake_isr",
		&tegra->usb2_irq);
	if (ret != 0)
		goto err_remove_usb3_hcd;

	/* Register interrupt wake handler for USB3 */
	ret = tegra_xhci_request_irq(pdev, 3, pmc_usb_phy_wake_isr,
		IRQF_SHARED | IRQF_TRIGGER_HIGH, "pmc_usb_phy_wake_isr",
		&tegra->usb3_irq);
	if (ret != 0)
		goto err_remove_usb3_hcd;

	tegra->ctle_ctx_saved = 0;
	tegra->dfe_ctx_saved = 0;

	tegra_xhci_enable_fw_message(tegra);
	hsic_pad_pretend_connect(tegra);

	tegra_xhci_debug_read_pads(tegra);

	tegra_pd_add_device(&pdev->dev);
	pm_runtime_enable(&pdev->dev);

	hsic_power_create_file(tegra);
	tegra->init_done = true;

	if (xhci->quirks & XHCI_LPM_SUPPORT)
		hcd_to_bus(xhci->shared_hcd)->root_hub->lpm_capable = 1;

	/* For otg port, init PortSc.PP to off. */
	if (tegra->bdata->otg_portmap & 0xff) {
		portsc = xhci_readl(xhci,
				xhci->usb3_ports[tegra->ss_otg_portnum]);
		portsc &= ~PORT_POWER;
		xhci_writel(xhci, portsc,
				xhci->usb3_ports[tegra->ss_otg_portnum]);
	}

	if (!IS_ERR_OR_NULL(tegra->transceiver)) {
		tegra->otgnb.notifier_call = tegra_xhci_otg_notify;
		usb_register_notifier(tegra->transceiver, &tegra->otgnb);
		otg_set_host(tegra->transceiver->otg, &hcd->self);
		otg_set_xhci_host(tegra->transceiver->otg,
			&xhci->shared_hcd->self);
	}

	reinit_started = false;
	return 0;

err_remove_usb3_hcd:
	usb_remove_hcd(xhci->shared_hcd);
err_put_usb3_hcd:
	usb_put_hcd(xhci->shared_hcd);
err_remove_usb2_hcd:
	usb_remove_hcd(hcd);
err_put_usb2_hcd:
	usb_put_hcd(hcd);
err_deinit_xusb_partition_clk:
	tegra_xusb_partitions_clk_deinit(tegra);
err_deinit_usb2_clocks:
	tegra_usb2_clocks_deinit(tegra);
err_deinit_tegra_xusb_regulator:
	tegra_xusb_regulator_deinit(tegra);
err_put_otg_transceiver:
	if (tegra->transceiver)
		usb_put_phy(tegra->transceiver);

	if (tegra->prod_list)
		tegra_prod_release(&tegra->prod_list);

	return ret;
}

static int tegra_xhci_remove(struct platform_device *pdev)
{
	struct tegra_xhci_hcd *tegra = platform_get_drvdata(pdev);
	unsigned pad;
#if defined(CONFIG_ARCH_TEGRA_21x_SOC)
	u32 port;
#endif
	int partition_id_xusba, partition_id_xusbc;

	if (tegra == NULL)
		return -EINVAL;

	mutex_lock(&tegra->sync_lock);

	for_each_enabled_hsic_pad(pad, tegra) {
		hsic_pad_disable(tegra, pad);
		hsic_power_rail_disable(tegra);
	}

	pm_runtime_disable(&pdev->dev);

	if (tegra->init_done) {
		struct xhci_hcd	*xhci = NULL;
		struct usb_hcd *hcd = NULL;

		tegra_xusb_boost_cpu_deinit(tegra);

#ifdef CONFIG_ARCH_TEGRA_21x_SOC
		utmi_phy_pad_disable(tegra->prod_list);
#else
		utmi_phy_pad_disable();
#endif
		utmi_phy_iddq_override(true);

		xhci = tegra->xhci;
		hcd = xhci_to_hcd(xhci);

		devm_free_irq(&pdev->dev, tegra->usb3_irq, tegra);
		devm_free_irq(&pdev->dev, tegra->usb2_irq, tegra);
		devm_free_irq(&pdev->dev, tegra->padctl_irq, tegra);
		devm_free_irq(&pdev->dev, tegra->smi_irq, tegra);
		usb_remove_hcd(xhci->shared_hcd);
		usb_put_hcd(xhci->shared_hcd);
		usb_remove_hcd(hcd);
		usb_put_hcd(hcd);
		kfree(xhci);

		flush_workqueue(tegra->mbox_wq);
		destroy_workqueue(tegra->mbox_wq);

#if defined(CONFIG_ARCH_TEGRA_21x_SOC)
		/* By default disable the BATTERY_CHRG_OTGPAD for all ports */
		for (port = 0; port <= XUSB_UTMI_COUNT; port++)
			t210_disable_battery_circuit(tegra, port);
#endif
		for_each_enabled_utmi_pad(pad, tegra)
			xusb_utmi_pad_deinit(pad);

		for_each_ss_pad(pad, tegra->soc_config->ss_pad_count) {
			if (tegra->bdata->portmap & (1 << pad))
				xusb_ss_pad_deinit(pad);
		}
		if (XUSB_DEVICE_ID_T114 != tegra->device_id)
			usb3_phy_pad_disable();

		tegra->init_done = false;
	}
	deinit_firmware(tegra);
	fw_log_deinit(tegra);

	if (pex_usb_pad_pll_reset_assert())
		pr_err("error assert pex pll\n");

	if (xusb_use_sata_lane(tegra)) {
		if (sata_usb_pad_pll_reset_assert())
			pr_err("error assert sata pll\n");
	}

	if (!tegra->hc_in_elpg) {
		partition_id_xusba = tegra_pd_get_powergate_id(tegra_xusba_pd);
		if (partition_id_xusba < 0)
			return -EINVAL;

		partition_id_xusbc = tegra_pd_get_powergate_id(tegra_xusbc_pd);
		if (partition_id_xusbc < 0)
			return -EINVAL;
		tegra_powergate_partition_with_clk_off(partition_id_xusba);
		tegra_powergate_partition_with_clk_off(partition_id_xusbc);
	}

	tegra_xusb_regulator_deinit(tegra);

	if (tegra->transceiver) {
		usb_put_phy(tegra->transceiver);
		usb_unregister_notifier(tegra->transceiver, &tegra->otgnb);
		otg_set_host(tegra->transceiver->otg, NULL);
		otg_set_xhci_host(tegra->transceiver->otg, NULL);
	}

	tegra_usb2_clocks_deinit(tegra);
	if (!tegra->hc_in_elpg)
		tegra_xusb_partitions_clk_deinit(tegra);

	if (tegra->prod_list)
		tegra_prod_release(&tegra->prod_list);

	tegra_pd_remove_device(&pdev->dev);
	platform_set_drvdata(pdev, NULL);

	hsic_power_remove_file(tegra);
	mutex_destroy(&tegra->sync_lock);
	mutex_destroy(&tegra->mbox_lock);
	mutex_unlock(&tegra->sync_lock);

	return 0;
}

static void tegra_xhci_shutdown(struct platform_device *pdev)
{
	struct tegra_xhci_hcd *tegra = platform_get_drvdata(pdev);
	struct xhci_hcd	*xhci = NULL;
	struct usb_hcd *hcd = NULL;

	if (tegra == NULL)
		return;

	if (tegra->hc_in_elpg) {
		pmc_disable_bus_ctrl(tegra);
	} else {
		xhci = tegra->xhci;
		hcd = xhci_to_hcd(xhci);
		xhci_shutdown(hcd);
	}
	tegra_xusb_boost_cpu_deinit(tegra);
}

static struct platform_driver tegra_xhci_driver = {
	.probe	= tegra_xhci_probe,
	.remove	= tegra_xhci_remove,
	.shutdown = tegra_xhci_shutdown,
#ifdef CONFIG_PM
	.suspend = tegra_xhci_suspend,
	.resume  = tegra_xhci_resume,
#endif
	.driver	= {
		.name = "tegra-xhci",
		.of_match_table = tegra_xhci_of_match,
	},
};
MODULE_ALIAS("platform:tegra-xhci");

static int tegra_xhci_register_plat(void)
{
	return platform_driver_register(&tegra_xhci_driver);
}

static void tegra_xhci_unregister_plat(void)
{
	platform_driver_unregister(&tegra_xhci_driver);
}

static void xhci_reinit_work(struct work_struct *work)
{
	if (reinit_started == false) {
		reinit_started = true;
		tegra_xhci_unregister_plat();
		usleep_range(10, 20);
		tegra_xhci_register_plat();
#ifdef CONFIG_NV_GAMEPAD_RESET
		udelay(10);
		tegra_loki_gamepad_reset();
#endif
	}
}
