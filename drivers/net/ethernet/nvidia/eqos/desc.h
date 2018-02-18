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
#ifndef __EQOS_DESC_H__

#define __EQOS_DESC_H__

static INT allocate_buffer_and_desc(struct eqos_prv_data *);
static void free_buffer_and_desc(struct eqos_prv_data *);

static void eqos_wrapper_tx_descriptor_init(struct eqos_prv_data
						   *pdata);

static void eqos_wrapper_tx_descriptor_init_single_q(struct
							    eqos_prv_data
							    *pdata, UINT qinx);

static void eqos_wrapper_rx_descriptor_init(struct eqos_prv_data
						   *pdata);

static void eqos_wrapper_rx_descriptor_init_single_q(struct
							    eqos_prv_data
							    *pdata, UINT qinx);

static int eqos_get_skb_hdr(struct sk_buff *skb, void **iphdr,
				   void **tcph, u64 *hdr_flags, void *priv);

static void eqos_tx_free_mem(struct eqos_prv_data *);

static void eqos_rx_free_mem(struct eqos_prv_data *);

static int tx_swcx_alloc(struct net_device *, struct sk_buff *);
static void tx_swcx_free(struct eqos_prv_data *, struct tx_swcx_desc *);

static void eqos_unmap_rx_skb(struct eqos_prv_data *,
				     struct rx_swcx_desc *);

static void eqos_re_alloc_skb(struct eqos_prv_data *pdata,
					UINT qinx);

static void eqos_tx_desc_free_mem(struct eqos_prv_data *pdata,
					 UINT tx_q_cnt);

static void eqos_tx_buf_free_mem(struct eqos_prv_data *pdata,
					UINT tx_q_cnt);

static void eqos_rx_desc_free_mem(struct eqos_prv_data *pdata,
					 UINT rx_q_cnt);

static void eqos_rx_buf_free_mem(struct eqos_prv_data *pdata,
					UINT rx_q_cnt);

static void eqos_rx_skb_free_mem(struct eqos_prv_data *pdata,
					UINT rx_qcnt);

static void eqos_tx_skb_free_mem(struct eqos_prv_data *pdata,
					UINT tx_qcnt);
#endif
