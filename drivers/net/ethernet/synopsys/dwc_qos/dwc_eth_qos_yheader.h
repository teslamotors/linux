/*
 * DWC Ether QOS controller driver for Samsung TRAV SoCs
 *
 * Copyright (C) Synopsys India Pvt. Ltd.
 * Copyright (C) 2017 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Author: Pankaj Dubey <pankaj.dubey@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __DWC_ETH_QOS__YHEADER__
#define __DWC_ETH_QOS__YHEADER__

#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/cdev.h>

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
#if defined(CONFIG_VLAN_8021Q) || defined(CONFIG_VLAN_8021Q_MODULE)
#define DWC_EQOS_ENABLE_VLAN_TAG
#include <linux/if_vlan.h>
#endif
/* for PPT */
#include <linux/net_tstamp.h>
#include <linux/ptp_clock_kernel.h>
#include <linux/clocksource.h>
#include <linux/platform_device.h>

#include <asm-generic/errno.h>

/* Macro definitions*/
/* NOTE: Uncomment below line for TX and RX DESCRIPTOR DUMP in KERNEL LOG */
//#define DWC_EQOS_ENABLE_TX_DESC_DUMP
//#define DWC_EQOS_ENABLE_RX_DESC_DUMP

/* NOTE: Uncomment below line for TX and RX PACKET DUMP in KERNEL LOG */
/*#define DWC_EQOS_ENABLE_TX_PKT_DUMP*/
/*#define DWC_EQOS_ENABLE_RX_PKT_DUMP*/

#define DWC_EQOS_TXPOLLING_MODE_ENABLE

#if IS_ENABLED(CONFIG_DWC_QOS_PTPSUPPORT)
#undef DWC_EQOS_TXPOLLING_MODE_ENABLE
#endif

/* Uncomment below macro to enable Double VLAN support. */
#define DWC_EQOS_ENABLE_DVLAN

/* NOTE: Uncomment below line for function trace log messages in KERNEL LOG */
//#define YDEBUG
//#define YDEBUG_MDIO
//#define YDEBUG_PTP
//#define YDEBUG_FILTER

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

#define GET_TX_ERROR_COUNTERS_PTR (&pdata->tx_error_counters)

#define GET_RX_ERROR_COUNTERS_PTR (&pdata->rx_error_counters)

#define GET_RX_PKT_FEATURES_PTR (&pdata->rx_pkt_features)

#define GET_TX_PKT_FEATURES_PTR (&pdata->tx_pkt_features)

#define MASK (0x1ULL << 0 | \
		0x13c7ULL << 32)
#define MAC_MASK (0x10ULL << 0)
#define TX_DESC_CNT 256
#define RX_DESC_CNT 256
#define MIN_RX_DESC_CNT 16
#define TX_BUF_SIZE 1536
#define RX_BUF_SIZE 1568

#define MIN_PACKET_SIZE 60

/*
#ifdef DWC_EQOS_ENABLE_VLAN_TAG
#define MAX_PACKET_SIZE VLAN_ETH_FRAME_LEN
#else
#define MAX_PACKET_SIZE 1514
#endif
 */

/* RX header size for split header */
#define DWC_EQOS_HDR_SIZE_64B   64   /* 64 bytes */
#define DWC_EQOS_HDR_SIZE_128B  128  /* 128 bytes */
#define DWC_EQOS_HDR_SIZE_256B  256  /* 256 bytes */
#define DWC_EQOS_HDR_SIZE_512B  512  /* 512 bytes */
#define DWC_EQOS_HDR_SIZE_1024B 1024 /* 1024 bytes */

#define DWC_EQOS_MAX_HDR_SIZE DWC_EQOS_HDR_SIZE_256B

#define MAX_MULTICAST_LIST 14
#define RX_DESC_DATA_LENGTH_LBIT 0
#define RX_DESC_DATA_LENGTH 0x7fff
#define DWC_EQOS_TX_FLAGS_IP_PKT 0x00000001
#define DWC_EQOS_TX_FLAGS_TCP_PKT 0x00000002

#define DEV_NAME "DWC_ETH_QOS"

/* MII/GMII register offset */
#define DWC_EQOS_AUTO_NEGO_NP	0x0007
#define DWC_EQOS_PHY_CTL	0x0010
#define DWC_EQOS_PHY_STS	0x0011

/* Default MTL queue operation mode values */
#define DWC_EQOS_Q_DISABLED	0x0
#define DWC_EQOS_Q_AVB	0x1
#define DWC_EQOS_Q_DCB	0x2
#define DWC_EQOS_Q_GENERIC	0x3

/* Driver PMT macros */
#define DWC_EQOS_DRIVER_CONTEXT 1
#define DWC_EQOS_IOCTL_CONTEXT 2
#define DWC_EQOS_MAGIC_WAKEUP	(1 << 0)
#define DWC_EQOS_REMOTE_WAKEUP	(1 << 1)
#define DWC_EQOS_POWER_DOWN_TYPE(x)	\
	((x->power_down_type & DWC_EQOS_MAGIC_WAKEUP) ? \
	 "Magic packet" : \
	 ((x->power_down_type & DWC_EQOS_REMOTE_WAKEUP) ? \
	  "Remote wakeup packet" : \
	  "<error>"))

#define DWC_EQOS_MAC_ADDR_LEN 6

#if IS_ENABLED(CONFIG_VLAN_8021Q)
#define DWC_EQOS_ETH_FRAME_LEN (ETH_FRAME_LEN + ETH_FCS_LEN + VLAN_HLEN)
#else
#define DWC_EQOS_ETH_FRAME_LEN (ETH_FRAME_LEN + ETH_FCS_LEN)
#endif

#define FIFO_SIZE_B(x) (x)
#define FIFO_SIZE_KB(x) (x * 1024)

/* 8 KB Maximum data per buffer pointer(in Bytes) */
#define DWC_EQOS_MAX_DATA_PER_TX_BUF (1 << 13)
/* Maxmimum data per descriptor(in Bytes) */
#define DWC_EQOS_MAX_DATA_PER_TXD (DWC_EQOS_MAX_DATA_PER_TX_BUF * 2)

#define DWC_EQOS_MAX_SUPPORTED_MTU 16380
/* Default maximum Gaint Packet Size Limit */
#define DWC_EQOS_MAX_GPSL 9000

#if IS_ENABLED(CONFIG_VLAN_8021Q)
#define DWC_EQOS_MIN_SUPPORTED_MTU (ETH_ZLEN + ETH_FCS_LEN + VLAN_HLEN)
#else
#define DWC_EQOS_MIN_SUPPORTED_MTU (ETH_ZLEN + ETH_FCS_LEN)
#endif

#define DWC_EQOS_RDESC3_OWN	0x80000000
#define DWC_EQOS_RDESC3_FD		0x20000000
#define DWC_EQOS_RDESC3_LD		0x10000000
#define DWC_EQOS_RDESC3_RS2V	0x08000000
#define DWC_EQOS_RDESC3_RS1V	0x04000000
#define DWC_EQOS_RDESC3_RS0V	0x02000000
#define DWC_EQOS_RDESC3_LT		0x00070000
#define DWC_EQOS_RDESC3_ES		0x00008000
#define DWC_EQOS_RDESC3_PL		0x00007FFF

#define DWC_EQOS_RDESC2_HL	0x000003FF

/* Payload type */
#define DWC_EQOS_RDESC1_PT		0x00000007
/* Payload type = TCP */
#define DWC_EQOS_RDESC1_PT_TCP	0x00000002

/* Maximum size of pkt that is copied to a new buffer on receive */
#define DWC_EQOS_COPYBREAK_DEFAULT 256
/* System clock is 62.5MHz */
#define DWC_EQOS_SYSCLOCK	24000000
/* System time period is 16ns */
#define DWC_EQOS_SYSTIMEPERIOD	16

#define DWC_EQOS_TX_QUEUE_CNT (pdata->tx_queue_cnt)
#define DWC_EQOS_RX_QUEUE_CNT (pdata->rx_queue_cnt)
#define DWC_EQOS_QUEUE_CNT min(DWC_EQOS_TX_QUEUE_CNT, DWC_EQOS_RX_QUEUE_CNT)

#define DWC_EQOS_NAPI_WEIGHT	64

/* Helper macros for TX descriptor handling */

#define GET_TX_QUEUE_PTR(qinx) (&pdata->tx_queue[(qinx)])

#define GET_TX_DESC_PTR(qinx, dinx) (pdata->tx_queue[(qinx)].tx_desc_data.tx_desc_ptrs[(dinx)])

#define GET_TX_DESC_DMA_ADDR(qinx, dinx) (pdata->tx_queue[(qinx)].tx_desc_data.tx_desc_dma_addrs[(dinx)])

#define GET_TX_WRAPPER_DESC(qinx) (&pdata->tx_queue[(qinx)].tx_desc_data)

#define GET_TX_BUF_PTR(qinx, dinx) (pdata->tx_queue[(qinx)].tx_desc_data.tx_buf_ptrs[(dinx)])

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

#define GET_CURRENT_XFER_DESC_CNT(qinx) (pdata->tx_queue[(qinx)].tx_desc_data.packet_count)

#define GET_CURRENT_XFER_LAST_DESC_INDEX(qinx, start_index, offset)\
	(GET_CURRENT_XFER_DESC_CNT((qinx)) == 0) ? (TX_DESC_CNT - 1) :\
((GET_CURRENT_XFER_DESC_CNT((qinx)) == 1) ? (INCR_TX_LOCAL_INDEX((start_index), (offset))) :\
INCR_TX_LOCAL_INDEX((start_index), (GET_CURRENT_XFER_DESC_CNT((qinx)) + (offset) - 1)))

#define GET_TX_TOT_LEN(buffer, start_index, packet_count, total_len) do {\
	int i, pkt_idx = (start_index);\
	for (i = 0; i < (packet_count); i++) {\
		(total_len) += ((buffer)[pkt_idx].len + (buffer)[pkt_idx].len2);\
		pkt_idx = INCR_TX_LOCAL_INDEX(pkt_idx, 1);\
	} \
} while (0)

/* Helper macros for RX descriptor handling */

#define GET_RX_QUEUE_PTR(qinx) (&(pdata->rx_queue[(qinx)]))

#define GET_RX_DESC_PTR(qinx, dinx) (pdata->rx_queue[(qinx)].rx_desc_data.rx_desc_ptrs[(dinx)])

#define GET_RX_DESC_DMA_ADDR(qinx, dinx) (pdata->rx_queue[(qinx)].rx_desc_data.rx_desc_dma_addrs[(dinx)])

#define GET_RX_WRAPPER_DESC(qinx) (&pdata->rx_queue[(qinx)].rx_desc_data)

#define GET_RX_BUF_PTR(qinx, dinx) (pdata->rx_queue[(qinx)].rx_desc_data.rx_buf_ptrs[(dinx)])

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

#define GET_CURRENT_RCVD_DESC_CNT(qinx) (pdata->rx_queue[(qinx)].rx_desc_data.pkt_received)

#define GET_CURRENT_RCVD_LAST_DESC_INDEX(start_index, offset) (RX_DESC_CNT - 1)

#define GET_TX_DESC_IDX(qinx, desc) (((desc) - GET_TX_DESC_DMA_ADDR((qinx), 0)) / (sizeof(struct s_tx_normal_desc)))

#define GET_RX_DESC_IDX(qinx, desc) (((desc) - GET_RX_DESC_DMA_ADDR((qinx), 0)) / (sizeof(struct s_rx_normal_desc)))

/* Helper macro for handling coalesce parameters via ethtool */
/* Obtained by trial and error  */
#define DWC_EQOS_OPTIMAL_DMA_RIWT_USEC  124
/* Max delay before RX interrupt after a pkt is received Max
 * delay in usecs is 1020 for 62.5MHz device clock
 */
#define DWC_EQOS_MAX_DMA_RIWT  0xff
/* Max no of pkts to be received before an RX interrupt */
#define DWC_EQOS_RX_MAX_FRAMES 16
/* Max no of descriptors to be transmitted before a TX interrupt */
#define DWC_EQOS_TX_MAX_COAL_DESC 16
#define DWC_COAL_TIMER(x) (jiffies + usecs_to_jiffies(x))

#define DMA_SBUS_AXI_PBL_MASK 0xFE

/* Helper macros for handling receive error */
#define DWC_EQOS_RX_LENGTH_ERR        0x00000001
#define DWC_EQOS_RX_BUF_OVERFLOW_ERR  0x00000002
#define DWC_EQOS_RX_CRC_ERR           0x00000004
#define DWC_EQOS_RX_FRAME_ERR         0x00000008
#define DWC_EQOS_RX_FIFO_OVERFLOW_ERR 0x00000010
#define DWC_EQOS_RX_MISSED_PKT_ERR    0x00000020

#define DWC_EQOS_RX_CHECKSUM_DONE 0x00000001
#define DWC_EQOS_RX_VLAN_PKT      0x00000002

/* MAC Time stamp contorl reg bit fields */
/* Enable timestamp */
#define MAC_TCR_TSENA         0x00000001
/* Enable Fine Timestamp Update */
#define MAC_TCR_TSCFUPDT      0x00000002
/* Enable timestamping for all packets */
#define MAC_TCR_TSENALL       0x00000100
/* Enable Timestamp Digitla Contorl (1ns accuracy )*/
#define MAC_TCR_TSCTRLSSR     0x00000200
/* Enable PTP packet processing for Version 2 Formate */
#define MAC_TCR_TSVER2ENA     0x00000400
/* Enable processing of PTP over Ethernet Packets */
#define MAC_TCR_TSIPENA       0x00000800
/* Enable processing of PTP Packets sent over IPv6-UDP Packets */
#define MAC_TCR_TSIPV6ENA     0x00001000
/* Enable processing of PTP Packets sent over IPv4-UDP Packets */
#define MAC_TCR_TSIPV4ENA     0x00002000
/* Enable Timestamp Snapshot for Event Messages */
#define MAC_TCR_TSEVENTENA    0x00004000
/* Enable snapshot for Message Relevent to Master */
#define MAC_TCR_TSMASTERENA   0x00008000
/* select PTP packets for taking snapshots */
#define MAC_TCR_SNAPTYPSEL_1  0x00010000
#define MAC_TCR_SNAPTYPSEL_2  0x00020000
#define MAC_TCR_SNAPTYPSEL_3  0x00030000
/* Enable AV 802.1AS Mode */
#define MAC_TCR_AV8021ASMEN   0x10000000

/* PTP Offloading control register bits (MAC_PTO_control)*/
/* PTP offload Enable */
#define MAC_PTOCR_PTOEN		  0x00000001
/* Automatic PTP Sync message enable */
#define MAC_PTOCR_ASYNCEN	  0x00000002
/* Automatic PTP Pdelay_Req message enable */
#define MAC_PTOCR_APDREQEN	  0x00000004

/* Hash Table Reg count */
#define DWC_EQOS_HTR_CNT (pdata->max_hash_table_size / 32)

/* For handling VLAN filtering */
#define DWC_EQOS_VLAN_PERFECT_FILTERING 0
#define DWC_EQOS_VLAN_HASH_FILTERING 1

/* For handling differnet PHY interfaces */
#define DWC_EQOS_GMII_MII	0x0
#define DWC_EQOS_RGMII	0x1
#define DWC_EQOS_SGMII	0x2
#define DWC_EQOS_TBI		0x3
#define DWC_EQOS_RMII	0x4
#define DWC_EQOS_RTBI	0x5
#define DWC_EQOS_SMII	0x6
#define DWC_EQOS_REVMII	0x7

/* LPI Tx local expiration time in msec */
#define DWC_EQOS_DEFAULT_LPI_TIMER 1000
#define DWC_EQOS_LPI_TIMER(x) (jiffies + msecs_to_jiffies(x))

/* Error and status macros defined below */

#define E_DMA_SR_TPS        6
#define E_DMA_SR_TBU        7
#define E_DMA_SR_RBU        8
#define E_DMA_SR_RPS        9
#define S_DMA_SR_RWT        2
#define E_DMA_SR_FBE       10
#define S_MAC_ISR_PMTIS     11

struct s_rx_context_desc {
	unsigned int rdes0;
	unsigned int rdes1;
	unsigned int rdes2;
	unsigned int rdes3;
};

typedef struct s_rx_context_desc t_rx_context_desc;

struct s_tx_context_desc {
	unsigned int tdes0;
	unsigned int tdes1;
	unsigned int tdes2;
	unsigned int tdes3;
};

typedef struct s_tx_context_desc t_tx_context_desc;

struct s_rx_normal_desc {
	unsigned int rdes0;
	unsigned int rdes1;
	unsigned int rdes2;
	unsigned int rdes3;
};

typedef struct s_rx_normal_desc t_rx_normal_desc;

struct s_tx_normal_desc {
	unsigned int tdes0;
	unsigned int tdes1;
	unsigned int tdes2;
	unsigned int tdes3;
};

typedef struct s_tx_normal_desc t_tx_normal_desc;

struct s_tx_error_counters {
	unsigned int tx_errors;
};

typedef struct s_tx_error_counters t_tx_error_counters;

struct s_rx_error_counters {
	unsigned int rx_errors;
};

typedef struct s_rx_error_counters t_rx_error_counters;

struct s_rx_pkt_features {
	unsigned int pkt_attributes;
	unsigned int vlan_tag;
};

typedef struct s_rx_pkt_features t_rx_pkt_features;

struct s_tx_pkt_features {
	unsigned int pkt_attributes;
	unsigned int vlan_tag;
	unsigned long mss;
	unsigned long hdr_len;
	unsigned long pay_len;
	unsigned char ipcss;
	unsigned char ipcso;
	unsigned short ipcse;
	unsigned char tucss;
	unsigned char tucso;
	unsigned short tucse;
	unsigned int pkt_type;
	unsigned long tcp_udp_hdr_len;
};

typedef struct s_tx_pkt_features t_tx_pkt_features;

enum e_dwc_eqos_int_id {
	EDWC_EQOS_DMA_ISR_DC0IS,
	EDWC_EQOS_DMA_SR0_TI,
	EDWC_EQOS_DMA_SR0_TPS,
	EDWC_EQOS_DMA_SR0_TBU,
	EDWC_EQOS_DMA_SR0_RI,
	EDWC_EQOS_DMA_SR0_RBU,
	EDWC_EQOS_DMA_SR0_RPS,
	EDWC_EQOS_DMA_SR0_FBE,
	EDWC_EQOS_ALL
};

enum e_dwc_eqos_mtl_fifo_size {
	EDWC_EQOS_256 = 0x0,
	EDWC_EQOS_512 = 0x1,
	EDWC_EQOS_1K = 0x3,
	EDWC_EQOS_2K = 0x7,
	EDWC_EQOS_4K = 0xf,
	EDWC_EQOS_8K = 0x1f,
	EDWC_EQOS_16K = 0x3f,
	EDWC_EQOS_32K = 0x7f
};

typedef enum {
	PTP_CLK,
	ACLK_CLK,
	HCLK_CLK,
	RGMII_CLK,
	RXI_CLK,
	EQOS_DBUS_CLK,
	EQOS_PBUS_CLK,
	EQOS_MAX_CLK
} dwc_eqos_clk;

/* do forward declaration of private data structure */
struct dwc_eqos_prv_data;
struct dwc_eqos_tx_wrapper_descriptor;

struct hw_if_struct {
	int (*tx_complete)(struct s_tx_normal_desc *txdesc);
	int (*tx_window_error)(struct s_tx_normal_desc *txdesc);
	int (*tx_aborted_error)(struct s_tx_normal_desc *txdesc);
	int (*tx_carrier_lost_error)(struct s_tx_normal_desc *txdesc);
	int (*tx_fifo_underrun)(struct s_tx_normal_desc *txdesc);
	int (*tx_get_collision_count)(struct s_tx_normal_desc *txdesc);
	int (*tx_handle_aborted_error)(struct s_tx_normal_desc *txdesc);
	int (*tx_update_fifo_threshold)(struct s_tx_normal_desc *txdesc);
	/*tx threshold config */
	int (*tx_config_threshold)(unsigned int);

	int (*set_promiscuous_mode)(struct dwc_eqos_prv_data *pdata);
	int (*set_all_multicast_mode)(struct dwc_eqos_prv_data *pdata);
	int (*set_multicast_list_mode)(struct dwc_eqos_prv_data *pdata);
	int (*set_unicast_mode)(struct dwc_eqos_prv_data *pdata);

	int (*enable_rx_csum)(struct net_device *dev);
	int (*disable_rx_csum)(struct net_device *dev);
	int (*get_rx_csum_status)(struct net_device *dev);

	int (*read_phy_regs)(struct dwc_eqos_prv_data *pdata,
			int, int, int*);
	int (*write_phy_regs)(struct dwc_eqos_prv_data *pdata,
			int, int, int);
	int (*set_full_duplex)(struct net_device *dev);
	int (*set_half_duplex)(struct net_device *dev);
	int (*set_mii_speed_100)(struct net_device *dev);
	int (*set_mii_speed_10)(struct net_device *dev);
	int (*set_gmii_speed)(struct net_device *dev);
	/* for PMT */
	int (*start_dma_rx)(struct dwc_eqos_prv_data *pdata,
			unsigned int);
	int (*stop_dma_rx)(struct dwc_eqos_prv_data *pdata,
			unsigned int);
	int (*start_dma_tx)(struct dwc_eqos_prv_data *pdata,
			unsigned int);
	int (*stop_dma_tx)(struct dwc_eqos_prv_data *pdata,
			unsigned int);
	int (*start_mac_tx_rx)(struct net_device *dev);
	int (*stop_mac_tx_rx)(struct net_device *dev);

	int (*init)(struct dwc_eqos_prv_data *pdata);
	int (*exit)(struct dwc_eqos_prv_data *pdata);
	int (*enable_int)(enum e_dwc_eqos_int_id);
	int (*disable_int)(enum e_dwc_eqos_int_id);
	void (*pre_xmit)(struct dwc_eqos_prv_data *pdata,
			unsigned int qinx);
	void (*dev_read)(struct dwc_eqos_prv_data *pdata,
			unsigned int qinx);
	void (*tx_desc_init)(struct dwc_eqos_prv_data *pdata,
			unsigned int qinx);
	void (*rx_desc_init)(struct dwc_eqos_prv_data *pdata,
			unsigned int qinx);
	void (*rx_desc_reset)(unsigned int, struct dwc_eqos_prv_data *pdata,
			unsigned int, unsigned int qinx);
	int (*tx_desc_reset)(unsigned int, struct dwc_eqos_prv_data *pdata,
			unsigned int qinx);
	/* last tx segmnet reports the tx status */
	int (*get_tx_desc_ls)(struct s_tx_normal_desc *);
	int (*get_tx_desc_ctxt)(struct s_tx_normal_desc *);
	void (*update_rx_tail_ptr)(struct dwc_eqos_prv_data *pdata,
			unsigned int qinx, unsigned int dma_addr);

	/* for FLOW ctrl */
	int (*enable_rx_flow_ctrl)(struct dwc_eqos_prv_data *pdata);
	int (*disable_rx_flow_ctrl)(struct dwc_eqos_prv_data *pdata);
	int (*enable_tx_flow_ctrl)(struct dwc_eqos_prv_data *pdata,
			unsigned int);
	int (*disable_tx_flow_ctrl)(struct dwc_eqos_prv_data *pdata,
			unsigned int);

	/* for RX watchdog timer */
	int (*config_rx_watchdog)(struct dwc_eqos_prv_data *pdata,
			unsigned int, u32 riwt);

	/* for RX and TX threshold config */
	int (*config_rx_threshold)(struct dwc_eqos_prv_data *pdata,
			unsigned int ch_no, unsigned int val);
	int (*config_tx_threshold)(struct dwc_eqos_prv_data *pdata,
			unsigned int ch_no, unsigned int val);

	/* for RX and TX Store and Forward Mode config */
	int (*config_rsf_mode)(struct dwc_eqos_prv_data *pdata,
			unsigned int ch_no, unsigned int val);
	int (*config_tsf_mode)(struct dwc_eqos_prv_data *pdata,
			unsigned int ch_no, unsigned int val);

	/* for TX DMA Operate on Second Frame config */
	int (*config_osf_mode)(struct dwc_eqos_prv_data *pdata,
			unsigned int ch_no, unsigned int val);

	/* for INCR/INCRX config */
	int (*config_incr_incrx_mode)(struct dwc_eqos_prv_data *pdata,
			unsigned int val);
	/* for AXI PBL config */
	int (*config_axi_pbl_val)(struct dwc_eqos_prv_data *pdata,
			unsigned int val);
	/* for AXI WORL config */
	int (*config_axi_worl_val)(struct dwc_eqos_prv_data *pdata,
			unsigned int val);
	/* for AXI RORL config */
	int (*config_axi_rorl_val)(struct dwc_eqos_prv_data *pdata,
			unsigned int val);

	/* for RX and TX PBL config */
	int (*config_rx_pbl_val)(struct dwc_eqos_prv_data *pdata,
			unsigned int ch_no, unsigned int val);
	int (*get_rx_pbl_val)(struct dwc_eqos_prv_data *pdata,
			unsigned int ch_no);
	int (*config_tx_pbl_val)(struct dwc_eqos_prv_data *pdata,
			unsigned int ch_no, unsigned int val);
	int (*get_tx_pbl_val)(struct dwc_eqos_prv_data *pdata,
			unsigned int ch_no);
	int (*config_pblx8)(struct dwc_eqos_prv_data *pdata,
			unsigned int ch_no, unsigned int val);

	/* for TX vlan control */
	void (*enable_vlan_reg_control)(struct dwc_eqos_prv_data *pdata,
			struct dwc_eqos_tx_wrapper_descriptor *desc_data);
	void (*enable_vlan_desc_control)(struct dwc_eqos_prv_data *pdata);

	/* for rx vlan stripping */
	//	 void (*config_rx_outer_vlan_stripping)(u32);
	//	 void (*config_rx_inner_vlan_stripping)(u32);

	/* for sa(source address) insert/replace */
	void (*configure_mac_addr0_reg)(struct dwc_eqos_prv_data *pdata,
			unsigned char *);
	void (*configure_mac_addr1_reg)(struct dwc_eqos_prv_data *pdata,
			unsigned char *);
	void (*configure_sa_via_reg)(struct net_device *dev, u32);

	/* for handling multi-queue */
	int (*disable_rx_interrupt)(struct dwc_eqos_prv_data *pdata,
			unsigned int);
	int (*enable_rx_interrupt)(struct dwc_eqos_prv_data *pdata,
			unsigned int);
	int (*disable_tx_interrupt)(struct dwc_eqos_prv_data *pdata,
								unsigned int);
	int (*enable_tx_interrupt)(struct dwc_eqos_prv_data *pdata,
							   unsigned int);

	/* for handling MMC */
	int (*disable_mmc_interrupts)(struct dwc_eqos_prv_data *pdata);
	int (*config_mmc_counters)(struct dwc_eqos_prv_data *pdata);

	/* for handling DCB and AVB */
	int (*set_dcb_algorithm)(struct dwc_eqos_prv_data *pdata,
			unsigned char dcb_algorithm);
	int (*set_dcb_queue_weight)(struct dwc_eqos_prv_data *pdata,
			unsigned int qinx, unsigned int q_weight);

	int (*set_tx_queue_operating_mode)(struct dwc_eqos_prv_data *pdata,
			unsigned int qinx, unsigned int q_mode);
	int (*set_avb_algorithm)(struct dwc_eqos_prv_data *pdata,
			unsigned int qinx, unsigned char avb_algorithm);
	int (*config_credit_control)(struct dwc_eqos_prv_data *pdata,
			unsigned int qinx, unsigned int cc);
	int (*config_send_slope)(struct dwc_eqos_prv_data *pdata,
			unsigned int qinx, unsigned int send_slope);
	int (*config_idle_slope)(struct dwc_eqos_prv_data *pdata,
			unsigned int qinx, unsigned int idle_slope);
	int (*config_high_credit)(struct dwc_eqos_prv_data *pdata,
			unsigned int qinx, unsigned int hi_credit);
	int (*config_low_credit)(struct dwc_eqos_prv_data *pdata,
			unsigned int qinx, unsigned int lo_credit);
	int (*config_slot_num_check)(struct dwc_eqos_prv_data *pdata,
			unsigned int qinx, unsigned char slot_check);
	int (*config_advance_slot_num_check)(struct dwc_eqos_prv_data *pdata,
			unsigned int qinx, unsigned char adv_slot_check);
	/* for hw time stamping */
	int (*config_hw_time_stamping)(struct dwc_eqos_prv_data *pdata,
			unsigned int);
	int (*config_sub_second_increment)(struct dwc_eqos_prv_data *pdata,
			unsigned long ptp_clock);
	int (*init_systime)(struct dwc_eqos_prv_data *pdata, unsigned int,
			unsigned int);
	int (*config_addend)(struct dwc_eqos_prv_data *pdata, unsigned int);
	int (*adjust_systime)(struct dwc_eqos_prv_data *pdata, unsigned int,
			unsigned int, int, bool);
	unsigned long long (*get_systime)(struct dwc_eqos_prv_data *pdata);
	unsigned int (*get_tx_tstamp_status)(struct s_tx_normal_desc *txdesc);
	unsigned long long (*get_tx_tstamp)(struct s_tx_normal_desc *txdesc);
	unsigned int (*get_tx_tstamp_status_via_reg)(struct dwc_eqos_prv_data *pdata);
	unsigned long long (*get_tx_tstamp_via_reg)(struct dwc_eqos_prv_data *pdata);
	unsigned int (*rx_tstamp_available)(struct s_rx_normal_desc *rxdesc);
	unsigned int (*get_rx_tstamp_status)(struct s_rx_context_desc *rxdesc);
	unsigned long long (*get_rx_tstamp)(struct s_rx_context_desc *rxdesc);
	int (*drop_tx_status_enabled)(struct dwc_eqos_prv_data *pdata);

	/* for l2, l3 and l4 layer filtering */
	int (*config_l2_da_perfect_inverse_match)(struct dwc_eqos_prv_data *pdata,
			int perfect_inverse_match);
	int (*update_mac_addr32_127_low_high_reg)(struct dwc_eqos_prv_data *pdata,
			int idx, unsigned char addr[]);
	int (*update_mac_addr1_31_low_high_reg)(struct dwc_eqos_prv_data *pdata,
			int idx, unsigned char addr[]);
	int (*update_hash_table_reg)(struct dwc_eqos_prv_data *pdata, int idx,
			unsigned int data);
	int (*config_mac_pkt_filter_reg)(struct dwc_eqos_prv_data *pdata,
			unsigned char, unsigned char, unsigned char,
			unsigned char, unsigned char);
	int (*config_l3_l4_filter_enable)(struct net_device *dev, int);
	int (*config_l3_filters)(struct net_device *dev, int filter_no,
			int enb_dis, int ipv4_ipv6_match,
			int src_dst_addr_match,	int perfect_inverse_match);
	int (*update_ip4_addr0)(struct net_device *dev, int filter_no,
			unsigned char addr[]);
	int (*update_ip4_addr1)(struct net_device *dev, int filter_no,
			unsigned char addr[]);
	int (*update_ip6_addr)(struct net_device *dev, int filter_no,
			unsigned short addr[]);
	int (*config_l4_filters)(struct net_device *dev, int filter_no,
			int enb_dis, int tcp_udp_match, int src_dst_port_match,
			int perfect_inverse_match);
	int (*update_l4_sa_port_no)(struct net_device *dev, int filter_no,
			unsigned short port_no);
	int (*update_l4_da_port_no)(struct net_device *dev, int filter_no,
			unsigned short port_no);

	/* for VLAN filtering */
	int (*get_vlan_hash_table_reg)(struct net_device *dev);
	int (*update_vlan_hash_table_reg)(struct net_device *dev,
			unsigned short data);
	int (*update_vlan_id)(struct net_device *dev, unsigned short vid);
	int (*config_vlan_filtering)(struct net_device *dev, int filter_enb_dis,
			int perfect_hash_filtering,
			int perfect_inverse_match);
	int (*config_mac_for_vlan_pkt)(struct dwc_eqos_prv_data *pdata);
	unsigned int (*get_vlan_tag_comparison)(struct net_device *dev);

	/* for MAC loopback */
	int (*config_mac_loopback_mode)(struct net_device *dev, unsigned int);

	/* for MAC Double VLAN Processing config */
	int (*config_tx_outer_vlan)(struct dwc_eqos_prv_data *pdata,
			unsigned int op_type, unsigned int outer_vlt);
	int (*config_tx_inner_vlan)(struct dwc_eqos_prv_data *pdata,
			unsigned int op_type, unsigned int inner_vlt);
	int (*config_svlan)(struct dwc_eqos_prv_data *pdata,
			unsigned int);
	void (*config_dvlan)(struct dwc_eqos_prv_data *pdata, bool enb_dis);
	void (*config_rx_outer_vlan_stripping)(struct dwc_eqos_prv_data *pdata,
			u32);
	void (*config_rx_inner_vlan_stripping)(struct dwc_eqos_prv_data *pdata,
			u32);

	/* for PFC */
	void (*config_pfc)(struct dwc_eqos_prv_data *pdata, int enb_dis);

	/* for PTP offloading */
	void (*config_ptpoffload_engine)(struct dwc_eqos_prv_data *pdata,
			unsigned int, unsigned int);
};

/* wrapper buffer structure to hold transmit pkt details */
struct dwc_eqos_tx_buffer {
	/* dma address of skb */
	dma_addr_t dma;
	/* virtual address of skb */
	struct sk_buff *skb;
	/* length of first skb */
	unsigned int short len;
	unsigned int buf1_mapped_as_page;

	/* dam address of second skb */
	dma_addr_t dma2;
	/* length of second skb */
	unsigned short len2;
	unsigned char buf2_mapped_as_page;
};

struct dwc_eqos_tx_wrapper_descriptor {
	/* ID of descriptor */
	char *desc_name;

	void *tx_desc_ptrs[TX_DESC_CNT];
	dma_addr_t tx_desc_dma_addrs[TX_DESC_CNT];

	struct dwc_eqos_tx_buffer *tx_buf_ptrs[TX_DESC_CNT];

	unsigned char contigous_mem;

	/* always gives index of desc which has to
	 * be used for current xfer
	 */
	int cur_tx;
	/* always gives index of desc which has to
	 * be checked for xfer complete
	 */
	int dirty_tx;
	/* always gives total number of available
	 * free desc count for driver
	 */
	unsigned int free_desc_cnt;
	/* always gives total number of packets
	 * queued for transmission
	 */
	unsigned int tx_pkt_queued;
	unsigned int queue_stopped;
	int packet_count;

	struct timer_list txtimer;

	/* descriptor count for current transmit */
	unsigned int cur_desc_count;

	/* Max no of descriptors to be transmitted before a TX interrupt */
	unsigned int tx_coal_max_desc;
	unsigned int tx_coal_cur_desc;

	/* contain bit value for TX threshold */
	unsigned int tx_threshold_val;
	/* set to 1 if TSF is enabled else set to 0 */
	unsigned int tsf_on;
	/* set to 1 if OSF is enabled else set to 0 */
	unsigned int osf_on;
	unsigned int tx_pbl;

	/* for tx vlan delete/insert/replace */
	u32 tx_vlan_tag_via_reg;
	u32 tx_vlan_tag_ctrl;

	unsigned short vlan_tag_id;
	unsigned int vlan_tag_present;

	/* for VLAN context descriptor operation */
	u32 context_setup;

	/* for TSO */
	u32 default_mss;
};

struct dwc_eqos_tx_queue {
	/* Tx descriptors */
	struct dwc_eqos_tx_wrapper_descriptor tx_desc_data;
	struct dwc_eqos_prv_data *pdata;
	struct napi_struct napi;
	int q_op_mode;
	int qinx;
};

/* wrapper buffer structure to hold received pkt details */
struct dwc_eqos_rx_buffer {
	/* dma address of skb */
	dma_addr_t dma;
	/* virtual address of skb */
	struct sk_buff *skb;
	/* length of received packet */
	unsigned short len;
	/* page address */
	struct page *page;
	unsigned char mapped_as_page;
	/* set to 1 if it is good packet else set to 0 */
	bool good_pkt;
	/* set to non-zero if INTE is set for corresponding desc */
	unsigned int inte;

	/* dma address of second skb */
	dma_addr_t dma2;
	/* page address of second buffer */
	struct page *page2;
	/* length of received packet-second buffer */
	unsigned short len2;

	/* header buff size in case of split header */
	unsigned short rx_hdr_size;
};

struct dwc_eqos_rx_wrapper_descriptor {
	char *desc_name;	/* ID of descriptor */

	void *rx_desc_ptrs[RX_DESC_CNT];
	dma_addr_t rx_desc_dma_addrs[RX_DESC_CNT];

	struct dwc_eqos_rx_buffer *rx_buf_ptrs[RX_DESC_CNT];

	unsigned char contigous_mem;

	/* always gives index of desc which needs to
	 * be checked for packet availabilty
	 */
	int cur_rx;
	int dirty_rx;
	/* always gives total number of packets
	 * received from device in one RX interrupt
	 */
	unsigned int pkt_received;
	unsigned int skb_realloc_idx;
	unsigned int skb_realloc_threshold;

	/* for rx coalesce schem */
	/* set to 1 if RX watchdog timer should be used
	 * for RX interrupt mitigation
	 */
	int use_riwt;
	u32 rx_riwt;
	/* Max no of pkts to be received before an RX interrupt */
	u32 rx_coal_frames;

	/* contain bit vlaue for RX threshold */
	unsigned int rx_threshold_val;
	/* set to 1 if RSF is enabled else set to 0 */
	unsigned int rsf_on;
	unsigned int rx_pbl;

	/* points to first skb in the chain in case of jumbo pkts */
	struct sk_buff *skb_top;

	/* for rx vlan stripping */
	u32 rx_inner_vlan_strip;
	u32 rx_outer_vlan_strip;
};

struct dwc_eqos_rx_queue {
	/* Rx descriptors */
	struct dwc_eqos_rx_wrapper_descriptor rx_desc_data;
	struct napi_struct napi;
	struct dwc_eqos_prv_data *pdata;
	int qinx;
};

struct desc_if_struct {
	int (*alloc_queue_struct)(struct dwc_eqos_prv_data *pdata);
	void (*free_queue_struct)(struct dwc_eqos_prv_data *pdata);
	int (*alloc_buff_and_desc)(struct dwc_eqos_prv_data *pdata);
	void (*realloc_skb)(struct dwc_eqos_prv_data *pdata,
			unsigned int qinx);
	void (*unmap_rx_skb)(struct dwc_eqos_prv_data *pdata,
			struct dwc_eqos_rx_buffer *buffer);
	void (*unmap_tx_skb)(struct dwc_eqos_prv_data *pdata,
			struct dwc_eqos_tx_buffer *buffer);
	unsigned int (*map_tx_skb)(struct net_device *dev,
			struct sk_buff *skb);
	void (*tx_free_mem)(struct dwc_eqos_prv_data *pdata);
	void (*rx_free_mem)(struct dwc_eqos_prv_data *pdata);
	void (*wrapper_tx_desc_init)(struct dwc_eqos_prv_data *pdata);
	void (*wrapper_tx_desc_init_single_q)(struct dwc_eqos_prv_data *pdata,
			unsigned int qinx);
	void (*wrapper_rx_desc_init)(struct dwc_eqos_prv_data *pdata);
	void (*wrapper_rx_desc_init_single_q)(struct dwc_eqos_prv_data *pdata,
			unsigned int qinx);

	void (*rx_skb_free_mem)(struct dwc_eqos_prv_data *pdata,
			unsigned int qinx);
	void (*rx_skb_free_mem_single_q)(struct dwc_eqos_prv_data *pdata,
			unsigned int qinx);
	void (*tx_skb_free_mem)(struct dwc_eqos_prv_data *pdata,
			unsigned int qinx);
	void (*tx_skb_free_mem_single_q)(struct dwc_eqos_prv_data *pdata,
			unsigned int qinx);

	int (*handle_tso)(struct net_device *dev, struct sk_buff *skb);
};

/*
 * This structure contains different flags and each flags will indicates
 * different hardware features.
 */
struct dwc_eqos_hw_features {
	/* HW Feature Register0 */
	/* 10/100 Mbps support */
	unsigned int mii_sel;
	/* 1000 Mbps support */
	unsigned int gmii_sel;
	/* Half-duplex support */
	unsigned int hd_sel;
	/* PCS registers(TBI, SGMII or RTBI PHY
	 * interface)
	 */
	unsigned int pcs_sel;
	/* VLAN Hash filter selected */
	unsigned int vlan_hash_en;
	/* SMA(MDIO) Interface */
	unsigned int sma_sel;
	/* PMT remote wake-up packet */
	unsigned int rwk_sel;
	/* PMT magic packet */
	unsigned int mgk_sel;
	/* RMON module */
	unsigned int mmc_sel;
	/* ARP Offload features is selected */
	unsigned int arp_offld_en;
	/* IEEE 1588-2008 Adavanced timestamp */
	unsigned int ts_sel;
	/* Energy Efficient Ethernet is enabled */
	unsigned int eee_sel;
	/* Tx Checksum Offload is enabled */
	unsigned int tx_coe_sel;
	/* Rx Checksum Offload is enabled */
	unsigned int rx_coe_sel;
	/* MAC Addresses 1-16 are selected */
	unsigned int mac_addr16_sel;
	/* MAC Addresses 32-63 are selected */
	unsigned int mac_addr32_sel;
	/* MAC Addresses 64-127 are selected */
	unsigned int mac_addr64_sel;
	/* Timestamp System Time Source */
	unsigned int tsstssel;
	/* Speed Select */
	unsigned int speed_sel;
	/* Source Address or VLAN Insertion */
	unsigned int sa_vlan_ins;
	/* Active PHY Selected */
	unsigned int act_phy_sel;

	/* HW Feature Register1 */
	/* MTL Receive FIFO Size */
	unsigned int rx_fifo_size;
	/* MTL Transmit FIFO Size */
	unsigned int tx_fifo_size;
	/* Advance timestamping High Word
	 * selected
	 */
	unsigned int adv_ts_hword;
	/* DCB Feature Enable */
	unsigned int dcb_en;
	/* Split Header Feature Enable */
	unsigned int sph_en;
	/* TCP Segmentation Offload Enable */
	unsigned int tso_en;
	/* DMA debug registers are enabled */
	unsigned int dma_debug_gen;
	/* AV Feature Enabled */
	unsigned int av_sel;
	/* Low Power Mode Enabled */
	unsigned int lp_mode_en;
	/* Hash Table Size */
	unsigned int hash_tbl_sz;
	/* Total number of L3-L4 Filters */
	unsigned int l3l4_filter_num;

	/* HW Feature Register2 */
	/* Number of MTL Receive Queues */
	unsigned int rx_q_cnt;
	/* Number of MTL Transmit Queues */
	unsigned int tx_q_cnt;
	/* Number of DMA Receive Channels */
	unsigned int rx_ch_cnt;
	/* Number of DMA Transmit Channels */
	unsigned int tx_ch_cnt;
	/* Number of PPS outputs */
	unsigned int pps_out_num;
	/* Number of Auxillary snapshot inputs */
	unsigned int aux_snap_num;
};

/* structure to hold MMC values */
struct dwc_eqos_mmc_counters {
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
	unsigned long mmc_tx_singlecol_g;
	unsigned long mmc_tx_multicol_g;
	unsigned long mmc_tx_deferred;
	unsigned long mmc_tx_latecol;
	unsigned long mmc_tx_exesscol;
	unsigned long mmc_tx_carrier_error;
	unsigned long mmc_tx_octetcount_g;
	unsigned long mmc_tx_framecount_g;
	unsigned long mmc_tx_excessdef;
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
	unsigned long mmc_rx_align_error;
	unsigned long mmc_rx_run_error;
	unsigned long mmc_rx_jabber_error;
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
	unsigned long mmc_rx_outofrangetype;
	unsigned long mmc_rx_pause_frames;
	unsigned long mmc_rx_fifo_overflow;
	unsigned long mmc_rx_vlan_frames_gb;
	unsigned long mmc_rx_watchdog_error;
	unsigned long mmc_rx_receive_error;
	unsigned long mmc_rx_ctrl_frames_g;

	/* IPC */
	unsigned long mmc_rx_ipc_intr_mask;
	unsigned long mmc_rx_ipc_intr;

	/* IPv4 */
	unsigned long mmc_rx_ipv4_gd;
	unsigned long mmc_rx_ipv4_hderr;
	unsigned long mmc_rx_ipv4_nopay;
	unsigned long mmc_rx_ipv4_frag;
	unsigned long mmc_rx_ipv4_udsbl;

	/* IPV6 */
	unsigned long mmc_rx_ipv6_gd_octets;
	unsigned long mmc_rx_ipv6_hderr_octets;
	unsigned long mmc_rx_ipv6_nopay_octets;

	/* Protocols */
	unsigned long mmc_rx_udp_gd;
	unsigned long mmc_rx_udp_err;
	unsigned long mmc_rx_tcp_gd;
	unsigned long mmc_rx_tcp_err;
	unsigned long mmc_rx_icmp_gd;
	unsigned long mmc_rx_icmp_err;

	/* IPv4 */
	unsigned long mmc_rx_ipv4_gd_octets;
	unsigned long mmc_rx_ipv4_hderr_octets;
	unsigned long mmc_rx_ipv4_nopay_octets;
	unsigned long mmc_rx_ipv4_frag_octets;
	unsigned long mmc_rx_ipv4_udsbl_octets;

	/* IPV6 */
	unsigned long mmc_rx_ipv6_gd;
	unsigned long mmc_rx_ipv6_hderr;
	unsigned long mmc_rx_ipv6_nopay;

	/* Protocols */
	unsigned long mmc_rx_udp_gd_octets;
	unsigned long mmc_rx_udp_err_octets;
	unsigned long mmc_rx_tcp_gd_octets;
	unsigned long mmc_rx_tcp_err_octets;
	unsigned long mmc_rx_icmp_gd_octets;
	unsigned long mmc_rx_icmp_err_octets;
};

struct dwc_eqos_extra_stats {
	unsigned long q_re_alloc_rx_buf_failed[8];

	/* Tx/Rx IRQ error info */
	unsigned long tx_process_stopped_irq_n[8];
	unsigned long rx_process_stopped_irq_n[8];
	unsigned long tx_buf_unavailable_irq_n[8];
	unsigned long rx_buf_unavailable_irq_n[8];
	unsigned long rx_watchdog_irq_n;
	unsigned long fatal_bus_error_irq_n;
	/* Tx/Rx IRQ Events */
	unsigned long tx_normal_irq_n[8];
	unsigned long rx_normal_irq_n[8];
	unsigned long napi_poll_txq[8];
	unsigned long napi_poll_rxq[8];
	unsigned long tx_clean_n[8];
	/* Tx/Rx frames */
	unsigned long tx_pkt_n;
	unsigned long rx_pkt_n;
	unsigned long tx_vlan_pkt_n;
	unsigned long rx_vlan_pkt_n;
	unsigned long tx_timestamp_captured_n;
	unsigned long rx_timestamp_captured_n;
	unsigned long tx_tso_pkt_n;
	unsigned long rx_split_hdr_pkt_n;

	/* Tx/Rx frames per channels/queues */
	unsigned long q_tx_pkt_n[8];
	unsigned long q_rx_pkt_n[8];
};

struct dwc_eqos_prv_data {
	void __iomem *base_addr;
	struct clk *apb_pclk;
	struct clk *eqos_clk[EQOS_MAX_CLK];
	struct clk *rxclk_mux;
	struct clk *rxclk_internal;
	struct clk *rxclk_external;
	struct net_device *ndev;
	struct platform_device *pdev;

	spinlock_t lock;
	spinlock_t tx_lock;
	unsigned int mem_start_addr;
	unsigned int mem_size;
	int irq_number;
	struct hw_if_struct hw_if;
	struct desc_if_struct desc_if;
	//handling the workqueue
	struct workqueue_struct *restart_wq;
	struct work_struct restart_work;
	int cur_qinx;
	struct s_tx_error_counters tx_error_counters;
	struct s_rx_error_counters rx_error_counters;
	struct s_rx_pkt_features rx_pkt_features;
	struct s_tx_pkt_features tx_pkt_features;

	/* TX Queue */
	struct dwc_eqos_tx_queue *tx_queue;
	unsigned char tx_queue_cnt;
	unsigned int tx_coal_timer_us;

	/* RX Queue */
	struct dwc_eqos_rx_queue *rx_queue;
	unsigned char rx_queue_cnt;

	struct mii_bus *mii;
	struct phy_device *phydev;
	int oldlink;
	int speed;
	int oldduplex;
	int phyaddr;
	int bus_id;
	u32 dev_state;
	u32 interface;

	/* saving state for Wake-on-LAN */
	int wolopts;

	/* Helper macros for handling FLOW control in HW */
#define DWC_EQOS_FLOW_CTRL_OFF 0
#define DWC_EQOS_FLOW_CTRL_RX  1
#define DWC_EQOS_FLOW_CTRL_TX  2
#define DWC_EQOS_FLOW_CTRL_TX_RX (DWC_EQOS_FLOW_CTRL_TX |\
		DWC_EQOS_FLOW_CTRL_RX)

	unsigned int flow_ctrl;

	/* keeps track of previous programmed flow control options */
	unsigned int oldflow_ctrl;

	struct dwc_eqos_hw_features hw_feat;

	/* for sa(source address) insert/replace */
	u32 tx_sa_ctrl_via_desc;
	u32 tx_sa_ctrl_via_reg;
	unsigned char mac_addr[DWC_EQOS_MAC_ADDR_LEN];

	/* AXI parameters */
	unsigned int incr_incrx;
	unsigned int axi_pbl;
	unsigned int axi_worl;
	unsigned int axi_rorl;

	/* for hanlding jumbo frames and split header feature on rx path */
	int (*clean_rx)(struct dwc_eqos_prv_data *pdata, int quota,
			unsigned int qinx);
	int (*alloc_rx_buf)(struct dwc_eqos_prv_data *pdata,
			struct dwc_eqos_rx_buffer *buffer, gfp_t gfp);
	unsigned int rx_buffer_len;

	/* variable frame burst size */
	unsigned int drop_tx_pktburstcnt;
	/* counter for enabling MAC transmit at
	 * drop tx packet
	 */
	unsigned int mac_enable_count;

	struct dwc_eqos_mmc_counters mmc;
	struct dwc_eqos_extra_stats xstats;

	/* rx split header mode */
	unsigned char rx_split_hdr;

	/* for MAC loopback */
	unsigned int mac_loopback_mode;

	/* for hw time stamping */
	unsigned char hwts_tx_en;
	unsigned char hwts_rx_en;
	struct ptp_clock *ptp_clock;
	struct ptp_clock_info ptp_clock_ops;
	/* protects registers */
	spinlock_t ptp_lock;
	unsigned int default_addend;
	/* set to 1 if one nano second accuracy
	 * is enabled else set to zero
	 */
	bool one_nsec_accuracy;

	/* delayed work to read and pass tstamp to skb */
	struct sk_buff *tstamp_skb;
	struct delayed_work delayed_tstamp_work;

	/* for filtering */
	int max_hash_table_size;
	int max_addr_reg_cnt;

	/* L3/L4 filtering */
	unsigned int l3_l4_filter;

	unsigned char vlan_hash_filtering;
	/* 0 - if perfect and 1 - if hash filtering */
	unsigned int l2_filtering_mode;

	/* For handling PCS(TBI/RTBI/SGMII) and RGMII/SMII interface */
	unsigned int pcs_link;
	unsigned int pcs_duplex;
	unsigned int pcs_speed;
	unsigned int pause;
	unsigned int duplex;
	unsigned int lp_pause;
	unsigned int lp_duplex;

	/* arp offload enable/disable. */
	u32 arp_offload;

	/* set to 1 when ptp offload is enabled, else 0. */
	u32 ptp_offload;
	/* ptp offloading mode - ORDINARY_SLAVE, ORDINARY_MASTER,
	 * TRANSPARENT_SLAVE, TRANSPARENT_MASTER, PTOP_TRANSPERENT.
	 */
	u32 ptp_offloading_mode;

	/* For configuring double VLAN via descriptor/reg */
	int inner_vlan_tag;
	int outer_vlan_tag;
	/* op_type will be
	 * 0/1/2/3 for none/delet/insert/replace respectively
	 */
	int op_type;
	/* in_out will be
	 * 1/2/3 for outer/inner/both respectively.
	 */
	int in_out;
	/* 0 for via registers and 1 for via descriptor */
	int via_reg_or_desc;

	/* this variable will hold vlan table value if vlan hash filtering
	 * is enabled else hold vlan id that is programmed in HW. Same is
	 * used to configure back into HW when device is reset during
	 * jumbo/split-header features.
	 */
	unsigned int vlan_ht_or_id;

	/* Used when LRO is enabled,
	 * set to 1 if skb has TCP payload else set to 0
	 */
	int tcp_pkt;
	int if_idx;
};

enum e_int_state {
	ESAVE,
	ERESTORE
};

/* Function prototypes*/

void dwc_eqos_init_function_ptrs_dev(struct hw_if_struct *hw_if);
void dwc_eqos_init_function_ptrs_desc(struct desc_if_struct *desc_if);
struct net_device_ops *dwc_eqos_get_netdev_ops(void);
struct ethtool_ops *dwc_eqos_get_ethtool_ops(void);
int dwc_eqos_poll_rxq(struct napi_struct *napi, int budget);
int dwc_eqos_poll_txq(struct napi_struct *napi, int budget);

void dwc_eqos_get_pdata(struct dwc_eqos_prv_data *pdata);

/* Debugfs related functions. */
int create_debug_files(void);
void remove_debug_files(void);

int dwc_eqos_mdio_register(struct net_device *dev);
void dwc_eqos_mdio_unregister(struct net_device *dev);
int dwc_eqos_mdio_read_direct(struct dwc_eqos_prv_data *pdata,
		int phyaddr, int phyreg, int *phydata);
int dwc_eqos_mdio_write_direct(struct dwc_eqos_prv_data *pdata,
		int phyaddr, int phyreg, int phydata);
int dwc_eqos_fixed_phy_register(struct net_device *ndev);
void dbgpr_regs(void);
void dump_phy_registers(struct dwc_eqos_prv_data *pdata);
void dump_tx_desc(struct dwc_eqos_prv_data *pdata, int first_desc_idx,
		int last_desc_idx, int flag, unsigned int qinx);
void dump_rx_desc(unsigned int qinx, struct s_rx_normal_desc *desc, int cur_rx);
void print_pkt(struct sk_buff *skb, int len, bool tx_rx, int desc_idx);
void dwc_eqos_get_all_hw_features(struct dwc_eqos_prv_data *pdata);
void dwc_eqos_print_all_hw_features(struct dwc_eqos_prv_data *pdata);
void dwc_eqos_initialize_work_queue(struct dwc_eqos_prv_data *pdata);
void dwc_eqos_configure_flow_ctrl(struct dwc_eqos_prv_data *pdata);
int dwc_eqos_powerup(struct net_device *dev, unsigned int caller);
int dwc_eqos_powerdown(struct net_device *dev, unsigned int wakeup_type,
		unsigned int caller);
u32 dwc_eqos_usec2riwt(u32 usec, struct dwc_eqos_prv_data *pdata);
void dwc_eqos_init_rx_coalesce(struct dwc_eqos_prv_data *pdata);
void dwc_eqos_init_tx_coalesce(struct dwc_eqos_prv_data *pdata);
void dwc_eqos_enable_all_ch_rx_interrpt(struct dwc_eqos_prv_data *pdata);
void dwc_eqos_disable_all_ch_rx_interrpt(struct dwc_eqos_prv_data *pdata);
void dwc_eqos_update_rx_errors(struct net_device *dev,
		unsigned int rx_status);
unsigned char get_tx_queue_count(void __iomem *base);
unsigned char get_rx_queue_count(void __iomem *base);
void dwc_eqos_mmc_read(struct dwc_eqos_prv_data *pdata,
		struct dwc_eqos_mmc_counters *mmc);
unsigned int dwc_eqos_get_total_desc_cnt(struct dwc_eqos_prv_data *pdata,
		struct sk_buff *skb, unsigned int qinx);

int dwc_eqos_ptp_init(struct dwc_eqos_prv_data *pdata);
void dwc_eqos_ptp_remove(struct dwc_eqos_prv_data *pdata);
phy_interface_t dwc_eqos_get_phy_interface(struct dwc_eqos_prv_data *pdata);

/* For debug prints*/
#define DRV_NAME "dwc_eth_qos_drv.c"

#ifdef YDEBUG
#define DBGPR(x...) pr_alert(x)
#define DBGPR_REGS() dbgpr_regs()
#define DBGPHY_REGS(x...) dump_phy_registers(x)
#else
#define DBGPR(x...) do { } while (0)
#define DBGPR_REGS() do { } while (0)
#define DBGPHY_REGS(x...) do { } while (0)
#endif

#ifdef YDEBUG_MDIO
#define DBGPR_MDIO(x...) pr_alert(x)
#else
#define DBGPR_MDIO(x...) do {} while (0)
#endif

#ifdef YDEBUG_PTP
#define DBGPR_PTP(x...) pr_alert(x)
#else
#define DBGPR_PTP(x...) do {} while (0)
#endif

#ifdef YDEBUG_FILTER
#define DBGPR_FILTER(x...) pr_alert(x)
#else
#define DBGPR_FILTER(x...) do {} while (0)
#endif

#endif
