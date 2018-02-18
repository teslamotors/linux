/*
 * xhci-tegra-legacy.h - Nvidia xHCI host controller related data
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

#ifndef __XUSB_H
#define __XUSB_H

#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/circ_buf.h>
#include <linux/device.h>
#include <linux/notifier.h>
#ifdef CONFIG_TEGRA_EHCI_BOOST_CPU_FREQ
#include <linux/pm_qos.h>
#endif

#include <mach/xusb.h>

#ifdef CONFIG_NV_GAMEPAD_RESET
extern void gamepad_reset_war(void);
#endif

#ifdef CONFIG_USB_OTG_WAKELOCK
extern void otgwl_acquire_temp_lock(void);
#endif
#define XUSB_CSB_MP_L2IMEMOP_TRIG				0x00101A14
#define XUSB_CSB_MEMPOOL_L2IMEMOP_RESULT			0x00101A18
#define XUSB_CSB_MP_APMAP					0x0010181C
#define XUSB_CSB_ARU_SCRATCH0				0x00100100
#define XUSB_CSB_RST_SSPI				0x00100408

/* Nvidia Cfg Registers */

#define XUSB_CFG_0					0x00000000
#define XUSB_CFG_1					0x00000004
#define XUSB_CFG_4					0x00000010
#define XUSB_CFG_16					0x00000040
#define XUSB_CFG_24					0x00000060
#define XUSB_CFG_FPCICFG					0x000000F8
#define XUSB_CFG_ARU_C11_CSBRANGE				0x0000041C
#define XUSB_CFG_ARU_SMI_INTR				0x00000428
#define XUSB_CFG_ARU_RST					0x0000042C
#define XUSB_CFG_ARU_SMI_INTR				0x00000428
#define XUSB_CFG_ARU_CONTEXT				0x0000043C
#define XUSB_CFG_ARU_FW_SCRATCH				0x00000440
#define XUSB_CFG_CSB_BASE_ADDR				0x00000800
#define XUSB_CFG_ARU_CONTEXT_HSFS_SPEED			0x00000480
#define XUSB_CFG_ARU_CONTEXT_HS_PLS			0x00000478
#define XUSB_CFG_ARU_CONTEXT_FS_PLS			0x0000047C
#define XUSB_CFG_ARU_CONTEXT_HSFS_SPEED			0x00000480
#define XUSB_CFG_ARU_CONTEXT_HSFS_PP			0x00000484
#define XUSB_CFG_CSB_BASE_ADDR				0x00000800

/* DEVICE ID */
#define XUSB_DEVICE_ID_T114				0x0E16
#define XUSB_DEVICE_ID_T124				0x0FA3
#define XUSB_DEVICE_ID_T210				0x0FAC
#define XUSB_IS_T114(t)	(t->device_id == XUSB_DEVICE_ID_T114)
#define XUSB_IS_T124(t)	(t->device_id == XUSB_DEVICE_ID_T124)
#define XUSB_IS_T114_OR_T124(t)			\
	((t->device_id == XUSB_DEVICE_ID_T114) ||	\
		(t->device_id == XUSB_DEVICE_ID_T124))
#define XUSB_IS_T210(t)	(t->device_id == XUSB_DEVICE_ID_T210)

/* TODO: Do not have the definitions of below
 * registers.
 */

/* IPFS Registers to save and restore  */
#define	IPFS_XUSB_HOST_MSI_BAR_SZ_0			0xC0
#define	IPFS_XUSB_HOST_MSI_AXI_BAR_ST_0			0xC4
#define	IPFS_XUSB_HOST_FPCI_BAR_ST_0			0xC8
#define	IPFS_XUSB_HOST_MSI_VEC0_0			0x100
#define	IPFS_XUSB_HOST_MSI_EN_VEC0_0			0x140
#define	IPFS_XUSB_HOST_CONFIGURATION_0			0x180
#define	IPFS_XUSB_HOST_FPCI_ERROR_MASKS_0		0x184
#define	IPFS_XUSB_HOST_INTR_MASK_0			0x188
#define	IPFS_XUSB_HOST_IPFS_INTR_ENABLE_0		0x198
#define	IPFS_XUSB_HOST_UFPCI_CONFIG_0			0x19C
#define	IPFS_XUSB_HOST_CLKGATE_HYSTERESIS_0		0x1BC
#define IPFS_XUSB_HOST_MCCIF_FIFOCTRL_0			0x1DC

/* IPFS bit definitions */
#define IPFS_EN_FPCI					(1 << 0)
#define IPFS_IP_INT_MASK				(1 << 16)

/* Nvidia MailBox Registers */

#define XUSB_CFG_ARU_MBOX_CMD		0xE4
#define XUSB_CFG_ARU_MBOX_DATA_IN	0xE8
#define  CMD_DATA_SHIFT		(0)
#define  CMD_DATA_MASK			(0xFFFFFF)
#define  CMD_DATA(_x)			((_x & CMD_DATA_MASK) << CMD_DATA_SHIFT)
#define  CMD_TYPE_SHIFT		(24)
#define  CMD_TYPE_MASK			(0xFF)
#define  CMD_TYPE(_x)			((_x & CMD_TYPE_MASK) << CMD_TYPE_SHIFT)
#define XUSB_CFG_ARU_MBOX_DATA_OUT	0xEC
#define XUSB_CFG_ARU_MBOX_OWNER	0xF0

/* Nvidia Falcon Registers */
#define XUSB_FALC_CPUCTL					0x00000100
#define XUSB_FALC_BOOTVEC					0x00000104
#define XUSB_FALC_DMACTL					0x0000010C
#define XUSB_FALC_IMFILLRNG1					0x00000154
#define XUSB_FALC_IMFILLCTL					0x00000158
#define XUSB_FALC_CMEMBASE					0x00000160
#define XUSB_FALC_DMEMAPERT					0x00000164
#define XUSB_FALC_IMEMC_START					0x00000180
#define XUSB_FALC_IMEMD_START					0x00000184
#define XUSB_FALC_IMEMT_START					0x00000188
#define XUSB_FALC_ICD_CMD					0x00000200
#define XUSB_FALC_ICD_RDATA					0x0000020C
#define XUSB_FALC_SS_PVTPORTSC1					0x00116000
#define XUSB_FALC_SS_PVTPORTSC2					0x00116004
#define XUSB_FALC_SS_PVTPORTSC3					0x00116008
#define XUSB_FALC_HS_PVTPORTSC1					0x00116800
#define XUSB_FALC_HS_PVTPORTSC2					0x00116804
#define XUSB_FALC_HS_PVTPORTSC3					0x00116808
#define XUSB_FALC_FS_PVTPORTSC1					0x00117000
#define XUSB_FALC_FS_PVTPORTSC2					0x00117004
#define XUSB_FALC_FS_PVTPORTSC3					0x00117008

#define XUSB_FALC_STATE_HALTED					0x00000010
/* Nvidia mailbox constants */
#define MBOX_INT_EN			(1 << 31)
#define MBOX_XHCI_INT_EN	(1 << 30)
#define MBOX_SMI_INT_EN		(1 << 29)
#define MBOX_PME_INT_EN		(1 << 28)
#define MBOX_FALC_INT_EN	(1 << 27)

#define MBOX_OWNER_FW						1
#define MBOX_OWNER_SW						2
#define MBOX_OWNER_ID_MASK					0xFF

#define SMI_INTR_STATUS_MBOX				(1 << 3)
#define SMI_INTR_STATUS_FW_REINIT			(1 << 1)

/* PMC Register */
#define PMC_SCRATCH34						0x124

#define TEGRA_POWERGATE_XUSBA	20
#define TEGRA_POWERGATE_XUSBB	21
#define TEGRA_POWERGATE_XUSBC	22

/* Nvidia Constants */
#define IMEM_BLOCK_SIZE						256

#define XUSB_CSB_MEMPOOL_L2IMEMOP_RESULT_VLD			(1 << 31)
#define MEMAPERT_ENABLE						0x00000010
#define DMEMAPERT_ENABLE_INIT				0x00000000
#define CPUCTL_STARTCPU						0x00000002
#define L2IMEMOP_SIZE_SRC_OFFSET_SHIFT		8
#define L2IMEMOP_SIZE_SRC_OFFSET_MASK		0x3ff
#define L2IMEMOP_SIZE_SRC_COUNT_SHIFT		24
#define L2IMEMOP_SIZE_SRC_COUNT_MASK		0xff
#define L2IMEMOP_TRIG_LOAD_LOCKED_SHIFT	24
#define IMFILLRNG_TAG_MASK			0xffff
#define IMFILLRNG1_TAG_HI_SHIFT		16
#define APMAP_BOOTPATH						(1 << 31)
#define L2IMEM_INVALIDATE_ALL				0x40000000
#define L2IMEM_LOAD_LOCKED_RESULT			(0x11 << 24)
#define FW_SIZE_OFFSET						0x64
#define HSIC_PORT1	0
#define HSIC_PORT0	1
#define ULPI_PORT	2
#define OTG_PORT1	3
#define OTG_PORT2	4

/* Nvidia Host Controller Device and Vendor ID */
#define XUSB_USB_DID		0xE16
#define XUSB_USB_VID		0x10DE

/* Nvidia CSB MP Registers */
#define XUSB_CSB_MP_ILOAD_ATTR		0x00101A00
#define XUSB_CSB_MP_ILOAD_BASE_LO		0x00101A04
#define XUSB_CSB_MP_ILOAD_BASE_HI		0x00101A08
#define XUSB_CSB_MP_L2IMEMOP_SIZE		0x00101A10

/*Nvidia CFG registers */
#define XUSB_CFG_ARU_CONTEXT_HSFS_SPEED	0x00000480
#define XUSB_CFG_ARU_CONTEXT_HS_PLS		0x00000478
#define XUSB_CFG_ARU_CONTEXT_FS_PLS		0x0000047C
#define ARU_CONTEXT_HSFS_PP				0x00000484
#define ARU_ULPI_REGACCESS				0x474
#define ARU_ULPI_REGACCESS_ADDR_MASK	0xff00
#define ARU_ULPI_REGACCESS_CMD_MASK		0x1
#define ARU_ULPI_REGACCESS_DATA_MASK	0xff0000

#define ARU_CONTEXT_HS_PLS_SUSPEND	3
#define ARU_CONTEXT_HS_PLS_FS_MODE	6

#define GET_SS_PORTMAP(map, p)		(((map) >> 4*(p)) & 0xF)

#define reg_dump(_dev, _base, _reg)					\
	dev_dbg(_dev, "%s: %s @%x = 0x%x\n", __func__, #_reg,		\
		_reg, readl(_base + _reg))

/*
 * FIXME: looks like no any .c requires below structure types
 * revisit and decide whether we can delete or not
 */
struct usb2_pad_port_map {
	u32 hsic_port0;
	u32 hsic_port1;
	u32 ulpi_port;
	u32 otg_port1;
	u32 otg_port0;
};

enum hsic_pad_pupd {
	PUPD_DISABLE = 0,
	PUPD_IDLE,
	PUPD_RESET
};

struct usb2_otg_caps {
	u32 port0_cap;
	u32 port0_internal;
	u32 port1_cap;
	u32 port1_internal;
};

struct usb2_ulpi_caps {
	u32	port_cap;
	u32 port_internal;
};

/* this is used to assign the SuperSpeed port mapping
 * to USB2.0 ports owned by XUSB, where the SuperSpeed ports inherit
 * their port capabilities from the USB2.0 ports they mapped to*/

struct usb2_ss_port_map {
	u32	port0;
	u32 port1;
};

struct hsic_pad0_ctl_0_vals {
	u32 tx_rtunep;
	u32 tx_rtunen;
	u32 tx_slewp;
	u32 tx_slewn;
	u32 hsic_opt;
};

struct hsic_pad0_ctl_1_vals {
	u32	tx_rtunep;
	u32 tx_rtunen;
	u32 tx_slewp;
	u32 tx_slewn;
	u32 hsic_opt;
};

struct snps_oc_map_0 {
	u32 controller1_oc;
	u32 controller2_oc;
	u32 controller3_oc;
};

struct usb2_oc_map_0 {
	u32 port0;
	u32 port1;
};

struct vbus_enable_oc_map {
	u32 vbus_en0;
	u32 vbus_en1;
};

struct xusb_save_regs {
	u32 msi_bar_sz;
	u32 msi_axi_barst;
	u32 msi_fpci_barst;
	u32 msi_vec0;
	u32 msi_en_vec0;
	u32 fpci_error_masks;
	u32 intr_mask;
	u32 ipfs_intr_enable;
	u32 ufpci_config;
	u32 clkgate_hysteresis;
	u32 xusb_host_mccif_fifo_cntrl;

	/* PG does not mention below */
	u32 hs_pls;
	u32 fs_pls;
	u32 hs_fs_speed;
	u32 hs_fs_pp;
	u32 cfg_aru;
	u32 cfg_order;
	u32 cfg_fladj;
	u32 cfg_sid;
	/* DFE and CTLE */
	u32 tap1_val[XUSB_SS_PORT_COUNT];
	u32 amp_val[XUSB_SS_PORT_COUNT];
	u32 ctle_z_val[XUSB_SS_PORT_COUNT];
	u32 ctle_g_val[XUSB_SS_PORT_COUNT];
};

struct tegra_xhci_firmware {
	void *data; /* kernel virtual address */
	size_t size; /* firmware size */
	dma_addr_t dma; /* dma address for controller */
};

struct tegra_xhci_firmware_log {
	dma_addr_t phys_addr;		/* dma-able address */
	void *virt_addr;		/* kernel va of the shared log buffer */
	struct log_entry *dequeue;	/* current dequeue pointer (va) */
	struct circ_buf circ;		/* big circular buffer */
	u32 seq;			/* log sequence number */

	struct task_struct *thread;	/* a thread to consume log */
	struct mutex mutex;
	wait_queue_head_t read_wait;
	wait_queue_head_t write_wait;
	wait_queue_head_t intr_wait;
	struct dentry *path;
	struct dentry *log_file;
	unsigned long flags;
};

struct tegra_xhci_hcd {
	struct platform_device *pdev;
	struct xhci_hcd *xhci;
	u16 device_id;

	spinlock_t lock;
	struct mutex sync_lock;

	int smi_irq;
	int padctl_irq;
	int usb3_irq;
	int usb2_irq;

	bool ss_wake_event;
	bool ss_pwr_gated;
	bool host_pwr_gated;
	bool hs_wake_event;
	bool host_resume_req;
	bool lp0_exit;
	u32 dfe_ctx_saved;
	u32 ctle_ctx_saved;
	unsigned long last_jiffies;
	unsigned long host_phy_base;
	unsigned long host_phy_size;
	void __iomem *host_phy_virt_base;

	void __iomem *padctl_base;
	void __iomem *fpci_base;
	void __iomem *ipfs_base;

	struct tegra_xusb_platform_data *pdata;
	struct tegra_xusb_board_data *bdata;
	struct tegra_xusb_chip_calib *cdata;
	const struct tegra_xusb_padctl_regs *padregs;
	const struct tegra_xusb_soc_config *soc_config;
	u64 tegra_xusb_dmamask;

	/* mailbox variables */
	struct mutex mbox_lock;
	struct mutex mbox_lock_ack; /* for sending mbox which needs FW ACK */
	u32 mbox_owner;
	u32 cmd_type;
	u32 cmd_data;
	u32 fw_ack;		    /* storing the mbox cmd_type from FW */
	wait_queue_head_t fw_ack_wq;/* sleep support for FW to SW mbox ack */

	bool otg_port_owned;
	bool otg_port_ownership_changed;
	u32  hs_otg_portnum;
	u32  ss_otg_portnum;

	struct regulator **xusb_utmi_vbus_regs;

	struct regulator *xusb_s1p05v_reg;
	struct regulator *xusb_s3p3v_reg;
	struct regulator *xusb_s1p8v_reg;
	struct regulator *vddio_hsic_reg;
	int vddio_hsic_refcnt;

	struct work_struct mbox_work;
	struct workqueue_struct *mbox_wq;
	struct work_struct ss_elpg_exit_work;
	struct work_struct host_elpg_exit_work;
	struct work_struct xotg_vbus_work;
	struct work_struct oc_handling_work;
	struct work_struct reset_otg_sspi_work;

	struct clk *host_clk;
	struct clk *ss_clk;

	/* XUSB Falcon SuperSpeed Clock */
	struct clk *falc_clk;

	/* EMC Clock */
	struct clk *emc_clk;
	/* XUSB SS PI Clock */
	struct clk *ss_src_clk;
	/* PLLE Clock */
	struct clk *plle_clk;
	struct clk *pll_u_480M;
	struct clk *clk_m;
	/* refPLLE clk */
	struct clk *pll_re_vco_clk;
	/*
	 * XUSB/IPFS specific registers these need to be saved/restored in
	 * addition to spec defined registers
	 */
	struct xusb_save_regs sregs;
	bool usb2_rh_suspend;
	bool usb3_rh_suspend;
	bool hc_in_elpg;
	bool system_in_lp0;

	/* otg transceiver */
	struct usb_phy *transceiver;
	struct notifier_block otgnb;

	unsigned long usb2_rh_remote_wakeup_ports; /* one bit per port */
	unsigned long usb3_rh_remote_wakeup_ports; /* one bit per port */
	/* firmware loading related */
	struct tegra_xhci_firmware firmware;

	struct tegra_xhci_firmware_log log;
	struct device_attribute hsic_power_attr[XUSB_HSIC_COUNT];

	struct tegra_prod_list *prod_list;
	void __iomem *base_list[4];

#ifdef CONFIG_TEGRA_EHCI_BOOST_CPU_FREQ
	struct mutex boost_cpufreq_lock;
	struct pm_qos_request boost_cpufreq_req;
	struct pm_qos_request boost_cpuon_req;
	struct work_struct boost_cpufreq_work;
	struct delayed_work restore_cpufreq_work;
	unsigned long cpufreq_last_boosted;
	bool cpufreq_boosted;
	bool restore_cpufreq_scheduled;
	unsigned int boost_cpu_trigger;
#endif
	bool init_done;
	bool clock_enable_done;
};

#define NOT_SUPPORTED	0xFFFFFFFF
#define PADCTL_REG_NONE	0xffff
static inline u32 padctl_readl(struct tegra_xhci_hcd *tegra, u32 reg)
{
	if (reg == PADCTL_REG_NONE)
		return PADCTL_REG_NONE;
	return readl(tegra->padctl_base + reg);
}

static inline void padctl_writel(struct tegra_xhci_hcd *tegra, u32 val, u32 reg)
{
	if (reg == PADCTL_REG_NONE)
		return;
	writel(val, tegra->padctl_base + reg);
}

/**
 * port_to_hsic_pad - given "port number", return with hsic pad number
 * @_port:	(zero-based) index to portsc registers array.
 */
#if defined(CONFIG_ARCH_TEGRA_11x_SOC)
#define port_to_hsic_pad(_port) ({			\
	int _pad = -1;					\
	int __p = _port;				\
	if (__p == 5)					\
		_pad = 0;				\
	else if (__p == 6)				\
		_pad = 1;				\
	_pad; })
#define hsic_pad_to_port(_pad) ({			\
	int _port = -1;					\
	int __p = _pad;					\
	if (__p == 0)					\
		_port = 5;				\
	else if (__p == 1)				\
		_port = 6;				\
	_port; })
#elif defined(CONFIG_ARCH_TEGRA_12x_SOC)
#define port_to_hsic_pad(_port) ({			\
	int _pad = -1;					\
	int __p = _port;				\
	if (__p == 6)					\
		_pad = 0;				\
	else if (__p == 7)				\
		_pad = 1;				\
	_pad; })

#define hsic_pad_to_port(_pad) ({			\
	int _port = -1;					\
	int __p = _pad;					\
	if (__p == 0)					\
		_port = 6;				\
	else if (__p == 1)				\
		_port = 7;				\
	_port; })
#elif defined(CONFIG_ARCH_TEGRA_21x_SOC)
#define port_to_hsic_pad(_port) ({			\
	int _pad = -1;					\
	int __p = _port;				\
	if (__p == 8)					\
		_pad = 0;				\
	else if (__p == 9)				\
		_pad = 1;				\
	_pad; })

#define hsic_pad_to_port(_pad) ({			\
	int _port = -1;					\
	int __p = _pad;					\
	if (__p == 0)					\
		_port = 8;				\
	else if (__p == 1)				\
		_port = 9;				\
	_port; })
#endif
#endif
