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
/*!@file: eqos_desc.c
 * @brief: Driver functions.
 */
#include "yheader.h"
#include "desc.h"
extern ULONG eqos_base_addr;
#include "yregacc.h"

/*!
* \brief API to free the transmit descriptor memory.
*
* \details This function is used to free the transmit descriptor memory.
*
* \param[in] pdata - pointer to private data structure.
*
* \retval void.
*/

static void eqos_tx_desc_free_mem(struct eqos_prv_data *pdata,
					 UINT tx_qcnt)
{
	struct tx_ring *ptx_ring = NULL;
	UINT qinx;
	uint tx_ring_size = sizeof(struct s_tx_desc) * TX_DESC_CNT;

	DBGPR("-->eqos_tx_desc_free_mem: tx_qcnt = %d\n", tx_qcnt);

	for (qinx = 0; qinx < tx_qcnt; qinx++) {
		ptx_ring = GET_TX_WRAPPER_DESC(qinx);

		if (GET_TX_DESC_PTR(qinx, 0)) {
			dma_free_coherent(&pdata->pdev->dev,
					  tx_ring_size,
					  GET_TX_DESC_PTR(qinx, 0),
					  GET_TX_DESC_DMA_ADDR(qinx, 0));
			GET_TX_DESC_PTR(qinx, 0) = NULL;
		}
	}
#ifdef DO_TX_ALIGN_TEST
	if (pdata->ptst_buf)
		dma_free_coherent(&pdata->pdev->dev, pdata->tst_buf_size,
		pdata->ptst_buf, (dma_addr_t)pdata->tst_buf_dma_addr);
#endif

	DBGPR("<--eqos_tx_desc_free_mem\n");
}

/*!
* \brief API to free the receive descriptor memory.
*
* \details This function is used to free the receive descriptor memory.
*
* \param[in] pdata - pointer to private data structure.
*
* \retval void.
*/

static void eqos_rx_desc_free_mem(struct eqos_prv_data *pdata,
					 UINT rx_qcnt)
{
	struct rx_ring *prx_ring = NULL;
	UINT qinx = 0;
	uint rx_ring_size = sizeof(struct s_rx_desc) * RX_DESC_CNT;

	DBGPR("-->eqos_rx_desc_free_mem: rx_qcnt = %d\n", rx_qcnt);

	for (qinx = 0; qinx < rx_qcnt; qinx++) {
		prx_ring = GET_RX_WRAPPER_DESC(qinx);

		if (GET_RX_DESC_PTR(qinx, 0)) {
			dma_free_coherent(&pdata->pdev->dev,
					  rx_ring_size,
					  GET_RX_DESC_PTR(qinx, 0),
					  GET_RX_DESC_DMA_ADDR(qinx, 0));
			GET_RX_DESC_PTR(qinx, 0) = NULL;
		}
	}

	DBGPR("<--eqos_rx_desc_free_mem\n");
}

/*!
* \brief API to alloc the queue memory.
*
* \details This function allocates the queue structure memory.
*
* \param[in] pdata - pointer to private data structure.
*
* \return integer
*
* \retval 0 on success & -ve number on failure.
*/

static int eqos_alloc_queue_struct(struct eqos_prv_data *pdata)
{
	int ret = 0;

	DBGPR("-->eqos_alloc_queue_struct: tx_queue_cnt = %d,"\
		"rx_queue_cnt = %d\n", pdata->tx_queue_cnt, pdata->rx_queue_cnt);

	pdata->tx_queue =
		kzalloc(sizeof(struct eqos_tx_queue) * pdata->tx_queue_cnt,
		GFP_KERNEL);
	if (pdata->tx_queue == NULL) {
		pr_err("ERROR: Unable to allocate Tx queue structure\n");
		ret = -ENOMEM;
		goto err_out_tx_q_alloc_failed;
	}

	pdata->rx_queue =
		kzalloc(sizeof(struct eqos_rx_queue) * pdata->rx_queue_cnt,
		GFP_KERNEL);
	if (pdata->rx_queue == NULL) {
		pr_err("ERROR: Unable to allocate Rx queue structure\n");
		ret = -ENOMEM;
		goto err_out_rx_q_alloc_failed;
	}

	DBGPR("<--eqos_alloc_queue_struct\n");

	return ret;

err_out_rx_q_alloc_failed:
	kfree(pdata->tx_queue);

err_out_tx_q_alloc_failed:
	return ret;
}


/*!
* \brief API to free the queue memory.
*
* \details This function free the queue structure memory.
*
* \param[in] pdata - pointer to private data structure.
*
* \return void
*/

static void eqos_free_queue_struct(struct eqos_prv_data *pdata)
{
	DBGPR("-->eqos_free_queue_struct\n");

	if (pdata->tx_queue != NULL) {
		kfree(pdata->tx_queue);
		pdata->tx_queue = NULL;
	}

	if (pdata->rx_queue != NULL) {
		kfree(pdata->rx_queue);
		pdata->rx_queue = NULL;
	}

	DBGPR("<--eqos_free_queue_struct\n");
}

/*!
* \brief API to free the memory for descriptor & buffers.
*
* \details This function is used to free the memory for device
* descriptors & buffers
* which are used by device for data transmission & reception.
*
* \param[in] pdata - pointer to private data structure.
*
* \return void
*
*/
static void free_buffer_and_desc(struct eqos_prv_data *pdata)
{
	eqos_tx_free_mem(pdata);
	eqos_rx_free_mem(pdata);
}

/*!
* \brief API to allocate the memory for descriptor & buffers.
*
* \details This function is used to allocate the memory for device
* descriptors & buffers
* which are used by device for data transmission & reception.
*
* \param[in] pdata - pointer to private data structure.
*
* \return integer
*
* \retval 0 on success & -ENOMEM number on failure.
*/

static INT allocate_buffer_and_desc(struct eqos_prv_data *pdata)
{
	INT ret = 0;
	UINT qinx;
	uint tx_ring_size = sizeof(struct s_tx_desc) * TX_DESC_CNT;
	uint rx_ring_size = sizeof(struct s_rx_desc) * RX_DESC_CNT;
	uint tx_swcx_size = sizeof(struct tx_swcx_desc) * TX_DESC_CNT;
	uint rx_swcx_size = sizeof(struct rx_swcx_desc) * RX_DESC_CNT;

	DBGPR("-->allocate_buffer_and_desc: TX_QUEUE_CNT = %d, "\
		"RX_QUEUE_CNT = %d\n", EQOS_TX_QUEUE_CNT,
		EQOS_RX_QUEUE_CNT);

	/* Allocate descriptors and buffers memory for all TX queues */
	for (qinx = 0; qinx < EQOS_TX_QUEUE_CNT; qinx++) {
		/* TX descriptors */
		GET_TX_DESC_PTR(qinx, 0) =
			dma_alloc_coherent(&pdata->pdev->dev,
					   tx_ring_size,
					   &(GET_TX_DESC_DMA_ADDR(qinx, 0)),
					   GFP_KERNEL);
		if (GET_TX_DESC_PTR(qinx, 0) == NULL) {
			ret = -ENOMEM;
			goto err_out_tx_desc;
		}
		/* Check if address is greater than 32 bit */
		BUG_ON((uint64_t)GET_TX_DESC_DMA_ADDR(qinx, 0) >> 32);
	}

	for (qinx = 0; qinx < EQOS_TX_QUEUE_CNT; qinx++) {
		/* TX wrapper buffer */
		GET_TX_BUF_PTR(qinx, 0) = kzalloc(tx_swcx_size, GFP_KERNEL);
		if (GET_TX_BUF_PTR(qinx, 0) == NULL) {
			ret = -ENOMEM;
			goto err_out_tx_buf;
		}
	}

	/* Allocate descriptors and buffers memory for all RX queues */
	for (qinx = 0; qinx < EQOS_RX_QUEUE_CNT; qinx++) {
		/* RX descriptors */
		GET_RX_DESC_PTR(qinx, 0) =
			dma_alloc_coherent(&pdata->pdev->dev,
					   rx_ring_size,
					   &(GET_RX_DESC_DMA_ADDR(qinx, 0)),
					   GFP_KERNEL);
		if (GET_RX_DESC_PTR(qinx, 0) == NULL) {
			ret = -ENOMEM;
			goto rx_alloc_failure;
		}
		/* Check if address is greater than 32 bit */
		BUG_ON((uint64_t)GET_RX_DESC_DMA_ADDR(qinx, 0) >> 32);
	}

	for (qinx = 0; qinx < EQOS_RX_QUEUE_CNT; qinx++) {
		/* RX wrapper buffer */
		GET_RX_BUF_PTR(qinx, 0) = kzalloc(rx_swcx_size, GFP_KERNEL);
		if (GET_RX_BUF_PTR(qinx, 0) == NULL) {
			ret = -ENOMEM;
			goto err_out_rx_buf;
		}
	}
#ifdef DO_TX_ALIGN_TEST
	{
		UINT cnt;

		pdata->tst_buf_size = 2048;
		pdata->ptst_buf = dma_alloc_coherent(&pdata->pdev->dev,
			pdata->tst_buf_size,
			(dma_addr_t *)&pdata->tst_buf_dma_addr,
			GFP_KERNEL);

		for (cnt = 0; cnt < 2048; cnt++)
			pdata->ptst_buf[cnt] = cnt;
	}
#endif

	DBGPR("<--allocate_buffer_and_desc\n");

	return ret;

 err_out_rx_buf:
	eqos_rx_buf_free_mem(pdata, qinx);
	qinx = EQOS_RX_QUEUE_CNT;

 rx_alloc_failure:
	eqos_rx_desc_free_mem(pdata, qinx);
	qinx = EQOS_TX_QUEUE_CNT;

 err_out_tx_buf:
	eqos_tx_buf_free_mem(pdata, qinx);
	qinx = EQOS_TX_QUEUE_CNT;

 err_out_tx_desc:
	eqos_tx_desc_free_mem(pdata, qinx);

	return ret;
}

/*!
* \brief API to initialize the transmit descriptors.
*
* \details This function is used to initialize transmit descriptors.
* Each descriptors are assigned a buffer. The base/starting address
* of the descriptors is updated in device register if required & all
* the private data structure variables related to transmit
* descriptor handling are updated in this function.
*
* \param[in] pdata - pointer to private data structure.
*
* \return void.
*/

static void eqos_wrapper_tx_descriptor_init_single_q(
			struct eqos_prv_data *pdata,
			UINT qinx)
{
	int i;
	struct tx_ring *ptx_ring =
	    GET_TX_WRAPPER_DESC(qinx);
	struct tx_swcx_desc *ptx_swcx_desc = GET_TX_BUF_PTR(qinx, 0);
	struct s_tx_desc *desc = GET_TX_DESC_PTR(qinx, 0);
	dma_addr_t desc_dma = GET_TX_DESC_DMA_ADDR(qinx, 0);
	struct hw_if_struct *hw_if = &(pdata->hw_if);

	DBGPR("-->eqos_wrapper_tx_descriptor_init_single_q: "\
		"qinx = %u\n", qinx);

	for (i = 0; i < TX_DESC_CNT; i++) {
		GET_TX_DESC_PTR(qinx, i) = &desc[i];
		GET_TX_DESC_DMA_ADDR(qinx, i) =
		    (desc_dma + sizeof(struct s_tx_desc) * i);
		GET_TX_BUF_PTR(qinx, i) = &ptx_swcx_desc[i];
	}

	ptx_ring->cur_tx = 0;
	ptx_ring->dirty_tx = 0;
	ptx_ring->queue_stopped = 0;
	ptx_ring->tx_pkt_queued = 0;
	ptx_ring->free_desc_cnt = TX_DESC_CNT;

	hw_if->tx_desc_init(pdata, qinx);
	ptx_ring->cur_tx = 0;

	DBGPR("<--eqos_wrapper_tx_descriptor_init_single_q\n");
}

/*!
* \brief API to initialize the receive descriptors.
*
* \details This function is used to initialize receive descriptors.
* skb buffer is allocated & assigned for each descriptors. The base/starting
* address of the descriptors is updated in device register if required and
* all the private data structure variables related to receive descriptor
* handling are updated in this function.
*
* \param[in] pdata - pointer to private data structure.
*
* \return void.
*/

static void eqos_wrapper_rx_descriptor_init_single_q(
			struct eqos_prv_data *pdata,
			UINT qinx)
{
	int i;
	struct rx_ring *prx_ring =
	    GET_RX_WRAPPER_DESC(qinx);
	struct rx_swcx_desc *prx_swcx_desc = GET_RX_BUF_PTR(qinx, 0);
	struct s_rx_desc *desc = GET_RX_DESC_PTR(qinx, 0);
	dma_addr_t desc_dma = GET_RX_DESC_DMA_ADDR(qinx, 0);
	struct hw_if_struct *hw_if = &(pdata->hw_if);

	DBGPR("-->eqos_wrapper_rx_descriptor_init_single_q: "\
		"qinx = %u\n", qinx);

	memset(prx_swcx_desc, 0, (sizeof(struct rx_swcx_desc) * RX_DESC_CNT));

	for (i = 0; i < RX_DESC_CNT; i++) {
		GET_RX_DESC_PTR(qinx, i) = &desc[i];
		GET_RX_DESC_DMA_ADDR(qinx, i) =
		    (desc_dma + sizeof(struct s_rx_desc) * i);
		GET_RX_BUF_PTR(qinx, i) = &prx_swcx_desc[i];

		/* allocate skb & assign to each desc */
		if (pdata->alloc_rx_buf(pdata, GET_RX_BUF_PTR(qinx, i), GFP_KERNEL))
			break;

		wmb();
	}

	prx_ring->cur_rx = 0;
	prx_ring->dirty_rx = 0;
	prx_ring->skb_realloc_idx = 0;
	prx_ring->skb_realloc_threshold = MIN_RX_DESC_CNT;
	prx_ring->pkt_received = 0;

	hw_if->rx_desc_init(pdata, qinx);
	prx_ring->cur_rx = 0;

	DBGPR("<--eqos_wrapper_rx_descriptor_init_single_q\n");
}

static void eqos_wrapper_tx_descriptor_init(struct eqos_prv_data
						   *pdata)
{
	UINT qinx;

	DBGPR("-->eqos_wrapper_tx_descriptor_init\n");

	for (qinx = 0; qinx < EQOS_TX_QUEUE_CNT; qinx++) {
		eqos_wrapper_tx_descriptor_init_single_q(pdata, qinx);
	}

	DBGPR("<--eqos_wrapper_tx_descriptor_init\n");
}

static void eqos_wrapper_rx_descriptor_init(struct eqos_prv_data
						   *pdata)
{
	struct eqos_rx_queue *rx_queue = NULL;
	UINT qinx;

	DBGPR("-->eqos_wrapper_rx_descriptor_init\n");

	for (qinx = 0; qinx < EQOS_RX_QUEUE_CNT; qinx++) {
		rx_queue = GET_RX_QUEUE_PTR(qinx);
		rx_queue->pdata = pdata;

		/* LRO configuration */
		rx_queue->lro_mgr.dev = pdata->dev;
		memset(&rx_queue->lro_mgr.stats, 0,
			sizeof(rx_queue->lro_mgr.stats));
		rx_queue->lro_mgr.features =
			LRO_F_NAPI | LRO_F_EXTRACT_VLAN_ID;
		rx_queue->lro_mgr.ip_summed = CHECKSUM_UNNECESSARY;
		rx_queue->lro_mgr.ip_summed_aggr = CHECKSUM_UNNECESSARY;
		rx_queue->lro_mgr.max_desc = EQOS_MAX_LRO_DESC;
		rx_queue->lro_mgr.max_aggr = (0xffff/pdata->dev->mtu);
		rx_queue->lro_mgr.lro_arr = rx_queue->lro_arr;
		rx_queue->lro_mgr.get_skb_header = eqos_get_skb_hdr;
		memset(&rx_queue->lro_arr, 0, sizeof(rx_queue->lro_arr));
		rx_queue->lro_flush_needed = 0;

		eqos_wrapper_rx_descriptor_init_single_q(pdata, qinx);
	}

	DBGPR("<--eqos_wrapper_rx_descriptor_init\n");
}

/*!
* \brief API to free the receive descriptor & buffer memory.
*
* \details This function is used to free the receive descriptor & buffer memory.
*
* \param[in] pdata - pointer to private data structure.
*
* \retval void.
*/

static void eqos_rx_free_mem(struct eqos_prv_data *pdata)
{
	DBGPR("-->eqos_rx_free_mem\n");

	/* free RX descriptor */
	eqos_rx_desc_free_mem(pdata, EQOS_RX_QUEUE_CNT);

	/* free RX wrapper buffer */
	eqos_rx_buf_free_mem(pdata, EQOS_RX_QUEUE_CNT);

	DBGPR("<--eqos_rx_free_mem\n");
}

/*!
* \brief API to free the transmit descriptor & buffer memory.
*
* \details This function is used to free the transmit descriptor
* & buffer memory.
*
* \param[in] pdata - pointer to private data structure.
*
* \retval void.
*/

static void eqos_tx_free_mem(struct eqos_prv_data *pdata)
{
	DBGPR("-->eqos_tx_free_mem\n");

	/* free TX descriptor */
	eqos_tx_desc_free_mem(pdata, EQOS_TX_QUEUE_CNT);

	/* free TX buffer */
	eqos_tx_buf_free_mem(pdata, EQOS_TX_QUEUE_CNT);

	DBGPR("<--eqos_tx_free_mem\n");
}

/*!
 * \details This function is invoked by other function to free
 * the tx socket buffers.
 *
 * \param[in] pdata – pointer to private data structure.
 *
 * \return void
 */

static void eqos_tx_skb_free_mem_single_q(struct eqos_prv_data *pdata,
							UINT qinx)
{
	struct tx_ring *ptx_ring =
	    GET_TX_WRAPPER_DESC(qinx);

	DBGPR("-->%s(): qinx = %u\n", __func__, qinx);

	/* Unmap and return skb for tx desc/bufs owned by hw.
	 * Caller ensures that hw is no longer accessing these descriptors
	 */
	while (ptx_ring->tx_pkt_queued > 0) {
		tx_swcx_free(pdata,
			     GET_TX_BUF_PTR(qinx, ptx_ring->dirty_tx));

		INCR_TX_DESC_INDEX(ptx_ring->dirty_tx, 1);
		ptx_ring->free_desc_cnt++;
		ptx_ring->tx_pkt_queued--;
	}
	DBGPR("<--%s()\n", __func__);
}

/*!
* \brief API to free the transmit descriptor skb memory.
*
* \details This function is used to free the transmit descriptor skb memory.
*
* \param[in] pdata - pointer to private data structure.
*
* \retval void.
*/

static void eqos_tx_skb_free_mem(struct eqos_prv_data *pdata,
					UINT tx_qcnt)
{
	UINT qinx;

	DBGPR("-->eqos_tx_skb_free_mem: tx_qcnt = %d\n", tx_qcnt);

	for (qinx = 0; qinx < tx_qcnt; qinx++)
		eqos_tx_skb_free_mem_single_q(pdata, qinx);

	DBGPR("<--eqos_tx_skb_free_mem\n");
}


/*!
 * \details This function is invoked by other function to free
 * the rx socket buffers.
 *
 * \param[in] pdata – pointer to private data structure.
 *
 * \return void
 */

static void eqos_rx_skb_free_mem_single_q(struct eqos_prv_data *pdata,
							UINT qinx)
{
	UINT i;

	DBGPR("-->eqos_rx_skb_free_mem_single_q: qinx = %u\n", qinx);

	for (i = 0; i < RX_DESC_CNT; i++)
		eqos_unmap_rx_skb(pdata, GET_RX_BUF_PTR(qinx, i));

	DBGPR("<--eqos_rx_skb_free_mem_single_q\n");
}

/*!
* \brief API to free the receive descriptor skb memory.
*
* \details This function is used to free the receive descriptor skb memory.
*
* \param[in] pdata - pointer to private data structure.
*
* \retval void.
*/

static void eqos_rx_skb_free_mem(struct eqos_prv_data *pdata,
					UINT rx_qcnt)
{
	UINT qinx;

	DBGPR("-->eqos_rx_skb_free_mem: rx_qcnt = %d\n", rx_qcnt);

	for (qinx = 0; qinx < rx_qcnt; qinx++)
		eqos_rx_skb_free_mem_single_q(pdata, qinx);

	DBGPR("<--eqos_rx_skb_free_mem\n");
}

/*!
* \brief API to free the transmit descriptor wrapper buffer memory.
*
* \details This function is used to free the transmit descriptor wrapper buffer memory.
*
* \param[in] pdata - pointer to private data structure.
*
* \retval void.
*/

static void eqos_tx_buf_free_mem(struct eqos_prv_data *pdata,
					UINT tx_qcnt)
{
	UINT qinx;

	DBGPR("-->eqos_tx_buf_free_mem: tx_qcnt = %d\n", tx_qcnt);

	for (qinx = 0; qinx < tx_qcnt; qinx++) {
		/* free TX buffer */
		if (GET_TX_BUF_PTR(qinx, 0)) {
			kfree(GET_TX_BUF_PTR(qinx, 0));
			GET_TX_BUF_PTR(qinx, 0) = NULL;
		}
	}

	DBGPR("<--eqos_tx_buf_free_mem\n");
}

/*!
* \brief API to free the receive descriptor wrapper buffer memory.
*
* \details This function is used to free the receive descriptor wrapper buffer memory.
*
* \param[in] pdata - pointer to private data structure.
*
* \retval void.
*/

static void eqos_rx_buf_free_mem(struct eqos_prv_data *pdata,
					UINT rx_qcnt)
{
	UINT qinx = 0;

	DBGPR("-->eqos_rx_buf_free_mem: rx_qcnt = %d\n", rx_qcnt);

	for (qinx = 0; qinx < rx_qcnt; qinx++) {
		if (GET_RX_BUF_PTR(qinx, 0)) {
			kfree(GET_RX_BUF_PTR(qinx, 0));
			GET_RX_BUF_PTR(qinx, 0) = NULL;
		}
	}

	DBGPR("<--eqos_rx_buf_free_mem\n");
}

/*!
* \brief Assigns the network and tcp header pointers
*
* \details This function gets the ip and tcp header pointers of the packet
* in the skb and assigns them to the corresponding arguments passed to the
* function. It also sets some flags indicating that the packet to be receieved
* is an ipv4 packet and that the protocol is tcp.
*
* \param[in] *skb - pointer to the sk buffer,
* \param[in] **iph - pointer to be pointed to the ip header,
* \param[in] **tcph - pointer to be pointed to the tcp header,
* \param[in] *hdr_flags - flags to be set
* \param[in] *pdata - private data structure
*
* \return integer
*
* \retval -1 if the packet does not conform to ip protocol = TCP, else 0
*/

static int eqos_get_skb_hdr(struct sk_buff *skb, void **iph,
				   void **tcph, u64 *flags, void *ptr)
{
	struct eqos_prv_data *pdata = ptr;

	DBGPR("-->eqos_get_skb_hdr\n");

	if (!pdata->tcp_pkt)
		return -1;

	skb_reset_network_header(skb);
	skb_set_transport_header(skb, ip_hdrlen(skb));
	*iph = ip_hdr(skb);
	*tcph = tcp_hdr(skb);
	*flags = LRO_IPV4 | LRO_TCP;

	DBGPR("<--eqos_get_skb_hdr\n");

	return 0;
}


/*!
 * \brief api to handle tso
 *
 * \details This function is invoked by start_xmit functions. This function
 * will get all the tso details like MSS(Maximum Segment Size), packet header length,
 * packet pay load length and tcp header length etc if the given skb has tso
 * packet and store it in other wrapper tx structure for later usage.
 *
 * \param[in] dev – pointer to net device structure.
 * \param[in] skb – pointer to socket buffer structure.
 *
 * \return integer
 *
 * \retval 1 on success, -ve no failure and 0 if not tso pkt
 * */

static int eqos_handle_tso(struct net_device *dev,
	struct sk_buff *skb)
{
	struct eqos_prv_data *pdata = netdev_priv(dev);
	UINT qinx = skb_get_queue_mapping(skb);
	struct s_tx_pkt_features *tx_pkt_features = GET_TX_PKT_FEATURES_PTR(qinx);
	int ret = 1;

	DBGPR("-->eqos_handle_tso\n");

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

	/* get TSO details */
	tx_pkt_features->mss = skb_shinfo(skb)->gso_size;
	tx_pkt_features->hdr_len = skb_transport_offset(skb) + tcp_hdrlen(skb);
	tx_pkt_features->pay_len = (skb->len - tx_pkt_features->hdr_len);
	tx_pkt_features->tcp_hdr_len = tcp_hdrlen(skb);

	DBGPR("mss         = %lu\n", tx_pkt_features->mss);
	DBGPR("hdr_len     = %lu\n", tx_pkt_features->hdr_len);
	DBGPR("pay_len     = %lu\n", tx_pkt_features->pay_len);
	DBGPR("tcp_hdr_len = %lu\n", tx_pkt_features->tcp_hdr_len);

	DBGPR("<--eqos_handle_tso\n");

	return ret;
}

/* returns 0 on success and -ve on failure */
static int eqos_map_non_page_buffs_64(struct eqos_prv_data *pdata,
	struct tx_swcx_desc *ptx_swcx_desc, struct sk_buff *skb,
	unsigned int offset, unsigned int size)
{
	DBGPR("-->eqos_map_non_page_buffs_64");

	if (size > EQOS_MAX_DATA_PER_TX_BUF) {
		pr_err("failed to allocate buffer(size = %d) with %d size\n",
				EQOS_MAX_DATA_PER_TX_BUF,
				size);
		return -ENOMEM;
	}

	ptx_swcx_desc->dma = dma_map_single((&pdata->pdev->dev),
			(skb->data + offset),
			size, DMA_TO_DEVICE);

	if (dma_mapping_error((&pdata->pdev->dev), ptx_swcx_desc->dma)) {
		pr_err("failed to do the dma map\n");
		ptx_swcx_desc->dma = 0;
		return -ENOMEM;
	}
	ptx_swcx_desc->len = size;
	ptx_swcx_desc->buf1_mapped_as_page = Y_FALSE;

	DBGPR("<--eqos_map_non_page_buffs_64");
	return 0;
}

static int eqos_map_page_buffs_64(struct eqos_prv_data *pdata,
			struct tx_swcx_desc *ptx_swcx_desc,
			struct skb_frag_struct *frag,
			unsigned int offset,
			unsigned int size)
{
	unsigned int page_idx = (frag->page_offset + offset) >> PAGE_SHIFT;
	unsigned int page_offset = (frag->page_offset + offset) & ~PAGE_MASK;
	DBGPR("-->eqos_map_page_buffs_64\n");
	/* fill the first buffer pointer in buffer->dma */
	ptx_swcx_desc->dma = dma_map_page((&pdata->pdev->dev),
				(frag->page.p + page_idx),
				page_offset,
				size, DMA_TO_DEVICE);
	if (dma_mapping_error((&pdata->pdev->dev),
				ptx_swcx_desc->dma)) {
		pr_err("failed to do the dma map\n");
		ptx_swcx_desc->dma = 0;
		return -ENOMEM;
	}
	ptx_swcx_desc->len = size;
	ptx_swcx_desc->buf1_mapped_as_page = Y_TRUE;

	DBGPR("<--eqos_map_page_buffs_64\n");
	return 0;
}


/*!
 * \details This function is invoked by start_xmit functions. Given a skb
 * this function will allocate and initialize tx_swcx entries.
 * Function needs to handle case where there is not enough free tx_swcx to
 * handle the skb.  A free tx_swcx entry is one with len set to zero.
 * Note that since tx_swcx mirrors the tx descriptor ring, an entry must
 * be used if a context descriptor is needed.  A len value of "-1" is used for
 * these tx_swcx entries.
 *
 * \param[in] dev – pointer to net device structure.
 * \param[in] skb – pointer to socket buffer structure.
 *
 * \return unsigned int
 *
 * \retval count of number of tx_swcx entries allocated.  "-1" is returned
 * if there is a map failure.  "0" is returned if there is not a free
 * tx_swcx entry.
 */

static int tx_swcx_alloc(struct net_device *dev, struct sk_buff *skb)
{
	struct eqos_prv_data *pdata = netdev_priv(dev);

	uint qinx = skb_get_queue_mapping(skb);

	struct tx_ring *ptx_ring = GET_TX_WRAPPER_DESC(qinx);
	int idx = (int)ptx_ring->cur_tx;

	struct tx_swcx_desc *ptx_swcx = NULL;
	struct s_tx_pkt_features *ppkt_opts = GET_TX_PKT_FEATURES_PTR(qinx);

	uint frag_cnt;
	uint hdr_len = 0;
	uint i;
	uint cnt = 0, offset = 0, size;
	int len;
	int totlen = 0;
	int ret = -1;
	bool is_pkt_tso, is_pkt_vlan;

	DBGPR("-->%s(): cur_tx = %d, qinx = %u\n", __func__, idx, qinx);

	TX_PKT_FEATURES_PKT_ATTRIBUTES_TSO_ENABLE_RD(
		ppkt_opts->pkt_attributes, is_pkt_tso);
	TX_PKT_FEATURES_PKT_ATTRIBUTES_VLAN_PKT_RD(
		ppkt_opts->pkt_attributes, is_pkt_vlan);

	if ((is_pkt_vlan) ||
	    (is_pkt_tso)) {
		ptx_swcx = GET_TX_BUF_PTR(qinx, idx);
		if (ptx_swcx->len)
			goto tx_swcx_alloc_failed;

		ptx_swcx->len = -1;
		cnt++;
		INCR_TX_DESC_INDEX(idx, 1);
	}

	if (is_pkt_tso) {
		hdr_len = skb_transport_offset(skb) + tcp_hdrlen(skb);
		len = hdr_len;
	} else {
		len = (skb->len - skb->data_len);
	}

	DBGPR("%s(): skb->len - skb->data_len = %d, hdr_len = %d\n",
	      __func__, len, hdr_len);

	totlen += len;
	while (len) {
		ptx_swcx = GET_TX_BUF_PTR(qinx, idx);
		if (ptx_swcx->len)
			goto tx_swcx_alloc_failed;

		size = min(len, EQOS_MAX_DATA_PER_TXD);

		ret = eqos_map_non_page_buffs_64(pdata, ptx_swcx,
						 skb, offset, size);
		if (ret < 0)
			goto tx_swcx_map_failed;

		len -= size;
		offset += size;
		cnt++;

		INCR_TX_DESC_INDEX(idx, 1);
	}

	/* Process remaining pay load in skb->data in case of TSO packet */
	if (is_pkt_tso) {
		len = ((skb->len - skb->data_len) - hdr_len);
		totlen += len;
		while (len > 0) {
			ptx_swcx = GET_TX_BUF_PTR(qinx, idx);
			if (ptx_swcx->len)
				goto tx_swcx_alloc_failed;

			size = min(len, EQOS_MAX_DATA_PER_TXD);

			ret = eqos_map_non_page_buffs_64(pdata, ptx_swcx,
							 skb, offset, size);
			if (ret < 0)
				goto tx_swcx_map_failed;

			len -= size;
			offset += size;
			cnt++;

			INCR_TX_DESC_INDEX(idx, 1);
		}
	}

	/* Process fragmented skb's */
	frag_cnt = skb_shinfo(skb)->nr_frags;
	for (i = 0; i < frag_cnt; i++) {
		struct skb_frag_struct *frag = &skb_shinfo(skb)->frags[i];

		len = frag->size;
		totlen += len;
		offset = 0;
		while (len) {
			ptx_swcx = GET_TX_BUF_PTR(qinx, idx);
		if (ptx_swcx->len)
			goto tx_swcx_alloc_failed;

			size = min(len, EQOS_MAX_DATA_PER_TXD);

			ret = eqos_map_page_buffs_64(pdata, ptx_swcx,
						     frag, offset, size);
			if (ret < 0)
				goto tx_swcx_map_failed;

			len -= size;
			offset += size;
			cnt++;

			INCR_TX_DESC_INDEX(idx, 1);
		}
	}
	ptx_swcx->skb = skb;

	ppkt_opts->desc_cnt = cnt;
	if (!is_pkt_tso)
		ppkt_opts->pay_len = totlen;

	DBGPR("<--%s(): ptx_swcx->dma = %#llx\n",
	      __func__, (ULONG_LONG) ptx_swcx->dma);

	return cnt;

tx_swcx_alloc_failed:

	ret = 0;
	DECR_TX_DESC_INDEX(idx);

tx_swcx_map_failed:
	while (cnt) {
		ptx_swcx = GET_TX_BUF_PTR(qinx, idx);
		tx_swcx_free(pdata, ptx_swcx);
		DECR_TX_DESC_INDEX(idx);
		cnt--;
	}
	return ret;
}


static void tx_swcx_free(struct eqos_prv_data *pdata,
			 struct tx_swcx_desc *ptx_swcx)
{
	DBGPR("-->%s()\n", __func__);
	if (ptx_swcx->dma) {
		if (ptx_swcx->buf1_mapped_as_page == Y_TRUE)
			dma_unmap_page((&pdata->pdev->dev), ptx_swcx->dma,
				       ptx_swcx->len,
				       DMA_TO_DEVICE);
		else
			dma_unmap_single((&pdata->pdev->dev),
					 ptx_swcx->dma,
					 ptx_swcx->len,
					 DMA_TO_DEVICE);

		ptx_swcx->dma = 0;
	}

	if (ptx_swcx->skb != NULL) {
		dev_kfree_skb_any(ptx_swcx->skb);
		ptx_swcx->skb = NULL;
	}
	ptx_swcx->len = 0;

	DBGPR("<--%s()\n", __func__);
}

/*!
 * \details This function is invoked by other function for releasing the socket
 * buffer which are received by device and passed to upper layer.
 *
 * \param[in] pdata – pointer to private device structure.
 * \param[in] buffer – pointer to rx wrapper buffer structure.
 *
 * \return void
 */

static void eqos_unmap_rx_skb(struct eqos_prv_data *pdata,
				     struct rx_swcx_desc *prx_swcx_desc)
{
	DBGPR("-->eqos_unmap_rx_skb\n");

	/* unmap the first buffer */
	if (prx_swcx_desc->dma) {
		if (pdata->dev->mtu > EQOS_ETH_FRAME_LEN) {
			dma_unmap_page(&pdata->pdev->dev, prx_swcx_desc->dma,
				       PAGE_SIZE, DMA_FROM_DEVICE);
		} else {
			dma_unmap_single(&pdata->pdev->dev, prx_swcx_desc->dma,
					 pdata->rx_buffer_len,
					 DMA_FROM_DEVICE);
		}
		prx_swcx_desc->dma = 0;
	}

	/* page1 will be present only if JUMBO is enabled */
	if (prx_swcx_desc->page) {
		put_page(prx_swcx_desc->page);
		prx_swcx_desc->page = NULL;
	}
	if (prx_swcx_desc->skb) {
		dev_kfree_skb_any(prx_swcx_desc->skb);
		prx_swcx_desc->skb = NULL;
	}

	DBGPR("<--eqos_unmap_rx_skb\n");
}

/*!
* \brief API to re-allocate the new skb to rx descriptors.
*
* \details This function is used to re-allocate & re-assign the new skb to
* receive descriptors from which driver has read the data. Also ownership bit
* and other bits are reset so that device can reuse the descriptors.
*
* \param[in] pdata - pointer to private data structure.
*
* \return void.
*/

static void eqos_re_alloc_skb(struct eqos_prv_data *pdata,
				UINT qinx)
{
	int i;
	struct rx_ring *prx_ring =
	    GET_RX_WRAPPER_DESC(qinx);
	struct rx_swcx_desc *prx_swcx_desc = NULL;
	struct hw_if_struct *hw_if = &pdata->hw_if;
	int tail_idx;

	DBGPR("-->%s: prx_ring->skb_realloc_idx = %d qinx=%u\n",
	      __func__, prx_ring->skb_realloc_idx, qinx);

	for (i = 0; i < prx_ring->dirty_rx; i++) {
		prx_swcx_desc = GET_RX_BUF_PTR(qinx, prx_ring->skb_realloc_idx);
		/* allocate skb & assign to each desc */
		if (pdata->alloc_rx_buf(pdata, prx_swcx_desc, GFP_ATOMIC)) {
			pr_err("Failed to re allocate skb\n");
			pdata->xstats.q_re_alloc_rx_buf_failed[qinx]++;
			break;
		}

		wmb();
		hw_if->rx_desc_reset(prx_ring->skb_realloc_idx, pdata,
				     prx_swcx_desc->inte, qinx);
		INCR_RX_DESC_INDEX(prx_ring->skb_realloc_idx, 1);
	}
	tail_idx = prx_ring->skb_realloc_idx;
	DECR_RX_DESC_INDEX(tail_idx);
	hw_if->update_rx_tail_ptr(qinx,
		GET_RX_DESC_DMA_ADDR(qinx, tail_idx));
	prx_ring->dirty_rx = 0;

	DBGPR("<--eqos_re_alloc_skb\n");

	return;
}

/*!
* \brief API to initialize the function pointers.
*
* \details This function is called in probe to initialize all the function
* pointers which are used in other functions to manage edscriptors.
*
* \param[in] desc_if - pointer to desc_if_struct structure.
*
* \return void.
*/

void eqos_init_function_ptrs_desc(struct desc_if_struct *desc_if)
{

	DBGPR("-->eqos_init_function_ptrs_desc\n");

	desc_if->alloc_queue_struct = eqos_alloc_queue_struct;
	desc_if->free_queue_struct = eqos_free_queue_struct;
	desc_if->alloc_buff_and_desc = allocate_buffer_and_desc;
	desc_if->free_buff_and_desc = free_buffer_and_desc;
	desc_if->realloc_skb = eqos_re_alloc_skb;
	desc_if->unmap_rx_skb = eqos_unmap_rx_skb;
	desc_if->tx_swcx_free = tx_swcx_free;
	desc_if->tx_swcx_alloc = tx_swcx_alloc;
	desc_if->tx_free_mem = eqos_tx_free_mem;
	desc_if->rx_free_mem = eqos_rx_free_mem;
	desc_if->wrapper_tx_desc_init = eqos_wrapper_tx_descriptor_init;
	desc_if->wrapper_tx_desc_init_single_q =
	    eqos_wrapper_tx_descriptor_init_single_q;
	desc_if->wrapper_rx_desc_init = eqos_wrapper_rx_descriptor_init;
	desc_if->wrapper_rx_desc_init_single_q =
	    eqos_wrapper_rx_descriptor_init_single_q;

	desc_if->rx_skb_free_mem = eqos_rx_skb_free_mem;
	desc_if->rx_skb_free_mem_single_q = eqos_rx_skb_free_mem_single_q;
	desc_if->tx_skb_free_mem = eqos_tx_skb_free_mem;
	desc_if->tx_skb_free_mem_single_q = eqos_tx_skb_free_mem_single_q;

	desc_if->handle_tso = eqos_handle_tso;

	DBGPR("<--eqos_init_function_ptrs_desc\n");
}
