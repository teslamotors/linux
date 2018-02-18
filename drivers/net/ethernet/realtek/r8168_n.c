/*
################################################################################
#
# r8168 is the Linux device driver released for Realtek Gigabit Ethernet
# controllers with PCI-Express interface.
#
# Copyright(c) 2015 Realtek Semiconductor Corp. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the Free
# Software Foundation; either version 2 of the License, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
# more details.
#
# You should have received a copy of the GNU General Public License along with
# this program; if not, see <http://www.gnu.org/licenses/>.
#
# Author:
# Realtek NIC software team <nicfae@realtek.com>
# No. 2, Innovation Road II, Hsinchu Science Park, Hsinchu 300, Taiwan
#
################################################################################
*/

/************************************************************************************
 *  This product is covered by one or more of the following patents:
 *  US6,570,884, US6,115,776, and US6,327,625.
 ***********************************************************************************/

/*
 * This driver is modified from r8169.c in Linux kernel 2.6.18
 */

#include <linux/module.h>
#include <linux/version.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/delay.h>
#include <linux/ethtool.h>
#include <linux/mii.h>
#include <linux/if_vlan.h>
#include <linux/crc32.h>
#include <linux/interrupt.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/init.h>
#include <linux/rtnetlink.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26)
#include <linux/pci-aspm.h>
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,37)
#include <linux/prefetch.h>
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
#define dev_printk(A,B,fmt,args...) printk(A fmt,##args)
#else
#include <linux/dma-mapping.h>
#include <linux/moduleparam.h>
#endif

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/uaccess.h>

#include "r8168.h"
#include "r8168_asf.h"
#include "rtl_eeprom.h"
#include "rtltool.h"

#ifdef ENABLE_R8168_PROCFS
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#endif

/* Maximum events (Rx packets, etc.) to handle at each interrupt. */
static const int max_interrupt_work = 20;

/* Maximum number of multicast addresses to filter (vs. Rx-all-multicast).
   The RTL chips use a 64 element hash table based on the Ethernet CRC. */
static const int multicast_filter_limit = 32;

#define _R(NAME,MAC,RCR,MASK, JumFrameSz) \
    { .name = NAME, .mcfg = MAC, .RCR_Cfg = RCR, .RxConfigMask = MASK, .jumbo_frame_sz = JumFrameSz }

static const struct {
        const char *name;
        u8 mcfg;
        u32 RCR_Cfg;
        u32 RxConfigMask;   /* Clears the bits supported by this chip */
        u32 jumbo_frame_sz;
} rtl_chip_info[] = {
        _R("RTL8168B/8111B",
        CFG_METHOD_1,
        (Reserved2_data << Reserved2_shift) | (RX_DMA_BURST << RxCfgDMAShift),
        0xff7e1880,
        Jumbo_Frame_4k),

        _R("RTL8168B/8111B",
        CFG_METHOD_2,
        (Reserved2_data << Reserved2_shift) | (RX_DMA_BURST << RxCfgDMAShift),
        0xff7e1880,
        Jumbo_Frame_4k),

        _R("RTL8168B/8111B",
        CFG_METHOD_3,
        (Reserved2_data << Reserved2_shift) | (RX_DMA_BURST << RxCfgDMAShift),
        0xff7e1880,
        Jumbo_Frame_4k),

        _R("RTL8168C/8111C",
        CFG_METHOD_4,
        RxCfg_128_int_en | RxCfg_fet_multi_en | (RX_DMA_BURST << RxCfgDMAShift),
        0xff7e1880,
        Jumbo_Frame_6k),

        _R("RTL8168C/8111C",
        CFG_METHOD_5,
        RxCfg_128_int_en | RxCfg_fet_multi_en | (RX_DMA_BURST << RxCfgDMAShift),
        0xff7e1880,
        Jumbo_Frame_6k),

        _R("RTL8168C/8111C",
        CFG_METHOD_6,
        RxCfg_128_int_en | RxCfg_fet_multi_en | (RX_DMA_BURST << RxCfgDMAShift),
        0xff7e1880,
        Jumbo_Frame_6k),

        _R("RTL8168CP/8111CP",
        CFG_METHOD_7,
        RxCfg_128_int_en | RxCfg_fet_multi_en | (RX_DMA_BURST << RxCfgDMAShift),
        0xff7e1880,
        Jumbo_Frame_6k),

        _R("RTL8168CP/8111CP",
        CFG_METHOD_8,
        RxCfg_128_int_en | RxCfg_fet_multi_en | (RX_DMA_BURST << RxCfgDMAShift),
        0xff7e1880,
        Jumbo_Frame_6k),

        _R("RTL8168D/8111D",
        CFG_METHOD_9,
        RxCfg_128_int_en | (RX_DMA_BURST << RxCfgDMAShift),
        0xff7e1880,
        Jumbo_Frame_9k),

        _R("RTL8168D/8111D",
        CFG_METHOD_10,
        RxCfg_128_int_en | (RX_DMA_BURST << RxCfgDMAShift),
        0xff7e1880,
        Jumbo_Frame_9k),

        _R("RTL8168DP/8111DP",
        CFG_METHOD_11,
        RxCfg_128_int_en | (RX_DMA_BURST << RxCfgDMAShift),
        0xff7e1880,
        Jumbo_Frame_9k),

        _R("RTL8168DP/8111DP",
        CFG_METHOD_12,
        RxCfg_128_int_en | (RX_DMA_BURST << RxCfgDMAShift),
        0xff7e1880,
        Jumbo_Frame_9k),

        _R("RTL8168DP/8111DP",
        CFG_METHOD_13,
        RxCfg_128_int_en | (RX_DMA_BURST << RxCfgDMAShift),
        0xff7e1880,
        Jumbo_Frame_9k),

        _R("RTL8168E/8111E",
        CFG_METHOD_14,
        RxCfg_128_int_en | (RX_DMA_BURST << RxCfgDMAShift),
        0xff7e1880,
        Jumbo_Frame_9k),

        _R("RTL8168E/8111E",
        CFG_METHOD_15,
        RxCfg_128_int_en | (RX_DMA_BURST << RxCfgDMAShift),
        0xff7e1880,
        Jumbo_Frame_9k),

        _R("RTL8168E-VL/8111E-VL",
        CFG_METHOD_16,
        RxCfg_128_int_en | RxEarly_off_V1 | (RX_DMA_BURST << RxCfgDMAShift),
        0xff7e0080,
        Jumbo_Frame_9k),

        _R("RTL8168E-VL/8111E-VL",
        CFG_METHOD_17,
        RxCfg_128_int_en | RxEarly_off_V1 | (RX_DMA_BURST << RxCfgDMAShift),
        0xff7e1880,
        Jumbo_Frame_9k),

        _R("RTL8168F/8111F",
        CFG_METHOD_18,
        RxCfg_128_int_en | RxEarly_off_V1 | (RX_DMA_BURST << RxCfgDMAShift),
        0xff7e1880,
        Jumbo_Frame_9k),

        _R("RTL8168F/8111F",
        CFG_METHOD_19,
        RxCfg_128_int_en | RxEarly_off_V1 | (RX_DMA_BURST << RxCfgDMAShift),
        0xff7e1880,
        Jumbo_Frame_9k),

        _R("RTL8411",
        CFG_METHOD_20,
        RxCfg_128_int_en | (RX_DMA_BURST << RxCfgDMAShift),
        0xff7e1880,
        Jumbo_Frame_9k),

        _R("RTL8168G/8111G",
        CFG_METHOD_21,
        RxCfg_128_int_en | RxEarly_off_V2 | Rx_Single_fetch_V2 | (RX_DMA_BURST << RxCfgDMAShift),
        0xff7e5880,
        Jumbo_Frame_9k),

        _R("RTL8168G/8111G",
        CFG_METHOD_22,
        RxCfg_128_int_en | RxEarly_off_V2 | Rx_Single_fetch_V2 | (RX_DMA_BURST << RxCfgDMAShift),
        0xff7e5880,
        Jumbo_Frame_9k),

        _R("RTL8168EP/8111EP",
        CFG_METHOD_23,
        RxCfg_128_int_en | RxEarly_off_V2 | Rx_Single_fetch_V2 | (RX_DMA_BURST << RxCfgDMAShift),
        0xff7e5880,
        Jumbo_Frame_9k),

        _R("RTL8168GU/8111GU",
        CFG_METHOD_24,
        RxCfg_128_int_en | RxEarly_off_V2 | Rx_Single_fetch_V2 | (RX_DMA_BURST << RxCfgDMAShift),
        0xff7e5880,
        Jumbo_Frame_9k),

        _R("RTL8168GU/8111GU",
        CFG_METHOD_25,
        RxCfg_128_int_en | RxEarly_off_V2 | Rx_Single_fetch_V2 | (RX_DMA_BURST << RxCfgDMAShift),
        0xff7e5880,
        Jumbo_Frame_9k),

        _R("8411B",
        CFG_METHOD_26,
        RxCfg_128_int_en | RxEarly_off_V2 | Rx_Single_fetch_V2 | (RX_DMA_BURST << RxCfgDMAShift),
        0xff7e5880,
        Jumbo_Frame_9k),

        _R("RTL8168EP/8111EP",
        CFG_METHOD_27,
        RxCfg_128_int_en | RxEarly_off_V2 | Rx_Single_fetch_V2 | (RX_DMA_BURST << RxCfgDMAShift),
        0xff7e5880,
        Jumbo_Frame_9k),

        _R("RTL8168EP/8111EP",
        CFG_METHOD_28,
        RxCfg_128_int_en | RxEarly_off_V2 | Rx_Single_fetch_V2 | (RX_DMA_BURST << RxCfgDMAShift),
        0xff7e5880,
        Jumbo_Frame_9k),

        _R("RTL8168H/8111H",
        CFG_METHOD_29,
        RxCfg_128_int_en | RxEarly_off_V2 | Rx_Single_fetch_V2 | (RX_DMA_BURST << RxCfgDMAShift),
        0xff7e5880,
        Jumbo_Frame_9k),

        _R("RTL8168H/8111H",
        CFG_METHOD_30,
        RxCfg_128_int_en | RxEarly_off_V2 | Rx_Single_fetch_V2 | (RX_DMA_BURST << RxCfgDMAShift),
        0xff7e5880,
        Jumbo_Frame_9k),

        _R("Unknown",
        CFG_METHOD_DEFAULT,
        (RX_DMA_BURST << RxCfgDMAShift),
        0xff7e5880,
        RX_BUF_SIZE)
};
#undef _R

#ifndef PCI_VENDOR_ID_DLINK
#define PCI_VENDOR_ID_DLINK 0x1186
#endif

static struct pci_device_id rtl8168_pci_tbl[] = {
        { PCI_DEVICE(PCI_VENDOR_ID_REALTEK, 0x8168), },
        { PCI_VENDOR_ID_DLINK, 0x4300, 0x1186, 0x4b10,},
        {0,},
};

MODULE_DEVICE_TABLE(pci, rtl8168_pci_tbl);

static int rx_copybreak = 0;
static int timer_count = 0x2600;

static struct {
        u32 msg_enable;
} debug = { -1 };

static unsigned short speed = SPEED_1000;
static int duplex = DUPLEX_FULL;
static int autoneg = AUTONEG_ENABLE;
#ifdef CONFIG_R8168_ASPM
static int aspm = 1;
#else
static int aspm = 0;
#endif
#ifdef CONFIG_R8168_S5WOL
static int s5wol = 1;
#else
static int s5wol = 0;
#endif
#ifdef ENABLE_EEE
static int eee_enable = 1;
#else
static int eee_enable = 0;
#endif
static ulong hwoptimize = 0;

MODULE_AUTHOR("Realtek and the Linux r8168 crew <netdev@vger.kernel.org>");
MODULE_DESCRIPTION("RealTek RTL-8168 Gigabit Ethernet driver");

module_param(speed, ushort, 0);
MODULE_PARM_DESC(speed, "force phy operation. Deprecated by ethtool (8).");

module_param(duplex, int, 0);
MODULE_PARM_DESC(duplex, "force phy operation. Deprecated by ethtool (8).");

module_param(autoneg, int, 0);
MODULE_PARM_DESC(autoneg, "force phy operation. Deprecated by ethtool (8).");

module_param(aspm, int, 0);
MODULE_PARM_DESC(aspm, "Enable ASPM.");

module_param(s5wol, int, 0);
MODULE_PARM_DESC(s5wol, "Enable Shutdown Wake On Lan.");

module_param(rx_copybreak, int, 0);
MODULE_PARM_DESC(rx_copybreak, "Copy breakpoint for copy-only-tiny-frames");

module_param(timer_count, int, 0);
MODULE_PARM_DESC(timer_count, "Timer Interrupt Interval.");

module_param(eee_enable, int, 0);
MODULE_PARM_DESC(eee_enable, "Enable Energy Efficient Ethernet.");

module_param(hwoptimize, ulong, 0);
MODULE_PARM_DESC(hwoptimize, "Enable HW optimization function.");

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
module_param_named(debug, debug.msg_enable, int, 0);
MODULE_PARM_DESC(debug, "Debug verbosity level (0=none, ..., 16=all)");
#endif//LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)

MODULE_LICENSE("GPL");

MODULE_VERSION(RTL8168_VERSION);

static void rtl8168_sleep_rx_enable(struct net_device *dev);
static void rtl8168_dsm(struct net_device *dev, int dev_state);

static void rtl8168_esd_timer(unsigned long __opaque);
static void rtl8168_link_timer(unsigned long __opaque);
static void rtl8168_tx_clear(struct rtl8168_private *tp);
static void rtl8168_rx_clear(struct rtl8168_private *tp);

static int rtl8168_open(struct net_device *dev);
static int rtl8168_start_xmit(struct sk_buff *skb, struct net_device *dev);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
static irqreturn_t rtl8168_interrupt(int irq, void *dev_instance, struct pt_regs *regs);
#else
static irqreturn_t rtl8168_interrupt(int irq, void *dev_instance);
#endif
static void rtl8168_rx_desc_offset0_init(struct rtl8168_private *, int);
static int rtl8168_init_ring(struct net_device *dev);
static void rtl8168_hw_config(struct net_device *dev);
static void rtl8168_hw_start(struct net_device *dev);
static int rtl8168_close(struct net_device *dev);
static void rtl8168_set_rx_mode(struct net_device *dev);
static void rtl8168_tx_timeout(struct net_device *dev);
static struct net_device_stats *rtl8168_get_stats(struct net_device *dev);
static int rtl8168_rx_interrupt(struct net_device *, struct rtl8168_private *, void __iomem *, u32 budget);
static int rtl8168_change_mtu(struct net_device *dev, int new_mtu);
static void rtl8168_down(struct net_device *dev);

static int rtl8168_set_mac_address(struct net_device *dev, void *p);
void rtl8168_rar_set(struct rtl8168_private *tp, uint8_t *addr);
static void rtl8168_desc_addr_fill(struct rtl8168_private *);
static void rtl8168_tx_desc_init(struct rtl8168_private *tp);
static void rtl8168_rx_desc_init(struct rtl8168_private *tp);

static void rtl8168_hw_reset(struct net_device *dev);

static void rtl8168_phy_power_up(struct net_device *dev);
static void rtl8168_phy_power_down(struct net_device *dev);
static int rtl8168_set_speed(struct net_device *dev, u8 autoneg,  u16 speed, u8 duplex);

#ifdef CONFIG_R8168_NAPI
static int rtl8168_poll(napi_ptr napi, napi_budget budget);
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
#undef ethtool_ops
#define ethtool_ops _kc_ethtool_ops

struct _kc_ethtool_ops {
        int  (*get_settings)(struct net_device *, struct ethtool_cmd *);
        int  (*set_settings)(struct net_device *, struct ethtool_cmd *);
        void (*get_drvinfo)(struct net_device *, struct ethtool_drvinfo *);
        int  (*get_regs_len)(struct net_device *);
        void (*get_regs)(struct net_device *, struct ethtool_regs *, void *);
        void (*get_wol)(struct net_device *, struct ethtool_wolinfo *);
        int  (*set_wol)(struct net_device *, struct ethtool_wolinfo *);
        u32  (*get_msglevel)(struct net_device *);
        void (*set_msglevel)(struct net_device *, u32);
        int  (*nway_reset)(struct net_device *);
        u32  (*get_link)(struct net_device *);
        int  (*get_eeprom_len)(struct net_device *);
        int  (*get_eeprom)(struct net_device *, struct ethtool_eeprom *, u8 *);
        int  (*set_eeprom)(struct net_device *, struct ethtool_eeprom *, u8 *);
        int  (*get_coalesce)(struct net_device *, struct ethtool_coalesce *);
        int  (*set_coalesce)(struct net_device *, struct ethtool_coalesce *);
        void (*get_ringparam)(struct net_device *, struct ethtool_ringparam *);
        int  (*set_ringparam)(struct net_device *, struct ethtool_ringparam *);
        void (*get_pauseparam)(struct net_device *,
                               struct ethtool_pauseparam*);
        int  (*set_pauseparam)(struct net_device *,
                               struct ethtool_pauseparam*);
        u32  (*get_rx_csum)(struct net_device *);
        int  (*set_rx_csum)(struct net_device *, u32);
        u32  (*get_tx_csum)(struct net_device *);
        int  (*set_tx_csum)(struct net_device *, u32);
        u32  (*get_sg)(struct net_device *);
        int  (*set_sg)(struct net_device *, u32);
        u32  (*get_tso)(struct net_device *);
        int  (*set_tso)(struct net_device *, u32);
        int  (*self_test_count)(struct net_device *);
        void (*self_test)(struct net_device *, struct ethtool_test *, u64 *);
        void (*get_strings)(struct net_device *, u32 stringset, u8 *);
        int  (*phys_id)(struct net_device *, u32);
        int  (*get_stats_count)(struct net_device *);
        void (*get_ethtool_stats)(struct net_device *, struct ethtool_stats *,
                                  u64 *);
} *ethtool_ops = NULL;

#undef SET_ETHTOOL_OPS
#define SET_ETHTOOL_OPS(netdev, ops) (ethtool_ops = (ops))

#endif //LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,16,0)
#ifndef SET_ETHTOOL_OPS
#define SET_ETHTOOL_OPS(netdev,ops) \
         ( (netdev)->ethtool_ops = (ops) )
#endif //SET_ETHTOOL_OPS
#endif //LINUX_VERSION_CODE >= KERNEL_VERSION(3,16,0)

//#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,5)
#ifndef netif_msg_init
#define netif_msg_init _kc_netif_msg_init
/* copied from linux kernel 2.6.20 include/linux/netdevice.h */
static inline u32 netif_msg_init(int debug_value, int default_msg_enable_bits)
{
        /* use default */
        if (debug_value < 0 || debug_value >= (sizeof(u32) * 8))
                return default_msg_enable_bits;
        if (debug_value == 0)   /* no output */
                return 0;
        /* set low N bits */
        return (1 << debug_value) - 1;
}

#endif //LINUX_VERSION_CODE < KERNEL_VERSION(2,6,5)

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,22)
static inline void eth_copy_and_sum (struct sk_buff *dest,
                                     const unsigned char *src,
                                     int len, int base)
{
        memcpy (dest->data, src, len);
}
#endif //LINUX_VERSION_CODE > KERNEL_VERSION(2,6,22)

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,7)
/* copied from linux kernel 2.6.20 /include/linux/time.h */
/* Parameters used to convert the timespec values: */
#define MSEC_PER_SEC    1000L

/* copied from linux kernel 2.6.20 /include/linux/jiffies.h */
/*
 * Change timeval to jiffies, trying to avoid the
 * most obvious overflows..
 *
 * And some not so obvious.
 *
 * Note that we don't want to return MAX_LONG, because
 * for various timeout reasons we often end up having
 * to wait "jiffies+1" in order to guarantee that we wait
 * at _least_ "jiffies" - so "jiffies+1" had better still
 * be positive.
 */
#define MAX_JIFFY_OFFSET ((~0UL >> 1)-1)

/*
 * Convert jiffies to milliseconds and back.
 *
 * Avoid unnecessary multiplications/divisions in the
 * two most common HZ cases:
 */
static inline unsigned int _kc_jiffies_to_msecs(const unsigned long j)
{
#if HZ <= MSEC_PER_SEC && !(MSEC_PER_SEC % HZ)
        return (MSEC_PER_SEC / HZ) * j;
#elif HZ > MSEC_PER_SEC && !(HZ % MSEC_PER_SEC)
        return (j + (HZ / MSEC_PER_SEC) - 1)/(HZ / MSEC_PER_SEC);
#else
        return (j * MSEC_PER_SEC) / HZ;
#endif
}

static inline unsigned long _kc_msecs_to_jiffies(const unsigned int m)
{
        if (m > _kc_jiffies_to_msecs(MAX_JIFFY_OFFSET))
                return MAX_JIFFY_OFFSET;
#if HZ <= MSEC_PER_SEC && !(MSEC_PER_SEC % HZ)
        return (m + (MSEC_PER_SEC / HZ) - 1) / (MSEC_PER_SEC / HZ);
#elif HZ > MSEC_PER_SEC && !(HZ % MSEC_PER_SEC)
        return m * (HZ / MSEC_PER_SEC);
#else
        return (m * HZ + MSEC_PER_SEC - 1) / MSEC_PER_SEC;
#endif
}
#endif  //LINUX_VERSION_CODE < KERNEL_VERSION(2,6,7)


#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,11)

/* copied from linux kernel 2.6.12.6 /include/linux/pm.h */
typedef int __bitwise pci_power_t;

/* copied from linux kernel 2.6.12.6 /include/linux/pci.h */
typedef u32 __bitwise pm_message_t;

#define PCI_D0  ((pci_power_t __force) 0)
#define PCI_D1  ((pci_power_t __force) 1)
#define PCI_D2  ((pci_power_t __force) 2)
#define PCI_D3hot   ((pci_power_t __force) 3)
#define PCI_D3cold  ((pci_power_t __force) 4)
#define PCI_POWER_ERROR ((pci_power_t __force) -1)

/* copied from linux kernel 2.6.12.6 /drivers/pci/pci.c */
/**
 * pci_choose_state - Choose the power state of a PCI device
 * @dev: PCI device to be suspended
 * @state: target sleep state for the whole system. This is the value
 *  that is passed to suspend() function.
 *
 * Returns PCI power state suitable for given device and given system
 * message.
 */

pci_power_t pci_choose_state(struct pci_dev *dev, pm_message_t state)
{
        if (!pci_find_capability(dev, PCI_CAP_ID_PM))
                return PCI_D0;

        switch (state) {
        case 0:
                return PCI_D0;
        case 3:
                return PCI_D3hot;
        default:
                printk("They asked me for state %d\n", state);
//      BUG();
        }
        return PCI_D0;
}
#endif  //LINUX_VERSION_CODE < KERNEL_VERSION(2,6,11)

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,9)
/**
 * msleep_interruptible - sleep waiting for waitqueue interruptions
 * @msecs: Time in milliseconds to sleep for
 */
#define msleep_interruptible _kc_msleep_interruptible
unsigned long _kc_msleep_interruptible(unsigned int msecs)
{
        unsigned long timeout = _kc_msecs_to_jiffies(msecs);

        while (timeout && !signal_pending(current)) {
                set_current_state(TASK_INTERRUPTIBLE);
                timeout = schedule_timeout(timeout);
        }
        return _kc_jiffies_to_msecs(timeout);
}
#endif  //LINUX_VERSION_CODE < KERNEL_VERSION(2,6,9)

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,7)
/* copied from linux kernel 2.6.20 include/linux/sched.h */
#ifndef __sched
#define __sched     __attribute__((__section__(".sched.text")))
#endif

/* copied from linux kernel 2.6.20 kernel/timer.c */
signed long __sched schedule_timeout_uninterruptible(signed long timeout)
{
        __set_current_state(TASK_UNINTERRUPTIBLE);
        return schedule_timeout(timeout);
}

/* copied from linux kernel 2.6.20 include/linux/mii.h */
#undef if_mii
#define if_mii _kc_if_mii
static inline struct mii_ioctl_data *if_mii(struct ifreq *rq)
{
        return (struct mii_ioctl_data *) &rq->ifr_ifru;
}
#endif  //LINUX_VERSION_CODE < KERNEL_VERSION(2,6,7)

static const char rtl8168_gstrings[][ETH_GSTRING_LEN] = {
        "tx_packets",
        "rx_packets",
        "tx_errors",
        "rx_errors",
        "rx_missed",
        "align_errors",
        "tx_single_collisions",
        "tx_multi_collisions",
        "unicast",
        "broadcast",
        "multicast",
        "tx_aborted",
        "tx_underrun",
};

struct rtl8168_counters {
        u64 tx_packets;
        u64 rx_packets;
        u64 tx_errors;
        u32 rx_errors;
        u16 rx_missed;
        u16 align_errors;
        u32 tx_one_collision;
        u32 tx_multi_collision;
        u64 rx_unicast;
        u64 rx_broadcast;
        u32 rx_multicast;
        u16 tx_aborted;
        u16 tx_underun;
};

#ifdef ENABLE_R8168_PROCFS
/****************************************************************************
*   -----------------------------PROCFS STUFF-------------------------
*****************************************************************************
*/

static struct proc_dir_entry *rtl8168_proc;
static int proc_init_num = 0;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
static int proc_get_driver_variable(struct seq_file *m, void *v)
{
        struct net_device *dev = m->private;
        struct rtl8168_private *tp = netdev_priv(dev);
        unsigned long flags;

        seq_puts(m, "\nDump Driver Variable\n");

        spin_lock_irqsave(&tp->lock, flags);
        seq_puts(m, "Variable\tValue\n----------\t-----\n");
        seq_printf(m, "MODULENAME\t%s\n", MODULENAME);
        seq_printf(m, "driver version\t%s\n", RTL8168_VERSION);
        seq_printf(m, "chipset\t%d\n", tp->chipset);
        seq_printf(m, "chipset_name\t%s\n", rtl_chip_info[tp->chipset].name);
        seq_printf(m, "mtu\t%d\n", dev->mtu);
        seq_printf(m, "NUM_RX_DESC\t0x%x\n", NUM_RX_DESC);
        seq_printf(m, "cur_rx\t0x%x\n", tp->cur_rx);
        seq_printf(m, "dirty_rx\t0x%x\n", tp->dirty_rx);
        seq_printf(m, "NUM_TX_DESC\t0x%x\n", NUM_TX_DESC);
        seq_printf(m, "cur_tx\t0x%x\n", tp->cur_tx);
        seq_printf(m, "dirty_tx\t0x%x\n", tp->dirty_tx);
        seq_printf(m, "rx_buf_sz\t0x%x\n", tp->rx_buf_sz);
        seq_printf(m, "esd_flag\t0x%x\n", tp->esd_flag);
        seq_printf(m, "pci_cfg_is_read\t0x%x\n", tp->pci_cfg_is_read);
        seq_printf(m, "rtl8168_rx_config\t0x%x\n", tp->rtl8168_rx_config);
        seq_printf(m, "cp_cmd\t0x%x\n", tp->cp_cmd);
        seq_printf(m, "intr_mask\t0x%x\n", tp->intr_mask);
        seq_printf(m, "timer_intr_mask\t0x%x\n", tp->timer_intr_mask);
        seq_printf(m, "wol_enabled\t0x%x\n", tp->wol_enabled);
        seq_printf(m, "wol_opts\t0x%x\n", tp->wol_opts);
        seq_printf(m, "efuse_ver\t0x%x\n", tp->efuse_ver);
        seq_printf(m, "eeprom_type\t0x%x\n", tp->eeprom_type);
        seq_printf(m, "autoneg\t0x%x\n", tp->autoneg);
        seq_printf(m, "duplex\t0x%x\n", tp->duplex);
        seq_printf(m, "speed\t%d\n", tp->speed);
        seq_printf(m, "eeprom_len\t0x%x\n", tp->eeprom_len);
        seq_printf(m, "cur_page\t0x%x\n", tp->cur_page);
        seq_printf(m, "bios_setting\t0x%x\n", tp->bios_setting);
        seq_printf(m, "features\t0x%x\n", tp->features);
        seq_printf(m, "org_pci_offset_99\t0x%x\n", tp->org_pci_offset_99);
        seq_printf(m, "org_pci_offset_180\t0x%x\n", tp->org_pci_offset_180);
        seq_printf(m, "issue_offset_99_event\t0x%x\n", tp->issue_offset_99_event);
        seq_printf(m, "org_pci_offset_80\t0x%x\n", tp->org_pci_offset_80);
        seq_printf(m, "org_pci_offset_81\t0x%x\n", tp->org_pci_offset_81);
        seq_printf(m, "use_timer_interrrupt\t0x%x\n", tp->use_timer_interrrupt);
        seq_printf(m, "HwIcVerUnknown\t0x%x\n", tp->HwIcVerUnknown);
        seq_printf(m, "NotWrRamCodeToMicroP\t0x%x\n", tp->NotWrRamCodeToMicroP);
        seq_printf(m, "NotWrMcuPatchCode\t0x%x\n", tp->NotWrMcuPatchCode);
        seq_printf(m, "HwHasWrRamCodeToMicroP\t0x%x\n", tp->HwHasWrRamCodeToMicroP);
        seq_printf(m, "sw_ram_code_ver\t0x%x\n", tp->sw_ram_code_ver);
        seq_printf(m, "hw_ram_code_ver\t0x%x\n", tp->hw_ram_code_ver);
        seq_printf(m, "rtk_enable_diag\t0x%x\n", tp->rtk_enable_diag);
        seq_printf(m, "ShortPacketSwChecksum\t0x%x\n", tp->ShortPacketSwChecksum);
        seq_printf(m, "UseSwPaddingShortPkt\t0x%x\n", tp->UseSwPaddingShortPkt);
        seq_printf(m, "RequireAdcBiasPatch\t0x%x\n", tp->RequireAdcBiasPatch);
        seq_printf(m, "AdcBiasPatchIoffset\t0x%x\n", tp->AdcBiasPatchIoffset);
        seq_printf(m, "RequireAdjustUpsTxLinkPulseTiming\t0x%x\n", tp->RequireAdjustUpsTxLinkPulseTiming);
        seq_printf(m, "SwrCnt1msIni\t0x%x\n", tp->SwrCnt1msIni);
        seq_printf(m, "HwSuppNowIsOobVer\t0x%x\n", tp->HwSuppNowIsOobVer);
        seq_printf(m, "RequiredSecLanDonglePatch\t0x%x\n", tp->RequiredSecLanDonglePatch);
        seq_printf(m, "HwSuppDashVer\t0x%x\n", tp->HwSuppDashVer);
        seq_printf(m, "DASH\t0x%x\n", tp->DASH);
        seq_printf(m, "HwSuppKCPOffloadVer\t0x%x\n", tp->HwSuppKCPOffloadVer);
        spin_unlock_irqrestore(&tp->lock, flags);

        seq_putc(m, '\n');
        return 0;
}

static int proc_get_tally_counter(struct seq_file *m, void *v)
{
        struct net_device *dev = m->private;
        struct rtl8168_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;
        struct rtl8168_counters *counters;
        dma_addr_t paddr;
        u32 cmd;
        u32 WaitCnt;
        unsigned long flags;

        seq_puts(m, "\nDump Tally Counter\n");

        ASSERT_RTNL();

        counters = tp->tally_vaddr;
        paddr = tp->tally_paddr;
        if (!counters) {
                seq_puts(m, "\nDump Tally Counter Fail\n");
                return 0;
        }

        spin_lock_irqsave(&tp->lock, flags);
        RTL_W32(CounterAddrHigh, (u64)paddr >> 32);
        cmd = (u64)paddr & DMA_BIT_MASK(32);
        RTL_W32(CounterAddrLow, cmd);
        RTL_W32(CounterAddrLow, cmd | CounterDump);

        WaitCnt = 0;
        while (RTL_R32(CounterAddrLow) & CounterDump) {
                udelay(10);

                WaitCnt++;
                if (WaitCnt > 20)
                        break;
        }
        spin_unlock_irqrestore(&tp->lock, flags);

        seq_puts(m, "Statistics\tValue\n----------\t-----\n");
        seq_printf(m, "tx_packets\t%lld\n", le64_to_cpu(counters->tx_packets));
        seq_printf(m, "rx_packets\t%lld\n", le64_to_cpu(counters->rx_packets));
        seq_printf(m, "tx_errors\t%lld\n", le64_to_cpu(counters->tx_errors));
        seq_printf(m, "rx_missed\t%lld\n", le64_to_cpu(counters->rx_missed));
        seq_printf(m, "align_errors\t%lld\n", le64_to_cpu(counters->align_errors));
        seq_printf(m, "tx_one_collision\t%lld\n", le64_to_cpu(counters->tx_one_collision));
        seq_printf(m, "tx_multi_collision\t%lld\n", le64_to_cpu(counters->tx_multi_collision));
        seq_printf(m, "rx_unicast\t%lld\n", le64_to_cpu(counters->rx_unicast));
        seq_printf(m, "rx_broadcast\t%lld\n", le64_to_cpu(counters->rx_broadcast));
        seq_printf(m, "rx_multicast\t%lld\n", le64_to_cpu(counters->rx_multicast));
        seq_printf(m, "tx_aborted\t%lld\n", le64_to_cpu(counters->tx_aborted));
        seq_printf(m, "tx_underun\t%lld\n", le64_to_cpu(counters->tx_underun));

        seq_putc(m, '\n');
        return 0;
}

static int proc_get_registers(struct seq_file *m, void *v)
{
        struct net_device *dev = m->private;
        int i, n, max = R8168_MAC_REGS_SIZE;
        u8 byte_rd;
        struct rtl8168_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;
        unsigned long flags;

        seq_puts(m, "\nDump MAC Registers\n");
        seq_puts(m, "Offset\tValue\n------\t-----\n");

        spin_lock_irqsave(&tp->lock, flags);
        for (n = 0; n < max;) {
                seq_printf(m, "\n0x%02x:\t", n);

                for (i = 0; i < 16 && n < max; i++, n++) {
                        byte_rd = readb(ioaddr + n);
                        seq_printf(m, "%02x ", byte_rd);
                }
        }
        spin_unlock_irqrestore(&tp->lock, flags);

        seq_putc(m, '\n');
        return 0;
}

static int proc_get_pcie_phy(struct seq_file *m, void *v)
{
        struct net_device *dev = m->private;
        int i, n, max = R8168_EPHY_REGS_SIZE/2;
        u16 word_rd;
        struct rtl8168_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;
        unsigned long flags;

        seq_puts(m, "\nDump PCIE PHY\n");
        seq_puts(m, "\nOffset\tValue\n------\t-----\n ");

        spin_lock_irqsave(&tp->lock, flags);
        for (n = 0; n < max;) {
                seq_printf(m, "\n0x%02x:\t", n);

                for (i = 0; i < 8 && n < max; i++, n++) {
                        word_rd = rtl8168_ephy_read(ioaddr, n);
                        seq_printf(m, "%04x ", word_rd);
                }
        }
        spin_unlock_irqrestore(&tp->lock, flags);

        seq_putc(m, '\n');
        return 0;
}

static int proc_get_eth_phy(struct seq_file *m, void *v)
{
        struct net_device *dev = m->private;
        int i, n, max = R8168_PHY_REGS_SIZE/2;
        u16 word_rd;
        struct rtl8168_private *tp = netdev_priv(dev);
        unsigned long flags;

        seq_puts(m, "\nDump Ethernet PHY\n");
        seq_puts(m, "\nOffset\tValue\n------\t-----\n ");

        spin_lock_irqsave(&tp->lock, flags);
        seq_puts(m, "\n####################page 0##################\n ");
        mdio_write(tp, 0x1f, 0x0000);
        for (n = 0; n < max;) {
                seq_printf(m, "\n0x%02x:\t", n);

                for (i = 0; i < 8 && n < max; i++, n++) {
                        word_rd = mdio_read(tp, n);
                        seq_printf(m, "%04x ", word_rd);
                }
        }
        spin_unlock_irqrestore(&tp->lock, flags);

        seq_putc(m, '\n');
        return 0;
}

static int proc_get_extended_registers(struct seq_file *m, void *v)
{
        struct net_device *dev = m->private;
        int i, n, max = R8168_ERI_REGS_SIZE;
        u32 dword_rd;
        struct rtl8168_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;
        unsigned long flags;

        switch (tp->mcfg) {
        case CFG_METHOD_1:
        case CFG_METHOD_2:
        case CFG_METHOD_3:
                /* RTL8168B does not support Extend GMAC */
                seq_puts(m, "\nNot Support Dump Extended Registers\n");
                return 0;
        }

        seq_puts(m, "\nDump Extended Registers\n");
        seq_puts(m, "\nOffset\tValue\n------\t-----\n ");

        spin_lock_irqsave(&tp->lock, flags);
        for (n = 0; n < max;) {
                seq_printf(m, "\n0x%02x:\t", n);

                for (i = 0; i < 4 && n < max; i++, n+=4) {
                        dword_rd = rtl8168_eri_read(ioaddr, n, 4, ERIAR_ExGMAC);
                        seq_printf(m, "%08x ", dword_rd);
                }
        }
        spin_unlock_irqrestore(&tp->lock, flags);

        seq_putc(m, '\n');
        return 0;
}
#else

static int proc_get_driver_variable(char *page, char **start,
                                    off_t offset, int count,
                                    int *eof, void *data)
{
        struct net_device *dev = data;
        struct rtl8168_private *tp = netdev_priv(dev);
        unsigned long flags;
        int len = 0;

        len += snprintf(page + len, count - len,
                        "\nDump Driver Driver\n");

        spin_lock_irqsave(&tp->lock, flags);
        len += snprintf(page + len, count - len,
                        "Variable\tValue\n----------\t-----\n");

        len += snprintf(page + len, count - len,
                        "MODULENAME\t%s\n"
                        "driver version\t%s\n"
                        "chipset\t%d\n"
                        "chipset_name\t%s\n"
                        "mtu\t%d\n"
                        "NUM_RX_DESC\t0x%x\n"
                        "cur_rx\t0x%x\n"
                        "dirty_rx\t0x%x\n"
                        "NUM_TX_DESC\t0x%x\n"
                        "cur_tx\t0x%x\n"
                        "dirty_tx\t0x%x\n"
                        "rx_buf_sz\t0x%x\n"
                        "esd_flag\t0x%x\n"
                        "pci_cfg_is_read\t0x%x\n"
                        "rtl8168_rx_config\t0x%x\n"
                        "cp_cmd\t0x%x\n"
                        "intr_mask\t0x%x\n"
                        "timer_intr_mask\t0x%x\n"
                        "wol_enabled\t0x%x\n"
                        "wol_opts\t0x%x\n"
                        "efuse_ver\t0x%x\n"
                        "eeprom_type\t0x%x\n"
                        "autoneg\t0x%x\n"
                        "duplex\t0x%x\n"
                        "speed\t%d\n"
                        "eeprom_len\t0x%x\n"
                        "cur_page\t0x%x\n"
                        "bios_setting\t0x%x\n"
                        "features\t0x%x\n"
                        "org_pci_offset_99\t0x%x\n"
                        "org_pci_offset_180\t0x%x\n"
                        "issue_offset_99_event\t0x%x\n"
                        "org_pci_offset_80\t0x%x\n"
                        "org_pci_offset_81\t0x%x\n"
                        "use_timer_interrrupt\t0x%x\n"
                        "HwIcVerUnknown\t0x%x\n"
                        "NotWrRamCodeToMicroP\t0x%x\n"
                        "NotWrMcuPatchCode\t0x%x\n"
                        "HwHasWrRamCodeToMicroP\t0x%x\n"
                        "sw_ram_code_ver\t0x%x\n"
                        "hw_ram_code_ver\t0x%x\n"
                        "rtk_enable_diag\t0x%x\n"
                        "ShortPacketSwChecksum\t0x%x\n"
                        "UseSwPaddingShortPkt\t0x%x\n"
                        "RequireAdcBiasPatch\t0x%x\n"
                        "AdcBiasPatchIoffset\t0x%x\n"
                        "RequireAdjustUpsTxLinkPulseTiming\t0x%x\n"
                        "SwrCnt1msIni\t0x%x\n"
                        "HwSuppNowIsOobVer\t0x%x\n"
                        "RequiredSecLanDonglePatch\t0x%x\n"
                        "HwSuppDashVer\t0x%x\n"
                        "DASH\t0x%x\n"
                        "HwSuppKCPOffloadVer\t0x%x\n",
                        MODULENAME,
                        RTL8168_VERSION,
                        tp->chipset,
                        rtl_chip_info[tp->chipset].name,
                        dev->mtu,
                        NUM_RX_DESC,
                        tp->cur_rx,
                        tp->dirty_rx,
                        NUM_TX_DESC,
                        tp->cur_tx,
                        tp->dirty_tx,
                        tp->rx_buf_sz,
                        tp->esd_flag,
                        tp->pci_cfg_is_read,
                        tp->rtl8168_rx_config,
                        tp->cp_cmd,
                        tp->intr_mask,
                        tp->timer_intr_mask,
                        tp->wol_enabled,
                        tp->wol_opts,
                        tp->efuse_ver,
                        tp->eeprom_type,
                        tp->autoneg,
                        tp->duplex,
                        tp->speed,
                        tp->eeprom_len,
                        tp->cur_page,
                        tp->bios_setting,
                        tp->features,
                        tp->org_pci_offset_99,
                        tp->org_pci_offset_180,
                        tp->issue_offset_99_event,
                        tp->org_pci_offset_80,
                        tp->org_pci_offset_81,
                        tp->use_timer_interrrupt,
                        tp->HwIcVerUnknown,
                        tp->NotWrRamCodeToMicroP,
                        tp->NotWrMcuPatchCode,
                        tp->HwHasWrRamCodeToMicroP,
                        tp->sw_ram_code_ver,
                        tp->hw_ram_code_ver,
                        tp->rtk_enable_diag,
                        tp->ShortPacketSwChecksum,
                        tp->UseSwPaddingShortPkt,
                        tp->RequireAdcBiasPatch,
                        tp->AdcBiasPatchIoffset,
                        tp->RequireAdjustUpsTxLinkPulseTiming,
                        tp->SwrCnt1msIni,
                        tp->HwSuppNowIsOobVer,
                        tp->RequiredSecLanDonglePatch,
                        tp->HwSuppDashVer,
                        tp->DASH,
                        tp->HwSuppKCPOffloadVer
                       );
        spin_unlock_irqrestore(&tp->lock, flags);

        len += snprintf(page + len, count - len, "\n");

        *eof = 1;
        return len;
}

static int proc_get_tally_counter(char *page, char **start,
                                  off_t offset, int count,
                                  int *eof, void *data)
{
        struct net_device *dev = data;
        struct rtl8168_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;
        struct rtl8168_counters *counters;
        dma_addr_t paddr;
        u32 cmd;
        u32 WaitCnt;
        unsigned long flags;
        int len = 0;

        len += snprintf(page + len, count - len,
                        "\nDump Tally Counter\n");

        ASSERT_RTNL();

        counters = tp->tally_vaddr;
        paddr = tp->tally_paddr;
        if (!counters) {
                len += snprintf(page + len, count - len,
                                "\nDump Tally Counter Fail\n");
                goto out;
        }

        spin_lock_irqsave(&tp->lock, flags);
        RTL_W32(CounterAddrHigh, (u64)paddr >> 32);
        cmd = (u64)paddr & DMA_BIT_MASK(32);
        RTL_W32(CounterAddrLow, cmd);
        RTL_W32(CounterAddrLow, cmd | CounterDump);

        WaitCnt = 0;
        while (RTL_R32(CounterAddrLow) & CounterDump) {
                udelay(10);

                WaitCnt++;
                if (WaitCnt > 20)
                        break;
        }
        spin_unlock_irqrestore(&tp->lock, flags);

        len += snprintf(page + len, count - len,
                        "Statistics\tValue\n----------\t-----\n");

        len += snprintf(page + len, count - len,
                        "tx_packets\t%lld\n"
                        "rx_packets\t%lld\n"
                        "tx_errors\t%lld\n"
                        "rx_missed\t%lld\n"
                        "align_errors\t%lld\n"
                        "tx_one_collision\t%lld\n"
                        "tx_multi_collision\t%lld\n"
                        "rx_unicast\t%lld\n"
                        "rx_broadcast\t%lld\n"
                        "rx_multicast\t%lld\n"
                        "tx_aborted\t%lld\n"
                        "tx_underun\t%lld\n",
                        le64_to_cpu(counters->tx_packets),
                        le64_to_cpu(counters->rx_packets),
                        le64_to_cpu(counters->tx_errors),
                        le64_to_cpu(counters->rx_missed),
                        le64_to_cpu(counters->align_errors),
                        le64_to_cpu(counters->tx_one_collision),
                        le64_to_cpu(counters->tx_multi_collision),
                        le64_to_cpu(counters->rx_unicast),
                        le64_to_cpu(counters->rx_broadcast),
                        le64_to_cpu(counters->rx_multicast),
                        le64_to_cpu(counters->tx_aborted),
                        le64_to_cpu(counters->tx_underun)
                       );

        len += snprintf(page + len, count - len, "\n");
out:
        *eof = 1;
        return len;
}

static int proc_get_registers(char *page, char **start,
                              off_t offset, int count,
                              int *eof, void *data)
{
        struct net_device *dev = data;
        int i, n, max = R8168_MAC_REGS_SIZE;
        u8 byte_rd;
        struct rtl8168_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;
        unsigned long flags;
        int len = 0;

        len += snprintf(page + len, count - len,
                        "\nDump MAC Registers\n"
                        "Offset\tValue\n------\t-----\n");

        spin_lock_irqsave(&tp->lock, flags);
        for (n = 0; n < max;) {
                len += snprintf(page + len, count - len,
                                "\n0x%02x:\t",
                                n);

                for (i = 0; i < 16 && n < max; i++, n++) {
                        byte_rd = readb(ioaddr + n);
                        len += snprintf(page + len, count - len,
                                        "%02x ",
                                        byte_rd);
                }
        }
        spin_unlock_irqrestore(&tp->lock, flags);

        len += snprintf(page + len, count - len, "\n");

        *eof = 1;
        return len;
}

static int proc_get_pcie_phy(char *page, char **start,
                             off_t offset, int count,
                             int *eof, void *data)
{
        struct net_device *dev = data;
        int i, n, max = R8168_EPHY_REGS_SIZE/2;
        u16 word_rd;
        struct rtl8168_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;
        unsigned long flags;
        int len = 0;

        len += snprintf(page + len, count - len,
                        "\nDump PCIE PHY\n"
                        "Offset\tValue\n------\t-----\n");

        spin_lock_irqsave(&tp->lock, flags);
        for (n = 0; n < max;) {
                len += snprintf(page + len, count - len,
                                "\n0x%02x:\t",
                                n);

                for (i = 0; i < 8 && n < max; i++, n++) {
                        word_rd = rtl8168_ephy_read(ioaddr, n);
                        len += snprintf(page + len, count - len,
                                        "%04x ",
                                        word_rd);
                }
        }
        spin_unlock_irqrestore(&tp->lock, flags);

        len += snprintf(page + len, count - len, "\n");

        *eof = 1;
        return len;
}

static int proc_get_eth_phy(char *page, char **start,
                            off_t offset, int count,
                            int *eof, void *data)
{
        struct net_device *dev = data;
        int i, n, max = R8168_PHY_REGS_SIZE/2;
        u16 word_rd;
        struct rtl8168_private *tp = netdev_priv(dev);
        unsigned long flags;
        int len = 0;

        len += snprintf(page + len, count - len,
                        "\nDump Ethernet PHY\n"
                        "Offset\tValue\n------\t-----\n");

        spin_lock_irqsave(&tp->lock, flags);
        len += snprintf(page + len, count - len,
                        "\n####################page 0##################\n");
        mdio_write(tp, 0x1f, 0x0000);
        for (n = 0; n < max;) {
                len += snprintf(page + len, count - len,
                                "\n0x%02x:\t",
                                n);

                for (i = 0; i < 8 && n < max; i++, n++) {
                        word_rd = mdio_read(tp, n);
                        len += snprintf(page + len, count - len,
                                        "%04x ",
                                        word_rd);
                }
        }
        spin_unlock_irqrestore(&tp->lock, flags);

        len += snprintf(page + len, count - len, "\n");

        *eof = 1;
        return len;
}

static int proc_get_extended_registers(char *page, char **start,
                                       off_t offset, int count,
                                       int *eof, void *data)
{
        struct net_device *dev = data;
        int i, n, max = R8168_ERI_REGS_SIZE;
        u32 dword_rd;
        struct rtl8168_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;
        unsigned long flags;
        int len = 0;

        switch (tp->mcfg) {
        case CFG_METHOD_1:
        case CFG_METHOD_2:
        case CFG_METHOD_3:
                /* RTL8168B does not support Extend GMAC */
                len += snprintf(page + len, count - len,
                                "\nNot Support Dump Extended Registers\n");

                goto out;
        }

        len += snprintf(page + len, count - len,
                        "\nDump Extended Registers\n"
                        "Offset\tValue\n------\t-----\n");

        spin_lock_irqsave(&tp->lock, flags);
        for (n = 0; n < max;) {
                len += snprintf(page + len, count - len,
                                "\n0x%02x:\t",
                                n);

                for (i = 0; i < 4 && n < max; i++, n+=4) {
                        dword_rd = rtl8168_eri_read(ioaddr, n, 4, ERIAR_ExGMAC);
                        len += snprintf(page + len, count - len,
                                        "%08x ",
                                        dword_rd);
                }
        }
        spin_unlock_irqrestore(&tp->lock, flags);

        len += snprintf(page + len, count - len, "\n");
out:
        *eof = 1;
        return len;
}

#endif
static void rtl8168_proc_module_init(void)
{
        //in case /proc/net/r8168 already exist
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
        remove_proc_subtree(MODULENAME, init_net.proc_net);
#else
        remove_proc_entry(MODULENAME, init_net.proc_net);
#endif

        //create /proc/net/r8168
        rtl8168_proc = proc_mkdir(MODULENAME, init_net.proc_net);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
/*
 * seq_file wrappers for procfile show routines.
 */
static int rtl8168_proc_open(struct inode *inode, struct file *file)
{
        struct net_device *dev = proc_get_parent_data(inode);
        int (*show)(struct seq_file *, void *) = PDE_DATA(inode);

        return single_open(file, show, dev);
}

static const struct file_operations rtl8168_proc_fops = {
        .open           = rtl8168_proc_open,
        .read           = seq_read,
        .llseek         = seq_lseek,
        .release        = single_release,
};
#endif

/*
 * Table of proc files we need to create.
 */
struct rtl8168_proc_file {
        char name[12];
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
        int (*show)(struct seq_file *, void *);
#else
        int (*show)(char *, char **, off_t, int, int *, void *);
#endif
};

static const struct rtl8168_proc_file rtl8168_proc_files[] = {
        { "driver_var", &proc_get_driver_variable },
        { "tally", &proc_get_tally_counter },
        { "registers", &proc_get_registers },
        { "pcie_phy", &proc_get_pcie_phy },
        { "eth_phy", &proc_get_eth_phy },
        { "ext_regs", &proc_get_extended_registers },
        { "" }
};

static void rtl8168_proc_init(struct net_device *dev)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        const struct rtl8168_proc_file *f;
        struct proc_dir_entry *dir;

        if (rtl8168_proc && !tp->proc_dir) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
                dir = proc_mkdir_data(dev->name, 0, rtl8168_proc, dev);
                if (!dir) {
                        printk("Unable to initialize /proc/net/%s/%s\n",
                               MODULENAME, dev->name);
                        return;
                }

                tp->proc_dir = dir;
                proc_init_num++;

                for (f = rtl8168_proc_files; f->name[0]; f++) {
                        if (!proc_create_data(f->name, S_IFREG | S_IRUGO, dir,
                                              &rtl8168_proc_fops, f->show)) {
                                printk("Unable to initialize "
                                       "/proc/net/%s/%s/%s\n",
                                       MODULENAME, dev->name, f->name);
                                return;
                        }
                }
#else
                dir = proc_mkdir(dev->name, rtl8168_proc);
                if (!dir) {
                        printk("Unable to initialize /proc/net/%s/%s\n",
                               MODULENAME, dev->name);
                        return;
                }

                tp->proc_dir = dir;
                proc_init_num++;

                for (f = rtl8168_proc_files; f->name[0]; f++) {
                        if (!create_proc_read_entry(f->name, S_IFREG | S_IRUGO,
                                                    dir, f->show, dev)) {
                                printk("Unable to initialize "
                                       "/proc/net/%s/%s/%s\n",
                                       MODULENAME, dev->name, f->name);
                                return;
                        }
                }
#endif
        }
}

static void rtl8168_proc_remove(struct net_device *dev)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
        remove_proc_subtree(dev->name, rtl8168_proc);
        proc_init_num--;
#else
        const struct rtl8168_proc_file *f;
        struct rtl8168_private *tp = netdev_priv(dev);

        for (f = rtl8168_proc_files; f->name[0]; f++)
                remove_proc_entry(f->name, tp->proc_dir);

        remove_proc_entry(dev->name, rtl8168_proc);
        proc_init_num--;
#endif
}

#endif //ENABLE_R8168_PROCFS

static inline u16 map_phy_ocp_addr(u16 PageNum, u8 RegNum)
{
        u16 OcpPageNum = 0;
        u8 OcpRegNum = 0;
        u16 OcpPhyAddress = 0;

        if( PageNum == 0 ) {
                OcpPageNum = OCP_STD_PHY_BASE_PAGE + ( RegNum / 8 );
                OcpRegNum = 0x10 + ( RegNum % 8 );
        } else {
                OcpPageNum = PageNum;
                OcpRegNum = RegNum;
        }

        OcpPageNum <<= 4;

        if( OcpRegNum < 16 ) {
                OcpPhyAddress = 0;
        } else {
                OcpRegNum -= 16;
                OcpRegNum <<= 1;

                OcpPhyAddress = OcpPageNum + OcpRegNum;
        }


        return OcpPhyAddress;
}

static void mdio_write_phy_ocp(struct rtl8168_private *tp,
                               u16 PageNum,
                               u32 RegAddr,
                               u32 value)
{
        void __iomem *ioaddr = tp->mmio_addr;
        u32 data32;
        u16 ocp_addr;
        int i;

        ocp_addr = map_phy_ocp_addr(PageNum, RegAddr);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,18)
        WARN_ON_ONCE(ocp_addr % 2);
#endif
        data32 = ocp_addr/2;
        data32 <<= OCPR_Addr_Reg_shift;
        data32 |= OCPR_Write | value;

        RTL_W32(PHYOCP, data32);
        for (i = 0; i < 100; i++) {
                udelay(1);

                if (!(RTL_R32(PHYOCP) & OCPR_Flag))
                        break;
        }
}

static void mdio_real_write(struct rtl8168_private *tp,
                            u32 RegAddr,
                            u32 value)
{
        void __iomem *ioaddr = tp->mmio_addr;
        int i;

        if (RegAddr == 0x1F) {
                tp->cur_page = value;
        }

        if (tp->mcfg == CFG_METHOD_11) {
                RTL_W32(OCPDR, OCPDR_Write |
                        (RegAddr & OCPDR_Reg_Mask) << OCPDR_GPHY_Reg_shift |
                        (value & OCPDR_Data_Mask));
                RTL_W32(OCPAR, OCPAR_GPHY_Write);
                RTL_W32(EPHY_RXER_NUM, 0);

                for (i = 0; i < 100; i++) {
                        mdelay(1);
                        if (!(RTL_R32(OCPAR) & OCPAR_Flag))
                                break;
                }
        } else if (tp->mcfg == CFG_METHOD_21 || tp->mcfg == CFG_METHOD_22 ||
                   tp->mcfg == CFG_METHOD_23 || tp->mcfg == CFG_METHOD_24 ||
                   tp->mcfg == CFG_METHOD_25 || tp->mcfg == CFG_METHOD_26 ||
                   tp->mcfg == CFG_METHOD_27 || tp->mcfg == CFG_METHOD_28 ||
                   tp->mcfg == CFG_METHOD_29 || tp->mcfg == CFG_METHOD_30) {
                if (RegAddr == 0x1F) {
                        return;
                }
                mdio_write_phy_ocp(tp, tp->cur_page, RegAddr, value);
        } else {
                if (tp->mcfg == CFG_METHOD_12 || tp->mcfg == CFG_METHOD_13)
                        RTL_W32(0xD0, RTL_R32(0xD0) & ~0x00020000);

                RTL_W32(PHYAR, PHYAR_Write |
                        (RegAddr & PHYAR_Reg_Mask) << PHYAR_Reg_shift |
                        (value & PHYAR_Data_Mask));

                for (i = 0; i < 10; i++) {
                        udelay(100);

                        /* Check if the RTL8168 has completed writing to the specified MII register */
                        if (!(RTL_R32(PHYAR) & PHYAR_Flag)) {
                                udelay(20);
                                break;
                        }
                }

                if (tp->mcfg == CFG_METHOD_12 || tp->mcfg == CFG_METHOD_13)
                        RTL_W32(0xD0, RTL_R32(0xD0) | 0x00020000);
        }
}

void mdio_write(struct rtl8168_private *tp,
                u32 RegAddr,
                u32 value)
{
        if (tp->rtk_enable_diag) return;

        mdio_real_write(tp, RegAddr, value);
}

void mdio_prot_write(struct rtl8168_private *tp,
                     u32 RegAddr,
                     u32 value)
{
        mdio_real_write(tp, RegAddr, value);
}

static u32 mdio_read_phy_ocp(struct rtl8168_private *tp,
                             u16 PageNum,
                             u32 RegAddr)
{
        void __iomem *ioaddr = tp->mmio_addr;
        u32 data32;
        u16 ocp_addr;
        int i, value = 0;

        ocp_addr = map_phy_ocp_addr(PageNum, RegAddr);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,18)
        WARN_ON_ONCE(ocp_addr % 2);
#endif
        data32 = ocp_addr/2;
        data32 <<= OCPR_Addr_Reg_shift;

        RTL_W32(PHYOCP, data32);
        for (i = 0; i < 100; i++) {
                udelay(1);

                if (RTL_R32(PHYOCP) & OCPR_Flag)
                        break;
        }
        value = RTL_R32(PHYOCP) & OCPDR_Data_Mask;

        return value;
}

u32 mdio_read(struct rtl8168_private *tp,
              u32 RegAddr)
{
        void __iomem *ioaddr = tp->mmio_addr;
        int i, value = 0;

        if (tp->mcfg==CFG_METHOD_11) {
                RTL_W32(OCPDR, OCPDR_Read |
                        (RegAddr & OCPDR_Reg_Mask) << OCPDR_GPHY_Reg_shift);
                RTL_W32(OCPAR, OCPAR_GPHY_Write);
                RTL_W32(EPHY_RXER_NUM, 0);

                for (i = 0; i < 100; i++) {
                        mdelay(1);
                        if (!(RTL_R32(OCPAR) & OCPAR_Flag))
                                break;
                }

                mdelay(1);
                RTL_W32(OCPAR, OCPAR_GPHY_Read);
                RTL_W32(EPHY_RXER_NUM, 0);

                for (i = 0; i < 100; i++) {
                        mdelay(1);
                        if (RTL_R32(OCPAR) & OCPAR_Flag)
                                break;
                }

                value = RTL_R32(OCPDR) & OCPDR_Data_Mask;
        } else if (tp->mcfg == CFG_METHOD_21 || tp->mcfg == CFG_METHOD_22 ||
                   tp->mcfg == CFG_METHOD_23 || tp->mcfg == CFG_METHOD_24 ||
                   tp->mcfg == CFG_METHOD_25 || tp->mcfg == CFG_METHOD_26 ||
                   tp->mcfg == CFG_METHOD_27 || tp->mcfg == CFG_METHOD_28 ||
                   tp->mcfg == CFG_METHOD_29 || tp->mcfg == CFG_METHOD_30) {
                value = mdio_read_phy_ocp(tp, tp->cur_page, RegAddr);
        } else {
                if (tp->mcfg == CFG_METHOD_12 || tp->mcfg == CFG_METHOD_13)
                        RTL_W32(0xD0, RTL_R32(0xD0) & ~0x00020000);

                RTL_W32(PHYAR,
                        PHYAR_Read | (RegAddr & PHYAR_Reg_Mask) << PHYAR_Reg_shift);

                for (i = 0; i < 10; i++) {
                        udelay(100);

                        /* Check if the RTL8168 has completed retrieving data from the specified MII register */
                        if (RTL_R32(PHYAR) & PHYAR_Flag) {
                                value = RTL_R32(PHYAR) & PHYAR_Data_Mask;
                                udelay(20);
                                break;
                        }
                }

                if (tp->mcfg == CFG_METHOD_12 || tp->mcfg == CFG_METHOD_13)
                        RTL_W32(0xD0, RTL_R32(0xD0) | 0x00020000);
        }

        return value;
}

static void ClearAndSetEthPhyBit(struct rtl8168_private *tp, u8  addr, u16 clearmask, u16 setmask)
{
        u16 PhyRegValue;


        PhyRegValue = mdio_read( tp, addr );
        PhyRegValue &= ~clearmask;
        PhyRegValue |= setmask;
        mdio_write( tp, addr, PhyRegValue);
}

void ClearEthPhyBit(struct rtl8168_private *tp, u8 addr, u16 mask)
{
        ClearAndSetEthPhyBit( tp,
                              addr,
                              mask,
                              0
                            );
}

void SetEthPhyBit(struct rtl8168_private *tp,  u8  addr, u16  mask)
{
        ClearAndSetEthPhyBit( tp,
                              addr,
                              0,
                              mask
                            );
}

void mac_ocp_write(struct rtl8168_private *tp, u16 reg_addr, u16 value)
{
        void __iomem *ioaddr = tp->mmio_addr;
        u32 data32;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,18)
        WARN_ON_ONCE(reg_addr % 2);
#endif

        data32 = reg_addr/2;
        data32 <<= OCPR_Addr_Reg_shift;
        data32 += value;
        data32 |= OCPR_Write;

        RTL_W32(MACOCP, data32);
}

u16 mac_ocp_read(struct rtl8168_private *tp, u16 reg_addr)
{
        void __iomem *ioaddr = tp->mmio_addr;
        u32 data32;
        u16 data16 = 0;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,18)
        WARN_ON_ONCE(reg_addr % 2);
#endif

        data32 = reg_addr/2;
        data32 <<= OCPR_Addr_Reg_shift;

        RTL_W32(MACOCP, data32);
        data16 = (u16)RTL_R32(MACOCP);

        return data16;
}

static u32 real_ocp_read(struct rtl8168_private *tp, u16 addr, u8 len)
{
        void __iomem *ioaddr = tp->mmio_addr;
        int i, val_shift, shift = 0;
        u32 value1 = 0, value2 = 0, mask;

        if (len > 4 || len <= 0)
                return -1;

        while (len > 0) {
                val_shift = addr % 4;
                addr = addr & ~0x3;

                RTL_W32(OCPAR, (0x0F<<12) | (addr&0xFFF));

                for (i = 0; i < 20; i++) {
                        udelay(100);
                        if (RTL_R32(OCPAR) & OCPAR_Flag)
                                break;
                }

                if (len == 1)       mask = (0xFF << (val_shift * 8)) & 0xFFFFFFFF;
                else if (len == 2)  mask = (0xFFFF << (val_shift * 8)) & 0xFFFFFFFF;
                else if (len == 3)  mask = (0xFFFFFF << (val_shift * 8)) & 0xFFFFFFFF;
                else            mask = (0xFFFFFFFF << (val_shift * 8)) & 0xFFFFFFFF;

                value1 = RTL_R32(OCPDR) & mask;
                value2 |= (value1 >> val_shift * 8) << shift * 8;

                if (len <= 4 - val_shift) {
                        len = 0;
                } else {
                        len -= (4 - val_shift);
                        shift = 4 - val_shift;
                        addr += 4;
                }
        }

        udelay(20);

        return value2;
}

u32 OCP_read(struct rtl8168_private *tp, u16 addr, u8 len)
{
        void __iomem *ioaddr = tp->mmio_addr;
        u32 value = 0;

        if (tp->mcfg == CFG_METHOD_23 || tp->mcfg == CFG_METHOD_27 ||
            tp->mcfg == CFG_METHOD_28) {
                value = rtl8168_eri_read(ioaddr, addr, len, ERIAR_OOB);
        } else {
                value = real_ocp_read(tp, addr, len);
        }

        return value;
}

static int real_ocp_write(struct rtl8168_private *tp, u16 addr, u8 len, u32 value)
{
        void __iomem *ioaddr = tp->mmio_addr;
        int i, val_shift, shift = 0;
        u32 value1 = 0, mask;

        if (len > 4 || len <= 0)
                return -1;

        while (len > 0) {
                val_shift = addr % 4;
                addr = addr & ~0x3;

                if (len == 1)       mask = (0xFF << (val_shift * 8)) & 0xFFFFFFFF;
                else if (len == 2)  mask = (0xFFFF << (val_shift * 8)) & 0xFFFFFFFF;
                else if (len == 3)  mask = (0xFFFFFF << (val_shift * 8)) & 0xFFFFFFFF;
                else            mask = (0xFFFFFFFF << (val_shift * 8)) & 0xFFFFFFFF;

                value1 = OCP_read(tp, addr, 4) & ~mask;
                value1 |= ((value << val_shift * 8) >> shift * 8);

                RTL_W32(OCPDR, value1);
                RTL_W32(OCPAR, OCPAR_Flag | (0x0F<<12) | (addr&0xFFF));

                for (i = 0; i < 10; i++) {
                        udelay(100);

                        /* Check if the RTL8168 has completed ERI write */
                        if (!(RTL_R32(OCPAR) & OCPAR_Flag))
                                break;
                }

                if (len <= 4 - val_shift) {
                        len = 0;
                } else {
                        len -= (4 - val_shift);
                        shift = 4 - val_shift;
                        addr += 4;
                }
        }

        udelay(20);

        return 0;
}

void OCP_write(struct rtl8168_private *tp, u16 addr, u8 len, u32 value)
{
        void __iomem *ioaddr = tp->mmio_addr;

        if (tp->mcfg == CFG_METHOD_23 || tp->mcfg == CFG_METHOD_27 ||
            tp->mcfg == CFG_METHOD_28) {
                rtl8168_eri_write(ioaddr, addr, len, value, ERIAR_OOB);
        } else {
                real_ocp_write(tp, addr, len, value);
        }
}

void OOB_mutex_lock(struct rtl8168_private *tp)
{
        u8 reg_16, reg_a0;
        u32 wait_cnt_0, wait_Cnt_1;
        u16 ocp_reg_mutex_ib;
        u16 ocp_reg_mutex_oob;
        u16 ocp_reg_mutex_prio;

        switch (tp->mcfg) {
        case CFG_METHOD_11:
        case CFG_METHOD_12:
                ocp_reg_mutex_oob = 0x16;
                ocp_reg_mutex_ib = 0x17;
                ocp_reg_mutex_prio = 0x9C;
                break;
        case CFG_METHOD_13:
                ocp_reg_mutex_oob = 0x06;
                ocp_reg_mutex_ib = 0x07;
                ocp_reg_mutex_prio = 0x9C;
                break;
        case CFG_METHOD_23:
        case CFG_METHOD_27:
        case CFG_METHOD_28:
        default:
                ocp_reg_mutex_oob = 0x110;
                ocp_reg_mutex_ib = 0x114;
                ocp_reg_mutex_prio = 0x11C;
                break;
        }

        OCP_write(tp, ocp_reg_mutex_ib, 1, BIT_0);
        reg_16 = OCP_read(tp, ocp_reg_mutex_oob, 1);
        wait_cnt_0 = 0;
        while(reg_16) {
                reg_a0 = OCP_read(tp, ocp_reg_mutex_prio, 1);
                if(reg_a0) {
                        OCP_write(tp, ocp_reg_mutex_ib, 1, 0x00);
                        reg_a0 = OCP_read(tp, ocp_reg_mutex_prio, 1);
                        wait_Cnt_1 = 0;
                        while(reg_a0) {
                                reg_a0 = OCP_read(tp, ocp_reg_mutex_prio, 1);

                                wait_Cnt_1++;

                                if(wait_Cnt_1 > 2000)
                                        break;
                        };
                        OCP_write(tp, ocp_reg_mutex_ib, 1, BIT_0);

                }
                reg_16 = OCP_read(tp, ocp_reg_mutex_oob, 1);

                wait_cnt_0++;

                if(wait_cnt_0 > 2000)
                        break;
        };
}

void OOB_mutex_unlock(struct rtl8168_private *tp)
{
        u16 ocp_reg_mutex_ib;
        u16 ocp_reg_mutex_oob;
        u16 ocp_reg_mutex_prio;

        switch (tp->mcfg) {
        case CFG_METHOD_11:
        case CFG_METHOD_12:
                ocp_reg_mutex_oob = 0x16;
                ocp_reg_mutex_ib = 0x17;
                ocp_reg_mutex_prio = 0x9C;
                break;
        case CFG_METHOD_13:
                ocp_reg_mutex_oob = 0x06;
                ocp_reg_mutex_ib = 0x07;
                ocp_reg_mutex_prio = 0x9C;
                break;
        case CFG_METHOD_23:
        case CFG_METHOD_27:
        case CFG_METHOD_28:
        default:
                ocp_reg_mutex_oob = 0x110;
                ocp_reg_mutex_ib = 0x114;
                ocp_reg_mutex_prio = 0x11C;
                break;
        }

        OCP_write(tp, ocp_reg_mutex_prio, 1, BIT_0);
        OCP_write(tp, ocp_reg_mutex_ib, 1, 0x00);
}

void OOB_notify(struct rtl8168_private *tp, u8 cmd)
{
        void __iomem *ioaddr = tp->mmio_addr;

        rtl8168_eri_write(ioaddr, 0xE8, 1, cmd, ERIAR_ExGMAC);

        OCP_write(tp, 0x30, 1, 0x01);
}

static int rtl8168_check_dash(struct rtl8168_private *tp)
{
        if (tp->mcfg == CFG_METHOD_23 || tp->mcfg == CFG_METHOD_27 ||
            tp->mcfg == CFG_METHOD_28) {
                if (OCP_read(tp, 0x128, 1) & BIT_0)
                        return 1;
                else
                        return 0;
        } else {
                u32 reg;

                if (tp->mcfg == CFG_METHOD_13)
                        reg = 0xb8;
                else
                        reg = 0x10;

                if (OCP_read(tp, reg, 2) & 0x00008000)
                        return 1;
                else
                        return 0;
        }
}

void Dash2DisableTx(struct rtl8168_private *tp)
{
        void __iomem *ioaddr = tp->mmio_addr;

        if (!tp->DASH) return;

        if( HW_DASH_SUPPORT_TYPE_2( tp ) ) {
                u16 WaitCnt;
                u8 TmpUchar;

                //Disable oob Tx
                RTL_W8(IBCR2, RTL_R8(IBCR2) & ~( BIT_0 ));
                WaitCnt = 0;

                //wait oob tx disable
                do {
                        TmpUchar = RTL_R8(IBISR0);

                        if( TmpUchar & ISRIMR_DASH_TYPE2_TX_DISABLE_IDLE ) {
                                break;
                        }

                        udelay( 50 );
                        WaitCnt++;
                } while(WaitCnt < 2000);

                //Clear ISRIMR_DASH_TYPE2_TX_DISABLE_IDLE
                RTL_W8(IBISR0, RTL_R8(IBISR0) | ISRIMR_DASH_TYPE2_TX_DISABLE_IDLE);
        }
}

void Dash2EnableTx(struct rtl8168_private *tp)
{
        void __iomem *ioaddr = tp->mmio_addr;

        if (!tp->DASH) return;

        if( HW_DASH_SUPPORT_TYPE_2( tp ) )
                RTL_W8(IBCR2, RTL_R8(IBCR2) | BIT_0);
}

void Dash2DisableRx(struct rtl8168_private *tp)
{
        void __iomem *ioaddr = tp->mmio_addr;

        if (!tp->DASH) return;

        if( HW_DASH_SUPPORT_TYPE_2( tp ) )
                RTL_W8(IBCR0, RTL_R8(IBCR0) & ~( BIT_0 ));
}

void Dash2EnableRx(struct rtl8168_private *tp)
{
        void __iomem *ioaddr = tp->mmio_addr;

        if (!tp->DASH) return;

        if( HW_DASH_SUPPORT_TYPE_2( tp ) )
                RTL_W8(IBCR0, RTL_R8(IBCR0) | BIT_0);
}

static void Dash2DisableTxRx(struct net_device *dev)
{
        struct rtl8168_private *tp = netdev_priv(dev);

        if( HW_DASH_SUPPORT_TYPE_2( tp ) ) {
                Dash2DisableTx( tp );
                Dash2DisableRx( tp );
        }
}

static void rtl8168_driver_start(struct rtl8168_private *tp)
{
        void __iomem *ioaddr = tp->mmio_addr;

        if (!tp->DASH)
                return;

        if (tp->mcfg == CFG_METHOD_23 || tp->mcfg == CFG_METHOD_27 ||
            tp->mcfg == CFG_METHOD_28) {
                int timeout;
                u32 tmp_value;

                OCP_write(tp, 0x180, 1, OOB_CMD_DRIVER_START);
                tmp_value = OCP_read(tp, 0x30, 1);
                tmp_value |= BIT_0;
                OCP_write(tp, 0x30, 1, tmp_value);

                for (timeout = 0; timeout < 10; timeout++) {
                        mdelay(10);
                        if (OCP_read(tp, 0x124, 1) & BIT_0)
                                break;
                }
        } else {
                int timeout;
                u32 reg;

                if (tp->mcfg == CFG_METHOD_13) {
                        RTL_W8(TwiCmdReg, RTL_R8(TwiCmdReg) | ( BIT_7 ));
                }

                OOB_notify(tp, OOB_CMD_DRIVER_START);

                if (tp->mcfg == CFG_METHOD_13)
                        reg = 0xB8;
                else
                        reg = 0x10;

                for (timeout = 0; timeout < 10; timeout++) {
                        mdelay(10);
                        if (OCP_read(tp, reg, 2) & BIT_11)
                                break;
                }
        }
}

static void rtl8168_driver_stop(struct rtl8168_private *tp)
{
        void __iomem *ioaddr = tp->mmio_addr;

        if (!tp->DASH)
                return;

        if (tp->mcfg == CFG_METHOD_23 || tp->mcfg == CFG_METHOD_27 ||
            tp->mcfg == CFG_METHOD_28) {
                struct net_device *dev = tp->dev;
                int timeout;
                u32 tmp_value;

                Dash2DisableTxRx(dev);

                OCP_write(tp, 0x180, 1, OOB_CMD_DRIVER_STOP);
                tmp_value = OCP_read(tp, 0x30, 1);
                tmp_value |= BIT_0;
                OCP_write(tp, 0x30, 1, tmp_value);

                for (timeout = 0; timeout < 10; timeout++) {
                        mdelay(10);
                        if (!(OCP_read(tp, 0x124, 1) & BIT_0))
                                break;
                }
        } else {
                int timeout;
                u32 reg;

                OOB_notify(tp, OOB_CMD_DRIVER_STOP);

                if (tp->mcfg == CFG_METHOD_13)
                        reg = 0xB8;
                else
                        reg = 0x10;

                for (timeout = 0; timeout < 10; timeout++) {
                        mdelay(10);
                        if ((OCP_read(tp, reg, 2) & BIT_11) == 0)
                                break;
                }

                if (tp->mcfg == CFG_METHOD_13) {
                        RTL_W8(TwiCmdReg, RTL_R8(TwiCmdReg) & ~( BIT_7 ));
                }
        }
}

void rtl8168_ephy_write(void __iomem *ioaddr, int RegAddr, int value)
{
        int i;

        RTL_W32(EPHYAR,
                EPHYAR_Write |
                (RegAddr & EPHYAR_Reg_Mask) << EPHYAR_Reg_shift |
                (value & EPHYAR_Data_Mask));

        for (i = 0; i < 10; i++) {
                udelay(100);

                /* Check if the RTL8168 has completed EPHY write */
                if (!(RTL_R32(EPHYAR) & EPHYAR_Flag))
                        break;
        }

        udelay(20);
}

u16 rtl8168_ephy_read(void __iomem *ioaddr, int RegAddr)
{
        int i;
        u16 value = 0xffff;

        RTL_W32(EPHYAR,
                EPHYAR_Read | (RegAddr & EPHYAR_Reg_Mask) << EPHYAR_Reg_shift);

        for (i = 0; i < 10; i++) {
                udelay(100);

                /* Check if the RTL8168 has completed EPHY read */
                if (RTL_R32(EPHYAR) & EPHYAR_Flag) {
                        value = (u16) (RTL_R32(EPHYAR) & EPHYAR_Data_Mask);
                        break;
                }
        }

        udelay(20);

        return value;
}

static void ClearAndSetPCIePhyBit(struct rtl8168_private *tp, u8 addr, u16 clearmask, u16 setmask)
{
        void __iomem *ioaddr = tp->mmio_addr;
        u16 EphyValue;

        EphyValue = rtl8168_ephy_read( ioaddr, addr );
        EphyValue &= ~clearmask;
        EphyValue |= setmask;
        rtl8168_ephy_write( ioaddr, addr, EphyValue);
}

static void ClearPCIePhyBit(struct rtl8168_private *tp, u8 addr, u16 mask)
{
        ClearAndSetPCIePhyBit( tp,
                               addr,
                               mask,
                               0
                             );
}

static void SetPCIePhyBit( struct rtl8168_private *tp, u8 addr, u16 mask)
{
        ClearAndSetPCIePhyBit( tp,
                               addr,
                               0,
                               mask
                             );
}

static u32
rtl8168_csi_other_fun_read(struct rtl8168_private *tp,
                           u8 multi_fun_sel_bit,
                           u32 addr)
{
        void __iomem *ioaddr = tp->mmio_addr;
        u32 cmd;
        int i;
        u32 value = 0;

        cmd = CSIAR_Read | CSIAR_ByteEn << CSIAR_ByteEn_shift | (addr & CSIAR_Addr_Mask);

        if (tp->mcfg != CFG_METHOD_20 && tp->mcfg != CFG_METHOD_23 &&
            tp->mcfg != CFG_METHOD_26 && tp->mcfg != CFG_METHOD_27 &&
            tp->mcfg != CFG_METHOD_28) {
                multi_fun_sel_bit = 0;
        }

        if( multi_fun_sel_bit > 7 ) {
                return 0xffffffff;
        }

        cmd |= multi_fun_sel_bit << 16;

        RTL_W32(CSIAR, cmd);

        for (i = 0; i < 10; i++) {
                udelay(100);

                /* Check if the RTL8168 has completed CSI read */
                if (RTL_R32(CSIAR) & CSIAR_Flag) {
                        value = (u32)RTL_R32(CSIDR);
                        break;
                }
        }

        udelay(20);

        return value;
}

static void
rtl8168_csi_other_fun_write(struct rtl8168_private *tp,
                            u8 multi_fun_sel_bit,
                            u32 addr,
                            u32 value)
{
        void __iomem *ioaddr = tp->mmio_addr;
        u32 cmd;
        int i;

        RTL_W32(CSIDR, value);
        cmd = CSIAR_Write | CSIAR_ByteEn << CSIAR_ByteEn_shift | (addr & CSIAR_Addr_Mask);
        if (tp->mcfg != CFG_METHOD_20 && tp->mcfg != CFG_METHOD_23 &&
            tp->mcfg != CFG_METHOD_26 && tp->mcfg != CFG_METHOD_27 &&
            tp->mcfg != CFG_METHOD_28) {
                multi_fun_sel_bit = 0;
        }

        if( multi_fun_sel_bit > 7 ) {
                return;
        }

        cmd |= multi_fun_sel_bit << 16;

        RTL_W32(CSIAR, cmd);

        for (i = 0; i < 10; i++) {
                udelay(100);

                /* Check if the RTL8168 has completed CSI write */
                if (!(RTL_R32(CSIAR) & CSIAR_Flag))
                        break;
        }

        udelay(20);
}

static u32
rtl8168_csi_read(struct rtl8168_private *tp,
                 u32 addr)
{
        u8 multi_fun_sel_bit;

        if (tp->mcfg == CFG_METHOD_20)
                multi_fun_sel_bit = 2;
        else if (tp->mcfg == CFG_METHOD_26)
                multi_fun_sel_bit = 1;
        else
                multi_fun_sel_bit = 0;


        return rtl8168_csi_other_fun_read(tp, multi_fun_sel_bit, addr);
}

static void
rtl8168_csi_write(struct rtl8168_private *tp,
                  u32 addr,
                  u32 value)
{
        u8 multi_fun_sel_bit;

        if (tp->mcfg == CFG_METHOD_20)
                multi_fun_sel_bit = 2;
        else if (tp->mcfg == CFG_METHOD_26)
                multi_fun_sel_bit = 1;
        else
                multi_fun_sel_bit = 0;

        rtl8168_csi_other_fun_write(tp, multi_fun_sel_bit, addr, value);
}

static u8
rtl8168_csi_fun0_read_byte(struct rtl8168_private *tp,
                           u32 addr)
{
        u8 RetVal = 0;

        if (tp->mcfg == CFG_METHOD_20 || tp->mcfg == CFG_METHOD_26) {
                u32 TmpUlong;
                u16 RegAlignAddr;
                u8 ShiftByte;

                RegAlignAddr = addr & ~(0x3);
                ShiftByte = addr & (0x3);
                TmpUlong = rtl8168_csi_other_fun_read(tp, 0, addr);
                TmpUlong >>= (8*ShiftByte);
                RetVal = (u8)TmpUlong;
        } else {
                struct pci_dev *pdev = tp->pci_dev;

                pci_read_config_byte(pdev, addr, &RetVal);
        }

        udelay(20);

        return RetVal;
}

static void
rtl8168_csi_fun0_write_byte(struct rtl8168_private *tp,
                            u32 addr,
                            u8 value)
{
        if (tp->mcfg == CFG_METHOD_20 || tp->mcfg == CFG_METHOD_26) {
                u32 TmpUlong;
                u16 RegAlignAddr;
                u8 ShiftByte;

                RegAlignAddr = addr & ~(0x3);
                ShiftByte = addr & (0x3);
                TmpUlong = rtl8168_csi_other_fun_read(tp, 0, RegAlignAddr);
                TmpUlong &= ~(0xFF << (8*ShiftByte));
                TmpUlong |= (value << (8*ShiftByte));
                rtl8168_csi_other_fun_write( tp, 0, RegAlignAddr, TmpUlong );
        } else {
                struct pci_dev *pdev = tp->pci_dev;

                pci_write_config_byte(pdev, addr, value);
        }

        udelay(20);
}

u32 rtl8168_eri_read(void __iomem *ioaddr, int addr, int len, int type)
{
        int i, val_shift, shift = 0;
        u32 value1 = 0, value2 = 0, mask;
        u32 eri_cmd;

        if (len > 4 || len <= 0)
                return -1;

        while (len > 0) {
                val_shift = addr % ERIAR_Addr_Align;
                addr = addr & ~0x3;

                eri_cmd = ERIAR_Read |
                          type << ERIAR_Type_shift |
                          ERIAR_ByteEn << ERIAR_ByteEn_shift |
                          (addr & 0x0FFF);
                if (addr & 0xF000) {
                        u32 tmp;

                        tmp = addr & 0xF000;
                        tmp >>= 12;
                        eri_cmd |= (tmp << 20) & 0x00F00000;
                }

                RTL_W32(ERIAR, eri_cmd);

                for (i = 0; i < 10; i++) {
                        udelay(100);

                        /* Check if the RTL8168 has completed ERI read */
                        if (RTL_R32(ERIAR) & ERIAR_Flag)
                                break;
                }

                if (len == 1)       mask = (0xFF << (val_shift * 8)) & 0xFFFFFFFF;
                else if (len == 2)  mask = (0xFFFF << (val_shift * 8)) & 0xFFFFFFFF;
                else if (len == 3)  mask = (0xFFFFFF << (val_shift * 8)) & 0xFFFFFFFF;
                else            mask = (0xFFFFFFFF << (val_shift * 8)) & 0xFFFFFFFF;

                value1 = RTL_R32(ERIDR) & mask;
                value2 |= (value1 >> val_shift * 8) << shift * 8;

                if (len <= 4 - val_shift) {
                        len = 0;
                } else {
                        len -= (4 - val_shift);
                        shift = 4 - val_shift;
                        addr += 4;
                }
        }

        udelay(20);

        return value2;
}

int rtl8168_eri_write(void __iomem *ioaddr, int addr, int len, u32 value, int type)
{

        int i, val_shift, shift = 0;
        u32 value1 = 0, mask;
        u32 eri_cmd;

        if (len > 4 || len <= 0)
                return -1;

        while (len > 0) {
                val_shift = addr % ERIAR_Addr_Align;
                addr = addr & ~0x3;

                if (len == 1)       mask = (0xFF << (val_shift * 8)) & 0xFFFFFFFF;
                else if (len == 2)  mask = (0xFFFF << (val_shift * 8)) & 0xFFFFFFFF;
                else if (len == 3)  mask = (0xFFFFFF << (val_shift * 8)) & 0xFFFFFFFF;
                else            mask = (0xFFFFFFFF << (val_shift * 8)) & 0xFFFFFFFF;

                value1 = rtl8168_eri_read(ioaddr, addr, 4, type) & ~mask;
                value1 |= ((value << val_shift * 8) >> shift * 8);

                RTL_W32(ERIDR, value1);

                eri_cmd = ERIAR_Write |
                          type << ERIAR_Type_shift |
                          ERIAR_ByteEn << ERIAR_ByteEn_shift |
                          (addr & 0x0FFF);
                if (addr & 0xF000) {
                        u32 tmp;

                        tmp = addr & 0xF000;
                        tmp >>= 12;
                        eri_cmd |= (tmp << 20) & 0x00F00000;
                }

                RTL_W32(ERIAR, eri_cmd);

                for (i = 0; i < 10; i++) {
                        udelay(100);

                        /* Check if the RTL8168 has completed ERI write */
                        if (!(RTL_R32(ERIAR) & ERIAR_Flag))
                                break;
                }

                if (len <= 4 - val_shift) {
                        len = 0;
                } else {
                        len -= (4 - val_shift);
                        shift = 4 - val_shift;
                        addr += 4;
                }
        }

        udelay(20);

        return 0;
}

static void
rtl8168_enable_rxdvgate(struct net_device *dev)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;

        switch (tp->mcfg) {
        case CFG_METHOD_21:
        case CFG_METHOD_22:
        case CFG_METHOD_23:
        case CFG_METHOD_24:
        case CFG_METHOD_25:
        case CFG_METHOD_26:
        case CFG_METHOD_27:
        case CFG_METHOD_28:
        case CFG_METHOD_29:
        case CFG_METHOD_30:
                RTL_W8(0xF2, RTL_R8(0xF2) | BIT_3);
                mdelay(2);
                break;
        }
}

static void
rtl8168_disable_rxdvgate(struct net_device *dev)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;

        switch (tp->mcfg) {
        case CFG_METHOD_21:
        case CFG_METHOD_22:
        case CFG_METHOD_23:
        case CFG_METHOD_24:
        case CFG_METHOD_25:
        case CFG_METHOD_26:
        case CFG_METHOD_27:
        case CFG_METHOD_28:
        case CFG_METHOD_29:
        case CFG_METHOD_30:
                RTL_W8(0xF2, RTL_R8(0xF2) & ~BIT_3);
                mdelay(2);
                break;
        }
}

void
rtl8168_wait_txrx_fifo_empty(struct net_device *dev)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;
        int i;

        switch (tp->mcfg) {
        case CFG_METHOD_21:
        case CFG_METHOD_22:
        case CFG_METHOD_23:
        case CFG_METHOD_24:
        case CFG_METHOD_25:
        case CFG_METHOD_26:
        case CFG_METHOD_27:
        case CFG_METHOD_28:
        case CFG_METHOD_29:
        case CFG_METHOD_30:
                for (i = 0; i < 10; i++) {
                        udelay(100);
                        if (RTL_R32(TxConfig) & BIT_11)
                                break;
                }

                for (i = 0; i < 10; i++) {
                        udelay(100);
                        if ((RTL_R8(MCUCmd_reg) & (Txfifo_empty | Rxfifo_empty)) == (Txfifo_empty | Rxfifo_empty))
                                break;

                }
                break;
        }
}

#ifdef ENABLE_DASH_SUPPORT

inline void
rtl8168_enable_dash2_interrupt(struct rtl8168_private *tp, void __iomem *ioaddr)
{
        if (!tp->DASH) return;

        if( HW_DASH_SUPPORT_TYPE_2( tp ) )
                RTL_W8(IBIMR0, ( ISRIMR_DASH_TYPE2_ROK | ISRIMR_DASH_TYPE2_TOK | ISRIMR_DASH_TYPE2_TDU | ISRIMR_DASH_TYPE2_RDU | ISRIMR_DASH_TYPE2_RX_DISABLE_IDLE ));
}

static inline void
rtl8168_disable_dash2_interrupt(struct rtl8168_private *tp, void __iomem *ioaddr)
{
        if (!tp->DASH) return;

        if( HW_DASH_SUPPORT_TYPE_2( tp ) )
                RTL_W8(IBIMR0, 0);
}
#endif

static inline void
rtl8168_enable_hw_interrupt(struct rtl8168_private *tp, void __iomem *ioaddr)
{
        RTL_W16(IntrMask, tp->intr_mask);

#ifdef ENABLE_DASH_SUPPORT
        if (tp->DASH)
                rtl8168_enable_dash2_interrupt(tp, ioaddr);
#endif
}

static inline void
rtl8168_disable_hw_interrupt(struct rtl8168_private *tp, void __iomem *ioaddr)
{
        RTL_W16(IntrMask, 0x0000);

#ifdef ENABLE_DASH_SUPPORT
        if (tp->DASH)
                rtl8168_disable_dash2_interrupt(tp, ioaddr);
#endif
}


static inline void
rtl8168_switch_to_hw_interrupt(struct rtl8168_private *tp, void __iomem *ioaddr)
{
        RTL_W32(TimeInt0, 0x0000);

        rtl8168_enable_hw_interrupt(tp, ioaddr);
}

static inline void
rtl8168_switch_to_timer_interrupt(struct rtl8168_private *tp, void __iomem *ioaddr)
{
        if (tp->use_timer_interrrupt) {
                RTL_W32(TCTR, timer_count);
                RTL_W32(TimeInt0, timer_count);
                RTL_W16(IntrMask, tp->timer_intr_mask);

#ifdef ENABLE_DASH_SUPPORT
                if (tp->DASH)
                        rtl8168_enable_dash2_interrupt(tp, ioaddr);
#endif
        } else {
                rtl8168_switch_to_hw_interrupt(tp, ioaddr);
        }
}

static void
rtl8168_irq_mask_and_ack(struct rtl8168_private *tp, void __iomem *ioaddr)
{
        rtl8168_disable_hw_interrupt(tp, ioaddr);
        RTL_W16(IntrStatus, RTL_R16(IntrStatus));

#ifdef ENABLE_DASH_SUPPORT
        if ( tp->DASH ) {
                if( HW_DASH_SUPPORT_TYPE_2( tp ) ) {
                        RTL_W8(IBISR0, RTL_R16(IBISR0));
                }
        }
#endif
}

static void
rtl8168_nic_reset(struct net_device *dev)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;
        int i;

        RTL_W32(RxConfig, (RX_DMA_BURST << RxCfgDMAShift));

        rtl8168_enable_rxdvgate(dev);

        rtl8168_wait_txrx_fifo_empty(dev);

        switch (tp->mcfg) {
        case CFG_METHOD_1:
        case CFG_METHOD_2:
        case CFG_METHOD_3:
                mdelay(10);
                break;
        case CFG_METHOD_4:
        case CFG_METHOD_5:
        case CFG_METHOD_6:
        case CFG_METHOD_7:
        case CFG_METHOD_8:
        case CFG_METHOD_9:
        case CFG_METHOD_10:
        case CFG_METHOD_14:
        case CFG_METHOD_15:
                RTL_W8(ChipCmd, StopReq | CmdRxEnb | CmdTxEnb);
                udelay(100);
                break;
        case CFG_METHOD_11:
        case CFG_METHOD_12:
        case CFG_METHOD_13:
                while (RTL_R8(TxPoll) & NPQ)
                        udelay(20);
                break;
        case CFG_METHOD_21:
        case CFG_METHOD_22:
        case CFG_METHOD_23:
        case CFG_METHOD_24:
        case CFG_METHOD_25:
        case CFG_METHOD_26:
        case CFG_METHOD_27:
        case CFG_METHOD_28:
        case CFG_METHOD_29:
        case CFG_METHOD_30:
                mdelay(2);
                break;
        default:
                mdelay(10);
                break;
        }

        /* Soft reset the chip. */
        RTL_W8(ChipCmd, CmdReset);

        /* Check that the chip has finished the reset. */
        for (i = 100; i > 0; i--) {
                udelay(100);
                if ((RTL_R8(ChipCmd) & CmdReset) == 0)
                        break;
        }

        switch (tp->mcfg) {
        case CFG_METHOD_11:
                OOB_mutex_lock(tp);
                OCP_write(tp, 0x10, 2, OCP_read(tp, 0x010, 2)&~0x00004000);
                OOB_mutex_unlock(tp);

                OOB_notify(tp, OOB_CMD_RESET);

                for (i = 0; i < 10; i++) {
                        mdelay(10);
                        if (OCP_read(tp, 0x010, 2)&0x00004000)
                                break;
                }

                for (i = 0; i < 5; i++) {
                        if ( OCP_read(tp, 0x034, 1) == 0)
                                break;
                }
                break;
        }
}

static void
rtl8168_hw_clear_timer_int(struct net_device *dev)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;

        RTL_W32(TimeInt0, 0x0000);

        switch (tp->mcfg) {
        case CFG_METHOD_4:
        case CFG_METHOD_5:
        case CFG_METHOD_6:
        case CFG_METHOD_7:
        case CFG_METHOD_8:
                RTL_W32(TimeInt1, 0x0000);
                break;
        case CFG_METHOD_9:
        case CFG_METHOD_10:
        case CFG_METHOD_11:
        case CFG_METHOD_12:
        case CFG_METHOD_13:
        case CFG_METHOD_14:
        case CFG_METHOD_15:
        case CFG_METHOD_16:
        case CFG_METHOD_17:
        case CFG_METHOD_18:
        case CFG_METHOD_19:
        case CFG_METHOD_20:
        case CFG_METHOD_21:
        case CFG_METHOD_22:
        case CFG_METHOD_23:
        case CFG_METHOD_24:
        case CFG_METHOD_25:
        case CFG_METHOD_26:
        case CFG_METHOD_27:
        case CFG_METHOD_28:
        case CFG_METHOD_29:
        case CFG_METHOD_30:
                RTL_W32(TimeInt1, 0x0000);
                RTL_W32(TimeInt2, 0x0000);
                RTL_W32(TimeInt3, 0x0000);
                break;
        }
}

static void
rtl8168_hw_reset(struct net_device *dev)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;

        /* Disable interrupts */
        rtl8168_irq_mask_and_ack(tp, ioaddr);

        rtl8168_hw_clear_timer_int(dev);

        rtl8168_nic_reset(dev);
}

static void rtl8168_mac_loopback_test(struct rtl8168_private *tp)
{
        void __iomem *ioaddr = tp->mmio_addr;
        struct net_device *dev = tp->dev;
        struct sk_buff *skb, *rx_skb;
        dma_addr_t mapping;
        struct TxDesc *txd;
        struct RxDesc *rxd;
        void *tmpAddr;
        u32 len, rx_len, rx_cmd;
        u16 type;
        u8 pattern;
        int i;

        if (tp->DASH)
                return;

        pattern = 0x5A;
        len = 60;
        type = htons(ETH_P_IP);
        txd = tp->TxDescArray;
        rxd = tp->RxDescArray;
        rx_skb = tp->Rx_skbuff[0];
        RTL_W32(TxConfig, (RTL_R32(TxConfig) & ~0x00060000) | 0x00020000);

        do {
                skb = dev_alloc_skb(len + RTK_RX_ALIGN);
                if (unlikely(!skb))
                        dev_printk(KERN_NOTICE, &tp->pci_dev->dev, "-ENOMEM;\n");
        } while (unlikely(skb == NULL));
        skb_reserve(skb, RTK_RX_ALIGN);

        memcpy(skb_put(skb, dev->addr_len), dev->dev_addr, dev->addr_len);
        memcpy(skb_put(skb, dev->addr_len), dev->dev_addr, dev->addr_len);
        memcpy(skb_put(skb, sizeof(type)), &type, sizeof(type));
        tmpAddr = skb_put(skb, len - 14);

        mapping = pci_map_single(tp->pci_dev, skb->data, len, PCI_DMA_TODEVICE);
        pci_dma_sync_single_for_cpu(tp->pci_dev, le64_to_cpu(mapping),
                                    len, PCI_DMA_TODEVICE);
        txd->addr = cpu_to_le64(mapping);
        txd->opts2 = 0;
        while (1) {
                memset(tmpAddr, pattern++, len - 14);
                pci_dma_sync_single_for_device(tp->pci_dev,
                                               le64_to_cpu(mapping),
                                               len, PCI_DMA_TODEVICE);
                txd->opts1 = cpu_to_le32(DescOwn | FirstFrag | LastFrag | len);

                RTL_W32(RxConfig, RTL_R32(RxConfig)  | AcceptMyPhys);

                smp_wmb();
                RTL_W8(TxPoll, NPQ);    /* set polling bit */

                for (i = 0; i < 50; i++) {
                        udelay(200);
                        rx_cmd = le32_to_cpu(rxd->opts1);
                        if ((rx_cmd & DescOwn) == 0)
                                break;
                }

                RTL_W32(RxConfig, RTL_R32(RxConfig) & ~(AcceptErr | AcceptRunt | AcceptBroadcast | AcceptMulticast | AcceptMyPhys |  AcceptAllPhys));

                rx_len = rx_cmd & 0x3FFF;
                rx_len -= 4;
                rxd->opts1 = cpu_to_le32(DescOwn | tp->rx_buf_sz);

                pci_dma_sync_single_for_cpu(tp->pci_dev, le64_to_cpu(mapping), len, PCI_DMA_TODEVICE);

                if (rx_len == len) {
                        pci_dma_sync_single_for_cpu(tp->pci_dev, le64_to_cpu(rxd->addr), tp->rx_buf_sz, PCI_DMA_FROMDEVICE);
                        i = memcmp(skb->data, rx_skb->data, rx_len);
                        pci_dma_sync_single_for_device(tp->pci_dev, le64_to_cpu(rxd->addr), tp->rx_buf_sz, PCI_DMA_FROMDEVICE);
                        if (i == 0) {
//              dev_printk(KERN_INFO, &tp->pci_dev->dev, "loopback test finished\n",rx_len,len);
                                break;
                        }
                }

                rtl8168_hw_reset(dev);
                rtl8168_disable_rxdvgate(dev);
                RTL_W8(ChipCmd, CmdTxEnb | CmdRxEnb);
        }
        tp->dirty_tx++;
        tp->dirty_rx++;
        tp->cur_tx++;
        tp->cur_rx++;
        pci_unmap_single(tp->pci_dev, le64_to_cpu(mapping),
                         len, PCI_DMA_TODEVICE);
        RTL_W32(TxConfig, RTL_R32(TxConfig) & ~0x00060000);
        dev_kfree_skb_any(skb);
        RTL_W16(IntrStatus, 0xFFBF);
}

static unsigned int
rtl8168_xmii_reset_pending(struct net_device *dev)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        unsigned int retval;
        unsigned long flags;

        spin_lock_irqsave(&tp->phy_lock, flags);
        mdio_write(tp, 0x1f, 0x0000);
        retval = mdio_read(tp, MII_BMCR) & BMCR_RESET;
        spin_unlock_irqrestore(&tp->phy_lock, flags);

        return retval;
}

static unsigned int
rtl8168_xmii_link_ok(struct net_device *dev)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;
        unsigned int retval;

        retval = (RTL_R8(PHYstatus) & LinkStatus) ? 1 : 0;

        return retval;
}

static void
rtl8168_xmii_reset_enable(struct net_device *dev)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        int i, val = 0;
        unsigned long flags;

        spin_lock_irqsave(&tp->phy_lock, flags);
        mdio_write(tp, 0x1f, 0x0000);
        mdio_write(tp, MII_BMCR, BMCR_RESET | BMCR_ANENABLE);

        for (i = 0; i < 2500; i++) {
                val = mdio_read(tp, MII_BMCR) & BMCR_RESET;

                if (!val) {
                        spin_unlock_irqrestore(&tp->phy_lock, flags);
                        return;
                }

                mdelay(1);
        }

        spin_unlock_irqrestore(&tp->phy_lock, flags);

        if (netif_msg_link(tp))
                printk(KERN_ERR "%s: PHY reset failed.\n", dev->name);
}

static void
rtl8168dp_10mbps_gphy_para(struct net_device *dev)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;
        u8 status = RTL_R8(PHYstatus);
        unsigned long flags;

        spin_lock_irqsave(&tp->phy_lock, flags);
        if ((status & LinkStatus) && (status & _10bps)) {
                mdio_write(tp, 0x1f, 0x0000);
                mdio_write(tp, 0x10, 0x04EE);
        } else {
                mdio_write(tp, 0x1f, 0x0000);
                mdio_write(tp, 0x10, 0x01EE);
        }
        spin_unlock_irqrestore(&tp->phy_lock, flags);
}

void rtl8168_init_ring_indexes(struct rtl8168_private *tp)
{
        tp->dirty_tx = 0;
        tp->dirty_rx = 0;
        tp->cur_tx = 0;
        tp->cur_rx = 0;
}

static void
rtl8168_issue_offset_99_event(struct rtl8168_private *tp)
{
        void __iomem *ioaddr = tp->mmio_addr;
        u32 csi_tmp;

        switch (tp->mcfg) {
        case CFG_METHOD_21:
        case CFG_METHOD_22:
        case CFG_METHOD_23:
        case CFG_METHOD_24:
        case CFG_METHOD_25:
        case CFG_METHOD_27:
        case CFG_METHOD_28:
                if (tp->mcfg == CFG_METHOD_24 || tp->mcfg == CFG_METHOD_25 ||
                    tp->mcfg == CFG_METHOD_27 || tp->mcfg == CFG_METHOD_28) {
                        rtl8168_eri_write(ioaddr, 0x3FC, 4, 0x00000000, ERIAR_ExGMAC);
                } else {
                        rtl8168_eri_write(ioaddr, 0x3FC, 4, 0x083C083C, ERIAR_ExGMAC);
                }
                csi_tmp = rtl8168_eri_read(ioaddr, 0x3F8, 1, ERIAR_ExGMAC);
                csi_tmp |= BIT_0;
                rtl8168_eri_write(ioaddr, 0x3F8, 1, csi_tmp, ERIAR_ExGMAC);
                break;
        case CFG_METHOD_29:
        case CFG_METHOD_30:
                csi_tmp = rtl8168_eri_read(ioaddr, 0x1EA, 1, ERIAR_ExGMAC);
                csi_tmp |= BIT_0;
                rtl8168_eri_write(ioaddr, 0x1EA, 1, csi_tmp, ERIAR_ExGMAC);
                break;
        }
}

#ifdef ENABLE_DASH_SUPPORT
static void
NICChkTypeEnableDashInterrupt(struct rtl8168_private *tp)
{
        void __iomem *ioaddr = tp->mmio_addr;

        if (tp->DASH) {
                //
                // even disconnected, enable 3 dash interrupt mask bits for in-band/out-band communication
                //
                if( HW_DASH_SUPPORT_TYPE_2( tp ) ) {
                        rtl8168_enable_dash2_interrupt(tp, ioaddr);
                        RTL_W16(IntrMask, (ISRIMR_DASH_INTR_EN | ISRIMR_DASH_INTR_CMAC_RESET));
                } else {
                        RTL_W16(IntrMask, (ISRIMR_DP_DASH_OK | ISRIMR_DP_HOST_OK | ISRIMR_DP_REQSYS_OK));
                }
        }
}
#endif

static void
rtl8168_check_link_status(struct net_device *dev)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;
        int link_status_on;
        unsigned long flags;

        link_status_on = tp->link_ok(dev);

        if (tp->mcfg == CFG_METHOD_11)
                rtl8168dp_10mbps_gphy_para(dev);

        if (netif_carrier_ok(dev) != link_status_on) {
                if (link_status_on) {
                        if (tp->mcfg == CFG_METHOD_18 || tp->mcfg == CFG_METHOD_19 || tp->mcfg == CFG_METHOD_20) {
                                if (RTL_R8(PHYstatus) & _1000bpsF) {
                                        rtl8168_eri_write(ioaddr, 0x1bc, 4, 0x00000011, ERIAR_ExGMAC);
                                        rtl8168_eri_write(ioaddr, 0x1dc, 4, 0x00000005, ERIAR_ExGMAC);
                                } else {
                                        rtl8168_eri_write(ioaddr, 0x1bc, 4, 0x0000001f, ERIAR_ExGMAC);
                                        rtl8168_eri_write(ioaddr, 0x1dc, 4, 0x0000003f, ERIAR_ExGMAC);
                                }
                        } else if ((tp->mcfg == CFG_METHOD_16 || tp->mcfg == CFG_METHOD_17) && netif_running(dev)) {
                                if (tp->mcfg == CFG_METHOD_16 && (RTL_R8(PHYstatus) & _10bps)) {
                                        RTL_W32(RxConfig, RTL_R32(RxConfig) | AcceptAllPhys);
                                } else if (tp->mcfg == CFG_METHOD_17) {
                                        if (RTL_R8(PHYstatus) & _1000bpsF) {
                                                rtl8168_eri_write(ioaddr, 0x1bc, 4, 0x00000011, ERIAR_ExGMAC);
                                                rtl8168_eri_write(ioaddr, 0x1dc, 4, 0x00000005, ERIAR_ExGMAC);
                                        } else if (RTL_R8(PHYstatus) & _100bps) {
                                                rtl8168_eri_write(ioaddr, 0x1bc, 4, 0x0000001f, ERIAR_ExGMAC);
                                                rtl8168_eri_write(ioaddr, 0x1dc, 4, 0x00000005, ERIAR_ExGMAC);
                                        } else {
                                                rtl8168_eri_write(ioaddr, 0x1bc, 4, 0x0000001f, ERIAR_ExGMAC);
                                                rtl8168_eri_write(ioaddr, 0x1dc, 4, 0x0000003f, ERIAR_ExGMAC);
                                        }
                                }
                        } else if ((tp->mcfg == CFG_METHOD_14 || tp->mcfg == CFG_METHOD_15) && eee_enable ==1) {
                                /*Full -Duplex  mode*/
                                if (RTL_R8(PHYstatus)&FullDup) {
                                        spin_lock_irqsave(&tp->phy_lock, flags);
                                        mdio_write(tp, 0x1F, 0x0006);
                                        mdio_write(tp, 0x00, 0x5a30);
                                        mdio_write(tp, 0x1F, 0x0000);
                                        spin_unlock_irqrestore(&tp->phy_lock, flags);
                                        if (RTL_R8(PHYstatus) & (_10bps | _100bps))
                                                RTL_W32(TxConfig, (RTL_R32(TxConfig) & ~BIT_19) | BIT_25);

                                } else {
                                        spin_lock_irqsave(&tp->phy_lock, flags);
                                        mdio_write(tp, 0x1F, 0x0006);
                                        mdio_write(tp, 0x00, 0x5a00);
                                        mdio_write(tp, 0x1F, 0x0000);
                                        spin_unlock_irqrestore(&tp->phy_lock, flags);
                                        if (RTL_R8(PHYstatus) & (_10bps | _100bps))
                                                RTL_W32(TxConfig, (RTL_R32(TxConfig) & ~BIT_19) | (InterFrameGap << TxInterFrameGapShift));
                                }
                        } else if ((tp->mcfg == CFG_METHOD_21 || tp->mcfg == CFG_METHOD_22 ||
                                    tp->mcfg == CFG_METHOD_23 || tp->mcfg == CFG_METHOD_24 ||
                                    tp->mcfg == CFG_METHOD_25 || tp->mcfg == CFG_METHOD_26 ||
                                    tp->mcfg == CFG_METHOD_27 || tp->mcfg == CFG_METHOD_28 ||
                                    tp->mcfg == CFG_METHOD_29 || tp->mcfg == CFG_METHOD_30) &&
                                   netif_running(dev)) {
                                if (RTL_R8(PHYstatus)&FullDup)
                                        RTL_W32(TxConfig, (RTL_R32(TxConfig) | (BIT_24 | BIT_25)) & ~BIT_19);
                                else
                                        RTL_W32(TxConfig, (RTL_R32(TxConfig) | BIT_25) & ~(BIT_19 | BIT_24));
                        }

                        if (tp->mcfg == CFG_METHOD_21 || tp->mcfg == CFG_METHOD_22 ||
                            tp->mcfg == CFG_METHOD_27 || tp->mcfg == CFG_METHOD_28) {
                                /*half mode*/
                                if (!(RTL_R8(PHYstatus)&FullDup)) {
                                        spin_lock_irqsave(&tp->phy_lock, flags);
                                        mdio_write(tp, 0x1F, 0x0000);
                                        mdio_write(tp, MII_ADVERTISE, mdio_read(tp, MII_ADVERTISE)&~(ADVERTISE_PAUSE_CAP|ADVERTISE_PAUSE_ASYM));
                                        spin_unlock_irqrestore(&tp->phy_lock, flags);
                                }
                        }

                        rtl8168_hw_start(dev);

                        netif_carrier_on(dev);

                        netif_wake_queue(dev);

                        if (netif_msg_ifup(tp))
                                printk(KERN_INFO PFX "%s: link up\n", dev->name);
                } else {
                        if (netif_msg_ifdown(tp))
                                printk(KERN_INFO PFX "%s: link down\n", dev->name);

                        netif_stop_queue(dev);

                        netif_carrier_off(dev);

                        rtl8168_hw_reset(dev);

                        rtl8168_tx_clear(tp);

                        rtl8168_rx_clear(tp);

                        rtl8168_init_ring(dev);

                        rtl8168_set_speed(dev, tp->autoneg, tp->speed, tp->duplex);

                        switch (tp->mcfg) {
                        case CFG_METHOD_21:
                        case CFG_METHOD_22:
                        case CFG_METHOD_23:
                        case CFG_METHOD_24:
                        case CFG_METHOD_25:
                        case CFG_METHOD_27:
                        case CFG_METHOD_28:
                                if (tp->org_pci_offset_99 & BIT_2)
                                        tp->issue_offset_99_event = TRUE;
                                break;
                        }

#ifdef ENABLE_DASH_SUPPORT
                        if (tp->DASH) {
                                NICChkTypeEnableDashInterrupt(tp);
                        }
#endif
                }
        }

        if (!link_status_on) {
                switch (tp->mcfg) {
                case CFG_METHOD_21:
                case CFG_METHOD_22:
                case CFG_METHOD_23:
                case CFG_METHOD_24:
                case CFG_METHOD_25:
                case CFG_METHOD_27:
                case CFG_METHOD_28:
                        if (tp->issue_offset_99_event) {
                                if (!(RTL_R8(PHYstatus) & PowerSaveStatus)) {
                                        tp->issue_offset_99_event = FALSE;
                                        rtl8168_issue_offset_99_event(tp);
                                }
                        }
                        break;
                }
        }
}

static void
rtl8168_link_option(u8 *aut,
                    u16 *spd,
                    u8 *dup)
{
        if ((*spd != SPEED_1000) && (*spd != SPEED_100) && (*spd != SPEED_10))
                *spd = SPEED_1000;

        if ((*dup != DUPLEX_FULL) && (*dup != DUPLEX_HALF))
                *dup = DUPLEX_FULL;

        if ((*aut != AUTONEG_ENABLE) && (*aut != AUTONEG_DISABLE))
                *aut = AUTONEG_ENABLE;
}

void
rtl8168_wait_ll_share_fifo_ready(struct net_device *dev)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;
        int i;

        for (i = 0; i < 10; i++) {
                udelay(100);
                if (RTL_R16(0xD2) & BIT_9)
                        break;
        }
}

static void
rtl8168_disable_pci_offset_99(struct rtl8168_private *tp)
{
        void __iomem *ioaddr = tp->mmio_addr;
        u32 csi_tmp;

        switch (tp->mcfg) {
        case CFG_METHOD_21:
        case CFG_METHOD_22:
        case CFG_METHOD_23:
        case CFG_METHOD_24:
        case CFG_METHOD_25:
        case CFG_METHOD_26:
        case CFG_METHOD_27:
        case CFG_METHOD_28:
        case CFG_METHOD_29:
        case CFG_METHOD_30:
                csi_tmp = rtl8168_eri_read(ioaddr, 0x3F2, 2, ERIAR_ExGMAC);
                csi_tmp &= ~(BIT_0 | BIT_1);
                rtl8168_eri_write(ioaddr, 0x3F2, 2, csi_tmp, ERIAR_ExGMAC);
                break;
        }

        switch (tp->mcfg) {
        case CFG_METHOD_21:
        case CFG_METHOD_22:
        case CFG_METHOD_26:
        case CFG_METHOD_29:
        case CFG_METHOD_30:
                rtl8168_csi_fun0_write_byte(tp, 0x99, 0x00);
                break;
        }
}

static void
rtl8168_enable_pci_offset_99(struct rtl8168_private *tp)
{
        void __iomem *ioaddr = tp->mmio_addr;
        u32 csi_tmp;

        switch (tp->mcfg) {
        case CFG_METHOD_21:
        case CFG_METHOD_22:
        case CFG_METHOD_26:
        case CFG_METHOD_29:
        case CFG_METHOD_30:
                rtl8168_csi_fun0_write_byte(tp, 0x99, tp->org_pci_offset_99);
                break;
        }

        switch (tp->mcfg) {
        case CFG_METHOD_21:
        case CFG_METHOD_22:
        case CFG_METHOD_23:
        case CFG_METHOD_24:
        case CFG_METHOD_25:
        case CFG_METHOD_26:
        case CFG_METHOD_27:
        case CFG_METHOD_28:
        case CFG_METHOD_29:
        case CFG_METHOD_30:
                csi_tmp = rtl8168_eri_read(ioaddr, 0x3F2, 2, ERIAR_ExGMAC);
                csi_tmp &= ~(BIT_0 | BIT_1);
                if (!(tp->org_pci_offset_99 & (BIT_5 | BIT_6)))
                        csi_tmp |= BIT_1;
                if (!(tp->org_pci_offset_99 & BIT_2))
                        csi_tmp |= BIT_0;
                rtl8168_eri_write(ioaddr, 0x3F2, 2, csi_tmp, ERIAR_ExGMAC);
                break;
        }
}

static void
rtl8168_init_pci_offset_99(struct rtl8168_private *tp)
{
        void __iomem *ioaddr = tp->mmio_addr;
        u32 csi_tmp;

        switch (tp->mcfg) {
        case CFG_METHOD_26:
                if (tp->org_pci_offset_99 & BIT_2) {
                        csi_tmp = rtl8168_eri_read(ioaddr, 0x5C2, 1, ERIAR_ExGMAC);
                        csi_tmp &= ~BIT_1;
                        rtl8168_eri_write(ioaddr, 0x5C2, 1, csi_tmp, ERIAR_ExGMAC);
                }
                break;
        }

        switch (tp->mcfg) {
        case CFG_METHOD_21:
        case CFG_METHOD_22:
        case CFG_METHOD_23:
        case CFG_METHOD_24:
        case CFG_METHOD_25:
        case CFG_METHOD_26:
        case CFG_METHOD_27:
        case CFG_METHOD_28:
        case CFG_METHOD_29:
        case CFG_METHOD_30:
                csi_tmp = rtl8168_eri_read(ioaddr, 0x3F2, 2, ERIAR_ExGMAC);
                csi_tmp &= ~( BIT_8 | BIT_9  | BIT_10 | BIT_11  | BIT_12  | BIT_13  | BIT_14 | BIT_15 );
                csi_tmp |= ( BIT_9 | BIT_10 | BIT_13  | BIT_14 | BIT_15 );
                rtl8168_eri_write(ioaddr, 0x3F2, 2, csi_tmp, ERIAR_ExGMAC);
                csi_tmp = rtl8168_eri_read(ioaddr, 0x3F5, 1, ERIAR_ExGMAC);
                csi_tmp |= BIT_6 | BIT_7;
                rtl8168_eri_write(ioaddr, 0x3F5, 1, csi_tmp, ERIAR_ExGMAC);
                mac_ocp_write(tp, 0xE02C, 0x1880);
                mac_ocp_write(tp, 0xE02E, 0x4880);
                break;
        }

        switch (tp->mcfg) {
        case CFG_METHOD_26:
                rtl8168_eri_write(ioaddr, 0x5C0, 1, 0xFA, ERIAR_ExGMAC);
                break;
        }

        switch (tp->mcfg) {
        case CFG_METHOD_26:
        case CFG_METHOD_29:
        case CFG_METHOD_30:
                if (tp->org_pci_offset_99 & BIT_2) {
                        csi_tmp = rtl8168_eri_read(ioaddr, 0x5C8, 1, ERIAR_ExGMAC);
                        csi_tmp |= BIT_0;
                        rtl8168_eri_write(ioaddr, 0x5C8, 1, csi_tmp, ERIAR_ExGMAC);
                }
                break;
        }

        switch (tp->mcfg) {
        case CFG_METHOD_23:
                rtl8168_eri_write(ioaddr, 0x2E8, 2, 0x883C, ERIAR_ExGMAC);
                rtl8168_eri_write(ioaddr, 0x2EA, 2, 0x8C12, ERIAR_ExGMAC);
                rtl8168_eri_write(ioaddr, 0x2EC, 2, 0x9003, ERIAR_ExGMAC);
                rtl8168_eri_write(ioaddr, 0x2E2, 2, 0x883C, ERIAR_ExGMAC);
                rtl8168_eri_write(ioaddr, 0x2E4, 2, 0x8C12, ERIAR_ExGMAC);
                rtl8168_eri_write(ioaddr, 0x2E6, 2, 0x9003, ERIAR_ExGMAC);
                break;
        case CFG_METHOD_21:
        case CFG_METHOD_22:
        case CFG_METHOD_24:
        case CFG_METHOD_25:
        case CFG_METHOD_26:
        case CFG_METHOD_27:
        case CFG_METHOD_28:
        case CFG_METHOD_29:
        case CFG_METHOD_30:
                rtl8168_eri_write(ioaddr, 0x2E8, 2, 0x9003, ERIAR_ExGMAC);
                rtl8168_eri_write(ioaddr, 0x2EA, 2, 0x9003, ERIAR_ExGMAC);
                rtl8168_eri_write(ioaddr, 0x2EC, 2, 0x9003, ERIAR_ExGMAC);
                rtl8168_eri_write(ioaddr, 0x2E2, 2, 0x883C, ERIAR_ExGMAC);
                rtl8168_eri_write(ioaddr, 0x2E4, 2, 0x8C12, ERIAR_ExGMAC);
                rtl8168_eri_write(ioaddr, 0x2E6, 2, 0x9003, ERIAR_ExGMAC);
                break;
        }

        switch (tp->mcfg) {
        case CFG_METHOD_21:
        case CFG_METHOD_22:
        case CFG_METHOD_24:
        case CFG_METHOD_25:
        case CFG_METHOD_26:
        case CFG_METHOD_27:
        case CFG_METHOD_28:
                csi_tmp = rtl8168_eri_read(ioaddr, 0x3FA, 2, ERIAR_ExGMAC);
                csi_tmp |= BIT_14;
                rtl8168_eri_write(ioaddr, 0x3FA, 2, csi_tmp, ERIAR_ExGMAC);
                break;
        }

        switch (tp->mcfg) {
        case CFG_METHOD_26:
        case CFG_METHOD_29:
        case CFG_METHOD_30:
                if (tp->org_pci_offset_99 & BIT_2)
                        RTL_W8(0xB6, RTL_R8(0xB6) | BIT_0);
                break;
        }

        rtl8168_enable_pci_offset_99(tp);
}

static void
rtl8168_disable_pci_offset_180(struct rtl8168_private *tp)
{
        void __iomem *ioaddr = tp->mmio_addr;
        u32 csi_tmp;

        switch (tp->mcfg) {
        case CFG_METHOD_24:
        case CFG_METHOD_25:
        case CFG_METHOD_26:
        case CFG_METHOD_27:
        case CFG_METHOD_28:
        case CFG_METHOD_29:
        case CFG_METHOD_30:
                csi_tmp = rtl8168_eri_read(ioaddr, 0x1E2, 1, ERIAR_ExGMAC);
                csi_tmp &= ~BIT_2;
                rtl8168_eri_write(ioaddr, 0x1E2, 1, csi_tmp, ERIAR_ExGMAC);
                break;
        }

        switch (tp->mcfg) {
        case CFG_METHOD_26:
                rtl8168_eri_write(ioaddr, 0x1E9, 1, 0x0A, ERIAR_ExGMAC);
                break;
        }
}

static void
rtl8168_enable_pci_offset_180(struct rtl8168_private *tp)
{
        void __iomem *ioaddr = tp->mmio_addr;
        u32 csi_tmp;

        switch (tp->mcfg) {
        case CFG_METHOD_25:
        case CFG_METHOD_28:
                csi_tmp = rtl8168_eri_read(ioaddr, 0x1E8, 4, ERIAR_ExGMAC);
                csi_tmp &= ~(0x0000FF00);
                csi_tmp |= (0x00006400);
                rtl8168_eri_write(ioaddr, 0x1E8, 4, csi_tmp, ERIAR_ExGMAC);

                csi_tmp = rtl8168_eri_read(ioaddr, 0x1E4, 4, ERIAR_ExGMAC);
                csi_tmp &= ~(0x0000FF00);
                rtl8168_eri_write(ioaddr, 0x1E4, 4, csi_tmp, ERIAR_ExGMAC);
                break;
        }

        switch (tp->mcfg) {
        case CFG_METHOD_24:
        case CFG_METHOD_25:
        case CFG_METHOD_26:
        case CFG_METHOD_27:
        case CFG_METHOD_28:
        case CFG_METHOD_29:
        case CFG_METHOD_30:
                csi_tmp = rtl8168_eri_read(ioaddr, 0x1E2, 1, ERIAR_ExGMAC);
                csi_tmp |= BIT_2;
                rtl8168_eri_write(ioaddr, 0x1E2, 1, csi_tmp, ERIAR_ExGMAC);
                break;
        }

        switch (tp->mcfg) {
        case CFG_METHOD_26:
                rtl8168_eri_write(ioaddr, 0x1E9, 1, 0x64, ERIAR_ExGMAC);
                break;
        }

        mac_ocp_write(tp, 0xE094, 0x0000);
}

static void
rtl8168_init_pci_offset_180(struct rtl8168_private *tp)
{
        if (tp->org_pci_offset_180 & (BIT_0|BIT_1))
                rtl8168_enable_pci_offset_180(tp);
        else
                rtl8168_disable_pci_offset_180(tp);
}

static void
rtl8168_set_pci_99_180_exit_driver_para(struct net_device *dev)
{
        struct rtl8168_private *tp = netdev_priv(dev);

        switch (tp->mcfg) {
        case CFG_METHOD_21:
        case CFG_METHOD_22:
        case CFG_METHOD_23:
        case CFG_METHOD_24:
        case CFG_METHOD_25:
        case CFG_METHOD_27:
        case CFG_METHOD_28:
        case CFG_METHOD_29:
        case CFG_METHOD_30:
                rtl8168_issue_offset_99_event(tp);
                break;
        }

        switch (tp->mcfg) {
        case CFG_METHOD_21:
        case CFG_METHOD_22:
        case CFG_METHOD_23:
        case CFG_METHOD_24:
        case CFG_METHOD_25:
        case CFG_METHOD_26:
        case CFG_METHOD_27:
        case CFG_METHOD_28:
        case CFG_METHOD_29:
        case CFG_METHOD_30:
                rtl8168_disable_pci_offset_99(tp);
                break;
        }
        switch (tp->mcfg) {
        case CFG_METHOD_24:
        case CFG_METHOD_25:
        case CFG_METHOD_26:
        case CFG_METHOD_27:
        case CFG_METHOD_28:
        case CFG_METHOD_29:
        case CFG_METHOD_30:
                rtl8168_disable_pci_offset_180(tp);
                break;
        }
}

static void
rtl8168_hw_d3_para(struct net_device *dev)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;
        unsigned long flags;

        RTL_W16(RxMaxSize, RX_BUF_SIZE);

        switch (tp->mcfg) {
        case CFG_METHOD_14:
        case CFG_METHOD_15:
        case CFG_METHOD_16:
        case CFG_METHOD_17:
        case CFG_METHOD_18:
        case CFG_METHOD_19:
        case CFG_METHOD_20:
        case CFG_METHOD_21:
        case CFG_METHOD_22:
        case CFG_METHOD_23:
        case CFG_METHOD_24:
        case CFG_METHOD_25:
        case CFG_METHOD_26:
        case CFG_METHOD_27:
        case CFG_METHOD_28:
        case CFG_METHOD_29:
        case CFG_METHOD_30:
                RTL_W8(0xF1, RTL_R8(0xF1) & ~BIT_7);
                RTL_W8(Cfg9346, Cfg9346_Unlock);
                RTL_W8(Config2, RTL_R8(Config2) & ~BIT_7);
                RTL_W8(Config5, RTL_R8(Config5) & ~BIT_0);
                RTL_W8(Cfg9346, Cfg9346_Lock);
                break;
        }

        if (tp->mcfg == CFG_METHOD_21 || tp->mcfg == CFG_METHOD_22 ||
            tp->mcfg == CFG_METHOD_23 || tp->mcfg == CFG_METHOD_24 ||
            tp->mcfg == CFG_METHOD_25 || tp->mcfg == CFG_METHOD_26 ||
            tp->mcfg == CFG_METHOD_27 || tp->mcfg == CFG_METHOD_28) {
                rtl8168_eri_write(ioaddr, 0x2F8, 2, 0x0064, ERIAR_ExGMAC);
        }

        if (tp->bios_setting & BIT_28) {
                if (tp->mcfg == CFG_METHOD_18 || tp->mcfg == CFG_METHOD_19 ||
                    tp->mcfg == CFG_METHOD_20) {
                        u32 gphy_val;

                        spin_lock_irqsave(&tp->phy_lock, flags);
                        mdio_write(tp, 0x1F, 0x0000);
                        mdio_write(tp, 0x04, 0x0061);
                        mdio_write(tp, 0x09, 0x0000);
                        mdio_write(tp, 0x00, 0x9200);
                        mdio_write(tp, 0x1F, 0x0005);
                        mdio_write(tp, 0x05, 0x8B80);
                        gphy_val = mdio_read(tp, 0x06);
                        gphy_val &= ~BIT_7;
                        mdio_write(tp, 0x06, gphy_val);
                        mdelay(1);
                        mdio_write(tp, 0x1F, 0x0007);
                        mdio_write(tp, 0x1E, 0x002C);
                        gphy_val = mdio_read(tp, 0x16);
                        gphy_val &= ~BIT_10;
                        mdio_write(tp, 0x16, gphy_val);
                        mdio_write(tp, 0x1F, 0x0000);
                        spin_unlock_irqrestore(&tp->phy_lock, flags);
                }
        }

        rtl8168_set_pci_99_180_exit_driver_para(dev);

        /*disable ocp phy power saving*/
        if (tp->mcfg == CFG_METHOD_25 || tp->mcfg == CFG_METHOD_26 ||
            tp->mcfg == CFG_METHOD_27 || tp->mcfg == CFG_METHOD_28 ||
            tp->mcfg == CFG_METHOD_29 || tp->mcfg == CFG_METHOD_30) {
                spin_lock_irqsave(&tp->phy_lock, flags);
                mdio_write_phy_ocp(tp, 0x0C41, 0x13, 0x0000);
                mdio_write_phy_ocp(tp, 0x0C41, 0x13, 0x0500);
                spin_unlock_irqrestore(&tp->phy_lock, flags);
        }

        rtl8168_disable_rxdvgate(dev);
}

#define WAKE_ANY (WAKE_PHY | WAKE_MAGIC | WAKE_UCAST | WAKE_BCAST | WAKE_MCAST)

static void
rtl8168_get_hw_wol(struct net_device *dev)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;
        u8 options;
        u32 csi_tmp;
        unsigned long flags;


        spin_lock_irqsave(&tp->lock, flags);

        tp->wol_opts = 0;
        options = RTL_R8(Config1);
        if (!(options & PMEnable))
                goto out_unlock;

        options = RTL_R8(Config3);
        if (options & LinkUp)
                tp->wol_opts |= WAKE_PHY;

        switch (tp->mcfg) {
        case CFG_METHOD_16:
        case CFG_METHOD_17:
        case CFG_METHOD_18:
        case CFG_METHOD_19:
        case CFG_METHOD_20:
        case CFG_METHOD_21:
        case CFG_METHOD_22:
        case CFG_METHOD_24:
        case CFG_METHOD_25:
        case CFG_METHOD_26:
        case CFG_METHOD_27:
        case CFG_METHOD_28:
        case CFG_METHOD_29:
        case CFG_METHOD_30:
                csi_tmp = rtl8168_eri_read(ioaddr, 0xDE, 1, ERIAR_ExGMAC);
                if (csi_tmp & BIT_0)
                        tp->wol_opts |= WAKE_MAGIC;
                break;
        default:
                if (options & MagicPacket)
                        tp->wol_opts |= WAKE_MAGIC;
                break;
        }

        options = RTL_R8(Config5);
        if (options & UWF)
                tp->wol_opts |= WAKE_UCAST;
        if (options & BWF)
                tp->wol_opts |= WAKE_BCAST;
        if (options & MWF)
                tp->wol_opts |= WAKE_MCAST;

out_unlock:
        tp->wol_enabled = (tp->wol_opts) ? WOL_ENABLED : WOL_DISABLED;

        spin_unlock_irqrestore(&tp->lock, flags);
}

static void
rtl8168_set_hw_wol(struct net_device *dev, u32 wolopts)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;
        int i,tmp;
        u32 csi_tmp;
        static struct {
                u32 opt;
                u16 reg;
                u8  mask;
        } cfg[] = {
                { WAKE_PHY,   Config3, LinkUp },
                { WAKE_UCAST, Config5, UWF },
                { WAKE_BCAST, Config5, BWF },
                { WAKE_MCAST, Config5, MWF },
                { WAKE_ANY,   Config5, LanWake },
                { WAKE_MAGIC, Config3, MagicPacket },
        };

        RTL_W8(Cfg9346, Cfg9346_Unlock);

        switch (tp->mcfg) {
        case CFG_METHOD_16:
        case CFG_METHOD_17:
        case CFG_METHOD_18:
        case CFG_METHOD_19:
        case CFG_METHOD_20:
        case CFG_METHOD_21:
        case CFG_METHOD_22:
        case CFG_METHOD_24:
        case CFG_METHOD_25:
        case CFG_METHOD_26:
        case CFG_METHOD_27:
        case CFG_METHOD_28:
        case CFG_METHOD_29:
        case CFG_METHOD_30:
                tmp = ARRAY_SIZE(cfg) - 1;

                csi_tmp = rtl8168_eri_read(ioaddr, 0xDE, 1, ERIAR_ExGMAC);
                if (wolopts & WAKE_MAGIC)
                        csi_tmp |= BIT_0;
                else
                        csi_tmp &= ~BIT_0;
                rtl8168_eri_write(ioaddr, 0xDE, 1, csi_tmp, ERIAR_ExGMAC);
                break;
        default:
                tmp = ARRAY_SIZE(cfg);
                break;
        }

        for (i = 0; i < tmp; i++) {
                u8 options = RTL_R8(cfg[i].reg) & ~cfg[i].mask;
                if (wolopts & cfg[i].opt)
                        options |= cfg[i].mask;
                RTL_W8(cfg[i].reg, options);
        }

        RTL_W8(Cfg9346, Cfg9346_Lock);
}

static void
rtl8168_powerdown_pll(struct net_device *dev)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;

        if (tp->wol_enabled == WOL_ENABLED || tp->DASH || tp->EnableKCPOffload) {
                int auto_nego;
                int giga_ctrl;
                u16 val;
                unsigned long flags;

                rtl8168_set_hw_wol(dev, tp->wol_opts);

                if (tp->mcfg == CFG_METHOD_16 || tp->mcfg == CFG_METHOD_17 ||
                    tp->mcfg == CFG_METHOD_21 || tp->mcfg == CFG_METHOD_22 ||
                    tp->mcfg == CFG_METHOD_24 || tp->mcfg == CFG_METHOD_25 ||
                    tp->mcfg == CFG_METHOD_26 || tp->mcfg == CFG_METHOD_23 ||
                    tp->mcfg == CFG_METHOD_27 || tp->mcfg == CFG_METHOD_28 ||
                    tp->mcfg == CFG_METHOD_29 || tp->mcfg == CFG_METHOD_30) {
                        RTL_W8(Cfg9346, Cfg9346_Unlock);
                        RTL_W8(Config2, RTL_R8(Config2) | PMSTS_En);
                        RTL_W8(Cfg9346, Cfg9346_Lock);
                }

                spin_lock_irqsave(&tp->phy_lock, flags);
                mdio_write(tp, 0x1F, 0x0000);
                auto_nego = mdio_read(tp, MII_ADVERTISE);
                auto_nego &= ~(ADVERTISE_10HALF | ADVERTISE_10FULL
                               | ADVERTISE_100HALF | ADVERTISE_100FULL);

                val = mdio_read(tp, MII_LPA);

#ifdef CONFIG_DOWN_SPEED_100
                auto_nego |= (ADVERTISE_100FULL | ADVERTISE_100HALF | ADVERTISE_10HALF | ADVERTISE_10FULL);
#else
                if (val & (LPA_10HALF | LPA_10FULL))
                        auto_nego |= (ADVERTISE_10HALF | ADVERTISE_10FULL);
                else
                        auto_nego |= (ADVERTISE_100FULL | ADVERTISE_100HALF | ADVERTISE_10HALF | ADVERTISE_10FULL);
#endif

                if (tp->DASH)
                        auto_nego |= (ADVERTISE_100FULL | ADVERTISE_100HALF | ADVERTISE_10HALF | ADVERTISE_10FULL);

                if (((tp->mcfg == CFG_METHOD_7) || (tp->mcfg == CFG_METHOD_8)) && (RTL_R16(CPlusCmd) & ASF))
                        auto_nego |= (ADVERTISE_100FULL | ADVERTISE_100HALF | ADVERTISE_10HALF | ADVERTISE_10FULL);

                giga_ctrl = mdio_read(tp, MII_CTRL1000) & ~(ADVERTISE_1000HALF | ADVERTISE_1000FULL);
                mdio_write(tp, MII_ADVERTISE, auto_nego);
                mdio_write(tp, MII_CTRL1000, giga_ctrl);
                mdio_write(tp, MII_BMCR, BMCR_RESET | BMCR_ANENABLE | BMCR_ANRESTART);
                spin_unlock_irqrestore(&tp->phy_lock, flags);

                RTL_W32(RxConfig, RTL_R32(RxConfig) | AcceptBroadcast | AcceptMulticast | AcceptMyPhys);

                return;
        }

        if (tp->DASH)
                return;

        if (((tp->mcfg == CFG_METHOD_7) || (tp->mcfg == CFG_METHOD_8)) && (RTL_R16(CPlusCmd) & ASF))
                return;

        rtl8168_phy_power_down(dev);

        switch (tp->mcfg) {
        case CFG_METHOD_9:
        case CFG_METHOD_10:
        case CFG_METHOD_11:
        case CFG_METHOD_12:
        case CFG_METHOD_13:
        case CFG_METHOD_14:
        case CFG_METHOD_15:
        case CFG_METHOD_17:
        case CFG_METHOD_18:
        case CFG_METHOD_19:
        case CFG_METHOD_21:
        case CFG_METHOD_22:
        case CFG_METHOD_24:
        case CFG_METHOD_25:
        case CFG_METHOD_26:
        case CFG_METHOD_27:
        case CFG_METHOD_28:
        case CFG_METHOD_29:
        case CFG_METHOD_30:
                RTL_W8(PMCH, RTL_R8(PMCH) & ~BIT_7);
                break;
        }
}

static void rtl8168_powerup_pll(struct net_device *dev)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;


        switch (tp->mcfg) {
        case CFG_METHOD_9:
        case CFG_METHOD_10:
        case CFG_METHOD_11:
        case CFG_METHOD_12:
        case CFG_METHOD_13:
        case CFG_METHOD_14:
        case CFG_METHOD_15:
        case CFG_METHOD_17:
        case CFG_METHOD_18:
        case CFG_METHOD_19:
        case CFG_METHOD_21:
        case CFG_METHOD_22:
        case CFG_METHOD_24:
        case CFG_METHOD_25:
        case CFG_METHOD_26:
        case CFG_METHOD_27:
        case CFG_METHOD_28:
        case CFG_METHOD_29:
        case CFG_METHOD_30:
                RTL_W8(PMCH, RTL_R8(PMCH) | BIT_7 | BIT_6);
                break;
        }

        rtl8168_phy_power_up(dev);
}

static void
rtl8168_get_wol(struct net_device *dev,
                struct ethtool_wolinfo *wol)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;
        u8 options;
        unsigned long flags;

        wol->wolopts = 0;

        if (tp->mcfg == CFG_METHOD_DEFAULT) {
                wol->supported = 0;
                return;
        } else {
                wol->supported = WAKE_ANY;
        }

        spin_lock_irqsave(&tp->lock, flags);

        options = RTL_R8(Config1);
        if (!(options & PMEnable))
                goto out_unlock;

        wol->wolopts = tp->wol_opts;

out_unlock:
        spin_unlock_irqrestore(&tp->lock, flags);
}

static int
rtl8168_set_wol(struct net_device *dev,
                struct ethtool_wolinfo *wol)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        unsigned long flags;

        if (tp->mcfg == CFG_METHOD_DEFAULT)
                return -EOPNOTSUPP;

        spin_lock_irqsave(&tp->lock, flags);

        tp->wol_opts = wol->wolopts;

        tp->wol_enabled = (tp->wol_opts) ? WOL_ENABLED : WOL_DISABLED;

        spin_unlock_irqrestore(&tp->lock, flags);

        return 0;
}

static void
rtl8168_get_drvinfo(struct net_device *dev,
                    struct ethtool_drvinfo *info)
{
        struct rtl8168_private *tp = netdev_priv(dev);

        strcpy(info->driver, MODULENAME);
        strcpy(info->version, RTL8168_VERSION);
        strcpy(info->bus_info, pci_name(tp->pci_dev));
        info->regdump_len = R8168_REGS_DUMP_SIZE;
        info->eedump_len = tp->eeprom_len;
}

static int
rtl8168_get_regs_len(struct net_device *dev)
{
        return R8168_REGS_DUMP_SIZE;
}

static int
rtl8168_set_speed_xmii(struct net_device *dev,
                       u8 autoneg,
                       u16 speed,
                       u8 duplex)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        int auto_nego = 0;
        int giga_ctrl = 0;
        int bmcr_true_force = 0;
        unsigned long flags;

        if (tp->mcfg == CFG_METHOD_29 || tp->mcfg == CFG_METHOD_30) {
                //Disable Giga Lite
                spin_lock_irqsave(&tp->phy_lock, flags);
                mdio_write(tp, 0x1F, 0x0A42);
                ClearEthPhyBit(tp, 0x14, BIT_9);
                mdio_write(tp, 0x1F, 0x0A40);
                mdio_write(tp, 0x1F, 0x0000);
                spin_unlock_irqrestore(&tp->phy_lock, flags);
        }

        if ((speed != SPEED_1000) &&
            (speed != SPEED_100) &&
            (speed != SPEED_10)) {
                speed = SPEED_1000;
                duplex = DUPLEX_FULL;
        }

        auto_nego = mdio_read(tp, MII_ADVERTISE);
        auto_nego &= ~(ADVERTISE_10HALF | ADVERTISE_10FULL | ADVERTISE_100HALF | ADVERTISE_100FULL | ADVERTISE_PAUSE_CAP | ADVERTISE_PAUSE_ASYM);

        giga_ctrl = mdio_read(tp, MII_CTRL1000);
        giga_ctrl &= ~(ADVERTISE_1000HALF | ADVERTISE_1000FULL);

        if ((autoneg == AUTONEG_ENABLE) || (speed == SPEED_1000)) {
                /*n-way force*/
                if ((speed == SPEED_10) && (duplex == DUPLEX_HALF)) {
                        auto_nego |= ADVERTISE_10HALF;
                } else if ((speed == SPEED_10) && (duplex == DUPLEX_FULL)) {
                        auto_nego |= ADVERTISE_10HALF |
                                     ADVERTISE_10FULL;
                } else if ((speed == SPEED_100) && (duplex == DUPLEX_HALF)) {
                        auto_nego |= ADVERTISE_100HALF |
                                     ADVERTISE_10HALF |
                                     ADVERTISE_10FULL;
                } else if ((speed == SPEED_100) && (duplex == DUPLEX_FULL)) {
                        auto_nego |= ADVERTISE_100HALF |
                                     ADVERTISE_100FULL |
                                     ADVERTISE_10HALF |
                                     ADVERTISE_10FULL;
                } else if (speed == SPEED_1000) {
                        giga_ctrl |= ADVERTISE_1000HALF |
                                     ADVERTISE_1000FULL;

                        auto_nego |= ADVERTISE_100HALF |
                                     ADVERTISE_100FULL |
                                     ADVERTISE_10HALF |
                                     ADVERTISE_10FULL;
                }

                //flow control
                if (dev->mtu <= ETH_DATA_LEN)
                        auto_nego |= ADVERTISE_PAUSE_CAP|ADVERTISE_PAUSE_ASYM;

                tp->phy_auto_nego_reg = auto_nego;
                tp->phy_1000_ctrl_reg = giga_ctrl;

                spin_lock_irqsave(&tp->phy_lock, flags);
                mdio_write(tp, 0x1f, 0x0000);
                mdio_write(tp, MII_ADVERTISE, auto_nego);
                mdio_write(tp, MII_CTRL1000, giga_ctrl);
                mdio_write(tp, MII_BMCR, BMCR_RESET | BMCR_ANENABLE | BMCR_ANRESTART);
                spin_unlock_irqrestore(&tp->phy_lock, flags);
                mdelay(20);
        } else {
                /*true force*/
#ifndef BMCR_SPEED100
#define BMCR_SPEED100   0x0040
#endif

#ifndef BMCR_SPEED10
#define BMCR_SPEED10    0x0000
#endif
                if ((speed == SPEED_10) && (duplex == DUPLEX_HALF)) {
                        bmcr_true_force = BMCR_SPEED10;
                } else if ((speed == SPEED_10) && (duplex == DUPLEX_FULL)) {
                        bmcr_true_force = BMCR_SPEED10 | BMCR_FULLDPLX;
                } else if ((speed == SPEED_100) && (duplex == DUPLEX_HALF)) {
                        bmcr_true_force = BMCR_SPEED100;
                } else if ((speed == SPEED_100) && (duplex == DUPLEX_FULL)) {
                        bmcr_true_force = BMCR_SPEED100 | BMCR_FULLDPLX;
                }

                spin_lock_irqsave(&tp->phy_lock, flags);
                mdio_write(tp, 0x1f, 0x0000);
                mdio_write(tp, MII_BMCR, bmcr_true_force);
                spin_unlock_irqrestore(&tp->phy_lock, flags);
        }

        tp->autoneg = autoneg;
        tp->speed = speed;
        tp->duplex = duplex;

        if (tp->mcfg == CFG_METHOD_11)
                rtl8168dp_10mbps_gphy_para(dev);

        return 0;
}

static int
rtl8168_set_speed(struct net_device *dev,
                  u8 autoneg,
                  u16 speed,
                  u8 duplex)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        int ret;

        ret = tp->set_speed(dev, autoneg, speed, duplex);

        return ret;
}

static int
rtl8168_set_settings(struct net_device *dev,
                     struct ethtool_cmd *cmd)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        int ret;
        unsigned long flags;

        spin_lock_irqsave(&tp->lock, flags);
        ret = rtl8168_set_speed(dev, cmd->autoneg, cmd->speed, cmd->duplex);
        spin_unlock_irqrestore(&tp->lock, flags);

        return ret;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,3,0)
static u32
rtl8168_get_tx_csum(struct net_device *dev)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        u32 ret;
        unsigned long flags;

        spin_lock_irqsave(&tp->lock, flags);
        ret = ((dev->features & NETIF_F_IP_CSUM) != 0);
        spin_unlock_irqrestore(&tp->lock, flags);

        return ret;
}

static u32
rtl8168_get_rx_csum(struct net_device *dev)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        u32 ret;
        unsigned long flags;

        spin_lock_irqsave(&tp->lock, flags);
        ret = tp->cp_cmd & RxChkSum;
        spin_unlock_irqrestore(&tp->lock, flags);

        return ret;
}

static int
rtl8168_set_tx_csum(struct net_device *dev,
                    u32 data)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        unsigned long flags;

        if (tp->mcfg == CFG_METHOD_DEFAULT)
                return -EOPNOTSUPP;

        spin_lock_irqsave(&tp->lock, flags);

        if (data)
                dev->features |= NETIF_F_IP_CSUM;
        else
                dev->features &= ~NETIF_F_IP_CSUM;

        spin_unlock_irqrestore(&tp->lock, flags);

        return 0;
}

static int
rtl8168_set_rx_csum(struct net_device *dev,
                    u32 data)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;
        unsigned long flags;

        if (tp->mcfg == CFG_METHOD_DEFAULT)
                return -EOPNOTSUPP;

        spin_lock_irqsave(&tp->lock, flags);

        if (data)
                tp->cp_cmd |= RxChkSum;
        else
                tp->cp_cmd &= ~RxChkSum;

        RTL_W16(CPlusCmd, tp->cp_cmd);

        spin_unlock_irqrestore(&tp->lock, flags);

        return 0;
}
#endif

#ifdef CONFIG_R8168_VLAN

static inline u32
rtl8168_tx_vlan_tag(struct rtl8168_private *tp,
                    struct sk_buff *skb)
{
        u32 tag;

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,0,0)
        tag = (tp->vlgrp && vlan_tx_tag_present(skb)) ?
              TxVlanTag | swab16(vlan_tx_tag_get(skb)) : 0x00;
#elif LINUX_VERSION_CODE < KERNEL_VERSION(4,0,0)
        tag = (vlan_tx_tag_present(skb)) ?
              TxVlanTag | swab16(vlan_tx_tag_get(skb)) : 0x00;
#else
        tag = (skb_vlan_tag_present(skb)) ?
              TxVlanTag | swab16(skb_vlan_tag_get(skb)) : 0x00;
#endif

        return tag;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,0,0)

static void
rtl8168_vlan_rx_register(struct net_device *dev,
                         struct vlan_group *grp)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;
        unsigned long flags;

        spin_lock_irqsave(&tp->lock, flags);
        tp->vlgrp = grp;
        if (tp->vlgrp)
                tp->cp_cmd |= RxVlan;
        else
                tp->cp_cmd &= ~RxVlan;
        RTL_W16(CPlusCmd, tp->cp_cmd);
        RTL_R16(CPlusCmd);
        spin_unlock_irqrestore(&tp->lock, flags);
}

#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22)
static void
rtl8168_vlan_rx_kill_vid(struct net_device *dev,
                         unsigned short vid)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        unsigned long flags;

        spin_lock_irqsave(&tp->lock, flags);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,21)
        if (tp->vlgrp)
                tp->vlgrp->vlan_devices[vid] = NULL;
#else
        vlan_group_set_device(tp->vlgrp, vid, NULL);
#endif //LINUX_VERSION_CODE < KERNEL_VERSION(2,6,21)
        spin_unlock_irqrestore(&tp->lock, flags);
}
#endif //LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22)

static int
rtl8168_rx_vlan_skb(struct rtl8168_private *tp,
                    struct RxDesc *desc,
                    struct sk_buff *skb)
{
        u32 opts2 = le32_to_cpu(desc->opts2);
        int ret = -1;

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,0,0)
        if (tp->vlgrp && (opts2 & RxVlanTag)) {
                rtl8168_rx_hwaccel_skb(skb, tp->vlgrp,
                                       swab16(opts2 & 0xffff));
                ret = 0;
        }
#elif LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
        if (opts2 & RxVlanTag)
                __vlan_hwaccel_put_tag(skb, swab16(opts2 & 0xffff));
#else
        if (opts2 & RxVlanTag)
                __vlan_hwaccel_put_tag(skb, htons(ETH_P_8021Q), swab16(opts2 & 0xffff));
#endif

        desc->opts2 = 0;
        return ret;
}

#else /* !CONFIG_R8168_VLAN */

static inline u32
rtl8168_tx_vlan_tag(struct rtl8168_private *tp,
                    struct sk_buff *skb)
{
        return 0;
}

static int
rtl8168_rx_vlan_skb(struct rtl8168_private *tp,
                    struct RxDesc *desc,
                    struct sk_buff *skb)
{
        return -1;
}

#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,0,0)

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,3,0)
static u32 rtl8168_fix_features(struct net_device *dev, u32 features)
#else
static netdev_features_t rtl8168_fix_features(struct net_device *dev,
                netdev_features_t features)
#endif
{
        struct rtl8168_private *tp = netdev_priv(dev);
        unsigned long flags;

        spin_lock_irqsave(&tp->lock, flags);
        if (dev->mtu > ETH_DATA_LEN) {
                features &= ~NETIF_F_ALL_TSO;
                features &= ~NETIF_F_ALL_CSUM;
        }
        spin_unlock_irqrestore(&tp->lock, flags);

        return features;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,3,0)
static int rtl8168_hw_set_features(struct net_device *dev, u32 features)
#else
static int rtl8168_hw_set_features(struct net_device *dev,
                                   netdev_features_t features)
#endif
{
        struct rtl8168_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;

        if (features & NETIF_F_RXCSUM)
                tp->cp_cmd |= RxChkSum;
        else
                tp->cp_cmd &= ~RxChkSum;

        if (dev->features & NETIF_F_HW_VLAN_RX)
                tp->cp_cmd |= RxVlan;
        else
                tp->cp_cmd &= ~RxVlan;

        RTL_W16(CPlusCmd, tp->cp_cmd);
        RTL_R16(CPlusCmd);

        return 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,3,0)
static int rtl8168_set_features(struct net_device *dev, u32 features)
#else
static int rtl8168_set_features(struct net_device *dev,
                                netdev_features_t features)
#endif
{
        struct rtl8168_private *tp = netdev_priv(dev);
        unsigned long flags;

        spin_lock_irqsave(&tp->lock, flags);

        rtl8168_hw_set_features(dev, features);

        spin_unlock_irqrestore(&tp->lock, flags);

        return 0;
}

#endif

static void rtl8168_gset_xmii(struct net_device *dev,
                              struct ethtool_cmd *cmd)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;
        u8 status;
        unsigned long flags;

        cmd->supported = SUPPORTED_10baseT_Half |
                         SUPPORTED_10baseT_Full |
                         SUPPORTED_100baseT_Half |
                         SUPPORTED_100baseT_Full |
                         SUPPORTED_1000baseT_Full |
                         SUPPORTED_Autoneg |
                         SUPPORTED_TP;

        spin_lock_irqsave(&tp->phy_lock, flags);
        mdio_write(tp, 0x1F, 0x0000);
        cmd->autoneg = (mdio_read(tp, MII_BMCR) & BMCR_ANENABLE) ? 1 : 0;
        spin_unlock_irqrestore(&tp->phy_lock, flags);
        cmd->advertising = ADVERTISED_TP | ADVERTISED_Autoneg;

        if (tp->phy_auto_nego_reg & ADVERTISE_10HALF)
                cmd->advertising |= ADVERTISED_10baseT_Half;
        if (tp->phy_auto_nego_reg & ADVERTISE_10FULL)
                cmd->advertising |= ADVERTISED_10baseT_Full;
        if (tp->phy_auto_nego_reg & ADVERTISE_100HALF)
                cmd->advertising |= ADVERTISED_100baseT_Half;
        if (tp->phy_auto_nego_reg & ADVERTISE_100FULL)
                cmd->advertising |= ADVERTISED_100baseT_Full;
        if (tp->phy_1000_ctrl_reg & ADVERTISE_1000FULL)
                cmd->advertising |= ADVERTISED_1000baseT_Full;

        status = RTL_R8(PHYstatus);

        if (status & _1000bpsF)
                cmd->speed = SPEED_1000;
        else if (status & _100bps)
                cmd->speed = SPEED_100;
        else if (status & _10bps)
                cmd->speed = SPEED_10;

        if (status & TxFlowCtrl)
                cmd->advertising |= ADVERTISED_Asym_Pause;

        if (status & RxFlowCtrl)
                cmd->advertising |= ADVERTISED_Pause;

        cmd->duplex = ((status & _1000bpsF) || (status & FullDup)) ?
                      DUPLEX_FULL : DUPLEX_HALF;


}

static int
rtl8168_get_settings(struct net_device *dev,
                     struct ethtool_cmd *cmd)
{
        struct rtl8168_private *tp = netdev_priv(dev);

        tp->get_settings(dev, cmd);

        return 0;
}

static void rtl8168_get_regs(struct net_device *dev, struct ethtool_regs *regs,
                             void *p)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;
        unsigned int i;
        u8 *data = p;
        unsigned long flags;

        if (regs->len < R8168_REGS_DUMP_SIZE)
                return /* -EINVAL */;

        memset(p, 0, regs->len);

        spin_lock_irqsave(&tp->lock, flags);
        for (i = 0; i < R8168_MAC_REGS_SIZE; i++)
                *data++ = readb(ioaddr + i);
        spin_unlock_irqrestore(&tp->lock, flags);
        data = (u8*)p + 256;

        spin_lock_irqsave(&tp->phy_lock, flags);
        mdio_write(tp, 0x1F, 0x0000);
        for (i = 0; i < R8168_PHY_REGS_SIZE/2; i++) {
                *(u16*)data = mdio_read(tp, i);
                data += 2;
        }
        data = (u8*)p + 256 * 2;

        for (i = 0; i < R8168_EPHY_REGS_SIZE/2; i++) {
                *(u16*)data = rtl8168_ephy_read(ioaddr, i);
                data += 2;
        }
        data = (u8*)p + 256 * 3;

        switch (tp->mcfg) {
        case CFG_METHOD_1:
        case CFG_METHOD_2:
        case CFG_METHOD_3:
                /* RTL8168B does not support Extend GMAC */
                break;
        default:
                for (i = 0; i < R8168_ERI_REGS_SIZE; i+=4) {
                        *(u32*)data = rtl8168_eri_read(ioaddr, i , 4, ERIAR_ExGMAC);
                        data += 4;
                }
                break;
        }
        spin_unlock_irqrestore(&tp->phy_lock, flags);
}

static u32
rtl8168_get_msglevel(struct net_device *dev)
{
        struct rtl8168_private *tp = netdev_priv(dev);

        return tp->msg_enable;
}

static void
rtl8168_set_msglevel(struct net_device *dev,
                     u32 value)
{
        struct rtl8168_private *tp = netdev_priv(dev);

        tp->msg_enable = value;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,33)
static int rtl8168_get_stats_count(struct net_device *dev)
{
        return ARRAY_SIZE(rtl8168_gstrings);
}
#else
static int rtl8168_get_sset_count(struct net_device *dev, int sset)
{
        switch (sset) {
        case ETH_SS_STATS:
                return ARRAY_SIZE(rtl8168_gstrings);
        default:
                return -EOPNOTSUPP;
        }
}
#endif
static void
rtl8168_get_ethtool_stats(struct net_device *dev,
                          struct ethtool_stats *stats,
                          u64 *data)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;
        struct rtl8168_counters *counters;
        dma_addr_t paddr;
        u32 cmd;
        u32 WaitCnt;
        unsigned long flags;

        ASSERT_RTNL();

        counters = tp->tally_vaddr;
        paddr = tp->tally_paddr;
        if (!counters)
                return;

        spin_lock_irqsave(&tp->lock, flags);
        RTL_W32(CounterAddrHigh, (u64)paddr >> 32);
        cmd = (u64)paddr & DMA_BIT_MASK(32);
        RTL_W32(CounterAddrLow, cmd);
        RTL_W32(CounterAddrLow, cmd | CounterDump);

        WaitCnt = 0;
        while (RTL_R32(CounterAddrLow) & CounterDump) {
                udelay(10);

                WaitCnt++;
                if (WaitCnt > 20)
                        break;
        }
        spin_unlock_irqrestore(&tp->lock, flags);

        data[0] = le64_to_cpu(counters->tx_packets);
        data[1] = le64_to_cpu(counters->rx_packets);
        data[2] = le64_to_cpu(counters->tx_errors);
        data[3] = le32_to_cpu(counters->rx_errors);
        data[4] = le16_to_cpu(counters->rx_missed);
        data[5] = le16_to_cpu(counters->align_errors);
        data[6] = le32_to_cpu(counters->tx_one_collision);
        data[7] = le32_to_cpu(counters->tx_multi_collision);
        data[8] = le64_to_cpu(counters->rx_unicast);
        data[9] = le64_to_cpu(counters->rx_broadcast);
        data[10] = le32_to_cpu(counters->rx_multicast);
        data[11] = le16_to_cpu(counters->tx_aborted);
        data[12] = le16_to_cpu(counters->tx_underun);
}

static void
rtl8168_get_strings(struct net_device *dev,
                    u32 stringset,
                    u8 *data)
{
        switch (stringset) {
        case ETH_SS_STATS:
                memcpy(data, *rtl8168_gstrings, sizeof(rtl8168_gstrings));
                break;
        }
}
static int rtl_get_eeprom_len(struct net_device *dev)
{
        struct rtl8168_private *tp = netdev_priv(dev);

        return tp->eeprom_len;
}

static int rtl_get_eeprom(struct net_device *dev, struct ethtool_eeprom *eeprom, u8 *buf)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        int i,j,ret;
        int start_w, end_w;
        int VPD_addr, VPD_data;
        u32 *eeprom_buff;
        u16 tmp;
        void __iomem *ioaddr = tp->mmio_addr;

        if (tp->eeprom_type == EEPROM_TYPE_NONE) {
                dev_printk(KERN_DEBUG, &tp->pci_dev->dev, "Detect none EEPROM\n");
                return -EOPNOTSUPP;
        } else if (eeprom->len == 0 || (eeprom->offset+eeprom->len) > tp->eeprom_len) {
                dev_printk(KERN_DEBUG, &tp->pci_dev->dev, "Invalid parameter\n");
                return -EINVAL;
        }

        switch (tp->mcfg) {
        case CFG_METHOD_9:
        case CFG_METHOD_10:
                VPD_addr = 0xCE;
                VPD_data = 0xD0;
                break;

        case CFG_METHOD_1:
        case CFG_METHOD_2:
        case CFG_METHOD_3:
        case CFG_METHOD_11:
        case CFG_METHOD_12:
        case CFG_METHOD_13:
                return -EOPNOTSUPP;
        default:
                VPD_addr = 0xD2;
                VPD_data = 0xD4;
                break;
        }

        start_w = eeprom->offset >> 2;
        end_w = (eeprom->offset + eeprom->len - 1) >> 2;

        eeprom_buff = kmalloc(sizeof(u32)*(end_w - start_w + 1), GFP_KERNEL);
        if (!eeprom_buff)
                return -ENOMEM;

        RTL_W8(Cfg9346, Cfg9346_Unlock);
        ret = -EFAULT;
        for (i=start_w; i<=end_w; i++) {
                pci_write_config_word(tp->pci_dev, VPD_addr, (u16)i*4);
                ret = -EFAULT;
                for (j = 0; j < 10; j++) {
                        udelay(400);
                        pci_read_config_word(tp->pci_dev, VPD_addr, &tmp);
                        if (tmp&0x8000) {
                                ret = 0;
                                break;
                        }
                }

                if (ret)
                        break;

                pci_read_config_dword(tp->pci_dev, VPD_data, &eeprom_buff[i-start_w]);
        }
        RTL_W8(Cfg9346, Cfg9346_Lock);

        if (!ret)
                memcpy(buf, (u8 *)eeprom_buff + (eeprom->offset & 3), eeprom->len);

        kfree(eeprom_buff);

        return ret;
}

#undef ethtool_op_get_link
#define ethtool_op_get_link _kc_ethtool_op_get_link
u32 _kc_ethtool_op_get_link(struct net_device *dev)
{
        return netif_carrier_ok(dev) ? 1 : 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,3,0)
#undef ethtool_op_get_sg
#define ethtool_op_get_sg _kc_ethtool_op_get_sg
u32 _kc_ethtool_op_get_sg(struct net_device *dev)
{
#ifdef NETIF_F_SG
        return (dev->features & NETIF_F_SG) != 0;
#else
        return 0;
#endif
}

#undef ethtool_op_set_sg
#define ethtool_op_set_sg _kc_ethtool_op_set_sg
int _kc_ethtool_op_set_sg(struct net_device *dev, u32 data)
{
        struct rtl8168_private *tp = netdev_priv(dev);

        if (tp->mcfg == CFG_METHOD_DEFAULT)
                return -EOPNOTSUPP;

#ifdef NETIF_F_SG
        if (data)
                dev->features |= NETIF_F_SG;
        else
                dev->features &= ~NETIF_F_SG;
#endif

        return 0;
}
#endif

static const struct ethtool_ops rtl8168_ethtool_ops = {
        .get_drvinfo        = rtl8168_get_drvinfo,
        .get_regs_len       = rtl8168_get_regs_len,
        .get_link       = ethtool_op_get_link,
        .get_settings       = rtl8168_get_settings,
        .set_settings       = rtl8168_set_settings,
        .get_msglevel       = rtl8168_get_msglevel,
        .set_msglevel       = rtl8168_set_msglevel,
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,3,0)
        .get_rx_csum        = rtl8168_get_rx_csum,
        .set_rx_csum        = rtl8168_set_rx_csum,
        .get_tx_csum        = rtl8168_get_tx_csum,
        .set_tx_csum        = rtl8168_set_tx_csum,
        .get_sg         = ethtool_op_get_sg,
        .set_sg         = ethtool_op_set_sg,
#ifdef NETIF_F_TSO
        .get_tso        = ethtool_op_get_tso,
        .set_tso        = ethtool_op_set_tso,
#endif
#endif
        .get_regs       = rtl8168_get_regs,
        .get_wol        = rtl8168_get_wol,
        .set_wol        = rtl8168_set_wol,
        .get_strings        = rtl8168_get_strings,
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,33)
        .get_stats_count    = rtl8168_get_stats_count,
#else
        .get_sset_count     = rtl8168_get_sset_count,
#endif
        .get_ethtool_stats  = rtl8168_get_ethtool_stats,
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,23)
#ifdef ETHTOOL_GPERMADDR
        .get_perm_addr      = ethtool_op_get_perm_addr,
#endif
#endif //LINUX_VERSION_CODE < KERNEL_VERSION(2,6,23)
        .get_eeprom     = rtl_get_eeprom,
        .get_eeprom_len     = rtl_get_eeprom_len,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,5,0)
        .get_ts_info        = ethtool_op_get_ts_info,
#endif //LINUX_VERSION_CODE >= KERNEL_VERSION(3,5,0)
};


static int rtl8168_enable_EEE(struct rtl8168_private *tp)
{
        void __iomem *ioaddr = tp->mmio_addr;
        int ret;
        u16 data;
        u16 PhyRegValue;
        u32 WaitCnt;
        unsigned long flags;

        ret = 0;
        switch (tp->mcfg) {
        case CFG_METHOD_14:
        case CFG_METHOD_15:
                spin_lock_irqsave(&tp->phy_lock, flags);
                mdio_write(tp, 0x1F, 0x0007);
                mdio_write(tp, 0x1E, 0x0020);
                data = mdio_read(tp, 0x15) | 0x0100;
                mdio_write(tp, 0x15, data);
                mdio_write(tp, 0x1F, 0x0005);
                mdio_write(tp, 0x05, 0x8B85);
                data = mdio_read(tp, 0x06) | 0x2000;
                mdio_write(tp, 0x06, data);
                mdio_write(tp, 0x1F, 0x0006);
                mdio_write(tp, 0x00, 0x5A30);
                mdio_write(tp, 0x1F, 0x0000);
                mdio_write(tp, 0x0D, 0x0007);
                mdio_write(tp, 0x0E, 0x003C);
                mdio_write(tp, 0x0D, 0x4007);
                mdio_write(tp, 0x0E, 0x0006);
                mdio_write(tp, 0x0D, 0x0000);
                spin_unlock_irqrestore(&tp->phy_lock, flags);
                if ((RTL_R8(Config4)&0x40) && (RTL_R8(0x6D) & BIT_7)) {
                        spin_lock_irqsave(&tp->phy_lock, flags);
                        mdio_write(tp, 0x1F, 0x0005);
                        mdio_write(tp, 0x05, 0x8AC8);
                        mdio_write(tp, 0x06, RTL_R16(CustomLED));
                        mdio_write(tp, 0x05, 0x8B82);
                        data = mdio_read(tp, 0x06) | 0x0010;
                        mdio_write(tp, 0x05, 0x8B82);
                        mdio_write(tp, 0x06, data);
                        mdio_write(tp, 0x1F, 0x0000);
                        spin_unlock_irqrestore(&tp->phy_lock, flags);
                }
                break;

        case CFG_METHOD_16:
        case CFG_METHOD_17:
                spin_lock_irqsave(&tp->phy_lock, flags);
                data = rtl8168_eri_read(ioaddr,0x1B0 ,4,ERIAR_ExGMAC) | 0x0003;
                rtl8168_eri_write(ioaddr, 0x1B0, 4, data, ERIAR_ExGMAC);
                mdio_write(tp,0x1F , 0x0004);
                mdio_write(tp,0x1F , 0x0007);
                mdio_write(tp,0x1E , 0x0020);
                data = mdio_read(tp, 0x15)|0x0100;
                mdio_write(tp,0x15 , data);
                mdio_write(tp,0x1F , 0x0002);
                mdio_write(tp,0x1F , 0x0005);
                mdio_write(tp,0x05 , 0x8B85);
                data = mdio_read(tp, 0x06)|0x2000;
                mdio_write(tp,0x06 , data);
                mdio_write(tp,0x1F , 0x0000);
                mdio_write(tp,0x0D , 0x0007);
                mdio_write(tp,0x0E , 0x003C);
                mdio_write(tp,0x0D , 0x4007);
                mdio_write(tp,0x0E , 0x0006);
                mdio_write(tp,0x1D , 0x0000);
                spin_unlock_irqrestore(&tp->phy_lock, flags);
                break;

        case CFG_METHOD_18:
        case CFG_METHOD_19:
        case CFG_METHOD_20:
                spin_lock_irqsave(&tp->phy_lock, flags);
                data = rtl8168_eri_read(ioaddr,0x1B0 ,4,ERIAR_ExGMAC);
                data |= BIT_1 | BIT_0;
                rtl8168_eri_write(ioaddr, 0x1B0, 4, data, ERIAR_ExGMAC);
                mdio_write(tp, 0x1F, 0x0007);
                mdio_write(tp, 0x1e, 0x0020);
                data = mdio_read(tp, 0x15);
                data |= BIT_8;
                mdio_write(tp, 0x15, data);
                mdio_write(tp, 0x1F, 0x0005);
                mdio_write(tp, 0x05, 0x8B85);
                data = mdio_read(tp, 0x06);
                data |= BIT_13;
                mdio_write(tp, 0x06, data);
                mdio_write(tp, 0x1F, 0x0000);
                mdio_write(tp, 0x0D, 0x0007);
                mdio_write(tp, 0x0E, 0x003C);
                mdio_write(tp, 0x0D, 0x4007);
                mdio_write(tp, 0x0E, 0x0006);
                mdio_write(tp, 0x0D, 0x0000);
                spin_unlock_irqrestore(&tp->phy_lock, flags);
                break;

        case CFG_METHOD_21:
        case CFG_METHOD_22:
        case CFG_METHOD_23:
        case CFG_METHOD_24:
        case CFG_METHOD_25:
        case CFG_METHOD_26:
        case CFG_METHOD_27:
        case CFG_METHOD_28:
        case CFG_METHOD_29:
        case CFG_METHOD_30:
                spin_lock_irqsave(&tp->phy_lock, flags);
                data = rtl8168_eri_read(ioaddr, 0x1B0, 4, ERIAR_ExGMAC);
                data |= BIT_1 | BIT_0;
                rtl8168_eri_write(ioaddr, 0x1B0, 4, data, ERIAR_ExGMAC);
                mdio_write(tp, 0x1F, 0x0A43);
                data = mdio_read(tp, 0x11);
                mdio_write(tp, 0x11, data | BIT_4);
                mdio_write(tp, 0x1F, 0x0A5D);
                mdio_write(tp, 0x10, 0x0006);
                mdio_write(tp, 0x1F, 0x0000);
                spin_unlock_irqrestore(&tp->phy_lock, flags);
                break;

        default:
//      dev_printk(KERN_DEBUG, &tp->pci_dev->dev, "Not Support EEE\n");
                ret = -EOPNOTSUPP;
                break;
        }

        switch (tp->mcfg) {
        case CFG_METHOD_29:
        case CFG_METHOD_30:
                spin_lock_irqsave(&tp->phy_lock, flags);
                mdio_write(tp, 0x1F, 0x0A4A);
                SetEthPhyBit(tp, 0x11, BIT_9);
                mdio_write(tp, 0x1F, 0x0A42);
                SetEthPhyBit(tp, 0x14, BIT_7);
                mdio_write(tp, 0x1F, 0x0000);
                spin_unlock_irqrestore(&tp->phy_lock, flags);
                break;
        }

        /*Advanced EEE*/
        switch (tp->mcfg) {
        case CFG_METHOD_24:
        case CFG_METHOD_25:
        case CFG_METHOD_26:
        case CFG_METHOD_29:
        case CFG_METHOD_30:
                spin_lock_irqsave(&tp->phy_lock, flags);
                mdio_write(tp,0x1F, 0x0B82);
                SetEthPhyBit(tp, 0x10, BIT_4);
                mdio_write(tp, 0x1F, 0x0000);

                mdio_write(tp,0x1F, 0x0B80);
                WaitCnt = 0;
                do {
                        PhyRegValue = mdio_read(tp, 0x10);
                        PhyRegValue &= 0x0040;
                        udelay(100);
                        WaitCnt++;
                } while(PhyRegValue != 0x0040 && WaitCnt <1000);

                mdio_write(tp, 0x1F, 0x0000);
                spin_unlock_irqrestore(&tp->phy_lock, flags);
                break;
        }

        switch (tp->mcfg) {
        case CFG_METHOD_25:
                rtl8168_eri_write(ioaddr, 0x1EA, 1, 0xFA, ERIAR_ExGMAC);

                spin_lock_irqsave(&tp->phy_lock, flags);
                mdio_write(tp, 0x1F, 0x0A43);
                data = mdio_read(tp, 0x10);
                if (data & BIT_10) {
                        mdio_write(tp, 0x1F, 0x0A42);
                        data = mdio_read(tp, 0x16);
                        data &= ~(BIT_1);
                        mdio_write(tp, 0x16, data);
                } else {
                        mdio_write(tp, 0x1F, 0x0A42);
                        data = mdio_read(tp, 0x16);
                        data |= BIT_1;
                        mdio_write(tp, 0x16, data);
                }
                mdio_write(tp, 0x1F, 0x0000);
                spin_unlock_irqrestore(&tp->phy_lock, flags);
                break;
        case CFG_METHOD_26:
                data = mac_ocp_read(tp, 0xE052);
                data |= BIT_0;
                mac_ocp_write(tp, 0xE052, data);
                data = mac_ocp_read(tp, 0xE056);
                data &= 0xFF0F;
                data |= (BIT_4 | BIT_5 | BIT_6);
                mac_ocp_write(tp, 0xE056, data);

                spin_lock_irqsave(&tp->phy_lock, flags);
                mdio_write(tp, 0x1F, 0x0A43);
                data = mdio_read(tp, 0x10);
                if (data & BIT_10) {
                        mdio_write(tp, 0x1F, 0x0A42);
                        data = mdio_read(tp, 0x16);
                        data &= ~(BIT_1);
                        mdio_write(tp, 0x16, data);
                } else {
                        mdio_write(tp, 0x1F, 0x0A42);
                        data = mdio_read(tp, 0x16);
                        data |= BIT_1;
                        mdio_write(tp, 0x16, data);
                }
                mdio_write(tp, 0x1F, 0x0000);
                spin_unlock_irqrestore(&tp->phy_lock, flags);
                break;
        case CFG_METHOD_27:
        case CFG_METHOD_28:
                OOB_mutex_lock(tp);
                data = mac_ocp_read(tp, 0xE052);
                data &= ~BIT_0;
                mac_ocp_write(tp, 0xE052, data);
                OOB_mutex_unlock(tp);
                data = mac_ocp_read(tp, 0xE056);
                data &= 0xFF0F;
                data |= (BIT_4 | BIT_5 | BIT_6);
                mac_ocp_write(tp, 0xE056, data);
                break;
        case CFG_METHOD_29:
        case CFG_METHOD_30:
                data = mac_ocp_read(tp, 0xE052);
                data |= BIT_0;
                mac_ocp_write(tp, 0xE052, data);

                spin_lock_irqsave(&tp->phy_lock, flags);
                mdio_write(tp, 0x1F, 0x0A43);
                data = mdio_read(tp, 0x10) | BIT_15;
                mdio_write(tp, 0x10, data);

                mdio_write(tp, 0x1F, 0x0A44);
                data = mdio_read(tp, 0x11) | BIT_13 | BIT_14;
                data &= ~(BIT_12);
                mdio_write(tp, 0x11, data);
                spin_unlock_irqrestore(&tp->phy_lock, flags);
                break;
        }

        switch (tp->mcfg) {
        case CFG_METHOD_24:
        case CFG_METHOD_25:
        case CFG_METHOD_26:
        case CFG_METHOD_29:
        case CFG_METHOD_30:
                spin_lock_irqsave(&tp->phy_lock, flags);
                mdio_write(tp, 0x1F, 0x0B82);
                ClearEthPhyBit(tp, 0x10, BIT_4);
                mdio_write(tp, 0x1F, 0x0000);
                spin_unlock_irqrestore(&tp->phy_lock, flags);
                break;
        }

        return ret;
}

static int rtl8168_disable_EEE(struct rtl8168_private *tp)
{
        void __iomem *ioaddr = tp->mmio_addr;
        int ret;
        u16 data;
        u16 PhyRegValue;
        u32 WaitCnt;
        unsigned long flags;

        ret = 0;
        switch (tp->mcfg) {
        case CFG_METHOD_14:
        case CFG_METHOD_15:
                spin_lock_irqsave(&tp->phy_lock, flags);
                mdio_write(tp, 0x1F, 0x0005);
                mdio_write(tp, 0x05, 0x8B85);
                data = mdio_read(tp, 0x06) & ~0x2000;
                mdio_write(tp, 0x06, data);
                mdio_write(tp, 0x1F, 0x0007);
                mdio_write(tp, 0x1E, 0x0020);
                data = mdio_read(tp, 0x15) & ~0x0100;
                mdio_write(tp, 0x15, data);
                mdio_write(tp, 0x1F, 0x0006);
                mdio_write(tp, 0x00, 0x5A00);
                mdio_write(tp, 0x1F, 0x0000);
                mdio_write(tp, 0x0D, 0x0007);
                mdio_write(tp, 0x0E, 0x003C);
                mdio_write(tp, 0x0D, 0x4007);
                mdio_write(tp, 0x0E, 0x0000);
                mdio_write(tp, 0x0D, 0x0000);
                mdio_write(tp, 0x1F, 0x0000);
                spin_unlock_irqrestore(&tp->phy_lock, flags);
                if (RTL_R8(Config4) & 0x40) {
                        spin_lock_irqsave(&tp->phy_lock, flags);
                        mdio_write(tp, 0x1F, 0x0005);
                        mdio_write(tp, 0x05, 0x8B82);
                        data = mdio_read(tp, 0x06) & ~0x0010;
                        mdio_write(tp, 0x05, 0x8B82);
                        mdio_write(tp, 0x06, data);
                        mdio_write(tp, 0x1F, 0x0000);
                        spin_unlock_irqrestore(&tp->phy_lock, flags);
                }
                break;

        case CFG_METHOD_16:
        case CFG_METHOD_17:
                spin_lock_irqsave(&tp->phy_lock, flags);
                data = rtl8168_eri_read(ioaddr,0x1B0 ,4,ERIAR_ExGMAC)& ~0x0003;
                rtl8168_eri_write(ioaddr, 0x1B0, 4, data, ERIAR_ExGMAC);
                mdio_write(tp, 0x1F, 0x0005);
                mdio_write(tp, 0x05, 0x8B85);
                data = mdio_read(tp, 0x06) & ~0x2000;
                mdio_write(tp, 0x06, data);
                mdio_write(tp, 0x1F, 0x0004);
                mdio_write(tp, 0x1F, 0x0007);
                mdio_write(tp, 0x1E, 0x0020);
                data = mdio_read(tp, 0x15) & ~0x0100;
                mdio_write(tp,0x15 , data);
                mdio_write(tp, 0x1F, 0x0002);
                mdio_write(tp, 0x1F, 0x0000);
                mdio_write(tp, 0x0D, 0x0007);
                mdio_write(tp, 0x0E, 0x003C);
                mdio_write(tp, 0x0D, 0x4007);
                mdio_write(tp, 0x0E, 0x0000);
                mdio_write(tp, 0x0D, 0x0000);
                mdio_write(tp, 0x1F, 0x0000);
                spin_unlock_irqrestore(&tp->phy_lock, flags);
                break;

        case CFG_METHOD_18:
        case CFG_METHOD_19:
        case CFG_METHOD_20:
                spin_lock_irqsave(&tp->phy_lock, flags);
                data = rtl8168_eri_read(ioaddr,0x1B0 ,4,ERIAR_ExGMAC);
                data &= ~(BIT_1 | BIT_0);
                rtl8168_eri_write(ioaddr, 0x1B0, 4, data, ERIAR_ExGMAC);
                mdio_write(tp, 0x1F, 0x0005);
                mdio_write(tp, 0x05, 0x8B85);
                data = mdio_read(tp, 0x06);
                data &= ~BIT_13;
                mdio_write(tp, 0x06, data);
                mdio_write(tp, 0x1F, 0x0007);
                mdio_write(tp, 0x1e, 0x0020);
                data = mdio_read(tp, 0x15);
                data &= ~BIT_8;
                mdio_write(tp, 0x15, data);
                mdio_write(tp, 0x1F, 0x0000);
                mdio_write(tp, 0x0D, 0x0007);
                mdio_write(tp, 0x0E, 0x003C);
                mdio_write(tp, 0x0D, 0x4007);
                mdio_write(tp, 0x0E, 0x0000);
                mdio_write(tp, 0x0D, 0x0000);
                mdio_write(tp, 0x1F, 0x0000);
                spin_unlock_irqrestore(&tp->phy_lock, flags);
                break;

        case CFG_METHOD_21:
        case CFG_METHOD_22:
        case CFG_METHOD_23:
        case CFG_METHOD_24:
        case CFG_METHOD_25:
        case CFG_METHOD_26:
        case CFG_METHOD_27:
        case CFG_METHOD_28:
        case CFG_METHOD_29:
        case CFG_METHOD_30:
                spin_lock_irqsave(&tp->phy_lock, flags);
                data = rtl8168_eri_read(ioaddr, 0x1B0, 4, ERIAR_ExGMAC);
                data &= ~(BIT_1 | BIT_0);
                rtl8168_eri_write(ioaddr, 0x1B0, 4, data, ERIAR_ExGMAC);
                mdio_write(tp, 0x1F, 0x0A43);
                data = mdio_read(tp, 0x11);
                mdio_write(tp, 0x11, data & ~BIT_4);
                mdio_write(tp, 0x1F, 0x0A5D);
                mdio_write(tp, 0x10, 0x0000);
                mdio_write(tp, 0x1F, 0x0000);
                spin_unlock_irqrestore(&tp->phy_lock, flags);
                break;

        default:
//      dev_printk(KERN_DEBUG, &tp->pci_dev->dev, "Not Support EEE\n");
                ret = -EOPNOTSUPP;
                break;
        }

        switch (tp->mcfg) {
        case CFG_METHOD_29:
        case CFG_METHOD_30:
                spin_lock_irqsave(&tp->phy_lock, flags);
                mdio_write(tp, 0x1F, 0x0A42);
                ClearEthPhyBit(tp, 0x14, BIT_7);
                mdio_write(tp, 0x1F, 0x0A4A);
                ClearEthPhyBit(tp, 0x11, BIT_9);
                mdio_write(tp, 0x1F, 0x0000);
                spin_unlock_irqrestore(&tp->phy_lock, flags);
                break;
        }

        /*Advanced EEE*/
        switch (tp->mcfg) {
        case CFG_METHOD_24:
        case CFG_METHOD_25:
        case CFG_METHOD_26:
        case CFG_METHOD_29:
        case CFG_METHOD_30:
                spin_lock_irqsave(&tp->phy_lock, flags);
                mdio_write(tp,0x1F, 0x0B82);
                SetEthPhyBit(tp, 0x10, BIT_4);
                mdio_write(tp, 0x1F, 0x0000);

                mdio_write(tp,0x1F, 0x0B80);
                WaitCnt = 0;
                do {
                        PhyRegValue = mdio_read(tp, 0x10);
                        PhyRegValue &= 0x0040;
                        udelay(100);
                        WaitCnt++;
                } while(PhyRegValue != 0x0040 && WaitCnt <1000);

                mdio_write(tp, 0x1F, 0x0000);
                spin_unlock_irqrestore(&tp->phy_lock, flags);
                break;
        }

        switch (tp->mcfg) {
        case CFG_METHOD_25:
                rtl8168_eri_write(ioaddr, 0x1EA, 1, 0x00, ERIAR_ExGMAC);

                spin_lock_irqsave(&tp->phy_lock, flags);
                mdio_write(tp, 0x1F, 0x0A42);
                data = mdio_read(tp, 0x16);
                data &= ~(BIT_1);
                mdio_write(tp, 0x16, data);
                mdio_write(tp, 0x1F, 0x0000);
                spin_unlock_irqrestore(&tp->phy_lock, flags);
                break;
        case CFG_METHOD_26:
                data = mac_ocp_read(tp, 0xE052);
                data &= ~(BIT_0);
                mac_ocp_write(tp, 0xE052, data);

                spin_lock_irqsave(&tp->phy_lock, flags);
                mdio_write(tp, 0x1F, 0x0A42);
                data = mdio_read(tp, 0x16);
                data &= ~(BIT_1);
                mdio_write(tp, 0x16, data);
                mdio_write(tp, 0x1F, 0x0000);
                spin_unlock_irqrestore(&tp->phy_lock, flags);
                break;
        case CFG_METHOD_27:
        case CFG_METHOD_28:
                data = mac_ocp_read(tp, 0xE052);
                data &= ~(BIT_0);
                mac_ocp_write(tp, 0xE052, data);
                break;
        case CFG_METHOD_29:
        case CFG_METHOD_30:
                data = mac_ocp_read(tp, 0xE052);
                data &= ~(BIT_0);
                mac_ocp_write(tp, 0xE052, data);

                spin_lock_irqsave(&tp->phy_lock, flags);
                mdio_write(tp, 0x1F, 0x0A43);
                data = mdio_read(tp, 0x10) & ~(BIT_15);
                mdio_write(tp, 0x10, data);

                mdio_write(tp, 0x1F, 0x0A44);
                data = mdio_read(tp, 0x11) & ~(BIT_12 | BIT_13 | BIT_14);
                mdio_write(tp, 0x11, data);
                spin_unlock_irqrestore(&tp->phy_lock, flags);
                break;
        }

        switch (tp->mcfg) {
        case CFG_METHOD_24:
        case CFG_METHOD_25:
        case CFG_METHOD_26:
        case CFG_METHOD_29:
        case CFG_METHOD_30:
                spin_lock_irqsave(&tp->phy_lock, flags);
                mdio_write(tp, 0x1F, 0x0B82);
                ClearEthPhyBit(tp, 0x10, BIT_4);
                mdio_write(tp, 0x1F, 0x0000);
                spin_unlock_irqrestore(&tp->phy_lock, flags);
                break;
        }

        return ret;
}

#if 0

static int rtl8168_enable_green_feature(struct rtl8168_private *tp)
{
        u16 gphy_val;
        unsigned long flags;

        switch (tp->mcfg) {
        case CFG_METHOD_14:
        case CFG_METHOD_15:
                spin_lock_irqsave(&tp->phy_lock, flags);
                mdio_write(tp, 0x1F, 0x0003);
                gphy_val = mdio_read(tp, 0x10) | 0x0400;
                mdio_write(tp, 0x10, gphy_val);
                gphy_val = mdio_read(tp, 0x19) | 0x0001;
                mdio_write(tp, 0x19, gphy_val);
                mdio_write(tp, 0x1F, 0x0005);
                gphy_val = mdio_read(tp, 0x01) & ~0x0100;
                mdio_write(tp, 0x01, gphy_val);
                mdio_write(tp, 0x1F, 0x0000);
                mdio_write(tp, 0x00, 0x9200);
                spin_unlock_irqrestore(&tp->phy_lock, flags);
                mdelay(20);
                break;

        case CFG_METHOD_17:
        case CFG_METHOD_18:
        case CFG_METHOD_19:
        case CFG_METHOD_20:
                spin_lock_irqsave(&tp->phy_lock, flags);
                mdio_write(tp, 0x1f, 0x0003);
                gphy_val = mdio_read(tp, 0x10);
                gphy_val |= BIT_10;
                mdio_write(tp, 0x10, gphy_val);
                gphy_val = mdio_read(tp, 0x19);
                gphy_val |= BIT_0;
                mdio_write(tp, 0x19, gphy_val);
                mdio_write(tp, 0x1F, 0x0005);
                gphy_val = mdio_read(tp, 0x01);
                gphy_val |= BIT_8;
                mdio_write(tp, 0x01, gphy_val);
                mdio_write(tp, 0x1f, 0x0000);
                mdio_write(tp, 0x00, 0x9200);
                spin_unlock_irqrestore(&tp->phy_lock, flags);
                break;
        case CFG_METHOD_21:
        case CFG_METHOD_23:
        case CFG_METHOD_24:
        case CFG_METHOD_25:
        case CFG_METHOD_26:
        case CFG_METHOD_27:
        case CFG_METHOD_28:
        case CFG_METHOD_29:
        case CFG_METHOD_30:
                spin_lock_irqsave(&tp->phy_lock, flags);
                mdio_write(tp, 0x1F, 0x0A43);
                mdio_write(tp, 0x13, 0x8011);
                if (tp->mcfg == CFG_METHOD_29 || tp->mcfg == CFG_METHOD_30) {
                        SetEthPhyBit( tp, 0x14, BIT_15 );
                } else {
                        SetEthPhyBit( tp, 0x14, BIT_14 );
                }
                mdio_write(tp, 0x1F, 0x0A40);
                mdio_write(tp, 0x00, 0x9200);
                spin_unlock_irqrestore(&tp->phy_lock, flags);
                break;

        default:
                dev_printk(KERN_DEBUG, &tp->pci_dev->dev, "Not Support Green Feature\n");
                break;
        }

        return 0;
}

static int rtl8168_disable_green_feature(struct rtl8168_private *tp)
{
        u16 gphy_val;
        unsigned long flags;

        switch (tp->mcfg) {
        case CFG_METHOD_14:
        case CFG_METHOD_15:
                spin_lock_irqsave(&tp->phy_lock, flags);
                mdio_write(tp, 0x1F, 0x0005);
                gphy_val = mdio_read(tp, 0x01) | 0x0100;
                mdio_write(tp, 0x01, gphy_val);
                mdio_write(tp, 0x1F, 0x0003);
                gphy_val = mdio_read(tp, 0x10) & ~0x0400;
                mdio_write(tp, 0x10, gphy_val);
                gphy_val = mdio_read(tp, 0x19) & ~0x0001;
                mdio_write(tp, 0x19, gphy_val);
                mdio_write(tp, 0x1F, 0x0002);
                gphy_val = mdio_read(tp, 0x06) & ~0x7000;
                gphy_val |= 0x3000;
                mdio_write(tp, 0x06, gphy_val);
                gphy_val = mdio_read(tp, 0x0D) & 0x0700;
                gphy_val |= 0x0500;
                mdio_write(tp, 0x0D, gphy_val);
                mdio_write(tp, 0x1F, 0x0000);
                spin_unlock_irqrestore(&tp->phy_lock, flags);
                break;

        case CFG_METHOD_17:
        case CFG_METHOD_18:
        case CFG_METHOD_19:
        case CFG_METHOD_20:
                spin_lock_irqsave(&tp->phy_lock, flags);
                mdio_write(tp, 0x1f, 0x0003);
                gphy_val = mdio_read(tp, 0x19);
                gphy_val &= ~BIT_0;
                mdio_write(tp, 0x19, gphy_val);
                gphy_val = mdio_read(tp, 0x10);
                gphy_val &= ~BIT_10;
                mdio_write(tp, 0x10, gphy_val);
                mdio_write(tp, 0x1f, 0x0000);
                spin_unlock_irqrestore(&tp->phy_lock, flags);
                break;
        case CFG_METHOD_21:
        case CFG_METHOD_23:
        case CFG_METHOD_24:
        case CFG_METHOD_25:
        case CFG_METHOD_26:
        case CFG_METHOD_27:
        case CFG_METHOD_28:
        case CFG_METHOD_29:
        case CFG_METHOD_30:
                spin_lock_irqsave(&tp->phy_lock, flags);
                mdio_write(tp, 0x1F, 0x0A43);
                mdio_write(tp, 0x13, 0x8011);
                if (tp->mcfg == CFG_METHOD_29 || tp->mcfg == CFG_METHOD_30) {
                        ClearEthPhyBit( tp, 0x14, BIT_15 );
                } else {
                        ClearEthPhyBit( tp, 0x14, BIT_14 );
                }
                mdio_write(tp, 0x1F, 0x0A40);
                mdio_write(tp, 0x00, 0x9200);
                spin_unlock_irqrestore(&tp->phy_lock, flags);
                break;

        default:
                dev_printk(KERN_DEBUG, &tp->pci_dev->dev, "Not Support Green Feature\n");
                break;
        }

        return 0;
}

#endif

static void rtl8168_get_mac_version(struct rtl8168_private *tp, void __iomem *ioaddr)
{
        u32 reg,val32;
        u32 ICVerID;

        val32 = RTL_R32(TxConfig)  ;
        reg = val32 & 0x7c800000;
        ICVerID = val32 & 0x00700000;

        switch (reg) {
        case 0x30000000:
                tp->mcfg = CFG_METHOD_1;
                tp->efuse_ver = EFUSE_NOT_SUPPORT;
                break;
        case 0x38000000:
                if (ICVerID == 0x00000000) {
                        tp->mcfg = CFG_METHOD_2;
                } else if (ICVerID == 0x00500000) {
                        tp->mcfg = CFG_METHOD_3;
                } else {
                        tp->mcfg = CFG_METHOD_3;
                        tp->HwIcVerUnknown = TRUE;
                }
                tp->efuse_ver = EFUSE_NOT_SUPPORT;
                break;
        case 0x3C000000:
                if (ICVerID == 0x00000000) {
                        tp->mcfg = CFG_METHOD_4;
                } else if (ICVerID == 0x00200000) {
                        tp->mcfg = CFG_METHOD_5;
                } else if (ICVerID == 0x00400000) {
                        tp->mcfg = CFG_METHOD_6;
                } else {
                        tp->mcfg = CFG_METHOD_6;
                        tp->HwIcVerUnknown = TRUE;
                }
                tp->efuse_ver = EFUSE_NOT_SUPPORT;
                break;
        case 0x3C800000:
                if (ICVerID == 0x00100000) {
                        tp->mcfg = CFG_METHOD_7;
                } else if (ICVerID == 0x00300000) {
                        tp->mcfg = CFG_METHOD_8;
                } else {
                        tp->mcfg = CFG_METHOD_8;
                        tp->HwIcVerUnknown = TRUE;
                }
                tp->efuse_ver = EFUSE_NOT_SUPPORT;
                break;
        case 0x28000000:
                if (ICVerID == 0x00100000) {
                        tp->mcfg = CFG_METHOD_9;
                } else if (ICVerID == 0x00300000) {
                        tp->mcfg = CFG_METHOD_10;
                } else {
                        tp->mcfg = CFG_METHOD_10;
                        tp->HwIcVerUnknown = TRUE;
                }
                tp->efuse_ver = EFUSE_SUPPORT_V1;
                break;
        case 0x28800000:
                if (ICVerID == 0x00000000) {
                        tp->mcfg = CFG_METHOD_11;
                } else if (ICVerID == 0x00200000) {
                        tp->mcfg = CFG_METHOD_12;
                        RTL_W32(0xD0, RTL_R32(0xD0) | 0x00020000);
                } else if (ICVerID == 0x00300000) {
                        tp->mcfg = CFG_METHOD_13;
                } else {
                        tp->mcfg = CFG_METHOD_13;
                        tp->HwIcVerUnknown = TRUE;
                }
                tp->efuse_ver = EFUSE_SUPPORT_V1;
                break;
        case 0x2C000000:
                if (ICVerID == 0x00100000) {
                        tp->mcfg = CFG_METHOD_14;
                } else if (ICVerID == 0x00200000) {
                        tp->mcfg = CFG_METHOD_15;
                } else {
                        tp->mcfg = CFG_METHOD_15;
                        tp->HwIcVerUnknown = TRUE;
                }
                tp->efuse_ver = EFUSE_SUPPORT_V2;
                break;
        case 0x2C800000:
                if (ICVerID == 0x00000000) {
                        tp->mcfg = CFG_METHOD_16;
                } else if (ICVerID == 0x00100000) {
                        tp->mcfg = CFG_METHOD_17;
                } else {
                        tp->mcfg = CFG_METHOD_17;
                        tp->HwIcVerUnknown = TRUE;
                }
                tp->efuse_ver = EFUSE_SUPPORT_V3;
                break;
        case 0x48000000:
                if (ICVerID == 0x00000000) {
                        tp->mcfg = CFG_METHOD_18;
                } else if (ICVerID == 0x00100000) {
                        tp->mcfg = CFG_METHOD_19;
                } else {
                        tp->mcfg = CFG_METHOD_19;
                        tp->HwIcVerUnknown = TRUE;
                }
                tp->efuse_ver = EFUSE_SUPPORT_V3;
                break;
        case 0x48800000:
                if (ICVerID == 0x00000000) {
                        tp->mcfg = CFG_METHOD_20;
                } else {
                        tp->mcfg = CFG_METHOD_20;
                        tp->HwIcVerUnknown = TRUE;
                }

                tp->efuse_ver = EFUSE_SUPPORT_V3;
                break;
        case 0x4C000000:
                if (ICVerID == 0x00000000) {
                        tp->mcfg = CFG_METHOD_21;
                } else if (ICVerID == 0x00100000) {
                        tp->mcfg = CFG_METHOD_22;
                } else {
                        tp->mcfg = CFG_METHOD_22;
                        tp->HwIcVerUnknown = TRUE;
                }
                tp->efuse_ver = EFUSE_SUPPORT_V3;
                break;
        case 0x50000000:
                if (ICVerID == 0x00000000) {
                        tp->mcfg = CFG_METHOD_23;
                } else if (ICVerID == 0x00100000) {
                        tp->mcfg = CFG_METHOD_27;
                } else if (ICVerID == 0x00200000) {
                        tp->mcfg = CFG_METHOD_28;
                } else {
                        tp->mcfg = CFG_METHOD_28;
                        tp->HwIcVerUnknown = TRUE;
                }
                tp->efuse_ver = EFUSE_SUPPORT_V3;
                break;
        case 0x50800000:
                if (ICVerID == 0x00000000) {
                        tp->mcfg = CFG_METHOD_24;
                } else if (ICVerID == 0x00100000) {
                        tp->mcfg = CFG_METHOD_25;
                } else {
                        tp->mcfg = CFG_METHOD_25;
                        tp->HwIcVerUnknown = TRUE;
                }
                tp->efuse_ver = EFUSE_SUPPORT_V3;
                break;
        case 0x5C800000:
                if (ICVerID == 0x00000000) {
                        tp->mcfg = CFG_METHOD_26;
                } else {
                        tp->mcfg = CFG_METHOD_26;
                        tp->HwIcVerUnknown = TRUE;
                }

                tp->efuse_ver = EFUSE_SUPPORT_V3;
                break;
        case 0x54000000:
                if (ICVerID == 0x00000000) {
                        tp->mcfg = CFG_METHOD_29;
                } else if (ICVerID == 0x00100000) {
                        tp->mcfg = CFG_METHOD_30;
                } else {
                        tp->mcfg = CFG_METHOD_30;
                        tp->HwIcVerUnknown = TRUE;
                }

                tp->efuse_ver = EFUSE_SUPPORT_V3;
                break;
        default:
                printk("unknown chip version (%x)\n",reg);
                tp->mcfg = CFG_METHOD_DEFAULT;
                tp->HwIcVerUnknown = TRUE;
                tp->efuse_ver = EFUSE_NOT_SUPPORT;
                break;
        }
}

static void
rtl8168_print_mac_version(struct rtl8168_private *tp)
{
        int i;
        for (i = ARRAY_SIZE(rtl_chip_info) - 1; i >= 0; i--) {
                if (tp->mcfg == rtl_chip_info[i].mcfg) {
                        dprintk("Realtek PCIe GBE Family Controller mcfg = %04d\n",
                                rtl_chip_info[i].mcfg);
                        return;
                }
        }

        dprintk("mac_version == Unknown\n");
}

static u8 rtl8168_calc_efuse_dummy_bit(u16 reg)
{
        int s,a,b;
        u8 dummyBitPos = 0;


        s=reg% 32;
        a=s % 16;
        b=s/16;

        if (s/16) {
                dummyBitPos = (u8)(16-a);
        } else {
                dummyBitPos = (u8)a;
        }

        return dummyBitPos;
}

static u32 rtl8168_decode_efuse_cmd(struct rtl8168_private *tp, u32 DwCmd)
{
        u16 reg = (u16)((DwCmd & 0x00FE0000) >> 17);
        u32 DummyPos = rtl8168_calc_efuse_dummy_bit(reg);
        u32 DeCodeDwCmd = DwCmd;
        u32 Dw17BitData;


        if(tp->efuse_ver < 3) {
                DeCodeDwCmd = (DwCmd>>(DummyPos+1))<<DummyPos;
                if(DummyPos > 0) {
                        DeCodeDwCmd |= ((DwCmd<<(32-DummyPos))>>(32-DummyPos));
                }
        } else {
                reg = (u16)((DwCmd & 0x007F0000) >> 16);
                DummyPos = rtl8168_calc_efuse_dummy_bit(reg);
                Dw17BitData = ((DwCmd & BIT_23) >> 23);
                Dw17BitData <<= 16;
                Dw17BitData |= (DwCmd & 0x0000FFFF);
                DeCodeDwCmd = (Dw17BitData>>(DummyPos+1))<<DummyPos;
                if(DummyPos > 0) {
                        DeCodeDwCmd |= ((Dw17BitData<<(32-DummyPos))>>(32-DummyPos));
                }
        }

        return DeCodeDwCmd;
}

static u8 rtl8168_efuse_read(struct rtl8168_private *tp, u16 reg)
{
        void __iomem *ioaddr = tp->mmio_addr;
        u8 efuse_data = 0;
        u32 temp;
        int cnt;

        if (tp->efuse_ver == EFUSE_NOT_SUPPORT)
                return EFUSE_READ_FAIL;

        if (tp->efuse_ver == EFUSE_SUPPORT_V1) {
                temp = EFUSE_READ | ((reg & EFUSE_Reg_Mask) << EFUSE_Reg_Shift);
                RTL_W32(EFUSEAR, temp);

                cnt = 0;
                do {
                        udelay(100);
                        temp = RTL_R32(EFUSEAR);
                        cnt++;
                } while (!(temp & EFUSE_READ_OK) && (cnt < EFUSE_Check_Cnt));

                if (cnt == EFUSE_Check_Cnt)
                        efuse_data = EFUSE_READ_FAIL;
                else
                        efuse_data = (u8)(RTL_R32(EFUSEAR) & EFUSE_Data_Mask);
        } else  if (tp->efuse_ver == EFUSE_SUPPORT_V2) {
                temp = (reg/2) & 0x03ff;
                temp <<= 17;
                temp |= EFUSE_READ;
                RTL_W32(EFUSEAR, temp);

                cnt = 0;
                do {
                        udelay(100);
                        temp = RTL_R32(EFUSEAR);
                        cnt++;
                } while (!(temp & EFUSE_READ_OK) && (cnt < EFUSE_Check_Cnt));

                if (cnt == EFUSE_Check_Cnt) {
                        efuse_data = EFUSE_READ_FAIL;
                } else {
                        temp = RTL_R32(EFUSEAR);
                        temp = rtl8168_decode_efuse_cmd(tp, temp);

                        if(reg%2) {
                                temp >>= 8;
                                efuse_data = (u8)temp;
                        } else {
                                efuse_data = (u8)temp;
                        }
                }

        } else  if (tp->efuse_ver == EFUSE_SUPPORT_V3) {
                temp = (reg/2) & 0x03ff;
                temp <<= 16;
                temp |= EFUSE_READ;
                RTL_W32(EFUSEAR, temp);

                cnt = 0;
                do {
                        udelay(100);
                        temp = RTL_R32(EFUSEAR);
                        cnt++;
                } while (!(temp & EFUSE_READ_OK) && (cnt < EFUSE_Check_Cnt));

                if (cnt == EFUSE_Check_Cnt) {
                        efuse_data = EFUSE_READ_FAIL;
                } else {
                        temp = RTL_R32(EFUSEAR);
                        temp = rtl8168_decode_efuse_cmd(tp, temp);

                        if(reg%2) {
                                temp >>= 8;
                                efuse_data = (u8)temp;
                        } else {
                                efuse_data = (u8)temp;
                        }
                }
        }

        udelay(20);

        return efuse_data;
}

static void
rtl8168_tally_counter_addr_fill(struct rtl8168_private *tp)
{
        void __iomem *ioaddr = tp->mmio_addr;

        if (!tp->tally_paddr)
                return;

        RTL_W32(CounterAddrHigh, (u64)tp->tally_paddr >> 32);
        RTL_W32(CounterAddrLow, (u64)tp->tally_paddr & (DMA_BIT_MASK(32)));
}

static void
rtl8168_tally_counter_clear(struct rtl8168_private *tp)
{
        void __iomem *ioaddr = tp->mmio_addr;

        if (tp->mcfg == CFG_METHOD_1 || tp->mcfg == CFG_METHOD_2 ||
            tp->mcfg == CFG_METHOD_3 )
                return;

        if (!tp->tally_paddr)
                return;

        RTL_W32(CounterAddrHigh, (u64)tp->tally_paddr >> 32);
        RTL_W32(CounterAddrLow, (u64)tp->tally_paddr & (DMA_BIT_MASK(32) | BIT_0));
}

static int
rtl8168_is_ups_resume(struct net_device *dev)
{
        struct rtl8168_private *tp = netdev_priv(dev);

        return (mac_ocp_read(tp, 0xD408) & BIT_0);
}

static void
rtl8168_clear_ups_resume_bit(struct net_device *dev)
{
        struct rtl8168_private *tp = netdev_priv(dev);

        mac_ocp_write(tp, 0xD408, mac_ocp_read(tp, 0xD408) & ~(BIT_0));
}

static void
rtl8168_wait_phy_ups_resume(struct net_device *dev, u16 PhyState)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        u16 TmpPhyState;
        int i=0;

        do {
                TmpPhyState = mdio_read_phy_ocp(tp, 0x0A42, 0x10);
                TmpPhyState &= 0x7;
                mdelay(1);
                i++;
        } while ((i < 100) && (TmpPhyState != PhyState));

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,18)
        WARN_ON_ONCE(i == 100);
#endif
}

void
EnableNowIsOob(struct rtl8168_private *tp)
{
        void __iomem *ioaddr = tp->mmio_addr;

        if( tp->HwSuppNowIsOobVer == 1 ) {
                RTL_W8(MCUCmd_reg, RTL_R8(MCUCmd_reg) | Now_is_oob);
        }
}

void
DisableNowIsOob(struct rtl8168_private *tp)
{
        void __iomem *ioaddr = tp->mmio_addr;

        if( tp->HwSuppNowIsOobVer == 1 ) {
                RTL_W8(MCUCmd_reg, RTL_R8(MCUCmd_reg) & ~Now_is_oob);
        }
}

static void
rtl8168_exit_oob(struct net_device *dev)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;
        u16 data16;

        RTL_W32(RxConfig, RTL_R32(RxConfig) & ~(AcceptErr | AcceptRunt | AcceptBroadcast | AcceptMulticast | AcceptMyPhys |  AcceptAllPhys));

        switch (tp->mcfg) {
        case CFG_METHOD_23:
        case CFG_METHOD_27:
        case CFG_METHOD_28:
                Dash2DisableTxRx(dev);
                break;
        }

        switch (tp->mcfg) {
        case CFG_METHOD_11:
        case CFG_METHOD_12:
        case CFG_METHOD_13:
        case CFG_METHOD_23:
        case CFG_METHOD_27:
        case CFG_METHOD_28:
                if (tp->DASH) {
                        rtl8168_driver_stop(tp);
                        rtl8168_driver_start(tp);
#ifdef ENABLE_DASH_SUPPORT
                        DashHwInit(dev);
#endif
                }
                break;
        }

        //Disable realwow  function
        switch (tp->mcfg) {
        case CFG_METHOD_18:
        case CFG_METHOD_19:
                RTL_W32(MACOCP, 0xE5A90000);
                RTL_W32(MACOCP, 0xF2100010);
                break;
        case CFG_METHOD_20:
                RTL_W32(MACOCP, 0xE5A90000);
                RTL_W32(MACOCP, 0xE4640000);
                RTL_W32(MACOCP, 0xF2100010);
                break;
        case CFG_METHOD_21:
        case CFG_METHOD_22:
                RTL_W32(MACOCP, 0x605E0000);
                RTL_W32(MACOCP, (0xE05E << 16) | (RTL_R32(MACOCP) & 0xFFFE));
                RTL_W32(MACOCP, 0xE9720000);
                RTL_W32(MACOCP, 0xF2140010);
                break;
        case CFG_METHOD_26:
                RTL_W32(MACOCP, 0xE05E00FF);
                RTL_W32(MACOCP, 0xE9720000);
                mac_ocp_write(tp, 0xE428, 0x0010);
                break;
        }

#ifndef ENABLE_REALWOW_SUPPORT
        switch (tp->mcfg) {
        case CFG_METHOD_21:
        case CFG_METHOD_22:
                rtl8168_eri_write(ioaddr, 0x174, 2, 0x0000, ERIAR_ExGMAC);
                mac_ocp_write(tp, 0xE428, 0x0010);
                break;
        case CFG_METHOD_24:
        case CFG_METHOD_25:
        case CFG_METHOD_26:
        case CFG_METHOD_28:
                rtl8168_eri_write(ioaddr, 0x174, 2, 0x00FF, ERIAR_ExGMAC);
                mac_ocp_write(tp, 0xE428, 0x0010);
                break;
        case CFG_METHOD_29:
        case CFG_METHOD_30: {
                u32 csi_tmp;
                csi_tmp = rtl8168_eri_read(ioaddr, 0x174, 2, ERIAR_ExGMAC);
                csi_tmp &= ~(BIT_8);
                csi_tmp |= (BIT_15);
                rtl8168_eri_write(ioaddr, 0x174, 2, csi_tmp, ERIAR_ExGMAC);
                mac_ocp_write(tp, 0xE428, 0x0010);
        }
        break;
        }
#endif //ENABLE_REALWOW_SUPPORT

#ifdef ENABLE_REALWOW_SUPPORT
        realwow_hw_init(dev);
#endif

        rtl8168_nic_reset(dev);

        switch (tp->mcfg) {
        case CFG_METHOD_20:
                rtl8168_wait_ll_share_fifo_ready(dev);

                data16 = mac_ocp_read(tp, 0xD4DE) | BIT_15;
                mac_ocp_write(tp, 0xD4DE, data16);

                rtl8168_wait_ll_share_fifo_ready(dev);
                break;
        case CFG_METHOD_21:
        case CFG_METHOD_22:
        case CFG_METHOD_23:
        case CFG_METHOD_24:
        case CFG_METHOD_25:
        case CFG_METHOD_26:
        case CFG_METHOD_27:
        case CFG_METHOD_28:
        case CFG_METHOD_29:
        case CFG_METHOD_30:
                DisableNowIsOob(tp);

                data16 = mac_ocp_read(tp, 0xE8DE) & ~BIT_14;
                mac_ocp_write(tp, 0xE8DE, data16);
                rtl8168_wait_ll_share_fifo_ready(dev);

                data16 = mac_ocp_read(tp, 0xE8DE) | BIT_15;
                mac_ocp_write(tp, 0xE8DE, data16);

                rtl8168_wait_ll_share_fifo_ready(dev);
                break;
        }

        //wait ups resume (phy state 2)
        switch (tp->mcfg) {
        case CFG_METHOD_29:
        case CFG_METHOD_30:
                if (rtl8168_is_ups_resume(dev)) {
                        unsigned long flags;

                        spin_lock_irqsave(&tp->phy_lock, flags);

                        rtl8168_wait_phy_ups_resume(dev, 2);

                        spin_unlock_irqrestore(&tp->phy_lock, flags);

                        rtl8168_clear_ups_resume_bit(dev);
                }
                break;
        };
}

void
rtl8168_hw_disable_mac_mcu_bps(struct net_device *dev)
{
        struct rtl8168_private *tp = netdev_priv(dev);

        switch (tp->mcfg) {
        case CFG_METHOD_21:
        case CFG_METHOD_22:
        case CFG_METHOD_23:
        case CFG_METHOD_24:
        case CFG_METHOD_25:
        case CFG_METHOD_26:
        case CFG_METHOD_27:
        case CFG_METHOD_28:
        case CFG_METHOD_29:
        case CFG_METHOD_30:
                mac_ocp_write(tp, 0xFC28, 0x0000);
                mac_ocp_write(tp, 0xFC2A, 0x0000);
                mac_ocp_write(tp, 0xFC2C, 0x0000);
                mac_ocp_write(tp, 0xFC2E, 0x0000);
                mac_ocp_write(tp, 0xFC30, 0x0000);
                mac_ocp_write(tp, 0xFC32, 0x0000);
                mac_ocp_write(tp, 0xFC34, 0x0000);
                mac_ocp_write(tp, 0xFC36, 0x0000);
                mdelay(3);
                mac_ocp_write(tp, 0xFC26, 0x0000);
                break;
        }
}

static void
rtl8168_hw_mac_mcu_config(struct net_device *dev)
{
        struct rtl8168_private *tp = netdev_priv(dev);

        if (tp->NotWrMcuPatchCode == TRUE) return;

        if (tp->mcfg == CFG_METHOD_21) {
                mac_ocp_write(tp, 0xE43C, 0x0000);
                mac_ocp_write(tp, 0xE43E, 0x0000);

                mac_ocp_write(tp, 0xE434, 0x0004);
                mac_ocp_write(tp, 0xE43C, 0x0004);

                rtl8168_hw_disable_mac_mcu_bps(dev);

                mac_ocp_write( tp, 0xF800, 0xE008 );
                mac_ocp_write( tp, 0xF802, 0xE01B );
                mac_ocp_write( tp, 0xF804, 0xE01D );
                mac_ocp_write( tp, 0xF806, 0xE01F );
                mac_ocp_write( tp, 0xF808, 0xE022 );
                mac_ocp_write( tp, 0xF80A, 0xE025 );
                mac_ocp_write( tp, 0xF80C, 0xE031 );
                mac_ocp_write( tp, 0xF80E, 0xE04D );
                mac_ocp_write( tp, 0xF810, 0x49D2 );
                mac_ocp_write( tp, 0xF812, 0xF10D );
                mac_ocp_write( tp, 0xF814, 0x766C );
                mac_ocp_write( tp, 0xF816, 0x49E2 );
                mac_ocp_write( tp, 0xF818, 0xF00A );
                mac_ocp_write( tp, 0xF81A, 0x1EC0 );
                mac_ocp_write( tp, 0xF81C, 0x8EE1 );
                mac_ocp_write( tp, 0xF81E, 0xC60A );
                mac_ocp_write( tp, 0xF820, 0x77C0 );
                mac_ocp_write( tp, 0xF822, 0x4870 );
                mac_ocp_write( tp, 0xF824, 0x9FC0 );
                mac_ocp_write( tp, 0xF826, 0x1EA0 );
                mac_ocp_write( tp, 0xF828, 0xC707 );
                mac_ocp_write( tp, 0xF82A, 0x8EE1 );
                mac_ocp_write( tp, 0xF82C, 0x9D6C );
                mac_ocp_write( tp, 0xF82E, 0xC603 );
                mac_ocp_write( tp, 0xF830, 0xBE00 );
                mac_ocp_write( tp, 0xF832, 0xB416 );
                mac_ocp_write( tp, 0xF834, 0x0076 );
                mac_ocp_write( tp, 0xF836, 0xE86C );
                mac_ocp_write( tp, 0xF838, 0xC602 );
                mac_ocp_write( tp, 0xF83A, 0xBE00 );
                mac_ocp_write( tp, 0xF83C, 0xA000 );
                mac_ocp_write( tp, 0xF83E, 0xC602 );
                mac_ocp_write( tp, 0xF840, 0xBE00 );
                mac_ocp_write( tp, 0xF842, 0x0000 );
                mac_ocp_write( tp, 0xF844, 0x1B76 );
                mac_ocp_write( tp, 0xF846, 0xC202 );
                mac_ocp_write( tp, 0xF848, 0xBA00 );
                mac_ocp_write( tp, 0xF84A, 0x059C );
                mac_ocp_write( tp, 0xF84C, 0x1B76 );
                mac_ocp_write( tp, 0xF84E, 0xC602 );
                mac_ocp_write( tp, 0xF850, 0xBE00 );
                mac_ocp_write( tp, 0xF852, 0x065A );
                mac_ocp_write( tp, 0xF854, 0x74E6 );
                mac_ocp_write( tp, 0xF856, 0x1B78 );
                mac_ocp_write( tp, 0xF858, 0x46DC );
                mac_ocp_write( tp, 0xF85A, 0x1300 );
                mac_ocp_write( tp, 0xF85C, 0xF005 );
                mac_ocp_write( tp, 0xF85E, 0x74F8 );
                mac_ocp_write( tp, 0xF860, 0x48C3 );
                mac_ocp_write( tp, 0xF862, 0x48C4 );
                mac_ocp_write( tp, 0xF864, 0x8CF8 );
                mac_ocp_write( tp, 0xF866, 0x64E7 );
                mac_ocp_write( tp, 0xF868, 0xC302 );
                mac_ocp_write( tp, 0xF86A, 0xBB00 );
                mac_ocp_write( tp, 0xF86C, 0x06A0 );
                mac_ocp_write( tp, 0xF86E, 0x74E4 );
                mac_ocp_write( tp, 0xF870, 0x49C5 );
                mac_ocp_write( tp, 0xF872, 0xF106 );
                mac_ocp_write( tp, 0xF874, 0x49C6 );
                mac_ocp_write( tp, 0xF876, 0xF107 );
                mac_ocp_write( tp, 0xF878, 0x48C8 );
                mac_ocp_write( tp, 0xF87A, 0x48C9 );
                mac_ocp_write( tp, 0xF87C, 0xE011 );
                mac_ocp_write( tp, 0xF87E, 0x48C9 );
                mac_ocp_write( tp, 0xF880, 0x4848 );
                mac_ocp_write( tp, 0xF882, 0xE00E );
                mac_ocp_write( tp, 0xF884, 0x4848 );
                mac_ocp_write( tp, 0xF886, 0x49C7 );
                mac_ocp_write( tp, 0xF888, 0xF00A );
                mac_ocp_write( tp, 0xF88A, 0x48C9 );
                mac_ocp_write( tp, 0xF88C, 0xC60D );
                mac_ocp_write( tp, 0xF88E, 0x1D1F );
                mac_ocp_write( tp, 0xF890, 0x8DC2 );
                mac_ocp_write( tp, 0xF892, 0x1D00 );
                mac_ocp_write( tp, 0xF894, 0x8DC3 );
                mac_ocp_write( tp, 0xF896, 0x1D11 );
                mac_ocp_write( tp, 0xF898, 0x8DC0 );
                mac_ocp_write( tp, 0xF89A, 0xE002 );
                mac_ocp_write( tp, 0xF89C, 0x4849 );
                mac_ocp_write( tp, 0xF89E, 0x94E5 );
                mac_ocp_write( tp, 0xF8A0, 0xC602 );
                mac_ocp_write( tp, 0xF8A2, 0xBE00 );
                mac_ocp_write( tp, 0xF8A4, 0x01F0 );
                mac_ocp_write( tp, 0xF8A6, 0xE434 );
                mac_ocp_write( tp, 0xF8A8, 0x49D9 );
                mac_ocp_write( tp, 0xF8AA, 0xF01B );
                mac_ocp_write( tp, 0xF8AC, 0xC31E );
                mac_ocp_write( tp, 0xF8AE, 0x7464 );
                mac_ocp_write( tp, 0xF8B0, 0x49C4 );
                mac_ocp_write( tp, 0xF8B2, 0xF114 );
                mac_ocp_write( tp, 0xF8B4, 0xC31B );
                mac_ocp_write( tp, 0xF8B6, 0x6460 );
                mac_ocp_write( tp, 0xF8B8, 0x14FA );
                mac_ocp_write( tp, 0xF8BA, 0xFA02 );
                mac_ocp_write( tp, 0xF8BC, 0xE00F );
                mac_ocp_write( tp, 0xF8BE, 0xC317 );
                mac_ocp_write( tp, 0xF8C0, 0x7460 );
                mac_ocp_write( tp, 0xF8C2, 0x49C0 );
                mac_ocp_write( tp, 0xF8C4, 0xF10B );
                mac_ocp_write( tp, 0xF8C6, 0xC311 );
                mac_ocp_write( tp, 0xF8C8, 0x7462 );
                mac_ocp_write( tp, 0xF8CA, 0x48C1 );
                mac_ocp_write( tp, 0xF8CC, 0x9C62 );
                mac_ocp_write( tp, 0xF8CE, 0x4841 );
                mac_ocp_write( tp, 0xF8D0, 0x9C62 );
                mac_ocp_write( tp, 0xF8D2, 0xC30A );
                mac_ocp_write( tp, 0xF8D4, 0x1C04 );
                mac_ocp_write( tp, 0xF8D6, 0x8C60 );
                mac_ocp_write( tp, 0xF8D8, 0xE004 );
                mac_ocp_write( tp, 0xF8DA, 0x1C15 );
                mac_ocp_write( tp, 0xF8DC, 0xC305 );
                mac_ocp_write( tp, 0xF8DE, 0x8C60 );
                mac_ocp_write( tp, 0xF8E0, 0xC602 );
                mac_ocp_write( tp, 0xF8E2, 0xBE00 );
                mac_ocp_write( tp, 0xF8E4, 0x0384 );
                mac_ocp_write( tp, 0xF8E6, 0xE434 );
                mac_ocp_write( tp, 0xF8E8, 0xE030 );
                mac_ocp_write( tp, 0xF8EA, 0xE61C );
                mac_ocp_write( tp, 0xF8EC, 0xE906 );

                mac_ocp_write( tp, 0xFC26, 0x8000 );

                mac_ocp_write( tp, 0xFC28, 0x0075 );
                mac_ocp_write( tp, 0xFC2E, 0x059B );
                mac_ocp_write( tp, 0xFC30, 0x0659 );
                mac_ocp_write( tp, 0xFC32, 0x0000 );
                mac_ocp_write( tp, 0xFC34, 0x0000 );
                mac_ocp_write( tp, 0xFC36, 0x0000 );
        } else if (tp->mcfg == CFG_METHOD_24) {
                rtl8168_hw_disable_mac_mcu_bps(dev);

                mac_ocp_write( tp, 0xF800, 0xE008 );
                mac_ocp_write( tp, 0xF802, 0xE011 );
                mac_ocp_write( tp, 0xF804, 0xE015 );
                mac_ocp_write( tp, 0xF806, 0xE018 );
                mac_ocp_write( tp, 0xF808, 0xE01B );
                mac_ocp_write( tp, 0xF80A, 0xE027 );
                mac_ocp_write( tp, 0xF80C, 0xE043 );
                mac_ocp_write( tp, 0xF80E, 0xE065 );
                mac_ocp_write( tp, 0xF810, 0x49E2 );
                mac_ocp_write( tp, 0xF812, 0xF005 );
                mac_ocp_write( tp, 0xF814, 0x49EA );
                mac_ocp_write( tp, 0xF816, 0xF003 );
                mac_ocp_write( tp, 0xF818, 0xC404 );
                mac_ocp_write( tp, 0xF81A, 0xBC00 );
                mac_ocp_write( tp, 0xF81C, 0xC403 );
                mac_ocp_write( tp, 0xF81E, 0xBC00 );
                mac_ocp_write( tp, 0xF820, 0x0496 );
                mac_ocp_write( tp, 0xF822, 0x051A );
                mac_ocp_write( tp, 0xF824, 0x1D01 );
                mac_ocp_write( tp, 0xF826, 0x8DE8 );
                mac_ocp_write( tp, 0xF828, 0xC602 );
                mac_ocp_write( tp, 0xF82A, 0xBE00 );
                mac_ocp_write( tp, 0xF82C, 0x0206 );
                mac_ocp_write( tp, 0xF82E, 0x1B76 );
                mac_ocp_write( tp, 0xF830, 0xC202 );
                mac_ocp_write( tp, 0xF832, 0xBA00 );
                mac_ocp_write( tp, 0xF834, 0x058A );
                mac_ocp_write( tp, 0xF836, 0x1B76 );
                mac_ocp_write( tp, 0xF838, 0xC602 );
                mac_ocp_write( tp, 0xF83A, 0xBE00 );
                mac_ocp_write( tp, 0xF83C, 0x0648 );
                mac_ocp_write( tp, 0xF83E, 0x74E6 );
                mac_ocp_write( tp, 0xF840, 0x1B78 );
                mac_ocp_write( tp, 0xF842, 0x46DC );
                mac_ocp_write( tp, 0xF844, 0x1300 );
                mac_ocp_write( tp, 0xF846, 0xF005 );
                mac_ocp_write( tp, 0xF848, 0x74F8 );
                mac_ocp_write( tp, 0xF84A, 0x48C3 );
                mac_ocp_write( tp, 0xF84C, 0x48C4 );
                mac_ocp_write( tp, 0xF84E, 0x8CF8 );
                mac_ocp_write( tp, 0xF850, 0x64E7 );
                mac_ocp_write( tp, 0xF852, 0xC302 );
                mac_ocp_write( tp, 0xF854, 0xBB00 );
                mac_ocp_write( tp, 0xF856, 0x068E );
                mac_ocp_write( tp, 0xF858, 0x74E4 );
                mac_ocp_write( tp, 0xF85A, 0x49C5 );
                mac_ocp_write( tp, 0xF85C, 0xF106 );
                mac_ocp_write( tp, 0xF85E, 0x49C6 );
                mac_ocp_write( tp, 0xF860, 0xF107 );
                mac_ocp_write( tp, 0xF862, 0x48C8 );
                mac_ocp_write( tp, 0xF864, 0x48C9 );
                mac_ocp_write( tp, 0xF866, 0xE011 );
                mac_ocp_write( tp, 0xF868, 0x48C9 );
                mac_ocp_write( tp, 0xF86A, 0x4848 );
                mac_ocp_write( tp, 0xF86C, 0xE00E );
                mac_ocp_write( tp, 0xF86E, 0x4848 );
                mac_ocp_write( tp, 0xF870, 0x49C7 );
                mac_ocp_write( tp, 0xF872, 0xF00A );
                mac_ocp_write( tp, 0xF874, 0x48C9 );
                mac_ocp_write( tp, 0xF876, 0xC60D );
                mac_ocp_write( tp, 0xF878, 0x1D1F );
                mac_ocp_write( tp, 0xF87A, 0x8DC2 );
                mac_ocp_write( tp, 0xF87C, 0x1D00 );
                mac_ocp_write( tp, 0xF87E, 0x8DC3 );
                mac_ocp_write( tp, 0xF880, 0x1D11 );
                mac_ocp_write( tp, 0xF882, 0x8DC0 );
                mac_ocp_write( tp, 0xF884, 0xE002 );
                mac_ocp_write( tp, 0xF886, 0x4849 );
                mac_ocp_write( tp, 0xF888, 0x94E5 );
                mac_ocp_write( tp, 0xF88A, 0xC602 );
                mac_ocp_write( tp, 0xF88C, 0xBE00 );
                mac_ocp_write( tp, 0xF88E, 0x0238 );
                mac_ocp_write( tp, 0xF890, 0xE434 );
                mac_ocp_write( tp, 0xF892, 0x49D9 );
                mac_ocp_write( tp, 0xF894, 0xF01B );
                mac_ocp_write( tp, 0xF896, 0xC31E );
                mac_ocp_write( tp, 0xF898, 0x7464 );
                mac_ocp_write( tp, 0xF89A, 0x49C4 );
                mac_ocp_write( tp, 0xF89C, 0xF114 );
                mac_ocp_write( tp, 0xF89E, 0xC31B );
                mac_ocp_write( tp, 0xF8A0, 0x6460 );
                mac_ocp_write( tp, 0xF8A2, 0x14FA );
                mac_ocp_write( tp, 0xF8A4, 0xFA02 );
                mac_ocp_write( tp, 0xF8A6, 0xE00F );
                mac_ocp_write( tp, 0xF8A8, 0xC317 );
                mac_ocp_write( tp, 0xF8AA, 0x7460 );
                mac_ocp_write( tp, 0xF8AC, 0x49C0 );
                mac_ocp_write( tp, 0xF8AE, 0xF10B );
                mac_ocp_write( tp, 0xF8B0, 0xC311 );
                mac_ocp_write( tp, 0xF8B2, 0x7462 );
                mac_ocp_write( tp, 0xF8B4, 0x48C1 );
                mac_ocp_write( tp, 0xF8B6, 0x9C62 );
                mac_ocp_write( tp, 0xF8B8, 0x4841 );
                mac_ocp_write( tp, 0xF8BA, 0x9C62 );
                mac_ocp_write( tp, 0xF8BC, 0xC30A );
                mac_ocp_write( tp, 0xF8BE, 0x1C04 );
                mac_ocp_write( tp, 0xF8C0, 0x8C60 );
                mac_ocp_write( tp, 0xF8C2, 0xE004 );
                mac_ocp_write( tp, 0xF8C4, 0x1C15 );
                mac_ocp_write( tp, 0xF8C6, 0xC305 );
                mac_ocp_write( tp, 0xF8C8, 0x8C60 );
                mac_ocp_write( tp, 0xF8CA, 0xC602 );
                mac_ocp_write( tp, 0xF8CC, 0xBE00 );
                mac_ocp_write( tp, 0xF8CE, 0x0374 );
                mac_ocp_write( tp, 0xF8D0, 0xE434 );
                mac_ocp_write( tp, 0xF8D2, 0xE030 );
                mac_ocp_write( tp, 0xF8D4, 0xE61C );
                mac_ocp_write( tp, 0xF8D6, 0xE906 );
                mac_ocp_write( tp, 0xF8D8, 0xC602 );
                mac_ocp_write( tp, 0xF8DA, 0xBE00 );
                mac_ocp_write( tp, 0xF8DC, 0x0000 );

                mac_ocp_write( tp, 0xFC26, 0x8000 );

                mac_ocp_write( tp, 0xFC28, 0x0493 );
                mac_ocp_write( tp, 0xFC2A, 0x0205 );
                mac_ocp_write( tp, 0xFC2C, 0x0589 );
                mac_ocp_write( tp, 0xFC2E, 0x0647 );
                mac_ocp_write( tp, 0xFC30, 0x0000 );
                mac_ocp_write( tp, 0xFC32, 0x0215 );
                mac_ocp_write( tp, 0xFC34, 0x0285 );
        } else if (tp->mcfg == CFG_METHOD_25) {
                rtl8168_hw_disable_mac_mcu_bps(dev);

                mac_ocp_write( tp,  0xF800, 0xE008 );
                mac_ocp_write( tp,  0xF802, 0xE00A );
                mac_ocp_write( tp,  0xF804, 0xE01D );
                mac_ocp_write( tp,  0xF806, 0xE033 );
                mac_ocp_write( tp,  0xF808, 0xE042 );
                mac_ocp_write( tp,  0xF80A, 0xE044 );
                mac_ocp_write( tp,  0xF80C, 0xE046 );
                mac_ocp_write( tp,  0xF80E, 0xE048 );
                mac_ocp_write( tp,  0xF810, 0xC602 );
                mac_ocp_write( tp,  0xF812, 0xBE00 );
                mac_ocp_write( tp,  0xF814, 0x0000 );
                mac_ocp_write( tp,  0xF816, 0xC513 );
                mac_ocp_write( tp,  0xF818, 0x64A0 );
                mac_ocp_write( tp,  0xF81A, 0x49C1 );
                mac_ocp_write( tp,  0xF81C, 0xF00A );
                mac_ocp_write( tp,  0xF81E, 0x1CEA );
                mac_ocp_write( tp,  0xF820, 0x2242 );
                mac_ocp_write( tp,  0xF822, 0x0402 );
                mac_ocp_write( tp,  0xF824, 0xC50B );
                mac_ocp_write( tp,  0xF826, 0x9CA2 );
                mac_ocp_write( tp,  0xF828, 0x1C11 );
                mac_ocp_write( tp,  0xF82A, 0x9CA0 );
                mac_ocp_write( tp,  0xF82C, 0xC506 );
                mac_ocp_write( tp,  0xF82E, 0xBD00 );
                mac_ocp_write( tp,  0xF830, 0x7444 );
                mac_ocp_write( tp,  0xF832, 0xC502 );
                mac_ocp_write( tp,  0xF834, 0xBD00 );
                mac_ocp_write( tp,  0xF836, 0x0A30 );
                mac_ocp_write( tp,  0xF838, 0x0A46 );
                mac_ocp_write( tp,  0xF83A, 0xE434 );
                mac_ocp_write( tp,  0xF83C, 0xE096 );
                mac_ocp_write( tp,  0xF83E, 0x49D9 );
                mac_ocp_write( tp,  0xF840, 0xF00F );
                mac_ocp_write( tp,  0xF842, 0xC512 );
                mac_ocp_write( tp,  0xF844, 0x74A0 );
                mac_ocp_write( tp,  0xF846, 0x48C8 );
                mac_ocp_write( tp,  0xF848, 0x48CA );
                mac_ocp_write( tp,  0xF84A, 0x9CA0 );
                mac_ocp_write( tp,  0xF84C, 0xC50F );
                mac_ocp_write( tp,  0xF84E, 0x1B00 );
                mac_ocp_write( tp,  0xF850, 0x9BA0 );
                mac_ocp_write( tp,  0xF852, 0x1B1C );
                mac_ocp_write( tp,  0xF854, 0x483F );
                mac_ocp_write( tp,  0xF856, 0x9BA2 );
                mac_ocp_write( tp,  0xF858, 0x1B04 );
                mac_ocp_write( tp,  0xF85A, 0xC5F0 );
                mac_ocp_write( tp,  0xF85C, 0x9BA0 );
                mac_ocp_write( tp,  0xF85E, 0xC602 );
                mac_ocp_write( tp,  0xF860, 0xBE00 );
                mac_ocp_write( tp,  0xF862, 0x03DE );
                mac_ocp_write( tp,  0xF864, 0xE434 );
                mac_ocp_write( tp,  0xF866, 0xE096 );
                mac_ocp_write( tp,  0xF868, 0xE860 );
                mac_ocp_write( tp,  0xF86A, 0xDE20 );
                mac_ocp_write( tp,  0xF86C, 0xC50F );
                mac_ocp_write( tp,  0xF86E, 0x76A4 );
                mac_ocp_write( tp,  0xF870, 0x49E3 );
                mac_ocp_write( tp,  0xF872, 0xF007 );
                mac_ocp_write( tp,  0xF874, 0x49C0 );
                mac_ocp_write( tp,  0xF876, 0xF103 );
                mac_ocp_write( tp,  0xF878, 0xC607 );
                mac_ocp_write( tp,  0xF87A, 0xBE00 );
                mac_ocp_write( tp,  0xF87C, 0xC606 );
                mac_ocp_write( tp,  0xF87E, 0xBE00 );
                mac_ocp_write( tp,  0xF880, 0xC602 );
                mac_ocp_write( tp,  0xF882, 0xBE00 );
                mac_ocp_write( tp,  0xF884, 0x0A88 );
                mac_ocp_write( tp,  0xF886, 0x0A64 );
                mac_ocp_write( tp,  0xF888, 0x0A68 );
                mac_ocp_write( tp,  0xF88A, 0xDC00 );
                mac_ocp_write( tp,  0xF88C, 0xC602 );
                mac_ocp_write( tp,  0xF88E, 0xBE00 );
                mac_ocp_write( tp,  0xF890, 0x0000 );
                mac_ocp_write( tp,  0xF892, 0xC602 );
                mac_ocp_write( tp,  0xF894, 0xBE00 );
                mac_ocp_write( tp,  0xF896, 0x0000 );
                mac_ocp_write( tp,  0xF898, 0xC602 );
                mac_ocp_write( tp,  0xF89A, 0xBE00 );
                mac_ocp_write( tp,  0xF89C, 0x0000 );
                mac_ocp_write( tp,  0xF89E, 0xC602 );
                mac_ocp_write( tp,  0xF8A0, 0xBE00 );
                mac_ocp_write( tp,  0xF8A2, 0x0000 );

                mac_ocp_write( tp, 0xFC26, 0x8000 );

                mac_ocp_write( tp, 0xFC2A, 0x0A2F );
                mac_ocp_write( tp, 0xFC2C, 0x0297 );
                mac_ocp_write( tp, 0xFC2E, 0x0A61 );
        } else if (tp->mcfg == CFG_METHOD_26) {
                rtl8168_hw_disable_mac_mcu_bps(dev);

                mac_ocp_write( tp, 0xF800, 0xE008 );
                mac_ocp_write( tp, 0xF802, 0xE00A );
                mac_ocp_write( tp, 0xF804, 0xE00C );
                mac_ocp_write( tp, 0xF806, 0xE00E );
                mac_ocp_write( tp, 0xF808, 0xE027 );
                mac_ocp_write( tp, 0xF80A, 0xE04F );
                mac_ocp_write( tp, 0xF80C, 0xE05E );
                mac_ocp_write( tp, 0xF80E, 0xE065 );
                mac_ocp_write( tp, 0xF810, 0xC602 );
                mac_ocp_write( tp, 0xF812, 0xBE00 );
                mac_ocp_write( tp, 0xF814, 0x0000 );
                mac_ocp_write( tp, 0xF816, 0xC502 );
                mac_ocp_write( tp, 0xF818, 0xBD00 );
                mac_ocp_write( tp, 0xF81A, 0x074C );
                mac_ocp_write( tp, 0xF81C, 0xC302 );
                mac_ocp_write( tp, 0xF81E, 0xBB00 );
                mac_ocp_write( tp, 0xF820, 0x080A );
                mac_ocp_write( tp, 0xF822, 0x6420 );
                mac_ocp_write( tp, 0xF824, 0x48C2 );
                mac_ocp_write( tp, 0xF826, 0x8C20 );
                mac_ocp_write( tp, 0xF828, 0xC516 );
                mac_ocp_write( tp, 0xF82A, 0x64A4 );
                mac_ocp_write( tp, 0xF82C, 0x49C0 );
                mac_ocp_write( tp, 0xF82E, 0xF009 );
                mac_ocp_write( tp, 0xF830, 0x74A2 );
                mac_ocp_write( tp, 0xF832, 0x8CA5 );
                mac_ocp_write( tp, 0xF834, 0x74A0 );
                mac_ocp_write( tp, 0xF836, 0xC50E );
                mac_ocp_write( tp, 0xF838, 0x9CA2 );
                mac_ocp_write( tp, 0xF83A, 0x1C11 );
                mac_ocp_write( tp, 0xF83C, 0x9CA0 );
                mac_ocp_write( tp, 0xF83E, 0xE006 );
                mac_ocp_write( tp, 0xF840, 0x74F8 );
                mac_ocp_write( tp, 0xF842, 0x48C4 );
                mac_ocp_write( tp, 0xF844, 0x8CF8 );
                mac_ocp_write( tp, 0xF846, 0xC404 );
                mac_ocp_write( tp, 0xF848, 0xBC00 );
                mac_ocp_write( tp, 0xF84A, 0xC403 );
                mac_ocp_write( tp, 0xF84C, 0xBC00 );
                mac_ocp_write( tp, 0xF84E, 0x0BF2 );
                mac_ocp_write( tp, 0xF850, 0x0C0A );
                mac_ocp_write( tp, 0xF852, 0xE434 );
                mac_ocp_write( tp, 0xF854, 0xD3C0 );
                mac_ocp_write( tp, 0xF856, 0x49D9 );
                mac_ocp_write( tp, 0xF858, 0xF01F );
                mac_ocp_write( tp, 0xF85A, 0xC526 );
                mac_ocp_write( tp, 0xF85C, 0x64A5 );
                mac_ocp_write( tp, 0xF85E, 0x1400 );
                mac_ocp_write( tp, 0xF860, 0xF007 );
                mac_ocp_write( tp, 0xF862, 0x0C01 );
                mac_ocp_write( tp, 0xF864, 0x8CA5 );
                mac_ocp_write( tp, 0xF866, 0x1C15 );
                mac_ocp_write( tp, 0xF868, 0xC51B );
                mac_ocp_write( tp, 0xF86A, 0x9CA0 );
                mac_ocp_write( tp, 0xF86C, 0xE013 );
                mac_ocp_write( tp, 0xF86E, 0xC519 );
                mac_ocp_write( tp, 0xF870, 0x74A0 );
                mac_ocp_write( tp, 0xF872, 0x48C4 );
                mac_ocp_write( tp, 0xF874, 0x8CA0 );
                mac_ocp_write( tp, 0xF876, 0xC516 );
                mac_ocp_write( tp, 0xF878, 0x74A4 );
                mac_ocp_write( tp, 0xF87A, 0x48C8 );
                mac_ocp_write( tp, 0xF87C, 0x48CA );
                mac_ocp_write( tp, 0xF87E, 0x9CA4 );
                mac_ocp_write( tp, 0xF880, 0xC512 );
                mac_ocp_write( tp, 0xF882, 0x1B00 );
                mac_ocp_write( tp, 0xF884, 0x9BA0 );
                mac_ocp_write( tp, 0xF886, 0x1B1C );
                mac_ocp_write( tp, 0xF888, 0x483F );
                mac_ocp_write( tp, 0xF88A, 0x9BA2 );
                mac_ocp_write( tp, 0xF88C, 0x1B04 );
                mac_ocp_write( tp, 0xF88E, 0xC508 );
                mac_ocp_write( tp, 0xF890, 0x9BA0 );
                mac_ocp_write( tp, 0xF892, 0xC505 );
                mac_ocp_write( tp, 0xF894, 0xBD00 );
                mac_ocp_write( tp, 0xF896, 0xC502 );
                mac_ocp_write( tp, 0xF898, 0xBD00 );
                mac_ocp_write( tp, 0xF89A, 0x0300 );
                mac_ocp_write( tp, 0xF89C, 0x051E );
                mac_ocp_write( tp, 0xF89E, 0xE434 );
                mac_ocp_write( tp, 0xF8A0, 0xE018 );
                mac_ocp_write( tp, 0xF8A2, 0xE092 );
                mac_ocp_write( tp, 0xF8A4, 0xDE20 );
                mac_ocp_write( tp, 0xF8A6, 0xD3C0 );
                mac_ocp_write( tp, 0xF8A8, 0xC50F );
                mac_ocp_write( tp, 0xF8AA, 0x76A4 );
                mac_ocp_write( tp, 0xF8AC, 0x49E3 );
                mac_ocp_write( tp, 0xF8AE, 0xF007 );
                mac_ocp_write( tp, 0xF8B0, 0x49C0 );
                mac_ocp_write( tp, 0xF8B2, 0xF103 );
                mac_ocp_write( tp, 0xF8B4, 0xC607 );
                mac_ocp_write( tp, 0xF8B6, 0xBE00 );
                mac_ocp_write( tp, 0xF8B8, 0xC606 );
                mac_ocp_write( tp, 0xF8BA, 0xBE00 );
                mac_ocp_write( tp, 0xF8BC, 0xC602 );
                mac_ocp_write( tp, 0xF8BE, 0xBE00 );
                mac_ocp_write( tp, 0xF8C0, 0x0C4C );
                mac_ocp_write( tp, 0xF8C2, 0x0C28 );
                mac_ocp_write( tp, 0xF8C4, 0x0C2C );
                mac_ocp_write( tp, 0xF8C6, 0xDC00 );
                mac_ocp_write( tp, 0xF8C8, 0xC707 );
                mac_ocp_write( tp, 0xF8CA, 0x1D00 );
                mac_ocp_write( tp, 0xF8CC, 0x8DE2 );
                mac_ocp_write( tp, 0xF8CE, 0x48C1 );
                mac_ocp_write( tp, 0xF8D0, 0xC502 );
                mac_ocp_write( tp, 0xF8D2, 0xBD00 );
                mac_ocp_write( tp, 0xF8D4, 0x00AA );
                mac_ocp_write( tp, 0xF8D6, 0xE0C0 );
                mac_ocp_write( tp, 0xF8D8, 0xC502 );
                mac_ocp_write( tp, 0xF8DA, 0xBD00 );
                mac_ocp_write( tp, 0xF8DC, 0x0132 );

                mac_ocp_write( tp, 0xFC26, 0x8000 );

                mac_ocp_write( tp, 0xFC2A, 0x0743 );
                mac_ocp_write( tp, 0xFC2C, 0x0801 );
                mac_ocp_write( tp, 0xFC2E, 0x0BE9 );
                mac_ocp_write( tp, 0xFC30, 0x02FD );
                mac_ocp_write( tp, 0xFC32, 0x0C25 );
                mac_ocp_write( tp, 0xFC34, 0x00A9 );
                mac_ocp_write( tp, 0xFC36, 0x012D );
        } else if (tp->mcfg == CFG_METHOD_27) {
                rtl8168_hw_disable_mac_mcu_bps(dev);

                mac_ocp_write( tp, 0xF800, 0xE008 );
                mac_ocp_write( tp, 0xF802, 0xE0D3 );
                mac_ocp_write( tp, 0xF804, 0xE0D6 );
                mac_ocp_write( tp, 0xF806, 0xE0D9 );
                mac_ocp_write( tp, 0xF808, 0xE0DB );
                mac_ocp_write( tp, 0xF80A, 0xE0DD );
                mac_ocp_write( tp, 0xF80C, 0xE0DF );
                mac_ocp_write( tp, 0xF80E, 0xE0E1 );
                mac_ocp_write( tp, 0xF810, 0xC251 );
                mac_ocp_write( tp, 0xF812, 0x7340 );
                mac_ocp_write( tp, 0xF814, 0x49B1 );
                mac_ocp_write( tp, 0xF816, 0xF010 );
                mac_ocp_write( tp, 0xF818, 0x1D02 );
                mac_ocp_write( tp, 0xF81A, 0x8D40 );
                mac_ocp_write( tp, 0xF81C, 0xC202 );
                mac_ocp_write( tp, 0xF81E, 0xBA00 );
                mac_ocp_write( tp, 0xF820, 0x2C3A );
                mac_ocp_write( tp, 0xF822, 0xC0F0 );
                mac_ocp_write( tp, 0xF824, 0xE8DE );
                mac_ocp_write( tp, 0xF826, 0x2000 );
                mac_ocp_write( tp, 0xF828, 0x8000 );
                mac_ocp_write( tp, 0xF82A, 0xC0B6 );
                mac_ocp_write( tp, 0xF82C, 0x268C );
                mac_ocp_write( tp, 0xF82E, 0x752C );
                mac_ocp_write( tp, 0xF830, 0x49D4 );
                mac_ocp_write( tp, 0xF832, 0xF112 );
                mac_ocp_write( tp, 0xF834, 0xE025 );
                mac_ocp_write( tp, 0xF836, 0xC2F6 );
                mac_ocp_write( tp, 0xF838, 0x7146 );
                mac_ocp_write( tp, 0xF83A, 0xC2F5 );
                mac_ocp_write( tp, 0xF83C, 0x7340 );
                mac_ocp_write( tp, 0xF83E, 0x49BE );
                mac_ocp_write( tp, 0xF840, 0xF103 );
                mac_ocp_write( tp, 0xF842, 0xC7F2 );
                mac_ocp_write( tp, 0xF844, 0xE002 );
                mac_ocp_write( tp, 0xF846, 0xC7F1 );
                mac_ocp_write( tp, 0xF848, 0x304F );
                mac_ocp_write( tp, 0xF84A, 0x6226 );
                mac_ocp_write( tp, 0xF84C, 0x49A1 );
                mac_ocp_write( tp, 0xF84E, 0xF1F0 );
                mac_ocp_write( tp, 0xF850, 0x7222 );
                mac_ocp_write( tp, 0xF852, 0x49A0 );
                mac_ocp_write( tp, 0xF854, 0xF1ED );
                mac_ocp_write( tp, 0xF856, 0x2525 );
                mac_ocp_write( tp, 0xF858, 0x1F28 );
                mac_ocp_write( tp, 0xF85A, 0x3097 );
                mac_ocp_write( tp, 0xF85C, 0x3091 );
                mac_ocp_write( tp, 0xF85E, 0x9A36 );
                mac_ocp_write( tp, 0xF860, 0x752C );
                mac_ocp_write( tp, 0xF862, 0x21DC );
                mac_ocp_write( tp, 0xF864, 0x25BC );
                mac_ocp_write( tp, 0xF866, 0xC6E2 );
                mac_ocp_write( tp, 0xF868, 0x77C0 );
                mac_ocp_write( tp, 0xF86A, 0x1304 );
                mac_ocp_write( tp, 0xF86C, 0xF014 );
                mac_ocp_write( tp, 0xF86E, 0x1303 );
                mac_ocp_write( tp, 0xF870, 0xF014 );
                mac_ocp_write( tp, 0xF872, 0x1302 );
                mac_ocp_write( tp, 0xF874, 0xF014 );
                mac_ocp_write( tp, 0xF876, 0x1301 );
                mac_ocp_write( tp, 0xF878, 0xF014 );
                mac_ocp_write( tp, 0xF87A, 0x49D4 );
                mac_ocp_write( tp, 0xF87C, 0xF103 );
                mac_ocp_write( tp, 0xF87E, 0xC3D7 );
                mac_ocp_write( tp, 0xF880, 0xBB00 );
                mac_ocp_write( tp, 0xF882, 0xC618 );
                mac_ocp_write( tp, 0xF884, 0x67C6 );
                mac_ocp_write( tp, 0xF886, 0x752E );
                mac_ocp_write( tp, 0xF888, 0x22D7 );
                mac_ocp_write( tp, 0xF88A, 0x26DD );
                mac_ocp_write( tp, 0xF88C, 0x1505 );
                mac_ocp_write( tp, 0xF88E, 0xF013 );
                mac_ocp_write( tp, 0xF890, 0xC60A );
                mac_ocp_write( tp, 0xF892, 0xBE00 );
                mac_ocp_write( tp, 0xF894, 0xC309 );
                mac_ocp_write( tp, 0xF896, 0xBB00 );
                mac_ocp_write( tp, 0xF898, 0xC308 );
                mac_ocp_write( tp, 0xF89A, 0xBB00 );
                mac_ocp_write( tp, 0xF89C, 0xC307 );
                mac_ocp_write( tp, 0xF89E, 0xBB00 );
                mac_ocp_write( tp, 0xF8A0, 0xC306 );
                mac_ocp_write( tp, 0xF8A2, 0xBB00 );
                mac_ocp_write( tp, 0xF8A4, 0x25C8 );
                mac_ocp_write( tp, 0xF8A6, 0x25A6 );
                mac_ocp_write( tp, 0xF8A8, 0x25AC );
                mac_ocp_write( tp, 0xF8AA, 0x25B2 );
                mac_ocp_write( tp, 0xF8AC, 0x25B8 );
                mac_ocp_write( tp, 0xF8AE, 0xCD08 );
                mac_ocp_write( tp, 0xF8B0, 0x0000 );
                mac_ocp_write( tp, 0xF8B2, 0xC0BC );
                mac_ocp_write( tp, 0xF8B4, 0xC2FF );
                mac_ocp_write( tp, 0xF8B6, 0x7340 );
                mac_ocp_write( tp, 0xF8B8, 0x49B0 );
                mac_ocp_write( tp, 0xF8BA, 0xF04E );
                mac_ocp_write( tp, 0xF8BC, 0x1F46 );
                mac_ocp_write( tp, 0xF8BE, 0x308F );
                mac_ocp_write( tp, 0xF8C0, 0xC3F7 );
                mac_ocp_write( tp, 0xF8C2, 0x1C04 );
                mac_ocp_write( tp, 0xF8C4, 0xE84D );
                mac_ocp_write( tp, 0xF8C6, 0x1401 );
                mac_ocp_write( tp, 0xF8C8, 0xF147 );
                mac_ocp_write( tp, 0xF8CA, 0x7226 );
                mac_ocp_write( tp, 0xF8CC, 0x49A7 );
                mac_ocp_write( tp, 0xF8CE, 0xF044 );
                mac_ocp_write( tp, 0xF8D0, 0x7222 );
                mac_ocp_write( tp, 0xF8D2, 0x2525 );
                mac_ocp_write( tp, 0xF8D4, 0x1F30 );
                mac_ocp_write( tp, 0xF8D6, 0x3097 );
                mac_ocp_write( tp, 0xF8D8, 0x3091 );
                mac_ocp_write( tp, 0xF8DA, 0x7340 );
                mac_ocp_write( tp, 0xF8DC, 0xC4EA );
                mac_ocp_write( tp, 0xF8DE, 0x401C );
                mac_ocp_write( tp, 0xF8E0, 0xF006 );
                mac_ocp_write( tp, 0xF8E2, 0xC6E8 );
                mac_ocp_write( tp, 0xF8E4, 0x75C0 );
                mac_ocp_write( tp, 0xF8E6, 0x49D7 );
                mac_ocp_write( tp, 0xF8E8, 0xF105 );
                mac_ocp_write( tp, 0xF8EA, 0xE036 );
                mac_ocp_write( tp, 0xF8EC, 0x1D08 );
                mac_ocp_write( tp, 0xF8EE, 0x8DC1 );
                mac_ocp_write( tp, 0xF8F0, 0x0208 );
                mac_ocp_write( tp, 0xF8F2, 0x6640 );
                mac_ocp_write( tp, 0xF8F4, 0x2764 );
                mac_ocp_write( tp, 0xF8F6, 0x1606 );
                mac_ocp_write( tp, 0xF8F8, 0xF12F );
                mac_ocp_write( tp, 0xF8FA, 0x6346 );
                mac_ocp_write( tp, 0xF8FC, 0x133B );
                mac_ocp_write( tp, 0xF8FE, 0xF12C );
                mac_ocp_write( tp, 0xF900, 0x9B34 );
                mac_ocp_write( tp, 0xF902, 0x1B18 );
                mac_ocp_write( tp, 0xF904, 0x3093 );
                mac_ocp_write( tp, 0xF906, 0xC32A );
                mac_ocp_write( tp, 0xF908, 0x1C10 );
                mac_ocp_write( tp, 0xF90A, 0xE82A );
                mac_ocp_write( tp, 0xF90C, 0x1401 );
                mac_ocp_write( tp, 0xF90E, 0xF124 );
                mac_ocp_write( tp, 0xF910, 0x1A36 );
                mac_ocp_write( tp, 0xF912, 0x308A );
                mac_ocp_write( tp, 0xF914, 0x7322 );
                mac_ocp_write( tp, 0xF916, 0x25B5 );
                mac_ocp_write( tp, 0xF918, 0x0B0E );
                mac_ocp_write( tp, 0xF91A, 0x1C00 );
                mac_ocp_write( tp, 0xF91C, 0xE82C );
                mac_ocp_write( tp, 0xF91E, 0xC71F );
                mac_ocp_write( tp, 0xF920, 0x4027 );
                mac_ocp_write( tp, 0xF922, 0xF11A );
                mac_ocp_write( tp, 0xF924, 0xE838 );
                mac_ocp_write( tp, 0xF926, 0x1F42 );
                mac_ocp_write( tp, 0xF928, 0x308F );
                mac_ocp_write( tp, 0xF92A, 0x1B08 );
                mac_ocp_write( tp, 0xF92C, 0xE824 );
                mac_ocp_write( tp, 0xF92E, 0x7236 );
                mac_ocp_write( tp, 0xF930, 0x7746 );
                mac_ocp_write( tp, 0xF932, 0x1700 );
                mac_ocp_write( tp, 0xF934, 0xF00D );
                mac_ocp_write( tp, 0xF936, 0xC313 );
                mac_ocp_write( tp, 0xF938, 0x401F );
                mac_ocp_write( tp, 0xF93A, 0xF103 );
                mac_ocp_write( tp, 0xF93C, 0x1F00 );
                mac_ocp_write( tp, 0xF93E, 0x9F46 );
                mac_ocp_write( tp, 0xF940, 0x7744 );
                mac_ocp_write( tp, 0xF942, 0x449F );
                mac_ocp_write( tp, 0xF944, 0x445F );
                mac_ocp_write( tp, 0xF946, 0xE817 );
                mac_ocp_write( tp, 0xF948, 0xC70A );
                mac_ocp_write( tp, 0xF94A, 0x4027 );
                mac_ocp_write( tp, 0xF94C, 0xF105 );
                mac_ocp_write( tp, 0xF94E, 0xC302 );
                mac_ocp_write( tp, 0xF950, 0xBB00 );
                mac_ocp_write( tp, 0xF952, 0x2E08 );
                mac_ocp_write( tp, 0xF954, 0x2DC2 );
                mac_ocp_write( tp, 0xF956, 0xC7FF );
                mac_ocp_write( tp, 0xF958, 0xBF00 );
                mac_ocp_write( tp, 0xF95A, 0xCDB8 );
                mac_ocp_write( tp, 0xF95C, 0xFFFF );
                mac_ocp_write( tp, 0xF95E, 0x0C02 );
                mac_ocp_write( tp, 0xF960, 0xA554 );
                mac_ocp_write( tp, 0xF962, 0xA5DC );
                mac_ocp_write( tp, 0xF964, 0x402F );
                mac_ocp_write( tp, 0xF966, 0xF105 );
                mac_ocp_write( tp, 0xF968, 0x1400 );
                mac_ocp_write( tp, 0xF96A, 0xF1FA );
                mac_ocp_write( tp, 0xF96C, 0x1C01 );
                mac_ocp_write( tp, 0xF96E, 0xE002 );
                mac_ocp_write( tp, 0xF970, 0x1C00 );
                mac_ocp_write( tp, 0xF972, 0xFF80 );
                mac_ocp_write( tp, 0xF974, 0x49B0 );
                mac_ocp_write( tp, 0xF976, 0xF004 );
                mac_ocp_write( tp, 0xF978, 0x0B01 );
                mac_ocp_write( tp, 0xF97A, 0xA1D3 );
                mac_ocp_write( tp, 0xF97C, 0xE003 );
                mac_ocp_write( tp, 0xF97E, 0x0B02 );
                mac_ocp_write( tp, 0xF980, 0xA5D3 );
                mac_ocp_write( tp, 0xF982, 0x3127 );
                mac_ocp_write( tp, 0xF984, 0x3720 );
                mac_ocp_write( tp, 0xF986, 0x0B02 );
                mac_ocp_write( tp, 0xF988, 0xA5D3 );
                mac_ocp_write( tp, 0xF98A, 0x3127 );
                mac_ocp_write( tp, 0xF98C, 0x3720 );
                mac_ocp_write( tp, 0xF98E, 0x1300 );
                mac_ocp_write( tp, 0xF990, 0xF1FB );
                mac_ocp_write( tp, 0xF992, 0xFF80 );
                mac_ocp_write( tp, 0xF994, 0x7322 );
                mac_ocp_write( tp, 0xF996, 0x25B5 );
                mac_ocp_write( tp, 0xF998, 0x1E28 );
                mac_ocp_write( tp, 0xF99A, 0x30DE );
                mac_ocp_write( tp, 0xF99C, 0x30D9 );
                mac_ocp_write( tp, 0xF99E, 0x7264 );
                mac_ocp_write( tp, 0xF9A0, 0x1E11 );
                mac_ocp_write( tp, 0xF9A2, 0x2368 );
                mac_ocp_write( tp, 0xF9A4, 0x3116 );
                mac_ocp_write( tp, 0xF9A6, 0xFF80 );
                mac_ocp_write( tp, 0xF9A8, 0x1B7E );
                mac_ocp_write( tp, 0xF9AA, 0xC602 );
                mac_ocp_write( tp, 0xF9AC, 0xBE00 );
                mac_ocp_write( tp, 0xF9AE, 0x06A6 );
                mac_ocp_write( tp, 0xF9B0, 0x1B7E );
                mac_ocp_write( tp, 0xF9B2, 0xC602 );
                mac_ocp_write( tp, 0xF9B4, 0xBE00 );
                mac_ocp_write( tp, 0xF9B6, 0x0764 );
                mac_ocp_write( tp, 0xF9B8, 0xC602 );
                mac_ocp_write( tp, 0xF9BA, 0xBE00 );
                mac_ocp_write( tp, 0xF9BC, 0x0000 );
                mac_ocp_write( tp, 0xF9BE, 0xC602 );
                mac_ocp_write( tp, 0xF9C0, 0xBE00 );
                mac_ocp_write( tp, 0xF9C2, 0x0000 );
                mac_ocp_write( tp, 0xF9C4, 0xC602 );
                mac_ocp_write( tp, 0xF9C6, 0xBE00 );
                mac_ocp_write( tp, 0xF9C8, 0x0000 );
                mac_ocp_write( tp, 0xF9CA, 0xC602 );
                mac_ocp_write( tp, 0xF9CC, 0xBE00 );
                mac_ocp_write( tp, 0xF9CE, 0x0000 );
                mac_ocp_write( tp, 0xF9D0, 0xC602 );
                mac_ocp_write( tp, 0xF9D2, 0xBE00 );
                mac_ocp_write( tp, 0xF9D4, 0x0000 );

                mac_ocp_write( tp, 0xFC26, 0x8000 );

                mac_ocp_write( tp, 0xFC28, 0x2549 );
                mac_ocp_write( tp, 0xFC2A, 0x06A5 );
                mac_ocp_write( tp, 0xFC2C, 0x0763 );
        } else if (tp->mcfg == CFG_METHOD_28) {
                rtl8168_hw_disable_mac_mcu_bps(dev);

                mac_ocp_write( tp, 0xF800, 0xE008 );
                mac_ocp_write( tp, 0xF802, 0xE017 );
                mac_ocp_write( tp, 0xF804, 0xE019 );
                mac_ocp_write( tp, 0xF806, 0xE01B );
                mac_ocp_write( tp, 0xF808, 0xE01D );
                mac_ocp_write( tp, 0xF80A, 0xE01F );
                mac_ocp_write( tp, 0xF80C, 0xE021 );
                mac_ocp_write( tp, 0xF80E, 0xE023 );
                mac_ocp_write( tp, 0xF810, 0xC50F );
                mac_ocp_write( tp, 0xF812, 0x76A4 );
                mac_ocp_write( tp, 0xF814, 0x49E3 );
                mac_ocp_write( tp, 0xF816, 0xF007 );
                mac_ocp_write( tp, 0xF818, 0x49C0 );
                mac_ocp_write( tp, 0xF81A, 0xF103 );
                mac_ocp_write( tp, 0xF81C, 0xC607 );
                mac_ocp_write( tp, 0xF81E, 0xBE00 );
                mac_ocp_write( tp, 0xF820, 0xC606 );
                mac_ocp_write( tp, 0xF822, 0xBE00 );
                mac_ocp_write( tp, 0xF824, 0xC602 );
                mac_ocp_write( tp, 0xF826, 0xBE00 );
                mac_ocp_write( tp, 0xF828, 0x0BDA );
                mac_ocp_write( tp, 0xF82A, 0x0BB0 );
                mac_ocp_write( tp, 0xF82C, 0x0BBA );
                mac_ocp_write( tp, 0xF82E, 0xDC00 );
                mac_ocp_write( tp, 0xF830, 0xC602 );
                mac_ocp_write( tp, 0xF832, 0xBE00 );
                mac_ocp_write( tp, 0xF834, 0x0000 );
                mac_ocp_write( tp, 0xF836, 0xC602 );
                mac_ocp_write( tp, 0xF838, 0xBE00 );
                mac_ocp_write( tp, 0xF83A, 0x0000 );
                mac_ocp_write( tp, 0xF83C, 0xC602 );
                mac_ocp_write( tp, 0xF83E, 0xBE00 );
                mac_ocp_write( tp, 0xF840, 0x0000 );
                mac_ocp_write( tp, 0xF842, 0xC602 );
                mac_ocp_write( tp, 0xF844, 0xBE00 );
                mac_ocp_write( tp, 0xF846, 0x0000 );
                mac_ocp_write( tp, 0xF848, 0xC602 );
                mac_ocp_write( tp, 0xF84A, 0xBE00 );
                mac_ocp_write( tp, 0xF84C, 0x0000 );
                mac_ocp_write( tp, 0xF84E, 0xC602 );
                mac_ocp_write( tp, 0xF850, 0xBE00 );
                mac_ocp_write( tp, 0xF852, 0x0000 );
                mac_ocp_write( tp, 0xF854, 0xC602 );
                mac_ocp_write( tp, 0xF856, 0xBE00 );
                mac_ocp_write( tp, 0xF858, 0x0000 );

                mac_ocp_write( tp, 0xFC26, 0x8000 );

                mac_ocp_write( tp, 0xFC28, 0x0BB3 );
        } else if (tp->mcfg == CFG_METHOD_29 || tp->mcfg == CFG_METHOD_30) {
                rtl8168_hw_disable_mac_mcu_bps(dev);

                mac_ocp_write(tp, 0xF800, 0xE008);
                mac_ocp_write(tp, 0xF802, 0xE00F);
                mac_ocp_write(tp, 0xF804, 0xE011);
                mac_ocp_write(tp, 0xF806, 0xE047);
                mac_ocp_write(tp, 0xF808, 0xE049);
                mac_ocp_write(tp, 0xF80A, 0xE073);
                mac_ocp_write(tp, 0xF80C, 0xE075);
                mac_ocp_write(tp, 0xF80E, 0xE077);
                mac_ocp_write(tp, 0xF810, 0xC707);
                mac_ocp_write(tp, 0xF812, 0x1D00);
                mac_ocp_write(tp, 0xF814, 0x8DE2);
                mac_ocp_write(tp, 0xF816, 0x48C1);
                mac_ocp_write(tp, 0xF818, 0xC502);
                mac_ocp_write(tp, 0xF81A, 0xBD00);
                mac_ocp_write(tp, 0xF81C, 0x00E4);
                mac_ocp_write(tp, 0xF81E, 0xE0C0);
                mac_ocp_write(tp, 0xF820, 0xC502);
                mac_ocp_write(tp, 0xF822, 0xBD00);
                mac_ocp_write(tp, 0xF824, 0x0216);
                mac_ocp_write(tp, 0xF826, 0xC634);
                mac_ocp_write(tp, 0xF828, 0x75C0);
                mac_ocp_write(tp, 0xF82A, 0x49D3);
                mac_ocp_write(tp, 0xF82C, 0xF027);
                mac_ocp_write(tp, 0xF82E, 0xC631);
                mac_ocp_write(tp, 0xF830, 0x75C0);
                mac_ocp_write(tp, 0xF832, 0x49D3);
                mac_ocp_write(tp, 0xF834, 0xF123);
                mac_ocp_write(tp, 0xF836, 0xC627);
                mac_ocp_write(tp, 0xF838, 0x75C0);
                mac_ocp_write(tp, 0xF83A, 0xB405);
                mac_ocp_write(tp, 0xF83C, 0xC525);
                mac_ocp_write(tp, 0xF83E, 0x9DC0);
                mac_ocp_write(tp, 0xF840, 0xC621);
                mac_ocp_write(tp, 0xF842, 0x75C8);
                mac_ocp_write(tp, 0xF844, 0x49D5);
                mac_ocp_write(tp, 0xF846, 0xF00A);
                mac_ocp_write(tp, 0xF848, 0x49D6);
                mac_ocp_write(tp, 0xF84A, 0xF008);
                mac_ocp_write(tp, 0xF84C, 0x49D7);
                mac_ocp_write(tp, 0xF84E, 0xF006);
                mac_ocp_write(tp, 0xF850, 0x49D8);
                mac_ocp_write(tp, 0xF852, 0xF004);
                mac_ocp_write(tp, 0xF854, 0x75D2);
                mac_ocp_write(tp, 0xF856, 0x49D9);
                mac_ocp_write(tp, 0xF858, 0xF111);
                mac_ocp_write(tp, 0xF85A, 0xC517);
                mac_ocp_write(tp, 0xF85C, 0x9DC8);
                mac_ocp_write(tp, 0xF85E, 0xC516);
                mac_ocp_write(tp, 0xF860, 0x9DD2);
                mac_ocp_write(tp, 0xF862, 0xC618);
                mac_ocp_write(tp, 0xF864, 0x75C0);
                mac_ocp_write(tp, 0xF866, 0x49D4);
                mac_ocp_write(tp, 0xF868, 0xF003);
                mac_ocp_write(tp, 0xF86A, 0x49D0);
                mac_ocp_write(tp, 0xF86C, 0xF104);
                mac_ocp_write(tp, 0xF86E, 0xC60A);
                mac_ocp_write(tp, 0xF870, 0xC50E);
                mac_ocp_write(tp, 0xF872, 0x9DC0);
                mac_ocp_write(tp, 0xF874, 0xB005);
                mac_ocp_write(tp, 0xF876, 0xC607);
                mac_ocp_write(tp, 0xF878, 0x9DC0);
                mac_ocp_write(tp, 0xF87A, 0xB007);
                mac_ocp_write(tp, 0xF87C, 0xC602);
                mac_ocp_write(tp, 0xF87E, 0xBE00);
                mac_ocp_write(tp, 0xF880, 0x1A06);
                mac_ocp_write(tp, 0xF882, 0xB400);
                mac_ocp_write(tp, 0xF884, 0xE86C);
                mac_ocp_write(tp, 0xF886, 0xA000);
                mac_ocp_write(tp, 0xF888, 0x01E1);
                mac_ocp_write(tp, 0xF88A, 0x0200);
                mac_ocp_write(tp, 0xF88C, 0x9200);
                mac_ocp_write(tp, 0xF88E, 0xE84C);
                mac_ocp_write(tp, 0xF890, 0xE004);
                mac_ocp_write(tp, 0xF892, 0xE908);
                mac_ocp_write(tp, 0xF894, 0xC502);
                mac_ocp_write(tp, 0xF896, 0xBD00);
                mac_ocp_write(tp, 0xF898, 0x0B58);
                mac_ocp_write(tp, 0xF89A, 0xB407);
                mac_ocp_write(tp, 0xF89C, 0xB404);
                mac_ocp_write(tp, 0xF89E, 0x2195);
                mac_ocp_write(tp, 0xF8A0, 0x25BD);
                mac_ocp_write(tp, 0xF8A2, 0x9BE0);
                mac_ocp_write(tp, 0xF8A4, 0x1C1C);
                mac_ocp_write(tp, 0xF8A6, 0x484F);
                mac_ocp_write(tp, 0xF8A8, 0x9CE2);
                mac_ocp_write(tp, 0xF8AA, 0x72E2);
                mac_ocp_write(tp, 0xF8AC, 0x49AE);
                mac_ocp_write(tp, 0xF8AE, 0xF1FE);
                mac_ocp_write(tp, 0xF8B0, 0x0B00);
                mac_ocp_write(tp, 0xF8B2, 0xF116);
                mac_ocp_write(tp, 0xF8B4, 0xC71C);
                mac_ocp_write(tp, 0xF8B6, 0xC419);
                mac_ocp_write(tp, 0xF8B8, 0x9CE0);
                mac_ocp_write(tp, 0xF8BA, 0x1C13);
                mac_ocp_write(tp, 0xF8BC, 0x484F);
                mac_ocp_write(tp, 0xF8BE, 0x9CE2);
                mac_ocp_write(tp, 0xF8C0, 0x74E2);
                mac_ocp_write(tp, 0xF8C2, 0x49CE);
                mac_ocp_write(tp, 0xF8C4, 0xF1FE);
                mac_ocp_write(tp, 0xF8C6, 0xC412);
                mac_ocp_write(tp, 0xF8C8, 0x9CE0);
                mac_ocp_write(tp, 0xF8CA, 0x1C13);
                mac_ocp_write(tp, 0xF8CC, 0x484F);
                mac_ocp_write(tp, 0xF8CE, 0x9CE2);
                mac_ocp_write(tp, 0xF8D0, 0x74E2);
                mac_ocp_write(tp, 0xF8D2, 0x49CE);
                mac_ocp_write(tp, 0xF8D4, 0xF1FE);
                mac_ocp_write(tp, 0xF8D6, 0xC70C);
                mac_ocp_write(tp, 0xF8D8, 0x74F8);
                mac_ocp_write(tp, 0xF8DA, 0x48C3);
                mac_ocp_write(tp, 0xF8DC, 0x8CF8);
                mac_ocp_write(tp, 0xF8DE, 0xB004);
                mac_ocp_write(tp, 0xF8E0, 0xB007);
                mac_ocp_write(tp, 0xF8E2, 0xC502);
                mac_ocp_write(tp, 0xF8E4, 0xBD00);
                mac_ocp_write(tp, 0xF8E6, 0x0F24);
                mac_ocp_write(tp, 0xF8E8, 0x0481);
                mac_ocp_write(tp, 0xF8EA, 0x0C81);
                mac_ocp_write(tp, 0xF8EC, 0xDE24);
                mac_ocp_write(tp, 0xF8EE, 0xE000);
                mac_ocp_write(tp, 0xF8F0, 0xC602);
                mac_ocp_write(tp, 0xF8F2, 0xBE00);
                mac_ocp_write(tp, 0xF8F4, 0x0CA4);
                mac_ocp_write(tp, 0xF8F6, 0xC502);
                mac_ocp_write(tp, 0xF8F8, 0xBD00);
                mac_ocp_write(tp, 0xF8FA, 0x0000);
                mac_ocp_write(tp, 0xF8FC, 0xC602);
                mac_ocp_write(tp, 0xF8FE, 0xBE00);
                mac_ocp_write(tp, 0xF900, 0x0000);

                mac_ocp_write(tp, 0xFC26, 0x8000);

                mac_ocp_write(tp, 0xFC28, 0x00E2);
                mac_ocp_write(tp, 0xFC2A, 0x0210);
                mac_ocp_write(tp, 0xFC2C, 0x1A04);
                mac_ocp_write(tp, 0xFC2E, 0x0B26);
                mac_ocp_write(tp, 0xFC30, 0x0F02);
                mac_ocp_write(tp, 0xFC32, 0x0CA0);

                mac_ocp_write(tp, 0xFC38, 0x003F);
        }
}

static void
rtl8168_hw_init(struct net_device *dev)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;
        unsigned long flags;

        switch (tp->mcfg) {
        case CFG_METHOD_14:
        case CFG_METHOD_15:
        case CFG_METHOD_16:
        case CFG_METHOD_17:
        case CFG_METHOD_18:
        case CFG_METHOD_19:
        case CFG_METHOD_20:
        case CFG_METHOD_21:
        case CFG_METHOD_22:
        case CFG_METHOD_23:
        case CFG_METHOD_24:
        case CFG_METHOD_25:
        case CFG_METHOD_26:
        case CFG_METHOD_27:
        case CFG_METHOD_28:
        case CFG_METHOD_29:
        case CFG_METHOD_30:
                RTL_W8(Cfg9346, Cfg9346_Unlock);
                RTL_W8(Config5, RTL_R8(Config5) & ~BIT_0);
                RTL_W8(Config2, RTL_R8(Config2) & ~BIT_7);
                RTL_W8(Cfg9346, Cfg9346_Lock);
                RTL_W8(0xF1, RTL_R8(0xF1) & ~BIT_7);
                break;
        }

        //Disable UPS
        switch (tp->mcfg) {
        case CFG_METHOD_29:
        case CFG_METHOD_30:
                mac_ocp_write(tp, 0xD400, mac_ocp_read( tp, 0xD400) & ~(BIT_0));
                break;
        }

        //Disable DMA Aggregation
        switch (tp->mcfg) {
        case CFG_METHOD_29:
        case CFG_METHOD_30:
                mac_ocp_write(tp, 0xE63E, mac_ocp_read( tp, 0xE63E) & ~(BIT_3 | BIT_2 | BIT_1));
                mac_ocp_write(tp, 0xE63E, mac_ocp_read( tp, 0xE63E) | (BIT_0));
                mac_ocp_write(tp, 0xE63E, mac_ocp_read( tp, 0xE63E) & ~(BIT_0));
                mac_ocp_write(tp, 0xC094, 0x0);
                mac_ocp_write(tp, 0xC09E, 0x0);
                break;
        }

        switch (tp->mcfg) {
        case CFG_METHOD_9:
        case CFG_METHOD_10:
                RTL_W8(DBG_reg, RTL_R8(DBG_reg) | BIT_1 | BIT_7);
                break;
        }

        switch (tp->mcfg) {
        case CFG_METHOD_14:
        case CFG_METHOD_15:
        case CFG_METHOD_16:
        case CFG_METHOD_17:
        case CFG_METHOD_18:
        case CFG_METHOD_19:
                RTL_W8(0xF2, (RTL_R8(0xF2) & ~(BIT_2 | BIT_1 | BIT_0)));
                break;
        }

        switch (tp->mcfg) {
        case CFG_METHOD_16:
        case CFG_METHOD_17:
        case CFG_METHOD_18:
        case CFG_METHOD_19:
        case CFG_METHOD_20:
                if (aspm) {
                        RTL_W8(0x6E, RTL_R8(0x6E) | BIT_6);
                        rtl8168_eri_write(ioaddr, 0x1AE, 2, 0x0403, ERIAR_ExGMAC);
                }
                break;
        case CFG_METHOD_21:
        case CFG_METHOD_22:
        case CFG_METHOD_29:
        case CFG_METHOD_30:
                if (aspm) {
                        if ((mac_ocp_read(tp, 0xDC00) & BIT_3) || (RTL_R8(Config0) & 0x07)) {
                                RTL_W8(0x6E, RTL_R8(0x6E) | BIT_6);
                                rtl8168_eri_write(ioaddr, 0x1AE, 2, 0x0403, ERIAR_ExGMAC);
                        }
                }
                break;
        }

        if (tp->mcfg == CFG_METHOD_10 || tp->mcfg == CFG_METHOD_14 || tp->mcfg == CFG_METHOD_15)
                RTL_W8(0xF3, RTL_R8(0xF3) | BIT_2);

        rtl8168_hw_mac_mcu_config(dev);

        /*disable ocp phy power saving*/
        if (tp->mcfg == CFG_METHOD_25 || tp->mcfg == CFG_METHOD_26 ||
            tp->mcfg == CFG_METHOD_27 || tp->mcfg == CFG_METHOD_28 ||
            tp->mcfg == CFG_METHOD_29 || tp->mcfg == CFG_METHOD_30) {
                spin_lock_irqsave(&tp->phy_lock, flags);
                mdio_write_phy_ocp(tp, 0x0C41, 0x13, 0x0000);
                mdio_write_phy_ocp(tp, 0x0C41, 0x13, 0x0500);
                spin_unlock_irqrestore(&tp->phy_lock, flags);
        }
}

static void
rtl8168_hw_ephy_config(struct net_device *dev)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;
        u16 ephy_data;


        if (tp->mcfg == CFG_METHOD_4) {
                /*Set EPHY registers    begin*/
                /*Set EPHY register offset 0x02 bit 11 to 0 and bit 12 to 1*/
                ephy_data = rtl8168_ephy_read(ioaddr, 0x02);
                ephy_data &= ~BIT_11;
                ephy_data |= BIT_12;
                rtl8168_ephy_write(ioaddr, 0x02, ephy_data);

                /*Set EPHY register offset 0x03 bit 1 to 1*/
                ephy_data = rtl8168_ephy_read(ioaddr, 0x03);
                ephy_data |= (1 << 1);
                rtl8168_ephy_write(ioaddr, 0x03, ephy_data);

                /*Set EPHY register offset 0x06 bit 7 to 0*/
                ephy_data = rtl8168_ephy_read(ioaddr, 0x06);
                ephy_data &= ~(1 << 7);
                rtl8168_ephy_write(ioaddr, 0x06, ephy_data);
                /*Set EPHY registers    end*/
        } else if (tp->mcfg == CFG_METHOD_5) {
                /* set EPHY registers */
                SetPCIePhyBit(tp, 0x01, BIT_0);

                ClearAndSetPCIePhyBit(tp,
                                      0x03,
                                      BIT_10,
                                      BIT_5
                                     );
        } else if (tp->mcfg == CFG_METHOD_9) {
                /* set EPHY registers */
                rtl8168_ephy_write(ioaddr, 0x01, 0x7C7F);
                rtl8168_ephy_write(ioaddr, 0x02, 0x011F);
                if(tp->eeprom_type != EEPROM_TYPE_NONE) {
                        ClearAndSetPCIePhyBit(tp,
                                              0x03,
                                              0xFFB0,
                                              0x05B0
                                             );
                } else {
                        ClearAndSetPCIePhyBit(tp,
                                              0x03,
                                              0xFFF0,
                                              0x05F0
                                             );
                }
                rtl8168_ephy_write(ioaddr, 0x06, 0xB271);
                rtl8168_ephy_write(ioaddr, 0x07, 0xCE00);
        } else if (tp->mcfg == CFG_METHOD_10) {
                /* set EPHY registers */
                rtl8168_ephy_write(ioaddr, 0x01, 0x6C7F);
                rtl8168_ephy_write(ioaddr, 0x02, 0x011F);
                ClearAndSetPCIePhyBit(tp,
                                      0x03,
                                      0xFFF0,
                                      0x01B0
                                     );
                rtl8168_ephy_write(ioaddr, 0x1A, 0x0546);
                rtl8168_ephy_write(ioaddr, 0x1C, 0x80C4);
                rtl8168_ephy_write(ioaddr, 0x1D, 0x78E5);
                rtl8168_ephy_write(ioaddr, 0x0A, 0x8100);
        } else if (tp->mcfg == CFG_METHOD_12) {
                ephy_data = rtl8168_ephy_read(ioaddr, 0x0B);
                rtl8168_ephy_write(ioaddr, 0x0B, ephy_data|0x48);
                ephy_data = rtl8168_ephy_read(ioaddr, 0x19);
                ephy_data &= ~0x20;
                rtl8168_ephy_write(ioaddr, 0x19, ephy_data|0x50);
                ephy_data = rtl8168_ephy_read(ioaddr, 0x0C);
                ephy_data &= ~0x100;
                rtl8168_ephy_write(ioaddr, 0x0C, ephy_data|0x20);
        } else if (tp->mcfg == CFG_METHOD_14 || tp->mcfg == CFG_METHOD_15) {
                /* set EPHY registers */
                ephy_data = rtl8168_ephy_read(ioaddr, 0x00) & ~0x0200;
                ephy_data |= 0x0100;
                rtl8168_ephy_write(ioaddr, 0x00, ephy_data);

                ephy_data = rtl8168_ephy_read(ioaddr, 0x00);
                ephy_data |= 0x0004;
                rtl8168_ephy_write(ioaddr, 0x00, ephy_data);

                ephy_data = rtl8168_ephy_read(ioaddr, 0x06) & ~0x0002;
                ephy_data |= 0x0001;
                rtl8168_ephy_write(ioaddr, 0x06, ephy_data);

                ephy_data = rtl8168_ephy_read(ioaddr, 0x06);
                ephy_data |= 0x0030;
                rtl8168_ephy_write(ioaddr, 0x06, ephy_data);

                ephy_data = rtl8168_ephy_read(ioaddr, 0x07);
                ephy_data |= 0x2000;
                rtl8168_ephy_write(ioaddr, 0x07, ephy_data);

                ephy_data = rtl8168_ephy_read(ioaddr, 0x00);
                ephy_data |= 0x0020;
                rtl8168_ephy_write(ioaddr, 0x00, ephy_data);

                ephy_data = rtl8168_ephy_read(ioaddr, 0x03) & ~0x5800;
                ephy_data |= 0x2000;
                rtl8168_ephy_write(ioaddr, 0x03, ephy_data);

                ephy_data = rtl8168_ephy_read(ioaddr, 0x03);
                ephy_data |= 0x0001;
                rtl8168_ephy_write(ioaddr, 0x03, ephy_data);

                ephy_data = rtl8168_ephy_read(ioaddr, 0x01) & ~0x0800;
                ephy_data |= 0x1000;
                rtl8168_ephy_write(ioaddr, 0x01, ephy_data);

                ephy_data = rtl8168_ephy_read(ioaddr, 0x07);
                ephy_data |= 0x4000;
                rtl8168_ephy_write(ioaddr, 0x07, ephy_data);

                ephy_data = rtl8168_ephy_read(ioaddr, 0x1E);
                ephy_data |= 0x2000;
                rtl8168_ephy_write(ioaddr, 0x1E, ephy_data);

                rtl8168_ephy_write(ioaddr, 0x19, 0xFE6C);

                ephy_data = rtl8168_ephy_read(ioaddr, 0x0A);
                ephy_data |= 0x0040;
                rtl8168_ephy_write(ioaddr, 0x0A, ephy_data);
        } else if (tp->mcfg == CFG_METHOD_16 || tp->mcfg == CFG_METHOD_17) {
                if (tp->mcfg == CFG_METHOD_16) {
                        rtl8168_ephy_write(ioaddr, 0x06, 0xF020);
                        rtl8168_ephy_write(ioaddr, 0x07, 0x01FF);
                        rtl8168_ephy_write(ioaddr, 0x00, 0x5027);
                        rtl8168_ephy_write(ioaddr, 0x01, 0x0003);
                        rtl8168_ephy_write(ioaddr, 0x02, 0x2D16);
                        rtl8168_ephy_write(ioaddr, 0x03, 0x6D49);
                        rtl8168_ephy_write(ioaddr, 0x08, 0x0006);
                        rtl8168_ephy_write(ioaddr, 0x0A, 0x00C8);
                }

                ephy_data = rtl8168_ephy_read(ioaddr, 0x09);
                ephy_data |= BIT_7;
                rtl8168_ephy_write(ioaddr, 0x09, ephy_data);

                ephy_data = rtl8168_ephy_read(ioaddr, 0x19);
                ephy_data |= (BIT_2 | BIT_5 | BIT_9);
                rtl8168_ephy_write(ioaddr, 0x19, ephy_data);

                ephy_data = rtl8168_ephy_read(ioaddr, 0x00);
                ephy_data |= BIT_3;
                rtl8168_ephy_write(ioaddr, 0x00, ephy_data);
                ephy_data = rtl8168_ephy_read(ioaddr, 0x0C);
                ephy_data &= ~(BIT_13 | BIT_12 | BIT_11 | BIT_10 | BIT_8 | BIT_7 | BIT_6 | BIT_5 | BIT_4);
                ephy_data |= BIT_9;
                rtl8168_ephy_write(ioaddr, 0x0C, ephy_data);
        } else if (tp->mcfg == CFG_METHOD_18 || tp->mcfg == CFG_METHOD_19) {
                if (tp->mcfg == CFG_METHOD_18) {
                        ephy_data = rtl8168_ephy_read(ioaddr, 0x06);
                        ephy_data |= BIT_5;
                        ephy_data &= ~(BIT_7 | BIT_6);
                        rtl8168_ephy_write(ioaddr, 0x06, ephy_data);

                        ephy_data = rtl8168_ephy_read(ioaddr, 0x08);
                        ephy_data |= BIT_1;
                        ephy_data &= ~BIT_0;
                        rtl8168_ephy_write(ioaddr, 0x08, ephy_data);
                }

                ephy_data = rtl8168_ephy_read(ioaddr, 0x09);
                ephy_data |= BIT_7;
                rtl8168_ephy_write(ioaddr, 0x09, ephy_data);

                ephy_data = rtl8168_ephy_read(ioaddr, 0x19);
                ephy_data |= (BIT_2 | BIT_5 | BIT_9);
                rtl8168_ephy_write(ioaddr, 0x19, ephy_data);

                ephy_data = rtl8168_ephy_read(ioaddr, 0x00);
                ephy_data |= BIT_3;
                rtl8168_ephy_write(ioaddr, 0x00, ephy_data);
                ephy_data = rtl8168_ephy_read(ioaddr, 0x0C);
                ephy_data &= ~(BIT_13 | BIT_12 | BIT_11 | BIT_10 | BIT_8 | BIT_7 | BIT_6 | BIT_5 | BIT_4);
                ephy_data |= BIT_9;
                rtl8168_ephy_write(ioaddr, 0x0C, ephy_data);
        } else if (tp->mcfg == CFG_METHOD_20) {
                ephy_data = rtl8168_ephy_read(ioaddr, 0x06);
                ephy_data |= BIT_5;
                ephy_data &= ~(BIT_7 | BIT_6);
                rtl8168_ephy_write(ioaddr, 0x06, ephy_data);

                rtl8168_ephy_write(ioaddr, 0x0f, 0x5200);

                ephy_data = rtl8168_ephy_read(ioaddr, 0x19);
                ephy_data |= (BIT_2 | BIT_5 | BIT_9);
                rtl8168_ephy_write(ioaddr, 0x19, ephy_data);

                ephy_data = rtl8168_ephy_read(ioaddr, 0x00);
                ephy_data |= BIT_3;
                rtl8168_ephy_write(ioaddr, 0x00, ephy_data);
                ephy_data = rtl8168_ephy_read(ioaddr, 0x0C);
                ephy_data &= ~(BIT_13 | BIT_12 | BIT_11 | BIT_10 | BIT_8 | BIT_7 | BIT_6 | BIT_5 | BIT_4);
                ephy_data |= BIT_9;
                rtl8168_ephy_write(ioaddr, 0x0C, ephy_data);
        } else if (tp->mcfg == CFG_METHOD_21 || tp->mcfg == CFG_METHOD_22) {

                ephy_data = rtl8168_ephy_read(ioaddr, 0x00);
                ephy_data &= ~(BIT_3);
                rtl8168_ephy_write(ioaddr, 0x00, ephy_data);
                ephy_data = rtl8168_ephy_read(ioaddr, 0x0C);
                ephy_data &= ~(BIT_13 | BIT_12 | BIT_11 | BIT_10 | BIT_9 | BIT_8 | BIT_7 | BIT_6 | BIT_5 | BIT_4);
                ephy_data |= (BIT_5 | BIT_11);
                rtl8168_ephy_write(ioaddr, 0x0C, ephy_data);

                ephy_data = rtl8168_ephy_read(ioaddr, 0x1E);
                ephy_data |= (BIT_0);
                rtl8168_ephy_write(ioaddr, 0x1E, ephy_data);

                ephy_data = rtl8168_ephy_read(ioaddr, 0x19);
                ephy_data &= ~(BIT_15);
                rtl8168_ephy_write(ioaddr, 0x19, ephy_data);
        } else if (tp->mcfg == CFG_METHOD_25) {
                ephy_data = rtl8168_ephy_read(ioaddr, 0x00);
                ephy_data &= ~BIT_3;
                rtl8168_ephy_write(ioaddr, 0x00, ephy_data);
                ephy_data = rtl8168_ephy_read(ioaddr, 0x0C);
                ephy_data &= ~(BIT_13 | BIT_12 | BIT_11 | BIT_10| BIT_9 | BIT_8 | BIT_7 | BIT_6 | BIT_5 | BIT_4);
                ephy_data |= (BIT_5 | BIT_11);
                rtl8168_ephy_write(ioaddr, 0x0C, ephy_data);

                rtl8168_ephy_write(ioaddr, 0x19, 0x7C00);
                rtl8168_ephy_write(ioaddr, 0x1E, 0x20EB);
                rtl8168_ephy_write(ioaddr, 0x0D, 0x1666);
                rtl8168_ephy_write(ioaddr, 0x00, 0x10A3);
                rtl8168_ephy_write(ioaddr, 0x06, 0xF050);
        } else if (tp->mcfg == CFG_METHOD_26) {
                ClearPCIePhyBit(tp, 0x00, BIT_3);
                ClearAndSetPCIePhyBit( tp,
                                       0x0C,
                                       (BIT_13 | BIT_12 | BIT_10 | BIT_9 | BIT_8 | BIT_7 | BIT_6 | BIT_4),
                                       (BIT_5 | BIT_11)
                                     );
                SetPCIePhyBit(tp, 0x1E, BIT_0);
                ClearPCIePhyBit(tp, 0x19, BIT_15);

                ClearPCIePhyBit(tp, 0x19, (BIT_5 | BIT_0));

                SetPCIePhyBit(tp, 0x1E, BIT_13);
                ClearPCIePhyBit(tp, 0x0D, BIT_8);
                SetPCIePhyBit(tp, 0x0D, BIT_9);
                SetPCIePhyBit(tp, 0x00, BIT_7);

                SetPCIePhyBit(tp, 0x06, BIT_4);
        } else if (tp->mcfg == CFG_METHOD_23) {
                rtl8168_ephy_write(ioaddr, 0x00, 0x10AB);
                rtl8168_ephy_write(ioaddr, 0x06, 0xf030);
                rtl8168_ephy_write(ioaddr, 0x08, 0x2006);
                rtl8168_ephy_write(ioaddr, 0x0D, 0x1666);

                ephy_data = rtl8168_ephy_read(ioaddr, 0x0C);
                ephy_data &= ~(BIT_13 | BIT_12 | BIT_11 | BIT_10 | BIT_9 | BIT_8 | BIT_7 | BIT_6 | BIT_5 | BIT_4);
                rtl8168_ephy_write(ioaddr, 0x0C, ephy_data);
        } else if (tp->mcfg == CFG_METHOD_27) {
                rtl8168_ephy_write(ioaddr, 0x00, 0x10A3);
                rtl8168_ephy_write(ioaddr, 0x19, 0xFC00);
                rtl8168_ephy_write(ioaddr, 0x1E, 0x20EA);
        } else if (tp->mcfg == CFG_METHOD_28) {
                SetPCIePhyBit(tp, 0x00, BIT_7);
                ClearAndSetPCIePhyBit(tp,
                                      0x0D,
                                      BIT_8,
                                      BIT_9
                                     );
                ClearPCIePhyBit(tp, 0x19, (BIT_15 | BIT_5 | BIT_0));
                SetPCIePhyBit(tp, 0x1E, BIT_13);

        } else if (tp->mcfg == CFG_METHOD_29 || tp->mcfg == CFG_METHOD_30) {
                ClearPCIePhyBit(tp, 0x1E, BIT_11);

                SetPCIePhyBit(tp, 0x1E, BIT_0);
                SetPCIePhyBit(tp, 0x1D, BIT_11);

                rtl8168_ephy_write(ioaddr, 0x05, 0x2089);
                rtl8168_ephy_write(ioaddr, 0x06, 0x5881);

                rtl8168_ephy_write(ioaddr, 0x04, 0x154A);
                rtl8168_ephy_write(ioaddr, 0x01, 0x068B);
        }
}

static int
rtl8168_check_hw_phy_mcu_code_ver(struct net_device *dev)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        int ram_code_ver_match = 0;

        switch (tp->mcfg) {
        case CFG_METHOD_14:
        case CFG_METHOD_15:
                mdio_write(tp, 0x1F, 0x0005);
                mdio_write(tp, 0x05, 0x8B60);
                tp->hw_ram_code_ver = mdio_read(tp, 0x06);
                mdio_write(tp, 0x1F, 0x0000);
                break;
        case CFG_METHOD_16:
        case CFG_METHOD_17:
        case CFG_METHOD_18:
        case CFG_METHOD_19:
        case CFG_METHOD_20:
                mdio_write(tp, 0x1F, 0x0005);
                mdio_write(tp, 0x05, 0x8B30);
                tp->hw_ram_code_ver = mdio_read(tp, 0x06);
                mdio_write(tp, 0x1F, 0x0000);
                break;
        case CFG_METHOD_21:
        case CFG_METHOD_22:
        case CFG_METHOD_23:
        case CFG_METHOD_27:
        case CFG_METHOD_28:
        case CFG_METHOD_24:
        case CFG_METHOD_25:
        case CFG_METHOD_26:
        case CFG_METHOD_29:
        case CFG_METHOD_30:
                mdio_write(tp, 0x1F, 0x0A43);
                mdio_write(tp, 0x13, 0x801E);
                tp->hw_ram_code_ver = mdio_read(tp, 0x14);
                mdio_write(tp, 0x1F, 0x0000);
                break;
        default:
                tp->hw_ram_code_ver = ~0;
                break;
        }

        if( tp->hw_ram_code_ver == tp->sw_ram_code_ver) {
                ram_code_ver_match = 1;
                tp->HwHasWrRamCodeToMicroP = TRUE;
        }

        return ram_code_ver_match;
}

static void
rtl8168_write_hw_phy_mcu_code_ver(struct net_device *dev)
{
        struct rtl8168_private *tp = netdev_priv(dev);

        switch (tp->mcfg) {
        case CFG_METHOD_14:
        case CFG_METHOD_15:
                mdio_write(tp, 0x1F, 0x0005);
                mdio_write(tp, 0x05, 0x8B60);
                mdio_write(tp, 0x06, tp->sw_ram_code_ver);
                mdio_write(tp, 0x1F, 0x0000);
                tp->hw_ram_code_ver = tp->sw_ram_code_ver;
                break;
        case CFG_METHOD_16:
        case CFG_METHOD_17:
        case CFG_METHOD_18:
        case CFG_METHOD_19:
        case CFG_METHOD_20:
                mdio_write(tp, 0x1F, 0x0005);
                mdio_write(tp, 0x05, 0x8B30);
                mdio_write(tp, 0x06, tp->sw_ram_code_ver);
                mdio_write(tp, 0x1F, 0x0000);
                tp->hw_ram_code_ver = tp->sw_ram_code_ver;
                break;
        case CFG_METHOD_21:
        case CFG_METHOD_22:
        case CFG_METHOD_23:
        case CFG_METHOD_27:
        case CFG_METHOD_28:
        case CFG_METHOD_24:
        case CFG_METHOD_25:
        case CFG_METHOD_26:
        case CFG_METHOD_29:
        case CFG_METHOD_30:
                mdio_write(tp, 0x1F, 0x0A43);
                mdio_write(tp, 0x13, 0x801E);
                mdio_write(tp, 0x14, tp->sw_ram_code_ver);
                mdio_write(tp, 0x1F, 0x0000);
                tp->hw_ram_code_ver = tp->sw_ram_code_ver;
                break;
        }
}
static int
rtl8168_phy_ram_code_check(struct net_device *dev)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        u16 PhyRegValue;
        u32 WaitCnt;
        int retval = TRUE;

        switch(tp->mcfg) {
        case CFG_METHOD_21:
                mdio_write(tp, 0x1f, 0x0A40);
                PhyRegValue = mdio_read(tp, 0x10);
                PhyRegValue &= ~(BIT_11);
                mdio_write(tp, 0x10, PhyRegValue);


                mdio_write(tp, 0x1f, 0x0A00);
                PhyRegValue = mdio_read(tp, 0x10);
                PhyRegValue &= ~(BIT_12 | BIT_13 | BIT_14 | BIT_15);
                mdio_write(tp, 0x10, PhyRegValue);

                mdio_write(tp, 0x1f, 0x0A43);
                mdio_write(tp, 0x13, 0x8010);
                PhyRegValue = mdio_read(tp, 0x14);
                PhyRegValue &= ~(BIT_11);
                mdio_write(tp, 0x14, PhyRegValue);

                mdio_write(tp,0x1f, 0x0B82);
                PhyRegValue = mdio_read(tp, 0x10);
                PhyRegValue |= BIT_4;
                mdio_write(tp,0x10, PhyRegValue);

                mdio_write(tp,0x1f, 0x0B80);
                WaitCnt = 0;
                do {
                        PhyRegValue = mdio_read(tp, 0x10);
                        PhyRegValue &= 0x0040;
                        udelay(100);
                        WaitCnt++;
                } while(PhyRegValue != 0x0040 && WaitCnt <1000);

                if(WaitCnt == 1000) {
                        retval = FALSE ;
                }

                mdio_write(tp, 0x1f, 0x0A40);
                mdio_write(tp, 0x10, 0x0140);

                mdio_write(tp, 0x1f, 0x0A4A);
                PhyRegValue = mdio_read(tp, 0x13);
                PhyRegValue &= ~(BIT_6);
                PhyRegValue |= (BIT_7);
                mdio_write(tp, 0x13, PhyRegValue);

                mdio_write(tp, 0x1f, 0x0A44);
                PhyRegValue = mdio_read(tp, 0x14);
                PhyRegValue |= (BIT_2);
                mdio_write(tp, 0x14, PhyRegValue);

                mdio_write(tp, 0x1f, 0x0A50);
                PhyRegValue = mdio_read(tp, 0x11);
                PhyRegValue |= (BIT_11|BIT_12);
                mdio_write(tp, 0x11, PhyRegValue);

                mdio_write(tp, 0x1f, 0x0B82);
                PhyRegValue = mdio_read(tp, 0x10);
                PhyRegValue &= ~(BIT_4);
                mdio_write(tp, 0x10, PhyRegValue);

                mdio_write(tp,0x1f, 0x0A22);
                WaitCnt = 0;
                do {
                        PhyRegValue = mdio_read(tp, 0x12);
                        PhyRegValue &= 0x0010;
                        udelay(100);
                        WaitCnt++;
                } while(PhyRegValue != 0x0010 && WaitCnt <1000);

                if(WaitCnt == 1000) {
                        retval = FALSE;
                }

                mdio_write(tp, 0x1f, 0x0A40);
                mdio_write(tp, 0x10, 0x1040);

                mdio_write(tp, 0x1f, 0x0A4A);
                PhyRegValue = mdio_read(tp, 0x13);
                PhyRegValue &= ~(BIT_6|BIT_7);
                mdio_write(tp, 0x13, PhyRegValue);

                mdio_write(tp, 0x1f, 0x0A44);
                PhyRegValue = mdio_read(tp, 0x14);
                PhyRegValue &= ~(BIT_2);
                mdio_write(tp, 0x14, PhyRegValue);

                mdio_write(tp, 0x1f, 0x0A50);
                PhyRegValue = mdio_read(tp, 0x11);
                PhyRegValue &= ~(BIT_11|BIT_12);
                mdio_write(tp, 0x11, PhyRegValue);

                mdio_write(tp, 0x1f, 0x0A43);
                mdio_write(tp, 0x13, 0x8010);
                PhyRegValue = mdio_read(tp, 0x14);
                PhyRegValue |= (BIT_11);
                mdio_write(tp, 0x14, PhyRegValue);

                mdio_write(tp,0x1f, 0x0B82);
                PhyRegValue = mdio_read(tp, 0x10);
                PhyRegValue |= BIT_4;
                mdio_write(tp,0x10, PhyRegValue);

                mdio_write(tp,0x1f, 0x0B80);
                WaitCnt = 0;
                do {
                        PhyRegValue = mdio_read(tp, 0x10);
                        PhyRegValue &= 0x0040;
                        udelay(100);
                        WaitCnt++;
                } while(PhyRegValue != 0x0040 && WaitCnt <1000);

                if( WaitCnt == 1000) {
                        retval = FALSE;
                }

                mdio_write(tp, 0x1f, 0x0A20);
                PhyRegValue = mdio_read(tp, 0x13);
                if(PhyRegValue & BIT_11) {
                        if(PhyRegValue & BIT_10) {
                                retval = FALSE;
                        }
                }

                mdio_write(tp, 0x1f, 0x0B82);
                PhyRegValue = mdio_read(tp, 0x10);
                PhyRegValue &= ~(BIT_4);
                mdio_write(tp, 0x10, PhyRegValue);

                mdelay(2);
                break;
        default:
                break;
        }

        mdio_write(tp, 0x1F, 0x0000);

        return retval;
}

static void
rtl8168_set_phy_ram_code_check_fail_flag(struct net_device *dev)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        u16 TmpUshort;

        switch(tp->mcfg) {
        case CFG_METHOD_21:
                TmpUshort = mac_ocp_read(tp, 0xD3C0);
                TmpUshort |= BIT_0;
                mac_ocp_write(tp, 0xD3C0, TmpUshort);
                break;
        }
}

static void
rtl8168_set_phy_mcu_8168e_1(struct net_device *dev)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        unsigned int gphy_val,i;

        mdio_write(tp, 0x1f, 0x0000);
        mdio_write(tp, 0x00, 0x1800);
        mdio_write(tp, 0x1f, 0x0007);
        mdio_write(tp, 0x1e, 0x0023);
        mdio_write(tp, 0x17, 0x0117);
        mdio_write(tp, 0x1f, 0x0007);
        mdio_write(tp, 0x1E, 0x002C);
        mdio_write(tp, 0x1B, 0x5000);
        mdio_write(tp, 0x1f, 0x0000);
        mdio_write(tp, 0x16, 0x4104);
        for (i = 0; i < 200; i++) {
                udelay(100);
                gphy_val = mdio_read(tp, 0x1E);
                gphy_val &= 0x03FF;
                if (gphy_val == 0x000C)
                        break;
        }
        mdio_write(tp, 0x1f, 0x0005);
        for (i = 0; i < 200; i++) {
                udelay(100);
                gphy_val = mdio_read(tp, 0x07);
                if ((gphy_val & BIT_5) == 0)
                        break;
        }
        gphy_val = mdio_read(tp, 0x07);
        if (gphy_val & BIT_5) {
                mdio_write(tp, 0x1f, 0x0007);
                mdio_write(tp, 0x1e, 0x00a1);
                mdio_write(tp, 0x17, 0x1000);
                mdio_write(tp, 0x17, 0x0000);
                mdio_write(tp, 0x17, 0x2000);
                mdio_write(tp, 0x1e, 0x002f);
                mdio_write(tp, 0x18, 0x9bfb);
                mdio_write(tp, 0x1f, 0x0005);
                mdio_write(tp, 0x07, 0x0000);
                mdio_write(tp, 0x1f, 0x0000);
        }
        mdio_write(tp, 0x1f, 0x0005);
        mdio_write(tp, 0x05, 0xfff6);
        mdio_write(tp, 0x06, 0x0080);
        gphy_val = mdio_read(tp, 0x00);
        gphy_val &= ~(BIT_7);
        mdio_write(tp, 0x00, gphy_val);
        mdio_write(tp, 0x1f, 0x0002);
        gphy_val = mdio_read(tp, 0x08);
        gphy_val &= ~(BIT_7);
        mdio_write(tp, 0x08, gphy_val);
        mdio_write(tp, 0x1f, 0x0000);
        mdio_write(tp, 0x1f, 0x0007);
        mdio_write(tp, 0x1e, 0x0023);
        mdio_write(tp, 0x16, 0x0306);
        mdio_write(tp, 0x16, 0x0307);
        mdio_write(tp, 0x15, 0x000e);
        mdio_write(tp, 0x19, 0x000a);
        mdio_write(tp, 0x15, 0x0010);
        mdio_write(tp, 0x19, 0x0008);
        mdio_write(tp, 0x15, 0x0018);
        mdio_write(tp, 0x19, 0x4801);
        mdio_write(tp, 0x15, 0x0019);
        mdio_write(tp, 0x19, 0x6801);
        mdio_write(tp, 0x15, 0x001a);
        mdio_write(tp, 0x19, 0x66a1);
        mdio_write(tp, 0x15, 0x001f);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x0020);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x0021);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x0022);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x0023);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x0024);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x0025);
        mdio_write(tp, 0x19, 0x64a1);
        mdio_write(tp, 0x15, 0x0026);
        mdio_write(tp, 0x19, 0x40ea);
        mdio_write(tp, 0x15, 0x0027);
        mdio_write(tp, 0x19, 0x4503);
        mdio_write(tp, 0x15, 0x0028);
        mdio_write(tp, 0x19, 0x9f00);
        mdio_write(tp, 0x15, 0x0029);
        mdio_write(tp, 0x19, 0xa631);
        mdio_write(tp, 0x15, 0x002a);
        mdio_write(tp, 0x19, 0x9717);
        mdio_write(tp, 0x15, 0x002b);
        mdio_write(tp, 0x19, 0x302c);
        mdio_write(tp, 0x15, 0x002c);
        mdio_write(tp, 0x19, 0x4802);
        mdio_write(tp, 0x15, 0x002d);
        mdio_write(tp, 0x19, 0x58da);
        mdio_write(tp, 0x15, 0x002e);
        mdio_write(tp, 0x19, 0x400d);
        mdio_write(tp, 0x15, 0x002f);
        mdio_write(tp, 0x19, 0x4488);
        mdio_write(tp, 0x15, 0x0030);
        mdio_write(tp, 0x19, 0x9e00);
        mdio_write(tp, 0x15, 0x0031);
        mdio_write(tp, 0x19, 0x63c8);
        mdio_write(tp, 0x15, 0x0032);
        mdio_write(tp, 0x19, 0x6481);
        mdio_write(tp, 0x15, 0x0033);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x0034);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x0035);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x0036);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x0037);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x0038);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x0039);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x003a);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x003b);
        mdio_write(tp, 0x19, 0x63e8);
        mdio_write(tp, 0x15, 0x003c);
        mdio_write(tp, 0x19, 0x7d00);
        mdio_write(tp, 0x15, 0x003d);
        mdio_write(tp, 0x19, 0x59d4);
        mdio_write(tp, 0x15, 0x003e);
        mdio_write(tp, 0x19, 0x63f8);
        mdio_write(tp, 0x15, 0x0040);
        mdio_write(tp, 0x19, 0x64a1);
        mdio_write(tp, 0x15, 0x0041);
        mdio_write(tp, 0x19, 0x30de);
        mdio_write(tp, 0x15, 0x0044);
        mdio_write(tp, 0x19, 0x480f);
        mdio_write(tp, 0x15, 0x0045);
        mdio_write(tp, 0x19, 0x6800);
        mdio_write(tp, 0x15, 0x0046);
        mdio_write(tp, 0x19, 0x6680);
        mdio_write(tp, 0x15, 0x0047);
        mdio_write(tp, 0x19, 0x7c10);
        mdio_write(tp, 0x15, 0x0048);
        mdio_write(tp, 0x19, 0x63c8);
        mdio_write(tp, 0x15, 0x0049);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x004a);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x004b);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x004c);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x004d);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x004e);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x004f);
        mdio_write(tp, 0x19, 0x40ea);
        mdio_write(tp, 0x15, 0x0050);
        mdio_write(tp, 0x19, 0x4503);
        mdio_write(tp, 0x15, 0x0051);
        mdio_write(tp, 0x19, 0x58ca);
        mdio_write(tp, 0x15, 0x0052);
        mdio_write(tp, 0x19, 0x63c8);
        mdio_write(tp, 0x15, 0x0053);
        mdio_write(tp, 0x19, 0x63d8);
        mdio_write(tp, 0x15, 0x0054);
        mdio_write(tp, 0x19, 0x66a0);
        mdio_write(tp, 0x15, 0x0055);
        mdio_write(tp, 0x19, 0x9f00);
        mdio_write(tp, 0x15, 0x0056);
        mdio_write(tp, 0x19, 0x3000);
        mdio_write(tp, 0x15, 0x006E);
        mdio_write(tp, 0x19, 0x9afa);
        mdio_write(tp, 0x15, 0x00a1);
        mdio_write(tp, 0x19, 0x3044);
        mdio_write(tp, 0x15, 0x00ab);
        mdio_write(tp, 0x19, 0x5820);
        mdio_write(tp, 0x15, 0x00ac);
        mdio_write(tp, 0x19, 0x5e04);
        mdio_write(tp, 0x15, 0x00ad);
        mdio_write(tp, 0x19, 0xb60c);
        mdio_write(tp, 0x15, 0x00af);
        mdio_write(tp, 0x19, 0x000a);
        mdio_write(tp, 0x15, 0x00b2);
        mdio_write(tp, 0x19, 0x30b9);
        mdio_write(tp, 0x15, 0x00b9);
        mdio_write(tp, 0x19, 0x4408);
        mdio_write(tp, 0x15, 0x00ba);
        mdio_write(tp, 0x19, 0x480b);
        mdio_write(tp, 0x15, 0x00bb);
        mdio_write(tp, 0x19, 0x5e00);
        mdio_write(tp, 0x15, 0x00bc);
        mdio_write(tp, 0x19, 0x405f);
        mdio_write(tp, 0x15, 0x00bd);
        mdio_write(tp, 0x19, 0x4448);
        mdio_write(tp, 0x15, 0x00be);
        mdio_write(tp, 0x19, 0x4020);
        mdio_write(tp, 0x15, 0x00bf);
        mdio_write(tp, 0x19, 0x4468);
        mdio_write(tp, 0x15, 0x00c0);
        mdio_write(tp, 0x19, 0x9c02);
        mdio_write(tp, 0x15, 0x00c1);
        mdio_write(tp, 0x19, 0x58a0);
        mdio_write(tp, 0x15, 0x00c2);
        mdio_write(tp, 0x19, 0xb605);
        mdio_write(tp, 0x15, 0x00c3);
        mdio_write(tp, 0x19, 0xc0d3);
        mdio_write(tp, 0x15, 0x00c4);
        mdio_write(tp, 0x19, 0x00e6);
        mdio_write(tp, 0x15, 0x00c5);
        mdio_write(tp, 0x19, 0xdaec);
        mdio_write(tp, 0x15, 0x00c6);
        mdio_write(tp, 0x19, 0x00fa);
        mdio_write(tp, 0x15, 0x00c7);
        mdio_write(tp, 0x19, 0x9df9);
        mdio_write(tp, 0x15, 0x00c8);
        mdio_write(tp, 0x19, 0x307a);
        mdio_write(tp, 0x15, 0x0112);
        mdio_write(tp, 0x19, 0x6421);
        mdio_write(tp, 0x15, 0x0113);
        mdio_write(tp, 0x19, 0x7c08);
        mdio_write(tp, 0x15, 0x0114);
        mdio_write(tp, 0x19, 0x63f0);
        mdio_write(tp, 0x15, 0x0115);
        mdio_write(tp, 0x19, 0x4003);
        mdio_write(tp, 0x15, 0x0116);
        mdio_write(tp, 0x19, 0x4418);
        mdio_write(tp, 0x15, 0x0117);
        mdio_write(tp, 0x19, 0x9b00);
        mdio_write(tp, 0x15, 0x0118);
        mdio_write(tp, 0x19, 0x6461);
        mdio_write(tp, 0x15, 0x0119);
        mdio_write(tp, 0x19, 0x64e1);
        mdio_write(tp, 0x15, 0x011a);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x0150);
        mdio_write(tp, 0x19, 0x7c80);
        mdio_write(tp, 0x15, 0x0151);
        mdio_write(tp, 0x19, 0x6461);
        mdio_write(tp, 0x15, 0x0152);
        mdio_write(tp, 0x19, 0x4003);
        mdio_write(tp, 0x15, 0x0153);
        mdio_write(tp, 0x19, 0x4540);
        mdio_write(tp, 0x15, 0x0154);
        mdio_write(tp, 0x19, 0x9f00);
        mdio_write(tp, 0x15, 0x0155);
        mdio_write(tp, 0x19, 0x9d00);
        mdio_write(tp, 0x15, 0x0156);
        mdio_write(tp, 0x19, 0x7c40);
        mdio_write(tp, 0x15, 0x0157);
        mdio_write(tp, 0x19, 0x6421);
        mdio_write(tp, 0x15, 0x0158);
        mdio_write(tp, 0x19, 0x7c80);
        mdio_write(tp, 0x15, 0x0159);
        mdio_write(tp, 0x19, 0x64a1);
        mdio_write(tp, 0x15, 0x015a);
        mdio_write(tp, 0x19, 0x30fe);
        mdio_write(tp, 0x15, 0x021e);
        mdio_write(tp, 0x19, 0x5410);
        mdio_write(tp, 0x15, 0x0225);
        mdio_write(tp, 0x19, 0x5400);
        mdio_write(tp, 0x15, 0x023D);
        mdio_write(tp, 0x19, 0x4050);
        mdio_write(tp, 0x15, 0x0295);
        mdio_write(tp, 0x19, 0x6c08);
        mdio_write(tp, 0x15, 0x02bd);
        mdio_write(tp, 0x19, 0xa523);
        mdio_write(tp, 0x15, 0x02be);
        mdio_write(tp, 0x19, 0x32ca);
        mdio_write(tp, 0x15, 0x02ca);
        mdio_write(tp, 0x19, 0x48b3);
        mdio_write(tp, 0x15, 0x02cb);
        mdio_write(tp, 0x19, 0x4020);
        mdio_write(tp, 0x15, 0x02cc);
        mdio_write(tp, 0x19, 0x4823);
        mdio_write(tp, 0x15, 0x02cd);
        mdio_write(tp, 0x19, 0x4510);
        mdio_write(tp, 0x15, 0x02ce);
        mdio_write(tp, 0x19, 0xb63a);
        mdio_write(tp, 0x15, 0x02cf);
        mdio_write(tp, 0x19, 0x7dc8);
        mdio_write(tp, 0x15, 0x02d6);
        mdio_write(tp, 0x19, 0x9bf8);
        mdio_write(tp, 0x15, 0x02d8);
        mdio_write(tp, 0x19, 0x85f6);
        mdio_write(tp, 0x15, 0x02d9);
        mdio_write(tp, 0x19, 0x32e0);
        mdio_write(tp, 0x15, 0x02e0);
        mdio_write(tp, 0x19, 0x4834);
        mdio_write(tp, 0x15, 0x02e1);
        mdio_write(tp, 0x19, 0x6c08);
        mdio_write(tp, 0x15, 0x02e2);
        mdio_write(tp, 0x19, 0x4020);
        mdio_write(tp, 0x15, 0x02e3);
        mdio_write(tp, 0x19, 0x4824);
        mdio_write(tp, 0x15, 0x02e4);
        mdio_write(tp, 0x19, 0x4520);
        mdio_write(tp, 0x15, 0x02e5);
        mdio_write(tp, 0x19, 0x4008);
        mdio_write(tp, 0x15, 0x02e6);
        mdio_write(tp, 0x19, 0x4560);
        mdio_write(tp, 0x15, 0x02e7);
        mdio_write(tp, 0x19, 0x9d04);
        mdio_write(tp, 0x15, 0x02e8);
        mdio_write(tp, 0x19, 0x48c4);
        mdio_write(tp, 0x15, 0x02e9);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x02ea);
        mdio_write(tp, 0x19, 0x4844);
        mdio_write(tp, 0x15, 0x02eb);
        mdio_write(tp, 0x19, 0x7dc8);
        mdio_write(tp, 0x15, 0x02f0);
        mdio_write(tp, 0x19, 0x9cf7);
        mdio_write(tp, 0x15, 0x02f1);
        mdio_write(tp, 0x19, 0xdf94);
        mdio_write(tp, 0x15, 0x02f2);
        mdio_write(tp, 0x19, 0x0002);
        mdio_write(tp, 0x15, 0x02f3);
        mdio_write(tp, 0x19, 0x6810);
        mdio_write(tp, 0x15, 0x02f4);
        mdio_write(tp, 0x19, 0xb614);
        mdio_write(tp, 0x15, 0x02f5);
        mdio_write(tp, 0x19, 0xc42b);
        mdio_write(tp, 0x15, 0x02f6);
        mdio_write(tp, 0x19, 0x00d4);
        mdio_write(tp, 0x15, 0x02f7);
        mdio_write(tp, 0x19, 0xc455);
        mdio_write(tp, 0x15, 0x02f8);
        mdio_write(tp, 0x19, 0x0093);
        mdio_write(tp, 0x15, 0x02f9);
        mdio_write(tp, 0x19, 0x92ee);
        mdio_write(tp, 0x15, 0x02fa);
        mdio_write(tp, 0x19, 0xefed);
        mdio_write(tp, 0x15, 0x02fb);
        mdio_write(tp, 0x19, 0x3312);
        mdio_write(tp, 0x15, 0x0312);
        mdio_write(tp, 0x19, 0x49b5);
        mdio_write(tp, 0x15, 0x0313);
        mdio_write(tp, 0x19, 0x7d00);
        mdio_write(tp, 0x15, 0x0314);
        mdio_write(tp, 0x19, 0x4d00);
        mdio_write(tp, 0x15, 0x0315);
        mdio_write(tp, 0x19, 0x6810);
        mdio_write(tp, 0x15, 0x031e);
        mdio_write(tp, 0x19, 0x404f);
        mdio_write(tp, 0x15, 0x031f);
        mdio_write(tp, 0x19, 0x44c8);
        mdio_write(tp, 0x15, 0x0320);
        mdio_write(tp, 0x19, 0xd64f);
        mdio_write(tp, 0x15, 0x0321);
        mdio_write(tp, 0x19, 0x00e7);
        mdio_write(tp, 0x15, 0x0322);
        mdio_write(tp, 0x19, 0x7c08);
        mdio_write(tp, 0x15, 0x0323);
        mdio_write(tp, 0x19, 0x8203);
        mdio_write(tp, 0x15, 0x0324);
        mdio_write(tp, 0x19, 0x4d48);
        mdio_write(tp, 0x15, 0x0325);
        mdio_write(tp, 0x19, 0x3327);
        mdio_write(tp, 0x15, 0x0326);
        mdio_write(tp, 0x19, 0x4d40);
        mdio_write(tp, 0x15, 0x0327);
        mdio_write(tp, 0x19, 0xc8d7);
        mdio_write(tp, 0x15, 0x0328);
        mdio_write(tp, 0x19, 0x0003);
        mdio_write(tp, 0x15, 0x0329);
        mdio_write(tp, 0x19, 0x7c20);
        mdio_write(tp, 0x15, 0x032a);
        mdio_write(tp, 0x19, 0x4c20);
        mdio_write(tp, 0x15, 0x032b);
        mdio_write(tp, 0x19, 0xc8ed);
        mdio_write(tp, 0x15, 0x032c);
        mdio_write(tp, 0x19, 0x00f4);
        mdio_write(tp, 0x15, 0x032d);
        mdio_write(tp, 0x19, 0x82b3);
        mdio_write(tp, 0x15, 0x032e);
        mdio_write(tp, 0x19, 0xd11d);
        mdio_write(tp, 0x15, 0x032f);
        mdio_write(tp, 0x19, 0x00b1);
        mdio_write(tp, 0x15, 0x0330);
        mdio_write(tp, 0x19, 0xde18);
        mdio_write(tp, 0x15, 0x0331);
        mdio_write(tp, 0x19, 0x0008);
        mdio_write(tp, 0x15, 0x0332);
        mdio_write(tp, 0x19, 0x91ee);
        mdio_write(tp, 0x15, 0x0333);
        mdio_write(tp, 0x19, 0x3339);
        mdio_write(tp, 0x15, 0x033a);
        mdio_write(tp, 0x19, 0x4064);
        mdio_write(tp, 0x15, 0x0340);
        mdio_write(tp, 0x19, 0x9e06);
        mdio_write(tp, 0x15, 0x0341);
        mdio_write(tp, 0x19, 0x7c08);
        mdio_write(tp, 0x15, 0x0342);
        mdio_write(tp, 0x19, 0x8203);
        mdio_write(tp, 0x15, 0x0343);
        mdio_write(tp, 0x19, 0x4d48);
        mdio_write(tp, 0x15, 0x0344);
        mdio_write(tp, 0x19, 0x3346);
        mdio_write(tp, 0x15, 0x0345);
        mdio_write(tp, 0x19, 0x4d40);
        mdio_write(tp, 0x15, 0x0346);
        mdio_write(tp, 0x19, 0xd11d);
        mdio_write(tp, 0x15, 0x0347);
        mdio_write(tp, 0x19, 0x0099);
        mdio_write(tp, 0x15, 0x0348);
        mdio_write(tp, 0x19, 0xbb17);
        mdio_write(tp, 0x15, 0x0349);
        mdio_write(tp, 0x19, 0x8102);
        mdio_write(tp, 0x15, 0x034a);
        mdio_write(tp, 0x19, 0x334d);
        mdio_write(tp, 0x15, 0x034b);
        mdio_write(tp, 0x19, 0xa22c);
        mdio_write(tp, 0x15, 0x034c);
        mdio_write(tp, 0x19, 0x3397);
        mdio_write(tp, 0x15, 0x034d);
        mdio_write(tp, 0x19, 0x91f2);
        mdio_write(tp, 0x15, 0x034e);
        mdio_write(tp, 0x19, 0xc218);
        mdio_write(tp, 0x15, 0x034f);
        mdio_write(tp, 0x19, 0x00f0);
        mdio_write(tp, 0x15, 0x0350);
        mdio_write(tp, 0x19, 0x3397);
        mdio_write(tp, 0x15, 0x0351);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x0364);
        mdio_write(tp, 0x19, 0xbc05);
        mdio_write(tp, 0x15, 0x0367);
        mdio_write(tp, 0x19, 0xa1fc);
        mdio_write(tp, 0x15, 0x0368);
        mdio_write(tp, 0x19, 0x3377);
        mdio_write(tp, 0x15, 0x0369);
        mdio_write(tp, 0x19, 0x328b);
        mdio_write(tp, 0x15, 0x036a);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x0377);
        mdio_write(tp, 0x19, 0x4b97);
        mdio_write(tp, 0x15, 0x0378);
        mdio_write(tp, 0x19, 0x6818);
        mdio_write(tp, 0x15, 0x0379);
        mdio_write(tp, 0x19, 0x4b07);
        mdio_write(tp, 0x15, 0x037a);
        mdio_write(tp, 0x19, 0x40ac);
        mdio_write(tp, 0x15, 0x037b);
        mdio_write(tp, 0x19, 0x4445);
        mdio_write(tp, 0x15, 0x037c);
        mdio_write(tp, 0x19, 0x404e);
        mdio_write(tp, 0x15, 0x037d);
        mdio_write(tp, 0x19, 0x4461);
        mdio_write(tp, 0x15, 0x037e);
        mdio_write(tp, 0x19, 0x9c09);
        mdio_write(tp, 0x15, 0x037f);
        mdio_write(tp, 0x19, 0x63da);
        mdio_write(tp, 0x15, 0x0380);
        mdio_write(tp, 0x19, 0x5440);
        mdio_write(tp, 0x15, 0x0381);
        mdio_write(tp, 0x19, 0x4b98);
        mdio_write(tp, 0x15, 0x0382);
        mdio_write(tp, 0x19, 0x7c60);
        mdio_write(tp, 0x15, 0x0383);
        mdio_write(tp, 0x19, 0x4c00);
        mdio_write(tp, 0x15, 0x0384);
        mdio_write(tp, 0x19, 0x4b08);
        mdio_write(tp, 0x15, 0x0385);
        mdio_write(tp, 0x19, 0x63d8);
        mdio_write(tp, 0x15, 0x0386);
        mdio_write(tp, 0x19, 0x338d);
        mdio_write(tp, 0x15, 0x0387);
        mdio_write(tp, 0x19, 0xd64f);
        mdio_write(tp, 0x15, 0x0388);
        mdio_write(tp, 0x19, 0x0080);
        mdio_write(tp, 0x15, 0x0389);
        mdio_write(tp, 0x19, 0x820c);
        mdio_write(tp, 0x15, 0x038a);
        mdio_write(tp, 0x19, 0xa10b);
        mdio_write(tp, 0x15, 0x038b);
        mdio_write(tp, 0x19, 0x9df3);
        mdio_write(tp, 0x15, 0x038c);
        mdio_write(tp, 0x19, 0x3395);
        mdio_write(tp, 0x15, 0x038d);
        mdio_write(tp, 0x19, 0xd64f);
        mdio_write(tp, 0x15, 0x038e);
        mdio_write(tp, 0x19, 0x00f9);
        mdio_write(tp, 0x15, 0x038f);
        mdio_write(tp, 0x19, 0xc017);
        mdio_write(tp, 0x15, 0x0390);
        mdio_write(tp, 0x19, 0x0005);
        mdio_write(tp, 0x15, 0x0391);
        mdio_write(tp, 0x19, 0x6c0b);
        mdio_write(tp, 0x15, 0x0392);
        mdio_write(tp, 0x19, 0xa103);
        mdio_write(tp, 0x15, 0x0393);
        mdio_write(tp, 0x19, 0x6c08);
        mdio_write(tp, 0x15, 0x0394);
        mdio_write(tp, 0x19, 0x9df9);
        mdio_write(tp, 0x15, 0x0395);
        mdio_write(tp, 0x19, 0x6c08);
        mdio_write(tp, 0x15, 0x0396);
        mdio_write(tp, 0x19, 0x3397);
        mdio_write(tp, 0x15, 0x0399);
        mdio_write(tp, 0x19, 0x6810);
        mdio_write(tp, 0x15, 0x03a4);
        mdio_write(tp, 0x19, 0x7c08);
        mdio_write(tp, 0x15, 0x03a5);
        mdio_write(tp, 0x19, 0x8203);
        mdio_write(tp, 0x15, 0x03a6);
        mdio_write(tp, 0x19, 0x4d08);
        mdio_write(tp, 0x15, 0x03a7);
        mdio_write(tp, 0x19, 0x33a9);
        mdio_write(tp, 0x15, 0x03a8);
        mdio_write(tp, 0x19, 0x4d00);
        mdio_write(tp, 0x15, 0x03a9);
        mdio_write(tp, 0x19, 0x9bfa);
        mdio_write(tp, 0x15, 0x03aa);
        mdio_write(tp, 0x19, 0x33b6);
        mdio_write(tp, 0x15, 0x03bb);
        mdio_write(tp, 0x19, 0x4056);
        mdio_write(tp, 0x15, 0x03bc);
        mdio_write(tp, 0x19, 0x44e9);
        mdio_write(tp, 0x15, 0x03bd);
        mdio_write(tp, 0x19, 0x405e);
        mdio_write(tp, 0x15, 0x03be);
        mdio_write(tp, 0x19, 0x44f8);
        mdio_write(tp, 0x15, 0x03bf);
        mdio_write(tp, 0x19, 0xd64f);
        mdio_write(tp, 0x15, 0x03c0);
        mdio_write(tp, 0x19, 0x0037);
        mdio_write(tp, 0x15, 0x03c1);
        mdio_write(tp, 0x19, 0xbd37);
        mdio_write(tp, 0x15, 0x03c2);
        mdio_write(tp, 0x19, 0x9cfd);
        mdio_write(tp, 0x15, 0x03c3);
        mdio_write(tp, 0x19, 0xc639);
        mdio_write(tp, 0x15, 0x03c4);
        mdio_write(tp, 0x19, 0x0011);
        mdio_write(tp, 0x15, 0x03c5);
        mdio_write(tp, 0x19, 0x9b03);
        mdio_write(tp, 0x15, 0x03c6);
        mdio_write(tp, 0x19, 0x7c01);
        mdio_write(tp, 0x15, 0x03c7);
        mdio_write(tp, 0x19, 0x4c01);
        mdio_write(tp, 0x15, 0x03c8);
        mdio_write(tp, 0x19, 0x9e03);
        mdio_write(tp, 0x15, 0x03c9);
        mdio_write(tp, 0x19, 0x7c20);
        mdio_write(tp, 0x15, 0x03ca);
        mdio_write(tp, 0x19, 0x4c20);
        mdio_write(tp, 0x15, 0x03cb);
        mdio_write(tp, 0x19, 0x9af4);
        mdio_write(tp, 0x15, 0x03cc);
        mdio_write(tp, 0x19, 0x7c12);
        mdio_write(tp, 0x15, 0x03cd);
        mdio_write(tp, 0x19, 0x4c52);
        mdio_write(tp, 0x15, 0x03ce);
        mdio_write(tp, 0x19, 0x4470);
        mdio_write(tp, 0x15, 0x03cf);
        mdio_write(tp, 0x19, 0x7c12);
        mdio_write(tp, 0x15, 0x03d0);
        mdio_write(tp, 0x19, 0x4c40);
        mdio_write(tp, 0x15, 0x03d1);
        mdio_write(tp, 0x19, 0x33bf);
        mdio_write(tp, 0x15, 0x03d6);
        mdio_write(tp, 0x19, 0x4047);
        mdio_write(tp, 0x15, 0x03d7);
        mdio_write(tp, 0x19, 0x4469);
        mdio_write(tp, 0x15, 0x03d8);
        mdio_write(tp, 0x19, 0x492b);
        mdio_write(tp, 0x15, 0x03d9);
        mdio_write(tp, 0x19, 0x4479);
        mdio_write(tp, 0x15, 0x03da);
        mdio_write(tp, 0x19, 0x7c09);
        mdio_write(tp, 0x15, 0x03db);
        mdio_write(tp, 0x19, 0x8203);
        mdio_write(tp, 0x15, 0x03dc);
        mdio_write(tp, 0x19, 0x4d48);
        mdio_write(tp, 0x15, 0x03dd);
        mdio_write(tp, 0x19, 0x33df);
        mdio_write(tp, 0x15, 0x03de);
        mdio_write(tp, 0x19, 0x4d40);
        mdio_write(tp, 0x15, 0x03df);
        mdio_write(tp, 0x19, 0xd64f);
        mdio_write(tp, 0x15, 0x03e0);
        mdio_write(tp, 0x19, 0x0017);
        mdio_write(tp, 0x15, 0x03e1);
        mdio_write(tp, 0x19, 0xbd17);
        mdio_write(tp, 0x15, 0x03e2);
        mdio_write(tp, 0x19, 0x9b03);
        mdio_write(tp, 0x15, 0x03e3);
        mdio_write(tp, 0x19, 0x7c20);
        mdio_write(tp, 0x15, 0x03e4);
        mdio_write(tp, 0x19, 0x4c20);
        mdio_write(tp, 0x15, 0x03e5);
        mdio_write(tp, 0x19, 0x88f5);
        mdio_write(tp, 0x15, 0x03e6);
        mdio_write(tp, 0x19, 0xc428);
        mdio_write(tp, 0x15, 0x03e7);
        mdio_write(tp, 0x19, 0x0008);
        mdio_write(tp, 0x15, 0x03e8);
        mdio_write(tp, 0x19, 0x9af2);
        mdio_write(tp, 0x15, 0x03e9);
        mdio_write(tp, 0x19, 0x7c12);
        mdio_write(tp, 0x15, 0x03ea);
        mdio_write(tp, 0x19, 0x4c52);
        mdio_write(tp, 0x15, 0x03eb);
        mdio_write(tp, 0x19, 0x4470);
        mdio_write(tp, 0x15, 0x03ec);
        mdio_write(tp, 0x19, 0x7c12);
        mdio_write(tp, 0x15, 0x03ed);
        mdio_write(tp, 0x19, 0x4c40);
        mdio_write(tp, 0x15, 0x03ee);
        mdio_write(tp, 0x19, 0x33da);
        mdio_write(tp, 0x15, 0x03ef);
        mdio_write(tp, 0x19, 0x3312);
        mdio_write(tp, 0x16, 0x0306);
        mdio_write(tp, 0x16, 0x0300);
        mdio_write(tp, 0x1f, 0x0000);
        mdio_write(tp, 0x17, 0x2179);
        mdio_write(tp, 0x1f, 0x0007);
        mdio_write(tp, 0x1e, 0x0040);
        mdio_write(tp, 0x18, 0x0645);
        mdio_write(tp, 0x19, 0xe200);
        mdio_write(tp, 0x18, 0x0655);
        mdio_write(tp, 0x19, 0x9000);
        mdio_write(tp, 0x18, 0x0d05);
        mdio_write(tp, 0x19, 0xbe00);
        mdio_write(tp, 0x18, 0x0d15);
        mdio_write(tp, 0x19, 0xd300);
        mdio_write(tp, 0x18, 0x0d25);
        mdio_write(tp, 0x19, 0xfe00);
        mdio_write(tp, 0x18, 0x0d35);
        mdio_write(tp, 0x19, 0x4000);
        mdio_write(tp, 0x18, 0x0d45);
        mdio_write(tp, 0x19, 0x7f00);
        mdio_write(tp, 0x18, 0x0d55);
        mdio_write(tp, 0x19, 0x1000);
        mdio_write(tp, 0x18, 0x0d65);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x18, 0x0d75);
        mdio_write(tp, 0x19, 0x8200);
        mdio_write(tp, 0x18, 0x0d85);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x18, 0x0d95);
        mdio_write(tp, 0x19, 0x7000);
        mdio_write(tp, 0x18, 0x0da5);
        mdio_write(tp, 0x19, 0x0f00);
        mdio_write(tp, 0x18, 0x0db5);
        mdio_write(tp, 0x19, 0x0100);
        mdio_write(tp, 0x18, 0x0dc5);
        mdio_write(tp, 0x19, 0x9b00);
        mdio_write(tp, 0x18, 0x0dd5);
        mdio_write(tp, 0x19, 0x7f00);
        mdio_write(tp, 0x18, 0x0de5);
        mdio_write(tp, 0x19, 0xe000);
        mdio_write(tp, 0x18, 0x0df5);
        mdio_write(tp, 0x19, 0xef00);
        mdio_write(tp, 0x18, 0x16d5);
        mdio_write(tp, 0x19, 0xe200);
        mdio_write(tp, 0x18, 0x16e5);
        mdio_write(tp, 0x19, 0xab00);
        mdio_write(tp, 0x18, 0x2904);
        mdio_write(tp, 0x19, 0x4000);
        mdio_write(tp, 0x18, 0x2914);
        mdio_write(tp, 0x19, 0x7f00);
        mdio_write(tp, 0x18, 0x2924);
        mdio_write(tp, 0x19, 0x0100);
        mdio_write(tp, 0x18, 0x2934);
        mdio_write(tp, 0x19, 0x2000);
        mdio_write(tp, 0x18, 0x2944);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x18, 0x2954);
        mdio_write(tp, 0x19, 0x4600);
        mdio_write(tp, 0x18, 0x2964);
        mdio_write(tp, 0x19, 0xfc00);
        mdio_write(tp, 0x18, 0x2974);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x18, 0x2984);
        mdio_write(tp, 0x19, 0x5000);
        mdio_write(tp, 0x18, 0x2994);
        mdio_write(tp, 0x19, 0x9d00);
        mdio_write(tp, 0x18, 0x29a4);
        mdio_write(tp, 0x19, 0xff00);
        mdio_write(tp, 0x18, 0x29b4);
        mdio_write(tp, 0x19, 0x4000);
        mdio_write(tp, 0x18, 0x29c4);
        mdio_write(tp, 0x19, 0x7f00);
        mdio_write(tp, 0x18, 0x29d4);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x18, 0x29e4);
        mdio_write(tp, 0x19, 0x2000);
        mdio_write(tp, 0x18, 0x29f4);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x18, 0x2a04);
        mdio_write(tp, 0x19, 0xe600);
        mdio_write(tp, 0x18, 0x2a14);
        mdio_write(tp, 0x19, 0xff00);
        mdio_write(tp, 0x18, 0x2a24);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x18, 0x2a34);
        mdio_write(tp, 0x19, 0x5000);
        mdio_write(tp, 0x18, 0x2a44);
        mdio_write(tp, 0x19, 0x8500);
        mdio_write(tp, 0x18, 0x2a54);
        mdio_write(tp, 0x19, 0x7f00);
        mdio_write(tp, 0x18, 0x2a64);
        mdio_write(tp, 0x19, 0xac00);
        mdio_write(tp, 0x18, 0x2a74);
        mdio_write(tp, 0x19, 0x0800);
        mdio_write(tp, 0x18, 0x2a84);
        mdio_write(tp, 0x19, 0xfc00);
        mdio_write(tp, 0x18, 0x2a94);
        mdio_write(tp, 0x19, 0xe000);
        mdio_write(tp, 0x18, 0x2aa4);
        mdio_write(tp, 0x19, 0x7400);
        mdio_write(tp, 0x18, 0x2ab4);
        mdio_write(tp, 0x19, 0x4000);
        mdio_write(tp, 0x18, 0x2ac4);
        mdio_write(tp, 0x19, 0x7f00);
        mdio_write(tp, 0x18, 0x2ad4);
        mdio_write(tp, 0x19, 0x0100);
        mdio_write(tp, 0x18, 0x2ae4);
        mdio_write(tp, 0x19, 0xff00);
        mdio_write(tp, 0x18, 0x2af4);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x18, 0x2b04);
        mdio_write(tp, 0x19, 0x4400);
        mdio_write(tp, 0x18, 0x2b14);
        mdio_write(tp, 0x19, 0xfc00);
        mdio_write(tp, 0x18, 0x2b24);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x18, 0x2b34);
        mdio_write(tp, 0x19, 0x4000);
        mdio_write(tp, 0x18, 0x2b44);
        mdio_write(tp, 0x19, 0x9d00);
        mdio_write(tp, 0x18, 0x2b54);
        mdio_write(tp, 0x19, 0xff00);
        mdio_write(tp, 0x18, 0x2b64);
        mdio_write(tp, 0x19, 0x4000);
        mdio_write(tp, 0x18, 0x2b74);
        mdio_write(tp, 0x19, 0x7f00);
        mdio_write(tp, 0x18, 0x2b84);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x18, 0x2b94);
        mdio_write(tp, 0x19, 0xff00);
        mdio_write(tp, 0x18, 0x2ba4);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x18, 0x2bb4);
        mdio_write(tp, 0x19, 0xfc00);
        mdio_write(tp, 0x18, 0x2bc4);
        mdio_write(tp, 0x19, 0xff00);
        mdio_write(tp, 0x18, 0x2bd4);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x18, 0x2be4);
        mdio_write(tp, 0x19, 0x4000);
        mdio_write(tp, 0x18, 0x2bf4);
        mdio_write(tp, 0x19, 0x8900);
        mdio_write(tp, 0x18, 0x2c04);
        mdio_write(tp, 0x19, 0x8300);
        mdio_write(tp, 0x18, 0x2c14);
        mdio_write(tp, 0x19, 0xe000);
        mdio_write(tp, 0x18, 0x2c24);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x18, 0x2c34);
        mdio_write(tp, 0x19, 0xac00);
        mdio_write(tp, 0x18, 0x2c44);
        mdio_write(tp, 0x19, 0x0800);
        mdio_write(tp, 0x18, 0x2c54);
        mdio_write(tp, 0x19, 0xfa00);
        mdio_write(tp, 0x18, 0x2c64);
        mdio_write(tp, 0x19, 0xe100);
        mdio_write(tp, 0x18, 0x2c74);
        mdio_write(tp, 0x19, 0x7f00);
        mdio_write(tp, 0x18, 0x0001);
        mdio_write(tp, 0x1f, 0x0000);
        mdio_write(tp, 0x17, 0x2100);
        mdio_write(tp, 0x1f, 0x0005);
        mdio_write(tp, 0x05, 0xfff6);
        mdio_write(tp, 0x06, 0x0080);
        mdio_write(tp, 0x05, 0x8b88);
        mdio_write(tp, 0x06, 0x0000);
        mdio_write(tp, 0x06, 0x0000);
        mdio_write(tp, 0x06, 0x0000);
        mdio_write(tp, 0x06, 0x0000);
        mdio_write(tp, 0x05, 0x8000);
        mdio_write(tp, 0x06, 0xd480);
        mdio_write(tp, 0x06, 0xc1e4);
        mdio_write(tp, 0x06, 0x8b9a);
        mdio_write(tp, 0x06, 0xe58b);
        mdio_write(tp, 0x06, 0x9bee);
        mdio_write(tp, 0x06, 0x8b83);
        mdio_write(tp, 0x06, 0x41bf);
        mdio_write(tp, 0x06, 0x8b88);
        mdio_write(tp, 0x06, 0xec00);
        mdio_write(tp, 0x06, 0x19a9);
        mdio_write(tp, 0x06, 0x8b90);
        mdio_write(tp, 0x06, 0xf9ee);
        mdio_write(tp, 0x06, 0xfff6);
        mdio_write(tp, 0x06, 0x00ee);
        mdio_write(tp, 0x06, 0xfff7);
        mdio_write(tp, 0x06, 0xffe0);
        mdio_write(tp, 0x06, 0xe140);
        mdio_write(tp, 0x06, 0xe1e1);
        mdio_write(tp, 0x06, 0x41f7);
        mdio_write(tp, 0x06, 0x2ff6);
        mdio_write(tp, 0x06, 0x28e4);
        mdio_write(tp, 0x06, 0xe140);
        mdio_write(tp, 0x06, 0xe5e1);
        mdio_write(tp, 0x06, 0x41f7);
        mdio_write(tp, 0x06, 0x0002);
        mdio_write(tp, 0x06, 0x020c);
        mdio_write(tp, 0x06, 0x0202);
        mdio_write(tp, 0x06, 0x1d02);
        mdio_write(tp, 0x06, 0x0230);
        mdio_write(tp, 0x06, 0x0202);
        mdio_write(tp, 0x06, 0x4002);
        mdio_write(tp, 0x06, 0x028b);
        mdio_write(tp, 0x06, 0x0280);
        mdio_write(tp, 0x06, 0x6c02);
        mdio_write(tp, 0x06, 0x8085);
        mdio_write(tp, 0x06, 0xe08b);
        mdio_write(tp, 0x06, 0x88e1);
        mdio_write(tp, 0x06, 0x8b89);
        mdio_write(tp, 0x06, 0x1e01);
        mdio_write(tp, 0x06, 0xe18b);
        mdio_write(tp, 0x06, 0x8a1e);
        mdio_write(tp, 0x06, 0x01e1);
        mdio_write(tp, 0x06, 0x8b8b);
        mdio_write(tp, 0x06, 0x1e01);
        mdio_write(tp, 0x06, 0xe18b);
        mdio_write(tp, 0x06, 0x8c1e);
        mdio_write(tp, 0x06, 0x01e1);
        mdio_write(tp, 0x06, 0x8b8d);
        mdio_write(tp, 0x06, 0x1e01);
        mdio_write(tp, 0x06, 0xe18b);
        mdio_write(tp, 0x06, 0x8e1e);
        mdio_write(tp, 0x06, 0x01a0);
        mdio_write(tp, 0x06, 0x00c7);
        mdio_write(tp, 0x06, 0xaec3);
        mdio_write(tp, 0x06, 0xf8e0);
        mdio_write(tp, 0x06, 0x8b8d);
        mdio_write(tp, 0x06, 0xad20);
        mdio_write(tp, 0x06, 0x10ee);
        mdio_write(tp, 0x06, 0x8b8d);
        mdio_write(tp, 0x06, 0x0002);
        mdio_write(tp, 0x06, 0x1310);
        mdio_write(tp, 0x06, 0x021f);
        mdio_write(tp, 0x06, 0x9d02);
        mdio_write(tp, 0x06, 0x1f0c);
        mdio_write(tp, 0x06, 0x0227);
        mdio_write(tp, 0x06, 0x49fc);
        mdio_write(tp, 0x06, 0x04f8);
        mdio_write(tp, 0x06, 0xe08b);
        mdio_write(tp, 0x06, 0x8ead);
        mdio_write(tp, 0x06, 0x200b);
        mdio_write(tp, 0x06, 0xf620);
        mdio_write(tp, 0x06, 0xe48b);
        mdio_write(tp, 0x06, 0x8e02);
        mdio_write(tp, 0x06, 0x830e);
        mdio_write(tp, 0x06, 0x021b);
        mdio_write(tp, 0x06, 0x67ad);
        mdio_write(tp, 0x06, 0x2211);
        mdio_write(tp, 0x06, 0xf622);
        mdio_write(tp, 0x06, 0xe48b);
        mdio_write(tp, 0x06, 0x8e02);
        mdio_write(tp, 0x06, 0x2ba5);
        mdio_write(tp, 0x06, 0x022a);
        mdio_write(tp, 0x06, 0x2402);
        mdio_write(tp, 0x06, 0x80c6);
        mdio_write(tp, 0x06, 0x022a);
        mdio_write(tp, 0x06, 0xf0ad);
        mdio_write(tp, 0x06, 0x2511);
        mdio_write(tp, 0x06, 0xf625);
        mdio_write(tp, 0x06, 0xe48b);
        mdio_write(tp, 0x06, 0x8e02);
        mdio_write(tp, 0x06, 0x8226);
        mdio_write(tp, 0x06, 0x0204);
        mdio_write(tp, 0x06, 0x0302);
        mdio_write(tp, 0x06, 0x19cc);
        mdio_write(tp, 0x06, 0x022b);
        mdio_write(tp, 0x06, 0x5bfc);
        mdio_write(tp, 0x06, 0x04ee);
        mdio_write(tp, 0x06, 0x8b8d);
        mdio_write(tp, 0x06, 0x0105);
        mdio_write(tp, 0x06, 0xf8e0);
        mdio_write(tp, 0x06, 0x8b83);
        mdio_write(tp, 0x06, 0xad24);
        mdio_write(tp, 0x06, 0x44e0);
        mdio_write(tp, 0x06, 0xe022);
        mdio_write(tp, 0x06, 0xe1e0);
        mdio_write(tp, 0x06, 0x23ad);
        mdio_write(tp, 0x06, 0x223b);
        mdio_write(tp, 0x06, 0xe08a);
        mdio_write(tp, 0x06, 0xbea0);
        mdio_write(tp, 0x06, 0x0005);
        mdio_write(tp, 0x06, 0x0228);
        mdio_write(tp, 0x06, 0xdeae);
        mdio_write(tp, 0x06, 0x42a0);
        mdio_write(tp, 0x06, 0x0105);
        mdio_write(tp, 0x06, 0x0228);
        mdio_write(tp, 0x06, 0xf1ae);
        mdio_write(tp, 0x06, 0x3aa0);
        mdio_write(tp, 0x06, 0x0205);
        mdio_write(tp, 0x06, 0x0281);
        mdio_write(tp, 0x06, 0x25ae);
        mdio_write(tp, 0x06, 0x32a0);
        mdio_write(tp, 0x06, 0x0305);
        mdio_write(tp, 0x06, 0x0229);
        mdio_write(tp, 0x06, 0x9aae);
        mdio_write(tp, 0x06, 0x2aa0);
        mdio_write(tp, 0x06, 0x0405);
        mdio_write(tp, 0x06, 0x0229);
        mdio_write(tp, 0x06, 0xaeae);
        mdio_write(tp, 0x06, 0x22a0);
        mdio_write(tp, 0x06, 0x0505);
        mdio_write(tp, 0x06, 0x0229);
        mdio_write(tp, 0x06, 0xd7ae);
        mdio_write(tp, 0x06, 0x1aa0);
        mdio_write(tp, 0x06, 0x0605);
        mdio_write(tp, 0x06, 0x0229);
        mdio_write(tp, 0x06, 0xfeae);
        mdio_write(tp, 0x06, 0x12ee);
        mdio_write(tp, 0x06, 0x8ac0);
        mdio_write(tp, 0x06, 0x00ee);
        mdio_write(tp, 0x06, 0x8ac1);
        mdio_write(tp, 0x06, 0x00ee);
        mdio_write(tp, 0x06, 0x8ac6);
        mdio_write(tp, 0x06, 0x00ee);
        mdio_write(tp, 0x06, 0x8abe);
        mdio_write(tp, 0x06, 0x00ae);
        mdio_write(tp, 0x06, 0x00fc);
        mdio_write(tp, 0x06, 0x04f8);
        mdio_write(tp, 0x06, 0x022a);
        mdio_write(tp, 0x06, 0x67e0);
        mdio_write(tp, 0x06, 0xe022);
        mdio_write(tp, 0x06, 0xe1e0);
        mdio_write(tp, 0x06, 0x230d);
        mdio_write(tp, 0x06, 0x0658);
        mdio_write(tp, 0x06, 0x03a0);
        mdio_write(tp, 0x06, 0x0202);
        mdio_write(tp, 0x06, 0xae2d);
        mdio_write(tp, 0x06, 0xa001);
        mdio_write(tp, 0x06, 0x02ae);
        mdio_write(tp, 0x06, 0x2da0);
        mdio_write(tp, 0x06, 0x004d);
        mdio_write(tp, 0x06, 0xe0e2);
        mdio_write(tp, 0x06, 0x00e1);
        mdio_write(tp, 0x06, 0xe201);
        mdio_write(tp, 0x06, 0xad24);
        mdio_write(tp, 0x06, 0x44e0);
        mdio_write(tp, 0x06, 0x8ac2);
        mdio_write(tp, 0x06, 0xe48a);
        mdio_write(tp, 0x06, 0xc4e0);
        mdio_write(tp, 0x06, 0x8ac3);
        mdio_write(tp, 0x06, 0xe48a);
        mdio_write(tp, 0x06, 0xc5ee);
        mdio_write(tp, 0x06, 0x8abe);
        mdio_write(tp, 0x06, 0x03e0);
        mdio_write(tp, 0x06, 0x8b83);
        mdio_write(tp, 0x06, 0xad25);
        mdio_write(tp, 0x06, 0x3aee);
        mdio_write(tp, 0x06, 0x8abe);
        mdio_write(tp, 0x06, 0x05ae);
        mdio_write(tp, 0x06, 0x34e0);
        mdio_write(tp, 0x06, 0x8ace);
        mdio_write(tp, 0x06, 0xae03);
        mdio_write(tp, 0x06, 0xe08a);
        mdio_write(tp, 0x06, 0xcfe1);
        mdio_write(tp, 0x06, 0x8ac2);
        mdio_write(tp, 0x06, 0x4905);
        mdio_write(tp, 0x06, 0xe58a);
        mdio_write(tp, 0x06, 0xc4e1);
        mdio_write(tp, 0x06, 0x8ac3);
        mdio_write(tp, 0x06, 0x4905);
        mdio_write(tp, 0x06, 0xe58a);
        mdio_write(tp, 0x06, 0xc5ee);
        mdio_write(tp, 0x06, 0x8abe);
        mdio_write(tp, 0x06, 0x0502);
        mdio_write(tp, 0x06, 0x2ab6);
        mdio_write(tp, 0x06, 0xac20);
        mdio_write(tp, 0x06, 0x1202);
        mdio_write(tp, 0x06, 0x819b);
        mdio_write(tp, 0x06, 0xac20);
        mdio_write(tp, 0x06, 0x0cee);
        mdio_write(tp, 0x06, 0x8ac1);
        mdio_write(tp, 0x06, 0x00ee);
        mdio_write(tp, 0x06, 0x8ac6);
        mdio_write(tp, 0x06, 0x00ee);
        mdio_write(tp, 0x06, 0x8abe);
        mdio_write(tp, 0x06, 0x02fc);
        mdio_write(tp, 0x06, 0x04d0);
        mdio_write(tp, 0x06, 0x0002);
        mdio_write(tp, 0x06, 0x81ad);
        mdio_write(tp, 0x06, 0x590f);
        mdio_write(tp, 0x06, 0x3902);
        mdio_write(tp, 0x06, 0xaa04);
        mdio_write(tp, 0x06, 0xd001);
        mdio_write(tp, 0x06, 0xae02);
        mdio_write(tp, 0x06, 0xd000);
        mdio_write(tp, 0x06, 0x04f9);
        mdio_write(tp, 0x06, 0xfae2);
        mdio_write(tp, 0x06, 0xe2d2);
        mdio_write(tp, 0x06, 0xe3e2);
        mdio_write(tp, 0x06, 0xd3f9);
        mdio_write(tp, 0x06, 0x5af7);
        mdio_write(tp, 0x06, 0xe6e2);
        mdio_write(tp, 0x06, 0xd2e7);
        mdio_write(tp, 0x06, 0xe2d3);
        mdio_write(tp, 0x06, 0xe2e0);
        mdio_write(tp, 0x06, 0x2ce3);
        mdio_write(tp, 0x06, 0xe02d);
        mdio_write(tp, 0x06, 0xf95b);
        mdio_write(tp, 0x06, 0xe01e);
        mdio_write(tp, 0x06, 0x30e6);
        mdio_write(tp, 0x06, 0xe02c);
        mdio_write(tp, 0x06, 0xe7e0);
        mdio_write(tp, 0x06, 0x2de2);
        mdio_write(tp, 0x06, 0xe2cc);
        mdio_write(tp, 0x06, 0xe3e2);
        mdio_write(tp, 0x06, 0xcdf9);
        mdio_write(tp, 0x06, 0x5a0f);
        mdio_write(tp, 0x06, 0x6a50);
        mdio_write(tp, 0x06, 0xe6e2);
        mdio_write(tp, 0x06, 0xcce7);
        mdio_write(tp, 0x06, 0xe2cd);
        mdio_write(tp, 0x06, 0xe0e0);
        mdio_write(tp, 0x06, 0x3ce1);
        mdio_write(tp, 0x06, 0xe03d);
        mdio_write(tp, 0x06, 0xef64);
        mdio_write(tp, 0x06, 0xfde0);
        mdio_write(tp, 0x06, 0xe2cc);
        mdio_write(tp, 0x06, 0xe1e2);
        mdio_write(tp, 0x06, 0xcd58);
        mdio_write(tp, 0x06, 0x0f5a);
        mdio_write(tp, 0x06, 0xf01e);
        mdio_write(tp, 0x06, 0x02e4);
        mdio_write(tp, 0x06, 0xe2cc);
        mdio_write(tp, 0x06, 0xe5e2);
        mdio_write(tp, 0x06, 0xcdfd);
        mdio_write(tp, 0x06, 0xe0e0);
        mdio_write(tp, 0x06, 0x2ce1);
        mdio_write(tp, 0x06, 0xe02d);
        mdio_write(tp, 0x06, 0x59e0);
        mdio_write(tp, 0x06, 0x5b1f);
        mdio_write(tp, 0x06, 0x1e13);
        mdio_write(tp, 0x06, 0xe4e0);
        mdio_write(tp, 0x06, 0x2ce5);
        mdio_write(tp, 0x06, 0xe02d);
        mdio_write(tp, 0x06, 0xfde0);
        mdio_write(tp, 0x06, 0xe2d2);
        mdio_write(tp, 0x06, 0xe1e2);
        mdio_write(tp, 0x06, 0xd358);
        mdio_write(tp, 0x06, 0xf75a);
        mdio_write(tp, 0x06, 0x081e);
        mdio_write(tp, 0x06, 0x02e4);
        mdio_write(tp, 0x06, 0xe2d2);
        mdio_write(tp, 0x06, 0xe5e2);
        mdio_write(tp, 0x06, 0xd3ef);
        mdio_write(tp, 0x06, 0x46fe);
        mdio_write(tp, 0x06, 0xfd04);
        mdio_write(tp, 0x06, 0xf8f9);
        mdio_write(tp, 0x06, 0xfaef);
        mdio_write(tp, 0x06, 0x69e0);
        mdio_write(tp, 0x06, 0xe022);
        mdio_write(tp, 0x06, 0xe1e0);
        mdio_write(tp, 0x06, 0x2358);
        mdio_write(tp, 0x06, 0xc4e1);
        mdio_write(tp, 0x06, 0x8b6e);
        mdio_write(tp, 0x06, 0x1f10);
        mdio_write(tp, 0x06, 0x9e58);
        mdio_write(tp, 0x06, 0xe48b);
        mdio_write(tp, 0x06, 0x6ead);
        mdio_write(tp, 0x06, 0x2222);
        mdio_write(tp, 0x06, 0xac27);
        mdio_write(tp, 0x06, 0x55ac);
        mdio_write(tp, 0x06, 0x2602);
        mdio_write(tp, 0x06, 0xae1a);
        mdio_write(tp, 0x06, 0xd106);
        mdio_write(tp, 0x06, 0xbf3b);
        mdio_write(tp, 0x06, 0xba02);
        mdio_write(tp, 0x06, 0x2dc1);
        mdio_write(tp, 0x06, 0xd107);
        mdio_write(tp, 0x06, 0xbf3b);
        mdio_write(tp, 0x06, 0xbd02);
        mdio_write(tp, 0x06, 0x2dc1);
        mdio_write(tp, 0x06, 0xd107);
        mdio_write(tp, 0x06, 0xbf3b);
        mdio_write(tp, 0x06, 0xc002);
        mdio_write(tp, 0x06, 0x2dc1);
        mdio_write(tp, 0x06, 0xae30);
        mdio_write(tp, 0x06, 0xd103);
        mdio_write(tp, 0x06, 0xbf3b);
        mdio_write(tp, 0x06, 0xc302);
        mdio_write(tp, 0x06, 0x2dc1);
        mdio_write(tp, 0x06, 0xd100);
        mdio_write(tp, 0x06, 0xbf3b);
        mdio_write(tp, 0x06, 0xc602);
        mdio_write(tp, 0x06, 0x2dc1);
        mdio_write(tp, 0x06, 0xd100);
        mdio_write(tp, 0x06, 0xbf82);
        mdio_write(tp, 0x06, 0xca02);
        mdio_write(tp, 0x06, 0x2dc1);
        mdio_write(tp, 0x06, 0xd10f);
        mdio_write(tp, 0x06, 0xbf3b);
        mdio_write(tp, 0x06, 0xba02);
        mdio_write(tp, 0x06, 0x2dc1);
        mdio_write(tp, 0x06, 0xd101);
        mdio_write(tp, 0x06, 0xbf3b);
        mdio_write(tp, 0x06, 0xbd02);
        mdio_write(tp, 0x06, 0x2dc1);
        mdio_write(tp, 0x06, 0xd101);
        mdio_write(tp, 0x06, 0xbf3b);
        mdio_write(tp, 0x06, 0xc002);
        mdio_write(tp, 0x06, 0x2dc1);
        mdio_write(tp, 0x06, 0xef96);
        mdio_write(tp, 0x06, 0xfefd);
        mdio_write(tp, 0x06, 0xfc04);
        mdio_write(tp, 0x06, 0xd100);
        mdio_write(tp, 0x06, 0xbf3b);
        mdio_write(tp, 0x06, 0xc302);
        mdio_write(tp, 0x06, 0x2dc1);
        mdio_write(tp, 0x06, 0xd011);
        mdio_write(tp, 0x06, 0x022b);
        mdio_write(tp, 0x06, 0xfb59);
        mdio_write(tp, 0x06, 0x03ef);
        mdio_write(tp, 0x06, 0x01d1);
        mdio_write(tp, 0x06, 0x00a0);
        mdio_write(tp, 0x06, 0x0002);
        mdio_write(tp, 0x06, 0xd101);
        mdio_write(tp, 0x06, 0xbf3b);
        mdio_write(tp, 0x06, 0xc602);
        mdio_write(tp, 0x06, 0x2dc1);
        mdio_write(tp, 0x06, 0xd111);
        mdio_write(tp, 0x06, 0xad20);
        mdio_write(tp, 0x06, 0x020c);
        mdio_write(tp, 0x06, 0x11ad);
        mdio_write(tp, 0x06, 0x2102);
        mdio_write(tp, 0x06, 0x0c12);
        mdio_write(tp, 0x06, 0xbf82);
        mdio_write(tp, 0x06, 0xca02);
        mdio_write(tp, 0x06, 0x2dc1);
        mdio_write(tp, 0x06, 0xaec8);
        mdio_write(tp, 0x06, 0x70e4);
        mdio_write(tp, 0x06, 0x2602);
        mdio_write(tp, 0x06, 0x82d1);
        mdio_write(tp, 0x06, 0x05f8);
        mdio_write(tp, 0x06, 0xfaef);
        mdio_write(tp, 0x06, 0x69e0);
        mdio_write(tp, 0x06, 0xe2fe);
        mdio_write(tp, 0x06, 0xe1e2);
        mdio_write(tp, 0x06, 0xffad);
        mdio_write(tp, 0x06, 0x2d1a);
        mdio_write(tp, 0x06, 0xe0e1);
        mdio_write(tp, 0x06, 0x4ee1);
        mdio_write(tp, 0x06, 0xe14f);
        mdio_write(tp, 0x06, 0xac2d);
        mdio_write(tp, 0x06, 0x22f6);
        mdio_write(tp, 0x06, 0x0302);
        mdio_write(tp, 0x06, 0x033b);
        mdio_write(tp, 0x06, 0xf703);
        mdio_write(tp, 0x06, 0xf706);
        mdio_write(tp, 0x06, 0xbf84);
        mdio_write(tp, 0x06, 0x4402);
        mdio_write(tp, 0x06, 0x2d21);
        mdio_write(tp, 0x06, 0xae11);
        mdio_write(tp, 0x06, 0xe0e1);
        mdio_write(tp, 0x06, 0x4ee1);
        mdio_write(tp, 0x06, 0xe14f);
        mdio_write(tp, 0x06, 0xad2d);
        mdio_write(tp, 0x06, 0x08bf);
        mdio_write(tp, 0x06, 0x844f);
        mdio_write(tp, 0x06, 0x022d);
        mdio_write(tp, 0x06, 0x21f6);
        mdio_write(tp, 0x06, 0x06ef);
        mdio_write(tp, 0x06, 0x96fe);
        mdio_write(tp, 0x06, 0xfc04);
        mdio_write(tp, 0x06, 0xf8fa);
        mdio_write(tp, 0x06, 0xef69);
        mdio_write(tp, 0x06, 0x0283);
        mdio_write(tp, 0x06, 0x4502);
        mdio_write(tp, 0x06, 0x83a2);
        mdio_write(tp, 0x06, 0xe0e0);
        mdio_write(tp, 0x06, 0x00e1);
        mdio_write(tp, 0x06, 0xe001);
        mdio_write(tp, 0x06, 0xad27);
        mdio_write(tp, 0x06, 0x1fd1);
        mdio_write(tp, 0x06, 0x01bf);
        mdio_write(tp, 0x06, 0x843b);
        mdio_write(tp, 0x06, 0x022d);
        mdio_write(tp, 0x06, 0xc1e0);
        mdio_write(tp, 0x06, 0xe020);
        mdio_write(tp, 0x06, 0xe1e0);
        mdio_write(tp, 0x06, 0x21ad);
        mdio_write(tp, 0x06, 0x200e);
        mdio_write(tp, 0x06, 0xd100);
        mdio_write(tp, 0x06, 0xbf84);
        mdio_write(tp, 0x06, 0x3b02);
        mdio_write(tp, 0x06, 0x2dc1);
        mdio_write(tp, 0x06, 0xbf3b);
        mdio_write(tp, 0x06, 0x9602);
        mdio_write(tp, 0x06, 0x2d21);
        mdio_write(tp, 0x06, 0xef96);
        mdio_write(tp, 0x06, 0xfefc);
        mdio_write(tp, 0x06, 0x04f8);
        mdio_write(tp, 0x06, 0xf9fa);
        mdio_write(tp, 0x06, 0xef69);
        mdio_write(tp, 0x06, 0xe08b);
        mdio_write(tp, 0x06, 0x87ad);
        mdio_write(tp, 0x06, 0x204c);
        mdio_write(tp, 0x06, 0xd200);
        mdio_write(tp, 0x06, 0xe0e2);
        mdio_write(tp, 0x06, 0x0058);
        mdio_write(tp, 0x06, 0x010c);
        mdio_write(tp, 0x06, 0x021e);
        mdio_write(tp, 0x06, 0x20e0);
        mdio_write(tp, 0x06, 0xe000);
        mdio_write(tp, 0x06, 0x5810);
        mdio_write(tp, 0x06, 0x1e20);
        mdio_write(tp, 0x06, 0xe0e0);
        mdio_write(tp, 0x06, 0x3658);
        mdio_write(tp, 0x06, 0x031e);
        mdio_write(tp, 0x06, 0x20e0);
        mdio_write(tp, 0x06, 0xe022);
        mdio_write(tp, 0x06, 0xe1e0);
        mdio_write(tp, 0x06, 0x2358);
        mdio_write(tp, 0x06, 0xe01e);
        mdio_write(tp, 0x06, 0x20e0);
        mdio_write(tp, 0x06, 0x8b64);
        mdio_write(tp, 0x06, 0x1f02);
        mdio_write(tp, 0x06, 0x9e22);
        mdio_write(tp, 0x06, 0xe68b);
        mdio_write(tp, 0x06, 0x64ad);
        mdio_write(tp, 0x06, 0x3214);
        mdio_write(tp, 0x06, 0xad34);
        mdio_write(tp, 0x06, 0x11ef);
        mdio_write(tp, 0x06, 0x0258);
        mdio_write(tp, 0x06, 0x039e);
        mdio_write(tp, 0x06, 0x07ad);
        mdio_write(tp, 0x06, 0x3508);
        mdio_write(tp, 0x06, 0x5ac0);
        mdio_write(tp, 0x06, 0x9f04);
        mdio_write(tp, 0x06, 0xd101);
        mdio_write(tp, 0x06, 0xae02);
        mdio_write(tp, 0x06, 0xd100);
        mdio_write(tp, 0x06, 0xbf84);
        mdio_write(tp, 0x06, 0x3e02);
        mdio_write(tp, 0x06, 0x2dc1);
        mdio_write(tp, 0x06, 0xef96);
        mdio_write(tp, 0x06, 0xfefd);
        mdio_write(tp, 0x06, 0xfc04);
        mdio_write(tp, 0x06, 0xf8f9);
        mdio_write(tp, 0x06, 0xfbe0);
        mdio_write(tp, 0x06, 0x8b85);
        mdio_write(tp, 0x06, 0xad25);
        mdio_write(tp, 0x06, 0x22e0);
        mdio_write(tp, 0x06, 0xe022);
        mdio_write(tp, 0x06, 0xe1e0);
        mdio_write(tp, 0x06, 0x23e2);
        mdio_write(tp, 0x06, 0xe036);
        mdio_write(tp, 0x06, 0xe3e0);
        mdio_write(tp, 0x06, 0x375a);
        mdio_write(tp, 0x06, 0xc40d);
        mdio_write(tp, 0x06, 0x0158);
        mdio_write(tp, 0x06, 0x021e);
        mdio_write(tp, 0x06, 0x20e3);
        mdio_write(tp, 0x06, 0x8ae7);
        mdio_write(tp, 0x06, 0xac31);
        mdio_write(tp, 0x06, 0x60ac);
        mdio_write(tp, 0x06, 0x3a08);
        mdio_write(tp, 0x06, 0xac3e);
        mdio_write(tp, 0x06, 0x26ae);
        mdio_write(tp, 0x06, 0x67af);
        mdio_write(tp, 0x06, 0x8437);
        mdio_write(tp, 0x06, 0xad37);
        mdio_write(tp, 0x06, 0x61e0);
        mdio_write(tp, 0x06, 0x8ae8);
        mdio_write(tp, 0x06, 0x10e4);
        mdio_write(tp, 0x06, 0x8ae8);
        mdio_write(tp, 0x06, 0xe18a);
        mdio_write(tp, 0x06, 0xe91b);
        mdio_write(tp, 0x06, 0x109e);
        mdio_write(tp, 0x06, 0x02ae);
        mdio_write(tp, 0x06, 0x51d1);
        mdio_write(tp, 0x06, 0x00bf);
        mdio_write(tp, 0x06, 0x8441);
        mdio_write(tp, 0x06, 0x022d);
        mdio_write(tp, 0x06, 0xc1ee);
        mdio_write(tp, 0x06, 0x8ae8);
        mdio_write(tp, 0x06, 0x00ae);
        mdio_write(tp, 0x06, 0x43ad);
        mdio_write(tp, 0x06, 0x3627);
        mdio_write(tp, 0x06, 0xe08a);
        mdio_write(tp, 0x06, 0xeee1);
        mdio_write(tp, 0x06, 0x8aef);
        mdio_write(tp, 0x06, 0xef74);
        mdio_write(tp, 0x06, 0xe08a);
        mdio_write(tp, 0x06, 0xeae1);
        mdio_write(tp, 0x06, 0x8aeb);
        mdio_write(tp, 0x06, 0x1b74);
        mdio_write(tp, 0x06, 0x9e2e);
        mdio_write(tp, 0x06, 0x14e4);
        mdio_write(tp, 0x06, 0x8aea);
        mdio_write(tp, 0x06, 0xe58a);
        mdio_write(tp, 0x06, 0xebef);
        mdio_write(tp, 0x06, 0x74e0);
        mdio_write(tp, 0x06, 0x8aee);
        mdio_write(tp, 0x06, 0xe18a);
        mdio_write(tp, 0x06, 0xef1b);
        mdio_write(tp, 0x06, 0x479e);
        mdio_write(tp, 0x06, 0x0fae);
        mdio_write(tp, 0x06, 0x19ee);
        mdio_write(tp, 0x06, 0x8aea);
        mdio_write(tp, 0x06, 0x00ee);
        mdio_write(tp, 0x06, 0x8aeb);
        mdio_write(tp, 0x06, 0x00ae);
        mdio_write(tp, 0x06, 0x0fac);
        mdio_write(tp, 0x06, 0x390c);
        mdio_write(tp, 0x06, 0xd101);
        mdio_write(tp, 0x06, 0xbf84);
        mdio_write(tp, 0x06, 0x4102);
        mdio_write(tp, 0x06, 0x2dc1);
        mdio_write(tp, 0x06, 0xee8a);
        mdio_write(tp, 0x06, 0xe800);
        mdio_write(tp, 0x06, 0xe68a);
        mdio_write(tp, 0x06, 0xe7ff);
        mdio_write(tp, 0x06, 0xfdfc);
        mdio_write(tp, 0x06, 0x0400);
        mdio_write(tp, 0x06, 0xe234);
        mdio_write(tp, 0x06, 0xcce2);
        mdio_write(tp, 0x06, 0x0088);
        mdio_write(tp, 0x06, 0xe200);
        mdio_write(tp, 0x06, 0xa725);
        mdio_write(tp, 0x06, 0xe50a);
        mdio_write(tp, 0x06, 0x1de5);
        mdio_write(tp, 0x06, 0x0a2c);
        mdio_write(tp, 0x06, 0xe50a);
        mdio_write(tp, 0x06, 0x6de5);
        mdio_write(tp, 0x06, 0x0a1d);
        mdio_write(tp, 0x06, 0xe50a);
        mdio_write(tp, 0x06, 0x1ce5);
        mdio_write(tp, 0x06, 0x0a2d);
        mdio_write(tp, 0x06, 0xa755);
        mdio_write(tp, 0x05, 0x8b64);
        mdio_write(tp, 0x06, 0x0000);
        mdio_write(tp, 0x05, 0x8b94);
        mdio_write(tp, 0x06, 0x82cd);
        mdio_write(tp, 0x05, 0x8b85);
        mdio_write(tp, 0x06, 0x2000);
        mdio_write(tp, 0x05, 0x8aee);
        mdio_write(tp, 0x06, 0x03b8);
        mdio_write(tp, 0x05, 0x8ae8);
        mdio_write(tp, 0x06, 0x0002);
        gphy_val = mdio_read(tp, 0x01);
        gphy_val |= BIT_0;
        mdio_write(tp, 0x01, gphy_val);
        gphy_val = mdio_read(tp, 0x00);
        gphy_val |= BIT_0;
        mdio_write(tp, 0x00, gphy_val);
        mdio_write(tp, 0x1f, 0x0000);
        mdio_write(tp, 0x1f, 0x0005);
        for (i = 0; i < 200; i++) {
                udelay(100);
                gphy_val = mdio_read(tp, 0x00);
                if (gphy_val & BIT_7)
                        break;
        }
        mdio_write(tp, 0x1f, 0x0007);
        mdio_write(tp, 0x1e, 0x0023);
        gphy_val = mdio_read(tp, 0x17);
        gphy_val &= ~(BIT_0);
        if (tp->RequiredSecLanDonglePatch)
                gphy_val &= ~(BIT_2);
        mdio_write(tp, 0x17, gphy_val);
        mdio_write(tp, 0x1f, 0x0007);
        mdio_write(tp, 0x1e, 0x0028);
        mdio_write(tp, 0x15, 0x0010);
        mdio_write(tp, 0x1f, 0x0007);
        mdio_write(tp, 0x1e, 0x0041);
        mdio_write(tp, 0x15, 0x0802);
        mdio_write(tp, 0x16, 0x2185);
        mdio_write(tp, 0x1f, 0x0000);
}

static void
rtl8168_set_phy_mcu_8168e_2(struct net_device *dev)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        unsigned int gphy_val,i;

        if (rtl8168_efuse_read(tp, 0x22) == 0x0c) {
                mdio_write(tp, 0x1f, 0x0000);
                mdio_write(tp, 0x00, 0x1800);
                mdio_write(tp, 0x1f, 0x0007);
                mdio_write(tp, 0x1e, 0x0023);
                mdio_write(tp, 0x17, 0x0117);
                mdio_write(tp, 0x1f, 0x0007);
                mdio_write(tp, 0x1E, 0x002C);
                mdio_write(tp, 0x1B, 0x5000);
                mdio_write(tp, 0x1f, 0x0000);
                mdio_write(tp, 0x16, 0x4104);
                for (i = 0; i < 200; i++) {
                        udelay(100);
                        gphy_val = mdio_read(tp, 0x1E);
                        gphy_val &= 0x03FF;
                        if (gphy_val==0x000C)
                                break;
                }
                mdio_write(tp, 0x1f, 0x0005);
                for (i = 0; i < 200; i++) {
                        udelay(100);
                        gphy_val = mdio_read(tp, 0x07);
                        if ((gphy_val & BIT_5) == 0)
                                break;
                }
                gphy_val = mdio_read(tp, 0x07);
                if (gphy_val & BIT_5) {
                        mdio_write(tp, 0x1f, 0x0007);
                        mdio_write(tp, 0x1e, 0x00a1);
                        mdio_write(tp, 0x17, 0x1000);
                        mdio_write(tp, 0x17, 0x0000);
                        mdio_write(tp, 0x17, 0x2000);
                        mdio_write(tp, 0x1e, 0x002f);
                        mdio_write(tp, 0x18, 0x9bfb);
                        mdio_write(tp, 0x1f, 0x0005);
                        mdio_write(tp, 0x07, 0x0000);
                        mdio_write(tp, 0x1f, 0x0000);
                }
                mdio_write(tp, 0x1f, 0x0005);
                mdio_write(tp, 0x05, 0xfff6);
                mdio_write(tp, 0x06, 0x0080);
                gphy_val = mdio_read(tp, 0x00);
                gphy_val &= ~(BIT_7);
                mdio_write(tp, 0x00, gphy_val);
                mdio_write(tp, 0x1f, 0x0002);
                gphy_val = mdio_read(tp, 0x08);
                gphy_val &= ~(BIT_7);
                mdio_write(tp, 0x08, gphy_val);
                mdio_write(tp, 0x1f, 0x0000);

                mdio_write(tp, 0x1f, 0x0007);
                mdio_write(tp, 0x1e, 0x0023);
                mdio_write(tp, 0x16, 0x0306);
                mdio_write(tp, 0x16, 0x0307);
                mdio_write(tp, 0x15, 0x000e);
                mdio_write(tp, 0x19, 0x000a);
                mdio_write(tp, 0x15, 0x0010);
                mdio_write(tp, 0x19, 0x0008);
                mdio_write(tp, 0x15, 0x0018);
                mdio_write(tp, 0x19, 0x4801);
                mdio_write(tp, 0x15, 0x0019);
                mdio_write(tp, 0x19, 0x6801);
                mdio_write(tp, 0x15, 0x001a);
                mdio_write(tp, 0x19, 0x66a1);
                mdio_write(tp, 0x15, 0x001f);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x0020);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x0021);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x0022);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x0023);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x0024);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x0025);
                mdio_write(tp, 0x19, 0x64a1);
                mdio_write(tp, 0x15, 0x0026);
                mdio_write(tp, 0x19, 0x40ea);
                mdio_write(tp, 0x15, 0x0027);
                mdio_write(tp, 0x19, 0x4503);
                mdio_write(tp, 0x15, 0x0028);
                mdio_write(tp, 0x19, 0x9f00);
                mdio_write(tp, 0x15, 0x0029);
                mdio_write(tp, 0x19, 0xa631);
                mdio_write(tp, 0x15, 0x002a);
                mdio_write(tp, 0x19, 0x9717);
                mdio_write(tp, 0x15, 0x002b);
                mdio_write(tp, 0x19, 0x302c);
                mdio_write(tp, 0x15, 0x002c);
                mdio_write(tp, 0x19, 0x4802);
                mdio_write(tp, 0x15, 0x002d);
                mdio_write(tp, 0x19, 0x58da);
                mdio_write(tp, 0x15, 0x002e);
                mdio_write(tp, 0x19, 0x400d);
                mdio_write(tp, 0x15, 0x002f);
                mdio_write(tp, 0x19, 0x4488);
                mdio_write(tp, 0x15, 0x0030);
                mdio_write(tp, 0x19, 0x9e00);
                mdio_write(tp, 0x15, 0x0031);
                mdio_write(tp, 0x19, 0x63c8);
                mdio_write(tp, 0x15, 0x0032);
                mdio_write(tp, 0x19, 0x6481);
                mdio_write(tp, 0x15, 0x0033);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x0034);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x0035);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x0036);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x0037);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x0038);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x0039);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x003a);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x003b);
                mdio_write(tp, 0x19, 0x63e8);
                mdio_write(tp, 0x15, 0x003c);
                mdio_write(tp, 0x19, 0x7d00);
                mdio_write(tp, 0x15, 0x003d);
                mdio_write(tp, 0x19, 0x59d4);
                mdio_write(tp, 0x15, 0x003e);
                mdio_write(tp, 0x19, 0x63f8);
                mdio_write(tp, 0x15, 0x0040);
                mdio_write(tp, 0x19, 0x64a1);
                mdio_write(tp, 0x15, 0x0041);
                mdio_write(tp, 0x19, 0x30de);
                mdio_write(tp, 0x15, 0x0044);
                mdio_write(tp, 0x19, 0x480f);
                mdio_write(tp, 0x15, 0x0045);
                mdio_write(tp, 0x19, 0x6800);
                mdio_write(tp, 0x15, 0x0046);
                mdio_write(tp, 0x19, 0x6680);
                mdio_write(tp, 0x15, 0x0047);
                mdio_write(tp, 0x19, 0x7c10);
                mdio_write(tp, 0x15, 0x0048);
                mdio_write(tp, 0x19, 0x63c8);
                mdio_write(tp, 0x15, 0x0049);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x004a);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x004b);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x004c);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x004d);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x004e);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x004f);
                mdio_write(tp, 0x19, 0x40ea);
                mdio_write(tp, 0x15, 0x0050);
                mdio_write(tp, 0x19, 0x4503);
                mdio_write(tp, 0x15, 0x0051);
                mdio_write(tp, 0x19, 0x58ca);
                mdio_write(tp, 0x15, 0x0052);
                mdio_write(tp, 0x19, 0x63c8);
                mdio_write(tp, 0x15, 0x0053);
                mdio_write(tp, 0x19, 0x63d8);
                mdio_write(tp, 0x15, 0x0054);
                mdio_write(tp, 0x19, 0x66a0);
                mdio_write(tp, 0x15, 0x0055);
                mdio_write(tp, 0x19, 0x9f00);
                mdio_write(tp, 0x15, 0x0056);
                mdio_write(tp, 0x19, 0x3000);
                mdio_write(tp, 0x15, 0x00a1);
                mdio_write(tp, 0x19, 0x3044);
                mdio_write(tp, 0x15, 0x00ab);
                mdio_write(tp, 0x19, 0x5820);
                mdio_write(tp, 0x15, 0x00ac);
                mdio_write(tp, 0x19, 0x5e04);
                mdio_write(tp, 0x15, 0x00ad);
                mdio_write(tp, 0x19, 0xb60c);
                mdio_write(tp, 0x15, 0x00af);
                mdio_write(tp, 0x19, 0x000a);
                mdio_write(tp, 0x15, 0x00b2);
                mdio_write(tp, 0x19, 0x30b9);
                mdio_write(tp, 0x15, 0x00b9);
                mdio_write(tp, 0x19, 0x4408);
                mdio_write(tp, 0x15, 0x00ba);
                mdio_write(tp, 0x19, 0x480b);
                mdio_write(tp, 0x15, 0x00bb);
                mdio_write(tp, 0x19, 0x5e00);
                mdio_write(tp, 0x15, 0x00bc);
                mdio_write(tp, 0x19, 0x405f);
                mdio_write(tp, 0x15, 0x00bd);
                mdio_write(tp, 0x19, 0x4448);
                mdio_write(tp, 0x15, 0x00be);
                mdio_write(tp, 0x19, 0x4020);
                mdio_write(tp, 0x15, 0x00bf);
                mdio_write(tp, 0x19, 0x4468);
                mdio_write(tp, 0x15, 0x00c0);
                mdio_write(tp, 0x19, 0x9c02);
                mdio_write(tp, 0x15, 0x00c1);
                mdio_write(tp, 0x19, 0x58a0);
                mdio_write(tp, 0x15, 0x00c2);
                mdio_write(tp, 0x19, 0xb605);
                mdio_write(tp, 0x15, 0x00c3);
                mdio_write(tp, 0x19, 0xc0d3);
                mdio_write(tp, 0x15, 0x00c4);
                mdio_write(tp, 0x19, 0x00e6);
                mdio_write(tp, 0x15, 0x00c5);
                mdio_write(tp, 0x19, 0xdaec);
                mdio_write(tp, 0x15, 0x00c6);
                mdio_write(tp, 0x19, 0x00fa);
                mdio_write(tp, 0x15, 0x00c7);
                mdio_write(tp, 0x19, 0x9df9);
                mdio_write(tp, 0x15, 0x0112);
                mdio_write(tp, 0x19, 0x6421);
                mdio_write(tp, 0x15, 0x0113);
                mdio_write(tp, 0x19, 0x7c08);
                mdio_write(tp, 0x15, 0x0114);
                mdio_write(tp, 0x19, 0x63f0);
                mdio_write(tp, 0x15, 0x0115);
                mdio_write(tp, 0x19, 0x4003);
                mdio_write(tp, 0x15, 0x0116);
                mdio_write(tp, 0x19, 0x4418);
                mdio_write(tp, 0x15, 0x0117);
                mdio_write(tp, 0x19, 0x9b00);
                mdio_write(tp, 0x15, 0x0118);
                mdio_write(tp, 0x19, 0x6461);
                mdio_write(tp, 0x15, 0x0119);
                mdio_write(tp, 0x19, 0x64e1);
                mdio_write(tp, 0x15, 0x011a);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x0150);
                mdio_write(tp, 0x19, 0x7c80);
                mdio_write(tp, 0x15, 0x0151);
                mdio_write(tp, 0x19, 0x6461);
                mdio_write(tp, 0x15, 0x0152);
                mdio_write(tp, 0x19, 0x4003);
                mdio_write(tp, 0x15, 0x0153);
                mdio_write(tp, 0x19, 0x4540);
                mdio_write(tp, 0x15, 0x0154);
                mdio_write(tp, 0x19, 0x9f00);
                mdio_write(tp, 0x15, 0x0155);
                mdio_write(tp, 0x19, 0x9d00);
                mdio_write(tp, 0x15, 0x0156);
                mdio_write(tp, 0x19, 0x7c40);
                mdio_write(tp, 0x15, 0x0157);
                mdio_write(tp, 0x19, 0x6421);
                mdio_write(tp, 0x15, 0x0158);
                mdio_write(tp, 0x19, 0x7c80);
                mdio_write(tp, 0x15, 0x0159);
                mdio_write(tp, 0x19, 0x64a1);
                mdio_write(tp, 0x15, 0x015a);
                mdio_write(tp, 0x19, 0x30fe);
                mdio_write(tp, 0x15, 0x029c);
                mdio_write(tp, 0x19, 0x0070);
                mdio_write(tp, 0x15, 0x02b2);
                mdio_write(tp, 0x19, 0x005a);
                mdio_write(tp, 0x15, 0x02bd);
                mdio_write(tp, 0x19, 0xa522);
                mdio_write(tp, 0x15, 0x02ce);
                mdio_write(tp, 0x19, 0xb63e);
                mdio_write(tp, 0x15, 0x02d9);
                mdio_write(tp, 0x19, 0x32df);
                mdio_write(tp, 0x15, 0x02df);
                mdio_write(tp, 0x19, 0x4500);
                mdio_write(tp, 0x15, 0x02e7);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x02f4);
                mdio_write(tp, 0x19, 0xb618);
                mdio_write(tp, 0x15, 0x02fb);
                mdio_write(tp, 0x19, 0xb900);
                mdio_write(tp, 0x15, 0x02fc);
                mdio_write(tp, 0x19, 0x49b5);
                mdio_write(tp, 0x15, 0x02fd);
                mdio_write(tp, 0x19, 0x6812);
                mdio_write(tp, 0x15, 0x02fe);
                mdio_write(tp, 0x19, 0x66a0);
                mdio_write(tp, 0x15, 0x02ff);
                mdio_write(tp, 0x19, 0x9900);
                mdio_write(tp, 0x15, 0x0300);
                mdio_write(tp, 0x19, 0x64a0);
                mdio_write(tp, 0x15, 0x0301);
                mdio_write(tp, 0x19, 0x3316);
                mdio_write(tp, 0x15, 0x0308);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x030c);
                mdio_write(tp, 0x19, 0x3000);
                mdio_write(tp, 0x15, 0x0312);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x0313);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x0314);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x0315);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x0316);
                mdio_write(tp, 0x19, 0x49b5);
                mdio_write(tp, 0x15, 0x0317);
                mdio_write(tp, 0x19, 0x7d00);
                mdio_write(tp, 0x15, 0x0318);
                mdio_write(tp, 0x19, 0x4d00);
                mdio_write(tp, 0x15, 0x0319);
                mdio_write(tp, 0x19, 0x6810);
                mdio_write(tp, 0x15, 0x031a);
                mdio_write(tp, 0x19, 0x6c08);
                mdio_write(tp, 0x15, 0x031b);
                mdio_write(tp, 0x19, 0x4925);
                mdio_write(tp, 0x15, 0x031c);
                mdio_write(tp, 0x19, 0x403b);
                mdio_write(tp, 0x15, 0x031d);
                mdio_write(tp, 0x19, 0xa602);
                mdio_write(tp, 0x15, 0x031e);
                mdio_write(tp, 0x19, 0x402f);
                mdio_write(tp, 0x15, 0x031f);
                mdio_write(tp, 0x19, 0x4484);
                mdio_write(tp, 0x15, 0x0320);
                mdio_write(tp, 0x19, 0x40c8);
                mdio_write(tp, 0x15, 0x0321);
                mdio_write(tp, 0x19, 0x44c4);
                mdio_write(tp, 0x15, 0x0322);
                mdio_write(tp, 0x19, 0x404f);
                mdio_write(tp, 0x15, 0x0323);
                mdio_write(tp, 0x19, 0x44c8);
                mdio_write(tp, 0x15, 0x0324);
                mdio_write(tp, 0x19, 0xd64f);
                mdio_write(tp, 0x15, 0x0325);
                mdio_write(tp, 0x19, 0x00e7);
                mdio_write(tp, 0x15, 0x0326);
                mdio_write(tp, 0x19, 0x7c08);
                mdio_write(tp, 0x15, 0x0327);
                mdio_write(tp, 0x19, 0x8203);
                mdio_write(tp, 0x15, 0x0328);
                mdio_write(tp, 0x19, 0x4d48);
                mdio_write(tp, 0x15, 0x0329);
                mdio_write(tp, 0x19, 0x332b);
                mdio_write(tp, 0x15, 0x032a);
                mdio_write(tp, 0x19, 0x4d40);
                mdio_write(tp, 0x15, 0x032c);
                mdio_write(tp, 0x19, 0x00f8);
                mdio_write(tp, 0x15, 0x032d);
                mdio_write(tp, 0x19, 0x82b2);
                mdio_write(tp, 0x15, 0x032f);
                mdio_write(tp, 0x19, 0x00b0);
                mdio_write(tp, 0x15, 0x0332);
                mdio_write(tp, 0x19, 0x91f2);
                mdio_write(tp, 0x15, 0x033f);
                mdio_write(tp, 0x19, 0xb6cd);
                mdio_write(tp, 0x15, 0x0340);
                mdio_write(tp, 0x19, 0x9e01);
                mdio_write(tp, 0x15, 0x0341);
                mdio_write(tp, 0x19, 0xd11d);
                mdio_write(tp, 0x15, 0x0342);
                mdio_write(tp, 0x19, 0x009d);
                mdio_write(tp, 0x15, 0x0343);
                mdio_write(tp, 0x19, 0xbb1c);
                mdio_write(tp, 0x15, 0x0344);
                mdio_write(tp, 0x19, 0x8102);
                mdio_write(tp, 0x15, 0x0345);
                mdio_write(tp, 0x19, 0x3348);
                mdio_write(tp, 0x15, 0x0346);
                mdio_write(tp, 0x19, 0xa231);
                mdio_write(tp, 0x15, 0x0347);
                mdio_write(tp, 0x19, 0x335b);
                mdio_write(tp, 0x15, 0x0348);
                mdio_write(tp, 0x19, 0x91f7);
                mdio_write(tp, 0x15, 0x0349);
                mdio_write(tp, 0x19, 0xc218);
                mdio_write(tp, 0x15, 0x034a);
                mdio_write(tp, 0x19, 0x00f5);
                mdio_write(tp, 0x15, 0x034b);
                mdio_write(tp, 0x19, 0x335b);
                mdio_write(tp, 0x15, 0x034c);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x034d);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x034e);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x034f);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x0350);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x035b);
                mdio_write(tp, 0x19, 0xa23c);
                mdio_write(tp, 0x15, 0x035c);
                mdio_write(tp, 0x19, 0x7c08);
                mdio_write(tp, 0x15, 0x035d);
                mdio_write(tp, 0x19, 0x4c00);
                mdio_write(tp, 0x15, 0x035e);
                mdio_write(tp, 0x19, 0x3397);
                mdio_write(tp, 0x15, 0x0363);
                mdio_write(tp, 0x19, 0xb6a9);
                mdio_write(tp, 0x15, 0x0366);
                mdio_write(tp, 0x19, 0x00f5);
                mdio_write(tp, 0x15, 0x0382);
                mdio_write(tp, 0x19, 0x7c40);
                mdio_write(tp, 0x15, 0x0388);
                mdio_write(tp, 0x19, 0x0084);
                mdio_write(tp, 0x15, 0x0389);
                mdio_write(tp, 0x19, 0xdd17);
                mdio_write(tp, 0x15, 0x038a);
                mdio_write(tp, 0x19, 0x000b);
                mdio_write(tp, 0x15, 0x038b);
                mdio_write(tp, 0x19, 0xa10a);
                mdio_write(tp, 0x15, 0x038c);
                mdio_write(tp, 0x19, 0x337e);
                mdio_write(tp, 0x15, 0x038d);
                mdio_write(tp, 0x19, 0x6c0b);
                mdio_write(tp, 0x15, 0x038e);
                mdio_write(tp, 0x19, 0xa107);
                mdio_write(tp, 0x15, 0x038f);
                mdio_write(tp, 0x19, 0x6c08);
                mdio_write(tp, 0x15, 0x0390);
                mdio_write(tp, 0x19, 0xc017);
                mdio_write(tp, 0x15, 0x0391);
                mdio_write(tp, 0x19, 0x0004);
                mdio_write(tp, 0x15, 0x0392);
                mdio_write(tp, 0x19, 0xd64f);
                mdio_write(tp, 0x15, 0x0393);
                mdio_write(tp, 0x19, 0x00f4);
                mdio_write(tp, 0x15, 0x0397);
                mdio_write(tp, 0x19, 0x4098);
                mdio_write(tp, 0x15, 0x0398);
                mdio_write(tp, 0x19, 0x4408);
                mdio_write(tp, 0x15, 0x0399);
                mdio_write(tp, 0x19, 0x55bf);
                mdio_write(tp, 0x15, 0x039a);
                mdio_write(tp, 0x19, 0x4bb9);
                mdio_write(tp, 0x15, 0x039b);
                mdio_write(tp, 0x19, 0x6810);
                mdio_write(tp, 0x15, 0x039c);
                mdio_write(tp, 0x19, 0x4b29);
                mdio_write(tp, 0x15, 0x039d);
                mdio_write(tp, 0x19, 0x4041);
                mdio_write(tp, 0x15, 0x039e);
                mdio_write(tp, 0x19, 0x442a);
                mdio_write(tp, 0x15, 0x039f);
                mdio_write(tp, 0x19, 0x4029);
                mdio_write(tp, 0x15, 0x03aa);
                mdio_write(tp, 0x19, 0x33b8);
                mdio_write(tp, 0x15, 0x03b6);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x03b7);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x03b8);
                mdio_write(tp, 0x19, 0x543f);
                mdio_write(tp, 0x15, 0x03b9);
                mdio_write(tp, 0x19, 0x499a);
                mdio_write(tp, 0x15, 0x03ba);
                mdio_write(tp, 0x19, 0x7c40);
                mdio_write(tp, 0x15, 0x03bb);
                mdio_write(tp, 0x19, 0x4c40);
                mdio_write(tp, 0x15, 0x03bc);
                mdio_write(tp, 0x19, 0x490a);
                mdio_write(tp, 0x15, 0x03bd);
                mdio_write(tp, 0x19, 0x405e);
                mdio_write(tp, 0x15, 0x03c2);
                mdio_write(tp, 0x19, 0x9a03);
                mdio_write(tp, 0x15, 0x03c4);
                mdio_write(tp, 0x19, 0x0015);
                mdio_write(tp, 0x15, 0x03c5);
                mdio_write(tp, 0x19, 0x9e03);
                mdio_write(tp, 0x15, 0x03c8);
                mdio_write(tp, 0x19, 0x9cf7);
                mdio_write(tp, 0x15, 0x03c9);
                mdio_write(tp, 0x19, 0x7c12);
                mdio_write(tp, 0x15, 0x03ca);
                mdio_write(tp, 0x19, 0x4c52);
                mdio_write(tp, 0x15, 0x03cb);
                mdio_write(tp, 0x19, 0x4458);
                mdio_write(tp, 0x15, 0x03cd);
                mdio_write(tp, 0x19, 0x4c40);
                mdio_write(tp, 0x15, 0x03ce);
                mdio_write(tp, 0x19, 0x33bf);
                mdio_write(tp, 0x15, 0x03cf);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x03d0);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x03d1);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x03d5);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x03d6);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x03d7);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x03d8);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x03d9);
                mdio_write(tp, 0x19, 0x49bb);
                mdio_write(tp, 0x15, 0x03da);
                mdio_write(tp, 0x19, 0x4478);
                mdio_write(tp, 0x15, 0x03db);
                mdio_write(tp, 0x19, 0x492b);
                mdio_write(tp, 0x15, 0x03dc);
                mdio_write(tp, 0x19, 0x7c01);
                mdio_write(tp, 0x15, 0x03dd);
                mdio_write(tp, 0x19, 0x4c00);
                mdio_write(tp, 0x15, 0x03de);
                mdio_write(tp, 0x19, 0xbd1a);
                mdio_write(tp, 0x15, 0x03df);
                mdio_write(tp, 0x19, 0xc428);
                mdio_write(tp, 0x15, 0x03e0);
                mdio_write(tp, 0x19, 0x0008);
                mdio_write(tp, 0x15, 0x03e1);
                mdio_write(tp, 0x19, 0x9cfd);
                mdio_write(tp, 0x15, 0x03e2);
                mdio_write(tp, 0x19, 0x7c12);
                mdio_write(tp, 0x15, 0x03e3);
                mdio_write(tp, 0x19, 0x4c52);
                mdio_write(tp, 0x15, 0x03e4);
                mdio_write(tp, 0x19, 0x4458);
                mdio_write(tp, 0x15, 0x03e5);
                mdio_write(tp, 0x19, 0x7c12);
                mdio_write(tp, 0x15, 0x03e6);
                mdio_write(tp, 0x19, 0x4c40);
                mdio_write(tp, 0x15, 0x03e7);
                mdio_write(tp, 0x19, 0x33de);
                mdio_write(tp, 0x15, 0x03e8);
                mdio_write(tp, 0x19, 0xc218);
                mdio_write(tp, 0x15, 0x03e9);
                mdio_write(tp, 0x19, 0x0002);
                mdio_write(tp, 0x15, 0x03ea);
                mdio_write(tp, 0x19, 0x32df);
                mdio_write(tp, 0x15, 0x03eb);
                mdio_write(tp, 0x19, 0x3316);
                mdio_write(tp, 0x15, 0x03ec);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x03ed);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x03ee);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x03ef);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x03f7);
                mdio_write(tp, 0x19, 0x330c);
                mdio_write(tp, 0x16, 0x0306);
                mdio_write(tp, 0x16, 0x0300);

                mdio_write(tp, 0x1f, 0x0005);
                mdio_write(tp, 0x05, 0xfff6);
                mdio_write(tp, 0x06, 0x0080);
                mdio_write(tp, 0x05, 0x8000);
                mdio_write(tp, 0x06, 0x0280);
                mdio_write(tp, 0x06, 0x48f7);
                mdio_write(tp, 0x06, 0x00e0);
                mdio_write(tp, 0x06, 0xfff7);
                mdio_write(tp, 0x06, 0xa080);
                mdio_write(tp, 0x06, 0x02ae);
                mdio_write(tp, 0x06, 0xf602);
                mdio_write(tp, 0x06, 0x0200);
                mdio_write(tp, 0x06, 0x0280);
                mdio_write(tp, 0x06, 0x9002);
                mdio_write(tp, 0x06, 0x0224);
                mdio_write(tp, 0x06, 0x0202);
                mdio_write(tp, 0x06, 0x3402);
                mdio_write(tp, 0x06, 0x027f);
                mdio_write(tp, 0x06, 0x0280);
                mdio_write(tp, 0x06, 0xa602);
                mdio_write(tp, 0x06, 0x80bf);
                mdio_write(tp, 0x06, 0xe08b);
                mdio_write(tp, 0x06, 0x88e1);
                mdio_write(tp, 0x06, 0x8b89);
                mdio_write(tp, 0x06, 0x1e01);
                mdio_write(tp, 0x06, 0xe18b);
                mdio_write(tp, 0x06, 0x8a1e);
                mdio_write(tp, 0x06, 0x01e1);
                mdio_write(tp, 0x06, 0x8b8b);
                mdio_write(tp, 0x06, 0x1e01);
                mdio_write(tp, 0x06, 0xe18b);
                mdio_write(tp, 0x06, 0x8c1e);
                mdio_write(tp, 0x06, 0x01e1);
                mdio_write(tp, 0x06, 0x8b8d);
                mdio_write(tp, 0x06, 0x1e01);
                mdio_write(tp, 0x06, 0xe18b);
                mdio_write(tp, 0x06, 0x8e1e);
                mdio_write(tp, 0x06, 0x01a0);
                mdio_write(tp, 0x06, 0x00c7);
                mdio_write(tp, 0x06, 0xaebb);
                mdio_write(tp, 0x06, 0xee8a);
                mdio_write(tp, 0x06, 0xe600);
                mdio_write(tp, 0x06, 0xee8a);
                mdio_write(tp, 0x06, 0xee03);
                mdio_write(tp, 0x06, 0xee8a);
                mdio_write(tp, 0x06, 0xefb8);
                mdio_write(tp, 0x06, 0xee8a);
                mdio_write(tp, 0x06, 0xe902);
                mdio_write(tp, 0x06, 0xee8b);
                mdio_write(tp, 0x06, 0x8285);
                mdio_write(tp, 0x06, 0xee8b);
                mdio_write(tp, 0x06, 0x8520);
                mdio_write(tp, 0x06, 0xee8b);
                mdio_write(tp, 0x06, 0x8701);
                mdio_write(tp, 0x06, 0xd481);
                mdio_write(tp, 0x06, 0x35e4);
                mdio_write(tp, 0x06, 0x8b94);
                mdio_write(tp, 0x06, 0xe58b);
                mdio_write(tp, 0x06, 0x95bf);
                mdio_write(tp, 0x06, 0x8b88);
                mdio_write(tp, 0x06, 0xec00);
                mdio_write(tp, 0x06, 0x19a9);
                mdio_write(tp, 0x06, 0x8b90);
                mdio_write(tp, 0x06, 0xf9ee);
                mdio_write(tp, 0x06, 0xfff6);
                mdio_write(tp, 0x06, 0x00ee);
                mdio_write(tp, 0x06, 0xfff7);
                mdio_write(tp, 0x06, 0xffe0);
                mdio_write(tp, 0x06, 0xe140);
                mdio_write(tp, 0x06, 0xe1e1);
                mdio_write(tp, 0x06, 0x41f7);
                mdio_write(tp, 0x06, 0x2ff6);
                mdio_write(tp, 0x06, 0x28e4);
                mdio_write(tp, 0x06, 0xe140);
                mdio_write(tp, 0x06, 0xe5e1);
                mdio_write(tp, 0x06, 0x4104);
                mdio_write(tp, 0x06, 0xf8e0);
                mdio_write(tp, 0x06, 0x8b89);
                mdio_write(tp, 0x06, 0xad20);
                mdio_write(tp, 0x06, 0x0dee);
                mdio_write(tp, 0x06, 0x8b89);
                mdio_write(tp, 0x06, 0x0002);
                mdio_write(tp, 0x06, 0x82f4);
                mdio_write(tp, 0x06, 0x021f);
                mdio_write(tp, 0x06, 0x4102);
                mdio_write(tp, 0x06, 0x2812);
                mdio_write(tp, 0x06, 0xfc04);
                mdio_write(tp, 0x06, 0xf8e0);
                mdio_write(tp, 0x06, 0x8b8d);
                mdio_write(tp, 0x06, 0xad20);
                mdio_write(tp, 0x06, 0x10ee);
                mdio_write(tp, 0x06, 0x8b8d);
                mdio_write(tp, 0x06, 0x0002);
                mdio_write(tp, 0x06, 0x139d);
                mdio_write(tp, 0x06, 0x0281);
                mdio_write(tp, 0x06, 0xd602);
                mdio_write(tp, 0x06, 0x1f99);
                mdio_write(tp, 0x06, 0x0227);
                mdio_write(tp, 0x06, 0xeafc);
                mdio_write(tp, 0x06, 0x04f8);
                mdio_write(tp, 0x06, 0xe08b);
                mdio_write(tp, 0x06, 0x8ead);
                mdio_write(tp, 0x06, 0x2014);
                mdio_write(tp, 0x06, 0xf620);
                mdio_write(tp, 0x06, 0xe48b);
                mdio_write(tp, 0x06, 0x8e02);
                mdio_write(tp, 0x06, 0x8104);
                mdio_write(tp, 0x06, 0x021b);
                mdio_write(tp, 0x06, 0xf402);
                mdio_write(tp, 0x06, 0x2c9c);
                mdio_write(tp, 0x06, 0x0281);
                mdio_write(tp, 0x06, 0x7902);
                mdio_write(tp, 0x06, 0x8443);
                mdio_write(tp, 0x06, 0xad22);
                mdio_write(tp, 0x06, 0x11f6);
                mdio_write(tp, 0x06, 0x22e4);
                mdio_write(tp, 0x06, 0x8b8e);
                mdio_write(tp, 0x06, 0x022c);
                mdio_write(tp, 0x06, 0x4602);
                mdio_write(tp, 0x06, 0x2ac5);
                mdio_write(tp, 0x06, 0x0229);
                mdio_write(tp, 0x06, 0x2002);
                mdio_write(tp, 0x06, 0x2b91);
                mdio_write(tp, 0x06, 0xad25);
                mdio_write(tp, 0x06, 0x11f6);
                mdio_write(tp, 0x06, 0x25e4);
                mdio_write(tp, 0x06, 0x8b8e);
                mdio_write(tp, 0x06, 0x0284);
                mdio_write(tp, 0x06, 0xe202);
                mdio_write(tp, 0x06, 0x043a);
                mdio_write(tp, 0x06, 0x021a);
                mdio_write(tp, 0x06, 0x5902);
                mdio_write(tp, 0x06, 0x2bfc);
                mdio_write(tp, 0x06, 0xfc04);
                mdio_write(tp, 0x06, 0xf8fa);
                mdio_write(tp, 0x06, 0xef69);
                mdio_write(tp, 0x06, 0xe0e0);
                mdio_write(tp, 0x06, 0x00e1);
                mdio_write(tp, 0x06, 0xe001);
                mdio_write(tp, 0x06, 0xad27);
                mdio_write(tp, 0x06, 0x1fd1);
                mdio_write(tp, 0x06, 0x01bf);
                mdio_write(tp, 0x06, 0x8638);
                mdio_write(tp, 0x06, 0x022f);
                mdio_write(tp, 0x06, 0x50e0);
                mdio_write(tp, 0x06, 0xe020);
                mdio_write(tp, 0x06, 0xe1e0);
                mdio_write(tp, 0x06, 0x21ad);
                mdio_write(tp, 0x06, 0x200e);
                mdio_write(tp, 0x06, 0xd100);
                mdio_write(tp, 0x06, 0xbf86);
                mdio_write(tp, 0x06, 0x3802);
                mdio_write(tp, 0x06, 0x2f50);
                mdio_write(tp, 0x06, 0xbf3d);
                mdio_write(tp, 0x06, 0x3902);
                mdio_write(tp, 0x06, 0x2eb0);
                mdio_write(tp, 0x06, 0xef96);
                mdio_write(tp, 0x06, 0xfefc);
                mdio_write(tp, 0x06, 0x0402);
                mdio_write(tp, 0x06, 0x8591);
                mdio_write(tp, 0x06, 0x0281);
                mdio_write(tp, 0x06, 0x3c05);
                mdio_write(tp, 0x06, 0xf8fa);
                mdio_write(tp, 0x06, 0xef69);
                mdio_write(tp, 0x06, 0xe0e2);
                mdio_write(tp, 0x06, 0xfee1);
                mdio_write(tp, 0x06, 0xe2ff);
                mdio_write(tp, 0x06, 0xad2d);
                mdio_write(tp, 0x06, 0x1ae0);
                mdio_write(tp, 0x06, 0xe14e);
                mdio_write(tp, 0x06, 0xe1e1);
                mdio_write(tp, 0x06, 0x4fac);
                mdio_write(tp, 0x06, 0x2d22);
                mdio_write(tp, 0x06, 0xf603);
                mdio_write(tp, 0x06, 0x0203);
                mdio_write(tp, 0x06, 0x36f7);
                mdio_write(tp, 0x06, 0x03f7);
                mdio_write(tp, 0x06, 0x06bf);
                mdio_write(tp, 0x06, 0x8622);
                mdio_write(tp, 0x06, 0x022e);
                mdio_write(tp, 0x06, 0xb0ae);
                mdio_write(tp, 0x06, 0x11e0);
                mdio_write(tp, 0x06, 0xe14e);
                mdio_write(tp, 0x06, 0xe1e1);
                mdio_write(tp, 0x06, 0x4fad);
                mdio_write(tp, 0x06, 0x2d08);
                mdio_write(tp, 0x06, 0xbf86);
                mdio_write(tp, 0x06, 0x2d02);
                mdio_write(tp, 0x06, 0x2eb0);
                mdio_write(tp, 0x06, 0xf606);
                mdio_write(tp, 0x06, 0xef96);
                mdio_write(tp, 0x06, 0xfefc);
                mdio_write(tp, 0x06, 0x04f8);
                mdio_write(tp, 0x06, 0xf9fa);
                mdio_write(tp, 0x06, 0xef69);
                mdio_write(tp, 0x06, 0xe08b);
                mdio_write(tp, 0x06, 0x87ad);
                mdio_write(tp, 0x06, 0x204c);
                mdio_write(tp, 0x06, 0xd200);
                mdio_write(tp, 0x06, 0xe0e2);
                mdio_write(tp, 0x06, 0x0058);
                mdio_write(tp, 0x06, 0x010c);
                mdio_write(tp, 0x06, 0x021e);
                mdio_write(tp, 0x06, 0x20e0);
                mdio_write(tp, 0x06, 0xe000);
                mdio_write(tp, 0x06, 0x5810);
                mdio_write(tp, 0x06, 0x1e20);
                mdio_write(tp, 0x06, 0xe0e0);
                mdio_write(tp, 0x06, 0x3658);
                mdio_write(tp, 0x06, 0x031e);
                mdio_write(tp, 0x06, 0x20e0);
                mdio_write(tp, 0x06, 0xe022);
                mdio_write(tp, 0x06, 0xe1e0);
                mdio_write(tp, 0x06, 0x2358);
                mdio_write(tp, 0x06, 0xe01e);
                mdio_write(tp, 0x06, 0x20e0);
                mdio_write(tp, 0x06, 0x8ae6);
                mdio_write(tp, 0x06, 0x1f02);
                mdio_write(tp, 0x06, 0x9e22);
                mdio_write(tp, 0x06, 0xe68a);
                mdio_write(tp, 0x06, 0xe6ad);
                mdio_write(tp, 0x06, 0x3214);
                mdio_write(tp, 0x06, 0xad34);
                mdio_write(tp, 0x06, 0x11ef);
                mdio_write(tp, 0x06, 0x0258);
                mdio_write(tp, 0x06, 0x039e);
                mdio_write(tp, 0x06, 0x07ad);
                mdio_write(tp, 0x06, 0x3508);
                mdio_write(tp, 0x06, 0x5ac0);
                mdio_write(tp, 0x06, 0x9f04);
                mdio_write(tp, 0x06, 0xd101);
                mdio_write(tp, 0x06, 0xae02);
                mdio_write(tp, 0x06, 0xd100);
                mdio_write(tp, 0x06, 0xbf86);
                mdio_write(tp, 0x06, 0x3e02);
                mdio_write(tp, 0x06, 0x2f50);
                mdio_write(tp, 0x06, 0xef96);
                mdio_write(tp, 0x06, 0xfefd);
                mdio_write(tp, 0x06, 0xfc04);
                mdio_write(tp, 0x06, 0xf8f9);
                mdio_write(tp, 0x06, 0xfae0);
                mdio_write(tp, 0x06, 0x8b81);
                mdio_write(tp, 0x06, 0xac26);
                mdio_write(tp, 0x06, 0x0ee0);
                mdio_write(tp, 0x06, 0x8b81);
                mdio_write(tp, 0x06, 0xac21);
                mdio_write(tp, 0x06, 0x08e0);
                mdio_write(tp, 0x06, 0x8b87);
                mdio_write(tp, 0x06, 0xac24);
                mdio_write(tp, 0x06, 0x02ae);
                mdio_write(tp, 0x06, 0x6bee);
                mdio_write(tp, 0x06, 0xe0ea);
                mdio_write(tp, 0x06, 0x00ee);
                mdio_write(tp, 0x06, 0xe0eb);
                mdio_write(tp, 0x06, 0x00e2);
                mdio_write(tp, 0x06, 0xe07c);
                mdio_write(tp, 0x06, 0xe3e0);
                mdio_write(tp, 0x06, 0x7da5);
                mdio_write(tp, 0x06, 0x1111);
                mdio_write(tp, 0x06, 0x15d2);
                mdio_write(tp, 0x06, 0x60d6);
                mdio_write(tp, 0x06, 0x6666);
                mdio_write(tp, 0x06, 0x0207);
                mdio_write(tp, 0x06, 0xf9d2);
                mdio_write(tp, 0x06, 0xa0d6);
                mdio_write(tp, 0x06, 0xaaaa);
                mdio_write(tp, 0x06, 0x0207);
                mdio_write(tp, 0x06, 0xf902);
                mdio_write(tp, 0x06, 0x825c);
                mdio_write(tp, 0x06, 0xae44);
                mdio_write(tp, 0x06, 0xa566);
                mdio_write(tp, 0x06, 0x6602);
                mdio_write(tp, 0x06, 0xae38);
                mdio_write(tp, 0x06, 0xa5aa);
                mdio_write(tp, 0x06, 0xaa02);
                mdio_write(tp, 0x06, 0xae32);
                mdio_write(tp, 0x06, 0xeee0);
                mdio_write(tp, 0x06, 0xea04);
                mdio_write(tp, 0x06, 0xeee0);
                mdio_write(tp, 0x06, 0xeb06);
                mdio_write(tp, 0x06, 0xe2e0);
                mdio_write(tp, 0x06, 0x7ce3);
                mdio_write(tp, 0x06, 0xe07d);
                mdio_write(tp, 0x06, 0xe0e0);
                mdio_write(tp, 0x06, 0x38e1);
                mdio_write(tp, 0x06, 0xe039);
                mdio_write(tp, 0x06, 0xad2e);
                mdio_write(tp, 0x06, 0x21ad);
                mdio_write(tp, 0x06, 0x3f13);
                mdio_write(tp, 0x06, 0xe0e4);
                mdio_write(tp, 0x06, 0x14e1);
                mdio_write(tp, 0x06, 0xe415);
                mdio_write(tp, 0x06, 0x6880);
                mdio_write(tp, 0x06, 0xe4e4);
                mdio_write(tp, 0x06, 0x14e5);
                mdio_write(tp, 0x06, 0xe415);
                mdio_write(tp, 0x06, 0x0282);
                mdio_write(tp, 0x06, 0x5cae);
                mdio_write(tp, 0x06, 0x0bac);
                mdio_write(tp, 0x06, 0x3e02);
                mdio_write(tp, 0x06, 0xae06);
                mdio_write(tp, 0x06, 0x0282);
                mdio_write(tp, 0x06, 0x8602);
                mdio_write(tp, 0x06, 0x82b0);
                mdio_write(tp, 0x06, 0xfefd);
                mdio_write(tp, 0x06, 0xfc04);
                mdio_write(tp, 0x06, 0xf8e1);
                mdio_write(tp, 0x06, 0x8b2e);
                mdio_write(tp, 0x06, 0xe08b);
                mdio_write(tp, 0x06, 0x81ad);
                mdio_write(tp, 0x06, 0x2605);
                mdio_write(tp, 0x06, 0x0221);
                mdio_write(tp, 0x06, 0xf3f7);
                mdio_write(tp, 0x06, 0x28e0);
                mdio_write(tp, 0x06, 0x8b81);
                mdio_write(tp, 0x06, 0xad21);
                mdio_write(tp, 0x06, 0x0502);
                mdio_write(tp, 0x06, 0x22f8);
                mdio_write(tp, 0x06, 0xf729);
                mdio_write(tp, 0x06, 0xe08b);
                mdio_write(tp, 0x06, 0x87ad);
                mdio_write(tp, 0x06, 0x2405);
                mdio_write(tp, 0x06, 0x0282);
                mdio_write(tp, 0x06, 0xebf7);
                mdio_write(tp, 0x06, 0x2ae5);
                mdio_write(tp, 0x06, 0x8b2e);
                mdio_write(tp, 0x06, 0xfc04);
                mdio_write(tp, 0x06, 0xf8e0);
                mdio_write(tp, 0x06, 0x8b81);
                mdio_write(tp, 0x06, 0xad26);
                mdio_write(tp, 0x06, 0x0302);
                mdio_write(tp, 0x06, 0x2134);
                mdio_write(tp, 0x06, 0xe08b);
                mdio_write(tp, 0x06, 0x81ad);
                mdio_write(tp, 0x06, 0x2109);
                mdio_write(tp, 0x06, 0xe08b);
                mdio_write(tp, 0x06, 0x2eac);
                mdio_write(tp, 0x06, 0x2003);
                mdio_write(tp, 0x06, 0x0283);
                mdio_write(tp, 0x06, 0x52e0);
                mdio_write(tp, 0x06, 0x8b87);
                mdio_write(tp, 0x06, 0xad24);
                mdio_write(tp, 0x06, 0x09e0);
                mdio_write(tp, 0x06, 0x8b2e);
                mdio_write(tp, 0x06, 0xac21);
                mdio_write(tp, 0x06, 0x0302);
                mdio_write(tp, 0x06, 0x8337);
                mdio_write(tp, 0x06, 0xfc04);
                mdio_write(tp, 0x06, 0xf8e1);
                mdio_write(tp, 0x06, 0x8b2e);
                mdio_write(tp, 0x06, 0xe08b);
                mdio_write(tp, 0x06, 0x81ad);
                mdio_write(tp, 0x06, 0x2608);
                mdio_write(tp, 0x06, 0xe085);
                mdio_write(tp, 0x06, 0xd2ad);
                mdio_write(tp, 0x06, 0x2502);
                mdio_write(tp, 0x06, 0xf628);
                mdio_write(tp, 0x06, 0xe08b);
                mdio_write(tp, 0x06, 0x81ad);
                mdio_write(tp, 0x06, 0x210a);
                mdio_write(tp, 0x06, 0xe086);
                mdio_write(tp, 0x06, 0x0af6);
                mdio_write(tp, 0x06, 0x27a0);
                mdio_write(tp, 0x06, 0x0502);
                mdio_write(tp, 0x06, 0xf629);
                mdio_write(tp, 0x06, 0xe08b);
                mdio_write(tp, 0x06, 0x87ad);
                mdio_write(tp, 0x06, 0x2408);
                mdio_write(tp, 0x06, 0xe08a);
                mdio_write(tp, 0x06, 0xedad);
                mdio_write(tp, 0x06, 0x2002);
                mdio_write(tp, 0x06, 0xf62a);
                mdio_write(tp, 0x06, 0xe58b);
                mdio_write(tp, 0x06, 0x2ea1);
                mdio_write(tp, 0x06, 0x0003);
                mdio_write(tp, 0x06, 0x0221);
                mdio_write(tp, 0x06, 0x11fc);
                mdio_write(tp, 0x06, 0x04ee);
                mdio_write(tp, 0x06, 0x8aed);
                mdio_write(tp, 0x06, 0x00ee);
                mdio_write(tp, 0x06, 0x8aec);
                mdio_write(tp, 0x06, 0x0004);
                mdio_write(tp, 0x06, 0xf8e0);
                mdio_write(tp, 0x06, 0x8b87);
                mdio_write(tp, 0x06, 0xad24);
                mdio_write(tp, 0x06, 0x3ae0);
                mdio_write(tp, 0x06, 0xe0ea);
                mdio_write(tp, 0x06, 0xe1e0);
                mdio_write(tp, 0x06, 0xeb58);
                mdio_write(tp, 0x06, 0xf8d1);
                mdio_write(tp, 0x06, 0x01e4);
                mdio_write(tp, 0x06, 0xe0ea);
                mdio_write(tp, 0x06, 0xe5e0);
                mdio_write(tp, 0x06, 0xebe0);
                mdio_write(tp, 0x06, 0xe07c);
                mdio_write(tp, 0x06, 0xe1e0);
                mdio_write(tp, 0x06, 0x7d5c);
                mdio_write(tp, 0x06, 0x00ff);
                mdio_write(tp, 0x06, 0x3c00);
                mdio_write(tp, 0x06, 0x1eab);
                mdio_write(tp, 0x06, 0x1ce0);
                mdio_write(tp, 0x06, 0xe04c);
                mdio_write(tp, 0x06, 0xe1e0);
                mdio_write(tp, 0x06, 0x4d58);
                mdio_write(tp, 0x06, 0xc1e4);
                mdio_write(tp, 0x06, 0xe04c);
                mdio_write(tp, 0x06, 0xe5e0);
                mdio_write(tp, 0x06, 0x4de0);
                mdio_write(tp, 0x06, 0xe0ee);
                mdio_write(tp, 0x06, 0xe1e0);
                mdio_write(tp, 0x06, 0xef69);
                mdio_write(tp, 0x06, 0x3ce4);
                mdio_write(tp, 0x06, 0xe0ee);
                mdio_write(tp, 0x06, 0xe5e0);
                mdio_write(tp, 0x06, 0xeffc);
                mdio_write(tp, 0x06, 0x04f8);
                mdio_write(tp, 0x06, 0xe08b);
                mdio_write(tp, 0x06, 0x87ad);
                mdio_write(tp, 0x06, 0x2412);
                mdio_write(tp, 0x06, 0xe0e0);
                mdio_write(tp, 0x06, 0xeee1);
                mdio_write(tp, 0x06, 0xe0ef);
                mdio_write(tp, 0x06, 0x59c3);
                mdio_write(tp, 0x06, 0xe4e0);
                mdio_write(tp, 0x06, 0xeee5);
                mdio_write(tp, 0x06, 0xe0ef);
                mdio_write(tp, 0x06, 0xee8a);
                mdio_write(tp, 0x06, 0xed01);
                mdio_write(tp, 0x06, 0xfc04);
                mdio_write(tp, 0x06, 0xf8e0);
                mdio_write(tp, 0x06, 0x8b81);
                mdio_write(tp, 0x06, 0xac25);
                mdio_write(tp, 0x06, 0x0502);
                mdio_write(tp, 0x06, 0x8363);
                mdio_write(tp, 0x06, 0xae03);
                mdio_write(tp, 0x06, 0x0225);
                mdio_write(tp, 0x06, 0x16fc);
                mdio_write(tp, 0x06, 0x04f8);
                mdio_write(tp, 0x06, 0xf9fa);
                mdio_write(tp, 0x06, 0xef69);
                mdio_write(tp, 0x06, 0xfae0);
                mdio_write(tp, 0x06, 0x860a);
                mdio_write(tp, 0x06, 0xa000);
                mdio_write(tp, 0x06, 0x19e0);
                mdio_write(tp, 0x06, 0x860b);
                mdio_write(tp, 0x06, 0xe18b);
                mdio_write(tp, 0x06, 0x331b);
                mdio_write(tp, 0x06, 0x109e);
                mdio_write(tp, 0x06, 0x04aa);
                mdio_write(tp, 0x06, 0x02ae);
                mdio_write(tp, 0x06, 0x06ee);
                mdio_write(tp, 0x06, 0x860a);
                mdio_write(tp, 0x06, 0x01ae);
                mdio_write(tp, 0x06, 0xe602);
                mdio_write(tp, 0x06, 0x241e);
                mdio_write(tp, 0x06, 0xae14);
                mdio_write(tp, 0x06, 0xa001);
                mdio_write(tp, 0x06, 0x1402);
                mdio_write(tp, 0x06, 0x2426);
                mdio_write(tp, 0x06, 0xbf26);
                mdio_write(tp, 0x06, 0x6d02);
                mdio_write(tp, 0x06, 0x2eb0);
                mdio_write(tp, 0x06, 0xee86);
                mdio_write(tp, 0x06, 0x0b00);
                mdio_write(tp, 0x06, 0xee86);
                mdio_write(tp, 0x06, 0x0a02);
                mdio_write(tp, 0x06, 0xaf84);
                mdio_write(tp, 0x06, 0x3ca0);
                mdio_write(tp, 0x06, 0x0252);
                mdio_write(tp, 0x06, 0xee86);
                mdio_write(tp, 0x06, 0x0400);
                mdio_write(tp, 0x06, 0xee86);
                mdio_write(tp, 0x06, 0x0500);
                mdio_write(tp, 0x06, 0xe086);
                mdio_write(tp, 0x06, 0x0be1);
                mdio_write(tp, 0x06, 0x8b32);
                mdio_write(tp, 0x06, 0x1b10);
                mdio_write(tp, 0x06, 0x9e04);
                mdio_write(tp, 0x06, 0xaa02);
                mdio_write(tp, 0x06, 0xaecb);
                mdio_write(tp, 0x06, 0xee86);
                mdio_write(tp, 0x06, 0x0b00);
                mdio_write(tp, 0x06, 0x0224);
                mdio_write(tp, 0x06, 0x3ae2);
                mdio_write(tp, 0x06, 0x8604);
                mdio_write(tp, 0x06, 0xe386);
                mdio_write(tp, 0x06, 0x05ef);
                mdio_write(tp, 0x06, 0x65e2);
                mdio_write(tp, 0x06, 0x8606);
                mdio_write(tp, 0x06, 0xe386);
                mdio_write(tp, 0x06, 0x071b);
                mdio_write(tp, 0x06, 0x56aa);
                mdio_write(tp, 0x06, 0x0eef);
                mdio_write(tp, 0x06, 0x56e6);
                mdio_write(tp, 0x06, 0x8606);
                mdio_write(tp, 0x06, 0xe786);
                mdio_write(tp, 0x06, 0x07e2);
                mdio_write(tp, 0x06, 0x8609);
                mdio_write(tp, 0x06, 0xe686);
                mdio_write(tp, 0x06, 0x08e0);
                mdio_write(tp, 0x06, 0x8609);
                mdio_write(tp, 0x06, 0xa000);
                mdio_write(tp, 0x06, 0x07ee);
                mdio_write(tp, 0x06, 0x860a);
                mdio_write(tp, 0x06, 0x03af);
                mdio_write(tp, 0x06, 0x8369);
                mdio_write(tp, 0x06, 0x0224);
                mdio_write(tp, 0x06, 0x8e02);
                mdio_write(tp, 0x06, 0x2426);
                mdio_write(tp, 0x06, 0xae48);
                mdio_write(tp, 0x06, 0xa003);
                mdio_write(tp, 0x06, 0x21e0);
                mdio_write(tp, 0x06, 0x8608);
                mdio_write(tp, 0x06, 0xe186);
                mdio_write(tp, 0x06, 0x091b);
                mdio_write(tp, 0x06, 0x019e);
                mdio_write(tp, 0x06, 0x0caa);
                mdio_write(tp, 0x06, 0x0502);
                mdio_write(tp, 0x06, 0x249d);
                mdio_write(tp, 0x06, 0xaee7);
                mdio_write(tp, 0x06, 0x0224);
                mdio_write(tp, 0x06, 0x8eae);
                mdio_write(tp, 0x06, 0xe2ee);
                mdio_write(tp, 0x06, 0x860a);
                mdio_write(tp, 0x06, 0x04ee);
                mdio_write(tp, 0x06, 0x860b);
                mdio_write(tp, 0x06, 0x00af);
                mdio_write(tp, 0x06, 0x8369);
                mdio_write(tp, 0x06, 0xa004);
                mdio_write(tp, 0x06, 0x15e0);
                mdio_write(tp, 0x06, 0x860b);
                mdio_write(tp, 0x06, 0xe18b);
                mdio_write(tp, 0x06, 0x341b);
                mdio_write(tp, 0x06, 0x109e);
                mdio_write(tp, 0x06, 0x05aa);
                mdio_write(tp, 0x06, 0x03af);
                mdio_write(tp, 0x06, 0x8383);
                mdio_write(tp, 0x06, 0xee86);
                mdio_write(tp, 0x06, 0x0a05);
                mdio_write(tp, 0x06, 0xae0c);
                mdio_write(tp, 0x06, 0xa005);
                mdio_write(tp, 0x06, 0x02ae);
                mdio_write(tp, 0x06, 0x0702);
                mdio_write(tp, 0x06, 0x2309);
                mdio_write(tp, 0x06, 0xee86);
                mdio_write(tp, 0x06, 0x0a00);
                mdio_write(tp, 0x06, 0xfeef);
                mdio_write(tp, 0x06, 0x96fe);
                mdio_write(tp, 0x06, 0xfdfc);
                mdio_write(tp, 0x06, 0x04f8);
                mdio_write(tp, 0x06, 0xf9fa);
                mdio_write(tp, 0x06, 0xef69);
                mdio_write(tp, 0x06, 0xfbe0);
                mdio_write(tp, 0x06, 0x8b85);
                mdio_write(tp, 0x06, 0xad25);
                mdio_write(tp, 0x06, 0x22e0);
                mdio_write(tp, 0x06, 0xe022);
                mdio_write(tp, 0x06, 0xe1e0);
                mdio_write(tp, 0x06, 0x23e2);
                mdio_write(tp, 0x06, 0xe036);
                mdio_write(tp, 0x06, 0xe3e0);
                mdio_write(tp, 0x06, 0x375a);
                mdio_write(tp, 0x06, 0xc40d);
                mdio_write(tp, 0x06, 0x0158);
                mdio_write(tp, 0x06, 0x021e);
                mdio_write(tp, 0x06, 0x20e3);
                mdio_write(tp, 0x06, 0x8ae7);
                mdio_write(tp, 0x06, 0xac31);
                mdio_write(tp, 0x06, 0x60ac);
                mdio_write(tp, 0x06, 0x3a08);
                mdio_write(tp, 0x06, 0xac3e);
                mdio_write(tp, 0x06, 0x26ae);
                mdio_write(tp, 0x06, 0x67af);
                mdio_write(tp, 0x06, 0x84db);
                mdio_write(tp, 0x06, 0xad37);
                mdio_write(tp, 0x06, 0x61e0);
                mdio_write(tp, 0x06, 0x8ae8);
                mdio_write(tp, 0x06, 0x10e4);
                mdio_write(tp, 0x06, 0x8ae8);
                mdio_write(tp, 0x06, 0xe18a);
                mdio_write(tp, 0x06, 0xe91b);
                mdio_write(tp, 0x06, 0x109e);
                mdio_write(tp, 0x06, 0x02ae);
                mdio_write(tp, 0x06, 0x51d1);
                mdio_write(tp, 0x06, 0x00bf);
                mdio_write(tp, 0x06, 0x863b);
                mdio_write(tp, 0x06, 0x022f);
                mdio_write(tp, 0x06, 0x50ee);
                mdio_write(tp, 0x06, 0x8ae8);
                mdio_write(tp, 0x06, 0x00ae);
                mdio_write(tp, 0x06, 0x43ad);
                mdio_write(tp, 0x06, 0x3627);
                mdio_write(tp, 0x06, 0xe08a);
                mdio_write(tp, 0x06, 0xeee1);
                mdio_write(tp, 0x06, 0x8aef);
                mdio_write(tp, 0x06, 0xef74);
                mdio_write(tp, 0x06, 0xe08a);
                mdio_write(tp, 0x06, 0xeae1);
                mdio_write(tp, 0x06, 0x8aeb);
                mdio_write(tp, 0x06, 0x1b74);
                mdio_write(tp, 0x06, 0x9e2e);
                mdio_write(tp, 0x06, 0x14e4);
                mdio_write(tp, 0x06, 0x8aea);
                mdio_write(tp, 0x06, 0xe58a);
                mdio_write(tp, 0x06, 0xebef);
                mdio_write(tp, 0x06, 0x74e0);
                mdio_write(tp, 0x06, 0x8aee);
                mdio_write(tp, 0x06, 0xe18a);
                mdio_write(tp, 0x06, 0xef1b);
                mdio_write(tp, 0x06, 0x479e);
                mdio_write(tp, 0x06, 0x0fae);
                mdio_write(tp, 0x06, 0x19ee);
                mdio_write(tp, 0x06, 0x8aea);
                mdio_write(tp, 0x06, 0x00ee);
                mdio_write(tp, 0x06, 0x8aeb);
                mdio_write(tp, 0x06, 0x00ae);
                mdio_write(tp, 0x06, 0x0fac);
                mdio_write(tp, 0x06, 0x390c);
                mdio_write(tp, 0x06, 0xd101);
                mdio_write(tp, 0x06, 0xbf86);
                mdio_write(tp, 0x06, 0x3b02);
                mdio_write(tp, 0x06, 0x2f50);
                mdio_write(tp, 0x06, 0xee8a);
                mdio_write(tp, 0x06, 0xe800);
                mdio_write(tp, 0x06, 0xe68a);
                mdio_write(tp, 0x06, 0xe7ff);
                mdio_write(tp, 0x06, 0xef96);
                mdio_write(tp, 0x06, 0xfefd);
                mdio_write(tp, 0x06, 0xfc04);
                mdio_write(tp, 0x06, 0xf8f9);
                mdio_write(tp, 0x06, 0xfaef);
                mdio_write(tp, 0x06, 0x69e0);
                mdio_write(tp, 0x06, 0xe022);
                mdio_write(tp, 0x06, 0xe1e0);
                mdio_write(tp, 0x06, 0x2358);
                mdio_write(tp, 0x06, 0xc4e1);
                mdio_write(tp, 0x06, 0x8b6e);
                mdio_write(tp, 0x06, 0x1f10);
                mdio_write(tp, 0x06, 0x9e24);
                mdio_write(tp, 0x06, 0xe48b);
                mdio_write(tp, 0x06, 0x6ead);
                mdio_write(tp, 0x06, 0x2218);
                mdio_write(tp, 0x06, 0xac27);
                mdio_write(tp, 0x06, 0x0dac);
                mdio_write(tp, 0x06, 0x2605);
                mdio_write(tp, 0x06, 0x0203);
                mdio_write(tp, 0x06, 0x8fae);
                mdio_write(tp, 0x06, 0x1302);
                mdio_write(tp, 0x06, 0x03c8);
                mdio_write(tp, 0x06, 0xae0e);
                mdio_write(tp, 0x06, 0x0203);
                mdio_write(tp, 0x06, 0xe102);
                mdio_write(tp, 0x06, 0x8520);
                mdio_write(tp, 0x06, 0xae06);
                mdio_write(tp, 0x06, 0x0203);
                mdio_write(tp, 0x06, 0x8f02);
                mdio_write(tp, 0x06, 0x8566);
                mdio_write(tp, 0x06, 0xef96);
                mdio_write(tp, 0x06, 0xfefd);
                mdio_write(tp, 0x06, 0xfc04);
                mdio_write(tp, 0x06, 0xf8fa);
                mdio_write(tp, 0x06, 0xef69);
                mdio_write(tp, 0x06, 0xe08b);
                mdio_write(tp, 0x06, 0x82ad);
                mdio_write(tp, 0x06, 0x2737);
                mdio_write(tp, 0x06, 0xbf86);
                mdio_write(tp, 0x06, 0x4402);
                mdio_write(tp, 0x06, 0x2f23);
                mdio_write(tp, 0x06, 0xac28);
                mdio_write(tp, 0x06, 0x2ed1);
                mdio_write(tp, 0x06, 0x01bf);
                mdio_write(tp, 0x06, 0x8647);
                mdio_write(tp, 0x06, 0x022f);
                mdio_write(tp, 0x06, 0x50bf);
                mdio_write(tp, 0x06, 0x8641);
                mdio_write(tp, 0x06, 0x022f);
                mdio_write(tp, 0x06, 0x23e5);
                mdio_write(tp, 0x06, 0x8af0);
                mdio_write(tp, 0x06, 0xe0e0);
                mdio_write(tp, 0x06, 0x22e1);
                mdio_write(tp, 0x06, 0xe023);
                mdio_write(tp, 0x06, 0xac2e);
                mdio_write(tp, 0x06, 0x04d1);
                mdio_write(tp, 0x06, 0x01ae);
                mdio_write(tp, 0x06, 0x02d1);
                mdio_write(tp, 0x06, 0x00bf);
                mdio_write(tp, 0x06, 0x8641);
                mdio_write(tp, 0x06, 0x022f);
                mdio_write(tp, 0x06, 0x50d1);
                mdio_write(tp, 0x06, 0x01bf);
                mdio_write(tp, 0x06, 0x8644);
                mdio_write(tp, 0x06, 0x022f);
                mdio_write(tp, 0x06, 0x50ef);
                mdio_write(tp, 0x06, 0x96fe);
                mdio_write(tp, 0x06, 0xfc04);
                mdio_write(tp, 0x06, 0xf8fa);
                mdio_write(tp, 0x06, 0xef69);
                mdio_write(tp, 0x06, 0xbf86);
                mdio_write(tp, 0x06, 0x4702);
                mdio_write(tp, 0x06, 0x2f23);
                mdio_write(tp, 0x06, 0xad28);
                mdio_write(tp, 0x06, 0x19d1);
                mdio_write(tp, 0x06, 0x00bf);
                mdio_write(tp, 0x06, 0x8644);
                mdio_write(tp, 0x06, 0x022f);
                mdio_write(tp, 0x06, 0x50e1);
                mdio_write(tp, 0x06, 0x8af0);
                mdio_write(tp, 0x06, 0xbf86);
                mdio_write(tp, 0x06, 0x4102);
                mdio_write(tp, 0x06, 0x2f50);
                mdio_write(tp, 0x06, 0xd100);
                mdio_write(tp, 0x06, 0xbf86);
                mdio_write(tp, 0x06, 0x4702);
                mdio_write(tp, 0x06, 0x2f50);
                mdio_write(tp, 0x06, 0xef96);
                mdio_write(tp, 0x06, 0xfefc);
                mdio_write(tp, 0x06, 0x04f8);
                mdio_write(tp, 0x06, 0xe0e2);
                mdio_write(tp, 0x06, 0xfee1);
                mdio_write(tp, 0x06, 0xe2ff);
                mdio_write(tp, 0x06, 0xad2e);
                mdio_write(tp, 0x06, 0x63e0);
                mdio_write(tp, 0x06, 0xe038);
                mdio_write(tp, 0x06, 0xe1e0);
                mdio_write(tp, 0x06, 0x39ad);
                mdio_write(tp, 0x06, 0x2f10);
                mdio_write(tp, 0x06, 0xe0e0);
                mdio_write(tp, 0x06, 0x34e1);
                mdio_write(tp, 0x06, 0xe035);
                mdio_write(tp, 0x06, 0xf726);
                mdio_write(tp, 0x06, 0xe4e0);
                mdio_write(tp, 0x06, 0x34e5);
                mdio_write(tp, 0x06, 0xe035);
                mdio_write(tp, 0x06, 0xae0e);
                mdio_write(tp, 0x06, 0xe0e2);
                mdio_write(tp, 0x06, 0xd6e1);
                mdio_write(tp, 0x06, 0xe2d7);
                mdio_write(tp, 0x06, 0xf728);
                mdio_write(tp, 0x06, 0xe4e2);
                mdio_write(tp, 0x06, 0xd6e5);
                mdio_write(tp, 0x06, 0xe2d7);
                mdio_write(tp, 0x06, 0xe0e2);
                mdio_write(tp, 0x06, 0x34e1);
                mdio_write(tp, 0x06, 0xe235);
                mdio_write(tp, 0x06, 0xf72b);
                mdio_write(tp, 0x06, 0xe4e2);
                mdio_write(tp, 0x06, 0x34e5);
                mdio_write(tp, 0x06, 0xe235);
                mdio_write(tp, 0x06, 0xd07d);
                mdio_write(tp, 0x06, 0xb0fe);
                mdio_write(tp, 0x06, 0xe0e2);
                mdio_write(tp, 0x06, 0x34e1);
                mdio_write(tp, 0x06, 0xe235);
                mdio_write(tp, 0x06, 0xf62b);
                mdio_write(tp, 0x06, 0xe4e2);
                mdio_write(tp, 0x06, 0x34e5);
                mdio_write(tp, 0x06, 0xe235);
                mdio_write(tp, 0x06, 0xe0e0);
                mdio_write(tp, 0x06, 0x34e1);
                mdio_write(tp, 0x06, 0xe035);
                mdio_write(tp, 0x06, 0xf626);
                mdio_write(tp, 0x06, 0xe4e0);
                mdio_write(tp, 0x06, 0x34e5);
                mdio_write(tp, 0x06, 0xe035);
                mdio_write(tp, 0x06, 0xe0e2);
                mdio_write(tp, 0x06, 0xd6e1);
                mdio_write(tp, 0x06, 0xe2d7);
                mdio_write(tp, 0x06, 0xf628);
                mdio_write(tp, 0x06, 0xe4e2);
                mdio_write(tp, 0x06, 0xd6e5);
                mdio_write(tp, 0x06, 0xe2d7);
                mdio_write(tp, 0x06, 0xfc04);
                mdio_write(tp, 0x06, 0xae20);
                mdio_write(tp, 0x06, 0x0000);
                mdio_write(tp, 0x06, 0x0000);
                mdio_write(tp, 0x06, 0x0000);
                mdio_write(tp, 0x06, 0x0000);
                mdio_write(tp, 0x06, 0x0000);
                mdio_write(tp, 0x06, 0x0000);
                mdio_write(tp, 0x06, 0x0000);
                mdio_write(tp, 0x06, 0x0000);
                mdio_write(tp, 0x06, 0x0000);
                mdio_write(tp, 0x06, 0x0000);
                mdio_write(tp, 0x06, 0x0000);
                mdio_write(tp, 0x06, 0x0000);
                mdio_write(tp, 0x06, 0x0000);
                mdio_write(tp, 0x06, 0x0000);
                mdio_write(tp, 0x06, 0x0000);
                mdio_write(tp, 0x06, 0x0000);
                mdio_write(tp, 0x06, 0xa725);
                mdio_write(tp, 0x06, 0xe50a);
                mdio_write(tp, 0x06, 0x1de5);
                mdio_write(tp, 0x06, 0x0a2c);
                mdio_write(tp, 0x06, 0xe50a);
                mdio_write(tp, 0x06, 0x6de5);
                mdio_write(tp, 0x06, 0x0a1d);
                mdio_write(tp, 0x06, 0xe50a);
                mdio_write(tp, 0x06, 0x1ce5);
                mdio_write(tp, 0x06, 0x0a2d);
                mdio_write(tp, 0x06, 0xa755);
                mdio_write(tp, 0x06, 0x00e2);
                mdio_write(tp, 0x06, 0x3488);
                mdio_write(tp, 0x06, 0xe200);
                mdio_write(tp, 0x06, 0xcce2);
                mdio_write(tp, 0x06, 0x0055);
                mdio_write(tp, 0x06, 0xe020);
                mdio_write(tp, 0x06, 0x55e2);
                mdio_write(tp, 0x06, 0xd600);
                mdio_write(tp, 0x06, 0xe24a);
                gphy_val = mdio_read(tp, 0x01);
                gphy_val |= BIT_0;
                mdio_write(tp, 0x01, gphy_val);
                gphy_val = mdio_read(tp, 0x00);
                gphy_val |= BIT_0;
                mdio_write(tp, 0x00, gphy_val);
                mdio_write(tp, 0x1f, 0x0000);

                mdio_write(tp, 0x1f, 0x0000);
                mdio_write(tp, 0x17, 0x2179);
                mdio_write(tp, 0x1f, 0x0001);
                mdio_write(tp, 0x10, 0xf274);
                mdio_write(tp, 0x1f, 0x0007);
                mdio_write(tp, 0x1e, 0x0042);
                mdio_write(tp, 0x15, 0x0f00);
                mdio_write(tp, 0x15, 0x0f00);
                mdio_write(tp, 0x16, 0x7408);
                mdio_write(tp, 0x15, 0x0e00);
                mdio_write(tp, 0x15, 0x0f00);
                mdio_write(tp, 0x15, 0x0f01);
                mdio_write(tp, 0x16, 0x4000);
                mdio_write(tp, 0x15, 0x0e01);
                mdio_write(tp, 0x15, 0x0f01);
                mdio_write(tp, 0x15, 0x0f02);
                mdio_write(tp, 0x16, 0x9400);
                mdio_write(tp, 0x15, 0x0e02);
                mdio_write(tp, 0x15, 0x0f02);
                mdio_write(tp, 0x15, 0x0f03);
                mdio_write(tp, 0x16, 0x7408);
                mdio_write(tp, 0x15, 0x0e03);
                mdio_write(tp, 0x15, 0x0f03);
                mdio_write(tp, 0x15, 0x0f04);
                mdio_write(tp, 0x16, 0x4008);
                mdio_write(tp, 0x15, 0x0e04);
                mdio_write(tp, 0x15, 0x0f04);
                mdio_write(tp, 0x15, 0x0f05);
                mdio_write(tp, 0x16, 0x9400);
                mdio_write(tp, 0x15, 0x0e05);
                mdio_write(tp, 0x15, 0x0f05);
                mdio_write(tp, 0x15, 0x0f06);
                mdio_write(tp, 0x16, 0x0803);
                mdio_write(tp, 0x15, 0x0e06);
                mdio_write(tp, 0x15, 0x0f06);
                mdio_write(tp, 0x15, 0x0d00);
                mdio_write(tp, 0x15, 0x0100);
                mdio_write(tp, 0x1f, 0x0001);
                mdio_write(tp, 0x10, 0xf074);
                mdio_write(tp, 0x1f, 0x0000);
                mdio_write(tp, 0x17, 0x2149);

                mdio_write(tp, 0x1f, 0x0005);
                for (i = 0; i < 200; i++) {
                        udelay(100);
                        gphy_val = mdio_read(tp, 0x00);
                        if (gphy_val & BIT_7)
                                break;
                }
                mdio_write(tp, 0x1f, 0x0007);
                mdio_write(tp, 0x1e, 0x0023);
                gphy_val = mdio_read(tp, 0x17);
                gphy_val &= ~(BIT_0);
                if (tp->RequiredSecLanDonglePatch)
                        gphy_val &= ~(BIT_2);
                mdio_write(tp, 0x17, gphy_val);
                mdio_write(tp, 0x1f, 0x0000);
                mdio_write(tp, 0x1f, 0x0007);
                mdio_write(tp, 0x1e, 0x0023);
                gphy_val = mdio_read(tp, 0x17);
                gphy_val |= BIT_14;
                mdio_write(tp, 0x17, gphy_val);
                mdio_write(tp, 0x1e, 0x0020);
                gphy_val = mdio_read(tp, 0x1b);
                gphy_val |= BIT_7;
                mdio_write(tp, 0x1b, gphy_val);
                mdio_write(tp, 0x1e, 0x0041);
                mdio_write(tp, 0x15, 0x0e02);
                mdio_write(tp, 0x1e, 0x0028);
                gphy_val = mdio_read(tp, 0x19);
                gphy_val |= BIT_15;
                mdio_write(tp, 0x19, gphy_val);
                mdio_write(tp, 0x1f, 0x0000);
        } else {
                mdio_write(tp, 0x1f, 0x0000);
                mdio_write(tp, 0x00, 0x1800);
                mdio_write(tp, 0x1f, 0x0007);
                mdio_write(tp, 0x1e, 0x0023);
                mdio_write(tp, 0x17, 0x0117);
                mdio_write(tp, 0x1f, 0x0007);
                mdio_write(tp, 0x1E, 0x002C);
                mdio_write(tp, 0x1B, 0x5000);
                mdio_write(tp, 0x1f, 0x0000);
                mdio_write(tp, 0x16, 0x4104);
                for (i = 0; i < 200; i++) {
                        udelay(100);
                        gphy_val = mdio_read(tp, 0x1E);
                        gphy_val &= 0x03FF;
                        if (gphy_val==0x000C)
                                break;
                }
                mdio_write(tp, 0x1f, 0x0005);
                for (i = 0; i < 200; i++) {
                        udelay(100);
                        gphy_val = mdio_read(tp, 0x07);
                        if ((gphy_val & BIT_5) == 0)
                                break;
                }
                gphy_val = mdio_read(tp, 0x07);
                if (gphy_val & BIT_5) {
                        mdio_write(tp, 0x1f, 0x0007);
                        mdio_write(tp, 0x1e, 0x00a1);
                        mdio_write(tp, 0x17, 0x1000);
                        mdio_write(tp, 0x17, 0x0000);
                        mdio_write(tp, 0x17, 0x2000);
                        mdio_write(tp, 0x1e, 0x002f);
                        mdio_write(tp, 0x18, 0x9bfb);
                        mdio_write(tp, 0x1f, 0x0005);
                        mdio_write(tp, 0x07, 0x0000);
                        mdio_write(tp, 0x1f, 0x0000);
                }
                mdio_write(tp, 0x1f, 0x0005);
                mdio_write(tp, 0x05, 0xfff6);
                mdio_write(tp, 0x06, 0x0080);
                gphy_val = mdio_read(tp, 0x00);
                gphy_val &= ~(BIT_7);
                mdio_write(tp, 0x00, gphy_val);
                mdio_write(tp, 0x1f, 0x0002);
                gphy_val = mdio_read(tp, 0x08);
                gphy_val &= ~(BIT_7);
                mdio_write(tp, 0x08, gphy_val);
                mdio_write(tp, 0x1f, 0x0000);

                mdio_write(tp, 0x1f, 0x0007);
                mdio_write(tp, 0x1e, 0x0023);
                mdio_write(tp, 0x16, 0x0306);
                mdio_write(tp, 0x16, 0x0307);
                mdio_write(tp, 0x15, 0x000e);
                mdio_write(tp, 0x19, 0x000a);
                mdio_write(tp, 0x15, 0x0010);
                mdio_write(tp, 0x19, 0x0008);
                mdio_write(tp, 0x15, 0x0018);
                mdio_write(tp, 0x19, 0x4801);
                mdio_write(tp, 0x15, 0x0019);
                mdio_write(tp, 0x19, 0x6801);
                mdio_write(tp, 0x15, 0x001a);
                mdio_write(tp, 0x19, 0x66a1);
                mdio_write(tp, 0x15, 0x001f);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x0020);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x0021);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x0022);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x0023);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x0024);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x0025);
                mdio_write(tp, 0x19, 0x64a1);
                mdio_write(tp, 0x15, 0x0026);
                mdio_write(tp, 0x19, 0x40ea);
                mdio_write(tp, 0x15, 0x0027);
                mdio_write(tp, 0x19, 0x4503);
                mdio_write(tp, 0x15, 0x0028);
                mdio_write(tp, 0x19, 0x9f00);
                mdio_write(tp, 0x15, 0x0029);
                mdio_write(tp, 0x19, 0xa631);
                mdio_write(tp, 0x15, 0x002a);
                mdio_write(tp, 0x19, 0x9717);
                mdio_write(tp, 0x15, 0x002b);
                mdio_write(tp, 0x19, 0x302c);
                mdio_write(tp, 0x15, 0x002c);
                mdio_write(tp, 0x19, 0x4802);
                mdio_write(tp, 0x15, 0x002d);
                mdio_write(tp, 0x19, 0x58da);
                mdio_write(tp, 0x15, 0x002e);
                mdio_write(tp, 0x19, 0x400d);
                mdio_write(tp, 0x15, 0x002f);
                mdio_write(tp, 0x19, 0x4488);
                mdio_write(tp, 0x15, 0x0030);
                mdio_write(tp, 0x19, 0x9e00);
                mdio_write(tp, 0x15, 0x0031);
                mdio_write(tp, 0x19, 0x63c8);
                mdio_write(tp, 0x15, 0x0032);
                mdio_write(tp, 0x19, 0x6481);
                mdio_write(tp, 0x15, 0x0033);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x0034);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x0035);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x0036);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x0037);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x0038);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x0039);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x003a);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x003b);
                mdio_write(tp, 0x19, 0x63e8);
                mdio_write(tp, 0x15, 0x003c);
                mdio_write(tp, 0x19, 0x7d00);
                mdio_write(tp, 0x15, 0x003d);
                mdio_write(tp, 0x19, 0x59d4);
                mdio_write(tp, 0x15, 0x003e);
                mdio_write(tp, 0x19, 0x63f8);
                mdio_write(tp, 0x15, 0x0040);
                mdio_write(tp, 0x19, 0x64a1);
                mdio_write(tp, 0x15, 0x0041);
                mdio_write(tp, 0x19, 0x30de);
                mdio_write(tp, 0x15, 0x0044);
                mdio_write(tp, 0x19, 0x480f);
                mdio_write(tp, 0x15, 0x0045);
                mdio_write(tp, 0x19, 0x6800);
                mdio_write(tp, 0x15, 0x0046);
                mdio_write(tp, 0x19, 0x6680);
                mdio_write(tp, 0x15, 0x0047);
                mdio_write(tp, 0x19, 0x7c10);
                mdio_write(tp, 0x15, 0x0048);
                mdio_write(tp, 0x19, 0x63c8);
                mdio_write(tp, 0x15, 0x0049);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x004a);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x004b);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x004c);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x004d);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x004e);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x004f);
                mdio_write(tp, 0x19, 0x40ea);
                mdio_write(tp, 0x15, 0x0050);
                mdio_write(tp, 0x19, 0x4503);
                mdio_write(tp, 0x15, 0x0051);
                mdio_write(tp, 0x19, 0x58ca);
                mdio_write(tp, 0x15, 0x0052);
                mdio_write(tp, 0x19, 0x63c8);
                mdio_write(tp, 0x15, 0x0053);
                mdio_write(tp, 0x19, 0x63d8);
                mdio_write(tp, 0x15, 0x0054);
                mdio_write(tp, 0x19, 0x66a0);
                mdio_write(tp, 0x15, 0x0055);
                mdio_write(tp, 0x19, 0x9f00);
                mdio_write(tp, 0x15, 0x0056);
                mdio_write(tp, 0x19, 0x3000);
                mdio_write(tp, 0x15, 0x00a1);
                mdio_write(tp, 0x19, 0x3044);
                mdio_write(tp, 0x15, 0x00ab);
                mdio_write(tp, 0x19, 0x5820);
                mdio_write(tp, 0x15, 0x00ac);
                mdio_write(tp, 0x19, 0x5e04);
                mdio_write(tp, 0x15, 0x00ad);
                mdio_write(tp, 0x19, 0xb60c);
                mdio_write(tp, 0x15, 0x00af);
                mdio_write(tp, 0x19, 0x000a);
                mdio_write(tp, 0x15, 0x00b2);
                mdio_write(tp, 0x19, 0x30b9);
                mdio_write(tp, 0x15, 0x00b9);
                mdio_write(tp, 0x19, 0x4408);
                mdio_write(tp, 0x15, 0x00ba);
                mdio_write(tp, 0x19, 0x480b);
                mdio_write(tp, 0x15, 0x00bb);
                mdio_write(tp, 0x19, 0x5e00);
                mdio_write(tp, 0x15, 0x00bc);
                mdio_write(tp, 0x19, 0x405f);
                mdio_write(tp, 0x15, 0x00bd);
                mdio_write(tp, 0x19, 0x4448);
                mdio_write(tp, 0x15, 0x00be);
                mdio_write(tp, 0x19, 0x4020);
                mdio_write(tp, 0x15, 0x00bf);
                mdio_write(tp, 0x19, 0x4468);
                mdio_write(tp, 0x15, 0x00c0);
                mdio_write(tp, 0x19, 0x9c02);
                mdio_write(tp, 0x15, 0x00c1);
                mdio_write(tp, 0x19, 0x58a0);
                mdio_write(tp, 0x15, 0x00c2);
                mdio_write(tp, 0x19, 0xb605);
                mdio_write(tp, 0x15, 0x00c3);
                mdio_write(tp, 0x19, 0xc0d3);
                mdio_write(tp, 0x15, 0x00c4);
                mdio_write(tp, 0x19, 0x00e6);
                mdio_write(tp, 0x15, 0x00c5);
                mdio_write(tp, 0x19, 0xdaec);
                mdio_write(tp, 0x15, 0x00c6);
                mdio_write(tp, 0x19, 0x00fa);
                mdio_write(tp, 0x15, 0x00c7);
                mdio_write(tp, 0x19, 0x9df9);
                mdio_write(tp, 0x15, 0x0112);
                mdio_write(tp, 0x19, 0x6421);
                mdio_write(tp, 0x15, 0x0113);
                mdio_write(tp, 0x19, 0x7c08);
                mdio_write(tp, 0x15, 0x0114);
                mdio_write(tp, 0x19, 0x63f0);
                mdio_write(tp, 0x15, 0x0115);
                mdio_write(tp, 0x19, 0x4003);
                mdio_write(tp, 0x15, 0x0116);
                mdio_write(tp, 0x19, 0x4418);
                mdio_write(tp, 0x15, 0x0117);
                mdio_write(tp, 0x19, 0x9b00);
                mdio_write(tp, 0x15, 0x0118);
                mdio_write(tp, 0x19, 0x6461);
                mdio_write(tp, 0x15, 0x0119);
                mdio_write(tp, 0x19, 0x64e1);
                mdio_write(tp, 0x15, 0x011a);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x0150);
                mdio_write(tp, 0x19, 0x7c80);
                mdio_write(tp, 0x15, 0x0151);
                mdio_write(tp, 0x19, 0x6461);
                mdio_write(tp, 0x15, 0x0152);
                mdio_write(tp, 0x19, 0x4003);
                mdio_write(tp, 0x15, 0x0153);
                mdio_write(tp, 0x19, 0x4540);
                mdio_write(tp, 0x15, 0x0154);
                mdio_write(tp, 0x19, 0x9f00);
                mdio_write(tp, 0x15, 0x0155);
                mdio_write(tp, 0x19, 0x9d00);
                mdio_write(tp, 0x15, 0x0156);
                mdio_write(tp, 0x19, 0x7c40);
                mdio_write(tp, 0x15, 0x0157);
                mdio_write(tp, 0x19, 0x6421);
                mdio_write(tp, 0x15, 0x0158);
                mdio_write(tp, 0x19, 0x7c80);
                mdio_write(tp, 0x15, 0x0159);
                mdio_write(tp, 0x19, 0x64a1);
                mdio_write(tp, 0x15, 0x015a);
                mdio_write(tp, 0x19, 0x30fe);
                mdio_write(tp, 0x15, 0x029c);
                mdio_write(tp, 0x19, 0x0070);
                mdio_write(tp, 0x15, 0x02b2);
                mdio_write(tp, 0x19, 0x005a);
                mdio_write(tp, 0x15, 0x02bd);
                mdio_write(tp, 0x19, 0xa522);
                mdio_write(tp, 0x15, 0x02ce);
                mdio_write(tp, 0x19, 0xb63e);
                mdio_write(tp, 0x15, 0x02d9);
                mdio_write(tp, 0x19, 0x32df);
                mdio_write(tp, 0x15, 0x02df);
                mdio_write(tp, 0x19, 0x4500);
                mdio_write(tp, 0x15, 0x02f4);
                mdio_write(tp, 0x19, 0xb618);
                mdio_write(tp, 0x15, 0x02fb);
                mdio_write(tp, 0x19, 0xb900);
                mdio_write(tp, 0x15, 0x02fc);
                mdio_write(tp, 0x19, 0x49b5);
                mdio_write(tp, 0x15, 0x02fd);
                mdio_write(tp, 0x19, 0x6812);
                mdio_write(tp, 0x15, 0x02fe);
                mdio_write(tp, 0x19, 0x66a0);
                mdio_write(tp, 0x15, 0x02ff);
                mdio_write(tp, 0x19, 0x9900);
                mdio_write(tp, 0x15, 0x0300);
                mdio_write(tp, 0x19, 0x64a0);
                mdio_write(tp, 0x15, 0x0301);
                mdio_write(tp, 0x19, 0x3316);
                mdio_write(tp, 0x15, 0x0308);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x030c);
                mdio_write(tp, 0x19, 0x3000);
                mdio_write(tp, 0x15, 0x0312);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x0313);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x0314);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x0315);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x0316);
                mdio_write(tp, 0x19, 0x49b5);
                mdio_write(tp, 0x15, 0x0317);
                mdio_write(tp, 0x19, 0x7d00);
                mdio_write(tp, 0x15, 0x0318);
                mdio_write(tp, 0x19, 0x4d00);
                mdio_write(tp, 0x15, 0x0319);
                mdio_write(tp, 0x19, 0x6810);
                mdio_write(tp, 0x15, 0x031a);
                mdio_write(tp, 0x19, 0x6c08);
                mdio_write(tp, 0x15, 0x031b);
                mdio_write(tp, 0x19, 0x4925);
                mdio_write(tp, 0x15, 0x031c);
                mdio_write(tp, 0x19, 0x403b);
                mdio_write(tp, 0x15, 0x031d);
                mdio_write(tp, 0x19, 0xa602);
                mdio_write(tp, 0x15, 0x031e);
                mdio_write(tp, 0x19, 0x402f);
                mdio_write(tp, 0x15, 0x031f);
                mdio_write(tp, 0x19, 0x4484);
                mdio_write(tp, 0x15, 0x0320);
                mdio_write(tp, 0x19, 0x40c8);
                mdio_write(tp, 0x15, 0x0321);
                mdio_write(tp, 0x19, 0x44c4);
                mdio_write(tp, 0x15, 0x0322);
                mdio_write(tp, 0x19, 0x404f);
                mdio_write(tp, 0x15, 0x0323);
                mdio_write(tp, 0x19, 0x44c8);
                mdio_write(tp, 0x15, 0x0324);
                mdio_write(tp, 0x19, 0xd64f);
                mdio_write(tp, 0x15, 0x0325);
                mdio_write(tp, 0x19, 0x00e7);
                mdio_write(tp, 0x15, 0x0326);
                mdio_write(tp, 0x19, 0x7c08);
                mdio_write(tp, 0x15, 0x0327);
                mdio_write(tp, 0x19, 0x8203);
                mdio_write(tp, 0x15, 0x0328);
                mdio_write(tp, 0x19, 0x4d48);
                mdio_write(tp, 0x15, 0x0329);
                mdio_write(tp, 0x19, 0x332b);
                mdio_write(tp, 0x15, 0x032a);
                mdio_write(tp, 0x19, 0x4d40);
                mdio_write(tp, 0x15, 0x032c);
                mdio_write(tp, 0x19, 0x00f8);
                mdio_write(tp, 0x15, 0x032d);
                mdio_write(tp, 0x19, 0x82b2);
                mdio_write(tp, 0x15, 0x032f);
                mdio_write(tp, 0x19, 0x00b0);
                mdio_write(tp, 0x15, 0x0332);
                mdio_write(tp, 0x19, 0x91f2);
                mdio_write(tp, 0x15, 0x033f);
                mdio_write(tp, 0x19, 0xb6cd);
                mdio_write(tp, 0x15, 0x0340);
                mdio_write(tp, 0x19, 0x9e01);
                mdio_write(tp, 0x15, 0x0341);
                mdio_write(tp, 0x19, 0xd11d);
                mdio_write(tp, 0x15, 0x0342);
                mdio_write(tp, 0x19, 0x009d);
                mdio_write(tp, 0x15, 0x0343);
                mdio_write(tp, 0x19, 0xbb1c);
                mdio_write(tp, 0x15, 0x0344);
                mdio_write(tp, 0x19, 0x8102);
                mdio_write(tp, 0x15, 0x0345);
                mdio_write(tp, 0x19, 0x3348);
                mdio_write(tp, 0x15, 0x0346);
                mdio_write(tp, 0x19, 0xa231);
                mdio_write(tp, 0x15, 0x0347);
                mdio_write(tp, 0x19, 0x335b);
                mdio_write(tp, 0x15, 0x0348);
                mdio_write(tp, 0x19, 0x91f7);
                mdio_write(tp, 0x15, 0x0349);
                mdio_write(tp, 0x19, 0xc218);
                mdio_write(tp, 0x15, 0x034a);
                mdio_write(tp, 0x19, 0x00f5);
                mdio_write(tp, 0x15, 0x034b);
                mdio_write(tp, 0x19, 0x335b);
                mdio_write(tp, 0x15, 0x034c);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x034d);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x034e);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x034f);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x0350);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x035b);
                mdio_write(tp, 0x19, 0xa23c);
                mdio_write(tp, 0x15, 0x035c);
                mdio_write(tp, 0x19, 0x7c08);
                mdio_write(tp, 0x15, 0x035d);
                mdio_write(tp, 0x19, 0x4c00);
                mdio_write(tp, 0x15, 0x035e);
                mdio_write(tp, 0x19, 0x3397);
                mdio_write(tp, 0x15, 0x0363);
                mdio_write(tp, 0x19, 0xb6a9);
                mdio_write(tp, 0x15, 0x0366);
                mdio_write(tp, 0x19, 0x00f5);
                mdio_write(tp, 0x15, 0x0382);
                mdio_write(tp, 0x19, 0x7c40);
                mdio_write(tp, 0x15, 0x0388);
                mdio_write(tp, 0x19, 0x0084);
                mdio_write(tp, 0x15, 0x0389);
                mdio_write(tp, 0x19, 0xdd17);
                mdio_write(tp, 0x15, 0x038a);
                mdio_write(tp, 0x19, 0x000b);
                mdio_write(tp, 0x15, 0x038b);
                mdio_write(tp, 0x19, 0xa10a);
                mdio_write(tp, 0x15, 0x038c);
                mdio_write(tp, 0x19, 0x337e);
                mdio_write(tp, 0x15, 0x038d);
                mdio_write(tp, 0x19, 0x6c0b);
                mdio_write(tp, 0x15, 0x038e);
                mdio_write(tp, 0x19, 0xa107);
                mdio_write(tp, 0x15, 0x038f);
                mdio_write(tp, 0x19, 0x6c08);
                mdio_write(tp, 0x15, 0x0390);
                mdio_write(tp, 0x19, 0xc017);
                mdio_write(tp, 0x15, 0x0391);
                mdio_write(tp, 0x19, 0x0004);
                mdio_write(tp, 0x15, 0x0392);
                mdio_write(tp, 0x19, 0xd64f);
                mdio_write(tp, 0x15, 0x0393);
                mdio_write(tp, 0x19, 0x00f4);
                mdio_write(tp, 0x15, 0x0397);
                mdio_write(tp, 0x19, 0x4098);
                mdio_write(tp, 0x15, 0x0398);
                mdio_write(tp, 0x19, 0x4408);
                mdio_write(tp, 0x15, 0x0399);
                mdio_write(tp, 0x19, 0x55bf);
                mdio_write(tp, 0x15, 0x039a);
                mdio_write(tp, 0x19, 0x4bb9);
                mdio_write(tp, 0x15, 0x039b);
                mdio_write(tp, 0x19, 0x6810);
                mdio_write(tp, 0x15, 0x039c);
                mdio_write(tp, 0x19, 0x4b29);
                mdio_write(tp, 0x15, 0x039d);
                mdio_write(tp, 0x19, 0x4041);
                mdio_write(tp, 0x15, 0x039e);
                mdio_write(tp, 0x19, 0x442a);
                mdio_write(tp, 0x15, 0x039f);
                mdio_write(tp, 0x19, 0x4029);
                mdio_write(tp, 0x15, 0x03aa);
                mdio_write(tp, 0x19, 0x33b8);
                mdio_write(tp, 0x15, 0x03b6);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x03b7);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x03b8);
                mdio_write(tp, 0x19, 0x543f);
                mdio_write(tp, 0x15, 0x03b9);
                mdio_write(tp, 0x19, 0x499a);
                mdio_write(tp, 0x15, 0x03ba);
                mdio_write(tp, 0x19, 0x7c40);
                mdio_write(tp, 0x15, 0x03bb);
                mdio_write(tp, 0x19, 0x4c40);
                mdio_write(tp, 0x15, 0x03bc);
                mdio_write(tp, 0x19, 0x490a);
                mdio_write(tp, 0x15, 0x03bd);
                mdio_write(tp, 0x19, 0x405e);
                mdio_write(tp, 0x15, 0x03c2);
                mdio_write(tp, 0x19, 0x9a03);
                mdio_write(tp, 0x15, 0x03c4);
                mdio_write(tp, 0x19, 0x0015);
                mdio_write(tp, 0x15, 0x03c5);
                mdio_write(tp, 0x19, 0x9e03);
                mdio_write(tp, 0x15, 0x03c8);
                mdio_write(tp, 0x19, 0x9cf7);
                mdio_write(tp, 0x15, 0x03c9);
                mdio_write(tp, 0x19, 0x7c12);
                mdio_write(tp, 0x15, 0x03ca);
                mdio_write(tp, 0x19, 0x4c52);
                mdio_write(tp, 0x15, 0x03cb);
                mdio_write(tp, 0x19, 0x4458);
                mdio_write(tp, 0x15, 0x03cd);
                mdio_write(tp, 0x19, 0x4c40);
                mdio_write(tp, 0x15, 0x03ce);
                mdio_write(tp, 0x19, 0x33bf);
                mdio_write(tp, 0x15, 0x03cf);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x03d0);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x03d1);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x03d5);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x03d6);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x03d7);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x03d8);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x03d9);
                mdio_write(tp, 0x19, 0x49bb);
                mdio_write(tp, 0x15, 0x03da);
                mdio_write(tp, 0x19, 0x4478);
                mdio_write(tp, 0x15, 0x03db);
                mdio_write(tp, 0x19, 0x492b);
                mdio_write(tp, 0x15, 0x03dc);
                mdio_write(tp, 0x19, 0x7c01);
                mdio_write(tp, 0x15, 0x03dd);
                mdio_write(tp, 0x19, 0x4c00);
                mdio_write(tp, 0x15, 0x03de);
                mdio_write(tp, 0x19, 0xbd1a);
                mdio_write(tp, 0x15, 0x03df);
                mdio_write(tp, 0x19, 0xc428);
                mdio_write(tp, 0x15, 0x03e0);
                mdio_write(tp, 0x19, 0x0008);
                mdio_write(tp, 0x15, 0x03e1);
                mdio_write(tp, 0x19, 0x9cfd);
                mdio_write(tp, 0x15, 0x03e2);
                mdio_write(tp, 0x19, 0x7c12);
                mdio_write(tp, 0x15, 0x03e3);
                mdio_write(tp, 0x19, 0x4c52);
                mdio_write(tp, 0x15, 0x03e4);
                mdio_write(tp, 0x19, 0x4458);
                mdio_write(tp, 0x15, 0x03e5);
                mdio_write(tp, 0x19, 0x7c12);
                mdio_write(tp, 0x15, 0x03e6);
                mdio_write(tp, 0x19, 0x4c40);
                mdio_write(tp, 0x15, 0x03e7);
                mdio_write(tp, 0x19, 0x33de);
                mdio_write(tp, 0x15, 0x03e8);
                mdio_write(tp, 0x19, 0xc218);
                mdio_write(tp, 0x15, 0x03e9);
                mdio_write(tp, 0x19, 0x0002);
                mdio_write(tp, 0x15, 0x03ea);
                mdio_write(tp, 0x19, 0x32df);
                mdio_write(tp, 0x15, 0x03eb);
                mdio_write(tp, 0x19, 0x3316);
                mdio_write(tp, 0x15, 0x03ec);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x03ed);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x03ee);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x03ef);
                mdio_write(tp, 0x19, 0x0000);
                mdio_write(tp, 0x15, 0x03f7);
                mdio_write(tp, 0x19, 0x330c);
                mdio_write(tp, 0x16, 0x0306);
                mdio_write(tp, 0x16, 0x0300);

                mdio_write(tp, 0x1f, 0x0005);
                mdio_write(tp, 0x05, 0xfff6);
                mdio_write(tp, 0x06, 0x0080);
                mdio_write(tp, 0x05, 0x8000);
                mdio_write(tp, 0x06, 0x0280);
                mdio_write(tp, 0x06, 0x48f7);
                mdio_write(tp, 0x06, 0x00e0);
                mdio_write(tp, 0x06, 0xfff7);
                mdio_write(tp, 0x06, 0xa080);
                mdio_write(tp, 0x06, 0x02ae);
                mdio_write(tp, 0x06, 0xf602);
                mdio_write(tp, 0x06, 0x0200);
                mdio_write(tp, 0x06, 0x0280);
                mdio_write(tp, 0x06, 0x9002);
                mdio_write(tp, 0x06, 0x0224);
                mdio_write(tp, 0x06, 0x0202);
                mdio_write(tp, 0x06, 0x3402);
                mdio_write(tp, 0x06, 0x027f);
                mdio_write(tp, 0x06, 0x0280);
                mdio_write(tp, 0x06, 0xa602);
                mdio_write(tp, 0x06, 0x80bf);
                mdio_write(tp, 0x06, 0xe08b);
                mdio_write(tp, 0x06, 0x88e1);
                mdio_write(tp, 0x06, 0x8b89);
                mdio_write(tp, 0x06, 0x1e01);
                mdio_write(tp, 0x06, 0xe18b);
                mdio_write(tp, 0x06, 0x8a1e);
                mdio_write(tp, 0x06, 0x01e1);
                mdio_write(tp, 0x06, 0x8b8b);
                mdio_write(tp, 0x06, 0x1e01);
                mdio_write(tp, 0x06, 0xe18b);
                mdio_write(tp, 0x06, 0x8c1e);
                mdio_write(tp, 0x06, 0x01e1);
                mdio_write(tp, 0x06, 0x8b8d);
                mdio_write(tp, 0x06, 0x1e01);
                mdio_write(tp, 0x06, 0xe18b);
                mdio_write(tp, 0x06, 0x8e1e);
                mdio_write(tp, 0x06, 0x01a0);
                mdio_write(tp, 0x06, 0x00c7);
                mdio_write(tp, 0x06, 0xaebb);
                mdio_write(tp, 0x06, 0xee8a);
                mdio_write(tp, 0x06, 0xe600);
                mdio_write(tp, 0x06, 0xee8a);
                mdio_write(tp, 0x06, 0xee03);
                mdio_write(tp, 0x06, 0xee8a);
                mdio_write(tp, 0x06, 0xefb8);
                mdio_write(tp, 0x06, 0xee8a);
                mdio_write(tp, 0x06, 0xe902);
                mdio_write(tp, 0x06, 0xee8b);
                mdio_write(tp, 0x06, 0x8285);
                mdio_write(tp, 0x06, 0xee8b);
                mdio_write(tp, 0x06, 0x8520);
                mdio_write(tp, 0x06, 0xee8b);
                mdio_write(tp, 0x06, 0x8701);
                mdio_write(tp, 0x06, 0xd481);
                mdio_write(tp, 0x06, 0x35e4);
                mdio_write(tp, 0x06, 0x8b94);
                mdio_write(tp, 0x06, 0xe58b);
                mdio_write(tp, 0x06, 0x95bf);
                mdio_write(tp, 0x06, 0x8b88);
                mdio_write(tp, 0x06, 0xec00);
                mdio_write(tp, 0x06, 0x19a9);
                mdio_write(tp, 0x06, 0x8b90);
                mdio_write(tp, 0x06, 0xf9ee);
                mdio_write(tp, 0x06, 0xfff6);
                mdio_write(tp, 0x06, 0x00ee);
                mdio_write(tp, 0x06, 0xfff7);
                mdio_write(tp, 0x06, 0xffe0);
                mdio_write(tp, 0x06, 0xe140);
                mdio_write(tp, 0x06, 0xe1e1);
                mdio_write(tp, 0x06, 0x41f7);
                mdio_write(tp, 0x06, 0x2ff6);
                mdio_write(tp, 0x06, 0x28e4);
                mdio_write(tp, 0x06, 0xe140);
                mdio_write(tp, 0x06, 0xe5e1);
                mdio_write(tp, 0x06, 0x4104);
                mdio_write(tp, 0x06, 0xf8e0);
                mdio_write(tp, 0x06, 0x8b89);
                mdio_write(tp, 0x06, 0xad20);
                mdio_write(tp, 0x06, 0x0dee);
                mdio_write(tp, 0x06, 0x8b89);
                mdio_write(tp, 0x06, 0x0002);
                mdio_write(tp, 0x06, 0x82f4);
                mdio_write(tp, 0x06, 0x021f);
                mdio_write(tp, 0x06, 0x4102);
                mdio_write(tp, 0x06, 0x2812);
                mdio_write(tp, 0x06, 0xfc04);
                mdio_write(tp, 0x06, 0xf8e0);
                mdio_write(tp, 0x06, 0x8b8d);
                mdio_write(tp, 0x06, 0xad20);
                mdio_write(tp, 0x06, 0x10ee);
                mdio_write(tp, 0x06, 0x8b8d);
                mdio_write(tp, 0x06, 0x0002);
                mdio_write(tp, 0x06, 0x139d);
                mdio_write(tp, 0x06, 0x0281);
                mdio_write(tp, 0x06, 0xd602);
                mdio_write(tp, 0x06, 0x1f99);
                mdio_write(tp, 0x06, 0x0227);
                mdio_write(tp, 0x06, 0xeafc);
                mdio_write(tp, 0x06, 0x04f8);
                mdio_write(tp, 0x06, 0xe08b);
                mdio_write(tp, 0x06, 0x8ead);
                mdio_write(tp, 0x06, 0x2014);
                mdio_write(tp, 0x06, 0xf620);
                mdio_write(tp, 0x06, 0xe48b);
                mdio_write(tp, 0x06, 0x8e02);
                mdio_write(tp, 0x06, 0x8104);
                mdio_write(tp, 0x06, 0x021b);
                mdio_write(tp, 0x06, 0xf402);
                mdio_write(tp, 0x06, 0x2c9c);
                mdio_write(tp, 0x06, 0x0281);
                mdio_write(tp, 0x06, 0x7902);
                mdio_write(tp, 0x06, 0x8443);
                mdio_write(tp, 0x06, 0xad22);
                mdio_write(tp, 0x06, 0x11f6);
                mdio_write(tp, 0x06, 0x22e4);
                mdio_write(tp, 0x06, 0x8b8e);
                mdio_write(tp, 0x06, 0x022c);
                mdio_write(tp, 0x06, 0x4602);
                mdio_write(tp, 0x06, 0x2ac5);
                mdio_write(tp, 0x06, 0x0229);
                mdio_write(tp, 0x06, 0x2002);
                mdio_write(tp, 0x06, 0x2b91);
                mdio_write(tp, 0x06, 0xad25);
                mdio_write(tp, 0x06, 0x11f6);
                mdio_write(tp, 0x06, 0x25e4);
                mdio_write(tp, 0x06, 0x8b8e);
                mdio_write(tp, 0x06, 0x0284);
                mdio_write(tp, 0x06, 0xe202);
                mdio_write(tp, 0x06, 0x043a);
                mdio_write(tp, 0x06, 0x021a);
                mdio_write(tp, 0x06, 0x5902);
                mdio_write(tp, 0x06, 0x2bfc);
                mdio_write(tp, 0x06, 0xfc04);
                mdio_write(tp, 0x06, 0xf8fa);
                mdio_write(tp, 0x06, 0xef69);
                mdio_write(tp, 0x06, 0xe0e0);
                mdio_write(tp, 0x06, 0x00e1);
                mdio_write(tp, 0x06, 0xe001);
                mdio_write(tp, 0x06, 0xad27);
                mdio_write(tp, 0x06, 0x1fd1);
                mdio_write(tp, 0x06, 0x01bf);
                mdio_write(tp, 0x06, 0x8638);
                mdio_write(tp, 0x06, 0x022f);
                mdio_write(tp, 0x06, 0x50e0);
                mdio_write(tp, 0x06, 0xe020);
                mdio_write(tp, 0x06, 0xe1e0);
                mdio_write(tp, 0x06, 0x21ad);
                mdio_write(tp, 0x06, 0x200e);
                mdio_write(tp, 0x06, 0xd100);
                mdio_write(tp, 0x06, 0xbf86);
                mdio_write(tp, 0x06, 0x3802);
                mdio_write(tp, 0x06, 0x2f50);
                mdio_write(tp, 0x06, 0xbf3d);
                mdio_write(tp, 0x06, 0x3902);
                mdio_write(tp, 0x06, 0x2eb0);
                mdio_write(tp, 0x06, 0xef96);
                mdio_write(tp, 0x06, 0xfefc);
                mdio_write(tp, 0x06, 0x0402);
                mdio_write(tp, 0x06, 0x8591);
                mdio_write(tp, 0x06, 0x0281);
                mdio_write(tp, 0x06, 0x3c05);
                mdio_write(tp, 0x06, 0xf8fa);
                mdio_write(tp, 0x06, 0xef69);
                mdio_write(tp, 0x06, 0xe0e2);
                mdio_write(tp, 0x06, 0xfee1);
                mdio_write(tp, 0x06, 0xe2ff);
                mdio_write(tp, 0x06, 0xad2d);
                mdio_write(tp, 0x06, 0x1ae0);
                mdio_write(tp, 0x06, 0xe14e);
                mdio_write(tp, 0x06, 0xe1e1);
                mdio_write(tp, 0x06, 0x4fac);
                mdio_write(tp, 0x06, 0x2d22);
                mdio_write(tp, 0x06, 0xf603);
                mdio_write(tp, 0x06, 0x0203);
                mdio_write(tp, 0x06, 0x36f7);
                mdio_write(tp, 0x06, 0x03f7);
                mdio_write(tp, 0x06, 0x06bf);
                mdio_write(tp, 0x06, 0x8622);
                mdio_write(tp, 0x06, 0x022e);
                mdio_write(tp, 0x06, 0xb0ae);
                mdio_write(tp, 0x06, 0x11e0);
                mdio_write(tp, 0x06, 0xe14e);
                mdio_write(tp, 0x06, 0xe1e1);
                mdio_write(tp, 0x06, 0x4fad);
                mdio_write(tp, 0x06, 0x2d08);
                mdio_write(tp, 0x06, 0xbf86);
                mdio_write(tp, 0x06, 0x2d02);
                mdio_write(tp, 0x06, 0x2eb0);
                mdio_write(tp, 0x06, 0xf606);
                mdio_write(tp, 0x06, 0xef96);
                mdio_write(tp, 0x06, 0xfefc);
                mdio_write(tp, 0x06, 0x04f8);
                mdio_write(tp, 0x06, 0xf9fa);
                mdio_write(tp, 0x06, 0xef69);
                mdio_write(tp, 0x06, 0xe08b);
                mdio_write(tp, 0x06, 0x87ad);
                mdio_write(tp, 0x06, 0x204c);
                mdio_write(tp, 0x06, 0xd200);
                mdio_write(tp, 0x06, 0xe0e2);
                mdio_write(tp, 0x06, 0x0058);
                mdio_write(tp, 0x06, 0x010c);
                mdio_write(tp, 0x06, 0x021e);
                mdio_write(tp, 0x06, 0x20e0);
                mdio_write(tp, 0x06, 0xe000);
                mdio_write(tp, 0x06, 0x5810);
                mdio_write(tp, 0x06, 0x1e20);
                mdio_write(tp, 0x06, 0xe0e0);
                mdio_write(tp, 0x06, 0x3658);
                mdio_write(tp, 0x06, 0x031e);
                mdio_write(tp, 0x06, 0x20e0);
                mdio_write(tp, 0x06, 0xe022);
                mdio_write(tp, 0x06, 0xe1e0);
                mdio_write(tp, 0x06, 0x2358);
                mdio_write(tp, 0x06, 0xe01e);
                mdio_write(tp, 0x06, 0x20e0);
                mdio_write(tp, 0x06, 0x8ae6);
                mdio_write(tp, 0x06, 0x1f02);
                mdio_write(tp, 0x06, 0x9e22);
                mdio_write(tp, 0x06, 0xe68a);
                mdio_write(tp, 0x06, 0xe6ad);
                mdio_write(tp, 0x06, 0x3214);
                mdio_write(tp, 0x06, 0xad34);
                mdio_write(tp, 0x06, 0x11ef);
                mdio_write(tp, 0x06, 0x0258);
                mdio_write(tp, 0x06, 0x039e);
                mdio_write(tp, 0x06, 0x07ad);
                mdio_write(tp, 0x06, 0x3508);
                mdio_write(tp, 0x06, 0x5ac0);
                mdio_write(tp, 0x06, 0x9f04);
                mdio_write(tp, 0x06, 0xd101);
                mdio_write(tp, 0x06, 0xae02);
                mdio_write(tp, 0x06, 0xd100);
                mdio_write(tp, 0x06, 0xbf86);
                mdio_write(tp, 0x06, 0x3e02);
                mdio_write(tp, 0x06, 0x2f50);
                mdio_write(tp, 0x06, 0xef96);
                mdio_write(tp, 0x06, 0xfefd);
                mdio_write(tp, 0x06, 0xfc04);
                mdio_write(tp, 0x06, 0xf8f9);
                mdio_write(tp, 0x06, 0xfae0);
                mdio_write(tp, 0x06, 0x8b81);
                mdio_write(tp, 0x06, 0xac26);
                mdio_write(tp, 0x06, 0x0ee0);
                mdio_write(tp, 0x06, 0x8b81);
                mdio_write(tp, 0x06, 0xac21);
                mdio_write(tp, 0x06, 0x08e0);
                mdio_write(tp, 0x06, 0x8b87);
                mdio_write(tp, 0x06, 0xac24);
                mdio_write(tp, 0x06, 0x02ae);
                mdio_write(tp, 0x06, 0x6bee);
                mdio_write(tp, 0x06, 0xe0ea);
                mdio_write(tp, 0x06, 0x00ee);
                mdio_write(tp, 0x06, 0xe0eb);
                mdio_write(tp, 0x06, 0x00e2);
                mdio_write(tp, 0x06, 0xe07c);
                mdio_write(tp, 0x06, 0xe3e0);
                mdio_write(tp, 0x06, 0x7da5);
                mdio_write(tp, 0x06, 0x1111);
                mdio_write(tp, 0x06, 0x15d2);
                mdio_write(tp, 0x06, 0x60d6);
                mdio_write(tp, 0x06, 0x6666);
                mdio_write(tp, 0x06, 0x0207);
                mdio_write(tp, 0x06, 0xf9d2);
                mdio_write(tp, 0x06, 0xa0d6);
                mdio_write(tp, 0x06, 0xaaaa);
                mdio_write(tp, 0x06, 0x0207);
                mdio_write(tp, 0x06, 0xf902);
                mdio_write(tp, 0x06, 0x825c);
                mdio_write(tp, 0x06, 0xae44);
                mdio_write(tp, 0x06, 0xa566);
                mdio_write(tp, 0x06, 0x6602);
                mdio_write(tp, 0x06, 0xae38);
                mdio_write(tp, 0x06, 0xa5aa);
                mdio_write(tp, 0x06, 0xaa02);
                mdio_write(tp, 0x06, 0xae32);
                mdio_write(tp, 0x06, 0xeee0);
                mdio_write(tp, 0x06, 0xea04);
                mdio_write(tp, 0x06, 0xeee0);
                mdio_write(tp, 0x06, 0xeb06);
                mdio_write(tp, 0x06, 0xe2e0);
                mdio_write(tp, 0x06, 0x7ce3);
                mdio_write(tp, 0x06, 0xe07d);
                mdio_write(tp, 0x06, 0xe0e0);
                mdio_write(tp, 0x06, 0x38e1);
                mdio_write(tp, 0x06, 0xe039);
                mdio_write(tp, 0x06, 0xad2e);
                mdio_write(tp, 0x06, 0x21ad);
                mdio_write(tp, 0x06, 0x3f13);
                mdio_write(tp, 0x06, 0xe0e4);
                mdio_write(tp, 0x06, 0x14e1);
                mdio_write(tp, 0x06, 0xe415);
                mdio_write(tp, 0x06, 0x6880);
                mdio_write(tp, 0x06, 0xe4e4);
                mdio_write(tp, 0x06, 0x14e5);
                mdio_write(tp, 0x06, 0xe415);
                mdio_write(tp, 0x06, 0x0282);
                mdio_write(tp, 0x06, 0x5cae);
                mdio_write(tp, 0x06, 0x0bac);
                mdio_write(tp, 0x06, 0x3e02);
                mdio_write(tp, 0x06, 0xae06);
                mdio_write(tp, 0x06, 0x0282);
                mdio_write(tp, 0x06, 0x8602);
                mdio_write(tp, 0x06, 0x82b0);
                mdio_write(tp, 0x06, 0xfefd);
                mdio_write(tp, 0x06, 0xfc04);
                mdio_write(tp, 0x06, 0xf8e1);
                mdio_write(tp, 0x06, 0x8b2e);
                mdio_write(tp, 0x06, 0xe08b);
                mdio_write(tp, 0x06, 0x81ad);
                mdio_write(tp, 0x06, 0x2605);
                mdio_write(tp, 0x06, 0x0221);
                mdio_write(tp, 0x06, 0xf3f7);
                mdio_write(tp, 0x06, 0x28e0);
                mdio_write(tp, 0x06, 0x8b81);
                mdio_write(tp, 0x06, 0xad21);
                mdio_write(tp, 0x06, 0x0502);
                mdio_write(tp, 0x06, 0x22f8);
                mdio_write(tp, 0x06, 0xf729);
                mdio_write(tp, 0x06, 0xe08b);
                mdio_write(tp, 0x06, 0x87ad);
                mdio_write(tp, 0x06, 0x2405);
                mdio_write(tp, 0x06, 0x0282);
                mdio_write(tp, 0x06, 0xebf7);
                mdio_write(tp, 0x06, 0x2ae5);
                mdio_write(tp, 0x06, 0x8b2e);
                mdio_write(tp, 0x06, 0xfc04);
                mdio_write(tp, 0x06, 0xf8e0);
                mdio_write(tp, 0x06, 0x8b81);
                mdio_write(tp, 0x06, 0xad26);
                mdio_write(tp, 0x06, 0x0302);
                mdio_write(tp, 0x06, 0x2134);
                mdio_write(tp, 0x06, 0xe08b);
                mdio_write(tp, 0x06, 0x81ad);
                mdio_write(tp, 0x06, 0x2109);
                mdio_write(tp, 0x06, 0xe08b);
                mdio_write(tp, 0x06, 0x2eac);
                mdio_write(tp, 0x06, 0x2003);
                mdio_write(tp, 0x06, 0x0283);
                mdio_write(tp, 0x06, 0x52e0);
                mdio_write(tp, 0x06, 0x8b87);
                mdio_write(tp, 0x06, 0xad24);
                mdio_write(tp, 0x06, 0x09e0);
                mdio_write(tp, 0x06, 0x8b2e);
                mdio_write(tp, 0x06, 0xac21);
                mdio_write(tp, 0x06, 0x0302);
                mdio_write(tp, 0x06, 0x8337);
                mdio_write(tp, 0x06, 0xfc04);
                mdio_write(tp, 0x06, 0xf8e1);
                mdio_write(tp, 0x06, 0x8b2e);
                mdio_write(tp, 0x06, 0xe08b);
                mdio_write(tp, 0x06, 0x81ad);
                mdio_write(tp, 0x06, 0x2608);
                mdio_write(tp, 0x06, 0xe085);
                mdio_write(tp, 0x06, 0xd2ad);
                mdio_write(tp, 0x06, 0x2502);
                mdio_write(tp, 0x06, 0xf628);
                mdio_write(tp, 0x06, 0xe08b);
                mdio_write(tp, 0x06, 0x81ad);
                mdio_write(tp, 0x06, 0x210a);
                mdio_write(tp, 0x06, 0xe086);
                mdio_write(tp, 0x06, 0x0af6);
                mdio_write(tp, 0x06, 0x27a0);
                mdio_write(tp, 0x06, 0x0502);
                mdio_write(tp, 0x06, 0xf629);
                mdio_write(tp, 0x06, 0xe08b);
                mdio_write(tp, 0x06, 0x87ad);
                mdio_write(tp, 0x06, 0x2408);
                mdio_write(tp, 0x06, 0xe08a);
                mdio_write(tp, 0x06, 0xedad);
                mdio_write(tp, 0x06, 0x2002);
                mdio_write(tp, 0x06, 0xf62a);
                mdio_write(tp, 0x06, 0xe58b);
                mdio_write(tp, 0x06, 0x2ea1);
                mdio_write(tp, 0x06, 0x0003);
                mdio_write(tp, 0x06, 0x0221);
                mdio_write(tp, 0x06, 0x11fc);
                mdio_write(tp, 0x06, 0x04ee);
                mdio_write(tp, 0x06, 0x8aed);
                mdio_write(tp, 0x06, 0x00ee);
                mdio_write(tp, 0x06, 0x8aec);
                mdio_write(tp, 0x06, 0x0004);
                mdio_write(tp, 0x06, 0xf8e0);
                mdio_write(tp, 0x06, 0x8b87);
                mdio_write(tp, 0x06, 0xad24);
                mdio_write(tp, 0x06, 0x3ae0);
                mdio_write(tp, 0x06, 0xe0ea);
                mdio_write(tp, 0x06, 0xe1e0);
                mdio_write(tp, 0x06, 0xeb58);
                mdio_write(tp, 0x06, 0xf8d1);
                mdio_write(tp, 0x06, 0x01e4);
                mdio_write(tp, 0x06, 0xe0ea);
                mdio_write(tp, 0x06, 0xe5e0);
                mdio_write(tp, 0x06, 0xebe0);
                mdio_write(tp, 0x06, 0xe07c);
                mdio_write(tp, 0x06, 0xe1e0);
                mdio_write(tp, 0x06, 0x7d5c);
                mdio_write(tp, 0x06, 0x00ff);
                mdio_write(tp, 0x06, 0x3c00);
                mdio_write(tp, 0x06, 0x1eab);
                mdio_write(tp, 0x06, 0x1ce0);
                mdio_write(tp, 0x06, 0xe04c);
                mdio_write(tp, 0x06, 0xe1e0);
                mdio_write(tp, 0x06, 0x4d58);
                mdio_write(tp, 0x06, 0xc1e4);
                mdio_write(tp, 0x06, 0xe04c);
                mdio_write(tp, 0x06, 0xe5e0);
                mdio_write(tp, 0x06, 0x4de0);
                mdio_write(tp, 0x06, 0xe0ee);
                mdio_write(tp, 0x06, 0xe1e0);
                mdio_write(tp, 0x06, 0xef69);
                mdio_write(tp, 0x06, 0x3ce4);
                mdio_write(tp, 0x06, 0xe0ee);
                mdio_write(tp, 0x06, 0xe5e0);
                mdio_write(tp, 0x06, 0xeffc);
                mdio_write(tp, 0x06, 0x04f8);
                mdio_write(tp, 0x06, 0xe08b);
                mdio_write(tp, 0x06, 0x87ad);
                mdio_write(tp, 0x06, 0x2412);
                mdio_write(tp, 0x06, 0xe0e0);
                mdio_write(tp, 0x06, 0xeee1);
                mdio_write(tp, 0x06, 0xe0ef);
                mdio_write(tp, 0x06, 0x59c3);
                mdio_write(tp, 0x06, 0xe4e0);
                mdio_write(tp, 0x06, 0xeee5);
                mdio_write(tp, 0x06, 0xe0ef);
                mdio_write(tp, 0x06, 0xee8a);
                mdio_write(tp, 0x06, 0xed01);
                mdio_write(tp, 0x06, 0xfc04);
                mdio_write(tp, 0x06, 0xf8e0);
                mdio_write(tp, 0x06, 0x8b81);
                mdio_write(tp, 0x06, 0xac25);
                mdio_write(tp, 0x06, 0x0502);
                mdio_write(tp, 0x06, 0x8363);
                mdio_write(tp, 0x06, 0xae03);
                mdio_write(tp, 0x06, 0x0225);
                mdio_write(tp, 0x06, 0x16fc);
                mdio_write(tp, 0x06, 0x04f8);
                mdio_write(tp, 0x06, 0xf9fa);
                mdio_write(tp, 0x06, 0xef69);
                mdio_write(tp, 0x06, 0xfae0);
                mdio_write(tp, 0x06, 0x860a);
                mdio_write(tp, 0x06, 0xa000);
                mdio_write(tp, 0x06, 0x19e0);
                mdio_write(tp, 0x06, 0x860b);
                mdio_write(tp, 0x06, 0xe18b);
                mdio_write(tp, 0x06, 0x331b);
                mdio_write(tp, 0x06, 0x109e);
                mdio_write(tp, 0x06, 0x04aa);
                mdio_write(tp, 0x06, 0x02ae);
                mdio_write(tp, 0x06, 0x06ee);
                mdio_write(tp, 0x06, 0x860a);
                mdio_write(tp, 0x06, 0x01ae);
                mdio_write(tp, 0x06, 0xe602);
                mdio_write(tp, 0x06, 0x241e);
                mdio_write(tp, 0x06, 0xae14);
                mdio_write(tp, 0x06, 0xa001);
                mdio_write(tp, 0x06, 0x1402);
                mdio_write(tp, 0x06, 0x2426);
                mdio_write(tp, 0x06, 0xbf26);
                mdio_write(tp, 0x06, 0x6d02);
                mdio_write(tp, 0x06, 0x2eb0);
                mdio_write(tp, 0x06, 0xee86);
                mdio_write(tp, 0x06, 0x0b00);
                mdio_write(tp, 0x06, 0xee86);
                mdio_write(tp, 0x06, 0x0a02);
                mdio_write(tp, 0x06, 0xaf84);
                mdio_write(tp, 0x06, 0x3ca0);
                mdio_write(tp, 0x06, 0x0252);
                mdio_write(tp, 0x06, 0xee86);
                mdio_write(tp, 0x06, 0x0400);
                mdio_write(tp, 0x06, 0xee86);
                mdio_write(tp, 0x06, 0x0500);
                mdio_write(tp, 0x06, 0xe086);
                mdio_write(tp, 0x06, 0x0be1);
                mdio_write(tp, 0x06, 0x8b32);
                mdio_write(tp, 0x06, 0x1b10);
                mdio_write(tp, 0x06, 0x9e04);
                mdio_write(tp, 0x06, 0xaa02);
                mdio_write(tp, 0x06, 0xaecb);
                mdio_write(tp, 0x06, 0xee86);
                mdio_write(tp, 0x06, 0x0b00);
                mdio_write(tp, 0x06, 0x0224);
                mdio_write(tp, 0x06, 0x3ae2);
                mdio_write(tp, 0x06, 0x8604);
                mdio_write(tp, 0x06, 0xe386);
                mdio_write(tp, 0x06, 0x05ef);
                mdio_write(tp, 0x06, 0x65e2);
                mdio_write(tp, 0x06, 0x8606);
                mdio_write(tp, 0x06, 0xe386);
                mdio_write(tp, 0x06, 0x071b);
                mdio_write(tp, 0x06, 0x56aa);
                mdio_write(tp, 0x06, 0x0eef);
                mdio_write(tp, 0x06, 0x56e6);
                mdio_write(tp, 0x06, 0x8606);
                mdio_write(tp, 0x06, 0xe786);
                mdio_write(tp, 0x06, 0x07e2);
                mdio_write(tp, 0x06, 0x8609);
                mdio_write(tp, 0x06, 0xe686);
                mdio_write(tp, 0x06, 0x08e0);
                mdio_write(tp, 0x06, 0x8609);
                mdio_write(tp, 0x06, 0xa000);
                mdio_write(tp, 0x06, 0x07ee);
                mdio_write(tp, 0x06, 0x860a);
                mdio_write(tp, 0x06, 0x03af);
                mdio_write(tp, 0x06, 0x8369);
                mdio_write(tp, 0x06, 0x0224);
                mdio_write(tp, 0x06, 0x8e02);
                mdio_write(tp, 0x06, 0x2426);
                mdio_write(tp, 0x06, 0xae48);
                mdio_write(tp, 0x06, 0xa003);
                mdio_write(tp, 0x06, 0x21e0);
                mdio_write(tp, 0x06, 0x8608);
                mdio_write(tp, 0x06, 0xe186);
                mdio_write(tp, 0x06, 0x091b);
                mdio_write(tp, 0x06, 0x019e);
                mdio_write(tp, 0x06, 0x0caa);
                mdio_write(tp, 0x06, 0x0502);
                mdio_write(tp, 0x06, 0x249d);
                mdio_write(tp, 0x06, 0xaee7);
                mdio_write(tp, 0x06, 0x0224);
                mdio_write(tp, 0x06, 0x8eae);
                mdio_write(tp, 0x06, 0xe2ee);
                mdio_write(tp, 0x06, 0x860a);
                mdio_write(tp, 0x06, 0x04ee);
                mdio_write(tp, 0x06, 0x860b);
                mdio_write(tp, 0x06, 0x00af);
                mdio_write(tp, 0x06, 0x8369);
                mdio_write(tp, 0x06, 0xa004);
                mdio_write(tp, 0x06, 0x15e0);
                mdio_write(tp, 0x06, 0x860b);
                mdio_write(tp, 0x06, 0xe18b);
                mdio_write(tp, 0x06, 0x341b);
                mdio_write(tp, 0x06, 0x109e);
                mdio_write(tp, 0x06, 0x05aa);
                mdio_write(tp, 0x06, 0x03af);
                mdio_write(tp, 0x06, 0x8383);
                mdio_write(tp, 0x06, 0xee86);
                mdio_write(tp, 0x06, 0x0a05);
                mdio_write(tp, 0x06, 0xae0c);
                mdio_write(tp, 0x06, 0xa005);
                mdio_write(tp, 0x06, 0x02ae);
                mdio_write(tp, 0x06, 0x0702);
                mdio_write(tp, 0x06, 0x2309);
                mdio_write(tp, 0x06, 0xee86);
                mdio_write(tp, 0x06, 0x0a00);
                mdio_write(tp, 0x06, 0xfeef);
                mdio_write(tp, 0x06, 0x96fe);
                mdio_write(tp, 0x06, 0xfdfc);
                mdio_write(tp, 0x06, 0x04f8);
                mdio_write(tp, 0x06, 0xf9fa);
                mdio_write(tp, 0x06, 0xef69);
                mdio_write(tp, 0x06, 0xfbe0);
                mdio_write(tp, 0x06, 0x8b85);
                mdio_write(tp, 0x06, 0xad25);
                mdio_write(tp, 0x06, 0x22e0);
                mdio_write(tp, 0x06, 0xe022);
                mdio_write(tp, 0x06, 0xe1e0);
                mdio_write(tp, 0x06, 0x23e2);
                mdio_write(tp, 0x06, 0xe036);
                mdio_write(tp, 0x06, 0xe3e0);
                mdio_write(tp, 0x06, 0x375a);
                mdio_write(tp, 0x06, 0xc40d);
                mdio_write(tp, 0x06, 0x0158);
                mdio_write(tp, 0x06, 0x021e);
                mdio_write(tp, 0x06, 0x20e3);
                mdio_write(tp, 0x06, 0x8ae7);
                mdio_write(tp, 0x06, 0xac31);
                mdio_write(tp, 0x06, 0x60ac);
                mdio_write(tp, 0x06, 0x3a08);
                mdio_write(tp, 0x06, 0xac3e);
                mdio_write(tp, 0x06, 0x26ae);
                mdio_write(tp, 0x06, 0x67af);
                mdio_write(tp, 0x06, 0x84db);
                mdio_write(tp, 0x06, 0xad37);
                mdio_write(tp, 0x06, 0x61e0);
                mdio_write(tp, 0x06, 0x8ae8);
                mdio_write(tp, 0x06, 0x10e4);
                mdio_write(tp, 0x06, 0x8ae8);
                mdio_write(tp, 0x06, 0xe18a);
                mdio_write(tp, 0x06, 0xe91b);
                mdio_write(tp, 0x06, 0x109e);
                mdio_write(tp, 0x06, 0x02ae);
                mdio_write(tp, 0x06, 0x51d1);
                mdio_write(tp, 0x06, 0x00bf);
                mdio_write(tp, 0x06, 0x863b);
                mdio_write(tp, 0x06, 0x022f);
                mdio_write(tp, 0x06, 0x50ee);
                mdio_write(tp, 0x06, 0x8ae8);
                mdio_write(tp, 0x06, 0x00ae);
                mdio_write(tp, 0x06, 0x43ad);
                mdio_write(tp, 0x06, 0x3627);
                mdio_write(tp, 0x06, 0xe08a);
                mdio_write(tp, 0x06, 0xeee1);
                mdio_write(tp, 0x06, 0x8aef);
                mdio_write(tp, 0x06, 0xef74);
                mdio_write(tp, 0x06, 0xe08a);
                mdio_write(tp, 0x06, 0xeae1);
                mdio_write(tp, 0x06, 0x8aeb);
                mdio_write(tp, 0x06, 0x1b74);
                mdio_write(tp, 0x06, 0x9e2e);
                mdio_write(tp, 0x06, 0x14e4);
                mdio_write(tp, 0x06, 0x8aea);
                mdio_write(tp, 0x06, 0xe58a);
                mdio_write(tp, 0x06, 0xebef);
                mdio_write(tp, 0x06, 0x74e0);
                mdio_write(tp, 0x06, 0x8aee);
                mdio_write(tp, 0x06, 0xe18a);
                mdio_write(tp, 0x06, 0xef1b);
                mdio_write(tp, 0x06, 0x479e);
                mdio_write(tp, 0x06, 0x0fae);
                mdio_write(tp, 0x06, 0x19ee);
                mdio_write(tp, 0x06, 0x8aea);
                mdio_write(tp, 0x06, 0x00ee);
                mdio_write(tp, 0x06, 0x8aeb);
                mdio_write(tp, 0x06, 0x00ae);
                mdio_write(tp, 0x06, 0x0fac);
                mdio_write(tp, 0x06, 0x390c);
                mdio_write(tp, 0x06, 0xd101);
                mdio_write(tp, 0x06, 0xbf86);
                mdio_write(tp, 0x06, 0x3b02);
                mdio_write(tp, 0x06, 0x2f50);
                mdio_write(tp, 0x06, 0xee8a);
                mdio_write(tp, 0x06, 0xe800);
                mdio_write(tp, 0x06, 0xe68a);
                mdio_write(tp, 0x06, 0xe7ff);
                mdio_write(tp, 0x06, 0xef96);
                mdio_write(tp, 0x06, 0xfefd);
                mdio_write(tp, 0x06, 0xfc04);
                mdio_write(tp, 0x06, 0xf8f9);
                mdio_write(tp, 0x06, 0xfaef);
                mdio_write(tp, 0x06, 0x69e0);
                mdio_write(tp, 0x06, 0xe022);
                mdio_write(tp, 0x06, 0xe1e0);
                mdio_write(tp, 0x06, 0x2358);
                mdio_write(tp, 0x06, 0xc4e1);
                mdio_write(tp, 0x06, 0x8b6e);
                mdio_write(tp, 0x06, 0x1f10);
                mdio_write(tp, 0x06, 0x9e24);
                mdio_write(tp, 0x06, 0xe48b);
                mdio_write(tp, 0x06, 0x6ead);
                mdio_write(tp, 0x06, 0x2218);
                mdio_write(tp, 0x06, 0xac27);
                mdio_write(tp, 0x06, 0x0dac);
                mdio_write(tp, 0x06, 0x2605);
                mdio_write(tp, 0x06, 0x0203);
                mdio_write(tp, 0x06, 0x8fae);
                mdio_write(tp, 0x06, 0x1302);
                mdio_write(tp, 0x06, 0x03c8);
                mdio_write(tp, 0x06, 0xae0e);
                mdio_write(tp, 0x06, 0x0203);
                mdio_write(tp, 0x06, 0xe102);
                mdio_write(tp, 0x06, 0x8520);
                mdio_write(tp, 0x06, 0xae06);
                mdio_write(tp, 0x06, 0x0203);
                mdio_write(tp, 0x06, 0x8f02);
                mdio_write(tp, 0x06, 0x8566);
                mdio_write(tp, 0x06, 0xef96);
                mdio_write(tp, 0x06, 0xfefd);
                mdio_write(tp, 0x06, 0xfc04);
                mdio_write(tp, 0x06, 0xf8fa);
                mdio_write(tp, 0x06, 0xef69);
                mdio_write(tp, 0x06, 0xe08b);
                mdio_write(tp, 0x06, 0x82ad);
                mdio_write(tp, 0x06, 0x2737);
                mdio_write(tp, 0x06, 0xbf86);
                mdio_write(tp, 0x06, 0x4402);
                mdio_write(tp, 0x06, 0x2f23);
                mdio_write(tp, 0x06, 0xac28);
                mdio_write(tp, 0x06, 0x2ed1);
                mdio_write(tp, 0x06, 0x01bf);
                mdio_write(tp, 0x06, 0x8647);
                mdio_write(tp, 0x06, 0x022f);
                mdio_write(tp, 0x06, 0x50bf);
                mdio_write(tp, 0x06, 0x8641);
                mdio_write(tp, 0x06, 0x022f);
                mdio_write(tp, 0x06, 0x23e5);
                mdio_write(tp, 0x06, 0x8af0);
                mdio_write(tp, 0x06, 0xe0e0);
                mdio_write(tp, 0x06, 0x22e1);
                mdio_write(tp, 0x06, 0xe023);
                mdio_write(tp, 0x06, 0xac2e);
                mdio_write(tp, 0x06, 0x04d1);
                mdio_write(tp, 0x06, 0x01ae);
                mdio_write(tp, 0x06, 0x02d1);
                mdio_write(tp, 0x06, 0x00bf);
                mdio_write(tp, 0x06, 0x8641);
                mdio_write(tp, 0x06, 0x022f);
                mdio_write(tp, 0x06, 0x50d1);
                mdio_write(tp, 0x06, 0x01bf);
                mdio_write(tp, 0x06, 0x8644);
                mdio_write(tp, 0x06, 0x022f);
                mdio_write(tp, 0x06, 0x50ef);
                mdio_write(tp, 0x06, 0x96fe);
                mdio_write(tp, 0x06, 0xfc04);
                mdio_write(tp, 0x06, 0xf8fa);
                mdio_write(tp, 0x06, 0xef69);
                mdio_write(tp, 0x06, 0xbf86);
                mdio_write(tp, 0x06, 0x4702);
                mdio_write(tp, 0x06, 0x2f23);
                mdio_write(tp, 0x06, 0xad28);
                mdio_write(tp, 0x06, 0x19d1);
                mdio_write(tp, 0x06, 0x00bf);
                mdio_write(tp, 0x06, 0x8644);
                mdio_write(tp, 0x06, 0x022f);
                mdio_write(tp, 0x06, 0x50e1);
                mdio_write(tp, 0x06, 0x8af0);
                mdio_write(tp, 0x06, 0xbf86);
                mdio_write(tp, 0x06, 0x4102);
                mdio_write(tp, 0x06, 0x2f50);
                mdio_write(tp, 0x06, 0xd100);
                mdio_write(tp, 0x06, 0xbf86);
                mdio_write(tp, 0x06, 0x4702);
                mdio_write(tp, 0x06, 0x2f50);
                mdio_write(tp, 0x06, 0xef96);
                mdio_write(tp, 0x06, 0xfefc);
                mdio_write(tp, 0x06, 0x04f8);
                mdio_write(tp, 0x06, 0xe0e2);
                mdio_write(tp, 0x06, 0xfee1);
                mdio_write(tp, 0x06, 0xe2ff);
                mdio_write(tp, 0x06, 0xad2e);
                mdio_write(tp, 0x06, 0x63e0);
                mdio_write(tp, 0x06, 0xe038);
                mdio_write(tp, 0x06, 0xe1e0);
                mdio_write(tp, 0x06, 0x39ad);
                mdio_write(tp, 0x06, 0x2f10);
                mdio_write(tp, 0x06, 0xe0e0);
                mdio_write(tp, 0x06, 0x34e1);
                mdio_write(tp, 0x06, 0xe035);
                mdio_write(tp, 0x06, 0xf726);
                mdio_write(tp, 0x06, 0xe4e0);
                mdio_write(tp, 0x06, 0x34e5);
                mdio_write(tp, 0x06, 0xe035);
                mdio_write(tp, 0x06, 0xae0e);
                mdio_write(tp, 0x06, 0xe0e2);
                mdio_write(tp, 0x06, 0xd6e1);
                mdio_write(tp, 0x06, 0xe2d7);
                mdio_write(tp, 0x06, 0xf728);
                mdio_write(tp, 0x06, 0xe4e2);
                mdio_write(tp, 0x06, 0xd6e5);
                mdio_write(tp, 0x06, 0xe2d7);
                mdio_write(tp, 0x06, 0xe0e2);
                mdio_write(tp, 0x06, 0x34e1);
                mdio_write(tp, 0x06, 0xe235);
                mdio_write(tp, 0x06, 0xf72b);
                mdio_write(tp, 0x06, 0xe4e2);
                mdio_write(tp, 0x06, 0x34e5);
                mdio_write(tp, 0x06, 0xe235);
                mdio_write(tp, 0x06, 0xd07d);
                mdio_write(tp, 0x06, 0xb0fe);
                mdio_write(tp, 0x06, 0xe0e2);
                mdio_write(tp, 0x06, 0x34e1);
                mdio_write(tp, 0x06, 0xe235);
                mdio_write(tp, 0x06, 0xf62b);
                mdio_write(tp, 0x06, 0xe4e2);
                mdio_write(tp, 0x06, 0x34e5);
                mdio_write(tp, 0x06, 0xe235);
                mdio_write(tp, 0x06, 0xe0e0);
                mdio_write(tp, 0x06, 0x34e1);
                mdio_write(tp, 0x06, 0xe035);
                mdio_write(tp, 0x06, 0xf626);
                mdio_write(tp, 0x06, 0xe4e0);
                mdio_write(tp, 0x06, 0x34e5);
                mdio_write(tp, 0x06, 0xe035);
                mdio_write(tp, 0x06, 0xe0e2);
                mdio_write(tp, 0x06, 0xd6e1);
                mdio_write(tp, 0x06, 0xe2d7);
                mdio_write(tp, 0x06, 0xf628);
                mdio_write(tp, 0x06, 0xe4e2);
                mdio_write(tp, 0x06, 0xd6e5);
                mdio_write(tp, 0x06, 0xe2d7);
                mdio_write(tp, 0x06, 0xfc04);
                mdio_write(tp, 0x06, 0xae20);
                mdio_write(tp, 0x06, 0x0000);
                mdio_write(tp, 0x06, 0x0000);
                mdio_write(tp, 0x06, 0x0000);
                mdio_write(tp, 0x06, 0x0000);
                mdio_write(tp, 0x06, 0x0000);
                mdio_write(tp, 0x06, 0x0000);
                mdio_write(tp, 0x06, 0x0000);
                mdio_write(tp, 0x06, 0x0000);
                mdio_write(tp, 0x06, 0x0000);
                mdio_write(tp, 0x06, 0x0000);
                mdio_write(tp, 0x06, 0x0000);
                mdio_write(tp, 0x06, 0x0000);
                mdio_write(tp, 0x06, 0x0000);
                mdio_write(tp, 0x06, 0x0000);
                mdio_write(tp, 0x06, 0x0000);
                mdio_write(tp, 0x06, 0x0000);
                mdio_write(tp, 0x06, 0xa725);
                mdio_write(tp, 0x06, 0xe50a);
                mdio_write(tp, 0x06, 0x1de5);
                mdio_write(tp, 0x06, 0x0a2c);
                mdio_write(tp, 0x06, 0xe50a);
                mdio_write(tp, 0x06, 0x6de5);
                mdio_write(tp, 0x06, 0x0a1d);
                mdio_write(tp, 0x06, 0xe50a);
                mdio_write(tp, 0x06, 0x1ce5);
                mdio_write(tp, 0x06, 0x0a2d);
                mdio_write(tp, 0x06, 0xa755);
                mdio_write(tp, 0x06, 0x00e2);
                mdio_write(tp, 0x06, 0x3488);
                mdio_write(tp, 0x06, 0xe200);
                mdio_write(tp, 0x06, 0xcce2);
                mdio_write(tp, 0x06, 0x0055);
                mdio_write(tp, 0x06, 0xe020);
                mdio_write(tp, 0x06, 0x55e2);
                mdio_write(tp, 0x06, 0xd600);
                mdio_write(tp, 0x06, 0xe24a);
                gphy_val = mdio_read(tp, 0x01);
                gphy_val |= BIT_0;
                mdio_write(tp, 0x01, gphy_val);
                gphy_val = mdio_read(tp, 0x00);
                gphy_val |= BIT_0;
                mdio_write(tp, 0x00, gphy_val);
                mdio_write(tp, 0x1f, 0x0000);

                mdio_write(tp, 0x1f, 0x0005);
                for (i = 0; i < 200; i++) {
                        udelay(100);
                        gphy_val = mdio_read(tp, 0x00);
                        if (gphy_val & BIT_7)
                                break;
                }
                mdio_write(tp, 0x1f, 0x0007);
                mdio_write(tp, 0x1e, 0x0023);
                gphy_val = mdio_read(tp, 0x17);
                gphy_val &= ~(BIT_0);
                if (tp->RequiredSecLanDonglePatch)
                        gphy_val &= ~(BIT_2);
                mdio_write(tp, 0x17, gphy_val);
                mdio_write(tp, 0x1f, 0x0000);
        }
}

static void
rtl8168_set_phy_mcu_8168evl_1(struct net_device *dev)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        unsigned int gphy_val,i;

        mdio_write(tp, 0x1f, 0x0000);
        mdio_write(tp, 0x00, 0x1800);
        gphy_val = mdio_read(tp, 0x15);
        gphy_val &= ~(BIT_12);
        mdio_write(tp, 0x15, gphy_val);
        mdelay(20);
        mdio_write(tp, 0x1f, 0x0004);
        mdio_write(tp, 0x1f, 0x0007);
        mdio_write(tp, 0x1e, 0x0023);
        gphy_val = mdio_read(tp, 0x17);
        if ((gphy_val & BIT_11) == 0x0000) {
                gphy_val |= BIT_0;
                mdio_write(tp, 0x17, gphy_val);
                for (i = 0; i < 200; i++) {
                        udelay(100);
                        gphy_val = mdio_read(tp, 0x17);
                        if (gphy_val & BIT_11)
                                break;
                }
        }
        gphy_val = mdio_read(tp, 0x17);
        gphy_val |= BIT_0;
        mdio_write(tp, 0x17, gphy_val);
        mdio_write(tp, 0x1f, 0x0004);
        mdio_write(tp, 0x1f, 0x0007);
        mdio_write(tp, 0x1E, 0x002C);
        mdio_write(tp, 0x1B, 0x5000);
        mdio_write(tp, 0x1E, 0x002d);
        mdio_write(tp, 0x19, 0x0004);
        mdio_write(tp, 0x1f, 0x0002);
        mdio_write(tp, 0x1f, 0x0000);
        for (i = 0; i < 200; i++) {
                udelay(100);
                gphy_val = mdio_read(tp, 0x1E);
                if ((gphy_val & 0x03FF) == 0x0014)
                        break;
        }
        mdio_write(tp, 0x1f, 0x0005);
        for (i = 0; i < 200; i++) {
                udelay(100);
                gphy_val = mdio_read(tp, 0x07);
                if ((gphy_val & BIT_5) == 0)
                        break;
        }
        gphy_val = mdio_read(tp, 0x07);
        if (gphy_val & BIT_5) {
                mdio_write(tp, 0x1f, 0x0004);
                mdio_write(tp, 0x1f, 0x0007);
                mdio_write(tp, 0x1e, 0x00a1);
                mdio_write(tp, 0x17, 0x1000);
                mdio_write(tp, 0x17, 0x0000);
                mdio_write(tp, 0x17, 0x2000);
                mdio_write(tp, 0x1e, 0x002f);
                mdio_write(tp, 0x18, 0x9bfb);
                mdio_write(tp, 0x1f, 0x0005);
                mdio_write(tp, 0x07, 0x0000);
                mdio_write(tp, 0x1f, 0x0002);
                mdio_write(tp, 0x1f, 0x0000);
        }
        mdio_write(tp, 0x1f, 0x0005);
        mdio_write(tp, 0x05, 0xfff6);
        mdio_write(tp, 0x06, 0x0080);
        gphy_val = mdio_read(tp, 0x00);
        gphy_val &= ~(BIT_7);
        mdio_write(tp, 0x00, gphy_val);
        mdio_write(tp, 0x1f, 0x0004);
        mdio_write(tp, 0x1f, 0x0007);
        mdio_write(tp, 0x1e, 0x0023);
        mdio_write(tp, 0x16, 0x0306);
        mdio_write(tp, 0x16, 0x0307);
        mdio_write(tp, 0x15, 0x0000);
        mdio_write(tp, 0x19, 0x407d);
        mdio_write(tp, 0x15, 0x0001);
        mdio_write(tp, 0x19, 0x440f);
        mdio_write(tp, 0x15, 0x0002);
        mdio_write(tp, 0x19, 0x7c03);
        mdio_write(tp, 0x15, 0x0003);
        mdio_write(tp, 0x19, 0x6c03);
        mdio_write(tp, 0x15, 0x0004);
        mdio_write(tp, 0x19, 0xc4d5);
        mdio_write(tp, 0x15, 0x0005);
        mdio_write(tp, 0x19, 0x00ff);
        mdio_write(tp, 0x15, 0x0006);
        mdio_write(tp, 0x19, 0x74f0);
        mdio_write(tp, 0x15, 0x0007);
        mdio_write(tp, 0x19, 0x4880);
        mdio_write(tp, 0x15, 0x0008);
        mdio_write(tp, 0x19, 0x4c00);
        mdio_write(tp, 0x15, 0x0009);
        mdio_write(tp, 0x19, 0x4800);
        mdio_write(tp, 0x15, 0x000a);
        mdio_write(tp, 0x19, 0x5000);
        mdio_write(tp, 0x15, 0x000b);
        mdio_write(tp, 0x19, 0x4400);
        mdio_write(tp, 0x15, 0x000c);
        mdio_write(tp, 0x19, 0x7801);
        mdio_write(tp, 0x15, 0x000d);
        mdio_write(tp, 0x19, 0x4000);
        mdio_write(tp, 0x15, 0x000e);
        mdio_write(tp, 0x19, 0x7800);
        mdio_write(tp, 0x15, 0x000f);
        mdio_write(tp, 0x19, 0x7010);
        mdio_write(tp, 0x15, 0x0010);
        mdio_write(tp, 0x19, 0x6804);
        mdio_write(tp, 0x15, 0x0011);
        mdio_write(tp, 0x19, 0x64a0);
        mdio_write(tp, 0x15, 0x0012);
        mdio_write(tp, 0x19, 0x63da);
        mdio_write(tp, 0x15, 0x0013);
        mdio_write(tp, 0x19, 0x63d8);
        mdio_write(tp, 0x15, 0x0014);
        mdio_write(tp, 0x19, 0x6f05);
        mdio_write(tp, 0x15, 0x0015);
        mdio_write(tp, 0x19, 0x5420);
        mdio_write(tp, 0x15, 0x0016);
        mdio_write(tp, 0x19, 0x58ce);
        mdio_write(tp, 0x15, 0x0017);
        mdio_write(tp, 0x19, 0x5cf3);
        mdio_write(tp, 0x15, 0x0018);
        mdio_write(tp, 0x19, 0xb600);
        mdio_write(tp, 0x15, 0x0019);
        mdio_write(tp, 0x19, 0xc659);
        mdio_write(tp, 0x15, 0x001a);
        mdio_write(tp, 0x19, 0x0018);
        mdio_write(tp, 0x15, 0x001b);
        mdio_write(tp, 0x19, 0xc403);
        mdio_write(tp, 0x15, 0x001c);
        mdio_write(tp, 0x19, 0x0016);
        mdio_write(tp, 0x15, 0x001d);
        mdio_write(tp, 0x19, 0xaa05);
        mdio_write(tp, 0x15, 0x001e);
        mdio_write(tp, 0x19, 0xc503);
        mdio_write(tp, 0x15, 0x001f);
        mdio_write(tp, 0x19, 0x0003);
        mdio_write(tp, 0x15, 0x0020);
        mdio_write(tp, 0x19, 0x89f8);
        mdio_write(tp, 0x15, 0x0021);
        mdio_write(tp, 0x19, 0x32ae);
        mdio_write(tp, 0x15, 0x0022);
        mdio_write(tp, 0x19, 0x7c03);
        mdio_write(tp, 0x15, 0x0023);
        mdio_write(tp, 0x19, 0x6c03);
        mdio_write(tp, 0x15, 0x0024);
        mdio_write(tp, 0x19, 0x7c03);
        mdio_write(tp, 0x15, 0x0025);
        mdio_write(tp, 0x19, 0x6801);
        mdio_write(tp, 0x15, 0x0026);
        mdio_write(tp, 0x19, 0x66a0);
        mdio_write(tp, 0x15, 0x0027);
        mdio_write(tp, 0x19, 0xa300);
        mdio_write(tp, 0x15, 0x0028);
        mdio_write(tp, 0x19, 0x64a0);
        mdio_write(tp, 0x15, 0x0029);
        mdio_write(tp, 0x19, 0x76f0);
        mdio_write(tp, 0x15, 0x002a);
        mdio_write(tp, 0x19, 0x7670);
        mdio_write(tp, 0x15, 0x002b);
        mdio_write(tp, 0x19, 0x7630);
        mdio_write(tp, 0x15, 0x002c);
        mdio_write(tp, 0x19, 0x31a6);
        mdio_write(tp, 0x15, 0x002d);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x002e);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x002f);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x0030);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x0031);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x0032);
        mdio_write(tp, 0x19, 0x4801);
        mdio_write(tp, 0x15, 0x0033);
        mdio_write(tp, 0x19, 0x6803);
        mdio_write(tp, 0x15, 0x0034);
        mdio_write(tp, 0x19, 0x66a1);
        mdio_write(tp, 0x15, 0x0035);
        mdio_write(tp, 0x19, 0x7c03);
        mdio_write(tp, 0x15, 0x0036);
        mdio_write(tp, 0x19, 0x6c03);
        mdio_write(tp, 0x15, 0x0037);
        mdio_write(tp, 0x19, 0xa300);
        mdio_write(tp, 0x15, 0x0038);
        mdio_write(tp, 0x19, 0x64a1);
        mdio_write(tp, 0x15, 0x0039);
        mdio_write(tp, 0x19, 0x7c08);
        mdio_write(tp, 0x15, 0x003a);
        mdio_write(tp, 0x19, 0x74f8);
        mdio_write(tp, 0x15, 0x003b);
        mdio_write(tp, 0x19, 0x63d0);
        mdio_write(tp, 0x15, 0x003c);
        mdio_write(tp, 0x19, 0x7ff0);
        mdio_write(tp, 0x15, 0x003d);
        mdio_write(tp, 0x19, 0x77f0);
        mdio_write(tp, 0x15, 0x003e);
        mdio_write(tp, 0x19, 0x7ff0);
        mdio_write(tp, 0x15, 0x003f);
        mdio_write(tp, 0x19, 0x7750);
        mdio_write(tp, 0x15, 0x0040);
        mdio_write(tp, 0x19, 0x63d8);
        mdio_write(tp, 0x15, 0x0041);
        mdio_write(tp, 0x19, 0x7cf0);
        mdio_write(tp, 0x15, 0x0042);
        mdio_write(tp, 0x19, 0x7708);
        mdio_write(tp, 0x15, 0x0043);
        mdio_write(tp, 0x19, 0xa654);
        mdio_write(tp, 0x15, 0x0044);
        mdio_write(tp, 0x19, 0x304a);
        mdio_write(tp, 0x15, 0x0045);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x0046);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x0047);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x0048);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x0049);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x004a);
        mdio_write(tp, 0x19, 0x4802);
        mdio_write(tp, 0x15, 0x004b);
        mdio_write(tp, 0x19, 0x4003);
        mdio_write(tp, 0x15, 0x004c);
        mdio_write(tp, 0x19, 0x4440);
        mdio_write(tp, 0x15, 0x004d);
        mdio_write(tp, 0x19, 0x63c8);
        mdio_write(tp, 0x15, 0x004e);
        mdio_write(tp, 0x19, 0x6481);
        mdio_write(tp, 0x15, 0x004f);
        mdio_write(tp, 0x19, 0x9d00);
        mdio_write(tp, 0x15, 0x0050);
        mdio_write(tp, 0x19, 0x63e8);
        mdio_write(tp, 0x15, 0x0051);
        mdio_write(tp, 0x19, 0x7d00);
        mdio_write(tp, 0x15, 0x0052);
        mdio_write(tp, 0x19, 0x5900);
        mdio_write(tp, 0x15, 0x0053);
        mdio_write(tp, 0x19, 0x63f8);
        mdio_write(tp, 0x15, 0x0054);
        mdio_write(tp, 0x19, 0x64a1);
        mdio_write(tp, 0x15, 0x0055);
        mdio_write(tp, 0x19, 0x3116);
        mdio_write(tp, 0x15, 0x0056);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x0057);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x0058);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x0059);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x005a);
        mdio_write(tp, 0x19, 0x7c03);
        mdio_write(tp, 0x15, 0x005b);
        mdio_write(tp, 0x19, 0x6c03);
        mdio_write(tp, 0x15, 0x005c);
        mdio_write(tp, 0x19, 0x7c08);
        mdio_write(tp, 0x15, 0x005d);
        mdio_write(tp, 0x19, 0x6000);
        mdio_write(tp, 0x15, 0x005e);
        mdio_write(tp, 0x19, 0x59ce);
        mdio_write(tp, 0x15, 0x005f);
        mdio_write(tp, 0x19, 0x4400);
        mdio_write(tp, 0x15, 0x0060);
        mdio_write(tp, 0x19, 0x7d00);
        mdio_write(tp, 0x15, 0x0061);
        mdio_write(tp, 0x19, 0x72b0);
        mdio_write(tp, 0x15, 0x0062);
        mdio_write(tp, 0x19, 0x400e);
        mdio_write(tp, 0x15, 0x0063);
        mdio_write(tp, 0x19, 0x4440);
        mdio_write(tp, 0x15, 0x0064);
        mdio_write(tp, 0x19, 0x9d00);
        mdio_write(tp, 0x15, 0x0065);
        mdio_write(tp, 0x19, 0x7f00);
        mdio_write(tp, 0x15, 0x0066);
        mdio_write(tp, 0x19, 0x70b0);
        mdio_write(tp, 0x15, 0x0067);
        mdio_write(tp, 0x19, 0x7c08);
        mdio_write(tp, 0x15, 0x0068);
        mdio_write(tp, 0x19, 0x6008);
        mdio_write(tp, 0x15, 0x0069);
        mdio_write(tp, 0x19, 0x7cf0);
        mdio_write(tp, 0x15, 0x006a);
        mdio_write(tp, 0x19, 0x7750);
        mdio_write(tp, 0x15, 0x006b);
        mdio_write(tp, 0x19, 0x4007);
        mdio_write(tp, 0x15, 0x006c);
        mdio_write(tp, 0x19, 0x4500);
        mdio_write(tp, 0x15, 0x006d);
        mdio_write(tp, 0x19, 0x4023);
        mdio_write(tp, 0x15, 0x006e);
        mdio_write(tp, 0x19, 0x4580);
        mdio_write(tp, 0x15, 0x006f);
        mdio_write(tp, 0x19, 0x9f00);
        mdio_write(tp, 0x15, 0x0070);
        mdio_write(tp, 0x19, 0xcd78);
        mdio_write(tp, 0x15, 0x0071);
        mdio_write(tp, 0x19, 0x0003);
        mdio_write(tp, 0x15, 0x0072);
        mdio_write(tp, 0x19, 0xbe02);
        mdio_write(tp, 0x15, 0x0073);
        mdio_write(tp, 0x19, 0x3070);
        mdio_write(tp, 0x15, 0x0074);
        mdio_write(tp, 0x19, 0x7cf0);
        mdio_write(tp, 0x15, 0x0075);
        mdio_write(tp, 0x19, 0x77f0);
        mdio_write(tp, 0x15, 0x0076);
        mdio_write(tp, 0x19, 0x4400);
        mdio_write(tp, 0x15, 0x0077);
        mdio_write(tp, 0x19, 0x4007);
        mdio_write(tp, 0x15, 0x0078);
        mdio_write(tp, 0x19, 0x4500);
        mdio_write(tp, 0x15, 0x0079);
        mdio_write(tp, 0x19, 0x4023);
        mdio_write(tp, 0x15, 0x007a);
        mdio_write(tp, 0x19, 0x4580);
        mdio_write(tp, 0x15, 0x007b);
        mdio_write(tp, 0x19, 0x9f00);
        mdio_write(tp, 0x15, 0x007c);
        mdio_write(tp, 0x19, 0xce80);
        mdio_write(tp, 0x15, 0x007d);
        mdio_write(tp, 0x19, 0x0004);
        mdio_write(tp, 0x15, 0x007e);
        mdio_write(tp, 0x19, 0xce80);
        mdio_write(tp, 0x15, 0x007f);
        mdio_write(tp, 0x19, 0x0002);
        mdio_write(tp, 0x15, 0x0080);
        mdio_write(tp, 0x19, 0x307c);
        mdio_write(tp, 0x15, 0x0081);
        mdio_write(tp, 0x19, 0x4400);
        mdio_write(tp, 0x15, 0x0082);
        mdio_write(tp, 0x19, 0x480f);
        mdio_write(tp, 0x15, 0x0083);
        mdio_write(tp, 0x19, 0x6802);
        mdio_write(tp, 0x15, 0x0084);
        mdio_write(tp, 0x19, 0x6680);
        mdio_write(tp, 0x15, 0x0085);
        mdio_write(tp, 0x19, 0x7c10);
        mdio_write(tp, 0x15, 0x0086);
        mdio_write(tp, 0x19, 0x6010);
        mdio_write(tp, 0x15, 0x0087);
        mdio_write(tp, 0x19, 0x400a);
        mdio_write(tp, 0x15, 0x0088);
        mdio_write(tp, 0x19, 0x4580);
        mdio_write(tp, 0x15, 0x0089);
        mdio_write(tp, 0x19, 0x9e00);
        mdio_write(tp, 0x15, 0x008a);
        mdio_write(tp, 0x19, 0x7d00);
        mdio_write(tp, 0x15, 0x008b);
        mdio_write(tp, 0x19, 0x5800);
        mdio_write(tp, 0x15, 0x008c);
        mdio_write(tp, 0x19, 0x63c8);
        mdio_write(tp, 0x15, 0x008d);
        mdio_write(tp, 0x19, 0x63d8);
        mdio_write(tp, 0x15, 0x008e);
        mdio_write(tp, 0x19, 0x66a0);
        mdio_write(tp, 0x15, 0x008f);
        mdio_write(tp, 0x19, 0x8300);
        mdio_write(tp, 0x15, 0x0090);
        mdio_write(tp, 0x19, 0x7ff0);
        mdio_write(tp, 0x15, 0x0091);
        mdio_write(tp, 0x19, 0x74f0);
        mdio_write(tp, 0x15, 0x0092);
        mdio_write(tp, 0x19, 0x3006);
        mdio_write(tp, 0x15, 0x0093);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x0094);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x0095);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x0096);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x0097);
        mdio_write(tp, 0x19, 0x4803);
        mdio_write(tp, 0x15, 0x0098);
        mdio_write(tp, 0x19, 0x7c03);
        mdio_write(tp, 0x15, 0x0099);
        mdio_write(tp, 0x19, 0x6c03);
        mdio_write(tp, 0x15, 0x009a);
        mdio_write(tp, 0x19, 0xa203);
        mdio_write(tp, 0x15, 0x009b);
        mdio_write(tp, 0x19, 0x64b1);
        mdio_write(tp, 0x15, 0x009c);
        mdio_write(tp, 0x19, 0x309e);
        mdio_write(tp, 0x15, 0x009d);
        mdio_write(tp, 0x19, 0x64b3);
        mdio_write(tp, 0x15, 0x009e);
        mdio_write(tp, 0x19, 0x4030);
        mdio_write(tp, 0x15, 0x009f);
        mdio_write(tp, 0x19, 0x440e);
        mdio_write(tp, 0x15, 0x00a0);
        mdio_write(tp, 0x19, 0x4020);
        mdio_write(tp, 0x15, 0x00a1);
        mdio_write(tp, 0x19, 0x4419);
        mdio_write(tp, 0x15, 0x00a2);
        mdio_write(tp, 0x19, 0x7801);
        mdio_write(tp, 0x15, 0x00a3);
        mdio_write(tp, 0x19, 0xc520);
        mdio_write(tp, 0x15, 0x00a4);
        mdio_write(tp, 0x19, 0x000b);
        mdio_write(tp, 0x15, 0x00a5);
        mdio_write(tp, 0x19, 0x4020);
        mdio_write(tp, 0x15, 0x00a6);
        mdio_write(tp, 0x19, 0x7800);
        mdio_write(tp, 0x15, 0x00a7);
        mdio_write(tp, 0x19, 0x58a4);
        mdio_write(tp, 0x15, 0x00a8);
        mdio_write(tp, 0x19, 0x63da);
        mdio_write(tp, 0x15, 0x00a9);
        mdio_write(tp, 0x19, 0x5cb0);
        mdio_write(tp, 0x15, 0x00aa);
        mdio_write(tp, 0x19, 0x7d00);
        mdio_write(tp, 0x15, 0x00ab);
        mdio_write(tp, 0x19, 0x72b0);
        mdio_write(tp, 0x15, 0x00ac);
        mdio_write(tp, 0x19, 0x7f00);
        mdio_write(tp, 0x15, 0x00ad);
        mdio_write(tp, 0x19, 0x70b0);
        mdio_write(tp, 0x15, 0x00ae);
        mdio_write(tp, 0x19, 0x30b8);
        mdio_write(tp, 0x15, 0x00AF);
        mdio_write(tp, 0x19, 0x4060);
        mdio_write(tp, 0x15, 0x00B0);
        mdio_write(tp, 0x19, 0x7800);
        mdio_write(tp, 0x15, 0x00B1);
        mdio_write(tp, 0x19, 0x7e00);
        mdio_write(tp, 0x15, 0x00B2);
        mdio_write(tp, 0x19, 0x72B0);
        mdio_write(tp, 0x15, 0x00B3);
        mdio_write(tp, 0x19, 0x7F00);
        mdio_write(tp, 0x15, 0x00B4);
        mdio_write(tp, 0x19, 0x73B0);
        mdio_write(tp, 0x15, 0x00b5);
        mdio_write(tp, 0x19, 0x58a0);
        mdio_write(tp, 0x15, 0x00b6);
        mdio_write(tp, 0x19, 0x63d2);
        mdio_write(tp, 0x15, 0x00b7);
        mdio_write(tp, 0x19, 0x5c00);
        mdio_write(tp, 0x15, 0x00b8);
        mdio_write(tp, 0x19, 0x5780);
        mdio_write(tp, 0x15, 0x00b9);
        mdio_write(tp, 0x19, 0xb60d);
        mdio_write(tp, 0x15, 0x00ba);
        mdio_write(tp, 0x19, 0x9bff);
        mdio_write(tp, 0x15, 0x00bb);
        mdio_write(tp, 0x19, 0x7c03);
        mdio_write(tp, 0x15, 0x00bc);
        mdio_write(tp, 0x19, 0x6001);
        mdio_write(tp, 0x15, 0x00bd);
        mdio_write(tp, 0x19, 0xc020);
        mdio_write(tp, 0x15, 0x00be);
        mdio_write(tp, 0x19, 0x002b);
        mdio_write(tp, 0x15, 0x00bf);
        mdio_write(tp, 0x19, 0xc137);
        mdio_write(tp, 0x15, 0x00c0);
        mdio_write(tp, 0x19, 0x0006);
        mdio_write(tp, 0x15, 0x00c1);
        mdio_write(tp, 0x19, 0x9af8);
        mdio_write(tp, 0x15, 0x00c2);
        mdio_write(tp, 0x19, 0x30c6);
        mdio_write(tp, 0x15, 0x00c3);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x00c4);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x00c5);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x00c6);
        mdio_write(tp, 0x19, 0x7d00);
        mdio_write(tp, 0x15, 0x00c7);
        mdio_write(tp, 0x19, 0x70b0);
        mdio_write(tp, 0x15, 0x00c8);
        mdio_write(tp, 0x19, 0x4400);
        mdio_write(tp, 0x15, 0x00c9);
        mdio_write(tp, 0x19, 0x4804);
        mdio_write(tp, 0x15, 0x00ca);
        mdio_write(tp, 0x19, 0x7c80);
        mdio_write(tp, 0x15, 0x00cb);
        mdio_write(tp, 0x19, 0x5c80);
        mdio_write(tp, 0x15, 0x00cc);
        mdio_write(tp, 0x19, 0x4010);
        mdio_write(tp, 0x15, 0x00cd);
        mdio_write(tp, 0x19, 0x4415);
        mdio_write(tp, 0x15, 0x00ce);
        mdio_write(tp, 0x19, 0x9b00);
        mdio_write(tp, 0x15, 0x00cf);
        mdio_write(tp, 0x19, 0x7f00);
        mdio_write(tp, 0x15, 0x00d0);
        mdio_write(tp, 0x19, 0x70b0);
        mdio_write(tp, 0x15, 0x00d1);
        mdio_write(tp, 0x19, 0x3177);
        mdio_write(tp, 0x15, 0x00d2);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x00d3);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x00d4);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x00d5);
        mdio_write(tp, 0x19, 0x4808);
        mdio_write(tp, 0x15, 0x00d6);
        mdio_write(tp, 0x19, 0x4007);
        mdio_write(tp, 0x15, 0x00d7);
        mdio_write(tp, 0x19, 0x4420);
        mdio_write(tp, 0x15, 0x00d8);
        mdio_write(tp, 0x19, 0x63d8);
        mdio_write(tp, 0x15, 0x00d9);
        mdio_write(tp, 0x19, 0xb608);
        mdio_write(tp, 0x15, 0x00da);
        mdio_write(tp, 0x19, 0xbcbd);
        mdio_write(tp, 0x15, 0x00db);
        mdio_write(tp, 0x19, 0xc60b);
        mdio_write(tp, 0x15, 0x00dc);
        mdio_write(tp, 0x19, 0x00fd);
        mdio_write(tp, 0x15, 0x00dd);
        mdio_write(tp, 0x19, 0x30e1);
        mdio_write(tp, 0x15, 0x00de);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x00df);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x00e0);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x00e1);
        mdio_write(tp, 0x19, 0x4809);
        mdio_write(tp, 0x15, 0x00e2);
        mdio_write(tp, 0x19, 0x7e40);
        mdio_write(tp, 0x15, 0x00e3);
        mdio_write(tp, 0x19, 0x5a40);
        mdio_write(tp, 0x15, 0x00e4);
        mdio_write(tp, 0x19, 0x305a);
        mdio_write(tp, 0x15, 0x00e5);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x00e6);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x00e7);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x00e8);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x00e9);
        mdio_write(tp, 0x19, 0x480a);
        mdio_write(tp, 0x15, 0x00ea);
        mdio_write(tp, 0x19, 0x5820);
        mdio_write(tp, 0x15, 0x00eb);
        mdio_write(tp, 0x19, 0x6c03);
        mdio_write(tp, 0x15, 0x00ec);
        mdio_write(tp, 0x19, 0xb60a);
        mdio_write(tp, 0x15, 0x00ed);
        mdio_write(tp, 0x19, 0xda07);
        mdio_write(tp, 0x15, 0x00ee);
        mdio_write(tp, 0x19, 0x0008);
        mdio_write(tp, 0x15, 0x00ef);
        mdio_write(tp, 0x19, 0xc60b);
        mdio_write(tp, 0x15, 0x00f0);
        mdio_write(tp, 0x19, 0x00fc);
        mdio_write(tp, 0x15, 0x00f1);
        mdio_write(tp, 0x19, 0x30f6);
        mdio_write(tp, 0x15, 0x00f2);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x00f3);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x00f4);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x00f5);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x00f6);
        mdio_write(tp, 0x19, 0x4408);
        mdio_write(tp, 0x15, 0x00f7);
        mdio_write(tp, 0x19, 0x480b);
        mdio_write(tp, 0x15, 0x00f8);
        mdio_write(tp, 0x19, 0x6f03);
        mdio_write(tp, 0x15, 0x00f9);
        mdio_write(tp, 0x19, 0x405f);
        mdio_write(tp, 0x15, 0x00fa);
        mdio_write(tp, 0x19, 0x4448);
        mdio_write(tp, 0x15, 0x00fb);
        mdio_write(tp, 0x19, 0x4020);
        mdio_write(tp, 0x15, 0x00fc);
        mdio_write(tp, 0x19, 0x4468);
        mdio_write(tp, 0x15, 0x00fd);
        mdio_write(tp, 0x19, 0x9c03);
        mdio_write(tp, 0x15, 0x00fe);
        mdio_write(tp, 0x19, 0x6f07);
        mdio_write(tp, 0x15, 0x00ff);
        mdio_write(tp, 0x19, 0x58a0);
        mdio_write(tp, 0x15, 0x0100);
        mdio_write(tp, 0x19, 0xd6d1);
        mdio_write(tp, 0x15, 0x0101);
        mdio_write(tp, 0x19, 0x0004);
        mdio_write(tp, 0x15, 0x0102);
        mdio_write(tp, 0x19, 0xc137);
        mdio_write(tp, 0x15, 0x0103);
        mdio_write(tp, 0x19, 0x0002);
        mdio_write(tp, 0x15, 0x0104);
        mdio_write(tp, 0x19, 0xa0e5);
        mdio_write(tp, 0x15, 0x0105);
        mdio_write(tp, 0x19, 0x9df8);
        mdio_write(tp, 0x15, 0x0106);
        mdio_write(tp, 0x19, 0x30c6);
        mdio_write(tp, 0x15, 0x0107);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x0108);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x0109);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x010a);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x010b);
        mdio_write(tp, 0x19, 0x4808);
        mdio_write(tp, 0x15, 0x010c);
        mdio_write(tp, 0x19, 0xc32d);
        mdio_write(tp, 0x15, 0x010d);
        mdio_write(tp, 0x19, 0x0003);
        mdio_write(tp, 0x15, 0x010e);
        mdio_write(tp, 0x19, 0xc8b3);
        mdio_write(tp, 0x15, 0x010f);
        mdio_write(tp, 0x19, 0x00fc);
        mdio_write(tp, 0x15, 0x0110);
        mdio_write(tp, 0x19, 0x4400);
        mdio_write(tp, 0x15, 0x0111);
        mdio_write(tp, 0x19, 0x3116);
        mdio_write(tp, 0x15, 0x0112);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x0113);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x0114);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x0115);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x0116);
        mdio_write(tp, 0x19, 0x4803);
        mdio_write(tp, 0x15, 0x0117);
        mdio_write(tp, 0x19, 0x7c03);
        mdio_write(tp, 0x15, 0x0118);
        mdio_write(tp, 0x19, 0x6c02);
        mdio_write(tp, 0x15, 0x0119);
        mdio_write(tp, 0x19, 0x7c04);
        mdio_write(tp, 0x15, 0x011a);
        mdio_write(tp, 0x19, 0x6000);
        mdio_write(tp, 0x15, 0x011b);
        mdio_write(tp, 0x19, 0x5cf7);
        mdio_write(tp, 0x15, 0x011c);
        mdio_write(tp, 0x19, 0x7c2a);
        mdio_write(tp, 0x15, 0x011d);
        mdio_write(tp, 0x19, 0x5800);
        mdio_write(tp, 0x15, 0x011e);
        mdio_write(tp, 0x19, 0x5400);
        mdio_write(tp, 0x15, 0x011f);
        mdio_write(tp, 0x19, 0x7c08);
        mdio_write(tp, 0x15, 0x0120);
        mdio_write(tp, 0x19, 0x74f0);
        mdio_write(tp, 0x15, 0x0121);
        mdio_write(tp, 0x19, 0x4019);
        mdio_write(tp, 0x15, 0x0122);
        mdio_write(tp, 0x19, 0x440d);
        mdio_write(tp, 0x15, 0x0123);
        mdio_write(tp, 0x19, 0xb6c1);
        mdio_write(tp, 0x15, 0x0124);
        mdio_write(tp, 0x19, 0xc05b);
        mdio_write(tp, 0x15, 0x0125);
        mdio_write(tp, 0x19, 0x00bf);
        mdio_write(tp, 0x15, 0x0126);
        mdio_write(tp, 0x19, 0xc025);
        mdio_write(tp, 0x15, 0x0127);
        mdio_write(tp, 0x19, 0x00bd);
        mdio_write(tp, 0x15, 0x0128);
        mdio_write(tp, 0x19, 0xc603);
        mdio_write(tp, 0x15, 0x0129);
        mdio_write(tp, 0x19, 0x00bb);
        mdio_write(tp, 0x15, 0x012a);
        mdio_write(tp, 0x19, 0x8805);
        mdio_write(tp, 0x15, 0x012b);
        mdio_write(tp, 0x19, 0x7801);
        mdio_write(tp, 0x15, 0x012c);
        mdio_write(tp, 0x19, 0x4001);
        mdio_write(tp, 0x15, 0x012d);
        mdio_write(tp, 0x19, 0x7800);
        mdio_write(tp, 0x15, 0x012e);
        mdio_write(tp, 0x19, 0xa3dd);
        mdio_write(tp, 0x15, 0x012f);
        mdio_write(tp, 0x19, 0x7c03);
        mdio_write(tp, 0x15, 0x0130);
        mdio_write(tp, 0x19, 0x6c03);
        mdio_write(tp, 0x15, 0x0131);
        mdio_write(tp, 0x19, 0x8407);
        mdio_write(tp, 0x15, 0x0132);
        mdio_write(tp, 0x19, 0x7c03);
        mdio_write(tp, 0x15, 0x0133);
        mdio_write(tp, 0x19, 0x6c02);
        mdio_write(tp, 0x15, 0x0134);
        mdio_write(tp, 0x19, 0xd9b8);
        mdio_write(tp, 0x15, 0x0135);
        mdio_write(tp, 0x19, 0x0003);
        mdio_write(tp, 0x15, 0x0136);
        mdio_write(tp, 0x19, 0xc240);
        mdio_write(tp, 0x15, 0x0137);
        mdio_write(tp, 0x19, 0x0015);
        mdio_write(tp, 0x15, 0x0138);
        mdio_write(tp, 0x19, 0x7c03);
        mdio_write(tp, 0x15, 0x0139);
        mdio_write(tp, 0x19, 0x6c02);
        mdio_write(tp, 0x15, 0x013a);
        mdio_write(tp, 0x19, 0x9ae9);
        mdio_write(tp, 0x15, 0x013b);
        mdio_write(tp, 0x19, 0x3140);
        mdio_write(tp, 0x15, 0x013c);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x013d);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x013e);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x013f);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x0140);
        mdio_write(tp, 0x19, 0x4807);
        mdio_write(tp, 0x15, 0x0141);
        mdio_write(tp, 0x19, 0x4004);
        mdio_write(tp, 0x15, 0x0142);
        mdio_write(tp, 0x19, 0x4410);
        mdio_write(tp, 0x15, 0x0143);
        mdio_write(tp, 0x19, 0x7c0c);
        mdio_write(tp, 0x15, 0x0144);
        mdio_write(tp, 0x19, 0x600c);
        mdio_write(tp, 0x15, 0x0145);
        mdio_write(tp, 0x19, 0x9b00);
        mdio_write(tp, 0x15, 0x0146);
        mdio_write(tp, 0x19, 0xa68f);
        mdio_write(tp, 0x15, 0x0147);
        mdio_write(tp, 0x19, 0x3116);
        mdio_write(tp, 0x15, 0x0148);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x0149);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x014a);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x014b);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x014c);
        mdio_write(tp, 0x19, 0x4804);
        mdio_write(tp, 0x15, 0x014d);
        mdio_write(tp, 0x19, 0x54c0);
        mdio_write(tp, 0x15, 0x014e);
        mdio_write(tp, 0x19, 0xb703);
        mdio_write(tp, 0x15, 0x014f);
        mdio_write(tp, 0x19, 0x5cff);
        mdio_write(tp, 0x15, 0x0150);
        mdio_write(tp, 0x19, 0x315f);
        mdio_write(tp, 0x15, 0x0151);
        mdio_write(tp, 0x19, 0x7c08);
        mdio_write(tp, 0x15, 0x0152);
        mdio_write(tp, 0x19, 0x74f8);
        mdio_write(tp, 0x15, 0x0153);
        mdio_write(tp, 0x19, 0x6421);
        mdio_write(tp, 0x15, 0x0154);
        mdio_write(tp, 0x19, 0x7c08);
        mdio_write(tp, 0x15, 0x0155);
        mdio_write(tp, 0x19, 0x6000);
        mdio_write(tp, 0x15, 0x0156);
        mdio_write(tp, 0x19, 0x4003);
        mdio_write(tp, 0x15, 0x0157);
        mdio_write(tp, 0x19, 0x4418);
        mdio_write(tp, 0x15, 0x0158);
        mdio_write(tp, 0x19, 0x9b00);
        mdio_write(tp, 0x15, 0x0159);
        mdio_write(tp, 0x19, 0x6461);
        mdio_write(tp, 0x15, 0x015a);
        mdio_write(tp, 0x19, 0x64e1);
        mdio_write(tp, 0x15, 0x015b);
        mdio_write(tp, 0x19, 0x7c20);
        mdio_write(tp, 0x15, 0x015c);
        mdio_write(tp, 0x19, 0x5820);
        mdio_write(tp, 0x15, 0x015d);
        mdio_write(tp, 0x19, 0x5ccf);
        mdio_write(tp, 0x15, 0x015e);
        mdio_write(tp, 0x19, 0x7050);
        mdio_write(tp, 0x15, 0x015f);
        mdio_write(tp, 0x19, 0xd9b8);
        mdio_write(tp, 0x15, 0x0160);
        mdio_write(tp, 0x19, 0x0008);
        mdio_write(tp, 0x15, 0x0161);
        mdio_write(tp, 0x19, 0xdab1);
        mdio_write(tp, 0x15, 0x0162);
        mdio_write(tp, 0x19, 0x0015);
        mdio_write(tp, 0x15, 0x0163);
        mdio_write(tp, 0x19, 0xc244);
        mdio_write(tp, 0x15, 0x0164);
        mdio_write(tp, 0x19, 0x0013);
        mdio_write(tp, 0x15, 0x0165);
        mdio_write(tp, 0x19, 0xc021);
        mdio_write(tp, 0x15, 0x0166);
        mdio_write(tp, 0x19, 0x00f9);
        mdio_write(tp, 0x15, 0x0167);
        mdio_write(tp, 0x19, 0x3177);
        mdio_write(tp, 0x15, 0x0168);
        mdio_write(tp, 0x19, 0x5cf7);
        mdio_write(tp, 0x15, 0x0169);
        mdio_write(tp, 0x19, 0x4010);
        mdio_write(tp, 0x15, 0x016a);
        mdio_write(tp, 0x19, 0x4428);
        mdio_write(tp, 0x15, 0x016b);
        mdio_write(tp, 0x19, 0x9c00);
        mdio_write(tp, 0x15, 0x016c);
        mdio_write(tp, 0x19, 0x7c08);
        mdio_write(tp, 0x15, 0x016d);
        mdio_write(tp, 0x19, 0x6008);
        mdio_write(tp, 0x15, 0x016e);
        mdio_write(tp, 0x19, 0x7c08);
        mdio_write(tp, 0x15, 0x016f);
        mdio_write(tp, 0x19, 0x74f0);
        mdio_write(tp, 0x15, 0x0170);
        mdio_write(tp, 0x19, 0x6461);
        mdio_write(tp, 0x15, 0x0171);
        mdio_write(tp, 0x19, 0x6421);
        mdio_write(tp, 0x15, 0x0172);
        mdio_write(tp, 0x19, 0x64a1);
        mdio_write(tp, 0x15, 0x0173);
        mdio_write(tp, 0x19, 0x3116);
        mdio_write(tp, 0x15, 0x0174);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x0175);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x0176);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x0177);
        mdio_write(tp, 0x19, 0x4805);
        mdio_write(tp, 0x15, 0x0178);
        mdio_write(tp, 0x19, 0xa103);
        mdio_write(tp, 0x15, 0x0179);
        mdio_write(tp, 0x19, 0x7c02);
        mdio_write(tp, 0x15, 0x017a);
        mdio_write(tp, 0x19, 0x6002);
        mdio_write(tp, 0x15, 0x017b);
        mdio_write(tp, 0x19, 0x7e00);
        mdio_write(tp, 0x15, 0x017c);
        mdio_write(tp, 0x19, 0x5400);
        mdio_write(tp, 0x15, 0x017d);
        mdio_write(tp, 0x19, 0x7c6b);
        mdio_write(tp, 0x15, 0x017e);
        mdio_write(tp, 0x19, 0x5c63);
        mdio_write(tp, 0x15, 0x017f);
        mdio_write(tp, 0x19, 0x407d);
        mdio_write(tp, 0x15, 0x0180);
        mdio_write(tp, 0x19, 0xa602);
        mdio_write(tp, 0x15, 0x0181);
        mdio_write(tp, 0x19, 0x4001);
        mdio_write(tp, 0x15, 0x0182);
        mdio_write(tp, 0x19, 0x4420);
        mdio_write(tp, 0x15, 0x0183);
        mdio_write(tp, 0x19, 0x4020);
        mdio_write(tp, 0x15, 0x0184);
        mdio_write(tp, 0x19, 0x44a1);
        mdio_write(tp, 0x15, 0x0185);
        mdio_write(tp, 0x19, 0xd6e0);
        mdio_write(tp, 0x15, 0x0186);
        mdio_write(tp, 0x19, 0x0009);
        mdio_write(tp, 0x15, 0x0187);
        mdio_write(tp, 0x19, 0x9efe);
        mdio_write(tp, 0x15, 0x0188);
        mdio_write(tp, 0x19, 0x7c02);
        mdio_write(tp, 0x15, 0x0189);
        mdio_write(tp, 0x19, 0x6000);
        mdio_write(tp, 0x15, 0x018a);
        mdio_write(tp, 0x19, 0x9c00);
        mdio_write(tp, 0x15, 0x018b);
        mdio_write(tp, 0x19, 0x318f);
        mdio_write(tp, 0x15, 0x018c);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x018d);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x018e);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x018f);
        mdio_write(tp, 0x19, 0x4806);
        mdio_write(tp, 0x15, 0x0190);
        mdio_write(tp, 0x19, 0x7c10);
        mdio_write(tp, 0x15, 0x0191);
        mdio_write(tp, 0x19, 0x5c10);
        mdio_write(tp, 0x15, 0x0192);
        mdio_write(tp, 0x19, 0x40fa);
        mdio_write(tp, 0x15, 0x0193);
        mdio_write(tp, 0x19, 0xa602);
        mdio_write(tp, 0x15, 0x0194);
        mdio_write(tp, 0x19, 0x4010);
        mdio_write(tp, 0x15, 0x0195);
        mdio_write(tp, 0x19, 0x4440);
        mdio_write(tp, 0x15, 0x0196);
        mdio_write(tp, 0x19, 0x9d00);
        mdio_write(tp, 0x15, 0x0197);
        mdio_write(tp, 0x19, 0x7c80);
        mdio_write(tp, 0x15, 0x0198);
        mdio_write(tp, 0x19, 0x6400);
        mdio_write(tp, 0x15, 0x0199);
        mdio_write(tp, 0x19, 0x4003);
        mdio_write(tp, 0x15, 0x019a);
        mdio_write(tp, 0x19, 0x4540);
        mdio_write(tp, 0x15, 0x019b);
        mdio_write(tp, 0x19, 0x7c08);
        mdio_write(tp, 0x15, 0x019c);
        mdio_write(tp, 0x19, 0x6008);
        mdio_write(tp, 0x15, 0x019d);
        mdio_write(tp, 0x19, 0x9f00);
        mdio_write(tp, 0x15, 0x019e);
        mdio_write(tp, 0x19, 0x7c40);
        mdio_write(tp, 0x15, 0x019f);
        mdio_write(tp, 0x19, 0x6400);
        mdio_write(tp, 0x15, 0x01a0);
        mdio_write(tp, 0x19, 0x7c80);
        mdio_write(tp, 0x15, 0x01a1);
        mdio_write(tp, 0x19, 0x6480);
        mdio_write(tp, 0x15, 0x01a2);
        mdio_write(tp, 0x19, 0x3140);
        mdio_write(tp, 0x15, 0x01a3);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x01a4);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x01a5);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x01a6);
        mdio_write(tp, 0x19, 0x4400);
        mdio_write(tp, 0x15, 0x01a7);
        mdio_write(tp, 0x19, 0x7c0b);
        mdio_write(tp, 0x15, 0x01a8);
        mdio_write(tp, 0x19, 0x6c01);
        mdio_write(tp, 0x15, 0x01a9);
        mdio_write(tp, 0x19, 0x64a8);
        mdio_write(tp, 0x15, 0x01aa);
        mdio_write(tp, 0x19, 0x6800);
        mdio_write(tp, 0x15, 0x01ab);
        mdio_write(tp, 0x19, 0x5cf0);
        mdio_write(tp, 0x15, 0x01ac);
        mdio_write(tp, 0x19, 0x588f);
        mdio_write(tp, 0x15, 0x01ad);
        mdio_write(tp, 0x19, 0xb628);
        mdio_write(tp, 0x15, 0x01ae);
        mdio_write(tp, 0x19, 0xc053);
        mdio_write(tp, 0x15, 0x01af);
        mdio_write(tp, 0x19, 0x0026);
        mdio_write(tp, 0x15, 0x01b0);
        mdio_write(tp, 0x19, 0xc02d);
        mdio_write(tp, 0x15, 0x01b1);
        mdio_write(tp, 0x19, 0x0024);
        mdio_write(tp, 0x15, 0x01b2);
        mdio_write(tp, 0x19, 0xc603);
        mdio_write(tp, 0x15, 0x01b3);
        mdio_write(tp, 0x19, 0x0022);
        mdio_write(tp, 0x15, 0x01b4);
        mdio_write(tp, 0x19, 0x8cf9);
        mdio_write(tp, 0x15, 0x01b5);
        mdio_write(tp, 0x19, 0x31ba);
        mdio_write(tp, 0x15, 0x01b6);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x01b7);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x01b8);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x01b9);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x01ba);
        mdio_write(tp, 0x19, 0x4400);
        mdio_write(tp, 0x15, 0x01bb);
        mdio_write(tp, 0x19, 0x5420);
        mdio_write(tp, 0x15, 0x01bc);
        mdio_write(tp, 0x19, 0x4811);
        mdio_write(tp, 0x15, 0x01bd);
        mdio_write(tp, 0x19, 0x5000);
        mdio_write(tp, 0x15, 0x01be);
        mdio_write(tp, 0x19, 0x4801);
        mdio_write(tp, 0x15, 0x01bf);
        mdio_write(tp, 0x19, 0x6800);
        mdio_write(tp, 0x15, 0x01c0);
        mdio_write(tp, 0x19, 0x31f5);
        mdio_write(tp, 0x15, 0x01c1);
        mdio_write(tp, 0x19, 0xb614);
        mdio_write(tp, 0x15, 0x01c2);
        mdio_write(tp, 0x19, 0x8ce4);
        mdio_write(tp, 0x15, 0x01c3);
        mdio_write(tp, 0x19, 0xb30c);
        mdio_write(tp, 0x15, 0x01c4);
        mdio_write(tp, 0x19, 0x7c03);
        mdio_write(tp, 0x15, 0x01c5);
        mdio_write(tp, 0x19, 0x6c02);
        mdio_write(tp, 0x15, 0x01c6);
        mdio_write(tp, 0x19, 0x8206);
        mdio_write(tp, 0x15, 0x01c7);
        mdio_write(tp, 0x19, 0x7c03);
        mdio_write(tp, 0x15, 0x01c8);
        mdio_write(tp, 0x19, 0x6c00);
        mdio_write(tp, 0x15, 0x01c9);
        mdio_write(tp, 0x19, 0x7c04);
        mdio_write(tp, 0x15, 0x01ca);
        mdio_write(tp, 0x19, 0x7404);
        mdio_write(tp, 0x15, 0x01cb);
        mdio_write(tp, 0x19, 0x31c0);
        mdio_write(tp, 0x15, 0x01cc);
        mdio_write(tp, 0x19, 0x7c04);
        mdio_write(tp, 0x15, 0x01cd);
        mdio_write(tp, 0x19, 0x7400);
        mdio_write(tp, 0x15, 0x01ce);
        mdio_write(tp, 0x19, 0x31c0);
        mdio_write(tp, 0x15, 0x01cf);
        mdio_write(tp, 0x19, 0x8df1);
        mdio_write(tp, 0x15, 0x01d0);
        mdio_write(tp, 0x19, 0x3248);
        mdio_write(tp, 0x15, 0x01d1);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x01d2);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x01d3);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x01d4);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x01d5);
        mdio_write(tp, 0x19, 0x4400);
        mdio_write(tp, 0x15, 0x01d6);
        mdio_write(tp, 0x19, 0x7c03);
        mdio_write(tp, 0x15, 0x01d7);
        mdio_write(tp, 0x19, 0x6c03);
        mdio_write(tp, 0x15, 0x01d8);
        mdio_write(tp, 0x19, 0x7670);
        mdio_write(tp, 0x15, 0x01d9);
        mdio_write(tp, 0x19, 0x4023);
        mdio_write(tp, 0x15, 0x01da);
        mdio_write(tp, 0x19, 0x4500);
        mdio_write(tp, 0x15, 0x01db);
        mdio_write(tp, 0x19, 0x4069);
        mdio_write(tp, 0x15, 0x01dc);
        mdio_write(tp, 0x19, 0x4580);
        mdio_write(tp, 0x15, 0x01dd);
        mdio_write(tp, 0x19, 0x9f00);
        mdio_write(tp, 0x15, 0x01de);
        mdio_write(tp, 0x19, 0xcff5);
        mdio_write(tp, 0x15, 0x01df);
        mdio_write(tp, 0x19, 0x00ff);
        mdio_write(tp, 0x15, 0x01e0);
        mdio_write(tp, 0x19, 0x76f0);
        mdio_write(tp, 0x15, 0x01e1);
        mdio_write(tp, 0x19, 0x4400);
        mdio_write(tp, 0x15, 0x01e2);
        mdio_write(tp, 0x19, 0x4023);
        mdio_write(tp, 0x15, 0x01e3);
        mdio_write(tp, 0x19, 0x4500);
        mdio_write(tp, 0x15, 0x01e4);
        mdio_write(tp, 0x19, 0x4069);
        mdio_write(tp, 0x15, 0x01e5);
        mdio_write(tp, 0x19, 0x4580);
        mdio_write(tp, 0x15, 0x01e6);
        mdio_write(tp, 0x19, 0x9f00);
        mdio_write(tp, 0x15, 0x01e7);
        mdio_write(tp, 0x19, 0xd0f5);
        mdio_write(tp, 0x15, 0x01e8);
        mdio_write(tp, 0x19, 0x00ff);
        mdio_write(tp, 0x15, 0x01e9);
        mdio_write(tp, 0x19, 0x4400);
        mdio_write(tp, 0x15, 0x01ea);
        mdio_write(tp, 0x19, 0x7c03);
        mdio_write(tp, 0x15, 0x01eb);
        mdio_write(tp, 0x19, 0x6800);
        mdio_write(tp, 0x15, 0x01ec);
        mdio_write(tp, 0x19, 0x66a0);
        mdio_write(tp, 0x15, 0x01ed);
        mdio_write(tp, 0x19, 0x8300);
        mdio_write(tp, 0x15, 0x01ee);
        mdio_write(tp, 0x19, 0x74f0);
        mdio_write(tp, 0x15, 0x01ef);
        mdio_write(tp, 0x19, 0x3006);
        mdio_write(tp, 0x15, 0x01f0);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x01f1);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x01f2);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x01f3);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x01f4);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x01f5);
        mdio_write(tp, 0x19, 0x7c03);
        mdio_write(tp, 0x15, 0x01f6);
        mdio_write(tp, 0x19, 0x6c02);
        mdio_write(tp, 0x15, 0x01f7);
        mdio_write(tp, 0x19, 0x409d);
        mdio_write(tp, 0x15, 0x01f8);
        mdio_write(tp, 0x19, 0x7c87);
        mdio_write(tp, 0x15, 0x01f9);
        mdio_write(tp, 0x19, 0xae14);
        mdio_write(tp, 0x15, 0x01fa);
        mdio_write(tp, 0x19, 0x4400);
        mdio_write(tp, 0x15, 0x01fb);
        mdio_write(tp, 0x19, 0x7c40);
        mdio_write(tp, 0x15, 0x01fc);
        mdio_write(tp, 0x19, 0x6800);
        mdio_write(tp, 0x15, 0x01fd);
        mdio_write(tp, 0x19, 0x7801);
        mdio_write(tp, 0x15, 0x01fe);
        mdio_write(tp, 0x19, 0x980e);
        mdio_write(tp, 0x15, 0x01ff);
        mdio_write(tp, 0x19, 0x930c);
        mdio_write(tp, 0x15, 0x0200);
        mdio_write(tp, 0x19, 0x9206);
        mdio_write(tp, 0x15, 0x0201);
        mdio_write(tp, 0x19, 0x4002);
        mdio_write(tp, 0x15, 0x0202);
        mdio_write(tp, 0x19, 0x7800);
        mdio_write(tp, 0x15, 0x0203);
        mdio_write(tp, 0x19, 0x588f);
        mdio_write(tp, 0x15, 0x0204);
        mdio_write(tp, 0x19, 0x5520);
        mdio_write(tp, 0x15, 0x0205);
        mdio_write(tp, 0x19, 0x320c);
        mdio_write(tp, 0x15, 0x0206);
        mdio_write(tp, 0x19, 0x4000);
        mdio_write(tp, 0x15, 0x0207);
        mdio_write(tp, 0x19, 0x7800);
        mdio_write(tp, 0x15, 0x0208);
        mdio_write(tp, 0x19, 0x588d);
        mdio_write(tp, 0x15, 0x0209);
        mdio_write(tp, 0x19, 0x5500);
        mdio_write(tp, 0x15, 0x020a);
        mdio_write(tp, 0x19, 0x320c);
        mdio_write(tp, 0x15, 0x020b);
        mdio_write(tp, 0x19, 0x4002);
        mdio_write(tp, 0x15, 0x020c);
        mdio_write(tp, 0x19, 0x3220);
        mdio_write(tp, 0x15, 0x020d);
        mdio_write(tp, 0x19, 0x4480);
        mdio_write(tp, 0x15, 0x020e);
        mdio_write(tp, 0x19, 0x9e03);
        mdio_write(tp, 0x15, 0x020f);
        mdio_write(tp, 0x19, 0x7c40);
        mdio_write(tp, 0x15, 0x0210);
        mdio_write(tp, 0x19, 0x6840);
        mdio_write(tp, 0x15, 0x0211);
        mdio_write(tp, 0x19, 0x7801);
        mdio_write(tp, 0x15, 0x0212);
        mdio_write(tp, 0x19, 0x980e);
        mdio_write(tp, 0x15, 0x0213);
        mdio_write(tp, 0x19, 0x930c);
        mdio_write(tp, 0x15, 0x0214);
        mdio_write(tp, 0x19, 0x9206);
        mdio_write(tp, 0x15, 0x0215);
        mdio_write(tp, 0x19, 0x4000);
        mdio_write(tp, 0x15, 0x0216);
        mdio_write(tp, 0x19, 0x7800);
        mdio_write(tp, 0x15, 0x0217);
        mdio_write(tp, 0x19, 0x588f);
        mdio_write(tp, 0x15, 0x0218);
        mdio_write(tp, 0x19, 0x5520);
        mdio_write(tp, 0x15, 0x0219);
        mdio_write(tp, 0x19, 0x3220);
        mdio_write(tp, 0x15, 0x021a);
        mdio_write(tp, 0x19, 0x4002);
        mdio_write(tp, 0x15, 0x021b);
        mdio_write(tp, 0x19, 0x7800);
        mdio_write(tp, 0x15, 0x021c);
        mdio_write(tp, 0x19, 0x588d);
        mdio_write(tp, 0x15, 0x021d);
        mdio_write(tp, 0x19, 0x5540);
        mdio_write(tp, 0x15, 0x021e);
        mdio_write(tp, 0x19, 0x3220);
        mdio_write(tp, 0x15, 0x021f);
        mdio_write(tp, 0x19, 0x4000);
        mdio_write(tp, 0x15, 0x0220);
        mdio_write(tp, 0x19, 0x7800);
        mdio_write(tp, 0x15, 0x0221);
        mdio_write(tp, 0x19, 0x7c03);
        mdio_write(tp, 0x15, 0x0222);
        mdio_write(tp, 0x19, 0x6c00);
        mdio_write(tp, 0x15, 0x0223);
        mdio_write(tp, 0x19, 0x3231);
        mdio_write(tp, 0x15, 0x0224);
        mdio_write(tp, 0x19, 0xab06);
        mdio_write(tp, 0x15, 0x0225);
        mdio_write(tp, 0x19, 0xbf08);
        mdio_write(tp, 0x15, 0x0226);
        mdio_write(tp, 0x19, 0x4076);
        mdio_write(tp, 0x15, 0x0227);
        mdio_write(tp, 0x19, 0x7d07);
        mdio_write(tp, 0x15, 0x0228);
        mdio_write(tp, 0x19, 0x4502);
        mdio_write(tp, 0x15, 0x0229);
        mdio_write(tp, 0x19, 0x3231);
        mdio_write(tp, 0x15, 0x022a);
        mdio_write(tp, 0x19, 0x7d80);
        mdio_write(tp, 0x15, 0x022b);
        mdio_write(tp, 0x19, 0x5180);
        mdio_write(tp, 0x15, 0x022c);
        mdio_write(tp, 0x19, 0x322f);
        mdio_write(tp, 0x15, 0x022d);
        mdio_write(tp, 0x19, 0x7d80);
        mdio_write(tp, 0x15, 0x022e);
        mdio_write(tp, 0x19, 0x5000);
        mdio_write(tp, 0x15, 0x022f);
        mdio_write(tp, 0x19, 0x7d07);
        mdio_write(tp, 0x15, 0x0230);
        mdio_write(tp, 0x19, 0x4402);
        mdio_write(tp, 0x15, 0x0231);
        mdio_write(tp, 0x19, 0x7c03);
        mdio_write(tp, 0x15, 0x0232);
        mdio_write(tp, 0x19, 0x6c02);
        mdio_write(tp, 0x15, 0x0233);
        mdio_write(tp, 0x19, 0x7c03);
        mdio_write(tp, 0x15, 0x0234);
        mdio_write(tp, 0x19, 0xb309);
        mdio_write(tp, 0x15, 0x0235);
        mdio_write(tp, 0x19, 0xb204);
        mdio_write(tp, 0x15, 0x0236);
        mdio_write(tp, 0x19, 0xb105);
        mdio_write(tp, 0x15, 0x0237);
        mdio_write(tp, 0x19, 0x6c00);
        mdio_write(tp, 0x15, 0x0238);
        mdio_write(tp, 0x19, 0x31c1);
        mdio_write(tp, 0x15, 0x0239);
        mdio_write(tp, 0x19, 0x6c00);
        mdio_write(tp, 0x15, 0x023a);
        mdio_write(tp, 0x19, 0x3261);
        mdio_write(tp, 0x15, 0x023b);
        mdio_write(tp, 0x19, 0x6c00);
        mdio_write(tp, 0x15, 0x023c);
        mdio_write(tp, 0x19, 0x3250);
        mdio_write(tp, 0x15, 0x023d);
        mdio_write(tp, 0x19, 0xb203);
        mdio_write(tp, 0x15, 0x023e);
        mdio_write(tp, 0x19, 0x6c00);
        mdio_write(tp, 0x15, 0x023f);
        mdio_write(tp, 0x19, 0x327a);
        mdio_write(tp, 0x15, 0x0240);
        mdio_write(tp, 0x19, 0x6c00);
        mdio_write(tp, 0x15, 0x0241);
        mdio_write(tp, 0x19, 0x3293);
        mdio_write(tp, 0x15, 0x0242);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x0243);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x0244);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x0245);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x0246);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x0247);
        mdio_write(tp, 0x19, 0x32a3);
        mdio_write(tp, 0x15, 0x0248);
        mdio_write(tp, 0x19, 0x5520);
        mdio_write(tp, 0x15, 0x0249);
        mdio_write(tp, 0x19, 0x403d);
        mdio_write(tp, 0x15, 0x024a);
        mdio_write(tp, 0x19, 0x440c);
        mdio_write(tp, 0x15, 0x024b);
        mdio_write(tp, 0x19, 0x4812);
        mdio_write(tp, 0x15, 0x024c);
        mdio_write(tp, 0x19, 0x5001);
        mdio_write(tp, 0x15, 0x024d);
        mdio_write(tp, 0x19, 0x4802);
        mdio_write(tp, 0x15, 0x024e);
        mdio_write(tp, 0x19, 0x6880);
        mdio_write(tp, 0x15, 0x024f);
        mdio_write(tp, 0x19, 0x31f5);
        mdio_write(tp, 0x15, 0x0250);
        mdio_write(tp, 0x19, 0xb685);
        mdio_write(tp, 0x15, 0x0251);
        mdio_write(tp, 0x19, 0x801c);
        mdio_write(tp, 0x15, 0x0252);
        mdio_write(tp, 0x19, 0xbaf5);
        mdio_write(tp, 0x15, 0x0253);
        mdio_write(tp, 0x19, 0xc07c);
        mdio_write(tp, 0x15, 0x0254);
        mdio_write(tp, 0x19, 0x00fb);
        mdio_write(tp, 0x15, 0x0255);
        mdio_write(tp, 0x19, 0x325a);
        mdio_write(tp, 0x15, 0x0256);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x0257);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x0258);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x0259);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x025a);
        mdio_write(tp, 0x19, 0x481a);
        mdio_write(tp, 0x15, 0x025b);
        mdio_write(tp, 0x19, 0x5001);
        mdio_write(tp, 0x15, 0x025c);
        mdio_write(tp, 0x19, 0x401b);
        mdio_write(tp, 0x15, 0x025d);
        mdio_write(tp, 0x19, 0x480a);
        mdio_write(tp, 0x15, 0x025e);
        mdio_write(tp, 0x19, 0x4418);
        mdio_write(tp, 0x15, 0x025f);
        mdio_write(tp, 0x19, 0x6900);
        mdio_write(tp, 0x15, 0x0260);
        mdio_write(tp, 0x19, 0x31f5);
        mdio_write(tp, 0x15, 0x0261);
        mdio_write(tp, 0x19, 0xb64b);
        mdio_write(tp, 0x15, 0x0262);
        mdio_write(tp, 0x19, 0xdb00);
        mdio_write(tp, 0x15, 0x0263);
        mdio_write(tp, 0x19, 0x0048);
        mdio_write(tp, 0x15, 0x0264);
        mdio_write(tp, 0x19, 0xdb7d);
        mdio_write(tp, 0x15, 0x0265);
        mdio_write(tp, 0x19, 0x0002);
        mdio_write(tp, 0x15, 0x0266);
        mdio_write(tp, 0x19, 0xa0fa);
        mdio_write(tp, 0x15, 0x0267);
        mdio_write(tp, 0x19, 0x4408);
        mdio_write(tp, 0x15, 0x0268);
        mdio_write(tp, 0x19, 0x3248);
        mdio_write(tp, 0x15, 0x0269);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x026a);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x026b);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x026c);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x026d);
        mdio_write(tp, 0x19, 0xb806);
        mdio_write(tp, 0x15, 0x026e);
        mdio_write(tp, 0x19, 0x588d);
        mdio_write(tp, 0x15, 0x026f);
        mdio_write(tp, 0x19, 0x5500);
        mdio_write(tp, 0x15, 0x0270);
        mdio_write(tp, 0x19, 0x7801);
        mdio_write(tp, 0x15, 0x0271);
        mdio_write(tp, 0x19, 0x4002);
        mdio_write(tp, 0x15, 0x0272);
        mdio_write(tp, 0x19, 0x7800);
        mdio_write(tp, 0x15, 0x0273);
        mdio_write(tp, 0x19, 0x4814);
        mdio_write(tp, 0x15, 0x0274);
        mdio_write(tp, 0x19, 0x500b);
        mdio_write(tp, 0x15, 0x0275);
        mdio_write(tp, 0x19, 0x4804);
        mdio_write(tp, 0x15, 0x0276);
        mdio_write(tp, 0x19, 0x40c4);
        mdio_write(tp, 0x15, 0x0277);
        mdio_write(tp, 0x19, 0x4425);
        mdio_write(tp, 0x15, 0x0278);
        mdio_write(tp, 0x19, 0x6a00);
        mdio_write(tp, 0x15, 0x0279);
        mdio_write(tp, 0x19, 0x31f5);
        mdio_write(tp, 0x15, 0x027a);
        mdio_write(tp, 0x19, 0xb632);
        mdio_write(tp, 0x15, 0x027b);
        mdio_write(tp, 0x19, 0xdc03);
        mdio_write(tp, 0x15, 0x027c);
        mdio_write(tp, 0x19, 0x0027);
        mdio_write(tp, 0x15, 0x027d);
        mdio_write(tp, 0x19, 0x80fc);
        mdio_write(tp, 0x15, 0x027e);
        mdio_write(tp, 0x19, 0x3283);
        mdio_write(tp, 0x15, 0x027f);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x0280);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x0281);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x0282);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x0283);
        mdio_write(tp, 0x19, 0xb806);
        mdio_write(tp, 0x15, 0x0284);
        mdio_write(tp, 0x19, 0x588f);
        mdio_write(tp, 0x15, 0x0285);
        mdio_write(tp, 0x19, 0x5520);
        mdio_write(tp, 0x15, 0x0286);
        mdio_write(tp, 0x19, 0x7801);
        mdio_write(tp, 0x15, 0x0287);
        mdio_write(tp, 0x19, 0x4000);
        mdio_write(tp, 0x15, 0x0288);
        mdio_write(tp, 0x19, 0x7800);
        mdio_write(tp, 0x15, 0x0289);
        mdio_write(tp, 0x19, 0x4818);
        mdio_write(tp, 0x15, 0x028a);
        mdio_write(tp, 0x19, 0x5051);
        mdio_write(tp, 0x15, 0x028b);
        mdio_write(tp, 0x19, 0x4808);
        mdio_write(tp, 0x15, 0x028c);
        mdio_write(tp, 0x19, 0x4050);
        mdio_write(tp, 0x15, 0x028d);
        mdio_write(tp, 0x19, 0x4462);
        mdio_write(tp, 0x15, 0x028e);
        mdio_write(tp, 0x19, 0x40c4);
        mdio_write(tp, 0x15, 0x028f);
        mdio_write(tp, 0x19, 0x4473);
        mdio_write(tp, 0x15, 0x0290);
        mdio_write(tp, 0x19, 0x5041);
        mdio_write(tp, 0x15, 0x0291);
        mdio_write(tp, 0x19, 0x6b00);
        mdio_write(tp, 0x15, 0x0292);
        mdio_write(tp, 0x19, 0x31f5);
        mdio_write(tp, 0x15, 0x0293);
        mdio_write(tp, 0x19, 0xb619);
        mdio_write(tp, 0x15, 0x0294);
        mdio_write(tp, 0x19, 0x80d9);
        mdio_write(tp, 0x15, 0x0295);
        mdio_write(tp, 0x19, 0xbd06);
        mdio_write(tp, 0x15, 0x0296);
        mdio_write(tp, 0x19, 0xbb0d);
        mdio_write(tp, 0x15, 0x0297);
        mdio_write(tp, 0x19, 0xaf14);
        mdio_write(tp, 0x15, 0x0298);
        mdio_write(tp, 0x19, 0x8efa);
        mdio_write(tp, 0x15, 0x0299);
        mdio_write(tp, 0x19, 0x5049);
        mdio_write(tp, 0x15, 0x029a);
        mdio_write(tp, 0x19, 0x3248);
        mdio_write(tp, 0x15, 0x029b);
        mdio_write(tp, 0x19, 0x4c10);
        mdio_write(tp, 0x15, 0x029c);
        mdio_write(tp, 0x19, 0x44b0);
        mdio_write(tp, 0x15, 0x029d);
        mdio_write(tp, 0x19, 0x4c00);
        mdio_write(tp, 0x15, 0x029e);
        mdio_write(tp, 0x19, 0x3292);
        mdio_write(tp, 0x15, 0x029f);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x02a0);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x02a1);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x02a2);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x02a3);
        mdio_write(tp, 0x19, 0x481f);
        mdio_write(tp, 0x15, 0x02a4);
        mdio_write(tp, 0x19, 0x5005);
        mdio_write(tp, 0x15, 0x02a5);
        mdio_write(tp, 0x19, 0x480f);
        mdio_write(tp, 0x15, 0x02a6);
        mdio_write(tp, 0x19, 0xac00);
        mdio_write(tp, 0x15, 0x02a7);
        mdio_write(tp, 0x19, 0x31a6);
        mdio_write(tp, 0x15, 0x02a8);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x02a9);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x02aa);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x02ab);
        mdio_write(tp, 0x19, 0x31ba);
        mdio_write(tp, 0x15, 0x02ac);
        mdio_write(tp, 0x19, 0x31d5);
        mdio_write(tp, 0x15, 0x02ad);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x02ae);
        mdio_write(tp, 0x19, 0x5cf0);
        mdio_write(tp, 0x15, 0x02af);
        mdio_write(tp, 0x19, 0x588c);
        mdio_write(tp, 0x15, 0x02b0);
        mdio_write(tp, 0x19, 0x542f);
        mdio_write(tp, 0x15, 0x02b1);
        mdio_write(tp, 0x19, 0x7ffb);
        mdio_write(tp, 0x15, 0x02b2);
        mdio_write(tp, 0x19, 0x6ff8);
        mdio_write(tp, 0x15, 0x02b3);
        mdio_write(tp, 0x19, 0x64a4);
        mdio_write(tp, 0x15, 0x02b4);
        mdio_write(tp, 0x19, 0x64a0);
        mdio_write(tp, 0x15, 0x02b5);
        mdio_write(tp, 0x19, 0x6800);
        mdio_write(tp, 0x15, 0x02b6);
        mdio_write(tp, 0x19, 0x4400);
        mdio_write(tp, 0x15, 0x02b7);
        mdio_write(tp, 0x19, 0x4020);
        mdio_write(tp, 0x15, 0x02b8);
        mdio_write(tp, 0x19, 0x4480);
        mdio_write(tp, 0x15, 0x02b9);
        mdio_write(tp, 0x19, 0x9e00);
        mdio_write(tp, 0x15, 0x02ba);
        mdio_write(tp, 0x19, 0x4891);
        mdio_write(tp, 0x15, 0x02bb);
        mdio_write(tp, 0x19, 0x4cc0);
        mdio_write(tp, 0x15, 0x02bc);
        mdio_write(tp, 0x19, 0x4801);
        mdio_write(tp, 0x15, 0x02bd);
        mdio_write(tp, 0x19, 0xa609);
        mdio_write(tp, 0x15, 0x02be);
        mdio_write(tp, 0x19, 0xd64f);
        mdio_write(tp, 0x15, 0x02bf);
        mdio_write(tp, 0x19, 0x004e);
        mdio_write(tp, 0x15, 0x02c0);
        mdio_write(tp, 0x19, 0x87fe);
        mdio_write(tp, 0x15, 0x02c1);
        mdio_write(tp, 0x19, 0x32c6);
        mdio_write(tp, 0x15, 0x02c2);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x02c3);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x02c4);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x02c5);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x02c6);
        mdio_write(tp, 0x19, 0x48b2);
        mdio_write(tp, 0x15, 0x02c7);
        mdio_write(tp, 0x19, 0x4020);
        mdio_write(tp, 0x15, 0x02c8);
        mdio_write(tp, 0x19, 0x4822);
        mdio_write(tp, 0x15, 0x02c9);
        mdio_write(tp, 0x19, 0x4488);
        mdio_write(tp, 0x15, 0x02ca);
        mdio_write(tp, 0x19, 0xd64f);
        mdio_write(tp, 0x15, 0x02cb);
        mdio_write(tp, 0x19, 0x0042);
        mdio_write(tp, 0x15, 0x02cc);
        mdio_write(tp, 0x19, 0x8203);
        mdio_write(tp, 0x15, 0x02cd);
        mdio_write(tp, 0x19, 0x4cc8);
        mdio_write(tp, 0x15, 0x02ce);
        mdio_write(tp, 0x19, 0x32d0);
        mdio_write(tp, 0x15, 0x02cf);
        mdio_write(tp, 0x19, 0x4cc0);
        mdio_write(tp, 0x15, 0x02d0);
        mdio_write(tp, 0x19, 0xc4d4);
        mdio_write(tp, 0x15, 0x02d1);
        mdio_write(tp, 0x19, 0x00f9);
        mdio_write(tp, 0x15, 0x02d2);
        mdio_write(tp, 0x19, 0xa51a);
        mdio_write(tp, 0x15, 0x02d3);
        mdio_write(tp, 0x19, 0x32d9);
        mdio_write(tp, 0x15, 0x02d4);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x02d5);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x02d6);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x02d7);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x02d8);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x02d9);
        mdio_write(tp, 0x19, 0x48b3);
        mdio_write(tp, 0x15, 0x02da);
        mdio_write(tp, 0x19, 0x4020);
        mdio_write(tp, 0x15, 0x02db);
        mdio_write(tp, 0x19, 0x4823);
        mdio_write(tp, 0x15, 0x02dc);
        mdio_write(tp, 0x19, 0x4410);
        mdio_write(tp, 0x15, 0x02dd);
        mdio_write(tp, 0x19, 0xb630);
        mdio_write(tp, 0x15, 0x02de);
        mdio_write(tp, 0x19, 0x7dc8);
        mdio_write(tp, 0x15, 0x02df);
        mdio_write(tp, 0x19, 0x8203);
        mdio_write(tp, 0x15, 0x02e0);
        mdio_write(tp, 0x19, 0x4c48);
        mdio_write(tp, 0x15, 0x02e1);
        mdio_write(tp, 0x19, 0x32e3);
        mdio_write(tp, 0x15, 0x02e2);
        mdio_write(tp, 0x19, 0x4c40);
        mdio_write(tp, 0x15, 0x02e3);
        mdio_write(tp, 0x19, 0x9bfa);
        mdio_write(tp, 0x15, 0x02e4);
        mdio_write(tp, 0x19, 0x84ca);
        mdio_write(tp, 0x15, 0x02e5);
        mdio_write(tp, 0x19, 0x85f8);
        mdio_write(tp, 0x15, 0x02e6);
        mdio_write(tp, 0x19, 0x32ec);
        mdio_write(tp, 0x15, 0x02e7);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x02e8);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x02e9);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x02ea);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x02eb);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x02ec);
        mdio_write(tp, 0x19, 0x48d4);
        mdio_write(tp, 0x15, 0x02ed);
        mdio_write(tp, 0x19, 0x4020);
        mdio_write(tp, 0x15, 0x02ee);
        mdio_write(tp, 0x19, 0x4844);
        mdio_write(tp, 0x15, 0x02ef);
        mdio_write(tp, 0x19, 0x4420);
        mdio_write(tp, 0x15, 0x02f0);
        mdio_write(tp, 0x19, 0x6800);
        mdio_write(tp, 0x15, 0x02f1);
        mdio_write(tp, 0x19, 0x7dc0);
        mdio_write(tp, 0x15, 0x02f2);
        mdio_write(tp, 0x19, 0x4c40);
        mdio_write(tp, 0x15, 0x02f3);
        mdio_write(tp, 0x19, 0x7c0b);
        mdio_write(tp, 0x15, 0x02f4);
        mdio_write(tp, 0x19, 0x6c08);
        mdio_write(tp, 0x15, 0x02f5);
        mdio_write(tp, 0x19, 0x3311);
        mdio_write(tp, 0x15, 0x02f6);
        mdio_write(tp, 0x19, 0x9cfd);
        mdio_write(tp, 0x15, 0x02f7);
        mdio_write(tp, 0x19, 0xb616);
        mdio_write(tp, 0x15, 0x02f8);
        mdio_write(tp, 0x19, 0xc42b);
        mdio_write(tp, 0x15, 0x02f9);
        mdio_write(tp, 0x19, 0x00e0);
        mdio_write(tp, 0x15, 0x02fa);
        mdio_write(tp, 0x19, 0xc455);
        mdio_write(tp, 0x15, 0x02fb);
        mdio_write(tp, 0x19, 0x00b3);
        mdio_write(tp, 0x15, 0x02fc);
        mdio_write(tp, 0x19, 0xb20a);
        mdio_write(tp, 0x15, 0x02fd);
        mdio_write(tp, 0x19, 0x7c03);
        mdio_write(tp, 0x15, 0x02fe);
        mdio_write(tp, 0x19, 0x6c02);
        mdio_write(tp, 0x15, 0x02ff);
        mdio_write(tp, 0x19, 0x8204);
        mdio_write(tp, 0x15, 0x0300);
        mdio_write(tp, 0x19, 0x7c04);
        mdio_write(tp, 0x15, 0x0301);
        mdio_write(tp, 0x19, 0x7404);
        mdio_write(tp, 0x15, 0x0302);
        mdio_write(tp, 0x19, 0x32f3);
        mdio_write(tp, 0x15, 0x0303);
        mdio_write(tp, 0x19, 0x7c04);
        mdio_write(tp, 0x15, 0x0304);
        mdio_write(tp, 0x19, 0x7400);
        mdio_write(tp, 0x15, 0x0305);
        mdio_write(tp, 0x19, 0x32f3);
        mdio_write(tp, 0x15, 0x0306);
        mdio_write(tp, 0x19, 0xefed);
        mdio_write(tp, 0x15, 0x0307);
        mdio_write(tp, 0x19, 0x3342);
        mdio_write(tp, 0x15, 0x0308);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x0309);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x030a);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x030b);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x030c);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x030d);
        mdio_write(tp, 0x19, 0x3006);
        mdio_write(tp, 0x15, 0x030e);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x030f);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x0310);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x0311);
        mdio_write(tp, 0x19, 0x7c08);
        mdio_write(tp, 0x15, 0x0312);
        mdio_write(tp, 0x19, 0xa207);
        mdio_write(tp, 0x15, 0x0313);
        mdio_write(tp, 0x19, 0x4c00);
        mdio_write(tp, 0x15, 0x0314);
        mdio_write(tp, 0x19, 0x3322);
        mdio_write(tp, 0x15, 0x0315);
        mdio_write(tp, 0x19, 0x4041);
        mdio_write(tp, 0x15, 0x0316);
        mdio_write(tp, 0x19, 0x7d07);
        mdio_write(tp, 0x15, 0x0317);
        mdio_write(tp, 0x19, 0x4502);
        mdio_write(tp, 0x15, 0x0318);
        mdio_write(tp, 0x19, 0x3322);
        mdio_write(tp, 0x15, 0x0319);
        mdio_write(tp, 0x19, 0x4c08);
        mdio_write(tp, 0x15, 0x031a);
        mdio_write(tp, 0x19, 0x3322);
        mdio_write(tp, 0x15, 0x031b);
        mdio_write(tp, 0x19, 0x7d80);
        mdio_write(tp, 0x15, 0x031c);
        mdio_write(tp, 0x19, 0x5180);
        mdio_write(tp, 0x15, 0x031d);
        mdio_write(tp, 0x19, 0x3320);
        mdio_write(tp, 0x15, 0x031e);
        mdio_write(tp, 0x19, 0x7d80);
        mdio_write(tp, 0x15, 0x031f);
        mdio_write(tp, 0x19, 0x5000);
        mdio_write(tp, 0x15, 0x0320);
        mdio_write(tp, 0x19, 0x7d07);
        mdio_write(tp, 0x15, 0x0321);
        mdio_write(tp, 0x19, 0x4402);
        mdio_write(tp, 0x15, 0x0322);
        mdio_write(tp, 0x19, 0x7c03);
        mdio_write(tp, 0x15, 0x0323);
        mdio_write(tp, 0x19, 0x6c02);
        mdio_write(tp, 0x15, 0x0324);
        mdio_write(tp, 0x19, 0x7c03);
        mdio_write(tp, 0x15, 0x0325);
        mdio_write(tp, 0x19, 0xb30c);
        mdio_write(tp, 0x15, 0x0326);
        mdio_write(tp, 0x19, 0xb206);
        mdio_write(tp, 0x15, 0x0327);
        mdio_write(tp, 0x19, 0xb103);
        mdio_write(tp, 0x15, 0x0328);
        mdio_write(tp, 0x19, 0x6c00);
        mdio_write(tp, 0x15, 0x0329);
        mdio_write(tp, 0x19, 0x32f6);
        mdio_write(tp, 0x15, 0x032a);
        mdio_write(tp, 0x19, 0x6c00);
        mdio_write(tp, 0x15, 0x032b);
        mdio_write(tp, 0x19, 0x3352);
        mdio_write(tp, 0x15, 0x032c);
        mdio_write(tp, 0x19, 0xb103);
        mdio_write(tp, 0x15, 0x032d);
        mdio_write(tp, 0x19, 0x6c00);
        mdio_write(tp, 0x15, 0x032e);
        mdio_write(tp, 0x19, 0x336a);
        mdio_write(tp, 0x15, 0x032f);
        mdio_write(tp, 0x19, 0x6c00);
        mdio_write(tp, 0x15, 0x0330);
        mdio_write(tp, 0x19, 0x3382);
        mdio_write(tp, 0x15, 0x0331);
        mdio_write(tp, 0x19, 0xb206);
        mdio_write(tp, 0x15, 0x0332);
        mdio_write(tp, 0x19, 0xb103);
        mdio_write(tp, 0x15, 0x0333);
        mdio_write(tp, 0x19, 0x6c00);
        mdio_write(tp, 0x15, 0x0334);
        mdio_write(tp, 0x19, 0x3395);
        mdio_write(tp, 0x15, 0x0335);
        mdio_write(tp, 0x19, 0x6c00);
        mdio_write(tp, 0x15, 0x0336);
        mdio_write(tp, 0x19, 0x33c6);
        mdio_write(tp, 0x15, 0x0337);
        mdio_write(tp, 0x19, 0xb103);
        mdio_write(tp, 0x15, 0x0338);
        mdio_write(tp, 0x19, 0x6c00);
        mdio_write(tp, 0x15, 0x0339);
        mdio_write(tp, 0x19, 0x33d7);
        mdio_write(tp, 0x15, 0x033a);
        mdio_write(tp, 0x19, 0x6c00);
        mdio_write(tp, 0x15, 0x033b);
        mdio_write(tp, 0x19, 0x33f2);
        mdio_write(tp, 0x15, 0x033c);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x033d);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x033e);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x033f);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x0340);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x0341);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x0342);
        mdio_write(tp, 0x19, 0x49b5);
        mdio_write(tp, 0x15, 0x0343);
        mdio_write(tp, 0x19, 0x7d00);
        mdio_write(tp, 0x15, 0x0344);
        mdio_write(tp, 0x19, 0x4d00);
        mdio_write(tp, 0x15, 0x0345);
        mdio_write(tp, 0x19, 0x6880);
        mdio_write(tp, 0x15, 0x0346);
        mdio_write(tp, 0x19, 0x7c08);
        mdio_write(tp, 0x15, 0x0347);
        mdio_write(tp, 0x19, 0x6c08);
        mdio_write(tp, 0x15, 0x0348);
        mdio_write(tp, 0x19, 0x4925);
        mdio_write(tp, 0x15, 0x0349);
        mdio_write(tp, 0x19, 0x403b);
        mdio_write(tp, 0x15, 0x034a);
        mdio_write(tp, 0x19, 0xa602);
        mdio_write(tp, 0x15, 0x034b);
        mdio_write(tp, 0x19, 0x402f);
        mdio_write(tp, 0x15, 0x034c);
        mdio_write(tp, 0x19, 0x4484);
        mdio_write(tp, 0x15, 0x034d);
        mdio_write(tp, 0x19, 0x40c8);
        mdio_write(tp, 0x15, 0x034e);
        mdio_write(tp, 0x19, 0x44c4);
        mdio_write(tp, 0x15, 0x034f);
        mdio_write(tp, 0x19, 0xd64f);
        mdio_write(tp, 0x15, 0x0350);
        mdio_write(tp, 0x19, 0x00bd);
        mdio_write(tp, 0x15, 0x0351);
        mdio_write(tp, 0x19, 0x3311);
        mdio_write(tp, 0x15, 0x0352);
        mdio_write(tp, 0x19, 0xc8ed);
        mdio_write(tp, 0x15, 0x0353);
        mdio_write(tp, 0x19, 0x00fc);
        mdio_write(tp, 0x15, 0x0354);
        mdio_write(tp, 0x19, 0x8221);
        mdio_write(tp, 0x15, 0x0355);
        mdio_write(tp, 0x19, 0xd11d);
        mdio_write(tp, 0x15, 0x0356);
        mdio_write(tp, 0x19, 0x001f);
        mdio_write(tp, 0x15, 0x0357);
        mdio_write(tp, 0x19, 0xde18);
        mdio_write(tp, 0x15, 0x0358);
        mdio_write(tp, 0x19, 0x0008);
        mdio_write(tp, 0x15, 0x0359);
        mdio_write(tp, 0x19, 0x91f6);
        mdio_write(tp, 0x15, 0x035a);
        mdio_write(tp, 0x19, 0x3360);
        mdio_write(tp, 0x15, 0x035b);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x035c);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x035d);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x035e);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x035f);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x0360);
        mdio_write(tp, 0x19, 0x4bb6);
        mdio_write(tp, 0x15, 0x0361);
        mdio_write(tp, 0x19, 0x4064);
        mdio_write(tp, 0x15, 0x0362);
        mdio_write(tp, 0x19, 0x4b26);
        mdio_write(tp, 0x15, 0x0363);
        mdio_write(tp, 0x19, 0x4410);
        mdio_write(tp, 0x15, 0x0364);
        mdio_write(tp, 0x19, 0x4006);
        mdio_write(tp, 0x15, 0x0365);
        mdio_write(tp, 0x19, 0x4490);
        mdio_write(tp, 0x15, 0x0366);
        mdio_write(tp, 0x19, 0x6900);
        mdio_write(tp, 0x15, 0x0367);
        mdio_write(tp, 0x19, 0xb6a6);
        mdio_write(tp, 0x15, 0x0368);
        mdio_write(tp, 0x19, 0x9e02);
        mdio_write(tp, 0x15, 0x0369);
        mdio_write(tp, 0x19, 0x3311);
        mdio_write(tp, 0x15, 0x036a);
        mdio_write(tp, 0x19, 0xd11d);
        mdio_write(tp, 0x15, 0x036b);
        mdio_write(tp, 0x19, 0x000a);
        mdio_write(tp, 0x15, 0x036c);
        mdio_write(tp, 0x19, 0xbb0f);
        mdio_write(tp, 0x15, 0x036d);
        mdio_write(tp, 0x19, 0x8102);
        mdio_write(tp, 0x15, 0x036e);
        mdio_write(tp, 0x19, 0x3371);
        mdio_write(tp, 0x15, 0x036f);
        mdio_write(tp, 0x19, 0xa21e);
        mdio_write(tp, 0x15, 0x0370);
        mdio_write(tp, 0x19, 0x33b6);
        mdio_write(tp, 0x15, 0x0371);
        mdio_write(tp, 0x19, 0x91f6);
        mdio_write(tp, 0x15, 0x0372);
        mdio_write(tp, 0x19, 0xc218);
        mdio_write(tp, 0x15, 0x0373);
        mdio_write(tp, 0x19, 0x00f4);
        mdio_write(tp, 0x15, 0x0374);
        mdio_write(tp, 0x19, 0x33b6);
        mdio_write(tp, 0x15, 0x0375);
        mdio_write(tp, 0x19, 0x32ec);
        mdio_write(tp, 0x15, 0x0376);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x0377);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x0378);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x0379);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x037a);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x037b);
        mdio_write(tp, 0x19, 0x4b97);
        mdio_write(tp, 0x15, 0x037c);
        mdio_write(tp, 0x19, 0x402b);
        mdio_write(tp, 0x15, 0x037d);
        mdio_write(tp, 0x19, 0x4b07);
        mdio_write(tp, 0x15, 0x037e);
        mdio_write(tp, 0x19, 0x4422);
        mdio_write(tp, 0x15, 0x037f);
        mdio_write(tp, 0x19, 0x6980);
        mdio_write(tp, 0x15, 0x0380);
        mdio_write(tp, 0x19, 0xb608);
        mdio_write(tp, 0x15, 0x0381);
        mdio_write(tp, 0x19, 0x3311);
        mdio_write(tp, 0x15, 0x0382);
        mdio_write(tp, 0x19, 0xbc05);
        mdio_write(tp, 0x15, 0x0383);
        mdio_write(tp, 0x19, 0xc21c);
        mdio_write(tp, 0x15, 0x0384);
        mdio_write(tp, 0x19, 0x0032);
        mdio_write(tp, 0x15, 0x0385);
        mdio_write(tp, 0x19, 0xa1fb);
        mdio_write(tp, 0x15, 0x0386);
        mdio_write(tp, 0x19, 0x338d);
        mdio_write(tp, 0x15, 0x0387);
        mdio_write(tp, 0x19, 0x32ae);
        mdio_write(tp, 0x15, 0x0388);
        mdio_write(tp, 0x19, 0x330d);
        mdio_write(tp, 0x15, 0x0389);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x038a);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x038b);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x038c);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x038d);
        mdio_write(tp, 0x19, 0x4b97);
        mdio_write(tp, 0x15, 0x038e);
        mdio_write(tp, 0x19, 0x6a08);
        mdio_write(tp, 0x15, 0x038f);
        mdio_write(tp, 0x19, 0x4b07);
        mdio_write(tp, 0x15, 0x0390);
        mdio_write(tp, 0x19, 0x40ac);
        mdio_write(tp, 0x15, 0x0391);
        mdio_write(tp, 0x19, 0x4445);
        mdio_write(tp, 0x15, 0x0392);
        mdio_write(tp, 0x19, 0x404e);
        mdio_write(tp, 0x15, 0x0393);
        mdio_write(tp, 0x19, 0x4461);
        mdio_write(tp, 0x15, 0x0394);
        mdio_write(tp, 0x19, 0x3311);
        mdio_write(tp, 0x15, 0x0395);
        mdio_write(tp, 0x19, 0x9c0a);
        mdio_write(tp, 0x15, 0x0396);
        mdio_write(tp, 0x19, 0x63da);
        mdio_write(tp, 0x15, 0x0397);
        mdio_write(tp, 0x19, 0x6f0c);
        mdio_write(tp, 0x15, 0x0398);
        mdio_write(tp, 0x19, 0x5440);
        mdio_write(tp, 0x15, 0x0399);
        mdio_write(tp, 0x19, 0x4b98);
        mdio_write(tp, 0x15, 0x039a);
        mdio_write(tp, 0x19, 0x7c40);
        mdio_write(tp, 0x15, 0x039b);
        mdio_write(tp, 0x19, 0x4c00);
        mdio_write(tp, 0x15, 0x039c);
        mdio_write(tp, 0x19, 0x4b08);
        mdio_write(tp, 0x15, 0x039d);
        mdio_write(tp, 0x19, 0x63d8);
        mdio_write(tp, 0x15, 0x039e);
        mdio_write(tp, 0x19, 0x33a5);
        mdio_write(tp, 0x15, 0x039f);
        mdio_write(tp, 0x19, 0xd64f);
        mdio_write(tp, 0x15, 0x03a0);
        mdio_write(tp, 0x19, 0x00e8);
        mdio_write(tp, 0x15, 0x03a1);
        mdio_write(tp, 0x19, 0x820e);
        mdio_write(tp, 0x15, 0x03a2);
        mdio_write(tp, 0x19, 0xa10d);
        mdio_write(tp, 0x15, 0x03a3);
        mdio_write(tp, 0x19, 0x9df1);
        mdio_write(tp, 0x15, 0x03a4);
        mdio_write(tp, 0x19, 0x33af);
        mdio_write(tp, 0x15, 0x03a5);
        mdio_write(tp, 0x19, 0xd64f);
        mdio_write(tp, 0x15, 0x03a6);
        mdio_write(tp, 0x19, 0x00f9);
        mdio_write(tp, 0x15, 0x03a7);
        mdio_write(tp, 0x19, 0xc017);
        mdio_write(tp, 0x15, 0x03a8);
        mdio_write(tp, 0x19, 0x0007);
        mdio_write(tp, 0x15, 0x03a9);
        mdio_write(tp, 0x19, 0x7c03);
        mdio_write(tp, 0x15, 0x03aa);
        mdio_write(tp, 0x19, 0x6c03);
        mdio_write(tp, 0x15, 0x03ab);
        mdio_write(tp, 0x19, 0xa104);
        mdio_write(tp, 0x15, 0x03ac);
        mdio_write(tp, 0x19, 0x7c03);
        mdio_write(tp, 0x15, 0x03ad);
        mdio_write(tp, 0x19, 0x6c00);
        mdio_write(tp, 0x15, 0x03ae);
        mdio_write(tp, 0x19, 0x9df7);
        mdio_write(tp, 0x15, 0x03af);
        mdio_write(tp, 0x19, 0x7c03);
        mdio_write(tp, 0x15, 0x03b0);
        mdio_write(tp, 0x19, 0x6c08);
        mdio_write(tp, 0x15, 0x03b1);
        mdio_write(tp, 0x19, 0x33b6);
        mdio_write(tp, 0x15, 0x03b2);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x03b3);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x03b4);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x03b5);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x03b6);
        mdio_write(tp, 0x19, 0x55af);
        mdio_write(tp, 0x15, 0x03b7);
        mdio_write(tp, 0x19, 0x7ff0);
        mdio_write(tp, 0x15, 0x03b8);
        mdio_write(tp, 0x19, 0x6ff0);
        mdio_write(tp, 0x15, 0x03b9);
        mdio_write(tp, 0x19, 0x4bb9);
        mdio_write(tp, 0x15, 0x03ba);
        mdio_write(tp, 0x19, 0x6a80);
        mdio_write(tp, 0x15, 0x03bb);
        mdio_write(tp, 0x19, 0x4b29);
        mdio_write(tp, 0x15, 0x03bc);
        mdio_write(tp, 0x19, 0x4041);
        mdio_write(tp, 0x15, 0x03bd);
        mdio_write(tp, 0x19, 0x440a);
        mdio_write(tp, 0x15, 0x03be);
        mdio_write(tp, 0x19, 0x4029);
        mdio_write(tp, 0x15, 0x03bf);
        mdio_write(tp, 0x19, 0x4418);
        mdio_write(tp, 0x15, 0x03c0);
        mdio_write(tp, 0x19, 0x4090);
        mdio_write(tp, 0x15, 0x03c1);
        mdio_write(tp, 0x19, 0x4438);
        mdio_write(tp, 0x15, 0x03c2);
        mdio_write(tp, 0x19, 0x40c4);
        mdio_write(tp, 0x15, 0x03c3);
        mdio_write(tp, 0x19, 0x447b);
        mdio_write(tp, 0x15, 0x03c4);
        mdio_write(tp, 0x19, 0xb6c4);
        mdio_write(tp, 0x15, 0x03c5);
        mdio_write(tp, 0x19, 0x3311);
        mdio_write(tp, 0x15, 0x03c6);
        mdio_write(tp, 0x19, 0x9bfe);
        mdio_write(tp, 0x15, 0x03c7);
        mdio_write(tp, 0x19, 0x33cc);
        mdio_write(tp, 0x15, 0x03c8);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x03c9);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x03ca);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x03cb);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x03cc);
        mdio_write(tp, 0x19, 0x542f);
        mdio_write(tp, 0x15, 0x03cd);
        mdio_write(tp, 0x19, 0x499a);
        mdio_write(tp, 0x15, 0x03ce);
        mdio_write(tp, 0x19, 0x7c40);
        mdio_write(tp, 0x15, 0x03cf);
        mdio_write(tp, 0x19, 0x4c40);
        mdio_write(tp, 0x15, 0x03d0);
        mdio_write(tp, 0x19, 0x490a);
        mdio_write(tp, 0x15, 0x03d1);
        mdio_write(tp, 0x19, 0x405e);
        mdio_write(tp, 0x15, 0x03d2);
        mdio_write(tp, 0x19, 0x44f8);
        mdio_write(tp, 0x15, 0x03d3);
        mdio_write(tp, 0x19, 0x6b00);
        mdio_write(tp, 0x15, 0x03d4);
        mdio_write(tp, 0x19, 0xd64f);
        mdio_write(tp, 0x15, 0x03d5);
        mdio_write(tp, 0x19, 0x0028);
        mdio_write(tp, 0x15, 0x03d6);
        mdio_write(tp, 0x19, 0x3311);
        mdio_write(tp, 0x15, 0x03d7);
        mdio_write(tp, 0x19, 0xbd27);
        mdio_write(tp, 0x15, 0x03d8);
        mdio_write(tp, 0x19, 0x9cfc);
        mdio_write(tp, 0x15, 0x03d9);
        mdio_write(tp, 0x19, 0xc639);
        mdio_write(tp, 0x15, 0x03da);
        mdio_write(tp, 0x19, 0x000f);
        mdio_write(tp, 0x15, 0x03db);
        mdio_write(tp, 0x19, 0x9e03);
        mdio_write(tp, 0x15, 0x03dc);
        mdio_write(tp, 0x19, 0x7c01);
        mdio_write(tp, 0x15, 0x03dd);
        mdio_write(tp, 0x19, 0x4c01);
        mdio_write(tp, 0x15, 0x03de);
        mdio_write(tp, 0x19, 0x9af6);
        mdio_write(tp, 0x15, 0x03df);
        mdio_write(tp, 0x19, 0x7c12);
        mdio_write(tp, 0x15, 0x03e0);
        mdio_write(tp, 0x19, 0x4c52);
        mdio_write(tp, 0x15, 0x03e1);
        mdio_write(tp, 0x19, 0x4470);
        mdio_write(tp, 0x15, 0x03e2);
        mdio_write(tp, 0x19, 0x7c12);
        mdio_write(tp, 0x15, 0x03e3);
        mdio_write(tp, 0x19, 0x4c40);
        mdio_write(tp, 0x15, 0x03e4);
        mdio_write(tp, 0x19, 0x33d4);
        mdio_write(tp, 0x15, 0x03e5);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x03e6);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x03e7);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x03e8);
        mdio_write(tp, 0x19, 0x0000);
        mdio_write(tp, 0x15, 0x03e9);
        mdio_write(tp, 0x19, 0x49bb);
        mdio_write(tp, 0x15, 0x03ea);
        mdio_write(tp, 0x19, 0x4478);
        mdio_write(tp, 0x15, 0x03eb);
        mdio_write(tp, 0x19, 0x492b);
        mdio_write(tp, 0x15, 0x03ec);
        mdio_write(tp, 0x19, 0x6b80);
        mdio_write(tp, 0x15, 0x03ed);
        mdio_write(tp, 0x19, 0x7c01);
        mdio_write(tp, 0x15, 0x03ee);
        mdio_write(tp, 0x19, 0x4c00);
        mdio_write(tp, 0x15, 0x03ef);
        mdio_write(tp, 0x19, 0xd64f);
        mdio_write(tp, 0x15, 0x03f0);
        mdio_write(tp, 0x19, 0x000d);
        mdio_write(tp, 0x15, 0x03f1);
        mdio_write(tp, 0x19, 0x3311);
        mdio_write(tp, 0x15, 0x03f2);
        mdio_write(tp, 0x19, 0xbd0c);
        mdio_write(tp, 0x15, 0x03f3);
        mdio_write(tp, 0x19, 0xc428);
        mdio_write(tp, 0x15, 0x03f4);
        mdio_write(tp, 0x19, 0x0008);
        mdio_write(tp, 0x15, 0x03f5);
        mdio_write(tp, 0x19, 0x9afa);
        mdio_write(tp, 0x15, 0x03f6);
        mdio_write(tp, 0x19, 0x7c12);
        mdio_write(tp, 0x15, 0x03f7);
        mdio_write(tp, 0x19, 0x4c52);
        mdio_write(tp, 0x15, 0x03f8);
        mdio_write(tp, 0x19, 0x4470);
        mdio_write(tp, 0x15, 0x03f9);
        mdio_write(tp, 0x19, 0x7c12);
        mdio_write(tp, 0x15, 0x03fa);
        mdio_write(tp, 0x19, 0x4c40);
        mdio_write(tp, 0x15, 0x03fb);
        mdio_write(tp, 0x19, 0x33ef);
        mdio_write(tp, 0x15, 0x03fc);
        mdio_write(tp, 0x19, 0x3342);
        mdio_write(tp, 0x15, 0x03fd);
        mdio_write(tp, 0x19, 0x330d);
        mdio_write(tp, 0x15, 0x03fe);
        mdio_write(tp, 0x19, 0x32ae);
        mdio_write(tp, 0x15, 0x0000);
        mdio_write(tp, 0x16, 0x0306);
        mdio_write(tp, 0x16, 0x0300);
        mdio_write(tp, 0x1f, 0x0002);
        mdio_write(tp, 0x1f, 0x0000);
        mdio_write(tp, 0x1f, 0x0005);
        mdio_write(tp, 0x05, 0xfff6);
        mdio_write(tp, 0x06, 0x0080);
        mdio_write(tp, 0x05, 0x8000);
        mdio_write(tp, 0x06, 0x0280);
        mdio_write(tp, 0x06, 0x48f7);
        mdio_write(tp, 0x06, 0x00e0);
        mdio_write(tp, 0x06, 0xfff7);
        mdio_write(tp, 0x06, 0xa080);
        mdio_write(tp, 0x06, 0x02ae);
        mdio_write(tp, 0x06, 0xf602);
        mdio_write(tp, 0x06, 0x0112);
        mdio_write(tp, 0x06, 0x0201);
        mdio_write(tp, 0x06, 0x1f02);
        mdio_write(tp, 0x06, 0x012c);
        mdio_write(tp, 0x06, 0x0201);
        mdio_write(tp, 0x06, 0x3c02);
        mdio_write(tp, 0x06, 0x0156);
        mdio_write(tp, 0x06, 0x0201);
        mdio_write(tp, 0x06, 0x6d02);
        mdio_write(tp, 0x06, 0x809d);
        mdio_write(tp, 0x06, 0xe08b);
        mdio_write(tp, 0x06, 0x88e1);
        mdio_write(tp, 0x06, 0x8b89);
        mdio_write(tp, 0x06, 0x1e01);
        mdio_write(tp, 0x06, 0xe18b);
        mdio_write(tp, 0x06, 0x8a1e);
        mdio_write(tp, 0x06, 0x01e1);
        mdio_write(tp, 0x06, 0x8b8b);
        mdio_write(tp, 0x06, 0x1e01);
        mdio_write(tp, 0x06, 0xe18b);
        mdio_write(tp, 0x06, 0x8c1e);
        mdio_write(tp, 0x06, 0x01e1);
        mdio_write(tp, 0x06, 0x8b8d);
        mdio_write(tp, 0x06, 0x1e01);
        mdio_write(tp, 0x06, 0xe18b);
        mdio_write(tp, 0x06, 0x8e1e);
        mdio_write(tp, 0x06, 0x01a0);
        mdio_write(tp, 0x06, 0x00c7);
        mdio_write(tp, 0x06, 0xaebb);
        mdio_write(tp, 0x06, 0xd100);
        mdio_write(tp, 0x06, 0xbf82);
        mdio_write(tp, 0x06, 0xc702);
        mdio_write(tp, 0x06, 0x320a);
        mdio_write(tp, 0x06, 0xd105);
        mdio_write(tp, 0x06, 0xbf82);
        mdio_write(tp, 0x06, 0xcd02);
        mdio_write(tp, 0x06, 0x320a);
        mdio_write(tp, 0x06, 0xd100);
        mdio_write(tp, 0x06, 0xbf82);
        mdio_write(tp, 0x06, 0xca02);
        mdio_write(tp, 0x06, 0x320a);
        mdio_write(tp, 0x06, 0xd105);
        mdio_write(tp, 0x06, 0xbf82);
        mdio_write(tp, 0x06, 0xd002);
        mdio_write(tp, 0x06, 0x320a);
        mdio_write(tp, 0x06, 0xd481);
        mdio_write(tp, 0x06, 0xc9e4);
        mdio_write(tp, 0x06, 0x8b90);
        mdio_write(tp, 0x06, 0xe58b);
        mdio_write(tp, 0x06, 0x91d4);
        mdio_write(tp, 0x06, 0x81b8);
        mdio_write(tp, 0x06, 0xe48b);
        mdio_write(tp, 0x06, 0x92e5);
        mdio_write(tp, 0x06, 0x8b93);
        mdio_write(tp, 0x06, 0xbf8b);
        mdio_write(tp, 0x06, 0x88ec);
        mdio_write(tp, 0x06, 0x0019);
        mdio_write(tp, 0x06, 0xa98b);
        mdio_write(tp, 0x06, 0x90f9);
        mdio_write(tp, 0x06, 0xeeff);
        mdio_write(tp, 0x06, 0xf600);
        mdio_write(tp, 0x06, 0xeeff);
        mdio_write(tp, 0x06, 0xf7fc);
        mdio_write(tp, 0x06, 0xd100);
        mdio_write(tp, 0x06, 0xbf82);
        mdio_write(tp, 0x06, 0xc102);
        mdio_write(tp, 0x06, 0x320a);
        mdio_write(tp, 0x06, 0xd101);
        mdio_write(tp, 0x06, 0xbf82);
        mdio_write(tp, 0x06, 0xc402);
        mdio_write(tp, 0x06, 0x320a);
        mdio_write(tp, 0x06, 0x04f8);
        mdio_write(tp, 0x06, 0xe08b);
        mdio_write(tp, 0x06, 0x8ead);
        mdio_write(tp, 0x06, 0x201a);
        mdio_write(tp, 0x06, 0xf620);
        mdio_write(tp, 0x06, 0xe48b);
        mdio_write(tp, 0x06, 0x8e02);
        mdio_write(tp, 0x06, 0x824b);
        mdio_write(tp, 0x06, 0x0281);
        mdio_write(tp, 0x06, 0x1902);
        mdio_write(tp, 0x06, 0x2c9d);
        mdio_write(tp, 0x06, 0x0203);
        mdio_write(tp, 0x06, 0x9602);
        mdio_write(tp, 0x06, 0x0473);
        mdio_write(tp, 0x06, 0x022e);
        mdio_write(tp, 0x06, 0x3902);
        mdio_write(tp, 0x06, 0x044d);
        mdio_write(tp, 0x06, 0xe08b);
        mdio_write(tp, 0x06, 0x8ead);
        mdio_write(tp, 0x06, 0x210b);
        mdio_write(tp, 0x06, 0xf621);
        mdio_write(tp, 0x06, 0xe48b);
        mdio_write(tp, 0x06, 0x8e02);
        mdio_write(tp, 0x06, 0x0416);
        mdio_write(tp, 0x06, 0x021b);
        mdio_write(tp, 0x06, 0xa4e0);
        mdio_write(tp, 0x06, 0x8b8e);
        mdio_write(tp, 0x06, 0xad22);
        mdio_write(tp, 0x06, 0x05f6);
        mdio_write(tp, 0x06, 0x22e4);
        mdio_write(tp, 0x06, 0x8b8e);
        mdio_write(tp, 0x06, 0xe08b);
        mdio_write(tp, 0x06, 0x8ead);
        mdio_write(tp, 0x06, 0x2305);
        mdio_write(tp, 0x06, 0xf623);
        mdio_write(tp, 0x06, 0xe48b);
        mdio_write(tp, 0x06, 0x8ee0);
        mdio_write(tp, 0x06, 0x8b8e);
        mdio_write(tp, 0x06, 0xad24);
        mdio_write(tp, 0x06, 0x05f6);
        mdio_write(tp, 0x06, 0x24e4);
        mdio_write(tp, 0x06, 0x8b8e);
        mdio_write(tp, 0x06, 0xe08b);
        mdio_write(tp, 0x06, 0x8ead);
        mdio_write(tp, 0x06, 0x2505);
        mdio_write(tp, 0x06, 0xf625);
        mdio_write(tp, 0x06, 0xe48b);
        mdio_write(tp, 0x06, 0x8ee0);
        mdio_write(tp, 0x06, 0x8b8e);
        mdio_write(tp, 0x06, 0xad26);
        mdio_write(tp, 0x06, 0x08f6);
        mdio_write(tp, 0x06, 0x26e4);
        mdio_write(tp, 0x06, 0x8b8e);
        mdio_write(tp, 0x06, 0x0281);
        mdio_write(tp, 0x06, 0xdae0);
        mdio_write(tp, 0x06, 0x8b8e);
        mdio_write(tp, 0x06, 0xad27);
        mdio_write(tp, 0x06, 0x05f6);
        mdio_write(tp, 0x06, 0x27e4);
        mdio_write(tp, 0x06, 0x8b8e);
        mdio_write(tp, 0x06, 0x0203);
        mdio_write(tp, 0x06, 0x5cfc);
        mdio_write(tp, 0x06, 0x04f8);
        mdio_write(tp, 0x06, 0xfaef);
        mdio_write(tp, 0x06, 0x69e0);
        mdio_write(tp, 0x06, 0x8b85);
        mdio_write(tp, 0x06, 0xad21);
        mdio_write(tp, 0x06, 0x57e0);
        mdio_write(tp, 0x06, 0xe022);
        mdio_write(tp, 0x06, 0xe1e0);
        mdio_write(tp, 0x06, 0x2358);
        mdio_write(tp, 0x06, 0xc059);
        mdio_write(tp, 0x06, 0x021e);
        mdio_write(tp, 0x06, 0x01e1);
        mdio_write(tp, 0x06, 0x8b3c);
        mdio_write(tp, 0x06, 0x1f10);
        mdio_write(tp, 0x06, 0x9e44);
        mdio_write(tp, 0x06, 0xe48b);
        mdio_write(tp, 0x06, 0x3cad);
        mdio_write(tp, 0x06, 0x211d);
        mdio_write(tp, 0x06, 0xe18b);
        mdio_write(tp, 0x06, 0x84f7);
        mdio_write(tp, 0x06, 0x29e5);
        mdio_write(tp, 0x06, 0x8b84);
        mdio_write(tp, 0x06, 0xac27);
        mdio_write(tp, 0x06, 0x0dac);
        mdio_write(tp, 0x06, 0x2605);
        mdio_write(tp, 0x06, 0x0281);
        mdio_write(tp, 0x06, 0x7fae);
        mdio_write(tp, 0x06, 0x2b02);
        mdio_write(tp, 0x06, 0x2c23);
        mdio_write(tp, 0x06, 0xae26);
        mdio_write(tp, 0x06, 0x022c);
        mdio_write(tp, 0x06, 0x41ae);
        mdio_write(tp, 0x06, 0x21e0);
        mdio_write(tp, 0x06, 0x8b87);
        mdio_write(tp, 0x06, 0xad22);
        mdio_write(tp, 0x06, 0x18e0);
        mdio_write(tp, 0x06, 0xfff7);
        mdio_write(tp, 0x06, 0x58fc);
        mdio_write(tp, 0x06, 0xe4ff);
        mdio_write(tp, 0x06, 0xf7d1);
        mdio_write(tp, 0x06, 0x00bf);
        mdio_write(tp, 0x06, 0x2eee);
        mdio_write(tp, 0x06, 0x0232);
        mdio_write(tp, 0x06, 0x0ad1);
        mdio_write(tp, 0x06, 0x00bf);
        mdio_write(tp, 0x06, 0x82e8);
        mdio_write(tp, 0x06, 0x0232);
        mdio_write(tp, 0x06, 0x0a02);
        mdio_write(tp, 0x06, 0x2bdf);
        mdio_write(tp, 0x06, 0xef96);
        mdio_write(tp, 0x06, 0xfefc);
        mdio_write(tp, 0x06, 0x04d0);
        mdio_write(tp, 0x06, 0x0202);
        mdio_write(tp, 0x06, 0x1e97);
        mdio_write(tp, 0x06, 0xe08b);
        mdio_write(tp, 0x06, 0x87ad);
        mdio_write(tp, 0x06, 0x2228);
        mdio_write(tp, 0x06, 0xd100);
        mdio_write(tp, 0x06, 0xbf82);
        mdio_write(tp, 0x06, 0xd302);
        mdio_write(tp, 0x06, 0x320a);
        mdio_write(tp, 0x06, 0xd10c);
        mdio_write(tp, 0x06, 0xbf82);
        mdio_write(tp, 0x06, 0xd602);
        mdio_write(tp, 0x06, 0x320a);
        mdio_write(tp, 0x06, 0xd104);
        mdio_write(tp, 0x06, 0xbf82);
        mdio_write(tp, 0x06, 0xd902);
        mdio_write(tp, 0x06, 0x320a);
        mdio_write(tp, 0x06, 0xd101);
        mdio_write(tp, 0x06, 0xbf82);
        mdio_write(tp, 0x06, 0xe802);
        mdio_write(tp, 0x06, 0x320a);
        mdio_write(tp, 0x06, 0xe0ff);
        mdio_write(tp, 0x06, 0xf768);
        mdio_write(tp, 0x06, 0x03e4);
        mdio_write(tp, 0x06, 0xfff7);
        mdio_write(tp, 0x06, 0xd004);
        mdio_write(tp, 0x06, 0x0228);
        mdio_write(tp, 0x06, 0x7a04);
        mdio_write(tp, 0x06, 0xf8e0);
        mdio_write(tp, 0x06, 0xe234);
        mdio_write(tp, 0x06, 0xe1e2);
        mdio_write(tp, 0x06, 0x35f6);
        mdio_write(tp, 0x06, 0x2be4);
        mdio_write(tp, 0x06, 0xe234);
        mdio_write(tp, 0x06, 0xe5e2);
        mdio_write(tp, 0x06, 0x35fc);
        mdio_write(tp, 0x06, 0x05f8);
        mdio_write(tp, 0x06, 0xe0e2);
        mdio_write(tp, 0x06, 0x34e1);
        mdio_write(tp, 0x06, 0xe235);
        mdio_write(tp, 0x06, 0xf72b);
        mdio_write(tp, 0x06, 0xe4e2);
        mdio_write(tp, 0x06, 0x34e5);
        mdio_write(tp, 0x06, 0xe235);
        mdio_write(tp, 0x06, 0xfc05);
        mdio_write(tp, 0x06, 0xf8f9);
        mdio_write(tp, 0x06, 0xfaef);
        mdio_write(tp, 0x06, 0x69ac);
        mdio_write(tp, 0x06, 0x1b4c);
        mdio_write(tp, 0x06, 0xbf2e);
        mdio_write(tp, 0x06, 0x3002);
        mdio_write(tp, 0x06, 0x31dd);
        mdio_write(tp, 0x06, 0xef01);
        mdio_write(tp, 0x06, 0xe28a);
        mdio_write(tp, 0x06, 0x76e4);
        mdio_write(tp, 0x06, 0x8a76);
        mdio_write(tp, 0x06, 0x1f12);
        mdio_write(tp, 0x06, 0x9e3a);
        mdio_write(tp, 0x06, 0xef12);
        mdio_write(tp, 0x06, 0x5907);
        mdio_write(tp, 0x06, 0x9f12);
        mdio_write(tp, 0x06, 0xf8e0);
        mdio_write(tp, 0x06, 0x8b40);
        mdio_write(tp, 0x06, 0xf721);
        mdio_write(tp, 0x06, 0xe48b);
        mdio_write(tp, 0x06, 0x40d0);
        mdio_write(tp, 0x06, 0x0302);
        mdio_write(tp, 0x06, 0x287a);
        mdio_write(tp, 0x06, 0x0282);
        mdio_write(tp, 0x06, 0x34fc);
        mdio_write(tp, 0x06, 0xa000);
        mdio_write(tp, 0x06, 0x1002);
        mdio_write(tp, 0x06, 0x2dc3);
        mdio_write(tp, 0x06, 0x022e);
        mdio_write(tp, 0x06, 0x21e0);
        mdio_write(tp, 0x06, 0x8b40);
        mdio_write(tp, 0x06, 0xf621);
        mdio_write(tp, 0x06, 0xe48b);
        mdio_write(tp, 0x06, 0x40ae);
        mdio_write(tp, 0x06, 0x0fbf);
        mdio_write(tp, 0x06, 0x3fa5);
        mdio_write(tp, 0x06, 0x0231);
        mdio_write(tp, 0x06, 0x6cbf);
        mdio_write(tp, 0x06, 0x3fa2);
        mdio_write(tp, 0x06, 0x0231);
        mdio_write(tp, 0x06, 0x6c02);
        mdio_write(tp, 0x06, 0x2dc3);
        mdio_write(tp, 0x06, 0xef96);
        mdio_write(tp, 0x06, 0xfefd);
        mdio_write(tp, 0x06, 0xfc04);
        mdio_write(tp, 0x06, 0xf8e0);
        mdio_write(tp, 0x06, 0xe2f4);
        mdio_write(tp, 0x06, 0xe1e2);
        mdio_write(tp, 0x06, 0xf5e4);
        mdio_write(tp, 0x06, 0x8a78);
        mdio_write(tp, 0x06, 0xe58a);
        mdio_write(tp, 0x06, 0x79ee);
        mdio_write(tp, 0x06, 0xe2f4);
        mdio_write(tp, 0x06, 0xd8ee);
        mdio_write(tp, 0x06, 0xe2f5);
        mdio_write(tp, 0x06, 0x20fc);
        mdio_write(tp, 0x06, 0x04f8);
        mdio_write(tp, 0x06, 0xf9fa);
        mdio_write(tp, 0x06, 0xef69);
        mdio_write(tp, 0x06, 0xe08b);
        mdio_write(tp, 0x06, 0x87ad);
        mdio_write(tp, 0x06, 0x2065);
        mdio_write(tp, 0x06, 0xd200);
        mdio_write(tp, 0x06, 0xbf2e);
        mdio_write(tp, 0x06, 0xe802);
        mdio_write(tp, 0x06, 0x31dd);
        mdio_write(tp, 0x06, 0x1e21);
        mdio_write(tp, 0x06, 0xbf82);
        mdio_write(tp, 0x06, 0xdf02);
        mdio_write(tp, 0x06, 0x31dd);
        mdio_write(tp, 0x06, 0x0c11);
        mdio_write(tp, 0x06, 0x1e21);
        mdio_write(tp, 0x06, 0xbf82);
        mdio_write(tp, 0x06, 0xe202);
        mdio_write(tp, 0x06, 0x31dd);
        mdio_write(tp, 0x06, 0x0c12);
        mdio_write(tp, 0x06, 0x1e21);
        mdio_write(tp, 0x06, 0xbf82);
        mdio_write(tp, 0x06, 0xe502);
        mdio_write(tp, 0x06, 0x31dd);
        mdio_write(tp, 0x06, 0x0c13);
        mdio_write(tp, 0x06, 0x1e21);
        mdio_write(tp, 0x06, 0xbf1f);
        mdio_write(tp, 0x06, 0x5302);
        mdio_write(tp, 0x06, 0x31dd);
        mdio_write(tp, 0x06, 0x0c14);
        mdio_write(tp, 0x06, 0x1e21);
        mdio_write(tp, 0x06, 0xbf82);
        mdio_write(tp, 0x06, 0xeb02);
        mdio_write(tp, 0x06, 0x31dd);
        mdio_write(tp, 0x06, 0x0c16);
        mdio_write(tp, 0x06, 0x1e21);
        mdio_write(tp, 0x06, 0xe083);
        mdio_write(tp, 0x06, 0xe01f);
        mdio_write(tp, 0x06, 0x029e);
        mdio_write(tp, 0x06, 0x22e6);
        mdio_write(tp, 0x06, 0x83e0);
        mdio_write(tp, 0x06, 0xad31);
        mdio_write(tp, 0x06, 0x14ad);
        mdio_write(tp, 0x06, 0x3011);
        mdio_write(tp, 0x06, 0xef02);
        mdio_write(tp, 0x06, 0x580c);
        mdio_write(tp, 0x06, 0x9e07);
        mdio_write(tp, 0x06, 0xad36);
        mdio_write(tp, 0x06, 0x085a);
        mdio_write(tp, 0x06, 0x309f);
        mdio_write(tp, 0x06, 0x04d1);
        mdio_write(tp, 0x06, 0x01ae);
        mdio_write(tp, 0x06, 0x02d1);
        mdio_write(tp, 0x06, 0x00bf);
        mdio_write(tp, 0x06, 0x82dc);
        mdio_write(tp, 0x06, 0x0232);
        mdio_write(tp, 0x06, 0x0aef);
        mdio_write(tp, 0x06, 0x96fe);
        mdio_write(tp, 0x06, 0xfdfc);
        mdio_write(tp, 0x06, 0x0400);
        mdio_write(tp, 0x06, 0xe140);
        mdio_write(tp, 0x06, 0x77e1);
        mdio_write(tp, 0x06, 0x4010);
        mdio_write(tp, 0x06, 0xe150);
        mdio_write(tp, 0x06, 0x32e1);
        mdio_write(tp, 0x06, 0x5030);
        mdio_write(tp, 0x06, 0xe144);
        mdio_write(tp, 0x06, 0x74e1);
        mdio_write(tp, 0x06, 0x44bb);
        mdio_write(tp, 0x06, 0xe2d2);
        mdio_write(tp, 0x06, 0x40e0);
        mdio_write(tp, 0x06, 0x2cfc);
        mdio_write(tp, 0x06, 0xe2cc);
        mdio_write(tp, 0x06, 0xcce2);
        mdio_write(tp, 0x06, 0x00cc);
        mdio_write(tp, 0x06, 0xe000);
        mdio_write(tp, 0x06, 0x99e0);
        mdio_write(tp, 0x06, 0x3688);
        mdio_write(tp, 0x06, 0xe036);
        mdio_write(tp, 0x06, 0x99e1);
        mdio_write(tp, 0x06, 0x40dd);
        mdio_write(tp, 0x06, 0xe022);
        mdio_write(tp, 0x05, 0xe142);
        gphy_val = mdio_read(tp, 0x06);
        gphy_val |= BIT_0;
        mdio_write(tp, 0x06, gphy_val);
        mdio_write(tp, 0x05, 0xe140);
        gphy_val = mdio_read(tp, 0x06);
        gphy_val |= BIT_0;
        mdio_write(tp, 0x06, gphy_val);
        mdio_write(tp, 0x1f, 0x0000);
        mdio_write(tp, 0x1f, 0x0005);
        for (i = 0; i < 200; i++) {
                udelay(100);
                gphy_val = mdio_read(tp, 0x00);
                if (gphy_val & BIT_7)
                        break;
        }
        mdio_write(tp, 0x1f, 0x0004);
        mdio_write(tp, 0x1f, 0x0007);
        mdio_write(tp, 0x1e, 0x0023);
        gphy_val = mdio_read(tp, 0x17);
        gphy_val &= ~(BIT_0);
        gphy_val |= BIT_2;
        mdio_write(tp, 0x17, gphy_val);
        mdio_write(tp, 0x1f, 0x0002);
        mdio_write(tp, 0x1f, 0x0000);
}

static void
rtl8168_set_phy_mcu_8168evl_2(struct net_device *dev)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        struct pci_dev *pdev = tp->pci_dev;
        unsigned int gphy_val,i;

        mdio_write(tp, 0x1f, 0x0000);
        mdio_write(tp, 0x00, 0x1800);
        gphy_val = mdio_read(tp, 0x15);
        gphy_val &= ~(BIT_12);
        mdio_write(tp, 0x15, gphy_val);
        mdio_write(tp, 0x00, 0x4800);
        mdio_write(tp, 0x1f, 0x0007);
        mdio_write(tp, 0x1e, 0x002f);
        for (i = 0; i < 1000; i++) {
                udelay(100);
                gphy_val = mdio_read(tp, 0x1c);
                if ((gphy_val & 0x0080) == 0x0080)
                        break;
        }
        mdio_write(tp, 0x1f, 0x0000);
        mdio_write(tp, 0x00, 0x1800);
        mdio_write(tp, 0x1f, 0x0007);
        mdio_write(tp, 0x1e, 0x0023);
        for (i = 0; i < 200; i++) {
                udelay(100);
                gphy_val = mdio_read(tp, 0x17);
                if (!(gphy_val & 0x0001))
                        break;
        }
        mdio_write(tp, 0x1f, 0x0005);
        mdio_write(tp, 0x05, 0xfff6);
        mdio_write(tp, 0x06, 0x0080);
        mdio_write(tp, 0x1f, 0x0007);
        mdio_write(tp, 0x1e, 0x0023);
        mdio_write(tp, 0x16, 0x0306);
        mdio_write(tp, 0x16, 0x0307);
        mdio_write(tp, 0x15, 0x00AF);
        mdio_write(tp, 0x19, 0x4060);
        mdio_write(tp, 0x15, 0x00B0);
        mdio_write(tp, 0x19, 0x7800);
        mdio_write(tp, 0x15, 0x00B1);
        mdio_write(tp, 0x19, 0x7e00);
        mdio_write(tp, 0x15, 0x00B2);
        mdio_write(tp, 0x19, 0x72B0);
        mdio_write(tp, 0x15, 0x00B3);
        mdio_write(tp, 0x19, 0x7F00);
        mdio_write(tp, 0x15, 0x00B4);
        mdio_write(tp, 0x19, 0x73B0);
        mdio_write(tp, 0x15, 0x0101);
        mdio_write(tp, 0x19, 0x0005);
        mdio_write(tp, 0x15, 0x0103);
        mdio_write(tp, 0x19, 0x0003);
        mdio_write(tp, 0x15, 0x0105);
        mdio_write(tp, 0x19, 0x30FD);
        mdio_write(tp, 0x15, 0x0106);
        mdio_write(tp, 0x19, 0x9DF7);
        mdio_write(tp, 0x15, 0x0107);
        mdio_write(tp, 0x19, 0x30C6);
        mdio_write(tp, 0x15, 0x0098);
        mdio_write(tp, 0x19, 0x7c0b);
        mdio_write(tp, 0x15, 0x0099);
        mdio_write(tp, 0x19, 0x6c0b);
        mdio_write(tp, 0x15, 0x00eb);
        mdio_write(tp, 0x19, 0x6c0b);
        mdio_write(tp, 0x15, 0x00f8);
        mdio_write(tp, 0x19, 0x6f0b);
        mdio_write(tp, 0x15, 0x00fe);
        mdio_write(tp, 0x19, 0x6f0f);
        mdio_write(tp, 0x15, 0x00db);
        mdio_write(tp, 0x19, 0x6f09);
        mdio_write(tp, 0x15, 0x00dc);
        mdio_write(tp, 0x19, 0xaefd);
        mdio_write(tp, 0x15, 0x00dd);
        mdio_write(tp, 0x19, 0x6f0b);
        mdio_write(tp, 0x15, 0x00de);
        mdio_write(tp, 0x19, 0xc60b);
        mdio_write(tp, 0x15, 0x00df);
        mdio_write(tp, 0x19, 0x00fa);
        mdio_write(tp, 0x15, 0x00e0);
        mdio_write(tp, 0x19, 0x30e1);
        mdio_write(tp, 0x15, 0x020c);
        mdio_write(tp, 0x19, 0x3224);
        mdio_write(tp, 0x15, 0x020e);
        mdio_write(tp, 0x19, 0x9813);
        mdio_write(tp, 0x15, 0x020f);
        mdio_write(tp, 0x19, 0x7801);
        mdio_write(tp, 0x15, 0x0210);
        mdio_write(tp, 0x19, 0x930f);
        mdio_write(tp, 0x15, 0x0211);
        mdio_write(tp, 0x19, 0x9206);
        mdio_write(tp, 0x15, 0x0212);
        mdio_write(tp, 0x19, 0x4002);
        mdio_write(tp, 0x15, 0x0213);
        mdio_write(tp, 0x19, 0x7800);
        mdio_write(tp, 0x15, 0x0214);
        mdio_write(tp, 0x19, 0x588f);
        mdio_write(tp, 0x15, 0x0215);
        mdio_write(tp, 0x19, 0x5520);
        mdio_write(tp, 0x15, 0x0216);
        mdio_write(tp, 0x19, 0x3224);
        mdio_write(tp, 0x15, 0x0217);
        mdio_write(tp, 0x19, 0x4002);
        mdio_write(tp, 0x15, 0x0218);
        mdio_write(tp, 0x19, 0x7800);
        mdio_write(tp, 0x15, 0x0219);
        mdio_write(tp, 0x19, 0x588d);
        mdio_write(tp, 0x15, 0x021a);
        mdio_write(tp, 0x19, 0x5540);
        mdio_write(tp, 0x15, 0x021b);
        mdio_write(tp, 0x19, 0x9e03);
        mdio_write(tp, 0x15, 0x021c);
        mdio_write(tp, 0x19, 0x7c40);
        mdio_write(tp, 0x15, 0x021d);
        mdio_write(tp, 0x19, 0x6840);
        mdio_write(tp, 0x15, 0x021e);
        mdio_write(tp, 0x19, 0x3224);
        mdio_write(tp, 0x15, 0x021f);
        mdio_write(tp, 0x19, 0x4002);
        mdio_write(tp, 0x15, 0x0220);
        mdio_write(tp, 0x19, 0x3224);
        mdio_write(tp, 0x15, 0x0221);
        mdio_write(tp, 0x19, 0x9e03);
        mdio_write(tp, 0x15, 0x0222);
        mdio_write(tp, 0x19, 0x7c40);
        mdio_write(tp, 0x15, 0x0223);
        mdio_write(tp, 0x19, 0x6840);
        mdio_write(tp, 0x15, 0x0224);
        mdio_write(tp, 0x19, 0x7800);
        mdio_write(tp, 0x15, 0x0225);
        mdio_write(tp, 0x19, 0x3231);
        mdio_write(tp, 0x15, 0x0000);
        mdio_write(tp, 0x16, 0x0306);
        mdio_write(tp, 0x16, 0x0300);
        mdio_write(tp, 0x1f, 0x0002);
        mdio_write(tp, 0x1f, 0x0000);
        mdio_write(tp, 0x1f, 0x0000);
        mdio_write(tp, 0x17, 0x2160);
        mdio_write(tp, 0x1f, 0x0007);
        mdio_write(tp, 0x1e, 0x0040);
        mdio_write(tp, 0x18, 0x0004);
        if (pdev->subsystem_vendor == 0x144d &&
            pdev->subsystem_device == 0xc0a6) {
                mdio_write(tp, 0x18, 0x0724);
                mdio_write(tp, 0x19, 0xfe00);
                mdio_write(tp, 0x18, 0x0734);
                mdio_write(tp, 0x19, 0xfd00);
                mdio_write(tp, 0x18, 0x1824);
                mdio_write(tp, 0x19, 0xfc00);
                mdio_write(tp, 0x18, 0x1834);
                mdio_write(tp, 0x19, 0xfd00);
        }
        mdio_write(tp, 0x18, 0x09d4);
        mdio_write(tp, 0x19, 0x4000);
        mdio_write(tp, 0x18, 0x09e4);
        mdio_write(tp, 0x19, 0x0800);
        mdio_write(tp, 0x18, 0x09f4);
        mdio_write(tp, 0x19, 0xff00);
        mdio_write(tp, 0x18, 0x0a04);
        mdio_write(tp, 0x19, 0x4000);
        mdio_write(tp, 0x18, 0x0a14);
        mdio_write(tp, 0x19, 0x0c00);
        mdio_write(tp, 0x18, 0x0a24);
        mdio_write(tp, 0x19, 0xff00);
        mdio_write(tp, 0x18, 0x0a74);
        mdio_write(tp, 0x19, 0xf600);
        mdio_write(tp, 0x18, 0x1a24);
        mdio_write(tp, 0x19, 0x7d00);
        mdio_write(tp, 0x18, 0x1a64);
        mdio_write(tp, 0x19, 0x0500);
        mdio_write(tp, 0x18, 0x1a74);
        mdio_write(tp, 0x19, 0x9500);
        mdio_write(tp, 0x18, 0x1a84);
        mdio_write(tp, 0x19, 0x8000);
        mdio_write(tp, 0x18, 0x1a94);
        mdio_write(tp, 0x19, 0x7d00);
        mdio_write(tp, 0x18, 0x1aa4);
        mdio_write(tp, 0x19, 0x9600);
        mdio_write(tp, 0x18, 0x1ac4);
        mdio_write(tp, 0x19, 0x4000);
        mdio_write(tp, 0x18, 0x1ad4);
        mdio_write(tp, 0x19, 0x0800);
        mdio_write(tp, 0x18, 0x1af4);
        mdio_write(tp, 0x19, 0xc400);
        mdio_write(tp, 0x18, 0x1b04);
        mdio_write(tp, 0x19, 0x4000);
        mdio_write(tp, 0x18, 0x1b14);
        mdio_write(tp, 0x19, 0x0800);
        mdio_write(tp, 0x18, 0x1b24);
        mdio_write(tp, 0x19, 0xfd00);
        mdio_write(tp, 0x18, 0x1b34);
        mdio_write(tp, 0x19, 0x4000);
        mdio_write(tp, 0x18, 0x1b44);
        mdio_write(tp, 0x19, 0x0400);
        mdio_write(tp, 0x18, 0x1b94);
        mdio_write(tp, 0x19, 0xf100);
        mdio_write(tp, 0x1f, 0x0000);
        mdio_write(tp, 0x17, 0x2100);
        mdio_write(tp, 0x1f, 0x0007);
        mdio_write(tp, 0x1e, 0x0040);
        mdio_write(tp, 0x18, 0x0000);
        mdio_write(tp, 0x1f, 0x0000);
        mdio_write(tp, 0x1f, 0x0005);
        mdio_write(tp, 0x05, 0xfff6);
        mdio_write(tp, 0x06, 0x0080);
        mdio_write(tp, 0x05, 0x8000);
        mdio_write(tp, 0x06, 0x0280);
        mdio_write(tp, 0x06, 0x48f7);
        mdio_write(tp, 0x06, 0x00e0);
        mdio_write(tp, 0x06, 0xfff7);
        mdio_write(tp, 0x06, 0xa080);
        mdio_write(tp, 0x06, 0x02ae);
        mdio_write(tp, 0x06, 0xf602);
        mdio_write(tp, 0x06, 0x0115);
        mdio_write(tp, 0x06, 0x0201);
        mdio_write(tp, 0x06, 0x2202);
        mdio_write(tp, 0x06, 0x80a0);
        mdio_write(tp, 0x06, 0x0201);
        mdio_write(tp, 0x06, 0x3f02);
        mdio_write(tp, 0x06, 0x0159);
        mdio_write(tp, 0x06, 0x0280);
        mdio_write(tp, 0x06, 0xbd02);
        mdio_write(tp, 0x06, 0x80da);
        mdio_write(tp, 0x06, 0xe08b);
        mdio_write(tp, 0x06, 0x88e1);
        mdio_write(tp, 0x06, 0x8b89);
        mdio_write(tp, 0x06, 0x1e01);
        mdio_write(tp, 0x06, 0xe18b);
        mdio_write(tp, 0x06, 0x8a1e);
        mdio_write(tp, 0x06, 0x01e1);
        mdio_write(tp, 0x06, 0x8b8b);
        mdio_write(tp, 0x06, 0x1e01);
        mdio_write(tp, 0x06, 0xe18b);
        mdio_write(tp, 0x06, 0x8c1e);
        mdio_write(tp, 0x06, 0x01e1);
        mdio_write(tp, 0x06, 0x8b8d);
        mdio_write(tp, 0x06, 0x1e01);
        mdio_write(tp, 0x06, 0xe18b);
        mdio_write(tp, 0x06, 0x8e1e);
        mdio_write(tp, 0x06, 0x01a0);
        mdio_write(tp, 0x06, 0x00c7);
        mdio_write(tp, 0x06, 0xaebb);
        mdio_write(tp, 0x06, 0xd481);
        mdio_write(tp, 0x06, 0xd2e4);
        mdio_write(tp, 0x06, 0x8b92);
        mdio_write(tp, 0x06, 0xe58b);
        mdio_write(tp, 0x06, 0x93d1);
        mdio_write(tp, 0x06, 0x03bf);
        mdio_write(tp, 0x06, 0x859e);
        mdio_write(tp, 0x06, 0x0237);
        mdio_write(tp, 0x06, 0x23d1);
        mdio_write(tp, 0x06, 0x02bf);
        mdio_write(tp, 0x06, 0x85a1);
        mdio_write(tp, 0x06, 0x0237);
        mdio_write(tp, 0x06, 0x23ee);
        mdio_write(tp, 0x06, 0x8608);
        mdio_write(tp, 0x06, 0x03ee);
        mdio_write(tp, 0x06, 0x860a);
        mdio_write(tp, 0x06, 0x60ee);
        mdio_write(tp, 0x06, 0x8610);
        mdio_write(tp, 0x06, 0x00ee);
        mdio_write(tp, 0x06, 0x8611);
        mdio_write(tp, 0x06, 0x00ee);
        mdio_write(tp, 0x06, 0x8abe);
        mdio_write(tp, 0x06, 0x07ee);
        mdio_write(tp, 0x06, 0x8abf);
        mdio_write(tp, 0x06, 0x73ee);
        mdio_write(tp, 0x06, 0x8a95);
        mdio_write(tp, 0x06, 0x02bf);
        mdio_write(tp, 0x06, 0x8b88);
        mdio_write(tp, 0x06, 0xec00);
        mdio_write(tp, 0x06, 0x19a9);
        mdio_write(tp, 0x06, 0x8b90);
        mdio_write(tp, 0x06, 0xf9ee);
        mdio_write(tp, 0x06, 0xfff6);
        mdio_write(tp, 0x06, 0x00ee);
        mdio_write(tp, 0x06, 0xfff7);
        mdio_write(tp, 0x06, 0xfed1);
        mdio_write(tp, 0x06, 0x00bf);
        mdio_write(tp, 0x06, 0x8595);
        mdio_write(tp, 0x06, 0x0237);
        mdio_write(tp, 0x06, 0x23d1);
        mdio_write(tp, 0x06, 0x01bf);
        mdio_write(tp, 0x06, 0x8598);
        mdio_write(tp, 0x06, 0x0237);
        mdio_write(tp, 0x06, 0x2304);
        mdio_write(tp, 0x06, 0xf8e0);
        mdio_write(tp, 0x06, 0x8b8a);
        mdio_write(tp, 0x06, 0xad20);
        mdio_write(tp, 0x06, 0x14ee);
        mdio_write(tp, 0x06, 0x8b8a);
        mdio_write(tp, 0x06, 0x0002);
        mdio_write(tp, 0x06, 0x1f9a);
        mdio_write(tp, 0x06, 0xe0e4);
        mdio_write(tp, 0x06, 0x26e1);
        mdio_write(tp, 0x06, 0xe427);
        mdio_write(tp, 0x06, 0xeee4);
        mdio_write(tp, 0x06, 0x2623);
        mdio_write(tp, 0x06, 0xe5e4);
        mdio_write(tp, 0x06, 0x27fc);
        mdio_write(tp, 0x06, 0x04f8);
        mdio_write(tp, 0x06, 0xe08b);
        mdio_write(tp, 0x06, 0x8dad);
        mdio_write(tp, 0x06, 0x2014);
        mdio_write(tp, 0x06, 0xee8b);
        mdio_write(tp, 0x06, 0x8d00);
        mdio_write(tp, 0x06, 0xe08a);
        mdio_write(tp, 0x06, 0x5a78);
        mdio_write(tp, 0x06, 0x039e);
        mdio_write(tp, 0x06, 0x0902);
        mdio_write(tp, 0x06, 0x05db);
        mdio_write(tp, 0x06, 0x0282);
        mdio_write(tp, 0x06, 0x7b02);
        mdio_write(tp, 0x06, 0x3231);
        mdio_write(tp, 0x06, 0xfc04);
        mdio_write(tp, 0x06, 0xf8e0);
        mdio_write(tp, 0x06, 0x8b8e);
        mdio_write(tp, 0x06, 0xad20);
        mdio_write(tp, 0x06, 0x1df6);
        mdio_write(tp, 0x06, 0x20e4);
        mdio_write(tp, 0x06, 0x8b8e);
        mdio_write(tp, 0x06, 0x0281);
        mdio_write(tp, 0x06, 0x5c02);
        mdio_write(tp, 0x06, 0x2bcb);
        mdio_write(tp, 0x06, 0x022d);
        mdio_write(tp, 0x06, 0x2902);
        mdio_write(tp, 0x06, 0x03b4);
        mdio_write(tp, 0x06, 0x0285);
        mdio_write(tp, 0x06, 0x6402);
        mdio_write(tp, 0x06, 0x2eca);
        mdio_write(tp, 0x06, 0x0284);
        mdio_write(tp, 0x06, 0xcd02);
        mdio_write(tp, 0x06, 0x046f);
        mdio_write(tp, 0x06, 0xe08b);
        mdio_write(tp, 0x06, 0x8ead);
        mdio_write(tp, 0x06, 0x210b);
        mdio_write(tp, 0x06, 0xf621);
        mdio_write(tp, 0x06, 0xe48b);
        mdio_write(tp, 0x06, 0x8e02);
        mdio_write(tp, 0x06, 0x8520);
        mdio_write(tp, 0x06, 0x021b);
        mdio_write(tp, 0x06, 0xe8e0);
        mdio_write(tp, 0x06, 0x8b8e);
        mdio_write(tp, 0x06, 0xad22);
        mdio_write(tp, 0x06, 0x05f6);
        mdio_write(tp, 0x06, 0x22e4);
        mdio_write(tp, 0x06, 0x8b8e);
        mdio_write(tp, 0x06, 0xe08b);
        mdio_write(tp, 0x06, 0x8ead);
        mdio_write(tp, 0x06, 0x2308);
        mdio_write(tp, 0x06, 0xf623);
        mdio_write(tp, 0x06, 0xe48b);
        mdio_write(tp, 0x06, 0x8e02);
        mdio_write(tp, 0x06, 0x311c);
        mdio_write(tp, 0x06, 0xe08b);
        mdio_write(tp, 0x06, 0x8ead);
        mdio_write(tp, 0x06, 0x2405);
        mdio_write(tp, 0x06, 0xf624);
        mdio_write(tp, 0x06, 0xe48b);
        mdio_write(tp, 0x06, 0x8ee0);
        mdio_write(tp, 0x06, 0x8b8e);
        mdio_write(tp, 0x06, 0xad25);
        mdio_write(tp, 0x06, 0x05f6);
        mdio_write(tp, 0x06, 0x25e4);
        mdio_write(tp, 0x06, 0x8b8e);
        mdio_write(tp, 0x06, 0xe08b);
        mdio_write(tp, 0x06, 0x8ead);
        mdio_write(tp, 0x06, 0x2608);
        mdio_write(tp, 0x06, 0xf626);
        mdio_write(tp, 0x06, 0xe48b);
        mdio_write(tp, 0x06, 0x8e02);
        mdio_write(tp, 0x06, 0x2df5);
        mdio_write(tp, 0x06, 0xe08b);
        mdio_write(tp, 0x06, 0x8ead);
        mdio_write(tp, 0x06, 0x2705);
        mdio_write(tp, 0x06, 0xf627);
        mdio_write(tp, 0x06, 0xe48b);
        mdio_write(tp, 0x06, 0x8e02);
        mdio_write(tp, 0x06, 0x037a);
        mdio_write(tp, 0x06, 0xfc04);
        mdio_write(tp, 0x06, 0xf8f9);
        mdio_write(tp, 0x06, 0xfaef);
        mdio_write(tp, 0x06, 0x69e0);
        mdio_write(tp, 0x06, 0x8b87);
        mdio_write(tp, 0x06, 0xad20);
        mdio_write(tp, 0x06, 0x65d2);
        mdio_write(tp, 0x06, 0x00bf);
        mdio_write(tp, 0x06, 0x2fe9);
        mdio_write(tp, 0x06, 0x0236);
        mdio_write(tp, 0x06, 0xf61e);
        mdio_write(tp, 0x06, 0x21bf);
        mdio_write(tp, 0x06, 0x2ff5);
        mdio_write(tp, 0x06, 0x0236);
        mdio_write(tp, 0x06, 0xf60c);
        mdio_write(tp, 0x06, 0x111e);
        mdio_write(tp, 0x06, 0x21bf);
        mdio_write(tp, 0x06, 0x2ff8);
        mdio_write(tp, 0x06, 0x0236);
        mdio_write(tp, 0x06, 0xf60c);
        mdio_write(tp, 0x06, 0x121e);
        mdio_write(tp, 0x06, 0x21bf);
        mdio_write(tp, 0x06, 0x2ffb);
        mdio_write(tp, 0x06, 0x0236);
        mdio_write(tp, 0x06, 0xf60c);
        mdio_write(tp, 0x06, 0x131e);
        mdio_write(tp, 0x06, 0x21bf);
        mdio_write(tp, 0x06, 0x1f97);
        mdio_write(tp, 0x06, 0x0236);
        mdio_write(tp, 0x06, 0xf60c);
        mdio_write(tp, 0x06, 0x141e);
        mdio_write(tp, 0x06, 0x21bf);
        mdio_write(tp, 0x06, 0x859b);
        mdio_write(tp, 0x06, 0x0236);
        mdio_write(tp, 0x06, 0xf60c);
        mdio_write(tp, 0x06, 0x161e);
        mdio_write(tp, 0x06, 0x21e0);
        mdio_write(tp, 0x06, 0x8a8c);
        mdio_write(tp, 0x06, 0x1f02);
        mdio_write(tp, 0x06, 0x9e22);
        mdio_write(tp, 0x06, 0xe68a);
        mdio_write(tp, 0x06, 0x8cad);
        mdio_write(tp, 0x06, 0x3114);
        mdio_write(tp, 0x06, 0xad30);
        mdio_write(tp, 0x06, 0x11ef);
        mdio_write(tp, 0x06, 0x0258);
        mdio_write(tp, 0x06, 0x0c9e);
        mdio_write(tp, 0x06, 0x07ad);
        mdio_write(tp, 0x06, 0x3608);
        mdio_write(tp, 0x06, 0x5a30);
        mdio_write(tp, 0x06, 0x9f04);
        mdio_write(tp, 0x06, 0xd101);
        mdio_write(tp, 0x06, 0xae02);
        mdio_write(tp, 0x06, 0xd100);
        mdio_write(tp, 0x06, 0xbf2f);
        mdio_write(tp, 0x06, 0xf202);
        mdio_write(tp, 0x06, 0x3723);
        mdio_write(tp, 0x06, 0xef96);
        mdio_write(tp, 0x06, 0xfefd);
        mdio_write(tp, 0x06, 0xfc04);
        mdio_write(tp, 0x06, 0xf8f9);
        mdio_write(tp, 0x06, 0xface);
        mdio_write(tp, 0x06, 0xfaef);
        mdio_write(tp, 0x06, 0x69fa);
        mdio_write(tp, 0x06, 0xd401);
        mdio_write(tp, 0x06, 0x55b4);
        mdio_write(tp, 0x06, 0xfebf);
        mdio_write(tp, 0x06, 0x85a7);
        mdio_write(tp, 0x06, 0x0236);
        mdio_write(tp, 0x06, 0xf6ac);
        mdio_write(tp, 0x06, 0x280b);
        mdio_write(tp, 0x06, 0xbf85);
        mdio_write(tp, 0x06, 0xa402);
        mdio_write(tp, 0x06, 0x36f6);
        mdio_write(tp, 0x06, 0xac28);
        mdio_write(tp, 0x06, 0x49ae);
        mdio_write(tp, 0x06, 0x64bf);
        mdio_write(tp, 0x06, 0x85a4);
        mdio_write(tp, 0x06, 0x0236);
        mdio_write(tp, 0x06, 0xf6ac);
        mdio_write(tp, 0x06, 0x285b);
        mdio_write(tp, 0x06, 0xd000);
        mdio_write(tp, 0x06, 0x0282);
        mdio_write(tp, 0x06, 0x60ac);
        mdio_write(tp, 0x06, 0x2105);
        mdio_write(tp, 0x06, 0xac22);
        mdio_write(tp, 0x06, 0x02ae);
        mdio_write(tp, 0x06, 0x4ebf);
        mdio_write(tp, 0x06, 0xe0c4);
        mdio_write(tp, 0x06, 0xbe86);
        mdio_write(tp, 0x06, 0x14d2);
        mdio_write(tp, 0x06, 0x04d8);
        mdio_write(tp, 0x06, 0x19d9);
        mdio_write(tp, 0x06, 0x1907);
        mdio_write(tp, 0x06, 0xdc19);
        mdio_write(tp, 0x06, 0xdd19);
        mdio_write(tp, 0x06, 0x0789);
        mdio_write(tp, 0x06, 0x89ef);
        mdio_write(tp, 0x06, 0x645e);
        mdio_write(tp, 0x06, 0x07ff);
        mdio_write(tp, 0x06, 0x0d65);
        mdio_write(tp, 0x06, 0x5cf8);
        mdio_write(tp, 0x06, 0x001e);
        mdio_write(tp, 0x06, 0x46dc);
        mdio_write(tp, 0x06, 0x19dd);
        mdio_write(tp, 0x06, 0x19b2);
        mdio_write(tp, 0x06, 0xe2d4);
        mdio_write(tp, 0x06, 0x0001);
        mdio_write(tp, 0x06, 0xbf85);
        mdio_write(tp, 0x06, 0xa402);
        mdio_write(tp, 0x06, 0x3723);
        mdio_write(tp, 0x06, 0xae1d);
        mdio_write(tp, 0x06, 0xbee0);
        mdio_write(tp, 0x06, 0xc4bf);
        mdio_write(tp, 0x06, 0x8614);
        mdio_write(tp, 0x06, 0xd204);
        mdio_write(tp, 0x06, 0xd819);
        mdio_write(tp, 0x06, 0xd919);
        mdio_write(tp, 0x06, 0x07dc);
        mdio_write(tp, 0x06, 0x19dd);
        mdio_write(tp, 0x06, 0x1907);
        mdio_write(tp, 0x06, 0xb2f4);
        mdio_write(tp, 0x06, 0xd400);
        mdio_write(tp, 0x06, 0x00bf);
        mdio_write(tp, 0x06, 0x85a4);
        mdio_write(tp, 0x06, 0x0237);
        mdio_write(tp, 0x06, 0x23fe);
        mdio_write(tp, 0x06, 0xef96);
        mdio_write(tp, 0x06, 0xfec6);
        mdio_write(tp, 0x06, 0xfefd);
        mdio_write(tp, 0x06, 0xfc05);
        mdio_write(tp, 0x06, 0xf9e2);
        mdio_write(tp, 0x06, 0xe0ea);
        mdio_write(tp, 0x06, 0xe3e0);
        mdio_write(tp, 0x06, 0xeb5a);
        mdio_write(tp, 0x06, 0x070c);
        mdio_write(tp, 0x06, 0x031e);
        mdio_write(tp, 0x06, 0x20e6);
        mdio_write(tp, 0x06, 0xe0ea);
        mdio_write(tp, 0x06, 0xe7e0);
        mdio_write(tp, 0x06, 0xebe0);
        mdio_write(tp, 0x06, 0xe0fc);
        mdio_write(tp, 0x06, 0xe1e0);
        mdio_write(tp, 0x06, 0xfdfd);
        mdio_write(tp, 0x06, 0x04f8);
        mdio_write(tp, 0x06, 0xf9e0);
        mdio_write(tp, 0x06, 0x8b81);
        mdio_write(tp, 0x06, 0xac26);
        mdio_write(tp, 0x06, 0x1ae0);
        mdio_write(tp, 0x06, 0x8b81);
        mdio_write(tp, 0x06, 0xac21);
        mdio_write(tp, 0x06, 0x14e0);
        mdio_write(tp, 0x06, 0x8b85);
        mdio_write(tp, 0x06, 0xac20);
        mdio_write(tp, 0x06, 0x0ee0);
        mdio_write(tp, 0x06, 0x8b85);
        mdio_write(tp, 0x06, 0xac23);
        mdio_write(tp, 0x06, 0x08e0);
        mdio_write(tp, 0x06, 0x8b87);
        mdio_write(tp, 0x06, 0xac24);
        mdio_write(tp, 0x06, 0x02ae);
        mdio_write(tp, 0x06, 0x3802);
        mdio_write(tp, 0x06, 0x1ab5);
        mdio_write(tp, 0x06, 0xeee4);
        mdio_write(tp, 0x06, 0x1c04);
        mdio_write(tp, 0x06, 0xeee4);
        mdio_write(tp, 0x06, 0x1d04);
        mdio_write(tp, 0x06, 0xe2e0);
        mdio_write(tp, 0x06, 0x7ce3);
        mdio_write(tp, 0x06, 0xe07d);
        mdio_write(tp, 0x06, 0xe0e0);
        mdio_write(tp, 0x06, 0x38e1);
        mdio_write(tp, 0x06, 0xe039);
        mdio_write(tp, 0x06, 0xad2e);
        mdio_write(tp, 0x06, 0x1bad);
        mdio_write(tp, 0x06, 0x390d);
        mdio_write(tp, 0x06, 0xd101);
        mdio_write(tp, 0x06, 0xbf21);
        mdio_write(tp, 0x06, 0xd502);
        mdio_write(tp, 0x06, 0x3723);
        mdio_write(tp, 0x06, 0x0282);
        mdio_write(tp, 0x06, 0xd8ae);
        mdio_write(tp, 0x06, 0x0bac);
        mdio_write(tp, 0x06, 0x3802);
        mdio_write(tp, 0x06, 0xae06);
        mdio_write(tp, 0x06, 0x0283);
        mdio_write(tp, 0x06, 0x1802);
        mdio_write(tp, 0x06, 0x8360);
        mdio_write(tp, 0x06, 0x021a);
        mdio_write(tp, 0x06, 0xc6fd);
        mdio_write(tp, 0x06, 0xfc04);
        mdio_write(tp, 0x06, 0xf8e1);
        mdio_write(tp, 0x06, 0x8af4);
        mdio_write(tp, 0x06, 0xe08b);
        mdio_write(tp, 0x06, 0x81ad);
        mdio_write(tp, 0x06, 0x2605);
        mdio_write(tp, 0x06, 0x0222);
        mdio_write(tp, 0x06, 0xa4f7);
        mdio_write(tp, 0x06, 0x28e0);
        mdio_write(tp, 0x06, 0x8b81);
        mdio_write(tp, 0x06, 0xad21);
        mdio_write(tp, 0x06, 0x0502);
        mdio_write(tp, 0x06, 0x23a9);
        mdio_write(tp, 0x06, 0xf729);
        mdio_write(tp, 0x06, 0xe08b);
        mdio_write(tp, 0x06, 0x85ad);
        mdio_write(tp, 0x06, 0x2005);
        mdio_write(tp, 0x06, 0x0214);
        mdio_write(tp, 0x06, 0xabf7);
        mdio_write(tp, 0x06, 0x2ae0);
        mdio_write(tp, 0x06, 0x8b85);
        mdio_write(tp, 0x06, 0xad23);
        mdio_write(tp, 0x06, 0x0502);
        mdio_write(tp, 0x06, 0x12e7);
        mdio_write(tp, 0x06, 0xf72b);
        mdio_write(tp, 0x06, 0xe08b);
        mdio_write(tp, 0x06, 0x87ad);
        mdio_write(tp, 0x06, 0x2405);
        mdio_write(tp, 0x06, 0x0283);
        mdio_write(tp, 0x06, 0xbcf7);
        mdio_write(tp, 0x06, 0x2ce5);
        mdio_write(tp, 0x06, 0x8af4);
        mdio_write(tp, 0x06, 0xfc04);
        mdio_write(tp, 0x06, 0xf8e0);
        mdio_write(tp, 0x06, 0x8b81);
        mdio_write(tp, 0x06, 0xad26);
        mdio_write(tp, 0x06, 0x0302);
        mdio_write(tp, 0x06, 0x21e5);
        mdio_write(tp, 0x06, 0xe08b);
        mdio_write(tp, 0x06, 0x81ad);
        mdio_write(tp, 0x06, 0x2109);
        mdio_write(tp, 0x06, 0xe08a);
        mdio_write(tp, 0x06, 0xf4ac);
        mdio_write(tp, 0x06, 0x2003);
        mdio_write(tp, 0x06, 0x0223);
        mdio_write(tp, 0x06, 0x98e0);
        mdio_write(tp, 0x06, 0x8b85);
        mdio_write(tp, 0x06, 0xad20);
        mdio_write(tp, 0x06, 0x09e0);
        mdio_write(tp, 0x06, 0x8af4);
        mdio_write(tp, 0x06, 0xac21);
        mdio_write(tp, 0x06, 0x0302);
        mdio_write(tp, 0x06, 0x13fb);
        mdio_write(tp, 0x06, 0xe08b);
        mdio_write(tp, 0x06, 0x85ad);
        mdio_write(tp, 0x06, 0x2309);
        mdio_write(tp, 0x06, 0xe08a);
        mdio_write(tp, 0x06, 0xf4ac);
        mdio_write(tp, 0x06, 0x2203);
        mdio_write(tp, 0x06, 0x0212);
        mdio_write(tp, 0x06, 0xfae0);
        mdio_write(tp, 0x06, 0x8b87);
        mdio_write(tp, 0x06, 0xad24);
        mdio_write(tp, 0x06, 0x09e0);
        mdio_write(tp, 0x06, 0x8af4);
        mdio_write(tp, 0x06, 0xac23);
        mdio_write(tp, 0x06, 0x0302);
        mdio_write(tp, 0x06, 0x83c1);
        mdio_write(tp, 0x06, 0xfc04);
        mdio_write(tp, 0x06, 0xf8e1);
        mdio_write(tp, 0x06, 0x8af4);
        mdio_write(tp, 0x06, 0xe08b);
        mdio_write(tp, 0x06, 0x81ad);
        mdio_write(tp, 0x06, 0x2608);
        mdio_write(tp, 0x06, 0xe083);
        mdio_write(tp, 0x06, 0xd2ad);
        mdio_write(tp, 0x06, 0x2502);
        mdio_write(tp, 0x06, 0xf628);
        mdio_write(tp, 0x06, 0xe08b);
        mdio_write(tp, 0x06, 0x81ad);
        mdio_write(tp, 0x06, 0x210a);
        mdio_write(tp, 0x06, 0xe084);
        mdio_write(tp, 0x06, 0x0af6);
        mdio_write(tp, 0x06, 0x27a0);
        mdio_write(tp, 0x06, 0x0502);
        mdio_write(tp, 0x06, 0xf629);
        mdio_write(tp, 0x06, 0xe08b);
        mdio_write(tp, 0x06, 0x85ad);
        mdio_write(tp, 0x06, 0x2008);
        mdio_write(tp, 0x06, 0xe08a);
        mdio_write(tp, 0x06, 0xe8ad);
        mdio_write(tp, 0x06, 0x2102);
        mdio_write(tp, 0x06, 0xf62a);
        mdio_write(tp, 0x06, 0xe08b);
        mdio_write(tp, 0x06, 0x85ad);
        mdio_write(tp, 0x06, 0x2308);
        mdio_write(tp, 0x06, 0xe08b);
        mdio_write(tp, 0x06, 0x20a0);
        mdio_write(tp, 0x06, 0x0302);
        mdio_write(tp, 0x06, 0xf62b);
        mdio_write(tp, 0x06, 0xe08b);
        mdio_write(tp, 0x06, 0x87ad);
        mdio_write(tp, 0x06, 0x2408);
        mdio_write(tp, 0x06, 0xe086);
        mdio_write(tp, 0x06, 0x02a0);
        mdio_write(tp, 0x06, 0x0302);
        mdio_write(tp, 0x06, 0xf62c);
        mdio_write(tp, 0x06, 0xe58a);
        mdio_write(tp, 0x06, 0xf4a1);
        mdio_write(tp, 0x06, 0x0008);
        mdio_write(tp, 0x06, 0xd100);
        mdio_write(tp, 0x06, 0xbf21);
        mdio_write(tp, 0x06, 0xd502);
        mdio_write(tp, 0x06, 0x3723);
        mdio_write(tp, 0x06, 0xfc04);
        mdio_write(tp, 0x06, 0xee86);
        mdio_write(tp, 0x06, 0x0200);
        mdio_write(tp, 0x06, 0x04f8);
        mdio_write(tp, 0x06, 0xe08b);
        mdio_write(tp, 0x06, 0x87ad);
        mdio_write(tp, 0x06, 0x241e);
        mdio_write(tp, 0x06, 0xe086);
        mdio_write(tp, 0x06, 0x02a0);
        mdio_write(tp, 0x06, 0x0005);
        mdio_write(tp, 0x06, 0x0283);
        mdio_write(tp, 0x06, 0xe8ae);
        mdio_write(tp, 0x06, 0xf5a0);
        mdio_write(tp, 0x06, 0x0105);
        mdio_write(tp, 0x06, 0x0283);
        mdio_write(tp, 0x06, 0xf8ae);
        mdio_write(tp, 0x06, 0x0ba0);
        mdio_write(tp, 0x06, 0x0205);
        mdio_write(tp, 0x06, 0x0284);
        mdio_write(tp, 0x06, 0x14ae);
        mdio_write(tp, 0x06, 0x03a0);
        mdio_write(tp, 0x06, 0x0300);
        mdio_write(tp, 0x06, 0xfc04);
        mdio_write(tp, 0x06, 0xf8fa);
        mdio_write(tp, 0x06, 0xef69);
        mdio_write(tp, 0x06, 0x0284);
        mdio_write(tp, 0x06, 0x2bee);
        mdio_write(tp, 0x06, 0x8602);
        mdio_write(tp, 0x06, 0x01ef);
        mdio_write(tp, 0x06, 0x96fe);
        mdio_write(tp, 0x06, 0xfc04);
        mdio_write(tp, 0x06, 0xf8ee);
        mdio_write(tp, 0x06, 0x8609);
        mdio_write(tp, 0x06, 0x0002);
        mdio_write(tp, 0x06, 0x8461);
        mdio_write(tp, 0x06, 0xfc04);
        mdio_write(tp, 0x06, 0xae10);
        mdio_write(tp, 0x06, 0x0000);
        mdio_write(tp, 0x06, 0x0000);
        mdio_write(tp, 0x06, 0x0000);
        mdio_write(tp, 0x06, 0x0000);
        mdio_write(tp, 0x06, 0x0000);
        mdio_write(tp, 0x06, 0x0000);
        mdio_write(tp, 0x06, 0x0000);
        mdio_write(tp, 0x06, 0x0000);
        mdio_write(tp, 0x06, 0xf8e0);
        mdio_write(tp, 0x06, 0x8608);
        mdio_write(tp, 0x06, 0xe186);
        mdio_write(tp, 0x06, 0x091f);
        mdio_write(tp, 0x06, 0x019e);
        mdio_write(tp, 0x06, 0x0611);
        mdio_write(tp, 0x06, 0xe586);
        mdio_write(tp, 0x06, 0x09ae);
        mdio_write(tp, 0x06, 0x04ee);
        mdio_write(tp, 0x06, 0x8602);
        mdio_write(tp, 0x06, 0x01fc);
        mdio_write(tp, 0x06, 0x04f8);
        mdio_write(tp, 0x06, 0xf9fa);
        mdio_write(tp, 0x06, 0xef69);
        mdio_write(tp, 0x06, 0xfbbf);
        mdio_write(tp, 0x06, 0x8604);
        mdio_write(tp, 0x06, 0xef79);
        mdio_write(tp, 0x06, 0xd200);
        mdio_write(tp, 0x06, 0xd400);
        mdio_write(tp, 0x06, 0x221e);
        mdio_write(tp, 0x06, 0x02bf);
        mdio_write(tp, 0x06, 0x2fec);
        mdio_write(tp, 0x06, 0x0237);
        mdio_write(tp, 0x06, 0x23bf);
        mdio_write(tp, 0x06, 0x13f2);
        mdio_write(tp, 0x06, 0x0236);
        mdio_write(tp, 0x06, 0xf60d);
        mdio_write(tp, 0x06, 0x4559);
        mdio_write(tp, 0x06, 0x1fef);
        mdio_write(tp, 0x06, 0x97dd);
        mdio_write(tp, 0x06, 0xd308);
        mdio_write(tp, 0x06, 0x1a93);
        mdio_write(tp, 0x06, 0xdd12);
        mdio_write(tp, 0x06, 0x17a2);
        mdio_write(tp, 0x06, 0x04de);
        mdio_write(tp, 0x06, 0xffef);
        mdio_write(tp, 0x06, 0x96fe);
        mdio_write(tp, 0x06, 0xfdfc);
        mdio_write(tp, 0x06, 0x04f8);
        mdio_write(tp, 0x06, 0xf9fa);
        mdio_write(tp, 0x06, 0xef69);
        mdio_write(tp, 0x06, 0xfbee);
        mdio_write(tp, 0x06, 0x8602);
        mdio_write(tp, 0x06, 0x03d5);
        mdio_write(tp, 0x06, 0x0080);
        mdio_write(tp, 0x06, 0xbf86);
        mdio_write(tp, 0x06, 0x04ef);
        mdio_write(tp, 0x06, 0x79ef);
        mdio_write(tp, 0x06, 0x45bf);
        mdio_write(tp, 0x06, 0x2fec);
        mdio_write(tp, 0x06, 0x0237);
        mdio_write(tp, 0x06, 0x23bf);
        mdio_write(tp, 0x06, 0x13f2);
        mdio_write(tp, 0x06, 0x0236);
        mdio_write(tp, 0x06, 0xf6ad);
        mdio_write(tp, 0x06, 0x2702);
        mdio_write(tp, 0x06, 0x78ff);
        mdio_write(tp, 0x06, 0xe186);
        mdio_write(tp, 0x06, 0x0a1b);
        mdio_write(tp, 0x06, 0x01aa);
        mdio_write(tp, 0x06, 0x2eef);
        mdio_write(tp, 0x06, 0x97d9);
        mdio_write(tp, 0x06, 0x7900);
        mdio_write(tp, 0x06, 0x9e2b);
        mdio_write(tp, 0x06, 0x81dd);
        mdio_write(tp, 0x06, 0xbf85);
        mdio_write(tp, 0x06, 0xad02);
        mdio_write(tp, 0x06, 0x3723);
        mdio_write(tp, 0x06, 0xd101);
        mdio_write(tp, 0x06, 0xef02);
        mdio_write(tp, 0x06, 0x100c);
        mdio_write(tp, 0x06, 0x11b0);
        mdio_write(tp, 0x06, 0xfc0d);
        mdio_write(tp, 0x06, 0x11bf);
        mdio_write(tp, 0x06, 0x85aa);
        mdio_write(tp, 0x06, 0x0237);
        mdio_write(tp, 0x06, 0x23d1);
        mdio_write(tp, 0x06, 0x00bf);
        mdio_write(tp, 0x06, 0x85aa);
        mdio_write(tp, 0x06, 0x0237);
        mdio_write(tp, 0x06, 0x23ee);
        mdio_write(tp, 0x06, 0x8602);
        mdio_write(tp, 0x06, 0x02ae);
        mdio_write(tp, 0x06, 0x0413);
        mdio_write(tp, 0x06, 0xa38b);
        mdio_write(tp, 0x06, 0xb4d3);
        mdio_write(tp, 0x06, 0x8012);
        mdio_write(tp, 0x06, 0x17a2);
        mdio_write(tp, 0x06, 0x04ad);
        mdio_write(tp, 0x06, 0xffef);
        mdio_write(tp, 0x06, 0x96fe);
        mdio_write(tp, 0x06, 0xfdfc);
        mdio_write(tp, 0x06, 0x04f8);
        mdio_write(tp, 0x06, 0xf9e0);
        mdio_write(tp, 0x06, 0x8b85);
        mdio_write(tp, 0x06, 0xad25);
        mdio_write(tp, 0x06, 0x48e0);
        mdio_write(tp, 0x06, 0x8a96);
        mdio_write(tp, 0x06, 0xe18a);
        mdio_write(tp, 0x06, 0x977c);
        mdio_write(tp, 0x06, 0x0000);
        mdio_write(tp, 0x06, 0x9e35);
        mdio_write(tp, 0x06, 0xee8a);
        mdio_write(tp, 0x06, 0x9600);
        mdio_write(tp, 0x06, 0xee8a);
        mdio_write(tp, 0x06, 0x9700);
        mdio_write(tp, 0x06, 0xe08a);
        mdio_write(tp, 0x06, 0xbee1);
        mdio_write(tp, 0x06, 0x8abf);
        mdio_write(tp, 0x06, 0xe286);
        mdio_write(tp, 0x06, 0x10e3);
        mdio_write(tp, 0x06, 0x8611);
        mdio_write(tp, 0x06, 0x0236);
        mdio_write(tp, 0x06, 0x1aad);
        mdio_write(tp, 0x06, 0x2012);
        mdio_write(tp, 0x06, 0xee8a);
        mdio_write(tp, 0x06, 0x9603);
        mdio_write(tp, 0x06, 0xee8a);
        mdio_write(tp, 0x06, 0x97b7);
        mdio_write(tp, 0x06, 0xee86);
        mdio_write(tp, 0x06, 0x1000);
        mdio_write(tp, 0x06, 0xee86);
        mdio_write(tp, 0x06, 0x1100);
        mdio_write(tp, 0x06, 0xae11);
        mdio_write(tp, 0x06, 0x15e6);
        mdio_write(tp, 0x06, 0x8610);
        mdio_write(tp, 0x06, 0xe786);
        mdio_write(tp, 0x06, 0x11ae);
        mdio_write(tp, 0x06, 0x08ee);
        mdio_write(tp, 0x06, 0x8610);
        mdio_write(tp, 0x06, 0x00ee);
        mdio_write(tp, 0x06, 0x8611);
        mdio_write(tp, 0x06, 0x00fd);
        mdio_write(tp, 0x06, 0xfc04);
        mdio_write(tp, 0x06, 0xf8fa);
        mdio_write(tp, 0x06, 0xef69);
        mdio_write(tp, 0x06, 0xe0e0);
        mdio_write(tp, 0x06, 0x00e1);
        mdio_write(tp, 0x06, 0xe001);
        mdio_write(tp, 0x06, 0xad27);
        mdio_write(tp, 0x06, 0x32e0);
        mdio_write(tp, 0x06, 0x8b40);
        mdio_write(tp, 0x06, 0xf720);
        mdio_write(tp, 0x06, 0xe48b);
        mdio_write(tp, 0x06, 0x40bf);
        mdio_write(tp, 0x06, 0x31f5);
        mdio_write(tp, 0x06, 0x0236);
        mdio_write(tp, 0x06, 0xf6ad);
        mdio_write(tp, 0x06, 0x2821);
        mdio_write(tp, 0x06, 0xe0e0);
        mdio_write(tp, 0x06, 0x20e1);
        mdio_write(tp, 0x06, 0xe021);
        mdio_write(tp, 0x06, 0xad20);
        mdio_write(tp, 0x06, 0x18e0);
        mdio_write(tp, 0x06, 0x8b40);
        mdio_write(tp, 0x06, 0xf620);
        mdio_write(tp, 0x06, 0xe48b);
        mdio_write(tp, 0x06, 0x40ee);
        mdio_write(tp, 0x06, 0x8b3b);
        mdio_write(tp, 0x06, 0xffe0);
        mdio_write(tp, 0x06, 0x8a8a);
        mdio_write(tp, 0x06, 0xe18a);
        mdio_write(tp, 0x06, 0x8be4);
        mdio_write(tp, 0x06, 0xe000);
        mdio_write(tp, 0x06, 0xe5e0);
        mdio_write(tp, 0x06, 0x01ef);
        mdio_write(tp, 0x06, 0x96fe);
        mdio_write(tp, 0x06, 0xfc04);
        mdio_write(tp, 0x06, 0xf8fa);
        mdio_write(tp, 0x06, 0xef69);
        mdio_write(tp, 0x06, 0xe08b);
        mdio_write(tp, 0x06, 0x80ad);
        mdio_write(tp, 0x06, 0x2722);
        mdio_write(tp, 0x06, 0xbf44);
        mdio_write(tp, 0x06, 0xfc02);
        mdio_write(tp, 0x06, 0x36f6);
        mdio_write(tp, 0x06, 0xe08b);
        mdio_write(tp, 0x06, 0x441f);
        mdio_write(tp, 0x06, 0x019e);
        mdio_write(tp, 0x06, 0x15e5);
        mdio_write(tp, 0x06, 0x8b44);
        mdio_write(tp, 0x06, 0xad29);
        mdio_write(tp, 0x06, 0x07ac);
        mdio_write(tp, 0x06, 0x2804);
        mdio_write(tp, 0x06, 0xd101);
        mdio_write(tp, 0x06, 0xae02);
        mdio_write(tp, 0x06, 0xd100);
        mdio_write(tp, 0x06, 0xbf85);
        mdio_write(tp, 0x06, 0xb002);
        mdio_write(tp, 0x06, 0x3723);
        mdio_write(tp, 0x06, 0xef96);
        mdio_write(tp, 0x06, 0xfefc);
        mdio_write(tp, 0x06, 0x0400);
        mdio_write(tp, 0x06, 0xe140);
        mdio_write(tp, 0x06, 0x77e1);
        mdio_write(tp, 0x06, 0x40dd);
        mdio_write(tp, 0x06, 0xe022);
        mdio_write(tp, 0x06, 0x32e1);
        mdio_write(tp, 0x06, 0x5074);
        mdio_write(tp, 0x06, 0xe144);
        mdio_write(tp, 0x06, 0xffe0);
        mdio_write(tp, 0x06, 0xdaff);
        mdio_write(tp, 0x06, 0xe0c0);
        mdio_write(tp, 0x06, 0x52e0);
        mdio_write(tp, 0x06, 0xeed9);
        mdio_write(tp, 0x06, 0xe04c);
        mdio_write(tp, 0x06, 0xbbe0);
        mdio_write(tp, 0x06, 0x2a00);
        mdio_write(tp, 0x05, 0xe142);
        gphy_val = mdio_read(tp, 0x06);
        gphy_val |= BIT_0;
        mdio_write(tp, 0x06, gphy_val);
        mdio_write(tp, 0x05, 0xe140);
        gphy_val = mdio_read(tp, 0x06);
        gphy_val |= BIT_0;
        mdio_write(tp, 0x06, gphy_val);
        mdio_write(tp, 0x1f, 0x0000);
        mdio_write(tp, 0x1f, 0x0005);
        for (i = 0; i < 200; i++) {
                udelay(100);
                gphy_val = mdio_read(tp, 0x00);
                if (gphy_val & BIT_7)
                        break;
        }
        mdio_write(tp, 0x1f, 0x0007);
        mdio_write(tp, 0x1e, 0x0042);
        mdio_write(tp, 0x18, 0x2300);
        mdio_write(tp, 0x1f, 0x0000);
        mdio_write(tp, 0x1f, 0x0007);
        mdio_write(tp, 0x1e, 0x0023);
        if (tp->RequiredSecLanDonglePatch) {
                gphy_val = mdio_read(tp, 0x17);
                gphy_val &= ~BIT_2;
                mdio_write(tp, 0x17, gphy_val);
        }

        mdio_write(tp, 0x1f, 0x0000);
        mdio_write(tp, 0x00, 0x9200);
}

static void
rtl8168_set_phy_mcu_8168f_1(struct net_device *dev)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        unsigned int gphy_val,i;

        mdio_write(tp,0x1f, 0x0000);
        mdio_write(tp,0x00, 0x1800);
        gphy_val = mdio_read(tp, 0x15);
        gphy_val &= ~(BIT_12);
        mdio_write(tp,0x15, gphy_val);
        mdio_write(tp,0x00, 0x4800);
        mdio_write(tp,0x1f, 0x0007);
        mdio_write(tp,0x1e, 0x002f);
        for (i = 0; i < 1000; i++) {
                udelay(100);
                gphy_val = mdio_read(tp, 0x1c);
                if (gphy_val & 0x0080)
                        break;
        }
        mdio_write(tp,0x1f, 0x0000);
        mdio_write(tp,0x00, 0x1800);
        mdio_write(tp,0x1f, 0x0007);
        mdio_write(tp,0x1e, 0x0023);
        for (i = 0; i < 200; i++) {
                udelay(100);
                gphy_val = mdio_read(tp, 0x18);
                if (!(gphy_val & 0x0001))
                        break;
        }
        mdio_write(tp, 0x1f, 0x0005);
        mdio_write(tp, 0x05, 0xfff6);
        mdio_write(tp, 0x06, 0x0080);
        mdio_write(tp, 0x1f, 0x0007);
        mdio_write(tp, 0x1e, 0x0023);
        mdio_write(tp, 0x16, 0x0306);
        mdio_write(tp, 0x16, 0x0307);
        mdio_write(tp, 0x15, 0x0194);
        mdio_write(tp, 0x19, 0x407D);
        mdio_write(tp, 0x15, 0x0098);
        mdio_write(tp, 0x19, 0x7c0b);
        mdio_write(tp, 0x15, 0x0099);
        mdio_write(tp, 0x19, 0x6c0b);
        mdio_write(tp, 0x15, 0x00eb);
        mdio_write(tp, 0x19, 0x6c0b);
        mdio_write(tp, 0x15, 0x00f8);
        mdio_write(tp, 0x19, 0x6f0b);
        mdio_write(tp, 0x15, 0x00fe);
        mdio_write(tp, 0x19, 0x6f0f);
        mdio_write(tp, 0x15, 0x00db);
        mdio_write(tp, 0x19, 0x6f09);
        mdio_write(tp, 0x15, 0x00dc);
        mdio_write(tp, 0x19, 0xaefd);
        mdio_write(tp, 0x15, 0x00dd);
        mdio_write(tp, 0x19, 0x6f0b);
        mdio_write(tp, 0x15, 0x00de);
        mdio_write(tp, 0x19, 0xc60b);
        mdio_write(tp, 0x15, 0x00df);
        mdio_write(tp, 0x19, 0x00fa);
        mdio_write(tp, 0x15, 0x00e0);
        mdio_write(tp, 0x19, 0x30e1);
        mdio_write(tp, 0x15, 0x020c);
        mdio_write(tp, 0x19, 0x3224);
        mdio_write(tp, 0x15, 0x020e);
        mdio_write(tp, 0x19, 0x9813);
        mdio_write(tp, 0x15, 0x020f);
        mdio_write(tp, 0x19, 0x7801);
        mdio_write(tp, 0x15, 0x0210);
        mdio_write(tp, 0x19, 0x930f);
        mdio_write(tp, 0x15, 0x0211);
        mdio_write(tp, 0x19, 0x9206);
        mdio_write(tp, 0x15, 0x0212);
        mdio_write(tp, 0x19, 0x4002);
        mdio_write(tp, 0x15, 0x0213);
        mdio_write(tp, 0x19, 0x7800);
        mdio_write(tp, 0x15, 0x0214);
        mdio_write(tp, 0x19, 0x588f);
        mdio_write(tp, 0x15, 0x0215);
        mdio_write(tp, 0x19, 0x5520);
        mdio_write(tp, 0x15, 0x0216);
        mdio_write(tp, 0x19, 0x3224);
        mdio_write(tp, 0x15, 0x0217);
        mdio_write(tp, 0x19, 0x4002);
        mdio_write(tp, 0x15, 0x0218);
        mdio_write(tp, 0x19, 0x7800);
        mdio_write(tp, 0x15, 0x0219);
        mdio_write(tp, 0x19, 0x588d);
        mdio_write(tp, 0x15, 0x021a);
        mdio_write(tp, 0x19, 0x5540);
        mdio_write(tp, 0x15, 0x021b);
        mdio_write(tp, 0x19, 0x9e03);
        mdio_write(tp, 0x15, 0x021c);
        mdio_write(tp, 0x19, 0x7c40);
        mdio_write(tp, 0x15, 0x021d);
        mdio_write(tp, 0x19, 0x6840);
        mdio_write(tp, 0x15, 0x021e);
        mdio_write(tp, 0x19, 0x3224);
        mdio_write(tp, 0x15, 0x021f);
        mdio_write(tp, 0x19, 0x4002);
        mdio_write(tp, 0x15, 0x0220);
        mdio_write(tp, 0x19, 0x3224);
        mdio_write(tp, 0x15, 0x0221);
        mdio_write(tp, 0x19, 0x9e03);
        mdio_write(tp, 0x15, 0x0222);
        mdio_write(tp, 0x19, 0x7c40);
        mdio_write(tp, 0x15, 0x0223);
        mdio_write(tp, 0x19, 0x6840);
        mdio_write(tp, 0x15, 0x0224);
        mdio_write(tp, 0x19, 0x7800);
        mdio_write(tp, 0x15, 0x0225);
        mdio_write(tp, 0x19, 0x3231);
        mdio_write(tp, 0x15, 0x0000);
        mdio_write(tp, 0x16, 0x0306);
        mdio_write(tp, 0x16, 0x0300);
        mdio_write(tp, 0x1f, 0x0000);
        mdio_write(tp, 0x1f, 0x0005);
        mdio_write(tp, 0x05, 0xfff6);
        mdio_write(tp, 0x06, 0x0080);
        mdio_write(tp, 0x05, 0x8000);
        mdio_write(tp, 0x06, 0x0280);
        mdio_write(tp, 0x06, 0x48f7);
        mdio_write(tp, 0x06, 0x00e0);
        mdio_write(tp, 0x06, 0xfff7);
        mdio_write(tp, 0x06, 0xa080);
        mdio_write(tp, 0x06, 0x02ae);
        mdio_write(tp, 0x06, 0xf602);
        mdio_write(tp, 0x06, 0x0118);
        mdio_write(tp, 0x06, 0x0201);
        mdio_write(tp, 0x06, 0x2502);
        mdio_write(tp, 0x06, 0x8090);
        mdio_write(tp, 0x06, 0x0201);
        mdio_write(tp, 0x06, 0x4202);
        mdio_write(tp, 0x06, 0x015c);
        mdio_write(tp, 0x06, 0x0280);
        mdio_write(tp, 0x06, 0xad02);
        mdio_write(tp, 0x06, 0x80ca);
        mdio_write(tp, 0x06, 0xe08b);
        mdio_write(tp, 0x06, 0x88e1);
        mdio_write(tp, 0x06, 0x8b89);
        mdio_write(tp, 0x06, 0x1e01);
        mdio_write(tp, 0x06, 0xe18b);
        mdio_write(tp, 0x06, 0x8a1e);
        mdio_write(tp, 0x06, 0x01e1);
        mdio_write(tp, 0x06, 0x8b8b);
        mdio_write(tp, 0x06, 0x1e01);
        mdio_write(tp, 0x06, 0xe18b);
        mdio_write(tp, 0x06, 0x8c1e);
        mdio_write(tp, 0x06, 0x01e1);
        mdio_write(tp, 0x06, 0x8b8d);
        mdio_write(tp, 0x06, 0x1e01);
        mdio_write(tp, 0x06, 0xe18b);
        mdio_write(tp, 0x06, 0x8e1e);
        mdio_write(tp, 0x06, 0x01a0);
        mdio_write(tp, 0x06, 0x00c7);
        mdio_write(tp, 0x06, 0xaebb);
        mdio_write(tp, 0x06, 0xd484);
        mdio_write(tp, 0x06, 0x3ce4);
        mdio_write(tp, 0x06, 0x8b92);
        mdio_write(tp, 0x06, 0xe58b);
        mdio_write(tp, 0x06, 0x93ee);
        mdio_write(tp, 0x06, 0x8ac8);
        mdio_write(tp, 0x06, 0x03ee);
        mdio_write(tp, 0x06, 0x8aca);
        mdio_write(tp, 0x06, 0x60ee);
        mdio_write(tp, 0x06, 0x8ac0);
        mdio_write(tp, 0x06, 0x00ee);
        mdio_write(tp, 0x06, 0x8ac1);
        mdio_write(tp, 0x06, 0x00ee);
        mdio_write(tp, 0x06, 0x8abe);
        mdio_write(tp, 0x06, 0x07ee);
        mdio_write(tp, 0x06, 0x8abf);
        mdio_write(tp, 0x06, 0x73ee);
        mdio_write(tp, 0x06, 0x8a95);
        mdio_write(tp, 0x06, 0x02bf);
        mdio_write(tp, 0x06, 0x8b88);
        mdio_write(tp, 0x06, 0xec00);
        mdio_write(tp, 0x06, 0x19a9);
        mdio_write(tp, 0x06, 0x8b90);
        mdio_write(tp, 0x06, 0xf9ee);
        mdio_write(tp, 0x06, 0xfff6);
        mdio_write(tp, 0x06, 0x00ee);
        mdio_write(tp, 0x06, 0xfff7);
        mdio_write(tp, 0x06, 0xfed1);
        mdio_write(tp, 0x06, 0x00bf);
        mdio_write(tp, 0x06, 0x85a4);
        mdio_write(tp, 0x06, 0x0238);
        mdio_write(tp, 0x06, 0x7dd1);
        mdio_write(tp, 0x06, 0x01bf);
        mdio_write(tp, 0x06, 0x85a7);
        mdio_write(tp, 0x06, 0x0238);
        mdio_write(tp, 0x06, 0x7d04);
        mdio_write(tp, 0x06, 0xf8e0);
        mdio_write(tp, 0x06, 0x8b8a);
        mdio_write(tp, 0x06, 0xad20);
        mdio_write(tp, 0x06, 0x14ee);
        mdio_write(tp, 0x06, 0x8b8a);
        mdio_write(tp, 0x06, 0x0002);
        mdio_write(tp, 0x06, 0x204b);
        mdio_write(tp, 0x06, 0xe0e4);
        mdio_write(tp, 0x06, 0x26e1);
        mdio_write(tp, 0x06, 0xe427);
        mdio_write(tp, 0x06, 0xeee4);
        mdio_write(tp, 0x06, 0x2623);
        mdio_write(tp, 0x06, 0xe5e4);
        mdio_write(tp, 0x06, 0x27fc);
        mdio_write(tp, 0x06, 0x04f8);
        mdio_write(tp, 0x06, 0xe08b);
        mdio_write(tp, 0x06, 0x8dad);
        mdio_write(tp, 0x06, 0x2014);
        mdio_write(tp, 0x06, 0xee8b);
        mdio_write(tp, 0x06, 0x8d00);
        mdio_write(tp, 0x06, 0xe08a);
        mdio_write(tp, 0x06, 0x5a78);
        mdio_write(tp, 0x06, 0x039e);
        mdio_write(tp, 0x06, 0x0902);
        mdio_write(tp, 0x06, 0x05e8);
        mdio_write(tp, 0x06, 0x0281);
        mdio_write(tp, 0x06, 0x4f02);
        mdio_write(tp, 0x06, 0x326c);
        mdio_write(tp, 0x06, 0xfc04);
        mdio_write(tp, 0x06, 0xf8e0);
        mdio_write(tp, 0x06, 0x8b8e);
        mdio_write(tp, 0x06, 0xad20);
        mdio_write(tp, 0x06, 0x1df6);
        mdio_write(tp, 0x06, 0x20e4);
        mdio_write(tp, 0x06, 0x8b8e);
        mdio_write(tp, 0x06, 0x022f);
        mdio_write(tp, 0x06, 0x0902);
        mdio_write(tp, 0x06, 0x2ab0);
        mdio_write(tp, 0x06, 0x0285);
        mdio_write(tp, 0x06, 0x1602);
        mdio_write(tp, 0x06, 0x03ba);
        mdio_write(tp, 0x06, 0x0284);
        mdio_write(tp, 0x06, 0xe502);
        mdio_write(tp, 0x06, 0x2df1);
        mdio_write(tp, 0x06, 0x0283);
        mdio_write(tp, 0x06, 0x8302);
        mdio_write(tp, 0x06, 0x0475);
        mdio_write(tp, 0x06, 0xe08b);
        mdio_write(tp, 0x06, 0x8ead);
        mdio_write(tp, 0x06, 0x210b);
        mdio_write(tp, 0x06, 0xf621);
        mdio_write(tp, 0x06, 0xe48b);
        mdio_write(tp, 0x06, 0x8e02);
        mdio_write(tp, 0x06, 0x83f8);
        mdio_write(tp, 0x06, 0x021c);
        mdio_write(tp, 0x06, 0x99e0);
        mdio_write(tp, 0x06, 0x8b8e);
        mdio_write(tp, 0x06, 0xad22);
        mdio_write(tp, 0x06, 0x08f6);
        mdio_write(tp, 0x06, 0x22e4);
        mdio_write(tp, 0x06, 0x8b8e);
        mdio_write(tp, 0x06, 0x0235);
        mdio_write(tp, 0x06, 0x63e0);
        mdio_write(tp, 0x06, 0x8b8e);
        mdio_write(tp, 0x06, 0xad23);
        mdio_write(tp, 0x06, 0x08f6);
        mdio_write(tp, 0x06, 0x23e4);
        mdio_write(tp, 0x06, 0x8b8e);
        mdio_write(tp, 0x06, 0x0231);
        mdio_write(tp, 0x06, 0x57e0);
        mdio_write(tp, 0x06, 0x8b8e);
        mdio_write(tp, 0x06, 0xad24);
        mdio_write(tp, 0x06, 0x05f6);
        mdio_write(tp, 0x06, 0x24e4);
        mdio_write(tp, 0x06, 0x8b8e);
        mdio_write(tp, 0x06, 0xe08b);
        mdio_write(tp, 0x06, 0x8ead);
        mdio_write(tp, 0x06, 0x2505);
        mdio_write(tp, 0x06, 0xf625);
        mdio_write(tp, 0x06, 0xe48b);
        mdio_write(tp, 0x06, 0x8ee0);
        mdio_write(tp, 0x06, 0x8b8e);
        mdio_write(tp, 0x06, 0xad26);
        mdio_write(tp, 0x06, 0x08f6);
        mdio_write(tp, 0x06, 0x26e4);
        mdio_write(tp, 0x06, 0x8b8e);
        mdio_write(tp, 0x06, 0x022d);
        mdio_write(tp, 0x06, 0x1ce0);
        mdio_write(tp, 0x06, 0x8b8e);
        mdio_write(tp, 0x06, 0xad27);
        mdio_write(tp, 0x06, 0x05f6);
        mdio_write(tp, 0x06, 0x27e4);
        mdio_write(tp, 0x06, 0x8b8e);
        mdio_write(tp, 0x06, 0x0203);
        mdio_write(tp, 0x06, 0x80fc);
        mdio_write(tp, 0x06, 0x04f8);
        mdio_write(tp, 0x06, 0xf9e0);
        mdio_write(tp, 0x06, 0x8b81);
        mdio_write(tp, 0x06, 0xac26);
        mdio_write(tp, 0x06, 0x1ae0);
        mdio_write(tp, 0x06, 0x8b81);
        mdio_write(tp, 0x06, 0xac21);
        mdio_write(tp, 0x06, 0x14e0);
        mdio_write(tp, 0x06, 0x8b85);
        mdio_write(tp, 0x06, 0xac20);
        mdio_write(tp, 0x06, 0x0ee0);
        mdio_write(tp, 0x06, 0x8b85);
        mdio_write(tp, 0x06, 0xac23);
        mdio_write(tp, 0x06, 0x08e0);
        mdio_write(tp, 0x06, 0x8b87);
        mdio_write(tp, 0x06, 0xac24);
        mdio_write(tp, 0x06, 0x02ae);
        mdio_write(tp, 0x06, 0x3802);
        mdio_write(tp, 0x06, 0x1ac2);
        mdio_write(tp, 0x06, 0xeee4);
        mdio_write(tp, 0x06, 0x1c04);
        mdio_write(tp, 0x06, 0xeee4);
        mdio_write(tp, 0x06, 0x1d04);
        mdio_write(tp, 0x06, 0xe2e0);
        mdio_write(tp, 0x06, 0x7ce3);
        mdio_write(tp, 0x06, 0xe07d);
        mdio_write(tp, 0x06, 0xe0e0);
        mdio_write(tp, 0x06, 0x38e1);
        mdio_write(tp, 0x06, 0xe039);
        mdio_write(tp, 0x06, 0xad2e);
        mdio_write(tp, 0x06, 0x1bad);
        mdio_write(tp, 0x06, 0x390d);
        mdio_write(tp, 0x06, 0xd101);
        mdio_write(tp, 0x06, 0xbf22);
        mdio_write(tp, 0x06, 0x7a02);
        mdio_write(tp, 0x06, 0x387d);
        mdio_write(tp, 0x06, 0x0281);
        mdio_write(tp, 0x06, 0xacae);
        mdio_write(tp, 0x06, 0x0bac);
        mdio_write(tp, 0x06, 0x3802);
        mdio_write(tp, 0x06, 0xae06);
        mdio_write(tp, 0x06, 0x0281);
        mdio_write(tp, 0x06, 0xe902);
        mdio_write(tp, 0x06, 0x822e);
        mdio_write(tp, 0x06, 0x021a);
        mdio_write(tp, 0x06, 0xd3fd);
        mdio_write(tp, 0x06, 0xfc04);
        mdio_write(tp, 0x06, 0xf8e1);
        mdio_write(tp, 0x06, 0x8af4);
        mdio_write(tp, 0x06, 0xe08b);
        mdio_write(tp, 0x06, 0x81ad);
        mdio_write(tp, 0x06, 0x2602);
        mdio_write(tp, 0x06, 0xf728);
        mdio_write(tp, 0x06, 0xe08b);
        mdio_write(tp, 0x06, 0x81ad);
        mdio_write(tp, 0x06, 0x2105);
        mdio_write(tp, 0x06, 0x0222);
        mdio_write(tp, 0x06, 0x8ef7);
        mdio_write(tp, 0x06, 0x29e0);
        mdio_write(tp, 0x06, 0x8b85);
        mdio_write(tp, 0x06, 0xad20);
        mdio_write(tp, 0x06, 0x0502);
        mdio_write(tp, 0x06, 0x14b8);
        mdio_write(tp, 0x06, 0xf72a);
        mdio_write(tp, 0x06, 0xe08b);
        mdio_write(tp, 0x06, 0x85ad);
        mdio_write(tp, 0x06, 0x2305);
        mdio_write(tp, 0x06, 0x0212);
        mdio_write(tp, 0x06, 0xf4f7);
        mdio_write(tp, 0x06, 0x2be0);
        mdio_write(tp, 0x06, 0x8b87);
        mdio_write(tp, 0x06, 0xad24);
        mdio_write(tp, 0x06, 0x0502);
        mdio_write(tp, 0x06, 0x8284);
        mdio_write(tp, 0x06, 0xf72c);
        mdio_write(tp, 0x06, 0xe58a);
        mdio_write(tp, 0x06, 0xf4fc);
        mdio_write(tp, 0x06, 0x04f8);
        mdio_write(tp, 0x06, 0xe08b);
        mdio_write(tp, 0x06, 0x81ad);
        mdio_write(tp, 0x06, 0x2600);
        mdio_write(tp, 0x06, 0xe08b);
        mdio_write(tp, 0x06, 0x81ad);
        mdio_write(tp, 0x06, 0x2109);
        mdio_write(tp, 0x06, 0xe08a);
        mdio_write(tp, 0x06, 0xf4ac);
        mdio_write(tp, 0x06, 0x2003);
        mdio_write(tp, 0x06, 0x0222);
        mdio_write(tp, 0x06, 0x7de0);
        mdio_write(tp, 0x06, 0x8b85);
        mdio_write(tp, 0x06, 0xad20);
        mdio_write(tp, 0x06, 0x09e0);
        mdio_write(tp, 0x06, 0x8af4);
        mdio_write(tp, 0x06, 0xac21);
        mdio_write(tp, 0x06, 0x0302);
        mdio_write(tp, 0x06, 0x1408);
        mdio_write(tp, 0x06, 0xe08b);
        mdio_write(tp, 0x06, 0x85ad);
        mdio_write(tp, 0x06, 0x2309);
        mdio_write(tp, 0x06, 0xe08a);
        mdio_write(tp, 0x06, 0xf4ac);
        mdio_write(tp, 0x06, 0x2203);
        mdio_write(tp, 0x06, 0x0213);
        mdio_write(tp, 0x06, 0x07e0);
        mdio_write(tp, 0x06, 0x8b87);
        mdio_write(tp, 0x06, 0xad24);
        mdio_write(tp, 0x06, 0x09e0);
        mdio_write(tp, 0x06, 0x8af4);
        mdio_write(tp, 0x06, 0xac23);
        mdio_write(tp, 0x06, 0x0302);
        mdio_write(tp, 0x06, 0x8289);
        mdio_write(tp, 0x06, 0xfc04);
        mdio_write(tp, 0x06, 0xf8e1);
        mdio_write(tp, 0x06, 0x8af4);
        mdio_write(tp, 0x06, 0xe08b);
        mdio_write(tp, 0x06, 0x81ad);
        mdio_write(tp, 0x06, 0x2602);
        mdio_write(tp, 0x06, 0xf628);
        mdio_write(tp, 0x06, 0xe08b);
        mdio_write(tp, 0x06, 0x81ad);
        mdio_write(tp, 0x06, 0x210a);
        mdio_write(tp, 0x06, 0xe083);
        mdio_write(tp, 0x06, 0xecf6);
        mdio_write(tp, 0x06, 0x27a0);
        mdio_write(tp, 0x06, 0x0502);
        mdio_write(tp, 0x06, 0xf629);
        mdio_write(tp, 0x06, 0xe08b);
        mdio_write(tp, 0x06, 0x85ad);
        mdio_write(tp, 0x06, 0x2008);
        mdio_write(tp, 0x06, 0xe08a);
        mdio_write(tp, 0x06, 0xe8ad);
        mdio_write(tp, 0x06, 0x2102);
        mdio_write(tp, 0x06, 0xf62a);
        mdio_write(tp, 0x06, 0xe08b);
        mdio_write(tp, 0x06, 0x85ad);
        mdio_write(tp, 0x06, 0x2308);
        mdio_write(tp, 0x06, 0xe08b);
        mdio_write(tp, 0x06, 0x20a0);
        mdio_write(tp, 0x06, 0x0302);
        mdio_write(tp, 0x06, 0xf62b);
        mdio_write(tp, 0x06, 0xe08b);
        mdio_write(tp, 0x06, 0x87ad);
        mdio_write(tp, 0x06, 0x2408);
        mdio_write(tp, 0x06, 0xe08a);
        mdio_write(tp, 0x06, 0xc2a0);
        mdio_write(tp, 0x06, 0x0302);
        mdio_write(tp, 0x06, 0xf62c);
        mdio_write(tp, 0x06, 0xe58a);
        mdio_write(tp, 0x06, 0xf4a1);
        mdio_write(tp, 0x06, 0x0008);
        mdio_write(tp, 0x06, 0xd100);
        mdio_write(tp, 0x06, 0xbf22);
        mdio_write(tp, 0x06, 0x7a02);
        mdio_write(tp, 0x06, 0x387d);
        mdio_write(tp, 0x06, 0xfc04);
        mdio_write(tp, 0x06, 0xee8a);
        mdio_write(tp, 0x06, 0xc200);
        mdio_write(tp, 0x06, 0x04f8);
        mdio_write(tp, 0x06, 0xe08b);
        mdio_write(tp, 0x06, 0x87ad);
        mdio_write(tp, 0x06, 0x241e);
        mdio_write(tp, 0x06, 0xe08a);
        mdio_write(tp, 0x06, 0xc2a0);
        mdio_write(tp, 0x06, 0x0005);
        mdio_write(tp, 0x06, 0x0282);
        mdio_write(tp, 0x06, 0xb0ae);
        mdio_write(tp, 0x06, 0xf5a0);
        mdio_write(tp, 0x06, 0x0105);
        mdio_write(tp, 0x06, 0x0282);
        mdio_write(tp, 0x06, 0xc0ae);
        mdio_write(tp, 0x06, 0x0ba0);
        mdio_write(tp, 0x06, 0x0205);
        mdio_write(tp, 0x06, 0x0282);
        mdio_write(tp, 0x06, 0xcaae);
        mdio_write(tp, 0x06, 0x03a0);
        mdio_write(tp, 0x06, 0x0300);
        mdio_write(tp, 0x06, 0xfc04);
        mdio_write(tp, 0x06, 0xf8fa);
        mdio_write(tp, 0x06, 0xef69);
        mdio_write(tp, 0x06, 0x0282);
        mdio_write(tp, 0x06, 0xe1ee);
        mdio_write(tp, 0x06, 0x8ac2);
        mdio_write(tp, 0x06, 0x01ef);
        mdio_write(tp, 0x06, 0x96fe);
        mdio_write(tp, 0x06, 0xfc04);
        mdio_write(tp, 0x06, 0xf8ee);
        mdio_write(tp, 0x06, 0x8ac9);
        mdio_write(tp, 0x06, 0x0002);
        mdio_write(tp, 0x06, 0x8317);
        mdio_write(tp, 0x06, 0xfc04);
        mdio_write(tp, 0x06, 0xf8e0);
        mdio_write(tp, 0x06, 0x8ac8);
        mdio_write(tp, 0x06, 0xe18a);
        mdio_write(tp, 0x06, 0xc91f);
        mdio_write(tp, 0x06, 0x019e);
        mdio_write(tp, 0x06, 0x0611);
        mdio_write(tp, 0x06, 0xe58a);
        mdio_write(tp, 0x06, 0xc9ae);
        mdio_write(tp, 0x06, 0x04ee);
        mdio_write(tp, 0x06, 0x8ac2);
        mdio_write(tp, 0x06, 0x01fc);
        mdio_write(tp, 0x06, 0x04f8);
        mdio_write(tp, 0x06, 0xf9fa);
        mdio_write(tp, 0x06, 0xef69);
        mdio_write(tp, 0x06, 0xfbbf);
        mdio_write(tp, 0x06, 0x8ac4);
        mdio_write(tp, 0x06, 0xef79);
        mdio_write(tp, 0x06, 0xd200);
        mdio_write(tp, 0x06, 0xd400);
        mdio_write(tp, 0x06, 0x221e);
        mdio_write(tp, 0x06, 0x02bf);
        mdio_write(tp, 0x06, 0x3024);
        mdio_write(tp, 0x06, 0x0238);
        mdio_write(tp, 0x06, 0x7dbf);
        mdio_write(tp, 0x06, 0x13ff);
        mdio_write(tp, 0x06, 0x0238);
        mdio_write(tp, 0x06, 0x500d);
        mdio_write(tp, 0x06, 0x4559);
        mdio_write(tp, 0x06, 0x1fef);
        mdio_write(tp, 0x06, 0x97dd);
        mdio_write(tp, 0x06, 0xd308);
        mdio_write(tp, 0x06, 0x1a93);
        mdio_write(tp, 0x06, 0xdd12);
        mdio_write(tp, 0x06, 0x17a2);
        mdio_write(tp, 0x06, 0x04de);
        mdio_write(tp, 0x06, 0xffef);
        mdio_write(tp, 0x06, 0x96fe);
        mdio_write(tp, 0x06, 0xfdfc);
        mdio_write(tp, 0x06, 0x04f8);
        mdio_write(tp, 0x06, 0xf9fa);
        mdio_write(tp, 0x06, 0xef69);
        mdio_write(tp, 0x06, 0xfbee);
        mdio_write(tp, 0x06, 0x8ac2);
        mdio_write(tp, 0x06, 0x03d5);
        mdio_write(tp, 0x06, 0x0080);
        mdio_write(tp, 0x06, 0xbf8a);
        mdio_write(tp, 0x06, 0xc4ef);
        mdio_write(tp, 0x06, 0x79ef);
        mdio_write(tp, 0x06, 0x45bf);
        mdio_write(tp, 0x06, 0x3024);
        mdio_write(tp, 0x06, 0x0238);
        mdio_write(tp, 0x06, 0x7dbf);
        mdio_write(tp, 0x06, 0x13ff);
        mdio_write(tp, 0x06, 0x0238);
        mdio_write(tp, 0x06, 0x50ad);
        mdio_write(tp, 0x06, 0x2702);
        mdio_write(tp, 0x06, 0x78ff);
        mdio_write(tp, 0x06, 0xe18a);
        mdio_write(tp, 0x06, 0xca1b);
        mdio_write(tp, 0x06, 0x01aa);
        mdio_write(tp, 0x06, 0x2eef);
        mdio_write(tp, 0x06, 0x97d9);
        mdio_write(tp, 0x06, 0x7900);
        mdio_write(tp, 0x06, 0x9e2b);
        mdio_write(tp, 0x06, 0x81dd);
        mdio_write(tp, 0x06, 0xbf85);
        mdio_write(tp, 0x06, 0xad02);
        mdio_write(tp, 0x06, 0x387d);
        mdio_write(tp, 0x06, 0xd101);
        mdio_write(tp, 0x06, 0xef02);
        mdio_write(tp, 0x06, 0x100c);
        mdio_write(tp, 0x06, 0x11b0);
        mdio_write(tp, 0x06, 0xfc0d);
        mdio_write(tp, 0x06, 0x11bf);
        mdio_write(tp, 0x06, 0x85aa);
        mdio_write(tp, 0x06, 0x0238);
        mdio_write(tp, 0x06, 0x7dd1);
        mdio_write(tp, 0x06, 0x00bf);
        mdio_write(tp, 0x06, 0x85aa);
        mdio_write(tp, 0x06, 0x0238);
        mdio_write(tp, 0x06, 0x7dee);
        mdio_write(tp, 0x06, 0x8ac2);
        mdio_write(tp, 0x06, 0x02ae);
        mdio_write(tp, 0x06, 0x0413);
        mdio_write(tp, 0x06, 0xa38b);
        mdio_write(tp, 0x06, 0xb4d3);
        mdio_write(tp, 0x06, 0x8012);
        mdio_write(tp, 0x06, 0x17a2);
        mdio_write(tp, 0x06, 0x04ad);
        mdio_write(tp, 0x06, 0xffef);
        mdio_write(tp, 0x06, 0x96fe);
        mdio_write(tp, 0x06, 0xfdfc);
        mdio_write(tp, 0x06, 0x04f8);
        mdio_write(tp, 0x06, 0xf9e0);
        mdio_write(tp, 0x06, 0x8b85);
        mdio_write(tp, 0x06, 0xad25);
        mdio_write(tp, 0x06, 0x48e0);
        mdio_write(tp, 0x06, 0x8a96);
        mdio_write(tp, 0x06, 0xe18a);
        mdio_write(tp, 0x06, 0x977c);
        mdio_write(tp, 0x06, 0x0000);
        mdio_write(tp, 0x06, 0x9e35);
        mdio_write(tp, 0x06, 0xee8a);
        mdio_write(tp, 0x06, 0x9600);
        mdio_write(tp, 0x06, 0xee8a);
        mdio_write(tp, 0x06, 0x9700);
        mdio_write(tp, 0x06, 0xe08a);
        mdio_write(tp, 0x06, 0xbee1);
        mdio_write(tp, 0x06, 0x8abf);
        mdio_write(tp, 0x06, 0xe28a);
        mdio_write(tp, 0x06, 0xc0e3);
        mdio_write(tp, 0x06, 0x8ac1);
        mdio_write(tp, 0x06, 0x0237);
        mdio_write(tp, 0x06, 0x74ad);
        mdio_write(tp, 0x06, 0x2012);
        mdio_write(tp, 0x06, 0xee8a);
        mdio_write(tp, 0x06, 0x9603);
        mdio_write(tp, 0x06, 0xee8a);
        mdio_write(tp, 0x06, 0x97b7);
        mdio_write(tp, 0x06, 0xee8a);
        mdio_write(tp, 0x06, 0xc000);
        mdio_write(tp, 0x06, 0xee8a);
        mdio_write(tp, 0x06, 0xc100);
        mdio_write(tp, 0x06, 0xae11);
        mdio_write(tp, 0x06, 0x15e6);
        mdio_write(tp, 0x06, 0x8ac0);
        mdio_write(tp, 0x06, 0xe78a);
        mdio_write(tp, 0x06, 0xc1ae);
        mdio_write(tp, 0x06, 0x08ee);
        mdio_write(tp, 0x06, 0x8ac0);
        mdio_write(tp, 0x06, 0x00ee);
        mdio_write(tp, 0x06, 0x8ac1);
        mdio_write(tp, 0x06, 0x00fd);
        mdio_write(tp, 0x06, 0xfc04);
        mdio_write(tp, 0x06, 0xae20);
        mdio_write(tp, 0x06, 0x0000);
        mdio_write(tp, 0x06, 0x0000);
        mdio_write(tp, 0x06, 0x0000);
        mdio_write(tp, 0x06, 0x0000);
        mdio_write(tp, 0x06, 0x0000);
        mdio_write(tp, 0x06, 0x0000);
        mdio_write(tp, 0x06, 0x0000);
        mdio_write(tp, 0x06, 0x0000);
        mdio_write(tp, 0x06, 0x0000);
        mdio_write(tp, 0x06, 0x0000);
        mdio_write(tp, 0x06, 0x0000);
        mdio_write(tp, 0x06, 0x0000);
        mdio_write(tp, 0x06, 0x0000);
        mdio_write(tp, 0x06, 0x0000);
        mdio_write(tp, 0x06, 0x0000);
        mdio_write(tp, 0x06, 0x0000);
        mdio_write(tp, 0x06, 0xf8fa);
        mdio_write(tp, 0x06, 0xef69);
        mdio_write(tp, 0x06, 0xe0e0);
        mdio_write(tp, 0x06, 0x00e1);
        mdio_write(tp, 0x06, 0xe001);
        mdio_write(tp, 0x06, 0xad27);
        mdio_write(tp, 0x06, 0x32e0);
        mdio_write(tp, 0x06, 0x8b40);
        mdio_write(tp, 0x06, 0xf720);
        mdio_write(tp, 0x06, 0xe48b);
        mdio_write(tp, 0x06, 0x40bf);
        mdio_write(tp, 0x06, 0x3230);
        mdio_write(tp, 0x06, 0x0238);
        mdio_write(tp, 0x06, 0x50ad);
        mdio_write(tp, 0x06, 0x2821);
        mdio_write(tp, 0x06, 0xe0e0);
        mdio_write(tp, 0x06, 0x20e1);
        mdio_write(tp, 0x06, 0xe021);
        mdio_write(tp, 0x06, 0xad20);
        mdio_write(tp, 0x06, 0x18e0);
        mdio_write(tp, 0x06, 0x8b40);
        mdio_write(tp, 0x06, 0xf620);
        mdio_write(tp, 0x06, 0xe48b);
        mdio_write(tp, 0x06, 0x40ee);
        mdio_write(tp, 0x06, 0x8b3b);
        mdio_write(tp, 0x06, 0xffe0);
        mdio_write(tp, 0x06, 0x8a8a);
        mdio_write(tp, 0x06, 0xe18a);
        mdio_write(tp, 0x06, 0x8be4);
        mdio_write(tp, 0x06, 0xe000);
        mdio_write(tp, 0x06, 0xe5e0);
        mdio_write(tp, 0x06, 0x01ef);
        mdio_write(tp, 0x06, 0x96fe);
        mdio_write(tp, 0x06, 0xfc04);
        mdio_write(tp, 0x06, 0xf8f9);
        mdio_write(tp, 0x06, 0xface);
        mdio_write(tp, 0x06, 0xfaef);
        mdio_write(tp, 0x06, 0x69fa);
        mdio_write(tp, 0x06, 0xd401);
        mdio_write(tp, 0x06, 0x55b4);
        mdio_write(tp, 0x06, 0xfebf);
        mdio_write(tp, 0x06, 0x1c1e);
        mdio_write(tp, 0x06, 0x0238);
        mdio_write(tp, 0x06, 0x50ac);
        mdio_write(tp, 0x06, 0x280b);
        mdio_write(tp, 0x06, 0xbf1c);
        mdio_write(tp, 0x06, 0x1b02);
        mdio_write(tp, 0x06, 0x3850);
        mdio_write(tp, 0x06, 0xac28);
        mdio_write(tp, 0x06, 0x49ae);
        mdio_write(tp, 0x06, 0x64bf);
        mdio_write(tp, 0x06, 0x1c1b);
        mdio_write(tp, 0x06, 0x0238);
        mdio_write(tp, 0x06, 0x50ac);
        mdio_write(tp, 0x06, 0x285b);
        mdio_write(tp, 0x06, 0xd000);
        mdio_write(tp, 0x06, 0x0284);
        mdio_write(tp, 0x06, 0xcaac);
        mdio_write(tp, 0x06, 0x2105);
        mdio_write(tp, 0x06, 0xac22);
        mdio_write(tp, 0x06, 0x02ae);
        mdio_write(tp, 0x06, 0x4ebf);
        mdio_write(tp, 0x06, 0xe0c4);
        mdio_write(tp, 0x06, 0xbe85);
        mdio_write(tp, 0x06, 0xf6d2);
        mdio_write(tp, 0x06, 0x04d8);
        mdio_write(tp, 0x06, 0x19d9);
        mdio_write(tp, 0x06, 0x1907);
        mdio_write(tp, 0x06, 0xdc19);
        mdio_write(tp, 0x06, 0xdd19);
        mdio_write(tp, 0x06, 0x0789);
        mdio_write(tp, 0x06, 0x89ef);
        mdio_write(tp, 0x06, 0x645e);
        mdio_write(tp, 0x06, 0x07ff);
        mdio_write(tp, 0x06, 0x0d65);
        mdio_write(tp, 0x06, 0x5cf8);
        mdio_write(tp, 0x06, 0x001e);
        mdio_write(tp, 0x06, 0x46dc);
        mdio_write(tp, 0x06, 0x19dd);
        mdio_write(tp, 0x06, 0x19b2);
        mdio_write(tp, 0x06, 0xe2d4);
        mdio_write(tp, 0x06, 0x0001);
        mdio_write(tp, 0x06, 0xbf1c);
        mdio_write(tp, 0x06, 0x1b02);
        mdio_write(tp, 0x06, 0x387d);
        mdio_write(tp, 0x06, 0xae1d);
        mdio_write(tp, 0x06, 0xbee0);
        mdio_write(tp, 0x06, 0xc4bf);
        mdio_write(tp, 0x06, 0x85f6);
        mdio_write(tp, 0x06, 0xd204);
        mdio_write(tp, 0x06, 0xd819);
        mdio_write(tp, 0x06, 0xd919);
        mdio_write(tp, 0x06, 0x07dc);
        mdio_write(tp, 0x06, 0x19dd);
        mdio_write(tp, 0x06, 0x1907);
        mdio_write(tp, 0x06, 0xb2f4);
        mdio_write(tp, 0x06, 0xd400);
        mdio_write(tp, 0x06, 0x00bf);
        mdio_write(tp, 0x06, 0x1c1b);
        mdio_write(tp, 0x06, 0x0238);
        mdio_write(tp, 0x06, 0x7dfe);
        mdio_write(tp, 0x06, 0xef96);
        mdio_write(tp, 0x06, 0xfec6);
        mdio_write(tp, 0x06, 0xfefd);
        mdio_write(tp, 0x06, 0xfc05);
        mdio_write(tp, 0x06, 0xf9e2);
        mdio_write(tp, 0x06, 0xe0ea);
        mdio_write(tp, 0x06, 0xe3e0);
        mdio_write(tp, 0x06, 0xeb5a);
        mdio_write(tp, 0x06, 0x070c);
        mdio_write(tp, 0x06, 0x031e);
        mdio_write(tp, 0x06, 0x20e6);
        mdio_write(tp, 0x06, 0xe0ea);
        mdio_write(tp, 0x06, 0xe7e0);
        mdio_write(tp, 0x06, 0xebe0);
        mdio_write(tp, 0x06, 0xe0fc);
        mdio_write(tp, 0x06, 0xe1e0);
        mdio_write(tp, 0x06, 0xfdfd);
        mdio_write(tp, 0x06, 0x04f8);
        mdio_write(tp, 0x06, 0xfaef);
        mdio_write(tp, 0x06, 0x69e0);
        mdio_write(tp, 0x06, 0x8b80);
        mdio_write(tp, 0x06, 0xad27);
        mdio_write(tp, 0x06, 0x22bf);
        mdio_write(tp, 0x06, 0x4616);
        mdio_write(tp, 0x06, 0x0238);
        mdio_write(tp, 0x06, 0x50e0);
        mdio_write(tp, 0x06, 0x8b44);
        mdio_write(tp, 0x06, 0x1f01);
        mdio_write(tp, 0x06, 0x9e15);
        mdio_write(tp, 0x06, 0xe58b);
        mdio_write(tp, 0x06, 0x44ad);
        mdio_write(tp, 0x06, 0x2907);
        mdio_write(tp, 0x06, 0xac28);
        mdio_write(tp, 0x06, 0x04d1);
        mdio_write(tp, 0x06, 0x01ae);
        mdio_write(tp, 0x06, 0x02d1);
        mdio_write(tp, 0x06, 0x00bf);
        mdio_write(tp, 0x06, 0x85b0);
        mdio_write(tp, 0x06, 0x0238);
        mdio_write(tp, 0x06, 0x7def);
        mdio_write(tp, 0x06, 0x96fe);
        mdio_write(tp, 0x06, 0xfc04);
        mdio_write(tp, 0x06, 0xf8e0);
        mdio_write(tp, 0x06, 0x8b85);
        mdio_write(tp, 0x06, 0xad26);
        mdio_write(tp, 0x06, 0x30e0);
        mdio_write(tp, 0x06, 0xe036);
        mdio_write(tp, 0x06, 0xe1e0);
        mdio_write(tp, 0x06, 0x37e1);
        mdio_write(tp, 0x06, 0x8b3f);
        mdio_write(tp, 0x06, 0x1f10);
        mdio_write(tp, 0x06, 0x9e23);
        mdio_write(tp, 0x06, 0xe48b);
        mdio_write(tp, 0x06, 0x3fac);
        mdio_write(tp, 0x06, 0x200b);
        mdio_write(tp, 0x06, 0xac21);
        mdio_write(tp, 0x06, 0x0dac);
        mdio_write(tp, 0x06, 0x250f);
        mdio_write(tp, 0x06, 0xac27);
        mdio_write(tp, 0x06, 0x11ae);
        mdio_write(tp, 0x06, 0x1202);
        mdio_write(tp, 0x06, 0x2c47);
        mdio_write(tp, 0x06, 0xae0d);
        mdio_write(tp, 0x06, 0x0285);
        mdio_write(tp, 0x06, 0x4fae);
        mdio_write(tp, 0x06, 0x0802);
        mdio_write(tp, 0x06, 0x2c69);
        mdio_write(tp, 0x06, 0xae03);
        mdio_write(tp, 0x06, 0x022c);
        mdio_write(tp, 0x06, 0x7cfc);
        mdio_write(tp, 0x06, 0x04f8);
        mdio_write(tp, 0x06, 0xfaef);
        mdio_write(tp, 0x06, 0x6902);
        mdio_write(tp, 0x06, 0x856c);
        mdio_write(tp, 0x06, 0xe0e0);
        mdio_write(tp, 0x06, 0x14e1);
        mdio_write(tp, 0x06, 0xe015);
        mdio_write(tp, 0x06, 0xad26);
        mdio_write(tp, 0x06, 0x08d1);
        mdio_write(tp, 0x06, 0x1ebf);
        mdio_write(tp, 0x06, 0x2cd9);
        mdio_write(tp, 0x06, 0x0238);
        mdio_write(tp, 0x06, 0x7def);
        mdio_write(tp, 0x06, 0x96fe);
        mdio_write(tp, 0x06, 0xfc04);
        mdio_write(tp, 0x06, 0xf8e0);
        mdio_write(tp, 0x06, 0x8b85);
        mdio_write(tp, 0x06, 0xad27);
        mdio_write(tp, 0x06, 0x2fd0);
        mdio_write(tp, 0x06, 0x0b02);
        mdio_write(tp, 0x06, 0x3682);
        mdio_write(tp, 0x06, 0x5882);
        mdio_write(tp, 0x06, 0x7882);
        mdio_write(tp, 0x06, 0x9f24);
        mdio_write(tp, 0x06, 0xe08b);
        mdio_write(tp, 0x06, 0x32e1);
        mdio_write(tp, 0x06, 0x8b33);
        mdio_write(tp, 0x06, 0x1f10);
        mdio_write(tp, 0x06, 0x9e1a);
        mdio_write(tp, 0x06, 0x10e4);
        mdio_write(tp, 0x06, 0x8b32);
        mdio_write(tp, 0x06, 0xe0e0);
        mdio_write(tp, 0x06, 0x28e1);
        mdio_write(tp, 0x06, 0xe029);
        mdio_write(tp, 0x06, 0xf72c);
        mdio_write(tp, 0x06, 0xe4e0);
        mdio_write(tp, 0x06, 0x28e5);
        mdio_write(tp, 0x06, 0xe029);
        mdio_write(tp, 0x06, 0xf62c);
        mdio_write(tp, 0x06, 0xe4e0);
        mdio_write(tp, 0x06, 0x28e5);
        mdio_write(tp, 0x06, 0xe029);
        mdio_write(tp, 0x06, 0xfc04);
        mdio_write(tp, 0x06, 0x00e1);
        mdio_write(tp, 0x06, 0x4077);
        mdio_write(tp, 0x06, 0xe140);
        mdio_write(tp, 0x06, 0x52e0);
        mdio_write(tp, 0x06, 0xeed9);
        mdio_write(tp, 0x06, 0xe04c);
        mdio_write(tp, 0x06, 0xbbe0);
        mdio_write(tp, 0x06, 0x2a00);
        mdio_write(tp, 0x05, 0xe142);
        gphy_val = mdio_read(tp, 0x06);
        gphy_val |= BIT_0;
        mdio_write(tp,0x06, gphy_val);
        mdio_write(tp, 0x05, 0xe140);
        gphy_val = mdio_read(tp, 0x06);
        gphy_val |= BIT_0;
        mdio_write(tp,0x06, gphy_val);
        mdio_write(tp, 0x1f, 0x0000);
        mdio_write(tp,0x1f, 0x0005);
        for (i = 0; i < 200; i++) {
                udelay(100);
                gphy_val = mdio_read(tp, 0x00);
                if (gphy_val & BIT_7)
                        break;
        }
        mdio_write(tp, 0x1f, 0x0007);
        mdio_write(tp, 0x1e, 0x0023);
        gphy_val = mdio_read(tp, 0x17);
        gphy_val |= BIT_1;
        if (tp->RequiredSecLanDonglePatch)
                gphy_val &= ~BIT_2;
        mdio_write(tp, 0x17, gphy_val);
        mdio_write(tp, 0x1f, 0x0000);

        mdio_write(tp, 0x1F, 0x0003);
        mdio_write(tp, 0x09, 0xA20F);
        mdio_write(tp, 0x1F, 0x0000);
        mdio_write(tp, 0x1f, 0x0003);
        mdio_write(tp, 0x01, 0x328A);
        mdio_write(tp, 0x1f, 0x0000);

        mdio_write(tp, 0x1f, 0x0000);
        mdio_write(tp, 0x00, 0x9200);
}

static void
rtl8168_set_phy_mcu_8168f_2(struct net_device *dev)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        unsigned int gphy_val,i;

        mdio_write(tp,0x1f, 0x0000);
        mdio_write(tp,0x00, 0x1800);
        gphy_val = mdio_read(tp, 0x15);
        gphy_val &= ~(BIT_12);
        mdio_write(tp,0x15, gphy_val);
        mdio_write(tp,0x00, 0x9800);
        mdio_write(tp,0x1f, 0x0007);
        mdio_write(tp,0x1e, 0x002f);
        for (i = 0; i < 1000; i++) {
                udelay(100);
                gphy_val = mdio_read(tp, 0x1c);
                if (gphy_val & 0x0080)
                        break;
        }
        mdio_write(tp, 0x1f, 0x0007);
        mdio_write(tp, 0x1e, 0x0023);
        mdio_write(tp, 0x16, 0x0306);
        mdio_write(tp, 0x16, 0x0307);
        mdio_write(tp, 0x15, 0x0098);
        mdio_write(tp, 0x19, 0x7c0b);
        mdio_write(tp, 0x15, 0x0099);
        mdio_write(tp, 0x19, 0x6c0b);
        mdio_write(tp, 0x15, 0x00eb);
        mdio_write(tp, 0x19, 0x6c0b);
        mdio_write(tp, 0x15, 0x00f8);
        mdio_write(tp, 0x19, 0x6f0b);
        mdio_write(tp, 0x15, 0x00fe);
        mdio_write(tp, 0x19, 0x6f0f);
        mdio_write(tp, 0x15, 0x00db);
        mdio_write(tp, 0x19, 0x6f09);
        mdio_write(tp, 0x15, 0x00dc);
        mdio_write(tp, 0x19, 0xaefd);
        mdio_write(tp, 0x15, 0x00dd);
        mdio_write(tp, 0x19, 0x6f0b);
        mdio_write(tp, 0x15, 0x00de);
        mdio_write(tp, 0x19, 0xc60b);
        mdio_write(tp, 0x15, 0x00df);
        mdio_write(tp, 0x19, 0x00fa);
        mdio_write(tp, 0x15, 0x00e0);
        mdio_write(tp, 0x19, 0x30e1);
        mdio_write(tp, 0x15, 0x020c);
        mdio_write(tp, 0x19, 0x3224);
        mdio_write(tp, 0x15, 0x020e);
        mdio_write(tp, 0x19, 0x9813);
        mdio_write(tp, 0x15, 0x020f);
        mdio_write(tp, 0x19, 0x7801);
        mdio_write(tp, 0x15, 0x0210);
        mdio_write(tp, 0x19, 0x930f);
        mdio_write(tp, 0x15, 0x0211);
        mdio_write(tp, 0x19, 0x9206);
        mdio_write(tp, 0x15, 0x0212);
        mdio_write(tp, 0x19, 0x4002);
        mdio_write(tp, 0x15, 0x0213);
        mdio_write(tp, 0x19, 0x7800);
        mdio_write(tp, 0x15, 0x0214);
        mdio_write(tp, 0x19, 0x588f);
        mdio_write(tp, 0x15, 0x0215);
        mdio_write(tp, 0x19, 0x5520);
        mdio_write(tp, 0x15, 0x0216);
        mdio_write(tp, 0x19, 0x3224);
        mdio_write(tp, 0x15, 0x0217);
        mdio_write(tp, 0x19, 0x4002);
        mdio_write(tp, 0x15, 0x0218);
        mdio_write(tp, 0x19, 0x7800);
        mdio_write(tp, 0x15, 0x0219);
        mdio_write(tp, 0x19, 0x588d);
        mdio_write(tp, 0x15, 0x021a);
        mdio_write(tp, 0x19, 0x5540);
        mdio_write(tp, 0x15, 0x021b);
        mdio_write(tp, 0x19, 0x9e03);
        mdio_write(tp, 0x15, 0x021c);
        mdio_write(tp, 0x19, 0x7c40);
        mdio_write(tp, 0x15, 0x021d);
        mdio_write(tp, 0x19, 0x6840);
        mdio_write(tp, 0x15, 0x021e);
        mdio_write(tp, 0x19, 0x3224);
        mdio_write(tp, 0x15, 0x021f);
        mdio_write(tp, 0x19, 0x4002);
        mdio_write(tp, 0x15, 0x0220);
        mdio_write(tp, 0x19, 0x3224);
        mdio_write(tp, 0x15, 0x0221);
        mdio_write(tp, 0x19, 0x9e03);
        mdio_write(tp, 0x15, 0x0222);
        mdio_write(tp, 0x19, 0x7c40);
        mdio_write(tp, 0x15, 0x0223);
        mdio_write(tp, 0x19, 0x6840);
        mdio_write(tp, 0x15, 0x0224);
        mdio_write(tp, 0x19, 0x7800);
        mdio_write(tp, 0x15, 0x0225);
        mdio_write(tp, 0x19, 0x3231);
        mdio_write(tp, 0x15, 0x0000);
        mdio_write(tp, 0x16, 0x0306);
        mdio_write(tp, 0x16, 0x0300);
        mdio_write(tp, 0x1f, 0x0000);
        mdio_write(tp, 0x1f, 0x0005);
        mdio_write(tp, 0x05, 0xfff6);
        mdio_write(tp, 0x06, 0x0080);
        mdio_write(tp, 0x05, 0x8000);
        mdio_write(tp, 0x06, 0x0280);
        mdio_write(tp, 0x06, 0x48f7);
        mdio_write(tp, 0x06, 0x00e0);
        mdio_write(tp, 0x06, 0xfff7);
        mdio_write(tp, 0x06, 0xa080);
        mdio_write(tp, 0x06, 0x02ae);
        mdio_write(tp, 0x06, 0xf602);
        mdio_write(tp, 0x06, 0x011b);
        mdio_write(tp, 0x06, 0x0201);
        mdio_write(tp, 0x06, 0x2802);
        mdio_write(tp, 0x06, 0x0135);
        mdio_write(tp, 0x06, 0x0201);
        mdio_write(tp, 0x06, 0x4502);
        mdio_write(tp, 0x06, 0x015f);
        mdio_write(tp, 0x06, 0x0280);
        mdio_write(tp, 0x06, 0x6b02);
        mdio_write(tp, 0x06, 0x80e5);
        mdio_write(tp, 0x06, 0xe08b);
        mdio_write(tp, 0x06, 0x88e1);
        mdio_write(tp, 0x06, 0x8b89);
        mdio_write(tp, 0x06, 0x1e01);
        mdio_write(tp, 0x06, 0xe18b);
        mdio_write(tp, 0x06, 0x8a1e);
        mdio_write(tp, 0x06, 0x01e1);
        mdio_write(tp, 0x06, 0x8b8b);
        mdio_write(tp, 0x06, 0x1e01);
        mdio_write(tp, 0x06, 0xe18b);
        mdio_write(tp, 0x06, 0x8c1e);
        mdio_write(tp, 0x06, 0x01e1);
        mdio_write(tp, 0x06, 0x8b8d);
        mdio_write(tp, 0x06, 0x1e01);
        mdio_write(tp, 0x06, 0xe18b);
        mdio_write(tp, 0x06, 0x8e1e);
        mdio_write(tp, 0x06, 0x01a0);
        mdio_write(tp, 0x06, 0x00c7);
        mdio_write(tp, 0x06, 0xaebb);
        mdio_write(tp, 0x06, 0xbf8b);
        mdio_write(tp, 0x06, 0x88ec);
        mdio_write(tp, 0x06, 0x0019);
        mdio_write(tp, 0x06, 0xa98b);
        mdio_write(tp, 0x06, 0x90f9);
        mdio_write(tp, 0x06, 0xeeff);
        mdio_write(tp, 0x06, 0xf600);
        mdio_write(tp, 0x06, 0xeeff);
        mdio_write(tp, 0x06, 0xf7fe);
        mdio_write(tp, 0x06, 0xd100);
        mdio_write(tp, 0x06, 0xbf81);
        mdio_write(tp, 0x06, 0x9802);
        mdio_write(tp, 0x06, 0x39f3);
        mdio_write(tp, 0x06, 0xd101);
        mdio_write(tp, 0x06, 0xbf81);
        mdio_write(tp, 0x06, 0x9b02);
        mdio_write(tp, 0x06, 0x39f3);
        mdio_write(tp, 0x06, 0x04f8);
        mdio_write(tp, 0x06, 0xe08b);
        mdio_write(tp, 0x06, 0x8dad);
        mdio_write(tp, 0x06, 0x2014);
        mdio_write(tp, 0x06, 0xee8b);
        mdio_write(tp, 0x06, 0x8d00);
        mdio_write(tp, 0x06, 0xe08a);
        mdio_write(tp, 0x06, 0x5a78);
        mdio_write(tp, 0x06, 0x039e);
        mdio_write(tp, 0x06, 0x0902);
        mdio_write(tp, 0x06, 0x05fc);
        mdio_write(tp, 0x06, 0x0280);
        mdio_write(tp, 0x06, 0x8802);
        mdio_write(tp, 0x06, 0x32dd);
        mdio_write(tp, 0x06, 0xfc04);
        mdio_write(tp, 0x06, 0xf8f9);
        mdio_write(tp, 0x06, 0xe08b);
        mdio_write(tp, 0x06, 0x81ac);
        mdio_write(tp, 0x06, 0x261a);
        mdio_write(tp, 0x06, 0xe08b);
        mdio_write(tp, 0x06, 0x81ac);
        mdio_write(tp, 0x06, 0x2114);
        mdio_write(tp, 0x06, 0xe08b);
        mdio_write(tp, 0x06, 0x85ac);
        mdio_write(tp, 0x06, 0x200e);
        mdio_write(tp, 0x06, 0xe08b);
        mdio_write(tp, 0x06, 0x85ac);
        mdio_write(tp, 0x06, 0x2308);
        mdio_write(tp, 0x06, 0xe08b);
        mdio_write(tp, 0x06, 0x87ac);
        mdio_write(tp, 0x06, 0x2402);
        mdio_write(tp, 0x06, 0xae38);
        mdio_write(tp, 0x06, 0x021a);
        mdio_write(tp, 0x06, 0xd6ee);
        mdio_write(tp, 0x06, 0xe41c);
        mdio_write(tp, 0x06, 0x04ee);
        mdio_write(tp, 0x06, 0xe41d);
        mdio_write(tp, 0x06, 0x04e2);
        mdio_write(tp, 0x06, 0xe07c);
        mdio_write(tp, 0x06, 0xe3e0);
        mdio_write(tp, 0x06, 0x7de0);
        mdio_write(tp, 0x06, 0xe038);
        mdio_write(tp, 0x06, 0xe1e0);
        mdio_write(tp, 0x06, 0x39ad);
        mdio_write(tp, 0x06, 0x2e1b);
        mdio_write(tp, 0x06, 0xad39);
        mdio_write(tp, 0x06, 0x0dd1);
        mdio_write(tp, 0x06, 0x01bf);
        mdio_write(tp, 0x06, 0x22c8);
        mdio_write(tp, 0x06, 0x0239);
        mdio_write(tp, 0x06, 0xf302);
        mdio_write(tp, 0x06, 0x21f0);
        mdio_write(tp, 0x06, 0xae0b);
        mdio_write(tp, 0x06, 0xac38);
        mdio_write(tp, 0x06, 0x02ae);
        mdio_write(tp, 0x06, 0x0602);
        mdio_write(tp, 0x06, 0x222d);
        mdio_write(tp, 0x06, 0x0222);
        mdio_write(tp, 0x06, 0x7202);
        mdio_write(tp, 0x06, 0x1ae7);
        mdio_write(tp, 0x06, 0xfdfc);
        mdio_write(tp, 0x06, 0x04f8);
        mdio_write(tp, 0x06, 0xe08b);
        mdio_write(tp, 0x06, 0x8ead);
        mdio_write(tp, 0x06, 0x201a);
        mdio_write(tp, 0x06, 0xf620);
        mdio_write(tp, 0x06, 0xe48b);
        mdio_write(tp, 0x06, 0x8e02);
        mdio_write(tp, 0x06, 0x2afe);
        mdio_write(tp, 0x06, 0x022c);
        mdio_write(tp, 0x06, 0x5c02);
        mdio_write(tp, 0x06, 0x03c5);
        mdio_write(tp, 0x06, 0x0281);
        mdio_write(tp, 0x06, 0x6702);
        mdio_write(tp, 0x06, 0x2e4f);
        mdio_write(tp, 0x06, 0x0204);
        mdio_write(tp, 0x06, 0x8902);
        mdio_write(tp, 0x06, 0x2f7a);
        mdio_write(tp, 0x06, 0xe08b);
        mdio_write(tp, 0x06, 0x8ead);
        mdio_write(tp, 0x06, 0x210b);
        mdio_write(tp, 0x06, 0xf621);
        mdio_write(tp, 0x06, 0xe48b);
        mdio_write(tp, 0x06, 0x8e02);
        mdio_write(tp, 0x06, 0x0445);
        mdio_write(tp, 0x06, 0x021c);
        mdio_write(tp, 0x06, 0xb8e0);
        mdio_write(tp, 0x06, 0x8b8e);
        mdio_write(tp, 0x06, 0xad22);
        mdio_write(tp, 0x06, 0x08f6);
        mdio_write(tp, 0x06, 0x22e4);
        mdio_write(tp, 0x06, 0x8b8e);
        mdio_write(tp, 0x06, 0x0235);
        mdio_write(tp, 0x06, 0xd4e0);
        mdio_write(tp, 0x06, 0x8b8e);
        mdio_write(tp, 0x06, 0xad23);
        mdio_write(tp, 0x06, 0x08f6);
        mdio_write(tp, 0x06, 0x23e4);
        mdio_write(tp, 0x06, 0x8b8e);
        mdio_write(tp, 0x06, 0x0231);
        mdio_write(tp, 0x06, 0xc8e0);
        mdio_write(tp, 0x06, 0x8b8e);
        mdio_write(tp, 0x06, 0xad24);
        mdio_write(tp, 0x06, 0x05f6);
        mdio_write(tp, 0x06, 0x24e4);
        mdio_write(tp, 0x06, 0x8b8e);
        mdio_write(tp, 0x06, 0xe08b);
        mdio_write(tp, 0x06, 0x8ead);
        mdio_write(tp, 0x06, 0x2505);
        mdio_write(tp, 0x06, 0xf625);
        mdio_write(tp, 0x06, 0xe48b);
        mdio_write(tp, 0x06, 0x8ee0);
        mdio_write(tp, 0x06, 0x8b8e);
        mdio_write(tp, 0x06, 0xad26);
        mdio_write(tp, 0x06, 0x08f6);
        mdio_write(tp, 0x06, 0x26e4);
        mdio_write(tp, 0x06, 0x8b8e);
        mdio_write(tp, 0x06, 0x022d);
        mdio_write(tp, 0x06, 0x6ae0);
        mdio_write(tp, 0x06, 0x8b8e);
        mdio_write(tp, 0x06, 0xad27);
        mdio_write(tp, 0x06, 0x05f6);
        mdio_write(tp, 0x06, 0x27e4);
        mdio_write(tp, 0x06, 0x8b8e);
        mdio_write(tp, 0x06, 0x0203);
        mdio_write(tp, 0x06, 0x8bfc);
        mdio_write(tp, 0x06, 0x04f8);
        mdio_write(tp, 0x06, 0xfaef);
        mdio_write(tp, 0x06, 0x69e0);
        mdio_write(tp, 0x06, 0x8b80);
        mdio_write(tp, 0x06, 0xad27);
        mdio_write(tp, 0x06, 0x22bf);
        mdio_write(tp, 0x06, 0x479a);
        mdio_write(tp, 0x06, 0x0239);
        mdio_write(tp, 0x06, 0xc6e0);
        mdio_write(tp, 0x06, 0x8b44);
        mdio_write(tp, 0x06, 0x1f01);
        mdio_write(tp, 0x06, 0x9e15);
        mdio_write(tp, 0x06, 0xe58b);
        mdio_write(tp, 0x06, 0x44ad);
        mdio_write(tp, 0x06, 0x2907);
        mdio_write(tp, 0x06, 0xac28);
        mdio_write(tp, 0x06, 0x04d1);
        mdio_write(tp, 0x06, 0x01ae);
        mdio_write(tp, 0x06, 0x02d1);
        mdio_write(tp, 0x06, 0x00bf);
        mdio_write(tp, 0x06, 0x819e);
        mdio_write(tp, 0x06, 0x0239);
        mdio_write(tp, 0x06, 0xf3ef);
        mdio_write(tp, 0x06, 0x96fe);
        mdio_write(tp, 0x06, 0xfc04);
        mdio_write(tp, 0x06, 0x00e1);
        mdio_write(tp, 0x06, 0x4077);
        mdio_write(tp, 0x06, 0xe140);
        mdio_write(tp, 0x06, 0xbbe0);
        mdio_write(tp, 0x06, 0x2a00);
        mdio_write(tp, 0x05, 0xe142);
        gphy_val = mdio_read(tp, 0x06);
        gphy_val |= BIT_0;
        mdio_write(tp, 0x06, gphy_val);
        mdio_write(tp, 0x05, 0xe140);
        gphy_val = mdio_read(tp, 0x06);
        gphy_val |= BIT_0;
        mdio_write(tp, 0x06, gphy_val);
        mdio_write(tp, 0x1f, 0x0000);
        mdio_write(tp, 0x1f, 0x0007);
        mdio_write(tp, 0x1e, 0x0023);
        gphy_val = mdio_read(tp, 0x17);
        gphy_val |= BIT_1;
        if (tp->RequiredSecLanDonglePatch)
                gphy_val &= ~BIT_2;
        mdio_write(tp, 0x17, gphy_val);
        mdio_write(tp, 0x1f, 0x0000);
        mdio_write(tp, 0x1f, 0x0000);
        mdio_write(tp, 0x00, 0x9200);
}

static void
rtl8168_set_phy_mcu_8411_1(struct net_device *dev)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        unsigned int gphy_val,i;

        mdio_write(tp,0x1f, 0x0000);
        mdio_write(tp,0x00, 0x1800);
        gphy_val = mdio_read(tp, 0x15);
        gphy_val &= ~(BIT_12);
        mdio_write(tp,0x15, gphy_val);
        mdio_write(tp,0x00, 0x4800);
        mdio_write(tp,0x1f, 0x0007);
        mdio_write(tp,0x1e, 0x002f);
        for (i = 0; i < 1000; i++) {
                udelay(100);
                gphy_val = mdio_read(tp, 0x1c);
                if (gphy_val & 0x0080)
                        break;
        }
        mdio_write(tp,0x1f, 0x0000);
        mdio_write(tp,0x00, 0x1800);
        mdio_write(tp,0x1f, 0x0007);
        mdio_write(tp,0x1e, 0x0023);
        for (i = 0; i < 200; i++) {
                udelay(100);
                gphy_val = mdio_read(tp, 0x18);
                if (!(gphy_val & 0x0001))
                        break;
        }
        mdio_write(tp,0x1f, 0x0005);
        mdio_write(tp,0x05, 0xfff6);
        mdio_write(tp,0x06, 0x0080);
        mdio_write(tp, 0x1f, 0x0007);
        mdio_write(tp, 0x1e, 0x0023);
        mdio_write(tp, 0x16, 0x0306);
        mdio_write(tp, 0x16, 0x0307);
        mdio_write(tp, 0x15, 0x0098);
        mdio_write(tp, 0x19, 0x7c0b);
        mdio_write(tp, 0x15, 0x0099);
        mdio_write(tp, 0x19, 0x6c0b);
        mdio_write(tp, 0x15, 0x00eb);
        mdio_write(tp, 0x19, 0x6c0b);
        mdio_write(tp, 0x15, 0x00f8);
        mdio_write(tp, 0x19, 0x6f0b);
        mdio_write(tp, 0x15, 0x00fe);
        mdio_write(tp, 0x19, 0x6f0f);
        mdio_write(tp, 0x15, 0x00db);
        mdio_write(tp, 0x19, 0x6f09);
        mdio_write(tp, 0x15, 0x00dc);
        mdio_write(tp, 0x19, 0xaefd);
        mdio_write(tp, 0x15, 0x00dd);
        mdio_write(tp, 0x19, 0x6f0b);
        mdio_write(tp, 0x15, 0x00de);
        mdio_write(tp, 0x19, 0xc60b);
        mdio_write(tp, 0x15, 0x00df);
        mdio_write(tp, 0x19, 0x00fa);
        mdio_write(tp, 0x15, 0x00e0);
        mdio_write(tp, 0x19, 0x30e1);
        mdio_write(tp, 0x15, 0x020c);
        mdio_write(tp, 0x19, 0x3224);
        mdio_write(tp, 0x15, 0x020e);
        mdio_write(tp, 0x19, 0x9813);
        mdio_write(tp, 0x15, 0x020f);
        mdio_write(tp, 0x19, 0x7801);
        mdio_write(tp, 0x15, 0x0210);
        mdio_write(tp, 0x19, 0x930f);
        mdio_write(tp, 0x15, 0x0211);
        mdio_write(tp, 0x19, 0x9206);
        mdio_write(tp, 0x15, 0x0212);
        mdio_write(tp, 0x19, 0x4002);
        mdio_write(tp, 0x15, 0x0213);
        mdio_write(tp, 0x19, 0x7800);
        mdio_write(tp, 0x15, 0x0214);
        mdio_write(tp, 0x19, 0x588f);
        mdio_write(tp, 0x15, 0x0215);
        mdio_write(tp, 0x19, 0x5520);
        mdio_write(tp, 0x15, 0x0216);
        mdio_write(tp, 0x19, 0x3224);
        mdio_write(tp, 0x15, 0x0217);
        mdio_write(tp, 0x19, 0x4002);
        mdio_write(tp, 0x15, 0x0218);
        mdio_write(tp, 0x19, 0x7800);
        mdio_write(tp, 0x15, 0x0219);
        mdio_write(tp, 0x19, 0x588d);
        mdio_write(tp, 0x15, 0x021a);
        mdio_write(tp, 0x19, 0x5540);
        mdio_write(tp, 0x15, 0x021b);
        mdio_write(tp, 0x19, 0x9e03);
        mdio_write(tp, 0x15, 0x021c);
        mdio_write(tp, 0x19, 0x7c40);
        mdio_write(tp, 0x15, 0x021d);
        mdio_write(tp, 0x19, 0x6840);
        mdio_write(tp, 0x15, 0x021e);
        mdio_write(tp, 0x19, 0x3224);
        mdio_write(tp, 0x15, 0x021f);
        mdio_write(tp, 0x19, 0x4002);
        mdio_write(tp, 0x15, 0x0220);
        mdio_write(tp, 0x19, 0x3224);
        mdio_write(tp, 0x15, 0x0221);
        mdio_write(tp, 0x19, 0x9e03);
        mdio_write(tp, 0x15, 0x0222);
        mdio_write(tp, 0x19, 0x7c40);
        mdio_write(tp, 0x15, 0x0223);
        mdio_write(tp, 0x19, 0x6840);
        mdio_write(tp, 0x15, 0x0224);
        mdio_write(tp, 0x19, 0x7800);
        mdio_write(tp, 0x15, 0x0225);
        mdio_write(tp, 0x19, 0x3231);
        mdio_write(tp, 0x15, 0x0000);
        mdio_write(tp, 0x16, 0x0306);
        mdio_write(tp, 0x16, 0x0300);
        mdio_write(tp, 0x1f, 0x0000);
        mdio_write(tp, 0x1f, 0x0005);
        mdio_write(tp, 0x05, 0xfff6);
        mdio_write(tp, 0x06, 0x0080);
        mdio_write(tp, 0x05, 0x8000);
        mdio_write(tp, 0x06, 0x0280);
        mdio_write(tp, 0x06, 0x48f7);
        mdio_write(tp, 0x06, 0x00e0);
        mdio_write(tp, 0x06, 0xfff7);
        mdio_write(tp, 0x06, 0xa080);
        mdio_write(tp, 0x06, 0x02ae);
        mdio_write(tp, 0x06, 0xf602);
        mdio_write(tp, 0x06, 0x011e);
        mdio_write(tp, 0x06, 0x0201);
        mdio_write(tp, 0x06, 0x2b02);
        mdio_write(tp, 0x06, 0x8077);
        mdio_write(tp, 0x06, 0x0201);
        mdio_write(tp, 0x06, 0x4802);
        mdio_write(tp, 0x06, 0x0162);
        mdio_write(tp, 0x06, 0x0280);
        mdio_write(tp, 0x06, 0x9402);
        mdio_write(tp, 0x06, 0x810e);
        mdio_write(tp, 0x06, 0xe08b);
        mdio_write(tp, 0x06, 0x88e1);
        mdio_write(tp, 0x06, 0x8b89);
        mdio_write(tp, 0x06, 0x1e01);
        mdio_write(tp, 0x06, 0xe18b);
        mdio_write(tp, 0x06, 0x8a1e);
        mdio_write(tp, 0x06, 0x01e1);
        mdio_write(tp, 0x06, 0x8b8b);
        mdio_write(tp, 0x06, 0x1e01);
        mdio_write(tp, 0x06, 0xe18b);
        mdio_write(tp, 0x06, 0x8c1e);
        mdio_write(tp, 0x06, 0x01e1);
        mdio_write(tp, 0x06, 0x8b8d);
        mdio_write(tp, 0x06, 0x1e01);
        mdio_write(tp, 0x06, 0xe18b);
        mdio_write(tp, 0x06, 0x8e1e);
        mdio_write(tp, 0x06, 0x01a0);
        mdio_write(tp, 0x06, 0x00c7);
        mdio_write(tp, 0x06, 0xaebb);
        mdio_write(tp, 0x06, 0xd481);
        mdio_write(tp, 0x06, 0xd4e4);
        mdio_write(tp, 0x06, 0x8b92);
        mdio_write(tp, 0x06, 0xe58b);
        mdio_write(tp, 0x06, 0x9302);
        mdio_write(tp, 0x06, 0x2e5a);
        mdio_write(tp, 0x06, 0xbf8b);
        mdio_write(tp, 0x06, 0x88ec);
        mdio_write(tp, 0x06, 0x0019);
        mdio_write(tp, 0x06, 0xa98b);
        mdio_write(tp, 0x06, 0x90f9);
        mdio_write(tp, 0x06, 0xeeff);
        mdio_write(tp, 0x06, 0xf600);
        mdio_write(tp, 0x06, 0xeeff);
        mdio_write(tp, 0x06, 0xf7fc);
        mdio_write(tp, 0x06, 0xd100);
        mdio_write(tp, 0x06, 0xbf83);
        mdio_write(tp, 0x06, 0x3c02);
        mdio_write(tp, 0x06, 0x3a21);
        mdio_write(tp, 0x06, 0xd101);
        mdio_write(tp, 0x06, 0xbf83);
        mdio_write(tp, 0x06, 0x3f02);
        mdio_write(tp, 0x06, 0x3a21);
        mdio_write(tp, 0x06, 0x04f8);
        mdio_write(tp, 0x06, 0xe08b);
        mdio_write(tp, 0x06, 0x8aad);
        mdio_write(tp, 0x06, 0x2014);
        mdio_write(tp, 0x06, 0xee8b);
        mdio_write(tp, 0x06, 0x8a00);
        mdio_write(tp, 0x06, 0x0220);
        mdio_write(tp, 0x06, 0x8be0);
        mdio_write(tp, 0x06, 0xe426);
        mdio_write(tp, 0x06, 0xe1e4);
        mdio_write(tp, 0x06, 0x27ee);
        mdio_write(tp, 0x06, 0xe426);
        mdio_write(tp, 0x06, 0x23e5);
        mdio_write(tp, 0x06, 0xe427);
        mdio_write(tp, 0x06, 0xfc04);
        mdio_write(tp, 0x06, 0xf8e0);
        mdio_write(tp, 0x06, 0x8b8d);
        mdio_write(tp, 0x06, 0xad20);
        mdio_write(tp, 0x06, 0x14ee);
        mdio_write(tp, 0x06, 0x8b8d);
        mdio_write(tp, 0x06, 0x00e0);
        mdio_write(tp, 0x06, 0x8a5a);
        mdio_write(tp, 0x06, 0x7803);
        mdio_write(tp, 0x06, 0x9e09);
        mdio_write(tp, 0x06, 0x0206);
        mdio_write(tp, 0x06, 0x2802);
        mdio_write(tp, 0x06, 0x80b1);
        mdio_write(tp, 0x06, 0x0232);
        mdio_write(tp, 0x06, 0xfdfc);
        mdio_write(tp, 0x06, 0x04f8);
        mdio_write(tp, 0x06, 0xf9e0);
        mdio_write(tp, 0x06, 0x8b81);
        mdio_write(tp, 0x06, 0xac26);
        mdio_write(tp, 0x06, 0x1ae0);
        mdio_write(tp, 0x06, 0x8b81);
        mdio_write(tp, 0x06, 0xac21);
        mdio_write(tp, 0x06, 0x14e0);
        mdio_write(tp, 0x06, 0x8b85);
        mdio_write(tp, 0x06, 0xac20);
        mdio_write(tp, 0x06, 0x0ee0);
        mdio_write(tp, 0x06, 0x8b85);
        mdio_write(tp, 0x06, 0xac23);
        mdio_write(tp, 0x06, 0x08e0);
        mdio_write(tp, 0x06, 0x8b87);
        mdio_write(tp, 0x06, 0xac24);
        mdio_write(tp, 0x06, 0x02ae);
        mdio_write(tp, 0x06, 0x3802);
        mdio_write(tp, 0x06, 0x1b02);
        mdio_write(tp, 0x06, 0xeee4);
        mdio_write(tp, 0x06, 0x1c04);
        mdio_write(tp, 0x06, 0xeee4);
        mdio_write(tp, 0x06, 0x1d04);
        mdio_write(tp, 0x06, 0xe2e0);
        mdio_write(tp, 0x06, 0x7ce3);
        mdio_write(tp, 0x06, 0xe07d);
        mdio_write(tp, 0x06, 0xe0e0);
        mdio_write(tp, 0x06, 0x38e1);
        mdio_write(tp, 0x06, 0xe039);
        mdio_write(tp, 0x06, 0xad2e);
        mdio_write(tp, 0x06, 0x1bad);
        mdio_write(tp, 0x06, 0x390d);
        mdio_write(tp, 0x06, 0xd101);
        mdio_write(tp, 0x06, 0xbf22);
        mdio_write(tp, 0x06, 0xe802);
        mdio_write(tp, 0x06, 0x3a21);
        mdio_write(tp, 0x06, 0x0222);
        mdio_write(tp, 0x06, 0x10ae);
        mdio_write(tp, 0x06, 0x0bac);
        mdio_write(tp, 0x06, 0x3802);
        mdio_write(tp, 0x06, 0xae06);
        mdio_write(tp, 0x06, 0x0222);
        mdio_write(tp, 0x06, 0x4d02);
        mdio_write(tp, 0x06, 0x2292);
        mdio_write(tp, 0x06, 0x021b);
        mdio_write(tp, 0x06, 0x13fd);
        mdio_write(tp, 0x06, 0xfc04);
        mdio_write(tp, 0x06, 0xf8e0);
        mdio_write(tp, 0x06, 0x8b8e);
        mdio_write(tp, 0x06, 0xad20);
        mdio_write(tp, 0x06, 0x1af6);
        mdio_write(tp, 0x06, 0x20e4);
        mdio_write(tp, 0x06, 0x8b8e);
        mdio_write(tp, 0x06, 0x022b);
        mdio_write(tp, 0x06, 0x1e02);
        mdio_write(tp, 0x06, 0x82ae);
        mdio_write(tp, 0x06, 0x0203);
        mdio_write(tp, 0x06, 0xc002);
        mdio_write(tp, 0x06, 0x827d);
        mdio_write(tp, 0x06, 0x022e);
        mdio_write(tp, 0x06, 0x6f02);
        mdio_write(tp, 0x06, 0x047b);
        mdio_write(tp, 0x06, 0x022f);
        mdio_write(tp, 0x06, 0x9ae0);
        mdio_write(tp, 0x06, 0x8b8e);
        mdio_write(tp, 0x06, 0xad21);
        mdio_write(tp, 0x06, 0x0bf6);
        mdio_write(tp, 0x06, 0x21e4);
        mdio_write(tp, 0x06, 0x8b8e);
        mdio_write(tp, 0x06, 0x0281);
        mdio_write(tp, 0x06, 0x9002);
        mdio_write(tp, 0x06, 0x1cd9);
        mdio_write(tp, 0x06, 0xe08b);
        mdio_write(tp, 0x06, 0x8ead);
        mdio_write(tp, 0x06, 0x2208);
        mdio_write(tp, 0x06, 0xf622);
        mdio_write(tp, 0x06, 0xe48b);
        mdio_write(tp, 0x06, 0x8e02);
        mdio_write(tp, 0x06, 0x35f4);
        mdio_write(tp, 0x06, 0xe08b);
        mdio_write(tp, 0x06, 0x8ead);
        mdio_write(tp, 0x06, 0x2308);
        mdio_write(tp, 0x06, 0xf623);
        mdio_write(tp, 0x06, 0xe48b);
        mdio_write(tp, 0x06, 0x8e02);
        mdio_write(tp, 0x06, 0x31e8);
        mdio_write(tp, 0x06, 0xe08b);
        mdio_write(tp, 0x06, 0x8ead);
        mdio_write(tp, 0x06, 0x2405);
        mdio_write(tp, 0x06, 0xf624);
        mdio_write(tp, 0x06, 0xe48b);
        mdio_write(tp, 0x06, 0x8ee0);
        mdio_write(tp, 0x06, 0x8b8e);
        mdio_write(tp, 0x06, 0xad25);
        mdio_write(tp, 0x06, 0x05f6);
        mdio_write(tp, 0x06, 0x25e4);
        mdio_write(tp, 0x06, 0x8b8e);
        mdio_write(tp, 0x06, 0xe08b);
        mdio_write(tp, 0x06, 0x8ead);
        mdio_write(tp, 0x06, 0x2608);
        mdio_write(tp, 0x06, 0xf626);
        mdio_write(tp, 0x06, 0xe48b);
        mdio_write(tp, 0x06, 0x8e02);
        mdio_write(tp, 0x06, 0x2d8a);
        mdio_write(tp, 0x06, 0xe08b);
        mdio_write(tp, 0x06, 0x8ead);
        mdio_write(tp, 0x06, 0x2705);
        mdio_write(tp, 0x06, 0xf627);
        mdio_write(tp, 0x06, 0xe48b);
        mdio_write(tp, 0x06, 0x8e02);
        mdio_write(tp, 0x06, 0x0386);
        mdio_write(tp, 0x06, 0xfc04);
        mdio_write(tp, 0x06, 0xf8fa);
        mdio_write(tp, 0x06, 0xef69);
        mdio_write(tp, 0x06, 0xe0e0);
        mdio_write(tp, 0x06, 0x00e1);
        mdio_write(tp, 0x06, 0xe001);
        mdio_write(tp, 0x06, 0xad27);
        mdio_write(tp, 0x06, 0x32e0);
        mdio_write(tp, 0x06, 0x8b40);
        mdio_write(tp, 0x06, 0xf720);
        mdio_write(tp, 0x06, 0xe48b);
        mdio_write(tp, 0x06, 0x40bf);
        mdio_write(tp, 0x06, 0x32c1);
        mdio_write(tp, 0x06, 0x0239);
        mdio_write(tp, 0x06, 0xf4ad);
        mdio_write(tp, 0x06, 0x2821);
        mdio_write(tp, 0x06, 0xe0e0);
        mdio_write(tp, 0x06, 0x20e1);
        mdio_write(tp, 0x06, 0xe021);
        mdio_write(tp, 0x06, 0xad20);
        mdio_write(tp, 0x06, 0x18e0);
        mdio_write(tp, 0x06, 0x8b40);
        mdio_write(tp, 0x06, 0xf620);
        mdio_write(tp, 0x06, 0xe48b);
        mdio_write(tp, 0x06, 0x40ee);
        mdio_write(tp, 0x06, 0x8b3b);
        mdio_write(tp, 0x06, 0xffe0);
        mdio_write(tp, 0x06, 0x8a8a);
        mdio_write(tp, 0x06, 0xe18a);
        mdio_write(tp, 0x06, 0x8be4);
        mdio_write(tp, 0x06, 0xe000);
        mdio_write(tp, 0x06, 0xe5e0);
        mdio_write(tp, 0x06, 0x01ef);
        mdio_write(tp, 0x06, 0x96fe);
        mdio_write(tp, 0x06, 0xfc04);
        mdio_write(tp, 0x06, 0xf8f9);
        mdio_write(tp, 0x06, 0xface);
        mdio_write(tp, 0x06, 0xfaef);
        mdio_write(tp, 0x06, 0x69fa);
        mdio_write(tp, 0x06, 0xd401);
        mdio_write(tp, 0x06, 0x55b4);
        mdio_write(tp, 0x06, 0xfebf);
        mdio_write(tp, 0x06, 0x1c5e);
        mdio_write(tp, 0x06, 0x0239);
        mdio_write(tp, 0x06, 0xf4ac);
        mdio_write(tp, 0x06, 0x280b);
        mdio_write(tp, 0x06, 0xbf1c);
        mdio_write(tp, 0x06, 0x5b02);
        mdio_write(tp, 0x06, 0x39f4);
        mdio_write(tp, 0x06, 0xac28);
        mdio_write(tp, 0x06, 0x49ae);
        mdio_write(tp, 0x06, 0x64bf);
        mdio_write(tp, 0x06, 0x1c5b);
        mdio_write(tp, 0x06, 0x0239);
        mdio_write(tp, 0x06, 0xf4ac);
        mdio_write(tp, 0x06, 0x285b);
        mdio_write(tp, 0x06, 0xd000);
        mdio_write(tp, 0x06, 0x0282);
        mdio_write(tp, 0x06, 0x62ac);
        mdio_write(tp, 0x06, 0x2105);
        mdio_write(tp, 0x06, 0xac22);
        mdio_write(tp, 0x06, 0x02ae);
        mdio_write(tp, 0x06, 0x4ebf);
        mdio_write(tp, 0x06, 0xe0c4);
        mdio_write(tp, 0x06, 0xbe85);
        mdio_write(tp, 0x06, 0xecd2);
        mdio_write(tp, 0x06, 0x04d8);
        mdio_write(tp, 0x06, 0x19d9);
        mdio_write(tp, 0x06, 0x1907);
        mdio_write(tp, 0x06, 0xdc19);
        mdio_write(tp, 0x06, 0xdd19);
        mdio_write(tp, 0x06, 0x0789);
        mdio_write(tp, 0x06, 0x89ef);
        mdio_write(tp, 0x06, 0x645e);
        mdio_write(tp, 0x06, 0x07ff);
        mdio_write(tp, 0x06, 0x0d65);
        mdio_write(tp, 0x06, 0x5cf8);
        mdio_write(tp, 0x06, 0x001e);
        mdio_write(tp, 0x06, 0x46dc);
        mdio_write(tp, 0x06, 0x19dd);
        mdio_write(tp, 0x06, 0x19b2);
        mdio_write(tp, 0x06, 0xe2d4);
        mdio_write(tp, 0x06, 0x0001);
        mdio_write(tp, 0x06, 0xbf1c);
        mdio_write(tp, 0x06, 0x5b02);
        mdio_write(tp, 0x06, 0x3a21);
        mdio_write(tp, 0x06, 0xae1d);
        mdio_write(tp, 0x06, 0xbee0);
        mdio_write(tp, 0x06, 0xc4bf);
        mdio_write(tp, 0x06, 0x85ec);
        mdio_write(tp, 0x06, 0xd204);
        mdio_write(tp, 0x06, 0xd819);
        mdio_write(tp, 0x06, 0xd919);
        mdio_write(tp, 0x06, 0x07dc);
        mdio_write(tp, 0x06, 0x19dd);
        mdio_write(tp, 0x06, 0x1907);
        mdio_write(tp, 0x06, 0xb2f4);
        mdio_write(tp, 0x06, 0xd400);
        mdio_write(tp, 0x06, 0x00bf);
        mdio_write(tp, 0x06, 0x1c5b);
        mdio_write(tp, 0x06, 0x023a);
        mdio_write(tp, 0x06, 0x21fe);
        mdio_write(tp, 0x06, 0xef96);
        mdio_write(tp, 0x06, 0xfec6);
        mdio_write(tp, 0x06, 0xfefd);
        mdio_write(tp, 0x06, 0xfc05);
        mdio_write(tp, 0x06, 0xf9e2);
        mdio_write(tp, 0x06, 0xe0ea);
        mdio_write(tp, 0x06, 0xe3e0);
        mdio_write(tp, 0x06, 0xeb5a);
        mdio_write(tp, 0x06, 0x070c);
        mdio_write(tp, 0x06, 0x031e);
        mdio_write(tp, 0x06, 0x20e6);
        mdio_write(tp, 0x06, 0xe0ea);
        mdio_write(tp, 0x06, 0xe7e0);
        mdio_write(tp, 0x06, 0xebe0);
        mdio_write(tp, 0x06, 0xe0fc);
        mdio_write(tp, 0x06, 0xe1e0);
        mdio_write(tp, 0x06, 0xfdfd);
        mdio_write(tp, 0x06, 0x04f8);
        mdio_write(tp, 0x06, 0xfaef);
        mdio_write(tp, 0x06, 0x69e0);
        mdio_write(tp, 0x06, 0x8b80);
        mdio_write(tp, 0x06, 0xad27);
        mdio_write(tp, 0x06, 0x22bf);
        mdio_write(tp, 0x06, 0x47ba);
        mdio_write(tp, 0x06, 0x0239);
        mdio_write(tp, 0x06, 0xf4e0);
        mdio_write(tp, 0x06, 0x8b44);
        mdio_write(tp, 0x06, 0x1f01);
        mdio_write(tp, 0x06, 0x9e15);
        mdio_write(tp, 0x06, 0xe58b);
        mdio_write(tp, 0x06, 0x44ad);
        mdio_write(tp, 0x06, 0x2907);
        mdio_write(tp, 0x06, 0xac28);
        mdio_write(tp, 0x06, 0x04d1);
        mdio_write(tp, 0x06, 0x01ae);
        mdio_write(tp, 0x06, 0x02d1);
        mdio_write(tp, 0x06, 0x00bf);
        mdio_write(tp, 0x06, 0x8342);
        mdio_write(tp, 0x06, 0x023a);
        mdio_write(tp, 0x06, 0x21ef);
        mdio_write(tp, 0x06, 0x96fe);
        mdio_write(tp, 0x06, 0xfc04);
        mdio_write(tp, 0x06, 0xf8e0);
        mdio_write(tp, 0x06, 0x8b85);
        mdio_write(tp, 0x06, 0xad26);
        mdio_write(tp, 0x06, 0x30e0);
        mdio_write(tp, 0x06, 0xe036);
        mdio_write(tp, 0x06, 0xe1e0);
        mdio_write(tp, 0x06, 0x37e1);
        mdio_write(tp, 0x06, 0x8b3f);
        mdio_write(tp, 0x06, 0x1f10);
        mdio_write(tp, 0x06, 0x9e23);
        mdio_write(tp, 0x06, 0xe48b);
        mdio_write(tp, 0x06, 0x3fac);
        mdio_write(tp, 0x06, 0x200b);
        mdio_write(tp, 0x06, 0xac21);
        mdio_write(tp, 0x06, 0x0dac);
        mdio_write(tp, 0x06, 0x250f);
        mdio_write(tp, 0x06, 0xac27);
        mdio_write(tp, 0x06, 0x11ae);
        mdio_write(tp, 0x06, 0x1202);
        mdio_write(tp, 0x06, 0x2cb5);
        mdio_write(tp, 0x06, 0xae0d);
        mdio_write(tp, 0x06, 0x0282);
        mdio_write(tp, 0x06, 0xe7ae);
        mdio_write(tp, 0x06, 0x0802);
        mdio_write(tp, 0x06, 0x2cd7);
        mdio_write(tp, 0x06, 0xae03);
        mdio_write(tp, 0x06, 0x022c);
        mdio_write(tp, 0x06, 0xeafc);
        mdio_write(tp, 0x06, 0x04f8);
        mdio_write(tp, 0x06, 0xfaef);
        mdio_write(tp, 0x06, 0x6902);
        mdio_write(tp, 0x06, 0x8304);
        mdio_write(tp, 0x06, 0xe0e0);
        mdio_write(tp, 0x06, 0x14e1);
        mdio_write(tp, 0x06, 0xe015);
        mdio_write(tp, 0x06, 0xad26);
        mdio_write(tp, 0x06, 0x08d1);
        mdio_write(tp, 0x06, 0x1ebf);
        mdio_write(tp, 0x06, 0x2d47);
        mdio_write(tp, 0x06, 0x023a);
        mdio_write(tp, 0x06, 0x21ef);
        mdio_write(tp, 0x06, 0x96fe);
        mdio_write(tp, 0x06, 0xfc04);
        mdio_write(tp, 0x06, 0xf8e0);
        mdio_write(tp, 0x06, 0x8b85);
        mdio_write(tp, 0x06, 0xad27);
        mdio_write(tp, 0x06, 0x2fd0);
        mdio_write(tp, 0x06, 0x0b02);
        mdio_write(tp, 0x06, 0x3826);
        mdio_write(tp, 0x06, 0x5882);
        mdio_write(tp, 0x06, 0x7882);
        mdio_write(tp, 0x06, 0x9f24);
        mdio_write(tp, 0x06, 0xe08b);
        mdio_write(tp, 0x06, 0x32e1);
        mdio_write(tp, 0x06, 0x8b33);
        mdio_write(tp, 0x06, 0x1f10);
        mdio_write(tp, 0x06, 0x9e1a);
        mdio_write(tp, 0x06, 0x10e4);
        mdio_write(tp, 0x06, 0x8b32);
        mdio_write(tp, 0x06, 0xe0e0);
        mdio_write(tp, 0x06, 0x28e1);
        mdio_write(tp, 0x06, 0xe029);
        mdio_write(tp, 0x06, 0xf72c);
        mdio_write(tp, 0x06, 0xe4e0);
        mdio_write(tp, 0x06, 0x28e5);
        mdio_write(tp, 0x06, 0xe029);
        mdio_write(tp, 0x06, 0xf62c);
        mdio_write(tp, 0x06, 0xe4e0);
        mdio_write(tp, 0x06, 0x28e5);
        mdio_write(tp, 0x06, 0xe029);
        mdio_write(tp, 0x06, 0xfc04);
        mdio_write(tp, 0x06, 0x00e1);
        mdio_write(tp, 0x06, 0x4077);
        mdio_write(tp, 0x06, 0xe140);
        mdio_write(tp, 0x06, 0xbbe0);
        mdio_write(tp, 0x06, 0x2a00);
        mdio_write(tp, 0x05, 0xe142);
        gphy_val = mdio_read(tp, 0x06);
        gphy_val |= BIT_0;
        mdio_write(tp,0x06, gphy_val);
        mdio_write(tp, 0x05, 0xe140);
        gphy_val = mdio_read(tp, 0x06);
        gphy_val |= BIT_0;
        mdio_write(tp,0x06, gphy_val);
        mdio_write(tp, 0x1f, 0x0000);
        mdio_write(tp,0x1f, 0x0005);
        for (i = 0; i < 200; i++) {
                udelay(100);
                gphy_val = mdio_read(tp, 0x00);
                if (gphy_val & BIT_7)
                        break;
        }
        mdio_write(tp,0x1f, 0x0007);
        mdio_write(tp,0x1e, 0x0023);
        gphy_val = mdio_read(tp, 0x17);
        gphy_val |= BIT_1;
        if (tp->RequiredSecLanDonglePatch)
                gphy_val &= ~BIT_2;
        mdio_write(tp,0x17, gphy_val);
        mdio_write(tp,0x1f, 0x0000);

        mdio_write(tp, 0x1F, 0x0003);
        mdio_write(tp, 0x09, 0xA20F);
        mdio_write(tp, 0x1F, 0x0000);
        mdio_write(tp, 0x1f, 0x0003);
        mdio_write(tp, 0x01, 0x328A);
        mdio_write(tp, 0x1f, 0x0000);

        mdio_write(tp,0x1f, 0x0000);
        mdio_write(tp,0x00, 0x9200);
}

static void
rtl8168_set_phy_mcu_8168g_1(struct net_device *dev)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        unsigned int gphy_val,i;

        mdio_write(tp, 0x1f, 0x0B82);
        gphy_val = mdio_read(tp, 0x10);
        gphy_val |= BIT_4;
        mdio_write(tp, 0x10, gphy_val);
        mdio_write(tp, 0x1f, 0x0B80);
        for (i = 0; i < 10; i++) {
                if (mdio_read(tp, 0x10) & 0x0040)
                        break;
                mdelay(10);
        }
        mdio_write(tp, 0x1f, 0x0A43);
        mdio_write(tp, 0x13, 0x8146);
        mdio_write(tp, 0x14, 0x2300);
        mdio_write(tp, 0x13, 0xB820);
        mdio_write(tp, 0x14, 0x0210);

        mdio_write(tp, 0x1F, 0x0A43);
        mdio_write(tp, 0x13, 0xB820);
        mdio_write(tp, 0x14, 0x0290);
        mdio_write(tp, 0x13, 0xA012);
        mdio_write(tp, 0x14, 0x0000);
        mdio_write(tp, 0x13, 0xA014);
        mdio_write(tp, 0x14, 0x2c04);
        mdio_write(tp, 0x14, 0x2c0c);
        mdio_write(tp, 0x14, 0x2c6c);
        mdio_write(tp, 0x14, 0x2d0d);
        mdio_write(tp, 0x14, 0x31ce);
        mdio_write(tp, 0x14, 0x506d);
        mdio_write(tp, 0x14, 0xd708);
        mdio_write(tp, 0x14, 0x3108);
        mdio_write(tp, 0x14, 0x106d);
        mdio_write(tp, 0x14, 0x1560);
        mdio_write(tp, 0x14, 0x15a9);
        mdio_write(tp, 0x14, 0x206e);
        mdio_write(tp, 0x14, 0x175b);
        mdio_write(tp, 0x14, 0x6062);
        mdio_write(tp, 0x14, 0xd700);
        mdio_write(tp, 0x14, 0x5fae);
        mdio_write(tp, 0x14, 0xd708);
        mdio_write(tp, 0x14, 0x3107);
        mdio_write(tp, 0x14, 0x4c1e);
        mdio_write(tp, 0x14, 0x4169);
        mdio_write(tp, 0x14, 0x316a);
        mdio_write(tp, 0x14, 0x0c19);
        mdio_write(tp, 0x14, 0x31aa);
        mdio_write(tp, 0x14, 0x0c19);
        mdio_write(tp, 0x14, 0x2c1b);
        mdio_write(tp, 0x14, 0x5e62);
        mdio_write(tp, 0x14, 0x26b5);
        mdio_write(tp, 0x14, 0x31ab);
        mdio_write(tp, 0x14, 0x5c1e);
        mdio_write(tp, 0x14, 0x2c0c);
        mdio_write(tp, 0x14, 0xc040);
        mdio_write(tp, 0x14, 0x8808);
        mdio_write(tp, 0x14, 0xc520);
        mdio_write(tp, 0x14, 0xc421);
        mdio_write(tp, 0x14, 0xd05a);
        mdio_write(tp, 0x14, 0xd19a);
        mdio_write(tp, 0x14, 0xd709);
        mdio_write(tp, 0x14, 0x608f);
        mdio_write(tp, 0x14, 0xd06b);
        mdio_write(tp, 0x14, 0xd18a);
        mdio_write(tp, 0x14, 0x2c2c);
        mdio_write(tp, 0x14, 0xd0be);
        mdio_write(tp, 0x14, 0xd188);
        mdio_write(tp, 0x14, 0x2c2c);
        mdio_write(tp, 0x14, 0xd708);
        mdio_write(tp, 0x14, 0x4072);
        mdio_write(tp, 0x14, 0xc104);
        mdio_write(tp, 0x14, 0x2c3e);
        mdio_write(tp, 0x14, 0x4076);
        mdio_write(tp, 0x14, 0xc110);
        mdio_write(tp, 0x14, 0x2c3e);
        mdio_write(tp, 0x14, 0x4071);
        mdio_write(tp, 0x14, 0xc102);
        mdio_write(tp, 0x14, 0x2c3e);
        mdio_write(tp, 0x14, 0x4070);
        mdio_write(tp, 0x14, 0xc101);
        mdio_write(tp, 0x14, 0x2c3e);
        mdio_write(tp, 0x14, 0x175b);
        mdio_write(tp, 0x14, 0xd709);
        mdio_write(tp, 0x14, 0x3390);
        mdio_write(tp, 0x14, 0x5c39);
        mdio_write(tp, 0x14, 0x2c4e);
        mdio_write(tp, 0x14, 0x175b);
        mdio_write(tp, 0x14, 0xd708);
        mdio_write(tp, 0x14, 0x6193);
        mdio_write(tp, 0x14, 0xd709);
        mdio_write(tp, 0x14, 0x5f9d);
        mdio_write(tp, 0x14, 0x408b);
        mdio_write(tp, 0x14, 0xd71e);
        mdio_write(tp, 0x14, 0x6042);
        mdio_write(tp, 0x14, 0xb401);
        mdio_write(tp, 0x14, 0x175b);
        mdio_write(tp, 0x14, 0xd708);
        mdio_write(tp, 0x14, 0x6073);
        mdio_write(tp, 0x14, 0x5fbc);
        mdio_write(tp, 0x14, 0x2c4d);
        mdio_write(tp, 0x14, 0x26ed);
        mdio_write(tp, 0x14, 0xb280);
        mdio_write(tp, 0x14, 0xa841);
        mdio_write(tp, 0x14, 0x9420);
        mdio_write(tp, 0x14, 0x8710);
        mdio_write(tp, 0x14, 0xd709);
        mdio_write(tp, 0x14, 0x42ec);
        mdio_write(tp, 0x14, 0x606d);
        mdio_write(tp, 0x14, 0xd207);
        mdio_write(tp, 0x14, 0x2c57);
        mdio_write(tp, 0x14, 0xd203);
        mdio_write(tp, 0x14, 0x33ff);
        mdio_write(tp, 0x14, 0x563b);
        mdio_write(tp, 0x14, 0x3275);
        mdio_write(tp, 0x14, 0x7c5e);
        mdio_write(tp, 0x14, 0xb240);
        mdio_write(tp, 0x14, 0xb402);
        mdio_write(tp, 0x14, 0x263b);
        mdio_write(tp, 0x14, 0x6096);
        mdio_write(tp, 0x14, 0xb240);
        mdio_write(tp, 0x14, 0xb406);
        mdio_write(tp, 0x14, 0x263b);
        mdio_write(tp, 0x14, 0x31d7);
        mdio_write(tp, 0x14, 0x7c67);
        mdio_write(tp, 0x14, 0xb240);
        mdio_write(tp, 0x14, 0xb40e);
        mdio_write(tp, 0x14, 0x263b);
        mdio_write(tp, 0x14, 0xb410);
        mdio_write(tp, 0x14, 0x8802);
        mdio_write(tp, 0x14, 0xb240);
        mdio_write(tp, 0x14, 0x940e);
        mdio_write(tp, 0x14, 0x263b);
        mdio_write(tp, 0x14, 0xba04);
        mdio_write(tp, 0x14, 0x1cd6);
        mdio_write(tp, 0x14, 0xa902);
        mdio_write(tp, 0x14, 0xd711);
        mdio_write(tp, 0x14, 0x4045);
        mdio_write(tp, 0x14, 0xa980);
        mdio_write(tp, 0x14, 0x3003);
        mdio_write(tp, 0x14, 0x59b1);
        mdio_write(tp, 0x14, 0xa540);
        mdio_write(tp, 0x14, 0xa601);
        mdio_write(tp, 0x14, 0xd710);
        mdio_write(tp, 0x14, 0x4043);
        mdio_write(tp, 0x14, 0xa910);
        mdio_write(tp, 0x14, 0xd711);
        mdio_write(tp, 0x14, 0x60a0);
        mdio_write(tp, 0x14, 0xca33);
        mdio_write(tp, 0x14, 0xcb33);
        mdio_write(tp, 0x14, 0xa941);
        mdio_write(tp, 0x14, 0x2c82);
        mdio_write(tp, 0x14, 0xcaff);
        mdio_write(tp, 0x14, 0xcbff);
        mdio_write(tp, 0x14, 0xa921);
        mdio_write(tp, 0x14, 0xce02);
        mdio_write(tp, 0x14, 0xe070);
        mdio_write(tp, 0x14, 0x0f10);
        mdio_write(tp, 0x14, 0xaf01);
        mdio_write(tp, 0x14, 0x8f01);
        mdio_write(tp, 0x14, 0x1766);
        mdio_write(tp, 0x14, 0x8e02);
        mdio_write(tp, 0x14, 0x1787);
        mdio_write(tp, 0x14, 0xd710);
        mdio_write(tp, 0x14, 0x609c);
        mdio_write(tp, 0x14, 0xd71e);
        mdio_write(tp, 0x14, 0x7fa4);
        mdio_write(tp, 0x14, 0x2cd4);
        mdio_write(tp, 0x14, 0x1ce9);
        mdio_write(tp, 0x14, 0xce04);
        mdio_write(tp, 0x14, 0xe070);
        mdio_write(tp, 0x14, 0x0f20);
        mdio_write(tp, 0x14, 0xaf01);
        mdio_write(tp, 0x14, 0x8f01);
        mdio_write(tp, 0x14, 0x1766);
        mdio_write(tp, 0x14, 0x8e04);
        mdio_write(tp, 0x14, 0x6044);
        mdio_write(tp, 0x14, 0x2cd4);
        mdio_write(tp, 0x14, 0xa520);
        mdio_write(tp, 0x14, 0xd710);
        mdio_write(tp, 0x14, 0x4043);
        mdio_write(tp, 0x14, 0x2cc1);
        mdio_write(tp, 0x14, 0xe00f);
        mdio_write(tp, 0x14, 0x0501);
        mdio_write(tp, 0x14, 0x1cef);
        mdio_write(tp, 0x14, 0xb801);
        mdio_write(tp, 0x14, 0xd71e);
        mdio_write(tp, 0x14, 0x4060);
        mdio_write(tp, 0x14, 0x7fc4);
        mdio_write(tp, 0x14, 0x2cd4);
        mdio_write(tp, 0x14, 0x1cf5);
        mdio_write(tp, 0x14, 0xe00f);
        mdio_write(tp, 0x14, 0x0502);
        mdio_write(tp, 0x14, 0x1cef);
        mdio_write(tp, 0x14, 0xb802);
        mdio_write(tp, 0x14, 0xd71e);
        mdio_write(tp, 0x14, 0x4061);
        mdio_write(tp, 0x14, 0x7fc4);
        mdio_write(tp, 0x14, 0x2cd4);
        mdio_write(tp, 0x14, 0x1cf5);
        mdio_write(tp, 0x14, 0xe00f);
        mdio_write(tp, 0x14, 0x0504);
        mdio_write(tp, 0x14, 0xd710);
        mdio_write(tp, 0x14, 0x6099);
        mdio_write(tp, 0x14, 0xd71e);
        mdio_write(tp, 0x14, 0x7fa4);
        mdio_write(tp, 0x14, 0x2cd4);
        mdio_write(tp, 0x14, 0xc17f);
        mdio_write(tp, 0x14, 0xc200);
        mdio_write(tp, 0x14, 0xc43f);
        mdio_write(tp, 0x14, 0xcc03);
        mdio_write(tp, 0x14, 0xa701);
        mdio_write(tp, 0x14, 0xa510);
        mdio_write(tp, 0x14, 0xd710);
        mdio_write(tp, 0x14, 0x4018);
        mdio_write(tp, 0x14, 0x9910);
        mdio_write(tp, 0x14, 0x8510);
        mdio_write(tp, 0x14, 0x2860);
        mdio_write(tp, 0x14, 0xe00f);
        mdio_write(tp, 0x14, 0x0504);
        mdio_write(tp, 0x14, 0xd710);
        mdio_write(tp, 0x14, 0x6099);
        mdio_write(tp, 0x14, 0xd71e);
        mdio_write(tp, 0x14, 0x7fa4);
        mdio_write(tp, 0x14, 0x2cd4);
        mdio_write(tp, 0x14, 0xa608);
        mdio_write(tp, 0x14, 0xc17d);
        mdio_write(tp, 0x14, 0xc200);
        mdio_write(tp, 0x14, 0xc43f);
        mdio_write(tp, 0x14, 0xcc03);
        mdio_write(tp, 0x14, 0xa701);
        mdio_write(tp, 0x14, 0xa510);
        mdio_write(tp, 0x14, 0xd710);
        mdio_write(tp, 0x14, 0x4018);
        mdio_write(tp, 0x14, 0x9910);
        mdio_write(tp, 0x14, 0x8510);
        mdio_write(tp, 0x14, 0x2926);
        mdio_write(tp, 0x14, 0x1792);
        mdio_write(tp, 0x14, 0x27db);
        mdio_write(tp, 0x14, 0xc000);
        mdio_write(tp, 0x14, 0xc100);
        mdio_write(tp, 0x14, 0xc200);
        mdio_write(tp, 0x14, 0xc300);
        mdio_write(tp, 0x14, 0xc400);
        mdio_write(tp, 0x14, 0xc500);
        mdio_write(tp, 0x14, 0xc600);
        mdio_write(tp, 0x14, 0xc7c1);
        mdio_write(tp, 0x14, 0xc800);
        mdio_write(tp, 0x14, 0xcc00);
        mdio_write(tp, 0x14, 0x0800);
        mdio_write(tp, 0x14, 0xca0f);
        mdio_write(tp, 0x14, 0xcbff);
        mdio_write(tp, 0x14, 0xa901);
        mdio_write(tp, 0x14, 0x8902);
        mdio_write(tp, 0x14, 0xc900);
        mdio_write(tp, 0x14, 0xca00);
        mdio_write(tp, 0x14, 0xcb00);
        mdio_write(tp, 0x14, 0x0800);
        mdio_write(tp, 0x14, 0xb804);
        mdio_write(tp, 0x14, 0x0800);
        mdio_write(tp, 0x14, 0xd71e);
        mdio_write(tp, 0x14, 0x6044);
        mdio_write(tp, 0x14, 0x9804);
        mdio_write(tp, 0x14, 0x0800);
        mdio_write(tp, 0x14, 0xd710);
        mdio_write(tp, 0x14, 0x6099);
        mdio_write(tp, 0x14, 0xd71e);
        mdio_write(tp, 0x14, 0x7fa4);
        mdio_write(tp, 0x14, 0x2cd4);
        mdio_write(tp, 0x14, 0x0800);
        mdio_write(tp, 0x14, 0xa510);
        mdio_write(tp, 0x14, 0xd710);
        mdio_write(tp, 0x14, 0x6098);
        mdio_write(tp, 0x14, 0xd71e);
        mdio_write(tp, 0x14, 0x7fa4);
        mdio_write(tp, 0x14, 0x2cd4);
        mdio_write(tp, 0x14, 0x8510);
        mdio_write(tp, 0x14, 0x0800);
        mdio_write(tp, 0x14, 0xd711);
        mdio_write(tp, 0x14, 0x3003);
        mdio_write(tp, 0x14, 0x1d01);
        mdio_write(tp, 0x14, 0x2d0b);
        mdio_write(tp, 0x14, 0xd710);
        mdio_write(tp, 0x14, 0x60be);
        mdio_write(tp, 0x14, 0xe060);
        mdio_write(tp, 0x14, 0x0920);
        mdio_write(tp, 0x14, 0x1cd6);
        mdio_write(tp, 0x14, 0x2c89);
        mdio_write(tp, 0x14, 0xd71e);
        mdio_write(tp, 0x14, 0x3063);
        mdio_write(tp, 0x14, 0x1948);
        mdio_write(tp, 0x14, 0x288a);
        mdio_write(tp, 0x14, 0x1cd6);
        mdio_write(tp, 0x14, 0x29bd);
        mdio_write(tp, 0x14, 0xa802);
        mdio_write(tp, 0x14, 0xa303);
        mdio_write(tp, 0x14, 0x843f);
        mdio_write(tp, 0x14, 0x81ff);
        mdio_write(tp, 0x14, 0x8208);
        mdio_write(tp, 0x14, 0xa201);
        mdio_write(tp, 0x14, 0xc001);
        mdio_write(tp, 0x14, 0xd710);
        mdio_write(tp, 0x14, 0x30a0);
        mdio_write(tp, 0x14, 0x0d1c);
        mdio_write(tp, 0x14, 0x30a0);
        mdio_write(tp, 0x14, 0x3d13);
        mdio_write(tp, 0x14, 0xd71e);
        mdio_write(tp, 0x14, 0x7f4c);
        mdio_write(tp, 0x14, 0x2ab6);
        mdio_write(tp, 0x14, 0xe003);
        mdio_write(tp, 0x14, 0x0202);
        mdio_write(tp, 0x14, 0xd710);
        mdio_write(tp, 0x14, 0x6090);
        mdio_write(tp, 0x14, 0xd71e);
        mdio_write(tp, 0x14, 0x7fac);
        mdio_write(tp, 0x14, 0x2ab6);
        mdio_write(tp, 0x14, 0xa20c);
        mdio_write(tp, 0x14, 0xd710);
        mdio_write(tp, 0x14, 0x6091);
        mdio_write(tp, 0x14, 0xd71e);
        mdio_write(tp, 0x14, 0x7fac);
        mdio_write(tp, 0x14, 0x2ab6);
        mdio_write(tp, 0x14, 0x820e);
        mdio_write(tp, 0x14, 0xa3e0);
        mdio_write(tp, 0x14, 0xa520);
        mdio_write(tp, 0x14, 0xd710);
        mdio_write(tp, 0x14, 0x609d);
        mdio_write(tp, 0x14, 0xd71e);
        mdio_write(tp, 0x14, 0x7fac);
        mdio_write(tp, 0x14, 0x2ab6);
        mdio_write(tp, 0x14, 0x8520);
        mdio_write(tp, 0x14, 0x6703);
        mdio_write(tp, 0x14, 0x2d34);
        mdio_write(tp, 0x14, 0xa13e);
        mdio_write(tp, 0x14, 0xc001);
        mdio_write(tp, 0x14, 0xd710);
        mdio_write(tp, 0x14, 0x4000);
        mdio_write(tp, 0x14, 0x6046);
        mdio_write(tp, 0x14, 0x2d0d);
        mdio_write(tp, 0x14, 0xa43f);
        mdio_write(tp, 0x14, 0xa101);
        mdio_write(tp, 0x14, 0xc020);
        mdio_write(tp, 0x14, 0xd710);
        mdio_write(tp, 0x14, 0x3121);
        mdio_write(tp, 0x14, 0x0d45);
        mdio_write(tp, 0x14, 0x30c0);
        mdio_write(tp, 0x14, 0x3d0d);
        mdio_write(tp, 0x14, 0xd71e);
        mdio_write(tp, 0x14, 0x7f4c);
        mdio_write(tp, 0x14, 0x2ab6);
        mdio_write(tp, 0x14, 0xa540);
        mdio_write(tp, 0x14, 0xc001);
        mdio_write(tp, 0x14, 0xd710);
        mdio_write(tp, 0x14, 0x4001);
        mdio_write(tp, 0x14, 0xe00f);
        mdio_write(tp, 0x14, 0x0501);
        mdio_write(tp, 0x14, 0x1dac);
        mdio_write(tp, 0x14, 0xc1c4);
        mdio_write(tp, 0x14, 0xa268);
        mdio_write(tp, 0x14, 0xa303);
        mdio_write(tp, 0x14, 0x8420);
        mdio_write(tp, 0x14, 0xe00f);
        mdio_write(tp, 0x14, 0x0502);
        mdio_write(tp, 0x14, 0x1dac);
        mdio_write(tp, 0x14, 0xc002);
        mdio_write(tp, 0x14, 0xd710);
        mdio_write(tp, 0x14, 0x4000);
        mdio_write(tp, 0x14, 0x8208);
        mdio_write(tp, 0x14, 0x8410);
        mdio_write(tp, 0x14, 0xa121);
        mdio_write(tp, 0x14, 0xc002);
        mdio_write(tp, 0x14, 0xd710);
        mdio_write(tp, 0x14, 0x4000);
        mdio_write(tp, 0x14, 0x8120);
        mdio_write(tp, 0x14, 0x8180);
        mdio_write(tp, 0x14, 0x1d97);
        mdio_write(tp, 0x14, 0xa180);
        mdio_write(tp, 0x14, 0xa13a);
        mdio_write(tp, 0x14, 0x8240);
        mdio_write(tp, 0x14, 0xa430);
        mdio_write(tp, 0x14, 0xc010);
        mdio_write(tp, 0x14, 0xd710);
        mdio_write(tp, 0x14, 0x30e1);
        mdio_write(tp, 0x14, 0x0abc);
        mdio_write(tp, 0x14, 0xd71e);
        mdio_write(tp, 0x14, 0x7f8c);
        mdio_write(tp, 0x14, 0x2ab6);
        mdio_write(tp, 0x14, 0xa480);
        mdio_write(tp, 0x14, 0xa230);
        mdio_write(tp, 0x14, 0xa303);
        mdio_write(tp, 0x14, 0xc001);
        mdio_write(tp, 0x14, 0xd70c);
        mdio_write(tp, 0x14, 0x4124);
        mdio_write(tp, 0x14, 0xd710);
        mdio_write(tp, 0x14, 0x6120);
        mdio_write(tp, 0x14, 0xd711);
        mdio_write(tp, 0x14, 0x3128);
        mdio_write(tp, 0x14, 0x3d76);
        mdio_write(tp, 0x14, 0x2d70);
        mdio_write(tp, 0x14, 0xa801);
        mdio_write(tp, 0x14, 0x2d6c);
        mdio_write(tp, 0x14, 0xd710);
        mdio_write(tp, 0x14, 0x4000);
        mdio_write(tp, 0x14, 0xe018);
        mdio_write(tp, 0x14, 0x0208);
        mdio_write(tp, 0x14, 0xa1f8);
        mdio_write(tp, 0x14, 0x8480);
        mdio_write(tp, 0x14, 0xc004);
        mdio_write(tp, 0x14, 0xd710);
        mdio_write(tp, 0x14, 0x4000);
        mdio_write(tp, 0x14, 0x6046);
        mdio_write(tp, 0x14, 0x2d0d);
        mdio_write(tp, 0x14, 0xa43f);
        mdio_write(tp, 0x14, 0xa105);
        mdio_write(tp, 0x14, 0x8228);
        mdio_write(tp, 0x14, 0xc004);
        mdio_write(tp, 0x14, 0xd710);
        mdio_write(tp, 0x14, 0x4000);
        mdio_write(tp, 0x14, 0x81bc);
        mdio_write(tp, 0x14, 0xa220);
        mdio_write(tp, 0x14, 0x1d97);
        mdio_write(tp, 0x14, 0x8220);
        mdio_write(tp, 0x14, 0xa1bc);
        mdio_write(tp, 0x14, 0xc040);
        mdio_write(tp, 0x14, 0xd710);
        mdio_write(tp, 0x14, 0x30e1);
        mdio_write(tp, 0x14, 0x0abc);
        mdio_write(tp, 0x14, 0x30e1);
        mdio_write(tp, 0x14, 0x3d0d);
        mdio_write(tp, 0x14, 0xd71e);
        mdio_write(tp, 0x14, 0x7f4c);
        mdio_write(tp, 0x14, 0x2ab6);
        mdio_write(tp, 0x14, 0xa802);
        mdio_write(tp, 0x14, 0xd70c);
        mdio_write(tp, 0x14, 0x4244);
        mdio_write(tp, 0x14, 0xa301);
        mdio_write(tp, 0x14, 0xc004);
        mdio_write(tp, 0x14, 0xd711);
        mdio_write(tp, 0x14, 0x3128);
        mdio_write(tp, 0x14, 0x3da5);
        mdio_write(tp, 0x14, 0xd710);
        mdio_write(tp, 0x14, 0x5f80);
        mdio_write(tp, 0x14, 0xd711);
        mdio_write(tp, 0x14, 0x3109);
        mdio_write(tp, 0x14, 0x3da7);
        mdio_write(tp, 0x14, 0x2dab);
        mdio_write(tp, 0x14, 0xa801);
        mdio_write(tp, 0x14, 0x2d9a);
        mdio_write(tp, 0x14, 0xa802);
        mdio_write(tp, 0x14, 0xc004);
        mdio_write(tp, 0x14, 0xd710);
        mdio_write(tp, 0x14, 0x4000);
        mdio_write(tp, 0x14, 0x0800);
        mdio_write(tp, 0x14, 0xa510);
        mdio_write(tp, 0x14, 0xd710);
        mdio_write(tp, 0x14, 0x609a);
        mdio_write(tp, 0x14, 0xd71e);
        mdio_write(tp, 0x14, 0x7fac);
        mdio_write(tp, 0x14, 0x2ab6);
        mdio_write(tp, 0x14, 0x8510);
        mdio_write(tp, 0x14, 0x0800);
        mdio_write(tp, 0x13, 0xA01A);
        mdio_write(tp, 0x14, 0x0000);
        mdio_write(tp, 0x13, 0xA006);
        mdio_write(tp, 0x14, 0x0ad6);
        mdio_write(tp, 0x13, 0xA004);
        mdio_write(tp, 0x14, 0x07f5);
        mdio_write(tp, 0x13, 0xA002);
        mdio_write(tp, 0x14, 0x06a9);
        mdio_write(tp, 0x13, 0xA000);
        mdio_write(tp, 0x14, 0xf069);
        mdio_write(tp, 0x13, 0xB820);
        mdio_write(tp, 0x14, 0x0210);

        mdio_write(tp, 0x1F, 0x0A43);
        mdio_write(tp, 0x13, 0x83a0);
        mdio_write(tp, 0x14, 0xaf83);
        mdio_write(tp, 0x14, 0xacaf);
        mdio_write(tp, 0x14, 0x83b8);
        mdio_write(tp, 0x14, 0xaf83);
        mdio_write(tp, 0x14, 0xcdaf);
        mdio_write(tp, 0x14, 0x83d3);
        mdio_write(tp, 0x14, 0x0204);
        mdio_write(tp, 0x14, 0x9a02);
        mdio_write(tp, 0x14, 0x09a9);
        mdio_write(tp, 0x14, 0x0284);
        mdio_write(tp, 0x14, 0x61af);
        mdio_write(tp, 0x14, 0x02fc);
        mdio_write(tp, 0x14, 0xad20);
        mdio_write(tp, 0x14, 0x0302);
        mdio_write(tp, 0x14, 0x867c);
        mdio_write(tp, 0x14, 0xad21);
        mdio_write(tp, 0x14, 0x0302);
        mdio_write(tp, 0x14, 0x85c9);
        mdio_write(tp, 0x14, 0xad22);
        mdio_write(tp, 0x14, 0x0302);
        mdio_write(tp, 0x14, 0x1bc0);
        mdio_write(tp, 0x14, 0xaf17);
        mdio_write(tp, 0x14, 0xe302);
        mdio_write(tp, 0x14, 0x8703);
        mdio_write(tp, 0x14, 0xaf18);
        mdio_write(tp, 0x14, 0x6201);
        mdio_write(tp, 0x14, 0x06e0);
        mdio_write(tp, 0x14, 0x8148);
        mdio_write(tp, 0x14, 0xaf3c);
        mdio_write(tp, 0x14, 0x69f8);
        mdio_write(tp, 0x14, 0xf9fa);
        mdio_write(tp, 0x14, 0xef69);
        mdio_write(tp, 0x14, 0xee80);
        mdio_write(tp, 0x14, 0x10f7);
        mdio_write(tp, 0x14, 0xee80);
        mdio_write(tp, 0x14, 0x131f);
        mdio_write(tp, 0x14, 0xd104);
        mdio_write(tp, 0x14, 0xbf87);
        mdio_write(tp, 0x14, 0xf302);
        mdio_write(tp, 0x14, 0x4259);
        mdio_write(tp, 0x14, 0x0287);
        mdio_write(tp, 0x14, 0x88bf);
        mdio_write(tp, 0x14, 0x87cf);
        mdio_write(tp, 0x14, 0xd7b8);
        mdio_write(tp, 0x14, 0x22d0);
        mdio_write(tp, 0x14, 0x0c02);
        mdio_write(tp, 0x14, 0x4252);
        mdio_write(tp, 0x14, 0xee80);
        mdio_write(tp, 0x14, 0xcda0);
        mdio_write(tp, 0x14, 0xee80);
        mdio_write(tp, 0x14, 0xce8b);
        mdio_write(tp, 0x14, 0xee80);
        mdio_write(tp, 0x14, 0xd1f5);
        mdio_write(tp, 0x14, 0xee80);
        mdio_write(tp, 0x14, 0xd2a9);
        mdio_write(tp, 0x14, 0xee80);
        mdio_write(tp, 0x14, 0xd30a);
        mdio_write(tp, 0x14, 0xee80);
        mdio_write(tp, 0x14, 0xf010);
        mdio_write(tp, 0x14, 0xee80);
        mdio_write(tp, 0x14, 0xf38f);
        mdio_write(tp, 0x14, 0xee81);
        mdio_write(tp, 0x14, 0x011e);
        mdio_write(tp, 0x14, 0xee81);
        mdio_write(tp, 0x14, 0x0b4a);
        mdio_write(tp, 0x14, 0xee81);
        mdio_write(tp, 0x14, 0x0c7c);
        mdio_write(tp, 0x14, 0xee81);
        mdio_write(tp, 0x14, 0x127f);
        mdio_write(tp, 0x14, 0xd100);
        mdio_write(tp, 0x14, 0x0210);
        mdio_write(tp, 0x14, 0xb5ee);
        mdio_write(tp, 0x14, 0x8088);
        mdio_write(tp, 0x14, 0xa4ee);
        mdio_write(tp, 0x14, 0x8089);
        mdio_write(tp, 0x14, 0x44ee);
        mdio_write(tp, 0x14, 0x809a);
        mdio_write(tp, 0x14, 0xa4ee);
        mdio_write(tp, 0x14, 0x809b);
        mdio_write(tp, 0x14, 0x44ee);
        mdio_write(tp, 0x14, 0x809c);
        mdio_write(tp, 0x14, 0xa7ee);
        mdio_write(tp, 0x14, 0x80a5);
        mdio_write(tp, 0x14, 0xa7d2);
        mdio_write(tp, 0x14, 0x0002);
        mdio_write(tp, 0x14, 0x0e66);
        mdio_write(tp, 0x14, 0x0285);
        mdio_write(tp, 0x14, 0xc0ee);
        mdio_write(tp, 0x14, 0x87fc);
        mdio_write(tp, 0x14, 0x00e0);
        mdio_write(tp, 0x14, 0x8245);
        mdio_write(tp, 0x14, 0xf622);
        mdio_write(tp, 0x14, 0xe482);
        mdio_write(tp, 0x14, 0x45ef);
        mdio_write(tp, 0x14, 0x96fe);
        mdio_write(tp, 0x14, 0xfdfc);
        mdio_write(tp, 0x14, 0x0402);
        mdio_write(tp, 0x14, 0x847a);
        mdio_write(tp, 0x14, 0x0284);
        mdio_write(tp, 0x14, 0xb302);
        mdio_write(tp, 0x14, 0x0cab);
        mdio_write(tp, 0x14, 0x020c);
        mdio_write(tp, 0x14, 0xc402);
        mdio_write(tp, 0x14, 0x0cef);
        mdio_write(tp, 0x14, 0x020d);
        mdio_write(tp, 0x14, 0x0802);
        mdio_write(tp, 0x14, 0x0d33);
        mdio_write(tp, 0x14, 0x020c);
        mdio_write(tp, 0x14, 0x3d04);
        mdio_write(tp, 0x14, 0xf8fa);
        mdio_write(tp, 0x14, 0xef69);
        mdio_write(tp, 0x14, 0xe182);
        mdio_write(tp, 0x14, 0x2fac);
        mdio_write(tp, 0x14, 0x291a);
        mdio_write(tp, 0x14, 0xe082);
        mdio_write(tp, 0x14, 0x24ac);
        mdio_write(tp, 0x14, 0x2102);
        mdio_write(tp, 0x14, 0xae22);
        mdio_write(tp, 0x14, 0x0210);
        mdio_write(tp, 0x14, 0x57f6);
        mdio_write(tp, 0x14, 0x21e4);
        mdio_write(tp, 0x14, 0x8224);
        mdio_write(tp, 0x14, 0xd101);
        mdio_write(tp, 0x14, 0xbf44);
        mdio_write(tp, 0x14, 0xd202);
        mdio_write(tp, 0x14, 0x4259);
        mdio_write(tp, 0x14, 0xae10);
        mdio_write(tp, 0x14, 0x0212);
        mdio_write(tp, 0x14, 0x4cf6);
        mdio_write(tp, 0x14, 0x29e5);
        mdio_write(tp, 0x14, 0x822f);
        mdio_write(tp, 0x14, 0xe082);
        mdio_write(tp, 0x14, 0x24f6);
        mdio_write(tp, 0x14, 0x21e4);
        mdio_write(tp, 0x14, 0x8224);
        mdio_write(tp, 0x14, 0xef96);
        mdio_write(tp, 0x14, 0xfefc);
        mdio_write(tp, 0x14, 0x04f8);
        mdio_write(tp, 0x14, 0xe182);
        mdio_write(tp, 0x14, 0x2fac);
        mdio_write(tp, 0x14, 0x2a18);
        mdio_write(tp, 0x14, 0xe082);
        mdio_write(tp, 0x14, 0x24ac);
        mdio_write(tp, 0x14, 0x2202);
        mdio_write(tp, 0x14, 0xae26);
        mdio_write(tp, 0x14, 0x0284);
        mdio_write(tp, 0x14, 0xf802);
        mdio_write(tp, 0x14, 0x8565);
        mdio_write(tp, 0x14, 0xd101);
        mdio_write(tp, 0x14, 0xbf44);
        mdio_write(tp, 0x14, 0xd502);
        mdio_write(tp, 0x14, 0x4259);
        mdio_write(tp, 0x14, 0xae0e);
        mdio_write(tp, 0x14, 0x0284);
        mdio_write(tp, 0x14, 0xea02);
        mdio_write(tp, 0x14, 0x85a9);
        mdio_write(tp, 0x14, 0xe182);
        mdio_write(tp, 0x14, 0x2ff6);
        mdio_write(tp, 0x14, 0x2ae5);
        mdio_write(tp, 0x14, 0x822f);
        mdio_write(tp, 0x14, 0xe082);
        mdio_write(tp, 0x14, 0x24f6);
        mdio_write(tp, 0x14, 0x22e4);
        mdio_write(tp, 0x14, 0x8224);
        mdio_write(tp, 0x14, 0xfc04);
        mdio_write(tp, 0x14, 0xf9e2);
        mdio_write(tp, 0x14, 0x8011);
        mdio_write(tp, 0x14, 0xad31);
        mdio_write(tp, 0x14, 0x05d2);
        mdio_write(tp, 0x14, 0x0002);
        mdio_write(tp, 0x14, 0x0e66);
        mdio_write(tp, 0x14, 0xfd04);
        mdio_write(tp, 0x14, 0xf8f9);
        mdio_write(tp, 0x14, 0xfaef);
        mdio_write(tp, 0x14, 0x69e0);
        mdio_write(tp, 0x14, 0x8011);
        mdio_write(tp, 0x14, 0xad21);
        mdio_write(tp, 0x14, 0x5cbf);
        mdio_write(tp, 0x14, 0x43be);
        mdio_write(tp, 0x14, 0x0242);
        mdio_write(tp, 0x14, 0x97ac);
        mdio_write(tp, 0x14, 0x281b);
        mdio_write(tp, 0x14, 0xbf43);
        mdio_write(tp, 0x14, 0xc102);
        mdio_write(tp, 0x14, 0x4297);
        mdio_write(tp, 0x14, 0xac28);
        mdio_write(tp, 0x14, 0x12bf);
        mdio_write(tp, 0x14, 0x43c7);
        mdio_write(tp, 0x14, 0x0242);
        mdio_write(tp, 0x14, 0x97ac);
        mdio_write(tp, 0x14, 0x2804);
        mdio_write(tp, 0x14, 0xd300);
        mdio_write(tp, 0x14, 0xae07);
        mdio_write(tp, 0x14, 0xd306);
        mdio_write(tp, 0x14, 0xaf85);
        mdio_write(tp, 0x14, 0x56d3);
        mdio_write(tp, 0x14, 0x03e0);
        mdio_write(tp, 0x14, 0x8011);
        mdio_write(tp, 0x14, 0xad26);
        mdio_write(tp, 0x14, 0x25bf);
        mdio_write(tp, 0x14, 0x4559);
        mdio_write(tp, 0x14, 0x0242);
        mdio_write(tp, 0x14, 0x97e2);
        mdio_write(tp, 0x14, 0x8073);
        mdio_write(tp, 0x14, 0x0d21);
        mdio_write(tp, 0x14, 0xf637);
        mdio_write(tp, 0x14, 0x0d11);
        mdio_write(tp, 0x14, 0xf62f);
        mdio_write(tp, 0x14, 0x1b21);
        mdio_write(tp, 0x14, 0xaa02);
        mdio_write(tp, 0x14, 0xae10);
        mdio_write(tp, 0x14, 0xe280);
        mdio_write(tp, 0x14, 0x740d);
        mdio_write(tp, 0x14, 0x21f6);
        mdio_write(tp, 0x14, 0x371b);
        mdio_write(tp, 0x14, 0x21aa);
        mdio_write(tp, 0x14, 0x0313);
        mdio_write(tp, 0x14, 0xae02);
        mdio_write(tp, 0x14, 0x2b02);
        mdio_write(tp, 0x14, 0x020e);
        mdio_write(tp, 0x14, 0x5102);
        mdio_write(tp, 0x14, 0x0e66);
        mdio_write(tp, 0x14, 0x020f);
        mdio_write(tp, 0x14, 0xa3ef);
        mdio_write(tp, 0x14, 0x96fe);
        mdio_write(tp, 0x14, 0xfdfc);
        mdio_write(tp, 0x14, 0x04f8);
        mdio_write(tp, 0x14, 0xf9fa);
        mdio_write(tp, 0x14, 0xef69);
        mdio_write(tp, 0x14, 0xe080);
        mdio_write(tp, 0x14, 0x12ad);
        mdio_write(tp, 0x14, 0x2733);
        mdio_write(tp, 0x14, 0xbf43);
        mdio_write(tp, 0x14, 0xbe02);
        mdio_write(tp, 0x14, 0x4297);
        mdio_write(tp, 0x14, 0xac28);
        mdio_write(tp, 0x14, 0x09bf);
        mdio_write(tp, 0x14, 0x43c1);
        mdio_write(tp, 0x14, 0x0242);
        mdio_write(tp, 0x14, 0x97ad);
        mdio_write(tp, 0x14, 0x2821);
        mdio_write(tp, 0x14, 0xbf45);
        mdio_write(tp, 0x14, 0x5902);
        mdio_write(tp, 0x14, 0x4297);
        mdio_write(tp, 0x14, 0xe387);
        mdio_write(tp, 0x14, 0xffd2);
        mdio_write(tp, 0x14, 0x001b);
        mdio_write(tp, 0x14, 0x45ac);
        mdio_write(tp, 0x14, 0x2711);
        mdio_write(tp, 0x14, 0xe187);
        mdio_write(tp, 0x14, 0xfebf);
        mdio_write(tp, 0x14, 0x87e4);
        mdio_write(tp, 0x14, 0x0242);
        mdio_write(tp, 0x14, 0x590d);
        mdio_write(tp, 0x14, 0x11bf);
        mdio_write(tp, 0x14, 0x87e7);
        mdio_write(tp, 0x14, 0x0242);
        mdio_write(tp, 0x14, 0x59ef);
        mdio_write(tp, 0x14, 0x96fe);
        mdio_write(tp, 0x14, 0xfdfc);
        mdio_write(tp, 0x14, 0x04f8);
        mdio_write(tp, 0x14, 0xfaef);
        mdio_write(tp, 0x14, 0x69d1);
        mdio_write(tp, 0x14, 0x00bf);
        mdio_write(tp, 0x14, 0x87e4);
        mdio_write(tp, 0x14, 0x0242);
        mdio_write(tp, 0x14, 0x59bf);
        mdio_write(tp, 0x14, 0x87e7);
        mdio_write(tp, 0x14, 0x0242);
        mdio_write(tp, 0x14, 0x59ef);
        mdio_write(tp, 0x14, 0x96fe);
        mdio_write(tp, 0x14, 0xfc04);
        mdio_write(tp, 0x14, 0xee87);
        mdio_write(tp, 0x14, 0xff46);
        mdio_write(tp, 0x14, 0xee87);
        mdio_write(tp, 0x14, 0xfe01);
        mdio_write(tp, 0x14, 0x04f8);
        mdio_write(tp, 0x14, 0xfaef);
        mdio_write(tp, 0x14, 0x69e0);
        mdio_write(tp, 0x14, 0x8241);
        mdio_write(tp, 0x14, 0xa000);
        mdio_write(tp, 0x14, 0x0502);
        mdio_write(tp, 0x14, 0x85eb);
        mdio_write(tp, 0x14, 0xae0e);
        mdio_write(tp, 0x14, 0xa001);
        mdio_write(tp, 0x14, 0x0502);
        mdio_write(tp, 0x14, 0x1a5a);
        mdio_write(tp, 0x14, 0xae06);
        mdio_write(tp, 0x14, 0xa002);
        mdio_write(tp, 0x14, 0x0302);
        mdio_write(tp, 0x14, 0x1ae6);
        mdio_write(tp, 0x14, 0xef96);
        mdio_write(tp, 0x14, 0xfefc);
        mdio_write(tp, 0x14, 0x04f8);
        mdio_write(tp, 0x14, 0xf9fa);
        mdio_write(tp, 0x14, 0xef69);
        mdio_write(tp, 0x14, 0xe082);
        mdio_write(tp, 0x14, 0x29f6);
        mdio_write(tp, 0x14, 0x21e4);
        mdio_write(tp, 0x14, 0x8229);
        mdio_write(tp, 0x14, 0xe080);
        mdio_write(tp, 0x14, 0x10ac);
        mdio_write(tp, 0x14, 0x2202);
        mdio_write(tp, 0x14, 0xae76);
        mdio_write(tp, 0x14, 0xe082);
        mdio_write(tp, 0x14, 0x27f7);
        mdio_write(tp, 0x14, 0x21e4);
        mdio_write(tp, 0x14, 0x8227);
        mdio_write(tp, 0x14, 0xbf43);
        mdio_write(tp, 0x14, 0x1302);
        mdio_write(tp, 0x14, 0x4297);
        mdio_write(tp, 0x14, 0xef21);
        mdio_write(tp, 0x14, 0xbf43);
        mdio_write(tp, 0x14, 0x1602);
        mdio_write(tp, 0x14, 0x4297);
        mdio_write(tp, 0x14, 0x0c11);
        mdio_write(tp, 0x14, 0x1e21);
        mdio_write(tp, 0x14, 0xbf43);
        mdio_write(tp, 0x14, 0x1902);
        mdio_write(tp, 0x14, 0x4297);
        mdio_write(tp, 0x14, 0x0c12);
        mdio_write(tp, 0x14, 0x1e21);
        mdio_write(tp, 0x14, 0xe682);
        mdio_write(tp, 0x14, 0x43a2);
        mdio_write(tp, 0x14, 0x000a);
        mdio_write(tp, 0x14, 0xe182);
        mdio_write(tp, 0x14, 0x27f6);
        mdio_write(tp, 0x14, 0x29e5);
        mdio_write(tp, 0x14, 0x8227);
        mdio_write(tp, 0x14, 0xae42);
        mdio_write(tp, 0x14, 0xe082);
        mdio_write(tp, 0x14, 0x44f7);
        mdio_write(tp, 0x14, 0x21e4);
        mdio_write(tp, 0x14, 0x8244);
        mdio_write(tp, 0x14, 0x0246);
        mdio_write(tp, 0x14, 0xaebf);
        mdio_write(tp, 0x14, 0x4325);
        mdio_write(tp, 0x14, 0x0242);
        mdio_write(tp, 0x14, 0x97ef);
        mdio_write(tp, 0x14, 0x21bf);
        mdio_write(tp, 0x14, 0x431c);
        mdio_write(tp, 0x14, 0x0242);
        mdio_write(tp, 0x14, 0x970c);
        mdio_write(tp, 0x14, 0x121e);
        mdio_write(tp, 0x14, 0x21bf);
        mdio_write(tp, 0x14, 0x431f);
        mdio_write(tp, 0x14, 0x0242);
        mdio_write(tp, 0x14, 0x970c);
        mdio_write(tp, 0x14, 0x131e);
        mdio_write(tp, 0x14, 0x21bf);
        mdio_write(tp, 0x14, 0x4328);
        mdio_write(tp, 0x14, 0x0242);
        mdio_write(tp, 0x14, 0x970c);
        mdio_write(tp, 0x14, 0x141e);
        mdio_write(tp, 0x14, 0x21bf);
        mdio_write(tp, 0x14, 0x44b1);
        mdio_write(tp, 0x14, 0x0242);
        mdio_write(tp, 0x14, 0x970c);
        mdio_write(tp, 0x14, 0x161e);
        mdio_write(tp, 0x14, 0x21e6);
        mdio_write(tp, 0x14, 0x8242);
        mdio_write(tp, 0x14, 0xee82);
        mdio_write(tp, 0x14, 0x4101);
        mdio_write(tp, 0x14, 0xef96);
        mdio_write(tp, 0x14, 0xfefd);
        mdio_write(tp, 0x14, 0xfc04);
        mdio_write(tp, 0x14, 0xf8fa);
        mdio_write(tp, 0x14, 0xef69);
        mdio_write(tp, 0x14, 0xe082);
        mdio_write(tp, 0x14, 0x46a0);
        mdio_write(tp, 0x14, 0x0005);
        mdio_write(tp, 0x14, 0x0286);
        mdio_write(tp, 0x14, 0x96ae);
        mdio_write(tp, 0x14, 0x06a0);
        mdio_write(tp, 0x14, 0x0103);
        mdio_write(tp, 0x14, 0x0219);
        mdio_write(tp, 0x14, 0x19ef);
        mdio_write(tp, 0x14, 0x96fe);
        mdio_write(tp, 0x14, 0xfc04);
        mdio_write(tp, 0x14, 0xf8fa);
        mdio_write(tp, 0x14, 0xef69);
        mdio_write(tp, 0x14, 0xe082);
        mdio_write(tp, 0x14, 0x29f6);
        mdio_write(tp, 0x14, 0x20e4);
        mdio_write(tp, 0x14, 0x8229);
        mdio_write(tp, 0x14, 0xe080);
        mdio_write(tp, 0x14, 0x10ac);
        mdio_write(tp, 0x14, 0x2102);
        mdio_write(tp, 0x14, 0xae54);
        mdio_write(tp, 0x14, 0xe082);
        mdio_write(tp, 0x14, 0x27f7);
        mdio_write(tp, 0x14, 0x20e4);
        mdio_write(tp, 0x14, 0x8227);
        mdio_write(tp, 0x14, 0xbf42);
        mdio_write(tp, 0x14, 0xe602);
        mdio_write(tp, 0x14, 0x4297);
        mdio_write(tp, 0x14, 0xac28);
        mdio_write(tp, 0x14, 0x22bf);
        mdio_write(tp, 0x14, 0x430d);
        mdio_write(tp, 0x14, 0x0242);
        mdio_write(tp, 0x14, 0x97e5);
        mdio_write(tp, 0x14, 0x8247);
        mdio_write(tp, 0x14, 0xac28);
        mdio_write(tp, 0x14, 0x20d1);
        mdio_write(tp, 0x14, 0x03bf);
        mdio_write(tp, 0x14, 0x4307);
        mdio_write(tp, 0x14, 0x0242);
        mdio_write(tp, 0x14, 0x59ee);
        mdio_write(tp, 0x14, 0x8246);
        mdio_write(tp, 0x14, 0x00e1);
        mdio_write(tp, 0x14, 0x8227);
        mdio_write(tp, 0x14, 0xf628);
        mdio_write(tp, 0x14, 0xe582);
        mdio_write(tp, 0x14, 0x27ae);
        mdio_write(tp, 0x14, 0x21d1);
        mdio_write(tp, 0x14, 0x04bf);
        mdio_write(tp, 0x14, 0x4307);
        mdio_write(tp, 0x14, 0x0242);
        mdio_write(tp, 0x14, 0x59ae);
        mdio_write(tp, 0x14, 0x08d1);
        mdio_write(tp, 0x14, 0x05bf);
        mdio_write(tp, 0x14, 0x4307);
        mdio_write(tp, 0x14, 0x0242);
        mdio_write(tp, 0x14, 0x59e0);
        mdio_write(tp, 0x14, 0x8244);
        mdio_write(tp, 0x14, 0xf720);
        mdio_write(tp, 0x14, 0xe482);
        mdio_write(tp, 0x14, 0x4402);
        mdio_write(tp, 0x14, 0x46ae);
        mdio_write(tp, 0x14, 0xee82);
        mdio_write(tp, 0x14, 0x4601);
        mdio_write(tp, 0x14, 0xef96);
        mdio_write(tp, 0x14, 0xfefc);
        mdio_write(tp, 0x14, 0x04f8);
        mdio_write(tp, 0x14, 0xfaef);
        mdio_write(tp, 0x14, 0x69e0);
        mdio_write(tp, 0x14, 0x8013);
        mdio_write(tp, 0x14, 0xad24);
        mdio_write(tp, 0x14, 0x1cbf);
        mdio_write(tp, 0x14, 0x87f0);
        mdio_write(tp, 0x14, 0x0242);
        mdio_write(tp, 0x14, 0x97ad);
        mdio_write(tp, 0x14, 0x2813);
        mdio_write(tp, 0x14, 0xe087);
        mdio_write(tp, 0x14, 0xfca0);
        mdio_write(tp, 0x14, 0x0005);
        mdio_write(tp, 0x14, 0x0287);
        mdio_write(tp, 0x14, 0x36ae);
        mdio_write(tp, 0x14, 0x10a0);
        mdio_write(tp, 0x14, 0x0105);
        mdio_write(tp, 0x14, 0x0287);
        mdio_write(tp, 0x14, 0x48ae);
        mdio_write(tp, 0x14, 0x08e0);
        mdio_write(tp, 0x14, 0x8230);
        mdio_write(tp, 0x14, 0xf626);
        mdio_write(tp, 0x14, 0xe482);
        mdio_write(tp, 0x14, 0x30ef);
        mdio_write(tp, 0x14, 0x96fe);
        mdio_write(tp, 0x14, 0xfc04);
        mdio_write(tp, 0x14, 0xf8e0);
        mdio_write(tp, 0x14, 0x8245);
        mdio_write(tp, 0x14, 0xf722);
        mdio_write(tp, 0x14, 0xe482);
        mdio_write(tp, 0x14, 0x4502);
        mdio_write(tp, 0x14, 0x46ae);
        mdio_write(tp, 0x14, 0xee87);
        mdio_write(tp, 0x14, 0xfc01);
        mdio_write(tp, 0x14, 0xfc04);
        mdio_write(tp, 0x14, 0xf8fa);
        mdio_write(tp, 0x14, 0xef69);
        mdio_write(tp, 0x14, 0xfb02);
        mdio_write(tp, 0x14, 0x46d3);
        mdio_write(tp, 0x14, 0xad50);
        mdio_write(tp, 0x14, 0x2fbf);
        mdio_write(tp, 0x14, 0x87ed);
        mdio_write(tp, 0x14, 0xd101);
        mdio_write(tp, 0x14, 0x0242);
        mdio_write(tp, 0x14, 0x59bf);
        mdio_write(tp, 0x14, 0x87ed);
        mdio_write(tp, 0x14, 0xd100);
        mdio_write(tp, 0x14, 0x0242);
        mdio_write(tp, 0x14, 0x59e0);
        mdio_write(tp, 0x14, 0x8245);
        mdio_write(tp, 0x14, 0xf622);
        mdio_write(tp, 0x14, 0xe482);
        mdio_write(tp, 0x14, 0x4502);
        mdio_write(tp, 0x14, 0x46ae);
        mdio_write(tp, 0x14, 0xd100);
        mdio_write(tp, 0x14, 0xbf87);
        mdio_write(tp, 0x14, 0xf002);
        mdio_write(tp, 0x14, 0x4259);
        mdio_write(tp, 0x14, 0xee87);
        mdio_write(tp, 0x14, 0xfc00);
        mdio_write(tp, 0x14, 0xe082);
        mdio_write(tp, 0x14, 0x30f6);
        mdio_write(tp, 0x14, 0x26e4);
        mdio_write(tp, 0x14, 0x8230);
        mdio_write(tp, 0x14, 0xffef);
        mdio_write(tp, 0x14, 0x96fe);
        mdio_write(tp, 0x14, 0xfc04);
        mdio_write(tp, 0x14, 0xf8f9);
        mdio_write(tp, 0x14, 0xface);
        mdio_write(tp, 0x14, 0xfaef);
        mdio_write(tp, 0x14, 0x69fb);
        mdio_write(tp, 0x14, 0xbf87);
        mdio_write(tp, 0x14, 0xb3d7);
        mdio_write(tp, 0x14, 0x001c);
        mdio_write(tp, 0x14, 0xd819);
        mdio_write(tp, 0x14, 0xd919);
        mdio_write(tp, 0x14, 0xda19);
        mdio_write(tp, 0x14, 0xdb19);
        mdio_write(tp, 0x14, 0x07ef);
        mdio_write(tp, 0x14, 0x9502);
        mdio_write(tp, 0x14, 0x4259);
        mdio_write(tp, 0x14, 0x073f);
        mdio_write(tp, 0x14, 0x0004);
        mdio_write(tp, 0x14, 0x9fec);
        mdio_write(tp, 0x14, 0xffef);
        mdio_write(tp, 0x14, 0x96fe);
        mdio_write(tp, 0x14, 0xc6fe);
        mdio_write(tp, 0x14, 0xfdfc);
        mdio_write(tp, 0x14, 0x0400);
        mdio_write(tp, 0x14, 0x0145);
        mdio_write(tp, 0x14, 0x7d00);
        mdio_write(tp, 0x14, 0x0345);
        mdio_write(tp, 0x14, 0x5c00);
        mdio_write(tp, 0x14, 0x0143);
        mdio_write(tp, 0x14, 0x4f00);
        mdio_write(tp, 0x14, 0x0387);
        mdio_write(tp, 0x14, 0xdb00);
        mdio_write(tp, 0x14, 0x0987);
        mdio_write(tp, 0x14, 0xde00);
        mdio_write(tp, 0x14, 0x0987);
        mdio_write(tp, 0x14, 0xe100);
        mdio_write(tp, 0x14, 0x0087);
        mdio_write(tp, 0x14, 0xeaa4);
        mdio_write(tp, 0x14, 0x00b8);
        mdio_write(tp, 0x14, 0x20c4);
        mdio_write(tp, 0x14, 0x1600);
        mdio_write(tp, 0x14, 0x000f);
        mdio_write(tp, 0x14, 0xf800);
        mdio_write(tp, 0x14, 0x7098);
        mdio_write(tp, 0x14, 0xa58a);
        mdio_write(tp, 0x14, 0xb6a8);
        mdio_write(tp, 0x14, 0x3e50);
        mdio_write(tp, 0x14, 0xa83e);
        mdio_write(tp, 0x14, 0x33bc);
        mdio_write(tp, 0x14, 0xc622);
        mdio_write(tp, 0x14, 0xbcc6);
        mdio_write(tp, 0x14, 0xaaa4);
        mdio_write(tp, 0x14, 0x42ff);
        mdio_write(tp, 0x14, 0xc408);
        mdio_write(tp, 0x14, 0x00c4);
        mdio_write(tp, 0x14, 0x16a8);
        mdio_write(tp, 0x14, 0xbcc0);
        mdio_write(tp, 0x13, 0xb818);
        mdio_write(tp, 0x14, 0x02f3);
        mdio_write(tp, 0x13, 0xb81a);
        mdio_write(tp, 0x14, 0x17d1);
        mdio_write(tp, 0x13, 0xb81c);
        mdio_write(tp, 0x14, 0x185a);
        mdio_write(tp, 0x13, 0xb81e);
        mdio_write(tp, 0x14, 0x3c66);
        mdio_write(tp, 0x13, 0xb820);
        mdio_write(tp, 0x14, 0x021f);
        mdio_write(tp, 0x13, 0xc416);
        mdio_write(tp, 0x14, 0x0500);
        mdio_write(tp, 0x13, 0xb82e);
        mdio_write(tp, 0x14, 0xfffc);

        mdio_write(tp, 0x1F, 0x0A43);
        mdio_write(tp, 0x13, 0x0000);
        mdio_write(tp, 0x14, 0x0000);
        mdio_write(tp, 0x1f, 0x0B82);
        gphy_val = mdio_read(tp, 0x10);
        gphy_val &= ~(BIT_9);
        mdio_write(tp, 0x10, gphy_val);
        mdio_write(tp, 0x1f, 0x0A43);
        mdio_write(tp, 0x13, 0x8146);
        mdio_write(tp, 0x14, 0x0000);

        mdio_write(tp, 0x1f, 0x0B82);
        gphy_val = mdio_read(tp, 0x10);
        gphy_val &= ~(BIT_4);
        mdio_write(tp, 0x10, gphy_val);
}

static void
rtl8168_set_phy_mcu_8168gu_2(struct net_device *dev)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        unsigned int gphy_val,i;

        mdio_write(tp, 0x1f, 0x0B82);
        gphy_val = mdio_read(tp, 0x10);
        gphy_val |= BIT_4;
        mdio_write(tp, 0x10, gphy_val);
        mdio_write(tp, 0x1f, 0x0B80);
        i = 0;
        do {
                gphy_val = mdio_read(tp, 0x10);
                gphy_val &= 0x0040;
                udelay(50);
                udelay(50);
                i++;
        } while(gphy_val != 0x0040 && i <1000);
        mdio_write(tp, 0x1f, 0x0A43);
        mdio_write(tp, 0x13, 0x8146);
        mdio_write(tp, 0x14, 0x0300);
        mdio_write(tp, 0x13, 0xB82E);
        mdio_write(tp, 0x14, 0x0001);
        mdio_write(tp, 0x1F, 0x0A43);
        mdio_write(tp, 0x13, 0xb820);
        mdio_write(tp, 0x14, 0x0290);
        mdio_write(tp, 0x13, 0xa012);
        mdio_write(tp, 0x14, 0x0000);
        mdio_write(tp, 0x13, 0xa014);
        mdio_write(tp, 0x14, 0x2c04);
        mdio_write(tp, 0x14, 0x2c07);
        mdio_write(tp, 0x14, 0x2c07);
        mdio_write(tp, 0x14, 0x2c07);
        mdio_write(tp, 0x14, 0xa304);
        mdio_write(tp, 0x14, 0xa301);
        mdio_write(tp, 0x14, 0x207e);
        mdio_write(tp, 0x13, 0xa01a);
        mdio_write(tp, 0x14, 0x0000);
        mdio_write(tp, 0x13, 0xa006);
        mdio_write(tp, 0x14, 0x0fff);
        mdio_write(tp, 0x13, 0xa004);
        mdio_write(tp, 0x14, 0x0fff);
        mdio_write(tp, 0x13, 0xa002);
        mdio_write(tp, 0x14, 0x0fff);
        mdio_write(tp, 0x13, 0xa000);
        mdio_write(tp, 0x14, 0x107c);
        mdio_write(tp, 0x13, 0xb820);
        mdio_write(tp, 0x14, 0x0210);
        mdio_write(tp, 0x1F, 0x0A43);
        mdio_write(tp, 0x13, 0x0000);
        mdio_write(tp, 0x14, 0x0000);
        mdio_write(tp, 0x1f, 0x0B82);
        gphy_val = mdio_read(tp, 0x17);
        gphy_val &= ~(BIT_0);
        mdio_write(tp, 0x17, gphy_val);
        mdio_write(tp, 0x1f, 0x0A43);
        mdio_write(tp, 0x13, 0x8146);
        mdio_write(tp, 0x14, 0x0000);
        mdio_write(tp, 0x1f, 0x0B82);
        gphy_val = mdio_read(tp, 0x10);
        gphy_val &= ~(BIT_4);
        mdio_write(tp, 0x10, gphy_val);
}

static void
rtl8168_set_phy_mcu_8411b_1(struct net_device *dev)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        unsigned int gphy_val,i;

        mdio_write(tp,0x1f, 0x0B82);
        gphy_val = mdio_read(tp, 0x10);
        gphy_val |= BIT_4;
        mdio_write(tp,0x10, gphy_val);

        mdio_write(tp,0x1f, 0x0B80);
        i = 0;
        do {
                gphy_val = mdio_read(tp, 0x10);
                gphy_val &= 0x0040;
                udelay(50);
                udelay(50);
                i++;
        } while(gphy_val != 0x0040 && i <1000);

        mdio_write(tp,0x1f, 0x0A43);
        mdio_write(tp,0x13, 0x8146);
        mdio_write(tp,0x14, 0x0100);
        mdio_write(tp,0x13, 0xB82E);
        mdio_write(tp,0x14, 0x0001);


        mdio_write(tp,0x1F, 0x0A43);
        mdio_write(tp, 0x13, 0xb820);
        mdio_write(tp, 0x14, 0x0290);
        mdio_write(tp, 0x13, 0xa012);
        mdio_write(tp, 0x14, 0x0000);
        mdio_write(tp, 0x13, 0xa014);
        mdio_write(tp, 0x14, 0x2c04);
        mdio_write(tp, 0x14, 0x2c07);
        mdio_write(tp, 0x14, 0x2c07);
        mdio_write(tp, 0x14, 0x2c07);
        mdio_write(tp, 0x14, 0xa304);
        mdio_write(tp, 0x14, 0xa301);
        mdio_write(tp, 0x14, 0x207e);
        mdio_write(tp, 0x13, 0xa01a);
        mdio_write(tp, 0x14, 0x0000);
        mdio_write(tp, 0x13, 0xa006);
        mdio_write(tp, 0x14, 0x0fff);
        mdio_write(tp, 0x13, 0xa004);
        mdio_write(tp, 0x14, 0x0fff);
        mdio_write(tp, 0x13, 0xa002);
        mdio_write(tp, 0x14, 0x0fff);
        mdio_write(tp, 0x13, 0xa000);
        mdio_write(tp, 0x14, 0x107c);
        mdio_write(tp, 0x13, 0xb820);
        mdio_write(tp, 0x14, 0x0210);


        mdio_write(tp,0x1F, 0x0A43);
        mdio_write(tp,0x13, 0x0000);
        mdio_write(tp,0x14, 0x0000);
        mdio_write(tp,0x1f, 0x0B82);
        gphy_val = mdio_read(tp, 0x17);
        gphy_val &= ~(BIT_0);
        mdio_write(tp,0x17, gphy_val);
        mdio_write(tp,0x1f, 0x0A43);
        mdio_write(tp,0x13, 0x8146);
        mdio_write(tp,0x14, 0x0000);


        mdio_write(tp,0x1f, 0x0B82);
        gphy_val = mdio_read(tp, 0x10);
        gphy_val &= ~(BIT_4);
        mdio_write(tp,0x10, gphy_val);
}

static void
rtl8168_set_phy_mcu_8168h_1(struct net_device *dev)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        unsigned int gphy_val,i;

        mdio_write(tp, 0x1f, 0x0B82);
        gphy_val = mdio_read(tp, 0x10);
        gphy_val |= BIT_4;
        mdio_write(tp, 0x10, gphy_val);

        mdio_write(tp, 0x1f, 0x0B80);
        i = 0;
        do {
                gphy_val = mdio_read(tp, 0x10);
                gphy_val &= 0x0040;
                udelay(50);
                udelay(50);
                i++;
        } while(gphy_val != 0x0040 && i <1000);

        mdio_write(tp, 0x1f, 0x0A43);
        mdio_write(tp, 0x13, 0x8028);
        mdio_write(tp, 0x14, 0x6200);
        mdio_write(tp, 0x13, 0xB82E);
        mdio_write(tp, 0x14, 0x0001);


        mdio_write(tp, 0x1F, 0x0A43);
        mdio_write(tp, 0x13, 0xB820);
        mdio_write(tp, 0x14, 0x0290);
        mdio_write(tp, 0x13, 0xA012);
        mdio_write(tp, 0x14, 0x0000);
        mdio_write(tp, 0x13, 0xA014);
        mdio_write(tp, 0x14, 0x2c04);
        mdio_write(tp, 0x14, 0x2c10);
        mdio_write(tp, 0x14, 0x2c10);
        mdio_write(tp, 0x14, 0x2c10);
        mdio_write(tp, 0x14, 0xa210);
        mdio_write(tp, 0x14, 0xa101);
        mdio_write(tp, 0x14, 0xce10);
        mdio_write(tp, 0x14, 0xe070);
        mdio_write(tp, 0x14, 0x0f40);
        mdio_write(tp, 0x14, 0xaf01);
        mdio_write(tp, 0x14, 0x8f01);
        mdio_write(tp, 0x14, 0x183e);
        mdio_write(tp, 0x14, 0x8e10);
        mdio_write(tp, 0x14, 0x8101);
        mdio_write(tp, 0x14, 0x8210);
        mdio_write(tp, 0x14, 0x28da);
        mdio_write(tp, 0x13, 0xA01A);
        mdio_write(tp, 0x14, 0x0000);
        mdio_write(tp, 0x13, 0xA006);
        mdio_write(tp, 0x14, 0x0017);
        mdio_write(tp, 0x13, 0xA004);
        mdio_write(tp, 0x14, 0x0015);
        mdio_write(tp, 0x13, 0xA002);
        mdio_write(tp, 0x14, 0x0013);
        mdio_write(tp, 0x13, 0xA000);
        mdio_write(tp, 0x14, 0x18d1);
        mdio_write(tp, 0x13, 0xB820);
        mdio_write(tp, 0x14, 0x0210);


        mdio_write(tp, 0x1F, 0x0A43);
        mdio_write(tp, 0x13, 0x0000);
        mdio_write(tp, 0x14, 0x0000);
        mdio_write(tp, 0x1f, 0x0B82);
        gphy_val = mdio_read(tp,  0x17);
        gphy_val &= ~(BIT_0);
        mdio_write(tp, 0x17, gphy_val);
        mdio_write(tp, 0x1f, 0x0A43);
        mdio_write(tp, 0x13, 0x8028);
        mdio_write(tp, 0x14, 0x0000);


        mdio_write(tp, 0x1f, 0x0B82);
        gphy_val = mdio_read(tp, 0x10);
        gphy_val &= ~(BIT_4);
        mdio_write(tp, 0x10, gphy_val);
}

static void
rtl8168_set_phy_mcu_8168h_2(struct net_device *dev)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        unsigned int gphy_val,i;

        mdio_write(tp, 0x1f, 0x0B82);
        gphy_val = mdio_read(tp, 0x10);
        gphy_val |= BIT_4;
        mdio_write(tp, 0x10, gphy_val);

        mdio_write(tp, 0x1f, 0x0B80);
        i = 0;
        do {
                gphy_val = mdio_read(tp, 0x10);
                gphy_val &= 0x0040;
                udelay(50);
                udelay(50);
                i++;
        } while(gphy_val != 0x0040 && i <1000);

        mdio_write(tp, 0x1f, 0x0A43);
        mdio_write(tp, 0x13, 0x8028);
        mdio_write(tp, 0x14, 0x6201);
        mdio_write(tp, 0x13, 0xB82E);
        mdio_write(tp, 0x14, 0x0001);


        mdio_write(tp, 0x1F, 0x0A43);
        mdio_write(tp, 0x13, 0xB820);
        mdio_write(tp, 0x14, 0x0290);
        mdio_write(tp, 0x13, 0xA012);
        mdio_write(tp, 0x14, 0x0000);
        mdio_write(tp, 0x13, 0xA014);
        mdio_write(tp, 0x14, 0x2c04);
        mdio_write(tp, 0x14, 0x2c09);
        mdio_write(tp, 0x14, 0x2c09);
        mdio_write(tp, 0x14, 0x2c09);
        mdio_write(tp, 0x14, 0xad01);
        mdio_write(tp, 0x14, 0xad01);
        mdio_write(tp, 0x14, 0xad01);
        mdio_write(tp, 0x14, 0xad01);
        mdio_write(tp, 0x14, 0x236c);
        mdio_write(tp, 0x13, 0xA01A);
        mdio_write(tp, 0x14, 0x0000);
        mdio_write(tp, 0x13, 0xA006);
        mdio_write(tp, 0x14, 0x0fff);
        mdio_write(tp, 0x13, 0xA004);
        mdio_write(tp, 0x14, 0x0fff);
        mdio_write(tp, 0x13, 0xA002);
        mdio_write(tp, 0x14, 0x0fff);
        mdio_write(tp, 0x13, 0xA000);
        mdio_write(tp, 0x14, 0x136b);
        mdio_write(tp, 0x13, 0xB820);
        mdio_write(tp, 0x14, 0x0210);


        mdio_write(tp,0x1F, 0x0A43);
        mdio_write(tp,0x13, 0x0000);
        mdio_write(tp,0x14, 0x0000);
        mdio_write(tp,0x1f, 0x0B82);
        gphy_val = mdio_read(tp, 0x17);
        gphy_val &= ~(BIT_0);
        mdio_write(tp,0x17, gphy_val);
        mdio_write(tp,0x1f, 0x0A43);
        mdio_write(tp,0x13, 0x8028);
        mdio_write(tp,0x14, 0x0000);


        mdio_write(tp, 0x1f, 0x0B82);
        gphy_val = mdio_read(tp, 0x10);
        gphy_val &= ~(BIT_4);
        mdio_write(tp, 0x10, gphy_val);

        if (tp->RequiredSecLanDonglePatch) {
                mdio_write(tp, 0x1F, 0x0A43);
                gphy_val = mdio_read(tp, 0x11);
                gphy_val &= ~BIT_6;
                mdio_write(tp, 0x11, gphy_val);
        }
}

static void
rtl8168_init_hw_phy_mcu(struct net_device *dev)
{
        struct rtl8168_private *tp = netdev_priv(dev);

        if (tp->NotWrRamCodeToMicroP == TRUE) return;
        if (rtl8168_check_hw_phy_mcu_code_ver(dev)) return;

        if (FALSE == rtl8168_phy_ram_code_check(dev)) {
                rtl8168_set_phy_ram_code_check_fail_flag(dev);
                return;
        }

        switch (tp->mcfg) {
        case CFG_METHOD_14:
                rtl8168_set_phy_mcu_8168e_1(dev);
                break;
        case CFG_METHOD_15:
                rtl8168_set_phy_mcu_8168e_2(dev);
                break;
        case CFG_METHOD_16:
                rtl8168_set_phy_mcu_8168evl_1(dev);
                break;
        case CFG_METHOD_17:
                rtl8168_set_phy_mcu_8168evl_2(dev);
                break;
        case CFG_METHOD_18:
                rtl8168_set_phy_mcu_8168f_1(dev);
                break;
        case CFG_METHOD_19:
                rtl8168_set_phy_mcu_8168f_2(dev);
                break;
        case CFG_METHOD_20:
                rtl8168_set_phy_mcu_8411_1(dev);
                break;
        case CFG_METHOD_21:
                rtl8168_set_phy_mcu_8168g_1(dev);
                break;
        case CFG_METHOD_25:
                rtl8168_set_phy_mcu_8168gu_2(dev);
                break;
        case CFG_METHOD_26:
                rtl8168_set_phy_mcu_8411b_1(dev);
                break;
        case CFG_METHOD_29:
                rtl8168_set_phy_mcu_8168h_1(dev);
                break;
        case CFG_METHOD_30:
                rtl8168_set_phy_mcu_8168h_2(dev);
                break;
        }

        rtl8168_write_hw_phy_mcu_code_ver(dev);

        mdio_write(tp,0x1F, 0x0000);

        tp->HwHasWrRamCodeToMicroP = TRUE;
}

static void
rtl8168_hw_phy_config(struct net_device *dev)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        struct pci_dev *pdev = tp->pci_dev;
        u16 gphy_val;
        unsigned int i;
        unsigned long flags;

        tp->phy_reset_enable(dev);

        spin_lock_irqsave(&tp->phy_lock, flags);

        rtl8168_init_hw_phy_mcu(dev);

        if (tp->mcfg == CFG_METHOD_1) {
                mdio_write(tp, 0x1F, 0x0001);
                mdio_write(tp, 0x0B, 0x94B0);

                mdio_write(tp, 0x1F, 0x0003);
                mdio_write(tp, 0x12, 0x6096);
                mdio_write(tp, 0x1F, 0x0000);

                mdio_write(tp, 0x0D, 0xF8A0);
        } else if (tp->mcfg == CFG_METHOD_2) {
                mdio_write(tp, 0x1F, 0x0001);
                mdio_write(tp, 0x0B, 0x94B0);

                mdio_write(tp, 0x1F, 0x0003);
                mdio_write(tp, 0x12, 0x6096);

                mdio_write(tp, 0x1F, 0x0000);
        } else if (tp->mcfg == CFG_METHOD_3) {
                mdio_write(tp, 0x1F, 0x0001);
                mdio_write(tp, 0x0B, 0x94B0);

                mdio_write(tp, 0x1F, 0x0003);
                mdio_write(tp, 0x12, 0x6096);

                mdio_write(tp, 0x1F, 0x0000);
        } else if (tp->mcfg == CFG_METHOD_4) {
                mdio_write(tp, 0x1F, 0x0001);
                mdio_write(tp, 0x12, 0x2300);
                mdio_write(tp, 0x1F, 0x0000);
                mdio_write(tp, 0x1F, 0x0003);
                mdio_write(tp, 0x16, 0x000A);
                mdio_write(tp, 0x1F, 0x0000);

                mdio_write(tp, 0x1F, 0x0003);
                mdio_write(tp, 0x12, 0xC096);
                mdio_write(tp, 0x1F, 0x0000);

                mdio_write(tp, 0x1F, 0x0002);
                mdio_write(tp, 0x00, 0x88DE);
                mdio_write(tp, 0x01, 0x82B1);
                mdio_write(tp, 0x1F, 0x0000);

                mdio_write(tp, 0x1F, 0x0002);
                mdio_write(tp, 0x08, 0x9E30);
                mdio_write(tp, 0x09, 0x01F0);
                mdio_write(tp, 0x1F, 0x0000);

                mdio_write(tp, 0x1F, 0x0002);
                mdio_write(tp, 0x0A, 0x5500);
                mdio_write(tp, 0x1F, 0x0000);

                mdio_write(tp, 0x1F, 0x0002);
                mdio_write(tp, 0x03, 0x7002);
                mdio_write(tp, 0x1F, 0x0000);

                mdio_write(tp, 0x1F, 0x0002);
                mdio_write(tp, 0x0C, 0x00C8);
                mdio_write(tp, 0x1F, 0x0000);

                mdio_write(tp, 0x1F, 0x0000);
                mdio_write(tp, 0x14, mdio_read(tp, 0x14) | (1 << 5));
                mdio_write(tp, 0x0D, mdio_read(tp, 0x0D) & ~(1 << 5));
        } else if (tp->mcfg == CFG_METHOD_5) {
                mdio_write(tp, 0x1F, 0x0001);
                mdio_write(tp, 0x12, 0x2300);
                mdio_write(tp, 0x1F, 0x0003);
                mdio_write(tp, 0x16, 0x0F0A);
                mdio_write(tp, 0x1F, 0x0000);

                mdio_write(tp, 0x1F, 0x0002);
                mdio_write(tp, 0x00, 0x88DE);
                mdio_write(tp, 0x01, 0x82B1);
                mdio_write(tp, 0x1F, 0x0000);

                mdio_write(tp, 0x1F, 0x0002);
                mdio_write(tp, 0x0C, 0x7EB8);
                mdio_write(tp, 0x1F, 0x0000);

                mdio_write(tp, 0x1F, 0x0002);
                mdio_write(tp, 0x06, 0x0761);
                mdio_write(tp, 0x1F, 0x0000);

                mdio_write(tp, 0x1F, 0x0001);
                mdio_write(tp, 0x03, 0x802F);
                mdio_write(tp, 0x02, 0x4F02);
                mdio_write(tp, 0x01, 0x0409);
                mdio_write(tp, 0x00, 0xF099);
                mdio_write(tp, 0x04, 0x9800);
                mdio_write(tp, 0x04, 0x9000);
                mdio_write(tp, 0x1F, 0x0000);

                mdio_write(tp, 0x1F, 0x0000);
                mdio_write(tp, 0x16, mdio_read(tp, 0x16) | (1 << 0));

                mdio_write(tp, 0x1F, 0x0000);
                mdio_write(tp, 0x14, mdio_read(tp, 0x14) | (1 << 5));
                mdio_write(tp, 0x0D, mdio_read(tp, 0x0D) & ~(1 << 5));

                mdio_write(tp, 0x1F, 0x0001);
                mdio_write(tp, 0x1D, 0x3D98);
                mdio_write(tp, 0x1F, 0x0000);

                mdio_write(tp, 0x1F, 0x0001);
                mdio_write(tp, 0x17, 0x0CC0);
                mdio_write(tp, 0x1F, 0x0000);
        } else if (tp->mcfg == CFG_METHOD_6) {
                mdio_write(tp, 0x1F, 0x0001);
                mdio_write(tp, 0x12, 0x2300);
                mdio_write(tp, 0x1F, 0x0003);
                mdio_write(tp, 0x16, 0x0F0A);
                mdio_write(tp, 0x1F, 0x0000);

                mdio_write(tp, 0x1F, 0x0002);
                mdio_write(tp, 0x00, 0x88DE);
                mdio_write(tp, 0x01, 0x82B1);
                mdio_write(tp, 0x1F, 0x0000);

                mdio_write(tp, 0x1F, 0x0002);
                mdio_write(tp, 0x0C, 0x7EB8);
                mdio_write(tp, 0x1F, 0x0000);

                mdio_write(tp, 0x1F, 0x0002);
                mdio_write(tp, 0x06, 0x5461);
                mdio_write(tp, 0x1F, 0x0000);

                mdio_write(tp, 0x1F, 0x0002);
                mdio_write(tp, 0x06, 0x5461);
                mdio_write(tp, 0x1F, 0x0000);

                mdio_write(tp, 0x1F, 0x0000);
                mdio_write(tp, 0x16, mdio_read(tp, 0x16) | (1 << 0));

                mdio_write(tp, 0x1F, 0x0000);
                mdio_write(tp, 0x14, mdio_read(tp, 0x14) | (1 << 5));
                mdio_write(tp, 0x0D, mdio_read(tp, 0x0D) & ~(1 << 5));

                mdio_write(tp, 0x1F, 0x0001);
                mdio_write(tp, 0x1D, 0x3D98);
                mdio_write(tp, 0x1F, 0x0000);

                mdio_write(tp, 0x1f, 0x0001);
                mdio_write(tp, 0x17, 0x0CC0);
                mdio_write(tp, 0x1F, 0x0000);
        } else if (tp->mcfg == CFG_METHOD_7) {
                mdio_write(tp, 0x1F, 0x0000);
                mdio_write(tp, 0x14, mdio_read(tp, 0x14) | (1 << 5));
                mdio_write(tp, 0x0D, mdio_read(tp, 0x0D) & ~(1 << 5));

                mdio_write(tp, 0x1F, 0x0001);
                mdio_write(tp, 0x1D, 0x3D98);

                mdio_write(tp, 0x1F, 0x0001);
                mdio_write(tp, 0x14, 0xCAA3);
                mdio_write(tp, 0x1C, 0x000A);
                mdio_write(tp, 0x18, 0x65D0);

                mdio_write(tp, 0x1F, 0x0003);
                mdio_write(tp, 0x17, 0xB580);
                mdio_write(tp, 0x18, 0xFF54);
                mdio_write(tp, 0x19, 0x3954);

                mdio_write(tp, 0x1F, 0x0002);
                mdio_write(tp, 0x0D, 0x310C);
                mdio_write(tp, 0x0E, 0x310C);
                mdio_write(tp, 0x0F, 0x311C);
                mdio_write(tp, 0x06, 0x0761);

                mdio_write(tp, 0x1F, 0x0003);
                mdio_write(tp, 0x18, 0xFF55);
                mdio_write(tp, 0x19, 0x3955);
                mdio_write(tp, 0x18, 0xFF54);
                mdio_write(tp, 0x19, 0x3954);

                mdio_write(tp, 0x1F, 0x0001);
                mdio_write(tp, 0x17, 0x0CC0);

                mdio_write(tp, 0x1F, 0x0000);
        } else if (tp->mcfg == CFG_METHOD_8) {
                mdio_write(tp, 0x1F, 0x0000);
                mdio_write(tp, 0x14, mdio_read(tp, 0x14) | (1 << 5));
                mdio_write(tp, 0x0D, mdio_read(tp, 0x0D) & ~(1 << 5));

                mdio_write(tp, 0x1F, 0x0001);
                mdio_write(tp, 0x14, 0xCAA3);
                mdio_write(tp, 0x1C, 0x000A);
                mdio_write(tp, 0x18, 0x65D0);

                mdio_write(tp, 0x1F, 0x0003);
                mdio_write(tp, 0x17, 0xB580);
                mdio_write(tp, 0x18, 0xFF54);
                mdio_write(tp, 0x19, 0x3954);

                mdio_write(tp, 0x1F, 0x0002);
                mdio_write(tp, 0x0D, 0x310C);
                mdio_write(tp, 0x0E, 0x310C);
                mdio_write(tp, 0x0F, 0x311C);
                mdio_write(tp, 0x06, 0x0761);

                mdio_write(tp, 0x1F, 0x0003);
                mdio_write(tp, 0x18, 0xFF55);
                mdio_write(tp, 0x19, 0x3955);
                mdio_write(tp, 0x18, 0xFF54);
                mdio_write(tp, 0x19, 0x3954);

                mdio_write(tp, 0x1F, 0x0001);
                mdio_write(tp, 0x17, 0x0CC0);

                mdio_write(tp, 0x1F, 0x0000);
                mdio_write(tp, 0x16, mdio_read(tp, 0x16) | (1 << 0));

                mdio_write(tp, 0x1F, 0x0000);
        } else if (tp->mcfg == CFG_METHOD_9) {
                mdio_write(tp, 0x1F, 0x0001);
                mdio_write(tp, 0x06, 0x4064);
                mdio_write(tp, 0x07, 0x2863);
                mdio_write(tp, 0x08, 0x059C);
                mdio_write(tp, 0x09, 0x26B4);
                mdio_write(tp, 0x0A, 0x6A19);
                mdio_write(tp, 0x0B, 0xDCC8);
                mdio_write(tp, 0x10, 0xF06D);
                mdio_write(tp, 0x14, 0x7F68);
                mdio_write(tp, 0x18, 0x7FD9);
                mdio_write(tp, 0x1C, 0xF0FF);
                mdio_write(tp, 0x1D, 0x3D9C);
                mdio_write(tp, 0x1F, 0x0003);
                mdio_write(tp, 0x12, 0xF49F);
                mdio_write(tp, 0x13, 0x070B);
                mdio_write(tp, 0x1A, 0x05AD);
                mdio_write(tp, 0x14, 0x94C0);

                mdio_write(tp, 0x1F, 0x0002);
                gphy_val = mdio_read(tp, 0x0B) & 0xFF00;
                gphy_val |= 0x10;
                mdio_write(tp, 0x0B, gphy_val);
                gphy_val = mdio_read(tp, 0x0C) & 0x00FF;
                gphy_val |= 0xA200;
                mdio_write(tp, 0x0C, gphy_val);

                mdio_write(tp, 0x1F, 0x0002);
                mdio_write(tp, 0x06, 0x5561);
                mdio_write(tp, 0x1F, 0x0005);
                mdio_write(tp, 0x05, 0x8332);
                mdio_write(tp, 0x06, 0x5561);

                if (rtl8168_efuse_read(tp, 0x01) == 0xb1) {
                        mdio_write(tp, 0x1F, 0x0002);
                        mdio_write(tp, 0x05, 0x669A);
                        mdio_write(tp, 0x1F, 0x0005);
                        mdio_write(tp, 0x05, 0x8330);
                        mdio_write(tp, 0x06, 0x669A);

                        mdio_write(tp, 0x1F, 0x0002);
                        gphy_val = mdio_read(tp, 0x0D);
                        if ((gphy_val & 0x00FF) != 0x006C) {
                                gphy_val &= 0xFF00;
                                mdio_write(tp, 0x1F, 0x0002);
                                mdio_write(tp, 0x0D, gphy_val | 0x0065);
                                mdio_write(tp, 0x0D, gphy_val | 0x0066);
                                mdio_write(tp, 0x0D, gphy_val | 0x0067);
                                mdio_write(tp, 0x0D, gphy_val | 0x0068);
                                mdio_write(tp, 0x0D, gphy_val | 0x0069);
                                mdio_write(tp, 0x0D, gphy_val | 0x006A);
                                mdio_write(tp, 0x0D, gphy_val | 0x006B);
                                mdio_write(tp, 0x0D, gphy_val | 0x006C);
                        }
                } else {
                        mdio_write(tp, 0x1F, 0x0002);
                        mdio_write(tp, 0x05, 0x6662);
                        mdio_write(tp, 0x1F, 0x0005);
                        mdio_write(tp, 0x05, 0x8330);
                        mdio_write(tp, 0x06, 0x6662);
                }

                mdio_write(tp, 0x1F, 0x0002);
                gphy_val = mdio_read(tp, 0x0D);
                gphy_val |= BIT_9;
                gphy_val |= BIT_8;
                mdio_write(tp, 0x0D, gphy_val);
                gphy_val = mdio_read(tp, 0x0F);
                gphy_val |= BIT_4;
                mdio_write(tp, 0x0F, gphy_val);

                mdio_write(tp, 0x1F, 0x0002);
                gphy_val = mdio_read(tp, 0x02);
                gphy_val &= ~BIT_10;
                gphy_val &= ~BIT_9;
                gphy_val |= BIT_8;
                mdio_write(tp, 0x02, gphy_val);
                gphy_val = mdio_read(tp, 0x03);
                gphy_val &= ~BIT_15;
                gphy_val &= ~BIT_14;
                gphy_val &= ~BIT_13;
                mdio_write(tp, 0x03, gphy_val);

                mdio_write(tp, 0x1F, 0x0001);
                mdio_write(tp, 0x17, 0x0CC0);

                mdio_write(tp, 0x1F, 0x0005);
                mdio_write(tp, 0x05, 0x001B);
                if (mdio_read(tp, 0x06) == 0xBF00) {
                        mdio_write(tp, 0x1f, 0x0005);
                        mdio_write(tp, 0x05, 0xfff6);
                        mdio_write(tp, 0x06, 0x0080);
                        mdio_write(tp, 0x05, 0x8000);
                        mdio_write(tp, 0x06, 0xf8f9);
                        mdio_write(tp, 0x06, 0xfaef);
                        mdio_write(tp, 0x06, 0x59ee);
                        mdio_write(tp, 0x06, 0xf8ea);
                        mdio_write(tp, 0x06, 0x00ee);
                        mdio_write(tp, 0x06, 0xf8eb);
                        mdio_write(tp, 0x06, 0x00e0);
                        mdio_write(tp, 0x06, 0xf87c);
                        mdio_write(tp, 0x06, 0xe1f8);
                        mdio_write(tp, 0x06, 0x7d59);
                        mdio_write(tp, 0x06, 0x0fef);
                        mdio_write(tp, 0x06, 0x0139);
                        mdio_write(tp, 0x06, 0x029e);
                        mdio_write(tp, 0x06, 0x06ef);
                        mdio_write(tp, 0x06, 0x1039);
                        mdio_write(tp, 0x06, 0x089f);
                        mdio_write(tp, 0x06, 0x2aee);
                        mdio_write(tp, 0x06, 0xf8ea);
                        mdio_write(tp, 0x06, 0x00ee);
                        mdio_write(tp, 0x06, 0xf8eb);
                        mdio_write(tp, 0x06, 0x01e0);
                        mdio_write(tp, 0x06, 0xf87c);
                        mdio_write(tp, 0x06, 0xe1f8);
                        mdio_write(tp, 0x06, 0x7d58);
                        mdio_write(tp, 0x06, 0x409e);
                        mdio_write(tp, 0x06, 0x0f39);
                        mdio_write(tp, 0x06, 0x46aa);
                        mdio_write(tp, 0x06, 0x0bbf);
                        mdio_write(tp, 0x06, 0x8290);
                        mdio_write(tp, 0x06, 0xd682);
                        mdio_write(tp, 0x06, 0x9802);
                        mdio_write(tp, 0x06, 0x014f);
                        mdio_write(tp, 0x06, 0xae09);
                        mdio_write(tp, 0x06, 0xbf82);
                        mdio_write(tp, 0x06, 0x98d6);
                        mdio_write(tp, 0x06, 0x82a0);
                        mdio_write(tp, 0x06, 0x0201);
                        mdio_write(tp, 0x06, 0x4fef);
                        mdio_write(tp, 0x06, 0x95fe);
                        mdio_write(tp, 0x06, 0xfdfc);
                        mdio_write(tp, 0x06, 0x05f8);
                        mdio_write(tp, 0x06, 0xf9fa);
                        mdio_write(tp, 0x06, 0xeef8);
                        mdio_write(tp, 0x06, 0xea00);
                        mdio_write(tp, 0x06, 0xeef8);
                        mdio_write(tp, 0x06, 0xeb00);
                        mdio_write(tp, 0x06, 0xe2f8);
                        mdio_write(tp, 0x06, 0x7ce3);
                        mdio_write(tp, 0x06, 0xf87d);
                        mdio_write(tp, 0x06, 0xa511);
                        mdio_write(tp, 0x06, 0x1112);
                        mdio_write(tp, 0x06, 0xd240);
                        mdio_write(tp, 0x06, 0xd644);
                        mdio_write(tp, 0x06, 0x4402);
                        mdio_write(tp, 0x06, 0x8217);
                        mdio_write(tp, 0x06, 0xd2a0);
                        mdio_write(tp, 0x06, 0xd6aa);
                        mdio_write(tp, 0x06, 0xaa02);
                        mdio_write(tp, 0x06, 0x8217);
                        mdio_write(tp, 0x06, 0xae0f);
                        mdio_write(tp, 0x06, 0xa544);
                        mdio_write(tp, 0x06, 0x4402);
                        mdio_write(tp, 0x06, 0xae4d);
                        mdio_write(tp, 0x06, 0xa5aa);
                        mdio_write(tp, 0x06, 0xaa02);
                        mdio_write(tp, 0x06, 0xae47);
                        mdio_write(tp, 0x06, 0xaf82);
                        mdio_write(tp, 0x06, 0x13ee);
                        mdio_write(tp, 0x06, 0x834e);
                        mdio_write(tp, 0x06, 0x00ee);
                        mdio_write(tp, 0x06, 0x834d);
                        mdio_write(tp, 0x06, 0x0fee);
                        mdio_write(tp, 0x06, 0x834c);
                        mdio_write(tp, 0x06, 0x0fee);
                        mdio_write(tp, 0x06, 0x834f);
                        mdio_write(tp, 0x06, 0x00ee);
                        mdio_write(tp, 0x06, 0x8351);
                        mdio_write(tp, 0x06, 0x00ee);
                        mdio_write(tp, 0x06, 0x834a);
                        mdio_write(tp, 0x06, 0xffee);
                        mdio_write(tp, 0x06, 0x834b);
                        mdio_write(tp, 0x06, 0xffe0);
                        mdio_write(tp, 0x06, 0x8330);
                        mdio_write(tp, 0x06, 0xe183);
                        mdio_write(tp, 0x06, 0x3158);
                        mdio_write(tp, 0x06, 0xfee4);
                        mdio_write(tp, 0x06, 0xf88a);
                        mdio_write(tp, 0x06, 0xe5f8);
                        mdio_write(tp, 0x06, 0x8be0);
                        mdio_write(tp, 0x06, 0x8332);
                        mdio_write(tp, 0x06, 0xe183);
                        mdio_write(tp, 0x06, 0x3359);
                        mdio_write(tp, 0x06, 0x0fe2);
                        mdio_write(tp, 0x06, 0x834d);
                        mdio_write(tp, 0x06, 0x0c24);
                        mdio_write(tp, 0x06, 0x5af0);
                        mdio_write(tp, 0x06, 0x1e12);
                        mdio_write(tp, 0x06, 0xe4f8);
                        mdio_write(tp, 0x06, 0x8ce5);
                        mdio_write(tp, 0x06, 0xf88d);
                        mdio_write(tp, 0x06, 0xaf82);
                        mdio_write(tp, 0x06, 0x13e0);
                        mdio_write(tp, 0x06, 0x834f);
                        mdio_write(tp, 0x06, 0x10e4);
                        mdio_write(tp, 0x06, 0x834f);
                        mdio_write(tp, 0x06, 0xe083);
                        mdio_write(tp, 0x06, 0x4e78);
                        mdio_write(tp, 0x06, 0x009f);
                        mdio_write(tp, 0x06, 0x0ae0);
                        mdio_write(tp, 0x06, 0x834f);
                        mdio_write(tp, 0x06, 0xa010);
                        mdio_write(tp, 0x06, 0xa5ee);
                        mdio_write(tp, 0x06, 0x834e);
                        mdio_write(tp, 0x06, 0x01e0);
                        mdio_write(tp, 0x06, 0x834e);
                        mdio_write(tp, 0x06, 0x7805);
                        mdio_write(tp, 0x06, 0x9e9a);
                        mdio_write(tp, 0x06, 0xe083);
                        mdio_write(tp, 0x06, 0x4e78);
                        mdio_write(tp, 0x06, 0x049e);
                        mdio_write(tp, 0x06, 0x10e0);
                        mdio_write(tp, 0x06, 0x834e);
                        mdio_write(tp, 0x06, 0x7803);
                        mdio_write(tp, 0x06, 0x9e0f);
                        mdio_write(tp, 0x06, 0xe083);
                        mdio_write(tp, 0x06, 0x4e78);
                        mdio_write(tp, 0x06, 0x019e);
                        mdio_write(tp, 0x06, 0x05ae);
                        mdio_write(tp, 0x06, 0x0caf);
                        mdio_write(tp, 0x06, 0x81f8);
                        mdio_write(tp, 0x06, 0xaf81);
                        mdio_write(tp, 0x06, 0xa3af);
                        mdio_write(tp, 0x06, 0x81dc);
                        mdio_write(tp, 0x06, 0xaf82);
                        mdio_write(tp, 0x06, 0x13ee);
                        mdio_write(tp, 0x06, 0x8348);
                        mdio_write(tp, 0x06, 0x00ee);
                        mdio_write(tp, 0x06, 0x8349);
                        mdio_write(tp, 0x06, 0x00e0);
                        mdio_write(tp, 0x06, 0x8351);
                        mdio_write(tp, 0x06, 0x10e4);
                        mdio_write(tp, 0x06, 0x8351);
                        mdio_write(tp, 0x06, 0x5801);
                        mdio_write(tp, 0x06, 0x9fea);
                        mdio_write(tp, 0x06, 0xd000);
                        mdio_write(tp, 0x06, 0xd180);
                        mdio_write(tp, 0x06, 0x1f66);
                        mdio_write(tp, 0x06, 0xe2f8);
                        mdio_write(tp, 0x06, 0xeae3);
                        mdio_write(tp, 0x06, 0xf8eb);
                        mdio_write(tp, 0x06, 0x5af8);
                        mdio_write(tp, 0x06, 0x1e20);
                        mdio_write(tp, 0x06, 0xe6f8);
                        mdio_write(tp, 0x06, 0xeae5);
                        mdio_write(tp, 0x06, 0xf8eb);
                        mdio_write(tp, 0x06, 0xd302);
                        mdio_write(tp, 0x06, 0xb3fe);
                        mdio_write(tp, 0x06, 0xe2f8);
                        mdio_write(tp, 0x06, 0x7cef);
                        mdio_write(tp, 0x06, 0x325b);
                        mdio_write(tp, 0x06, 0x80e3);
                        mdio_write(tp, 0x06, 0xf87d);
                        mdio_write(tp, 0x06, 0x9e03);
                        mdio_write(tp, 0x06, 0x7dff);
                        mdio_write(tp, 0x06, 0xff0d);
                        mdio_write(tp, 0x06, 0x581c);
                        mdio_write(tp, 0x06, 0x551a);
                        mdio_write(tp, 0x06, 0x6511);
                        mdio_write(tp, 0x06, 0xa190);
                        mdio_write(tp, 0x06, 0xd3e2);
                        mdio_write(tp, 0x06, 0x8348);
                        mdio_write(tp, 0x06, 0xe383);
                        mdio_write(tp, 0x06, 0x491b);
                        mdio_write(tp, 0x06, 0x56ab);
                        mdio_write(tp, 0x06, 0x08ef);
                        mdio_write(tp, 0x06, 0x56e6);
                        mdio_write(tp, 0x06, 0x8348);
                        mdio_write(tp, 0x06, 0xe783);
                        mdio_write(tp, 0x06, 0x4910);
                        mdio_write(tp, 0x06, 0xd180);
                        mdio_write(tp, 0x06, 0x1f66);
                        mdio_write(tp, 0x06, 0xa004);
                        mdio_write(tp, 0x06, 0xb9e2);
                        mdio_write(tp, 0x06, 0x8348);
                        mdio_write(tp, 0x06, 0xe383);
                        mdio_write(tp, 0x06, 0x49ef);
                        mdio_write(tp, 0x06, 0x65e2);
                        mdio_write(tp, 0x06, 0x834a);
                        mdio_write(tp, 0x06, 0xe383);
                        mdio_write(tp, 0x06, 0x4b1b);
                        mdio_write(tp, 0x06, 0x56aa);
                        mdio_write(tp, 0x06, 0x0eef);
                        mdio_write(tp, 0x06, 0x56e6);
                        mdio_write(tp, 0x06, 0x834a);
                        mdio_write(tp, 0x06, 0xe783);
                        mdio_write(tp, 0x06, 0x4be2);
                        mdio_write(tp, 0x06, 0x834d);
                        mdio_write(tp, 0x06, 0xe683);
                        mdio_write(tp, 0x06, 0x4ce0);
                        mdio_write(tp, 0x06, 0x834d);
                        mdio_write(tp, 0x06, 0xa000);
                        mdio_write(tp, 0x06, 0x0caf);
                        mdio_write(tp, 0x06, 0x81dc);
                        mdio_write(tp, 0x06, 0xe083);
                        mdio_write(tp, 0x06, 0x4d10);
                        mdio_write(tp, 0x06, 0xe483);
                        mdio_write(tp, 0x06, 0x4dae);
                        mdio_write(tp, 0x06, 0x0480);
                        mdio_write(tp, 0x06, 0xe483);
                        mdio_write(tp, 0x06, 0x4de0);
                        mdio_write(tp, 0x06, 0x834e);
                        mdio_write(tp, 0x06, 0x7803);
                        mdio_write(tp, 0x06, 0x9e0b);
                        mdio_write(tp, 0x06, 0xe083);
                        mdio_write(tp, 0x06, 0x4e78);
                        mdio_write(tp, 0x06, 0x049e);
                        mdio_write(tp, 0x06, 0x04ee);
                        mdio_write(tp, 0x06, 0x834e);
                        mdio_write(tp, 0x06, 0x02e0);
                        mdio_write(tp, 0x06, 0x8332);
                        mdio_write(tp, 0x06, 0xe183);
                        mdio_write(tp, 0x06, 0x3359);
                        mdio_write(tp, 0x06, 0x0fe2);
                        mdio_write(tp, 0x06, 0x834d);
                        mdio_write(tp, 0x06, 0x0c24);
                        mdio_write(tp, 0x06, 0x5af0);
                        mdio_write(tp, 0x06, 0x1e12);
                        mdio_write(tp, 0x06, 0xe4f8);
                        mdio_write(tp, 0x06, 0x8ce5);
                        mdio_write(tp, 0x06, 0xf88d);
                        mdio_write(tp, 0x06, 0xe083);
                        mdio_write(tp, 0x06, 0x30e1);
                        mdio_write(tp, 0x06, 0x8331);
                        mdio_write(tp, 0x06, 0x6801);
                        mdio_write(tp, 0x06, 0xe4f8);
                        mdio_write(tp, 0x06, 0x8ae5);
                        mdio_write(tp, 0x06, 0xf88b);
                        mdio_write(tp, 0x06, 0xae37);
                        mdio_write(tp, 0x06, 0xee83);
                        mdio_write(tp, 0x06, 0x4e03);
                        mdio_write(tp, 0x06, 0xe083);
                        mdio_write(tp, 0x06, 0x4ce1);
                        mdio_write(tp, 0x06, 0x834d);
                        mdio_write(tp, 0x06, 0x1b01);
                        mdio_write(tp, 0x06, 0x9e04);
                        mdio_write(tp, 0x06, 0xaaa1);
                        mdio_write(tp, 0x06, 0xaea8);
                        mdio_write(tp, 0x06, 0xee83);
                        mdio_write(tp, 0x06, 0x4e04);
                        mdio_write(tp, 0x06, 0xee83);
                        mdio_write(tp, 0x06, 0x4f00);
                        mdio_write(tp, 0x06, 0xaeab);
                        mdio_write(tp, 0x06, 0xe083);
                        mdio_write(tp, 0x06, 0x4f78);
                        mdio_write(tp, 0x06, 0x039f);
                        mdio_write(tp, 0x06, 0x14ee);
                        mdio_write(tp, 0x06, 0x834e);
                        mdio_write(tp, 0x06, 0x05d2);
                        mdio_write(tp, 0x06, 0x40d6);
                        mdio_write(tp, 0x06, 0x5554);
                        mdio_write(tp, 0x06, 0x0282);
                        mdio_write(tp, 0x06, 0x17d2);
                        mdio_write(tp, 0x06, 0xa0d6);
                        mdio_write(tp, 0x06, 0xba00);
                        mdio_write(tp, 0x06, 0x0282);
                        mdio_write(tp, 0x06, 0x17fe);
                        mdio_write(tp, 0x06, 0xfdfc);
                        mdio_write(tp, 0x06, 0x05f8);
                        mdio_write(tp, 0x06, 0xe0f8);
                        mdio_write(tp, 0x06, 0x60e1);
                        mdio_write(tp, 0x06, 0xf861);
                        mdio_write(tp, 0x06, 0x6802);
                        mdio_write(tp, 0x06, 0xe4f8);
                        mdio_write(tp, 0x06, 0x60e5);
                        mdio_write(tp, 0x06, 0xf861);
                        mdio_write(tp, 0x06, 0xe0f8);
                        mdio_write(tp, 0x06, 0x48e1);
                        mdio_write(tp, 0x06, 0xf849);
                        mdio_write(tp, 0x06, 0x580f);
                        mdio_write(tp, 0x06, 0x1e02);
                        mdio_write(tp, 0x06, 0xe4f8);
                        mdio_write(tp, 0x06, 0x48e5);
                        mdio_write(tp, 0x06, 0xf849);
                        mdio_write(tp, 0x06, 0xd000);
                        mdio_write(tp, 0x06, 0x0282);
                        mdio_write(tp, 0x06, 0x5bbf);
                        mdio_write(tp, 0x06, 0x8350);
                        mdio_write(tp, 0x06, 0xef46);
                        mdio_write(tp, 0x06, 0xdc19);
                        mdio_write(tp, 0x06, 0xddd0);
                        mdio_write(tp, 0x06, 0x0102);
                        mdio_write(tp, 0x06, 0x825b);
                        mdio_write(tp, 0x06, 0x0282);
                        mdio_write(tp, 0x06, 0x77e0);
                        mdio_write(tp, 0x06, 0xf860);
                        mdio_write(tp, 0x06, 0xe1f8);
                        mdio_write(tp, 0x06, 0x6158);
                        mdio_write(tp, 0x06, 0xfde4);
                        mdio_write(tp, 0x06, 0xf860);
                        mdio_write(tp, 0x06, 0xe5f8);
                        mdio_write(tp, 0x06, 0x61fc);
                        mdio_write(tp, 0x06, 0x04f9);
                        mdio_write(tp, 0x06, 0xfafb);
                        mdio_write(tp, 0x06, 0xc6bf);
                        mdio_write(tp, 0x06, 0xf840);
                        mdio_write(tp, 0x06, 0xbe83);
                        mdio_write(tp, 0x06, 0x50a0);
                        mdio_write(tp, 0x06, 0x0101);
                        mdio_write(tp, 0x06, 0x071b);
                        mdio_write(tp, 0x06, 0x89cf);
                        mdio_write(tp, 0x06, 0xd208);
                        mdio_write(tp, 0x06, 0xebdb);
                        mdio_write(tp, 0x06, 0x19b2);
                        mdio_write(tp, 0x06, 0xfbff);
                        mdio_write(tp, 0x06, 0xfefd);
                        mdio_write(tp, 0x06, 0x04f8);
                        mdio_write(tp, 0x06, 0xe0f8);
                        mdio_write(tp, 0x06, 0x48e1);
                        mdio_write(tp, 0x06, 0xf849);
                        mdio_write(tp, 0x06, 0x6808);
                        mdio_write(tp, 0x06, 0xe4f8);
                        mdio_write(tp, 0x06, 0x48e5);
                        mdio_write(tp, 0x06, 0xf849);
                        mdio_write(tp, 0x06, 0x58f7);
                        mdio_write(tp, 0x06, 0xe4f8);
                        mdio_write(tp, 0x06, 0x48e5);
                        mdio_write(tp, 0x06, 0xf849);
                        mdio_write(tp, 0x06, 0xfc04);
                        mdio_write(tp, 0x06, 0x4d20);
                        mdio_write(tp, 0x06, 0x0002);
                        mdio_write(tp, 0x06, 0x4e22);
                        mdio_write(tp, 0x06, 0x0002);
                        mdio_write(tp, 0x06, 0x4ddf);
                        mdio_write(tp, 0x06, 0xff01);
                        mdio_write(tp, 0x06, 0x4edd);
                        mdio_write(tp, 0x06, 0xff01);
                        mdio_write(tp, 0x06, 0xf8fa);
                        mdio_write(tp, 0x06, 0xfbef);
                        mdio_write(tp, 0x06, 0x79bf);
                        mdio_write(tp, 0x06, 0xf822);
                        mdio_write(tp, 0x06, 0xd819);
                        mdio_write(tp, 0x06, 0xd958);
                        mdio_write(tp, 0x06, 0x849f);
                        mdio_write(tp, 0x06, 0x09bf);
                        mdio_write(tp, 0x06, 0x82be);
                        mdio_write(tp, 0x06, 0xd682);
                        mdio_write(tp, 0x06, 0xc602);
                        mdio_write(tp, 0x06, 0x014f);
                        mdio_write(tp, 0x06, 0xef97);
                        mdio_write(tp, 0x06, 0xfffe);
                        mdio_write(tp, 0x06, 0xfc05);
                        mdio_write(tp, 0x06, 0x17ff);
                        mdio_write(tp, 0x06, 0xfe01);
                        mdio_write(tp, 0x06, 0x1700);
                        mdio_write(tp, 0x06, 0x0102);
                        mdio_write(tp, 0x05, 0x83d8);
                        mdio_write(tp, 0x06, 0x8051);
                        mdio_write(tp, 0x05, 0x83d6);
                        mdio_write(tp, 0x06, 0x82a0);
                        mdio_write(tp, 0x05, 0x83d4);
                        mdio_write(tp, 0x06, 0x8000);
                        mdio_write(tp, 0x02, 0x2010);
                        mdio_write(tp, 0x03, 0xdc00);
                        mdio_write(tp, 0x1f, 0x0000);
                        mdio_write(tp, 0x0b, 0x0600);
                        mdio_write(tp, 0x1f, 0x0005);
                        mdio_write(tp, 0x05, 0xfff6);
                        mdio_write(tp, 0x06, 0x00fc);
                        mdio_write(tp, 0x1f, 0x0000);
                }

                mdio_write(tp, 0x1F, 0x0000);
                mdio_write(tp, 0x0D, 0xF880);
                mdio_write(tp, 0x1F, 0x0000);
        } else if (tp->mcfg == CFG_METHOD_10) {
                mdio_write(tp, 0x1F, 0x0001);
                mdio_write(tp, 0x06, 0x4064);
                mdio_write(tp, 0x07, 0x2863);
                mdio_write(tp, 0x08, 0x059C);
                mdio_write(tp, 0x09, 0x26B4);
                mdio_write(tp, 0x0A, 0x6A19);
                mdio_write(tp, 0x0B, 0xDCC8);
                mdio_write(tp, 0x10, 0xF06D);
                mdio_write(tp, 0x14, 0x7F68);
                mdio_write(tp, 0x18, 0x7FD9);
                mdio_write(tp, 0x1C, 0xF0FF);
                mdio_write(tp, 0x1D, 0x3D9C);
                mdio_write(tp, 0x1F, 0x0003);
                mdio_write(tp, 0x12, 0xF49F);
                mdio_write(tp, 0x13, 0x070B);
                mdio_write(tp, 0x1A, 0x05AD);
                mdio_write(tp, 0x14, 0x94C0);

                mdio_write(tp, 0x1F, 0x0002);
                mdio_write(tp, 0x06, 0x5561);
                mdio_write(tp, 0x1F, 0x0005);
                mdio_write(tp, 0x05, 0x8332);
                mdio_write(tp, 0x06, 0x5561);

                if (rtl8168_efuse_read(tp, 0x01) == 0xb1) {
                        mdio_write(tp, 0x1F, 0x0002);
                        mdio_write(tp, 0x05, 0x669A);
                        mdio_write(tp, 0x1F, 0x0005);
                        mdio_write(tp, 0x05, 0x8330);
                        mdio_write(tp, 0x06, 0x669A);

                        mdio_write(tp, 0x1F, 0x0002);
                        gphy_val = mdio_read(tp, 0x0D);
                        if ((gphy_val & 0x00FF) != 0x006C) {
                                gphy_val &= 0xFF00;
                                mdio_write(tp, 0x1F, 0x0002);
                                mdio_write(tp, 0x0D, gphy_val | 0x0065);
                                mdio_write(tp, 0x0D, gphy_val | 0x0066);
                                mdio_write(tp, 0x0D, gphy_val | 0x0067);
                                mdio_write(tp, 0x0D, gphy_val | 0x0068);
                                mdio_write(tp, 0x0D, gphy_val | 0x0069);
                                mdio_write(tp, 0x0D, gphy_val | 0x006A);
                                mdio_write(tp, 0x0D, gphy_val | 0x006B);
                                mdio_write(tp, 0x0D, gphy_val | 0x006C);
                        }
                } else {
                        mdio_write(tp, 0x1F, 0x0002);
                        mdio_write(tp, 0x05, 0x2642);
                        mdio_write(tp, 0x1F, 0x0005);
                        mdio_write(tp, 0x05, 0x8330);
                        mdio_write(tp, 0x06, 0x2642);
                }

                if (rtl8168_efuse_read(tp, 0x30) == 0x98) {
                        mdio_write(tp, 0x1F, 0x0000);
                        mdio_write(tp, 0x11, mdio_read(tp, 0x11) & ~BIT_1);
                        mdio_write(tp, 0x1F, 0x0005);
                        mdio_write(tp, 0x01, mdio_read(tp, 0x01) | BIT_9);
                } else if (rtl8168_efuse_read(tp, 0x30) == 0x90) {
                        mdio_write(tp, 0x1F, 0x0005);
                        mdio_write(tp, 0x01, mdio_read(tp, 0x01) & ~BIT_9);
                        mdio_write(tp, 0x1F, 0x0000);
                        mdio_write(tp, 0x16, 0x5101);
                }

                mdio_write(tp, 0x1F, 0x0002);
                gphy_val = mdio_read(tp, 0x02);
                gphy_val &= ~BIT_10;
                gphy_val &= ~BIT_9;
                gphy_val |= BIT_8;
                mdio_write(tp, 0x02, gphy_val);
                gphy_val = mdio_read(tp, 0x03);
                gphy_val &= ~BIT_15;
                gphy_val &= ~BIT_14;
                gphy_val &= ~BIT_13;
                mdio_write(tp, 0x03, gphy_val);

                mdio_write(tp, 0x1F, 0x0001);
                mdio_write(tp, 0x17, 0x0CC0);

                mdio_write(tp, 0x1F, 0x0002);
                gphy_val = mdio_read(tp, 0x0F);
                gphy_val |= BIT_4;
                gphy_val |= BIT_2;
                gphy_val |= BIT_1;
                gphy_val |= BIT_0;
                mdio_write(tp, 0x0F, gphy_val);

                mdio_write(tp, 0x1F, 0x0005);
                mdio_write(tp, 0x05, 0x001B);
                if (mdio_read(tp, 0x06) == 0xB300) {
                        mdio_write(tp, 0x1f, 0x0005);
                        mdio_write(tp, 0x05, 0xfff6);
                        mdio_write(tp, 0x06, 0x0080);
                        mdio_write(tp, 0x05, 0x8000);
                        mdio_write(tp, 0x06, 0xf8f9);
                        mdio_write(tp, 0x06, 0xfaee);
                        mdio_write(tp, 0x06, 0xf8ea);
                        mdio_write(tp, 0x06, 0x00ee);
                        mdio_write(tp, 0x06, 0xf8eb);
                        mdio_write(tp, 0x06, 0x00e2);
                        mdio_write(tp, 0x06, 0xf87c);
                        mdio_write(tp, 0x06, 0xe3f8);
                        mdio_write(tp, 0x06, 0x7da5);
                        mdio_write(tp, 0x06, 0x1111);
                        mdio_write(tp, 0x06, 0x12d2);
                        mdio_write(tp, 0x06, 0x40d6);
                        mdio_write(tp, 0x06, 0x4444);
                        mdio_write(tp, 0x06, 0x0281);
                        mdio_write(tp, 0x06, 0xc6d2);
                        mdio_write(tp, 0x06, 0xa0d6);
                        mdio_write(tp, 0x06, 0xaaaa);
                        mdio_write(tp, 0x06, 0x0281);
                        mdio_write(tp, 0x06, 0xc6ae);
                        mdio_write(tp, 0x06, 0x0fa5);
                        mdio_write(tp, 0x06, 0x4444);
                        mdio_write(tp, 0x06, 0x02ae);
                        mdio_write(tp, 0x06, 0x4da5);
                        mdio_write(tp, 0x06, 0xaaaa);
                        mdio_write(tp, 0x06, 0x02ae);
                        mdio_write(tp, 0x06, 0x47af);
                        mdio_write(tp, 0x06, 0x81c2);
                        mdio_write(tp, 0x06, 0xee83);
                        mdio_write(tp, 0x06, 0x4e00);
                        mdio_write(tp, 0x06, 0xee83);
                        mdio_write(tp, 0x06, 0x4d0f);
                        mdio_write(tp, 0x06, 0xee83);
                        mdio_write(tp, 0x06, 0x4c0f);
                        mdio_write(tp, 0x06, 0xee83);
                        mdio_write(tp, 0x06, 0x4f00);
                        mdio_write(tp, 0x06, 0xee83);
                        mdio_write(tp, 0x06, 0x5100);
                        mdio_write(tp, 0x06, 0xee83);
                        mdio_write(tp, 0x06, 0x4aff);
                        mdio_write(tp, 0x06, 0xee83);
                        mdio_write(tp, 0x06, 0x4bff);
                        mdio_write(tp, 0x06, 0xe083);
                        mdio_write(tp, 0x06, 0x30e1);
                        mdio_write(tp, 0x06, 0x8331);
                        mdio_write(tp, 0x06, 0x58fe);
                        mdio_write(tp, 0x06, 0xe4f8);
                        mdio_write(tp, 0x06, 0x8ae5);
                        mdio_write(tp, 0x06, 0xf88b);
                        mdio_write(tp, 0x06, 0xe083);
                        mdio_write(tp, 0x06, 0x32e1);
                        mdio_write(tp, 0x06, 0x8333);
                        mdio_write(tp, 0x06, 0x590f);
                        mdio_write(tp, 0x06, 0xe283);
                        mdio_write(tp, 0x06, 0x4d0c);
                        mdio_write(tp, 0x06, 0x245a);
                        mdio_write(tp, 0x06, 0xf01e);
                        mdio_write(tp, 0x06, 0x12e4);
                        mdio_write(tp, 0x06, 0xf88c);
                        mdio_write(tp, 0x06, 0xe5f8);
                        mdio_write(tp, 0x06, 0x8daf);
                        mdio_write(tp, 0x06, 0x81c2);
                        mdio_write(tp, 0x06, 0xe083);
                        mdio_write(tp, 0x06, 0x4f10);
                        mdio_write(tp, 0x06, 0xe483);
                        mdio_write(tp, 0x06, 0x4fe0);
                        mdio_write(tp, 0x06, 0x834e);
                        mdio_write(tp, 0x06, 0x7800);
                        mdio_write(tp, 0x06, 0x9f0a);
                        mdio_write(tp, 0x06, 0xe083);
                        mdio_write(tp, 0x06, 0x4fa0);
                        mdio_write(tp, 0x06, 0x10a5);
                        mdio_write(tp, 0x06, 0xee83);
                        mdio_write(tp, 0x06, 0x4e01);
                        mdio_write(tp, 0x06, 0xe083);
                        mdio_write(tp, 0x06, 0x4e78);
                        mdio_write(tp, 0x06, 0x059e);
                        mdio_write(tp, 0x06, 0x9ae0);
                        mdio_write(tp, 0x06, 0x834e);
                        mdio_write(tp, 0x06, 0x7804);
                        mdio_write(tp, 0x06, 0x9e10);
                        mdio_write(tp, 0x06, 0xe083);
                        mdio_write(tp, 0x06, 0x4e78);
                        mdio_write(tp, 0x06, 0x039e);
                        mdio_write(tp, 0x06, 0x0fe0);
                        mdio_write(tp, 0x06, 0x834e);
                        mdio_write(tp, 0x06, 0x7801);
                        mdio_write(tp, 0x06, 0x9e05);
                        mdio_write(tp, 0x06, 0xae0c);
                        mdio_write(tp, 0x06, 0xaf81);
                        mdio_write(tp, 0x06, 0xa7af);
                        mdio_write(tp, 0x06, 0x8152);
                        mdio_write(tp, 0x06, 0xaf81);
                        mdio_write(tp, 0x06, 0x8baf);
                        mdio_write(tp, 0x06, 0x81c2);
                        mdio_write(tp, 0x06, 0xee83);
                        mdio_write(tp, 0x06, 0x4800);
                        mdio_write(tp, 0x06, 0xee83);
                        mdio_write(tp, 0x06, 0x4900);
                        mdio_write(tp, 0x06, 0xe083);
                        mdio_write(tp, 0x06, 0x5110);
                        mdio_write(tp, 0x06, 0xe483);
                        mdio_write(tp, 0x06, 0x5158);
                        mdio_write(tp, 0x06, 0x019f);
                        mdio_write(tp, 0x06, 0xead0);
                        mdio_write(tp, 0x06, 0x00d1);
                        mdio_write(tp, 0x06, 0x801f);
                        mdio_write(tp, 0x06, 0x66e2);
                        mdio_write(tp, 0x06, 0xf8ea);
                        mdio_write(tp, 0x06, 0xe3f8);
                        mdio_write(tp, 0x06, 0xeb5a);
                        mdio_write(tp, 0x06, 0xf81e);
                        mdio_write(tp, 0x06, 0x20e6);
                        mdio_write(tp, 0x06, 0xf8ea);
                        mdio_write(tp, 0x06, 0xe5f8);
                        mdio_write(tp, 0x06, 0xebd3);
                        mdio_write(tp, 0x06, 0x02b3);
                        mdio_write(tp, 0x06, 0xfee2);
                        mdio_write(tp, 0x06, 0xf87c);
                        mdio_write(tp, 0x06, 0xef32);
                        mdio_write(tp, 0x06, 0x5b80);
                        mdio_write(tp, 0x06, 0xe3f8);
                        mdio_write(tp, 0x06, 0x7d9e);
                        mdio_write(tp, 0x06, 0x037d);
                        mdio_write(tp, 0x06, 0xffff);
                        mdio_write(tp, 0x06, 0x0d58);
                        mdio_write(tp, 0x06, 0x1c55);
                        mdio_write(tp, 0x06, 0x1a65);
                        mdio_write(tp, 0x06, 0x11a1);
                        mdio_write(tp, 0x06, 0x90d3);
                        mdio_write(tp, 0x06, 0xe283);
                        mdio_write(tp, 0x06, 0x48e3);
                        mdio_write(tp, 0x06, 0x8349);
                        mdio_write(tp, 0x06, 0x1b56);
                        mdio_write(tp, 0x06, 0xab08);
                        mdio_write(tp, 0x06, 0xef56);
                        mdio_write(tp, 0x06, 0xe683);
                        mdio_write(tp, 0x06, 0x48e7);
                        mdio_write(tp, 0x06, 0x8349);
                        mdio_write(tp, 0x06, 0x10d1);
                        mdio_write(tp, 0x06, 0x801f);
                        mdio_write(tp, 0x06, 0x66a0);
                        mdio_write(tp, 0x06, 0x04b9);
                        mdio_write(tp, 0x06, 0xe283);
                        mdio_write(tp, 0x06, 0x48e3);
                        mdio_write(tp, 0x06, 0x8349);
                        mdio_write(tp, 0x06, 0xef65);
                        mdio_write(tp, 0x06, 0xe283);
                        mdio_write(tp, 0x06, 0x4ae3);
                        mdio_write(tp, 0x06, 0x834b);
                        mdio_write(tp, 0x06, 0x1b56);
                        mdio_write(tp, 0x06, 0xaa0e);
                        mdio_write(tp, 0x06, 0xef56);
                        mdio_write(tp, 0x06, 0xe683);
                        mdio_write(tp, 0x06, 0x4ae7);
                        mdio_write(tp, 0x06, 0x834b);
                        mdio_write(tp, 0x06, 0xe283);
                        mdio_write(tp, 0x06, 0x4de6);
                        mdio_write(tp, 0x06, 0x834c);
                        mdio_write(tp, 0x06, 0xe083);
                        mdio_write(tp, 0x06, 0x4da0);
                        mdio_write(tp, 0x06, 0x000c);
                        mdio_write(tp, 0x06, 0xaf81);
                        mdio_write(tp, 0x06, 0x8be0);
                        mdio_write(tp, 0x06, 0x834d);
                        mdio_write(tp, 0x06, 0x10e4);
                        mdio_write(tp, 0x06, 0x834d);
                        mdio_write(tp, 0x06, 0xae04);
                        mdio_write(tp, 0x06, 0x80e4);
                        mdio_write(tp, 0x06, 0x834d);
                        mdio_write(tp, 0x06, 0xe083);
                        mdio_write(tp, 0x06, 0x4e78);
                        mdio_write(tp, 0x06, 0x039e);
                        mdio_write(tp, 0x06, 0x0be0);
                        mdio_write(tp, 0x06, 0x834e);
                        mdio_write(tp, 0x06, 0x7804);
                        mdio_write(tp, 0x06, 0x9e04);
                        mdio_write(tp, 0x06, 0xee83);
                        mdio_write(tp, 0x06, 0x4e02);
                        mdio_write(tp, 0x06, 0xe083);
                        mdio_write(tp, 0x06, 0x32e1);
                        mdio_write(tp, 0x06, 0x8333);
                        mdio_write(tp, 0x06, 0x590f);
                        mdio_write(tp, 0x06, 0xe283);
                        mdio_write(tp, 0x06, 0x4d0c);
                        mdio_write(tp, 0x06, 0x245a);
                        mdio_write(tp, 0x06, 0xf01e);
                        mdio_write(tp, 0x06, 0x12e4);
                        mdio_write(tp, 0x06, 0xf88c);
                        mdio_write(tp, 0x06, 0xe5f8);
                        mdio_write(tp, 0x06, 0x8de0);
                        mdio_write(tp, 0x06, 0x8330);
                        mdio_write(tp, 0x06, 0xe183);
                        mdio_write(tp, 0x06, 0x3168);
                        mdio_write(tp, 0x06, 0x01e4);
                        mdio_write(tp, 0x06, 0xf88a);
                        mdio_write(tp, 0x06, 0xe5f8);
                        mdio_write(tp, 0x06, 0x8bae);
                        mdio_write(tp, 0x06, 0x37ee);
                        mdio_write(tp, 0x06, 0x834e);
                        mdio_write(tp, 0x06, 0x03e0);
                        mdio_write(tp, 0x06, 0x834c);
                        mdio_write(tp, 0x06, 0xe183);
                        mdio_write(tp, 0x06, 0x4d1b);
                        mdio_write(tp, 0x06, 0x019e);
                        mdio_write(tp, 0x06, 0x04aa);
                        mdio_write(tp, 0x06, 0xa1ae);
                        mdio_write(tp, 0x06, 0xa8ee);
                        mdio_write(tp, 0x06, 0x834e);
                        mdio_write(tp, 0x06, 0x04ee);
                        mdio_write(tp, 0x06, 0x834f);
                        mdio_write(tp, 0x06, 0x00ae);
                        mdio_write(tp, 0x06, 0xabe0);
                        mdio_write(tp, 0x06, 0x834f);
                        mdio_write(tp, 0x06, 0x7803);
                        mdio_write(tp, 0x06, 0x9f14);
                        mdio_write(tp, 0x06, 0xee83);
                        mdio_write(tp, 0x06, 0x4e05);
                        mdio_write(tp, 0x06, 0xd240);
                        mdio_write(tp, 0x06, 0xd655);
                        mdio_write(tp, 0x06, 0x5402);
                        mdio_write(tp, 0x06, 0x81c6);
                        mdio_write(tp, 0x06, 0xd2a0);
                        mdio_write(tp, 0x06, 0xd6ba);
                        mdio_write(tp, 0x06, 0x0002);
                        mdio_write(tp, 0x06, 0x81c6);
                        mdio_write(tp, 0x06, 0xfefd);
                        mdio_write(tp, 0x06, 0xfc05);
                        mdio_write(tp, 0x06, 0xf8e0);
                        mdio_write(tp, 0x06, 0xf860);
                        mdio_write(tp, 0x06, 0xe1f8);
                        mdio_write(tp, 0x06, 0x6168);
                        mdio_write(tp, 0x06, 0x02e4);
                        mdio_write(tp, 0x06, 0xf860);
                        mdio_write(tp, 0x06, 0xe5f8);
                        mdio_write(tp, 0x06, 0x61e0);
                        mdio_write(tp, 0x06, 0xf848);
                        mdio_write(tp, 0x06, 0xe1f8);
                        mdio_write(tp, 0x06, 0x4958);
                        mdio_write(tp, 0x06, 0x0f1e);
                        mdio_write(tp, 0x06, 0x02e4);
                        mdio_write(tp, 0x06, 0xf848);
                        mdio_write(tp, 0x06, 0xe5f8);
                        mdio_write(tp, 0x06, 0x49d0);
                        mdio_write(tp, 0x06, 0x0002);
                        mdio_write(tp, 0x06, 0x820a);
                        mdio_write(tp, 0x06, 0xbf83);
                        mdio_write(tp, 0x06, 0x50ef);
                        mdio_write(tp, 0x06, 0x46dc);
                        mdio_write(tp, 0x06, 0x19dd);
                        mdio_write(tp, 0x06, 0xd001);
                        mdio_write(tp, 0x06, 0x0282);
                        mdio_write(tp, 0x06, 0x0a02);
                        mdio_write(tp, 0x06, 0x8226);
                        mdio_write(tp, 0x06, 0xe0f8);
                        mdio_write(tp, 0x06, 0x60e1);
                        mdio_write(tp, 0x06, 0xf861);
                        mdio_write(tp, 0x06, 0x58fd);
                        mdio_write(tp, 0x06, 0xe4f8);
                        mdio_write(tp, 0x06, 0x60e5);
                        mdio_write(tp, 0x06, 0xf861);
                        mdio_write(tp, 0x06, 0xfc04);
                        mdio_write(tp, 0x06, 0xf9fa);
                        mdio_write(tp, 0x06, 0xfbc6);
                        mdio_write(tp, 0x06, 0xbff8);
                        mdio_write(tp, 0x06, 0x40be);
                        mdio_write(tp, 0x06, 0x8350);
                        mdio_write(tp, 0x06, 0xa001);
                        mdio_write(tp, 0x06, 0x0107);
                        mdio_write(tp, 0x06, 0x1b89);
                        mdio_write(tp, 0x06, 0xcfd2);
                        mdio_write(tp, 0x06, 0x08eb);
                        mdio_write(tp, 0x06, 0xdb19);
                        mdio_write(tp, 0x06, 0xb2fb);
                        mdio_write(tp, 0x06, 0xfffe);
                        mdio_write(tp, 0x06, 0xfd04);
                        mdio_write(tp, 0x06, 0xf8e0);
                        mdio_write(tp, 0x06, 0xf848);
                        mdio_write(tp, 0x06, 0xe1f8);
                        mdio_write(tp, 0x06, 0x4968);
                        mdio_write(tp, 0x06, 0x08e4);
                        mdio_write(tp, 0x06, 0xf848);
                        mdio_write(tp, 0x06, 0xe5f8);
                        mdio_write(tp, 0x06, 0x4958);
                        mdio_write(tp, 0x06, 0xf7e4);
                        mdio_write(tp, 0x06, 0xf848);
                        mdio_write(tp, 0x06, 0xe5f8);
                        mdio_write(tp, 0x06, 0x49fc);
                        mdio_write(tp, 0x06, 0x044d);
                        mdio_write(tp, 0x06, 0x2000);
                        mdio_write(tp, 0x06, 0x024e);
                        mdio_write(tp, 0x06, 0x2200);
                        mdio_write(tp, 0x06, 0x024d);
                        mdio_write(tp, 0x06, 0xdfff);
                        mdio_write(tp, 0x06, 0x014e);
                        mdio_write(tp, 0x06, 0xddff);
                        mdio_write(tp, 0x06, 0x01f8);
                        mdio_write(tp, 0x06, 0xfafb);
                        mdio_write(tp, 0x06, 0xef79);
                        mdio_write(tp, 0x06, 0xbff8);
                        mdio_write(tp, 0x06, 0x22d8);
                        mdio_write(tp, 0x06, 0x19d9);
                        mdio_write(tp, 0x06, 0x5884);
                        mdio_write(tp, 0x06, 0x9f09);
                        mdio_write(tp, 0x06, 0xbf82);
                        mdio_write(tp, 0x06, 0x6dd6);
                        mdio_write(tp, 0x06, 0x8275);
                        mdio_write(tp, 0x06, 0x0201);
                        mdio_write(tp, 0x06, 0x4fef);
                        mdio_write(tp, 0x06, 0x97ff);
                        mdio_write(tp, 0x06, 0xfefc);
                        mdio_write(tp, 0x06, 0x0517);
                        mdio_write(tp, 0x06, 0xfffe);
                        mdio_write(tp, 0x06, 0x0117);
                        mdio_write(tp, 0x06, 0x0001);
                        mdio_write(tp, 0x06, 0x0200);
                        mdio_write(tp, 0x05, 0x83d8);
                        mdio_write(tp, 0x06, 0x8000);
                        mdio_write(tp, 0x05, 0x83d6);
                        mdio_write(tp, 0x06, 0x824f);
                        mdio_write(tp, 0x02, 0x2010);
                        mdio_write(tp, 0x03, 0xdc00);
                        mdio_write(tp, 0x1f, 0x0000);
                        mdio_write(tp, 0x0b, 0x0600);
                        mdio_write(tp, 0x1f, 0x0005);
                        mdio_write(tp, 0x05, 0xfff6);
                        mdio_write(tp, 0x06, 0x00fc);
                        mdio_write(tp, 0x1f, 0x0000);
                }

                mdio_write(tp, 0x1F, 0x0000);
                mdio_write(tp, 0x0D, 0xF880);
                mdio_write(tp, 0x1F, 0x0000);
        } else if (tp->mcfg == CFG_METHOD_11) {
                mdio_write(tp, 0x1F, 0x0002);
                mdio_write(tp, 0x10, 0x0008);
                mdio_write(tp, 0x0D, 0x006C);

                mdio_write(tp, 0x1F, 0x0001);
                mdio_write(tp, 0x17, 0x0CC0);

                mdio_write(tp, 0x1F, 0x0001);
                mdio_write(tp, 0x0B, 0xA4D8);
                mdio_write(tp, 0x09, 0x281C);
                mdio_write(tp, 0x07, 0x2883);
                mdio_write(tp, 0x0A, 0x6B35);
                mdio_write(tp, 0x1D, 0x3DA4);
                mdio_write(tp, 0x1C, 0xEFFD);
                mdio_write(tp, 0x14, 0x7F52);
                mdio_write(tp, 0x18, 0x7FC6);
                mdio_write(tp, 0x08, 0x0601);
                mdio_write(tp, 0x06, 0x4063);
                mdio_write(tp, 0x10, 0xF074);
                mdio_write(tp, 0x1F, 0x0003);
                mdio_write(tp, 0x13, 0x0789);
                mdio_write(tp, 0x12, 0xF4BD);
                mdio_write(tp, 0x1A, 0x04FD);
                mdio_write(tp, 0x14, 0x84B0);
                mdio_write(tp, 0x1F, 0x0000);
                mdio_write(tp, 0x00, 0x9200);

                mdio_write(tp, 0x1F, 0x0005);
                mdio_write(tp, 0x01, 0x0340);
                mdio_write(tp, 0x1F, 0x0001);
                mdio_write(tp, 0x04, 0x4000);
                mdio_write(tp, 0x03, 0x1D21);
                mdio_write(tp, 0x02, 0x0C32);
                mdio_write(tp, 0x01, 0x0200);
                mdio_write(tp, 0x00, 0x5554);
                mdio_write(tp, 0x04, 0x4800);
                mdio_write(tp, 0x04, 0x4000);
                mdio_write(tp, 0x04, 0xF000);
                mdio_write(tp, 0x03, 0xDF01);
                mdio_write(tp, 0x02, 0xDF20);
                mdio_write(tp, 0x01, 0x101A);
                mdio_write(tp, 0x00, 0xA0FF);
                mdio_write(tp, 0x04, 0xF800);
                mdio_write(tp, 0x04, 0xF000);
                mdio_write(tp, 0x1F, 0x0000);

                mdio_write(tp, 0x1F, 0x0007);
                mdio_write(tp, 0x1E, 0x0023);
                mdio_write(tp, 0x16, 0x0000);
                mdio_write(tp, 0x1F, 0x0000);

                gphy_val = mdio_read(tp, 0x0D);
                gphy_val |= BIT_5;
                mdio_write(tp, 0x0D, gphy_val);
        } else if (tp->mcfg == CFG_METHOD_12 || tp->mcfg == CFG_METHOD_13) {
                mdio_write(tp, 0x1F, 0x0001);
                mdio_write(tp, 0x17, 0x0CC0);

                mdio_write(tp, 0x1F, 0x0007);
                mdio_write(tp, 0x1E, 0x002D);
                mdio_write(tp, 0x18, 0x0040);

                mdio_write(tp, 0x1F, 0x0000);
                gphy_val = mdio_read(tp, 0x0D);
                gphy_val |= BIT_5;
                mdio_write(tp, 0x0D, gphy_val);

                mdio_write(tp, 0x1F, 0x0002);
                gphy_val = mdio_read(tp, 0x0C);
                gphy_val |= BIT_10;
                mdio_write(tp, 0x0C, gphy_val);
        } else if (tp->mcfg == CFG_METHOD_14 || tp->mcfg == CFG_METHOD_15) {
                mdio_write(tp, 0x1F, 0x0007);
                mdio_write(tp, 0x1E, 0x0023);
                gphy_val = mdio_read(tp, 0x17) | BIT_1;
                if (tp->RequiredSecLanDonglePatch)
                        gphy_val &= ~(BIT_2);
                else
                        gphy_val |= (BIT_2);
                mdio_write(tp, 0x17, gphy_val);
                mdio_write(tp, 0x1F, 0x0000);

                mdio_write(tp, 0x1f, 0x0005);
                mdio_write(tp, 0x05, 0x8b80);
                mdio_write(tp, 0x06, 0xc896);
                mdio_write(tp, 0x1f, 0x0000);

                mdio_write(tp, 0x1F, 0x0001);
                mdio_write(tp, 0x0B, 0x6C20);
                mdio_write(tp, 0x07, 0x2872);
                mdio_write(tp, 0x1C, 0xEFFF);
                mdio_write(tp, 0x1F, 0x0003);
                mdio_write(tp, 0x14, 0x6420);
                mdio_write(tp, 0x1F, 0x0000);

                mdio_write(tp, 0x1F, 0x0002);
                gphy_val = mdio_read(tp, 0x08) & 0x00FF;
                mdio_write(tp, 0x08, gphy_val | 0x8000);
                mdio_write(tp, 0x1F, 0x0000);

                mdio_write(tp, 0x1F, 0x0007);
                mdio_write(tp, 0x1E, 0x002D);
                gphy_val = mdio_read(tp, 0x18);
                mdio_write(tp, 0x18, gphy_val | 0x0010);
                mdio_write(tp, 0x1F, 0x0000);
                gphy_val = mdio_read(tp, 0x14);
                mdio_write(tp, 0x14, gphy_val | 0x8000);

                mdio_write(tp, 0x1F, 0x0002);
                mdio_write(tp, 0x00, 0x080B);
                mdio_write(tp, 0x0B, 0x09D7);
                mdio_write(tp, 0x1F, 0x0000);

                if (aspm) {
                        if (tp->HwHasWrRamCodeToMicroP == TRUE) {
                                mdio_write(tp, 0x1f, 0x0000);
                                mdio_write(tp, 0x15, 0x1006);
                        }
                }

                mdio_write(tp, 0x1F, 0x0003);
                mdio_write(tp, 0x19, 0x7F46);
                mdio_write(tp, 0x1F, 0x0005);
                mdio_write(tp, 0x05, 0x8AD2);
                mdio_write(tp, 0x06, 0x6810);
                mdio_write(tp, 0x05, 0x8AD4);
                mdio_write(tp, 0x06, 0x8002);
                mdio_write(tp, 0x05, 0x8ADE);
                mdio_write(tp, 0x06, 0x8025);
                mdio_write(tp, 0x1F, 0x0000);

                mdio_write(tp, 0x1F, 0x0007);
                mdio_write(tp, 0x1E, 0x002F);
                mdio_write(tp, 0x15, 0x1919);
                mdio_write(tp, 0x1F, 0x0000);

                mdio_write(tp, 0x1F, 0x0007);
                mdio_write(tp, 0x1E, 0x002D);
                gphy_val = mdio_read(tp, 0x18);
                mdio_write(tp, 0x18, gphy_val | 0x0040);
                mdio_write(tp, 0x1F, 0x0000);

                mdio_write(tp, 0x1F, 0x0005);
                mdio_write(tp, 0x05, 0x8B86);
                gphy_val = mdio_read(tp, 0x06);
                mdio_write(tp, 0x06, gphy_val | 0x0001);
                mdio_write(tp, 0x1F, 0x0000);

                mdio_write(tp, 0x1F, 0x0007);
                mdio_write(tp, 0x1E, 0x00AC);
                mdio_write(tp, 0x18, 0x0006);
                mdio_write(tp, 0x1F, 0x0000);
        } else if (tp->mcfg == CFG_METHOD_16) {
                mdio_write(tp, 0x1F, 0x0005);
                mdio_write(tp, 0x05, 0x8B80);
                gphy_val = mdio_read(tp, 0x06);
                gphy_val |= BIT_2 | BIT_1;
                mdio_write(tp, 0x06, gphy_val);
                mdio_write(tp, 0x1f, 0x0000);

                mdio_write(tp, 0x1f, 0x0004);
                mdio_write(tp, 0x1f, 0x0007);
                mdio_write(tp, 0x1e, 0x002D);
                gphy_val = mdio_read(tp, 0x18);
                gphy_val |= BIT_4;
                mdio_write(tp, 0x18, gphy_val);
                mdio_write(tp, 0x1f, 0x0002);
                mdio_write(tp, 0x1f, 0x0000);
                gphy_val = mdio_read(tp, 0x14);
                gphy_val |= BIT_15;
                mdio_write(tp, 0x14, gphy_val);

                mdio_write(tp, 0x1F, 0x0000);
                mdio_write(tp, 0x15, 0x1006);

                mdio_write(tp, 0x1F, 0x0005);
                mdio_write(tp, 0x05, 0x8B86);
                gphy_val = mdio_read(tp, 0x06);
                gphy_val |= BIT_0;
                mdio_write(tp, 0x06, gphy_val);
                mdio_write(tp, 0x1F, 0x0000);

                mdio_write(tp, 0x1F, 0x0001);
                mdio_write(tp, 0x0B, 0x6C14);
                mdio_write(tp, 0x14, 0x7F3D);
                mdio_write(tp, 0x1C, 0xFAFE);
                mdio_write(tp, 0x08, 0x07C5);
                mdio_write(tp, 0x10, 0xF090);
                mdio_write(tp, 0x1F, 0x0003);
                mdio_write(tp, 0x14, 0x641A);
                mdio_write(tp, 0x1A, 0x0606);
                mdio_write(tp, 0x12, 0xF480);
                mdio_write(tp, 0x13, 0x0747);
                mdio_write(tp, 0x1F, 0x0000);

                mdio_write(tp, 0x1F, 0x0004);
                mdio_write(tp, 0x1F, 0x0007);
                mdio_write(tp, 0x1E, 0x0078);
                mdio_write(tp, 0x15, 0xA408);
                mdio_write(tp, 0x17, 0x5100);
                mdio_write(tp, 0x19, 0x0008);
                mdio_write(tp, 0x1F, 0x0002);
                mdio_write(tp, 0x1F, 0x0000);

                mdio_write(tp, 0x1F, 0x0003);
                mdio_write(tp, 0x0D, 0x0207);
                mdio_write(tp, 0x02, 0x5FD0);
                mdio_write(tp, 0x1F, 0x0000);

                mdio_write(tp, 0x1F, 0x0004);
                mdio_write(tp, 0x1F, 0x0007);
                mdio_write(tp, 0x1E, 0x00A1);
                gphy_val = mdio_read(tp, 0x1A);
                gphy_val &= ~BIT_2;
                mdio_write(tp, 0x1A, gphy_val);
                mdio_write(tp, 0x1F, 0x0002);
                mdio_write(tp, 0x1F, 0x0000);

                mdio_write(tp, 0x1F, 0x0004);
                mdio_write(tp, 0x1F, 0x0007);
                mdio_write(tp, 0x1E, 0x002D);
                gphy_val = mdio_read(tp, 0x16);
                gphy_val |= BIT_5;
                mdio_write(tp, 0x16, gphy_val);
                mdio_write(tp, 0x1F, 0x0002);
                mdio_write(tp, 0x1F, 0x0000);

                mdio_write(tp, 0x1F, 0x0004);
                mdio_write(tp, 0x1F, 0x0007);
                mdio_write(tp, 0x1E, 0x00AC);
                mdio_write(tp, 0x18, 0x0006);
                mdio_write(tp, 0x1F, 0x0002);
                mdio_write(tp, 0x1F, 0x0000);

                mdio_write(tp, 0x1F, 0x0003);
                mdio_write(tp, 0x09, 0xA20F);
                mdio_write(tp, 0x1F, 0x0000);

                mdio_write(tp, 0x1F, 0x0005);
                mdio_write(tp, 0x05, 0x8B5B);
                mdio_write(tp, 0x06, 0x9222);
                mdio_write(tp, 0x05, 0x8B6D);
                mdio_write(tp, 0x06, 0x8000);
                mdio_write(tp, 0x05, 0x8B76);
                mdio_write(tp, 0x06, 0x8000);
                mdio_write(tp, 0x1F, 0x0000);

                if (pdev->subsystem_vendor == 0x1043 &&
                    pdev->subsystem_device == 0x13F7) {

                        static const u16 evl_phy_value[] = {
                                0x8B56, 0x8B5F, 0x8B68, 0x8B71,
                                0x8B7A, 0x8A7B, 0x8A7E, 0x8A81,
                                0x8A84, 0x8A87
                        };

                        mdio_write(tp, 0x1F, 0x0005);
                        for (i = 0; i < ARRAY_SIZE(evl_phy_value); i++) {
                                mdio_write(tp, 0x05, evl_phy_value[i]);
                                gphy_val = (0xAA << 8) | (mdio_read(tp, 0x06) & 0xFF);
                                mdio_write(tp, 0x06, gphy_val);
                        }
                        mdio_write(tp, 0x1F, 0x0007);
                        mdio_write(tp, 0x1E, 0x0078);
                        mdio_write(tp, 0x17, 0x51AA);
                        mdio_write(tp, 0x1F, 0x0000);
                }

                mdio_write(tp, 0x1f, 0x0005);
                mdio_write(tp, 0x05, 0x8B54);
                mdio_write(tp, 0x06, mdio_read(tp, 0x06) & ~BIT_11);
                mdio_write(tp, 0x05, 0x8B5D);
                mdio_write(tp, 0x06, mdio_read(tp, 0x06) & ~BIT_11);
                mdio_write(tp, 0x05, 0x8A7C);
                mdio_write(tp, 0x06, mdio_read(tp, 0x06) & ~BIT_8);
                mdio_write(tp, 0x05, 0x8A7F);
                mdio_write(tp, 0x06, mdio_read(tp, 0x06) | BIT_8);
                mdio_write(tp, 0x05, 0x8A82);
                mdio_write(tp, 0x06, mdio_read(tp, 0x06) & ~BIT_8);
                mdio_write(tp, 0x05, 0x8A85);
                mdio_write(tp, 0x06, mdio_read(tp, 0x06) & ~BIT_8);
                mdio_write(tp, 0x05, 0x8A88);
                mdio_write(tp, 0x06, mdio_read(tp, 0x06) & ~BIT_8);
                mdio_write(tp, 0x1f, 0x0000);

                mdio_write(tp, 0x1F, 0x0005);
                mdio_write(tp, 0x05, 0x8B85);
                gphy_val = mdio_read(tp, 0x06) | BIT_14 | BIT_15;
                mdio_write(tp, 0x06, gphy_val);
                mdio_write(tp, 0x1f, 0x0000);
        } else if (tp->mcfg == CFG_METHOD_17) {
                if (pdev->subsystem_vendor == 0x144d &&
                    pdev->subsystem_device == 0xc0a6) {
                        mdio_write(tp, 0x1F, 0x0001);
                        mdio_write(tp, 0x0e, 0x6b7f);
                        mdio_write(tp, 0x1f, 0x0000);
                        mdio_write(tp, 0x1F, 0x0005);
                        mdio_write(tp, 0x05, 0x8B86);
                        gphy_val = mdio_read(tp, 0x06);
                        gphy_val |= BIT_4;
                        mdio_write(tp, 0x06, gphy_val);
                        mdio_write(tp, 0x1f, 0x0000);
                } else {
                        mdio_write(tp, 0x1F, 0x0005);
                        mdio_write(tp, 0x05, 0x8B80);
                        gphy_val = mdio_read(tp, 0x06);
                        gphy_val |= BIT_2 | BIT_1;
                        mdio_write(tp, 0x06, gphy_val);
                        mdio_write(tp, 0x1f, 0x0000);

                        mdio_write(tp, 0x1F, 0x0005);
                        mdio_write(tp, 0x05, 0x8B86);
                        gphy_val = mdio_read(tp, 0x06);
                        gphy_val &= ~BIT_4;
                        mdio_write(tp, 0x06, gphy_val);
                        mdio_write(tp, 0x1f, 0x0000);
                }

                mdio_write(tp, 0x1f, 0x0004);
                mdio_write(tp, 0x1f, 0x0007);
                mdio_write(tp, 0x1e, 0x002D);
                gphy_val = mdio_read(tp, 0x18);
                gphy_val |= BIT_4;
                mdio_write(tp, 0x18, gphy_val);
                mdio_write(tp, 0x1f, 0x0002);
                mdio_write(tp, 0x1f, 0x0000);
                gphy_val = mdio_read(tp, 0x14);
                gphy_val |= BIT_15;
                mdio_write(tp, 0x14, gphy_val);

                mdio_write(tp, 0x1F, 0x0005);
                mdio_write(tp, 0x05, 0x8B86);
                gphy_val = mdio_read(tp, 0x06);
                gphy_val |= BIT_0;
                mdio_write(tp, 0x06, gphy_val);
                mdio_write(tp, 0x1f, 0x0000);

                mdio_write(tp, 0x1F, 0x0004);
                mdio_write(tp, 0x1F, 0x0007);
                mdio_write(tp, 0x1E, 0x00AC);
                mdio_write(tp, 0x18, 0x0006);
                mdio_write(tp, 0x1F, 0x0002);
                mdio_write(tp, 0x1F, 0x0000);

                mdio_write(tp, 0x1F, 0x0003);
                mdio_write(tp, 0x09, 0xA20F);
                mdio_write(tp, 0x1F, 0x0000);

                mdio_write(tp, 0x1F, 0x0005);
                mdio_write(tp, 0x05, 0x8B85);
                gphy_val = mdio_read(tp, 0x06) | BIT_14 | BIT_15;
                mdio_write(tp, 0x06, gphy_val);
                mdio_write(tp, 0x1f, 0x0000);

                mdio_write(tp, 0x1F, 0x0005);
                mdio_write(tp, 0x05, 0x8B5B);
                mdio_write(tp, 0x06, 0x9222);
                mdio_write(tp, 0x05, 0x8B6D);
                mdio_write(tp, 0x06, 0x8000);
                mdio_write(tp, 0x05, 0x8B76);
                mdio_write(tp, 0x06, 0x8000);
                mdio_write(tp, 0x1F, 0x0000);

                if (pdev->subsystem_vendor == 0x1043 &&
                    pdev->subsystem_device == 0x13F7) {

                        static const u16 evl_phy_value[] = {
                                0x8B56, 0x8B5F, 0x8B68, 0x8B71,
                                0x8B7A, 0x8A7B, 0x8A7E, 0x8A81,
                                0x8A84, 0x8A87
                        };

                        mdio_write(tp, 0x1F, 0x0005);
                        for (i = 0; i < ARRAY_SIZE(evl_phy_value); i++) {
                                mdio_write(tp, 0x05, evl_phy_value[i]);
                                gphy_val = (0xAA << 8) | (mdio_read(tp, 0x06) & 0xFF);
                                mdio_write(tp, 0x06, gphy_val);
                        }
                        mdio_write(tp, 0x1F, 0x0007);
                        mdio_write(tp, 0x1E, 0x0078);
                        mdio_write(tp, 0x17, 0x51AA);
                        mdio_write(tp, 0x1F, 0x0000);
                }

                mdio_write(tp, 0x1f, 0x0005);
                mdio_write(tp, 0x05, 0x8B54);
                mdio_write(tp, 0x06, mdio_read(tp, 0x06) & ~BIT_11);
                mdio_write(tp, 0x05, 0x8B5D);
                mdio_write(tp, 0x06, mdio_read(tp, 0x06) & ~BIT_11);
                mdio_write(tp, 0x05, 0x8A7C);
                mdio_write(tp, 0x06, mdio_read(tp, 0x06) & ~BIT_8);
                mdio_write(tp, 0x05, 0x8A7F);
                mdio_write(tp, 0x06, mdio_read(tp, 0x06) | BIT_8);
                mdio_write(tp, 0x05, 0x8A82);
                mdio_write(tp, 0x06, mdio_read(tp, 0x06) & ~BIT_8);
                mdio_write(tp, 0x05, 0x8A85);
                mdio_write(tp, 0x06, mdio_read(tp, 0x06) & ~BIT_8);
                mdio_write(tp, 0x05, 0x8A88);
                mdio_write(tp, 0x06, mdio_read(tp, 0x06) & ~BIT_8);
                mdio_write(tp, 0x1f, 0x0000);

                if (aspm) {
                        if (tp->HwHasWrRamCodeToMicroP == TRUE) {
                                mdio_write(tp, 0x1f, 0x0000);
                                gphy_val = mdio_read(tp, 0x15);
                                gphy_val |= BIT_12;
                                mdio_write(tp, 0x15, gphy_val);
                        }
                }
        } else if (tp->mcfg == CFG_METHOD_18) {
                if (tp->HwHasWrRamCodeToMicroP == TRUE) {
                        mdio_write(tp, 0x1F, 0x0005);
                        mdio_write(tp, 0x05, 0x8b80);
                        gphy_val = mdio_read(tp, 0x06);
                        gphy_val |= BIT_2 | BIT_1;
                        mdio_write(tp, 0x06, gphy_val);
                        mdio_write(tp, 0x1F, 0x0000);
                }

                mdio_write(tp, 0x1f, 0x0007);
                mdio_write(tp, 0x1e, 0x002D);
                gphy_val = mdio_read(tp, 0x18);
                gphy_val |= BIT_4;
                mdio_write(tp, 0x18, gphy_val);
                mdio_write(tp, 0x1f, 0x0000);
                gphy_val = mdio_read(tp, 0x14);
                gphy_val |= BIT_15;
                mdio_write(tp, 0x14, gphy_val);

                mdio_write(tp, 0x1F, 0x0005);
                mdio_write(tp, 0x05, 0x8B86);
                gphy_val = mdio_read(tp, 0x06);
                gphy_val |= BIT_0;
                mdio_write(tp, 0x06, gphy_val);
                mdio_write(tp, 0x1f, 0x0000);

                mdio_write(tp, 0x1F, 0x0005);
                mdio_write(tp, 0x05, 0x8B85);
                gphy_val = mdio_read(tp, 0x06);
                gphy_val |= BIT_14;
                mdio_write(tp, 0x06, gphy_val);
                mdio_write(tp, 0x1f, 0x0000);

                mdio_write(tp, 0x1F, 0x0003);
                mdio_write(tp, 0x09, 0xA20F);
                mdio_write(tp, 0x1F, 0x0000);

                mdio_write(tp, 0x1F, 0x0005);
                mdio_write(tp, 0x05, 0x8B55);
                mdio_write(tp, 0x06, 0x0000);
                mdio_write(tp, 0x05, 0x8B5E);
                mdio_write(tp, 0x06, 0x0000);
                mdio_write(tp, 0x05, 0x8B67);
                mdio_write(tp, 0x06, 0x0000);
                mdio_write(tp, 0x05, 0x8B70);
                mdio_write(tp, 0x06, 0x0000);
                mdio_write(tp, 0x1F, 0x0000);
                mdio_write(tp, 0x1F, 0x0007);
                mdio_write(tp, 0x1E, 0x0078);
                mdio_write(tp, 0x17, 0x0000);
                mdio_write(tp, 0x19, 0x00FB);
                mdio_write(tp, 0x1F, 0x0000);

                mdio_write(tp, 0x1F, 0x0005);
                mdio_write(tp, 0x05, 0x8B79);
                mdio_write(tp, 0x06, 0xAA00);
                mdio_write(tp, 0x1F, 0x0000);

                mdio_write(tp, 0x1f, 0x0003);
                mdio_write(tp, 0x01, 0x328A);
                mdio_write(tp, 0x1f, 0x0000);

                mdio_write(tp, 0x1f, 0x0005);
                mdio_write(tp, 0x05, 0x8B54);
                mdio_write(tp, 0x06, mdio_read(tp, 0x06) & ~BIT_11);
                mdio_write(tp, 0x05, 0x8B5D);
                mdio_write(tp, 0x06, mdio_read(tp, 0x06) & ~BIT_11);
                mdio_write(tp, 0x05, 0x8A7C);
                mdio_write(tp, 0x06, mdio_read(tp, 0x06) & ~BIT_8);
                mdio_write(tp, 0x05, 0x8A7F);
                mdio_write(tp, 0x06, mdio_read(tp, 0x06) | BIT_8);
                mdio_write(tp, 0x05, 0x8A82);
                mdio_write(tp, 0x06, mdio_read(tp, 0x06) & ~BIT_8);
                mdio_write(tp, 0x05, 0x8A85);
                mdio_write(tp, 0x06, mdio_read(tp, 0x06) & ~BIT_8);
                mdio_write(tp, 0x05, 0x8A88);
                mdio_write(tp, 0x06, mdio_read(tp, 0x06) & ~BIT_8);
                mdio_write(tp, 0x1f, 0x0000);

                if (tp->HwHasWrRamCodeToMicroP == TRUE) {
                        mdio_write(tp, 0x1f, 0x0005);
                        mdio_write(tp, 0x05, 0x8b85);
                        mdio_write(tp, 0x06, mdio_read(tp, 0x06) | BIT_15);
                        mdio_write(tp, 0x1f, 0x0000);
                }

                if (aspm) {
                        if (tp->HwHasWrRamCodeToMicroP == TRUE) {
                                mdio_write(tp, 0x1f, 0x0000);
                                gphy_val = mdio_read(tp, 0x15);
                                gphy_val |= BIT_12;
                                mdio_write(tp, 0x15, gphy_val);
                        }
                }
        } else if (tp->mcfg == CFG_METHOD_19) {
                if (tp->HwHasWrRamCodeToMicroP == TRUE) {
                        mdio_write(tp, 0x1F, 0x0005);
                        mdio_write(tp, 0x05, 0x8b80);
                        gphy_val = mdio_read(tp, 0x06);
                        gphy_val |= BIT_2 | BIT_1;
                        mdio_write(tp, 0x06, gphy_val);
                        mdio_write(tp, 0x1F, 0x0000);
                }

                mdio_write(tp, 0x1f, 0x0007);
                mdio_write(tp, 0x1e, 0x002D);
                gphy_val = mdio_read(tp, 0x18);
                gphy_val |= BIT_4;
                mdio_write(tp, 0x18, gphy_val);
                mdio_write(tp, 0x1f, 0x0000);
                gphy_val = mdio_read(tp, 0x14);
                gphy_val |= BIT_15;
                mdio_write(tp, 0x14, gphy_val);

                mdio_write(tp, 0x1F, 0x0005);
                mdio_write(tp, 0x05, 0x8B86);
                gphy_val = mdio_read(tp, 0x06);
                gphy_val |= BIT_0;
                mdio_write(tp, 0x06, gphy_val);
                mdio_write(tp, 0x1f, 0x0000);

                mdio_write(tp, 0x1f, 0x0005);
                mdio_write(tp, 0x05, 0x8B54);
                mdio_write(tp, 0x06, mdio_read(tp, 0x06) & ~BIT_11);
                mdio_write(tp, 0x05, 0x8B5D);
                mdio_write(tp, 0x06, mdio_read(tp, 0x06) & ~BIT_11);
                mdio_write(tp, 0x05, 0x8A7C);
                mdio_write(tp, 0x06, mdio_read(tp, 0x06) & ~BIT_8);
                mdio_write(tp, 0x05, 0x8A7F);
                mdio_write(tp, 0x06, mdio_read(tp, 0x06) | BIT_8);
                mdio_write(tp, 0x05, 0x8A82);
                mdio_write(tp, 0x06, mdio_read(tp, 0x06) & ~BIT_8);
                mdio_write(tp, 0x05, 0x8A85);
                mdio_write(tp, 0x06, mdio_read(tp, 0x06) & ~BIT_8);
                mdio_write(tp, 0x05, 0x8A88);
                mdio_write(tp, 0x06, mdio_read(tp, 0x06) & ~BIT_8);
                mdio_write(tp, 0x1f, 0x0000);

                if (tp->HwHasWrRamCodeToMicroP == TRUE) {
                        mdio_write(tp, 0x1f, 0x0005);
                        mdio_write(tp, 0x05, 0x8b85);
                        mdio_write(tp, 0x06, mdio_read(tp, 0x06) | BIT_15);
                        mdio_write(tp, 0x1f, 0x0000);
                }

                if (aspm) {
                        if (tp->HwHasWrRamCodeToMicroP == TRUE) {
                                mdio_write(tp, 0x1f, 0x0000);
                                gphy_val = mdio_read(tp, 0x15);
                                gphy_val |= BIT_12;
                                mdio_write(tp, 0x15, gphy_val);
                        }
                }
        } else if (tp->mcfg == CFG_METHOD_20) {

                if (tp->HwHasWrRamCodeToMicroP == TRUE) {
                        mdio_write(tp, 0x1F, 0x0005);
                        mdio_write(tp, 0x05, 0x8b80);
                        gphy_val = mdio_read(tp, 0x06);
                        gphy_val |= BIT_2 | BIT_1;
                        mdio_write(tp, 0x06, gphy_val);
                        mdio_write(tp, 0x1F, 0x0000);
                }

                mdio_write(tp, 0x1f, 0x0007);
                mdio_write(tp, 0x1e, 0x002D);
                gphy_val = mdio_read(tp, 0x18);
                gphy_val |= BIT_4;
                mdio_write(tp, 0x18, gphy_val);
                mdio_write(tp, 0x1f, 0x0000);
                gphy_val = mdio_read(tp, 0x14);
                gphy_val |= BIT_15;
                mdio_write(tp, 0x14, gphy_val);

                mdio_write(tp, 0x1F, 0x0005);
                mdio_write(tp, 0x05, 0x8B86);
                gphy_val = mdio_read(tp, 0x06);
                gphy_val |= BIT_0;
                mdio_write(tp, 0x06, gphy_val);
                mdio_write(tp, 0x1f, 0x0000);

                mdio_write(tp, 0x1F, 0x0005);
                mdio_write(tp, 0x05, 0x8B85);
                gphy_val = mdio_read(tp, 0x06);
                gphy_val |= BIT_14;
                mdio_write(tp, 0x06, gphy_val);
                mdio_write(tp, 0x1f, 0x0000);

                mdio_write(tp, 0x1F, 0x0003);
                mdio_write(tp, 0x09, 0xA20F);
                mdio_write(tp, 0x1F, 0x0000);

                mdio_write(tp, 0x1F, 0x0005);
                mdio_write(tp, 0x05, 0x8B55);
                mdio_write(tp, 0x06, 0x0000);
                mdio_write(tp, 0x05, 0x8B5E);
                mdio_write(tp, 0x06, 0x0000);
                mdio_write(tp, 0x05, 0x8B67);
                mdio_write(tp, 0x06, 0x0000);
                mdio_write(tp, 0x05, 0x8B70);
                mdio_write(tp, 0x06, 0x0000);
                mdio_write(tp, 0x1F, 0x0000);
                mdio_write(tp, 0x1F, 0x0007);
                mdio_write(tp, 0x1E, 0x0078);
                mdio_write(tp, 0x17, 0x0000);
                mdio_write(tp, 0x19, 0x00FB);
                mdio_write(tp, 0x1F, 0x0000);

                mdio_write(tp, 0x1F, 0x0005);
                mdio_write(tp, 0x05, 0x8B79);
                mdio_write(tp, 0x06, 0xAA00);
                mdio_write(tp, 0x1F, 0x0000);

                mdio_write(tp, 0x1f, 0x0005);
                mdio_write(tp, 0x05, 0x8B54);
                mdio_write(tp, 0x06, mdio_read(tp, 0x06) & ~BIT_11);
                mdio_write(tp, 0x05, 0x8B5D);
                mdio_write(tp, 0x06, mdio_read(tp, 0x06) & ~BIT_11);
                mdio_write(tp, 0x05, 0x8A7C);
                mdio_write(tp, 0x06, mdio_read(tp, 0x06) & ~BIT_8);
                mdio_write(tp, 0x05, 0x8A7F);
                mdio_write(tp, 0x06, mdio_read(tp, 0x06) | BIT_8);
                mdio_write(tp, 0x05, 0x8A82);
                mdio_write(tp, 0x06, mdio_read(tp, 0x06) & ~BIT_8);
                mdio_write(tp, 0x05, 0x8A85);
                mdio_write(tp, 0x06, mdio_read(tp, 0x06) & ~BIT_8);
                mdio_write(tp, 0x05, 0x8A88);
                mdio_write(tp, 0x06, mdio_read(tp, 0x06) & ~BIT_8);
                mdio_write(tp, 0x1f, 0x0000);

                if (tp->HwHasWrRamCodeToMicroP == TRUE) {
                        mdio_write(tp, 0x1f, 0x0005);
                        mdio_write(tp, 0x05, 0x8b85);
                        mdio_write(tp, 0x06, mdio_read(tp, 0x06) | BIT_15);
                        mdio_write(tp, 0x1f, 0x0000);
                }

                if (aspm) {
                        if (tp->HwHasWrRamCodeToMicroP == TRUE) {
                                mdio_write(tp, 0x1f, 0x0000);
                                gphy_val = mdio_read(tp, 0x15);
                                gphy_val |= BIT_12;
                                mdio_write(tp, 0x15, gphy_val);
                        }
                }
        } else if (tp->mcfg == CFG_METHOD_21) {
                mdio_write(tp, 0x1F, 0x0A46);
                gphy_val = mdio_read(tp, 0x10);
                mdio_write(tp, 0x1F, 0x0BCC);
                if (gphy_val & BIT_8)
                        ClearEthPhyBit(tp, 0x12, BIT_15);
                else
                        SetEthPhyBit(tp, 0x12, BIT_15);
                mdio_write(tp, 0x1F, 0x0A46);
                gphy_val = mdio_read(tp, 0x13);
                mdio_write(tp, 0x1F, 0x0C41);
                if (gphy_val & BIT_8)
                        SetEthPhyBit(tp, 0x15, BIT_1);
                else
                        ClearEthPhyBit(tp, 0x15, BIT_1);

                mdio_write(tp, 0x1F, 0x0A44);
                mdio_write(tp, 0x11, mdio_read(tp, 0x11) | BIT_2 | BIT_3);

                mdio_write(tp, 0x1F, 0x0BCC);
                mdio_write(tp, 0x14, mdio_read(tp, 0x14) & ~BIT_8);
                mdio_write(tp, 0x1F, 0x0A44);
                mdio_write(tp, 0x11, mdio_read(tp, 0x11) | BIT_7);
                mdio_write(tp, 0x11, mdio_read(tp, 0x11) | BIT_6);
                mdio_write(tp, 0x1F, 0x0A43);
                mdio_write(tp, 0x13, 0x8084);
                mdio_write(tp, 0x14, mdio_read(tp, 0x14) & ~(BIT_14 | BIT_13));
                mdio_write(tp, 0x10, mdio_read(tp, 0x10) | BIT_12);
                mdio_write(tp, 0x10, mdio_read(tp, 0x10) | BIT_1);
                mdio_write(tp, 0x10, mdio_read(tp, 0x10) | BIT_0);

                mdio_write(tp, 0x1F, 0x0A4B);
                mdio_write(tp, 0x11, mdio_read(tp, 0x11) | BIT_2);

                mdio_write(tp, 0x1F, 0x0A43);
                mdio_write(tp, 0x13, 0x8012);
                mdio_write(tp, 0x14, mdio_read(tp, 0x14) | BIT_15);

                mdio_write(tp, 0x1F, 0x0C42);
                gphy_val = mdio_read(tp, 0x11);
                gphy_val |= BIT_14;
                gphy_val &= ~BIT_13;
                mdio_write(tp, 0x11, gphy_val);

                mdio_write(tp, 0x1F, 0x0A43);
                mdio_write(tp, 0x13, 0x809A);
                mdio_write(tp, 0x14, 0x8022);
                mdio_write(tp, 0x13, 0x80A0);
                gphy_val = mdio_read(tp, 0x14) & 0x00FF;
                gphy_val |= 0x1000;
                mdio_write(tp, 0x14, gphy_val);
                mdio_write(tp, 0x13, 0x8088);
                mdio_write(tp, 0x14, 0x9222);

                if (aspm) {
                        if (tp->HwHasWrRamCodeToMicroP == TRUE) {
                                mdio_write(tp, 0x1F, 0x0A43);
                                mdio_write(tp, 0x10, mdio_read(tp, 0x10) | BIT_2);
                        }
                }

                mdio_write(tp, 0x1F, 0x0000);
        } else if (tp->mcfg == CFG_METHOD_22) {
                //do nothing
        } else if (tp->mcfg == CFG_METHOD_23) {
                mdio_write(tp, 0x1F, 0x0A44);
                mdio_write(tp, 0x11, mdio_read(tp, 0x11) | (BIT_3 | BIT_2));
                mdio_write(tp, 0x1F, 0x0000);

                mdio_write(tp, 0x1F, 0x0BCC);
                mdio_write(tp, 0x14, mdio_read(tp, 0x14) & ~BIT_8);
                mdio_write(tp, 0x1F, 0x0A44);
                mdio_write(tp, 0x11, mdio_read(tp, 0x11) | BIT_7);
                mdio_write(tp, 0x11, mdio_read(tp, 0x11) | BIT_6);
                mdio_write(tp, 0x1F, 0x0A43);
                mdio_write(tp, 0x13, 0x8084);
                mdio_write(tp, 0x14, mdio_read(tp, 0x14) & ~(BIT_14 | BIT_13));
                mdio_write(tp, 0x10, mdio_read(tp, 0x10) | BIT_12);
                mdio_write(tp, 0x10, mdio_read(tp, 0x10) | BIT_1);
                mdio_write(tp, 0x10, mdio_read(tp, 0x10) | BIT_0);

                mdio_write(tp, 0x1F, 0x0A4B);
                mdio_write(tp, 0x11, mdio_read(tp, 0x11) | BIT_2);
                mdio_write(tp, 0x1F, 0x0000);

                mdio_write(tp, 0x1F, 0x0A43);
                mdio_write(tp, 0x13, 0x8012);
                mdio_write(tp, 0x14, mdio_read(tp, 0x14) | BIT_15);
                mdio_write(tp, 0x1F, 0x0000);

                mdio_write(tp, 0x1F, 0x0C42);
                ClearAndSetEthPhyBit(tp,
                                     0x11,
                                     BIT_13,
                                     BIT_14
                                    );
                mdio_write(tp, 0x1F, 0x0000);

                if (aspm) {
                        if (tp->HwHasWrRamCodeToMicroP == TRUE) {
                                mdio_write(tp, 0x1F, 0x0A43);
                                mdio_write(tp, 0x10, mdio_read(tp, 0x10) | BIT_2);
                        }
                }
        } else if (tp->mcfg == CFG_METHOD_24) {
                mdio_write(tp, 0x1F, 0x0BCC);
                mdio_write(tp, 0x14, mdio_read(tp, 0x14) & ~BIT_8);
                mdio_write(tp, 0x1F, 0x0A44);
                mdio_write(tp, 0x11, mdio_read(tp, 0x11) | BIT_7);
                mdio_write(tp, 0x11, mdio_read(tp, 0x11) | BIT_6);
                mdio_write(tp, 0x1F, 0x0A43);
                mdio_write(tp, 0x13, 0x8084);
                mdio_write(tp, 0x14, mdio_read(tp, 0x14) & ~(BIT_14 | BIT_13));
                mdio_write(tp, 0x10, mdio_read(tp, 0x10) | BIT_12);
                mdio_write(tp, 0x10, mdio_read(tp, 0x10) | BIT_1);
                mdio_write(tp, 0x10, mdio_read(tp, 0x10) | BIT_0);
                mdio_write(tp, 0x1F, 0x0A43);
                mdio_write(tp, 0x13, 0x8012);
                mdio_write(tp, 0x14, mdio_read(tp, 0x14) | BIT_15);

                mdio_write(tp, 0x1F, 0x0C42);
                gphy_val = mdio_read(tp, 0x11);
                gphy_val |= BIT_14;
                gphy_val &= ~BIT_13;
                mdio_write(tp, 0x11, gphy_val);

                if (aspm) {
                        if (tp->HwHasWrRamCodeToMicroP == TRUE) {
                                mdio_write(tp, 0x1F, 0x0A43);
                                mdio_write(tp, 0x10, mdio_read(tp, 0x10) | BIT_2);
                        }
                }
        } else if (tp->mcfg == CFG_METHOD_25 || tp->mcfg == CFG_METHOD_26) {
                mdio_write(tp, 0x1F, 0x0BCC);
                mdio_write(tp, 0x14, mdio_read(tp, 0x14) & ~BIT_8);
                mdio_write(tp, 0x1F, 0x0A44);
                mdio_write(tp, 0x11, mdio_read(tp, 0x11) | BIT_7);
                mdio_write(tp, 0x11, mdio_read(tp, 0x11) | BIT_6);
                mdio_write(tp, 0x1F, 0x0A43);
                mdio_write(tp, 0x13, 0x8084);
                mdio_write(tp, 0x14, mdio_read(tp, 0x14) & ~(BIT_14 | BIT_13));
                mdio_write(tp, 0x10, mdio_read(tp, 0x10) | BIT_12);
                mdio_write(tp, 0x10, mdio_read(tp, 0x10) | BIT_1);
                mdio_write(tp, 0x10, mdio_read(tp, 0x10) | BIT_0);

                mdio_write(tp, 0x1F, 0x0A43);
                mdio_write(tp, 0x13, 0x8012);
                mdio_write(tp, 0x14, mdio_read(tp, 0x14) | BIT_15);

                mdio_write(tp, 0x1F, 0x0BCE);
                mdio_write(tp, 0x12, 0x8860);

                mdio_write(tp, 0x1F, 0x0A43);
                mdio_write(tp, 0x13, 0x80F3);
                mdio_write(tp, 0x14, (mdio_read(tp, 0x14) & ~0xFF00) | 0x8B00);
                mdio_write(tp, 0x13, 0x80F0);
                mdio_write(tp, 0x14, (mdio_read(tp, 0x14) & ~0xFF00) | 0x3A00);
                mdio_write(tp, 0x13, 0x80EF);
                mdio_write(tp, 0x14, (mdio_read(tp, 0x14) & ~0xFF00) | 0x0500);
                mdio_write(tp, 0x13, 0x80F6);
                mdio_write(tp, 0x14, (mdio_read(tp, 0x14) & ~0xFF00) | 0x6E00);
                mdio_write(tp, 0x13, 0x80EC);
                mdio_write(tp, 0x14, (mdio_read(tp, 0x14) & ~0xFF00) | 0x6800);
                mdio_write(tp, 0x13, 0x80ED);
                mdio_write(tp, 0x14, (mdio_read(tp, 0x14) & ~0xFF00) | 0x7C00);
                mdio_write(tp, 0x13, 0x80F2);
                mdio_write(tp, 0x14, (mdio_read(tp, 0x14) & ~0xFF00) | 0xF400);
                mdio_write(tp, 0x13, 0x80F4);
                mdio_write(tp, 0x14, (mdio_read(tp, 0x14) & ~0xFF00) | 0x8500);
                mdio_write(tp, 0x1F, 0x0A43);
                mdio_write(tp, 0x13, 0x8110);
                mdio_write(tp, 0x14, (mdio_read(tp, 0x14) & ~0xFF00) | 0xA800);
                mdio_write(tp, 0x13, 0x810F);
                mdio_write(tp, 0x14, (mdio_read(tp, 0x14) & ~0xFF00) | 0x1D00);
                mdio_write(tp, 0x13, 0x8111);
                mdio_write(tp, 0x14, (mdio_read(tp, 0x14) & ~0xFF00) | 0xF500);
                mdio_write(tp, 0x13, 0x8113);
                mdio_write(tp, 0x14, (mdio_read(tp, 0x14) & ~0xFF00) | 0x6100);
                mdio_write(tp, 0x13, 0x8115);
                mdio_write(tp, 0x14, (mdio_read(tp, 0x14) & ~0xFF00) | 0x9200);
                mdio_write(tp, 0x13, 0x810E);
                mdio_write(tp, 0x14, (mdio_read(tp, 0x14) & ~0xFF00) | 0x0400);
                mdio_write(tp, 0x13, 0x810C);
                mdio_write(tp, 0x14, (mdio_read(tp, 0x14) & ~0xFF00) | 0x7C00);
                mdio_write(tp, 0x13, 0x810B);
                mdio_write(tp, 0x14, (mdio_read(tp, 0x14) & ~0xFF00) | 0x5A00);
                mdio_write(tp, 0x1F, 0x0A43);
                mdio_write(tp, 0x13, 0x80D1);
                mdio_write(tp, 0x14, (mdio_read(tp, 0x14) & ~0xFF00) | 0xFF00);
                mdio_write(tp, 0x13, 0x80CD);
                mdio_write(tp, 0x14, (mdio_read(tp, 0x14) & ~0xFF00) | 0x9E00);
                mdio_write(tp, 0x13, 0x80D3);
                mdio_write(tp, 0x14, (mdio_read(tp, 0x14) & ~0xFF00) | 0x0E00);
                mdio_write(tp, 0x13, 0x80D5);
                mdio_write(tp, 0x14, (mdio_read(tp, 0x14) & ~0xFF00) | 0xCA00);
                mdio_write(tp, 0x13, 0x80D7);
                mdio_write(tp, 0x14, (mdio_read(tp, 0x14) & ~0xFF00) | 0x8400);

                if (aspm) {
                        if (tp->HwHasWrRamCodeToMicroP == TRUE) {
                                mdio_write(tp, 0x1F, 0x0A43);
                                mdio_write(tp, 0x10, mdio_read(tp, 0x10) | BIT_2);
                        }
                }
        } else if (tp->mcfg == CFG_METHOD_27 || tp->mcfg == CFG_METHOD_28) {
                mdio_write(tp, 0x1F, 0x0BCC);
                mdio_write(tp, 0x14, mdio_read(tp, 0x14) & ~BIT_8);
                mdio_write(tp, 0x1F, 0x0A44);
                mdio_write(tp, 0x11, mdio_read(tp, 0x11) | BIT_7);
                mdio_write(tp, 0x11, mdio_read(tp, 0x11) | BIT_6);
                mdio_write(tp, 0x1F, 0x0A43);
                mdio_write(tp, 0x13, 0x8084);
                mdio_write(tp, 0x14, mdio_read(tp, 0x14) & ~(BIT_14 | BIT_13));
                mdio_write(tp, 0x10, mdio_read(tp, 0x10) | BIT_12);
                mdio_write(tp, 0x10, mdio_read(tp, 0x10) | BIT_1);
                mdio_write(tp, 0x10, mdio_read(tp, 0x10) | BIT_0);

                mdio_write(tp, 0x1F, 0x0A43);
                mdio_write(tp, 0x13, 0x8012);
                mdio_write(tp, 0x14, mdio_read(tp, 0x14) | BIT_15);

                mdio_write(tp, 0x1F, 0x0C42);
                mdio_write(tp, 0x11, (mdio_read(tp, 0x11) & ~BIT_13) | BIT_14);

                mdio_write(tp, 0x1F, 0x0A43);
                mdio_write(tp, 0x13, 0x80F3);
                mdio_write(tp, 0x14, (mdio_read(tp, 0x14) & ~0xFF00) | 0x8B00);
                mdio_write(tp, 0x13, 0x80F0);
                mdio_write(tp, 0x14, (mdio_read(tp, 0x14) & ~0xFF00) | 0x3A00);
                mdio_write(tp, 0x13, 0x80EF);
                mdio_write(tp, 0x14, (mdio_read(tp, 0x14) & ~0xFF00) | 0x0500);
                mdio_write(tp, 0x13, 0x80F6);
                mdio_write(tp, 0x14, (mdio_read(tp, 0x14) & ~0xFF00) | 0x6E00);
                mdio_write(tp, 0x13, 0x80EC);
                mdio_write(tp, 0x14, (mdio_read(tp, 0x14) & ~0xFF00) | 0x6800);
                mdio_write(tp, 0x13, 0x80ED);
                mdio_write(tp, 0x14, (mdio_read(tp, 0x14) & ~0xFF00) | 0x7C00);
                mdio_write(tp, 0x13, 0x80F2);
                mdio_write(tp, 0x14, (mdio_read(tp, 0x14) & ~0xFF00) | 0xF400);
                mdio_write(tp, 0x13, 0x80F4);
                mdio_write(tp, 0x14, (mdio_read(tp, 0x14) & ~0xFF00) | 0x8500);
                mdio_write(tp, 0x1F, 0x0A43);
                mdio_write(tp, 0x13, 0x8110);
                mdio_write(tp, 0x14, (mdio_read(tp, 0x14) & ~0xFF00) | 0xA800);
                mdio_write(tp, 0x13, 0x810F);
                mdio_write(tp, 0x14, (mdio_read(tp, 0x14) & ~0xFF00) | 0x1D00);
                mdio_write(tp, 0x13, 0x8111);
                mdio_write(tp, 0x14, (mdio_read(tp, 0x14) & ~0xFF00) | 0xF500);
                mdio_write(tp, 0x13, 0x8113);
                mdio_write(tp, 0x14, (mdio_read(tp, 0x14) & ~0xFF00) | 0x6100);
                mdio_write(tp, 0x13, 0x8115);
                mdio_write(tp, 0x14, (mdio_read(tp, 0x14) & ~0xFF00) | 0x9200);
                mdio_write(tp, 0x13, 0x810E);
                mdio_write(tp, 0x14, (mdio_read(tp, 0x14) & ~0xFF00) | 0x0400);
                mdio_write(tp, 0x13, 0x810C);
                mdio_write(tp, 0x14, (mdio_read(tp, 0x14) & ~0xFF00) | 0x7C00);
                mdio_write(tp, 0x13, 0x810B);
                mdio_write(tp, 0x14, (mdio_read(tp, 0x14) & ~0xFF00) | 0x5A00);
                mdio_write(tp, 0x1F, 0x0A43);
                mdio_write(tp, 0x13, 0x80D1);
                mdio_write(tp, 0x14, (mdio_read(tp, 0x14) & ~0xFF00) | 0xFF00);
                mdio_write(tp, 0x13, 0x80CD);
                mdio_write(tp, 0x14, (mdio_read(tp, 0x14) & ~0xFF00) | 0x9E00);
                mdio_write(tp, 0x13, 0x80D3);
                mdio_write(tp, 0x14, (mdio_read(tp, 0x14) & ~0xFF00) | 0x0E00);
                mdio_write(tp, 0x13, 0x80D5);
                mdio_write(tp, 0x14, (mdio_read(tp, 0x14) & ~0xFF00) | 0xCA00);
                mdio_write(tp, 0x13, 0x80D7);
                mdio_write(tp, 0x14, (mdio_read(tp, 0x14) & ~0xFF00) | 0x8400);

                if (aspm) {
                        if (tp->HwHasWrRamCodeToMicroP == TRUE) {
                                mdio_write(tp, 0x1F, 0x0A43);
                                mdio_write(tp, 0x10, mdio_read(tp, 0x10) | BIT_2);
                        }
                }
        } else if (tp->mcfg == CFG_METHOD_29) {
                mdio_write(tp, 0x1F, 0x0A43);
                mdio_write(tp, 0x13, 0x809b);
                ClearAndSetEthPhyBit( tp,
                                      0x14,
                                      0xF800 ,
                                      0x8000
                                    );
                mdio_write(tp, 0x13, 0x80A2);
                ClearAndSetEthPhyBit( tp,
                                      0x14,
                                      0xFF00 ,
                                      0x8000
                                    );
                mdio_write(tp, 0x13, 0x80A4);
                ClearAndSetEthPhyBit( tp,
                                      0x14,
                                      0xFF00 ,
                                      0x8500
                                    );
                mdio_write(tp, 0x13, 0x809C);
                ClearAndSetEthPhyBit( tp,
                                      0x14,
                                      0xFF00 ,
                                      0xbd00
                                    );
                mdio_write(tp, 0x1F, 0x0000);

                mdio_write(tp, 0x1F, 0x0A43);
                mdio_write(tp, 0x13, 0x80AD);
                ClearAndSetEthPhyBit( tp,
                                      0x14,
                                      0xF800 ,
                                      0x7000
                                    );
                mdio_write(tp, 0x13, 0x80B4);
                ClearAndSetEthPhyBit( tp,
                                      0x14,
                                      0xFF00 ,
                                      0x5000
                                    );
                mdio_write(tp, 0x13, 0x80AC);
                ClearAndSetEthPhyBit( tp,
                                      0x14,
                                      0xFF00 ,
                                      0x4000
                                    );
                mdio_write(tp, 0x1F, 0x0000);

                mdio_write(tp, 0x1F, 0x0A43);
                mdio_write(tp, 0x13, 0x808E);
                ClearAndSetEthPhyBit( tp,
                                      0x14,
                                      0xFF00 ,
                                      0x1200
                                    );
                mdio_write(tp, 0x13, 0x8090);
                ClearAndSetEthPhyBit( tp,
                                      0x14,
                                      0xFF00 ,
                                      0xE500
                                    );
                mdio_write(tp, 0x13, 0x8092);
                ClearAndSetEthPhyBit( tp,
                                      0x14,
                                      0xFF00 ,
                                      0x9F00
                                    );
                mdio_write(tp, 0x1F, 0x0000);

                if (tp->HwHasWrRamCodeToMicroP) {
                        u16 dout_tapbin;

                        dout_tapbin = 0x0000;
                        mdio_write( tp, 0x1F, 0x0A46 );
                        gphy_val = mdio_read( tp, 0x13 );
                        gphy_val &= (BIT_1|BIT_0);
                        gphy_val <<= 2;
                        dout_tapbin |= gphy_val;

                        gphy_val = mdio_read( tp, 0x12 );
                        gphy_val &= (BIT_15|BIT_14);
                        gphy_val >>= 14;
                        dout_tapbin |= gphy_val;

                        dout_tapbin = ~( dout_tapbin^BIT_3 );
                        dout_tapbin <<= 12;
                        dout_tapbin &= 0xF000;

                        mdio_write( tp, 0x1F, 0x0A43 );

                        mdio_write( tp, 0x13, 0x827A );
                        ClearAndSetEthPhyBit( tp,
                                              0x14,
                                              BIT_15|BIT_14|BIT_13|BIT_12,
                                              dout_tapbin
                                            );


                        mdio_write( tp, 0x13, 0x827B );
                        ClearAndSetEthPhyBit( tp,
                                              0x14,
                                              BIT_15|BIT_14|BIT_13|BIT_12,
                                              dout_tapbin
                                            );


                        mdio_write( tp, 0x13, 0x827C );
                        ClearAndSetEthPhyBit( tp,
                                              0x14,
                                              BIT_15|BIT_14|BIT_13|BIT_12,
                                              dout_tapbin
                                            );


                        mdio_write( tp, 0x13, 0x827D );
                        ClearAndSetEthPhyBit( tp,
                                              0x14,
                                              BIT_15|BIT_14|BIT_13|BIT_12,
                                              dout_tapbin
                                            );

                        mdio_write(tp, 0x1F, 0x0A43);
                        mdio_write(tp, 0x13, 0x8011);
                        SetEthPhyBit(tp, 0x14, BIT_11);
                        mdio_write(tp, 0x1F, 0x0A42);
                        SetEthPhyBit(tp, 0x16, BIT_1);
                }

                mdio_write(tp, 0x1F, 0x0A44);
                SetEthPhyBit( tp, 0x11, BIT_11 );
                mdio_write(tp, 0x1F, 0x0000);


                mdio_write(tp, 0x1F, 0x0BCA);
                ClearAndSetEthPhyBit( tp,
                                      0x17,
                                      (BIT_13 | BIT_12) ,
                                      BIT_14
                                    );
                mdio_write(tp, 0x1F, 0x0000);

                mdio_write(tp, 0x1F, 0x0A43);
                mdio_write(tp, 0x13, 0x803F);
                ClearEthPhyBit( tp, 0x14, (BIT_13 | BIT_12));
                mdio_write(tp, 0x13, 0x8047);
                ClearEthPhyBit( tp, 0x14, (BIT_13 | BIT_12));
                mdio_write(tp, 0x13, 0x804F);
                ClearEthPhyBit( tp, 0x14, (BIT_13 | BIT_12));
                mdio_write(tp, 0x13, 0x8057);
                ClearEthPhyBit( tp, 0x14, (BIT_13 | BIT_12));
                mdio_write(tp, 0x13, 0x805F);
                ClearEthPhyBit( tp, 0x14, (BIT_13 | BIT_12));
                mdio_write(tp, 0x13, 0x8067 );
                ClearEthPhyBit( tp, 0x14, (BIT_13 | BIT_12));
                mdio_write(tp, 0x13, 0x806F );
                ClearEthPhyBit( tp, 0x14, (BIT_13 | BIT_12));
                mdio_write(tp, 0x1F, 0x0000);

                if (aspm) {
                        if (tp->HwHasWrRamCodeToMicroP == TRUE) {
                                mdio_write(tp, 0x1F, 0x0A43);
                                SetEthPhyBit( tp, 0x10, BIT_2 );
                                mdio_write(tp, 0x1F, 0x0000);
                        }
                }
        } else if (tp->mcfg == CFG_METHOD_30) {
                mdio_write(tp, 0x1F, 0x0A43);
                mdio_write(tp, 0x13, 0x808A);
                ClearAndSetEthPhyBit( tp,
                                      0x14,
                                      BIT_5 | BIT_4 | BIT_3 | BIT_2 | BIT_1 | BIT_0,
                                      0x0A );

                if (tp->HwHasWrRamCodeToMicroP) {
                        mdio_write(tp, 0x1F, 0x0A43);
                        mdio_write(tp, 0x13, 0x8011);
                        SetEthPhyBit(tp, 0x14, BIT_11);
                        mdio_write(tp, 0x1F, 0x0A42);
                        SetEthPhyBit(tp, 0x16, BIT_1);
                }

                mdio_write(tp, 0x1F, 0x0A44);
                SetEthPhyBit( tp, 0x11, BIT_11 );
                mdio_write(tp, 0x1F, 0x0000);

                if(tp->RequireAdcBiasPatch) {
                        mdio_write(tp, 0x1F, 0x0BCF);
                        mdio_write(tp, 0x16, tp->AdcBiasPatchIoffset);
                        mdio_write(tp, 0x1F, 0x0000);
                }

                {
                        u16 rlen;

                        mdio_write(tp, 0x1F, 0x0BCD);
                        gphy_val = mdio_read( tp, 0x16 );
                        gphy_val &= 0x000F;

                        if( gphy_val > 3 ) {
                                rlen = gphy_val - 3;
                        } else {
                                rlen = 0;
                        }

                        gphy_val = rlen | (rlen<<4) | (rlen<<8) | (rlen<<12);

                        mdio_write(tp, 0x1F, 0x0BCD);
                        mdio_write(tp, 0x17, gphy_val);
                        mdio_write(tp, 0x1F, 0x0000);
                }

                if (aspm) {
                        if (tp->HwHasWrRamCodeToMicroP == TRUE) {
                                mdio_write(tp, 0x1F, 0x0A43);
                                SetEthPhyBit( tp, 0x10, BIT_2 );
                                mdio_write(tp, 0x1F, 0x0000);
                        }
                }
        }

        //EthPhyPPSW
        if (tp->mcfg == CFG_METHOD_21 || tp->mcfg == CFG_METHOD_22 ||
            tp->mcfg == CFG_METHOD_24 || tp->mcfg == CFG_METHOD_25 ||
            tp->mcfg == CFG_METHOD_26) {
                //disable EthPhyPPSW
                mdio_write(tp, 0x1F, 0x0BCD);
                mdio_write(tp, 0x14, 0x5065);
                mdio_write(tp, 0x14, 0xD065);
                mdio_write(tp, 0x1F, 0x0BC8);
                mdio_write(tp, 0x11, 0x5655);
                mdio_write(tp, 0x1F, 0x0BCD);
                mdio_write(tp, 0x14, 0x1065);
                mdio_write(tp, 0x14, 0x9065);
                mdio_write(tp, 0x14, 0x1065);
                mdio_write(tp, 0x1F, 0x0000);
        } else if (tp->mcfg == CFG_METHOD_29 || tp->mcfg == CFG_METHOD_30) {
                //enable EthPhyPPSW
                mdio_write(tp, 0x1F, 0x0A44);
                SetEthPhyBit( tp, 0x11, BIT_7 );
                mdio_write(tp, 0x1F, 0x0000);
        }

        /*ocp phy power saving*/
        if (tp->mcfg == CFG_METHOD_25 || tp->mcfg == CFG_METHOD_26 ||
            tp->mcfg == CFG_METHOD_27 || tp->mcfg == CFG_METHOD_28 ||
            tp->mcfg == CFG_METHOD_29 || tp->mcfg == CFG_METHOD_30) {
                if (aspm) {
                        mdio_write_phy_ocp(tp, 0x0C41, 0x13, 0x0000);
                        mdio_write_phy_ocp(tp, 0x0C41, 0x13, 0x0050);
                }
        }

        mdio_write(tp, 0x1F, 0x0000);

        spin_unlock_irqrestore(&tp->phy_lock, flags);

        if (tp->HwHasWrRamCodeToMicroP == TRUE) {
                if (eee_enable == 1)
                        rtl8168_enable_EEE(tp);
                else
                        rtl8168_disable_EEE(tp);
        }
}

static inline void rtl8168_delete_esd_timer(struct net_device *dev, struct timer_list *timer)
{
        del_timer_sync(timer);
}

static inline void rtl8168_request_esd_timer(struct net_device *dev)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        struct timer_list *timer = &tp->esd_timer;

        init_timer(timer);
        timer->expires = jiffies + RTL8168_ESD_TIMEOUT;
        timer->data = (unsigned long)(dev);
        timer->function = rtl8168_esd_timer;
        add_timer(timer);
}

static inline void rtl8168_delete_link_timer(struct net_device *dev, struct timer_list *timer)
{
        del_timer_sync(timer);
}

static inline void rtl8168_request_link_timer(struct net_device *dev)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        struct timer_list *timer = &tp->link_timer;

        init_timer(timer);
        timer->expires = jiffies + RTL8168_LINK_TIMEOUT;
        timer->data = (unsigned long)(dev);
        timer->function = rtl8168_link_timer;
        add_timer(timer);
}

#ifdef CONFIG_NET_POLL_CONTROLLER
/*
 * Polling 'interrupt' - used by things like netconsole to send skbs
 * without having to re-enable interrupts. It's not called while
 * the interrupt routine is executing.
 */
static void
rtl8168_netpoll(struct net_device *dev)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        struct pci_dev *pdev = tp->pci_dev;

        disable_irq(pdev->irq);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
        rtl8168_interrupt(pdev->irq, dev, NULL);
#else
        rtl8168_interrupt(pdev->irq, dev);
#endif
        enable_irq(pdev->irq);
}
#endif

static void
rtl8168_get_bios_setting(struct net_device *dev)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;

        switch (tp->mcfg) {
        case CFG_METHOD_9:
        case CFG_METHOD_10:
        case CFG_METHOD_11:
        case CFG_METHOD_12:
        case CFG_METHOD_13:
        case CFG_METHOD_14:
        case CFG_METHOD_15:
        case CFG_METHOD_16:
        case CFG_METHOD_17:
        case CFG_METHOD_18:
        case CFG_METHOD_19:
        case CFG_METHOD_20:
        case CFG_METHOD_21:
        case CFG_METHOD_22:
        case CFG_METHOD_23:
        case CFG_METHOD_24:
        case CFG_METHOD_25:
        case CFG_METHOD_26:
        case CFG_METHOD_27:
        case CFG_METHOD_28:
        case CFG_METHOD_29:
        case CFG_METHOD_30:
                tp->bios_setting = RTL_R32(0x8c);
                break;
        }
}

static void
rtl8168_set_bios_setting(struct net_device *dev)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;

        switch (tp->mcfg) {
        case CFG_METHOD_9:
        case CFG_METHOD_10:
        case CFG_METHOD_11:
        case CFG_METHOD_12:
        case CFG_METHOD_13:
        case CFG_METHOD_14:
        case CFG_METHOD_15:
        case CFG_METHOD_16:
        case CFG_METHOD_17:
        case CFG_METHOD_18:
        case CFG_METHOD_19:
        case CFG_METHOD_20:
        case CFG_METHOD_21:
        case CFG_METHOD_22:
        case CFG_METHOD_23:
        case CFG_METHOD_24:
        case CFG_METHOD_25:
        case CFG_METHOD_26:
        case CFG_METHOD_27:
        case CFG_METHOD_28:
        case CFG_METHOD_29:
        case CFG_METHOD_30:
                RTL_W32(0x8C, tp->bios_setting);
                break;
        }
}

static void
rtl8168_init_software_variable(struct net_device *dev)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        struct pci_dev *pdev = tp->pci_dev;

        rtl8168_get_bios_setting(dev);

        switch (tp->mcfg) {
        case CFG_METHOD_11:
        case CFG_METHOD_12:
        case CFG_METHOD_13:
                tp->HwSuppDashVer = 1;
                break;
        case CFG_METHOD_23:
        case CFG_METHOD_27:
        case CFG_METHOD_28:
                tp->HwSuppDashVer = 2;
                break;
        default:
                tp->HwSuppDashVer = 0;
                break;
        }

        switch (tp->mcfg) {
        case CFG_METHOD_21:
        case CFG_METHOD_22:
        case CFG_METHOD_23:
        case CFG_METHOD_24:
        case CFG_METHOD_25:
        case CFG_METHOD_26:
        case CFG_METHOD_27:
        case CFG_METHOD_28:
        case CFG_METHOD_29:
        case CFG_METHOD_30:
                tp->HwSuppNowIsOobVer = 1;
                break;
        }

#ifdef ENABLE_REALWOW_SUPPORT
        get_realwow_hw_version(dev);
#endif //ENABLE_REALWOW_SUPPORT

        if (HW_DASH_SUPPORT_DASH(tp) && rtl8168_check_dash(tp))
                tp->DASH = 1;
        else
                tp->DASH = 0;

        switch (tp->mcfg) {
        case CFG_METHOD_1:
                tp->intr_mask = RxDescUnavail | RxFIFOOver | TxDescUnavail | TxOK | RxOK | SWInt;
                tp->timer_intr_mask = PCSTimeout | RxFIFOOver;
                break;
        case CFG_METHOD_2:
        case CFG_METHOD_3:
        case CFG_METHOD_4:
                tp->intr_mask = RxDescUnavail | TxDescUnavail | TxOK | RxOK | SWInt;
                tp->timer_intr_mask = PCSTimeout;
                break;
        default:
                tp->intr_mask = RxDescUnavail | TxOK | RxOK | SWInt;
                tp->timer_intr_mask = PCSTimeout;
                break;
        }

#ifdef ENABLE_DASH_SUPPORT
        if(tp->DASH) {
                if( HW_DASH_SUPPORT_TYPE_2( tp ) ) {
                        tp->timer_intr_mask |= ( ISRIMR_DASH_INTR_EN | ISRIMR_DASH_INTR_CMAC_RESET);
                        tp->intr_mask |= ( ISRIMR_DASH_INTR_EN | ISRIMR_DASH_INTR_CMAC_RESET);
                } else {
                        tp->timer_intr_mask |= ( ISRIMR_DP_DASH_OK | ISRIMR_DP_HOST_OK | ISRIMR_DP_REQSYS_OK );
                        tp->intr_mask |= ( ISRIMR_DP_DASH_OK | ISRIMR_DP_HOST_OK | ISRIMR_DP_REQSYS_OK );
                }
        }
#endif

        tp->max_jumbo_frame_size = rtl_chip_info[tp->chipset].jumbo_frame_sz;

        if (aspm) {
                switch (tp->mcfg) {
                case CFG_METHOD_21:
                case CFG_METHOD_22:
                case CFG_METHOD_23:
                case CFG_METHOD_24:
                case CFG_METHOD_25:
                case CFG_METHOD_26:
                case CFG_METHOD_27:
                case CFG_METHOD_28:
                case CFG_METHOD_29:
                case CFG_METHOD_30:
                        tp->org_pci_offset_99 = rtl8168_csi_fun0_read_byte(tp, 0x99);
                        tp->org_pci_offset_99 &= ~(BIT_5|BIT_6);
                        break;
                }
                switch (tp->mcfg) {
                case CFG_METHOD_24:
                case CFG_METHOD_25:
                case CFG_METHOD_26:
                case CFG_METHOD_27:
                case CFG_METHOD_28:
                case CFG_METHOD_29:
                case CFG_METHOD_30:
                        tp->org_pci_offset_180 = rtl8168_csi_fun0_read_byte(tp, 0x180);
                        break;
                }
        }

        pci_read_config_byte(pdev, 0x80, &tp->org_pci_offset_80);
        pci_read_config_byte(pdev, 0x81, &tp->org_pci_offset_81);

        switch (tp->mcfg) {
        case CFG_METHOD_16:
        case CFG_METHOD_17:
        case CFG_METHOD_18:
        case CFG_METHOD_19:
        case CFG_METHOD_20:
        case CFG_METHOD_21:
        case CFG_METHOD_22:
        case CFG_METHOD_23:
        case CFG_METHOD_24:
        case CFG_METHOD_25:
        case CFG_METHOD_26:
        case CFG_METHOD_27:
                if ((tp->features & RTL_FEATURE_MSI) && (tp->org_pci_offset_80 & BIT_1))
                        tp->use_timer_interrrupt = FALSE;
                else
                        tp->use_timer_interrrupt = TRUE;
                break;
        default:
                tp->use_timer_interrrupt = TRUE;
                break;
        }

        switch (tp->mcfg) {
        case CFG_METHOD_1:
        case CFG_METHOD_2:
        case CFG_METHOD_3:
                tp->ShortPacketSwChecksum = TRUE;
                break;
        case CFG_METHOD_16:
        case CFG_METHOD_17:
                tp->ShortPacketSwChecksum = TRUE;
                tp->UseSwPaddingShortPkt = TRUE;
                break;
        }

        switch (tp->mcfg) {
        case CFG_METHOD_30: {
                u16 ioffset_p3, ioffset_p2, ioffset_p1, ioffset_p0;
                u16 TmpUshort;

                mac_ocp_write( tp, 0xDD02, 0x807D);
                TmpUshort = mac_ocp_read( tp, 0xDD02 );
                ioffset_p3 = ( (TmpUshort & BIT_7) >>7 );
                ioffset_p3 <<= 3;
                TmpUshort = mac_ocp_read( tp, 0xDD00 );

                ioffset_p3 |= ((TmpUshort & (BIT_15 | BIT_14 | BIT_13))>>13);

                ioffset_p2 = ((TmpUshort & (BIT_12|BIT_11|BIT_10|BIT_9))>>9);
                ioffset_p1 = ((TmpUshort & (BIT_8|BIT_7|BIT_6|BIT_5))>>5);

                ioffset_p0 = ( (TmpUshort & BIT_4) >>4 );
                ioffset_p0 <<= 3;
                ioffset_p0 |= (TmpUshort & (BIT_2| BIT_1 | BIT_0));

                if((ioffset_p3 == 0x0F) && (ioffset_p2 == 0x0F) && (ioffset_p1 == 0x0F) && (ioffset_p0 == 0x0F)) {
                        tp->RequireAdcBiasPatch = FALSE;
                } else {
                        tp->RequireAdcBiasPatch = TRUE;
                        tp->AdcBiasPatchIoffset = (ioffset_p3<<12)|(ioffset_p2<<8)|(ioffset_p1<<4)|(ioffset_p0);
                }
        }
        break;
        }

        switch (tp->mcfg) {
        case CFG_METHOD_29:
        case CFG_METHOD_30: {
                u16 rg_saw_cnt;

                mdio_write(tp, 0x1F, 0x0C42);
                rg_saw_cnt = mdio_read(tp, 0x13);
                rg_saw_cnt &= ~(BIT_15|BIT_14);
                mdio_write(tp, 0x1F, 0x0000);

                if ( rg_saw_cnt > 0) {
                        tp->SwrCnt1msIni = 16000000/rg_saw_cnt;
                        tp->SwrCnt1msIni &= 0x0FFF;

                        tp->RequireAdjustUpsTxLinkPulseTiming = TRUE;
                }
        }
        break;
        }

        if (pdev->subsystem_vendor == 0x144d) {
                if (pdev->subsystem_device == 0xc098 ||
                    pdev->subsystem_device == 0xc0b1 ||
                    pdev->subsystem_device == 0xc0b8)
                        hwoptimize |= HW_PATCH_SAMSUNG_LAN_DONGLE;
        }

        if (hwoptimize & HW_PATCH_SAMSUNG_LAN_DONGLE) {
                switch (tp->mcfg) {
                case CFG_METHOD_14:
                case CFG_METHOD_15:
                case CFG_METHOD_16:
                case CFG_METHOD_17:
                case CFG_METHOD_18:
                case CFG_METHOD_19:
                case CFG_METHOD_20:
                case CFG_METHOD_30:
                        tp->RequiredSecLanDonglePatch = TRUE;
                        break;
                }
        }

        switch (tp->mcfg) {
        case CFG_METHOD_14:
        case CFG_METHOD_15:
                tp->sw_ram_code_ver = NIC_RAMCODE_VERSION_CFG_METHOD_14;
                break;
        case CFG_METHOD_16:
        case CFG_METHOD_17:
                tp->sw_ram_code_ver = NIC_RAMCODE_VERSION_CFG_METHOD_16;
                break;
        case CFG_METHOD_18:
        case CFG_METHOD_19:
                tp->sw_ram_code_ver = NIC_RAMCODE_VERSION_CFG_METHOD_18;
                break;
        case CFG_METHOD_20:
                tp->sw_ram_code_ver = NIC_RAMCODE_VERSION_CFG_METHOD_20;
                break;
        case CFG_METHOD_21:
        case CFG_METHOD_22:
                tp->sw_ram_code_ver = NIC_RAMCODE_VERSION_CFG_METHOD_21;
                break;
        case CFG_METHOD_23:
        case CFG_METHOD_27:
                tp->sw_ram_code_ver = NIC_RAMCODE_VERSION_CFG_METHOD_23;
                break;
        case CFG_METHOD_24:
        case CFG_METHOD_25:
                tp->sw_ram_code_ver = NIC_RAMCODE_VERSION_CFG_METHOD_24;
                break;
        case CFG_METHOD_26:
                tp->sw_ram_code_ver = NIC_RAMCODE_VERSION_CFG_METHOD_26;
                break;
        case CFG_METHOD_28:
                tp->sw_ram_code_ver = NIC_RAMCODE_VERSION_CFG_METHOD_28;
                break;
        case CFG_METHOD_29:
        case CFG_METHOD_30:
                tp->sw_ram_code_ver = NIC_RAMCODE_VERSION_CFG_METHOD_29;
                break;
        }

        if (tp->HwIcVerUnknown) {
                tp->NotWrRamCodeToMicroP = TRUE;
                tp->NotWrMcuPatchCode = TRUE;
        }

        rtl8168_get_hw_wol(dev);

        rtl8168_link_option((u8*)&autoneg, (u16*)&speed, (u8*)&duplex);

        tp->autoneg = autoneg;
        tp->speed = speed;
        tp->duplex = duplex;
}

static void
rtl8168_release_board(struct pci_dev *pdev,
                      struct net_device *dev,
                      void __iomem *ioaddr)
{
        struct rtl8168_private *tp = netdev_priv(dev);

        rtl8168_set_bios_setting(dev);
        rtl8168_rar_set(tp, tp->org_mac_addr);
        tp->wol_enabled = WOL_DISABLED;

        if(!tp->DASH)
                rtl8168_phy_power_down(dev);

#ifdef ENABLE_DASH_SUPPORT
        if(tp->DASH)
                FreeAllocatedDashShareMemory(dev);
#endif

        iounmap(ioaddr);
        free_netdev(dev);
}

static int
rtl8168_get_mac_address(struct net_device *dev)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;
        int i;


        if (tp->mcfg == CFG_METHOD_18 ||
            tp->mcfg == CFG_METHOD_19 ||
            tp->mcfg == CFG_METHOD_20 ||
            tp->mcfg == CFG_METHOD_21 ||
            tp->mcfg == CFG_METHOD_22 ||
            tp->mcfg == CFG_METHOD_23 ||
            tp->mcfg == CFG_METHOD_24 ||
            tp->mcfg == CFG_METHOD_25 ||
            tp->mcfg == CFG_METHOD_26 ||
            tp->mcfg == CFG_METHOD_27 ||
            tp->mcfg == CFG_METHOD_28 ||
            tp->mcfg == CFG_METHOD_29 ||
            tp->mcfg == CFG_METHOD_30) {
                u16 mac_addr[3];

                *(u32*)&mac_addr[0] = rtl8168_eri_read(ioaddr, 0xE0, 4, ERIAR_ExGMAC);
                *(u16*)&mac_addr[2] = rtl8168_eri_read(ioaddr, 0xE4, 2, ERIAR_ExGMAC);

                if (is_valid_ether_addr((u8*)mac_addr))
                        rtl8168_rar_set(tp, (uint8_t*)mac_addr);
        } else {
                if (tp->eeprom_type != EEPROM_TYPE_NONE) {
                        u16 mac_addr[3];

                        /* Get MAC address from EEPROM */
                        if (tp->mcfg == CFG_METHOD_16 ||
                            tp->mcfg == CFG_METHOD_17 ||
                            tp->mcfg == CFG_METHOD_18 ||
                            tp->mcfg == CFG_METHOD_19 ||
                            tp->mcfg == CFG_METHOD_20 ||
                            tp->mcfg == CFG_METHOD_21 ||
                            tp->mcfg == CFG_METHOD_22 ||
                            tp->mcfg == CFG_METHOD_23 ||
                            tp->mcfg == CFG_METHOD_24 ||
                            tp->mcfg == CFG_METHOD_25 ||
                            tp->mcfg == CFG_METHOD_26 ||
                            tp->mcfg == CFG_METHOD_27 ||
                            tp->mcfg == CFG_METHOD_28 ||
                            tp->mcfg == CFG_METHOD_29 ||
                            tp->mcfg == CFG_METHOD_30) {
                                mac_addr[0] = rtl_eeprom_read_sc(tp, 1);
                                mac_addr[1] = rtl_eeprom_read_sc(tp, 2);
                                mac_addr[2] = rtl_eeprom_read_sc(tp, 3);
                        } else {
                                mac_addr[0] = rtl_eeprom_read_sc(tp, 7);
                                mac_addr[1] = rtl_eeprom_read_sc(tp, 8);
                                mac_addr[2] = rtl_eeprom_read_sc(tp, 9);
                        }

                        if (is_valid_ether_addr((u8*)mac_addr))
                                rtl8168_rar_set(tp, (uint8_t*)mac_addr);
                }
        }

        for (i = 0; i < MAC_ADDR_LEN; i++) {
                dev->dev_addr[i] = RTL_R8(MAC0 + i);
                tp->org_mac_addr[i] = dev->dev_addr[i]; /* keep the original MAC address */
        }
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,13)
        memcpy(dev->perm_addr, dev->dev_addr, dev->addr_len);
#endif
//  memcpy(dev->dev_addr, dev->dev_addr, dev->addr_len);

        return 0;
}

/**
 * rtl8168_set_mac_address - Change the Ethernet Address of the NIC
 * @dev: network interface device structure
 * @p:   pointer to an address structure
 *
 * Return 0 on success, negative on failure
 **/
static int
rtl8168_set_mac_address(struct net_device *dev,
                        void *p)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        struct sockaddr *addr = p;
        unsigned long flags;

        if (!is_valid_ether_addr(addr->sa_data))
                return -EADDRNOTAVAIL;

        spin_lock_irqsave(&tp->lock, flags);

        memcpy(dev->dev_addr, addr->sa_data, dev->addr_len);

        rtl8168_rar_set(tp, dev->dev_addr);

        spin_unlock_irqrestore(&tp->lock, flags);

        return 0;
}

/******************************************************************************
 * rtl8168_rar_set - Puts an ethernet address into a receive address register.
 *
 * tp - The private data structure for driver
 * addr - Address to put into receive address register
 *****************************************************************************/
void
rtl8168_rar_set(struct rtl8168_private *tp,
                uint8_t *addr)
{
        void __iomem *ioaddr = tp->mmio_addr;
        uint32_t rar_low = 0;
        uint32_t rar_high = 0;

        rar_low = ((uint32_t) addr[0] |
                   ((uint32_t) addr[1] << 8) |
                   ((uint32_t) addr[2] << 16) |
                   ((uint32_t) addr[3] << 24));

        rar_high = ((uint32_t) addr[4] |
                    ((uint32_t) addr[5] << 8));

        RTL_W8(Cfg9346, Cfg9346_Unlock);
        RTL_W32(MAC0, rar_low);
        RTL_W32(MAC4, rar_high);

        switch (tp->mcfg) {
        case CFG_METHOD_14:
        case CFG_METHOD_15:
                RTL_W32(SecMAC0, rar_low);
                RTL_W16(SecMAC4, (uint16_t)rar_high);
                break;
        }

        if (tp->mcfg == CFG_METHOD_17) {
                rtl8168_eri_write(ioaddr, 0xf0, 4, rar_low << 16, ERIAR_ExGMAC);
                rtl8168_eri_write(ioaddr, 0xf4, 4, rar_low >> 16 | rar_high << 16, ERIAR_ExGMAC);
        }

        RTL_W8(Cfg9346, Cfg9346_Lock);
}

#ifdef ETHTOOL_OPS_COMPAT
static int ethtool_get_settings(struct net_device *dev, void *useraddr)
{
        struct ethtool_cmd cmd = { ETHTOOL_GSET };
        int err;

        if (!ethtool_ops->get_settings)
                return -EOPNOTSUPP;

        err = ethtool_ops->get_settings(dev, &cmd);
        if (err < 0)
                return err;

        if (copy_to_user(useraddr, &cmd, sizeof(cmd)))
                return -EFAULT;
        return 0;
}

static int ethtool_set_settings(struct net_device *dev, void *useraddr)
{
        struct ethtool_cmd cmd;

        if (!ethtool_ops->set_settings)
                return -EOPNOTSUPP;

        if (copy_from_user(&cmd, useraddr, sizeof(cmd)))
                return -EFAULT;

        return ethtool_ops->set_settings(dev, &cmd);
}

static int ethtool_get_drvinfo(struct net_device *dev, void *useraddr)
{
        struct ethtool_drvinfo info;
        struct ethtool_ops *ops = ethtool_ops;

        if (!ops->get_drvinfo)
                return -EOPNOTSUPP;

        memset(&info, 0, sizeof(info));
        info.cmd = ETHTOOL_GDRVINFO;
        ops->get_drvinfo(dev, &info);

        if (ops->self_test_count)
                info.testinfo_len = ops->self_test_count(dev);
        if (ops->get_stats_count)
                info.n_stats = ops->get_stats_count(dev);
        if (ops->get_regs_len)
                info.regdump_len = ops->get_regs_len(dev);
        if (ops->get_eeprom_len)
                info.eedump_len = ops->get_eeprom_len(dev);

        if (copy_to_user(useraddr, &info, sizeof(info)))
                return -EFAULT;
        return 0;
}

static int ethtool_get_regs(struct net_device *dev, char *useraddr)
{
        struct ethtool_regs regs;
        struct ethtool_ops *ops = ethtool_ops;
        void *regbuf;
        int reglen, ret;

        if (!ops->get_regs || !ops->get_regs_len)
                return -EOPNOTSUPP;

        if (copy_from_user(&regs, useraddr, sizeof(regs)))
                return -EFAULT;

        reglen = ops->get_regs_len(dev);
        if (regs.len > reglen)
                regs.len = reglen;

        regbuf = kmalloc(reglen, GFP_USER);
        if (!regbuf)
                return -ENOMEM;

        ops->get_regs(dev, &regs, regbuf);

        ret = -EFAULT;
        if (copy_to_user(useraddr, &regs, sizeof(regs)))
                goto out;
        useraddr += offsetof(struct ethtool_regs, data);
        if (copy_to_user(useraddr, regbuf, reglen))
                goto out;
        ret = 0;

out:
        kfree(regbuf);
        return ret;
}

static int ethtool_get_wol(struct net_device *dev, char *useraddr)
{
        struct ethtool_wolinfo wol = { ETHTOOL_GWOL };

        if (!ethtool_ops->get_wol)
                return -EOPNOTSUPP;

        ethtool_ops->get_wol(dev, &wol);

        if (copy_to_user(useraddr, &wol, sizeof(wol)))
                return -EFAULT;
        return 0;
}

static int ethtool_set_wol(struct net_device *dev, char *useraddr)
{
        struct ethtool_wolinfo wol;

        if (!ethtool_ops->set_wol)
                return -EOPNOTSUPP;

        if (copy_from_user(&wol, useraddr, sizeof(wol)))
                return -EFAULT;

        return ethtool_ops->set_wol(dev, &wol);
}

static int ethtool_get_msglevel(struct net_device *dev, char *useraddr)
{
        struct ethtool_value edata = { ETHTOOL_GMSGLVL };

        if (!ethtool_ops->get_msglevel)
                return -EOPNOTSUPP;

        edata.data = ethtool_ops->get_msglevel(dev);

        if (copy_to_user(useraddr, &edata, sizeof(edata)))
                return -EFAULT;
        return 0;
}

static int ethtool_set_msglevel(struct net_device *dev, char *useraddr)
{
        struct ethtool_value edata;

        if (!ethtool_ops->set_msglevel)
                return -EOPNOTSUPP;

        if (copy_from_user(&edata, useraddr, sizeof(edata)))
                return -EFAULT;

        ethtool_ops->set_msglevel(dev, edata.data);
        return 0;
}

static int ethtool_nway_reset(struct net_device *dev)
{
        if (!ethtool_ops->nway_reset)
                return -EOPNOTSUPP;

        return ethtool_ops->nway_reset(dev);
}

static int ethtool_get_link(struct net_device *dev, void *useraddr)
{
        struct ethtool_value edata = { ETHTOOL_GLINK };

        if (!ethtool_ops->get_link)
                return -EOPNOTSUPP;

        edata.data = ethtool_ops->get_link(dev);

        if (copy_to_user(useraddr, &edata, sizeof(edata)))
                return -EFAULT;
        return 0;
}

static int ethtool_get_eeprom(struct net_device *dev, void *useraddr)
{
        struct ethtool_eeprom eeprom;
        struct ethtool_ops *ops = ethtool_ops;
        u8 *data;
        int ret;

        if (!ops->get_eeprom || !ops->get_eeprom_len)
                return -EOPNOTSUPP;

        if (copy_from_user(&eeprom, useraddr, sizeof(eeprom)))
                return -EFAULT;

        /* Check for wrap and zero */
        if (eeprom.offset + eeprom.len <= eeprom.offset)
                return -EINVAL;

        /* Check for exceeding total eeprom len */
        if (eeprom.offset + eeprom.len > ops->get_eeprom_len(dev))
                return -EINVAL;

        data = kmalloc(eeprom.len, GFP_USER);
        if (!data)
                return -ENOMEM;

        ret = -EFAULT;
        if (copy_from_user(data, useraddr + sizeof(eeprom), eeprom.len))
                goto out;

        ret = ops->get_eeprom(dev, &eeprom, data);
        if (ret)
                goto out;

        ret = -EFAULT;
        if (copy_to_user(useraddr, &eeprom, sizeof(eeprom)))
                goto out;
        if (copy_to_user(useraddr + sizeof(eeprom), data, eeprom.len))
                goto out;
        ret = 0;

out:
        kfree(data);
        return ret;
}

static int ethtool_set_eeprom(struct net_device *dev, void *useraddr)
{
        struct ethtool_eeprom eeprom;
        struct ethtool_ops *ops = ethtool_ops;
        u8 *data;
        int ret;

        if (!ops->set_eeprom || !ops->get_eeprom_len)
                return -EOPNOTSUPP;

        if (copy_from_user(&eeprom, useraddr, sizeof(eeprom)))
                return -EFAULT;

        /* Check for wrap and zero */
        if (eeprom.offset + eeprom.len <= eeprom.offset)
                return -EINVAL;

        /* Check for exceeding total eeprom len */
        if (eeprom.offset + eeprom.len > ops->get_eeprom_len(dev))
                return -EINVAL;

        data = kmalloc(eeprom.len, GFP_USER);
        if (!data)
                return -ENOMEM;

        ret = -EFAULT;
        if (copy_from_user(data, useraddr + sizeof(eeprom), eeprom.len))
                goto out;

        ret = ops->set_eeprom(dev, &eeprom, data);
        if (ret)
                goto out;

        if (copy_to_user(useraddr + sizeof(eeprom), data, eeprom.len))
                ret = -EFAULT;

out:
        kfree(data);
        return ret;
}

static int ethtool_get_coalesce(struct net_device *dev, void *useraddr)
{
        struct ethtool_coalesce coalesce = { ETHTOOL_GCOALESCE };

        if (!ethtool_ops->get_coalesce)
                return -EOPNOTSUPP;

        ethtool_ops->get_coalesce(dev, &coalesce);

        if (copy_to_user(useraddr, &coalesce, sizeof(coalesce)))
                return -EFAULT;
        return 0;
}

static int ethtool_set_coalesce(struct net_device *dev, void *useraddr)
{
        struct ethtool_coalesce coalesce;

        if (!ethtool_ops->get_coalesce)
                return -EOPNOTSUPP;

        if (copy_from_user(&coalesce, useraddr, sizeof(coalesce)))
                return -EFAULT;

        return ethtool_ops->set_coalesce(dev, &coalesce);
}

static int ethtool_get_ringparam(struct net_device *dev, void *useraddr)
{
        struct ethtool_ringparam ringparam = { ETHTOOL_GRINGPARAM };

        if (!ethtool_ops->get_ringparam)
                return -EOPNOTSUPP;

        ethtool_ops->get_ringparam(dev, &ringparam);

        if (copy_to_user(useraddr, &ringparam, sizeof(ringparam)))
                return -EFAULT;
        return 0;
}

static int ethtool_set_ringparam(struct net_device *dev, void *useraddr)
{
        struct ethtool_ringparam ringparam;

        if (!ethtool_ops->get_ringparam)
                return -EOPNOTSUPP;

        if (copy_from_user(&ringparam, useraddr, sizeof(ringparam)))
                return -EFAULT;

        return ethtool_ops->set_ringparam(dev, &ringparam);
}

static int ethtool_get_pauseparam(struct net_device *dev, void *useraddr)
{
        struct ethtool_pauseparam pauseparam = { ETHTOOL_GPAUSEPARAM };

        if (!ethtool_ops->get_pauseparam)
                return -EOPNOTSUPP;

        ethtool_ops->get_pauseparam(dev, &pauseparam);

        if (copy_to_user(useraddr, &pauseparam, sizeof(pauseparam)))
                return -EFAULT;
        return 0;
}

static int ethtool_set_pauseparam(struct net_device *dev, void *useraddr)
{
        struct ethtool_pauseparam pauseparam;

        if (!ethtool_ops->get_pauseparam)
                return -EOPNOTSUPP;

        if (copy_from_user(&pauseparam, useraddr, sizeof(pauseparam)))
                return -EFAULT;

        return ethtool_ops->set_pauseparam(dev, &pauseparam);
}

static int ethtool_get_rx_csum(struct net_device *dev, char *useraddr)
{
        struct ethtool_value edata = { ETHTOOL_GRXCSUM };

        if (!ethtool_ops->get_rx_csum)
                return -EOPNOTSUPP;

        edata.data = ethtool_ops->get_rx_csum(dev);

        if (copy_to_user(useraddr, &edata, sizeof(edata)))
                return -EFAULT;
        return 0;
}

static int ethtool_set_rx_csum(struct net_device *dev, char *useraddr)
{
        struct ethtool_value edata;

        if (!ethtool_ops->set_rx_csum)
                return -EOPNOTSUPP;

        if (copy_from_user(&edata, useraddr, sizeof(edata)))
                return -EFAULT;

        ethtool_ops->set_rx_csum(dev, edata.data);
        return 0;
}

static int ethtool_get_tx_csum(struct net_device *dev, char *useraddr)
{
        struct ethtool_value edata = { ETHTOOL_GTXCSUM };

        if (!ethtool_ops->get_tx_csum)
                return -EOPNOTSUPP;

        edata.data = ethtool_ops->get_tx_csum(dev);

        if (copy_to_user(useraddr, &edata, sizeof(edata)))
                return -EFAULT;
        return 0;
}

static int ethtool_set_tx_csum(struct net_device *dev, char *useraddr)
{
        struct ethtool_value edata;

        if (!ethtool_ops->set_tx_csum)
                return -EOPNOTSUPP;

        if (copy_from_user(&edata, useraddr, sizeof(edata)))
                return -EFAULT;

        return ethtool_ops->set_tx_csum(dev, edata.data);
}

static int ethtool_get_sg(struct net_device *dev, char *useraddr)
{
        struct ethtool_value edata = { ETHTOOL_GSG };

        if (!ethtool_ops->get_sg)
                return -EOPNOTSUPP;

        edata.data = ethtool_ops->get_sg(dev);

        if (copy_to_user(useraddr, &edata, sizeof(edata)))
                return -EFAULT;
        return 0;
}

static int ethtool_set_sg(struct net_device *dev, char *useraddr)
{
        struct ethtool_value edata;

        if (!ethtool_ops->set_sg)
                return -EOPNOTSUPP;

        if (copy_from_user(&edata, useraddr, sizeof(edata)))
                return -EFAULT;

        return ethtool_ops->set_sg(dev, edata.data);
}

static int ethtool_get_tso(struct net_device *dev, char *useraddr)
{
        struct ethtool_value edata = { ETHTOOL_GTSO };

        if (!ethtool_ops->get_tso)
                return -EOPNOTSUPP;

        edata.data = ethtool_ops->get_tso(dev);

        if (copy_to_user(useraddr, &edata, sizeof(edata)))
                return -EFAULT;
        return 0;
}

static int ethtool_set_tso(struct net_device *dev, char *useraddr)
{
        struct ethtool_value edata;

        if (!ethtool_ops->set_tso)
                return -EOPNOTSUPP;

        if (copy_from_user(&edata, useraddr, sizeof(edata)))
                return -EFAULT;

        return ethtool_ops->set_tso(dev, edata.data);
}

static int ethtool_self_test(struct net_device *dev, char *useraddr)
{
        struct ethtool_test test;
        struct ethtool_ops *ops = ethtool_ops;
        u64 *data;
        int ret;

        if (!ops->self_test || !ops->self_test_count)
                return -EOPNOTSUPP;

        if (copy_from_user(&test, useraddr, sizeof(test)))
                return -EFAULT;

        test.len = ops->self_test_count(dev);
        data = kmalloc(test.len * sizeof(u64), GFP_USER);
        if (!data)
                return -ENOMEM;

        ops->self_test(dev, &test, data);

        ret = -EFAULT;
        if (copy_to_user(useraddr, &test, sizeof(test)))
                goto out;
        useraddr += sizeof(test);
        if (copy_to_user(useraddr, data, test.len * sizeof(u64)))
                goto out;
        ret = 0;

out:
        kfree(data);
        return ret;
}

static int ethtool_get_strings(struct net_device *dev, void *useraddr)
{
        struct ethtool_gstrings gstrings;
        struct ethtool_ops *ops = ethtool_ops;
        u8 *data;
        int ret;

        if (!ops->get_strings)
                return -EOPNOTSUPP;

        if (copy_from_user(&gstrings, useraddr, sizeof(gstrings)))
                return -EFAULT;

        switch (gstrings.string_set) {
        case ETH_SS_TEST:
                if (!ops->self_test_count)
                        return -EOPNOTSUPP;
                gstrings.len = ops->self_test_count(dev);
                break;
        case ETH_SS_STATS:
                if (!ops->get_stats_count)
                        return -EOPNOTSUPP;
                gstrings.len = ops->get_stats_count(dev);
                break;
        default:
                return -EINVAL;
        }

        data = kmalloc(gstrings.len * ETH_GSTRING_LEN, GFP_USER);
        if (!data)
                return -ENOMEM;

        ops->get_strings(dev, gstrings.string_set, data);

        ret = -EFAULT;
        if (copy_to_user(useraddr, &gstrings, sizeof(gstrings)))
                goto out;
        useraddr += sizeof(gstrings);
        if (copy_to_user(useraddr, data, gstrings.len * ETH_GSTRING_LEN))
                goto out;
        ret = 0;

out:
        kfree(data);
        return ret;
}

static int ethtool_phys_id(struct net_device *dev, void *useraddr)
{
        struct ethtool_value id;

        if (!ethtool_ops->phys_id)
                return -EOPNOTSUPP;

        if (copy_from_user(&id, useraddr, sizeof(id)))
                return -EFAULT;

        return ethtool_ops->phys_id(dev, id.data);
}

static int ethtool_get_stats(struct net_device *dev, void *useraddr)
{
        struct ethtool_stats stats;
        struct ethtool_ops *ops = ethtool_ops;
        u64 *data;
        int ret;

        if (!ops->get_ethtool_stats || !ops->get_stats_count)
                return -EOPNOTSUPP;

        if (copy_from_user(&stats, useraddr, sizeof(stats)))
                return -EFAULT;

        stats.n_stats = ops->get_stats_count(dev);
        data = kmalloc(stats.n_stats * sizeof(u64), GFP_USER);
        if (!data)
                return -ENOMEM;

        ops->get_ethtool_stats(dev, &stats, data);

        ret = -EFAULT;
        if (copy_to_user(useraddr, &stats, sizeof(stats)))
                goto out;
        useraddr += sizeof(stats);
        if (copy_to_user(useraddr, data, stats.n_stats * sizeof(u64)))
                goto out;
        ret = 0;

out:
        kfree(data);
        return ret;
}

static int ethtool_ioctl(struct ifreq *ifr)
{
        struct net_device *dev = __dev_get_by_name(ifr->ifr_name);
        void *useraddr = (void *) ifr->ifr_data;
        u32 ethcmd;

        /*
         * XXX: This can be pushed down into the ethtool_* handlers that
         * need it.  Keep existing behaviour for the moment.
         */
        if (!capable(CAP_NET_ADMIN))
                return -EPERM;

        if (!dev || !netif_device_present(dev))
                return -ENODEV;

        if (copy_from_user(&ethcmd, useraddr, sizeof (ethcmd)))
                return -EFAULT;

        switch (ethcmd) {
        case ETHTOOL_GSET:
                return ethtool_get_settings(dev, useraddr);
        case ETHTOOL_SSET:
                return ethtool_set_settings(dev, useraddr);
        case ETHTOOL_GDRVINFO:
                return ethtool_get_drvinfo(dev, useraddr);
        case ETHTOOL_GREGS:
                return ethtool_get_regs(dev, useraddr);
        case ETHTOOL_GWOL:
                return ethtool_get_wol(dev, useraddr);
        case ETHTOOL_SWOL:
                return ethtool_set_wol(dev, useraddr);
        case ETHTOOL_GMSGLVL:
                return ethtool_get_msglevel(dev, useraddr);
        case ETHTOOL_SMSGLVL:
                return ethtool_set_msglevel(dev, useraddr);
        case ETHTOOL_NWAY_RST:
                return ethtool_nway_reset(dev);
        case ETHTOOL_GLINK:
                return ethtool_get_link(dev, useraddr);
        case ETHTOOL_GEEPROM:
                return ethtool_get_eeprom(dev, useraddr);
        case ETHTOOL_SEEPROM:
                return ethtool_set_eeprom(dev, useraddr);
        case ETHTOOL_GCOALESCE:
                return ethtool_get_coalesce(dev, useraddr);
        case ETHTOOL_SCOALESCE:
                return ethtool_set_coalesce(dev, useraddr);
        case ETHTOOL_GRINGPARAM:
                return ethtool_get_ringparam(dev, useraddr);
        case ETHTOOL_SRINGPARAM:
                return ethtool_set_ringparam(dev, useraddr);
        case ETHTOOL_GPAUSEPARAM:
                return ethtool_get_pauseparam(dev, useraddr);
        case ETHTOOL_SPAUSEPARAM:
                return ethtool_set_pauseparam(dev, useraddr);
        case ETHTOOL_GRXCSUM:
                return ethtool_get_rx_csum(dev, useraddr);
        case ETHTOOL_SRXCSUM:
                return ethtool_set_rx_csum(dev, useraddr);
        case ETHTOOL_GTXCSUM:
                return ethtool_get_tx_csum(dev, useraddr);
        case ETHTOOL_STXCSUM:
                return ethtool_set_tx_csum(dev, useraddr);
        case ETHTOOL_GSG:
                return ethtool_get_sg(dev, useraddr);
        case ETHTOOL_SSG:
                return ethtool_set_sg(dev, useraddr);
        case ETHTOOL_GTSO:
                return ethtool_get_tso(dev, useraddr);
        case ETHTOOL_STSO:
                return ethtool_set_tso(dev, useraddr);
        case ETHTOOL_TEST:
                return ethtool_self_test(dev, useraddr);
        case ETHTOOL_GSTRINGS:
                return ethtool_get_strings(dev, useraddr);
        case ETHTOOL_PHYS_ID:
                return ethtool_phys_id(dev, useraddr);
        case ETHTOOL_GSTATS:
                return ethtool_get_stats(dev, useraddr);
        default:
                return -EOPNOTSUPP;
        }

        return -EOPNOTSUPP;
}
#endif //ETHTOOL_OPS_COMPAT

static int
rtl8168_do_ioctl(struct net_device *dev,
                 struct ifreq *ifr,
                 int cmd)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        struct mii_ioctl_data *data = if_mii(ifr);
        int ret;
        unsigned long flags;

        ret = 0;
        switch (cmd) {
        case SIOCGMIIPHY:
                data->phy_id = 32; /* Internal PHY */
                break;

        case SIOCGMIIREG:
                spin_lock_irqsave(&tp->phy_lock, flags);
                mdio_write(tp, 0x1F, 0x0000);
                data->val_out = mdio_read(tp, data->reg_num);
                spin_unlock_irqrestore(&tp->phy_lock, flags);
                break;

        case SIOCSMIIREG:
                if (!capable(CAP_NET_ADMIN))
                        return -EPERM;
                spin_lock_irqsave(&tp->phy_lock, flags);
                mdio_write(tp, 0x1F, 0x0000);
                mdio_write(tp, data->reg_num, data->val_in);
                spin_unlock_irqrestore(&tp->phy_lock, flags);
                break;

#ifdef ETHTOOL_OPS_COMPAT
        case SIOCETHTOOL:
                ret = ethtool_ioctl(ifr);
                break;
#endif
        case SIOCDEVPRIVATE_RTLASF:
                if (!netif_running(dev)) {
                        ret = -ENODEV;
                        break;
                }

                ret = rtl8168_asf_ioctl(dev, ifr);
                break;

#ifdef ENABLE_DASH_SUPPORT
        case SIOCDEVPRIVATE_RTLDASH:
                if (!netif_running(dev)) {
                        ret = -ENODEV;
                        break;
                }

                ret = rtl8168_dash_ioctl(dev, ifr);
                break;
#endif

#ifdef ENABLE_REALWOW_SUPPORT
        case SIOCDEVPRIVATE_RTLREALWOW:
                if (!netif_running(dev)) {
                        ret = -ENODEV;
                        break;
                }

                ret = rtl8168_realwow_ioctl(dev, ifr);
                break;
#endif

        case SIOCRTLTOOL:
                ret = rtltool_ioctl(tp, ifr);
                break;

        default:
                ret = -EOPNOTSUPP;
                break;
        }

        return ret;
}

static void
rtl8168_phy_power_up(struct net_device *dev)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;
        u32 csi_tmp;
        unsigned long flags;

        spin_lock_irqsave(&tp->phy_lock, flags);
        mdio_write(tp, 0x1F, 0x0000);
        switch (tp->mcfg) {
        case CFG_METHOD_1:
        case CFG_METHOD_2:
        case CFG_METHOD_3:
        case CFG_METHOD_4:
        case CFG_METHOD_5:
        case CFG_METHOD_6:
        case CFG_METHOD_7:
        case CFG_METHOD_8:
        case CFG_METHOD_9:
        case CFG_METHOD_10:
        case CFG_METHOD_11:
        case CFG_METHOD_12:
        case CFG_METHOD_13:
                mdio_write(tp, 0x0E, 0x0000);
                break;
        case CFG_METHOD_21:
        case CFG_METHOD_22:
        case CFG_METHOD_23:
        case CFG_METHOD_24:
                csi_tmp = rtl8168_eri_read(ioaddr, 0x1AB, 1, ERIAR_ExGMAC);
                csi_tmp |= ( BIT_2 | BIT_3 | BIT_4 | BIT_5 | BIT_6 | BIT_7 );
                rtl8168_eri_write(ioaddr, 0x1AB, 1, csi_tmp, ERIAR_ExGMAC);
                break;
        default:
                break;
        }
        mdio_write(tp, MII_BMCR, BMCR_ANENABLE);

        //wait mdc/mdio ready
        switch (tp->mcfg) {
        case CFG_METHOD_23:
        case CFG_METHOD_27:
        case CFG_METHOD_28:
                mdelay(10);
                break;
        }

        //wait ups resume (phy state 3)
        switch (tp->mcfg) {
        case CFG_METHOD_29:
        case CFG_METHOD_30:
                rtl8168_wait_phy_ups_resume(dev, 3);
                break;
        };
        spin_unlock_irqrestore(&tp->phy_lock, flags);
}

static void
rtl8168_phy_power_down(struct net_device *dev)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;
        u32 csi_tmp;
        unsigned long flags;

        spin_lock_irqsave(&tp->phy_lock, flags);
        mdio_write(tp, 0x1F, 0x0000);
        switch (tp->mcfg) {
        case CFG_METHOD_1:
        case CFG_METHOD_2:
        case CFG_METHOD_3:
        case CFG_METHOD_4:
        case CFG_METHOD_5:
        case CFG_METHOD_6:
        case CFG_METHOD_7:
        case CFG_METHOD_8:
        case CFG_METHOD_9:
        case CFG_METHOD_10:
        case CFG_METHOD_11:
        case CFG_METHOD_12:
        case CFG_METHOD_13:
                mdio_write(tp, 0x0E, 0x0200);
                mdio_write(tp, MII_BMCR, BMCR_PDOWN);
                break;
        case CFG_METHOD_14:
        case CFG_METHOD_15:
        case CFG_METHOD_29:
        case CFG_METHOD_30:
                mdio_write(tp, MII_BMCR, BMCR_ANENABLE | BMCR_PDOWN);
                break;
        case CFG_METHOD_21:
        case CFG_METHOD_22:
                mdio_write(tp, MII_BMCR, BMCR_ANENABLE | BMCR_PDOWN);

                csi_tmp = rtl8168_eri_read(ioaddr, 0x1AB, 1, ERIAR_ExGMAC);
                csi_tmp &= ~( BIT_2 | BIT_3 | BIT_4 | BIT_5 | BIT_6 | BIT_7 );
                rtl8168_eri_write(ioaddr, 0x1AB, 1, csi_tmp, ERIAR_ExGMAC);

                RTL_W8(0xD0, RTL_R8(0xD0) & ~BIT_6);
                break;
        case CFG_METHOD_23:
        case CFG_METHOD_24:
                mdio_write(tp, MII_BMCR, BMCR_ANENABLE | BMCR_PDOWN);

                csi_tmp = rtl8168_eri_read(ioaddr, 0x1AB, 1, ERIAR_ExGMAC);
                csi_tmp &= ~( BIT_2 | BIT_3 | BIT_4 | BIT_5 | BIT_6 | BIT_7 );
                rtl8168_eri_write(ioaddr, 0x1AB, 1, csi_tmp, ERIAR_ExGMAC);
                break;
        default:
                mdio_write(tp, MII_BMCR, BMCR_PDOWN);
                break;
        }
        spin_unlock_irqrestore(&tp->phy_lock, flags);
}

static int __devinit
rtl8168_init_board(struct pci_dev *pdev,
                   struct net_device **dev_out,
                   void __iomem **ioaddr_out)
{
        void __iomem *ioaddr;
        struct net_device *dev;
        struct rtl8168_private *tp;
        int rc = -ENOMEM, i, pm_cap;

        assert(ioaddr_out != NULL);

        /* dev zeroed in alloc_etherdev */
        dev = alloc_etherdev(sizeof (*tp));
        if (dev == NULL) {
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
                if (netif_msg_drv(&debug))
                        dev_err(&pdev->dev, "unable to alloc new ethernet\n");
#endif //LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
                goto err_out;
        }

        SET_MODULE_OWNER(dev);
        SET_NETDEV_DEV(dev, &pdev->dev);
        tp = netdev_priv(dev);
        tp->dev = dev;
        tp->msg_enable = netif_msg_init(debug.msg_enable, R8168_MSG_DEFAULT);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26)
        if (!aspm || tp->mcfg == CFG_METHOD_9)
                pci_disable_link_state(pdev, PCIE_LINK_STATE_L0S | PCIE_LINK_STATE_L1 |
                                       PCIE_LINK_STATE_CLKPM);
#endif

        rc = pci_set_mwi(pdev);
        if (rc < 0)
		goto err_out_free_dev;

        /* save power state before pci_enable_device overwrites it */
        pm_cap = pci_find_capability(pdev, PCI_CAP_ID_PM);
        if (pm_cap) {
                u16 pwr_command;

                pci_read_config_word(pdev, pm_cap + PCI_PM_CTRL, &pwr_command);
        } else {
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
                if (netif_msg_probe(tp)) {
                        dev_err(&pdev->dev, "PowerManagement capability not found.\n");
                }
#else
                printk("PowerManagement capability not found.\n");
#endif //LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)

        }

        /* make sure PCI base addr 1 is MMIO */
        if (!(pci_resource_flags(pdev, 2) & IORESOURCE_MEM)) {
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
                if (netif_msg_probe(tp))
                        dev_err(&pdev->dev, "region #1 not an MMIO resource, aborting\n");
#endif //LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
                rc = -ENODEV;
                goto err_out_mwi;
        }
        /* check for weird/broken PCI region reporting */
        if (pci_resource_len(pdev, 2) < R8168_REGS_SIZE) {
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
                if (netif_msg_probe(tp))
                        dev_err(&pdev->dev, "Invalid PCI region size(s), aborting\n");
#endif //LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
                rc = -ENODEV;
                goto err_out_mwi;
        }

        if ((sizeof(dma_addr_t) > 4) &&
            !pci_set_dma_mask(pdev, DMA_BIT_MASK(64))) {
                dev->features |= NETIF_F_HIGHDMA;
        } else {
                rc = pci_set_dma_mask(pdev, DMA_BIT_MASK(32));
                if (rc < 0) {
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
                        if (netif_msg_probe(tp))
                                dev_err(&pdev->dev, "DMA configuration failed.\n");
#endif //LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
			goto err_out_mwi;
                }
        }

        /* ioremap MMIO region */
        ioaddr = ioremap(pci_resource_start(pdev, 2), R8168_REGS_SIZE);
        if (ioaddr == NULL) {
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
                if (netif_msg_probe(tp))
                        dev_err(&pdev->dev, "cannot remap MMIO, aborting\n");
#endif //LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
                rc = -EIO;
		goto err_out_mwi;
        }

        /* Identify chip attached to board */
        rtl8168_get_mac_version(tp, ioaddr);

        rtl8168_print_mac_version(tp);

        for (i = ARRAY_SIZE(rtl_chip_info) - 1; i >= 0; i--) {
                if (tp->mcfg == rtl_chip_info[i].mcfg)
                        break;
        }

        if (i < 0) {
                /* Unknown chip: assume array element #0, original RTL-8168 */
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
                if (netif_msg_probe(tp))
                        dev_printk(KERN_DEBUG, &pdev->dev, "unknown chip version, assuming %s\n", rtl_chip_info[0].name);
#else
                printk("Realtek unknown chip version, assuming %s\n", rtl_chip_info[0].name);
#endif //LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
                i++;
        }

        tp->chipset = i;

        *ioaddr_out = ioaddr;
        *dev_out = dev;
out:
        return rc;

err_out_mwi:
        pci_clear_mwi(pdev);

err_out_free_dev:
        free_netdev(dev);
err_out:
        *ioaddr_out = NULL;
        *dev_out = NULL;
        goto out;
}

#define PCI_DEVICE_SERIAL_NUMBER (0x0164)

static void
rtl8168_esd_timer(unsigned long __opaque)
{
        struct net_device *dev = (struct net_device *)__opaque;
        struct rtl8168_private *tp = netdev_priv(dev);
        struct pci_dev *pdev = tp->pci_dev;
        struct timer_list *timer = &tp->esd_timer;
        unsigned long timeout = RTL8168_ESD_TIMEOUT;
        unsigned long flags;
        u8 cmd;
        u16 io_base_l;
        u16 mem_base_l;
        u16 mem_base_h;
        u8 ilr;
        u16 resv_0x1c_h;
        u16 resv_0x1c_l;
        u16 resv_0x20_l;
        u16 resv_0x20_h;
        u16 resv_0x24_l;
        u16 resv_0x24_h;
        u16 resv_0x2c_h;
        u16 resv_0x2c_l;
        u32 pci_sn_l;
        u32 pci_sn_h;

        spin_lock_irqsave(&tp->lock, flags);

        tp->esd_flag = 0;

        pci_read_config_byte(pdev, PCI_COMMAND, &cmd);
        if (cmd != tp->pci_cfg_space.cmd) {
                pci_write_config_byte(pdev, PCI_COMMAND, tp->pci_cfg_space.cmd);
                tp->esd_flag |= BIT_0;
        }

        pci_read_config_word(pdev, PCI_BASE_ADDRESS_0, &io_base_l);
        if (io_base_l != tp->pci_cfg_space.io_base_l) {
                pci_write_config_word(pdev, PCI_BASE_ADDRESS_0, tp->pci_cfg_space.io_base_l);
                tp->esd_flag |= BIT_1;
        }

        pci_read_config_word(pdev, PCI_BASE_ADDRESS_2, &mem_base_l);
        if (mem_base_l != tp->pci_cfg_space.mem_base_l) {
                pci_write_config_word(pdev, PCI_BASE_ADDRESS_2, tp->pci_cfg_space.mem_base_l);
                tp->esd_flag |= BIT_2;
        }

        pci_read_config_word(pdev, PCI_BASE_ADDRESS_2 + 2, &mem_base_h);
        if (mem_base_h!= tp->pci_cfg_space.mem_base_h) {
                pci_write_config_word(pdev, PCI_BASE_ADDRESS_2 + 2, tp->pci_cfg_space.mem_base_h);
                tp->esd_flag |= BIT_3;
        }

        pci_read_config_word(pdev, PCI_BASE_ADDRESS_3, &resv_0x1c_l);
        if (resv_0x1c_l != tp->pci_cfg_space.resv_0x1c_l) {
                pci_write_config_word(pdev, PCI_BASE_ADDRESS_3, tp->pci_cfg_space.resv_0x1c_l);
                tp->esd_flag |= BIT_4;
        }

        pci_read_config_word(pdev, PCI_BASE_ADDRESS_3 + 2, &resv_0x1c_h);
        if (resv_0x1c_h != tp->pci_cfg_space.resv_0x1c_h) {
                pci_write_config_word(pdev, PCI_BASE_ADDRESS_3 + 2, tp->pci_cfg_space.resv_0x1c_h);
                tp->esd_flag |= BIT_5;
        }

        pci_read_config_word(pdev, PCI_BASE_ADDRESS_4, &resv_0x20_l);
        if (resv_0x20_l != tp->pci_cfg_space.resv_0x20_l) {
                pci_write_config_word(pdev, PCI_BASE_ADDRESS_4, tp->pci_cfg_space.resv_0x20_l);
                tp->esd_flag |= BIT_6;
        }

        pci_read_config_word(pdev, PCI_BASE_ADDRESS_4 + 2, &resv_0x20_h);
        if (resv_0x20_h != tp->pci_cfg_space.resv_0x20_h) {
                pci_write_config_word(pdev, PCI_BASE_ADDRESS_4 + 2, tp->pci_cfg_space.resv_0x20_h);
                tp->esd_flag |= BIT_7;
        }

        pci_read_config_word(pdev, PCI_BASE_ADDRESS_5, &resv_0x24_l);
        if (resv_0x24_l != tp->pci_cfg_space.resv_0x24_l) {
                pci_write_config_word(pdev, PCI_BASE_ADDRESS_5, tp->pci_cfg_space.resv_0x24_l);
                tp->esd_flag |= BIT_8;
        }

        pci_read_config_word(pdev, PCI_BASE_ADDRESS_5 + 2, &resv_0x24_h);
        if (resv_0x24_h != tp->pci_cfg_space.resv_0x24_h) {
                pci_write_config_word(pdev, PCI_BASE_ADDRESS_5 + 2, tp->pci_cfg_space.resv_0x24_h);
                tp->esd_flag |= BIT_9;
        }

        pci_read_config_byte(pdev, PCI_INTERRUPT_LINE, &ilr);
        if (ilr != tp->pci_cfg_space.ilr) {
                pci_write_config_byte(pdev, PCI_INTERRUPT_LINE, tp->pci_cfg_space.ilr);
                tp->esd_flag |= BIT_10;
        }

        pci_read_config_word(pdev, PCI_SUBSYSTEM_VENDOR_ID, &resv_0x2c_l);
        if (resv_0x2c_l != tp->pci_cfg_space.resv_0x2c_l) {
                pci_write_config_word(pdev, PCI_SUBSYSTEM_VENDOR_ID, tp->pci_cfg_space.resv_0x2c_l);
                tp->esd_flag |= BIT_11;
        }

        pci_read_config_word(pdev, PCI_SUBSYSTEM_VENDOR_ID + 2, &resv_0x2c_h);
        if (resv_0x2c_h != tp->pci_cfg_space.resv_0x2c_h) {
                pci_write_config_word(pdev, PCI_SUBSYSTEM_VENDOR_ID + 2, tp->pci_cfg_space.resv_0x2c_h);
                tp->esd_flag |= BIT_12;
        }

        pci_sn_l = rtl8168_csi_read(tp, PCI_DEVICE_SERIAL_NUMBER);
        if (pci_sn_l != tp->pci_cfg_space.pci_sn_l) {
                rtl8168_csi_write(tp, PCI_DEVICE_SERIAL_NUMBER, tp->pci_cfg_space.pci_sn_l);
                tp->esd_flag |= BIT_13;
        }

        pci_sn_h = rtl8168_csi_read(tp, PCI_DEVICE_SERIAL_NUMBER + 4);
        if (pci_sn_h != tp->pci_cfg_space.pci_sn_h) {
                rtl8168_csi_write(tp, PCI_DEVICE_SERIAL_NUMBER + 4, tp->pci_cfg_space.pci_sn_h);
                tp->esd_flag |= BIT_14;
        }

        if (tp->esd_flag != 0) {
                netif_stop_queue(dev);
                netif_carrier_off(dev);
                rtl8168_hw_reset(dev);
                rtl8168_tx_clear(tp);
                rtl8168_rx_clear(tp);
                rtl8168_init_ring(dev);
                rtl8168_hw_init(dev);
                rtl8168_powerup_pll(dev);
                rtl8168_hw_ephy_config(dev);
                rtl8168_hw_phy_config(dev);
                rtl8168_hw_config(dev);
                rtl8168_set_speed(dev, tp->autoneg, tp->speed, tp->duplex);
                tp->esd_flag = 0;
        }
        spin_unlock_irqrestore(&tp->lock, flags);

        mod_timer(timer, jiffies + timeout);
}

static void
rtl8168_link_timer(unsigned long __opaque)
{
        struct net_device *dev = (struct net_device *)__opaque;
        struct rtl8168_private *tp = netdev_priv(dev);
        struct timer_list *timer = &tp->link_timer;
        unsigned long flags;

        spin_lock_irqsave(&tp->lock, flags);
        rtl8168_check_link_status(dev);
        spin_unlock_irqrestore(&tp->lock, flags);

        mod_timer(timer, jiffies + RTL8168_LINK_TIMEOUT);
}

/* Cfg9346_Unlock assumed. */
static unsigned rtl8168_try_msi(struct pci_dev *pdev, struct rtl8168_private *tp)
{
        unsigned msi = 0;

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,13)
        if (pci_enable_msi(pdev))
                dev_info(&pdev->dev, "no MSI. Back to INTx.\n");
        else
                msi |= RTL_FEATURE_MSI;
#endif

        switch (tp->mcfg) {
        case CFG_METHOD_1:
        case CFG_METHOD_2:
        case CFG_METHOD_3:
        case CFG_METHOD_4:
        case CFG_METHOD_5:
        case CFG_METHOD_6:
        case CFG_METHOD_7:
        case CFG_METHOD_8:
                msi &= ~RTL_FEATURE_MSI;
                break;
        }

        return msi;
}

static void rtl8168_disable_msi(struct pci_dev *pdev, struct rtl8168_private *tp)
{
        if (tp->features & RTL_FEATURE_MSI) {
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,13)
                pci_disable_msi(pdev);
#endif
                tp->features &= ~RTL_FEATURE_MSI;
        }
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,29)
static const struct net_device_ops rtl8168_netdev_ops = {
        .ndo_open       = rtl8168_open,
        .ndo_stop       = rtl8168_close,
        .ndo_get_stats      = rtl8168_get_stats,
        .ndo_start_xmit     = rtl8168_start_xmit,
        .ndo_tx_timeout     = rtl8168_tx_timeout,
        .ndo_change_mtu     = rtl8168_change_mtu,
        .ndo_set_mac_address    = rtl8168_set_mac_address,
        .ndo_do_ioctl       = rtl8168_do_ioctl,
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,1,0)
        .ndo_set_multicast_list = rtl8168_set_rx_mode,
#else
        .ndo_set_rx_mode    = rtl8168_set_rx_mode,
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,0,0)
#ifdef CONFIG_R8168_VLAN
        .ndo_vlan_rx_register   = rtl8168_vlan_rx_register,
#endif
#else
        .ndo_fix_features   = rtl8168_fix_features,
        .ndo_set_features   = rtl8168_set_features,
#endif
#ifdef CONFIG_NET_POLL_CONTROLLER
        .ndo_poll_controller    = rtl8168_netpoll,
#endif
};
#endif

int __devinit
rtl8168_init_one(struct pci_dev *pdev, const struct pci_device_id *ent)
{
        struct net_device *dev = NULL;
        struct rtl8168_private *tp;
        void __iomem *ioaddr = NULL;
        static int board_idx = -1;

        int rc;

        assert(pdev != NULL);
        assert(ent != NULL);

        board_idx++;

        if (netif_msg_drv(&debug))
                printk(KERN_INFO "%s Gigabit Ethernet driver %s loaded\n",
                       MODULENAME, RTL8168_VERSION);

        rc = rtl8168_init_board(pdev, &dev, &ioaddr);
        if (rc)
                return rc;

        tp = netdev_priv(dev);
        assert(ioaddr != NULL);

        tp->mmio_addr = ioaddr;
        tp->set_speed = rtl8168_set_speed_xmii;
        tp->get_settings = rtl8168_gset_xmii;
        tp->phy_reset_enable = rtl8168_xmii_reset_enable;
        tp->phy_reset_pending = rtl8168_xmii_reset_pending;
        tp->link_ok = rtl8168_xmii_link_ok;

        tp->features |= rtl8168_try_msi(pdev, tp);

        RTL_NET_DEVICE_OPS(rtl8168_netdev_ops);

        SET_ETHTOOL_OPS(dev, &rtl8168_ethtool_ops);

        dev->watchdog_timeo = RTL8168_TX_TIMEOUT;
        dev->irq = pdev->irq;
        dev->base_addr = (unsigned long) ioaddr;

#ifdef CONFIG_R8168_NAPI
        RTL_NAPI_CONFIG(dev, tp, rtl8168_poll, R8168_NAPI_WEIGHT);
#endif

#ifdef CONFIG_R8168_VLAN
        if (tp->mcfg != CFG_METHOD_DEFAULT) {
                dev->features |= NETIF_F_HW_VLAN_TX | NETIF_F_HW_VLAN_RX;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22)
                dev->vlan_rx_kill_vid = rtl8168_vlan_rx_kill_vid;
#endif //LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22)
        }
#endif

        tp->cp_cmd |= RTL_R16(CPlusCmd);
        if (tp->mcfg != CFG_METHOD_DEFAULT) {
                dev->features |= NETIF_F_IP_CSUM;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,0,0)
                tp->cp_cmd |= RxChkSum;
#else
                dev->features |= NETIF_F_RXCSUM | NETIF_F_SG;
                dev->hw_features = NETIF_F_SG | NETIF_F_IP_CSUM | NETIF_F_TSO |
                                   NETIF_F_RXCSUM | NETIF_F_HW_VLAN_TX | NETIF_F_HW_VLAN_RX;
                dev->vlan_features = NETIF_F_SG | NETIF_F_IP_CSUM | NETIF_F_TSO |
                                     NETIF_F_HIGHDMA;
#endif
        }

        tp->pci_dev = pdev;

        spin_lock_init(&tp->lock);

        spin_lock_init(&tp->phy_lock);

        rtl8168_init_software_variable(dev);

#ifdef ENABLE_DASH_SUPPORT
        if(tp->DASH)
                AllocateDashShareMemory(dev);
#endif

        rtl8168_exit_oob(dev);

        rtl8168_hw_init(dev);

        rtl8168_hw_reset(dev);

        /* Get production from EEPROM */
        if (((tp->mcfg == CFG_METHOD_21 || tp->mcfg == CFG_METHOD_22 ||
              tp->mcfg == CFG_METHOD_25 || tp->mcfg == CFG_METHOD_29 ||
              tp->mcfg == CFG_METHOD_30) && (mac_ocp_read(tp, 0xDC00) & BIT_3)) ||
            ((tp->mcfg == CFG_METHOD_26) && (mac_ocp_read(tp, 0xDC00) & BIT_4)))
                tp->eeprom_type = EEPROM_TYPE_NONE;
        else
                rtl_eeprom_type(tp);

        if (tp->eeprom_type == EEPROM_TYPE_93C46 || tp->eeprom_type == EEPROM_TYPE_93C56)
                rtl_set_eeprom_sel_low(ioaddr);

        rtl8168_get_mac_address(dev);

        pci_set_drvdata(pdev, dev);

        rc = register_netdev(dev);
        if (rc) {
#ifdef  CONFIG_R8168_NAPI
                RTL_NAPI_DEL(tp);
#endif
                rtl8168_disable_msi(pdev, tp);
                rtl8168_release_board(pdev, dev, ioaddr);
                return rc;
        }

        printk(KERN_INFO "%s: This product is covered by one or more of the following patents: US6,570,884, US6,115,776, and US6,327,625.\n", MODULENAME);

        netif_carrier_off(dev);

        printk("%s", GPL_CLAIM);

        return 0;
}

void __devexit rtl8168_remove_one(struct pci_dev *pdev)
{
        struct net_device *dev = pci_get_drvdata(pdev);
        struct rtl8168_private *tp = netdev_priv(dev);

        assert(dev != NULL);
        assert(tp != NULL);

#ifdef  CONFIG_R8168_NAPI
        RTL_NAPI_DEL(tp);
#endif

        switch (tp->mcfg) {
        case CFG_METHOD_11:
        case CFG_METHOD_12:
        case CFG_METHOD_13:
        case CFG_METHOD_23:
        case CFG_METHOD_27:
        case CFG_METHOD_28:
                if (tp->DASH)
                        rtl8168_driver_stop(tp);
                break;
        }

        unregister_netdev(dev);
        rtl8168_disable_msi(pdev, tp);
#ifdef ENABLE_R8168_PROCFS
        rtl8168_proc_remove(dev);
#endif
        rtl8168_release_board(pdev, dev, tp->mmio_addr);
        pci_set_drvdata(pdev, NULL);
}

static void
rtl8168_set_rxbufsize(struct rtl8168_private *tp,
                      struct net_device *dev)
{
        unsigned int mtu = dev->mtu;

        tp->rx_buf_sz = (mtu > ETH_DATA_LEN) ? mtu + ETH_HLEN + 8 + 1 : RX_BUF_SIZE;
}

static int rtl8168_open(struct net_device *dev)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        struct pci_dev *pdev = tp->pci_dev;
        int retval;

        retval = -ENOMEM;

#ifdef ENABLE_R8168_PROCFS
        rtl8168_proc_init(dev);
#endif
        rtl8168_set_rxbufsize(tp, dev);
        /*
         * Rx and Tx descriptors needs 256 bytes alignment.
         * pci_alloc_consistent provides more.
         */
        tp->TxDescArray = pci_alloc_consistent(pdev, R8168_TX_RING_BYTES,
                                               &tp->TxPhyAddr);
        if (!tp->TxDescArray)
                goto err_free_all_allocated_mem;

        tp->RxDescArray = pci_alloc_consistent(pdev, R8168_RX_RING_BYTES,
                                               &tp->RxPhyAddr);
        if (!tp->RxDescArray)
                goto err_free_all_allocated_mem;

        tp->tally_vaddr = pci_alloc_consistent(pdev, sizeof(*tp->tally_vaddr), &tp->tally_paddr);
        if (!tp->tally_vaddr)
                goto err_free_all_allocated_mem;

        if (tp->UseSwPaddingShortPkt) {
                tp->ShortPacketEmptyBuffer = pci_alloc_consistent(pdev, SHORT_PACKET_PADDING_BUF_SIZE,
                                             &tp->ShortPacketEmptyBufferPhy);
                if (!tp->ShortPacketEmptyBuffer)
                        goto err_free_all_allocated_mem;

                memset(tp->ShortPacketEmptyBuffer, 0x0, SHORT_PACKET_PADDING_BUF_SIZE);
        }

        retval = rtl8168_init_ring(dev);
        if (retval < 0)
                goto err_free_all_allocated_mem;

        if (netif_msg_probe(tp)) {
                printk(KERN_INFO "%s: 0x%lx, "
                       "%2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x, "
                       "IRQ %d\n",
                       dev->name,
                       dev->base_addr,
                       dev->dev_addr[0], dev->dev_addr[1],
                       dev->dev_addr[2], dev->dev_addr[3],
                       dev->dev_addr[4], dev->dev_addr[5], dev->irq);
        }

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
        INIT_WORK(&tp->task, NULL, dev);
#else
        INIT_DELAYED_WORK(&tp->task, NULL);
#endif

#ifdef  CONFIG_R8168_NAPI
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
        RTL_NAPI_ENABLE(dev, &tp->napi);
#endif
#endif

        rtl8168_exit_oob(dev);

        rtl8168_tally_counter_clear(tp);

        rtl8168_hw_init(dev);

        rtl8168_hw_reset(dev);

        rtl8168_powerup_pll(dev);

        rtl8168_hw_ephy_config(dev);

        rtl8168_hw_phy_config(dev);

        rtl8168_hw_config(dev);

        rtl8168_dsm(dev, DSM_IF_UP);

        rtl8168_set_speed(dev, tp->autoneg, tp->speed, tp->duplex);

        retval = request_irq(dev->irq, rtl8168_interrupt, (tp->features & RTL_FEATURE_MSI) ? 0 : SA_SHIRQ, dev->name, dev);
        if (retval<0)
                goto err_free_all_allocated_mem;

        if (tp->esd_flag == 0)
                rtl8168_request_esd_timer(dev);

        rtl8168_request_link_timer(dev);

out:

        return retval;

err_free_all_allocated_mem:
        if (tp->tally_vaddr != NULL) {
                pci_free_consistent(pdev, sizeof(*tp->tally_vaddr), tp->tally_vaddr,
                                    tp->tally_paddr);

                tp->tally_vaddr = NULL;
        }

        if (tp->RxDescArray != NULL) {
                pci_free_consistent(pdev, R8168_RX_RING_BYTES, tp->RxDescArray,
                                    tp->RxPhyAddr);
                tp->RxDescArray = NULL;
        }

        if (tp->TxDescArray != NULL) {
                pci_free_consistent(pdev, R8168_TX_RING_BYTES, tp->TxDescArray,
                                    tp->TxPhyAddr);
                tp->TxDescArray = NULL;
        }

        if (tp->ShortPacketEmptyBuffer != NULL) {
                pci_free_consistent(pdev, ETH_ZLEN, tp->ShortPacketEmptyBuffer,
                                    tp->ShortPacketEmptyBufferPhy);
                tp->ShortPacketEmptyBuffer = NULL;
        }

        goto out;
}

static void
rtl8168_dsm(struct net_device *dev, int dev_state)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;

        switch (dev_state) {
        case DSM_MAC_INIT:
                if ((tp->mcfg == CFG_METHOD_5) || (tp->mcfg == CFG_METHOD_6)) {
                        if (RTL_R8(MACDBG) & 0x80)
                                RTL_W8(GPIO, RTL_R8(GPIO) | GPIO_en);
                        else
                                RTL_W8(GPIO, RTL_R8(GPIO) & ~GPIO_en);
                }

                break;
        case DSM_NIC_GOTO_D3:
        case DSM_IF_DOWN:
                if ((tp->mcfg == CFG_METHOD_5) || (tp->mcfg == CFG_METHOD_6)) {
                        if (RTL_R8(MACDBG) & 0x80)
                                RTL_W8(GPIO, RTL_R8(GPIO) & ~GPIO_en);
                }
                break;

        case DSM_NIC_RESUME_D3:
        case DSM_IF_UP:
                if ((tp->mcfg == CFG_METHOD_5) || (tp->mcfg == CFG_METHOD_6)) {
                        if (RTL_R8(MACDBG) & 0x80)
                                RTL_W8(GPIO, RTL_R8(GPIO) | GPIO_en);
                }

                break;
        }

}

static void
set_offset70F(struct rtl8168_private *tp, u8 setting)
{
        u32 csi_tmp;
        u32 temp = (u32)setting;
        temp = temp << 24;
        /*set PCI configuration space offset 0x70F to setting*/
        /*When the register offset of PCI configuration space larger than 0xff, use CSI to access it.*/

        csi_tmp = rtl8168_csi_read(tp, 0x70c) & 0x00ffffff;
        rtl8168_csi_write(tp, 0x70c, csi_tmp | temp);
}

static void
set_offset79(struct rtl8168_private *tp, u8 setting)
{
        //Set PCI configuration space offset 0x79 to setting

        struct pci_dev *pdev = tp->pci_dev;
        u8 device_control;

        pci_read_config_byte(pdev, 0x79, &device_control);
        device_control &= ~0x70;
        device_control |= setting;
        pci_write_config_byte(pdev, 0x79, device_control);

}

static void
set_offset711(struct rtl8168_private *tp, u8 setting)
{
        u32 csi_tmp;
        u32 temp = (u32)setting;
	temp &= 0x0f;
        temp = temp << 12;
        /*set PCI configuration space offset 0x711 to setting*/

        csi_tmp = rtl8168_csi_read(tp, 0x710) & 0xffff0fff;
        rtl8168_csi_write(tp, 0x710, csi_tmp | temp);
}

static void
rtl8168_hw_set_rx_packet_filter(struct net_device *dev)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;
        u32 mc_filter[2];   /* Multicast hash filter */
        int rx_mode;
        u32 tmp = 0;

        if (dev->flags & IFF_PROMISC) {
                /* Unconditionally log net taps. */
                if (netif_msg_link(tp))
                        printk(KERN_NOTICE "%s: Promiscuous mode enabled.\n",
                               dev->name);

                rx_mode =
                        AcceptBroadcast | AcceptMulticast | AcceptMyPhys |
                        AcceptAllPhys;
                mc_filter[1] = mc_filter[0] = 0xffffffff;
        } else if ((netdev_mc_count(dev) > multicast_filter_limit)
                   || (dev->flags & IFF_ALLMULTI)) {
                /* Too many to filter perfectly -- accept all multicasts. */
                rx_mode = AcceptBroadcast | AcceptMulticast | AcceptMyPhys;
                mc_filter[1] = mc_filter[0] = 0xffffffff;
        } else {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35)
                struct dev_mc_list *mclist;
                unsigned int i;

                rx_mode = AcceptBroadcast | AcceptMyPhys;
                mc_filter[1] = mc_filter[0] = 0;
                for (i = 0, mclist = dev->mc_list; mclist && i < dev->mc_count;
                     i++, mclist = mclist->next) {
                        int bit_nr = ether_crc(ETH_ALEN, mclist->dmi_addr) >> 26;
                        mc_filter[bit_nr >> 5] |= 1 << (bit_nr & 31);
                        rx_mode |= AcceptMulticast;
                }
#else
                struct netdev_hw_addr *ha;

                rx_mode = AcceptBroadcast | AcceptMyPhys;
                mc_filter[1] = mc_filter[0] = 0;
                netdev_for_each_mc_addr(ha, dev) {
                        int bit_nr = ether_crc(ETH_ALEN, ha->addr) >> 26;
                        mc_filter[bit_nr >> 5] |= 1 << (bit_nr & 31);
                        rx_mode |= AcceptMulticast;
                }
#endif
        }

        tmp = mc_filter[0];
        mc_filter[0] = swab32(mc_filter[1]);
        mc_filter[1] = swab32(tmp);

        tp->rtl8168_rx_config = rtl_chip_info[tp->chipset].RCR_Cfg;
        tmp = tp->rtl8168_rx_config | rx_mode | (RTL_R32(RxConfig) & rtl_chip_info[tp->chipset].RxConfigMask);

        RTL_W32(RxConfig, tmp);
        RTL_W32(MAR0 + 0, mc_filter[0]);
        RTL_W32(MAR0 + 4, mc_filter[1]);
}

static void
rtl8168_set_rx_mode(struct net_device *dev)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        unsigned long flags;

        spin_lock_irqsave(&tp->lock, flags);

        rtl8168_hw_set_rx_packet_filter(dev);

        spin_unlock_irqrestore(&tp->lock, flags);
}

static void
rtl8168_hw_config(struct net_device *dev)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;
        struct pci_dev *pdev = tp->pci_dev;
        u8 device_control;
        u16 mac_ocp_data;
        u32 csi_tmp;
        unsigned long flags;

        RTL_W32(RxConfig, (RX_DMA_BURST << RxCfgDMAShift));

        rtl8168_hw_reset(dev);

        RTL_W8(Cfg9346, Cfg9346_Unlock);
        switch (tp->mcfg) {
        case CFG_METHOD_14:
        case CFG_METHOD_15:
        case CFG_METHOD_16:
        case CFG_METHOD_17:
        case CFG_METHOD_18:
        case CFG_METHOD_19:
        case CFG_METHOD_20:
        case CFG_METHOD_21:
        case CFG_METHOD_22:
        case CFG_METHOD_23:
        case CFG_METHOD_24:
        case CFG_METHOD_25:
        case CFG_METHOD_26:
        case CFG_METHOD_27:
        case CFG_METHOD_28:
        case CFG_METHOD_29:
        case CFG_METHOD_30:
                RTL_W8(0xF1, RTL_R8(0xF1) & ~BIT_7);
                RTL_W8(Config2, RTL_R8(Config2) & ~BIT_7);
                RTL_W8(Config5, RTL_R8(Config5) & ~BIT_0);
                break;
        }

        //clear io_rdy_l23
        switch (tp->mcfg) {
        case CFG_METHOD_20:
        case CFG_METHOD_21:
        case CFG_METHOD_22:
        case CFG_METHOD_23:
        case CFG_METHOD_24:
        case CFG_METHOD_25:
        case CFG_METHOD_26:
        case CFG_METHOD_27:
        case CFG_METHOD_28:
        case CFG_METHOD_29:
        case CFG_METHOD_30:
                RTL_W8(Config3, RTL_R8(Config3) & ~BIT_1);
                break;
        }

        RTL_W8(MTPS, Reserved1_data);

        tp->cp_cmd |= INTT_1;
        if (tp->use_timer_interrrupt)
                tp->cp_cmd |= PktCntrDisable;
        else
                tp->cp_cmd &= ~PktCntrDisable;

        RTL_W16(IntrMitigate, 0x5f51);

        rtl8168_tally_counter_addr_fill(tp);

        rtl8168_desc_addr_fill(tp);

        /* Set DMA burst size and Interframe Gap Time */
        if (tp->mcfg == CFG_METHOD_1)
                RTL_W32(TxConfig, (TX_DMA_BURST_512 << TxDMAShift) |
                        (InterFrameGap << TxInterFrameGapShift));
        else
                RTL_W32(TxConfig, (TX_DMA_BURST_unlimited << TxDMAShift) |
                        (InterFrameGap << TxInterFrameGapShift));

        if (tp->mcfg == CFG_METHOD_4) {
                set_offset70F(tp, 0x27);

                RTL_W8(DBG_reg, (0x0E << 4) | Fix_Nak_1 | Fix_Nak_2);

                RTL_W8(Config3, RTL_R8(Config3) & ~Beacon_en);

                //disable clock request.
                pci_write_config_byte(pdev, 0x81, 0x00);

                if (dev->mtu > ETH_DATA_LEN) {
                        RTL_W8(Config3, RTL_R8(Config3) | Jumbo_En0);
                        RTL_W8(Config4, RTL_R8(Config4) | Jumbo_En1);

                        set_offset79(tp, 0x20);

                        //tx checksum offload disable
                        dev->features &= ~NETIF_F_IP_CSUM;

                        //rx checksum offload disable
                } else {
                        RTL_W8(Config3, RTL_R8(Config3) & ~Jumbo_En0);
                        RTL_W8(Config4, RTL_R8(Config4) & ~Jumbo_En1);

                        set_offset79(tp, 0x50);

                        //tx checksum offload enable
                        dev->features |= NETIF_F_IP_CSUM;
                }

                //rx checksum offload enable
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,0,0)
                tp->cp_cmd |= RxChkSum;
#else
                dev->features |= NETIF_F_RXCSUM;
#endif

                tp->cp_cmd &= ~(EnableBist | Macdbgo_oe | Force_halfdup |
                                Force_rxflow_en | Force_txflow_en | Cxpl_dbg_sel |
                                ASF | PktCntrDisable | Macdbgo_sel);
        } else if (tp->mcfg == CFG_METHOD_5) {

                set_offset70F(tp, 0x27);

                RTL_W8(Config3, RTL_R8(Config3) & ~Beacon_en);

                //disable clock request.
                pci_write_config_byte(pdev, 0x81, 0x00);

                if (dev->mtu > ETH_DATA_LEN) {
                        RTL_W8(Config3, RTL_R8(Config3) | Jumbo_En0);
                        RTL_W8(Config4, RTL_R8(Config4) | Jumbo_En1);

                        set_offset79(tp, 0x20);

                        //tx checksum offload disable
                        dev->features &= ~NETIF_F_IP_CSUM;
                } else {
                        RTL_W8(Config3, RTL_R8(Config3) & ~Jumbo_En0);
                        RTL_W8(Config4, RTL_R8(Config4) & ~Jumbo_En1);

                        set_offset79(tp, 0x50);

                        //tx checksum offload enable
                        dev->features |= NETIF_F_IP_CSUM;
                }

                //rx checksum offload enable
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,0,0)
                tp->cp_cmd |= RxChkSum;
#else
                dev->features |= NETIF_F_RXCSUM;
#endif
        } else if (tp->mcfg == CFG_METHOD_6) {
                set_offset70F(tp, 0x27);

                RTL_W8(Config3, RTL_R8(Config3) & ~Beacon_en);

                //disable clock request.
                pci_write_config_byte(pdev, 0x81, 0x00);

                if (dev->mtu > ETH_DATA_LEN) {
                        RTL_W8(Config3, RTL_R8(Config3) | Jumbo_En0);
                        RTL_W8(Config4, RTL_R8(Config4) | Jumbo_En1);

                        set_offset79(tp, 0x20);

                        //tx checksum offload disable
                        dev->features &= ~NETIF_F_IP_CSUM;
                } else {
                        RTL_W8(Config3, RTL_R8(Config3) & ~Jumbo_En0);
                        RTL_W8(Config4, RTL_R8(Config4) & ~Jumbo_En1);

                        set_offset79(tp, 0x50);

                        //tx checksum offload enable
                        dev->features |= NETIF_F_IP_CSUM;
                }

                //rx checksum offload enable
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,0,0)
                tp->cp_cmd |= RxChkSum;
#else
                dev->features |= NETIF_F_RXCSUM;
#endif
        } else if (tp->mcfg == CFG_METHOD_7) {
                set_offset70F(tp, 0x27);

                rtl8168_eri_write(ioaddr, 0x1EC, 1, 0x07, ERIAR_ASF);

                //disable clock request.
                pci_write_config_byte(pdev, 0x81, 0x00);

                RTL_W8(Config3, RTL_R8(Config3) & ~Beacon_en);

                if (dev->mtu > ETH_DATA_LEN) {
                        RTL_W8(Config3, RTL_R8(Config3) | Jumbo_En0);
                        RTL_W8(Config4, RTL_R8(Config4) | Jumbo_En1);

                        set_offset79(tp, 0x20);

                        //tx checksum offload disable
                        dev->features &= ~NETIF_F_IP_CSUM;
                } else {
                        RTL_W8(Config3, RTL_R8(Config3) & ~Jumbo_En0);
                        RTL_W8(Config4, RTL_R8(Config4) & ~Jumbo_En1);


                        set_offset79(tp, 0x50);

                        //tx checksum offload enable
                        dev->features |= NETIF_F_IP_CSUM;
                }
        } else if (tp->mcfg == CFG_METHOD_8) {

                set_offset70F(tp, 0x27);

                rtl8168_eri_write(ioaddr, 0x1EC, 1, 0x07, ERIAR_ASF);

                //disable clock request.
                pci_write_config_byte(pdev, 0x81, 0x00);

                RTL_W8(Config3, RTL_R8(Config3) & ~Beacon_en);

                RTL_W8(0xD1, 0x20);

                if (dev->mtu > ETH_DATA_LEN) {
                        RTL_W8(Config3, RTL_R8(Config3) | Jumbo_En0);
                        RTL_W8(Config4, RTL_R8(Config4) | Jumbo_En1);

                        set_offset79(tp, 0x20);

                        //tx checksum offload disable
                        dev->features &= ~NETIF_F_IP_CSUM;
                } else {
                        RTL_W8(Config3, RTL_R8(Config3) & ~Jumbo_En0);
                        RTL_W8(Config4, RTL_R8(Config4) & ~Jumbo_En1);


                        set_offset79(tp, 0x50);

                        //tx checksum offload enable
                        dev->features |= NETIF_F_IP_CSUM;
                }
        } else if (tp->mcfg == CFG_METHOD_9) {
                set_offset70F(tp, 0x27);

                /* disable clock request. */
                pci_write_config_byte(pdev, 0x81, 0x00);

                RTL_W8(Config3, RTL_R8(Config3) & ~BIT_4);
                RTL_W8(DBG_reg, RTL_R8(DBG_reg) | BIT_7 | BIT_1);

                if (dev->mtu > ETH_DATA_LEN) {
                        RTL_W8(Config3, RTL_R8(Config3) | Jumbo_En0);
                        RTL_W8(Config4, RTL_R8(Config4) | Jumbo_En1);

                        set_offset79(tp, 0x20);

                        /* tx checksum offload disable */
                        dev->features &= ~NETIF_F_IP_CSUM;
                } else {
                        RTL_W8(Config3, RTL_R8(Config3) & ~Jumbo_En0);
                        RTL_W8(Config4, RTL_R8(Config4) & ~Jumbo_En1);


                        set_offset79(tp, 0x50);

                        /* tx checksum offload enable */
                        dev->features |= NETIF_F_IP_CSUM;
                }

                RTL_W8(TDFNR, 0x8);

        } else if (tp->mcfg == CFG_METHOD_10) {
                set_offset70F(tp, 0x27);

                RTL_W8(DBG_reg, RTL_R8(DBG_reg) | BIT_7 | BIT_1);

                if (dev->mtu > ETH_DATA_LEN) {
                        RTL_W8(Config3, RTL_R8(Config3) | Jumbo_En0);
                        RTL_W8(Config4, RTL_R8(Config4) | Jumbo_En1);

                        set_offset79(tp, 0x20);

                        /* tx checksum offload disable */
                        dev->features &= ~NETIF_F_IP_CSUM;
                } else {
                        RTL_W8(Config3, RTL_R8(Config3) & ~Jumbo_En0);
                        RTL_W8(Config4, RTL_R8(Config4) & ~Jumbo_En1);

                        set_offset79(tp, 0x50);

                        /* tx checksum offload enable */
                        dev->features |= NETIF_F_IP_CSUM;
                }

                RTL_W8(TDFNR, 0x8);

                RTL_W8(Config1, RTL_R8(Config1) | 0x10);

                /* disable clock request. */
                pci_write_config_byte(pdev, 0x81, 0x00);
        } else if (tp->mcfg == CFG_METHOD_11 || tp->mcfg == CFG_METHOD_13) {
                set_offset70F(tp, 0x17);
                set_offset79(tp, 0x50);

                if (dev->mtu > ETH_DATA_LEN) {
                        RTL_W8(Config3, RTL_R8(Config3) | Jumbo_En0);

                        /* tx checksum offload disable */
                        dev->features &= ~NETIF_F_IP_CSUM;
                } else {
                        RTL_W8(Config3, RTL_R8(Config3) & ~Jumbo_En0);

                        /* tx checksum offload enable */
                        dev->features |= NETIF_F_IP_CSUM;
                }

                pci_write_config_byte(pdev, 0x81, 0x00);

                RTL_W8(Config1, RTL_R8(Config1) | 0x10);

        } else if (tp->mcfg == CFG_METHOD_12) {
                set_offset70F(tp, 0x17);
                set_offset79(tp, 0x50);

                if (dev->mtu > ETH_DATA_LEN) {
                        RTL_W8(Config3, RTL_R8(Config3) | Jumbo_En0);

                        /* tx checksum offload disable */
                        dev->features &= ~NETIF_F_IP_CSUM;
                } else {
                        RTL_W8(Config3, RTL_R8(Config3) & ~Jumbo_En0);

                        /* tx checksum offload enable */
                        dev->features |= NETIF_F_IP_CSUM;
                }

                pci_write_config_byte(pdev, 0x81, 0x01);

                RTL_W8(Config1, RTL_R8(Config1) | 0x10);

        } else if (tp->mcfg == CFG_METHOD_14 || tp->mcfg == CFG_METHOD_15) {

                set_offset70F(tp, 0x27);
                set_offset79(tp, 0x50);

                if (dev->mtu > ETH_DATA_LEN) {
                        RTL_W8(MTPS, 0x24);
                        RTL_W8(Config3, RTL_R8(Config3) | Jumbo_En0);
                        RTL_W8(Config4, RTL_R8(Config4) | 0x01);

                        /* tx checksum offload disable */
                        dev->features &= ~NETIF_F_IP_CSUM;
                } else {
                        RTL_W8(Config3, RTL_R8(Config3) & ~Jumbo_En0);
                        RTL_W8(Config4, RTL_R8(Config4) & ~0x01);

                        /* tx checksum offload enable */
                        dev->features |= NETIF_F_IP_CSUM;
                }

                RTL_W8(0xF3, RTL_R8(0xF3) | BIT_5);
                RTL_W8(0xF3, RTL_R8(0xF3) & ~BIT_5);

                RTL_W8(0xD0, RTL_R8(0xD0) | BIT_7 | BIT_6);

                RTL_W8(0xD1, RTL_R8(0xD1) | BIT_2 | BIT_3);

                RTL_W8(0xF1, RTL_R8(0xF1) | BIT_6 | BIT_5 | BIT_4 | BIT_2 | BIT_1);

                RTL_W8(TDFNR, 0x8);

                if (aspm)
                        RTL_W8(0xF1, RTL_R8(0xF1) | BIT_7);

                RTL_W8(Config5, RTL_R8(Config5) & ~BIT_3);

                RTL_W8(Config3, RTL_R8(Config3) & ~Beacon_en);

                RTL_W8(Config1, RTL_R8(Config1) & ~0x10);
        } else if (tp->mcfg == CFG_METHOD_16 || tp->mcfg == CFG_METHOD_17) {
                set_offset70F(tp, 0x17);
                set_offset79(tp, 0x50);

                csi_tmp = rtl8168_eri_read(ioaddr, 0xD5, 1, ERIAR_ExGMAC) | BIT_3 | BIT_2;
                rtl8168_eri_write(ioaddr, 0xD5, 1, csi_tmp, ERIAR_ExGMAC);
                rtl8168_eri_write(ioaddr, 0xC0, 2, 0x0000, ERIAR_ExGMAC);
                rtl8168_eri_write(ioaddr, 0xB8, 4, 0x00000000, ERIAR_ExGMAC);
                rtl8168_eri_write(ioaddr, 0xC8, 4, 0x00100002, ERIAR_ExGMAC);
                rtl8168_eri_write(ioaddr, 0xE8, 4, 0x00100006, ERIAR_ExGMAC);
                csi_tmp = rtl8168_eri_read(ioaddr, 0x1D0, 4, ERIAR_ExGMAC);
                csi_tmp |= BIT_1;
                rtl8168_eri_write(ioaddr, 0x1D0, 1, csi_tmp, ERIAR_ExGMAC);

                csi_tmp = rtl8168_eri_read(ioaddr, 0xDC, 1, ERIAR_ExGMAC);
                csi_tmp &= ~BIT_0;
                rtl8168_eri_write(ioaddr, 0xDC, 1, csi_tmp, ERIAR_ExGMAC);
                csi_tmp |= BIT_0;
                rtl8168_eri_write(ioaddr, 0xDC, 1, csi_tmp, ERIAR_ExGMAC);

                RTL_W32(TxConfig, RTL_R32(TxConfig) | BIT_7);
                RTL_W8(0xD3, RTL_R8(0xD3) & ~BIT_7);
                RTL_W8(0x1B, RTL_R8(0x1B) & ~0x07);

                if (tp->mcfg == CFG_METHOD_16) {
                        RTL_W32(0xB0, 0xEE480010);
                        RTL_W8(0x1A, RTL_R8(0x1A) & ~(BIT_2|BIT_3));
                        rtl8168_eri_write(ioaddr, 0x1DC, 1, 0x64, ERIAR_ExGMAC);
                } else {
                        csi_tmp = rtl8168_eri_read(ioaddr, 0x1B0, 4, ERIAR_ExGMAC);
                        csi_tmp |= BIT_4;
                        rtl8168_eri_write(ioaddr, 0x1B0, 1, csi_tmp, ERIAR_ExGMAC);
                        rtl8168_eri_write(ioaddr, 0xCC, 4, 0x00000050, ERIAR_ExGMAC);
                        rtl8168_eri_write(ioaddr, 0xD0, 4, 0x07ff0060, ERIAR_ExGMAC);
                }

                RTL_W8(TDFNR, 0x8);

                RTL_W8(Config2, RTL_R8(Config2) & ~PMSTS_En);

                RTL_W8(0xD0, RTL_R8(0xD0) | BIT_6);
                RTL_W8(0xF2, RTL_R8(0xF2) | BIT_6);

                if (dev->mtu > ETH_DATA_LEN) {
                        RTL_W8(MTPS, 0x27);

                        /* tx checksum offload disable */
                        dev->features &= ~NETIF_F_IP_CSUM;
                } else {
                        /* tx checksum offload enable */
                        dev->features |= NETIF_F_IP_CSUM;
                }

                /* disable clock request. */
                pci_write_config_byte(pdev, 0x81, 0x00);

        } else if (tp->mcfg == CFG_METHOD_18 || tp->mcfg == CFG_METHOD_19) {
                set_offset70F(tp, 0x17);
                set_offset79(tp, 0x50);

                rtl8168_eri_write(ioaddr, 0xC8, 4, 0x00100002, ERIAR_ExGMAC);
                rtl8168_eri_write(ioaddr, 0xE8, 4, 0x00100006, ERIAR_ExGMAC);
                RTL_W32(TxConfig, RTL_R32(TxConfig) | BIT_7);
                RTL_W8(0xD3, RTL_R8(0xD3) & ~BIT_7);
                csi_tmp = rtl8168_eri_read(ioaddr, 0xDC, 1, ERIAR_ExGMAC);
                csi_tmp &= ~BIT_0;
                rtl8168_eri_write(ioaddr, 0xDC, 1, csi_tmp, ERIAR_ExGMAC);
                csi_tmp |= BIT_0;
                rtl8168_eri_write(ioaddr, 0xDC, 1, csi_tmp, ERIAR_ExGMAC);

                if (aspm)
                        RTL_W8(0xF1, RTL_R8(0xF1) | BIT_7);

                if (dev->mtu > ETH_DATA_LEN) {
                        RTL_W8(MTPS, 0x27);

                        /* tx checksum offload disable */
                        dev->features &= ~NETIF_F_IP_CSUM;
                } else {
                        /* tx checksum offload enable */
                        dev->features |= NETIF_F_IP_CSUM;
                }

                RTL_W8(TDFNR, 0x8);

                RTL_W8(0xD0, RTL_R8(0xD0) | BIT_6);
                RTL_W8(0xF2, RTL_R8(0xF2) | BIT_6);

                rtl8168_eri_write(ioaddr, 0xC0, 2, 0x0000, ERIAR_ExGMAC);
                rtl8168_eri_write(ioaddr, 0xB8, 4, 0x00000000, ERIAR_ExGMAC);
                csi_tmp = rtl8168_eri_read(ioaddr, 0xD5, 1, ERIAR_ExGMAC);
                csi_tmp |= BIT_3 | BIT_2;
                rtl8168_eri_write(ioaddr, 0xD5, 1, csi_tmp, ERIAR_ExGMAC);
                RTL_W8(0x1B,RTL_R8(0x1B) & ~0x07);

                csi_tmp = rtl8168_eri_read(ioaddr, 0x1B0, 1, ERIAR_ExGMAC);
                csi_tmp |= BIT_4;
                rtl8168_eri_write(ioaddr, 0x1B0, 1, csi_tmp, ERIAR_ExGMAC);
                csi_tmp = rtl8168_eri_read(ioaddr, 0x1d0, 1, ERIAR_ExGMAC);
                csi_tmp |= BIT_4 | BIT_1;
                rtl8168_eri_write(ioaddr, 0x1d0, 1, csi_tmp, ERIAR_ExGMAC);
                rtl8168_eri_write(ioaddr, 0xCC, 4, 0x00000050, ERIAR_ExGMAC);
                rtl8168_eri_write(ioaddr, 0xd0, 4, 0x00000060, ERIAR_ExGMAC);
        } else if (tp->mcfg == CFG_METHOD_20) {
                set_offset70F(tp, 0x17);
                set_offset79(tp, 0x50);

                rtl8168_eri_write(ioaddr, 0xC8, 4, 0x00100002, ERIAR_ExGMAC);
                rtl8168_eri_write(ioaddr, 0xE8, 4, 0x00100006, ERIAR_ExGMAC);
                RTL_W32(TxConfig, RTL_R32(TxConfig) | BIT_7);
                RTL_W8(0xD3, RTL_R8(0xD3) & ~BIT_7);
                csi_tmp = rtl8168_eri_read(ioaddr, 0xDC, 1, ERIAR_ExGMAC);
                csi_tmp &= ~BIT_0;
                rtl8168_eri_write(ioaddr, 0xDC, 1, csi_tmp, ERIAR_ExGMAC);
                csi_tmp |= BIT_0;
                rtl8168_eri_write(ioaddr, 0xDC, 1, csi_tmp, ERIAR_ExGMAC);

                if (aspm)
                        RTL_W8(0xF1, RTL_R8(0xF1) | BIT_7);

                if (dev->mtu > ETH_DATA_LEN) {
                        RTL_W8(MTPS, 0x27);

                        /* tx checksum offload disable */
                        dev->features &= ~NETIF_F_IP_CSUM;
                } else {
                        /* tx checksum offload enable */
                        dev->features |= NETIF_F_IP_CSUM;
                }

                RTL_W8(TDFNR, 0x8);

                RTL_W8(0xD0, RTL_R8(0xD0) | BIT_6);
                RTL_W8(0xF2, RTL_R8(0xF2) | BIT_6);
                rtl8168_eri_write(ioaddr, 0xC0, 2, 0x0000, ERIAR_ExGMAC);
                rtl8168_eri_write(ioaddr, 0xB8, 4, 0x00000000, ERIAR_ExGMAC);
                csi_tmp = rtl8168_eri_read(ioaddr, 0xD5, 1, ERIAR_ExGMAC);
                csi_tmp |= BIT_3 | BIT_2;
                rtl8168_eri_write(ioaddr, 0xD5, 1, csi_tmp, ERIAR_ExGMAC);

                csi_tmp = rtl8168_eri_read(ioaddr, 0x1B0, 1, ERIAR_ExGMAC);
                csi_tmp |= BIT_4;
                rtl8168_eri_write(ioaddr, 0x1B0, 1, csi_tmp, ERIAR_ExGMAC);
                csi_tmp = rtl8168_eri_read(ioaddr, 0x1d0, 1, ERIAR_ExGMAC);
                csi_tmp |= BIT_4 | BIT_1;
                rtl8168_eri_write(ioaddr, 0x1d0, 1, csi_tmp, ERIAR_ExGMAC);
                rtl8168_eri_write(ioaddr, 0xCC, 4, 0x00000050, ERIAR_ExGMAC);
                rtl8168_eri_write(ioaddr, 0xd0, 4, 0x00000060, ERIAR_ExGMAC);
        } else if (tp->mcfg == CFG_METHOD_21 || tp->mcfg == CFG_METHOD_22 ||
                   tp->mcfg == CFG_METHOD_24 || tp->mcfg == CFG_METHOD_25 ||
                   tp->mcfg == CFG_METHOD_26 || tp->mcfg == CFG_METHOD_29 ||
                   tp->mcfg == CFG_METHOD_30) {
                set_offset70F(tp, 0x17);
                set_offset79(tp, 0x50);
                if (tp->mcfg == CFG_METHOD_21 || tp->mcfg == CFG_METHOD_22)
                        set_offset711(tp, 0x04);

                rtl8168_eri_write(ioaddr, 0xC8, 4, 0x00080002, ERIAR_ExGMAC);
                rtl8168_eri_write(ioaddr, 0xCC, 1, 0x38, ERIAR_ExGMAC);
                rtl8168_eri_write(ioaddr, 0xD0, 1, 0x48, ERIAR_ExGMAC);
                rtl8168_eri_write(ioaddr, 0xE8, 4, 0x00100006, ERIAR_ExGMAC);

                RTL_W32(TxConfig, RTL_R32(TxConfig) | BIT_7);

                csi_tmp = rtl8168_eri_read(ioaddr, 0xDC, 1, ERIAR_ExGMAC);
                csi_tmp &= ~BIT_0;
                rtl8168_eri_write(ioaddr, 0xDC, 1, csi_tmp, ERIAR_ExGMAC);
                csi_tmp |= BIT_0;
                rtl8168_eri_write(ioaddr, 0xDC, 1, csi_tmp, ERIAR_ExGMAC);

                if (tp->mcfg == CFG_METHOD_26) {
                        mac_ocp_data = mac_ocp_read(tp, 0xD3C0);
                        mac_ocp_data &= ~(BIT_11 | BIT_10 | BIT_9 | BIT_8 | BIT_7 | BIT_6 | BIT_5 | BIT_4 | BIT_3 | BIT_2 | BIT_1 | BIT_0);
                        mac_ocp_data |= 0x03A9;
                        mac_ocp_write(tp, 0xD3C0, mac_ocp_data);
                        mac_ocp_data = mac_ocp_read(tp, 0xD3C2);
                        mac_ocp_data &= ~(BIT_7 | BIT_6 | BIT_5 | BIT_4 | BIT_3 | BIT_2 | BIT_1 | BIT_0);
                        mac_ocp_write(tp, 0xD3C2, mac_ocp_data);
                        mac_ocp_data = mac_ocp_read(tp, 0xD3C4);
                        mac_ocp_data |= BIT_0;
                        mac_ocp_write(tp, 0xD3C4, mac_ocp_data);
                } else if (tp->mcfg == CFG_METHOD_29 || tp->mcfg == CFG_METHOD_30) {

                        if(tp->RequireAdjustUpsTxLinkPulseTiming) {
                                mac_ocp_data = mac_ocp_read(tp, 0xD412);
                                mac_ocp_data &= ~(0x0FFF);
                                mac_ocp_data |= tp->SwrCnt1msIni ;
                                mac_ocp_write(tp, 0xD412, mac_ocp_data);
                        }

                        mac_ocp_data = mac_ocp_read(tp, 0xE056);
                        mac_ocp_data &= ~(BIT_7 | BIT_6 | BIT_5 | BIT_4);
                        mac_ocp_data |= (BIT_6 | BIT_5 | BIT_4);
                        mac_ocp_write(tp, 0xE056, mac_ocp_data);

                        mac_ocp_data = mac_ocp_read(tp, 0xE052);
                        mac_ocp_data &= ~( BIT_14 | BIT_13);
                        mac_ocp_data |= BIT_15;
                        mac_ocp_data |= BIT_3;
                        mac_ocp_write(tp, 0xE052, mac_ocp_data);

                        mac_ocp_data = mac_ocp_read(tp, 0xD420);
                        mac_ocp_data &= ~(BIT_11 | BIT_10 | BIT_9 | BIT_8 | BIT_7 | BIT_6 | BIT_5 | BIT_4 | BIT_3 | BIT_2 | BIT_1 | BIT_0);
                        mac_ocp_data |= 0x47F;
                        mac_ocp_write(tp, 0xD420, mac_ocp_data);

                        mac_ocp_data = mac_ocp_read(tp, 0xE0D6);
                        mac_ocp_data &= ~(BIT_8 | BIT_7 | BIT_6 | BIT_5 | BIT_4 | BIT_3 | BIT_2 | BIT_1 | BIT_0);
                        mac_ocp_data |= 0x17F;
                        mac_ocp_write(tp, 0xE0D6, mac_ocp_data);
                }

                RTL_W8(Config3, RTL_R8(Config3) & ~Beacon_en);

                RTL_W8(0x1B, RTL_R8(0x1B) & ~0x07);

                RTL_W8(TDFNR, 0x4);

                RTL_W8(Config2, RTL_R8(Config2) & ~PMSTS_En);

                if (aspm)
                        RTL_W8(0xF1, RTL_R8(0xF1) | BIT_7);

                if (dev->mtu > ETH_DATA_LEN) {
                        RTL_W8(MTPS, 0x27);

                        /* tx checksum offload disable */
                        dev->features &= ~NETIF_F_IP_CSUM;
                } else {
                        /* tx checksum offload enable */
                        dev->features |= NETIF_F_IP_CSUM;
                }

                RTL_W8(0xD0, RTL_R8(0xD0) | BIT_6);
                RTL_W8(0xF2, RTL_R8(0xF2) | BIT_6);

                RTL_W8(0xD0, RTL_R8(0xD0) | BIT_7);

                rtl8168_eri_write(ioaddr, 0xC0, 2, 0x0000, ERIAR_ExGMAC);
                rtl8168_eri_write(ioaddr, 0xB8, 4, 0x00000000, ERIAR_ExGMAC);

                rtl8168_eri_write(ioaddr, 0x5F0, 2, 0x4F87, ERIAR_ExGMAC);

                if (tp->mcfg == CFG_METHOD_29 || tp->mcfg == CFG_METHOD_30) {
                        csi_tmp = rtl8168_eri_read(ioaddr, 0xD4, 4, ERIAR_ExGMAC);
                        csi_tmp |= (BIT_8 | BIT_9 | BIT_10 | BIT_11 | BIT_12);
                        rtl8168_eri_write(ioaddr, 0xD4, 4, csi_tmp, ERIAR_ExGMAC);

                        csi_tmp = rtl8168_eri_read(ioaddr, 0xDC, 4, ERIAR_ExGMAC);
                        csi_tmp |= (BIT_2 | BIT_3 | BIT_4);
                        rtl8168_eri_write(ioaddr, 0xDC, 4, csi_tmp, ERIAR_ExGMAC);
                } else {
                        csi_tmp = rtl8168_eri_read(ioaddr, 0xD4, 4, ERIAR_ExGMAC);
                        csi_tmp |= (BIT_7 | BIT_8 | BIT_9 | BIT_10 | BIT_11 | BIT_12);
                        rtl8168_eri_write(ioaddr, 0xD4, 4, csi_tmp, ERIAR_ExGMAC);
                }

                if (tp->mcfg == CFG_METHOD_21 || tp->mcfg == CFG_METHOD_22 ||
                    tp->mcfg == CFG_METHOD_24 || tp->mcfg == CFG_METHOD_25) {
                        mac_ocp_write(tp, 0xC140, 0xFFFF);
                } else if (tp->mcfg == CFG_METHOD_29 || tp->mcfg == CFG_METHOD_30) {
                        mac_ocp_write(tp, 0xC140, 0xFFFF);
                        mac_ocp_write(tp, 0xC142, 0xFFFF);
                }

                csi_tmp = rtl8168_eri_read(ioaddr, 0x1B0, 4, ERIAR_ExGMAC);
                csi_tmp &= ~BIT_12;
                rtl8168_eri_write(ioaddr, 0x1B0, 4, csi_tmp, ERIAR_ExGMAC);

                if (tp->mcfg == CFG_METHOD_29 || tp->mcfg == CFG_METHOD_30) {
                        csi_tmp = rtl8168_eri_read(ioaddr, 0x2FC, 1, ERIAR_ExGMAC);
                        csi_tmp &= ~(BIT_2);
                        rtl8168_eri_write(ioaddr, 0x2FC, 1, csi_tmp, ERIAR_ExGMAC);
                } else {
                        csi_tmp = rtl8168_eri_read(ioaddr, 0x2FC, 1, ERIAR_ExGMAC);
                        csi_tmp &= ~(BIT_0 | BIT_1 | BIT_2);
                        csi_tmp |= BIT_0;
                        rtl8168_eri_write(ioaddr, 0x2FC, 1, csi_tmp, ERIAR_ExGMAC);
                }

                csi_tmp = rtl8168_eri_read(ioaddr, 0x1D0, 1, ERIAR_ExGMAC);
                csi_tmp |= BIT_1;
                rtl8168_eri_write(ioaddr, 0x1D0, 1, csi_tmp, ERIAR_ExGMAC);
        } else if (tp->mcfg == CFG_METHOD_23 || tp->mcfg == CFG_METHOD_27 ||
                   tp->mcfg == CFG_METHOD_28) {
                set_offset70F(tp, 0x17);
                set_offset79(tp, 0x50);

                rtl8168_eri_write(ioaddr, 0xC8, 4, 0x00080002, ERIAR_ExGMAC);
                rtl8168_eri_write(ioaddr, 0xCC, 1, 0x2F, ERIAR_ExGMAC);
                rtl8168_eri_write(ioaddr, 0xD0, 1, 0x5F, ERIAR_ExGMAC);
                rtl8168_eri_write(ioaddr, 0xE8, 4, 0x00100006, ERIAR_ExGMAC);

                RTL_W32(TxConfig, RTL_R32(TxConfig) | BIT_7);

                csi_tmp = rtl8168_eri_read(ioaddr, 0xDC, 1, ERIAR_ExGMAC);
                csi_tmp &= ~BIT_0;
                rtl8168_eri_write(ioaddr, 0xDC, 1, csi_tmp, ERIAR_ExGMAC);
                csi_tmp |= BIT_0;
                rtl8168_eri_write(ioaddr, 0xDC, 1, csi_tmp, ERIAR_ExGMAC);

                RTL_W8(Config3, RTL_R8(Config3) & ~Beacon_en);

                RTL_W8(0xD0, RTL_R8(0xD0) | BIT_6);
                RTL_W8(0xF2, RTL_R8(0xF2) | BIT_6);

                RTL_W8(0xD0, RTL_R8(0xD0) | BIT_7);

                rtl8168_eri_write(ioaddr, 0xC0, 2, 0x0000, ERIAR_ExGMAC);
                rtl8168_eri_write(ioaddr, 0xB8, 4, 0x00000000, ERIAR_ExGMAC);
                RTL_W8(0x1B, RTL_R8(0x1B) & ~0x07);

                RTL_W8(TDFNR, 0x4);

                if (aspm)
                        RTL_W8(0xF1, RTL_R8(0xF1) | BIT_7);

                csi_tmp = rtl8168_eri_read(ioaddr, 0x1B0, 4, ERIAR_ExGMAC);
                csi_tmp &= ~BIT_12;
                rtl8168_eri_write(ioaddr, 0x1B0, 4, csi_tmp, ERIAR_ExGMAC);

                csi_tmp = rtl8168_eri_read(ioaddr, 0x2FC, 1, ERIAR_ExGMAC);
                csi_tmp &= ~(BIT_0 | BIT_1 | BIT_2);
                csi_tmp |= BIT_0;
                rtl8168_eri_write(ioaddr, 0x2FC, 1, csi_tmp, ERIAR_ExGMAC);

                csi_tmp = rtl8168_eri_read(ioaddr, 0x1D0, 1, ERIAR_ExGMAC);
                csi_tmp |= BIT_1;
                rtl8168_eri_write(ioaddr, 0x1D0, 1, csi_tmp, ERIAR_ExGMAC);

                if (dev->mtu > ETH_DATA_LEN) {
                        RTL_W8(MTPS, 0x27);

                        /* tx checksum offload disable */
                        dev->features &= ~NETIF_F_IP_CSUM;
                } else {
                        /* tx checksum offload enable */
                        dev->features |= NETIF_F_IP_CSUM;
                }

                if (tp->mcfg == CFG_METHOD_27 || tp->mcfg == CFG_METHOD_28) {
                        OOB_mutex_lock(tp);
                        rtl8168_eri_write(ioaddr, 0x5F0, 2, 0x4F87, ERIAR_ExGMAC);
                        OOB_mutex_unlock(tp);
                }

                csi_tmp = rtl8168_eri_read(ioaddr, 0xD4, 4, ERIAR_ExGMAC);
                csi_tmp  |= ( BIT_7 | BIT_8 | BIT_9 | BIT_10 | BIT_11 | BIT_12 );
                rtl8168_eri_write(ioaddr, 0xD4, 4, csi_tmp, ERIAR_ExGMAC);

                mac_ocp_write(tp, 0xC140, 0xFFFF);
                mac_ocp_write(tp, 0xC142, 0xFFFF);

                if (tp->mcfg == CFG_METHOD_28) {
                        mac_ocp_data = mac_ocp_read(tp, 0xD3E2);
                        mac_ocp_data &= 0xF000;
                        mac_ocp_data |= 0x3A9;
                        mac_ocp_write(tp, 0xD3E2, mac_ocp_data);

                        mac_ocp_data = mac_ocp_read(tp, 0xD3E4);
                        mac_ocp_data &= 0xFF00;
                        mac_ocp_write(tp, 0xD3E4, mac_ocp_data);

                        mac_ocp_data = mac_ocp_read(tp, 0xE860);
                        mac_ocp_data |= BIT_7;
                        mac_ocp_write(tp, 0xE860, mac_ocp_data);
                }
        } else if (tp->mcfg == CFG_METHOD_1) {
                RTL_W8(Config3, RTL_R8(Config3) & ~Beacon_en);

                if (dev->mtu > ETH_DATA_LEN) {
                        pci_read_config_byte(pdev, 0x69, &device_control);
                        device_control &= ~0x70;
                        device_control |= 0x28;
                        pci_write_config_byte(pdev, 0x69, device_control);
                } else {
                        pci_read_config_byte(pdev, 0x69, &device_control);
                        device_control &= ~0x70;
                        device_control |= 0x58;
                        pci_write_config_byte(pdev, 0x69, device_control);
                }
        } else if (tp->mcfg == CFG_METHOD_2) {
                RTL_W8(Config3, RTL_R8(Config3) & ~Beacon_en);

                if (dev->mtu > ETH_DATA_LEN) {
                        pci_read_config_byte(pdev, 0x69, &device_control);
                        device_control &= ~0x70;
                        device_control |= 0x28;
                        pci_write_config_byte(pdev, 0x69, device_control);

                        RTL_W8(Config4, RTL_R8(Config4) | (1 << 0));
                } else {
                        pci_read_config_byte(pdev, 0x69, &device_control);
                        device_control &= ~0x70;
                        device_control |= 0x58;
                        pci_write_config_byte(pdev, 0x69, device_control);

                        RTL_W8(Config4, RTL_R8(Config4) & ~(1 << 0));
                }
        } else if (tp->mcfg == CFG_METHOD_3) {
                RTL_W8(Config3, RTL_R8(Config3) & ~Beacon_en);

                if (dev->mtu > ETH_DATA_LEN) {
                        pci_read_config_byte(pdev, 0x69, &device_control);
                        device_control &= ~0x70;
                        device_control |= 0x28;
                        pci_write_config_byte(pdev, 0x69, device_control);

                        RTL_W8(Config4, RTL_R8(Config4) | (1 << 0));
                } else {
                        pci_read_config_byte(pdev, 0x69, &device_control);
                        device_control &= ~0x70;
                        device_control |= 0x58;
                        pci_write_config_byte(pdev, 0x69, device_control);

                        RTL_W8(Config4, RTL_R8(Config4) & ~(1 << 0));
                }
        } else if (tp->mcfg == CFG_METHOD_DEFAULT) {
                dev->features &= ~NETIF_F_IP_CSUM;
        }

        if ((tp->mcfg == CFG_METHOD_1) || (tp->mcfg == CFG_METHOD_2) || (tp->mcfg == CFG_METHOD_3)) {
                /* csum offload command for RTL8168B/8111B */
                tp->tx_tcp_csum_cmd = TxIPCS | TxTCPCS;
                tp->tx_udp_csum_cmd = TxIPCS | TxUDPCS;
                tp->tx_ip_csum_cmd = TxIPCS;
        } else {
                /* csum offload command for RTL8168C/8111C and RTL8168CP/8111CP */
                tp->tx_tcp_csum_cmd = TxIPCS_C | TxTCPCS_C;
                tp->tx_udp_csum_cmd = TxIPCS_C | TxUDPCS_C;
                tp->tx_ip_csum_cmd = TxIPCS_C;
        }


        //other hw parameters
        if (tp->mcfg == CFG_METHOD_21 || tp->mcfg == CFG_METHOD_22 ||
            tp->mcfg == CFG_METHOD_23 || tp->mcfg == CFG_METHOD_24 ||
            tp->mcfg == CFG_METHOD_25 || tp->mcfg == CFG_METHOD_26 ||
            tp->mcfg == CFG_METHOD_27 || tp->mcfg == CFG_METHOD_28)
                rtl8168_eri_write(ioaddr, 0x2F8, 2, 0x1D8F, ERIAR_ExGMAC);

        if (tp->bios_setting & BIT_28) {
                if (tp->mcfg == CFG_METHOD_18 || tp->mcfg == CFG_METHOD_19 ||
                    tp->mcfg == CFG_METHOD_20) {
                        u32 gphy_val;

                        spin_lock_irqsave(&tp->phy_lock, flags);
                        mdio_write(tp, 0x1F, 0x0007);
                        mdio_write(tp, 0x1E, 0x002C);
                        gphy_val = mdio_read(tp, 0x16);
                        gphy_val |= BIT_10;
                        mdio_write(tp, 0x16, gphy_val);
                        mdio_write(tp, 0x1F, 0x0005);
                        mdio_write(tp, 0x05, 0x8B80);
                        gphy_val = mdio_read(tp, 0x06);
                        gphy_val |= BIT_7;
                        mdio_write(tp, 0x06, gphy_val);
                        mdio_write(tp, 0x1F, 0x0000);
                        spin_unlock_irqrestore(&tp->phy_lock, flags);
                }
        }

        rtl8168_hw_clear_timer_int(dev);

        switch (tp->mcfg) {
        case CFG_METHOD_29:
        case CFG_METHOD_30:
                mac_ocp_write(tp, 0xE098, 0x0AA2);
                break;
        }

        switch (tp->mcfg) {
        case CFG_METHOD_21:
        case CFG_METHOD_22:
        case CFG_METHOD_23:
        case CFG_METHOD_24:
        case CFG_METHOD_25:
        case CFG_METHOD_26:
        case CFG_METHOD_27:
        case CFG_METHOD_28:
        case CFG_METHOD_29:
        case CFG_METHOD_30:
                if (aspm) {
                        rtl8168_init_pci_offset_99(tp);
                }
                break;
        }
        switch (tp->mcfg) {
        case CFG_METHOD_24:
        case CFG_METHOD_25:
        case CFG_METHOD_26:
        case CFG_METHOD_27:
        case CFG_METHOD_28:
        case CFG_METHOD_29:
        case CFG_METHOD_30:
                if (aspm) {
                        rtl8168_init_pci_offset_180(tp);
                }
                break;
        }

        tp->cp_cmd &= ~(EnableBist | Macdbgo_oe | Force_halfdup |
                        Force_rxflow_en | Force_txflow_en | Cxpl_dbg_sel |
                        ASF | Macdbgo_sel);

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,0,0)
        RTL_W16(CPlusCmd, tp->cp_cmd);
#else
        rtl8168_hw_set_features(dev, dev->features);
#endif

        switch (tp->mcfg) {
        case CFG_METHOD_16:
        case CFG_METHOD_17:
        case CFG_METHOD_18:
        case CFG_METHOD_19:
        case CFG_METHOD_20:
        case CFG_METHOD_21:
        case CFG_METHOD_22:
        case CFG_METHOD_23:
        case CFG_METHOD_24:
        case CFG_METHOD_25:
        case CFG_METHOD_26:
        case CFG_METHOD_27:
        case CFG_METHOD_28:
        case CFG_METHOD_29:
        case CFG_METHOD_30:	{
                int timeout;
                for (timeout = 0; timeout < 10; timeout++) {
                        if ((rtl8168_eri_read(ioaddr, 0x1AE, 2, ERIAR_ExGMAC) & BIT_13)==0)
                                break;
                        mdelay(1);
                }
        }
        break;
        }

        RTL_W16(RxMaxSize, tp->rx_buf_sz);

        rtl8168_disable_rxdvgate(dev);

        if (tp->mcfg == CFG_METHOD_11 || tp->mcfg == CFG_METHOD_12)
                rtl8168_mac_loopback_test(tp);

        if (!tp->pci_cfg_is_read) {
                pci_read_config_byte(pdev, PCI_COMMAND, &tp->pci_cfg_space.cmd);
                pci_read_config_word(pdev, PCI_BASE_ADDRESS_0, &tp->pci_cfg_space.io_base_l);
                pci_read_config_word(pdev, PCI_BASE_ADDRESS_0 + 2, &tp->pci_cfg_space.io_base_h);
                pci_read_config_word(pdev, PCI_BASE_ADDRESS_2, &tp->pci_cfg_space.mem_base_l);
                pci_read_config_word(pdev, PCI_BASE_ADDRESS_2 + 2, &tp->pci_cfg_space.mem_base_h);
                pci_read_config_word(pdev, PCI_BASE_ADDRESS_3, &tp->pci_cfg_space.resv_0x1c_l);
                pci_read_config_word(pdev, PCI_BASE_ADDRESS_3 + 2, &tp->pci_cfg_space.resv_0x1c_h);
                pci_read_config_byte(pdev, PCI_INTERRUPT_LINE, &tp->pci_cfg_space.ilr);
                pci_read_config_word(pdev, PCI_BASE_ADDRESS_4, &tp->pci_cfg_space.resv_0x20_l);
                pci_read_config_word(pdev, PCI_BASE_ADDRESS_4 + 2, &tp->pci_cfg_space.resv_0x20_h);
                pci_read_config_word(pdev, PCI_BASE_ADDRESS_5, &tp->pci_cfg_space.resv_0x24_l);
                pci_read_config_word(pdev, PCI_BASE_ADDRESS_5 + 2, &tp->pci_cfg_space.resv_0x24_h);
                pci_read_config_word(pdev, PCI_SUBSYSTEM_VENDOR_ID, &tp->pci_cfg_space.resv_0x2c_l);
                pci_read_config_word(pdev, PCI_SUBSYSTEM_VENDOR_ID + 2, &tp->pci_cfg_space.resv_0x2c_h);
                tp->pci_cfg_space.pci_sn_l = rtl8168_csi_read(tp, PCI_DEVICE_SERIAL_NUMBER);
                tp->pci_cfg_space.pci_sn_h = rtl8168_csi_read(tp, PCI_DEVICE_SERIAL_NUMBER + 4);

                tp->pci_cfg_is_read = 1;
        }

        rtl8168_dsm(dev, DSM_MAC_INIT);

        /* Set Rx packet filter */
        rtl8168_hw_set_rx_packet_filter(dev);

#ifdef ENABLE_DASH_SUPPORT
        if (tp->DASH)
                NICChkTypeEnableDashInterrupt(tp);
#endif

        switch (tp->mcfg) {
        case CFG_METHOD_14:
        case CFG_METHOD_15:
        case CFG_METHOD_16:
        case CFG_METHOD_17:
        case CFG_METHOD_18:
        case CFG_METHOD_19:
        case CFG_METHOD_20:
        case CFG_METHOD_21:
        case CFG_METHOD_22:
        case CFG_METHOD_23:
        case CFG_METHOD_24:
        case CFG_METHOD_25:
        case CFG_METHOD_26:
        case CFG_METHOD_27:
        case CFG_METHOD_28:
        case CFG_METHOD_29:
        case CFG_METHOD_30:
                if (aspm) {
                        RTL_W8(Config5, RTL_R8(Config5) | BIT_0);
                        RTL_W8(Config2, RTL_R8(Config2) | BIT_7);
                } else {
                        RTL_W8(Config2, RTL_R8(Config2) & ~BIT_7);
                        RTL_W8(Config5, RTL_R8(Config5) & ~BIT_0);
                }
                break;
        }

        RTL_W8(Cfg9346, Cfg9346_Lock);

        udelay(10);
}

static void
rtl8168_hw_start(struct net_device *dev)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;

        rtl8168_hw_config(dev);

        RTL_W8(ChipCmd, CmdTxEnb | CmdRxEnb);

        rtl8168_enable_hw_interrupt(tp, ioaddr);
}


static int
rtl8168_change_mtu(struct net_device *dev,
                   int new_mtu)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        int max_mtu;
        int ret = 0;
        unsigned long flags;

        if (tp->mcfg == CFG_METHOD_DEFAULT)
                max_mtu = ETH_DATA_LEN;
        else
                max_mtu = tp->max_jumbo_frame_size - ETH_HLEN - 8;

        if (new_mtu < ETH_ZLEN)
                return -EINVAL;
        else if (new_mtu > max_mtu)
                new_mtu = max_mtu;

        if (!netif_running(dev))
                goto out;

        rtl8168_down(dev);

        spin_lock_irqsave(&tp->lock, flags);

        dev->mtu = new_mtu;

        rtl8168_set_rxbufsize(tp, dev);

        ret = rtl8168_init_ring(dev);

        if (ret < 0) {
                spin_unlock_irqrestore(&tp->lock, flags);
                goto out;
        }

#ifdef CONFIG_R8168_NAPI
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
        RTL_NAPI_ENABLE(dev, &tp->napi);
#endif
#endif//CONFIG_R8168_NAPI

        netif_stop_queue(dev);
        netif_carrier_off(dev);
        rtl8168_hw_config(dev);
        spin_unlock_irqrestore(&tp->lock, flags);
        rtl8168_set_speed(dev, tp->autoneg, tp->speed, tp->duplex);

        mod_timer(&tp->esd_timer, jiffies + RTL8168_ESD_TIMEOUT);
        mod_timer(&tp->link_timer, jiffies + RTL8168_LINK_TIMEOUT);

out:
        return ret;
}

static inline void
rtl8168_make_unusable_by_asic(struct RxDesc *desc)
{
        desc->addr = 0x0badbadbadbadbadull;
        desc->opts1 &= ~cpu_to_le32(DescOwn | RsvdMask);
}

static void
rtl8168_free_rx_skb(struct rtl8168_private *tp,
                    struct sk_buff **sk_buff,
                    struct RxDesc *desc)
{
        struct pci_dev *pdev = tp->pci_dev;

        pci_unmap_single(pdev, le64_to_cpu(desc->addr), tp->rx_buf_sz,
                         PCI_DMA_FROMDEVICE);
        dev_kfree_skb(*sk_buff);
        *sk_buff = NULL;
        rtl8168_make_unusable_by_asic(desc);
}

static inline void
rtl8168_mark_to_asic(struct RxDesc *desc,
                     u32 rx_buf_sz)
{
        u32 eor = le32_to_cpu(desc->opts1) & RingEnd;

        desc->opts1 = cpu_to_le32(DescOwn | eor | rx_buf_sz);
}

static inline void
rtl8168_map_to_asic(struct RxDesc *desc,
                    dma_addr_t mapping,
                    u32 rx_buf_sz)
{
        desc->addr = cpu_to_le64(mapping);
        wmb();
        rtl8168_mark_to_asic(desc, rx_buf_sz);
}

static int
rtl8168_alloc_rx_skb(struct pci_dev *pdev,
                     struct sk_buff **sk_buff,
                     struct RxDesc *desc,
                     int rx_buf_sz)
{
        struct sk_buff *skb;
        dma_addr_t mapping;
        int ret = 0;

        skb = dev_alloc_skb(rx_buf_sz + RTK_RX_ALIGN);
        if (!skb)
                goto err_out;

        skb_reserve(skb, RTK_RX_ALIGN);
        *sk_buff = skb;

        mapping = pci_map_single(pdev, skb->data, rx_buf_sz,
                                 PCI_DMA_FROMDEVICE);

        rtl8168_map_to_asic(desc, mapping, rx_buf_sz);

out:
        return ret;

err_out:
        ret = -ENOMEM;
        rtl8168_make_unusable_by_asic(desc);
        goto out;
}

static void
rtl8168_rx_clear(struct rtl8168_private *tp)
{
        int i;

        for (i = 0; i < NUM_RX_DESC; i++) {
                if (tp->Rx_skbuff[i])
                        rtl8168_free_rx_skb(tp, tp->Rx_skbuff + i,
                                            tp->RxDescArray + i);
        }
}

static u32
rtl8168_rx_fill(struct rtl8168_private *tp,
                struct net_device *dev,
                u32 start,
                u32 end)
{
        u32 cur;

        for (cur = start; end - cur > 0; cur++) {
                int ret, i = cur % NUM_RX_DESC;

                if (tp->Rx_skbuff[i])
                        continue;

                ret = rtl8168_alloc_rx_skb(tp->pci_dev, tp->Rx_skbuff + i,
                                           tp->RxDescArray + i, tp->rx_buf_sz);
                if (ret < 0)
                        break;
        }
        return cur - start;
}

static inline void
rtl8168_mark_as_last_descriptor(struct RxDesc *desc)
{
        desc->opts1 |= cpu_to_le32(RingEnd);
}

static void
rtl8168_desc_addr_fill(struct rtl8168_private *tp)
{
        void __iomem *ioaddr = tp->mmio_addr;

        if (!tp->TxPhyAddr || !tp->RxPhyAddr)
                return;

        RTL_W32(TxDescStartAddrLow, ((u64) tp->TxPhyAddr & DMA_BIT_MASK(32)));
        RTL_W32(TxDescStartAddrHigh, ((u64) tp->TxPhyAddr >> 32));
        RTL_W32(RxDescAddrLow, ((u64) tp->RxPhyAddr & DMA_BIT_MASK(32)));
        RTL_W32(RxDescAddrHigh, ((u64) tp->RxPhyAddr >> 32));
}

static void
rtl8168_tx_desc_init(struct rtl8168_private *tp)
{
        int i = 0;

        memset(tp->TxDescArray, 0x0, NUM_TX_DESC * sizeof(struct TxDesc));

        for (i = 0; i < NUM_TX_DESC; i++) {
                if (i == (NUM_TX_DESC - 1))
                        tp->TxDescArray[i].opts1 = cpu_to_le32(RingEnd);
        }
}

static void
rtl8168_rx_desc_offset0_init(struct rtl8168_private *tp, int own)
{
        int i = 0;
        int ownbit = 0;

        if (own)
                ownbit = DescOwn;

        for (i = 0; i < NUM_RX_DESC; i++) {
                if (i == (NUM_RX_DESC - 1))
                        tp->RxDescArray[i].opts1 = cpu_to_le32((ownbit | RingEnd) | (unsigned long)tp->rx_buf_sz);
                else
                        tp->RxDescArray[i].opts1 = cpu_to_le32(ownbit | (unsigned long)tp->rx_buf_sz);
        }
}

static void
rtl8168_rx_desc_init(struct rtl8168_private *tp)
{
        memset(tp->RxDescArray, 0x0, NUM_RX_DESC * sizeof(struct RxDesc));
}

static int
rtl8168_init_ring(struct net_device *dev)
{
        struct rtl8168_private *tp = netdev_priv(dev);

        rtl8168_init_ring_indexes(tp);

        memset(tp->tx_skb, 0x0, NUM_TX_DESC * sizeof(struct ring_info));
        memset(tp->Rx_skbuff, 0x0, NUM_RX_DESC * sizeof(struct sk_buff *));

        rtl8168_tx_desc_init(tp);
        rtl8168_rx_desc_init(tp);

        if (rtl8168_rx_fill(tp, dev, 0, NUM_RX_DESC) != NUM_RX_DESC)
                goto err_out;

        rtl8168_mark_as_last_descriptor(tp->RxDescArray + NUM_RX_DESC - 1);

        return 0;

err_out:
        rtl8168_rx_clear(tp);
        return -ENOMEM;
}

static void
rtl8168_unmap_tx_skb(struct pci_dev *pdev,
                     struct ring_info *tx_skb,
                     struct TxDesc *desc)
{
        unsigned int len = tx_skb->len;

        pci_unmap_single(pdev, le64_to_cpu(desc->addr), len, PCI_DMA_TODEVICE);
        desc->opts1 = 0x00;
        desc->opts2 = 0x00;
        desc->addr = 0x00;
        tx_skb->len = 0;
}

static void
rtl8168_tx_clear(struct rtl8168_private *tp)
{
        unsigned int i;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,22)
        struct net_device *dev = tp->dev;
#endif

        for (i = tp->dirty_tx; i < tp->dirty_tx + NUM_TX_DESC; i++) {
                unsigned int entry = i % NUM_TX_DESC;
                struct ring_info *tx_skb = tp->tx_skb + entry;
                unsigned int len = tx_skb->len;

                if (len) {
                        struct sk_buff *skb = tx_skb->skb;

                        rtl8168_unmap_tx_skb(tp->pci_dev, tx_skb,
                                             tp->TxDescArray + entry);
                        if (skb) {
                                dev_kfree_skb(skb);
                                tx_skb->skb = NULL;
                        }
                        RTLDEV->stats.tx_dropped++;
                }
        }
        tp->cur_tx = tp->dirty_tx = 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
static void rtl8168_schedule_work(struct net_device *dev, void (*task)(void *))
{
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
        struct rtl8168_private *tp = netdev_priv(dev);

        INIT_WORK(&tp->task, task, dev);
        schedule_delayed_work(&tp->task, 4);
#endif //LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
}

static void rtl8168_cancel_schedule_work(struct net_device *dev)
{
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
        struct rtl8168_private *tp = netdev_priv(dev);

        cancel_work_sync(&tp->task);
#endif //LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
}

#else
static void rtl8168_schedule_work(struct net_device *dev, work_func_t task)
{
        struct rtl8168_private *tp = netdev_priv(dev);

        INIT_DELAYED_WORK(&tp->task, task);
        schedule_delayed_work(&tp->task, 4);
}

static void rtl8168_cancel_schedule_work(struct net_device *dev)
{
        struct rtl8168_private *tp = netdev_priv(dev);

        cancel_delayed_work_sync(&tp->task);
}
#endif

static void
rtl8168_wait_for_quiescence(struct net_device *dev)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;

        synchronize_irq(dev->irq);

        /* Wait for any pending NAPI task to complete */
#ifdef CONFIG_R8168_NAPI
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
        RTL_NAPI_DISABLE(dev, &tp->napi);
#endif
#endif//CONFIG_R8168_NAPI

        rtl8168_irq_mask_and_ack(tp, ioaddr);

#ifdef CONFIG_R8168_NAPI
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
        RTL_NAPI_ENABLE(dev, &tp->napi);
#endif
#endif//CONFIG_R8168_NAPI
}

#if 0
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
static void rtl8168_reinit_task(void *_data)
#else
static void rtl8168_reinit_task(struct work_struct *work)
#endif
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
        struct net_device *dev = _data;
#else
        struct rtl8168_private *tp =
                container_of(work, struct rtl8168_private, task.work);
        struct net_device *dev = tp->dev;
#endif
        int ret;

        if (netif_running(dev)) {
                rtl8168_wait_for_quiescence(dev);
                rtl8168_close(dev);
        }

        ret = rtl8168_open(dev);
        if (unlikely(ret < 0)) {
                if (net_ratelimit()) {
                        struct rtl8168_private *tp = netdev_priv(dev);

                        if (netif_msg_drv(tp)) {
                                printk(PFX KERN_ERR
                                       "%s: reinit failure (status = %d)."
                                       " Rescheduling.\n", dev->name, ret);
                        }
                }
                rtl8168_schedule_work(dev, rtl8168_reinit_task);
        }
}
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
static void rtl8168_reset_task(void *_data)
{
        struct net_device *dev = _data;
        struct rtl8168_private *tp = netdev_priv(dev);
#else
static void rtl8168_reset_task(struct work_struct *work)
{
        struct rtl8168_private *tp =
                container_of(work, struct rtl8168_private, task.work);
        struct net_device *dev = tp->dev;
#endif
        unsigned long flags;

        if (!netif_running(dev))
                return;

        rtl8168_wait_for_quiescence(dev);

        rtl8168_rx_interrupt(dev, tp, tp->mmio_addr, ~(u32)0);

        spin_lock_irqsave(&tp->lock, flags);

        rtl8168_tx_clear(tp);

        if (tp->dirty_rx == tp->cur_rx) {
                rtl8168_rx_clear(tp);
                rtl8168_init_ring(dev);
                rtl8168_set_speed(dev, tp->autoneg, tp->speed, tp->duplex);
                spin_unlock_irqrestore(&tp->lock, flags);
        } else {
                spin_unlock_irqrestore(&tp->lock, flags);
                if (net_ratelimit()) {
                        struct rtl8168_private *tp = netdev_priv(dev);

                        if (netif_msg_intr(tp)) {
                                printk(PFX KERN_EMERG
                                       "%s: Rx buffers shortage\n", dev->name);
                        }
                }
                rtl8168_schedule_work(dev, rtl8168_reset_task);
        }
}

static void
rtl8168_tx_timeout(struct net_device *dev)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        unsigned long flags;

        spin_lock_irqsave(&tp->lock, flags);
        netif_stop_queue(dev);
        netif_carrier_off(dev);
        rtl8168_hw_reset(dev);
        spin_unlock_irqrestore(&tp->lock, flags);

        /* Let's wait a bit while any (async) irq lands on */
        rtl8168_schedule_work(dev, rtl8168_reset_task);
}

static int
rtl8168_xmit_frags(struct rtl8168_private *tp,
                   struct sk_buff *skb,
                   u32 opts1,
                   u32 opts2)
{
        struct skb_shared_info *info = skb_shinfo(skb);
        unsigned int cur_frag, entry;
        struct TxDesc *txd = NULL;

        entry = tp->cur_tx;
        for (cur_frag = 0; cur_frag < info->nr_frags; cur_frag++) {
                skb_frag_t *frag = info->frags + cur_frag;
                dma_addr_t mapping;
                u32 status, len;
                void *addr;

                entry = (entry + 1) % NUM_TX_DESC;

                txd = tp->TxDescArray + entry;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,2,0)
                len = frag->size;
                addr = ((void *) page_address(frag->page)) + frag->page_offset;
#else
                len = skb_frag_size(frag);
                addr = skb_frag_address(frag);
#endif
                mapping = pci_map_single(tp->pci_dev, addr, len, PCI_DMA_TODEVICE);

                /* anti gcc 2.95.3 bugware (sic) */
                status = opts1 | len | (RingEnd * !((entry + 1) % NUM_TX_DESC));

                txd->addr = cpu_to_le64(mapping);

                tp->tx_skb[entry].len = len;

                txd->opts1 = cpu_to_le32(status);
                txd->opts2 = cpu_to_le32(opts2);
        }

        if (cur_frag) {
                tp->tx_skb[entry].skb = skb;
                wmb();
                txd->opts1 |= cpu_to_le32(LastFrag);
        }

        return cur_frag;
}

static inline u32
rtl8168_tx_csum(struct sk_buff *skb,
                struct net_device *dev)
{
        struct rtl8168_private *tp = netdev_priv(dev);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22)
        const struct iphdr *ip = skb->nh.iph;
#else
        const struct iphdr *ip = ip_hdr(skb);
#endif
        u32 csum_cmd = 0;

        if (skb->ip_summed == CHECKSUM_PARTIAL) {
                if (ip->protocol == IPPROTO_TCP)
                        csum_cmd = tp->tx_tcp_csum_cmd;
                else if (ip->protocol == IPPROTO_UDP)
                        csum_cmd = tp->tx_udp_csum_cmd;
                else if (ip->protocol == IPPROTO_IP)
                        csum_cmd = tp->tx_ip_csum_cmd;
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
                else
                        WARN_ON(1); /* we need a WARN() */
#endif
        }

        if (tp->ShortPacketSwChecksum && skb->len < 60) {
                if (csum_cmd != 0) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,10) && LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,7)
                        skb_checksum_help(&skb, 0);
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19) && LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,10)
                        skb_checksum_help(skb, 0);
#else
                        skb_checksum_help(skb);
#endif
                        csum_cmd = 0;
                }
        }

        return csum_cmd;
}

static void
rtl8168_sw_padding_short_pkt(struct rtl8168_private *tp,
                             struct sk_buff *skb,
                             u32 opts1,
                             u32 opts2)
{
        unsigned int entry;
        dma_addr_t mapping;
        u32 status, len;
        void *addr;
        struct TxDesc *txd = NULL;

        if (skb->len >= ETH_ZLEN) return;

        entry = tp->cur_tx;
        do {
                entry = (entry + 1) % NUM_TX_DESC;

                txd = tp->TxDescArray + entry;
                len = ETH_ZLEN - skb->len;
                addr = tp->ShortPacketEmptyBuffer;
                mapping = pci_map_single(tp->pci_dev, addr, len, PCI_DMA_TODEVICE);

                status = opts1 | len | (RingEnd * !((entry + 1) % NUM_TX_DESC));

                txd->addr = cpu_to_le64(mapping);

                txd->opts1 = cpu_to_le32(status);
                txd->opts2 = cpu_to_le32(opts2);

                wmb();
                txd->opts1 |= cpu_to_le32(LastFrag);
        } while(FALSE);
}


static int
rtl8168_start_xmit(struct sk_buff *skb,
                   struct net_device *dev)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        unsigned int frags, entry;
        struct TxDesc *txd;
        void __iomem *ioaddr = tp->mmio_addr;
        dma_addr_t mapping;
        u32 len;
        u32 opts1;
        u32 opts2;
        int ret = NETDEV_TX_OK;
        unsigned long flags, large_send;

        spin_lock_irqsave(&tp->lock, flags);

        if (unlikely(TX_BUFFS_AVAIL(tp) < skb_shinfo(skb)->nr_frags)) {
                if (netif_msg_drv(tp)) {
                        printk(KERN_ERR
                               "%s: BUG! Tx Ring full when queue awake!\n",
                               dev->name);
                }
                goto err_stop;
        }

        entry = tp->cur_tx % NUM_TX_DESC;
        txd = tp->TxDescArray + entry;

        if (unlikely(le32_to_cpu(txd->opts1) & DescOwn))
                goto err_stop;

        opts1 = DescOwn;
        opts2 = rtl8168_tx_vlan_tag(tp, skb);

        large_send = 0;
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
        if (dev->features & NETIF_F_TSO) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18)
                u32 mss = skb_shinfo(skb)->tso_size;
#else
                u32 mss = skb_shinfo(skb)->gso_size;
#endif //LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18)

                /* TCP Segmentation Offload (or TCP Large Send) */
                if (mss) {
                        if ((tp->mcfg == CFG_METHOD_1) ||
                            (tp->mcfg == CFG_METHOD_2) ||
                            (tp->mcfg == CFG_METHOD_3)) {
                                opts1 |= LargeSend | ((mss & MSSMask) << 16);
                        } else if ((tp->mcfg == CFG_METHOD_11) ||
                                   (tp->mcfg == CFG_METHOD_12) ||
                                   (tp->mcfg == CFG_METHOD_13)) {
                                opts2 |= LargeSend_DP | ((mss & MSSMask) << 18);
                        } else {
                                opts1 |= LargeSend;
                                opts2 |= (mss & MSSMask) << 18;
                        }
                        large_send = 1;
                }
        }
#endif //LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)

        if (large_send == 0) {
                if (dev->features & NETIF_F_IP_CSUM) {
                        if ((tp->mcfg == CFG_METHOD_1) || (tp->mcfg == CFG_METHOD_2) || (tp->mcfg == CFG_METHOD_3))
                                opts1 |= rtl8168_tx_csum(skb, dev);
                        else
                                opts2 |= rtl8168_tx_csum(skb, dev);
                }
        }

        frags = rtl8168_xmit_frags(tp, skb, opts1, opts2);
        if (frags) {
                len = skb_headlen(skb);
                opts1 |= FirstFrag;
        } else {
                len = skb->len;

                tp->tx_skb[entry].skb = skb;

                if (tp->UseSwPaddingShortPkt && len < 60) {
                        rtl8168_sw_padding_short_pkt(tp, skb, opts1, opts2);
                        opts1 |= FirstFrag;
                        frags++;
                } else {
                        opts1 |= FirstFrag | LastFrag;
                }
        }

        opts1 |= len | (RingEnd * !((entry + 1) % NUM_TX_DESC));
        mapping = pci_map_single(tp->pci_dev, skb->data, len, PCI_DMA_TODEVICE);
        tp->tx_skb[entry].len = len;
        txd->addr = cpu_to_le64(mapping);
        txd->opts2 = cpu_to_le32(opts2);
        txd->opts1 = cpu_to_le32(opts1&~DescOwn);
        wmb();
        txd->opts1 = cpu_to_le32(opts1);

        dev->trans_start = jiffies;

        tp->cur_tx += frags + 1;

        wmb();

        RTL_W8(TxPoll, NPQ);    /* set polling bit */

        if (TX_BUFFS_AVAIL(tp) < MAX_SKB_FRAGS) {
                netif_stop_queue(dev);
                smp_rmb();
                if (TX_BUFFS_AVAIL(tp) >= MAX_SKB_FRAGS)
                        netif_wake_queue(dev);
        }

        spin_unlock_irqrestore(&tp->lock, flags);

out:
        return ret;
err_stop:
        netif_stop_queue(dev);
        ret = NETDEV_TX_BUSY;
        RTLDEV->stats.tx_dropped++;

        spin_unlock_irqrestore(&tp->lock, flags);
        goto out;
}

static void
rtl8168_tx_interrupt(struct net_device *dev,
                     struct rtl8168_private *tp,
                     void __iomem *ioaddr)
{
        unsigned int dirty_tx, tx_left;

        assert(dev != NULL);
        assert(tp != NULL);
        assert(ioaddr != NULL);

        dirty_tx = tp->dirty_tx;
        smp_rmb();
        tx_left = tp->cur_tx - dirty_tx;

        while (tx_left > 0) {
                unsigned int entry = dirty_tx % NUM_TX_DESC;
                struct ring_info *tx_skb = tp->tx_skb + entry;
                u32 len = tx_skb->len;
                u32 status;

                rmb();
                status = le32_to_cpu(tp->TxDescArray[entry].opts1);
                if (status & DescOwn)
                        break;

                RTLDEV->stats.tx_bytes += len;
                RTLDEV->stats.tx_packets++;

                rtl8168_unmap_tx_skb(tp->pci_dev,
                                     tx_skb,
                                     tp->TxDescArray + entry);

                if (tx_skb->skb!=NULL) {
                        dev_kfree_skb_irq(tx_skb->skb);
                        tx_skb->skb = NULL;
                }
                dirty_tx++;
                tx_left--;
        }

        if (tp->dirty_tx != dirty_tx) {
                tp->dirty_tx = dirty_tx;
                smp_wmb();
                if (netif_queue_stopped(dev) &&
                    (TX_BUFFS_AVAIL(tp) >= MAX_SKB_FRAGS)) {
                        netif_wake_queue(dev);
                }
                smp_rmb();
                if (tp->cur_tx != dirty_tx)
                        RTL_W8(TxPoll, NPQ);
        }
}

static inline int
rtl8168_fragmented_frame(u32 status)
{
        return (status & (FirstFrag | LastFrag)) != (FirstFrag | LastFrag);
}

static inline void
rtl8168_rx_csum(struct rtl8168_private *tp,
                struct sk_buff *skb,
                struct RxDesc *desc)
{
        u32 opts1 = le32_to_cpu(desc->opts1);
        u32 opts2 = le32_to_cpu(desc->opts2);
        u32 status = opts1 & RxProtoMask;

        if ((tp->mcfg == CFG_METHOD_1) ||
            (tp->mcfg == CFG_METHOD_2) ||
            (tp->mcfg == CFG_METHOD_3)) {
                /* rx csum offload for RTL8168B/8111B */
                if (((status == RxProtoTCP) && !(opts1 & RxTCPF)) ||
                    ((status == RxProtoUDP) && !(opts1 & RxUDPF)) ||
                    ((status == RxProtoIP) && !(opts1 & RxIPF)))
                        skb->ip_summed = CHECKSUM_UNNECESSARY;
                else
                        skb->ip_summed = CHECKSUM_NONE;
        } else {
                /* rx csum offload for RTL8168C/8111C and RTL8168CP/8111CP */
                if (((status == RxTCPT) && !(opts1 & RxTCPF)) ||
                    ((status == RxUDPT) && !(opts1 & RxUDPF)) ||
                    ((status == 0) && (opts2 & RxV4F) && !(opts1 & RxIPF)))
                        skb->ip_summed = CHECKSUM_UNNECESSARY;
                else
                        skb->ip_summed = CHECKSUM_NONE;
        }
}

static inline int
rtl8168_try_rx_copy(struct sk_buff **sk_buff,
                    int pkt_size,
                    struct RxDesc *desc,
                    int rx_buf_sz)
{
        int ret = -1;

        if (pkt_size < rx_copybreak) {
                struct sk_buff *skb;

                skb = dev_alloc_skb(pkt_size + RTK_RX_ALIGN);
                if (skb) {
                        u8 *data;

                        data = sk_buff[0]->data;
                        skb_reserve(skb, RTK_RX_ALIGN);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,37)
                        prefetch(data - RTK_RX_ALIGN);
#endif
                        eth_copy_and_sum(skb, data, pkt_size, 0);
                        *sk_buff = skb;
                        rtl8168_mark_to_asic(desc, rx_buf_sz);
                        ret = 0;
                }
        }
        return ret;
}

static inline void
rtl8168_rx_skb(struct rtl8168_private *tp,
               struct sk_buff *skb)
{
#ifdef CONFIG_R8168_NAPI
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,29)
        netif_receive_skb(skb);
#else
        napi_gro_receive(&tp->napi, skb);
#endif
#else
        netif_rx(skb);
#endif
}

static int
rtl8168_rx_interrupt(struct net_device *dev,
                     struct rtl8168_private *tp,
                     void __iomem *ioaddr, u32 budget)
{
        unsigned int cur_rx, rx_left;
        unsigned int delta, count = 0;
        unsigned int entry;
        struct RxDesc *desc;
        u32 status;
        u32 rx_quota = RTL_RX_QUOTA(dev, budget);

        assert(dev != NULL);
        assert(tp != NULL);
        assert(ioaddr != NULL);

        if ((tp->RxDescArray == NULL) || (tp->Rx_skbuff == NULL))
                goto rx_out;

        cur_rx = tp->cur_rx;
        entry = cur_rx % NUM_RX_DESC;
        desc = tp->RxDescArray + entry;
        rx_left = NUM_RX_DESC + tp->dirty_rx - cur_rx;
        rx_left = rtl8168_rx_quota(rx_left, (u32)rx_quota);

        for (; rx_left > 0; rx_left--) {
                rmb();
                status = le32_to_cpu(desc->opts1);
                if (status & DescOwn)
                        break;
                if (unlikely(status & RxRES)) {
                        if (netif_msg_rx_err(tp)) {
                                printk(KERN_INFO
                                       "%s: Rx ERROR. status = %08x\n",
                                       dev->name, status);
                        }

                        RTLDEV->stats.rx_errors++;

                        if (status & (RxRWT | RxRUNT))
                                RTLDEV->stats.rx_length_errors++;
                        if (status & RxCRC)
                                RTLDEV->stats.rx_crc_errors++;
                        rtl8168_mark_to_asic(desc, tp->rx_buf_sz);
                } else {
                        struct sk_buff *skb = tp->Rx_skbuff[entry];
                        int pkt_size = (status & 0x00003FFF) - 4;
                        void (*pci_action)(struct pci_dev *, dma_addr_t,
                                           size_t, int) = pci_dma_sync_single_for_device;

                        /*
                         * The driver does not support incoming fragmented
                         * frames. They are seen as a symptom of over-mtu
                         * sized frames.
                         */
                        if (unlikely(rtl8168_fragmented_frame(status))) {
                                RTLDEV->stats.rx_dropped++;
                                RTLDEV->stats.rx_length_errors++;
                                rtl8168_mark_to_asic(desc, tp->rx_buf_sz);
                                continue;
                        }

                        if (tp->cp_cmd & RxChkSum)
                                rtl8168_rx_csum(tp, skb, desc);

                        pci_dma_sync_single_for_cpu(tp->pci_dev,
                                                    le64_to_cpu(desc->addr), tp->rx_buf_sz,
                                                    PCI_DMA_FROMDEVICE);

                        if (rtl8168_try_rx_copy(&skb, pkt_size, desc,
                                                tp->rx_buf_sz)) {
                                pci_action = pci_unmap_single;
                                tp->Rx_skbuff[entry] = NULL;
                        }

                        pci_action(tp->pci_dev, le64_to_cpu(desc->addr),
                                   tp->rx_buf_sz, PCI_DMA_FROMDEVICE);

                        skb->dev = dev;
                        skb_put(skb, pkt_size);
                        skb->protocol = eth_type_trans(skb, dev);

                        if (rtl8168_rx_vlan_skb(tp, desc, skb) < 0)
                                rtl8168_rx_skb(tp, skb);

                        dev->last_rx = jiffies;
                        RTLDEV->stats.rx_bytes += pkt_size;
                        RTLDEV->stats.rx_packets++;
                }

                cur_rx++;
                entry = cur_rx % NUM_RX_DESC;
                desc = tp->RxDescArray + entry;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,37)
                prefetch(desc);
#endif
        }

        count = cur_rx - tp->cur_rx;
        tp->cur_rx = cur_rx;

        delta = rtl8168_rx_fill(tp, dev, tp->dirty_rx, tp->cur_rx);
        if (!delta && count && netif_msg_intr(tp))
                printk(KERN_INFO "%s: no Rx buffer allocated\n", dev->name);
        tp->dirty_rx += delta;

        /*
         * FIXME: until there is periodic timer to try and refill the ring,
         * a temporary shortage may definitely kill the Rx process.
         * - disable the asic to try and avoid an overflow and kick it again
         *   after refill ?
         * - how do others driver handle this condition (Uh oh...).
         */
        if ((tp->dirty_rx + NUM_RX_DESC == tp->cur_rx) && netif_msg_intr(tp))
                printk(KERN_EMERG "%s: Rx buffers exhausted\n", dev->name);

rx_out:
        return count;
}

/*
 *The interrupt handler does all of the Rx thread work and cleans up after
 *the Tx thread.
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
static irqreturn_t rtl8168_interrupt(int irq, void *dev_instance, struct pt_regs *regs)
#else
static irqreturn_t rtl8168_interrupt(int irq, void *dev_instance)
#endif
{
        struct net_device *dev = (struct net_device *) dev_instance;
        struct rtl8168_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;
        int status;
        int handled = 0;

        do {
                status = RTL_R16(IntrStatus);

                if(!(tp->features & RTL_FEATURE_MSI)) {
                        /* hotplug/major error/no more work/shared irq */
                        if ((status == 0xFFFF) || !status)
                                break;

                        if (!(status & (tp->intr_mask | tp->timer_intr_mask)))
                                break;

                        if (unlikely(!netif_running(dev))) {
                                break;
                        }
                }

                handled = 1;

                rtl8168_disable_hw_interrupt(tp, ioaddr);

                switch (tp->mcfg) {
                case CFG_METHOD_9:
                case CFG_METHOD_10:
                case CFG_METHOD_11:
                case CFG_METHOD_12:
                case CFG_METHOD_13:
                case CFG_METHOD_14:
                case CFG_METHOD_15:
                case CFG_METHOD_16:
                case CFG_METHOD_17:
                case CFG_METHOD_18:
                case CFG_METHOD_19:
                case CFG_METHOD_20:
                case CFG_METHOD_21:
                case CFG_METHOD_22:
                case CFG_METHOD_23:
                case CFG_METHOD_24:
                case CFG_METHOD_25:
                case CFG_METHOD_26:
                case CFG_METHOD_27:
                case CFG_METHOD_28:
                case CFG_METHOD_29:
                case CFG_METHOD_30:
                        /* RX_OVERFLOW RE-START mechanism now HW handles it automatically*/
                        RTL_W16(IntrStatus, status&~RxFIFOOver);
                        break;
                default:
                        RTL_W16(IntrStatus, status);
                        break;
                }

                //Work around for rx fifo overflow
                if (unlikely(status & RxFIFOOver)) {
                        if (tp->mcfg == CFG_METHOD_1) {
                                netif_stop_queue(dev);
                                udelay(300);
                                rtl8168_hw_reset(dev);
                                rtl8168_tx_clear(tp);
                                rtl8168_rx_clear(tp);
                                rtl8168_init_ring(dev);
                                rtl8168_hw_start(dev);
                                netif_wake_queue(dev);
                        }
                }

#ifdef ENABLE_DASH_SUPPORT
                if ( tp->DASH ) {
                        if( HW_DASH_SUPPORT_TYPE_2( tp ) ) {
                                u8 DashIntType2Status;

                                DashIntType2Status = RTL_R8(IBISR0);
                                if (DashIntType2Status & ISRIMR_DASH_TYPE2_ROK) {
                                        tp->RcvFwDashOkEvt = TRUE;
                                }
                                if (DashIntType2Status & ISRIMR_DASH_TYPE2_TOK) {
                                        tp->SendFwHostOkEvt = TRUE;
                                }
                                if(DashIntType2Status & ISRIMR_DASH_TYPE2_RX_DISABLE_IDLE) {
                                        tp->DashFwDisableRx = TRUE;
                                }

                                RTL_W8(IBISR0, DashIntType2Status);

                                //hau_dbg
                                //printk("status = %X DashIntType2Status = %X.\n", status, DashIntType2Status);
                        } else {
                                if (IntrStatus & ISRIMR_DP_REQSYS_OK) {
                                        tp->RcvFwReqSysOkEvt = TRUE;
                                }
                                if (IntrStatus & ISRIMR_DP_DASH_OK) {
                                        tp->RcvFwDashOkEvt = TRUE;
                                }
                                if (IntrStatus & ISRIMR_DP_HOST_OK) {
                                        tp->SendFwHostOkEvt = TRUE;
                                }
                        }
                }
#endif

#ifdef CONFIG_R8168_NAPI
                if (status & tp->intr_mask || tp->keep_intr_cnt > 0) {
                        if (tp->keep_intr_cnt > 0) tp->keep_intr_cnt--;

                        if (likely(RTL_NETIF_RX_SCHEDULE_PREP(dev, &tp->napi)))
                                __RTL_NETIF_RX_SCHEDULE(dev, &tp->napi);
                        else if (netif_msg_intr(tp))
                                printk(KERN_INFO "%s: interrupt %04x in poll\n",
                                       dev->name, status);
                } else {
                        tp->keep_intr_cnt = RTK_KEEP_INTERRUPT_COUNT;
                        rtl8168_switch_to_hw_interrupt(tp, ioaddr);
                }
#else
                if (status & tp->intr_mask || tp->keep_intr_cnt > 0) {
                        if (tp->keep_intr_cnt > 0) tp->keep_intr_cnt--;

                        rtl8168_rx_interrupt(dev, tp, tp->mmio_addr, ~(u32)0);
                        rtl8168_tx_interrupt(dev, tp, ioaddr);

#ifdef ENABLE_DASH_SUPPORT
                        if ( tp->DASH ) {
                                struct net_device *dev = tp->dev;

                                HandleDashInterrupt(dev);
                        }
#endif

                        rtl8168_switch_to_timer_interrupt(tp, ioaddr);
                } else {
                        tp->keep_intr_cnt = RTK_KEEP_INTERRUPT_COUNT;
                        rtl8168_switch_to_hw_interrupt(tp, ioaddr);
                }
#endif

        } while (false);

        return IRQ_RETVAL(handled);
}

#ifdef CONFIG_R8168_NAPI
static int rtl8168_poll(napi_ptr napi, napi_budget budget)
{
        struct rtl8168_private *tp = RTL_GET_PRIV(napi, struct rtl8168_private);
        void __iomem *ioaddr = tp->mmio_addr;
        RTL_GET_NETDEV(tp)
        unsigned int work_to_do = RTL_NAPI_QUOTA(budget, dev);
        unsigned int work_done;
        unsigned long flags;

        work_done = rtl8168_rx_interrupt(dev, tp, ioaddr, (u32) budget);

        spin_lock_irqsave(&tp->lock, flags);
        rtl8168_tx_interrupt(dev, tp, ioaddr);
        spin_unlock_irqrestore(&tp->lock, flags);

        RTL_NAPI_QUOTA_UPDATE(dev, work_done, budget);

        if (work_done < work_to_do) {
#ifdef ENABLE_DASH_SUPPORT
                if ( tp->DASH ) {
                        struct net_device *dev = tp->dev;

                        spin_lock_irqsave(&tp->lock, flags);
                        HandleDashInterrupt(dev);
                        spin_unlock_irqrestore(&tp->lock, flags);
                }
#endif

                RTL_NETIF_RX_COMPLETE(dev, napi);
                /*
                 * 20040426: the barrier is not strictly required but the
                 * behavior of the irq handler could be less predictable
                 * without it. Btw, the lack of flush for the posted pci
                 * write is safe - FR
                 */
                smp_wmb();

                rtl8168_switch_to_timer_interrupt(tp, ioaddr);
        }

        return RTL_NAPI_RETURN_VALUE;
}
#endif//CONFIG_R8168_NAPI

static void rtl8168_sleep_rx_enable(struct net_device *dev)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;

        if ((tp->mcfg == CFG_METHOD_1) || (tp->mcfg == CFG_METHOD_2)) {
                RTL_W8(ChipCmd, CmdReset);
                rtl8168_rx_desc_offset0_init(tp, 0);
                RTL_W8(ChipCmd, CmdRxEnb);
        } else if (tp->mcfg == CFG_METHOD_14 || tp->mcfg == CFG_METHOD_15) {
                rtl8168_ephy_write(ioaddr, 0x19, 0xFF64);
                RTL_W32(RxConfig, RTL_R32(RxConfig) | AcceptBroadcast | AcceptMulticast | AcceptMyPhys);
        }
}

static void rtl8168_down(struct net_device *dev)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        unsigned long flags;

        rtl8168_delete_esd_timer(dev, &tp->esd_timer);

        rtl8168_delete_link_timer(dev, &tp->link_timer);

#ifdef CONFIG_R8168_NAPI
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,23)
        napi_disable(&tp->napi);
#endif
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,23)) && (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0))
        netif_poll_disable(dev);
#endif
#endif

        netif_stop_queue(dev);

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,11)
        /* Give a racing hard_start_xmit a few cycles to complete. */
        synchronize_sched();  /* FIXME: should this be synchronize_irq()? */
#endif

        spin_lock_irqsave(&tp->lock, flags);

        netif_carrier_off(dev);

        rtl8168_dsm(dev, DSM_IF_DOWN);

        rtl8168_hw_reset(dev);

        spin_unlock_irqrestore(&tp->lock, flags);

        synchronize_irq(dev->irq);

        spin_lock_irqsave(&tp->lock, flags);

        rtl8168_tx_clear(tp);

        rtl8168_rx_clear(tp);

        rtl8168_sleep_rx_enable(dev);

        spin_unlock_irqrestore(&tp->lock, flags);
}

static int rtl8168_close(struct net_device *dev)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        struct pci_dev *pdev = tp->pci_dev;

        if (tp->TxDescArray!=NULL && tp->RxDescArray!=NULL) {
                rtl8168_cancel_schedule_work(dev);

                rtl8168_down(dev);

                rtl8168_hw_d3_para(dev);

                rtl8168_powerdown_pll(dev);

                free_irq(dev->irq, dev);

                pci_free_consistent(pdev, R8168_RX_RING_BYTES, tp->RxDescArray,
                                    tp->RxPhyAddr);
                pci_free_consistent(pdev, R8168_TX_RING_BYTES, tp->TxDescArray,
                                    tp->TxPhyAddr);
                tp->TxDescArray = NULL;
                tp->RxDescArray = NULL;

                if (tp->tally_vaddr != NULL) {
                        pci_free_consistent(pdev, sizeof(*tp->tally_vaddr), tp->tally_vaddr, tp->tally_paddr);
                        tp->tally_vaddr = NULL;
                }

                if (tp->ShortPacketEmptyBuffer != NULL) {
                        pci_free_consistent(pdev, SHORT_PACKET_PADDING_BUF_SIZE, tp->ShortPacketEmptyBuffer,
                                            tp->ShortPacketEmptyBufferPhy);
                        tp->ShortPacketEmptyBuffer = NULL;
                }
        }

        return 0;
}

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,11)
static void rtl8168_shutdown(struct pci_dev *pdev)
{
        struct net_device *dev = pci_get_drvdata(pdev);
        struct rtl8168_private *tp = netdev_priv(dev);

        switch (tp->mcfg) {
        case CFG_METHOD_11:
        case CFG_METHOD_12:
        case CFG_METHOD_13:
        case CFG_METHOD_23:
        case CFG_METHOD_27:
        case CFG_METHOD_28:
                if (tp->DASH)
                        rtl8168_driver_stop(tp);
                break;
        }

        rtl8168_set_bios_setting(dev);
        rtl8168_rar_set(tp, tp->org_mac_addr);

#ifdef ENABLE_REALWOW_SUPPORT
        set_realwow_d3_para(dev);
#endif

        if (s5wol == 0)
                tp->wol_enabled = WOL_DISABLED;

        rtl8168_close(dev);
        rtl8168_disable_msi(pdev, tp);
}
#endif

/**
 *  rtl8168_get_stats - Get rtl8168 read/write statistics
 *  @dev: The Ethernet Device to get statistics for
 *
 *  Get TX/RX statistics for rtl8168
 */
static struct
net_device_stats *rtl8168_get_stats(struct net_device *dev)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22)
        struct rtl8168_private *tp = netdev_priv(dev);
#endif
        if (netif_running(dev)) {
//      spin_lock_irqsave(&tp->lock, flags);
//      spin_unlock_irqrestore(&tp->lock, flags);
        }

        return &RTLDEV->stats;
}

#ifdef CONFIG_PM

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,11)
static int
rtl8168_suspend(struct pci_dev *pdev, u32 state)
#else
static int
rtl8168_suspend(struct pci_dev *pdev, pm_message_t state)
#endif
{
        struct net_device *dev = pci_get_drvdata(pdev);
        struct rtl8168_private *tp = netdev_priv(dev);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,10)
        u32 pci_pm_state = pci_choose_state(pdev, state);
#endif
        unsigned long flags;

        if (!netif_running(dev))
                goto out;

        rtl8168_cancel_schedule_work(dev);

        rtl8168_delete_esd_timer(dev, &tp->esd_timer);

        rtl8168_delete_link_timer(dev, &tp->link_timer);

        netif_stop_queue(dev);

        netif_carrier_off(dev);

        rtl8168_dsm(dev, DSM_NIC_GOTO_D3);

        netif_device_detach(dev);

        spin_lock_irqsave(&tp->lock, flags);

        rtl8168_hw_reset(dev);

        rtl8168_sleep_rx_enable(dev);

        rtl8168_hw_d3_para(dev);

#ifdef ENABLE_REALWOW_SUPPORT
        set_realwow_d3_para(dev);
#endif

        rtl8168_powerdown_pll(dev);

        spin_unlock_irqrestore(&tp->lock, flags);

        switch (tp->mcfg) {
        case CFG_METHOD_11:
        case CFG_METHOD_12:
        case CFG_METHOD_13:
        case CFG_METHOD_23:
        case CFG_METHOD_27:
        case CFG_METHOD_28:
                if (tp->DASH)
                        rtl8168_driver_stop(tp);
                break;
        }

out:

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,10)
        pci_save_state(pdev, &pci_pm_state);
#else
        pci_save_state(pdev);
#endif
        pci_enable_wake(pdev, pci_choose_state(pdev, state), tp->wol_enabled);
//  pci_set_power_state(pdev, pci_choose_state(pdev, state));

        return 0;
}

static int
rtl8168_resume(struct pci_dev *pdev)
{
        struct net_device *dev = pci_get_drvdata(pdev);
        struct rtl8168_private *tp = netdev_priv(dev);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,10)
        u32 pci_pm_state = PCI_D0;
#endif

        pci_set_power_state(pdev, PCI_D0);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,10)
        pci_restore_state(pdev, &pci_pm_state);
#else
        pci_restore_state(pdev);
#endif
        pci_enable_wake(pdev, PCI_D0, 0);

        /* restore last modified mac address */
        rtl8168_rar_set(tp, dev->dev_addr);

        if (!netif_running(dev))
                goto out;

        rtl8168_exit_oob(dev);

        rtl8168_dsm(dev, DSM_NIC_RESUME_D3);

        rtl8168_hw_init(dev);

        rtl8168_powerup_pll(dev);

        rtl8168_hw_ephy_config(dev);

        rtl8168_hw_phy_config(dev);

        rtl8168_schedule_work(dev, rtl8168_reset_task);

        netif_device_attach(dev);

        mod_timer(&tp->esd_timer, jiffies + RTL8168_ESD_TIMEOUT);
        mod_timer(&tp->link_timer, jiffies + RTL8168_LINK_TIMEOUT);
out:
        return 0;
}

#endif /* CONFIG_PM */

static int __devinit
rtl8168_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	struct rtl8168_private *tp;
	struct net_device *dev;
	PPGDEV	mydev;
	int ret;

	ret = pgdrv_prob(pdev, id);
	if (ret)
		goto exit_probe;

	mydev = pci_get_drvdata(pdev);
	pci_set_drvdata(pdev, NULL);

	ret = rtl8168_init_one(pdev, id);
	if (ret) {
		pgdrv_remove(pdev);
		goto exit_probe;
	}

	dev = pci_get_drvdata(pdev);
	tp = netdev_priv(dev);
	tp->pgdev = mydev;

exit_probe:
	return ret;
}

static void __devexit rtl8168_remove(struct pci_dev *pdev)
{
	struct net_device *dev = pci_get_drvdata(pdev);
	struct rtl8168_private *tp = netdev_priv(dev);
	PPGDEV	mydev = tp->pgdev;

	rtl8168_remove_one(pdev);
	pci_set_drvdata(pdev, mydev);
	pgdrv_remove(pdev);
}

static struct pci_driver rtl8168_pci_driver = {
        .name       = MODULENAME,
        .id_table   = rtl8168_pci_tbl,
	.probe      = rtl8168_probe,
	.remove     = __devexit_p(rtl8168_remove),
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,11)
        .shutdown   = rtl8168_shutdown,
#endif
#ifdef CONFIG_PM
        .suspend    = rtl8168_suspend,
        .resume     = rtl8168_resume,
#endif
};

static int __init
rtl8168_init_module(void)
{
	int ret;

	ret = pgdrv_init();
	if (ret < 0)
		return ret;

#ifdef ENABLE_R8168_PROCFS
        rtl8168_proc_module_init();
#endif
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
        return pci_register_driver(&rtl8168_pci_driver);
#else
        return pci_module_init(&rtl8168_pci_driver);
#endif
}

static void __exit
rtl8168_cleanup_module(void)
{
        pci_unregister_driver(&rtl8168_pci_driver);
	pgdrv_exit();
#ifdef ENABLE_R8168_PROCFS
        if (rtl8168_proc) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
                remove_proc_subtree(MODULENAME, init_net.proc_net);
#else
                remove_proc_entry(MODULENAME, init_net.proc_net);
#endif
                rtl8168_proc = NULL;
        }
#endif
}

module_init(rtl8168_init_module);
module_exit(rtl8168_cleanup_module);
