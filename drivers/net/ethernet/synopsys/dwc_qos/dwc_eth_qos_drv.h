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

#ifndef __DWC_ETH_QOS_DRV_H__
#define __DWC_ETH_QOS_DRV_H__

static int dwc_eqos_open(struct net_device *);

static int dwc_eqos_close(struct net_device *);

static void dwc_eqos_set_rx_mode(struct net_device *);

static int dwc_eqos_start_xmit(struct sk_buff *, struct net_device *);

static struct net_device_stats *dwc_eqos_get_stats(struct net_device *);

#ifdef CONFIG_NET_POLL_CONTROLLER
static void dwc_eqos_poll_controller(struct net_device *);
#endif				/*end of CONFIG_NET_POLL_CONTROLLER */

static int dwc_eqos_set_features(struct net_device *dev,
				 netdev_features_t features);

static netdev_features_t dwc_eqos_fix_features(struct net_device *dev,
					       netdev_features_t features);

int dwc_eqos_configure_remotewakeup(struct net_device *,
				    struct ifr_data_struct *);

static void dwc_eqos_program_dcb_algorithm(struct dwc_eqos_prv_data *pdata,
					   struct ifr_data_struct *req);

static void dwc_eqos_program_avb_algorithm(struct dwc_eqos_prv_data *pdata,
					   struct ifr_data_struct *req);

static void dwc_eqos_config_tx_pbl(struct dwc_eqos_prv_data *pdata,
				   unsigned int tx_pbl, unsigned int ch_no);
static void dwc_eqos_config_rx_pbl(struct dwc_eqos_prv_data *pdata,
				   unsigned int rx_pbl, unsigned int ch_no);

static int dwc_eqos_handle_prv_ioctl(struct dwc_eqos_prv_data *pdata,
				     struct ifr_data_struct *req);

static int dwc_eqos_ioctl(struct net_device *, struct ifreq *, int);

irqreturn_t dwc_eqos_isr_sw_dwc_eqos(int, void *);

static int dwc_eqos_change_mtu(struct net_device *dev, int new_mtu);

static int dwc_eqos_clean_split_hdr_rx_irq(struct dwc_eqos_prv_data *pdata,
					   int quota, unsigned int qinx);

static int dwc_eqos_clean_jumbo_rx_irq(struct dwc_eqos_prv_data *pdata,
				       int quota, unsigned int qinx);

static int dwc_eqos_clean_rx_irq(struct dwc_eqos_prv_data *pdata,
				 int quota, unsigned int qinx);

static void dwc_eqos_consume_page(struct dwc_eqos_rx_buffer *buffer,
				  struct sk_buff *skb,
				  u16 length, u16 buf2_len);

static void dwc_eqos_receive_skb(struct dwc_eqos_prv_data *pdata,
				 struct net_device *dev,
				 struct sk_buff *skb,
				 unsigned int qinx);

static void dwc_eqos_configure_rx_fun_ptr(struct dwc_eqos_prv_data
					  *pdata);

static int dwc_eqos_alloc_split_hdr_rx_buf(struct dwc_eqos_prv_data *pdata,
					   struct dwc_eqos_rx_buffer *buffer,
					   gfp_t gfp);

static int dwc_eqos_alloc_jumbo_rx_buf(struct dwc_eqos_prv_data *pdata,
				       struct dwc_eqos_rx_buffer *buffer,
				       gfp_t gfp);

static int dwc_eqos_alloc_rx_buf(struct dwc_eqos_prv_data *pdata,
				 struct dwc_eqos_rx_buffer *buffer,
				 gfp_t gfp);

static void dwc_eqos_default_common_confs(struct dwc_eqos_prv_data
					  *pdata);
static void dwc_eqos_default_tx_confs(struct dwc_eqos_prv_data *pdata);
static void dwc_eqos_default_tx_confs_single_q(struct dwc_eqos_prv_data
					       *pdata, unsigned int qinx);
static void dwc_eqos_default_rx_confs(struct dwc_eqos_prv_data *pdata);
static void dwc_eqos_default_rx_confs_single_q(struct dwc_eqos_prv_data
					       *pdata, unsigned int qinx);

int dwc_eqos_poll(struct dwc_eqos_prv_data *pdata, int budget, int qinx);

inline unsigned int dwc_eqos_reg_read(volatile unsigned long *ptr);

static int dwc_eqos_vlan_rx_add_vid(struct net_device *dev,
				    __be16 proto, u16 vid);
static int dwc_eqos_vlan_rx_kill_vid(struct net_device *dev,
				     __be16 proto, u16 vid);

#endif
