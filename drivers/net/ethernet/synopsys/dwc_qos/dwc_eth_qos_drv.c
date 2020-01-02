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
#include <linux/clk.h>
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>
#include <linux/platform_device.h>

#include "dwc_eth_qos_yheader.h"
#include "dwc_eth_qos_yapphdr.h"
#include "dwc_eth_qos_drv.h"
#include "dwc_eth_qos_yregacc.h"

#define OPTIONAL_CLK_INDEX 4
static int dwc_eqos_gstatus;

/* SA(Source Address) operations on TX */
unsigned char mac_addr0[6] = { 0x00, 0x11, 0x22, 0x33, 0x44, 0x55 };
unsigned char mac_addr1[6] = { 0x00, 0x66, 0x77, 0x88, 0x99, 0xaa };

/* module parameters for configuring the queue modes
 * set default mode as GENERIC
 */
static int q_op_mode[DWC_EQOS_MAX_TX_QUEUE_CNT] = {
	DWC_EQOS_Q_GENERIC,
	DWC_EQOS_Q_GENERIC,
	DWC_EQOS_Q_GENERIC,
	DWC_EQOS_Q_GENERIC,
	DWC_EQOS_Q_GENERIC,
	DWC_EQOS_Q_GENERIC,
	DWC_EQOS_Q_GENERIC,
	DWC_EQOS_Q_GENERIC
};

module_param_array(q_op_mode, int, NULL, 0644);
MODULE_PARM_DESC(q_op_mode,
		 "MTL queue operation mode" \
		 "[0-DISABLED, 1-AVB, 2-DCB, 3-GENERIC]");
bool tstamp_raw_monotonic;
module_param(tstamp_raw_monotonic, bool, 0644);
MODULE_PARM_DESC(tstamp_raw_monotonic,
		 "PTP timestamp in raw monotonic mode" \
		 "[0-DISABLED, 1-ENABLED]");

static void dwc_eqos_napi_enable_mq(struct dwc_eqos_prv_data *pdata)
{
	int qinx;

	DBGPR("-->dwc_eqos_napi_enable_mq\n");

	for (qinx = 0; qinx < pdata->rx_queue_cnt; qinx++) {
		struct dwc_eqos_rx_queue *rx_queue = NULL;

		rx_queue = &pdata->rx_queue[qinx];
		napi_enable(&rx_queue->napi);
	}

	for (qinx = 0; qinx < pdata->tx_queue_cnt; qinx++) {
		struct dwc_eqos_tx_queue *tx_queue = NULL;

		tx_queue = &pdata->tx_queue[qinx];
		napi_enable(&tx_queue->napi);
	}

	DBGPR("<--dwc_eqos_napi_enable_mq\n");
}

static void dwc_eqos_all_ch_napi_disable(struct dwc_eqos_prv_data *pdata)
{
	int qinx;

	DBGPR("-->dwc_eqos_napi_enable\n");

	for (qinx = 0; qinx < pdata->rx_queue_cnt; qinx++) {
		struct dwc_eqos_rx_queue *rx_queue = NULL;

		rx_queue = &pdata->rx_queue[qinx];
		napi_disable(&rx_queue->napi);
	}

	for (qinx = 0; qinx < pdata->tx_queue_cnt; qinx++) {
		struct dwc_eqos_tx_queue *tx_queue = NULL;

		tx_queue = &pdata->tx_queue[qinx];
		napi_disable(&tx_queue->napi);
	}

	DBGPR("<--dwc_eqos_napi_enable\n");
}

static int dwc_eqos_rxmux_setup(struct dwc_eqos_prv_data *pdata, bool external)
{
	/* doesn't support RX clock mux */
	if (!pdata->rxclk_mux)
		return 0;

	if (external)
		return clk_set_parent(pdata->rxclk_mux, pdata->rxclk_external);
	else
		return clk_set_parent(pdata->rxclk_mux, pdata->rxclk_internal);
}

static int dwc_eqos_setup_rxclock_skew(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	char *clk_str = "rx-clock-skew";

	/* no phy ? */
	if (np &&
	    of_property_match_string(pdev->dev.of_node, "use-phy", "NONE_PHY") >= 0)
		clk_str = "rx-clock-skew-no-phy";

	if (np && of_property_read_bool(np, clk_str)) {
		unsigned int reg, val;
		struct regmap *syscon = syscon_regmap_lookup_by_phandle(np, clk_str);

		if (IS_ERR(syscon)) {
			dev_err(&pdev->dev, "couldn't get the %s syscon!\n", clk_str);
			return -1;
		}

		if (of_property_read_u32_index(np, clk_str, 1, &reg)) {
			dev_err(&pdev->dev, "couldn't get the %s reg. offset!\n", clk_str);
			return -1;
		}

		if (of_property_read_u32_index(np, clk_str, 2, &val)) {
			dev_err(&pdev->dev, "couldn't get the %s reg. val!\n", clk_str);
			return -1;
		}

		regmap_write(syscon, reg, val);
	}

	return 0;
}

static void dwc_eqos_tx_desc_mang_ds_dump(struct dwc_eqos_prv_data *pdata)
{
	struct dwc_eqos_tx_wrapper_descriptor *tx_desc_data = NULL;
	struct s_tx_normal_desc *tx_desc = NULL;
	int qinx, i;

//#ifndef YDEBUG
	return;
//#endif
	pr_alert("/**** TX DESC MANAGEMENT DATA STRUCTURE DUMP ****/\n");

	pr_alert("TX_DESC_QUEUE_CNT = %d\n", pdata->tx_queue_cnt);
	for (qinx = 0; qinx < pdata->tx_queue_cnt; qinx++) {
		tx_desc_data = GET_TX_WRAPPER_DESC(qinx);

		pr_alert("DMA CHANNEL = %d\n", qinx);

		pr_alert("\tcur_tx           = %d\n",
			 tx_desc_data->cur_tx);
		pr_alert("\tdirty_tx         = %d\n",
			 tx_desc_data->dirty_tx);
		pr_alert("\tfree_desc_cnt    = %d\n",
			 tx_desc_data->free_desc_cnt);
		pr_alert("\ttx_pkt_queued    = %d\n",
			 tx_desc_data->tx_pkt_queued);
		pr_alert("\tqueue_stopped    = %d\n",
			 tx_desc_data->queue_stopped);
		pr_alert("\tpacket_count     = %d\n",
			 tx_desc_data->packet_count);
		pr_alert("\ttx_threshold_val = %d\n",
			 tx_desc_data->tx_threshold_val);
		pr_alert("\ttsf_on           = %d\n", tx_desc_data->tsf_on);
		pr_alert("\tosf_on           = %d\n", tx_desc_data->osf_on);
		pr_alert("\ttx_pbl           = %d\n", tx_desc_data->tx_pbl);

		pr_alert("\t[<desc_add> <index>] = <TDES0> : "
			"<TDES1> : <TDES2> : <TDES3>\n");
		for (i = 0; i < TX_DESC_CNT; i++) {
			tx_desc = GET_TX_DESC_PTR(qinx, i);
			pr_alert("\t[%4p %03d] = %#x : %#x : %#x : %#x\n",
				 tx_desc, i, tx_desc->tdes0, tx_desc->tdes1,
				 tx_desc->tdes2, tx_desc->tdes3);
		}
	}

	pr_alert("/************************************************/\n");
}

static void dwc_eqos_rx_desc_mang_ds_dump(struct dwc_eqos_prv_data *pdata)
{
	struct dwc_eqos_rx_wrapper_descriptor *rx_desc_data = NULL;
	struct s_rx_normal_desc *rx_desc = NULL;
	int qinx, i;

//#ifndef YDEBUG
	return;
//#endif
	pr_alert("/**** RX DESC MANAGEMENT DATA STRUCTURE DUMP ****/\n");

	pr_alert("RX_DESC_QUEUE_CNT = %d\n", pdata->rx_queue_cnt);
	for (qinx = 0; qinx < pdata->rx_queue_cnt; qinx++) {
		rx_desc_data = GET_RX_WRAPPER_DESC(qinx);

		pr_alert("DMA CHANNEL = %d\n", qinx);

		pr_alert("\tcur_rx                = %d\n",
			 rx_desc_data->cur_rx);
		pr_alert("\tdirty_rx              = %d\n",
			 rx_desc_data->dirty_rx);
		pr_alert("\tpkt_received          = %d\n",
			 rx_desc_data->pkt_received);
		pr_alert("\tskb_realloc_idx       = %d\n",
			 rx_desc_data->skb_realloc_idx);
		pr_alert("\tskb_realloc_threshold = %d\n",
			 rx_desc_data->skb_realloc_threshold);
		pr_alert("\tuse_riwt              = %d\n",
			 rx_desc_data->use_riwt);
		pr_alert("\trx_riwt               = %d\n",
			 rx_desc_data->rx_riwt);
		pr_alert("\trx_coal_frames        = %d\n",
			 rx_desc_data->rx_coal_frames);
		pr_alert("\trx_threshold_val      = %d\n",
			 rx_desc_data->rx_threshold_val);
		pr_alert("\trsf_on                = %d\n",
			 rx_desc_data->rsf_on);
		pr_alert("\trx_pbl                = %d\n",
			 rx_desc_data->rx_pbl);

		pr_alert("\t[<desc_add> <index>] = <RDES0> : <RDES1> :"
			"<RDES2> : <RDES3>\n");
		for (i = 0; i < RX_DESC_CNT; i++) {
			rx_desc = GET_RX_DESC_PTR(qinx, i);
			pr_alert(
				"\t[%4p %03d] = %#x : %#x : %#x : %#x\n",
				 rx_desc, i, rx_desc->rdes0, rx_desc->rdes1,
				 rx_desc->rdes2, rx_desc->rdes3);
		}
	}

	pr_alert("/************************************************/\n");
}

static void dwc_eqos_restart_phy(struct dwc_eqos_prv_data *pdata)
{
	DBGPR("-->dwc_eqos_restart_phy\n");

	pdata->oldlink = 0;
	pdata->speed = 0;
	pdata->oldduplex = -1;

	if (pdata->phydev)
		phy_start_aneg(pdata->phydev);

	DBGPR("<--dwc_eqos_restart_phy\n");
}

/* This function is invoked by isr handler when device issues an FATAL
 * bus error interrupt.  Following operations are performed in this function.
 * - Stop the device.
 * - Start the device
 */
static void dwc_eqos_restart_work(struct work_struct *restart_wq)
{

	struct dwc_eqos_prv_data *pdata = container_of(restart_wq,
				struct dwc_eqos_prv_data, restart_work);

	struct desc_if_struct *desc_if = &pdata->desc_if;
	struct hw_if_struct *hw_if = &pdata->hw_if;
	struct dwc_eqos_rx_queue *rx_queue = &pdata->rx_queue[pdata->cur_qinx];
	struct dwc_eqos_tx_queue *tx_queue = &pdata->tx_queue[pdata->cur_qinx];

	DBGPR("-->dwc_eqos_restart_work\n");

	netif_stop_subqueue(pdata->ndev, pdata->cur_qinx);
	napi_disable(&rx_queue->napi);
	napi_disable(&tx_queue->napi);

	/* stop DMA TX/RX */
	hw_if->stop_dma_tx(pdata, pdata->cur_qinx);
	hw_if->stop_dma_rx(pdata, pdata->cur_qinx);

	/* free tx skb's */
	desc_if->tx_skb_free_mem_single_q(pdata, pdata->cur_qinx);
	/* free rx skb's */
	desc_if->rx_skb_free_mem_single_q(pdata, pdata->cur_qinx);

	if ((pdata->tx_queue_cnt == 0) &&
		(pdata->rx_queue_cnt == 0)) {
		/* issue software reset to device */
		hw_if->exit(pdata);

		dwc_eqos_configure_rx_fun_ptr(pdata);
		dwc_eqos_default_common_confs(pdata);
	}
	/* reset all variables */
	dwc_eqos_default_tx_confs_single_q(pdata, pdata->cur_qinx);
	dwc_eqos_default_rx_confs_single_q(pdata, pdata->cur_qinx);

	/* reinit descriptor */
	desc_if->wrapper_tx_desc_init_single_q(pdata, pdata->cur_qinx);
	desc_if->wrapper_rx_desc_init_single_q(pdata, pdata->cur_qinx);

	napi_enable(&rx_queue->napi);
	napi_enable(&tx_queue->napi);

	/* initializes MAC and DMA
	 * NOTE : Do we need to init only one channel
	 * which generate FBE
	 */
	hw_if->init(pdata);

	dwc_eqos_restart_phy(pdata);

	netif_wake_subqueue(pdata->ndev, pdata->cur_qinx);

	DBGPR("<--dwc_eqos_restart_work\n");
}

static void dwc_eqos_disable_ch_tx_interrpt(struct dwc_eqos_prv_data *pdata,
											int qinx)
{
	struct hw_if_struct *hw_if = &pdata->hw_if;

	hw_if->disable_tx_interrupt(pdata, qinx);
}

static void dwc_eqos_enable_ch_tx_interrpt(struct dwc_eqos_prv_data *pdata,
										   int qinx)
{
	struct hw_if_struct *hw_if = &pdata->hw_if;

	hw_if->enable_tx_interrupt(pdata, qinx);
}

static void dwc_eqos_disable_ch_rx_interrpt(struct dwc_eqos_prv_data *pdata,
											int qinx)
{
	struct hw_if_struct *hw_if = &pdata->hw_if;

	hw_if->disable_rx_interrupt(pdata, qinx);
}

static void dwc_eqos_enable_ch_rx_interrpt(struct dwc_eqos_prv_data *pdata,
										   int qinx)
{
	struct hw_if_struct *hw_if = &pdata->hw_if;

	hw_if->enable_rx_interrupt(pdata, qinx);
}

static void dwc_eqos_tx_napi_schedule(struct dwc_eqos_tx_queue *tx_q)
{
	struct dwc_eqos_prv_data *pdata = tx_q->pdata;
	unsigned long flags;
	unsigned int napi_scheduled = 0;

	spin_lock_irqsave(&pdata->lock, flags);
	if (likely(napi_schedule_prep(&tx_q->napi))) {
		dwc_eqos_disable_ch_tx_interrpt(pdata, tx_q->qinx);
		__napi_schedule(&tx_q->napi);
		napi_scheduled = 1;
	} else {
		dwc_eqos_disable_ch_tx_interrpt(pdata, tx_q->qinx);
	}
	spin_unlock_irqrestore(&pdata->lock, flags);

	/* Rearm the timer to reschedule NAPI, just to ensure
	 * we reaped all the completed descriptors.
	 */
	if (!napi_scheduled)
		mod_timer(&tx_q->tx_desc_data.txtimer,
				  DWC_COAL_TIMER(pdata->tx_coal_timer_us));
}

/* Interrupt Service Routine */
irqreturn_t dwc_eqos_isr_sw_dwc_eqos(int irq, void *device_id)
{
	unsigned long vardma_isr;
	unsigned long vardma_sr;
	unsigned long varmac_isr;
	unsigned long varmac_imr;
	unsigned long vardma_ier;
	struct dwc_eqos_prv_data *pdata =
	    (struct dwc_eqos_prv_data *)device_id;
	struct net_device *dev = pdata->ndev;
	struct hw_if_struct *hw_if = &pdata->hw_if;
	unsigned int qinx;
	struct dwc_eqos_rx_queue *rx_queue = NULL;
	struct dwc_eqos_tx_queue *tx_queue = NULL;
	unsigned long varmac_ans = 0;
	unsigned long varmac_pcs = 0;

	DBGPR("-->dwc_eqos_isr_sw_dwc_eqos\n");

	vardma_isr = dwceqos_readl(pdata, DWCEQOS_DMA_INTR_STAT);
	if (vardma_isr == 0x0)
		return IRQ_NONE;

	varmac_isr = dwceqos_readl(pdata, DWCEQOS_MAC_INTR_STAT);

	DBGPR("DMA_ISR = %#lx, MAC_ISR = %#lx\n", vardma_isr, varmac_isr);

	/* Handle DMA interrupts */
	for (qinx = 0; qinx < pdata->tx_queue_cnt; qinx++) {
		rx_queue = &pdata->rx_queue[qinx];
		tx_queue = &pdata->tx_queue[qinx];

		vardma_sr = dwceqos_readl(pdata, DWCEQOS_DMA_CH_STAT(qinx));
		/* clear interrupts */
		dwceqos_writel(pdata, DWCEQOS_DMA_CH_STAT(qinx), vardma_sr);

		vardma_ier = dwceqos_readl(pdata, DWCEQOS_DMA_CH_INTR_EN(qinx));
		/* handle only those DMA interrupts which are enabled */
		vardma_sr = (vardma_sr & vardma_ier);

		DBGPR("DMA_SR[%d] = %#lx\n", qinx, vardma_sr);

		if (vardma_sr == 0)
			continue;

		if ((GET_VALUE(vardma_sr, DMA_SR_RI_LPOS, DMA_SR_RI_HPOS) & 1) ||
			(GET_VALUE(vardma_sr, DMA_SR_RBU_LPOS, DMA_SR_RBU_HPOS) & 1)) {
			unsigned long flags;

			spin_lock_irqsave(&pdata->lock, flags);
			if (likely(napi_schedule_prep(&rx_queue->napi))) {
				dwc_eqos_disable_ch_rx_interrpt(pdata, rx_queue->qinx);
				__napi_schedule(&rx_queue->napi);
			} else {
				pr_warn("Rx interrupt while in poll\n");
				dwc_eqos_disable_ch_rx_interrpt(pdata, rx_queue->qinx);
			}
			spin_unlock_irqrestore(&pdata->lock, flags);

			if ((GET_VALUE(vardma_sr, DMA_SR_RI_LPOS, DMA_SR_RI_HPOS) & 1))
				pdata->xstats.rx_normal_irq_n[qinx]++;
			else
				pdata->xstats.rx_buf_unavailable_irq_n[qinx]++;
		}
		if (
		GET_VALUE(vardma_sr, DMA_SR_TI_LPOS, DMA_SR_TI_HPOS) & 1) {
			dwc_eqos_tx_napi_schedule(tx_queue);
			pdata->xstats.tx_normal_irq_n[qinx]++;
		}
		if (
		GET_VALUE(vardma_sr, DMA_SR_TPS_LPOS, DMA_SR_TPS_HPOS) & 1) {
			pdata->xstats.tx_process_stopped_irq_n[qinx]++;
			dwc_eqos_gstatus = -E_DMA_SR_TPS;
		}
		if (
		GET_VALUE(vardma_sr, DMA_SR_TBU_LPOS, DMA_SR_TBU_HPOS) & 1) {
			pdata->xstats.tx_buf_unavailable_irq_n[qinx]++;
			dwc_eqos_gstatus = -E_DMA_SR_TBU;
		}
		if (
		GET_VALUE(vardma_sr, DMA_SR_RPS_LPOS, DMA_SR_RPS_HPOS) & 1) {
			pdata->xstats.rx_process_stopped_irq_n[qinx]++;
			dwc_eqos_gstatus = -E_DMA_SR_RPS;
		}
		if (
		GET_VALUE(vardma_sr, DMA_SR_RWT_LPOS, DMA_SR_RWT_HPOS) & 1) {
			pdata->xstats.rx_watchdog_irq_n++;
			dwc_eqos_gstatus = S_DMA_SR_RWT;
		}
		if (
		GET_VALUE(vardma_sr, DMA_SR_FBE_LPOS, DMA_SR_FBE_HPOS) & 1) {
			pdata->xstats.fatal_bus_error_irq_n++;
			dwc_eqos_gstatus = -E_DMA_SR_FBE;
			pdata->cur_qinx = qinx;
			queue_work(pdata->restart_wq, &pdata->restart_work);

		}
	}

	/* Handle MAC interrupts */
	if (GET_VALUE(vardma_isr, DMA_ISR_MACIS_LPOS, DMA_ISR_MACIS_HPOS) & 1) {
		/* handle only those MAC interrupts which are enabled */
		varmac_imr = dwceqos_readl(pdata, DWCEQOS_MAC_INTR_EN);
		varmac_isr = (varmac_isr & varmac_imr);

		/* RGMII/SMII interrupt */
		if (GET_VALUE(varmac_isr, MAC_ISR_RGSMIIS_LPOS, MAC_ISR_RGSMIIS_HPOS) & 1) {
			varmac_pcs = dwceqos_readl(pdata, DWCEQOS_MAC_PHYIF_CS);
			pr_alert("RGMII/SMII interrupt: MAC_PCS = %#lx\n", varmac_pcs);
			if ((varmac_pcs & 0x80000) == 0x80000) {
				pdata->pcs_link = 1;
				netif_carrier_on(dev);
				if ((varmac_pcs & 0x10000) == 0x10000) {
					pdata->pcs_duplex = 1;
					hw_if->set_full_duplex(dev);
				} else {
					pdata->pcs_duplex = 0;
					hw_if->set_half_duplex(dev);
				}

				if ((varmac_pcs & 0x60000) == 0x0) {
					pdata->pcs_speed = SPEED_10;
					hw_if->set_mii_speed_10(dev);
				} else if ((varmac_pcs & 0x60000) == 0x20000) {
					pdata->pcs_speed = SPEED_100;
					hw_if->set_mii_speed_100(dev);
				} else if ((varmac_pcs & 0x60000) == 0x40000) {
					pdata->pcs_speed = SPEED_1000;
					hw_if->set_gmii_speed(dev);
				}
				pr_alert("Link is UP:%dMbps & %s duplex\n",
					pdata->pcs_speed,
					pdata->pcs_duplex ? "Full" : "Half");
			} else {
				pr_alert("Link is Down\n");
				pdata->pcs_link = 0;
				netif_carrier_off(dev);
			}
		}

		/* PCS Link Status interrupt */
		if (GET_VALUE(varmac_isr, MAC_ISR_PCSLCHGIS_LPOS, MAC_ISR_PCSLCHGIS_HPOS) & 1) {
			pr_alert("PCS Link Status interrupt\n");
			varmac_ans = dwceqos_readl(pdata, DWCEQOS_MAC_AN_STAT);
			if (GET_VALUE(varmac_ans, MAC_ANS_LS_LPOS, MAC_ANS_LS_HPOS) & 1) {
				pr_alert("Link: Up\n");
				netif_carrier_on(dev);
				pdata->pcs_link = 1;
			} else {
				pr_alert("Link: Down\n");
				netif_carrier_off(dev);
				pdata->pcs_link = 0;
			}
		}

		/* PCS Auto-Negotiation Complete interrupt */
		if (GET_VALUE(varmac_isr, MAC_ISR_PCSANCIS_LPOS, MAC_ISR_PCSANCIS_HPOS) & 1) {
			pr_alert("PCS Auto-Negotiation Complete interrupt\n");
			varmac_ans = dwceqos_readl(pdata, DWCEQOS_MAC_AN_STAT);
		}
	}

	DBGPR("<--dwc_eqos_isr_sw_dwc_eqos\n");

	return IRQ_HANDLED;
}

/* This function is used to check what are all the different
 * features the device supports.
 */
void dwc_eqos_get_all_hw_features(struct dwc_eqos_prv_data *pdata)
{
	unsigned int varmac_hfr0;
	unsigned int varmac_hfr1;
	unsigned int varmac_hfr2;

	DBGPR("-->dwc_eqos_get_all_hw_features\n");

	varmac_hfr0 = dwceqos_readl(pdata, DWCEQOS_MAC_HW_FEATURE0);
	varmac_hfr1 = dwceqos_readl(pdata, DWCEQOS_MAC_HW_FEATURE1);
	varmac_hfr2 = dwceqos_readl(pdata, DWCEQOS_MAC_HW_FEATURE2);

	memset(&pdata->hw_feat, 0, sizeof(pdata->hw_feat));
	pdata->hw_feat.mii_sel = ((varmac_hfr0 >> 0) & (0x1));
	pdata->hw_feat.gmii_sel = ((varmac_hfr0 >> 1) & (0x1));
	pdata->hw_feat.hd_sel = ((varmac_hfr0 >> 2) & (0x1));
	pdata->hw_feat.pcs_sel = ((varmac_hfr0 >> 3) & (0x1));
	pdata->hw_feat.vlan_hash_en =
	    ((varmac_hfr0 >> 4) & (0x1));
	pdata->hw_feat.sma_sel = ((varmac_hfr0 >> 5) & (0x1));
	pdata->hw_feat.rwk_sel = ((varmac_hfr0 >> 6) & (0x1));
	pdata->hw_feat.mgk_sel = ((varmac_hfr0 >> 7) & (0x1));
	pdata->hw_feat.mmc_sel = ((varmac_hfr0 >> 8) & (0x1));
	pdata->hw_feat.arp_offld_en =
	    ((varmac_hfr0 >> 9) & (0x1));
	pdata->hw_feat.ts_sel =
	    ((varmac_hfr0 >> 12) & (0x1));
	pdata->hw_feat.eee_sel = ((varmac_hfr0 >> 13) & (0x1));
	pdata->hw_feat.tx_coe_sel =
	    ((varmac_hfr0 >> 14) & (0x1));
	pdata->hw_feat.rx_coe_sel =
	    ((varmac_hfr0 >> 16) & (0x1));
	pdata->hw_feat.mac_addr16_sel =
	    ((varmac_hfr0 >> 18) & (0x1f));
	pdata->hw_feat.mac_addr32_sel =
	    ((varmac_hfr0 >> 23) & (0x1));
	pdata->hw_feat.mac_addr64_sel =
	    ((varmac_hfr0 >> 24) & (0x1));
	pdata->hw_feat.tsstssel =
	    ((varmac_hfr0 >> 25) & (0x3));
	pdata->hw_feat.sa_vlan_ins =
	    ((varmac_hfr0 >> 27) & (0x1));
	pdata->hw_feat.act_phy_sel =
	    ((varmac_hfr0 >> 28) & (0x7));

	pdata->hw_feat.rx_fifo_size =
	    ((varmac_hfr1 >> 0) & (0x1f));
	    //8;
	pdata->hw_feat.tx_fifo_size =
	    ((varmac_hfr1 >> 6) & (0x1f));
	    //8;
	pdata->hw_feat.adv_ts_hword =
	    ((varmac_hfr1 >> 13) & (0x1));
	pdata->hw_feat.dcb_en = ((varmac_hfr1 >> 16) & (0x1));
	pdata->hw_feat.sph_en = ((varmac_hfr1 >> 17) & (0x1));
	pdata->hw_feat.tso_en = ((varmac_hfr1 >> 18) & (0x1));
	pdata->hw_feat.dma_debug_gen =
	    ((varmac_hfr1 >> 19) & (0x1));
	pdata->hw_feat.av_sel = ((varmac_hfr1 >> 20) & (0x1));
	pdata->hw_feat.lp_mode_en =
	    ((varmac_hfr1 >> 23) & (0x1));
	pdata->hw_feat.hash_tbl_sz =
	    ((varmac_hfr1 >> 24) & (0x3));
	pdata->hw_feat.l3l4_filter_num =
	    ((varmac_hfr1 >> 27) & (0xf));

	pdata->hw_feat.rx_q_cnt = ((varmac_hfr2 >> 0) & (0xf));
	pdata->hw_feat.tx_q_cnt = ((varmac_hfr2 >> 6) & (0xf));
	pdata->hw_feat.rx_ch_cnt =
	    ((varmac_hfr2 >> 12) & (0xf));
	pdata->hw_feat.tx_ch_cnt =
	    ((varmac_hfr2 >> 18) & (0xf));
	pdata->hw_feat.pps_out_num =
	    ((varmac_hfr2 >> 24) & (0x7));
	pdata->hw_feat.aux_snap_num =
	    ((varmac_hfr2 >> 28) & (0x7));

	DBGPR("<--dwc_eqos_get_all_hw_features\n");
}

/* This function is used to print all the device feature.
 */
void dwc_eqos_print_all_hw_features(struct dwc_eqos_prv_data *pdata)
{
	char *str = NULL;

	DBGPR("-->dwc_eqos_print_all_hw_features\n");

	pr_info("\n");
	pr_info("=====================================================/\n");
	pr_info("\n");
	pr_info(
		"10/100 Mbps Support                         : %s\n",
		pdata->hw_feat.mii_sel ? "YES" : "NO");
	pr_info(
		"1000 Mbps Support                           : %s\n",
		pdata->hw_feat.gmii_sel ? "YES" : "NO");
	pr_info(
		"Half-duplex Support                         : %s\n",
		pdata->hw_feat.hd_sel ? "YES" : "NO");
	pr_info(
		"PCS Registers(TBI/SGMII/RTBI PHY interface) : %s\n",
		pdata->hw_feat.pcs_sel ? "YES" : "NO");
	pr_info(
		"VLAN Hash Filter Selected                   : %s\n",
		pdata->hw_feat.vlan_hash_en ? "YES" : "NO");
	pdata->vlan_hash_filtering = pdata->hw_feat.vlan_hash_en;
	pr_info(
		"SMA (MDIO) Interface                        : %s\n",
		pdata->hw_feat.sma_sel ? "YES" : "NO");
	pr_info(
		"PMT Remote Wake-up Packet Enable            : %s\n",
		pdata->hw_feat.rwk_sel ? "YES" : "NO");
	pr_info(
		"PMT Magic Packet Enable                     : %s\n",
		pdata->hw_feat.mgk_sel ? "YES" : "NO");
	pr_info(
		"RMON/MMC Module Enable                      : %s\n",
		pdata->hw_feat.mmc_sel ? "YES" : "NO");
	pr_info(
		"ARP Offload Enabled                         : %s\n",
		pdata->hw_feat.arp_offld_en ? "YES" : "NO");
	pr_info(
		"IEEE 1588-2008 Timestamp Enabled            : %s\n",
		pdata->hw_feat.ts_sel ? "YES" : "NO");
	pr_info(
		 "Energy Efficient Ethernet Enabled           : %s\n",
		pdata->hw_feat.eee_sel ? "YES" : "NO");
	pr_info(
		"Transmit Checksum Offload Enabled           : %s\n",
		pdata->hw_feat.tx_coe_sel ? "YES" : "NO");
	pr_info(
		"Receive Checksum Offload Enabled            : %s\n",
		pdata->hw_feat.rx_coe_sel ? "YES" : "NO");
	pr_info(
		"MAC Addresses 16–31 Selected                : %s\n",
		pdata->hw_feat.mac_addr16_sel ? "YES" : "NO");
	pr_info(
		"MAC Addresses 32–63 Selected                : %s\n",
		pdata->hw_feat.mac_addr32_sel ? "YES" : "NO");
	pr_info(
		"MAC Addresses 64–127 Selected               : %s\n",
		pdata->hw_feat.mac_addr64_sel ? "YES" : "NO");

	if (pdata->hw_feat.mac_addr64_sel)
		pdata->max_addr_reg_cnt = 128;
	else if (pdata->hw_feat.mac_addr32_sel)
		pdata->max_addr_reg_cnt = 64;
	else if (pdata->hw_feat.mac_addr16_sel)
		pdata->max_addr_reg_cnt = 32;
	else
		pdata->max_addr_reg_cnt = 1;

	switch (pdata->hw_feat.tsstssel) {
	case 0:
		str = "RESERVED";
		break;
	case 1:
		str = "INTERNAL";
		break;
	case 2:
		str = "EXTERNAL";
		break;
	case 3:
		str = "BOTH";
		break;
	}
	pr_info("Timestamp System Time Source                : %s\n", str);
	pr_info(
		"Source Address or VLAN Insertion Enable     : %s\n",
		pdata->hw_feat.sa_vlan_ins ? "YES" : "NO");

	switch (pdata->hw_feat.act_phy_sel) {
	case 0:
		str = "GMII/MII";
		break;
	case 1:
		str = "RGMII";
		break;
	case 2:
		str = "SGMII";
		break;
	case 3:
		str = "TBI";
		break;
	case 4:
		str = "RMII";
		break;
	case 5:
		str = "RTBI";
		break;
	case 6:
		str = "SMII";
		break;
	case 7:
		str = "RevMII";
		break;
	default:
		str = "RESERVED";
	}
	pr_info("Active PHY Selected                         : %s\n", str);

	switch (pdata->hw_feat.rx_fifo_size) {
	case 0:
		str = "128 bytes";
		break;
	case 1:
		str = "256 bytes";
		break;
	case 2:
		str = "512 bytes";
		break;
	case 3:
		str = "1 KBytes";
		break;
	case 4:
		str = "2 KBytes";
		break;
	case 5:
		str = "4 KBytes";
		break;
	case 6:
		str = "8 KBytes";
		break;
	case 7:
		str = "16 KBytes";
		break;
	case 8:
		str = "32 kBytes";
		break;
	case 9:
		str = "64 KBytes";
		break;
	case 10:
		str = "128 KBytes";
		break;
	case 11:
		str = "256 KBytes";
		break;
	default:
		str = "RESERVED";
	}
	pr_info("MTL Receive FIFO Size                       : %s\n", str);

	switch (pdata->hw_feat.tx_fifo_size) {
	case 0:
		str = "128 bytes";
		break;
	case 1:
		str = "256 bytes";
		break;
	case 2:
		str = "512 bytes";
		break;
	case 3:
		str = "1 KBytes";
		break;
	case 4:
		str = "2 KBytes";
		break;
	case 5:
		str = "4 KBytes";
		break;
	case 6:
		str = "8 KBytes";
		break;
	case 7:
		str = "16 KBytes";
		break;
	case 8:
		str = "32 kBytes";
		break;
	case 9:
		str = "64 KBytes";
		break;
	case 10:
		str = "128 KBytes";
		break;
	case 11:
		str = "256 KBytes";
		break;
	default:
		str = "RESERVED";
	}
	pr_info("MTL Transmit FIFO Size                       : %s\n",	str);
	pr_info(
		"IEEE 1588 High Word Register Enable          : %s\n",
		pdata->hw_feat.adv_ts_hword ? "YES" : "NO");
	pr_info(
		"DCB Feature Enable                           : %s\n",
		pdata->hw_feat.dcb_en ? "YES" : "NO");
	pr_info(
		"Split Header Feature Enable                  : %s\n",
		pdata->hw_feat.sph_en ? "YES" : "NO");
	pr_info(
		"TCP Segmentation Offload Enable              : %s\n",
		pdata->hw_feat.tso_en ? "YES" : "NO");
	pr_info(
		"DMA Debug Registers Enabled                  : %s\n",
		pdata->hw_feat.dma_debug_gen ? "YES" : "NO");
	pr_info(
		"AV Feature Enabled                           : %s\n",
		pdata->hw_feat.av_sel ? "YES" : "NO");
	pr_info(
		"Low Power Mode Enabled                       : %s\n",
		pdata->hw_feat.lp_mode_en ? "YES" : "NO");

	switch (pdata->hw_feat.hash_tbl_sz) {
	case 0:
		str = "No hash table selected";
		pdata->max_hash_table_size = 0;
		break;
	case 1:
		str = "64";
		pdata->max_hash_table_size = 64;
		break;
	case 2:
		str = "128";
		pdata->max_hash_table_size = 128;
		break;
	case 3:
		str = "256";
		pdata->max_hash_table_size = 256;
		break;
	}
	pr_info("Hash Table Size                              : %s\n",	str);
	pr_info(
		"Total number of L3 or L4 Filters      : %d L3/L4 Filter\n",
		pdata->hw_feat.l3l4_filter_num);
	pr_info(
		"Number of MTL Receive Queues                 : %d\n",
		(pdata->hw_feat.rx_q_cnt + 1));
	pr_info(
		"Number of MTL Transmit Queues                : %d\n",
		(pdata->hw_feat.tx_q_cnt + 1));
	pr_info(
		"Number of DMA Receive Channels               : %d\n",
		(pdata->hw_feat.rx_ch_cnt + 1));
	pr_info(
		"Number of DMA Transmit Channels              : %d\n",
		(pdata->hw_feat.tx_ch_cnt + 1));

	switch (pdata->hw_feat.pps_out_num) {
	case 0:
		str = "No PPS output";
		break;
	case 1:
		str = "1 PPS output";
		break;
	case 2:
		str = "2 PPS output";
		break;
	case 3:
		str = "3 PPS output";
		break;
	case 4:
		str = "4 PPS output";
		break;
	default:
		str = "RESERVED";
	}
	pr_info("Number of PPS Outputs                        : %s\n", str);

	switch (pdata->hw_feat.aux_snap_num) {
	case 0:
		str = "No auxiliary input";
		break;
	case 1:
		str = "1 auxiliary input";
		break;
	case 2:
		str = "2 auxiliary input";
		break;
	case 3:
		str = "3 auxiliary input";
		break;
	case 4:
		str = "4 auxiliary input";
		break;
	default:
		str = "RESERVED";
	}
	pr_info("Number of Auxiliary Snapshot Inputs          : %s", str);

	pr_info("\n");
	pr_info("=====================================================/\n");

	DBGPR("<--dwc_eqos_print_all_hw_features\n");
}

static void dwc_eqos_get_tx_reg_hwtstamp_work(struct work_struct *tstamp_wq);
void dwc_eqos_initialize_work_queue(struct dwc_eqos_prv_data *pdata)
{
	pdata->restart_wq = alloc_workqueue(pdata->ndev->name,
						   WQ_MEM_RECLAIM, 0);
	INIT_WORK(&pdata->restart_work, dwc_eqos_restart_work);
	INIT_DELAYED_WORK(&pdata->delayed_tstamp_work, dwc_eqos_get_tx_reg_hwtstamp_work);
}

static netdev_features_t dwc_eqos_features_check(struct sk_buff *skb, struct net_device *dev,
                    netdev_features_t features)
{
       unsigned int qinx = skb_get_queue_mapping(skb);

	if(qinx == 0 || qinx == 1)
		return features | (NETIF_F_IP_CSUM |
		                   NETIF_F_HW_CSUM |
		                   NETIF_F_IPV6_CSUM );

	else
		return features & ~( NETIF_F_IP_CSUM|
		                     NETIF_F_HW_CSUM |
		                     NETIF_F_IPV6_CSUM );

}

static const struct net_device_ops dwc_eqos_netdev_ops = {
	.ndo_open = dwc_eqos_open,
	.ndo_stop = dwc_eqos_close,
	.ndo_start_xmit = dwc_eqos_start_xmit,
	.ndo_get_stats = dwc_eqos_get_stats,
	.ndo_set_rx_mode = dwc_eqos_set_rx_mode,
	.ndo_features_check = dwc_eqos_features_check,
#ifdef CONFIG_NET_POLL_CONTROLLER
	.ndo_poll_controller = dwc_eqos_poll_controller,
#endif /* end of CONFIG_NET_POLL_CONTROLLER */
	.ndo_set_features = dwc_eqos_set_features,
	.ndo_fix_features = dwc_eqos_fix_features,
	.ndo_do_ioctl = dwc_eqos_ioctl,
	.ndo_change_mtu = dwc_eqos_change_mtu,
	.ndo_vlan_rx_add_vid = dwc_eqos_vlan_rx_add_vid,
	.ndo_vlan_rx_kill_vid = dwc_eqos_vlan_rx_kill_vid,
};

struct net_device_ops *dwc_eqos_get_netdev_ops(void)
{
	return (struct net_device_ops *)&dwc_eqos_netdev_ops;
}

/* This function is invoked by other api's for
 * allocating the Rx skb's if split header feature is enabled.
 */
static int dwc_eqos_alloc_split_hdr_rx_buf(
		struct dwc_eqos_prv_data *pdata,
		struct dwc_eqos_rx_buffer *buffer,
		gfp_t gfp)
{
	struct sk_buff *skb = buffer->skb;

	DBGPR("-->dwc_eqos_alloc_split_hdr_rx_buf\n");

	if (skb) {
		skb_trim(skb, 0);
		goto check_page;
	}

	buffer->rx_hdr_size = DWC_EQOS_MAX_HDR_SIZE;
	/* allocate twice the maximum header size */
	skb = __netdev_alloc_skb_ip_align(
			pdata->ndev, (2 * buffer->rx_hdr_size),	gfp);
	if (!skb) {
		pr_alert("Failed to allocate skb\n");
		return -ENOMEM;
	}
	buffer->skb = skb;
	DBGPR("Maximum header buffer size allocated = %d\n",
		buffer->rx_hdr_size);
 check_page:
	if (!buffer->dma)
		buffer->dma = dma_map_single(pdata->ndev->dev.parent,
					buffer->skb->data,
					(2 * buffer->rx_hdr_size),
					DMA_FROM_DEVICE);
	buffer->len = buffer->rx_hdr_size;

	/* allocate a new page if necessary */
	if (!buffer->page2) {
		buffer->page2 = alloc_page(gfp);
		if (unlikely(!buffer->page2)) {
			pr_alert("Failed to allocate page for second buffer\n");
			return -ENOMEM;
		}
	}
	if (!buffer->dma2)
		buffer->dma2 = dma_map_page(pdata->ndev->dev.parent,
				    buffer->page2, 0,
				    PAGE_SIZE, DMA_FROM_DEVICE);
	buffer->len2 = PAGE_SIZE;
	buffer->mapped_as_page = Y_TRUE;

	DBGPR("<--dwc_eqos_alloc_split_hdr_rx_buf\n");

	return 0;
}

/* This function is invoked by other api's for
 * allocating the Rx skb's if jumbo frame is enabled.
 */
static int dwc_eqos_alloc_jumbo_rx_buf(struct dwc_eqos_prv_data *pdata,
					  struct dwc_eqos_rx_buffer *buffer,
					  gfp_t gfp)
{
	struct sk_buff *skb = buffer->skb;
	unsigned int bufsz = (256 - 16);	/* for skb_reserve */

	DBGPR("-->dwc_eqos_alloc_jumbo_rx_buf\n");

	if (skb) {
		skb_trim(skb, 0);
		goto check_page;
	}

	skb = __netdev_alloc_skb_ip_align(pdata->ndev, bufsz, gfp);
	if (!skb) {
		pr_alert("Failed to allocate skb\n");
		return -ENOMEM;
	}
	buffer->skb = skb;
 check_page:
	/* allocate a new page if necessary */
	if (!buffer->page) {
		buffer->page = alloc_page(gfp);
		if (unlikely(!buffer->page)) {
			pr_alert("Failed to allocate page\n");
			return -ENOMEM;
		}
	}
	if (!buffer->dma)
		buffer->dma = dma_map_page(pdata->ndev->dev.parent,
					   buffer->page, 0,
					   PAGE_SIZE, DMA_FROM_DEVICE);
	buffer->len = PAGE_SIZE;

	if (!buffer->page2) {
		buffer->page2 = alloc_page(gfp);
		if (unlikely(!buffer->page2)) {
			pr_alert(
			       "Failed to allocate page for second buffer\n");
			return -ENOMEM;
		}
	}
	if (!buffer->dma2)
		buffer->dma2 = dma_map_page(pdata->ndev->dev.parent,
					    buffer->page2, 0,
					    PAGE_SIZE, DMA_FROM_DEVICE);
	buffer->len2 = PAGE_SIZE;

	buffer->mapped_as_page = Y_TRUE;

	DBGPR("<--dwc_eqos_alloc_jumbo_rx_buf\n");

	return 0;
}

/* This function is invoked by other api's for
 * allocating the Rx skb's with default Rx mode ie non-jumbo
 * and non-split header mode.
 */
static int dwc_eqos_alloc_rx_buf(struct dwc_eqos_prv_data *pdata,
				    struct dwc_eqos_rx_buffer *buffer,
				    gfp_t gfp)
{
	struct sk_buff *skb = buffer->skb;

	DBGPR("-->dwc_eqos_alloc_rx_buf\n");

	if (skb) {
		skb_trim(skb, 0);
		goto map_skb;
	}

	skb = __netdev_alloc_skb_ip_align(
			pdata->ndev, pdata->rx_buffer_len, gfp);
	if (!skb) {
		pr_alert("Failed to allocate skb\n");
		return -ENOMEM;
	}
	buffer->skb = skb;
	buffer->len = pdata->rx_buffer_len;
 map_skb:
	buffer->dma = dma_map_single(pdata->ndev->dev.parent, skb->data,
				     pdata->rx_buffer_len, DMA_FROM_DEVICE);
	if (dma_mapping_error(pdata->ndev->dev.parent, buffer->dma))
		pr_alert("failed to do the RX dma map\n");

	buffer->mapped_as_page = Y_FALSE;

	DBGPR("<--dwc_eqos_alloc_rx_buf\n");

	return 0;
}

/* This function will initialize the receive function pointers
 * which are used for allocating skb's and receiving the packets based
 * Rx mode - default/jumbo/split header.
 */
static void dwc_eqos_configure_rx_fun_ptr(struct dwc_eqos_prv_data *pdata)
{
	DBGPR("-->dwc_eqos_configure_rx_fun_ptr\n");

	if (pdata->rx_split_hdr) {
		pdata->clean_rx = dwc_eqos_clean_split_hdr_rx_irq;
		pdata->alloc_rx_buf = dwc_eqos_alloc_split_hdr_rx_buf;
	} else if (pdata->ndev->mtu > DWC_EQOS_ETH_FRAME_LEN) {
		pdata->clean_rx = dwc_eqos_clean_jumbo_rx_irq;
		pdata->alloc_rx_buf = dwc_eqos_alloc_jumbo_rx_buf;
	} else {
		pdata->rx_buffer_len = ALIGN(DWC_EQOS_ETH_FRAME_LEN, 128);
		pdata->clean_rx = dwc_eqos_clean_rx_irq;
		pdata->alloc_rx_buf = dwc_eqos_alloc_rx_buf;
	}

	DBGPR("<--dwc_eqos_configure_rx_fun_ptr\n");
}

/* This function is used to initialize differnet parameters to
 * default values which are common parameters between Tx and Rx path.
 */
static void dwc_eqos_default_common_confs(struct dwc_eqos_prv_data *pdata)
{
	DBGPR("-->dwc_eqos_default_common_confs\n");

	pdata->drop_tx_pktburstcnt = 1;
	pdata->mac_enable_count = 0;
	pdata->incr_incrx = DWC_EQOS_INCR_ENABLE;
	pdata->flow_ctrl = DWC_EQOS_FLOW_CTRL_TX_RX;
	pdata->oldflow_ctrl = DWC_EQOS_FLOW_CTRL_TX_RX;
	pdata->tx_sa_ctrl_via_desc = DWC_EQOS_SA0_NONE;
	pdata->tx_sa_ctrl_via_reg = DWC_EQOS_SA0_NONE;
	pdata->hwts_tx_en = 0;
	pdata->hwts_rx_en = 0;
	pdata->l3_l4_filter = 0;
	pdata->l2_filtering_mode = !!pdata->hw_feat.hash_tbl_sz;
	pdata->one_nsec_accuracy = 1;

	DBGPR("<--dwc_eqos_default_common_confs\n");
}

/* This function is used to initialize all Tx
 * parameters to default values on reset.
 */
static void dwc_eqos_default_tx_confs_single_q(
		struct dwc_eqos_prv_data *pdata,
		unsigned int qinx)
{
	struct dwc_eqos_tx_queue *queue_data = &pdata->tx_queue[qinx];
	struct dwc_eqos_tx_wrapper_descriptor *desc_data =
		GET_TX_WRAPPER_DESC(qinx);

	DBGPR("-->dwc_eqos_default_tx_confs_single_q\n");

	queue_data->q_op_mode = q_op_mode[qinx];

	desc_data->tx_threshold_val = DWC_EQOS_TX_THRESHOLD_32;
	desc_data->tsf_on = DWC_EQOS_TSF_ENABLE;
	desc_data->osf_on = DWC_EQOS_OSF_ENABLE;
	desc_data->tx_pbl = DWC_EQOS_PBL_16;
	desc_data->tx_vlan_tag_via_reg = Y_FALSE;
	desc_data->tx_vlan_tag_ctrl = DWC_EQOS_TX_VLAN_TAG_INSERT;
	desc_data->vlan_tag_present = 0;
	desc_data->context_setup = 0;
	desc_data->default_mss = 0;

	DBGPR("<--dwc_eqos_default_tx_confs_single_q\n");
}

/* This function is used to initialize all Rx
 * parameters to default values on reset.
 */
static void dwc_eqos_default_rx_confs_single_q(
		struct dwc_eqos_prv_data *pdata,
		unsigned int qinx)
{
	struct dwc_eqos_rx_wrapper_descriptor *desc_data =
		GET_RX_WRAPPER_DESC(qinx);

	DBGPR("-->dwc_eqos_default_rx_confs_single_q\n");

	desc_data->rx_threshold_val = DWC_EQOS_RX_THRESHOLD_64;
	desc_data->rsf_on = DWC_EQOS_RSF_DISABLE;
	desc_data->rx_pbl = DWC_EQOS_PBL_16;
	desc_data->rx_outer_vlan_strip = DWC_EQOS_RX_VLAN_STRIP_ALWAYS;
	desc_data->rx_inner_vlan_strip = DWC_EQOS_RX_VLAN_STRIP_ALWAYS;

	DBGPR("<--dwc_eqos_default_rx_confs_single_q\n");
}

static void dwc_eqos_default_tx_confs(struct dwc_eqos_prv_data *pdata)
{
	unsigned int qinx;

	DBGPR("-->dwc_eqos_default_tx_confs\n");

	for (qinx = 0; qinx < pdata->tx_queue_cnt; qinx++)
		dwc_eqos_default_tx_confs_single_q(pdata, qinx);

	DBGPR("<--dwc_eqos_default_tx_confs\n");
}

static void dwc_eqos_default_rx_confs(struct dwc_eqos_prv_data *pdata)
{
	unsigned int qinx;

	DBGPR("-->dwc_eqos_default_rx_confs\n");

	for (qinx = 0; qinx < pdata->rx_queue_cnt; qinx++)
		dwc_eqos_default_rx_confs_single_q(pdata, qinx);

	DBGPR("<--dwc_eqos_default_rx_confs\n");
}

static void dwc_eqos_disable_clks(struct dwc_eqos_prv_data *pdata, int index)
{
	int i;

	for( i = 0 ; i < index ; i++) {
		if(pdata->eqos_clk[i] != NULL) {
			clk_disable_unprepare(pdata->eqos_clk[i]);
			pdata->eqos_clk[i] = NULL;
		}
	}
}

static int dwc_eqos_enable_clks(struct dwc_eqos_prv_data *pdata)
{
	int index;
	int ret;
	struct clk* eqos_clk = NULL;
	const char *clk_names[] = {"ptp", "aclk", "hclk", "rgmii",
		"rx", "master_bus", "slave_bus"};
	struct platform_device *pdev = pdata->pdev ;

	/*enable compulsory clocks. for optional clock continue*/
	for (index = 0; index < EQOS_MAX_CLK ; index++) {

		eqos_clk = devm_clk_get(&pdev->dev, clk_names[index]);

		if (IS_ERR(eqos_clk)) {
			//continue for opional clock
			if( index > OPTIONAL_CLK_INDEX)
				continue;
			dev_err(&pdev->dev, "%s clock not found.\n", clk_names[index]);
			ret = PTR_ERR(eqos_clk);
			goto eqos_clk_err;
		}

		ret = clk_prepare_enable(eqos_clk);
		if (ret) {
			//continue for opional clock
			if( index > OPTIONAL_CLK_INDEX)
				continue;
			dev_err(&pdev->dev, "Unable to enable %s clock.\n", clk_names[index]);
			goto eqos_clk_err;
		}

		pdata->eqos_clk[index] = eqos_clk;
	}

	return ret;

/* clock failure */
eqos_clk_err:
	dwc_eqos_disable_clks(pdata, index);
	return ret;
}

/*!
 * \details This function is invoked by open() function. This function will
 * clear MMC structure.
 *
 * \param[in] pdata – pointer to private data structure.
 *
 * \return void
 */
static void dwc_eqos_mmc_setup(struct dwc_eqos_prv_data *pdata)
{
	DBGPR("-->dwc_eth_qos_mmc_setup\n");

	if (pdata->hw_feat.mmc_sel) {
		memset(&pdata->mmc, 0, sizeof(struct dwc_eqos_mmc_counters));
	} else
		pr_info("No MMC/RMON module available in the HW\n");

	DBGPR("<--dwc_eqos_mmc_setup\n");
}

/* Opens the interface. The interface is opned whenever
 * ifconfig activates it. The open method should register any
 * system resource it needs like I/O ports, IRQ, DMA, etc,
 * turn on the hardware, and perform any other setup your device requires.
 */
static int dwc_eqos_open(struct net_device *dev)
{
	struct dwc_eqos_prv_data *pdata = netdev_priv(dev);
	int ret = Y_SUCCESS;
	struct hw_if_struct *hw_if = &pdata->hw_if;
	struct desc_if_struct *desc_if = &pdata->desc_if;

	DBGPR("-->dwc_eqos_open\n");
	//enable all the clocks
	dwc_eqos_enable_clks(pdata);

	if (dwc_eqos_setup_rxclock_skew(pdata->pdev)) {
		pr_alert("ERROR: Unable to setup rxclock skew\n");
		return -ENXIO;
	}

	/* issue software reset to device */
	if (hw_if->exit(pdata))
		return -EBUSY;

	pdata->irq_number = dev->irq;
	ret = request_irq(pdata->irq_number, dwc_eqos_isr_sw_dwc_eqos,
			IRQF_SHARED, DEV_NAME, pdata);
	if (ret != 0) {
		pr_alert("Unable to register IRQ %d\n",
			pdata->irq_number);
		ret = -EBUSY;
		goto err_irq_0;
	}
	ret = desc_if->alloc_buff_and_desc(pdata);
	if (ret < 0) {
		pr_alert("failed to allocate buffer/descriptor memory\n");
		ret = -ENOMEM;
		goto err_out_desc_buf_alloc_failed;
	}

	/* default configuration */
	dwc_eqos_default_common_confs(pdata);
	dwc_eqos_default_tx_confs(pdata);
	dwc_eqos_default_rx_confs(pdata);
	dwc_eqos_configure_rx_fun_ptr(pdata);

	dwc_eqos_napi_enable_mq(pdata);

	dwc_eqos_set_rx_mode(dev);
	desc_if->wrapper_tx_desc_init(pdata);
	desc_if->wrapper_rx_desc_init(pdata);

	dwc_eqos_tx_desc_mang_ds_dump(pdata);
	dwc_eqos_rx_desc_mang_ds_dump(pdata);

	dwc_eqos_initialize_work_queue(pdata);

	dwc_eqos_mmc_setup(pdata);

	/* initializes MAC and DMA */
	hw_if->init(pdata);

	if (pdata->phydev)
		phy_start(pdata->phydev);

	if (pdata->phydev)
		netif_tx_start_all_queues(dev);

	dwc_eqos_rxmux_setup(pdata, true);
	DBGPR("<--dwc_eqos_open\n");

	return ret;

 err_out_desc_buf_alloc_failed:
	free_irq(pdata->irq_number, pdata);
	pdata->irq_number = 0;

 err_irq_0:
	DBGPR("<--dwc_eqos_open\n");
	return ret;
}

/* This function should reverse operations performed at open time.
 */
static int dwc_eqos_close(struct net_device *dev)
{
	struct dwc_eqos_prv_data *pdata = netdev_priv(dev);
	struct hw_if_struct *hw_if = &pdata->hw_if;
	struct desc_if_struct *desc_if = &pdata->desc_if;
	int qinx;

	DBGPR("-->dwc_eqos_close\n");

	dwc_eqos_rxmux_setup(pdata, false);
	if (pdata->phydev)
		phy_stop(pdata->phydev);

	netif_tx_disable(dev);
	dwc_eqos_all_ch_napi_disable(pdata);

	flush_delayed_work(&pdata->delayed_tstamp_work);
	flush_workqueue(pdata->restart_wq);
	destroy_workqueue(pdata->restart_wq);

	for (qinx = 0; qinx < pdata->tx_queue_cnt; qinx++)
		del_timer_sync(&GET_TX_WRAPPER_DESC(qinx)->txtimer);

	if (pdata->irq_number != 0) {
		free_irq(pdata->irq_number, pdata);
		pdata->irq_number = 0;
	}

	/* stop DMA TX/RX */
	for (qinx = 0; qinx < pdata->tx_queue_cnt; qinx++)
		hw_if->stop_dma_tx(pdata, qinx);
	for (qinx = 0; qinx < pdata->rx_queue_cnt; qinx++)
		hw_if->stop_dma_rx(pdata, qinx);

	dwc_eqos_disable_clks(pdata, EQOS_MAX_CLK);

	desc_if->tx_free_mem(pdata);
	desc_if->rx_free_mem(pdata);

	DBGPR("<--dwc_eqos_close\n");

	return Y_SUCCESS;
}

/* This function collects all the multicast addresse
 * and updates the device.
 * returns val 0 if perfect filtering is seleted & 1 if hash
 * filtering is seleted.
 */
static int dwc_eqos_prepare_mc_list(struct net_device *dev)
{
	struct dwc_eqos_prv_data *pdata = netdev_priv(dev);
	int htr_cnt = pdata->max_hash_table_size / 32;
	struct hw_if_struct *hw_if = &pdata->hw_if;
	struct netdev_hw_addr *ha = NULL;
	u32 mc_filter[htr_cnt];
	int crc32_val = 0;
	int ret = 0, i = 1;

	DBGPR_FILTER("-->dwc_eqos_prepare_mc_list\n");

	if (pdata->l2_filtering_mode) {
		DBGPR_FILTER(
		"select HASH FILTERING for mc addresses: mc_count = %d\n",
		netdev_mc_count(dev));
		ret = 1;
		memset(mc_filter, 0, sizeof(mc_filter));

		switch (pdata->max_hash_table_size) {
		case 64:
			netdev_for_each_mc_addr(ha, dev) {
				DBGPR_FILTER(
					"mc addr[%d] = %#x:%#x:%#x:%#x:%#x:%#x\n", i++,
					ha->addr[0], ha->addr[1], ha->addr[2],
					ha->addr[3], ha->addr[4], ha->addr[5]);
				/* The upper 6 bits of the calculated CRC are
				 * used to index the content of the Hash Table
				 * Reg 0 and 1.
				 */
				crc32_val =
					(bitrev32(~crc32_le(~0, ha->addr, 6)) >> 26);
				/* The most significant bit determines the
				 * registeri to use (Hash Table Reg X, X = 0
				 * and 1) while theother 5(0x1F) bits determines
				 * the bit within the selected register
				 */
				mc_filter[crc32_val >> 5] |=
					(1 << (crc32_val & 0x1F));
			}
			break;
		case 128:
			netdev_for_each_mc_addr(ha, dev) {
				DBGPR_FILTER(
					"mc addr[%d] = %#x:%#x:%#x:%#x:%#x:%#x\n", i++,
					ha->addr[0], ha->addr[1], ha->addr[2],
					ha->addr[3], ha->addr[4], ha->addr[5]);
				/* The upper 7 bits of the calculated CRC are
				 * used to index the content of the Hash Table
				 * Reg 0,1,2 and 3.
				 */
				crc32_val =
					(bitrev32(~crc32_le(~0, ha->addr, 6)) >> 25);

				pr_info(
					"crc_le = %#x, crc_be = %#x\n",
					bitrev32(~crc32_le(~0, ha->addr, 6)),
					bitrev32(~crc32_be(~0, ha->addr, 6)));

				/* The most significant 2 bits determines the
				 * register to use (Hash Table Reg X, X = 0,1,
				 * 2 and 3) while the other 5(0x1F) bits
				 * determines the bit within the selected
				 * register
				 */
				mc_filter[crc32_val >> 5] |=
						(1 << (crc32_val & 0x1F));
			}
			break;
		case 256:
			netdev_for_each_mc_addr(ha, dev) {
				DBGPR_FILTER(
					"mc addr[%d] = %#x:%#x:%#x:%#x:%#x:%#x\n", i++,
					ha->addr[0], ha->addr[1], ha->addr[2],
					ha->addr[3], ha->addr[4], ha->addr[5]);
				/* The upper 8 bits of the calculated CRC are
				 * used to index the content of the Hash Table
				 * Reg 0,1,2,3,4, 5,6, and 7.
				 */
				crc32_val =
					(bitrev32(~crc32_le(~0, ha->addr, 6)) >> 24);
				/* The most significant 3 bits determines the
				 * register to use (Hash Table Reg X, X = 0,
				 * 1,2,3,4,5,6 and 7) while the other 5(0x1F)
				 * bits determines the bit within the
				 * selected register
				 */
				mc_filter[crc32_val >> 5] |=
						(1 << (crc32_val & 0x1F));
			}
			break;
		}

		for (i = 0; i < htr_cnt; i++)
			hw_if->update_hash_table_reg(pdata, i, mc_filter[i]);

	} else {
		DBGPR_FILTER("select PERFECT FILTERING for mc addresses,"
				"mc_count = %d, max_addr_reg_cnt = %d\n",
				netdev_mc_count(dev), pdata->max_addr_reg_cnt);

		netdev_for_each_mc_addr(ha, dev) {
			DBGPR_FILTER("mc addr[%d] = %#x:%#x:%#x:%#x:%#x:%#x\n", i,
					ha->addr[0], ha->addr[1], ha->addr[2],
					ha->addr[3], ha->addr[4], ha->addr[5]);
			if (i < 32)
				hw_if->update_mac_addr1_31_low_high_reg(pdata, i, ha->addr);
			else
				hw_if->update_mac_addr32_127_low_high_reg(pdata, i, ha->addr);
			i++;
		}
	}

	DBGPR_FILTER("<--dwc_eqos_prepare_mc_list\n");

	return ret;
}

/* This function collects all the unicast addresses
 * and updates the device.
 * retval 0 if perfect filtering is seleted  & 1 if hash
 * filtering is seleted.
 */
static int dwc_eqos_prepare_uc_list(struct net_device *dev)
{
	struct dwc_eqos_prv_data *pdata = netdev_priv(dev);
	int htr_cnt = pdata->max_hash_table_size / 32;
	struct hw_if_struct *hw_if = &pdata->hw_if;
	u32 uc_filter[htr_cnt];
	struct netdev_hw_addr *ha = NULL;
	int crc32_val = 0;
	int ret = 0, i = 1;

	DBGPR_FILTER("-->dwc_eqos_prepare_uc_list\n");

	if (pdata->l2_filtering_mode) {
		DBGPR_FILTER(
			"select HASH FILTERING for uc addresses: uc_count = %d\n",
			netdev_uc_count(dev));
		ret = 1;
		memset(uc_filter, 0, sizeof(uc_filter));

		if (pdata->max_hash_table_size == 64) {
			netdev_for_each_uc_addr(ha, dev) {
				DBGPR_FILTER("uc addr[%d] = %#x:%#x:%#x:%#x:%#x:%#x\n", i++,
						ha->addr[0], ha->addr[1], ha->addr[2],
						ha->addr[3], ha->addr[4], ha->addr[5]);
				crc32_val =
					(bitrev32(~crc32_le(~0, ha->addr, 6)) >> 26);
				uc_filter[crc32_val >> 5] |= (1 << (crc32_val & 0x1F));
			}
		} else if (pdata->max_hash_table_size == 128) {
			netdev_for_each_uc_addr(ha, dev) {
				DBGPR_FILTER("uc addr[%d] = %#x:%#x:%#x:%#x:%#x:%#x\n", i++,
						ha->addr[0], ha->addr[1], ha->addr[2],
						ha->addr[3], ha->addr[4], ha->addr[5]);
				crc32_val =
					(bitrev32(~crc32_le(~0, ha->addr, 6)) >> 25);
				uc_filter[crc32_val >> 5] |= (1 << (crc32_val & 0x1F));
			}
		} else if (pdata->max_hash_table_size == 256) {
			netdev_for_each_uc_addr(ha, dev) {
				DBGPR_FILTER("uc addr[%d] = %#x:%#x:%#x:%#x:%#x:%#x\n", i++,
						ha->addr[0], ha->addr[1], ha->addr[2],
						ha->addr[3], ha->addr[4], ha->addr[5]);
				crc32_val =
					(bitrev32(~crc32_le(~0, ha->addr, 6)) >> 24);
				uc_filter[crc32_val >> 5] |= (1 << (crc32_val & 0x1F));
			}
		}

		/* configure hash value of real/default interface also */
		DBGPR_FILTER("real/default dev_addr = %#x:%#x:%#x:%#x:%#x:%#x\n",
				dev->dev_addr[0], dev->dev_addr[1], dev->dev_addr[2],
				dev->dev_addr[3], dev->dev_addr[4], dev->dev_addr[5]);

		if (pdata->max_hash_table_size == 64) {
			crc32_val =
				(bitrev32(~crc32_le(~0, dev->dev_addr, 6)) >> 26);
			uc_filter[crc32_val >> 5] |= (1 << (crc32_val & 0x1F));
		} else if (pdata->max_hash_table_size == 128) {
			crc32_val =
				(bitrev32(~crc32_le(~0, dev->dev_addr, 6)) >> 25);
			uc_filter[crc32_val >> 5] |= (1 << (crc32_val & 0x1F));

		} else if (pdata->max_hash_table_size == 256) {
			crc32_val =
				(bitrev32(~crc32_le(~0, dev->dev_addr, 6)) >> 24);
			uc_filter[crc32_val >> 5] |= (1 << (crc32_val & 0x1F));
		}

		for (i = 0; i < htr_cnt; i++)
			hw_if->update_hash_table_reg(pdata, i, uc_filter[i]);

	} else {
		DBGPR_FILTER("select PERFECT FILTERING for uc addresses: uc_count = %d\n",
				netdev_uc_count(dev));

		netdev_for_each_uc_addr(ha, dev) {
			DBGPR_FILTER("uc addr[%d] = %#x:%#x:%#x:%#x:%#x:%#x\n", i,
					ha->addr[0], ha->addr[1], ha->addr[2],
					ha->addr[3], ha->addr[4], ha->addr[5]);
			if (i < 32)
				hw_if->update_mac_addr1_31_low_high_reg(pdata, i, ha->addr);
			else
				hw_if->update_mac_addr32_127_low_high_reg(pdata, i, ha->addr);
			i++;
		}
	}

	DBGPR_FILTER("<--dwc_eqos_prepare_uc_list\n");

	return ret;
}

/* The set_multicast_list function is called when the multicast list
 * for the device changes and when the flags change.
 */
static void dwc_eqos_set_rx_mode(struct net_device *dev)
{
	struct dwc_eqos_prv_data *pdata = netdev_priv(dev);
	int htr_cnt = pdata->max_hash_table_size / 32;
	struct hw_if_struct *hw_if = &pdata->hw_if;
	unsigned long flags;
	unsigned char pr_mode = 0;
	unsigned char huc_mode = 0;
	unsigned char hmc_mode = 0;
	unsigned char pm_mode = 0;
	unsigned char hpf_mode = 0;
	int mode, i;

	DBGPR_FILTER("-->dwc_eqos_set_rx_mode\n");

	spin_lock_irqsave(&pdata->lock, flags);

	if (dev->flags & IFF_PROMISC) {
		DBGPR_FILTER("PROMISCUOUS MODE (Accept all packets irrespective of DA)\n");
		pr_mode = 1;
	} else if ((dev->flags & IFF_ALLMULTI) ||
			(netdev_mc_count(dev) > (pdata->max_hash_table_size))) {
		DBGPR_FILTER("pass all multicast pkt\n");
		pm_mode = 1;
		if (pdata->max_hash_table_size) {
			for (i = 0; i < htr_cnt; i++)
				hw_if->update_hash_table_reg(
						pdata, i, 0xffffffff);
		}
	} else if (!netdev_mc_empty(dev)) {
		DBGPR_FILTER("pass list of multicast pkt\n");
		if ((netdev_mc_count(dev) > (pdata->max_addr_reg_cnt - 1)) &&
			(!pdata->max_hash_table_size)) {
			/* switch to PROMISCUOUS mode */
			pr_mode = 1;
		} else {
			mode = dwc_eqos_prepare_mc_list(dev);
			if (mode) {
				/* Hash filtering for multicast */
				hmc_mode = 1;
			} else {
				/* Perfect filtering for multicast */
				hmc_mode = 0;
				hpf_mode = 1;
			}
		}
	}

	/* Handle multiple unicast addresses */
	if ((netdev_uc_count(dev) > (pdata->max_addr_reg_cnt - 1)) &&
		(!pdata->max_hash_table_size)) {
		/* switch to PROMISCUOUS mode */
		pr_mode = 1;
	} else if (!netdev_uc_empty(dev)) {
		mode = dwc_eqos_prepare_uc_list(dev);
		if (mode) {
			/* Hash filtering for unicast */
			huc_mode = 1;
		} else {
			/* Perfect filtering for unicast */
			huc_mode = 0;
			hpf_mode = 1;
		}
	}

	hw_if->config_mac_pkt_filter_reg(pdata, pr_mode, huc_mode,
		hmc_mode, pm_mode, hpf_mode);

	spin_unlock_irqrestore(&pdata->lock, flags);

	DBGPR("<--dwc_eqos_set_rx_mode\n");
}

/* This function is invoked by start_xmit function. This function
 * calculates number of transmit descriptor required for a given transfer.
 * retval number of descriptor required.
 */
unsigned int dwc_eqos_get_total_desc_cnt(
		struct dwc_eqos_prv_data *pdata,
		struct sk_buff *skb, unsigned int qinx)
{
	unsigned int count = 0, size = 0;
	int length = 0;
	struct hw_if_struct *hw_if = &pdata->hw_if;
	struct s_tx_pkt_features *tx_pkt_features = &pdata->tx_pkt_features;
	struct dwc_eqos_tx_wrapper_descriptor *desc_data =
	    GET_TX_WRAPPER_DESC(qinx);

	/* SG fragment count */
	count += skb_shinfo(skb)->nr_frags;

	/* descriptors required based on data limit per descriptor */
	length = (skb->len - skb->data_len);
	while (length) {
		size = min(length, DWC_EQOS_MAX_DATA_PER_TXD);
		count++;
		length = length - size;
	}

	/* we need one context descriptor to carry tso details */
	if (skb_shinfo(skb)->gso_size != 0)
		count++;
#if (IS_ENABLED(CONFIG_VLAN_8021Q))
		desc_data->vlan_tag_present = 0;
		if (skb_vlan_tag_present(skb)) {
			unsigned short vlan_tag = skb_vlan_tag_get(skb);
			vlan_tag |= (qinx << 13);
			desc_data->vlan_tag_present = 1;
			if (
				vlan_tag != desc_data->vlan_tag_id ||
				desc_data->context_setup == 1) {
				desc_data->vlan_tag_id = vlan_tag;
				if (Y_TRUE == desc_data->tx_vlan_tag_via_reg) {
					pr_info("VLAN control info update via register\n\n");
					hw_if->enable_vlan_reg_control(
							pdata, desc_data);
				} else {
					hw_if->enable_vlan_desc_control(pdata);
					TX_PKT_FEATURES_PKT_ATTRIBUTES_VLAN_PKT_MLF_WR
						(tx_pkt_features->pkt_attributes, 1);
					TX_PKT_FEATURES_VLAN_TAG_VT_MLF_WR
						(tx_pkt_features->vlan_tag, vlan_tag);
					/* we need one context descriptor to carry vlan tag info */
					count++;
				}
			}
			pdata->xstats.tx_vlan_pkt_n++;
		}
#endif
#ifdef DWC_ETH_QOS_ENABLE_DVLAN
	if (pdata->via_reg_or_desc == DWC_EQOS_VIA_DESC) {
		/* we need one context descriptor to carry vlan tag info */
		count++;
	}
#endif /* End of DWC_ETH_QOS_ENABLE_DVLAN */

	return count;
}

/* The start_xmit function initiates the transmission of a packet.
 * The full packet (protocol headers and all) is contained in a socket buffer
 * (sk_buff) structure.
 */
static int dwc_eqos_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct dwc_eqos_prv_data *pdata = netdev_priv(dev);
	unsigned int qinx = skb_get_queue_mapping(skb);
	struct dwc_eqos_tx_wrapper_descriptor *desc_data =
	    GET_TX_WRAPPER_DESC(qinx);
	struct s_tx_pkt_features *tx_pkt_features = &pdata->tx_pkt_features;
	unsigned long flags;
	unsigned int desc_count = 0;
	unsigned int count = 0;
	struct hw_if_struct *hw_if = &pdata->hw_if;
	struct desc_if_struct *desc_if = &pdata->desc_if;
	struct netdev_queue *txq = netdev_get_tx_queue(dev, 0);
	int retval = NETDEV_TX_OK;
	unsigned int varvlan_pkt;
	int tso;

	DBGPR("-->dwc_eqos_start_xmit: skb->len = %d, qinx = %u\n",
		skb->len, qinx);
	spin_lock_irqsave(&pdata->tx_lock, flags);

	if (skb->len <= 0) {
		dev_kfree_skb_any(skb);
		printk(KERN_ERR "%s : Empty skb received from stack\n",
			dev->name);
		goto tx_netdev_return;
	}

	if ((skb_shinfo(skb)->gso_size == 0) &&	(skb->len > DWC_EQOS_MAX_SUPPORTED_MTU)) {
		printk(KERN_ERR "%s : big packet = %d\n", dev->name,
			(u16)skb->len);
		dev_kfree_skb_any(skb);
		dev->stats.tx_dropped++;
		goto tx_netdev_return;
	}

	memset(&pdata->tx_pkt_features, 0, sizeof(pdata->tx_pkt_features));

	/* check total number of desc required for current xfer */
	desc_count = dwc_eqos_get_total_desc_cnt(pdata, skb, qinx);
	if (desc_data->free_desc_cnt < desc_count) {
		desc_data->queue_stopped = 1;
		netif_stop_subqueue(dev, qinx);
		DBGPR("stopped TX queue(%d) since there are no sufficient "
			"descriptor available for the current transfer\n",
			qinx);
		retval = NETDEV_TX_BUSY;
		goto tx_netdev_return;
	}

	desc_data->cur_desc_count = desc_count;

	/* check for hw tstamping */
	if (pdata->hw_feat.tsstssel && pdata->hwts_tx_en) {
		if (skb_shinfo(skb)->tx_flags & SKBTX_HW_TSTAMP) {
			/* declare that device is doing timestamping */
			skb_shinfo(skb)->tx_flags |= SKBTX_IN_PROGRESS;
			TX_PKT_FEATURES_PKT_ATTRIBUTES_PTP_ENABLE_MLF_WR(
					tx_pkt_features->pkt_attributes, 1);
			DBGPR_PTP(
			"Got PTP pkt to transmit [qinx = %d, cur_tx = %d]\n",
			qinx, desc_data->cur_tx);
		}
	}

	tso = desc_if->handle_tso(dev, skb);
	if (tso < 0) {
		pr_alert("Unable to handle TSO\n");
		dev_kfree_skb_any(skb);
		retval = NETDEV_TX_OK;
		goto tx_netdev_return;
	}
	if (tso) {
		pdata->xstats.tx_tso_pkt_n++;
		TX_PKT_FEATURES_PKT_ATTRIBUTES_TSO_ENABLE_MLF_WR(tx_pkt_features->pkt_attributes, 1);
	} else if (skb->ip_summed == CHECKSUM_PARTIAL) {
		TX_PKT_FEATURES_PKT_ATTRIBUTES_CSUM_ENABLE_MLF_WR(tx_pkt_features->pkt_attributes, 1);
	}

	count = desc_if->map_tx_skb(dev, skb);
	if (count == 0) {
		dev_kfree_skb_any(skb);
		retval = NETDEV_TX_OK;
		goto tx_netdev_return;
	}

	desc_data->packet_count = count;

	if (tso && (desc_data->default_mss != tx_pkt_features->mss))
		count++;

	txq->trans_start = jiffies;

	if (IS_ENABLED(CONFIG_VLAN_8021Q)) {
		TX_PKT_FEATURES_PKT_ATTRIBUTES_VLAN_PKT_MLF_RD
			(tx_pkt_features->pkt_attributes, varvlan_pkt);
		if (varvlan_pkt == 0x1)
			count++;
	}
#ifdef DWC_ETH_QOS_ENABLE_DVLAN
	if (pdata->via_reg_or_desc == DWC_EQOS_VIA_DESC)
		count++;
#endif /* End of DWC_ETH_QOS_ENABLE_DVLAN */

	desc_data->free_desc_cnt -= count;
	desc_data->tx_pkt_queued += count;

#ifdef DWC_EQOS_ENABLE_TX_PKT_DUMP
	print_pkt(skb, skb->len, 1, (desc_data->cur_tx - 1));
#endif

	/* fallback to software time stamping if core doesn't
	 * support hardware time stamping */
	if ((pdata->hw_feat.tsstssel == 0) || (pdata->hwts_tx_en == 0))
		skb_tx_timestamp(skb);

	/* configure required descriptor fields for transmission */
	hw_if->pre_xmit(pdata, qinx);

tx_netdev_return:
	spin_unlock_irqrestore(&pdata->tx_lock, flags);

	DBGPR("<--dwc_eqos_start_xmit\n");

	return retval;
}

static void dwc_eqos_print_rx_tstamp_info(struct s_rx_normal_desc *rxdesc, unsigned int qinx)
{
	u32 ptp_status = 0;
	u32 pkt_type = 0;
	char *tstamp_dropped = NULL;
	char *tstamp_available = NULL;
	char *ptp_version = NULL;
	char *ptp_pkt_type = NULL;
	char *ptp_msg_type = NULL;

	DBGPR_PTP("-->dwc_eqos_print_rx_tstamp_info\n");

	/* status in RDES1 is not valid */
	if (!(rxdesc->rdes3 & DWC_EQOS_RDESC3_RS1V))
		return;

	ptp_status = rxdesc->rdes1;
	tstamp_dropped = ((ptp_status & 0x8000) ? "YES" : "NO");
	tstamp_available = ((ptp_status & 0x4000) ? "YES" : "NO");
	ptp_version = ((ptp_status & 0x2000) ? "v2 (1588-2008)" : "v1 (1588-2002)");
	ptp_pkt_type = ((ptp_status & 0x1000) ? "ptp over Eth" : "ptp over IPv4/6");

	pkt_type = ((ptp_status & 0xF00) >> 8);
	switch (pkt_type) {
	case 0:
		ptp_msg_type = "NO PTP msg received";
		break;
	case 1:
		ptp_msg_type = "SYNC";
		break;
	case 2:
		ptp_msg_type = "Follow_Up";
		break;
	case 3:
		ptp_msg_type = "Delay_Req";
		break;
	case 4:
		ptp_msg_type = "Delay_Resp";
		break;
	case 5:
		ptp_msg_type = "Pdelay_Req";
		break;
	case 6:
		ptp_msg_type = "Pdelay_Resp";
		break;
	case 7:
		ptp_msg_type = "Pdelay_Resp_Follow_up";
		break;
	case 8:
		ptp_msg_type = "Announce";
		break;
	case 9:
		ptp_msg_type = "Management";
		break;
	case 10:
		ptp_msg_type = "Signaling";
		break;
	case 11:
	case 12:
	case 13:
	case 14:
		ptp_msg_type = "Reserved";
		break;
	case 15:
		ptp_msg_type = "PTP pkr with Reserved Msg Type";
		break;
	}

	DBGPR_PTP("Rx timestamp detail for queue %d\n"
			"tstamp dropped    = %s\n"
			"tstamp available  = %s\n"
			"PTP version       = %s\n"
			"PTP Pkt Type      = %s\n"
			"PTP Msg Type      = %s\n",
			qinx, tstamp_dropped, tstamp_available,
			ptp_version, ptp_pkt_type, ptp_msg_type);

	DBGPR_PTP("<--dwc_eqos_print_rx_tstamp_info\n");
}

/* This function will read received packet's timestamp from
 * the descriptor and pass it to stack and also perform some sanity checks.
 * retval 0 if no context descriptor
 * retval 1 if timestamp is valid
 * retval 2 if time stamp is corrupted
 */
static unsigned char dwc_eqos_get_rx_hwtstamp(
	struct dwc_eqos_prv_data *pdata,
	struct sk_buff *skb,
	struct dwc_eqos_rx_wrapper_descriptor *desc_data,
	unsigned int qinx)
{
	struct s_rx_normal_desc *rx_normal_desc =
		GET_RX_DESC_PTR(qinx, desc_data->cur_rx);
	struct s_rx_context_desc *rx_context_desc = NULL;
	struct hw_if_struct *hw_if = &pdata->hw_if;
	struct skb_shared_hwtstamps *shhwtstamp = NULL;
	u64 ns;
	int retry, ret;

	DBGPR_PTP("-->dwc_eqos_get_rx_hwtstamp\n");

	dwc_eqos_print_rx_tstamp_info(rx_normal_desc, qinx);

	desc_data->dirty_rx++;
	INCR_RX_DESC_INDEX(desc_data->cur_rx, 1);
	rx_context_desc = GET_RX_DESC_PTR(qinx, desc_data->cur_rx);

	DBGPR_PTP("\nRX_CONTEX_DESC[%d %4p %d RECEIVED FROM DEVICE]"\
			" = %#x:%#x:%#x:%#x",
			qinx, rx_context_desc, desc_data->cur_rx, rx_context_desc->rdes0,
			rx_context_desc->rdes1,
			rx_context_desc->rdes2, rx_context_desc->rdes3);

	/* check rx tsatmp */
	for (retry = 0; retry < 10; retry++) {
		ret = hw_if->get_rx_tstamp_status(rx_context_desc);
		if (ret == 1) {
			/* time stamp is valid */
			break;
		} else if (ret == 0) {
			pr_alert("Device has not yet updated the context "
				"desc to hold Rx time stamp(retry = %d)\n", retry);
		} else {
			pr_alert("Error: Rx time stamp is corrupted(retry = %d)\n", retry);
			return 2;
		}
	}

	if (retry == 10) {
			pr_alert("Device has not yet updated the context "
				"desc to hold Rx time stamp(retry = %d)\n", retry);
			desc_data->dirty_rx--;
			DECR_RX_DESC_INDEX(desc_data->cur_rx);
			return 0;
	}

	pdata->xstats.rx_timestamp_captured_n++;
	/* get valid tstamp */
	ns = hw_if->get_rx_tstamp(rx_context_desc);

	shhwtstamp = skb_hwtstamps(skb);
	memset(shhwtstamp, 0, sizeof(struct skb_shared_hwtstamps));
	shhwtstamp->hwtstamp = ns_to_ktime(ns);

	DBGPR_PTP("<--dwc_eqos_get_rx_hwtstamp\n");

	return 1;
}

static void dwc_eqos_get_tx_reg_hwtstamp_work(struct work_struct *tstamp_wq)
{
	struct dwc_eqos_prv_data *pdata = container_of(tstamp_wq,
				struct dwc_eqos_prv_data, delayed_tstamp_work.work);
	struct skb_shared_hwtstamps shhwtstamp;
	struct hw_if_struct *hw_if;
	struct sk_buff *skb;
	unsigned long flags;
	u64 ns;

	DBGPR_PTP("-->dwc_eqos_get_tx_reg_hwtstamp_work\n");

	spin_lock_irqsave(&pdata->tx_lock, flags);
	skb = pdata->tstamp_skb;
	/* no skb to pass tstamp to */
	if (!skb) {
		spin_unlock_irqrestore(&pdata->tx_lock, flags);
		return;
	}

	pdata->tstamp_skb = NULL;
	hw_if = &pdata->hw_if;
	/* check tx tstamp status */
	if (!hw_if->get_tx_tstamp_status_via_reg(pdata)) {
		pr_alert("tx timestamp is not captured for this packet\n");
		/* clear the HW latched error status by doing spurious read */
		hw_if->get_tx_tstamp_via_reg(pdata);
		dev_kfree_skb_any(skb);
		spin_unlock_irqrestore(&pdata->tx_lock, flags);
		return;
	}

	/* get the valid tstamp */
	ns = hw_if->get_tx_tstamp_via_reg(pdata);

	pdata->xstats.tx_timestamp_captured_n++;
	memset(&shhwtstamp, 0, sizeof(struct skb_shared_hwtstamps));
	shhwtstamp.hwtstamp = ns_to_ktime(ns);
	/* pass TStamp to stack */
	skb_tstamp_tx(skb, &shhwtstamp);
	dev_kfree_skb_any(skb);
	spin_unlock_irqrestore(&pdata->tx_lock, flags);

	DBGPR_PTP("<--dwc_eqos_get_tx_reg_hwtstamp_work\n");
}

/* This function will read timestamp from the descriptor
 * and pass it to stack and also perform some sanity checks.
 * retval 1 if time stamp is taken
 * retval 0 if time stamp in not taken/valid
 */
static unsigned int dwc_eqos_get_tx_hwtstamp(
	struct dwc_eqos_prv_data *pdata,
	struct s_tx_normal_desc *txdesc,
	struct sk_buff *skb)
{
	struct hw_if_struct *hw_if = &pdata->hw_if;
	struct skb_shared_hwtstamps shhwtstamp;
	u64 ns;

	DBGPR_PTP("-->dwc_eqos_get_tx_hwtstamp\n");

	if (hw_if->drop_tx_status_enabled(pdata) == 0) {
		/* check tx tstamp status */
		if (!hw_if->get_tx_tstamp_status(txdesc)) {
			pr_alert("tx timestamp is not captured for this packet\n");
			return 0;
		}

		/* get the valid tstamp */
		ns = hw_if->get_tx_tstamp(txdesc);
	} else {
		/* drop tx status mode is enabled, hence read time
		 * stamp from register instead of descriptor. very often the transmit
		 * complete for the descriptor is seen before the timestamp register
		 * is updated with latest timestamp. So delay the read of the timestamp
		 * reigster by a millisecond.
		 */
		if (!pdata->tstamp_skb) {
			pdata->tstamp_skb = skb_get(skb);
			queue_delayed_work(system_unbound_rtpri_wq, &pdata->delayed_tstamp_work,
							   msecs_to_jiffies(1));
		} else {
			dev_err_once(&pdata->pdev->dev,
						 "Previous timestamp read still queued, dropping new read");
		}
		return 0;
	}

	pdata->xstats.tx_timestamp_captured_n++;
	memset(&shhwtstamp, 0, sizeof(struct skb_shared_hwtstamps));
	shhwtstamp.hwtstamp = ns_to_ktime(ns);
	/* pass tstamp to stack */
	skb_tstamp_tx(skb, &shhwtstamp);

	DBGPR_PTP("<--dwc_eqos_get_tx_hwtstamp\n");

	return 1;
}

/* This function is called in isr handler once after getting
 * transmit complete interrupt to update the transmited packet status
 * and it does some house keeping work like updating the
 * private data structure variables.
 */
static int dwc_eqos_tx_clean(struct net_device *dev,
							 struct dwc_eqos_prv_data *pdata,
							 unsigned int qinx, unsigned int budget)
{
	struct dwc_eqos_tx_wrapper_descriptor *desc_data =
	    GET_TX_WRAPPER_DESC(qinx);
	struct s_tx_normal_desc *txptr = NULL;
	struct dwc_eqos_tx_buffer *buffer = NULL;
	struct hw_if_struct *hw_if = &pdata->hw_if;
	struct desc_if_struct *desc_if = &pdata->desc_if;
	unsigned int tstamp_taken = 0;
	unsigned long flags;
	int mmc_value = 0;
	int tx_pkt_queued = 0, tx_pkt_processed = 0;
	int err_incremented;

	spin_lock_irqsave(&pdata->tx_lock, flags);
	tx_pkt_queued = desc_data->tx_pkt_queued;
	spin_unlock_irqrestore(&pdata->tx_lock, flags);

	if (tx_pkt_queued > budget)
		tx_pkt_queued = budget;

	pdata->xstats.tx_clean_n[qinx]++;
	while (tx_pkt_queued > 0) {
		txptr = GET_TX_DESC_PTR(qinx, desc_data->dirty_tx);
		buffer = GET_TX_BUF_PTR(qinx, desc_data->dirty_tx);
		tstamp_taken = 0;

		if (!hw_if->tx_complete(txptr))
			break;

#ifdef DWC_EQOS_ENABLE_TX_DESC_DUMP
		dump_tx_desc(pdata, desc_data->dirty_tx, desc_data->dirty_tx,
			     0, qinx);
#endif

		/* update the tx error if any by looking at last segment
		 * for NORMAL descriptors
		 */
		if ((hw_if->get_tx_desc_ls(txptr)) && !(hw_if->get_tx_desc_ctxt(txptr))) {
			if (buffer->skb) {
				/* check whether skb support hw tstamp */
				if ((pdata->hw_feat.tsstssel) && (skb_shinfo(buffer->skb)->tx_flags & SKBTX_IN_PROGRESS)) {
					tstamp_taken = dwc_eqos_get_tx_hwtstamp(
							pdata,
							txptr, buffer->skb);
					if (tstamp_taken) {
						//dump_tx_desc(pdata, desc_data->dirty_tx, desc_data->dirty_tx,
						//		0, qinx);
						DBGPR_PTP("passed tx timestamp to stack[qinx = %d, dirty_tx = %d]\n",
							qinx, desc_data->dirty_tx);
					}
				}
			}

			if(pdata->hw_feat.mmc_sel) {
				err_incremented = 0;
				mmc_value = dwceqos_readl(pdata, DWCEQOS_TX_CARRIER_ERR_PKTS);
				if (mmc_value) {
					pdata->mmc.mmc_tx_carrier_error += mmc_value;
					err_incremented = 1;
					dev->stats.tx_carrier_errors++;
				}

				mmc_value = dwceqos_readl(pdata, DWCEQOS_TX_UFLOW_ERR_PKTS);
				if (mmc_value) {
					pdata->mmc.mmc_tx_underflow_error += mmc_value;
					err_incremented = 1;
					dev->stats.tx_fifo_errors++;
				}

				if (err_incremented == 1)
					dev->stats.tx_errors++;
			}

			pdata->xstats.q_tx_pkt_n[qinx]++;
			pdata->xstats.tx_pkt_n++;
			dev->stats.tx_packets++;
		}

		dev->stats.tx_bytes += buffer->len;
		dev->stats.tx_bytes += buffer->len2;
		desc_if->unmap_tx_skb(pdata, buffer);

		/* reset the descriptor so that driver/host can reuse it */
		hw_if->tx_desc_reset(desc_data->dirty_tx, pdata, qinx);

		INCR_TX_DESC_INDEX(desc_data->dirty_tx, 1);
		tx_pkt_queued--;
		tx_pkt_processed++;
	}

	spin_lock_irqsave(&pdata->tx_lock, flags);
	desc_data->free_desc_cnt += tx_pkt_processed;
	desc_data->tx_pkt_queued -= tx_pkt_processed;

	if (desc_data->tx_pkt_queued)
		mod_timer(&desc_data->txtimer, DWC_COAL_TIMER(pdata->tx_coal_timer_us));

	if ((desc_data->queue_stopped == 1) && (desc_data->free_desc_cnt > 0)) {
		desc_data->queue_stopped = 0;
		netif_wake_subqueue(dev, qinx);
	}
	spin_unlock_irqrestore(&pdata->tx_lock, flags);

	return tx_pkt_processed;
}

#ifdef YDEBUG_FILTER
static void dwc_eqos_check_rx_filter_status(struct s_rx_normal_desc *rx_normal_desc)
{
	u32 rdes2 = rx_normal_desc->rdes2;
	u32 rdes3 = rx_normal_desc->rdes3;

	/* Receive Status RDES2 Valid ? */
	if ((rdes3 & 0x8000000) == 0x8000000) {
		if ((rdes2 & 0x400) == 0x400)
			pr_alert("ARP pkt received\n");
		if ((rdes2 & 0x800) == 0x800)
			pr_alert("ARP reply not generated\n");
		if ((rdes2 & 0x8000) == 0x8000)
			pr_alert("VLAN pkt passed VLAN filter\n");
		if ((rdes2 & 0x10000) == 0x10000)
			pr_alert("SA Address filter fail\n");
		if ((rdes2 & 0x20000) == 0x20000)
			pr_alert("DA Addess filter fail\n");
		if ((rdes2 & 0x40000) == 0x40000)
			pr_alert(
				"pkt passed the HASH filter in MAC and HASH value = %#x\n",
				(rdes2 >> 19) & 0xff);
		if ((rdes2 & 0x8000000) == 0x8000000)
			pr_alert("L3 filter(%d) Match\n", ((rdes2 >> 29) & 0x7));
		if ((rdes2 & 0x10000000) == 0x10000000)
			pr_alert("L4 filter(%d) Match\n", ((rdes2 >> 29) & 0x7));
	}
}
#endif /* YDEBUG_FILTER */

/* pass skb to upper layer */
static void dwc_eqos_receive_skb(struct dwc_eqos_prv_data *pdata,
				    struct net_device *dev, struct sk_buff *skb,
				    unsigned int qinx)
{
	struct dwc_eqos_rx_queue *rx_queue = &pdata->rx_queue[qinx];

	skb_record_rx_queue(skb, qinx);
	skb->dev = dev;
	skb->protocol = eth_type_trans(skb, dev);

	if (dev->features & NETIF_F_GRO)
		napi_gro_receive(&rx_queue->napi, skb);
	else
		netif_receive_skb(skb);
}

static void dwc_eqos_consume_page(struct dwc_eqos_rx_buffer *buffer,
				     struct sk_buff *skb,
				     u16 length, u16 buf2_used)
{
	buffer->page = NULL;
	if (buf2_used)
		buffer->page2 = NULL;
	skb->len += length;
	skb->data_len += length;
	skb->truesize += length;
}

static void dwc_eqos_consume_page_split_hdr(
				struct dwc_eqos_rx_buffer *buffer,
				struct sk_buff *skb,
				u16 length,
				unsigned short page2_used)
{
	if (page2_used)
		buffer->page2 = NULL;

	skb->len += length;
	skb->data_len += length;
	skb->truesize += length;
}

/* Receive Checksum Offload configuration */
static inline void dwc_eqos_config_rx_csum(
		struct dwc_eqos_prv_data *pdata,
		struct sk_buff *skb,
		struct s_rx_normal_desc *rx_normal_desc)
{
	unsigned int varrdes1;

	skb->ip_summed = CHECKSUM_NONE;

	if ((pdata->dev_state & NETIF_F_RXCSUM) == NETIF_F_RXCSUM) {
		/* Receive Status RDES1 Valid ? */
		if ((rx_normal_desc->rdes3 & DWC_EQOS_RDESC3_RS1V)) {
			/* check(RDES1.IPCE bit) whether device has done csum correctly or not */
			varrdes1 = rx_normal_desc->rdes1;
			if ((varrdes1 & 0xC8) == 0x0)
				skb->ip_summed = CHECKSUM_UNNECESSARY;	/* csum done by device */
		}
	}
}

static inline void dwc_eqos_get_rx_vlan(
			struct dwc_eqos_prv_data *pdata,
			struct sk_buff *skb,
			struct s_rx_normal_desc *rx_normal_desc)
{
	unsigned short vlan_tag = 0;

	if ((pdata->dev_state & NETIF_F_HW_VLAN_CTAG_RX) == NETIF_F_HW_VLAN_CTAG_RX) {
		/* Receive Status RDES0 Valid ? */
		if ((rx_normal_desc->rdes3 & DWC_EQOS_RDESC3_RS0V)) {
			/* device received frame with VLAN Tag or
			 * double VLAN Tag ? */
			if (((rx_normal_desc->rdes3 & DWC_EQOS_RDESC3_LT) == 0x40000) || ((rx_normal_desc->rdes3 & DWC_EQOS_RDESC3_LT) == 0x50000)) {
				vlan_tag = rx_normal_desc->rdes0 & 0xffff;
				/* insert VLAN tag into skb */
#if IS_ENABLED(CONFIG_VLAN_8021Q)
				__vlan_hwaccel_put_tag(skb, htons(ETH_P_8021Q), vlan_tag);
#endif
				pdata->xstats.rx_vlan_pkt_n++;
			}
		}
	}
}

/* This function is invoked by main NAPI function if RX
 * split header feature is enabled. This function checks the device
 * descriptor for the packets and passes it to stack if any packets
 * are received by device.
 * retval number of packets received.
 */
static int dwc_eqos_clean_split_hdr_rx_irq(
			struct dwc_eqos_prv_data *pdata,
			int quota,
			unsigned int qinx)
{
	struct dwc_eqos_rx_wrapper_descriptor *desc_data =
	    GET_RX_WRAPPER_DESC(qinx);
	struct net_device *dev = pdata->ndev;
	struct desc_if_struct *desc_if = &pdata->desc_if;
	struct hw_if_struct *hw_if = &pdata->hw_if;
	struct sk_buff *skb = NULL;
	int received = 0;
	struct dwc_eqos_rx_buffer *buffer = NULL;
	struct s_rx_normal_desc *rx_normal_desc = NULL;
	u16 pkt_len;
	unsigned short hdr_len = 0;
	unsigned short payload_len = 0;
	unsigned char intermediate_desc_cnt = 0;
	unsigned char buf2_used = 0;
	int ret;

	DBGPR("-->dwc_eqos_clean_split_hdr_rx_irq: qinx = %u, quota = %d\n",
		qinx, quota);

	while (received < quota) {
		buffer = GET_RX_BUF_PTR(qinx, desc_data->cur_rx);
		rx_normal_desc = GET_RX_DESC_PTR(qinx, desc_data->cur_rx);

		/* check for data availability */
		if (!(rx_normal_desc->rdes3 & DWC_EQOS_RDESC3_OWN)) {
#ifdef DWC_EQOS_ENABLE_RX_DESC_DUMP
			dump_rx_desc(qinx, rx_normal_desc, desc_data->cur_rx);
#endif
			/* assign it to new skb */
			skb = buffer->skb;
			buffer->skb = NULL;

			/* first buffer pointer */
			dma_unmap_single(pdata->ndev->dev.parent, buffer->dma,
					(2 * buffer->rx_hdr_size), DMA_FROM_DEVICE);
			buffer->dma = 0;

			/* second buffer pointer */
			dma_unmap_page(pdata->ndev->dev.parent, buffer->dma2,
				       PAGE_SIZE, DMA_FROM_DEVICE);
			buffer->dma2 = 0;

			/* get the packet length */
			pkt_len =
			    (rx_normal_desc->rdes3 & DWC_EQOS_RDESC3_PL);

			/* FIRST desc and Receive Status RDES2 Valid ? */
			if ((rx_normal_desc->rdes3 & DWC_EQOS_RDESC3_FD) && (rx_normal_desc->rdes3 & DWC_EQOS_RDESC3_RS2V)) {
				/* get header length */
				hdr_len = (rx_normal_desc->rdes2 & DWC_EQOS_RDESC2_HL);
				DBGPR("Device has %s HEADER SPLIT: hdr_len = %d\n",
					(hdr_len ? "done" : "not done"), hdr_len);
				if (hdr_len)
					pdata->xstats.rx_split_hdr_pkt_n++;
			}

			/* check for bad packet,
			 * error is valid only for last descriptor(OWN + LD bit set).
			 */
			if ((rx_normal_desc->rdes3 & DWC_EQOS_RDESC3_ES) &&
			    (rx_normal_desc->rdes3 & DWC_EQOS_RDESC3_LD)) {
				DBGPR("Error in rcved pkt, failed to pass it to upper layer\n");
				dump_rx_desc(qinx, rx_normal_desc, desc_data->cur_rx);
				dev->stats.rx_errors++;
				dwc_eqos_update_rx_errors(dev, rx_normal_desc->rdes3);

				/* recycle both page/buff and skb */
				buffer->skb = skb;
				if (desc_data->skb_top)
					dev_kfree_skb_any(desc_data->skb_top);

				desc_data->skb_top = NULL;
				goto next_desc;
			}

			if (!(rx_normal_desc->rdes3 & DWC_EQOS_RDESC3_LD)) {
				intermediate_desc_cnt++;
				buf2_used = 1;
				/* this descriptor is only the beginning/middle */
				if (rx_normal_desc->rdes3 & DWC_EQOS_RDESC3_FD) {
					/* this is the beginning of a chain */

					/* here skb/skb_top may contain
					 * if (device done split header)
					 *	only header
					 * else
					 *	header/(header + payload)
					 */
					desc_data->skb_top = skb;
					/* page2 always contain only payload */
					if (hdr_len) {
						/* add header len to first skb->len */
						skb_put(skb, hdr_len);
						payload_len = pdata->rx_buffer_len;
						skb_fill_page_desc(skb, 0,
							buffer->page2, 0,
							payload_len);
					} else {
						/* add header len to first skb->len */
						skb_put(skb, buffer->rx_hdr_size);
						/* No split header, hence
						 * pkt_len = (payload + hdr_len)
						 */
						payload_len = (pkt_len - buffer->rx_hdr_size);
						skb_fill_page_desc(skb, 0,
							buffer->page2, 0,
							payload_len);
					}
				} else {
					/* this is the middle of a chain */
					payload_len = pdata->rx_buffer_len;
					skb_fill_page_desc(desc_data->skb_top,
						skb_shinfo(desc_data->skb_top)->nr_frags,
						buffer->page2, 0,
						payload_len);

					/* re-use this skb, as consumed only the page */
					buffer->skb = skb;
				}
				dwc_eqos_consume_page_split_hdr(buffer,
							desc_data->skb_top,
							payload_len, buf2_used);
				goto next_desc;
			} else {
				if (!(rx_normal_desc->rdes3 & DWC_EQOS_RDESC3_FD)) {
					buf2_used = 1;
					/* end of the chain */
					if (hdr_len) {
						payload_len = (pkt_len -
							(pdata->rx_buffer_len * intermediate_desc_cnt) -
							hdr_len);
					} else {
						payload_len = (pkt_len -
							(pdata->rx_buffer_len * intermediate_desc_cnt) -
							buffer->rx_hdr_size);
					}

					skb_fill_page_desc(desc_data->skb_top,
						skb_shinfo(desc_data->skb_top)->nr_frags,
						buffer->page2, 0,
						payload_len);

					/* re-use this skb, as consumed only the page */
					buffer->skb = skb;
					skb = desc_data->skb_top;
					desc_data->skb_top = NULL;
					dwc_eqos_consume_page_split_hdr(buffer, skb,
								 payload_len, buf2_used);
				} else {
					/* no chain, got both FD + LD together */
					if (hdr_len) {
						buf2_used = 1;
						/* add header len to first skb->len */
						skb_put(skb, hdr_len);

						payload_len = pkt_len - hdr_len;
						skb_fill_page_desc(skb, 0,
							buffer->page2, 0,
							payload_len);
					} else {
						/* No split header, hence
						 * payload_len = (payload + hdr_len)
						 */
						if (pkt_len > buffer->rx_hdr_size) {
							buf2_used = 1;
							/* add header len to first skb->len */
							skb_put(skb, buffer->rx_hdr_size);

							payload_len = (pkt_len - buffer->rx_hdr_size);
							skb_fill_page_desc(skb, 0,
								buffer->page2, 0,
								payload_len);
						} else {
							buf2_used = 0;
							/* add header len to first skb->len */
							skb_put(skb, pkt_len);
							payload_len = 0; /* no data in page2 */
						}
					}
					dwc_eqos_consume_page_split_hdr(buffer,
							skb, payload_len,
							buf2_used);
				}
				/* reset for next new packet/frame */
				intermediate_desc_cnt = 0;
				hdr_len = 0;
			}

			dwc_eqos_config_rx_csum(pdata, skb, rx_normal_desc);

			if (IS_ENABLED(CONFIG_VLAN_8021Q))
				dwc_eqos_get_rx_vlan(pdata, skb, rx_normal_desc);

#ifdef YDEBUG_FILTER
			dwc_eqos_check_rx_filter_status(rx_normal_desc);
#endif

			if ((pdata->hw_feat.tsstssel) && (pdata->hwts_rx_en)) {
				/* get rx tstamp if available */
				if (hw_if->rx_tstamp_available(rx_normal_desc)) {
					ret = dwc_eqos_get_rx_hwtstamp(pdata,
								skb, desc_data, qinx);
					if (ret == 0) {
						/* device has not yet updated the CONTEXT desc to hold the
						 * time stamp, hence delay the packet reception
						 */
						buffer->skb = skb;
						buffer->dma = dma_map_single(pdata->ndev->dev.parent, skb->data,
								pdata->rx_buffer_len, DMA_FROM_DEVICE);
						if (dma_mapping_error(pdata->ndev->dev.parent, buffer->dma))
							pr_alert("failed to do the RX dma map\n");

						goto rx_tstmp_failed;
					}
				}
			}

			/* update the statistics */
			dev->stats.rx_packets++;
			dev->stats.rx_bytes += skb->len;
			dwc_eqos_receive_skb(pdata, dev, skb, qinx);
			received++;
 next_desc:
			desc_data->dirty_rx++;
			if (desc_data->dirty_rx >= desc_data->skb_realloc_threshold)
				desc_if->realloc_skb(pdata, qinx);

			INCR_RX_DESC_INDEX(desc_data->cur_rx, 1);
			buf2_used = 0;
		} else {
			/* no more data to read */
			break;
		}
	}

rx_tstmp_failed:

	if (desc_data->dirty_rx)
		desc_if->realloc_skb(pdata, qinx);

	DBGPR("<--dwc_eqos_clean_split_hdr_rx_irq: received = %d\n",
		received);

	return received;
}

/* This function is invoked by main NAPI function if Rx
 * jumbe frame is enabled. This function checks the device descriptor
 * for the packets and passes it to stack if any packets are received
 * by device.
 * retval number of packets received.
 */
static int dwc_eqos_clean_jumbo_rx_irq(struct dwc_eqos_prv_data *pdata,
					  int quota,
					  unsigned int qinx)
{
	struct dwc_eqos_rx_wrapper_descriptor *desc_data =
	    GET_RX_WRAPPER_DESC(qinx);
	struct net_device *dev = pdata->ndev;
	struct desc_if_struct *desc_if = &pdata->desc_if;
	struct hw_if_struct *hw_if = &pdata->hw_if;
	struct sk_buff *skb = NULL;
	int received = 0;
	struct dwc_eqos_rx_buffer *buffer = NULL;
	struct s_rx_normal_desc *rx_normal_desc = NULL;
	u16 pkt_len;
	unsigned char intermediate_desc_cnt = 0;
	unsigned int buf2_used;
	int ret;

	DBGPR("-->dwc_eqos_clean_jumbo_rx_irq: qinx = %u, quota = %d\n",
		qinx, quota);

	while (received < quota) {
		buffer = GET_RX_BUF_PTR(qinx, desc_data->cur_rx);
		rx_normal_desc = GET_RX_DESC_PTR(qinx, desc_data->cur_rx);

		/* check for data availability */
		if (!(rx_normal_desc->rdes3 & DWC_EQOS_RDESC3_OWN)) {
#ifdef DWC_EQOS_ENABLE_RX_DESC_DUMP
			dump_rx_desc(qinx, rx_normal_desc, desc_data->cur_rx);
#endif
			/* assign it to new skb */
			skb = buffer->skb;
			buffer->skb = NULL;

			/* first buffer pointer */
			dma_unmap_page(pdata->ndev->dev.parent, buffer->dma,
				       PAGE_SIZE, DMA_FROM_DEVICE);
			buffer->dma = 0;

			/* second buffer pointer */
			dma_unmap_page(pdata->ndev->dev.parent, buffer->dma2,
				       PAGE_SIZE, DMA_FROM_DEVICE);
			buffer->dma2 = 0;

			/* get the packet length */
			pkt_len =
				(rx_normal_desc->rdes3 & DWC_EQOS_RDESC3_PL);

			/* check for bad packet,
			 * error is valid only for last descriptor (OWN + LD bit set).
			 */
			if ((rx_normal_desc->rdes3 & DWC_EQOS_RDESC3_ES) &&
			    (rx_normal_desc->rdes3 & DWC_EQOS_RDESC3_LD)) {
				DBGPR("Error in rcved pkt, failed to pass it to upper layer\n");
				dump_rx_desc(qinx, rx_normal_desc, desc_data->cur_rx);
				dev->stats.rx_errors++;
				dwc_eqos_update_rx_errors(dev,
					rx_normal_desc->rdes3);

				/* recycle both page and skb */
				buffer->skb = skb;
				if (desc_data->skb_top)
					dev_kfree_skb_any(desc_data->skb_top);

				desc_data->skb_top = NULL;
				goto next_desc;
			}

			if (!(rx_normal_desc->rdes3 & DWC_EQOS_RDESC3_LD)) {
				intermediate_desc_cnt++;
				buf2_used = 1;
				/* this descriptor is only the beginning/middle */
				if (rx_normal_desc->rdes3 & DWC_EQOS_RDESC3_FD) {
					/* this is the beginning of a chain */
					desc_data->skb_top = skb;
					skb_fill_page_desc(skb, 0,
						buffer->page, 0,
						pdata->rx_buffer_len);

					DBGPR("RX: pkt in second buffer pointer\n");
					skb_fill_page_desc(
						desc_data->skb_top,
						skb_shinfo(desc_data->skb_top)->nr_frags,
						buffer->page2, 0,
						pdata->rx_buffer_len);
				} else {
					/* this is the middle of a chain */
					skb_fill_page_desc(desc_data->skb_top,
						skb_shinfo(desc_data->skb_top)->nr_frags,
						buffer->page, 0,
						pdata->rx_buffer_len);

					DBGPR("RX: pkt in second buffer pointer\n");
					skb_fill_page_desc(desc_data->skb_top,
						skb_shinfo(desc_data->skb_top)->nr_frags,
						buffer->page2, 0,
						pdata->rx_buffer_len);
					/* re-use this skb, as consumed only the page */
					buffer->skb = skb;
				}
				dwc_eqos_consume_page(buffer,
							 desc_data->skb_top,
							 (pdata->rx_buffer_len * 2),
							 buf2_used);
				goto next_desc;
			} else {
				if (!(rx_normal_desc->rdes3 & DWC_EQOS_RDESC3_FD)) {
					/* end of the chain */
					pkt_len =
						(pkt_len - (pdata->rx_buffer_len * intermediate_desc_cnt));
					if (pkt_len > pdata->rx_buffer_len) {
						skb_fill_page_desc(desc_data->skb_top,
							skb_shinfo(desc_data->skb_top)->nr_frags,
							buffer->page, 0,
							pdata->rx_buffer_len);

						DBGPR("RX: pkt in second buffer pointer\n");
						skb_fill_page_desc(desc_data->skb_top,
							skb_shinfo(desc_data->skb_top)->nr_frags,
							buffer->page2, 0,
							(pkt_len - pdata->rx_buffer_len));
						buf2_used = 1;
					} else {
						skb_fill_page_desc(desc_data->skb_top,
							skb_shinfo(desc_data->skb_top)->nr_frags,
							buffer->page, 0,
							pkt_len);
						buf2_used = 0;
					}
					/* re-use this skb, as consumed only the page */
					buffer->skb = skb;
					skb = desc_data->skb_top;
					desc_data->skb_top = NULL;
					dwc_eqos_consume_page(buffer, skb,
								 pkt_len,
								 buf2_used);
				} else {
					/* no chain, got both FD + LD together */

					/* code added for copybreak, this should improve
					 * performance for small pkts with large amount
					 * of reassembly being done in the stack
					 */
					if ((pkt_len <= DWC_EQOS_COPYBREAK_DEFAULT)
					    && (skb_tailroom(skb) >= pkt_len)) {
						u8 *vaddr;
						vaddr =
						    kmap_atomic(buffer->page);
						memcpy(skb_tail_pointer(skb),
						       vaddr, pkt_len);
						kunmap_atomic(vaddr);
						/* re-use the page, so don't erase buffer->page/page2 */
						skb_put(skb, pkt_len);
					} else {
						if (pkt_len > pdata->rx_buffer_len) {
							skb_fill_page_desc(skb,
								0, buffer->page,
								0,
								pdata->rx_buffer_len);

							DBGPR ("RX: pkt in second buffer pointer\n");
							skb_fill_page_desc(skb,
								skb_shinfo(skb)->nr_frags, buffer->page2,
								0,
								(pkt_len - pdata->rx_buffer_len));
							buf2_used = 1;
						} else {
							skb_fill_page_desc(skb,
								0, buffer->page,
								0,
								pkt_len);
							buf2_used = 0;
						}
						dwc_eqos_consume_page(buffer,
								skb,
								pkt_len,
								buf2_used);
					}
				}
				intermediate_desc_cnt = 0;
			}

			dwc_eqos_config_rx_csum(pdata, skb, rx_normal_desc);

			if (IS_ENABLED(CONFIG_VLAN_8021Q))
				dwc_eqos_get_rx_vlan(pdata, skb, rx_normal_desc);

#ifdef YDEBUG_FILTER
			dwc_eqos_check_rx_filter_status(rx_normal_desc);
#endif

			if ((pdata->hw_feat.tsstssel) && (pdata->hwts_rx_en)) {
				/* get rx tstamp if available */
				if (hw_if->rx_tstamp_available(rx_normal_desc)) {
					ret = dwc_eqos_get_rx_hwtstamp(pdata,
							skb, desc_data, qinx);
					if (ret == 0) {
						/* device has not yet updated the CONTEXT desc to hold the
						 * time stamp, hence delay the packet reception
						 * */
						buffer->skb = skb;
						buffer->dma = dma_map_single(pdata->ndev->dev.parent, skb->data,
								pdata->rx_buffer_len, DMA_FROM_DEVICE);
						if (dma_mapping_error(pdata->ndev->dev.parent, buffer->dma))
							pr_alert("failed to do the RX dma map\n");

						goto rx_tstmp_failed;
					}
				}
			}

			/* update the statistics */
			dev->stats.rx_packets++;
			dev->stats.rx_bytes += skb->len;

			/* eth type trans needs skb->data to point to something */
			if (!pskb_may_pull(skb, ETH_HLEN)) {
				pr_alert("pskb_may_pull failed\n");
				dev_kfree_skb_any(skb);
				goto next_desc;
			}

			dwc_eqos_receive_skb(pdata, dev, skb, qinx);
			received++;
 next_desc:
			desc_data->dirty_rx++;
			if (desc_data->dirty_rx >= desc_data->skb_realloc_threshold)
				desc_if->realloc_skb(pdata, qinx);

			INCR_RX_DESC_INDEX(desc_data->cur_rx, 1);
		} else {
			/* no more data to read */
			break;
		}
	}

rx_tstmp_failed:

	if (desc_data->dirty_rx)
		desc_if->realloc_skb(pdata, qinx);

	DBGPR("<--dwc_eqos_clean_jumbo_rx_irq: received = %d\n", received);

	return received;
}

/* API to pass the Rx packets to stack if default mode is enabled.
 * This function is invoked by main NAPI function in default
 * Rx mode(non jumbo and non split header). This function checks the
 * device descriptor for the packets and passes it to stack if any packtes
 * are received by device.
 * retval number of packets received.
 */
static int dwc_eqos_clean_rx_irq(struct dwc_eqos_prv_data *pdata,
				    int quota,
				    unsigned int qinx)
{
	struct dwc_eqos_rx_wrapper_descriptor *desc_data =
	    GET_RX_WRAPPER_DESC(qinx);
	struct net_device *dev = pdata->ndev;
	struct desc_if_struct *desc_if = &pdata->desc_if;
	struct hw_if_struct *hw_if = &(pdata->hw_if);
	struct sk_buff *skb = NULL;
	int received = 0;
	struct dwc_eqos_rx_buffer *buffer = NULL;
	struct s_rx_normal_desc *rx_normal_desc = NULL;
	unsigned pkt_len;
	int ret;

	DBGPR("-->dwc_eqos_clean_rx_irq: qinx = %u, quota = %d\n",
		qinx, quota);

	while (received < quota) {
		buffer = GET_RX_BUF_PTR(qinx, desc_data->cur_rx);
		rx_normal_desc = GET_RX_DESC_PTR(qinx, desc_data->cur_rx);

		/* check for data availability */
		if (!(rx_normal_desc->rdes3 & DWC_EQOS_RDESC3_OWN)) {
#ifdef DWC_EQOS_ENABLE_RX_DESC_DUMP
			dump_rx_desc(qinx, rx_normal_desc, desc_data->cur_rx);
#endif
			/* assign it to new skb */
			skb = buffer->skb;
			buffer->skb = NULL;
			dma_unmap_single(pdata->ndev->dev.parent, buffer->dma,
					 pdata->rx_buffer_len, DMA_FROM_DEVICE);
			buffer->dma = 0;

			/* get the packet length */
			pkt_len =
			    (rx_normal_desc->rdes3 & DWC_EQOS_RDESC3_PL);

#ifdef DWC_EQOS_ENABLE_RX_PKT_DUMP
			print_pkt(skb, pkt_len, 0, (desc_data->cur_rx));
#endif
			/* check for bad/oversized packet,
			 * error is valid only for last descriptor (OWN + LD bit set).
			 * */
			if (!(rx_normal_desc->rdes3 & DWC_EQOS_RDESC3_ES) &&
			    (rx_normal_desc->rdes3 & DWC_EQOS_RDESC3_LD)) {
				/* pkt_len = pkt_len - 4; */ /* CRC stripping */

				/* code added for copybreak, this should improve
				 * performance for small pkts with large amount
				 * of reassembly being done in the stack
				 * */
				if (pkt_len < DWC_EQOS_COPYBREAK_DEFAULT) {
					struct sk_buff *new_skb =
					    netdev_alloc_skb_ip_align(dev,
								      pkt_len);
					if (new_skb) {
						skb_copy_to_linear_data_offset(new_skb,
							-NET_IP_ALIGN,
							(skb->data - NET_IP_ALIGN),
							(pkt_len + NET_IP_ALIGN));
						/* recycle actual desc skb */
						buffer->skb = skb;
						skb = new_skb;
					} else {
						/* just continue with the old skb */
					}
				}
				skb_put(skb, pkt_len);

				dwc_eqos_config_rx_csum(pdata, skb,
							rx_normal_desc);

				if (IS_ENABLED(CONFIG_VLAN_8021Q))
					dwc_eqos_get_rx_vlan(pdata, skb, rx_normal_desc);

#ifdef YDEBUG_FILTER
				dwc_eqos_check_rx_filter_status(rx_normal_desc);
#endif

				if ((pdata->hw_feat.tsstssel) && (pdata->hwts_rx_en)) {
					/* get rx tstamp if available */
					if (hw_if->rx_tstamp_available(rx_normal_desc)) {
						ret = dwc_eqos_get_rx_hwtstamp(pdata,
								skb, desc_data, qinx);
						if (ret == 0) {
							/* device has not yet updated the CONTEXT desc to hold the
							 * time stamp, hence delay the packet reception
							 * */
							buffer->skb = skb;
							buffer->dma = dma_map_single(pdata->ndev->dev.parent, skb->data,
									pdata->rx_buffer_len, DMA_FROM_DEVICE);
							if (dma_mapping_error(pdata->ndev->dev.parent, buffer->dma))
								pr_alert("failed to do the RX dma map\n");

							goto rx_tstmp_failed;
						}
					}
				}


				/* update the statistics */
				dev->stats.rx_packets++;
				dev->stats.rx_bytes += skb->len;
				dwc_eqos_receive_skb(pdata, dev, skb, qinx);
				received++;
			} else {
				dump_rx_desc(qinx, rx_normal_desc, desc_data->cur_rx);
				if (!(rx_normal_desc->rdes3 & DWC_EQOS_RDESC3_LD))
					DBGPR("Received oversized pkt, spanned across multiple desc\n");

				/* recycle skb */
				buffer->skb = skb;
				dev->stats.rx_errors++;
				dwc_eqos_update_rx_errors(dev,
					rx_normal_desc->rdes3);
			}

			desc_data->dirty_rx++;
			if (desc_data->dirty_rx >= desc_data->skb_realloc_threshold)
				desc_if->realloc_skb(pdata, qinx);

			INCR_RX_DESC_INDEX(desc_data->cur_rx, 1);
		} else {
			/* no more data to read */
			break;
		}
	}

rx_tstmp_failed:

	if (desc_data->dirty_rx)
		desc_if->realloc_skb(pdata, qinx);

	DBGPR("<--dwc_eqos_clean_rx_irq: received = %d\n", received);

	return received;
}

/* API to update the rx status.
 * This function is called in poll function to update the
 * status of received packets.
 */
void dwc_eqos_update_rx_errors(struct net_device *dev,
				 unsigned int rx_status)
{
	DBGPR("-->dwc_eqos_update_rx_errors\n");

	/* received pkt with crc error */
	if ((rx_status & 0x1000000))
		dev->stats.rx_crc_errors++;

	/* received frame alignment */
	if ((rx_status & 0x100000))
		dev->stats.rx_frame_errors++;

	/* receiver fifo overrun */
	if ((rx_status & 0x200000))
		dev->stats.rx_fifo_errors++;

	DBGPR("<--dwc_eqos_update_rx_errors\n");
}

/* API to pass the received packets to stack
 * This function is provided by NAPI-compliant drivers to operate
 * the interface in a polled mode, with interrupts disabled.
 * retval number of packets received.
 */
int dwc_eqos_poll_rxq(struct napi_struct *napi, int budget)
{
	struct dwc_eqos_rx_queue *rx_queue =
		container_of(napi, struct dwc_eqos_rx_queue, napi);
	struct dwc_eqos_prv_data *pdata = rx_queue->pdata;
	int received = 0;

	pdata->xstats.napi_poll_rxq[rx_queue->qinx]++;

	received = pdata->clean_rx(pdata, budget, rx_queue->qinx);

	pdata->xstats.rx_pkt_n += received;
	pdata->xstats.q_rx_pkt_n[rx_queue->qinx] += received;

	/* If we processed all pkts, we are done;
	 * tell the kernel & re-enable interrupt */
	if (received < budget) {
		unsigned long flags;

		spin_lock_irqsave(&pdata->lock, flags);
		if (pdata->ndev->features & NETIF_F_GRO) {
			/* to turn off polling */
			napi_complete(napi);
			/* Enable ch RX interrupt */
			dwc_eqos_enable_ch_rx_interrpt(pdata, rx_queue->qinx);
		} else {
			napi_complete_done(napi, received);
			/* Enable ch RX interrupt */
			dwc_eqos_enable_ch_rx_interrpt(pdata, rx_queue->qinx);
		}
		spin_unlock_irqrestore(&pdata->lock, flags);
	}

	return received;
}

int dwc_eqos_poll_txq(struct napi_struct *napi, int budget)
{
	struct dwc_eqos_tx_queue *tx_queue =
		container_of(napi, struct dwc_eqos_tx_queue, napi);
	struct dwc_eqos_prv_data *pdata = tx_queue->pdata;
	int processed = 0;

	pdata->xstats.napi_poll_txq[tx_queue->qinx]++;

	processed = dwc_eqos_tx_clean(pdata->ndev, pdata, tx_queue->qinx,
								  budget);

	/* If we processed all pkts, we are done;
	 * tell the kernel & re-enable interrupt
	 */
	if (processed < budget) {
		unsigned long flags;

		spin_lock_irqsave(&pdata->lock, flags);
		napi_complete_done(napi, processed);
		/* Enable ch TX interrupt */
		dwc_eqos_enable_ch_tx_interrpt(pdata, tx_queue->qinx);
		spin_unlock_irqrestore(&pdata->lock, flags);
	}

	return processed;
}

/* API to return the device/interface status.
 * The get_stats function is called whenever an application needs to
 * get statistics for the interface. For example, this happend when ifconfig
 * or netstat -i is run.
 * retval net_device_stats - returns pointer to net_device_stats structure.
 */
static struct net_device_stats *dwc_eqos_get_stats(struct net_device *dev)
{

	return &dev->stats;
}

#ifdef CONFIG_NET_POLL_CONTROLLER

/*!
* \brief API to receive packets in polling mode.
*
* \details This is polling receive function used by netconsole and other
* diagnostic tool to allow network i/o with interrupts disabled.
*
* \param[in] dev - pointer to net_device structure
*
* \return void
*/

static void dwc_eqos_poll_controller(struct net_device *dev)
{
	struct dwc_eqos_prv_data *pdata = netdev_priv(dev);

	DBGPR("-->dwc_eqos_poll_controller\n");

	disable_irq(pdata->irq_number);
	dwc_eqos_isr_sw_dwc_eqos(pdata->irq_number, pdata);
	enable_irq(pdata->irq_number);

	DBGPR("<--dwc_eqos_poll_controller\n");
}

#endif	/*end of CONFIG_NET_POLL_CONTROLLER */

/*!
 * \brief User defined parameter setting API
 *
 * \details This function is invoked by kernel to update the device
 * configuration to new features. This function supports enabling and
 * disabling of TX and RX csum features.
 *
 * \param[in] dev – pointer to net device structure.
 * \param[in] features – device feature to be enabled/disabled.
 *
 * \return int
 *
 * \retval 0
 */
static int dwc_eqos_set_features(struct net_device *dev, netdev_features_t features)
{
	struct dwc_eqos_prv_data *pdata = netdev_priv(dev);
	struct hw_if_struct *hw_if = &(pdata->hw_if);
	unsigned int dev_rxcsum_enable;
	unsigned int dev_rxvlan_enable, dev_txvlan_enable;

	if (pdata->hw_feat.rx_coe_sel) {
		dev_rxcsum_enable = !!(pdata->dev_state & NETIF_F_RXCSUM);

		if (((features & NETIF_F_RXCSUM) == NETIF_F_RXCSUM)
		    && !dev_rxcsum_enable) {
			hw_if->enable_rx_csum(dev);
			pdata->dev_state |= NETIF_F_RXCSUM;
			pr_alert("State change - rxcsum enable\n");
		} else if (((features & NETIF_F_RXCSUM) == 0)
			   && dev_rxcsum_enable) {
			hw_if->disable_rx_csum(dev);
			pdata->dev_state &= ~NETIF_F_RXCSUM;
			pr_alert("State change - rxcsum disable\n");
		}
	}
#if IS_ENABLED(CONFIG_VLAN_8021Q)
	dev_rxvlan_enable = !!(pdata->dev_state & NETIF_F_HW_VLAN_CTAG_RX);
	if (((features & NETIF_F_HW_VLAN_CTAG_RX) == NETIF_F_HW_VLAN_CTAG_RX)
	    && !dev_rxvlan_enable) {
		pdata->dev_state |= NETIF_F_HW_VLAN_CTAG_RX;
		hw_if->config_rx_outer_vlan_stripping(pdata, DWC_EQOS_RX_VLAN_STRIP_ALWAYS);
		pr_alert("State change - rxvlan enable\n");
	} else if (((features & NETIF_F_HW_VLAN_CTAG_RX) == 0) &&
			dev_rxvlan_enable) {
		pdata->dev_state &= ~NETIF_F_HW_VLAN_CTAG_RX;
		hw_if->config_rx_outer_vlan_stripping(pdata, DWC_EQOS_RX_NO_VLAN_STRIP);
		pr_alert("State change - rxvlan disable\n");
	}

	dev_txvlan_enable = !!(pdata->dev_state & NETIF_F_HW_VLAN_CTAG_TX_BIT);
	if (((features & NETIF_F_HW_VLAN_CTAG_TX_BIT) == NETIF_F_HW_VLAN_CTAG_TX_BIT)
	    && !dev_txvlan_enable) {
		pdata->dev_state |= NETIF_F_HW_VLAN_CTAG_TX_BIT;
		pr_alert("State change - txvlan enable\n");
	} else if (((features & NETIF_F_HW_VLAN_CTAG_TX_BIT) == 0) &&
			dev_txvlan_enable) {
		pdata->dev_state &= ~NETIF_F_HW_VLAN_CTAG_TX_BIT;
		pr_alert("State change - txvlan disable\n");
	}
#endif	/* CONFIG_VLAN_8021Q */

	DBGPR("<--dwc_eqos_set_features\n");

	return 0;
}

/*!
 * \brief User defined parameter setting API
 *
 * \details This function is invoked by kernel to adjusts the requested
 * feature flags according to device-specific constraints, and returns the
 * resulting flags. This API must not modify the device state.
 *
 * \param[in] dev – pointer to net device structure.
 * \param[in] features – device supported features.
 *
 * \return u32
 *
 * \retval modified flag
 */

static netdev_features_t dwc_eqos_fix_features(struct net_device *dev, netdev_features_t features)
{
	struct dwc_eqos_prv_data *pdata = netdev_priv(dev);

	DBGPR("-->dwc_eqos_fix_features: %#llx\n", features);

	if (IS_ENABLED(CONFIG_VLAN_8021Q)) {
		if (pdata->rx_split_hdr) {
			/* The VLAN tag stripping must be set for the split function.
			 * For instance, the DMA separates the header and payload of
			 * an untagged packet only. Hence, when a tagged packet is
			 * received, the QOS must be programmed such that the VLAN
			 * tags are deleted/stripped from the received packets.
			 * */
			features |= NETIF_F_HW_VLAN_CTAG_RX;
		}
	}

	DBGPR("<--dwc_eqos_fix_features: %#llx\n", features);

	return features;
}

/*!
 * \details This function is invoked by ioctl function when user issues
 * an ioctl command to enable/disable L3/L4 filtering.
 *
 * \param[in] dev – pointer to net device structure.
 * \param[in] flags – flag to indicate whether L3/L4 filtering to be
 *                  enabled/disabled.
 *
 * \return integer
 *
 * \retval zero on success and -ve number on failure.
 */
static int dwc_eqos_config_l3_l4_filtering(struct net_device *dev,
		unsigned int flags)
{
	struct dwc_eqos_prv_data *pdata = netdev_priv(dev);
	struct hw_if_struct *hw_if = &(pdata->hw_if);
	int ret = 0;

	DBGPR_FILTER("-->dwc_eqos_config_l3_l4_filtering\n");

	if (flags && pdata->l3_l4_filter) {
		pr_info("L3/L4 filtering is already enabled\n");
		return -EINVAL;
	}

	if (!flags && !pdata->l3_l4_filter) {
		pr_info("L3/L4 filtering is already disabled\n");
		return -EINVAL;
	}

	pdata->l3_l4_filter = !!flags;
	hw_if->config_l3_l4_filter_enable(dev, pdata->l3_l4_filter);

	DBGPR_FILTER("Succesfully %s L3/L4 filtering\n",
		(flags ? "ENABLED" : "DISABLED"));

	DBGPR_FILTER("<--dwc_eqos_config_l3_l4_filtering\n");

	return ret;
}

/*!
 * \details This function is invoked by ioctl function when user issues an
 * ioctl command to configure L3(IPv4) filtering. This function does following,
 * - enable/disable IPv4 filtering.
 * - select source/destination address matching.
 * - select perfect/inverse matching.
 * - Update the IPv4 address into MAC register.
 *
 * \param[in] dev – pointer to net device structure.
 * \param[in] req – pointer to IOCTL specific structure.
 *
 * \return integer
 *
 * \retval zero on success and -ve number on failure.
 */
static int dwc_eqos_config_ip4_filters(struct net_device *dev,
		struct ifr_data_struct *req)
{
	struct dwc_eqos_prv_data *pdata = netdev_priv(dev);
	struct hw_if_struct *hw_if = &(pdata->hw_if);
	struct dwc_eqos_l3_l4_filter *u_l3_filter =
		(struct dwc_eqos_l3_l4_filter *)req->ptr;
	struct dwc_eqos_l3_l4_filter l_l3_filter;
	int ret = 0;

	DBGPR_FILTER("-->dwc_eqos_config_ip4_filters\n");

	if (pdata->hw_feat.l3l4_filter_num == 0)
		return DWC_EQOS_NO_HW_SUPPORT;

	if (copy_from_user(&l_l3_filter, u_l3_filter,
		sizeof(struct dwc_eqos_l3_l4_filter)))
		return -EFAULT;

	if ((l_l3_filter.filter_no + 1) > pdata->hw_feat.l3l4_filter_num) {
		pr_alert("%d filter is not supported in the HW\n",
			l_l3_filter.filter_no);
		return DWC_EQOS_NO_HW_SUPPORT;
	}

	if (!pdata->l3_l4_filter) {
		hw_if->config_l3_l4_filter_enable(dev, 1);
		pdata->l3_l4_filter = 1;
	}

	/* configure the L3 filters */
	hw_if->config_l3_filters(dev, l_l3_filter.filter_no,
			l_l3_filter.filter_enb_dis, 0,
			l_l3_filter.src_dst_addr_match,
			l_l3_filter.perfect_inverse_match);

	if (!l_l3_filter.src_dst_addr_match)
		hw_if->update_ip4_addr0(dev, l_l3_filter.filter_no,
				l_l3_filter.ip4_addr);
	else
		hw_if->update_ip4_addr1(dev, l_l3_filter.filter_no,
				l_l3_filter.ip4_addr);

	DBGPR_FILTER("Successfully %s IPv4 %s %s addressing filtering on %d filter\n",
		(l_l3_filter.filter_enb_dis ? "ENABLED" : "DISABLED"),
		(l_l3_filter.perfect_inverse_match ? "INVERSE" : "PERFECT"),
		(l_l3_filter.src_dst_addr_match ? "DESTINATION" : "SOURCE"),
		l_l3_filter.filter_no);

	DBGPR_FILTER("<--dwc_eqos_config_ip4_filters\n");

	return ret;
}

/*!
 * \details This function is invoked by ioctl function when user issues an
 * ioctl command to configure L3(IPv6) filtering. This function does following,
 * - enable/disable IPv6 filtering.
 * - select source/destination address matching.
 * - select perfect/inverse matching.
 * - Update the IPv6 address into MAC register.
 *
 * \param[in] dev – pointer to net device structure.
 * \param[in] req – pointer to IOCTL specific structure.
 *
 * \return integer
 *
 * \retval zero on success and -ve number on failure.
 */
static int dwc_eqos_config_ip6_filters(struct net_device *dev,
		struct ifr_data_struct *req)
{
	struct dwc_eqos_prv_data *pdata = netdev_priv(dev);
	struct hw_if_struct *hw_if = &(pdata->hw_if);
	struct dwc_eqos_l3_l4_filter *u_l3_filter =
		(struct dwc_eqos_l3_l4_filter *)req->ptr;
	struct dwc_eqos_l3_l4_filter l_l3_filter;
	int ret = 0;

	DBGPR_FILTER("-->dwc_eqos_config_ip6_filters\n");

	if (pdata->hw_feat.l3l4_filter_num == 0)
		return DWC_EQOS_NO_HW_SUPPORT;

	if (copy_from_user(&l_l3_filter, u_l3_filter,
		sizeof(struct dwc_eqos_l3_l4_filter)))
		return -EFAULT;

	if ((l_l3_filter.filter_no + 1) > pdata->hw_feat.l3l4_filter_num) {
		pr_alert("%d filter is not supported in the HW\n",
			l_l3_filter.filter_no);
		return DWC_EQOS_NO_HW_SUPPORT;
	}

	if (!pdata->l3_l4_filter) {
		hw_if->config_l3_l4_filter_enable(dev, 1);
		pdata->l3_l4_filter = 1;
	}

	/* configure the L3 filters */
	hw_if->config_l3_filters(dev, l_l3_filter.filter_no,
			l_l3_filter.filter_enb_dis, 1,
			l_l3_filter.src_dst_addr_match,
			l_l3_filter.perfect_inverse_match);

	hw_if->update_ip6_addr(dev, l_l3_filter.filter_no,
			l_l3_filter.ip6_addr);

	DBGPR_FILTER("Successfully %s IPv6 %s %s addressing filtering on %d filter\n",
		(l_l3_filter.filter_enb_dis ? "ENABLED" : "DISABLED"),
		(l_l3_filter.perfect_inverse_match ? "INVERSE" : "PERFECT"),
		(l_l3_filter.src_dst_addr_match ? "DESTINATION" : "SOURCE"),
		l_l3_filter.filter_no);

	DBGPR_FILTER("<--dwc_eqos_config_ip6_filters\n");

	return ret;
}

/*!
 * \details This function is invoked by ioctl function when user issues an
 * ioctl command to configure L4(TCP/UDP) filtering. This function does following,
 * - enable/disable L4 filtering.
 * - select TCP/UDP filtering.
 * - select source/destination port matching.
 * - select perfect/inverse matching.
 * - Update the port number into MAC register.
 *
 * \param[in] dev – pointer to net device structure.
 * \param[in] req – pointer to IOCTL specific structure.
 * \param[in] tcp_udp – flag to indicate TCP/UDP filtering.
 *
 * \return integer
 *
 * \retval zero on success and -ve number on failure.
 */
static int dwc_eqos_config_tcp_udp_filters(struct net_device *dev,
		struct ifr_data_struct *req,
		int tcp_udp)
{
	struct dwc_eqos_prv_data *pdata = netdev_priv(dev);
	struct hw_if_struct *hw_if = &(pdata->hw_if);
	struct dwc_eqos_l3_l4_filter *u_l4_filter =
		(struct dwc_eqos_l3_l4_filter *)req->ptr;
	struct dwc_eqos_l3_l4_filter l_l4_filter;
	int ret = 0;

	DBGPR_FILTER("-->dwc_eqos_config_tcp_udp_filters\n");

	if (pdata->hw_feat.l3l4_filter_num == 0)
		return DWC_EQOS_NO_HW_SUPPORT;

	if (copy_from_user(&l_l4_filter, u_l4_filter,
		sizeof(struct dwc_eqos_l3_l4_filter)))
		return -EFAULT;

	if ((l_l4_filter.filter_no + 1) > pdata->hw_feat.l3l4_filter_num) {
		pr_alert("%d filter is not supported in the HW\n",
			l_l4_filter.filter_no);
		return DWC_EQOS_NO_HW_SUPPORT;
	}

	if (!pdata->l3_l4_filter) {
		hw_if->config_l3_l4_filter_enable(dev, 1);
		pdata->l3_l4_filter = 1;
	}

	/* configure the L4 filters */
	hw_if->config_l4_filters(dev, l_l4_filter.filter_no,
			l_l4_filter.filter_enb_dis,
			tcp_udp,
			l_l4_filter.src_dst_addr_match,
			l_l4_filter.perfect_inverse_match);

	if (l_l4_filter.src_dst_addr_match)
		hw_if->update_l4_da_port_no(dev, l_l4_filter.filter_no,
				l_l4_filter.port_no);
	else
		hw_if->update_l4_sa_port_no(dev, l_l4_filter.filter_no,
				l_l4_filter.port_no);

	DBGPR_FILTER("Successfully %s %s %s %s Port number filtering on %d filter\n",
		(l_l4_filter.filter_enb_dis ? "ENABLED" : "DISABLED"),
		(tcp_udp ? "UDP" : "TCP"),
		(l_l4_filter.perfect_inverse_match ? "INVERSE" : "PERFECT"),
		(l_l4_filter.src_dst_addr_match ? "DESTINATION" : "SOURCE"),
		l_l4_filter.filter_no);

	DBGPR_FILTER("<--dwc_eqos_config_tcp_udp_filters\n");

	return ret;
}

/*!
 * \details This function is invoked by ioctl function when user issues an
 * ioctl command to configure VALN filtering. This function does following,
 * - enable/disable VLAN filtering.
 * - select perfect/hash filtering.
 *
 * \param[in] dev – pointer to net device structure.
 * \param[in] req – pointer to IOCTL specific structure.
 *
 * \return integer
 *
 * \retval zero on success and -ve number on failure.
 */
static int dwc_eqos_config_vlan_filter(struct net_device *dev,
		struct ifr_data_struct *req)
{
	struct dwc_eqos_prv_data *pdata = netdev_priv(dev);
	struct hw_if_struct *hw_if = &(pdata->hw_if);
	struct dwc_eqos_vlan_filter *u_vlan_filter =
		(struct dwc_eqos_vlan_filter *)req->ptr;
	struct dwc_eqos_vlan_filter l_vlan_filter;
	int ret = 0;

	DBGPR_FILTER("-->dwc_eqos_config_vlan_filter\n");

	if (copy_from_user(&l_vlan_filter, u_vlan_filter,
		sizeof(struct dwc_eqos_vlan_filter)))
		return -EFAULT;

	if ((l_vlan_filter.perfect_hash) &&
		(pdata->hw_feat.vlan_hash_en == 0)) {
		pr_alert("VLAN HASH filtering is not supported\n");
		return DWC_EQOS_NO_HW_SUPPORT;
	}

	/* configure the vlan filter */
	hw_if->config_vlan_filtering(dev, l_vlan_filter.filter_enb_dis,
					l_vlan_filter.perfect_hash,
					l_vlan_filter.perfect_inverse_match);
	pdata->vlan_hash_filtering = l_vlan_filter.perfect_hash;

	DBGPR_FILTER("Successfully %s VLAN %s filtering and %s matching\n",
		(l_vlan_filter.filter_enb_dis ? "ENABLED" : "DISABLED"),
		(l_vlan_filter.perfect_hash ? "HASH" : "PERFECT"),
		(l_vlan_filter.perfect_inverse_match ? "INVERSE" : "PERFECT"));

	DBGPR_FILTER("<--dwc_eqos_config_vlan_filter\n");

	return ret;
}

/*!
 * \details This function is invoked by ioctl function when user issues an
 * ioctl command to configure L2 destination addressing filtering mode. This
 * function dose following,
 * - selects perfect/hash filtering.
 * - selects perfect/inverse matching.
 *
 * \param[in] dev – pointer to net device structure.
 * \param[in] req – pointer to IOCTL specific structure.
 *
 * \return integer
 *
 * \retval zero on success and -ve number on failure.
 */
static int dwc_eqos_confing_l2_da_filter(struct net_device *dev,
		struct ifr_data_struct *req)
{
	struct dwc_eqos_prv_data *pdata = netdev_priv(dev);
	struct hw_if_struct *hw_if = &(pdata->hw_if);
	struct dwc_eqos_l2_da_filter *u_l2_da_filter =
	  (struct dwc_eqos_l2_da_filter *)req->ptr;
	struct dwc_eqos_l2_da_filter l_l2_da_filter;
	int ret = 0;

	DBGPR_FILTER("-->dwc_eqos_confing_l2_da_filter\n");

	if (copy_from_user(&l_l2_da_filter, u_l2_da_filter,
	      sizeof(struct dwc_eqos_l2_da_filter)))
		return -EFAULT;

	if (l_l2_da_filter.perfect_hash) {
		if (pdata->hw_feat.hash_tbl_sz > 0)
			pdata->l2_filtering_mode = 1;
		else
			ret = DWC_EQOS_NO_HW_SUPPORT;
	} else {
		if (pdata->max_addr_reg_cnt > 1)
			pdata->l2_filtering_mode = 0;
		else
			ret = DWC_EQOS_NO_HW_SUPPORT;
	}

	/* configure L2 DA perfect/inverse_matching */
	hw_if->config_l2_da_perfect_inverse_match(pdata, l_l2_da_filter.perfect_inverse_match);

	DBGPR_FILTER("Successfully selected L2 %s filtering and %s DA matching\n",
		(l_l2_da_filter.perfect_hash ? "HASH" : "PERFECT"),
		(l_l2_da_filter.perfect_inverse_match ? "INVERSE" : "PERFECT"));

	DBGPR_FILTER("<--dwc_eqos_confing_l2_da_filter\n");

	return ret;
}

/*!
 * \details This function is invoked by ioctl function when user issues
 * an ioctl command to enable/disable mac loopback mode.
 *
 * \param[in] dev – pointer to net device structure.
 * \param[in] flags – flag to indicate whether mac loopback mode to be
 *                  enabled/disabled.
 *
 * \return integer
 *
 * \retval zero on success and -ve number on failure.
 */
static int dwc_eqos_config_mac_loopback_mode(struct net_device *dev,
		unsigned int flags)
{
	struct dwc_eqos_prv_data *pdata = netdev_priv(dev);
	struct hw_if_struct *hw_if = &(pdata->hw_if);
	int ret = 0;

	DBGPR("-->dwc_eqos_config_mac_loopback_mode\n");

	if (flags && pdata->mac_loopback_mode) {
		pr_info("MAC loopback mode is already enabled\n");
		return -EINVAL;
	}
	if (!flags && !pdata->mac_loopback_mode) {
		pr_info("MAC loopback mode is already disabled\n");
		return -EINVAL;
	}
	pdata->mac_loopback_mode = !!flags;
	hw_if->config_mac_loopback_mode(dev, flags);

	pr_alert("Succesfully %s MAC loopback mode\n",
		(flags ? "enabled" : "disabled"));

	DBGPR("<--dwc_eqos_config_mac_loopback_mode\n");

	return ret;
}

#ifdef DWC_ETH_QOS_ENABLE_DVLAN
static int config_tx_dvlan_processing_via_reg(struct dwc_eqos_prv_data *pdata,
						unsigned int flags)
{
	struct hw_if_struct *hw_if = &(pdata->hw_if);

	pr_alert("--> config_tx_dvlan_processing_via_reg()\n");

	if (pdata->in_out & DWC_EQOS_DVLAN_OUTER)
		hw_if->config_tx_outer_vlan(pdata, pdata->op_type,
					pdata->outer_vlan_tag);

	if (pdata->in_out & DWC_EQOS_DVLAN_INNER)
		hw_if->config_tx_inner_vlan(pdata, pdata->op_type,
					pdata->inner_vlan_tag);

	if (flags == DWC_EQOS_DVLAN_DISABLE)
		hw_if->config_mac_for_vlan_pkt(pdata); /* restore default configurations */
	else
		hw_if->config_dvlan(pdata, 1);

	pr_alert("<-- config_tx_dvlan_processing_via_reg()\n");

	return Y_SUCCESS;
}

static int config_tx_dvlan_processing_via_desc(struct dwc_eqos_prv_data *pdata,
						unsigned int flags)
{
	u32 regval;
	struct hw_if_struct *hw_if = &(pdata->hw_if);

	pr_alert("-->config_tx_dvlan_processing_via_desc\n");

	if (flags == DWC_ETH_QOS_DVLAN_DISABLE) {
		hw_if->config_mac_for_vlan_pkt(pdata); /* restore default configurations */
		pdata->via_reg_or_desc = 0;
	} else {
		hw_if->config_dvlan(pdata, 1);
	}

	if (pdata->in_out & DWC_EQOS_DVLAN_INNER) {
		regval = dwceqos_readl(pdata, DWCEQOS_MAC_INNER_VLAN_INCL);
		regval |= DWCEQOS_MAC_IVLAN_INCL_VLTI;
		dwceqos_writel(pdata, DWCEQOS_MAC_INNER_VLAN_INCL, regval);
	}

	if (pdata->in_out & DWC_EQOS_DVLAN_OUTER) {
		regval = dwceqos_readl(pdata, DWCEQOS_MAC_VLAN_INCL);
		regval |= DWCEQOS_MAC_VLAN_INCL_VLTI;
		dwceqos_writel(pdata, DWCEQOS_MAC_VLAN_INCL, regval);
	}

	pr_alert("<--config_tx_dvlan_processing_via_desc\n");

	return Y_SUCCESS;
}

/*!
 * \details This function is invoked by ioctl function when user issues
 * an ioctl command to configure mac double vlan processing feature.
 *
 * \param[in] pdata - pointer to private data structure.
 * \param[in] flags – Each bit in this variable carry some information related
 *		      double vlan processing.
 *
 * \return integer
 *
 * \retval zero on success and -ve number on failure.
 */
static int dwc_eqos_config_tx_dvlan_processing(
		struct dwc_eqos_prv_data *pdata,
		struct ifr_data_struct *req)
{
	struct dwc_eqos_config_dvlan l_config_doubule_vlan,
					  *u_config_doubule_vlan = req->ptr;
	int ret = 0;

	DBGPR("-->dwc_eqos_config_tx_dvlan_processing\n");

	if (copy_from_user(&l_config_doubule_vlan, u_config_doubule_vlan,
				sizeof(struct dwc_eqos_config_dvlan))) {
		pr_alert("Failed to fetch Double vlan Struct info from user\n");
		return DWC_EQOS_CONFIG_FAIL;
	}

	pdata->inner_vlan_tag = l_config_doubule_vlan.inner_vlan_tag;
	pdata->outer_vlan_tag = l_config_doubule_vlan.outer_vlan_tag;
	pdata->op_type = l_config_doubule_vlan.op_type;
	pdata->in_out = l_config_doubule_vlan.in_out;
	pdata->via_reg_or_desc = l_config_doubule_vlan.via_reg_or_desc;

	if (pdata->via_reg_or_desc == DWC_EQOS_VIA_REG)
		ret = config_tx_dvlan_processing_via_reg(pdata, req->flags);
	else
		ret = config_tx_dvlan_processing_via_desc(pdata, req->flags);

	DBGPR("<--dwc_eqos_config_tx_dvlan_processing\n");

	return ret;
}

/*!
 * \details This function is invoked by ioctl function when user issues
 * an ioctl command to configure mac double vlan processing feature.
 *
 * \param[in] pdata - pointer to private data structure.
 * \param[in] flags – Each bit in this variable carry some information related
 *		      double vlan processing.
 *
 * \return integer
 *
 * \retval zero on success and -ve number on failure.
 */
static int dwc_eqos_config_rx_dvlan_processing(
		struct dwc_eqos_prv_data *pdata, unsigned int flags)
{
	struct hw_if_struct *hw_if = &(pdata->hw_if);
	int ret = 0;

	DBGPR("-->dwc_eqos_config_rx_dvlan_processing\n");

	hw_if->config_dvlan(pdata, 1);
	if (flags == DWC_EQOS_DVLAN_NONE) {
		hw_if->config_dvlan(pdata, 0);
		hw_if->config_rx_outer_vlan_stripping(pdata, DWC_EQOS_RX_NO_VLAN_STRIP);
		hw_if->config_rx_inner_vlan_stripping(pdata, DWC_EQOS_RX_NO_VLAN_STRIP);
	} else if (flags == DWC_EQOS_DVLAN_INNER) {
		hw_if->config_rx_outer_vlan_stripping(pdata, DWC_EQOS_RX_NO_VLAN_STRIP);
		hw_if->config_rx_inner_vlan_stripping(pdata, DWC_EQOS_RX_VLAN_STRIP_ALWAYS);
	} else if (flags == DWC_EQOS_DVLAN_OUTER) {
		hw_if->config_rx_outer_vlan_stripping(pdata, DWC_EQOS_RX_VLAN_STRIP_ALWAYS);
		hw_if->config_rx_inner_vlan_stripping(pdata, DWC_EQOS_RX_NO_VLAN_STRIP);
	} else if (flags == DWC_EQOS_DVLAN_BOTH) {
		hw_if->config_rx_outer_vlan_stripping(pdata, DWC_EQOS_RX_VLAN_STRIP_ALWAYS);
		hw_if->config_rx_inner_vlan_stripping(pdata, DWC_EQOS_RX_VLAN_STRIP_ALWAYS);
	} else {
		pr_alert("ERROR : double VLAN Rx configuration - Invalid argument");
		ret = DWC_EQOS_CONFIG_FAIL;
	}

	DBGPR("<--dwc_eqos_config_rx_dvlan_processing\n");

	return ret;
}

/*!
 * \details This function is invoked by ioctl function when user issues
 * an ioctl command to configure mac double vlan (svlan) processing feature.
 *
 * \param[in] pdata - pointer to private data structure.
 * \param[in] flags – Each bit in this variable carry some information related
 *		      double vlan processing.
 *
 * \return integer
 *
 * \retval zero on success and -ve number on failure.
 */
static int dwc_eqos_config_svlan(struct dwc_eqos_prv_data *pdata,
					unsigned int flags)
{
	struct hw_if_struct *hw_if = &(pdata->hw_if);
	int ret = 0;

	DBGPR("-->dwc_eqos_config_svlan\n");

	ret = hw_if->config_svlan(pdata, flags);
	if (ret == Y_FAILURE)
		ret = DWC_EQOS_CONFIG_FAIL;

	DBGPR("<--dwc_eqos_config_svlan\n");

	return ret;
}
#endif /* end of DWC_ETH_QOS_ENABLE_DVLAN */

static void dwc_eqos_config_timer_registers(
				struct dwc_eqos_prv_data *pdata)
{
		struct timespec now;
		struct hw_if_struct *hw_if = &(pdata->hw_if);
		u64 temp;

		DBGPR("-->dwc_eqos_config_timer_registers\n");

		/* program Sub Second Increment Reg */
		hw_if->config_sub_second_increment(pdata, DWC_EQOS_SYSCLOCK);

		/* formula is :
		 * addend = 2^32/freq_div_ratio;
		 *
		 * where, freq_div_ratio = DWC_EQOS_SYSCLOCK/20MHz
		 *
		 * hence, addend = ((2^32) * 20MHz)/DWC_EQOS_SYSCLOCK;
		 *
		 * NOTE: DWC_EQOS_SYSCLOCK should be >= 20MHz to
		 *       achieve 50ns accuracy.
		 *
		 * 2^x * y == (y << x), hence
		 * 2^32 * 20000000 ==> (20000000 << 32)
		 * */
		temp = (u64)(20000000ULL << 32);
		pdata->default_addend = div_u64(temp, DWC_EQOS_SYSCLOCK);

		hw_if->config_addend(pdata, pdata->default_addend);

		/* initialize system time */
		if (tstamp_raw_monotonic)
			getrawmonotonic(&now);
		else
			getnstimeofday(&now);
		hw_if->init_systime(pdata, now.tv_sec, now.tv_nsec);

		DBGPR("-->dwc_eqos_config_timer_registers\n");
}

/*!
 * \details This function is invoked by ioctl function when user issues
 * an ioctl command to configure PTP offloading feature.
 *
 * \param[in] pdata - pointer to private data structure.
 * \param[in] flags – Each bit in this variable carry some information related
 *		      double vlan processing.
 *
 * \return integer
 *
 * \retval zero on success and -ve number on failure.
 */
static int dwc_eqos_config_ptpoffload(
		struct dwc_eqos_prv_data *pdata,
		struct dwc_eqos_config_ptpoffloading *u_conf_ptp)
{
	unsigned int pto_cntrl;
	unsigned int varMAC_TCR;
	struct dwc_eqos_config_ptpoffloading l_conf_ptp;
	struct hw_if_struct *hw_if = &(pdata->hw_if);


	if (copy_from_user(&l_conf_ptp, u_conf_ptp,
				sizeof(struct dwc_eqos_config_ptpoffloading))) {
		pr_alert("Failed to fetch Double vlan Struct info from user\n");
		return DWC_EQOS_CONFIG_FAIL;
	}

	pr_info("-->dwc_eqos_config_ptpoffload - %d\n", l_conf_ptp.mode);

	pto_cntrl = MAC_PTOCR_PTOEN; /* enable ptp offloading */
	varMAC_TCR = MAC_TCR_TSENA | MAC_TCR_TSIPENA | MAC_TCR_TSVER2ENA
			| MAC_TCR_TSCFUPDT | MAC_TCR_TSCTRLSSR;
	if (l_conf_ptp.mode == DWC_EQOS_PTP_ORDINARY_SLAVE) {

		varMAC_TCR |= MAC_TCR_TSEVENTENA;
		pdata->ptp_offloading_mode = DWC_EQOS_PTP_ORDINARY_SLAVE;

	} else if (l_conf_ptp.mode == DWC_EQOS_PTP_TRASPARENT_SLAVE) {

		pto_cntrl |= MAC_PTOCR_APDREQEN;
		varMAC_TCR |= MAC_TCR_TSEVENTENA;
		varMAC_TCR |= MAC_TCR_SNAPTYPSEL_1;
		pdata->ptp_offloading_mode =
			DWC_EQOS_PTP_TRASPARENT_SLAVE;

	} else if (l_conf_ptp.mode == DWC_EQOS_PTP_ORDINARY_MASTER) {

		pto_cntrl |= MAC_PTOCR_ASYNCEN;
		varMAC_TCR |= MAC_TCR_TSEVENTENA;
		varMAC_TCR |= MAC_TCR_TSMASTERENA;
		pdata->ptp_offloading_mode = DWC_EQOS_PTP_ORDINARY_MASTER;

	} else if (l_conf_ptp.mode == DWC_EQOS_PTP_TRASPARENT_MASTER) {

		pto_cntrl |= MAC_PTOCR_ASYNCEN | MAC_PTOCR_APDREQEN;
		varMAC_TCR |= MAC_TCR_SNAPTYPSEL_1;
		varMAC_TCR |= MAC_TCR_TSEVENTENA;
		varMAC_TCR |= MAC_TCR_TSMASTERENA;
		pdata->ptp_offloading_mode =
			DWC_EQOS_PTP_TRASPARENT_MASTER;

	} else if (l_conf_ptp.mode == DWC_EQOS_PTP_PEER_TO_PEER_TRANSPARENT) {

		pto_cntrl |= MAC_PTOCR_APDREQEN;
		varMAC_TCR |= MAC_TCR_SNAPTYPSEL_3;
		pdata->ptp_offloading_mode =
			DWC_EQOS_PTP_PEER_TO_PEER_TRANSPARENT;
	}

	pdata->ptp_offload = 1;
	if (l_conf_ptp.en_dis == DWC_EQOS_PTP_OFFLOADING_DISABLE) {
		pto_cntrl = 0;
		varMAC_TCR = 0;
		pdata->ptp_offload = 0;
	}

	pto_cntrl |= (l_conf_ptp.domain_num << 8);
	hw_if->config_hw_time_stamping(pdata, varMAC_TCR);
	dwc_eqos_config_timer_registers(pdata);
	hw_if->config_ptpoffload_engine(pdata, pto_cntrl, l_conf_ptp.mc_uc);

	pr_info("<--dwc_eqos_config_ptpoffload\n");

	return Y_SUCCESS;
}

/*!
 * \details This function is invoked by ioctl function when user issues
 * an ioctl command to enable/disable pfc.
 *
 * \param[in] dev – pointer to net device structure.
 * \param[in] flags – flag to indicate whether pfc to be enabled/disabled.
 *
 * \return integer
 *
 * \retval zero on success and -ve number on failure.
 */
static int dwc_eqos_config_pfc(struct net_device *dev,
		unsigned int flags)
{
	struct dwc_eqos_prv_data *pdata = netdev_priv(dev);
	struct hw_if_struct *hw_if = &(pdata->hw_if);
	int ret = 0;

	DBGPR("-->dwc_eqos_config_pfc\n");

	if (!pdata->hw_feat.dcb_en) {
		pr_alert("PFC is not supported\n");
		return DWC_EQOS_NO_HW_SUPPORT;
	}

	hw_if->config_pfc(pdata, flags);

	pr_alert("Succesfully %s PFC(Priority Based Flow Control)\n",
		(flags ? "enabled" : "disabled"));

	DBGPR("<--dwc_eqos_config_pfc\n");

	return ret;
}

/*!
 * \brief Driver IOCTL routine
 *
 * \details This function is invoked by main ioctl function when
 * users request to configure various device features like,
 * PMT module, TX and RX PBL, TX and RX FIFO threshold level,
 * TX and RX OSF mode, SA insert/replacement, L2/L3/L4 and
 * VLAN filtering, AVB/DCB algorithm etc.
 *
 * \param[in] pdata – pointer to private data structure.
 * \param[in] req – pointer to ioctl structure.
 *
 * \return int
 *
 * \retval 0 - success
 * \retval negative - failure
 */
static int dwc_eqos_handle_prv_ioctl(struct dwc_eqos_prv_data *pdata,
					struct ifr_data_struct *req)
{
	unsigned int qinx = req->qinx;
	struct dwc_eqos_tx_wrapper_descriptor *tx_desc_data =
	    GET_TX_WRAPPER_DESC(qinx);
	struct dwc_eqos_rx_wrapper_descriptor *rx_desc_data =
	    GET_RX_WRAPPER_DESC(qinx);
	struct hw_if_struct *hw_if = &(pdata->hw_if);
	struct net_device *dev = pdata->ndev;
	int ret = 0;

	DBGPR("-->dwc_eqos_handle_prv_ioctl\n");

	if (qinx > DWC_EQOS_QUEUE_CNT) {
		pr_alert("Queue number %d is invalid\n" \
				"Hardware has only %d Tx/Rx Queues\n",
				qinx, DWC_EQOS_QUEUE_CNT);
		ret = DWC_EQOS_NO_HW_SUPPORT;
		return ret;
	}

	switch (req->cmd) {
	case DWC_EQOS_RX_THRESHOLD_CMD:
		rx_desc_data->rx_threshold_val = req->flags;
		hw_if->config_rx_threshold(pdata, qinx,
					rx_desc_data->rx_threshold_val);
		pr_alert("Configured Rx threshold with %d\n",
		       rx_desc_data->rx_threshold_val);
		break;

	case DWC_EQOS_TX_THRESHOLD_CMD:
		tx_desc_data->tx_threshold_val = req->flags;
		hw_if->config_tx_threshold(pdata, qinx,
					tx_desc_data->tx_threshold_val);
		pr_alert("Configured Tx threshold with %d\n",
		       tx_desc_data->tx_threshold_val);
		break;

	case DWC_EQOS_RSF_CMD:
		rx_desc_data->rsf_on = req->flags;
		hw_if->config_rsf_mode(pdata, qinx, rx_desc_data->rsf_on);
		pr_alert("Receive store and forward mode %s\n",
		       (rx_desc_data->rsf_on) ? "enabled" : "disabled");
		break;

	case DWC_EQOS_TSF_CMD:
		tx_desc_data->tsf_on = req->flags;
		hw_if->config_tsf_mode(pdata, qinx, tx_desc_data->tsf_on);
		pr_alert("Transmit store and forward mode %s\n",
		       (tx_desc_data->tsf_on) ? "enabled" : "disabled");
		break;

	case DWC_EQOS_OSF_CMD:
		tx_desc_data->osf_on = req->flags;
		hw_if->config_osf_mode(pdata, qinx, tx_desc_data->osf_on);
		pr_alert("Transmit DMA OSF mode is %s\n",
		       (tx_desc_data->osf_on) ? "enabled" : "disabled");
		break;

	case DWC_EQOS_INCR_INCRX_CMD:
		pdata->incr_incrx = req->flags;
		hw_if->config_incr_incrx_mode(pdata, pdata->incr_incrx);
		pr_alert("%s mode is enabled\n",
		       (pdata->incr_incrx) ? "INCRX" : "INCR");
		break;

	case DWC_EQOS_RX_PBL_CMD:
		rx_desc_data->rx_pbl = req->flags;
		dwc_eqos_config_rx_pbl(pdata, rx_desc_data->rx_pbl, qinx);
		break;

	case DWC_EQOS_TX_PBL_CMD:
		tx_desc_data->tx_pbl = req->flags;
		dwc_eqos_config_tx_pbl(pdata, tx_desc_data->tx_pbl, qinx);
		break;

#ifdef DWC_ETH_QOS_ENABLE_DVLAN
	case DWC_EQOS_DVLAN_TX_PROCESSING_CMD:
		if (pdata->hw_feat.sa_vlan_ins) {
			ret = dwc_eqos_config_tx_dvlan_processing(pdata, req);
		} else {
			pr_alert("No HW support for Single/Double VLAN\n");
			ret = DWC_EQOS_NO_HW_SUPPORT;
		}
		break;
	case DWC_EQOS_DVLAN_RX_PROCESSING_CMD:
		if (pdata->hw_feat.sa_vlan_ins) {
			ret = dwc_eqos_config_rx_dvlan_processing(pdata, req->flags);
		} else {
			pr_alert("No HW support for Single/Double VLAN\n");
			ret = DWC_EQOS_NO_HW_SUPPORT;
		}
		break;
	case DWC_EQOS_SVLAN_CMD:
		if (pdata->hw_feat.sa_vlan_ins) {
			ret = dwc_eqos_config_svlan(pdata, req->flags);
		} else {
			pr_alert("No HW support for Single/Double VLAN\n");
			ret = DWC_EQOS_NO_HW_SUPPORT;
		}
		break;
#endif /* end of DWC_ETH_QOS_ENABLE_DVLAN */
	case DWC_EQOS_PTPOFFLOADING_CMD:
		if (pdata->hw_feat.tsstssel) {
			ret = dwc_eqos_config_ptpoffload(pdata,
					req->ptr);
		} else {
			pr_alert("No HW support for PTP\n");
			ret = DWC_EQOS_NO_HW_SUPPORT;
		}
		break;

	case DWC_EQOS_SA0_DESC_CMD:
		if (pdata->hw_feat.sa_vlan_ins) {
			pdata->tx_sa_ctrl_via_desc = req->flags;
			pdata->tx_sa_ctrl_via_reg = DWC_EQOS_SA0_NONE;
			if (req->flags == DWC_EQOS_SA0_NONE) {
				memcpy(pdata->mac_addr, pdata->ndev->dev_addr,
				       DWC_EQOS_MAC_ADDR_LEN);
			} else {
				memcpy(pdata->mac_addr, mac_addr0,
				       DWC_EQOS_MAC_ADDR_LEN);
			}
			hw_if->configure_mac_addr0_reg(pdata, pdata->mac_addr);
			hw_if->configure_sa_via_reg(pdata->ndev, pdata->tx_sa_ctrl_via_reg);
			pr_info("SA will use MAC0 with descriptor for configuration %d\n",
			       pdata->tx_sa_ctrl_via_desc);
		} else {
			pr_info("Device doesn't supports SA Insertion/Replacement\n");
			ret = DWC_EQOS_NO_HW_SUPPORT;
		}
		break;

	case DWC_EQOS_SA1_DESC_CMD:
		if (pdata->hw_feat.sa_vlan_ins) {
			pdata->tx_sa_ctrl_via_desc = req->flags;
			pdata->tx_sa_ctrl_via_reg = DWC_EQOS_SA1_NONE;
			if (req->flags == DWC_EQOS_SA1_NONE) {
				memcpy(pdata->mac_addr, pdata->ndev->dev_addr,
				       DWC_EQOS_MAC_ADDR_LEN);
			} else {
				memcpy(pdata->mac_addr, mac_addr1,
				       DWC_EQOS_MAC_ADDR_LEN);
			}
			hw_if->configure_mac_addr1_reg(pdata, pdata->mac_addr);
			hw_if->configure_sa_via_reg(pdata->ndev, pdata->tx_sa_ctrl_via_reg);
			pr_info("SA will use MAC1 with descriptor for configuration %d\n",
			       pdata->tx_sa_ctrl_via_desc);
		} else {
			pr_info("Device doesn't supports SA Insertion/Replacement\n");
			ret = DWC_EQOS_NO_HW_SUPPORT;
		}
		break;

	case DWC_EQOS_SA0_REG_CMD:
		if (pdata->hw_feat.sa_vlan_ins) {
			pdata->tx_sa_ctrl_via_reg = req->flags;
			pdata->tx_sa_ctrl_via_desc = DWC_EQOS_SA0_NONE;
			if (req->flags == DWC_EQOS_SA0_NONE) {
				memcpy(pdata->mac_addr, pdata->ndev->dev_addr,
				       DWC_EQOS_MAC_ADDR_LEN);
			} else {
				memcpy(pdata->mac_addr, mac_addr0,
				       DWC_EQOS_MAC_ADDR_LEN);
			}
			hw_if->configure_mac_addr0_reg(pdata, pdata->mac_addr);
			hw_if->configure_sa_via_reg(pdata->ndev, pdata->tx_sa_ctrl_via_reg);
			pr_info("SA will use MAC0 with register for configuration %d\n",
			       pdata->tx_sa_ctrl_via_desc);
		} else {
			pr_info("Device doesn't supports SA Insertion/Replacement\n");
			ret = DWC_EQOS_NO_HW_SUPPORT;
		}
		break;

	case DWC_EQOS_SA1_REG_CMD:
		if (pdata->hw_feat.sa_vlan_ins) {
			pdata->tx_sa_ctrl_via_reg = req->flags;
			pdata->tx_sa_ctrl_via_desc = DWC_EQOS_SA1_NONE;
			if (req->flags == DWC_EQOS_SA1_NONE) {
				memcpy(pdata->mac_addr, pdata->ndev->dev_addr,
				       DWC_EQOS_MAC_ADDR_LEN);
			} else {
				memcpy(pdata->mac_addr, mac_addr1,
				       DWC_EQOS_MAC_ADDR_LEN);
			}
			hw_if->configure_mac_addr1_reg(pdata, pdata->mac_addr);
			hw_if->configure_sa_via_reg(pdata->ndev, pdata->tx_sa_ctrl_via_reg);
			pr_info("SA will use MAC1 with register for configuration %d\n",
			       pdata->tx_sa_ctrl_via_desc);
		} else {
			pr_info("Device doesn't supports SA Insertion/Replacement\n");
			ret = DWC_EQOS_NO_HW_SUPPORT;
		}
		break;

	case DWC_EQOS_SETUP_CONTEXT_DESCRIPTOR:
		if (pdata->hw_feat.sa_vlan_ins) {
			tx_desc_data->context_setup = req->context_setup;
			if (tx_desc_data->context_setup == 1) {
				pr_alert("Context descriptor will be transmitted"\
						" with every normal descriptor on %d DMA Channel\n",
						qinx);
			} else {
				pr_alert("Context descriptor will be setup"\
						" only if VLAN id changes %d\n", qinx);
			}
		} else {
			pr_info("Device doesn't support VLAN operations\n");
			ret = DWC_EQOS_NO_HW_SUPPORT;
		}
		break;

	case DWC_EQOS_GET_RX_QCNT:
		req->qinx = pdata->rx_queue_cnt;
		break;

	case DWC_EQOS_GET_TX_QCNT:
		req->qinx = pdata->tx_queue_cnt;
		break;

	case DWC_EQOS_GET_CONNECTED_SPEED:
		req->connected_speed = pdata->speed;
		break;

	case DWC_EQOS_DCB_ALGORITHM:
		dwc_eqos_program_dcb_algorithm(pdata, req);
		break;

	case DWC_EQOS_AVB_ALGORITHM:
		dwc_eqos_program_avb_algorithm(pdata, req);
		break;

	case DWC_EQOS_L3_L4_FILTER_CMD:
		if (pdata->hw_feat.l3l4_filter_num > 0) {
			ret = dwc_eqos_config_l3_l4_filtering(dev, req->flags);
			if (ret == 0)
				ret = DWC_EQOS_CONFIG_SUCCESS;
			else
				ret = DWC_EQOS_CONFIG_FAIL;
		} else {
			ret = DWC_EQOS_NO_HW_SUPPORT;
		}
		break;
	case DWC_EQOS_IPV4_FILTERING_CMD:
		ret = dwc_eqos_config_ip4_filters(dev, req);
		break;
	case DWC_EQOS_IPV6_FILTERING_CMD:
		ret = dwc_eqos_config_ip6_filters(dev, req);
		break;
	case DWC_EQOS_UDP_FILTERING_CMD:
		ret = dwc_eqos_config_tcp_udp_filters(dev, req, 1);
		break;
	case DWC_EQOS_TCP_FILTERING_CMD:
		ret = dwc_eqos_config_tcp_udp_filters(dev, req, 0);
		break;
	case DWC_EQOS_VLAN_FILTERING_CMD:
		ret = dwc_eqos_config_vlan_filter(dev, req);
		break;
	case DWC_EQOS_L2_DA_FILTERING_CMD:
		ret = dwc_eqos_confing_l2_da_filter(dev, req);
		break;
	case DWC_EQOS_AXI_PBL_CMD:
		pdata->axi_pbl = req->flags;
		hw_if->config_axi_pbl_val(pdata, pdata->axi_pbl);
		pr_alert("AXI PBL value: %d\n", pdata->axi_pbl);
		break;
	case DWC_EQOS_AXI_WORL_CMD:
		pdata->axi_worl = req->flags;
		hw_if->config_axi_worl_val(pdata, pdata->axi_worl);
		pr_alert("AXI WORL value: %d\n", pdata->axi_worl);
		break;
	case DWC_EQOS_AXI_RORL_CMD:
		pdata->axi_rorl = req->flags;
		hw_if->config_axi_rorl_val(pdata, pdata->axi_rorl);
		pr_alert("AXI RORL value: %d\n", pdata->axi_rorl);
		break;
	case DWC_EQOS_MAC_LOOPBACK_MODE_CMD:
		ret = dwc_eqos_config_mac_loopback_mode(dev, req->flags);
		if (ret == 0)
			ret = DWC_EQOS_CONFIG_SUCCESS;
		else
			ret = DWC_EQOS_CONFIG_FAIL;
		break;
	case DWC_EQOS_PFC_CMD:
		ret = dwc_eqos_config_pfc(dev, req->flags);
		break;
	default:
		ret = -EOPNOTSUPP;
		pr_alert("Unsupported command call\n");
	}

	DBGPR("<--dwc_eqos_handle_prv_ioctl\n");

	return ret;
}

/*!
 * \brief control hw timestamping.
 *
 * \details This function is used to configure the MAC to enable/disable both
 * outgoing(Tx) and incoming(Rx) packets time stamping based on user input.
 *
 * \param[in] pdata – pointer to private data structure.
 * \param[in] ifr – pointer to IOCTL specific structure.
 *
 * \return int
 *
 * \retval 0 - success
 * \retval negative - failure
 */
static int dwc_eqos_handle_hwtstamp_ioctl(struct dwc_eqos_prv_data *pdata,
	struct ifreq *ifr)
{
	struct hw_if_struct *hw_if = &(pdata->hw_if);
	struct hwtstamp_config config;
	u32 ptp_v2 = 0;
	u32 tstamp_all = 0;
	u32 ptp_over_ipv4_udp = 0;
	u32 ptp_over_ipv6_udp = 0;
	u32 ptp_over_ethernet = 0;
	u32 snap_type_sel = 0;
	u32 ts_master_en = 0;
	u32 ts_event_en = 0;
	u32 av_8021asm_en = 0;
	u32 varMAC_TCR = 0;
	u64 temp = 0;
	struct timespec now;

	DBGPR_PTP("-->dwc_eqos_handle_hwtstamp_ioctl\n");

	if (!pdata->hw_feat.tsstssel) {
		pr_alert("No hw timestamping is available in this core\n");
		return -EOPNOTSUPP;
	}

	if (copy_from_user(&config, ifr->ifr_data,
		sizeof(struct hwtstamp_config)))
		return -EFAULT;

	DBGPR_PTP("config.flags = %#x, tx_type = %#x, rx_filter = %#x\n",
		config.flags, config.tx_type, config.rx_filter);

	/* reserved for future extensions */
	if (config.flags)
		return -EINVAL;

	switch (config.tx_type) {
	case HWTSTAMP_TX_OFF:
		pdata->hwts_tx_en = 0;
		break;
	case HWTSTAMP_TX_ON:
		pdata->hwts_tx_en = 1;
		break;
	default:
		return -ERANGE;
	}

	switch (config.rx_filter) {
	/* time stamp no incoming packet at all */
	case HWTSTAMP_FILTER_NONE:
		config.rx_filter = HWTSTAMP_FILTER_NONE;
		break;

	/* PTP v1, UDP, any kind of event packet */
	case HWTSTAMP_FILTER_PTP_V1_L4_EVENT:
		config.rx_filter = HWTSTAMP_FILTER_PTP_V1_L4_EVENT;
		/* take time stamp for all event messages */
		snap_type_sel = MAC_TCR_SNAPTYPSEL_1;

		ptp_over_ipv4_udp = MAC_TCR_TSIPV4ENA;
		ptp_over_ipv6_udp = MAC_TCR_TSIPV6ENA;
		break;

	/* PTP v1, UDP, Sync packet */
	case HWTSTAMP_FILTER_PTP_V1_L4_SYNC:
		config.rx_filter = HWTSTAMP_FILTER_PTP_V1_L4_SYNC;
		/* take time stamp for SYNC messages only */
		ts_event_en = MAC_TCR_TSEVENTENA;

		ptp_over_ipv4_udp = MAC_TCR_TSIPV4ENA;
		ptp_over_ipv6_udp = MAC_TCR_TSIPV6ENA;
		break;

	/* PTP v1, UDP, Delay_req packet */
	case HWTSTAMP_FILTER_PTP_V1_L4_DELAY_REQ:
		config.rx_filter = HWTSTAMP_FILTER_PTP_V1_L4_DELAY_REQ;
		/* take time stamp for Delay_Req messages only */
		ts_master_en = MAC_TCR_TSMASTERENA;
		ts_event_en = MAC_TCR_TSEVENTENA;

		ptp_over_ipv4_udp = MAC_TCR_TSIPV4ENA;
		ptp_over_ipv6_udp = MAC_TCR_TSIPV6ENA;
		break;

	/* PTP v2, UDP, any kind of event packet */
	case HWTSTAMP_FILTER_PTP_V2_L4_EVENT:
		config.rx_filter = HWTSTAMP_FILTER_PTP_V2_L4_EVENT;
		ptp_v2 = MAC_TCR_TSVER2ENA;
		/* take time stamp for all event messages */
		snap_type_sel = MAC_TCR_SNAPTYPSEL_1;

		ptp_over_ipv4_udp = MAC_TCR_TSIPV4ENA;
		ptp_over_ipv6_udp = MAC_TCR_TSIPV6ENA;
		break;

	/* PTP v2, UDP, Sync packet */
	case HWTSTAMP_FILTER_PTP_V2_L4_SYNC:
		config.rx_filter = HWTSTAMP_FILTER_PTP_V2_L4_SYNC;
		ptp_v2 = MAC_TCR_TSVER2ENA;
		/* take time stamp for SYNC messages only */
		ts_event_en = MAC_TCR_TSEVENTENA;

		ptp_over_ipv4_udp = MAC_TCR_TSIPV4ENA;
		ptp_over_ipv6_udp = MAC_TCR_TSIPV6ENA;
		break;

	/* PTP v2, UDP, Delay_req packet */
	case HWTSTAMP_FILTER_PTP_V2_L4_DELAY_REQ:
		config.rx_filter = HWTSTAMP_FILTER_PTP_V2_L4_DELAY_REQ;
		ptp_v2 = MAC_TCR_TSVER2ENA;
		/* take time stamp for Delay_Req messages only */
		ts_master_en = MAC_TCR_TSMASTERENA;
		ts_event_en = MAC_TCR_TSEVENTENA;

		ptp_over_ipv4_udp = MAC_TCR_TSIPV4ENA;
		ptp_over_ipv6_udp = MAC_TCR_TSIPV6ENA;
		break;

	/* PTP v2/802.AS1, any layer, any kind of event packet */
	case HWTSTAMP_FILTER_PTP_V2_EVENT:
		config.rx_filter = HWTSTAMP_FILTER_PTP_V2_EVENT;
		ptp_v2 = MAC_TCR_TSVER2ENA;
		/* take time stamp for all event messages */
		snap_type_sel = MAC_TCR_SNAPTYPSEL_1;

		ptp_over_ipv4_udp = MAC_TCR_TSIPV4ENA;
		ptp_over_ipv6_udp = MAC_TCR_TSIPV6ENA;
		ptp_over_ethernet = MAC_TCR_TSIPENA;
		av_8021asm_en = MAC_TCR_AV8021ASMEN;
		break;

	/* PTP v2/802.AS1, any layer, Sync packet */
	case HWTSTAMP_FILTER_PTP_V2_SYNC:
		config.rx_filter = HWTSTAMP_FILTER_PTP_V2_SYNC;
		ptp_v2 = MAC_TCR_TSVER2ENA;
		/* take time stamp for SYNC messages only */
		ts_event_en = MAC_TCR_TSEVENTENA;

		ptp_over_ipv4_udp = MAC_TCR_TSIPV4ENA;
		ptp_over_ipv6_udp = MAC_TCR_TSIPV6ENA;
		ptp_over_ethernet = MAC_TCR_TSIPENA;
		av_8021asm_en = MAC_TCR_AV8021ASMEN;
		break;

	/* PTP v2/802.AS1, any layer, Delay_req packet */
	case HWTSTAMP_FILTER_PTP_V2_DELAY_REQ:
		config.rx_filter = HWTSTAMP_FILTER_PTP_V2_DELAY_REQ;
		ptp_v2 = MAC_TCR_TSVER2ENA;
		/* take time stamp for Delay_Req messages only */
		ts_master_en = MAC_TCR_TSMASTERENA;
		ts_event_en = MAC_TCR_TSEVENTENA;

		ptp_over_ipv4_udp = MAC_TCR_TSIPV4ENA;
		ptp_over_ipv6_udp = MAC_TCR_TSIPV6ENA;
		ptp_over_ethernet = MAC_TCR_TSIPENA;
		av_8021asm_en = MAC_TCR_AV8021ASMEN;
		break;

	/* time stamp any incoming packet */
	case HWTSTAMP_FILTER_ALL:
		config.rx_filter = HWTSTAMP_FILTER_ALL;
		tstamp_all = MAC_TCR_TSENALL;
		break;

	default:
		return -ERANGE;
	}
	pdata->hwts_rx_en = ((config.rx_filter == HWTSTAMP_FILTER_NONE) ? 0 : 1);

	if (!pdata->hwts_tx_en && !pdata->hwts_rx_en) {
		/* disable hw time stamping */
		hw_if->config_hw_time_stamping(pdata, varMAC_TCR);
	} else {
		varMAC_TCR = (MAC_TCR_TSENA | MAC_TCR_TSCFUPDT | MAC_TCR_TSCTRLSSR |
				tstamp_all | ptp_v2 | ptp_over_ethernet | ptp_over_ipv6_udp |
				ptp_over_ipv4_udp | ts_event_en | ts_master_en |
				snap_type_sel | av_8021asm_en);

		if (!pdata->one_nsec_accuracy)
			varMAC_TCR &= ~MAC_TCR_TSCTRLSSR;

		hw_if->config_hw_time_stamping(pdata, varMAC_TCR);

		/* program Sub Second Increment Reg */
		hw_if->config_sub_second_increment(pdata, DWC_EQOS_SYSCLOCK);

		/* formula is :
		 * addend = 2^32/freq_div_ratio;
		 *
		 * where, freq_div_ratio = DWC_EQOS_SYSCLOCK/20MHz
		 *
		 * hence, addend = ((2^32) * 20MHz)/DWC_EQOS_SYSCLOCK;
		 *
		 * NOTE: DWC_EQOS_SYSCLOCK should be >= 20MHz to
		 *       achive 50ns accuracy.
		 *
		 * 2^x * y == (y << x), hence
		 * 2^32 * 20000000 ==> (20000000 << 32)
		 * */
		temp = (u64)(20000000ULL << 32);
		pdata->default_addend = div_u64(temp, DWC_EQOS_SYSCLOCK);

		hw_if->config_addend(pdata, pdata->default_addend);

		/* initialize system time */
		if (tstamp_raw_monotonic)
			getrawmonotonic(&now);
		else
			getnstimeofday(&now);
		hw_if->init_systime(pdata, now.tv_sec, now.tv_nsec);
	}

	DBGPR_PTP("config.flags = %#x, tx_type = %#x, rx_filter = %#x\n",
		config.flags, config.tx_type, config.rx_filter);

	DBGPR_PTP("<--dwc_eqos_handle_hwtstamp_ioctl\n");

	return (copy_to_user(ifr->ifr_data, &config,
		sizeof(struct hwtstamp_config))) ? -EFAULT : 0;
}

/*!
 * \brief Driver IOCTL routine
 *
 * \details This function is invoked by kernel when a user request an ioctl
 * which can't be handled by the generic interface code. Following operations
 * are performed in this functions.
 * - Configuring the PMT module.
 * - Configuring TX and RX PBL.
 * - Configuring the TX and RX FIFO threshold level.
 * - Configuring the TX and RX OSF mode.
 *
 * \param[in] dev – pointer to net device structure.
 * \param[in] ifr – pointer to IOCTL specific structure.
 * \param[in] cmd – IOCTL command.
 *
 * \return int
 *
 * \retval 0 - success
 * \retval negative - failure
 */
static int dwc_eqos_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	struct dwc_eqos_prv_data *pdata = netdev_priv(dev);
	struct ifr_data_struct *req = ifr->ifr_ifru.ifru_data;
	struct mii_ioctl_data *data = if_mii(ifr);
	unsigned int reg_val = 0;
	int ret = 0;

	DBGPR("-->dwc_eqos_ioctl\n");

	if ((!netif_running(dev))/* || (!pdata->phydev)*/) {
		DBGPR("<--dwc_eqos_ioctl - error\n");
		DBGPR("exit if");
		return -EINVAL;
	}

	spin_lock(&pdata->lock);
	switch (cmd) {
	case SIOCGMIIPHY:
		data->phy_id = pdata->phyaddr;
		pr_alert("PHY ID: SIOCGMIIPHY\n");
		break;

	case SIOCGMIIREG:
		ret =
			dwc_eqos_mdio_read_direct(pdata, pdata->phyaddr,
					(data->reg_num & 0x1F), &reg_val);
		if (ret)
			ret = -EIO;

		data->val_out = reg_val;
		pr_alert("PHY ID: SIOCGMIIREG reg:%#x reg_val:%#x\n",
				(data->reg_num & 0x1F), reg_val);
		break;

	case SIOCSMIIREG:
		pr_alert("PHY ID: SIOCSMIIPHY\n");
		break;

	case DWC_EQOS_PRV_IOCTL:
		ret = dwc_eqos_handle_prv_ioctl(pdata, req);
		req->command_error = ret;
		break;

	case SIOCSHWTSTAMP:
		ret = dwc_eqos_handle_hwtstamp_ioctl(pdata, ifr);
		break;

	default:
		ret = -EOPNOTSUPP;
		pr_alert("Unsupported IOCTL call\n");
	}
	spin_unlock(&pdata->lock);

	DBGPR("<--dwc_eqos_ioctl\n");

	return ret;
}

/*!
* \brief API to change MTU.
*
* \details This function is invoked by upper layer when user changes
* MTU (Maximum Transfer Unit). The MTU is used by the Network layer
* to driver packet transmission. Ethernet has a default MTU of
* 1500Bytes. This value can be changed with ifconfig -
* ifconfig <interface_name> mtu <new_mtu_value>
*
* \param[in] dev - pointer to net_device structure
* \param[in] new_mtu - the new MTU for the device.
*
* \return integer
*
* \retval 0 - on success and -ve on failure.
*/
static int dwc_eqos_change_mtu(struct net_device *dev, int new_mtu)
{
	struct dwc_eqos_prv_data *pdata = netdev_priv(dev);
	int max_frame;

	/* Don't allow changing MTU when the interface is running.
	 * Stopping and starting eth1 can be problematic because
	 * of mux shifts between external and internal clocks
	 * which can fail if peer turbo is not in a good state.
	 */
	if (netif_running(dev)) {
		netdev_err(dev, "must be stopped to change its MTU\n");
		return -EBUSY;
	}

#if (IS_ENABLED(CONFIG_VLAN_8021Q))
		max_frame = (new_mtu + ETH_HLEN + ETH_FCS_LEN + VLAN_HLEN);
#else
		max_frame = (new_mtu + ETH_HLEN + ETH_FCS_LEN);
#endif

	DBGPR("-->dwc_eqos_change_mtu: new_mtu:%d\n", new_mtu);

	if (dev->mtu == new_mtu) {
		pr_info("%s: is already configured to %d mtu\n",
		       dev->name, new_mtu);
		return 0;
	}

	/* Supported frame sizes */
	if ((new_mtu < DWC_EQOS_MIN_SUPPORTED_MTU) ||
	    (max_frame > DWC_EQOS_MAX_SUPPORTED_MTU)) {
		pr_info("%s: invalid MTU, min %d and max %d MTU are supported\n",
		       dev->name, DWC_EQOS_MIN_SUPPORTED_MTU,
		       DWC_EQOS_MAX_SUPPORTED_MTU);
		return -EINVAL;
	}

	pr_info("changing MTU from %d to %d\n", dev->mtu, new_mtu);

	if (max_frame <= 2048)
		pdata->rx_buffer_len = 2048;
	else
		pdata->rx_buffer_len = PAGE_SIZE; /* in case of JUMBO frame,
						max buffer allocated is
						PAGE_SIZE */

	if (max_frame == DWC_EQOS_ETH_FRAME_LEN)
		pdata->rx_buffer_len =
		    ALIGN(DWC_EQOS_ETH_FRAME_LEN, 128);

	dev->mtu = new_mtu;

	DBGPR("<--dwc_eqos_change_mtu\n");

	return 0;
}

unsigned int crc32_snps_le(unsigned int initval, unsigned char *data, unsigned int size)
{
	unsigned int crc = initval;
	unsigned int poly = 0x04c11db7;
	unsigned int temp = 0;
	unsigned char my_data = 0;
	int bit_count;
	for (bit_count = 0; bit_count < size; bit_count++) {
		if ((bit_count % 8) == 0)
			my_data = data[bit_count/8];
		DBGPR_FILTER("%s my_data = %x crc=%x\n", __func__, my_data, crc);
		temp = ((crc >> 31) ^  my_data) &  0x1;
		crc <<= 1;
		if (temp != 0)
			crc ^= poly;
		my_data >>= 1;
	}
		DBGPR_FILTER("%s my_data = %x crc=%x\n", __func__, my_data, crc);
	return ~crc;
}

/*!
* \brief API to delete vid to HW filter.
*
* \details This function is invoked by upper layer when a VLAN id is removed.
* This function deletes the VLAN id from the HW filter.
* vlan id can be removed with vconfig -
* vconfig rem <interface_name > <vlan_id>
*
* \param[in] dev - pointer to net_device structure
* \param[in] vid - vlan id to be removed.
*
* \return void
*/
static int dwc_eqos_vlan_rx_kill_vid(struct net_device *dev, __be16 proto, u16 vid)
{
	struct dwc_eqos_prv_data *pdata = netdev_priv(dev);
	struct hw_if_struct *hw_if = &(pdata->hw_if);
	unsigned short new_index, old_index;
	int crc32_val = 0;
	unsigned int enb_12bit_vhash;

	pr_alert("-->dwc_eqos_vlan_rx_kill_vid: vid = %d\n",
		vid);

	if (pdata->vlan_hash_filtering) {
		crc32_val = (bitrev32(~crc32_le(~0, (unsigned char *)&vid, 2)) >> 28);

		enb_12bit_vhash = hw_if->get_vlan_tag_comparison(dev);
		if (enb_12bit_vhash) {
			/* neget 4-bit crc value for 12-bit VLAN hash comparison */
			new_index = (1 << (~crc32_val & 0xF));
		} else {
			new_index = (1 << (crc32_val & 0xF));
		}

		old_index = hw_if->get_vlan_hash_table_reg(dev);
		old_index &= ~new_index;
		hw_if->update_vlan_hash_table_reg(dev, old_index);
		pdata->vlan_ht_or_id = old_index;
	} else {
		/* By default, receive only VLAN pkt with VID = 1
		 * becasue writting 0 will pass all VLAN pkt */
		hw_if->update_vlan_id(dev, 1);
		pdata->vlan_ht_or_id = 1;
	}

	pr_alert("<--dwc_eqos_vlan_rx_kill_vid\n");
	return 0;
}

/*!
* \brief API to add vid to HW filter.
*
* \details This function is invoked by upper layer when a new VALN id is
* registered. This function updates the HW filter with new VLAN id.
* New vlan id can be added with vconfig -
* vconfig add <interface_name > <vlan_id>
*
* \param[in] dev - pointer to net_device structure
* \param[in] vid - new vlan id.
*
* \return void
*/
static int dwc_eqos_vlan_rx_add_vid(struct net_device *dev, __be16 proto, u16 vid)
{
	struct dwc_eqos_prv_data *pdata = netdev_priv(dev);
	struct hw_if_struct *hw_if = &(pdata->hw_if);
	unsigned short new_index, old_index;
	int crc32_val = 0;
	unsigned int enb_12bit_vhash;

	pr_info("-->dwc_eqos_vlan_rx_add_vid: vid = %d\n",
		vid);

	if (pdata->vlan_hash_filtering) {
		/* The upper 4 bits of the calculated CRC are used to
		 * index the content of the VLAN Hash Table Reg.
		 * */
		crc32_val = (bitrev32(~crc32_le(~0, (unsigned char *)&vid, 2)) >> 28);

		/* These 4(0xF) bits determines the bit within the
		 * VLAN Hash Table Reg 0
		 * */
		enb_12bit_vhash = hw_if->get_vlan_tag_comparison(dev);
		if (enb_12bit_vhash) {
			/* neget 4-bit crc value for 12-bit VLAN hash comparison */
			new_index = (1 << (~crc32_val & 0xF));
		} else {
			new_index = (1 << (crc32_val & 0xF));
		}

		old_index = hw_if->get_vlan_hash_table_reg(dev);
		old_index |= new_index;
		hw_if->update_vlan_hash_table_reg(dev, old_index);
		pdata->vlan_ht_or_id = old_index;
	} else {
		hw_if->update_vlan_id(dev, vid);
		pdata->vlan_ht_or_id = vid;
	}

	pr_info("<--dwc_eqos_vlan_rx_add_vid\n");
	return 0;
}

/*!
 * \details This function is invoked by ioctl function when the user issues an
 * ioctl command to change the RX DMA PBL value. This function will program
 * the device to configure the user specified RX PBL value.
 *
 * \param[in] pdata – pointer to private data structure.
 * \param[in] rx_pbl – RX DMA pbl value to be programmed.
 *
 * \return void
 *
 * \retval none
 */
static void dwc_eqos_config_rx_pbl(struct dwc_eqos_prv_data *pdata,
				      unsigned int rx_pbl,
				      unsigned int qinx)
{
	struct hw_if_struct *hw_if = &(pdata->hw_if);
	unsigned int pblx8_val = 0;

	DBGPR("-->dwc_eqos_config_rx_pbl: %d\n", rx_pbl);

	switch (rx_pbl) {
	case DWC_EQOS_PBL_1:
	case DWC_EQOS_PBL_2:
	case DWC_EQOS_PBL_4:
	case DWC_EQOS_PBL_8:
	case DWC_EQOS_PBL_16:
	case DWC_EQOS_PBL_32:
		hw_if->config_rx_pbl_val(pdata, qinx, rx_pbl);
		hw_if->config_pblx8(pdata, qinx, 0);
		break;
	case DWC_EQOS_PBL_64:
	case DWC_EQOS_PBL_128:
	case DWC_EQOS_PBL_256:
		hw_if->config_rx_pbl_val(pdata, qinx, rx_pbl / 8);
		hw_if->config_pblx8(pdata, qinx, 1);
		pblx8_val = 1;
		break;
	}

	switch (pblx8_val) {
	case 0:
		pr_alert("Tx PBL[%d] value: %d\n",
				qinx, hw_if->get_tx_pbl_val(pdata, qinx));
		pr_alert("Rx PBL[%d] value: %d\n",
				qinx, hw_if->get_rx_pbl_val(pdata, qinx));
		break;
	case 1:
		pr_alert("Tx PBL[%d] value: %d\n",
				qinx, (hw_if->get_tx_pbl_val(pdata, qinx) * 8));
		pr_alert("Rx PBL[%d] value: %d\n",
				qinx, (hw_if->get_rx_pbl_val(pdata, qinx) * 8));
		break;
	}

	DBGPR("<--dwc_eqos_config_rx_pbl\n");
}

/*!
 * \details This function is invoked by ioctl function when the user issues an
 * ioctl command to change the TX DMA PBL value. This function will program
 * the device to configure the user specified TX PBL value.
 *
 * \param[in] pdata – pointer to private data structure.
 * \param[in] tx_pbl – TX DMA pbl value to be programmed.
 *
 * \return void
 *
 * \retval none
 */
static void dwc_eqos_config_tx_pbl(struct dwc_eqos_prv_data *pdata,
				      unsigned int tx_pbl,
				      unsigned int qinx)
{
	struct hw_if_struct *hw_if = &(pdata->hw_if);
	unsigned int pblx8_val = 0;

	DBGPR("-->dwc_eqos_config_tx_pbl: %d\n", tx_pbl);

	switch (tx_pbl) {
	case DWC_EQOS_PBL_1:
	case DWC_EQOS_PBL_2:
	case DWC_EQOS_PBL_4:
	case DWC_EQOS_PBL_8:
	case DWC_EQOS_PBL_16:
	case DWC_EQOS_PBL_32:
		hw_if->config_tx_pbl_val(pdata, qinx, tx_pbl);
		hw_if->config_pblx8(pdata, qinx, 0);
		break;
	case DWC_EQOS_PBL_64:
	case DWC_EQOS_PBL_128:
	case DWC_EQOS_PBL_256:
		hw_if->config_tx_pbl_val(pdata, qinx, tx_pbl / 8);
		hw_if->config_pblx8(pdata, qinx, 1);
		pblx8_val = 1;
		break;
	}

	switch (pblx8_val) {
	case 0:
		pr_alert("Tx PBL[%d] value: %d\n",
				qinx, hw_if->get_tx_pbl_val(pdata, qinx));
		pr_alert("Rx PBL[%d] value: %d\n",
				qinx, hw_if->get_rx_pbl_val(pdata, qinx));
		break;
	case 1:
		pr_alert("Tx PBL[%d] value: %d\n",
				qinx, (hw_if->get_tx_pbl_val(pdata, qinx) * 8));
		pr_alert("Rx PBL[%d] value: %d\n",
				qinx, (hw_if->get_rx_pbl_val(pdata, qinx) * 8));
		break;
	}

	DBGPR("<--dwc_eqos_config_tx_pbl\n");
}

/* This function is invoked by ioctl function when the user issues an
 * ioctl command to select the DCB algorithm.
 */
static void dwc_eqos_program_dcb_algorithm(struct dwc_eqos_prv_data *pdata,
		struct ifr_data_struct *req)
{
	struct dwc_eqos_dcb_algorithm l_dcb_struct, *u_dcb_struct =
		(struct dwc_eqos_dcb_algorithm *)req->ptr;
	struct hw_if_struct *hw_if = &pdata->hw_if;

	DBGPR("-->dwc_eqos_program_dcb_algorithm\n");

	if (copy_from_user(&l_dcb_struct, u_dcb_struct,
			   sizeof(struct dwc_eqos_dcb_algorithm)))
		pr_alert("Failed to fetch DCB Struct info from user\n");

	hw_if->set_tx_queue_operating_mode(pdata, l_dcb_struct.qinx,
		(unsigned int)l_dcb_struct.op_mode);
	hw_if->set_dcb_algorithm(pdata, l_dcb_struct.algorithm);
	hw_if->set_dcb_queue_weight(pdata, l_dcb_struct.qinx, l_dcb_struct.weight);

	DBGPR("<--dwc_eqos_program_dcb_algorithm\n");

	return;
}

/* This function is invoked by ioctl function when the user issues an
 * ioctl command to select the AVB algorithm. This function also configures other
 * parameters like send and idle slope, high and low credit.
 */
static void dwc_eqos_program_avb_algorithm(struct dwc_eqos_prv_data *pdata,
		struct ifr_data_struct *req)
{
	struct dwc_eqos_avb_algorithm l_avb_struct, *u_avb_struct =
		(struct dwc_eqos_avb_algorithm *)req->ptr;
	struct hw_if_struct *hw_if = &pdata->hw_if;

	DBGPR("-->dwc_eqos_program_avb_algorithm\n");

	if (copy_from_user(&l_avb_struct, u_avb_struct,
				sizeof(struct dwc_eqos_avb_algorithm)))
		pr_alert("Failed to fetch AVB Struct info from user\n");

	hw_if->set_tx_queue_operating_mode(pdata, l_avb_struct.qinx,
		(unsigned int)l_avb_struct.op_mode);
	hw_if->set_avb_algorithm(pdata, l_avb_struct.qinx, l_avb_struct.algorithm);
	hw_if->config_credit_control(pdata, l_avb_struct.qinx, l_avb_struct.cc);
	hw_if->config_send_slope(pdata, l_avb_struct.qinx, l_avb_struct.send_slope);
	hw_if->config_idle_slope(pdata, l_avb_struct.qinx, l_avb_struct.idle_slope);
	hw_if->config_high_credit(pdata, l_avb_struct.qinx, l_avb_struct.hi_credit);
	hw_if->config_low_credit(pdata, l_avb_struct.qinx, l_avb_struct.low_credit);

	DBGPR("<--dwc_eqos_program_avb_algorithm\n");

	return;
}

/* This function is invoked by dwc_eqos_start_xmit and
 * dwc_eqos_tx_clean function for dumping the TX descriptor contents
 * which are prepared for packet transmission and which are transmitted by
 * device. It is mainly used during development phase for debug purpose. Use
 * of these function may affect the performance during normal operation.
 */
void dump_tx_desc(struct dwc_eqos_prv_data *pdata, int first_desc_idx,
		  int last_desc_idx, int flag, unsigned int qinx)
{
	int i;
	struct s_tx_normal_desc *desc = NULL;
	unsigned int varctxt;

	if (first_desc_idx == last_desc_idx) {
		desc = GET_TX_DESC_PTR(qinx, first_desc_idx);

		TX_NORMAL_DESC_TDES3_CTXT_MLF_RD(desc->tdes3, varctxt);

		pr_alert("%s[%02d %4p %03d %s] = %#x:%#x:%#x:%#x\n",
		       (varctxt == 1) ? "TX_CONTXT_DESC" : "TX_NORMAL_DESC",
		       qinx, desc, first_desc_idx,
		       ((flag == 1) ? "QUEUED FOR TRANSMISSION" :
			((flag == 0) ? "FREED/FETCHED BY DEVICE" : "DEBUG DESC DUMP")),
			desc->tdes0, desc->tdes1,
			desc->tdes2, desc->tdes3);
	} else {
		int lp_cnt;
		if (first_desc_idx > last_desc_idx)
			lp_cnt = last_desc_idx + TX_DESC_CNT - first_desc_idx;
		else
			lp_cnt = last_desc_idx - first_desc_idx;

		for (i = first_desc_idx; lp_cnt >= 0; lp_cnt--) {
			desc = GET_TX_DESC_PTR(qinx, i);

			TX_NORMAL_DESC_TDES3_CTXT_MLF_RD(desc->tdes3, varctxt);

			pr_alert("%s[%02d %4p %03d %s] = %#x:%#x:%#x:%#x\n",
			       (varctxt == 1) ?
				"TX_CONTXT_DESC" : "TX_NORMAL_DESC",
				qinx, desc, i,
			       ((flag == 1) ? "QUEUED FOR TRANSMISSION" :
				"FREED/FETCHED BY DEVICE"), desc->tdes0,
			       desc->tdes1, desc->tdes2, desc->tdes3);
			INCR_TX_DESC_INDEX(i, 1);
		}
	}
}

/* This function is invoked by poll function for dumping the
 * RX descriptor contents. It is mainly used during development phase for
 * debug purpose. Use of these function may affect the performance during
 * normal operation
 */
void dump_rx_desc(unsigned int qinx, struct s_rx_normal_desc *desc, int desc_id)
{
	pr_alert("rx_normal_desc[%02d %4p %03d FROM DEVICE] = %#x:%#x:%#x:%#x",
		qinx, desc, desc_id, desc->rdes0, desc->rdes1, desc->rdes2,
		desc->rdes3);
}

/* This function is invoked by start_xmit and poll function for
 * dumping the content of packet to be transmitted by device or received
 * from device. It is mainly used during development phase for debug purpose.
 * Use of these functions may affect the performance during normal operation.
 */
void print_pkt(struct sk_buff *skb, int len, bool tx_rx, int desc_idx)
{
	int i, j = 0;
	unsigned char *buf = skb->data;

	pr_alert("***********************************************************\n");
	pr_alert("%s pkt of %d Bytes [DESC index = %d]\n",
		(tx_rx ? "TX" : "RX"), len, desc_idx);
	pr_alert("Dst MAC addr(6 bytes)\n");

	for (i = 0; i < 6; i++)
		pr_alert("%#.2x%s", buf[i], (((i == 5) ? "" : ":")));

	pr_alert("Src MAC addr(6 bytes)\n");
	for (i = 6; i <= 11; i++)
		pr_info("%#.2x%s", buf[i], (((i == 11) ? "" : ":")));

	i = (buf[12] << 8 | buf[13]);
	pr_alert("Type/Length(2 bytes)\n%#x", i);

	pr_alert("Pay Load : %d bytes\n", (len - 14));
	for (i = 14, j = 1; i < len; i++, j++) {
		pr_info("%#.2x%s", buf[i], (((i == (len - 1)) ? "" : ":")));
		if ((j % 16) == 0)
			pr_alert("");
	}

	pr_alert("*************************************************************\n");
}

/* This function is invoked by probe function. This function will
 * initialize default receive coalesce parameters and sw timer value and store
 * it in respective receive data structure.
 */
void dwc_eqos_init_rx_coalesce(struct dwc_eqos_prv_data *pdata)
{
	struct dwc_eqos_rx_wrapper_descriptor *rx_desc_data = NULL;
	unsigned int i;

	DBGPR("-->dwc_eqos_init_rx_coalesce\n");

	for (i = 0; i < pdata->rx_queue_cnt; i++) {
		rx_desc_data = GET_RX_WRAPPER_DESC(i);

		rx_desc_data->use_riwt = 1;
		rx_desc_data->rx_coal_frames = DWC_EQOS_RX_MAX_FRAMES;
		rx_desc_data->rx_riwt =
			dwc_eqos_usec2riwt(DWC_EQOS_OPTIMAL_DMA_RIWT_USEC,
					      pdata);
	}

	DBGPR("<--dwc_eqos_init_rx_coalesce\n");
}

static void dwc_eqos_tx_timer(unsigned long data)
{
	struct dwc_eqos_tx_queue *tx_q = (struct dwc_eqos_tx_queue *)data;

	dwc_eqos_tx_napi_schedule(tx_q);
}

#define TX_COAL_1MS_TIMER 1000

void dwc_eqos_init_tx_coalesce(struct dwc_eqos_prv_data *pdata)
{
	struct dwc_eqos_tx_wrapper_descriptor *tx_desc_data = NULL;
	unsigned int i;

	DBGPR("-->dwc_eqos_init_tx_coalesce\n");

	pdata->tx_coal_timer_us = TX_COAL_1MS_TIMER;
	for (i = 0; i < pdata->tx_queue_cnt; i++) {
		tx_desc_data = GET_TX_WRAPPER_DESC(i);

		tx_desc_data->tx_coal_max_desc = DWC_EQOS_TX_MAX_COAL_DESC;
		setup_timer(&tx_desc_data->txtimer, dwc_eqos_tx_timer,
					(unsigned long) GET_TX_QUEUE_PTR(i));
	}

	DBGPR("<--dwc_eqos_init_tx_coalesce\n");
}
inline unsigned int dwc_eqos_reg_read(volatile unsigned long *ptr)
{
		return ioread32((void *)ptr);
}

/*!
 * \details This function is invoked by ethtool function when user wants to
 * read MMC counters. This function will read the MMC if supported by core
 * and store it in dwc_eqos_mmc_counters structure. By default all the
 * MMC are programmed "read on reset" hence all the fields of the
 * dwc_eqos_mmc_counters are incremented.
 *
 *
 * \param[in] pdata – pointer to private data structure.
 *
 * \return void
 */
void dwc_eqos_mmc_read(struct dwc_eqos_prv_data *pdata,
			 struct dwc_eqos_mmc_counters *mmc)
{
	DBGPR("-->dwc_eqos_mmc_read\n");

	/* MMC TX counter registers */
	mmc->mmc_tx_octetcount_gb += dwceqos_readl(pdata, DWCEQOS_TX_OCTET_CNT_GOOD_BAD);
	mmc->mmc_tx_framecount_gb += dwceqos_readl(pdata, DWCEQOS_TX_PKT_CNT_GOOD_BAD);
	mmc->mmc_tx_broadcastframe_g += dwceqos_readl(pdata, DWCEQOS_TX_BDT_PKTS_GOOD);
	mmc->mmc_tx_multicastframe_g += dwceqos_readl(pdata, DWCEQOS_TX_MLT_PKTS_GOOD);
	mmc->mmc_tx_64_octets_gb += dwceqos_readl(pdata, DWCEQOS_TX_64_OCTS_PKTS_GB);
	mmc->mmc_tx_65_to_127_octets_gb += dwceqos_readl(pdata, DWCEQOS_TX_65_127_OCTS_PKTS_GB);
	mmc->mmc_tx_128_to_255_octets_gb += dwceqos_readl(pdata, DWCEQOS_TX_128_255_OCTS_PKTS_GB);
	mmc->mmc_tx_256_to_511_octets_gb += dwceqos_readl(pdata, DWCEQOS_TX_256_511_OCTS_PKTS_GB);
	mmc->mmc_tx_512_to_1023_octets_gb += dwceqos_readl(pdata, DWCEQOS_TX_512_1023_OCTS_PKTS_GB);
	mmc->mmc_tx_1024_to_max_octets_gb += dwceqos_readl(pdata, DWCEQOS_TX_1024_MAX_OCTS_PKTS_GB);
	mmc->mmc_tx_unicast_gb += dwceqos_readl(pdata, DWCEQOS_TX_UCT_PKTS_GOOD_BAD);
	mmc->mmc_tx_multicast_gb += dwceqos_readl(pdata, DWCEQOS_TX_MLT_PKTS_GOOD_BAD);
	mmc->mmc_tx_broadcast_gb += dwceqos_readl(pdata, DWCEQOS_TX_BDT_PKTS_GOOD_BAD);
	mmc->mmc_tx_underflow_error += dwceqos_readl(pdata, DWCEQOS_TX_UFLOW_ERR_PKTS);
	mmc->mmc_tx_singlecol_g += dwceqos_readl(pdata, DWCEQOS_TX_SGL_COLLSN_GOOD_PKTS);
	mmc->mmc_tx_multicol_g += dwceqos_readl(pdata, DWCEQOS_TX_MUL_COLLSN_GOOD_PKTS);
	mmc->mmc_tx_deferred += dwceqos_readl(pdata, DWCEQOS_TX_DEF_PKTS);
	mmc->mmc_tx_latecol += dwceqos_readl(pdata, DWCEQOS_TX_LATE_COLLSN_PKTS);
	mmc->mmc_tx_exesscol += dwceqos_readl(pdata, DWCEQOS_TX_EXCESS_COLLSN_PKTS);
	mmc->mmc_tx_carrier_error += dwceqos_readl(pdata, DWCEQOS_TX_CARRIER_ERR_PKTS);
	mmc->mmc_tx_octetcount_g += dwceqos_readl(pdata, DWCEWOS_TX_OCT_CNT_GOOD);
	mmc->mmc_tx_framecount_g += dwceqos_readl(pdata, DWCEQOS_TX_PKT_CNT_GOOD);
	mmc->mmc_tx_excessdef += dwceqos_readl(pdata, DWCEQOS_TX_EXCESS_DEF_ERR);
	mmc->mmc_tx_pause_frame += dwceqos_readl(pdata, DWCEQOS_TX_PAUSE_PKTS);
	mmc->mmc_tx_vlan_frame_g += dwceqos_readl(pdata, DWCEQOS_TX_VLAN_PKTS_GOOD);
	mmc->mmc_tx_osize_frame_g += dwceqos_readl(pdata, DWCEQOS_TX_OSIZE_PKTS_GOOD);

	/* MMC RX counter registers */
	mmc->mmc_rx_framecount_gb += dwceqos_readl(pdata, DWCEQOS_RX_PKTS_CNT_GOOD_BAD);
	mmc->mmc_rx_octetcount_gb += dwceqos_readl(pdata, DWCEQOS_RX_OCTET_CNT_GOOD_BAD);
	mmc->mmc_rx_octetcount_g += dwceqos_readl(pdata, DWCEWOS_RX_OCT_CNT_GOOD);
	mmc->mmc_rx_broadcastframe_g += dwceqos_readl(pdata, DWCEQOS_RX_BDT_PKTS_GOOD);
	mmc->mmc_rx_multicastframe_g += dwceqos_readl(pdata, DWCEQOS_RX_MLT_PKTS_GOOD);
	mmc->mmc_rx_crc_errror += dwceqos_readl(pdata, DWCEQOS_RX_CRC_ERR_PKTS);
	mmc->mmc_rx_align_error += dwceqos_readl(pdata, DWCEQOS_RX_ALGMNT_ERR_PKTS);
	mmc->mmc_rx_run_error += dwceqos_readl(pdata, DWCEQOS_RX_RUNT_ERR_PKTS);
	mmc->mmc_rx_jabber_error += dwceqos_readl(pdata, DWCEQOS_RX_JABBER_ERR_PKTS);
	mmc->mmc_rx_undersize_g += dwceqos_readl(pdata, DWCEQOS_RX_USIZE_PKTS_GOOD);
	mmc->mmc_rx_oversize_g += dwceqos_readl(pdata, DWCEQOS_RX_OSIZE_PKTS_GOOD);
	mmc->mmc_rx_64_octets_gb += dwceqos_readl(pdata, DWCEQOS_RX_64_OCTS_PKTS_GB);
	mmc->mmc_rx_65_to_127_octets_gb += dwceqos_readl(pdata, DWCEQOS_RX_65_127_OCTS_PKTS_GB);
	mmc->mmc_rx_128_to_255_octets_gb += dwceqos_readl(pdata, DWCEQOS_RX_128_255_OCTS_PKTS_GB);
	mmc->mmc_rx_256_to_511_octets_gb += dwceqos_readl(pdata, DWCEQOS_RX_256_511_OCTS_PKTS_GB);
	mmc->mmc_rx_512_to_1023_octets_gb += dwceqos_readl(pdata, DWCEQOS_RX_512_1023_OCTS_PKTS_GB);
	mmc->mmc_rx_1024_to_max_octets_gb += dwceqos_readl(pdata, DWCEQOS_RX_1024_MAX_OCTS_PKTS_GB);
	mmc->mmc_rx_unicast_g += dwceqos_readl(pdata, DWCEQOS_RX_UCT_PKTS_GOOD);
	mmc->mmc_rx_length_error += dwceqos_readl(pdata, DWCEQOS_RX_LEN_ERR_PKTS);
	mmc->mmc_rx_outofrangetype += dwceqos_readl(pdata, DWCEQOS_RX_OUT_OF_RNG_TYPE_PKTS);
	mmc->mmc_rx_pause_frames += dwceqos_readl(pdata, DWCEQOS_RX_PAUSE_PKTS);
	mmc->mmc_rx_fifo_overflow += dwceqos_readl(pdata, DWCEQOS_RX_FIFO_OFLOW_PKTS);
	mmc->mmc_rx_vlan_frames_gb += dwceqos_readl(pdata, DWCEQOS_RX_VLAN_PKTS_GOOD_BAD);
	mmc->mmc_rx_watchdog_error += dwceqos_readl(pdata, DWCEQOS_RX_WDOG_ERR_PKTS);
	mmc->mmc_rx_receive_error += dwceqos_readl(pdata, DWCEQOS_RX_RECEIVE_ERR_PKTS);
	mmc->mmc_rx_ctrl_frames_g += dwceqos_readl(pdata, DWCEQOS_RX_CTRL_PKTS_GOOD);

	/* IPC */
	mmc->mmc_rx_ipc_intr_mask += dwceqos_readl(pdata, DWCEQOS_MMC_IPC_RX_INTR_MASK);
	mmc->mmc_rx_ipc_intr += dwceqos_readl(pdata, DWCEQOS_MMC_IPC_RX_INTR);

	/* IPv4 */
	mmc->mmc_rx_ipv4_gd += dwceqos_readl(pdata, DWCEQOS_RXIPV4_GOOD_PKTS);
	mmc->mmc_rx_ipv4_hderr += dwceqos_readl(pdata, DWCEQOS_RXIPV4_HDR_ERR_PKTS);
	mmc->mmc_rx_ipv4_nopay += dwceqos_readl(pdata, DWCEQOS_RXIPV4_NO_PAYLOAD_PKTS);
	mmc->mmc_rx_ipv4_frag += dwceqos_readl(pdata, DWCEQOS_RXIPV4_FRAGMENTED_PKTS);
	mmc->mmc_rx_ipv4_udsbl += dwceqos_readl(pdata, DWCEQOS_RXIPV4_UDP_CKSM_DSBLD_PKT);

	/* IPV6 */
	mmc->mmc_rx_ipv6_gd += dwceqos_readl(pdata, DWCEQOS_RXIPV6_GOOD_PKTS);
	mmc->mmc_rx_ipv6_hderr += dwceqos_readl(pdata, DWCEQOS_RXIPV6_HDR_ERR_PKTS);
	mmc->mmc_rx_ipv6_nopay += dwceqos_readl(pdata, DWCEQOS_RXIPV6_NO_PAYLOAD_PKTS);

	/* Protocols */
	mmc->mmc_rx_udp_gd += dwceqos_readl(pdata, DWCEQOS_RXUDP_GOOD_PKTS);
	mmc->mmc_rx_udp_err += dwceqos_readl(pdata, DWCEQOS_RXUDP_ERR_PKT);
	mmc->mmc_rx_tcp_gd += dwceqos_readl(pdata, DWCEQOS_RXTCP_GOOD_PKTS);
	mmc->mmc_rx_tcp_err += dwceqos_readl(pdata, DWCEQOS_RXTCP_ERR_PKTS);
	mmc->mmc_rx_icmp_gd += dwceqos_readl(pdata, DWCEQOS_RXICMP_GOOD_PKTS);
	mmc->mmc_rx_icmp_err += dwceqos_readl(pdata, DWCEQOS_RXICMP_ERR_PKTS);

	/* IPv4 */
	mmc->mmc_rx_ipv4_gd_octets += dwceqos_readl(pdata, DWCEQOS_RXIPV4_GOOD_OCTS);
	mmc->mmc_rx_ipv4_hderr_octets += dwceqos_readl(pdata, DWCEQOS_RXIPV4_HDR_ERR_OCTS);
	mmc->mmc_rx_ipv4_nopay_octets += dwceqos_readl(pdata, DWCEQOS_RXIPV4_NO_PAYLOAD_OCTS);
	mmc->mmc_rx_ipv4_frag_octets += dwceqos_readl(pdata, DWCEQOS_RXIPV4_FRAGMENTED_OCTS);
	mmc->mmc_rx_ipv4_udsbl_octets += dwceqos_readl(pdata, DWCEQOS_RXIPV4_UDP_CKSM_DSBL_OCTS);

	/* IPV6 */
	mmc->mmc_rx_ipv6_gd_octets += dwceqos_readl(pdata, DWCEQOS_RXIPV6_GOOD_OCTS);
	mmc->mmc_rx_ipv6_hderr_octets += dwceqos_readl(pdata, DWCEQOS_RXIPV6_HDR_ERR_OCTS);
	mmc->mmc_rx_ipv6_nopay_octets += dwceqos_readl(pdata, DWCEQOS_RXIPV6_NO_PAYLOAD_OCTS);

	/* Protocols */
	mmc->mmc_rx_udp_gd_octets += dwceqos_readl(pdata, DWCEQOS_RXUDP_GOOD_OCTS);
	mmc->mmc_rx_udp_err_octets += dwceqos_readl(pdata, DWCEQOS_RXUDP_ERR_OCTS);
	mmc->mmc_rx_tcp_gd_octets += dwceqos_readl(pdata, DWCEQOS_RXTCP_GOOD_OCTS);
	mmc->mmc_rx_tcp_err_octets += dwceqos_readl(pdata, DWCEQOS_RXTCP_ERR_OCTS);
	mmc->mmc_rx_icmp_gd_octets += dwceqos_readl(pdata, DWCEQOS_RXICMP_GOOD_OCTS);
	mmc->mmc_rx_icmp_err_octets += dwceqos_readl(pdata, DWCEQOS_RXICMP_ERR_OCTS);

	DBGPR("<--dwc_eqos_mmc_read\n");
}

phy_interface_t dwc_eqos_get_phy_interface(struct dwc_eqos_prv_data *pdata)
{
	phy_interface_t ret = PHY_INTERFACE_MODE_MII;

	DBGPR("-->dwc_eqos_get_phy_interface\n");

	if (pdata->hw_feat.act_phy_sel == DWC_EQOS_GMII_MII) {
		if (pdata->hw_feat.gmii_sel)
			ret = PHY_INTERFACE_MODE_GMII;
		else if (pdata->hw_feat.mii_sel)
			ret = PHY_INTERFACE_MODE_MII;
	} else if (pdata->hw_feat.act_phy_sel == DWC_EQOS_RGMII) {
		ret = PHY_INTERFACE_MODE_RGMII;
	} else if (pdata->hw_feat.act_phy_sel == DWC_EQOS_SGMII) {
		ret = PHY_INTERFACE_MODE_SGMII;
	} else if (pdata->hw_feat.act_phy_sel == DWC_EQOS_TBI) {
		ret = PHY_INTERFACE_MODE_TBI;
	} else if (pdata->hw_feat.act_phy_sel == DWC_EQOS_RMII) {
		ret = PHY_INTERFACE_MODE_RMII;
	} else if (pdata->hw_feat.act_phy_sel == DWC_EQOS_RTBI) {
		ret = PHY_INTERFACE_MODE_RTBI;
	} else if (pdata->hw_feat.act_phy_sel == DWC_EQOS_SMII) {
		ret = PHY_INTERFACE_MODE_SMII;
	} else if (pdata->hw_feat.act_phy_sel == DWC_EQOS_REVMII) {
		//what to return ?
	} else {
		pr_alert("Missing interface support between PHY and MAC\n");
		ret = PHY_INTERFACE_MODE_NA;
	}

	DBGPR("<--dwc_eqos_get_phy_interface\n");

	return ret;
}
