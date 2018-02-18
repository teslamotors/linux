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
 * =========================================================================
 */
/*
 * Copyright (c) 2015, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef __EQOS_DRV_H__

#define __EQOS_DRV_H__

static int eqos_open(struct net_device *);

static int eqos_close(struct net_device *);

static void eqos_set_rx_mode(struct net_device *);

static int eqos_start_xmit(struct sk_buff *, struct net_device *);

static void process_tx_completions(struct net_device *,
				   struct eqos_prv_data *,
				   uint qinx);

static struct net_device_stats *eqos_get_stats(struct net_device *);

static int eqos_set_features(struct net_device *dev,
	netdev_features_t features);

INT eqos_configure_remotewakeup(struct net_device *,
				       struct ifr_data_struct *);

static void eqos_program_dcb_algorithm(struct eqos_prv_data *pdata,
		struct ifr_data_struct *req);

static void eqos_program_avb_algorithm(struct eqos_prv_data *pdata,
		struct ifr_data_struct *req);

static void eqos_config_tx_pbl(struct eqos_prv_data *pdata,
				      UINT tx_pbl, UINT ch_no);
static void eqos_config_rx_pbl(struct eqos_prv_data *pdata,
				      UINT rx_pbl, UINT ch_no);

static int eqos_handle_prv_ioctl(struct eqos_prv_data *pdata,
					struct ifr_data_struct *req);

static int eqos_ioctl(struct net_device *, struct ifreq *, int);

irqreturn_t eqos_isr(int, void *);

static INT eqos_change_mtu(struct net_device *dev, INT new_mtu);

static int process_rx_completions(struct eqos_prv_data *pdata,
				  int quota, UINT qinx);

static void eqos_receive_skb(struct eqos_prv_data *pdata,
				    struct net_device *dev,
				    struct sk_buff *skb,
				    UINT qinx);

static void eqos_configure_rx_fun_ptr(struct eqos_prv_data
					     *pdata);


static int eqos_alloc_rx_buf(struct eqos_prv_data *pdata,
				    struct rx_swcx_desc *buffer,
				    gfp_t gfp);

static void eqos_default_common_confs(struct eqos_prv_data
					     *pdata);
static void eqos_default_tx_confs(struct eqos_prv_data *pdata);
static void eqos_default_tx_confs_single_q(struct eqos_prv_data
						  *pdata, UINT qinx);
static void eqos_default_rx_confs(struct eqos_prv_data *pdata);
static void eqos_default_rx_confs_single_q(struct eqos_prv_data
						  *pdata, UINT qinx);

static void eqos_mmc_setup(struct eqos_prv_data *pdata);
inline unsigned int eqos_reg_read(volatile ULONG *ptr);
static int eqos_vlan_rx_add_vid(struct net_device *dev, __be16 proto, u16 vid);
static int eqos_vlan_rx_kill_vid(struct net_device *dev, __be16 proto, u16 vid);
void eqos_stop_dev(struct eqos_prv_data *pdata);
void eqos_start_dev(struct eqos_prv_data *pdata);
#endif
