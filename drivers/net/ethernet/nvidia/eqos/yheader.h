/* =========================================================================
 * The Synopsys DWC ETHER QOS Software Driver and documentation (hereinafter
 * "Software") is an unsupported proprietary work of Synopsys, Inc. unless
 * otherwise expressly agreed to in writing between Synopsys and you.
 *
 * The Software IS NOT an item of Licensed Software or Licensed Product under
 * any End User Software License Agreement or Agreement for Licensed Product
 * with Synopsys or any supplement thereto.  Permission is hereby granted,
 * free of charge, to any person obtaining a copy of this software annotated
 * with this license and the Software, to deal in the Software without
 * restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject
 * to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THIS SOFTWARE IS BEING DISTRIBUTED BY SYNOPSYS SOLELY ON AN "AS IS" BASIS
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE HEREBY DISCLAIMED. IN NO EVENT SHALL SYNOPSYS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 * ========================================================================= */
/*
 * Copyright (c) 2015-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */
#ifndef __EQOS__YHEADER__

#define __EQOS__YHEADER__

/* OS Specific declarations and definitions */

#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/cdev.h>

#include <linux/init.h>
#include <linux/timer.h>
#include <linux/sched.h>
#include <linux/highmem.h>
#include <linux/proc_fs.h>
#include <linux/in.h>
#include <linux/ctype.h>
#include <linux/version.h>
#include <linux/ptrace.h>
#include <linux/dma-mapping.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/skbuff.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/crc32.h>
#include <linux/bitops.h>
#include <linux/mii.h>
#include <asm/processor.h>
#include <asm/dma.h>
#include <asm/page.h>
#include <asm/irq.h>
#include <net/checksum.h>
#include <linux/tcp.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <linux/inet_lro.h>
#include <linux/semaphore.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/phy.h>
#include <linux/mdio.h>
#include <linux/of_mdio.h>
#include <linux/thermal.h>

#define L32(data) ((data)&0xFFFFFFFF)
#define H32(data) (((data)&0xFFFFFFFF00000000)>>32)

#if defined(CONFIG_VLAN_8021Q) || defined(CONFIG_VLAN_8021Q_MODULE)
#define EQOS_ENABLE_VLAN_TAG
#include <linux/if_vlan.h>
#endif
#include <linux/if_vlan.h>
/* for PPT */
#include <linux/net_tstamp.h>
#include <linux/ptp_clock_kernel.h>
#include <linux/clocksource.h>
//#include <linux/timecompare.h>
#include <linux/platform_device.h>

/* QOS Version Control Macros */
//#define EQOS_VER_4_0  /* Default Configuration is for QOS version 4.1 and above */

/* Macro definitions*/

#include <asm-generic/errno.h>
#include <linux/platform/tegra/isomgr.h>

#ifdef CONFIG_PTPSUPPORT_OBJ
#define EQOS_CONFIG_PTP
#endif

#ifdef CONFIG_DEBUGFS_OBJ
#define EQOS_CONFIG_DEBUGFS
#endif


/* Enable PBLX8 setting */
#define PBLX8

/* EQOS DMA Burst size in bytes.  Note below must be power of 2 */
#define EQOS_DMA_BURST_SIZE 128

/* Width of AXI bus in bytes */
#define AXI_BUS_WIDTH 16

/* Length of RX buffer in bytes.
 * Note that this should be a multiple
 * of EQOS_DMA_BURST_SIZE.
 */
#define EQOS_RX_BUF_LEN 2048

/* Max value of RXPBL */
#define MAX_RXPBL 32

#define ALIGN_BOUNDARY (EQOS_DMA_BURST_SIZE - 1)
#define ALIGN_SIZE(size) ((size + ALIGN_BOUNDARY) & (~ALIGN_BOUNDARY))

#ifdef PBLX8
#define RXPBL (EQOS_DMA_BURST_SIZE / (AXI_BUS_WIDTH * 8))
#else
#define RXPBL (EQOS_DMA_BURST_SIZE / (AXI_BUS_WIDTH))
#endif

/* NOTE: Uncomment below line for TX and RX DESCRIPTOR DUMP in KERNEL LOG */
//#define EQOS_ENABLE_TX_DESC_DUMP
//#define EQOS_ENABLE_RX_DESC_DUMP

/* NOTE: Uncomment below line for TX and RX PACKET DUMP in KERNEL LOG */
//#define EQOS_ENABLE_TX_PKT_DUMP
//#define EQOS_ENABLE_RX_PKT_DUMP

//#define ENABLE_CHANNEL_DATA_CHECK

/* Uncomment below macro definitions for testing corresponding IP features in driver */
#define EQOS_QUEUE_SELECT_ALGO
#define EQOS_TXPOLLING_MODE_ENABLE

//#ifdef EQOS_CONFIG_PTP
//#undef EQOS_TXPOLLING_MODE_ENABLE
//#endif

/* Enable this to have support for only 1588 V2 VLAN un-TAGGED ptp packets */
//#define DWC_1588_VLAN_UNTAGGED


/* Uncomment below macro to test EEE feature Tx path with
 * no EEE supported PHY card
 * */
#define EQOS_ENABLE_EEE

/* Uncomment below enable tx buffer alignment test code */
/* #define DO_TX_ALIGN_TEST */

#ifdef EQOS_CUSTOMIZED_EEE_TEST
#undef EQOS_TXPOLLING_MODE_ENABLE
#endif

/* NOTE: Uncomment below line for function trace log messages in KERNEL LOG */
/* #define YDEBUG */
/* #define YDEBUG_PG */
/* #define YDEBUG_MDIO */
/* #define YDEBUG_PTP */
/* #define YDEBUG_FILTER */
/* #define YDEBUG_EEE */
/* #define YDEBUG_DMSG */
/* #define YDEBUG_ETHTOOL_ */

#define Y_TRUE 1
#define Y_FALSE 0
#define Y_SUCCESS 0
#define Y_FAILURE 1
#define Y_INV_WR 1
#define Y_INV_RD 2
#define Y_INV_ARG 3
#define Y_MAX_THRD_XEEDED 4

/* The following macros map error macros to POSIX errno values */
#define ERR_READ_TIMEOUT ETIME
#define ERR_WRITE_TIMEOUT ETIME
#define ERR_FIFO_READ_FAILURE EIO
#define ERR_FIFO_WRITE_FAILURE EIO
#define ERR_READ_OVRFLW ENOBUFS
#define ERR_READ_UNDRFLW ENODATA
#define ERR_WRITE_OVRFLW ENOBUFS
#define ERR_WRITE_UNDRFLW ENODATA

/* Helper macros for STANDARD VIRTUAL register handling */

#define GET_TX_ERROR_COUNTERS_PTR (&(pdata->tx_error_counters))

#define GET_RX_ERROR_COUNTERS_PTR (&(pdata->rx_error_counters))

#define GET_TX_PKT_FEATURES_PTR(qinx) (&(pdata->tx_pkt_ctx[(qinx)]))

#define MASK (0x1ULL << 0 | \
	0x13c7ULL << 32)
#define MAC_MASK (0x10ULL << 0)
#define TX_DESC_CNT 256
#define RX_DESC_CNT 256


#define MIN_RX_DESC_CNT 16
#define TX_BUF_SIZE 1536
#define RX_BUF_SIZE 1568
#define EQOS_MAX_LRO_DESC 16
#define EQOS_MAX_LRO_AGGR 32

#define MIN_PACKET_SIZE 60

/* Max number of chans supported */
#define MAX_CHANS	4

/*
#ifdef EQOS_ENABLE_VLAN_TAG
#define MAX_PACKET_SIZE VLAN_ETH_FRAME_LEN
#else
#define MAX_PACKET_SIZE 1514
#endif
*/

#define EQOS_MAX_HDR_SIZE EQOS_HDR_SIZE_256B

#define MAX_MULTICAST_LIST 14
#define RX_DESC_DATA_LENGTH_LBIT 0
#define RX_DESC_DATA_LENGTH 0x7fff
#define EQOS_TX_FLAGS_IP_PKT 0x00000001
#define EQOS_TX_FLAGS_TCP_PKT 0x00000002

#define DEV_NAME "eqos"
#define DEV_ADDRESS 0xffffffff
#define DEV_REG_MMAP_SIZE 0x14e8

/*PTP Multicast MAC addresses*/
#define PTP1_MAC0 0x01
#define PTP1_MAC1 0x1B
#define PTP1_MAC2 0x19
#define PTP1_MAC3 0x00
#define PTP1_MAC4 0x00
#define PTP1_MAC5 0x00

#define PTP2_MAC0 0x01
#define PTP2_MAC1 0x80
#define PTP2_MAC2 0xC2
#define PTP2_MAC3 0x00
#define PTP2_MAC4 0x00
#define PTP2_MAC5 0x0E

#define PTP_DMA_CH_DEFAULT 0
#define PTP_DMA_CH_MAX 3


/* MII/GMII register offset */
#define EQOS_AUTO_NEGO_NP    0x0007
#define EQOS_PHY_CTL     0x0010
#define EQOS_PHY_STS     0x0011
#define EQOS_AUX_CTL     0x0018

/* Broadcom phy regs */
#define BCM_RX_ERR_CNT_REG			0x12
#define BCM_FALSE_CARR_CNT_REG			0x13
#define BCM_RX_NOT_OK_CNT_REG			0x14
#define BCM_EXPANSION_REG			0x15
#define BCM_EXPANSION_CTRL_REG			0x17
#define BCM_AUX_CTRL_SHADOW_REG			0x18
#define BCM_INT_STATUS_REG			0x1a
#define BCM_INT_MASK_REG			0x1b

/* Broadcom Shadow regs */
#define BCM_10BASET_SHADOW_REG			0x1
#define BCM_POWER_MII_CTRL_SHADOW_REG		0x2
#define BCM_MISC_TEST_SHADOW_REG		0x4
#define BCM_MISC_CTRL_SHADOW_REG		0x7

/* Broadcom expansion regs */
#define BCM_PKT_CNT_EXP_REG			0xf00

/* Clause 22 registers to access clause 45 register set */
#define MMD_CTRL_REG		0x0D	/* MMD Access Control Register */
#define MMD_ADDR_DATA_REG	0x0E	/* MMD Access Address Data Register */

/* MMD Access Control register fields */
#define MMD_CTRL_FUNC_ADDR		0x0000	/* address */
#define MMD_CTRL_FUNC_DATA_NOINCR	0x4000	/* data, no post increment */
/* data, post increment on reads & writes */
#define MMD_CTRL_FUNC_DATA_INCR_ON_RDWT	0x8000
/* data, post increment on writes only */
#define MMD_CTRL_FUNC_DATA_INCR_ON_WT	0xC000
/* Clause 45 expansion register */
#define CL45_PCS_EEE_ABLE 0x14	/* EEE Capability register */
#define CL45_ADV_EEE_REG 0x3C   /* EEE advertisement */
#define CL45_RES_EEE_REG 0x803E   /* EEE resolution */
#define CL45_LPI_COUNTER_REG 0x803F   /* LPI Mode Counter */
#define CL45_AN_EEE_LPABLE_REG	0x3D	/* EEE Link Partner ability reg */
#define CL45_CLK_STOP_EN_REG 0x0 /* Clock Stop enable reg */

/* Clause 45 expansion registers fields */
#define CL45_LP_ADV_EEE_STATS_1000BASE_T 0x0004	/* LP EEE capabilities status */
#define CL45_CLK_STOP_EN	0x400 /* Enable xMII Clock Stop */

/* Default MTL queue operation mode values */
#define EQOS_Q_DISABLED	0x0
#define EQOS_Q_AVB 			0x1
#define EQOS_Q_DCB 			0x2
#define EQOS_Q_GENERIC 	0x3

/* Driver PMT macros */
#define EQOS_DRIVER_CONTEXT 1
#define EQOS_IOCTL_CONTEXT 2
#define EQOS_MAGIC_WAKEUP	(1 << 0)
#define EQOS_REMOTE_WAKEUP	(1 << 1)
#define EQOS_POWER_DOWN_TYPE(x)	\
		((x->power_down_type & EQOS_MAGIC_WAKEUP) ? \
		"Magic packet" : \
		((x->power_down_type & EQOS_REMOTE_WAKEUP) ? \
		"Remote wakeup packet" : \
		"<error>"))

#define EQOS_MAC_ADDR_LEN 6
#define EQOS_ETH_FRAME_LEN (ETH_FRAME_LEN + ETH_FCS_LEN + VLAN_HLEN)

/* max size of standard/default frame */
#define EQOS_MAX_ETH_FRAME_LEN_DEFAULT \
	(ETH_FRAME_LEN + ETH_FCS_LEN + VLAN_HLEN)

#define FIFO_SIZE_B(x) (x)
#define FIFO_SIZE_KB(x) (x*1024)

//#define EQOS_MAX_DATA_PER_TX_BUF (1 << 13)     /* 8 KB Maximum data per buffer pointer(in Bytes) */
#define EQOS_MAX_DATA_PER_TX_BUF (1 << 12)	/* for testing purpose: 4 KB Maximum data per buffer pointer(in Bytes) */
#define EQOS_MAX_DATA_PER_TXD (EQOS_MAX_DATA_PER_TX_BUF)

#define EQOS_MAX_SUPPORTED_MTU 1500
#define EQOS_MAX_GPSL 9000 /* Default maximum Gaint Packet Size Limit */
#define EQOS_MIN_SUPPORTED_MTU (ETH_ZLEN + ETH_FCS_LEN + VLAN_HLEN)

#define EQOS_RDESC3_OWN	0x80000000
#define EQOS_RDESC3_FD		0x20000000
#define EQOS_RDESC3_LD		0x10000000
#define EQOS_RDESC3_RS2V	0x08000000
#define EQOS_RDESC3_RS1V	0x04000000
#define EQOS_RDESC3_RS0V	0x02000000
#define EQOS_RDESC3_CRC		0x01000000
#define EQOS_RDESC3_GP		0x00800000
#define EQOS_RDESC3_WD		0x00400000
#define EQOS_RDESC3_OF		0x00200000
#define EQOS_RDESC3_RE		0x00100000
#define EQOS_RDESC3_DRIB	0x00080000
#define EQOS_RDESC3_LT		0x00070000
#define EQOS_RDESC3_ES		0x00008000
#define EQOS_RDESC3_PL		0x00007FFF

/* err bits which make up err summary */
#define EQOS_RDESC3_ES_BITS \
		(EQOS_RDESC3_CRC | EQOS_RDESC3_GP | EQOS_RDESC3_WD | \
		EQOS_RDESC3_OF | EQOS_RDESC3_RE | EQOS_RDESC3_DRIB)

#define EQOS_RDESC2_HL	0x000003FF

#define EQOS_RDESC1_PT		0x00000007 /* Payload type */
#define EQOS_RDESC1_PT_TCP	0x00000002 /* Payload type = TCP */

/* Maximum size of pkt that is copied to a new buffer on receive */
#define EQOS_COPYBREAK_DEFAULT 256
#define EQOS_SYSCLOCK	62500000 /* System clock is 62.5MHz */
#define EQOS_SYSTIMEPERIOD	16 /* System time period is 16ns */

#define EQOS_TX_QUEUE_CNT (pdata->tx_queue_cnt)
#define EQOS_RX_QUEUE_CNT (pdata->rx_queue_cnt)
#define EQOS_QUEUE_CNT min(EQOS_TX_QUEUE_CNT, EQOS_RX_QUEUE_CNT)

/* Helper macros for TX descriptor handling */

#define GET_TX_QUEUE_PTR(qinx) (&(pdata->tx_queue[(qinx)]))

#define GET_TX_DESC_PTR(qinx, dinx)\
	(pdata->tx_queue[(qinx)].ptx_ring.tx_desc_ptrs[(dinx)])

#define GET_TX_DESC_DMA_ADDR(qinx, dinx)\
	(pdata->tx_queue[(qinx)].ptx_ring.tx_desc_dma_addrs[(dinx)])

#define GET_TX_WRAPPER_DESC(qinx) (&(pdata->tx_queue[(qinx)].ptx_ring))

#define GET_TX_BUF_PTR(qinx, dinx)\
	(pdata->tx_queue[(qinx)].ptx_ring.tx_buf_ptrs[(dinx)])

#define INCR_TX_DESC_INDEX(inx, offset) do {\
	(inx) += (offset);\
	if ((inx) >= TX_DESC_CNT)\
		(inx) = ((inx) - TX_DESC_CNT);\
} while (0)

#define DECR_TX_DESC_INDEX(inx) do {\
  (inx)--;\
  if ((inx) < 0)\
    (inx) = (TX_DESC_CNT + (inx));\
} while (0)

#define INCR_TX_LOCAL_INDEX(inx, offset)\
	(((inx) + (offset)) >= TX_DESC_CNT ?\
	((inx) + (offset) - TX_DESC_CNT) : ((inx) + (offset)))

/* Helper macros for RX descriptor handling */

#define GET_RX_QUEUE_PTR(qinx) (&(pdata->rx_queue[(qinx)]))

#define GET_RX_DESC_PTR(qinx, dinx)\
	(pdata->rx_queue[(qinx)].prx_ring.rx_desc_ptrs[(dinx)])

#define GET_RX_DESC_DMA_ADDR(qinx, dinx)\
	(pdata->rx_queue[(qinx)].prx_ring.rx_desc_dma_addrs[(dinx)])

#define GET_RX_WRAPPER_DESC(qinx) (&(pdata->rx_queue[(qinx)].prx_ring))

#define GET_RX_BUF_PTR(qinx, dinx)\
	(pdata->rx_queue[(qinx)].prx_ring.rx_buf_ptrs[(dinx)])

#define INCR_RX_DESC_INDEX(inx, offset) do {\
  (inx) += (offset);\
  if ((inx) >= RX_DESC_CNT)\
    (inx) = ((inx) - RX_DESC_CNT);\
} while (0)

#define DECR_RX_DESC_INDEX(inx) do {\
	(inx)--;\
	if ((inx) < 0)\
		(inx) = (RX_DESC_CNT + (inx));\
} while (0)

#define INCR_RX_LOCAL_INDEX(inx, offset)\
	(((inx) + (offset)) >= RX_DESC_CNT ?\
	((inx) + (offset) - RX_DESC_CNT) : ((inx) + (offset)))

#define GET_CURRENT_RCVD_DESC_CNT(qinx)\
	(pdata->rx_queue[(qinx)].prx_ring.pkt_received)

#define GET_CURRENT_RCVD_LAST_DESC_INDEX(start_index, offset) (RX_DESC_CNT - 1)

#define GET_TX_DESC_IDX(qinx, desc)\
	(((desc) - GET_TX_DESC_DMA_ADDR((qinx), 0))/(sizeof(struct s_tx_desc)))

#define GET_RX_DESC_IDX(qinx, desc)\
	 (((desc) - GET_RX_DESC_DMA_ADDR((qinx), 0))/(sizeof(struct s_rx_desc)))

/* Helper macro for handling coalesce parameters via ethtool */
/* Obtained by trial and error  */
#define EQOS_OPTIMAL_DMA_RIWT_USEC  124
/* Max delay before RX interrupt after a pkt is received Max
 * delay in usecs is 1020 for 62.5MHz device clock */
#define EQOS_MAX_DMA_RIWT  0xff
/* Max no of pkts to be received before an RX interrupt */
#define EQOS_RX_MAX_FRAMES 16

#define DMA_SBUS_AXI_PBL_MASK 0xFE

/* Helper macros for handling receive error */
#define EQOS_RX_LENGTH_ERR        0x00000001
#define EQOS_RX_BUF_OVERFLOW_ERR  0x00000002
#define EQOS_RX_CRC_ERR           0x00000004
#define EQOS_RX_FRAME_ERR         0x00000008
#define EQOS_RX_FIFO_OVERFLOW_ERR 0x00000010
#define EQOS_RX_MISSED_PKT_ERR    0x00000020

#define EQOS_RX_CHECKSUM_DONE 0x00000001
#define EQOS_RX_VLAN_PKT      0x00000002

/* MAC Time stamp contorl reg bit fields */
#define MAC_TCR_TSENA         0x00000001 /* Enable timestamp */
#define MAC_TCR_TSCFUPDT      0x00000002 /* Enable Fine Timestamp Update */
#define MAC_TCR_TSENALL       0x00000100 /* Enable timestamping for all packets */
#define MAC_TCR_TSCTRLSSR     0x00000200 /* Enable Timestamp Digitla Contorl (1ns accuracy )*/
#define MAC_TCR_TSVER2ENA     0x00000400 /* Enable PTP packet processing for Version 2 Formate */
#define MAC_TCR_TSIPENA       0x00000800 /* Enable processing of PTP over Ethernet Packets */
#define MAC_TCR_TSIPV6ENA     0x00001000 /* Enable processing of PTP Packets sent over IPv6-UDP Packets */
#define MAC_TCR_TSIPV4ENA     0x00002000 /* Enable processing of PTP Packets sent over IPv4-UDP Packets */
#define MAC_TCR_TSEVENTENA    0x00004000 /* Enable Timestamp Snapshot for Event Messages */
#define MAC_TCR_TSMASTERENA   0x00008000 /* Enable snapshot for Message Relevent to Master */
#define MAC_TCR_SNAPTYPSEL_1  0x00010000 /* select PTP packets for taking snapshots */
#define MAC_TCR_SNAPTYPSEL_2  0x00020000
#define MAC_TCR_SNAPTYPSEL_3  0x00030000
#define MAC_TCR_AV8021ASMEN   0x10000000 /* Enable AV 802.1AS Mode */

/* PTP Offloading control register bits (MAC_PTO_control)*/
#define MAC_PTOCR_PTOEN		  0x00000001 /* PTP offload Enable */
#define MAC_PTOCR_ASYNCEN	  0x00000002 /* Automatic PTP Sync message enable */
#define MAC_PTOCR_APDREQEN	  0x00000004 /* Automatic PTP Pdelay_Req message enable */


/* Hash Table Reg count */
#define EQOS_HTR_CNT (pdata->max_hash_table_size/32)

/* For handling VLAN filtering */
#define EQOS_VLAN_PERFECT_FILTERING 0
#define EQOS_VLAN_HASH_FILTERING 1

/* Enabling perfect filter for L2. If disabled has filter will be used */
#define ENABLE_PERFECT_L2_FILTER

/* For handling differnet PHY interfaces */
#define EQOS_GMII_MII	0x0
#define EQOS_RGMII	0x1
#define EQOS_SGMII	0x2
#define EQOS_TBI		0x3
#define EQOS_RMII	0x4
#define EQOS_RTBI	0x5
#define EQOS_SMII	0x6
#define EQOS_REV_MII	0x7

/* for EEE */
#define EQOS_DEFAULT_LPI_LS_TIMER 0x3E8 /* 1000 in decimal */
#define EQOS_DEFAULT_LPI_TWT_TIMER 0x0

#define EQOS_DEFAULT_LPI_TIMER 1000 /* LPI Tx local expiration time in msec */
#define EQOS_LPI_TIMER(x) (jiffies + msecs_to_jiffies(x))

/* Error and status macros defined below */

#define E_DMA_SR_TPS        6
#define E_DMA_SR_TBU        7
#define E_DMA_SR_RBU        8
#define E_DMA_SR_RPS        9
#define S_DMA_SR_RWT        2
#define E_DMA_SR_FBE       10
#define S_MAC_ISR_PMTIS     11

/* IRQs for eqos device.  This must match what is in our DT. */
#define IRQ_COMMON_IDX		0
#define IRQ_POWER_IDX		1
#define IRQ_CHAN0_RX_IDX	2
#define IRQ_CHAN0_TX_IDX	3
#define IRQ_CHAN1_RX_IDX	4
#define IRQ_CHAN1_TX_IDX	5
#define IRQ_CHAN2_RX_IDX	6
#define IRQ_CHAN2_TX_IDX	7
#define IRQ_CHAN3_RX_IDX	8
#define IRQ_CHAN3_TX_IDX	9
#define IRQ_MAX_IDX		9

/* Tx DMA States */
#define DMA_TX_STATE_STOPPED	0
#define DMA_TX_STATE_SUSPENDED	6

/* Rx DMA States */
#define DMA_RX_STATE_STOPPED	0
#define DMA_RX_STATE_IDLE	3
#define DMA_RX_STATE_SUSPENDED	4

/* C data types typedefs */

typedef unsigned short BOOL;
typedef char CHAR;
typedef char *CHARP;
typedef double DOUBLE;
typedef double *DOUBLEP;
typedef float FLOAT;
typedef float *FLOATP;
typedef int INT;
typedef int *INTP;
typedef long LONG;
typedef long *LONGP;
typedef short SHORT;
typedef short *SHORTP;
typedef unsigned UINT;
typedef unsigned *UINTP;
typedef unsigned char UCHAR;
typedef unsigned char *UCHARP;
typedef unsigned long ULONG;
typedef unsigned long *ULONGP;
typedef unsigned long long ULONG_LONG;
typedef unsigned short USHORT;
typedef unsigned short *USHORTP;
typedef void VOID;
typedef void *VOIDP;

struct s_rx_context_desc {
	UINT rdes0;
	UINT rdes1;
	UINT rdes2;
	UINT rdes3;
};

typedef struct s_rx_context_desc t_rx_context_desc;

struct s_tx_context_desc {
	UINT tdes0;
	UINT tdes1;
	UINT tdes2;
	UINT tdes3;
};

typedef struct s_tx_context_desc t_tx_context_desc;

struct s_rx_desc {
	UINT rdes0;
	UINT rdes1;
	UINT rdes2;
	UINT rdes3;
};

typedef struct s_rx_desc t_rx_desc;

struct s_tx_desc {
	UINT tdes0;
	UINT tdes1;
	UINT tdes2;
	UINT tdes3;
};

typedef struct s_tx_desc t_tx_desc;

struct s_tx_error_counters {
	UINT tx_errors;
};

typedef struct s_tx_error_counters t_tx_error_counters;

struct s_rx_error_counters {
	UINT rx_errors;
};

typedef struct s_rx_error_counters t_rx_error_counters;

struct s_rx_pkt_features {
	UINT pkt_attributes;
	UINT vlan_tag;
};

typedef struct s_rx_pkt_features t_rx_pkt_features;

struct s_tx_pkt_features {
	UINT pkt_attributes;
	UINT vlan_tag;
	ULONG mss;
	ULONG hdr_len;
	ULONG pay_len;
	UCHAR ipcss;
	UCHAR ipcso;
	USHORT ipcse;
	UCHAR tucss;
	UCHAR tucso;
	USHORT tucse;
	UINT pkt_type;
	ULONG tcp_hdr_len;
	uint desc_cnt;
};

typedef struct s_tx_pkt_features t_tx_pkt_features;

typedef enum {
	EQOS_DMA_ISR_DC0IS,
	EQOS_DMA_SR0_TI,
	EQOS_DMA_SR0_TPS,
	EQOS_DMA_SR0_TBU,
	EQOS_DMA_SR0_RI,
	EQOS_DMA_SR0_RBU,
	EQOS_DMA_SR0_RPS,
	EQOS_DMA_SR0_FBE,
	EQOS_ALL
} e_eqos_int_id;

typedef enum {
	EQOS_256 = 0x0,
	EQOS_512 = 0x1,
	EQOS_1K = 0x3,
	EQOS_2K = 0x7,
	EQOS_4K = 0xf,
	EQOS_8K = 0x1f,
	EQOS_16K = 0x3f,
	EQOS_32K = 0x7f
} eqos_mtl_fifo_size;

/* do forward declaration of private data structure */
struct eqos_prv_data;
struct tx_ring;

struct hw_if_struct {

	INT (*tx_complete)(struct s_tx_desc *);
	INT (*tx_window_error)(struct s_tx_desc *);
	INT (*tx_aborted_error)(struct s_tx_desc *);
	INT (*tx_carrier_lost_error)(struct s_tx_desc *);
	INT (*tx_fifo_underrun)(struct s_tx_desc *);
	INT (*tx_get_collision_count)(struct s_tx_desc *);
	INT (*tx_handle_aborted_error)(struct s_tx_desc *);
	INT (*tx_update_fifo_threshold)(struct s_tx_desc *);
	/*tx threshold config */
	INT(*tx_config_threshold) (UINT);

	INT(*set_promiscuous_mode) (VOID);
	INT(*set_all_multicast_mode) (VOID);
	INT(*set_multicast_list_mode) (VOID);
	INT(*set_unicast_mode) (VOID);

	INT(*enable_rx_csum) (void);
	INT(*disable_rx_csum) (void);
	INT(*get_rx_csum_status) (void);

	INT(*read_phy_regs) (INT, INT, INT*);
	INT(*write_phy_regs) (INT, INT, INT);
	INT(*set_full_duplex) (VOID);
	INT(*set_half_duplex) (VOID);
	INT(*set_mii_speed_100) (struct eqos_prv_data *);
	INT(*set_mii_speed_10) (struct eqos_prv_data *);
	INT(*set_gmii_speed) (struct eqos_prv_data *);
	INT (*set_tx_clk_speed) (struct eqos_prv_data *, INT);
	/* for PMT */
	INT(*start_dma_rx) (UINT);
	INT(*stop_dma_rx) (UINT);
	INT(*start_dma_tx) (UINT);
	INT(*stop_dma_tx) (struct eqos_prv_data *, UINT);
	INT(*start_mac_tx_rx) (VOID);
	INT(*stop_mac_tx) (VOID);
	INT(*stop_mac_rx) (VOID);

	INT(*init) (struct eqos_prv_data *);
	INT(*exit) (void);
	INT(*car_reset) (struct eqos_prv_data *);
	INT(*pad_calibrate) (struct eqos_prv_data *);
	void(*disable_pad_cal) (struct eqos_prv_data *);
	void(*read_err_counter) (struct eqos_prv_data *, bool);
	INT(*enable_int) (e_eqos_int_id);
	INT(*disable_int) (e_eqos_int_id);
	void (*pre_xmit) (struct eqos_prv_data *, UINT qinx);
	void (*dev_read) (struct eqos_prv_data *, UINT qinx);
	void (*tx_desc_init) (struct eqos_prv_data *, UINT qinx);
	void (*rx_desc_init) (struct eqos_prv_data *, UINT qinx);
	void (*rx_desc_reset) (UINT, struct eqos_prv_data *,
			       UINT, UINT qinx);
	 INT(*tx_desc_reset) (UINT, struct eqos_prv_data *, UINT qinx);
	/* last tx segmnet reports the tx status */
	 INT (*get_tx_desc_ls)(struct s_tx_desc *);
	 INT (*get_tx_desc_ctxt)(struct s_tx_desc *);
	void (*update_rx_tail_ptr) (unsigned int qinx, unsigned int dma_addr);

	/* for FLOW ctrl */
	 INT(*enable_rx_flow_ctrl) (VOID);
	 INT(*disable_rx_flow_ctrl) (VOID);
	 INT(*enable_tx_flow_ctrl) (UINT);
	 INT(*disable_tx_flow_ctrl) (UINT);

	/* for PMT operations */
	 INT(*enable_magic_pmt) (VOID);
	 INT(*disable_magic_pmt) (VOID);
	 INT(*enable_remote_pmt) (VOID);
	 INT(*disable_remote_pmt) (VOID);
	 INT(*configure_rwk_filter) (UINT *, UINT);

	/* for RX watchdog timer */
	 INT(*config_rx_watchdog) (UINT, u32 riwt);

	/* for RX and TX threshold config */
	 INT(*config_rx_threshold) (UINT ch_no, UINT val);
	 INT(*config_tx_threshold) (UINT ch_no, UINT val);

	/* for RX and TX Store and Forward Mode config */
	 INT(*config_rsf_mode) (UINT ch_no, UINT val);
	 INT(*config_tsf_mode) (UINT ch_no, UINT val);

	/* for TX DMA Operate on Second Frame config */
	 INT(*config_osf_mode) (UINT ch_no, UINT val);

	/* for INCR/INCRX config */
	 INT(*config_incr_incrx_mode) (UINT val);
	/* for AXI PBL config */
	INT(*config_axi_pbl_val) (UINT val);
	/* for AXI WORL config */
	INT(*config_axi_worl_val) (UINT val);
	/* for AXI RORL config */
	INT(*config_axi_rorl_val) (UINT val);

	/* for RX and TX PBL config */
	 INT(*config_rx_pbl_val) (UINT ch_no, UINT val);
	 INT(*get_rx_pbl_val) (UINT ch_no);
	 INT(*config_tx_pbl_val) (UINT ch_no, UINT val);
	 INT(*get_tx_pbl_val) (UINT ch_no);
	 INT(*config_pblx8) (UINT ch_no, UINT val);

	/* for TX vlan control */
	 VOID (*enable_vlan_reg_control)(struct tx_ring *ptx_ring);
	 VOID (*enable_vlan_desc_control)(struct eqos_prv_data *pdata);

	/* for rx vlan stripping */
//	 VOID(*config_rx_outer_vlan_stripping) (u32);
//	 VOID(*config_rx_inner_vlan_stripping) (u32);

	/* for sa(source address) insert/replace */
	 VOID(*configure_mac_addr0_reg) (UCHAR *);
	 VOID(*configure_mac_addr1_reg) (UCHAR *);
	 VOID(*configure_sa_via_reg) (u32);

	/* for handling multi-queue */
	INT(*disable_rx_interrupt)(UINT, struct eqos_prv_data *);
	INT(*enable_rx_interrupt)(UINT, struct eqos_prv_data *);
	INT (*disable_chan_interrupts)(UINT, struct eqos_prv_data *);
	INT (*enable_chan_interrupts)(UINT, struct eqos_prv_data *);

	/* for handling MMC */
	INT(*disable_mmc_interrupts)(VOID);
	INT(*config_mmc_counters)(VOID);

	/* for handling DCB and AVB */
	INT(*set_dcb_algorithm)(UCHAR dcb_algorithm);
	INT(*set_dcb_queue_weight)(UINT qinx, UINT q_weight);

	INT(*set_tx_queue_operating_mode)(UINT qinx, UINT q_mode);
	INT(*set_avb_algorithm)(UINT qinx, UCHAR avb_algorithm);
	INT(*config_credit_control)(UINT qinx, UINT cc);
	INT(*config_send_slope)(UINT qinx, UINT send_slope);
	INT(*config_idle_slope)(UINT qinx, UINT idle_slope);
	INT(*config_high_credit)(UINT qinx, UINT hi_credit);
	INT(*config_low_credit)(UINT qinx, UINT lo_credit);
	INT(*config_slot_num_check)(UINT qinx, UCHAR slot_check);
	INT(*config_advance_slot_num_check)(UINT qinx, UCHAR adv_slot_check);

	/* for hw time stamping */
	INT(*config_hw_time_stamping)(UINT);
	INT(*config_sub_second_increment)(unsigned long ptp_clock);
	INT(*init_systime)(UINT, UINT);
	INT(*config_addend)(UINT);
	INT(*adjust_systime)(UINT, UINT, INT, bool);
	ULONG_LONG(*get_systime)(void);
	UINT (*get_tx_tstamp_status)(struct s_tx_desc *txdesc);
	ULONG_LONG (*get_tx_tstamp)(struct s_tx_desc *txdesc);
	UINT (*get_tx_tstamp_status_via_reg)(void);
	ULONG_LONG(*get_tx_tstamp_via_reg)(void);
	UINT (*rx_tstamp_available)(struct s_rx_desc *rxdesc);
	UINT(*get_rx_tstamp_status)(struct s_rx_context_desc *rxdesc);
	ULONG_LONG(*get_rx_tstamp)(struct s_rx_context_desc *rxdesc);
	INT(*drop_tx_status_enabled)(void);

	/* for l2, l3 and l4 layer filtering */
	INT(*config_l2_da_perfect_inverse_match)(INT perfect_inverse_match);
	INT(*update_mac_addr32_127_low_high_reg)(INT idx, UCHAR addr[]);
	INT(*update_mac_addr1_31_low_high_reg)(INT idx, UCHAR addr[]);
	INT(*update_hash_table_reg)(INT idx, UINT data);
	INT(*config_mac_pkt_filter_reg)(UCHAR, UCHAR, UCHAR, UCHAR, UCHAR);
	INT(*config_l3_l4_filter_enable)(INT);
	INT(*config_l3_filters)(INT filter_no, INT enb_dis, INT ipv4_ipv6_match,
                     INT src_dst_addr_match, INT perfect_inverse_match);
	INT(*update_ip4_addr0)(INT filter_no, UCHAR addr[]);
	INT(*update_ip4_addr1)(INT filter_no, UCHAR addr[]);
	INT(*update_ip6_addr)(INT filter_no, USHORT addr[]);
	INT(*config_l4_filters)(INT filter_no, INT enb_dis,
		INT tcp_udp_match, INT src_dst_port_match,
		INT perfect_inverse_match);
	INT(*update_l4_sa_port_no)(INT filter_no, USHORT port_no);
	INT(*update_l4_da_port_no)(INT filter_no, USHORT port_no);

	/* for VLAN filtering */
	INT(*get_vlan_hash_table_reg)(void);
	INT(*update_vlan_hash_table_reg)(USHORT data);
	INT(*update_vlan_id)(USHORT vid);
	INT(*config_vlan_filtering)(INT filter_enb_dis,
				INT perfect_hash_filtering,
				INT perfect_inverse_match);
    INT(*config_mac_for_vlan_pkt)(void);
	UINT(*get_vlan_tag_comparison)(void);

	/* for differnet PHY interconnect */
	INT(*control_an)(bool enable, bool restart);
	INT(*get_an_adv_pause_param)(void);
	INT(*get_an_adv_duplex_param)(void);
	INT(*get_lp_an_adv_pause_param)(void);
	INT(*get_lp_an_adv_duplex_param)(void);

	/* for EEE */
	INT(*set_eee_mode)(void);
	INT(*reset_eee_mode)(void);
	INT(*set_eee_pls)(int phy_link);
	INT(*set_eee_timer)(int lpi_lst, int lpi_twt);
	u32(*get_lpi_status)(void);
	INT(*set_lpi_tx_automate)(void);

	/* for ARP */
	INT(*config_arp_offload)(int enb_dis);
	INT(*update_arp_offload_ip_addr)(UCHAR addr[]);

	/* for MAC loopback */
	INT(*config_mac_loopback_mode)(UINT);

	/* for MAC Double VLAN Processing config */
	VOID(*config_rx_outer_vlan_stripping)(u32);
	VOID(*config_rx_inner_vlan_stripping)(u32);

	/* for PFC */
	void (*config_pfc)(int enb_dis);

    /* for PTP offloading */
	VOID(*config_ptpoffload_engine)(UINT, UINT);

	/* for PTP channel configuration */
	VOID(*config_ptp_channel)(UINT, UINT);
};

/* wrapper buffer structure to hold transmit pkt details */
struct tx_swcx_desc {
	dma_addr_t dma;		/* dma address of skb */
	struct sk_buff *skb;	/* virtual address of skb */
	unsigned short len;	/* length of fragment */
	unsigned char buf1_mapped_as_page;
};

struct tx_ring {
	char *desc_name;	/* ID of descriptor */

	void *tx_desc_ptrs[TX_DESC_CNT];
	dma_addr_t tx_desc_dma_addrs[TX_DESC_CNT];

	struct tx_swcx_desc *tx_buf_ptrs[TX_DESC_CNT];

	unsigned char contigous_mem;

	int cur_tx;	/* always gives index of desc which has to
				be used for current xfer */
	int dirty_tx;	/* always gives index of desc which has to
				be checked for xfer complete */
	unsigned int free_desc_cnt;	/* always gives total number of available
					free desc count for driver */
	unsigned int tx_pkt_queued;	/* always gives total number of packets
					queued for transmission */
	unsigned int queue_stopped;

	UINT tx_threshold_val;	/* contain bit value for TX threshold */
	UINT tsf_on;		/* set to 1 if TSF is enabled else set to 0 */
	UINT osf_on;		/* set to 1 if OSF is enabled else set to 0 */
	UINT tx_pbl;

	/* for tx vlan delete/insert/replace */
	u32 tx_vlan_tag_via_reg;
	u32 tx_vlan_tag_ctrl;

	USHORT vlan_tag_id;
	UINT vlan_tag_present;

	/* for VLAN context descriptor operation */
	u32 context_setup;

	/* for TSO */
	u32 default_mss;
};

struct eqos_tx_queue {
	/* Tx descriptors */
	struct tx_ring ptx_ring;
	int q_op_mode;
};

/* wrapper buffer structure to hold received pkt details */
struct rx_swcx_desc {
	dma_addr_t dma;		/* dma address of skb */
	struct sk_buff *skb;	/* virtual address of skb */
	unsigned short len;	/* length of received packet */
	struct page *page;	/* page address */
	unsigned char mapped_as_page;
	bool good_pkt;		/* set to 1 if it is good packet else
				set to 0 */
	unsigned int inte;	/* set to non-zero if INTE is set for
				corresponding desc */
};

struct rx_ring {
	char *desc_name;	/* ID of descriptor */

	void *rx_desc_ptrs[RX_DESC_CNT];
	dma_addr_t rx_desc_dma_addrs[RX_DESC_CNT];

	struct rx_swcx_desc *rx_buf_ptrs[RX_DESC_CNT];

	unsigned char contigous_mem;

	int cur_rx;	/* always gives index of desc which needs to
				be checked for packet availabilty */
	int dirty_rx;
	unsigned int pkt_received;	/* always gives total number of packets
					received from device in one RX interrupt */
	unsigned int skb_realloc_idx;
	unsigned int skb_realloc_threshold;

	/* for rx coalesce schem */
	int use_riwt;		/* set to 1 if RX watchdog timer should be used
				for RX interrupt mitigation */
	u32 rx_riwt;
	u32 rx_coal_frames;	/* Max no of pkts to be received before
				an RX interrupt */

	UINT rx_threshold_val;	/* contain bit vlaue for RX threshold */
	UINT rsf_on;		/* set to 1 if RSF is enabled else set to 0 */
	UINT rx_pbl;

	/* for rx vlan stripping */
	u32 rx_inner_vlan_strip;
	u32 rx_outer_vlan_strip;

	u32 hw_last_rx_desc_addr;
};

struct eqos_rx_queue {
	/* Rx descriptors */
	struct rx_ring prx_ring;
	struct napi_struct napi;
	struct eqos_prv_data *pdata;
	uint	chan_num;

	struct net_lro_mgr lro_mgr;
	struct net_lro_desc lro_arr[EQOS_MAX_LRO_DESC];
	int lro_flush_needed;
};

struct desc_if_struct {
	INT(*alloc_queue_struct) (struct eqos_prv_data *);
	void(*free_queue_struct) (struct eqos_prv_data *);
	INT(*alloc_buff_and_desc) (struct eqos_prv_data *);
	void(*free_buff_and_desc) (struct eqos_prv_data *);
	void (*realloc_skb) (struct eqos_prv_data *, UINT);
	void (*unmap_rx_skb) (struct eqos_prv_data *,
			      struct rx_swcx_desc *);
	void (*tx_swcx_free)(struct eqos_prv_data *, struct tx_swcx_desc *);
	int (*tx_swcx_alloc)(struct net_device *, struct sk_buff *);
	void (*tx_free_mem) (struct eqos_prv_data *);
	void (*rx_free_mem) (struct eqos_prv_data *);
	void (*wrapper_tx_desc_init) (struct eqos_prv_data *);
	void (*wrapper_tx_desc_init_single_q) (struct eqos_prv_data *,
					       UINT);
	void (*wrapper_rx_desc_init) (struct eqos_prv_data *);
	void (*wrapper_rx_desc_init_single_q) (struct eqos_prv_data *,
					       UINT);

	void (*rx_skb_free_mem) (struct eqos_prv_data *, UINT);
	void (*rx_skb_free_mem_single_q) (struct eqos_prv_data *, UINT);
	void (*tx_skb_free_mem) (struct eqos_prv_data *, UINT);
	void (*tx_skb_free_mem_single_q) (struct eqos_prv_data *, UINT);

	int (*handle_tso) (struct net_device *dev, struct sk_buff *skb);
};

/*
 * This structure contains different flags and each flags will indicates
 * different hardware features.
 */
struct eqos_hw_features {
	/* HW Feature Register0 */
	unsigned int mii_sel;	/* 10/100 Mbps support */
	unsigned int gmii_sel;	/* 1000 Mbps support */
	unsigned int hd_sel;	/* Half-duplex support */
	unsigned int pcs_sel;	/* PCS registers(TBI, SGMII or RTBI PHY
				interface) */
	unsigned int vlan_hash_en;	/* VLAN Hash filter selected */
	unsigned int sma_sel;	/* SMA(MDIO) Interface */
	unsigned int rwk_sel;	/* PMT remote wake-up packet */
	unsigned int mgk_sel;	/* PMT magic packet */
	unsigned int mmc_sel;	/* RMON module */
	unsigned int arp_offld_en;	/* ARP Offload features is selected */
	unsigned int ts_sel;	/* IEEE 1588-2008 Adavanced timestamp */
	unsigned int eee_sel;	/* Energy Efficient Ethernet is enabled */
	unsigned int tx_coe_sel;	/* Tx Checksum Offload is enabled */
	unsigned int rx_coe_sel;	/* Rx Checksum Offload is enabled */
	unsigned int mac_addr16_sel;	/* MAC Addresses 1-16 are selected */
	unsigned int mac_addr32_sel;	/* MAC Addresses 32-63 are selected */
	unsigned int mac_addr64_sel;	/* MAC Addresses 64-127 are selected */
	unsigned int tsstssel;	/* Timestamp System Time Source */
	unsigned int speed_sel;	/* Speed Select */
	unsigned int sa_vlan_ins;	/* Source Address or VLAN Insertion */
	unsigned int act_phy_sel;	/* Active PHY Selected */

	/* HW Feature Register1 */
	unsigned int rx_fifo_size;	/* MTL Receive FIFO Size */
	unsigned int tx_fifo_size;	/* MTL Transmit FIFO Size */
	unsigned int adv_ts_hword;	/* Advance timestamping High Word
					selected */
	unsigned int dcb_en;	/* DCB Feature Enable */
	unsigned int sph_en;	/* Split Header Feature Enable */
	unsigned int tso_en;	/* TCP Segmentation Offload Enable */
	unsigned int dma_debug_gen;	/* DMA debug registers are enabled */
	unsigned int av_sel;	/* AV Feature Enabled */
	unsigned int lp_mode_en;	/* Low Power Mode Enabled */
	unsigned int hash_tbl_sz;	/* Hash Table Size */
	unsigned int l3l4_filter_num;	/* Total number of L3-L4 Filters */

	/* HW Feature Register2 */
	unsigned int rx_q_cnt;	/* Number of MTL Receive Queues */
	unsigned int tx_q_cnt;	/* Number of MTL Transmit Queues */
	unsigned int rx_ch_cnt;	/* Number of DMA Receive Channels */
	unsigned int tx_ch_cnt;	/* Number of DMA Transmit Channels */
	unsigned int pps_out_num;	/* Number of PPS outputs */
	unsigned int aux_snap_num;	/* Number of Auxillary snapshot
					inputs */
};

/* structure to hold MMC values */
struct eqos_mmc_counters {
	/* MMC TX counters */
	unsigned long mmc_tx_octetcount_gb;
	unsigned long mmc_tx_framecount_gb;
	unsigned long mmc_tx_broadcastframe_g;
	unsigned long mmc_tx_multicastframe_g;
	unsigned long mmc_tx_64_octets_gb;
	unsigned long mmc_tx_65_to_127_octets_gb;
	unsigned long mmc_tx_128_to_255_octets_gb;
	unsigned long mmc_tx_256_to_511_octets_gb;
	unsigned long mmc_tx_512_to_1023_octets_gb;
	unsigned long mmc_tx_1024_to_max_octets_gb;
	unsigned long mmc_tx_unicast_gb;
	unsigned long mmc_tx_multicast_gb;
	unsigned long mmc_tx_broadcast_gb;
	unsigned long mmc_tx_underflow_error;
	unsigned long mmc_tx_underflow_error_pre_recalib;
	unsigned long mmc_tx_singlecol_g;
	unsigned long mmc_tx_multicol_g;
	unsigned long mmc_tx_deferred;
	unsigned long mmc_tx_latecol;
	unsigned long mmc_tx_exesscol;
	unsigned long mmc_tx_carrier_error;
	unsigned long mmc_tx_carrier_error_pre_recalib;
	unsigned long mmc_tx_octetcount_g;
	unsigned long mmc_tx_framecount_g;
	unsigned long mmc_tx_excessdef;
	unsigned long mmc_tx_excessdef_pre_recalib;
	unsigned long mmc_tx_pause_frame;
	unsigned long mmc_tx_vlan_frame_g;
	unsigned long mmc_tx_osize_frame_g;

	/* MMC RX counters */
	unsigned long mmc_rx_framecount_gb;
	unsigned long mmc_rx_octetcount_gb;
	unsigned long mmc_rx_octetcount_g;
	unsigned long mmc_rx_broadcastframe_g;
	unsigned long mmc_rx_multicastframe_g;
	unsigned long mmc_rx_crc_errror;
	unsigned long mmc_rx_crc_errror_pre_recalib;
	unsigned long mmc_rx_align_error;
	unsigned long mmc_rx_align_error_pre_recalib;
	unsigned long mmc_rx_run_error;
	unsigned long mmc_rx_run_error_pre_recalib;
	unsigned long mmc_rx_jabber_error;
	unsigned long mmc_rx_jabber_error_pre_recalib;
	unsigned long mmc_rx_undersize_g;
	unsigned long mmc_rx_oversize_g;
	unsigned long mmc_rx_64_octets_gb;
	unsigned long mmc_rx_65_to_127_octets_gb;
	unsigned long mmc_rx_128_to_255_octets_gb;
	unsigned long mmc_rx_256_to_511_octets_gb;
	unsigned long mmc_rx_512_to_1023_octets_gb;
	unsigned long mmc_rx_1024_to_max_octets_gb;
	unsigned long mmc_rx_unicast_g;
	unsigned long mmc_rx_length_error;
	unsigned long mmc_rx_length_error_pre_recalib;
	unsigned long mmc_rx_outofrangetype;
	unsigned long mmc_rx_outofrangetype_pre_recalib;
	unsigned long mmc_rx_pause_frames;
	unsigned long mmc_rx_fifo_overflow;
	unsigned long mmc_rx_fifo_overflow_pre_recalib;
	unsigned long mmc_rx_vlan_frames_gb;
	unsigned long mmc_rx_watchdog_error;
	unsigned long mmc_rx_watchdog_error_pre_recalib;
	unsigned long mmc_rx_receive_error;
	unsigned long mmc_rx_receive_error_pre_recalib;
	unsigned long mmc_rx_ctrl_frames_g;

	/* IPC */
	unsigned long mmc_rx_ipc_intr_mask;
	unsigned long mmc_rx_ipc_intr;

	/* IPv4 */
	unsigned long mmc_rx_ipv4_gd;
	unsigned long mmc_rx_ipv4_hderr;
	unsigned long mmc_rx_ipv4_hderr_pre_recalib;
	unsigned long mmc_rx_ipv4_nopay;
	unsigned long mmc_rx_ipv4_frag;
	unsigned long mmc_rx_ipv4_udsbl;

	/* IPV6 */
	unsigned long mmc_rx_ipv6_gd_octets;
	unsigned long mmc_rx_ipv6_hderr_octets;
	unsigned long mmc_rx_ipv6_hderr_octets_pre_recalib;
	unsigned long mmc_rx_ipv6_nopay_octets;

	/* Protocols */
	unsigned long mmc_rx_udp_gd;
	unsigned long mmc_rx_udp_err;
	unsigned long mmc_rx_udp_err_pre_recalib;
	unsigned long mmc_rx_tcp_gd;
	unsigned long mmc_rx_tcp_err;
	unsigned long mmc_rx_tcp_err_pre_recalib;
	unsigned long mmc_rx_icmp_gd;
	unsigned long mmc_rx_icmp_err;
	unsigned long mmc_rx_icmp_err_pre_recalib;

	/* IPv4 */
	unsigned long mmc_rx_ipv4_gd_octets;
	unsigned long mmc_rx_ipv4_hderr_octets;
	unsigned long mmc_rx_ipv4_hderr_octets_pre_recalib;
	unsigned long mmc_rx_ipv4_nopay_octets;
	unsigned long mmc_rx_ipv4_frag_octets;
	unsigned long mmc_rx_ipv4_udsbl_octets;

	/* IPV6 */
	unsigned long mmc_rx_ipv6_gd;
	unsigned long mmc_rx_ipv6_hderr;
	unsigned long mmc_rx_ipv6_hderr_pre_recalib;
	unsigned long mmc_rx_ipv6_nopay;

	/* Protocols */
	unsigned long mmc_rx_udp_gd_octets;
	unsigned long mmc_rx_udp_err_octets;
	unsigned long mmc_rx_udp_err_octets_pre_recalib;
	unsigned long mmc_rx_tcp_gd_octets;
	unsigned long mmc_rx_tcp_err_octets;
	unsigned long mmc_rx_tcp_err_octets_pre_recalib;
	unsigned long mmc_rx_icmp_gd_octets;
	unsigned long mmc_rx_icmp_err_octets;
	unsigned long mmc_rx_icmp_err_octets_pre_recalib;
};

struct eqos_extra_stats {
	unsigned long q_re_alloc_rx_buf_failed[8];

	/* Tx/Rx IRQ error info */
	unsigned long tx_process_stopped_irq_n[8];
	unsigned long rx_process_stopped_irq_n[8];
	unsigned long tx_buf_unavailable_irq_n[8];
	unsigned long rx_buf_unavailable_irq_n[8];
	unsigned long rx_watchdog_irq_n;
	unsigned long fatal_bus_error_irq_n;
	unsigned long pmt_irq_n;
	/* Tx/Rx IRQ Events */
	unsigned long tx_normal_irq_n[8];
	unsigned long rx_normal_irq_n[8];
	unsigned long napi_poll_n;
	unsigned long tx_clean_n[8];
	/* EEE */
	unsigned long tx_path_in_lpi_mode_irq_n;
	unsigned long tx_path_exit_lpi_mode_irq_n;
	unsigned long rx_path_in_lpi_mode_irq_n;
	unsigned long rx_path_exit_lpi_mode_irq_n;
	/* Tx/Rx frames */
	unsigned long tx_pkt_n;
	unsigned long rx_pkt_n;
	unsigned long tx_vlan_pkt_n;
	unsigned long rx_vlan_pkt_n;
	unsigned long tx_timestamp_captured_n;
	unsigned long rx_timestamp_captured_n;
	unsigned long tx_tso_pkt_n;

	/* Tx/Rx frames per channels/queues */
	unsigned long q_tx_pkt_n[8];
	unsigned long q_rx_pkt_n[8];

	unsigned long link_disconnect_count;
	unsigned long link_connect_count;
	unsigned long temp_pad_recalib_count;
};


struct ptp_cfg_params {
	bool use_tagged_ptp;
	unsigned int ptp_dma_ch_id;
};

typedef enum {
	RXQ_CTRL_NOT_ENABLED,
	RXQ_CTRL_AV,
	RXQ_CTRL_LEGACY,
	RXQ_CTRL_RSVRD,
	RXQ_CTRL_MAX,
} rxq_ctrl_e;

#define RXQ_CTRL_DEFAULT RXQ_CTRL_LEGACY

typedef enum {
	PAUSE_FRAMES_ENABLED,
	PAUSE_FRAMES_DISABLED,
	PAUSE_FRAMES_MAX
} pause_frames_e;
#define PAUSE_FRAMES_DEFAULT PAUSE_FRAMES_ENABLED

#define QUEUE_PRIO_DEFAULT 0
#define QUEUE_PRIO_MAX 7
#define CHAN_NAPI_QUOTA_DEFAULT	64
#define CHAN_NAPI_QUOTA_MAX	CHAN_NAPI_QUOTA_DEFAULT
#define ISO_BW_DEFAULT (80 * 1024)

/* PHY max frame size in kb */
#define PHY_MAX_FRAME_SIZE_DEFAULT 10
#define PHY_MAX_FRAME_SIZE_MAX 10
struct eqos_cfg {
	bool	use_multi_q;	/* 0=single queue, jumbo frames enabled */
	rxq_ctrl_e	rxq_ctrl[MAX_CHANS];
	uint		q_prio[MAX_CHANS];
	uint		chan_napi_quota[MAX_CHANS];
	pause_frames_e	pause_frames;
	uint		iso_bw;
	uint		eth_iso_enable;
	u32		phy_max_frame_size; /* max size jumbo frames allowed */
};

struct chan_data {
	uint	chan_num;
	uint	cpu;
	u32	int_mask;
	spinlock_t chan_tx_lock;
	spinlock_t chan_lock;
	spinlock_t irq_lock;
};

struct eqos_prv_data {
	struct net_device *dev;
	struct platform_device *pdev;

	bool hw_stopped;
	struct mutex hw_change_lock;

	spinlock_t lock;
	spinlock_t tx_lock;
	spinlock_t pmt_lock;
	UINT mem_start_addr;
	UINT mem_size;
	INT irq_number;
	INT power_irq;
	INT phyirq;
	INT common_irq;
	INT rx_irqs[MAX_CHANS];
	INT tx_irqs[MAX_CHANS];
	int phy_intr_gpio;
	int phy_reset_gpio;

	struct clk *axi_clk;
	struct clk *axi_cbb_clk;
	struct clk *rx_clk;
	struct clk *ptp_ref_clk;
	struct clk *tx_clk;
	struct reset_control *eqos_rst;

	struct regulator *vddio_sys_enet_bias;
	struct regulator *vddio_enet;
	struct regulator *phy_vdd_1v8;
	struct regulator *phy_ovdd_rgmii;
	struct regulator *phy_pllvdd;

	struct eqos_cfg dt_cfg;
	struct chan_data chinfo[MAX_CHANS];
	uint	num_chans;

	uint rx_irq_alloc_mask;
	uint tx_irq_alloc_mask;

	struct hw_if_struct hw_if;
	struct desc_if_struct desc_if;

	struct s_tx_error_counters tx_error_counters;
	struct s_rx_error_counters rx_error_counters;

	struct s_tx_pkt_features tx_pkt_ctx[MAX_CHANS];

	/* TX Queue */
	struct eqos_tx_queue *tx_queue;
	UCHAR tx_queue_cnt;
	UINT tx_qinx;

	/* RX Queue */
	struct eqos_rx_queue *rx_queue;
	UCHAR rx_queue_cnt;
	UINT rx_qinx;

	struct mii_bus *mii;
	struct phy_device *phydev;
	struct device_node *phy_dn;
	int oldlink;
	int speed;
	int oldspeed;
	int oldduplex;
	int phyaddr;
	int bus_id;
	netdev_features_t dev_state;
	u32 interface;
	bool use_fixed_phy;

	/* saving state for Wake-on-LAN */
	int wolopts;

/* Helper macros for handling FLOW control in HW */
#define EQOS_FLOW_CTRL_OFF 0
#define EQOS_FLOW_CTRL_RX  1
#define EQOS_FLOW_CTRL_TX  2
#define EQOS_FLOW_CTRL_TX_RX (EQOS_FLOW_CTRL_TX |\
					EQOS_FLOW_CTRL_RX)

	unsigned int flow_ctrl;

	/* keeps track of previous programmed flow control options */
	unsigned int oldflow_ctrl;

	struct eqos_hw_features hw_feat;

	/* for sa(source address) insert/replace */
	u32 tx_sa_ctrl_via_desc;
	u32 tx_sa_ctrl_via_reg;
	unsigned char mac_addr[EQOS_MAC_ADDR_LEN];

	/* keeps track of power mode for API based PMT control */
	u32 power_down;
	u32 power_down_type;

	/* RXQ enable control */
	u32 rxq_enable_ctrl[4];

	/* ptp configuration options */
	struct ptp_cfg_params ptp_cfg;

	/* AXI parameters */
	UINT incr_incrx;
	UINT axi_pbl;
	UINT axi_worl;
	UINT axi_rorl;

	int (*process_rx_completions)(struct eqos_prv_data *pdata,
				      int quota, UINT qinx);
	int (*alloc_rx_buf) (struct eqos_prv_data *pdata,
			     struct rx_swcx_desc *buffer, gfp_t gfp);
	unsigned int rx_buffer_len;
	unsigned int rx_max_frame_size;

	/* variable frame burst size */
	UINT drop_tx_pktburstcnt;
	unsigned int mac_enable_count;	/* counter for enabling MAC transmit at
					drop tx packet  */

	struct eqos_mmc_counters mmc;
	struct eqos_extra_stats xstats;

	/* for MAC loopback */
	unsigned int mac_loopback_mode;

	/* for hw time stamping */
	unsigned char hwts_tx_en;
	unsigned char hwts_rx_en;
	struct ptp_clock *ptp_clock;
	struct ptp_clock_info ptp_clock_ops;
	spinlock_t ptp_lock; /* protects registers */
	unsigned int default_addend;
	bool one_nsec_accuracy; /* set to 1 if one nano second accuracy
				   is enabled else set to zero */

	bool suspended;
	/* for filtering */
	int max_hash_table_size;
	int max_addr_reg_cnt;

	/* L3/L4 filtering */
	unsigned int l3_l4_filter;

	unsigned char vlan_hash_filtering;
	unsigned int l2_filtering_mode; /* 0 - if perfect and 1 - if hash filtering */

	/* For handling PCS(TBI/RTBI/SGMII) and RGMII/SMII interface */
	unsigned int pcs_link;
	unsigned int pcs_duplex;
	unsigned int pcs_speed;
	unsigned int pause;
	unsigned int duplex;
	unsigned int lp_pause;
	unsigned int lp_duplex;


	/* for handling EEE */
	struct timer_list eee_ctrl_timer;
	bool tx_path_in_lpi_mode;
	bool use_lpi_tx_automate;
	int eee_enabled;
	int eee_active;
	int tx_lpi_timer;

	/* arp offload enable/disable. */
	u32 arp_offload;

	/* set to 1 when ptp offload is enabled, else 0. */
	u32 ptp_offload;
	/* ptp offloading mode - ORDINARY_SLAVE, ORDINARY_MASTER,
     * TRANSPARENT_SLAVE, TRANSPARENT_MASTER, PTOP_TRANSPERENT.
     * */
	u32 ptp_offloading_mode;


	/* For configuring double VLAN via descriptor/reg */
	int inner_vlan_tag;
	int outer_vlan_tag;
	/* op_type will be
	 * 0/1/2/3 for none/delet/insert/replace respectively
	 * */
	int op_type;
	/* in_out will be
	 * 1/2/3 for outer/inner/both respectively.
	 * */
	int in_out;
	/* 0 for via registers and 1 for via descriptor */
	int via_reg_or_desc;



	/* this variable will hold vlan table value if vlan hash filtering
	 * is enabled else hold vlan id that is programmed in HW.
	 */
	UINT vlan_ht_or_id;

	/* Used when LRO is enabled,
	 * set to 1 if skb has TCP payload else set to 0
	 */
	int tcp_pkt;

	u32 csr_clock_speed;

	struct workqueue_struct *fbe_wq;
	struct work_struct fbe_work;
	u8	fbe_chan_mask;

#ifdef CONFIG_THERMAL
	struct thermal_cooling_device *cdev;
	atomic_t therm_state;
#endif
#ifdef DO_TX_ALIGN_TEST
	u8 *ptst_buf;
	u32 tst_buf_dma_addr;
	int tst_buf_size;
#endif
	tegra_isomgr_handle isomgr_handle;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0)
	struct tegra_prod_list    *prod_list;
#else
	struct tegra_prod    *prod_list;
#endif
	void __iomem *pads;
};

typedef enum {
	save,
	restore
} e_int_state;

/* Function prototypes*/

void eqos_init_function_ptrs_dev(struct hw_if_struct *);
void eqos_init_function_ptrs_desc(struct desc_if_struct *);
struct net_device_ops *eqos_get_netdev_ops(void);
struct ethtool_ops *eqos_get_ethtool_ops(void);
int eqos_napi_mq(struct napi_struct *, int);

void eqos_get_pdata(struct eqos_prv_data *pdata);

/* Debugfs related functions. */
int create_debug_files(void);
void remove_debug_files(void);

int eqos_mdio_register(struct net_device *dev);
void eqos_mdio_unregister(struct net_device *dev);
INT eqos_mdio_read_direct(struct eqos_prv_data *pdata,
				 int phyaddr, int phyreg, int *phydata);
INT eqos_mdio_write_direct(struct eqos_prv_data *pdata,
				  int phyaddr, int phyreg, int phydata);
void dbgpr_regs(void);
void dump_phy_registers(struct eqos_prv_data *);
void dump_tx_desc(struct eqos_prv_data *pdata, int first_desc_idx,
		  int last_desc_idx, int flag, UINT qinx);
void dump_rx_desc(UINT, struct s_rx_desc *desc, int cur_rx);
void print_pkt(struct sk_buff *skb, int len, bool tx_rx, int desc_idx);
void eqos_get_all_hw_features(struct eqos_prv_data *pdata);
void eqos_print_all_hw_features(struct eqos_prv_data *pdata);
void eqos_configure_flow_ctrl(struct eqos_prv_data *pdata);
INT eqos_powerup(struct net_device *, UINT);
INT eqos_powerdown(struct net_device *, UINT, UINT);
u32 eqos_usec2riwt(u32 usec, struct eqos_prv_data *pdata);
void eqos_init_rx_coalesce(struct eqos_prv_data *pdata);
void eqos_enable_all_ch_rx_interrpt(struct eqos_prv_data *pdata);
void eqos_disable_all_ch_rx_interrpt(struct eqos_prv_data *pdata);
void eqos_update_rx_errors(struct net_device *, unsigned int);
void eqos_stop_all_ch_tx_dma(struct eqos_prv_data *pdata);
UCHAR get_tx_queue_count(void);
UCHAR get_rx_queue_count(void);
void eqos_mmc_read(struct eqos_mmc_counters *mmc);

int eqos_ptp_init(struct eqos_prv_data *pdata);
void eqos_ptp_remove(struct eqos_prv_data *pdata);
phy_interface_t eqos_get_phy_interface(struct eqos_prv_data *pdata);
bool eqos_eee_init(struct eqos_prv_data *pdata);
void eqos_handle_eee_interrupt(struct eqos_prv_data *pdata);
void eqos_disable_eee_mode(struct eqos_prv_data *pdata);
void eqos_enable_eee_mode(struct eqos_prv_data *pdata);

int eqos_handle_mem_iso_ioctl(struct eqos_prv_data *pdata, void *ptr);
int eqos_handle_csr_iso_ioctl(struct eqos_prv_data *pdata, void *ptr);
int eqos_handle_phy_loopback(struct eqos_prv_data *pdata, void *ptr);
void eqos_fbe_work(struct work_struct *work);
int eqos_fixed_phy_register(struct net_device *ndev);

/* For debug prints*/
#define DRV_NAME "eqos_drv.c"

#ifdef YDEBUG
#define DBGPR(x...) printk(KERN_ALERT x)
#define DBGPR_REGS() dbgpr_regs()
#define DBGPHY_REGS(x...) dump_phy_registers(x)
#else
#define DBGPR(x...) do { } while (0)
#define DBGPR_REGS() do { } while (0)
#define DBGPHY_REGS(x...) do { } while (0)
#endif

#ifdef YDEBUG_PG
#define DBGPR_PG(x...) printk(KERN_ALERT x)
#else
#define DBGPR_PG(x...) do {} while (0)
#endif

#ifdef YDEBUG_MDIO
#define DBGPR_MDIO(x...) printk(KERN_ALERT x)
#else
#define DBGPR_MDIO(x...) do {} while (0)
#endif

#ifdef YDEBUG_PTP
#define DBGPR_PTP(x...) printk(KERN_ALERT x)
#else
#define DBGPR_PTP(x...) do {} while (0)
#endif

#ifdef YDEBUG_FILTER
#define DBGPR_FILTER(x...) printk(KERN_ALERT x)
#else
#define DBGPR_FILTER(x...) do {} while (0)
#endif

#ifdef YDEBUG_EEE
#define DBGPR_EEE(x...) printk(KERN_ALERT x)
#else
#define DBGPR_EEE(x...) do {} while (0)
#endif

#ifdef YDEBUG_DMSG
#define DBGPR_DMSG(x...) printk(KERN_ALERT x)
#else
#define DBGPR_DMSG(x...) do {} while (0)
#endif

#ifdef YDEBUG_ETHTOOL_
#define DBGPR_ETHTOOL(x...) printk(KERN_ALERT x)
#else
#define DBGPR_ETHTOOL(x...) do {} while (0)
#endif

#endif
