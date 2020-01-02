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

#include "dwc_eth_qos_yheader.h"
#include "dwc_eth_qos_desc.h"
#include "dwc_eth_qos_yregacc.h"

/* This function is used to free the transmit descriptor memory.
 *
 * param[in] pdata - pointer to private data structure.
 *
 * retval void.
 */
static void dwc_eqos_tx_desc_free_mem(struct dwc_eqos_prv_data *pdata,
				      unsigned int tx_qcnt)
{
	struct dwc_eqos_tx_wrapper_descriptor *desc_data = NULL;
	unsigned int qinx;

	DBGPR("-->dwc_eqos_tx_desc_free_mem: tx_qcnt = %d\n", tx_qcnt);

	for (qinx = 0; qinx < tx_qcnt; qinx++) {
		desc_data = GET_TX_WRAPPER_DESC(qinx);

		if (GET_TX_DESC_PTR(qinx, 0)) {
			dma_free_coherent(
				pdata->ndev->dev.parent,
				(sizeof(struct s_tx_normal_desc) * TX_DESC_CNT),
				GET_TX_DESC_PTR(qinx, 0),
				GET_TX_DESC_DMA_ADDR(qinx, 0));
			GET_TX_DESC_PTR(qinx, 0) = NULL;
		}
	}

	DBGPR("<--dwc_eqos_tx_desc_free_mem\n");
}

/* This function is used to free the receive descriptor memory.
 *
 * param[in] pdata - pointer to private data structure.
 *
 * retval void.
 */
static void dwc_eqos_rx_desc_free_mem(struct dwc_eqos_prv_data *pdata,
				      unsigned int rx_qcnt)
{
	struct dwc_eqos_rx_wrapper_descriptor *desc_data = NULL;
	unsigned int qinx = 0;

	DBGPR("-->dwc_eqos_rx_desc_free_mem: rx_qcnt = %d\n", rx_qcnt);

	for (qinx = 0; qinx < rx_qcnt; qinx++) {
		desc_data = GET_RX_WRAPPER_DESC(qinx);

		if (GET_RX_DESC_PTR(qinx, 0)) {
			dma_free_coherent(
				pdata->ndev->dev.parent,
				(sizeof(struct s_rx_normal_desc) * RX_DESC_CNT),
				GET_RX_DESC_PTR(qinx, 0),
				GET_RX_DESC_DMA_ADDR(qinx, 0));
			GET_RX_DESC_PTR(qinx, 0) = NULL;
		}
	}

	DBGPR("<--dwc_eqos_rx_desc_free_mem\n");
}

/* This function allocates the queue structure memory.
 *
 * param[in] pdata - pointer to private data structure.
 * return integer
 *
 * retval 0 on success & -ve number on failure.
 */
static int dwc_eqos_alloc_queue_struct(struct dwc_eqos_prv_data *pdata)
{
	int ret = 0;

	DBGPR("-->dwc_eqos_alloc_queue_struct: tx_queue_cnt = %d,"\
		"rx_queue_cnt = %d\n",
		pdata->tx_queue_cnt, pdata->rx_queue_cnt);

	pdata->tx_queue = kzalloc(
		sizeof(struct dwc_eqos_tx_queue) * pdata->tx_queue_cnt,
		GFP_KERNEL);

	if (!pdata->tx_queue) {
		pr_alert("ERROR: Unable to allocate Tx queue structure\n");
		ret = -ENOMEM;
		goto err_out_tx_q_alloc_failed;
	}

	pdata->rx_queue = kzalloc(
		sizeof(struct dwc_eqos_rx_queue) * pdata->rx_queue_cnt,
		GFP_KERNEL);

	if (!pdata->rx_queue) {
		pr_alert("ERROR: Unable to allocate Rx queue structure\n");
		ret = -ENOMEM;
		goto err_out_rx_q_alloc_failed;
	}

	DBGPR("<--dwc_eqos_alloc_queue_struct\n");

	return ret;

err_out_rx_q_alloc_failed:
	kfree(pdata->tx_queue);

err_out_tx_q_alloc_failed:
	return ret;
}

/* This function free the queue structure memory.
 *
 * param[in] pdata - pointer to private data structure.
 * return void
 */
static void dwc_eqos_free_queue_struct(struct dwc_eqos_prv_data *pdata)
{
	DBGPR("-->dwc_eqos_free_queue_struct\n");

	if (pdata->tx_queue) {
		kfree(pdata->tx_queue);
		pdata->tx_queue = NULL;
	}

	if (pdata->rx_queue) {
		kfree(pdata->rx_queue);
		pdata->rx_queue = NULL;
	}

	DBGPR("<--dwc_eqos_free_queue_struct\n");
}

/* This function is used to allocate the memory for device
 * descriptors & buffers
 * which are used by device for data transmission & reception.
 *
 * param[in] pdata - pointer to private data structure.
 * return integer
 *
 * retval 0 on success & -ENOMEM number on failure.
 */
static int allocate_buffer_and_desc(struct dwc_eqos_prv_data *pdata)
{
	int ret = 0;
	unsigned int qinx;

	DBGPR("-->allocate_buffer_and_desc: TX_QUEUE_CNT = %d, RX_QUEUE_CNT = %d\n",
	      DWC_EQOS_TX_QUEUE_CNT, DWC_EQOS_RX_QUEUE_CNT);

	/* Allocate descriptors and buffers memory for all TX queues */
	for (qinx = 0; qinx < DWC_EQOS_TX_QUEUE_CNT; qinx++) {
		/* TX descriptors */
		GET_TX_DESC_PTR(qinx, 0) = dma_alloc_coherent(pdata->ndev->dev.parent,
			(sizeof(struct s_tx_normal_desc) * TX_DESC_CNT),
			&(GET_TX_DESC_DMA_ADDR(qinx, 0)), GFP_KERNEL);

		if (!GET_TX_DESC_PTR(qinx, 0)) {
			ret = -ENOMEM;
			goto err_out_tx_desc;
		}
	}

	for (qinx = 0; qinx < DWC_EQOS_TX_QUEUE_CNT; qinx++) {
		/* TX wrapper buffer */
		GET_TX_BUF_PTR(qinx, 0) = kzalloc((sizeof(struct dwc_eqos_tx_buffer) *
						  TX_DESC_CNT), GFP_KERNEL);

		if (!GET_TX_BUF_PTR(qinx, 0)) {
			ret = -ENOMEM;
			goto err_out_tx_buf;
		}
	}

	/* Allocate descriptors and buffers memory for all RX queues */
	for (qinx = 0; qinx < DWC_EQOS_RX_QUEUE_CNT; qinx++) {
		/* RX descriptors */
		GET_RX_DESC_PTR(qinx, 0) = dma_alloc_coherent(pdata->ndev->dev.parent,
			(sizeof(struct s_rx_normal_desc) * RX_DESC_CNT),
			&(GET_RX_DESC_DMA_ADDR(qinx, 0)), GFP_KERNEL);

		if (!GET_RX_DESC_PTR(qinx, 0)) {
			ret = -ENOMEM;
			goto rx_alloc_failure;
		}
	}

	for (qinx = 0; qinx < DWC_EQOS_RX_QUEUE_CNT; qinx++) {
		/* RX wrapper buffer */
		GET_RX_BUF_PTR(qinx, 0) = kzalloc((sizeof(struct dwc_eqos_rx_buffer) *
						   RX_DESC_CNT), GFP_KERNEL);
		if (!GET_RX_BUF_PTR(qinx, 0)) {
			ret = -ENOMEM;
			goto err_out_rx_buf;
		}
	}

	DBGPR("<--allocate_buffer_and_desc\n");

	return ret;

 err_out_rx_buf:
	dwc_eqos_rx_buf_free_mem(pdata, qinx);
	qinx = DWC_EQOS_RX_QUEUE_CNT;

 rx_alloc_failure:
	dwc_eqos_rx_desc_free_mem(pdata, qinx);
	qinx = DWC_EQOS_TX_QUEUE_CNT;

 err_out_tx_buf:
	dwc_eqos_tx_buf_free_mem(pdata, qinx);
	qinx = DWC_EQOS_TX_QUEUE_CNT;

 err_out_tx_desc:
	dwc_eqos_tx_desc_free_mem(pdata, qinx);

	return ret;
}

/* This function is used to initialize transmit descriptors.
 * Each descriptors are assigned a buffer. The base/starting address
 * of the descriptors is updated in device register if required & all
 * the private data structure variables related to transmit
 * descriptor handling are updated in this function.
 *
 * param[in] pdata - pointer to private data structure.
 * return void.
 */
static void dwc_eqos_wrapper_tx_descriptor_init_single_q(
			struct dwc_eqos_prv_data *pdata,
			unsigned int qinx)
{
	int i;
	struct dwc_eqos_tx_wrapper_descriptor *desc_data =
	    GET_TX_WRAPPER_DESC(qinx);
	struct dwc_eqos_tx_buffer *buffer = GET_TX_BUF_PTR(qinx, 0);
	struct s_tx_normal_desc *desc = GET_TX_DESC_PTR(qinx, 0);
	dma_addr_t desc_dma = GET_TX_DESC_DMA_ADDR(qinx, 0);
	struct hw_if_struct *hw_if = &pdata->hw_if;

	DBGPR("-->dwc_eqos_wrapper_tx_descriptor_init_single_q: qinx = %u\n",
	      qinx);

	for (i = 0; i < TX_DESC_CNT; i++) {
		GET_TX_DESC_PTR(qinx, i) = &desc[i];
		GET_TX_DESC_DMA_ADDR(qinx, i) =
		    (desc_dma + sizeof(struct s_tx_normal_desc) * i);
		GET_TX_BUF_PTR(qinx, i) = &buffer[i];
	}

	desc_data->cur_tx = 0;
	desc_data->dirty_tx = 0;
	desc_data->queue_stopped = 0;
	desc_data->tx_pkt_queued = 0;
	desc_data->packet_count = 0;
	desc_data->free_desc_cnt = TX_DESC_CNT;

	hw_if->tx_desc_init(pdata, qinx);

	desc_data->cur_tx = 0;

	DBGPR("<--dwc_eqos_wrapper_tx_descriptor_init_single_q\n");
}

/* This function is used to initialize receive descriptors.
 * skb buffer is allocated & assigned for each descriptors. The base/starting
 * address of the descriptors is updated in device register if required and
 * all the private data structure variables related to receive descriptor
 * handling are updated in this function.
 *
 * param[in] pdata - pointer to private data structure.
 * return void.
 */
static void dwc_eqos_wrapper_rx_descriptor_init_single_q(
			struct dwc_eqos_prv_data *pdata,
			unsigned int qinx)
{
	int i;
	struct dwc_eqos_rx_wrapper_descriptor *desc_data =
	    GET_RX_WRAPPER_DESC(qinx);
	struct dwc_eqos_rx_buffer *buffer = GET_RX_BUF_PTR(qinx, 0);
	struct s_rx_normal_desc *desc = GET_RX_DESC_PTR(qinx, 0);
	dma_addr_t desc_dma = GET_RX_DESC_DMA_ADDR(qinx, 0);
	struct hw_if_struct *hw_if = &pdata->hw_if;

	DBGPR("-->dwc_eqos_wrapper_rx_descriptor_init_single_q: qinx = %u\n",
	      qinx);

	memset(buffer, 0, (sizeof(struct dwc_eqos_rx_buffer) * RX_DESC_CNT));

	for (i = 0; i < RX_DESC_CNT; i++) {
		GET_RX_DESC_PTR(qinx, i) = &desc[i];
		GET_RX_DESC_DMA_ADDR(qinx, i) =
		    (desc_dma + sizeof(struct s_rx_normal_desc) * i);
		GET_RX_BUF_PTR(qinx, i) = &buffer[i];
		/* allocate skb & assign to each desc */
		if (pdata->alloc_rx_buf(pdata, GET_RX_BUF_PTR(qinx, i),
					GFP_KERNEL))
			break;

		wmb();
	}

	desc_data->cur_rx = 0;
	desc_data->dirty_rx = 0;
	desc_data->skb_realloc_idx = 0;
	desc_data->skb_realloc_threshold = MIN_RX_DESC_CNT;
	desc_data->pkt_received = 0;

	hw_if->rx_desc_init(pdata, qinx);

	desc_data->cur_rx = 0;

	DBGPR("<--dwc_eqos_wrapper_rx_descriptor_init_single_q\n");
}

static void dwc_eqos_wrapper_tx_descriptor_init(struct dwc_eqos_prv_data
						*pdata)
{
	struct dwc_eqos_tx_queue *tx_queue = NULL;
	unsigned int qinx;

	DBGPR("-->dwc_eqos_wrapper_tx_descriptor_init\n");

	for (qinx = 0; qinx < DWC_EQOS_TX_QUEUE_CNT; qinx++) {
		tx_queue = GET_TX_QUEUE_PTR(qinx);
		tx_queue->pdata = pdata;
		tx_queue->qinx = qinx;
		dwc_eqos_wrapper_tx_descriptor_init_single_q(pdata, qinx);
	}

	DBGPR("<--dwc_eqos_wrapper_tx_descriptor_init\n");
}

static void dwc_eqos_wrapper_rx_descriptor_init(struct dwc_eqos_prv_data
						*pdata)
{
	struct dwc_eqos_rx_queue *rx_queue = NULL;
	unsigned int qinx;

	DBGPR("-->dwc_eqos_wrapper_rx_descriptor_init\n");

	for (qinx = 0; qinx < DWC_EQOS_RX_QUEUE_CNT; qinx++) {
		rx_queue = GET_RX_QUEUE_PTR(qinx);
		rx_queue->pdata = pdata;
		rx_queue->qinx = qinx;
		dwc_eqos_wrapper_rx_descriptor_init_single_q(pdata, qinx);
	}

	DBGPR("<--dwc_eqos_wrapper_rx_descriptor_init\n");
}

/* details This function is used to free the receive descriptor & buffer memory.
 *
 * param[in] pdata - pointer to private data structure.
 *
 * retval void.
 */
static void dwc_eqos_rx_free_mem(struct dwc_eqos_prv_data *pdata)
{
	DBGPR("-->dwc_eqos_rx_free_mem\n");

	/* free RX descriptor */
	dwc_eqos_rx_desc_free_mem(pdata, DWC_EQOS_RX_QUEUE_CNT);

	/* free RX skb's */
	dwc_eqos_rx_skb_free_mem(pdata, DWC_EQOS_RX_QUEUE_CNT);

	/* free RX wrapper buffer */
	dwc_eqos_rx_buf_free_mem(pdata, DWC_EQOS_RX_QUEUE_CNT);

	DBGPR("<--dwc_eqos_rx_free_mem\n");
}

/* This function is used to free the transmit descriptor
 * & buffer memory.
 *
 * param[in] pdata - pointer to private data structure.
 *
 * retval void.
 */
static void dwc_eqos_tx_free_mem(struct dwc_eqos_prv_data *pdata)
{
	DBGPR("-->dwc_eqos_tx_free_mem\n");

	/* free TX descriptor */
	dwc_eqos_tx_desc_free_mem(pdata, DWC_EQOS_TX_QUEUE_CNT);

	/* free TX buffer */
	dwc_eqos_tx_buf_free_mem(pdata, DWC_EQOS_TX_QUEUE_CNT);

	DBGPR("<--dwc_eqos_tx_free_mem\n");
}

/* This function is invoked by other function to free
 * the tx socket buffers.
 *
 * param[in] pdata – pointer to private data structure.
 * return void
 */
static void dwc_eqos_tx_skb_free_mem_single_q(
		struct dwc_eqos_prv_data *pdata, unsigned int qinx)
{
	unsigned int i;

	DBGPR("-->dwc_eqos_tx_skb_free_mem_single_q: qinx = %u\n", qinx);

	for (i = 0; i < TX_DESC_CNT; i++)
		dwc_eqos_unmap_tx_skb(pdata, GET_TX_BUF_PTR(qinx, i));

	DBGPR("<--dwc_eqos_tx_skb_free_mem_single_q\n");
}

/* This function is used to free the transmit descriptor skb memory.
 *
 * param[in] pdata - pointer to private data structure.
 *
 * retval void.
 */
static void dwc_eqos_tx_skb_free_mem(struct dwc_eqos_prv_data *pdata,
				     unsigned int tx_qcnt)
{
	unsigned int qinx;

	DBGPR("-->dwc_eqos_tx_skb_free_mem: tx_qcnt = %d\n", tx_qcnt);

	for (qinx = 0; qinx < tx_qcnt; qinx++)
		dwc_eqos_tx_skb_free_mem_single_q(pdata, qinx);

	DBGPR("<--dwc_eqos_tx_skb_free_mem\n");
}

/* This function is invoked by other function to free
 * the rx socket buffers.
 *
 * param[in] pdata – pointer to private data structure.
 * return void
 */
static void dwc_eqos_rx_skb_free_mem_single_q(
		struct dwc_eqos_prv_data *pdata, unsigned int qinx)
{
	struct dwc_eqos_rx_wrapper_descriptor *desc_data =
	    GET_RX_WRAPPER_DESC(qinx);
	unsigned int i;

	DBGPR("-->dwc_eqos_rx_skb_free_mem_single_q: qinx = %u\n", qinx);

	for (i = 0; i < RX_DESC_CNT; i++)
		dwc_eqos_unmap_rx_skb(pdata, GET_RX_BUF_PTR(qinx, i));

	/* there are also some cached data from a chained rx */
	if (desc_data->skb_top)
		dev_kfree_skb_any(desc_data->skb_top);

	desc_data->skb_top = NULL;

	DBGPR("<--dwc_eqos_rx_skb_free_mem_single_q\n");
}

/* This function is used to free the receive descriptor skb memory.
 *
 * param[in] pdata - pointer to private data structure.
 *
 * retval void.
 */
static void dwc_eqos_rx_skb_free_mem(struct dwc_eqos_prv_data *pdata,
				     unsigned int rx_qcnt)
{
	unsigned int qinx;

	DBGPR("-->dwc_eqos_rx_skb_free_mem: rx_qcnt = %d\n", rx_qcnt);

	for (qinx = 0; qinx < rx_qcnt; qinx++)
		dwc_eqos_rx_skb_free_mem_single_q(pdata, qinx);

	DBGPR("<--dwc_eqos_rx_skb_free_mem\n");
}

/* This function is used to free the transmit descriptor wrapper buffer memory.
 *
 * param[in] pdata - pointer to private data structure.
 *
 * retval void.
 */
static void dwc_eqos_tx_buf_free_mem(struct dwc_eqos_prv_data *pdata,
				     unsigned int tx_qcnt)
{
	unsigned int qinx;

	DBGPR("-->dwc_eqos_tx_buf_free_mem: tx_qcnt = %d\n", tx_qcnt);

	for (qinx = 0; qinx < tx_qcnt; qinx++) {
		/* free TX buffer */
		if (GET_TX_BUF_PTR(qinx, 0)) {
			kfree(GET_TX_BUF_PTR(qinx, 0));
			GET_TX_BUF_PTR(qinx, 0) = NULL;
		}
	}

	DBGPR("<--dwc_eqos_tx_buf_free_mem\n");
}

/* This function is used to free the receive descriptor wrapper buffer memory.
 *
 * param[in] pdata - pointer to private data structure.
 *
 * retval void.
 */
static void dwc_eqos_rx_buf_free_mem(struct dwc_eqos_prv_data *pdata,
				     unsigned int rx_qcnt)
{
	unsigned int qinx = 0;

	DBGPR("-->dwc_eqos_rx_buf_free_mem: rx_qcnt = %d\n", rx_qcnt);

	for (qinx = 0; qinx < rx_qcnt; qinx++) {
		if (GET_RX_BUF_PTR(qinx, 0)) {
			kfree(GET_RX_BUF_PTR(qinx, 0));
			GET_RX_BUF_PTR(qinx, 0) = NULL;
		}
	}

	DBGPR("<--dwc_eqos_rx_buf_free_mem\n");
}

/* This function is invoked by dwc_eqos_handle_tso. This function
 * will get the header type and return the header length.
 *
 * param[in] skb – pointer to socket buffer structure.
 * return integer
 *
 * retval tcp or udp header length
 */
static unsigned int tcp_udp_hdrlen(struct sk_buff *skb)
{
	return tcp_hdrlen(skb);
}

/* This function is invoked by start_xmit functions. This function
 * will get all the tso details like MSS(Maximum Segment Size),
 * packet header length,
 * packet pay load length and tcp header length etc if the given skb has tso
 * packet and store it in other wrapper tx structure for later usage.
 *
 * param[in] dev – pointer to net device structure.
 * param[in] skb – pointer to socket buffer structure.
 * return integer
 *
 * retval 1 on success, -ve no failure and 0 if not tso pkt
 */
static int dwc_eqos_handle_tso(struct net_device *dev, struct sk_buff *skb)
{
	struct dwc_eqos_prv_data *pdata = netdev_priv(dev);
	struct s_tx_pkt_features *tx_pkt_features = GET_TX_PKT_FEATURES_PTR;
	int ret = 1;

	DBGPR("-->dwc_eqos_handle_tso\n");

	if (skb_is_gso(skb) == 0) {
		DBGPR("This is not a TSO/LSO/GSO packet\n");
		return 0;
	}

	DBGPR("Got TSO packet\n");

	if (skb_header_cloned(skb)) {
		ret = pskb_expand_head(skb, 0, 0, GFP_ATOMIC);
		if (ret)
			return ret;
	}

	/* get TSO or UFO details */
	tx_pkt_features->hdr_len =
			skb_transport_offset(skb) + tcp_udp_hdrlen(skb);
	tx_pkt_features->tcp_udp_hdr_len = tcp_udp_hdrlen(skb);

	tx_pkt_features->mss = skb_shinfo(skb)->gso_size;

	tx_pkt_features->pay_len = (skb->len - tx_pkt_features->hdr_len);
	DBGPR("mss		= %lu\n", tx_pkt_features->mss);
	DBGPR("hdr_len		= %lu\n", tx_pkt_features->hdr_len);
	DBGPR("pay_len		= %lu\n", tx_pkt_features->pay_len);
	DBGPR("tcp_udp_hdr_len	= %lu\n", tx_pkt_features->tcp_udp_hdr_len);

	DBGPR("<--dwc_eqos_handle_tso\n");

	return ret;
}

/* returns 0 on success and -ve on failure */
static int dwc_eqos_map_non_page_buffs(struct dwc_eqos_prv_data *pdata,
				       struct dwc_eqos_tx_buffer *buffer,
				       struct dwc_eqos_tx_buffer *prev_buffer,
				       struct sk_buff *skb, unsigned int offset,
				       unsigned int size)
{
	DBGPR("-->dwc_eqos_map_non_page_buffs\n");

	if (size > DWC_EQOS_MAX_DATA_PER_TX_BUF) {
		if (prev_buffer && !prev_buffer->dma2) {
			/* fill the first buffer pointer in prev_buffer->dma2 */
			prev_buffer->dma2 = dma_map_single(
							(pdata->ndev->dev.parent),
							(skb->data + offset),
							DWC_EQOS_MAX_DATA_PER_TX_BUF,
							DMA_TO_DEVICE);
			if (dma_mapping_error((pdata->ndev->dev.parent), prev_buffer->dma2)) {
				pr_alert("failed to do the dma map\n");
				return -ENOMEM;
			}
			prev_buffer->len2 = DWC_EQOS_MAX_DATA_PER_TX_BUF;
			prev_buffer->buf2_mapped_as_page = Y_FALSE;

			/* fill the second buffer pointer in buffer->dma */
			buffer->dma = dma_map_single((pdata->ndev->dev.parent),
						(skb->data + offset + DWC_EQOS_MAX_DATA_PER_TX_BUF),
						(size - DWC_EQOS_MAX_DATA_PER_TX_BUF),
						DMA_TO_DEVICE);
			if (dma_mapping_error((pdata->ndev->dev.parent), buffer->dma)) {
				pr_alert("failed to do the dma map\n");
				return -ENOMEM;
			}
			buffer->len = (size - DWC_EQOS_MAX_DATA_PER_TX_BUF);
			buffer->buf1_mapped_as_page = Y_FALSE;
			buffer->dma2 = 0;
			buffer->len2 = 0;
		} else {
			/* fill the first buffer pointer in buffer->dma */
			buffer->dma = dma_map_single((pdata->ndev->dev.parent),
					(skb->data + offset),
					DWC_EQOS_MAX_DATA_PER_TX_BUF,
					DMA_TO_DEVICE);
			if (dma_mapping_error((pdata->ndev->dev.parent), buffer->dma)) {
				pr_alert("failed to do the dma map\n");
				return -ENOMEM;
			}
			buffer->len = DWC_EQOS_MAX_DATA_PER_TX_BUF;
			buffer->buf1_mapped_as_page = Y_FALSE;

			/* fill the second buffer pointer in buffer->dma2 */
			buffer->dma2 = dma_map_single((pdata->ndev->dev.parent),
					(skb->data + offset + DWC_EQOS_MAX_DATA_PER_TX_BUF),
					(size - DWC_EQOS_MAX_DATA_PER_TX_BUF),
					DMA_TO_DEVICE);
			if (dma_mapping_error((pdata->ndev->dev.parent), buffer->dma2)) {
				pr_alert("failed to do the dma map\n");
				return -ENOMEM;
			}
			buffer->len2 = (size - DWC_EQOS_MAX_DATA_PER_TX_BUF);
			buffer->buf2_mapped_as_page = Y_FALSE;
		}
	} else {
		if (prev_buffer && !prev_buffer->dma2) {
			/* fill the first buffer pointer in prev_buffer->dma2 */
			prev_buffer->dma2 = dma_map_single(
						(pdata->ndev->dev.parent),
						(skb->data + offset),
						size, DMA_TO_DEVICE);
			if (dma_mapping_error((pdata->ndev->dev.parent), prev_buffer->dma2)) {
				pr_alert("failed to do the dma map\n");
				return -ENOMEM;
			}
			prev_buffer->len2 = size;
			prev_buffer->buf2_mapped_as_page = Y_FALSE;

			/* indicate current buffer struct is not used */
			buffer->dma = 0;
			buffer->len = 0;
			buffer->dma2 = 0;
			buffer->len2 = 0;
		} else {
			/* fill the first buffer pointer in buffer->dma */
			buffer->dma = dma_map_single((pdata->ndev->dev.parent),
						(skb->data + offset),
						size, DMA_TO_DEVICE);
			if (dma_mapping_error((pdata->ndev->dev.parent), buffer->dma)) {
				pr_alert("failed to do the dma map\n");
				return -ENOMEM;
			}
			buffer->len = size;
			buffer->buf1_mapped_as_page = Y_FALSE;
			buffer->dma2 = 0;
			buffer->len2 = 0;
		}
	}

	DBGPR("<--dwc_eqos_map_non_page_buffs\n");

	return 0;
}

/* returns 0 on success and -ve on failure */
static int dwc_eqos_map_page_buffs(struct dwc_eqos_prv_data *pdata,
				   struct dwc_eqos_tx_buffer *buffer,
				   struct dwc_eqos_tx_buffer *prev_buffer,
				   struct skb_frag_struct *frag,
				   unsigned int offset, unsigned int size)
{
	DBGPR("-->dwc_eqos_map_page_buffs\n");

	if (size > DWC_EQOS_MAX_DATA_PER_TX_BUF) {
		if (!prev_buffer->dma2) {
			DBGPR("prev_buffer->dma2 is empty\n");
			/* fill the first buffer pointer in pre_buffer->dma2 */
			prev_buffer->dma2 = dma_map_page(
				(pdata->ndev->dev.parent),
						frag->page.p,
						(frag->page_offset + offset),
						DWC_EQOS_MAX_DATA_PER_TX_BUF,
						DMA_TO_DEVICE);
			if (dma_mapping_error(
				(pdata->ndev->dev.parent), prev_buffer->dma2)) {
				pr_alert("failed to do the dma map\n");
				return -ENOMEM;
			}
			prev_buffer->len2 = DWC_EQOS_MAX_DATA_PER_TX_BUF;
			prev_buffer->buf2_mapped_as_page = Y_TRUE;

			/* fill the second buffer pointer in buffer->dma */
			buffer->dma = dma_map_page((pdata->ndev->dev.parent),
						frag->page.p,
						(frag->page_offset + offset + DWC_EQOS_MAX_DATA_PER_TX_BUF),
						(size - DWC_EQOS_MAX_DATA_PER_TX_BUF),
						DMA_TO_DEVICE);
			if (dma_mapping_error(
				(pdata->ndev->dev.parent), buffer->dma)) {
				pr_alert("failed to do the dma map\n");
				return -ENOMEM;
			}
			buffer->len = (size - DWC_EQOS_MAX_DATA_PER_TX_BUF);
			buffer->buf1_mapped_as_page = Y_TRUE;
			buffer->dma2 = 0;
			buffer->len2 = 0;
		} else {
			/* fill the first buffer pointer in buffer->dma */
			buffer->dma = dma_map_page((pdata->ndev->dev.parent),
						frag->page.p,
						(frag->page_offset + offset),
						DWC_EQOS_MAX_DATA_PER_TX_BUF,
						DMA_TO_DEVICE);
			if (dma_mapping_error(
				(pdata->ndev->dev.parent), buffer->dma)) {
				pr_alert("failed to do the dma map\n");
				return -ENOMEM;
			}
			buffer->len = DWC_EQOS_MAX_DATA_PER_TX_BUF;
			buffer->buf1_mapped_as_page = Y_TRUE;

			/* fill the second buffer pointer in buffer->dma2 */
			buffer->dma2 = dma_map_page((pdata->ndev->dev.parent),
						frag->page.p,
						(frag->page_offset + offset + DWC_EQOS_MAX_DATA_PER_TX_BUF),
						(size - DWC_EQOS_MAX_DATA_PER_TX_BUF),
						DMA_TO_DEVICE);
			if (dma_mapping_error(
				(pdata->ndev->dev.parent), buffer->dma2)) {
				pr_alert("failed to do the dma map\n");
				return -ENOMEM;
			}
			buffer->len2 = (size - DWC_EQOS_MAX_DATA_PER_TX_BUF);
			buffer->buf2_mapped_as_page = Y_TRUE;
		}
	} else {
		if (!prev_buffer->dma2) {
			DBGPR("prev_buffer->dma2 is empty\n");
			/* fill the first buffer pointer in pre_buffer->dma2 */
			prev_buffer->dma2 = dma_map_page(
						(pdata->ndev->dev.parent),
						frag->page.p,
						frag->page_offset + offset,
						size, DMA_TO_DEVICE);
			if (dma_mapping_error(
				(pdata->ndev->dev.parent), prev_buffer->dma2)) {
				pr_alert("failed to do the dma map\n");
				return -ENOMEM;
			}
			prev_buffer->len2 = size;
			prev_buffer->buf2_mapped_as_page = Y_TRUE;

			/* indicate current buffer struct is not used */
			buffer->dma = 0;
			buffer->len = 0;
			buffer->dma2 = 0;
			buffer->len2 = 0;
		} else {
			/* fill the first buffer pointer in buffer->dma */
			buffer->dma = dma_map_page((pdata->ndev->dev.parent),
						frag->page.p,
						frag->page_offset + offset,
						size, DMA_TO_DEVICE);
			if (dma_mapping_error(
				(pdata->ndev->dev.parent), buffer->dma)) {
				pr_alert("failed to do the dma map\n");
				return -ENOMEM;
			}
			buffer->len = size;
			buffer->buf1_mapped_as_page = Y_TRUE;
			buffer->dma2 = 0;
			buffer->len2 = 0;
		}
	}

	DBGPR("<--dwc_eqos_map_page_buffs\n");

	return 0;
}

/*!
 * \details This function is invoked by start_xmit functions. This function
 * will get the dma/physical address of the packet to be transmitted and
 * its length. All this information about the packet to be transmitted is
 * stored in private data structure and same is used later in the driver to
 * setup the descriptor for transmission.
 *
 * \param[in] dev – pointer to net device structure.
 * \param[in] skb – pointer to socket buffer structure.
 *
 * \return unsigned int
 *
 * \retval count – number of packet to be programmed in the descriptor or
 * zero on failure.
 */
static unsigned int dwc_eqos_map_skb(struct net_device *dev,
				     struct sk_buff *skb)
{
	struct dwc_eqos_prv_data *pdata = netdev_priv(dev);
	unsigned int qinx = skb_get_queue_mapping(skb);
	struct dwc_eqos_tx_wrapper_descriptor *desc_data =
	    GET_TX_WRAPPER_DESC(qinx);
	struct dwc_eqos_tx_buffer *buffer =
	    GET_TX_BUF_PTR(qinx, desc_data->cur_tx);
	struct dwc_eqos_tx_buffer *prev_buffer = NULL;
	struct s_tx_pkt_features *tx_pkt_features = GET_TX_PKT_FEATURES_PTR;
	unsigned int varvlan_pkt;
	int index = (int)desc_data->cur_tx;
	unsigned int frag_cnt = skb_shinfo(skb)->nr_frags;
	unsigned int hdr_len = 0;
	unsigned int i;
	unsigned int count = 0, offset = 0, size;
	int len;
	int vartso_enable = 0;
	int ret;

	DBGPR("-->dwc_eqos_map_skb: cur_tx = %d, qinx = %u\n",
	      desc_data->cur_tx, qinx);

	TX_PKT_FEATURES_PKT_ATTRIBUTES_TSO_ENABLE_MLF_RD(
		tx_pkt_features->pkt_attributes, vartso_enable);
	TX_PKT_FEATURES_PKT_ATTRIBUTES_VLAN_PKT_MLF_RD(
			tx_pkt_features->pkt_attributes, varvlan_pkt);
	if (varvlan_pkt == 0x1) {
		DBGPR("Skipped preparing index %d (VLAN Context descriptor)\n\n",
		      index);
		INCR_TX_DESC_INDEX(index, 1);
		buffer = GET_TX_BUF_PTR(qinx, index);
	} else if ((vartso_enable == 0x1) &&
		(desc_data->default_mss != tx_pkt_features->mss)) {
		/* keep space for CONTEXT descriptor in the RING */
		INCR_TX_DESC_INDEX(index, 1);
		buffer = GET_TX_BUF_PTR(qinx, index);
	}
#ifdef DWC_EQOS_ENABLE_DVLAN
	if (pdata->via_reg_or_desc) {
		DBGPR("Skipped preparing index %d (Double VLAN Context descriptor)\n",
		      index);
		INCR_TX_DESC_INDEX(index, 1);
		buffer = GET_TX_BUF_PTR(qinx, index);
	}
#endif /* End of DWC_EQOS_ENABLE_DVLAN */

	if (vartso_enable) {
		hdr_len = skb_transport_offset(skb) + tcp_udp_hdrlen(skb);
		len = hdr_len;
	} else {
		len = (skb->len - skb->data_len);
	}

	DBGPR("skb->len: %d\nskb->data_len: %d\n", skb->len, skb->data_len);
	DBGPR("skb->len - skb->data_len = %d, hdr_len = %d\n", len, hdr_len);
	while (len) {
		size = min(len, DWC_EQOS_MAX_DATA_PER_TXD);

		buffer = GET_TX_BUF_PTR(qinx, index);
		ret = dwc_eqos_map_non_page_buffs(pdata, buffer, prev_buffer,
						  skb, offset, size);
		if (ret < 0)
			goto err_out_dma_map_fail;

		len -= size;
		offset += size;
		prev_buffer = buffer;
		INCR_TX_DESC_INDEX(index, 1);
		count++;
	}

	/* Process remaining pay load in skb->data in case of TSO packet */
	if (vartso_enable) {
		len = ((skb->len - skb->data_len) - hdr_len);
		while (len > 0) {
			size = min(len, DWC_EQOS_MAX_DATA_PER_TXD);

			buffer = GET_TX_BUF_PTR(qinx, index);
			ret = dwc_eqos_map_non_page_buffs(pdata, buffer, prev_buffer,
							  skb, offset, size);
			if (ret < 0)
				goto err_out_dma_map_fail;

			len -= size;
			offset += size;
			if (buffer->dma != 0) {
				prev_buffer = buffer;
				INCR_TX_DESC_INDEX(index, 1);
				count++;
			}
		}
	}

	DBGPR("frag_cnt: %d\n", frag_cnt);
	/* Process fragmented skb's */
	for (i = 0; i < frag_cnt; i++) {
		struct skb_frag_struct *frag = &skb_shinfo(skb)->frags[i];

		DBGPR("frag[%d] size: 0x%x\n", i, frag->size);
		len = frag->size;
		offset = 0;
		while (len) {
			size = min(len, DWC_EQOS_MAX_DATA_PER_TXD);

			buffer = GET_TX_BUF_PTR(qinx, index);
			ret = dwc_eqos_map_page_buffs(pdata, buffer,
						      prev_buffer, frag,
						      offset, size);
			if (ret < 0)
				goto err_out_dma_map_fail;

			len -= size;
			offset += size;
			if (buffer->dma != 0) {
				prev_buffer = buffer;
				INCR_TX_DESC_INDEX(index, 1);
				count++;
			}
		}
	}
	buffer->skb = skb;

	DBGPR("<--dwc_eqos_map_skb: buffer->dma = %#x\n",
	      (unsigned int)buffer->dma);

	return count;

 err_out_dma_map_fail:
	pr_alert("Tx DMA map failed\n");

	for (; count > 0; count--) {
		DECR_TX_DESC_INDEX(index);
		buffer = GET_TX_BUF_PTR(qinx, index);
		dwc_eqos_unmap_tx_skb(pdata, buffer);
	}

	return 0;
}

/* This function is called in *_tx_interrupt function to release
 * the skb for the successfully transmited packets.
 *
 * param[in] pdata - pointer to private data structure.
 * param[in] buffer - pointer to *_tx_buffer structure
 * return void
 */
static void dwc_eqos_unmap_tx_skb(struct dwc_eqos_prv_data *pdata,
				  struct dwc_eqos_tx_buffer *buffer)
{
	DBGPR("-->dwc_eqos_unmap_tx_skb\n");

	if (buffer->dma) {
		if (buffer->buf1_mapped_as_page == Y_TRUE)
			dma_unmap_page((pdata->ndev->dev.parent), buffer->dma,
				       buffer->len, DMA_TO_DEVICE);
		else
			dma_unmap_single((pdata->ndev->dev.parent), buffer->dma,
					 buffer->len, DMA_TO_DEVICE);

		buffer->dma = 0;
		buffer->len = 0;
	}

	if (buffer->dma2) {
		if (buffer->buf2_mapped_as_page == Y_TRUE)
			dma_unmap_page((pdata->ndev->dev.parent), buffer->dma2,
				       buffer->len2, DMA_TO_DEVICE);
		else
			dma_unmap_single((pdata->ndev->dev.parent),
					 buffer->dma2, buffer->len2,
					 DMA_TO_DEVICE);

		buffer->dma2 = 0;
		buffer->len2 = 0;
	}

	if (buffer->skb) {
		dev_kfree_skb_any(buffer->skb);
		buffer->skb = NULL;
	}

	DBGPR("<--dwc_eqos_unmap_tx_skb\n");
}

/* This function is invoked by other function for releasing the socket
 * buffer which are received by device and passed to upper layer.
 *
 * param[in] pdata – pointer to private device structure.
 * param[in] buffer – pointer to rx wrapper buffer structure.
 * return void
 */
static void dwc_eqos_unmap_rx_skb(struct dwc_eqos_prv_data *pdata,
				  struct dwc_eqos_rx_buffer *buffer)
{
	DBGPR("-->dwc_eqos_unmap_rx_skb\n");

	/* unmap the first buffer */
	if (buffer->dma) {
		if (pdata->rx_split_hdr) {
			dma_unmap_single(pdata->ndev->dev.parent, buffer->dma,
					 (2 * buffer->rx_hdr_size),
					 DMA_FROM_DEVICE);
		} else if (pdata->ndev->mtu > DWC_EQOS_ETH_FRAME_LEN) {
			dma_unmap_page(pdata->ndev->dev.parent, buffer->dma,
				       PAGE_SIZE, DMA_FROM_DEVICE);
		} else {
			dma_unmap_single(pdata->ndev->dev.parent, buffer->dma,
					 pdata->rx_buffer_len, DMA_FROM_DEVICE);
		}
		buffer->dma = 0;
	}

	/* unmap the second buffer */
	if (buffer->dma2) {
		dma_unmap_page(pdata->ndev->dev.parent, buffer->dma2,
			       PAGE_SIZE, DMA_FROM_DEVICE);
		buffer->dma2 = 0;
	}

	/* page1 will be present only if JUMBO is enabled */
	if (buffer->page) {
		put_page(buffer->page);
		buffer->page = NULL;
	}
	/* page2 will be present if JUMBO/SPLIT HDR is enabled */
	if (buffer->page2) {
		put_page(buffer->page2);
		buffer->page2 = NULL;
	}

	if (buffer->skb) {
		dev_kfree_skb_any(buffer->skb);
		buffer->skb = NULL;
	}

	DBGPR("<--dwc_eqos_unmap_rx_skb\n");
}

/* This function is used to re-allocate & re-assign the new skb to
 * receive descriptors from which driver has read the data. Also ownership bit
 * and other bits are reset so that device can reuse the descriptors.
 *
 * param[in] pdata - pointer to private data structure.
 * return void.
 */
static void dwc_eqos_re_alloc_skb(
	struct dwc_eqos_prv_data *pdata, unsigned int qinx)
{
	int i;
	struct dwc_eqos_rx_wrapper_descriptor *desc_data =
	    GET_RX_WRAPPER_DESC(qinx);
	struct dwc_eqos_rx_buffer *buffer = NULL;
	struct hw_if_struct *hw_if = &pdata->hw_if;
	int tail_idx;

	DBGPR("-->dwc_eqos_re_alloc_skb: desc_data->skb_realloc_idx = %d qinx = %u\n",
	      desc_data->skb_realloc_idx, qinx);

	for (i = 0; i < desc_data->dirty_rx; i++) {
		buffer = GET_RX_BUF_PTR(qinx, desc_data->skb_realloc_idx);
		/* allocate skb & assign to each desc */
		if (pdata->alloc_rx_buf(pdata, buffer, GFP_ATOMIC)) {
			pr_alert("Failed to re allocate skb\n");
			pdata->xstats.q_re_alloc_rx_buf_failed[qinx]++;
			break;
		}

		wmb();
		hw_if->rx_desc_reset(desc_data->skb_realloc_idx, pdata,
				     buffer->inte, qinx);
		INCR_RX_DESC_INDEX(desc_data->skb_realloc_idx, 1);
	}
	tail_idx = desc_data->skb_realloc_idx;
	DECR_RX_DESC_INDEX(tail_idx);
	hw_if->update_rx_tail_ptr(pdata, qinx,
		GET_RX_DESC_DMA_ADDR(qinx, tail_idx));
	desc_data->dirty_rx = 0;

	DBGPR("<--dwc_eqos_re_alloc_skb\n");
}

/* This function is called in probe to initialize all the function
 * pointers which are used in other functions to manage edscriptors.
 *
 * param[in] desc_if - pointer to desc_if_struct structure.
 *
 * return void.
 */
void dwc_eqos_init_function_ptrs_desc(struct desc_if_struct *desc_if)
{
	DBGPR("-->dwc_eqos_init_function_ptrs_desc\n");

	desc_if->alloc_queue_struct = dwc_eqos_alloc_queue_struct;
	desc_if->free_queue_struct = dwc_eqos_free_queue_struct;
	desc_if->alloc_buff_and_desc = allocate_buffer_and_desc;
	desc_if->realloc_skb = dwc_eqos_re_alloc_skb;
	desc_if->unmap_rx_skb = dwc_eqos_unmap_rx_skb;
	desc_if->unmap_tx_skb = dwc_eqos_unmap_tx_skb;
	desc_if->map_tx_skb = dwc_eqos_map_skb;
	desc_if->tx_free_mem = dwc_eqos_tx_free_mem;
	desc_if->rx_free_mem = dwc_eqos_rx_free_mem;
	desc_if->wrapper_tx_desc_init =
			dwc_eqos_wrapper_tx_descriptor_init;
	desc_if->wrapper_tx_desc_init_single_q =
	    dwc_eqos_wrapper_tx_descriptor_init_single_q;
	desc_if->wrapper_rx_desc_init =
			dwc_eqos_wrapper_rx_descriptor_init;
	desc_if->wrapper_rx_desc_init_single_q =
	    dwc_eqos_wrapper_rx_descriptor_init_single_q;

	desc_if->rx_skb_free_mem = dwc_eqos_rx_skb_free_mem;
	desc_if->rx_skb_free_mem_single_q =
			dwc_eqos_rx_skb_free_mem_single_q;
	desc_if->tx_skb_free_mem = dwc_eqos_tx_skb_free_mem;
	desc_if->tx_skb_free_mem_single_q =
			dwc_eqos_tx_skb_free_mem_single_q;

	desc_if->handle_tso = dwc_eqos_handle_tso;

	DBGPR("<--dwc_eqos_init_function_ptrs_desc\n");
}
