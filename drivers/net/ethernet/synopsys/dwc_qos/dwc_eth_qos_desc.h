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

#ifndef __DWC_ETH_QOS_DESC_H__
#define __DWC_ETH_QOS_DESC_H__

static int allocate_buffer_and_desc(struct dwc_eqos_prv_data *);

static void dwc_eqos_wrapper_tx_descriptor_init(struct dwc_eqos_prv_data
						*pdata);

static void dwc_eqos_wrapper_tx_descriptor_init_single_q(
					struct dwc_eqos_prv_data *pdata,
					unsigned int qinx);

static void dwc_eqos_wrapper_rx_descriptor_init(struct dwc_eqos_prv_data
						*pdata);

static void dwc_eqos_wrapper_rx_descriptor_init_single_q(
					struct dwc_eqos_prv_data *pdata,
					unsigned int qinx);

static void dwc_eqos_tx_free_mem(struct dwc_eqos_prv_data *);

static void dwc_eqos_rx_free_mem(struct dwc_eqos_prv_data *);

static unsigned int dwc_eqos_map_skb(struct net_device *, struct sk_buff *);

static void dwc_eqos_unmap_tx_skb(struct dwc_eqos_prv_data *,
				  struct dwc_eqos_tx_buffer *);

static void dwc_eqos_unmap_rx_skb(struct dwc_eqos_prv_data *,
				  struct dwc_eqos_rx_buffer *);

static void dwc_eqos_re_alloc_skb(
				struct dwc_eqos_prv_data *pdata,
				unsigned int qinx);

static void dwc_eqos_tx_desc_free_mem(struct dwc_eqos_prv_data *pdata,
				      unsigned int tx_q_cnt);

static void dwc_eqos_tx_buf_free_mem(struct dwc_eqos_prv_data *pdata,
				     unsigned int tx_q_cnt);

static void dwc_eqos_rx_desc_free_mem(struct dwc_eqos_prv_data *pdata,
				      unsigned int rx_q_cnt);

static void dwc_eqos_rx_buf_free_mem(struct dwc_eqos_prv_data *pdata,
				     unsigned int rx_q_cnt);

static void dwc_eqos_rx_skb_free_mem(struct dwc_eqos_prv_data *pdata,
				     unsigned int rx_qcnt);

static void dwc_eqos_tx_skb_free_mem(struct dwc_eqos_prv_data *pdata,
				     unsigned int tx_qcnt);
#endif
