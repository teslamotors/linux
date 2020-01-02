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

#ifndef __DWC_EQOS_YAPPHDR_H__
#define __DWC_EQOS_YAPPHDR_H__

#define DWC_EQOS_MAX_TX_QUEUE_CNT 8
#define DWC_EQOS_MAX_RX_QUEUE_CNT 8

/* Private IOCTL for handling device specific task */
#define DWC_EQOS_PRV_IOCTL	SIOCDEVPRIVATE

#define DWC_EQOS_POWERUP_MAGIC_CMD	1
#define DWC_EQOS_POWERDOWN_MAGIC_CMD	2
#define DWC_EQOS_POWERUP_REMOTE_WAKEUP_CMD	3
#define DWC_EQOS_POWERDOWN_REMOTE_WAKEUP_CMD	4

/* for TX and RX threshold configures */
#define DWC_EQOS_RX_THRESHOLD_CMD	5
#define DWC_EQOS_TX_THRESHOLD_CMD	6

/* for TX and RX Store and Forward mode configures */
#define DWC_EQOS_RSF_CMD	7
#define DWC_EQOS_TSF_CMD	8

/* for TX DMA Operate on Second Frame mode configures */
#define DWC_EQOS_OSF_CMD	9

/* for TX and RX PBL configures */
#define DWC_EQOS_TX_PBL_CMD	10
#define DWC_EQOS_RX_PBL_CMD	11

/* INCR and INCRX mode */
#define DWC_EQOS_INCR_INCRX_CMD	12

/* for MAC Double VLAN Processing config */
#define DWC_EQOS_DVLAN_TX_PROCESSING_CMD		13
#define DWC_EQOS_DVLAN_RX_PROCESSING_CMD		14
#define DWC_EQOS_SVLAN_CMD				15

//Manju: Remove the below defines
/* RX/TX VLAN */
//#define DWC_EQOS_RX_OUTER_VLAN_STRIPPING_CMD	13
//#define DWC_EQOS_RX_INNER_VLAN_STRIPPING_CMD	14
//#define DWC_EQOS_TX_VLAN_DESC_CMD	15
//#define DWC_EQOS_TX_VLAN_REG_CMD	16

/* SA on TX */
#define DWC_EQOS_SA0_DESC_CMD	17
#define DWC_EQOS_SA1_DESC_CMD	18
#define DWC_EQOS_SA0_REG_CMD		19
#define DWC_EQOS_SA1_REG_CMD		20

/* CONTEX desc setup control */
#define DWC_EQOS_SETUP_CONTEXT_DESCRIPTOR 21

/* Packet generation */
#define DWC_EQOS_PG_TEST		22

/* TX/RX channel/queue count */
#define DWC_EQOS_GET_TX_QCNT		23
#define DWC_EQOS_GET_RX_QCNT		24

/* Line speed */
#define DWC_EQOS_GET_CONNECTED_SPEED		25

/* DCB/AVB algorithm */
#define DWC_EQOS_DCB_ALGORITHM		26
#define DWC_EQOS_AVB_ALGORITHM		27

/* L3/L4 filter */
#define DWC_EQOS_L3_L4_FILTER_CMD		29
/* IPv4/6 and TCP/UDP filtering */
#define DWC_EQOS_IPV4_FILTERING_CMD		30
#define DWC_EQOS_IPV6_FILTERING_CMD		31
#define DWC_EQOS_UDP_FILTERING_CMD		32
#define DWC_EQOS_TCP_FILTERING_CMD		33
/* VLAN filtering */
#define DWC_EQOS_VLAN_FILTERING_CMD		34
/* L2 DA filtering */
#define DWC_EQOS_L2_DA_FILTERING_CMD		35
/* for AXI PBL configures */
#define DWC_EQOS_AXI_PBL_CMD			37
/* for AXI Write Outstanding Request Limit configures */
#define DWC_EQOS_AXI_WORL_CMD		38
/* for AXI Read Outstanding Request Limit configures */
#define DWC_EQOS_AXI_RORL_CMD		39
/* for MAC LOOPBACK configuration */
#define DWC_EQOS_MAC_LOOPBACK_MODE_CMD	40
/* PFC(Priority Based Flow Control) mode */
#define DWC_EQOS_PFC_CMD			41
/* for PTP OFFLOADING configuration */
#define DWC_EQOS_PTPOFFLOADING_CMD			42

#define DWC_EQOS_RWK_FILTER_LENGTH	8

/* List of command errors driver can set */
#define	DWC_EQOS_NO_HW_SUPPORT	-1
#define	DWC_EQOS_CONFIG_FAIL	-3
#define	DWC_EQOS_CONFIG_SUCCESS	0

/* RX THRESHOLD operations */
#define DWC_EQOS_RX_THRESHOLD_32	0x1
#define DWC_EQOS_RX_THRESHOLD_64	0x0
#define DWC_EQOS_RX_THRESHOLD_96	0x2
#define DWC_EQOS_RX_THRESHOLD_128	0x3

/* TX THRESHOLD operations */
#define DWC_EQOS_TX_THRESHOLD_32	0x1
#define DWC_EQOS_TX_THRESHOLD_64	0x0
#define DWC_EQOS_TX_THRESHOLD_96	0x2
#define DWC_EQOS_TX_THRESHOLD_128	0x3
#define DWC_EQOS_TX_THRESHOLD_192	0x4
#define DWC_EQOS_TX_THRESHOLD_256	0x5
#define DWC_EQOS_TX_THRESHOLD_384	0x6
#define DWC_EQOS_TX_THRESHOLD_512	0x7

/* TX and RX Store and Forward Mode operations */
#define DWC_EQOS_RSF_DISABLE	0x0
#define DWC_EQOS_RSF_ENABLE	0x1

#define DWC_EQOS_TSF_DISABLE	0x0
#define DWC_EQOS_TSF_ENABLE	0x1

/* TX DMA Operate on Second Frame operations */
#define DWC_EQOS_OSF_DISABLE	0x0
#define DWC_EQOS_OSF_ENABLE	0x1

/* INCR and INCRX mode */
#define DWC_EQOS_INCR_ENABLE		0x1
#define DWC_EQOS_INCRX_ENABLE	0x0

/* TX and RX PBL operations */
#define DWC_EQOS_PBL_1	1
#define DWC_EQOS_PBL_2	2
#define DWC_EQOS_PBL_4	4
#define DWC_EQOS_PBL_8	8
#define DWC_EQOS_PBL_16	16
#define DWC_EQOS_PBL_32	32
#define DWC_EQOS_PBL_64	64	/* 8 x 8 */
#define DWC_EQOS_PBL_128	128	/* 8 x 16 */
#define DWC_EQOS_PBL_256	256	/* 8 x 32 */

/* AXI operations */
#define DWC_EQOS_AXI_PBL_4	0x2
#define DWC_EQOS_AXI_PBL_8	0x6
#define DWC_EQOS_AXI_PBL_16	0xE
#define DWC_EQOS_AXI_PBL_32	0x1E
#define DWC_EQOS_AXI_PBL_64	0x3E
#define DWC_EQOS_AXI_PBL_128	0x7E
#define DWC_EQOS_AXI_PBL_256	0xFE

#define DWC_EQOS_MAX_AXI_WORL 31
#define DWC_EQOS_MAX_AXI_RORL 31

/* RX VLAN operations */
/* Do not strip VLAN tag from received pkt */
#define DWC_EQOS_RX_NO_VLAN_STRIP	0x0
/* Strip VLAN tag if received pkt pass VLAN filter */
#define DWC_EQOS_RX_VLAN_STRIP_IF_FILTER_PASS  0x1
/* Strip VLAN tag if received pkt fial VLAN filter */
#define DWC_EQOS_RX_VLAN_STRIP_IF_FILTER_FAIL  0x2
/* Strip VALN tag always from received pkt */
#define DWC_EQOS_RX_VLAN_STRIP_ALWAYS	0x3

/* TX VLAN operations */
/* Do not add a VLAN tag dring pkt transmission */
#define DWC_EQOS_TX_VLAN_TAG_NONE	0x0
/* Remove the VLAN tag from the pkt before transmission */
#define DWC_EQOS_TX_VLAN_TAG_DELETE	0x1
/* Insert the VLAN tag into pkt to be transmitted */
#define DWC_EQOS_TX_VLAN_TAG_INSERT	0x2
/* Replace the VLAN tag into pkt to be transmitted */
#define DWC_EQOS_TX_VLAN_TAG_REPLACE	0x3

/* RX split header operations */
#define DWC_EQOS_RX_SPLIT_HDR_DISABLE 0x0
#define DWC_EQOS_RX_SPLIT_HDR_ENABLE 0x1

/* L3/L4 filter operations */
#define DWC_EQOS_L3_L4_FILTER_DISABLE 0x0
#define DWC_EQOS_L3_L4_FILTER_ENABLE 0x1

/* Loopback mode */
#define DWC_EQOS_MAC_LOOPBACK_DISABLE 0x0
#define DWC_EQOS_MAC_LOOPBACK_ENABLE 0x1

/* PFC(Priority Based Flow Control) mode */
#define DWC_EQOS_PFC_DISABLE 0x0
#define DWC_EQOS_PFC_ENABLE 0x1

#define DWC_EQOS_MAC0REG 0
#define DWC_EQOS_MAC1REG 1

/* Do not include the SA */
#define DWC_EQOS_SA0_NONE		((DWC_EQOS_MAC0REG << 2) | 0)
/* Include/Insert the SA with value given in MAC Addr 0 Reg */
#define DWC_EQOS_SA0_DESC_INSERT	((DWC_EQOS_MAC0REG << 2) | 1)
/* Replace the SA with the value given in MAC Addr 0 Reg */
#define DWC_EQOS_SA0_DESC_REPLACE	((DWC_EQOS_MAC0REG << 2) | 2)
/* Include/Insert the SA with value given in MAC Addr 0 Reg */
#define DWC_EQOS_SA0_REG_INSERT	((DWC_EQOS_MAC0REG << 2) | 2)
/* Replace the SA with the value given in MAC Addr 0 Reg */
#define DWC_EQOS_SA0_REG_REPLACE	((DWC_EQOS_MAC0REG << 2) | 3)

/* Do not include the SA */
#define DWC_EQOS_SA1_NONE		((DWC_EQOS_MAC1REG << 2) | 0)
/* Include/Insert the SA with value given in MAC Addr 1 Reg */
#define DWC_EQOS_SA1_DESC_INSERT	((DWC_EQOS_MAC1REG << 2) | 1)
/* Replace the SA with the value given in MAC Addr 1 Reg */
#define DWC_EQOS_SA1_DESC_REPLACE	((DWC_EQOS_MAC1REG << 2) | 2)
/* Include/Insert the SA with value given in MAC Addr 1 Reg */
#define DWC_EQOS_SA1_REG_INSERT	((DWC_EQOS_MAC1REG << 2) | 2)
/* Replace the SA with the value given in MAC Addr 1 Reg */
#define DWC_EQOS_SA1_REG_REPLACE	((DWC_EQOS_MAC1REG << 2) | 3)

/* value for bandwidth calculation */
#define DWC_EQOS_MAX_WFQ_WEIGHT	0X7FFF

#define DWC_EQOS_MAX_INT_FRAME_SIZE (1024 * 16)

enum e_dwc_eqos_dma_tx_arb_algo {
	EDWC_EQOS_DMA_TX_FP = 0,
	EDWC_EQOS_DMA_TX_WSP = 1,
	EDWC_EQOS_DMA_TX_WRR = 2,
};

enum e_dwc_eqos_dcb_algorithm {
	EDWC_EQOS_DCB_WRR = 0,
	EDWC_EQOS_DCB_WFQ = 1,
	EDWC_EQOS_DCB_DWRR = 2,
	EDWC_EQOS_DCB_SP = 3,
};

enum e_dwc_eqos_avb_algorithm {
	EDWC_EQOS_AVB_SP = 0,
	EDWC_EQOS_AVB_CBS = 1,
};

enum e_dwc_eqos_queue_operating_mode {
	EDWC_EQOS_QDISABLED = 0x0,
	EDWC_EQOS_QAVB,
	EDWC_EQOS_QDCB,
	EDWC_EQOS_QGENERIC
};

/* common data structure between driver and application for
 * sharing info through ioctl
 */
struct ifr_data_struct {
	unsigned int flags;
	/* dma channel no to be configured */
	unsigned int qinx;
	unsigned int cmd;
	unsigned int context_setup;
	unsigned int connected_speed;
	unsigned int rwk_filter_values[DWC_EQOS_RWK_FILTER_LENGTH];
	unsigned int rwk_filter_length;
	int command_error;
	int test_done;
	void *ptr;
};

struct dwc_eqos_dcb_algorithm {
	unsigned int qinx;
	unsigned int algorithm;
	unsigned int weight;
	enum e_dwc_eqos_queue_operating_mode op_mode;
};

struct dwc_eqos_avb_algorithm {
	unsigned int qinx;
	unsigned int algorithm;
	unsigned int cc;
	unsigned int idle_slope;
	unsigned int send_slope;
	unsigned int hi_credit;
	unsigned int low_credit;
	enum e_dwc_eqos_queue_operating_mode op_mode;
};

struct dwc_eqos_l3_l4_filter {
	/* 0, 1,2,3,4,5,6 or 7*/
	int filter_no;
	/* 0 - disable and 1 - enable */
	int filter_enb_dis;
	/* 0 - src addr/port and 1- dst addr/port match */
	int src_dst_addr_match;
	/* 0 - perfect and 1 - inverse match filtering */
	int perfect_inverse_match;
	/* To hold source/destination IPv4 addresses */
	unsigned char ip4_addr[4];
	/* holds single IPv6 addresses */
	unsigned short ip6_addr[8];

	/* TCP/UDP src/dst port number */
	unsigned short port_no;
};

struct dwc_eqos_vlan_filter {
	/* 0 - disable and 1 - enable */
	int filter_enb_dis;
	/* 0 - perfect and 1 - hash filtering */
	int perfect_hash;
	/* 0 - perfect and 1 - inverse matching */
	int perfect_inverse_match;
};

struct dwc_eqos_l2_da_filter {
	/* 0 - perfect and 1 - hash filtering */
	int perfect_hash;
	/* 0 - perfect and 1 - inverse matching */
	int perfect_inverse_match;
};

struct dwc_eqos_arp_offload {
	unsigned char ip_addr[4];
};

#define DWC_EQOS_VIA_REG	0
#define DWC_EQOS_VIA_DESC	1

/* for MAC Double VLAN Processing config */
#define DWC_EQOS_DVLAN_OUTER	(1)
#define DWC_EQOS_DVLAN_INNER	(1 << 1)
#define DWC_EQOS_DVLAN_BOTH	(DWC_EQOS_DVLAN_OUTER | DWC_EQOS_DVLAN_INNER)

#define DWC_EQOS_DVLAN_NONE	0
#define DWC_EQOS_DVLAN_DELETE	1
#define DWC_EQOS_DVLAN_INSERT	2
#define DWC_EQOS_DVLAN_REPLACE	3

#define DWC_EQOS_DVLAN_DISABLE	0
#define DWC_EQOS_DVLAN_ENABLE	1

struct dwc_eqos_config_dvlan {
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
};

/* for PTP offloading configuration */
#define DWC_EQOS_PTP_OFFLOADING_DISABLE	0
#define DWC_EQOS_PTP_OFFLOADING_ENABLE	1

#define DWC_EQOS_PTP_ORDINARY_SLAVE			1
#define DWC_EQOS_PTP_ORDINARY_MASTER			2
#define DWC_EQOS_PTP_TRASPARENT_SLAVE		3
#define DWC_EQOS_PTP_TRASPARENT_MASTER		4
#define DWC_EQOS_PTP_PEER_TO_PEER_TRANSPARENT	5

struct dwc_eqos_config_ptpoffloading {
	int en_dis;
	int mode;
	int domain_num;
	int mc_uc;
};

#endif
