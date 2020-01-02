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
#include "dwc_eth_qos_yapphdr.h"

#include "dwc_eth_qos_yregacc.h"

/* This sequence is used to enable/disable MAC loopback mode
 */
static int config_mac_loopback_mode(struct net_device *dev, unsigned int enb_dis)
{
	struct dwc_eqos_prv_data *pdata = netdev_priv(dev);
	u32 regval;

	regval = dwceqos_readl(pdata, DWCEQOS_MAC_CFG);
	regval &= ~(DWCEQOS_MAC_CFG_LM);

	if (enb_dis)
		regval |= DWCEQOS_MAC_CFG_LM;

	dwceqos_writel(pdata, DWCEQOS_MAC_CFG, regval);

	return 0;
}

/* enable/disable PFC(Priority Based Flow Control)
 */
static void config_pfc(struct dwc_eqos_prv_data *pdata, int enb_dis)
{
	u32 regval;

	regval = dwceqos_readl(pdata, DWCEQOS_MAC_RX_FLOW_CTRL);
	regval &= ~(DWCEQOS_MAC_RX_CTRL_PFCE);

	if (enb_dis)
		regval |= DWCEQOS_MAC_RX_CTRL_PFCE;

	dwceqos_writel(pdata, DWCEQOS_MAC_RX_FLOW_CTRL, regval);
}

/* This sequence is used to configure mac double vlan processing feature.
 */
static int config_tx_outer_vlan(struct dwc_eqos_prv_data *pdata,
				unsigned int op_type, unsigned int outer_vlt)
{
	u32 regval;

	pr_info("--> config_tx_outer_vlan()\n");

	regval = dwceqos_readl(pdata, DWCEQOS_MAC_VLAN_INCL);
	regval &= ~(DWCEQOS_MAC_VLAN_INCL_VLTI);
	regval &= ~(DWCEQOS_MAC_VLAN_INCL_VLT(0xffff));
	regval |= DWCEQOS_MAC_VLAN_INCL_VLT(outer_vlt);
	regval &= ~(DWCEQOS_MAC_VLAN_INCL_VLP);
	regval |= DWCEQOS_MAC_VLAN_INCL_VLP;
	regval &= ~(DWCEQOS_MAC_VLAN_INCL_VLC(0x3));
	regval |= DWCEQOS_MAC_VLAN_INCL_VLC(op_type);
	dwceqos_writel(pdata, DWCEQOS_MAC_VLAN_INCL, regval);

	if (op_type == DWC_EQOS_DVLAN_NONE) {
		regval = dwceqos_readl(pdata, DWCEQOS_MAC_VLAN_INCL);
		regval &= ~(DWCEQOS_MAC_VLAN_INCL_VLP);
		regval &= ~(DWCEQOS_MAC_VLAN_INCL_VLT(0xffff));
		dwceqos_writel(pdata, DWCEQOS_MAC_VLAN_INCL, regval);
	}

	pr_info("<-- config_tx_outer_vlan()\n");

	return 0;
}

static int config_tx_inner_vlan(struct dwc_eqos_prv_data *pdata,
				unsigned int op_type, unsigned int inner_vlt)
{
	u32 regval;

	pr_info("--> config_tx_inner_vlan()\n");

	regval = dwceqos_readl(pdata, DWCEQOS_MAC_INNER_VLAN_INCL);
	regval &= ~(DWCEQOS_MAC_IVLAN_INCL_VLTI);
	regval &= ~(DWCEQOS_MAC_IVLAN_INCL_VLT(0xffff));
	regval |= DWCEQOS_MAC_IVLAN_INCL_VLT(inner_vlt);
	regval &= ~(DWCEQOS_MAC_IVLAN_INCL_VLP);
	regval |= DWCEQOS_MAC_IVLAN_INCL_VLP;
	regval &= ~(DWCEQOS_MAC_IVLAN_INCL_VLC(0x3));
	regval |= DWCEQOS_MAC_IVLAN_INCL_VLC(op_type);
	dwceqos_writel(pdata, DWCEQOS_MAC_INNER_VLAN_INCL, regval);

	if (op_type == DWC_EQOS_DVLAN_NONE) {
		regval = dwceqos_readl(pdata, DWCEQOS_MAC_INNER_VLAN_INCL);
		regval &= ~(DWCEQOS_MAC_IVLAN_INCL_VLP);
		regval &= ~(DWCEQOS_MAC_IVLAN_INCL_VLT(0xffff));
		dwceqos_writel(pdata, DWCEQOS_MAC_INNER_VLAN_INCL, regval);
	}

	pr_info("<-- config_tx_inner_vlan()\n");

	return 0;
}

static int config_svlan(struct dwc_eqos_prv_data *pdata, unsigned int flags)
{
	u32 regval;

	pr_info("--> config_svlan()\n");

	regval = dwceqos_readl(pdata, DWCEQOS_MAC_VLAN_TAG_CTRL);
	regval |= DWCEQOS_MAC_VLAN_TAG_CTRL_ESVL;
	dwceqos_writel(pdata, DWCEQOS_MAC_VLAN_TAG_CTRL, regval);
	if (flags == DWC_EQOS_DVLAN_NONE) {
		regval = dwceqos_readl(pdata, DWCEQOS_MAC_VLAN_TAG_CTRL);
		regval &= ~(DWCEQOS_MAC_VLAN_TAG_CTRL_ESVL);
		dwceqos_writel(pdata, DWCEQOS_MAC_VLAN_TAG_CTRL, regval);
		regval = dwceqos_readl(pdata, DWCEQOS_MAC_VLAN_INCL);
		regval &= ~(DWCEQOS_MAC_VLAN_INCL_CSVL);
		dwceqos_writel(pdata, DWCEQOS_MAC_VLAN_INCL, regval);
		regval = dwceqos_readl(pdata, DWCEQOS_MAC_INNER_VLAN_INCL);
		regval &= ~(DWCEQOS_MAC_IVLAN_INCL_CSVL);
		dwceqos_writel(pdata, DWCEQOS_MAC_INNER_VLAN_INCL, regval);
	} else if (flags == DWC_EQOS_DVLAN_INNER) {
		regval = dwceqos_readl(pdata, DWCEQOS_MAC_INNER_VLAN_INCL);
		regval |= DWCEQOS_MAC_IVLAN_INCL_CSVL;
		dwceqos_writel(pdata, DWCEQOS_MAC_INNER_VLAN_INCL, regval);
	} else if (flags == DWC_EQOS_DVLAN_OUTER) {
		regval = dwceqos_readl(pdata, DWCEQOS_MAC_VLAN_INCL);
		regval |= DWCEQOS_MAC_VLAN_INCL_CSVL;
		dwceqos_writel(pdata, DWCEQOS_MAC_VLAN_INCL, regval);
	} else if (flags == DWC_EQOS_DVLAN_BOTH) {
		regval = dwceqos_readl(pdata, DWCEQOS_MAC_INNER_VLAN_INCL);
		regval |= DWCEQOS_MAC_IVLAN_INCL_CSVL;
		dwceqos_writel(pdata, DWCEQOS_MAC_INNER_VLAN_INCL, regval);
		regval = dwceqos_readl(pdata, DWCEQOS_MAC_VLAN_INCL);
		regval |= DWCEQOS_MAC_VLAN_INCL_CSVL;
		dwceqos_writel(pdata, DWCEQOS_MAC_VLAN_INCL, regval);
	} else {
		pr_alert("ERROR : double VLAN enable SVLAN configuration - Invalid argument");
		return -1;
	}

	pr_info("<-- config_svlan()\n");

	return 0;
}

static void config_dvlan(struct dwc_eqos_prv_data *pdata, bool enb_dis)
{
	u32 regval;

	regval = dwceqos_readl(pdata, DWCEQOS_MAC_VLAN_TAG_CTRL);
	if (!enb_dis)
		regval &= ~(DWCEQOS_MAC_VLAN_TAG_CTRL_EDVLP);
	else
		regval |= DWCEQOS_MAC_VLAN_TAG_CTRL_EDVLP;

	dwceqos_writel(pdata, DWCEQOS_MAC_VLAN_TAG_CTRL, regval);
}

static void vlan_tag_ctrl_wait_on_ob(struct dwc_eqos_prv_data *pdata)
{
	int retries;
	u32 regval;

	/* Retry for 100msecs. For now, panic on timeout.
	 * Most-likely MAC is not in a good state to continue.
	 */
	for (retries = 0; retries < 1000; retries++) {
		regval = dwceqos_readl(pdata, DWCEQOS_MAC_VLAN_TAG_CTRL);
		if (!(regval & DWCEQOS_MAC_VLAN_TAG_CTRL_OB))
			return;
		udelay(100);
	}

	panic("%s timedout on MAC Vlan TAG Ctrl", pdata->ndev->name);
}

static int vlan_tag_ctrl_set_cmd_rw(struct dwc_eqos_prv_data *pdata,
				    int offset, int rw)
{
	u32 regval;

	regval = dwceqos_readl(pdata, DWCEQOS_MAC_VLAN_TAG_CTRL);
	regval &= ~(DWCEQOS_MAC_VLAN_TAG_CTRL_OFS(0x1f));
	regval |= DWCEQOS_MAC_VLAN_TAG_CTRL_OB;
	if (rw == 0)
		regval &= ~(DWCEQOS_MAC_VLAN_TAG_CTRL_CT);
	else
		regval |= DWCEQOS_MAC_VLAN_TAG_CTRL_CT;
	regval |= DWCEQOS_MAC_VLAN_TAG_CTRL_OFS(offset);
	dwceqos_writel(pdata, DWCEQOS_MAC_VLAN_TAG_CTRL, regval);
	vlan_tag_ctrl_wait_on_ob(pdata);

	return 0;
}

static unsigned int get_vlan_tag_comparison(struct net_device *dev)
{
	struct dwc_eqos_prv_data *pdata = netdev_priv(dev);
	u32 regval;

	regval = dwceqos_readl(pdata, DWCEQOS_MAC_VLAN_TAG_CTRL);
	regval = ((regval >> 16) & 0x1);

	return regval;
}

/* This sequence is used to enable/disable VLAN filtering and
 * also selects VLAN filtering mode- perfect/hash
 */
static int config_vlan_filtering(struct net_device *dev, int filt_en,
				 int perf_hash_filt, int perf_inv_match)
{
	struct dwc_eqos_prv_data *pdata = netdev_priv(dev);
	u32 regval;

	regval = dwceqos_readl(pdata, DWCEQOS_MAC_PKT_FILT);
	if (filt_en)
		regval |= DWCEQOS_MAC_PKT_FILT_VTFE;
	else
		regval &= ~(DWCEQOS_MAC_PKT_FILT_VTFE);
	dwceqos_writel(pdata, DWCEQOS_MAC_PKT_FILT, regval);

	regval = dwceqos_readl(pdata, DWCEQOS_MAC_VLAN_TAG_CTRL);
	if (perf_inv_match)
		regval |= DWCEQOS_MAC_VLAN_TAG_CTRL_VTIM;
	else
		regval &= ~(DWCEQOS_MAC_VLAN_TAG_CTRL_VTIM);
	dwceqos_writel(pdata, DWCEQOS_MAC_VLAN_TAG_CTRL, regval);

	regval = dwceqos_readl(pdata, DWCEQOS_MAC_VLAN_TAG_CTRL);
	if (perf_hash_filt)
		regval |= DWCEQOS_MAC_VLAN_TAG_CTRL_VTHM;
	else
		regval &= ~(DWCEQOS_MAC_VLAN_TAG_CTRL_VTHM);
	dwceqos_writel(pdata, DWCEQOS_MAC_VLAN_TAG_CTRL, regval);

	/* To enable only HASH filtering then VL/VID
	 * should be > zero. Hence we are writing 1 into VL.
	 * It also means that MAC will always receive VLAN pkt with
	 * VID = 1 if inverse march is not set.
	 */
	if (perf_hash_filt) {
		vlan_tag_ctrl_wait_on_ob(pdata);
		vlan_tag_ctrl_set_cmd_rw(pdata, 0, 1);
		regval = dwceqos_readl(pdata, DWCEQOS_MAC_VLAN_TAG_FILTER);
		regval &= ~(DWCEQOS_MAC_VLAN_TAG_FILTER_VID(0xffff));
		regval |= DWCEQOS_MAC_VLAN_TAG_FILTER_VID(0x1);
		dwceqos_writel(pdata, DWCEQOS_MAC_VLAN_TAG_FILTER, regval);
		vlan_tag_ctrl_set_cmd_rw(pdata, 0, 0);

		/* By default enable MAC to calculate vlan hash on
		 * only 12-bits of received VLAN tag (ie only on
		 * VLAN id and ignore priority and other fields)
		 */
		regval = dwceqos_readl(pdata, DWCEQOS_MAC_VLAN_TAG_CTRL);
		regval |= DWCEQOS_MAC_VLAN_TAG_CTRL_ETV;
		dwceqos_writel(pdata, DWCEQOS_MAC_VLAN_TAG_CTRL, regval);
	}

	if (!perf_hash_filt) {
		unsigned long v;

		vlan_tag_ctrl_wait_on_ob(pdata);
		vlan_tag_ctrl_set_cmd_rw(pdata, 0, 1);
		v = dwceqos_readl(pdata, DWCEQOS_MAC_VLAN_TAG_FILTER);
		v |= DWCEQOS_MAC_VLAN_TAG_FILTER_VEN;
		v |= DWCEQOS_MAC_VLAN_TAG_FILTER_ETV;
		v |= DWCEQOS_MAC_VLAN_TAG_FILTER_DOVLTC;
		dwceqos_writel(pdata, DWCEQOS_MAC_VLAN_TAG_FILTER, v);
		vlan_tag_ctrl_set_cmd_rw(pdata, 0, 0);
	}

	return 0;
}

/* This sequence is used to update the VLAN ID for perfect filtering
 */
static int update_vlan_id(struct net_device *dev, unsigned short vid)
{
	struct dwc_eqos_prv_data *pdata = netdev_priv(dev);
	u32 regval;

	vlan_tag_ctrl_wait_on_ob(pdata);
	vlan_tag_ctrl_set_cmd_rw(pdata, 0, 1);
	regval = dwceqos_readl(pdata, DWCEQOS_MAC_VLAN_TAG_FILTER);
	regval &= ~(DWCEQOS_MAC_VLAN_TAG_FILTER_VID(0xffff));
	regval |= DWCEQOS_MAC_VLAN_TAG_FILTER_VID(vid);
	dwceqos_writel(pdata, DWCEQOS_MAC_VLAN_TAG_FILTER, regval);
	vlan_tag_ctrl_set_cmd_rw(pdata, 0, 0);

	return 0;
}

/*This sequence is used to update the VLAN Hash Table reg with new VLAN ID
 */
static int update_vlan_hash_table_reg(struct net_device *dev, unsigned short data)
{
	struct dwc_eqos_prv_data *pdata = netdev_priv(dev);
	u32 regval = 0;

	regval &= ~(DWCEQOS_MAC_VLAN_HT_VLHT(0xffff));
	regval |= DWCEQOS_MAC_VLAN_HT_VLHT(data);
	dwceqos_writel(pdata, DWCEQOS_MAC_VLAN_HT, regval);

	return 0;
}

/* This sequence is used to get the content of VLAN Hash Table reg
 */
static int get_vlan_hash_table_reg(struct net_device *dev)
{
	struct dwc_eqos_prv_data *pdata = netdev_priv(dev);
	u32 regval;

	regval = dwceqos_readl(pdata, DWCEQOS_MAC_VLAN_HT);
	return GET_VALUE(regval, MAC_VLAN_HT_VLHT_LPOS, MAC_VLAN_HT_VLHT_HPOS);
}

/* This sequence is used to update Destination Port Number for
 * L4(TCP/UDP) layer filtering
 */
static int update_l4_da_port_no(struct net_device *dev, int f_no,
				unsigned short port_no)
{
	struct dwc_eqos_prv_data *pdata = netdev_priv(dev);
	u32 regval;

	regval = dwceqos_readl(pdata, DWCEQOS_MAC_L4_ADDR(f_no));
	regval &= ~(DWCEQOS_MAC_L4_AR_L4DP0(0xFFFF));
	regval |= (DWCEQOS_MAC_L4_AR_L4DP0(port_no));

	dwceqos_writel(pdata, DWCEQOS_MAC_L4_ADDR(f_no), regval);
	return 0;
}

/* This sequence is used to update Source Port Number for
 * L4(TCP/UDP) layer filtering
 */
static int update_l4_sa_port_no(struct net_device *dev, int f_no,
				unsigned short port_no)
{
	struct dwc_eqos_prv_data *pdata = netdev_priv(dev);
	u32 regval;

	regval = dwceqos_readl(pdata, DWCEQOS_MAC_L4_ADDR(f_no));
	regval &= ~(DWCEQOS_MAC_L4_AR_L4SP0(0xFFFF));
	regval |= (DWCEQOS_MAC_L4_AR_L4SP0(port_no));

	dwceqos_writel(pdata, DWCEQOS_MAC_L4_ADDR(f_no), regval);
	return 0;
}

/* This sequence is used to configure L4(TCP/UDP) filters for
 * SA and DA Port Number matching
 */
static int config_l4_filters(struct net_device *dev, int f_no,
			     int endis, int is_udp, int src_dest_port_match,
			     int perf_inv)
{
	struct dwc_eqos_prv_data *pdata = netdev_priv(dev);
	u32 val = 0;
	val = dwceqos_readl(pdata, DWCEQOS_MAC_L3L4_CTRL(f_no));

	if (is_udp)
		val |= (DWCEQOS_MAC_L3L4_CR_L4PEN0);
	else
		val &= ~(DWCEQOS_MAC_L3L4_CR_L4PEN0);

	dwceqos_writel(pdata, DWCEQOS_MAC_L3L4_CTRL(f_no), val);

	if (!src_dest_port_match) {
		if (endis) {
			/* Enable L4 filters for source port no matching */
			val = dwceqos_readl(pdata, DWCEQOS_MAC_L3L4_CTRL(f_no));
			val |= (DWCEQOS_MAC_L3L4_CR_L4SPM0);
			dwceqos_writel(pdata, DWCEQOS_MAC_L3L4_CTRL(f_no), val);

			val = dwceqos_readl(pdata, DWCEQOS_MAC_L3L4_CTRL(f_no));
			if (perf_inv)
				val |= (DWCEQOS_MAC_L3L4_CR_L4SPIM0);
			else
				val &= ~(DWCEQOS_MAC_L3L4_CR_L4SPIM0);
			dwceqos_writel(pdata, DWCEQOS_MAC_L3L4_CTRL(f_no), val);
		} else {
			/* Disable L4 filters for source port no matching */
			val = dwceqos_readl(pdata, DWCEQOS_MAC_L3L4_CTRL(f_no));
			val &= ~(DWCEQOS_MAC_L3L4_CR_L4SPM0);
			dwceqos_writel(pdata, DWCEQOS_MAC_L3L4_CTRL(f_no), val);

			val = dwceqos_readl(pdata, DWCEQOS_MAC_L3L4_CTRL(f_no));
			val &= ~(DWCEQOS_MAC_L3L4_CR_L4SPIM0);
			dwceqos_writel(pdata, DWCEQOS_MAC_L3L4_CTRL(f_no), val);
		}
	} else {
		if (endis) {
			/* Enable L4 filters for destination port no matching */
			val = dwceqos_readl(pdata, DWCEQOS_MAC_L3L4_CTRL(f_no));
			val |= (DWCEQOS_MAC_L3L4_CR_L4DPM0);
			dwceqos_writel(pdata, DWCEQOS_MAC_L3L4_CTRL(f_no), val);

			val = dwceqos_readl(pdata, DWCEQOS_MAC_L3L4_CTRL(f_no));
			if (perf_inv)
				val |= (DWCEQOS_MAC_L3L4_CR_L4DPIM0);
			else
				val &= ~(DWCEQOS_MAC_L3L4_CR_L4DPIM0);
			dwceqos_writel(pdata, DWCEQOS_MAC_L3L4_CTRL(f_no), val);
		} else {
			/* Disable L4 filters for destination port no matching */
			val = dwceqos_readl(pdata, DWCEQOS_MAC_L3L4_CTRL(f_no));
			val &= ~(DWCEQOS_MAC_L3L4_CR_L4DPM0);
			dwceqos_writel(pdata, DWCEQOS_MAC_L3L4_CTRL(f_no), val);

			val = dwceqos_readl(pdata, DWCEQOS_MAC_L3L4_CTRL(f_no));
			val &= ~(DWCEQOS_MAC_L3L4_CR_L4DPIM0);
			dwceqos_writel(pdata, DWCEQOS_MAC_L3L4_CTRL(f_no), val);
		}
	}

	return 0;
}

/* This sequence is used to update IPv6 source/destination
 * Address for L3 layer filtering
 */
static int update_ip6_addr(struct net_device *dev, int f_no,
			   unsigned short addr[])
{
	struct dwc_eqos_prv_data *pdata = netdev_priv(dev);
	u32 regval;

	/* update Bits[31:0] of 128-bit IP addr */
	regval = (addr[7] | (addr[6] << 16));
	dwceqos_writel(pdata, DWCEQOS_MAC_L3_ADDR0(f_no), regval);
	/* update Bits[63:32] of 128-bit IP addr */
	regval = (addr[5] | (addr[4] << 16));
	dwceqos_writel(pdata, DWCEQOS_MAC_L3_ADDR1(f_no), regval);
	/* update Bits[95:64] of 128-bit IP addr */
	regval = (addr[3] | (addr[2] << 16));
	dwceqos_writel(pdata, DWCEQOS_MAC_L3_ADDR2(f_no), regval);
	/* update Bits[127:96] of 128-bit IP addr */
	regval = (addr[1] | (addr[0] << 16));
	dwceqos_writel(pdata, DWCEQOS_MAC_L3_ADDR3(f_no), regval);

	return 0;
}

/* This sequence is used to update IPv4 destination
 * Address for L3 layer filtering
 */
static int update_ip4_addr1(struct net_device *dev, int f_no,
			    unsigned char addr[])
{
	struct dwc_eqos_prv_data *pdata = netdev_priv(dev);
	u32 regval;

	regval = (addr[3] | (addr[2] << 8) | (addr[1] << 16) | (addr[0] << 24));
	dwceqos_writel(pdata, DWCEQOS_MAC_L3_ADDR1(f_no), regval);

	return 0;
}

/* This sequence is used to update IPv4 source
 * Address for L3 layer filtering
 */
static int update_ip4_addr0(struct net_device *dev, int f_no,
			    unsigned char addr[])
{
	struct dwc_eqos_prv_data *pdata = netdev_priv(dev);
	u32 regval;

	regval = (addr[3] | (addr[2] << 8) | (addr[1] << 16) | (addr[0] << 24));
	dwceqos_writel(pdata, DWCEQOS_MAC_L3_ADDR0(f_no), regval);

	return 0;
}

/* This sequence is used to configure L3(IPv4/IPv6) filters
 * for SA/DA Address matching
 */
static void enable_ipv6_src_l3_filter(struct dwc_eqos_prv_data *dev, int f_no,
				      int perf_inv)
{
	u32 val;

	/* Enable L3 filters for IPv6 Source addr matching */
	val = dwceqos_readl(dev, DWCEQOS_MAC_L3L4_CTRL(f_no));
	val |= (DWCEQOS_MAC_L3L4_CR_L3SAM0);
	dwceqos_writel(dev, DWCEQOS_MAC_L3L4_CTRL(f_no), val);

	val = dwceqos_readl(dev, DWCEQOS_MAC_L3L4_CTRL(f_no));
	if (perf_inv)
		val |= (DWCEQOS_MAC_L3L4_CR_L3SAIM0);
	else
		val &= ~(DWCEQOS_MAC_L3L4_CR_L3SAIM0);
	dwceqos_writel(dev, DWCEQOS_MAC_L3L4_CTRL(f_no), val);

	val = dwceqos_readl(dev, DWCEQOS_MAC_L3L4_CTRL(f_no));
	val &= ~(DWCEQOS_MAC_L3L4_CR_L3DAM0);
	dwceqos_writel(dev, DWCEQOS_MAC_L3L4_CTRL(f_no), val);

	val = dwceqos_readl(dev, DWCEQOS_MAC_L3L4_CTRL(f_no));
	val &= ~(DWCEQOS_MAC_L3L4_CR_L3DAIM0);
	dwceqos_writel(dev, DWCEQOS_MAC_L3L4_CTRL(f_no), val);
}

static void enable_ipv6_dest_l3_filter(struct dwc_eqos_prv_data *dev, int f_no,
				       int perf_inv)
{
	u32 val;

	/* Enable L3 filters for IPv6 Destination addr matching */
	val = dwceqos_readl(dev, DWCEQOS_MAC_L3L4_CTRL(f_no));
	val &= ~(DWCEQOS_MAC_L3L4_CR_L3SAM0);
	dwceqos_writel(dev, DWCEQOS_MAC_L3L4_CTRL(f_no), val);

	val = dwceqos_readl(dev, DWCEQOS_MAC_L3L4_CTRL(f_no));
	val &= ~(DWCEQOS_MAC_L3L4_CR_L3SAIM0);
	dwceqos_writel(dev, DWCEQOS_MAC_L3L4_CTRL(f_no), val);

	val = dwceqos_readl(dev, DWCEQOS_MAC_L3L4_CTRL(f_no));
	val |= (DWCEQOS_MAC_L3L4_CR_L3DAM0);
	dwceqos_writel(dev, DWCEQOS_MAC_L3L4_CTRL(f_no), val);

	val = dwceqos_readl(dev, DWCEQOS_MAC_L3L4_CTRL(f_no));
	if (perf_inv)
		val |= (DWCEQOS_MAC_L3L4_CR_L3DAIM0);
	else
		val &= ~(DWCEQOS_MAC_L3L4_CR_L3DAIM0);
	dwceqos_writel(dev, DWCEQOS_MAC_L3L4_CTRL(f_no), val);
}

static void disable_ipv6_l3_filter(struct dwc_eqos_prv_data *dev, int f_no)
{
	u32 val;

	/* Disable L3 filters for IPv6 Source/Destination addr matching */
	val = dwceqos_readl(dev, DWCEQOS_MAC_L3L4_CTRL(f_no));
	val &= ~(DWCEQOS_MAC_L3L4_CR_L3SAM0);
	dwceqos_writel(dev, DWCEQOS_MAC_L3L4_CTRL(f_no), val);

	val = dwceqos_readl(dev, DWCEQOS_MAC_L3L4_CTRL(f_no));
	val &= ~(DWCEQOS_MAC_L3L4_CR_L3SAIM0);
	dwceqos_writel(dev, DWCEQOS_MAC_L3L4_CTRL(f_no), val);

	val = dwceqos_readl(dev, DWCEQOS_MAC_L3L4_CTRL(f_no));
	val &= ~(DWCEQOS_MAC_L3L4_CR_L3DAM0);
	dwceqos_writel(dev, DWCEQOS_MAC_L3L4_CTRL(f_no), val);

	val = dwceqos_readl(dev, DWCEQOS_MAC_L3L4_CTRL(f_no));
	val &= ~(DWCEQOS_MAC_L3L4_CR_L3DAIM0);
	dwceqos_writel(dev, DWCEQOS_MAC_L3L4_CTRL(f_no), val);
}

static void update_ipv4_src_l3_filter(struct dwc_eqos_prv_data *dev, int f_no,
				      int enable, int perf_inv)
{
	u32 val;

	if (enable) {
		/* Enable L3 filters for IPv4 Source addr matching */
		val = dwceqos_readl(dev, DWCEQOS_MAC_L3L4_CTRL(f_no));
		val |= (DWCEQOS_MAC_L3L4_CR_L3SAM0);
		dwceqos_writel(dev, DWCEQOS_MAC_L3L4_CTRL(f_no), val);

		val = dwceqos_readl(dev, DWCEQOS_MAC_L3L4_CTRL(f_no));
		if (perf_inv)
			val |= (DWCEQOS_MAC_L3L4_CR_L3SAIM0);
		else
			val &= ~(DWCEQOS_MAC_L3L4_CR_L3SAIM0);
		dwceqos_writel(dev, DWCEQOS_MAC_L3L4_CTRL(f_no), val);
	} else {
		/* Disable L3 filters for IPv4 Source addr matching */
		val = dwceqos_readl(dev, DWCEQOS_MAC_L3L4_CTRL(f_no));
		val &= ~(DWCEQOS_MAC_L3L4_CR_L3SAM0);
		dwceqos_writel(dev, DWCEQOS_MAC_L3L4_CTRL(f_no), val);

		val = dwceqos_readl(dev, DWCEQOS_MAC_L3L4_CTRL(f_no));
		val &= ~(DWCEQOS_MAC_L3L4_CR_L3SAIM0);
		dwceqos_writel(dev, DWCEQOS_MAC_L3L4_CTRL(f_no), val);
	}
}

static void update_ipv4_dest_l3_filter(struct dwc_eqos_prv_data *dev, int f_no,
				       int enable, int perf_inv)
{
	u32 val;

	if (enable) {
		/* Enable L3 filters for IPv4 Destination addr matching */
		val = dwceqos_readl(dev, DWCEQOS_MAC_L3L4_CTRL(f_no));
		val |= (DWCEQOS_MAC_L3L4_CR_L3DAM0);
		dwceqos_writel(dev, DWCEQOS_MAC_L3L4_CTRL(f_no), val);

		val = dwceqos_readl(dev, DWCEQOS_MAC_L3L4_CTRL(f_no));
		if (perf_inv)
			val |= (DWCEQOS_MAC_L3L4_CR_L3DAIM0);
		else
			val &= ~(DWCEQOS_MAC_L3L4_CR_L3DAIM0);
		dwceqos_writel(dev, DWCEQOS_MAC_L3L4_CTRL(f_no), val);
	} else {
		/* Disable L3 filters for IPv4 Destination addr matching */
		val = dwceqos_readl(dev, DWCEQOS_MAC_L3L4_CTRL(f_no));
		val &= ~(DWCEQOS_MAC_L3L4_CR_L3DAM0);
		dwceqos_writel(dev, DWCEQOS_MAC_L3L4_CTRL(f_no), val);

		val = dwceqos_readl(dev, DWCEQOS_MAC_L3L4_CTRL(f_no));
		val &= ~(DWCEQOS_MAC_L3L4_CR_L3DAIM0);
		dwceqos_writel(dev, DWCEQOS_MAC_L3L4_CTRL(f_no), val);
	}
}

static int config_l3_filters(struct net_device *dev, int f_no,
			     int endis, int ipv6, int src_dest_addr_match,
			     int perf_inv)
{
	struct dwc_eqos_prv_data *pdata = netdev_priv(dev);
	u32 val;

	val = dwceqos_readl(pdata, DWCEQOS_MAC_L3L4_CTRL(f_no));
	if (ipv6) {
		val |= (DWCEQOS_MAC_L3L4_CR_L3PEN0);
		dwceqos_writel(pdata, DWCEQOS_MAC_L3L4_CTRL(f_no), val);
		if (endis) {
			if (src_dest_addr_match == 0)
				enable_ipv6_src_l3_filter(pdata, f_no, perf_inv);
			else
				enable_ipv6_dest_l3_filter(pdata, f_no, perf_inv);
		} else {
			disable_ipv6_l3_filter(pdata, f_no);
		}
	} else {
		val &= ~(DWCEQOS_MAC_L3L4_CR_L3PEN0);
		dwceqos_writel(pdata, DWCEQOS_MAC_L3L4_CTRL(f_no), val);
		if (src_dest_addr_match == 0)
			update_ipv4_src_l3_filter(pdata, f_no, endis, perf_inv);
		else
			update_ipv4_dest_l3_filter(pdata, f_no, endis, perf_inv);
	}

	return 0;
}

/* This sequence is used to configure MAC in differnet pkt processing
 * modes like promiscuous, multicast, unicast, hash unicast/multicast.
 */
static int config_mac_pkt_filter_reg(struct dwc_eqos_prv_data *pdata,
				     unsigned char pr_mode,
				     unsigned char huc_mode,
				     unsigned char hmc_mode,
				     unsigned char pm_mode,
				     unsigned char hpf_mode)
{
	unsigned long varmac_mpfr;

	/* configure device in differnet modes
	 * promiscuous, hash unicast, hash multicast,
	 * all multicast and perfect/hash filtering mode.
	 */
	varmac_mpfr = dwceqos_readl(pdata, DWCEQOS_MAC_PKT_FILT);
	if (pr_mode)
		varmac_mpfr |= DWCEQOS_MAC_PKT_FILT_PR;
	else
		varmac_mpfr &= ~(DWCEQOS_MAC_PKT_FILT_PR);

	if (huc_mode)
		varmac_mpfr |= DWCEQOS_MAC_PKT_FILT_HUC;
	else
		varmac_mpfr &= ~(DWCEQOS_MAC_PKT_FILT_HUC);

	if (hmc_mode)
		varmac_mpfr |= DWCEQOS_MAC_PKT_FILT_HMC;
	else
		varmac_mpfr &= ~(DWCEQOS_MAC_PKT_FILT_HMC);

	if (pm_mode)
		varmac_mpfr |= DWCEQOS_MAC_PKT_FILT_PM;
	else
		varmac_mpfr &= ~(DWCEQOS_MAC_PKT_FILT_PM);

	if (hpf_mode)
		varmac_mpfr |= DWCEQOS_MAC_PKT_FILT_HPF;
	else
		varmac_mpfr &= ~(DWCEQOS_MAC_PKT_FILT_HPF);

	dwceqos_writel(pdata, DWCEQOS_MAC_PKT_FILT, varmac_mpfr);

	return 0;
}

/* This sequence is used to enable/disable L3 and L4 filtering
 */
static int config_l3_l4_filter_enable(struct net_device *dev, int endis)
{
	struct dwc_eqos_prv_data *pdata = netdev_priv(dev);
	u32 regval;

	regval = dwceqos_readl(pdata, DWCEQOS_MAC_PKT_FILT);
	if (endis)
		regval |= (DWCEQOS_MAC_PKT_FILT_IPFE);
	else
		regval &= ~(DWCEQOS_MAC_PKT_FILT_IPFE);
	dwceqos_writel(pdata, DWCEQOS_MAC_PKT_FILT, regval);

	return 0;
}

/* This sequence is used to select perfect/inverse matching for L2 DA
 */
static int config_l2_da_perfect_inverse_match(struct dwc_eqos_prv_data *pdata,
					      int perfect_inverse_match)
{
	u32 regval;

	regval = dwceqos_readl(pdata, DWCEQOS_MAC_PKT_FILT);
	if (perfect_inverse_match)
		regval |= (DWCEQOS_MAC_PKT_FILT_DAIF);
	else
		regval &= ~(DWCEQOS_MAC_PKT_FILT_DAIF);
	dwceqos_writel(pdata, DWCEQOS_MAC_PKT_FILT, regval);

	return 0;
}

/* This sequence is used to update the MAC address in last 96 MAC
 * address Low and High register(32-127) for L2 layer filtering
 */
static int update_mac_addr32_127_low_high_reg(struct dwc_eqos_prv_data *pdata,
					      int idx, unsigned char addr[])
{
	u32 regval;

	dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(idx), (addr[0] |
		      (addr[1] << 8) | (addr[2] << 16) | (addr[3] << 24)));
	regval = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(idx));
	regval &= ~(DWCEQOS_MAC_ADDR_HIGH_ADDRHI(0xffff));
	regval |= DWCEQOS_MAC_ADDR_HIGH_ADDRHI(addr[4] | (addr[5] << 8));
	regval |= DWCEQOS_MAC_ADDR_HIGH_AE;
	dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(idx), regval);

	return 0;
}

/* This sequence is used to update the MAC address in first 31 MAC
 * address Low and High register(1-31) for L2 layer filtering
 */
static int update_mac_addr1_31_low_high_reg(struct dwc_eqos_prv_data *pdata,
					    int idx, unsigned char addr[])
{
	u32 regval;

	dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(idx), (addr[0] |
		      (addr[1] << 8) | (addr[2] << 16) | (addr[3] << 24)));
	regval = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(idx));
	regval &= ~(DWCEQOS_MAC_ADDR_HIGH_ADDRHI(0xffff));
	regval |= DWCEQOS_MAC_ADDR_HIGH_ADDRHI(addr[4] | (addr[5] << 8));
	regval |= DWCEQOS_MAC_ADDR_HIGH_AE;
	dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(idx), regval);

	return 0;
}

/* This sequence is used to configure hash table register for
 * hash address filtering
 */
static int update_hash_table_reg(struct dwc_eqos_prv_data *pdata, int idx,
				 unsigned int data)
{
	dwceqos_writel(pdata, (0x10 + (idx * 4)), data);

	return 0;
}

/* This sequence is used check whether Tx drop status in the
 * MTL is enabled or not, returns 1 if it is enabled and 0 if
 * it is disabled.
 */
static int drop_tx_status_enabled(struct dwc_eqos_prv_data *pdata)
{
	unsigned long varmtl_omr;

	varmtl_omr = dwceqos_readl(pdata, DWCEQOS_MTL_OP_MODE);
	return GET_VALUE(varmtl_omr, MTL_OMR_DTXSTS_LPOS, MTL_OMR_DTXSTS_HPOS);
}

/* This sequence is used configure MAC SSIR
 */
static int config_sub_second_increment(struct dwc_eqos_prv_data *pdata,
				       unsigned long ptp_clock)
{
	unsigned long val;
	unsigned long varmac_tcr;
	u32 regval;

	varmac_tcr = dwceqos_readl(pdata, DWCEQOS_MAC_TS_CTRL);

	/* convert the PTP_CLOCK to nano second
	 *  formula is : ((1/ptp_clock) * 1000000000)
	 *  where, ptp_clock = 50MHz if FINE correction
	 *  and ptp_clock = DWC_EQOS_SYSCLOCK if COARSE correction
	 */
	if (GET_VALUE(varmac_tcr, MAC_TCR_TSCFUPDT_LPOS, MAC_TCR_TSCFUPDT_HPOS) == 1)
		val = ((1 * 1000000000ull) / 20000000);
	else
		val = ((1 * 1000000000ull) / ptp_clock);

	/* 0.465ns accurecy */
	if (GET_VALUE(varmac_tcr, MAC_TCR_TSCTRLSSR_LPOS, MAC_TCR_TSCTRLSSR_HPOS) == 0)
		val = (val * 1000) / 465;
	regval = dwceqos_readl(pdata, DWCEQOS_MAC_SUB_SEC_INC);
	regval &= ~(DWCEQOS_MAC_SUB_SEC_INC_SSINC(0xff));
	regval |= DWCEQOS_MAC_SUB_SEC_INC_SSINC(val);
	dwceqos_writel(pdata, DWCEQOS_MAC_SUB_SEC_INC, regval);

	return 0;
}

/* This sequence is used get 64-bit system time in nano sec
 */
static unsigned long long get_systime(struct dwc_eqos_prv_data *pdata)
{
	unsigned long long ns;
	unsigned long varmac_stnsr;
	unsigned long varmac_stsr;

	varmac_stnsr = dwceqos_readl(pdata, DWCEQOS_MAC_SYS_TIME_NSEC);
	ns = GET_VALUE(varmac_stnsr, MAC_STNSR_TSSS_LPOS, MAC_STNSR_TSSS_HPOS);
	/* convert sec/high time value to nanosecond */
	varmac_stsr = dwceqos_readl(pdata,  DWCEQOS_MAC_SYS_TIME_SEC);
	ns = ns + (varmac_stsr * 1000000000ull);

	return ns;
}

/* This sequence is used to adjust/update the system time
 */
static int adjust_systime(struct dwc_eqos_prv_data *pdata, unsigned int sec,
			  unsigned int nsec, int add_sub,
			  bool one_nsec_accuracy)
{
	u32 regval;
	unsigned long retrycount = 100000;
	unsigned long vy_count;
	volatile unsigned long varmac_tcr;
	/* wait for previous(if any) time adjust/update to complete. */
	/* Poll */

	vy_count = 0;
	while (1) {
		if (vy_count > retrycount)
			return -1;
		varmac_tcr = dwceqos_readl(pdata, DWCEQOS_MAC_TS_CTRL);
		if (GET_VALUE(varmac_tcr, MAC_TCR_TSUPDT_LPOS, MAC_TCR_TSUPDT_HPOS) == 0)
			break;
		vy_count++;
		mdelay(1);
	}

	if (add_sub) {
		/* If the new sec value needs to be subtracted with
		 * the system time, then MAC_STSUR reg should be
		 * programmed with (2^32 – <new_sec_value>)
		 */
		sec = (0x100000000ull - sec);

		/* If the new nsec value need to be subtracted with
		 * the system time, then MAC_STNSUR.TSSS field should be
		 * programmed with,
		 * (10^9 - <new_nsec_value>) if MAC_TCR.TSCTRLSSR is set or
		 * (2^31 - <new_nsec_value> if MAC_TCR.TSCTRLSSR is reset)
		 */
		if (one_nsec_accuracy)
			nsec = (0x3B9ACA00 - nsec);
		else
			nsec = (0x80000000 - nsec);
	}

	dwceqos_writel(pdata, DWCEQOS_MAC_SYS_TIME_SEC_UPD, sec);
	regval = dwceqos_readl(pdata, DWCEQOS_MAC_SYS_TIME_NSEC_UPD);
	regval &= ~(DWCEQOS_MAC_SYS_TIME_NSEC_UPD_TSSS(0x7fffffff));
	regval |= DWCEQOS_MAC_SYS_TIME_NSEC_UPD_TSSS(nsec);
	if (add_sub) {
		regval &= ~(DWCEQOS_MAC_SYS_TIME_NSEC_UPD_ADDSUB);
		regval |= DWCEQOS_MAC_SYS_TIME_NSEC_UPD_ADDSUB;
	} else {
		regval &= ~(DWCEQOS_MAC_SYS_TIME_NSEC_UPD_ADDSUB);
	}
	dwceqos_writel(pdata, DWCEQOS_MAC_SYS_TIME_NSEC_UPD, regval);

	/* issue command to initialize system time with the value */
	/* specified in MAC_STSUR and MAC_STNSUR. */
	regval = dwceqos_readl(pdata, DWCEQOS_MAC_TS_CTRL);
	regval &= ~(DWCEQOS_MAC_TS_CTRL_TSUPDT);
	regval |= DWCEQOS_MAC_TS_CTRL_TSUPDT;
	dwceqos_writel(pdata, DWCEQOS_MAC_TS_CTRL, regval);
	/* wait for present time initialize to complete. */

	/*Poll*/
	vy_count = 0;
	while (1) {
		if (vy_count > retrycount)
			return -1;
		varmac_tcr = dwceqos_readl(pdata, DWCEQOS_MAC_TS_CTRL);
		if (GET_VALUE(varmac_tcr, MAC_TCR_TSUPDT_LPOS, MAC_TCR_TSUPDT_HPOS) == 0)
			break;
		vy_count++;
		mdelay(1);
	}

	return 0;
}

/* This sequence is used to adjust the ptp operating frequency.
 */
static int config_addend(struct dwc_eqos_prv_data *pdata, unsigned int data)
{
	unsigned long retrycount = 100000;
	unsigned long vy_count;
	volatile unsigned long varmac_tcr;
	u32 regval;

	/* wait for previous(if any) added update to complete. */

	/*Poll*/
	vy_count = 0;
	while (1) {
		if (vy_count > retrycount)
			return -1;
		varmac_tcr = dwceqos_readl(pdata, DWCEQOS_MAC_TS_CTRL);
		if (GET_VALUE(varmac_tcr, MAC_TCR_TSADDREG_LPOS, MAC_TCR_TSADDREG_HPOS) == 0)
			break;
		vy_count++;
		mdelay(1);
	}
	dwceqos_writel(pdata, DWCEQOS_MAC_TS_ADDEND, data);

	/* issue command to update the added value */
	regval = dwceqos_readl(pdata, DWCEQOS_MAC_TS_CTRL);
	regval &= ~(DWCEQOS_MAC_TS_CTRL_TSADDREG);
	regval |= DWCEQOS_MAC_TS_CTRL_TSADDREG;
	dwceqos_writel(pdata, DWCEQOS_MAC_TS_CTRL, regval);
	/* wait for present added update to complete. */

	/*Poll*/
	vy_count = 0;
	while (1) {
		if (vy_count > retrycount)
			return -1;
		varmac_tcr = dwceqos_readl(pdata, DWCEQOS_MAC_TS_CTRL);
		if (GET_VALUE(varmac_tcr, MAC_TCR_TSADDREG_LPOS, MAC_TCR_TSADDREG_HPOS) == 0)
			break;
		vy_count++;
		mdelay(1);
	}

	return 0;
}

/* This sequence is used to initialize the system time
 */
static int init_systime(struct dwc_eqos_prv_data *pdata, unsigned int sec,
			unsigned int nsec)
{
	unsigned long retrycount = 100000;
	unsigned long vy_count;
	volatile unsigned long varmac_tcr;
	u32 regval;

	/* wait for previous(if any) time initialize to complete. */

	/*Poll*/
	vy_count = 0;
	while (1) {
		if (vy_count > retrycount)
			return -1;
		varmac_tcr = dwceqos_readl(pdata, DWCEQOS_MAC_TS_CTRL);
		if (GET_VALUE(varmac_tcr, MAC_TCR_TSINIT_LPOS, MAC_TCR_TSINIT_HPOS) == 0)
			break;
		vy_count++;
		mdelay(1);
	}
	dwceqos_writel(pdata, DWCEQOS_MAC_SYS_TIME_SEC_UPD, sec);
	dwceqos_writel(pdata, DWCEQOS_MAC_SYS_TIME_NSEC_UPD, nsec);
	/* issue command to initialize system time with the value */
	/* specified in MAC_STSUR and MAC_STNSUR. */
	regval = dwceqos_readl(pdata, DWCEQOS_MAC_TS_CTRL);
	regval &= ~(DWCEQOS_MAC_TS_CTRL_TSINIT);
	regval |= DWCEQOS_MAC_TS_CTRL_TSINIT;
	dwceqos_writel(pdata, DWCEQOS_MAC_TS_CTRL, regval);
	/* wait for present time initialize to complete. */

	/*Poll*/
	vy_count = 0;
	while (1) {
		if (vy_count > retrycount)
			return -1;
		varmac_tcr = dwceqos_readl(pdata, DWCEQOS_MAC_TS_CTRL);
		if (GET_VALUE(varmac_tcr, MAC_TCR_TSINIT_LPOS, MAC_TCR_TSINIT_HPOS) == 0)
			break;
		vy_count++;
		mdelay(1);
	}

	return 0;
}

/* This sequence is used to enable HW time stamping
 * and receive frames
 */
static int config_hw_time_stamping(struct dwc_eqos_prv_data *pdata,
				   unsigned int config_val)
{
	dwceqos_writel(pdata, DWCEQOS_MAC_TS_CTRL, config_val);

	return 0;
}

/* This sequence is used get the 64-bit of the timestamp
 * captured by the device for the corresponding received packet
 * in nanosecond.
 */
static unsigned long long get_rx_tstamp(t_rx_context_desc *rxdesc)
{
	unsigned long long ns;
	unsigned long varrdes1;

	ns = rxdesc->rdes0;
	varrdes1 = rxdesc->rdes1;
	ns = ns + (varrdes1 * 1000000000ull);

	return ns;
}

/* This sequence is used to check whether the captured timestamp
 * for the corresponding received packet is valid or not.
 * Returns 0 if no context descriptor
 * Returns 1 if timestamp is valid
 * Returns 2 if time stamp is corrupted
 */
static unsigned int get_rx_tstamp_status(t_rx_context_desc *rxdesc)
{
	unsigned int varown;
	unsigned int varctxt;
	unsigned int varrdes0;
	unsigned int varrdes1;

	/* check for own bit and CTXT bit */
	RX_CONTEXT_DESC_RDES3_OWN_MLF_RD(rxdesc->rdes3, varown);
	RX_CONTEXT_DESC_RDES3_CTXT_MLF_RD(rxdesc->rdes3, varctxt);
	if ((varown == 0) && (varctxt == 0x1)) {
		varrdes0 = rxdesc->rdes0;
		varrdes1 = rxdesc->rdes1;
		if ((varrdes0 == 0xffffffff) && (varrdes1 == 0xffffffff))
			/* time stamp is corrupted */
			return 2;
		/* time stamp is valid */
		return 1;
	}
	/* no CONTEX desc to hold time stamp value */
	return 0;
}

/* This sequence is used to check whether the timestamp value
 * is available in a context descriptor or not. Returns 1 if timestamp
 * is available else returns 0
 */
static unsigned int rx_tstamp_available(t_rx_normal_desc *rxdesc)
{
	unsigned int varrs1v;
	unsigned int vartsa;

	RX_NORMAL_DESC_RDES3_RS1V_MLF_RD(rxdesc->rdes3, varrs1v);
	if (varrs1v == 1) {
		RX_NORMAL_DESC_RDES1_TSA_MLF_RD(rxdesc->rdes1, vartsa);
		return vartsa;
	} else {
		return 0;
	}
}

/* This sequence is used get the least 64-bit of the timestamp
 * captured by the device for the corresponding transmit packet in nanosecond
 * returns ns
 */
static unsigned long long get_tx_tstamp_via_reg(struct dwc_eqos_prv_data *pdata)
{
	unsigned long long ns;
	unsigned long varmac_ttn;

	ns = dwceqos_readl(pdata, DWCEQOS_MAC_TX_TS_STAT_NSEC);
	ns = ns & (0x7fffffff);
	varmac_ttn = dwceqos_readl(pdata, DWCEQOS_MAC_TX_TS_STAT_SEC);
	ns = ns + (varmac_ttn * 1000000000ull);

	return ns;
}

/* This sequence is used to check whether a timestamp has been
 * captured for the corresponding transmit packet. Returns 1 if
 * timestamp is taken else returns 0
 */
static unsigned int get_tx_tstamp_status_via_reg(struct dwc_eqos_prv_data *pdata)
{
	unsigned long varmac_tcr;
	unsigned long varmac_ttsn;

	/* device is configured to overwrite the timesatmp of */
	/* eariler packet if driver has not yet read it. */
	varmac_tcr = dwceqos_readl(pdata, DWCEQOS_MAC_TS_CTRL);
	if (GET_VALUE(varmac_tcr, MAC_TCR_TXTSSTSM_LPOS, MAC_TCR_TXTSSTSM_HPOS) == 1) {
		/* nothing to do */
	} else {
		/* timesatmp of the current pkt is ignored or not captured */
		varmac_ttsn = dwceqos_readl(pdata, DWCEQOS_MAC_TX_TS_STAT_NSEC);
		if (GET_VALUE(varmac_ttsn, MAC_TTSN_TXTSSTSMIS_LPOS, MAC_TTSN_TXTSSTSMIS_HPOS) == 1)
			return 0;
		else
			return 1;
		}

	return 0;
}

/* This sequence is used get the 64-bit of the timestamp captured
 * by the device for the corresponding transmit packet in nanosecond.
 */
static unsigned long long get_tx_tstamp(t_tx_normal_desc *txdesc)
{
	unsigned long long ns;
	unsigned long vartdes1;

	ns = txdesc->tdes0;
	vartdes1 = txdesc->tdes1;
	ns = ns + (vartdes1 * 1000000000ull);

	return ns;
}

/* This sequence is used to check whether a timestamp has been
 * captured for the corresponding transmit packet. Returns 1 if
 * timestamp is taken else returns 0
 */
static unsigned int get_tx_tstamp_status(t_tx_normal_desc *txdesc)
{
	unsigned int vartdes3;

	vartdes3 = txdesc->tdes3;

	return (vartdes3 & 0x20000);
}

/* This sequence is used to set tx queue operating mode for Queue[0 - 7]
 */
static int set_tx_queue_operating_mode(struct dwc_eqos_prv_data *pdata,
				       unsigned int qinx, unsigned int q_mode)
{
	u32 regval;

	regval = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_OP_MODE(qinx));
	regval &= ~(DWCEQOS_MTL_TQOM_QEN(0x3));
	regval |= DWCEQOS_MTL_TQOM_QEN(q_mode);
	dwceqos_writel(pdata, DWCEQOS_MTL_TQ_OP_MODE(qinx), regval);

	return 0;
}

/* This sequence is used to select Tx Scheduling Algorithm for AVB feature for Queue[1 - 7]
 */
static int set_avb_algorithm(struct dwc_eqos_prv_data *pdata,
			     unsigned int qinx, unsigned char avb_algo)
{
	u32 regval;

	regval = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_ETS_CTRL(qinx));
	regval &= ~(DWCEQOS_MTL_QECR_AVALG);

	if (avb_algo)
		regval |= DWCEQOS_MTL_QECR_AVALG;

	dwceqos_writel(pdata, DWCEQOS_MTL_TQ_ETS_CTRL(qinx), regval);

	return 0;
}

/* This sequence is used to configure credit-control for Queue[1 - 7]
 */
static int config_credit_control(struct dwc_eqos_prv_data *pdata,
				 unsigned int qinx, unsigned int cc)
{
	u32 regval;

	regval = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_ETS_CTRL(qinx));
	regval &= ~(DWCEQOS_MTL_QECR_CC);

	if (cc)
		regval |= DWCEQOS_MTL_QECR_CC;

	dwceqos_writel(pdata, DWCEQOS_MTL_TQ_ETS_CTRL(qinx), regval);

	return 0;
}

/* This sequence is used to configure send slope credit value
 * required for the credit-based shaper alogorithm for Queue[1 - 7]
 */
static int config_send_slope(struct dwc_eqos_prv_data *pdata, unsigned int qinx,
			     unsigned int sendslope)
{
	dwceqos_writel(pdata, DWCEQOS_MTL_TQ_SSC(qinx), sendslope);

	return 0;
}

/* This sequence is used to configure idle slope credit value
 * required for the credit-based shaper alogorithm for Queue[1 - 7]
 */
static int config_idle_slope(struct dwc_eqos_prv_data *pdata, unsigned int qinx,
			     unsigned int idleslope)
{
	u32 regval;

	regval = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_QW(qinx));
	regval &= ~(DWCEQOS_MTL_TQQW_ISCQW(0x1fffff));
	regval |= DWCEQOS_MTL_TQQW_ISCQW(idleslope);
	dwceqos_writel(pdata, DWCEQOS_MTL_TQ_QW(qinx), regval);

	return 0;
}

/* This sequence is used to configure low credit value
 * required for the credit-based shaper alogorithm for Queue[1 - 7]
 */
static int config_low_credit(struct dwc_eqos_prv_data *pdata, unsigned int qinx,
			     unsigned int lowcredit)
{
	int lowcredit_neg = lowcredit;

	pr_info("lowCreidt = %08x lowCredit_neg:%08x\n",
		 lowcredit, lowcredit_neg);

	dwceqos_writel(pdata, DWCEQOS_MTL_TQ_LC(qinx), lowcredit_neg);

	return 0;
}

/* This sequence is used to enable/disable slot number check When set,
 * this bit enables the checking of the slot number programmed in the TX
 * descriptor with the current reference given in the RSN field. The DMA fetches
 * the data from the corresponding buffer only when the slot number is: equal to
 * the reference slot number or  ahead of the reference slot number by one.
 */
static int config_slot_num_check(struct dwc_eqos_prv_data *pdata,
				 unsigned int qinx, unsigned char slot_check)
{
	u32 regval;

	regval = dwceqos_readl(pdata, DWCEQOS_DMA_CH_SLOT_FUNC_CS(qinx));
	regval &= ~(DWCEQOS_DMA_CH_SFCS_ESC);

	if (slot_check)
		regval |= DWCEQOS_DMA_CH_SFCS_ESC;

	dwceqos_writel(pdata, DWCEQOS_DMA_CH_SLOT_FUNC_CS(qinx), regval);

	return 0;
}

/* This sequence is used to enable/disable advance slot check When set,
 * this bit enables the DAM to fetch the data from the buffer when the slot
 * number programmed in TX descriptor is equal to the reference slot number
 * given in RSN field or ahead of the reference number by upto two slots
 */
static int config_advance_slot_num_check(struct dwc_eqos_prv_data *pdata,
					 unsigned int qinx,
					 unsigned char adv_slot_check)
{
	u32 regval;

	regval = dwceqos_readl(pdata, DWCEQOS_DMA_CH_SLOT_FUNC_CS(qinx));
	regval &= ~(DWCEQOS_DMA_CH_SFCS_ASC);

	if (adv_slot_check)
		regval |= DWCEQOS_DMA_CH_SFCS_ASC;

	dwceqos_writel(pdata, DWCEQOS_DMA_CH_SLOT_FUNC_CS(qinx), regval);

	return 0;
}

/* This sequence is used to configure high credit value required
 * for the credit-based shaper alogorithm for Queue[1 - 7]
 */
static int config_high_credit(struct dwc_eqos_prv_data *pdata,
			      unsigned int qinx, unsigned int hicredit)
{
	dwceqos_writel(pdata, DWCEQOS_MTL_TQ_HC(qinx), hicredit);

	return 0;
}

/* This sequence is used to set weights for DCB feature for Queue[0 - 7]
 */
static int set_dcb_queue_weight(struct dwc_eqos_prv_data *pdata,
				unsigned int qinx, unsigned int q_weight)
{
	u32 regval;

	regval = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_QW(qinx));
	regval &= ~(DWCEQOS_MTL_TQQW_ISCQW(0x1fffff));
	regval |= DWCEQOS_MTL_TQQW_ISCQW(q_weight);
	dwceqos_writel(pdata, DWCEQOS_MTL_TQ_QW(qinx), regval);

	return 0;
}

/* This sequence is used to select Tx Scheduling Algorithm for DCB feature
 */
static int set_dcb_algorithm(struct dwc_eqos_prv_data *pdata,
			     unsigned char dcb_algo)
{
	u32 regval;

	regval = dwceqos_readl(pdata, DWCEQOS_MTL_OP_MODE);
	regval &= ~(DWCEQOS_MTL_OP_MODE_SCHALG(0x3));
	regval |= DWCEQOS_MTL_OP_MODE_SCHALG(dcb_algo);
	dwceqos_writel(pdata, DWCEQOS_MTL_OP_MODE, regval);

	return 0;
}

/* This sequence is used to get Tx queue count
 */
unsigned char get_tx_queue_count(void __iomem *base)
{
	unsigned char count;
	unsigned long varmac_hfr2;

	varmac_hfr2 = __raw_readl(base + 0x124);
	count = GET_VALUE(varmac_hfr2, MAC_HFR2_TXQCNT_LPOS, MAC_HFR2_TXQCNT_HPOS);

	return (count + 1);
}

/* This sequence is used to get Rx queue count
 */
unsigned char get_rx_queue_count(void __iomem *base)
{
	unsigned char count;
	unsigned long varmac_hfr2;

	varmac_hfr2 = __raw_readl(base + 0x124);
	count = GET_VALUE(varmac_hfr2, MAC_HFR2_RXQCNT_LPOS, MAC_HFR2_RXQCNT_HPOS);

	return (count + 1);
}

/* This sequence is used to disables all Tx/Rx MMC interrupts
 */
static int disable_mmc_interrupts(struct dwc_eqos_prv_data *pdata)
{
	/* disable all TX interrupts */
	dwceqos_writel(pdata, DWCEQOS_MMC_TX_INTR_MASK, 0xffffffff);
	/* disable all RX interrupts */
	dwceqos_writel(pdata, DWCEQOS_MMC_RX_INTR_MASK, 0xffffffff);
	/* Disable MMC Rx Interrupts for IPC */
	dwceqos_writel(pdata, DWCEQOS_MMC_IPC_RX_INTR_MASK, 0xffffffff);

return 0;
}

/* This sequence is used to configure MMC counters
 */
static int config_mmc_counters(struct dwc_eqos_prv_data *pdata)
{
	unsigned long varmmc_cntrl;

	/* set COUNTER RESET */
	/* set RESET ON READ */
	/* set COUNTER PRESET */
	/* set FULL_HALF PRESET */
	varmmc_cntrl = dwceqos_readl(pdata, DWCEQOS_MMC_CTRL);
	varmmc_cntrl |= (DWCEQOS_MMC_CTRL_CNTRST | DWCEQOS_MMC_CTRL_RSTONRD |
			 DWCEQOS_MMC_CTRL_CNTPRST | DWCEQOS_MMC_CTRL_CNTPRSTLVL);
	dwceqos_writel(pdata, DWCEQOS_MMC_CTRL, varmmc_cntrl);

	return 0;
}

/* This sequence is used to disable given DMA channel rx interrupts,
 * called with pdata->lock held.
 */
static int disable_rx_interrupt(struct dwc_eqos_prv_data *pdata,
				unsigned int qinx)
{
	u32 regval;

	regval = dwceqos_readl(pdata, DWCEQOS_DMA_CH_INTR_EN(qinx));
	regval &= ~(DWCEQOS_DMA_CH_IE_RBUE);
	regval &= ~(DWCEQOS_DMA_CH_IE_RIE);
	dwceqos_writel(pdata, DWCEQOS_DMA_CH_INTR_EN(qinx), regval);

	return 0;
}

/* This sequence is used to enable given DMA channel rx interrupts,
 * called with pdata->lock held.
 */
static int enable_rx_interrupt(struct dwc_eqos_prv_data *pdata,
			       unsigned int qinx)
{
	u32 regval;

	regval = dwceqos_readl(pdata, DWCEQOS_DMA_CH_INTR_EN(qinx));
	regval &= ~(DWCEQOS_DMA_CH_IE_RBUE);
	regval |= DWCEQOS_DMA_CH_IE_RBUE;
	regval &= ~(DWCEQOS_DMA_CH_IE_RIE);
	regval |= DWCEQOS_DMA_CH_IE_RIE;
	dwceqos_writel(pdata, DWCEQOS_DMA_CH_INTR_EN(qinx), regval);

	return 0;
}

/* This sequence is used to disable given DMA channel tx interrupts,
 * called with pdata->lock held.
 */
static int disable_tx_interrupt(struct dwc_eqos_prv_data *pdata,
								unsigned int qinx)
{
	u32 regval;

	regval = dwceqos_readl(pdata, DWCEQOS_DMA_CH_INTR_EN(qinx));
	regval &= ~DWCEQOS_DMA_CH_IE_TIE;
	dwceqos_writel(pdata, DWCEQOS_DMA_CH_INTR_EN(qinx), regval);

	return 0;
}

/* This sequence is used to enable given DMA channel tx interrupts,
 * called with pdata->lock held.
 */
static int enable_tx_interrupt(struct dwc_eqos_prv_data *pdata,
							   unsigned int qinx)
{
	u32 regval;

	regval = dwceqos_readl(pdata, DWCEQOS_DMA_CH_INTR_EN(qinx));
	regval |= DWCEQOS_DMA_CH_IE_TIE;
	dwceqos_writel(pdata, DWCEQOS_DMA_CH_INTR_EN(qinx), regval);

	return 0;
}
static void configure_sa_via_reg(struct net_device *dev, u32 cmd)
{
	struct dwc_eqos_prv_data *pdata = netdev_priv(dev);
	u32 regval;

	regval = dwceqos_readl(pdata, DWCEQOS_MAC_CFG);
	regval &= ~(DWCEQOS_MAC_CFG_SARC(0x7));
	regval |= DWCEQOS_MAC_CFG_SARC(cmd);
	dwceqos_writel(pdata, DWCEQOS_MAC_CFG, regval);
}

static void configure_mac_addr1_reg(struct dwc_eqos_prv_data *pdata,
				    unsigned char *mac_addr)
{
	dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(1),
		       ((mac_addr[5] << 8) | (mac_addr[4])));
	dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(1),
		       ((mac_addr[3] << 24) | (mac_addr[2] << 16) |
		       (mac_addr[1] << 8) | (mac_addr[0])));
}

static void configure_mac_addr0_reg(struct dwc_eqos_prv_data *pdata,
				    unsigned char *mac_addr)
{
	dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(0),
		       ((mac_addr[5] << 8) | (mac_addr[4])));
	dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(0),
		       ((mac_addr[3] << 24) | (mac_addr[2] << 16) |
		       (mac_addr[1] << 8) | (mac_addr[0])));
}

static void config_rx_outer_vlan_stripping(struct dwc_eqos_prv_data *pdata,
					   u32 cmd)
{
	u32 regval;

	regval = dwceqos_readl(pdata, DWCEQOS_MAC_VLAN_TAG_CTRL);
	regval &= ~(DWCEQOS_MAC_VLAN_TAG_CTRL_EVLS(0x3));
	regval |= DWCEQOS_MAC_VLAN_TAG_CTRL_EVLS(cmd);
	dwceqos_writel(pdata, DWCEQOS_MAC_VLAN_TAG_CTRL, regval);
}

static void config_rx_inner_vlan_stripping(struct dwc_eqos_prv_data *pdata,
					   u32 cmd)
{
	u32 regval;

	regval = dwceqos_readl(pdata, DWCEQOS_MAC_VLAN_TAG_CTRL);
	regval &= ~(DWCEQOS_MAC_VLAN_TAG_CTRL_EIVLS(0x3));
	regval |= DWCEQOS_MAC_VLAN_TAG_CTRL_EIVLS(cmd);
	dwceqos_writel(pdata, DWCEQOS_MAC_VLAN_TAG_CTRL, regval);
}

static void config_ptpoffload_engine(struct dwc_eqos_prv_data *pdata,
				     unsigned int pto_cr, unsigned int mc_uc)
{
	u32 regval;

	dwceqos_writel(pdata, DWCEQOS_MAC_PTO_CTRL, pto_cr);
	regval = dwceqos_readl(pdata, DWCEQOS_MAC_TS_CTRL);
	regval &= ~(DWCEQOS_MAC_TS_CTRL_TSENMACADDR);

	if (mc_uc)
		regval |= DWCEQOS_MAC_TS_CTRL_TSENMACADDR;

	dwceqos_writel(pdata, DWCEQOS_MAC_TS_CTRL, regval);
}

static void configure_reg_vlan_control(struct dwc_eqos_prv_data *pdata,
				       struct dwc_eqos_tx_wrapper_descriptor
				       *desc_data)
{
	unsigned short vlan_id = desc_data->vlan_tag_id;
	unsigned int vlan_control = desc_data->tx_vlan_tag_ctrl;
	u32 regval;

	regval = dwceqos_readl(pdata, DWCEQOS_MAC_VLAN_INCL);
	regval &= ~(DWCEQOS_MAC_VLAN_INCL_VLT(0xffff));
	regval |= DWCEQOS_MAC_VLAN_INCL_VLT(vlan_id);
	regval &= ~(DWCEQOS_MAC_VLAN_INCL_VLC(0x3));
	regval |= DWCEQOS_MAC_VLAN_INCL_VLC(vlan_control);
	regval |= DWCEQOS_MAC_VLAN_INCL_VLP;
	dwceqos_writel(pdata, DWCEQOS_MAC_VLAN_INCL, regval);
}

static void configure_desc_vlan_control(struct dwc_eqos_prv_data *pdata)
{
	u32 regval;

	regval = dwceqos_readl(pdata, DWCEQOS_MAC_VLAN_INCL);
	regval |= DWCEQOS_MAC_VLAN_INCL_VLTI;
	dwceqos_writel(pdata, DWCEQOS_MAC_VLAN_INCL, regval);
}

static int configure_mac_for_vlan_pkt(struct dwc_eqos_prv_data *pdata)
{
	u32 regval;

	/* Enable VLAN Tag stripping always */
	config_rx_outer_vlan_stripping(pdata, 0x3);

	/* Enable operation on the outer VLAN Tag, if present */
	regval = dwceqos_readl(pdata, DWCEQOS_MAC_VLAN_TAG_CTRL);
	regval &= ~(DWCEQOS_MAC_VLAN_TAG_CTRL_ERIVLT);
	dwceqos_writel(pdata, DWCEQOS_MAC_VLAN_TAG_CTRL, regval);

	/* Disable double VLAN Tag processing on TX and RX */
	config_dvlan(pdata, 0);

	/* Enable VLAN Tag in RX Status. */
	regval = dwceqos_readl(pdata, DWCEQOS_MAC_VLAN_TAG_CTRL);
	regval |= DWCEQOS_MAC_VLAN_TAG_CTRL_EVLRXS;
	dwceqos_writel(pdata, DWCEQOS_MAC_VLAN_TAG_CTRL, regval);

	/* Disable VLAN Type Check */
	regval = dwceqos_readl(pdata, DWCEQOS_MAC_VLAN_TAG_CTRL);
	regval |= DWCEQOS_MAC_VLAN_TAG_CTRL_DOVLTC;
	dwceqos_writel(pdata, DWCEQOS_MAC_VLAN_TAG_CTRL, regval);

	/* configure MAC to get VLAN Tag to be inserted/replaced from */
	/* TX descriptor(context desc) */
	configure_desc_vlan_control(pdata);

	/* insert/replace C_VLAN in 13th ans 14th bytes of transmitted frames */
	regval = dwceqos_readl(pdata, DWCEQOS_MAC_VLAN_INCL);
	regval &= ~(DWCEQOS_MAC_VLAN_INCL_CSVL);
	dwceqos_writel(pdata, DWCEQOS_MAC_VLAN_INCL, regval);

	return 0;
}

static int config_pblx8(struct dwc_eqos_prv_data *pdata, unsigned int qinx,
			unsigned int val)
{
	u32 regval;

	regval = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CTRL(qinx));
	regval &= ~(DWCEQOS_DMA_CH_CTRL_PBLX8);

	if (val)
		regval |= DWCEQOS_DMA_CH_CTRL_PBLX8;

	dwceqos_writel(pdata, DWCEQOS_DMA_CH_CTRL(qinx), regval);

	return 0;
}

/* Returns programmed Tx PBL value
 */
static int get_tx_pbl_val(struct dwc_eqos_prv_data *pdata, unsigned int qinx)
{
	unsigned int tx_pbl;

	tx_pbl = dwceqos_readl(pdata, DWCEQOS_DMA_CH_TX_CTRL(qinx));
	tx_pbl = (tx_pbl >> 16) & (0x3f);

	return tx_pbl;
}

static int config_tx_pbl_val(struct dwc_eqos_prv_data *pdata,
			     unsigned int qinx, unsigned int tx_pbl)
{
	u32 regval;

	regval = dwceqos_readl(pdata, DWCEQOS_DMA_CH_TX_CTRL(qinx));
	regval &= ~(DWCEQOS_DMA_CH_TC_TXPBL(0x7));
	regval |= DWCEQOS_DMA_CH_TC_TXPBL(tx_pbl);
	dwceqos_writel(pdata, DWCEQOS_DMA_CH_TX_CTRL(qinx), regval);

	return 0;
}

/* Returns programmed Rx PBL value
 */
static int get_rx_pbl_val(struct dwc_eqos_prv_data *pdata, unsigned int qinx)
{
	unsigned int rx_pbl;

	rx_pbl = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RX_CTRL(qinx));
	rx_pbl = (rx_pbl >> 16) & (0x3f);

	return rx_pbl;
}

static int config_rx_pbl_val(struct dwc_eqos_prv_data *pdata,
			     unsigned int qinx, unsigned int rx_pbl)
{
	u32 regval;

	regval = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RX_CTRL(qinx));
	regval &= ~(DWCEQOS_DMA_CH_RC_RXPBL(0x7));
	regval |= DWCEQOS_DMA_CH_RC_RXPBL(rx_pbl);
	dwceqos_writel(pdata, DWCEQOS_DMA_CH_RX_CTRL(qinx), regval);

	return 0;
}

static int config_axi_rorl_val(struct dwc_eqos_prv_data *pdata,
			       unsigned int axi_rorl)
{
	u32 regval;

	regval = dwceqos_readl(pdata, DWCEQOS_DMA_SYSBUS_MODE);
	regval &= ~(DWCEQOS_DMA_SBM_RD_OSR_LMT(0xf));
	regval |= DWCEQOS_DMA_SBM_RD_OSR_LMT(axi_rorl);
	dwceqos_writel(pdata, DWCEQOS_DMA_SYSBUS_MODE, regval);

	return 0;
}

static int config_axi_worl_val(struct dwc_eqos_prv_data *pdata,
			       unsigned int axi_worl)
{
	u32 regval;

	regval = dwceqos_readl(pdata, DWCEQOS_DMA_SYSBUS_MODE);
	regval &= ~(DWCEQOS_DMA_SBM_WR_OSR_LMT(0xf));
	regval |= DWCEQOS_DMA_SBM_WR_OSR_LMT(axi_worl);
	dwceqos_writel(pdata, DWCEQOS_DMA_SYSBUS_MODE, regval);

	return 0;
}

static int config_axi_pbl_val(struct dwc_eqos_prv_data *pdata,
			      unsigned int axi_pbl)
{
	unsigned int vardma_sbus;

	vardma_sbus = dwceqos_readl(pdata, DWCEQOS_DMA_SYSBUS_MODE);
	vardma_sbus &= ~DMA_SBUS_AXI_PBL_MASK;
	vardma_sbus |= axi_pbl;
	dwceqos_writel(pdata, DWCEQOS_DMA_SYSBUS_MODE, vardma_sbus);

	return 0;
}

static int config_incr_incrx_mode(struct dwc_eqos_prv_data *pdata,
				  unsigned int val)
{
	u32 regval;

	regval = dwceqos_readl(pdata, DWCEQOS_DMA_SYSBUS_MODE);
	regval &= ~(DWCEQOS_DMA_SBM_FB);

	if (val)
		regval |= DWCEQOS_DMA_SBM_FB;

	dwceqos_writel(pdata, DWCEQOS_DMA_SYSBUS_MODE, regval);

	return 0;
}

static int config_osf_mode(struct dwc_eqos_prv_data *pdata,
			   unsigned int qinx, unsigned int val)
{
	u32 regval;

	regval = dwceqos_readl(pdata, DWCEQOS_DMA_CH_TX_CTRL(qinx));
	regval &= ~(DWCEQOS_DMA_CH_TC_OSP);

	if (val)
		regval |= DWCEQOS_DMA_CH_TC_OSP;

	dwceqos_writel(pdata, DWCEQOS_DMA_CH_TX_CTRL(qinx), regval);

	return 0;
}

static int config_rsf_mode(struct dwc_eqos_prv_data *pdata,
			   unsigned int qinx, unsigned int val)
{
	u32 regval;

	regval = dwceqos_readl(pdata, DWCEQOS_MTL_RQ_OP_MODE(qinx));
	regval &= ~(DWCEQOS_MTL_RQOM_RSF);

	if (val)
		regval |= DWCEQOS_MTL_RQOM_RSF;

	dwceqos_writel(pdata, DWCEQOS_MTL_RQ_OP_MODE(qinx), regval);

	return 0;
}

static int config_tsf_mode(struct dwc_eqos_prv_data *pdata,
			   unsigned int qinx, unsigned int val)
{
	u32 regval;

	regval = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_OP_MODE(qinx));
	regval &= ~(DWCEQOS_MTL_TQOM_TSF);

	if (val)
		regval |= DWCEQOS_MTL_TQOM_TSF;

	dwceqos_writel(pdata, DWCEQOS_MTL_TQ_OP_MODE(qinx), regval);

	return 0;
}

static int config_rx_threshold(struct dwc_eqos_prv_data *pdata,
			       unsigned int qinx, unsigned int val)
{
	u32 regval;

	regval = dwceqos_readl(pdata, DWCEQOS_MTL_RQ_OP_MODE(qinx));
	regval &= ~(DWCEQOS_MTL_RQOM_RTC(0x3));
	regval |= DWCEQOS_MTL_RQOM_RTC(val);
	dwceqos_writel(pdata, DWCEQOS_MTL_RQ_OP_MODE(qinx), regval);

	return 0;
}

static int config_tx_threshold(struct dwc_eqos_prv_data *pdata,
			       unsigned int qinx, unsigned int val)
{
	u32 regval;

	regval = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_OP_MODE(qinx));
	regval &= ~(DWCEQOS_MTL_TQOM_TTC(0x7));
	regval |= DWCEQOS_MTL_TQOM_TTC(val);
	dwceqos_writel(pdata, DWCEQOS_MTL_TQ_OP_MODE(qinx), regval);

	return 0;
}

static int config_rx_watchdog_timer(struct dwc_eqos_prv_data *pdata,
				    unsigned int qinx, u32 riwt)
{
	u32 regval;

	regval = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RX_INTR_WD_TIMER(qinx));
	regval &= ~(DWCEQOS_DMA_CH_RX_IWDT_RWT(0xff));
	regval |= DWCEQOS_DMA_CH_RX_IWDT_RWT(riwt);
	dwceqos_writel(pdata, DWCEQOS_DMA_CH_RX_INTR_WD_TIMER(qinx), regval);

	return 0;
}

/* for flow control */

static int control_tx_flow_ctrl(struct dwc_eqos_prv_data *pdata, int qno, int en)
{
	u32 regval;

	regval = dwceqos_readl(pdata, DWCEQOS_MAC_Q_TX_FLOW_CTRL(qno));
	if (en)
		regval |= DWCEQOS_MAC_TX_CTRL_TFE;
	else
		regval &= ~(DWCEQOS_MAC_TX_CTRL_TFE);
	dwceqos_writel(pdata, DWCEQOS_MAC_Q_TX_FLOW_CTRL(qno), regval);

	return 0;
}

static int control_rx_flow_ctrl(struct dwc_eqos_prv_data *pdata, int en)
{
	u32 regval;

	regval = dwceqos_readl(pdata, DWCEQOS_MAC_RX_FLOW_CTRL);
	if (en)
		regval |= DWCEQOS_MAC_RX_CTRL_RFE;
	else
		regval &= ~(DWCEQOS_MAC_RX_CTRL_RFE);

	dwceqos_writel(pdata, DWCEQOS_MAC_RX_FLOW_CTRL, regval);
	return 0;
}

static int disable_tx_flow_ctrl(struct dwc_eqos_prv_data *pdata, unsigned int qinx)
{
	control_tx_flow_ctrl(pdata, qinx, 0);

	return 0;
}

static int enable_tx_flow_ctrl(struct dwc_eqos_prv_data *pdata, unsigned int qinx)
{
	control_tx_flow_ctrl(pdata, qinx, 1);

	return 0;
}

static int disable_rx_flow_ctrl(struct dwc_eqos_prv_data *pdata)
{
	control_rx_flow_ctrl(pdata, 0);

	return 0;
}

static int enable_rx_flow_ctrl(struct dwc_eqos_prv_data *pdata)
{
	control_rx_flow_ctrl(pdata, 1);

	return 0;
}

static int stop_dma_rx(struct dwc_eqos_prv_data *pdata, unsigned int qinx)
{
	unsigned long retrycount = 10;
	unsigned long vy_count;
	volatile unsigned long vardma_dsr0;
	volatile unsigned long vardma_dsr1;
	volatile unsigned long vardma_dsr2;
	u32 regval;

	/* issue Rx dma stop command */
	regval = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RX_CTRL(qinx));
	regval &= ~(DWCEQOS_DMA_CH_RC_SR);
	dwceqos_writel(pdata, DWCEQOS_DMA_CH_RX_CTRL(qinx), regval);

	/* wait for Rx DMA to stop, ie wait till Rx DMA
	 * goes in either Running or Suspend state.
	 */
	if (qinx == 0) {
		/*Poll*/
		vy_count = 0;

		while (1) {
			if (vy_count > retrycount) {
				pr_alert("ERROR: Rx Channel 0 stop failed");
				pr_alert("DSR0 = %#lx\n", vardma_dsr0);
				return -1;
			}

			vardma_dsr0 = dwceqos_readl(pdata,
						    DWCEQOS_DMA_DBG_STAT0);
			if ((GET_VALUE(vardma_dsr0, DMA_DSR0_RPS0_LPOS,
				       DMA_DSR0_RPS0_HPOS) == 0x3) ||
			   (GET_VALUE(vardma_dsr0, DMA_DSR0_RPS0_LPOS,
				       DMA_DSR0_RPS0_HPOS) == 0x4) ||
			   (GET_VALUE(vardma_dsr0, DMA_DSR0_RPS0_LPOS,
				      DMA_DSR0_RPS0_HPOS) == 0x0)) {
				break;
			}
			vy_count++;
			mdelay(1);
		}
	} else if (qinx == 1) {
		/*Poll*/
		vy_count = 0;

		while (1) {
			if (vy_count > retrycount) {
				pr_alert("ERROR: Rx Channel 1 stop failed");
				pr_alert("DSR0 = %#lx\n", vardma_dsr0);
				return -1;
			}

			vardma_dsr0 = dwceqos_readl(pdata,
						    DWCEQOS_DMA_DBG_STAT0);
			if ((GET_VALUE(vardma_dsr0, DMA_DSR0_RPS1_LPOS,
				       DMA_DSR0_RPS1_HPOS) == 0x3) ||
			    (GET_VALUE(vardma_dsr0, DMA_DSR0_RPS1_LPOS,
				      DMA_DSR0_RPS1_HPOS) == 0x4) ||
			    (GET_VALUE(vardma_dsr0, DMA_DSR0_RPS1_LPOS,
				      DMA_DSR0_RPS1_HPOS) == 0x0)) {
				break;
			}
			vy_count++;
			mdelay(1);
		}
	} else if (qinx == 2) {
		/*Poll*/
		vy_count = 0;

		while (1) {
			if (vy_count > retrycount) {
				pr_info("ERROR: Rx Channel 2 stop failed");
				pr_info("DSR0 = %#lx\n", vardma_dsr0);
				return -1;
			}

			vardma_dsr0 = dwceqos_readl(pdata,
						    DWCEQOS_DMA_DBG_STAT0);
			if ((GET_VALUE(vardma_dsr0, DMA_DSR0_RPS2_LPOS,
				       DMA_DSR0_RPS2_HPOS) == 0x3) ||
			    (GET_VALUE(vardma_dsr0, DMA_DSR0_RPS2_LPOS,
				       DMA_DSR0_RPS2_HPOS) == 0x4) ||
			    (GET_VALUE(vardma_dsr0, DMA_DSR0_RPS2_LPOS,
				       DMA_DSR0_RPS2_HPOS) == 0x0)) {
				break;
			}
			vy_count++;
			mdelay(1);
		}
	} else if (qinx == 3) {
		/*Poll*/
		vy_count = 0;
		while (1) {
			if (vy_count > retrycount) {
				pr_alert("ERROR: Rx Channel 3 stop failed");
				pr_alert(" DSR0 = %#lx\n", vardma_dsr1);
				return -1;
			}

			vardma_dsr1 = dwceqos_readl(pdata,
						    DWCEQOS_DMA_DBG_STAT1);
			if ((GET_VALUE(vardma_dsr1, DMA_DSR1_RPS3_LPOS,
				       DMA_DSR1_RPS3_HPOS) == 0x3) ||
			    (GET_VALUE(vardma_dsr1, DMA_DSR1_RPS3_LPOS,
				       DMA_DSR1_RPS3_HPOS) == 0x4) ||
			    (GET_VALUE(vardma_dsr1, DMA_DSR1_RPS3_LPOS,
				       DMA_DSR1_RPS3_HPOS) == 0x0)) {
				break;
			}
			vy_count++;
			mdelay(1);
		}
	} else if (qinx == 4) {
		/*Poll*/
		vy_count = 0;
		while (1) {
			if (vy_count > retrycount) {
				pr_alert("ERROR: Rx Channel 4 stop failed");
				pr_alert(" DSR0 = %#lx\n", vardma_dsr1);
				return -1;
			}

			vardma_dsr1 = dwceqos_readl(pdata,
						    DWCEQOS_DMA_DBG_STAT1);
			if ((GET_VALUE(vardma_dsr1, DMA_DSR1_RPS4_LPOS,
				       DMA_DSR1_RPS4_HPOS) == 0x3) ||
			    (GET_VALUE(vardma_dsr1, DMA_DSR1_RPS4_LPOS,
				       DMA_DSR1_RPS4_HPOS) == 0x4) ||
			    (GET_VALUE(vardma_dsr1, DMA_DSR1_RPS4_LPOS,
				       DMA_DSR1_RPS4_HPOS) == 0x0)) {
				break;
			}
			vy_count++;
			mdelay(1);
		}
	} else if (qinx == 5) {
		/*Poll*/
		vy_count = 0;
		while (1) {
			if (vy_count > retrycount) {
				pr_alert("ERROR: Rx Channel 5 stop failed");
				pr_alert(" DSR0 = %#lx\n", vardma_dsr1);
				return -1;
			}

			vardma_dsr1 = dwceqos_readl(pdata,
						    DWCEQOS_DMA_DBG_STAT1);
			if ((GET_VALUE(vardma_dsr1, DMA_DSR1_RPS5_LPOS,
				       DMA_DSR1_RPS5_HPOS) == 0x3) ||
			    (GET_VALUE(vardma_dsr1, DMA_DSR1_RPS5_LPOS,
				       DMA_DSR1_RPS5_HPOS) == 0x4) ||
			    (GET_VALUE(vardma_dsr1, DMA_DSR1_RPS5_LPOS,
				       DMA_DSR1_RPS5_HPOS) == 0x0)) {
				break;
			}
			vy_count++;
			mdelay(1);
		}
	} else if (qinx == 6) {
		/*Poll*/
		vy_count = 0;
		while (1) {
			if (vy_count > retrycount) {
				pr_alert("ERROR: Rx Channel 6 stop failed");
				pr_alert(" DSR0 = %#lx\n", vardma_dsr1);
				return -1;
			}

			vardma_dsr1 = dwceqos_readl(pdata,
						    DWCEQOS_DMA_DBG_STAT1);
			if ((GET_VALUE(vardma_dsr1, DMA_DSR1_RPS6_LPOS,
				       DMA_DSR1_RPS6_HPOS) == 0x3) ||
			    (GET_VALUE(vardma_dsr1, DMA_DSR1_RPS6_LPOS,
				       DMA_DSR1_RPS6_HPOS) == 0x4) ||
			    (GET_VALUE(vardma_dsr1, DMA_DSR1_RPS6_LPOS,
				       DMA_DSR1_RPS6_HPOS) == 0x0)) {
				break;
			}
			vy_count++;
			mdelay(1);
		}
	} else if (qinx == 7) {
		/*Poll*/
		vy_count = 0;
		while (1) {
			if (vy_count > retrycount) {
				pr_alert("ERROR: Rx Channel 7 stop failed");
				pr_alert(" DSR0 = %#lx\n", vardma_dsr2);
				return -1;
			}

			vardma_dsr2 = dwceqos_readl(pdata,
						    DWCEQOS_DMA_DBG_STAT2);
			if ((GET_VALUE(vardma_dsr2, DMA_DSR2_RPS7_LPOS,
				       DMA_DSR2_RPS7_HPOS) == 0x3) ||
			    (GET_VALUE(vardma_dsr2, DMA_DSR2_RPS7_LPOS,
				       DMA_DSR2_RPS7_HPOS) == 0x4) ||
			    (GET_VALUE(vardma_dsr2, DMA_DSR2_RPS7_LPOS,
				       DMA_DSR2_RPS7_HPOS) == 0x0)) {
				break;
			}
			vy_count++;
			mdelay(1);
		}
	}

	return 0;
}

static int start_dma_rx(struct dwc_eqos_prv_data *pdata, unsigned int qinx)
{
	u32 regval;

	regval = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RX_CTRL(qinx));
	regval &= ~(DWCEQOS_DMA_CH_RC_SR);
	regval |= DWCEQOS_DMA_CH_RC_SR;
	dwceqos_writel(pdata, DWCEQOS_DMA_CH_RX_CTRL(qinx), regval);

	return 0;
}

static int stop_dma_tx(struct dwc_eqos_prv_data *pdata, unsigned int qinx)
{
	unsigned long retrycount = 10;
	unsigned long vy_count;
	volatile unsigned long vardma_dsr0;
	volatile unsigned long vardma_dsr1;
	volatile unsigned long vardma_dsr2;
	u32 regval;

	/* issue Tx dma stop command */
	regval = dwceqos_readl(pdata, DWCEQOS_DMA_CH_TX_CTRL(qinx));
	regval &= ~(DWCEQOS_DMA_CH_TC_ST);
	dwceqos_writel(pdata, DWCEQOS_DMA_CH_TX_CTRL(qinx), regval);

	/* wait for Tx DMA to stop, ie wait till Tx DMA
	 * goes in Suspend state or stopped state.
	 */
	if (qinx == 0) {
		/*Poll*/
		vy_count = 0;
		while (1) {
			if (vy_count > retrycount) {
				pr_alert("ERROR: Channel 0 stop failed");
				pr_alert("DSR0 = %lx\n", vardma_dsr0);
				return -1;
			}

			vardma_dsr0 = dwceqos_readl(pdata,
						    DWCEQOS_DMA_DBG_STAT0);
			if ((GET_VALUE(vardma_dsr0, DMA_DSR0_TPS0_LPOS,
				       DMA_DSR0_TPS0_HPOS) == 0x6) ||
			    (GET_VALUE(vardma_dsr0, DMA_DSR0_TPS0_LPOS,
				       DMA_DSR0_TPS0_HPOS) == 0x0)) {
				break;
			}
			vy_count++;
			mdelay(1);
		}
	} else if (qinx == 1) {
		/*Poll*/
		vy_count = 0;
		while (1) {
			if (vy_count > retrycount) {
				pr_alert("ERROR: Channel 1 stop failed");
				pr_alert("DSR0 = %lx\n", vardma_dsr0);
				return -1;
			}

			vardma_dsr0 = dwceqos_readl(pdata,
						    DWCEQOS_DMA_DBG_STAT0);
			if ((GET_VALUE(vardma_dsr0, DMA_DSR0_TPS1_LPOS,
				       DMA_DSR0_TPS1_HPOS) == 0x6) ||
			    (GET_VALUE(vardma_dsr0, DMA_DSR0_TPS1_LPOS,
				       DMA_DSR0_TPS1_HPOS) == 0x0)) {
				break;
			}
			vy_count++;
			mdelay(1);
		}
	} else if (qinx == 2) {
		/*Poll*/
		vy_count = 0;
		while (1) {
			if (vy_count > retrycount) {
				pr_alert("ERROR: Channel 2 stop failed");
				pr_alert("DSR0 = %lx\n", vardma_dsr0);
				return -1;
			}

			vardma_dsr0 = dwceqos_readl(pdata,
						    DWCEQOS_DMA_DBG_STAT0);
			if ((GET_VALUE(vardma_dsr0, DMA_DSR0_TPS2_LPOS,
				       DMA_DSR0_TPS2_HPOS) == 0x6) ||
			    (GET_VALUE(vardma_dsr0, DMA_DSR0_TPS2_LPOS,
				       DMA_DSR0_TPS2_HPOS) == 0x0)) {
				break;
			}
			vy_count++;
			mdelay(1);
		}
	} else if (qinx == 3) {
		/*Poll*/
		vy_count = 0;
		while (1) {
			if (vy_count > retrycount) {
				pr_alert("ERROR: Channel 3 stop failed");
				pr_alert("DSR0 = %lx\n", vardma_dsr1);
				return -1;
			}

			vardma_dsr1 = dwceqos_readl(pdata,
						    DWCEQOS_DMA_DBG_STAT1);
			if ((GET_VALUE(vardma_dsr1, DMA_DSR1_TPS3_LPOS,
				       DMA_DSR1_TPS3_HPOS) == 0x6) ||
			    (GET_VALUE(vardma_dsr1, DMA_DSR1_TPS3_LPOS,
				       DMA_DSR1_TPS3_HPOS) == 0x0)) {
				break;
			}
			vy_count++;
			mdelay(1);
		}
	} else if (qinx == 4) {
		/*Poll*/
		vy_count = 0;
		while (1) {
			if (vy_count > retrycount) {
				pr_alert("ERROR: Channel 4 stop failed");
				pr_alert("DSR0 = %lx\n", vardma_dsr1);
				return -1;
			}

			vardma_dsr1 = dwceqos_readl(pdata,
						    DWCEQOS_DMA_DBG_STAT1);
			if ((GET_VALUE(vardma_dsr1, DMA_DSR1_TPS4_LPOS,
				       DMA_DSR1_TPS4_HPOS) == 0x6) ||
			    (GET_VALUE(vardma_dsr1, DMA_DSR1_TPS4_LPOS,
				       DMA_DSR1_TPS4_HPOS) == 0x0)) {
				break;
			}
			vy_count++;
			mdelay(1);
		}
	} else if (qinx == 5) {
		/*Poll*/
		vy_count = 0;
		while (1) {
			if (vy_count > retrycount) {
				pr_alert("ERROR: Channel 5 stop failed");
				pr_alert("DSR0 = %lx\n", vardma_dsr1);
				return -1;
			}

			vardma_dsr1 = dwceqos_readl(pdata,
						    DWCEQOS_DMA_DBG_STAT1);
			if ((GET_VALUE(vardma_dsr1, DMA_DSR1_TPS5_LPOS,
				       DMA_DSR1_TPS5_HPOS) == 0x6) ||
			    (GET_VALUE(vardma_dsr1, DMA_DSR1_TPS5_LPOS,
				       DMA_DSR1_TPS5_HPOS) == 0x0)) {
				break;
			}
			vy_count++;
			mdelay(1);
		}
	} else if (qinx == 6) {
		/*Poll*/
		vy_count = 0;
		while (1) {
			if (vy_count > retrycount) {
				pr_alert("ERROR: Channel 6 stop failed");
				pr_alert("DSR0 = %lx\n", vardma_dsr1);
				return -1;
			}

			vardma_dsr1 = dwceqos_readl(pdata,
						    DWCEQOS_DMA_DBG_STAT1);
			if ((GET_VALUE(vardma_dsr1, DMA_DSR1_TPS6_LPOS,
				       DMA_DSR1_TPS6_HPOS) == 0x6) ||
			    (GET_VALUE(vardma_dsr1, DMA_DSR1_TPS6_LPOS,
				       DMA_DSR1_TPS6_HPOS) == 0x0)) {
				break;
			}
			vy_count++;
			mdelay(1);
		}
	} else if (qinx == 7) {
		/*Poll*/
		vy_count = 0;
		while (1) {
			if (vy_count > retrycount) {
				pr_alert("ERROR: Channel 7 stop failed");
				pr_alert("DSR0 = %lx\n", vardma_dsr2);
				return -1;
			}

			vardma_dsr2 = dwceqos_readl(pdata,
						    DWCEQOS_DMA_DBG_STAT2);
			if ((GET_VALUE(vardma_dsr2, DMA_DSR2_TPS7_LPOS,
				       DMA_DSR2_TPS7_HPOS) == 0x6) ||
			    (GET_VALUE(vardma_dsr2, DMA_DSR2_TPS7_LPOS,
				       DMA_DSR2_TPS7_HPOS) == 0x0)) {
				break;
			}
			vy_count++;
			mdelay(1);
		}
	}

	return 0;
}

static int start_dma_tx(struct dwc_eqos_prv_data *pdata, unsigned int qinx)
{
	u32 regval;

	regval = dwceqos_readl(pdata, DWCEQOS_DMA_CH_TX_CTRL(qinx));
	regval &= ~(DWCEQOS_DMA_CH_TC_ST);
	regval |= DWCEQOS_DMA_CH_TC_ST;
	dwceqos_writel(pdata, DWCEQOS_DMA_CH_TX_CTRL(qinx), regval);

	return 0;
}

static int stop_mac_tx_rx(struct net_device *dev)
{
	struct dwc_eqos_prv_data *pdata = netdev_priv(dev);
	u32 regval;

	regval = dwceqos_readl(pdata, DWCEQOS_MAC_CFG);
	regval &= ~(DWCEQOS_MAC_CFG_TE);
	regval &= ~(DWCEQOS_MAC_CFG_RE);
	dwceqos_writel(pdata, DWCEQOS_MAC_CFG, regval);

	return 0;
}

static int start_mac_tx_rx(struct net_device *dev)
{
	struct dwc_eqos_prv_data *pdata = netdev_priv(dev);
	u32 regval;

	regval = dwceqos_readl(pdata, DWCEQOS_MAC_CFG);
	regval |= (DWCEQOS_MAC_CFG_TE | DWCEQOS_MAC_CFG_RE);
	dwceqos_writel(pdata, DWCEQOS_MAC_CFG, regval);

	return 0;
}

static int enable_dma_interrupts(struct dwc_eqos_prv_data *pdata,
				 unsigned int qinx)
{
	unsigned int tmp;
	unsigned long vardma_sr;
	unsigned long vardma_ier;

	/* clear all the interrupts which are set */
	vardma_sr = dwceqos_readl(pdata, DWCEQOS_DMA_CH_STAT(qinx));
	tmp = vardma_sr;
	dwceqos_writel(pdata, DWCEQOS_DMA_CH_STAT(qinx), tmp);
	/* Enable following interrupts for Queue 0 */
	/* TXSE - Transmit Stopped Enable */
	/* RIE - Receive Interrupt Enable */
	/* RBUE - Receive Buffer Unavailable Enable  */
	/* RSE - Receive Stopped Enable */
	/* AIE - Abnormal Interrupt Summary Enable */
	/* NIE - Normal Interrupt Summary Enable */
	/* FBE - Fatal Bus Error Enable */
	vardma_ier = dwceqos_readl(pdata, DWCEQOS_DMA_CH_INTR_EN(qinx));
	vardma_ier |= (DWCEQOS_DMA_CH_IE_TXSE |
		       DWCEQOS_DMA_CH_IE_RIE | DWCEQOS_DMA_CH_IE_RBUE |
		       DWCEQOS_DMA_CH_IE_RSE | DWCEQOS_DMA_CH_IE_FBEE |
		       DWCEQOS_DMA_CH_IE_AIE | DWCEQOS_DMA_CH_IE_NIE);

	/* TIE - Transmit Interrupt Enable */
	vardma_ier |= DWCEQOS_DMA_CH_IE_TIE;
	dwceqos_writel(pdata, DWCEQOS_DMA_CH_INTR_EN(qinx), vardma_ier);

	return 0;
}

/* This sequence is used to configure the MAC registers for
 * GMII-1000Mbps speed
 */
static int set_gmii_speed(struct net_device *dev)
{
	struct dwc_eqos_prv_data *pdata = netdev_priv(dev);
	u32 regval;

	regval = dwceqos_readl(pdata, DWCEQOS_MAC_CFG);
	regval &= ~(DWCEQOS_MAC_CFG_PS | DWCEQOS_MAC_CFG_FES);
	dwceqos_writel(pdata, DWCEQOS_MAC_CFG, regval);

	return 0;
}

/* This sequence is used to configure the MAC registers for
 * MII-10Mpbs speed
 */
static int set_mii_speed_10(struct net_device *dev)
{
	struct dwc_eqos_prv_data *pdata = netdev_priv(dev);
	u32 regval;

	regval = dwceqos_readl(pdata, DWCEQOS_MAC_CFG);
	regval &= ~(DWCEQOS_MAC_CFG_PS | DWCEQOS_MAC_CFG_FES);
	regval |= DWCEQOS_MAC_CFG_PS;
	dwceqos_writel(pdata, DWCEQOS_MAC_CFG, regval);

	return 0;
}

/* This sequence is used to configure the MAC registers for
 * MII-100Mpbs speed
 */
static int set_mii_speed_100(struct net_device *dev)
{
	struct dwc_eqos_prv_data *pdata = netdev_priv(dev);
	u32 regval;

	regval = dwceqos_readl(pdata, DWCEQOS_MAC_CFG);
	regval &= ~(DWCEQOS_MAC_CFG_PS | DWCEQOS_MAC_CFG_FES);
	regval |= (DWCEQOS_MAC_CFG_PS | DWCEQOS_MAC_CFG_FES);
	dwceqos_writel(pdata, DWCEQOS_MAC_CFG, regval);

	return 0;
}

/* This sequence is used to configure the MAC registers for
 * half duplex mode
 */
static int set_half_duplex(struct net_device *dev)
{
	struct dwc_eqos_prv_data *pdata = netdev_priv(dev);
	u32 regval;

	regval = dwceqos_readl(pdata, DWCEQOS_MAC_CFG);
	regval &= ~(DWCEQOS_MAC_CFG_DM);
	dwceqos_writel(pdata, DWCEQOS_MAC_CFG, regval);

	return 0;
}

/* This sequence is used to configure the MAC registers for
 * full duplex mode
 */
static int set_full_duplex(struct net_device *dev)
{
	struct dwc_eqos_prv_data *pdata = netdev_priv(dev);
	u32 regval;

	regval = dwceqos_readl(pdata, DWCEQOS_MAC_CFG);
	regval &= ~(DWCEQOS_MAC_CFG_DM);
	regval |= DWCEQOS_MAC_CFG_DM;
	dwceqos_writel(pdata, DWCEQOS_MAC_CFG, regval);

	return 0;
}

/* This sequence is used to configure the device in list of
 * multicast mode
 */
static int set_multicast_list_mode(struct dwc_eqos_prv_data *pdata)
{
	u32 regval;

	regval = dwceqos_readl(pdata, DWCEQOS_MAC_PKT_FILT);
	regval &= ~(DWCEQOS_MAC_PKT_FILT_HMC);
	dwceqos_writel(pdata, DWCEQOS_MAC_PKT_FILT, regval);

	return 0;
}

/* This sequence is used to configure the device in unicast mode
 */
static int set_unicast_mode(struct dwc_eqos_prv_data *pdata)
{
	u32 regval;

	regval = dwceqos_readl(pdata, DWCEQOS_MAC_PKT_FILT);
	regval &= ~(DWCEQOS_MAC_PKT_FILT_HUC);
	dwceqos_writel(pdata, DWCEQOS_MAC_PKT_FILT, regval);

	return 0;
}

/* This sequence is used to configure the device in all multicast mode
 */
static int set_all_multicast_mode(struct dwc_eqos_prv_data *pdata)
{
	u32 regval;

	regval = dwceqos_readl(pdata, DWCEQOS_MAC_PKT_FILT);
	regval &= ~(DWCEQOS_MAC_PKT_FILT_PM);
	regval |= DWCEQOS_MAC_PKT_FILT_PM;
	dwceqos_writel(pdata, DWCEQOS_MAC_PKT_FILT, regval);

	return 0;
}

/* This sequence is used to configure the device in promiscuous mode
 */
static int set_promiscuous_mode(struct dwc_eqos_prv_data *pdata)
{
	u32 regval;

	regval = dwceqos_readl(pdata, DWCEQOS_MAC_PKT_FILT);
	regval &= ~(DWCEQOS_MAC_PKT_FILT_PR);
	regval |= DWCEQOS_MAC_PKT_FILT_PR;
	dwceqos_writel(pdata, DWCEQOS_MAC_PKT_FILT, regval);

	return 0;
}

/* This sequence is used to write into phy registers
 */
static int write_phy_regs(struct dwc_eqos_prv_data *pdata,
			  int phy_id, int phy_reg, int phy_reg_data)
{
	unsigned long retrycount = 1000;
	unsigned long vy_count;
	volatile unsigned long varmac_gmiiar;
	u32 regval;

	/* wait for any previous MII read/write operation to complete */

	/*Poll Until Poll Condition */
	vy_count = 0;
	while (1) {
		if (vy_count > retrycount)
			return -1;
		vy_count++;
		varmac_gmiiar = dwceqos_readl(pdata, DWCEQOS_MAC_MDIO_ADDR);
		if (GET_VALUE(varmac_gmiiar, MAC_GMIIAR_GB_LPOS,
			      MAC_GMIIAR_GB_HPOS) == 0)
			break;
		mdelay(1);
	}
	/* write the data */
	regval = dwceqos_readl(pdata, DWCEQOS_MAC_MDIO_DATA);
	regval &= ~(DWCEQOS_MAC_MDIO_DATA_GD(0xffff));
	regval |= DWCEQOS_MAC_MDIO_DATA_GD(phy_reg_data);
	dwceqos_writel(pdata, DWCEQOS_MAC_MDIO_DATA, regval);
	/* initiate the MII write operation by updating desired */
	/* phy address/id (0 - 31) */
	/* phy register offset */
	/* CSR Clock Range (20 - 35MHz) */
	/* Select write operation */
	/* set busy bit */
	varmac_gmiiar = dwceqos_readl(pdata, DWCEQOS_MAC_MDIO_ADDR);
	varmac_gmiiar &= ~(DWCEQOS_MAC_MDIO_ADDR_GOC_1);
	varmac_gmiiar &= ~(DWCEQOS_MAC_MDIO_ADDR_NTC(0x7));
	varmac_gmiiar &= ~(DWCEQOS_MAC_MDIO_ADDR_BTB);
	varmac_gmiiar &= ~(DWCEQOS_MAC_MDIO_ADDR_PSE);
	varmac_gmiiar &= ~(DWCEQOS_MAC_MDIO_ADDR_PA(0x1f));
	varmac_gmiiar |= DWCEQOS_MAC_MDIO_ADDR_PA(phy_id);
	varmac_gmiiar &= ~(DWCEQOS_MAC_MDIO_ADDR_RDA(0x1f));
	varmac_gmiiar |= DWCEQOS_MAC_MDIO_ADDR_RDA(phy_reg);
	varmac_gmiiar &= ~(DWCEQOS_MAC_MDIO_ADDR_CR(0xf));
	varmac_gmiiar |= DWCEQOS_MAC_MDIO_ADDR_CR(0x2);
	varmac_gmiiar |= DWCEQOS_MAC_MDIO_ADDR_GOC_0;
	varmac_gmiiar |= DWCEQOS_MAC_MDIO_ADDR_GB;
	dwceqos_writel(pdata, DWCEQOS_MAC_MDIO_ADDR, varmac_gmiiar);

	/*DELAY IMPLEMENTATION USING udelay() */
	udelay(10);
	/* wait for MII write operation to complete */

	/*Poll Until Poll Condition */
	vy_count = 0;
	while (1) {
		if (vy_count > retrycount)
			return -1;
		vy_count++;
		varmac_gmiiar = dwceqos_readl(pdata, DWCEQOS_MAC_MDIO_ADDR);
		if (GET_VALUE(varmac_gmiiar, MAC_GMIIAR_GB_LPOS,
			      MAC_GMIIAR_GB_HPOS) == 0)
			break;
		mdelay(1);
	}

	return 0;
}

/* This sequence is used to read the phy registers
 */
static int read_phy_regs(struct dwc_eqos_prv_data *pdata,
			 int phy_id, int phy_reg, int *phy_reg_data)
{
	unsigned long retrycount = 1000;
	unsigned long vy_count;
	volatile unsigned long varmac_gmiiar;
	unsigned long varmac_gmiidr;

	/* wait for any previous MII read/write operation to complete */

	/*Poll Until Poll Condition */
	vy_count = 0;
	while (1) {
		if (vy_count > retrycount)
			return -1;
		vy_count++;
		mdelay(1);
		varmac_gmiiar = dwceqos_readl(pdata, DWCEQOS_MAC_MDIO_ADDR);
		if (GET_VALUE(varmac_gmiiar, MAC_GMIIAR_GB_LPOS,
			      MAC_GMIIAR_GB_HPOS) == 0)
			break;
	}
	/* initiate the MII read operation by updating desired */
	/* phy address/id (0 - 31) */
	/* phy register offset */
	/* CSR Clock Range (20 - 35MHz) */
	/* Select read operation */
	/* set busy bit */

	varmac_gmiiar = dwceqos_readl(pdata, DWCEQOS_MAC_MDIO_ADDR);
	varmac_gmiiar &= ~(DWCEQOS_MAC_MDIO_ADDR_NTC(0x7));
	varmac_gmiiar &= ~(DWCEQOS_MAC_MDIO_ADDR_BTB);
	varmac_gmiiar &= ~(DWCEQOS_MAC_MDIO_ADDR_PSE);
	varmac_gmiiar &= ~(DWCEQOS_MAC_MDIO_ADDR_PA(0x1f));
	varmac_gmiiar |= DWCEQOS_MAC_MDIO_ADDR_PA(phy_id);
	varmac_gmiiar &= ~(DWCEQOS_MAC_MDIO_ADDR_RDA(0x1f));
	varmac_gmiiar |= DWCEQOS_MAC_MDIO_ADDR_RDA(phy_reg);
	varmac_gmiiar &= ~(DWCEQOS_MAC_MDIO_ADDR_CR(0xf));
	varmac_gmiiar |= DWCEQOS_MAC_MDIO_ADDR_CR(0x2);
	varmac_gmiiar |= DWCEQOS_MAC_MDIO_ADDR_GOC_1;
	varmac_gmiiar |= DWCEQOS_MAC_MDIO_ADDR_GOC_0;
	varmac_gmiiar |= DWCEQOS_MAC_MDIO_ADDR_GB;
	dwceqos_writel(pdata, DWCEQOS_MAC_MDIO_ADDR, varmac_gmiiar);

	/*DELAY IMPLEMENTATION USING udelay() */
	udelay(10);
	/* wait for MII write operation to complete */

	/*Poll Until Poll Condition */
	vy_count = 0;
	while (1) {
		if (vy_count > retrycount)
			return -1;
		vy_count++;
		mdelay(1);
		varmac_gmiiar = dwceqos_readl(pdata, DWCEQOS_MAC_MDIO_ADDR);
		if (GET_VALUE(varmac_gmiiar, MAC_GMIIAR_GB_LPOS,
			      MAC_GMIIAR_GB_HPOS) == 0)
			break;
	}
	/* read the data */
	varmac_gmiidr = dwceqos_readl(pdata, DWCEQOS_MAC_MDIO_DATA);
	*phy_reg_data =
		GET_VALUE(varmac_gmiidr, MAC_GMIIDR_GD_LPOS,
			  MAC_GMIIDR_GD_HPOS);

	return 0;
}

/* This sequence is used to check whether transmiitted pkts have
 * fifo under run loss error or not, returns 1 if fifo under run error
 * else returns 0
 */
static int tx_fifo_underrun(t_tx_normal_desc *txdesc)
{
	unsigned int vartdes3;

	/* check TDES3.UF bit */
	vartdes3 = txdesc->tdes3;
	if ((vartdes3 & 0x4) == 0x4)
		return 1;
	else
		return 0;
}

/* This sequence is used to check whether transmitted pkts have
 * carrier loss error or not, returns 1 if carrier loss error else returns 0
 */
static int tx_carrier_lost_error(t_tx_normal_desc *txdesc)
{
	unsigned int vartdes3;

	/* check TDES3.LoC and tdes3.NC bits */
	vartdes3 = txdesc->tdes3;
	if (((vartdes3 & 0x800) == 0x800) || ((vartdes3 & 0x400) == 0x400))
		return 1;
	else
		return 0;
}

/* This sequence is used to check whether transmission is aborted
 * or not returns 1 if transmission is aborted else returns 0
 */
static int tx_aborted_error(t_tx_normal_desc *txdesc)
{
	unsigned int vartdes3;

	/* check for TDES3.LC and TDES3.EC */
	vartdes3 = txdesc->tdes3;
	if (((vartdes3 & 0x200) == 0x200) || ((vartdes3 & 0x100) == 0x100))
		return 1;
	else
		return 0;
}

/* This sequence is used to check whether the pkt transmitted is
 * successful or not, returns 1 if transmission is success else returns 0
 */
static int tx_complete(t_tx_normal_desc *txdesc)
{
	unsigned int varown;

	TX_NORMAL_DESC_TDES3_OWN_MLF_RD(txdesc->tdes3, varown);
	if (varown == 0)
		return 1;
	else
		return 0;
}

/* This sequence is used to check whethet rx csum is enabled/disabled
 * returns 1 if rx csum is enabled else returns 0
 */
static int get_rx_csum_status(struct net_device *dev)
{
	struct dwc_eqos_prv_data *pdata = netdev_priv(dev);
	u32 regval;

	regval = dwceqos_readl(pdata, DWCEQOS_MAC_CFG);
	regval = ((regval >> 27) & 0x1);

	return regval;
}

/* This sequence is used to disable the rx csum
 */
static int disable_rx_csum(struct net_device *dev)
{
	struct dwc_eqos_prv_data *pdata = netdev_priv(dev);
	u32 regval;

	regval = dwceqos_readl(pdata, DWCEQOS_MAC_CFG);
	regval &= ~(DWCEQOS_MAC_CFG_IPC);
	dwceqos_writel(pdata, DWCEQOS_MAC_CFG, regval);

	return 0;
}

/* This sequence is used to enable the rx csum
 */
static int enable_rx_csum(struct net_device *dev)
{
	struct dwc_eqos_prv_data *pdata = netdev_priv(dev);
	u32 regval;

	regval = dwceqos_readl(pdata, DWCEQOS_MAC_CFG);
	regval &= ~(DWCEQOS_MAC_CFG_IPC);
	regval |= DWCEQOS_MAC_CFG_IPC;
	dwceqos_writel(pdata, DWCEQOS_MAC_CFG, regval);

	return 0;
}

/* This sequence is used to reinitialize the TX descriptor fields,
 * so that device can reuse the descriptors
 */
static int tx_descriptor_reset(unsigned int idx, struct dwc_eqos_prv_data *pdata,
			       unsigned int qinx)
{
	struct s_tx_normal_desc *tx_normal_desc =
		GET_TX_DESC_PTR(qinx, idx);

	DBGPR("-->tx_descriptor_reset\n");

	/* update buffer 1 address pointer to zero */
	tx_normal_desc->tdes0 = 0;
	/* update buffer 2 address pointer to zero */
	tx_normal_desc->tdes1 = 0;
	/* set all other control bits (IC, TTSE, B2L & B1L) to zero */
	tx_normal_desc->tdes2 = 0;
	/* set all other control bits (OWN, CTXT, FD, LD, CPC, CIC etc) to zero */
	tx_normal_desc->tdes3 = 0;

	DBGPR("<--tx_descriptor_reset\n");

	return 0;
}

/* This sequence is used to reinitialize the RX descriptor fields,
 * so that device can reuse the descriptors
 */
static void rx_descriptor_reset(unsigned int idx,
				struct dwc_eqos_prv_data *pdata,
				unsigned int inte, unsigned int qinx)
{
	struct dwc_eqos_rx_buffer *buffer = GET_RX_BUF_PTR(qinx, idx);
	struct s_rx_normal_desc *rx_normal_desc = GET_RX_DESC_PTR(qinx, idx);

	DBGPR("-->rx_descriptor_reset\n");

	memset(rx_normal_desc, 0, sizeof(struct s_rx_normal_desc));
	/* update buffer 1 address pointer */
	rx_normal_desc->rdes0 = buffer->dma;
	/* set to zero */
	rx_normal_desc->rdes1 = 0;

	if ((pdata->ndev->mtu > DWC_EQOS_ETH_FRAME_LEN) ||
	    (pdata->rx_split_hdr == 1)) {
		/* update buffer 2 address pointer */
		rx_normal_desc->rdes2 = buffer->dma2;
		/* set control bits - OWN, INTE, BUF1V and BUF2V */
		rx_normal_desc->rdes3 = (0x83000000 | inte);
	} else {
		/* set buffer 2 address pointer to zero */
		rx_normal_desc->rdes2 = 0;
		/* set control bits - OWN, INTE and BUF1V */
		rx_normal_desc->rdes3 = (0x81000000 | inte);
	}

	DBGPR("<--rx_descriptor_reset\n");
}

/* This sequence is used to initialize the rx descriptors.
 */
static void rx_descriptor_init(struct dwc_eqos_prv_data *pdata,
			       unsigned int qinx)
{
	struct dwc_eqos_rx_wrapper_descriptor *rx_desc_data =
		GET_RX_WRAPPER_DESC(qinx);
	struct dwc_eqos_rx_buffer *buffer =
		GET_RX_BUF_PTR(qinx, rx_desc_data->cur_rx);
	struct s_rx_normal_desc *rx_normal_desc =
		GET_RX_DESC_PTR(qinx, rx_desc_data->cur_rx);
	int i;
	int start_index = rx_desc_data->cur_rx;
	int last_index;

	DBGPR("-->rx_descriptor_init\n");

	/* initialize all desc */

	for (i = 0; i < RX_DESC_CNT; i++) {
		memset(rx_normal_desc, 0, sizeof(struct s_rx_normal_desc));
		/* update buffer 1 address pointer */
		rx_normal_desc->rdes0 = buffer->dma;
		/* set to zero  */
		rx_normal_desc->rdes1 = 0;

		if ((pdata->ndev->mtu > DWC_EQOS_ETH_FRAME_LEN) ||
		    (pdata->rx_split_hdr == 1)) {
			/* update buffer 2 address pointer */
			rx_normal_desc->rdes2 = buffer->dma2;
			/* set control bits - OWN, INTE, BUF1V and BUF2V */
			rx_normal_desc->rdes3 = (0xc3000000);
		} else {
			/* set buffer 2 address pointer to zero */
			rx_normal_desc->rdes2 = 0;
			/* set control bits - OWN, INTE and BUF1V */
			rx_normal_desc->rdes3 = (0xc1000000);
		}
		buffer->inte = (1 << 30);

		/* reconfigure INTE bit if RX watchdog timer is enabled */
		if (rx_desc_data->use_riwt) {
			if ((i % rx_desc_data->rx_coal_frames) != 0) {
				unsigned int varrdes3 = 0;

				varrdes3 = rx_normal_desc->rdes3;
				/* reset INTE */
				rx_normal_desc->rdes3 = (varrdes3 & ~(1 << 30));
				buffer->inte = 0;
			}
		}

		INCR_RX_DESC_INDEX(rx_desc_data->cur_rx, 1);
		rx_normal_desc =
			GET_RX_DESC_PTR(qinx, rx_desc_data->cur_rx);
		buffer = GET_RX_BUF_PTR(qinx, rx_desc_data->cur_rx);
	}
	/* update the total no of Rx descriptors count */
	dwceqos_writel(pdata, DWCEQOS_DMA_CH_RXDESC_RING_LEN(qinx),
		       (RX_DESC_CNT - 1));
	/* update the Rx Descriptor Tail Pointer */
	last_index = GET_CURRENT_RCVD_LAST_DESC_INDEX(start_index, 0);
	dwceqos_writel(pdata, DWCEQOS_DMA_CH_RXDESC_TAIL_PTR(qinx),
		       GET_RX_DESC_DMA_ADDR(qinx, last_index));
	/* update the starting address of desc chain/ring */
	dwceqos_writel(pdata, DWCEQOS_DMA_CH_RXDESC_LIST_ADDR(qinx),
		       GET_RX_DESC_DMA_ADDR(qinx, start_index));

	DBGPR("<--rx_descriptor_init\n");
}

/* This sequence is used to initialize the tx descriptors.
 */
static void tx_descriptor_init(struct dwc_eqos_prv_data *pdata,
			       unsigned int qinx)
{
	struct dwc_eqos_tx_wrapper_descriptor *tx_desc_data =
		GET_TX_WRAPPER_DESC(qinx);
	struct s_tx_normal_desc *tx_normal_desc =
		GET_TX_DESC_PTR(qinx, tx_desc_data->cur_tx);
	int i;
	int start_index = tx_desc_data->cur_tx;

	DBGPR("-->tx_descriptor_init\n");

	/* initialze all descriptors. */

	for (i = 0; i < TX_DESC_CNT; i++) {
		/* update buffer 1 address pointer to zero */
		tx_normal_desc->tdes0 = 0;
		/* update buffer 2 address pointer to zero */
		tx_normal_desc->tdes1 = 0;
		/* set all other control bits (IC, TTSE, B2L & B1L) to zero */
		tx_normal_desc->tdes2 = 0;
		/* set all other control bits
		 *  (OWN, CTXT, FD, LD, CPC, CIC etc) to zero
		 */
		tx_normal_desc->tdes3 = 0;

		INCR_TX_DESC_INDEX(tx_desc_data->cur_tx, 1);
		tx_normal_desc = GET_TX_DESC_PTR(qinx, tx_desc_data->cur_tx);
	}
	/* update the total no of Tx descriptors count */
	dwceqos_writel(pdata, DWCEQOS_DMA_CH_TXDESC_RING_LEN(qinx),
		       (TX_DESC_CNT - 1));
	/* update the starting address of desc chain/ring */
	dwceqos_writel(pdata, DWCEQOS_DMA_CH_TXDESC_LIST_ADDR(qinx),
		       GET_TX_DESC_DMA_ADDR(qinx, start_index));

	DBGPR("<--tx_descriptor_init\n");
}

/* This sequence is used to prepare tx descriptor for
 * packet transmission and issue the poll demand command to TxDMA
 */
static void pre_transmit(struct dwc_eqos_prv_data *pdata,
			 unsigned int qinx)
{
	struct dwc_eqos_tx_wrapper_descriptor *tx_desc_data =
		GET_TX_WRAPPER_DESC(qinx);
	struct dwc_eqos_tx_buffer *buffer =
		GET_TX_BUF_PTR(qinx, tx_desc_data->cur_tx);
	struct s_tx_normal_desc *tx_normal_desc =
		GET_TX_DESC_PTR(qinx, tx_desc_data->cur_tx);
	struct s_tx_context_desc *tx_context_desc =
		GET_TX_DESC_PTR(qinx, tx_desc_data->cur_tx);
	unsigned int varcsum_enable;
	unsigned int varvlan_pkt;
	unsigned int varvt;
	int i;
	int start_index = tx_desc_data->cur_tx;
	int last_index, original_start_index = tx_desc_data->cur_tx;
	struct s_tx_pkt_features *tx_pkt_features = &pdata->tx_pkt_features;
	unsigned int vartso_enable = 0;
	unsigned int varmss = 0;
	unsigned int  varpay_len = 0;
	unsigned int vartcp_udp_hdr_len = 0;
	unsigned int varptp_enable = 0;
	int total_len = 0;
	u32 regval;

	DBGPR("-->pre_transmit: qinx = %u\n", qinx);

	if (IS_ENABLED(CONFIG_VLAN_8021Q)) {
		TX_PKT_FEATURES_PKT_ATTRIBUTES_VLAN_PKT_MLF_RD(
				tx_pkt_features->pkt_attributes, varvlan_pkt);
		if (varvlan_pkt == 0x1) {
			/* put vlan tag in contex descriptor and set other control
			 * bits accordingly
			 */
			TX_PKT_FEATURES_VLAN_TAG_VT_MLF_RD(tx_pkt_features->vlan_tag, varvt);
			TX_CONTEXT_DESC_TDES3_VT_MLF_WR(tx_context_desc->tdes3,
							varvt);
			TX_CONTEXT_DESC_TDES3_VLTV_MLF_WR(tx_context_desc->tdes3,
							  0x1);
			TX_CONTEXT_DESC_TDES3_CTXT_MLF_WR(tx_context_desc->tdes3,
							  0x1);
			TX_CONTEXT_DESC_TDES3_OWN_MLF_WR(tx_context_desc->tdes3,
							 0x1);

			original_start_index = tx_desc_data->cur_tx;
			INCR_TX_DESC_INDEX(tx_desc_data->cur_tx, 1);
			start_index = tx_desc_data->cur_tx;
			tx_normal_desc =
				GET_TX_DESC_PTR(qinx, tx_desc_data->cur_tx);
			buffer = GET_TX_BUF_PTR(qinx, tx_desc_data->cur_tx);
		}
	}

#ifdef DWC_EQOS_ENABLE_DVLAN
	if (pdata->via_reg_or_desc == DWC_EQOS_VIA_DESC) {
		/* put vlan tag in contex descriptor and set other control
		 * bits accordingly
		 */

		if (pdata->in_out & DWC_EQOS_DVLAN_OUTER) {
			TX_CONTEXT_DESC_TDES3_VT_MLF_WR(tx_context_desc->tdes3,
							pdata->outer_vlan_tag);
			TX_CONTEXT_DESC_TDES3_VLTV_MLF_WR(tx_context_desc->tdes3,
							  0x1);
			/* operation (insertion/replacement/deletion/none) will be
			 * specified in normal descriptor tdes2
			 */
		}
		if (pdata->in_out & DWC_EQOS_DVLAN_INNER) {
			TX_CONTEXT_DESC_TDES2_IVT_MLF_WR(tx_context_desc->tdes2,
							 pdata->inner_vlan_tag);
			TX_CONTEXT_DESC_TDES3_IVLTV_MLF_WR(tx_context_desc->tdes3, 0x1);
			TX_CONTEXT_DESC_TDES3_IVTIR_MLF_WR(tx_context_desc->tdes3,
							   pdata->op_type);
		}
		TX_CONTEXT_DESC_TDES3_CTXT_MLF_WR(tx_context_desc->tdes3, 0x1);
		TX_CONTEXT_DESC_TDES3_OWN_MLF_WR(tx_context_desc->tdes3, 0x1);

		original_start_index = tx_desc_data->cur_tx;
		INCR_TX_DESC_INDEX(tx_desc_data->cur_tx, 1);
		start_index = tx_desc_data->cur_tx;
		tx_normal_desc = GET_TX_DESC_PTR(qinx, tx_desc_data->cur_tx);
		buffer = GET_TX_BUF_PTR(qinx, tx_desc_data->cur_tx);
	}
#endif /* End of DWC_EQOS_ENABLE_DVLAN */

	/* prepare CONTEXT descriptor for TSO */
	TX_PKT_FEATURES_PKT_ATTRIBUTES_TSO_ENABLE_MLF_RD(
			tx_pkt_features->pkt_attributes, vartso_enable);
	if (vartso_enable && (tx_pkt_features->mss != tx_desc_data->default_mss)) {
		/* get MSS and update */
		TX_PKT_FEATURES_MSS_MSS_MLF_RD(tx_pkt_features->mss, varmss);
		TX_CONTEXT_DESC_TDES2_MSS_MLF_WR(tx_context_desc->tdes2, varmss);
		/* set MSS valid, CTXT and OWN bits */
		TX_CONTEXT_DESC_TDES3_TCMSSV_MLF_WR(tx_context_desc->tdes3, 0x1);
		TX_CONTEXT_DESC_TDES3_CTXT_MLF_WR(tx_context_desc->tdes3, 0x1);
		TX_CONTEXT_DESC_TDES3_OWN_MLF_WR(tx_context_desc->tdes3, 0x1);

		/* DMA uses the MSS value programed in DMA_CR if driver
		 * doesn't provided the CONTEXT descriptor
		 */
		regval = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CTRL(qinx));
		regval &= ~(DWCEQOS_DMA_CH_CTRL_MSS(0xffff));
		regval |= DWCEQOS_DMA_CH_CTRL_MSS(tx_pkt_features->mss);
		dwceqos_writel(pdata, DWCEQOS_DMA_CH_CTRL(qinx), regval);

		tx_desc_data->default_mss = tx_pkt_features->mss;

		original_start_index = tx_desc_data->cur_tx;
		INCR_TX_DESC_INDEX(tx_desc_data->cur_tx, 1);
		start_index = tx_desc_data->cur_tx;
		tx_normal_desc = GET_TX_DESC_PTR(qinx, tx_desc_data->cur_tx);
		buffer = GET_TX_BUF_PTR(qinx, tx_desc_data->cur_tx);
	}

	/* update the first buffer pointer and length */
	tx_normal_desc->tdes0 = buffer->dma;
	TX_NORMAL_DESC_TDES2_HL_B1L_MLF_WR(tx_normal_desc->tdes2, buffer->len);
	if (buffer->dma2 != 0) {
		/* update the second buffer pointer and length */
		tx_normal_desc->tdes1 = buffer->dma2;
		TX_NORMAL_DESC_TDES2_B2L_MLF_WR(tx_normal_desc->tdes2, buffer->len2);
	}

	if (vartso_enable) {
		/* update TCP payload length (only for the descriptor with FD set) */
		varpay_len = tx_pkt_features->pay_len;
		/* TDES3[17:0] will be TCP payload length */
		tx_normal_desc->tdes3 |= varpay_len;
	} else {
		/* update total length of packet */
		GET_TX_TOT_LEN(GET_TX_BUF_PTR(qinx, 0), tx_desc_data->cur_tx,
			       GET_CURRENT_XFER_DESC_CNT(qinx), total_len);
		TX_NORMAL_DESC_TDES3_FL_MLF_WR(tx_normal_desc->tdes3, total_len);
	}

	if (IS_ENABLED(CONFIG_VLAN_8021Q)) {
		/* Insert a VLAN tag with a tag value programmed in MAC Reg 24 or
		 * CONTEXT descriptor
		 */
		if (tx_desc_data->vlan_tag_present && Y_FALSE == tx_desc_data->tx_vlan_tag_via_reg) {
			//printk(KERN_ALERT "VLAN control info update via descriptor\n\n");
			TX_NORMAL_DESC_TDES2_VTIR_MLF_WR(tx_normal_desc->tdes2,
							 tx_desc_data->tx_vlan_tag_ctrl);
		}
	}

#ifdef DWC_EQOS_ENABLE_DVLAN
	if (pdata->via_reg_or_desc == DWC_EQOS_VIA_DESC) {
		if (pdata->in_out & DWC_EQOS_DVLAN_OUTER) {
			TX_NORMAL_DESC_TDES2_VTIR_MLF_WR(tx_normal_desc->tdes2,
							 pdata->op_type);
		}
	}
#endif /* End of DWC_EQOS_ENABLE_DVLAN */

	/* Mark it as First Descriptor */
	TX_NORMAL_DESC_TDES3_FD_MLF_WR(tx_normal_desc->tdes3, 0x1);
	/* Enable CRC and Pad Insertion (NOTE: set this only
	 * for FIRST descriptor)
	 */
	TX_NORMAL_DESC_TDES3_CPC_MLF_WR(tx_normal_desc->tdes3, 0);
	/* Mark it as NORMAL descriptor */
	TX_NORMAL_DESC_TDES3_CTXT_MLF_WR(tx_normal_desc->tdes3, 0);
	/* Enable HW CSUM */
	TX_PKT_FEATURES_PKT_ATTRIBUTES_CSUM_ENABLE_MLF_RD(tx_pkt_features->pkt_attributes,
							  varcsum_enable);
	if (varcsum_enable == 0x1)
		TX_NORMAL_DESC_TDES3_CIC_MLF_WR(tx_normal_desc->tdes3, 0x3);
	/* configure SA Insertion Control */
	TX_NORMAL_DESC_TDES3_SAIC_MLF_WR(tx_normal_desc->tdes3,
					 pdata->tx_sa_ctrl_via_desc);
	if (vartso_enable) {
		/* set TSE bit */
		TX_NORMAL_DESC_TDES3_TSE_MLF_WR(tx_normal_desc->tdes3, 0x1);

		/* update tcp data offset or tcp hdr len */
		vartcp_udp_hdr_len = tx_pkt_features->tcp_udp_hdr_len;
		/* convert to bit value */
		vartcp_udp_hdr_len = vartcp_udp_hdr_len / 4;
		TX_NORMAL_DESC_TDES3_SLOTNUM_TCPHDRLEN_MLF_WR(tx_normal_desc->tdes3, vartcp_udp_hdr_len);
	}

	/* enable timestamping */
	TX_PKT_FEATURES_PKT_ATTRIBUTES_PTP_ENABLE_MLF_RD(tx_pkt_features->pkt_attributes, varptp_enable);
	if (varptp_enable)
		TX_NORMAL_DESC_TDES2_TTSE_MLF_WR(tx_normal_desc->tdes2, 0x1);
	INCR_TX_DESC_INDEX(tx_desc_data->cur_tx, 1);
	tx_normal_desc = GET_TX_DESC_PTR(qinx, tx_desc_data->cur_tx);
	buffer = GET_TX_BUF_PTR(qinx, tx_desc_data->cur_tx);

	for (i = 1; i < GET_CURRENT_XFER_DESC_CNT(qinx); i++) {
		/* update the first buffer pointer and length */
		tx_normal_desc->tdes0 = buffer->dma;
		TX_NORMAL_DESC_TDES2_HL_B1L_MLF_WR(tx_normal_desc->tdes2, buffer->len);
		if (buffer->dma2 != 0) {
			/* update the second buffer pointer and length */
			tx_normal_desc->tdes1 = buffer->dma2;
			TX_NORMAL_DESC_TDES2_B2L_MLF_WR(tx_normal_desc->tdes2, buffer->len2);
		}

		/* set own bit */
		TX_NORMAL_DESC_TDES3_OWN_MLF_WR(tx_normal_desc->tdes3, 0x1);
		/* Mark it as NORMAL descriptor */
		TX_NORMAL_DESC_TDES3_CTXT_MLF_WR(tx_normal_desc->tdes3, 0);

		INCR_TX_DESC_INDEX(tx_desc_data->cur_tx, 1);
		tx_normal_desc = GET_TX_DESC_PTR(qinx, tx_desc_data->cur_tx);
		buffer = GET_TX_BUF_PTR(qinx, tx_desc_data->cur_tx);
	}
	/* Mark it as LAST descriptor */
	last_index =
		GET_CURRENT_XFER_LAST_DESC_INDEX(qinx, start_index, 0);
	tx_normal_desc = GET_TX_DESC_PTR(qinx, last_index);
	TX_NORMAL_DESC_TDES3_LD_MLF_WR(tx_normal_desc->tdes3, 0x1);

	tx_desc_data->tx_coal_cur_desc += tx_desc_data->cur_desc_count;
	/* set Interrupt on Completion for last descriptor or for PTP packets */
	if (varptp_enable ||
	    tx_desc_data->tx_coal_cur_desc >= tx_desc_data->tx_coal_max_desc) {
		TX_NORMAL_DESC_TDES2_IC_MLF_WR(tx_normal_desc->tdes2, 0x1);
		tx_desc_data->tx_coal_cur_desc = 0;
	} else if (tx_desc_data->tx_pkt_queued == tx_desc_data->cur_desc_count) {
		/* Arm the timer for the first packet queued */
		mod_timer(&tx_desc_data->txtimer,
				  DWC_COAL_TIMER(pdata->tx_coal_timer_us));
	}

	/* set OWN bit of FIRST descriptor at end to avoid race condition */
	tx_normal_desc = GET_TX_DESC_PTR(qinx, start_index);
	TX_NORMAL_DESC_TDES3_OWN_MLF_WR(tx_normal_desc->tdes3, 0x1);

	/* make descriptor updates visible before asking hw to send it out */
	wmb();

#ifdef DWC_EQOS_ENABLE_TX_DESC_DUMP
	dump_tx_desc(pdata, original_start_index, (tx_desc_data->cur_tx - 1),
		     1, qinx);
#endif

	/* issue a poll command to Tx DMA by writing address
	 * of next immediate free descriptor
	 */
	last_index = GET_CURRENT_XFER_LAST_DESC_INDEX(qinx, start_index, 1);
	dwceqos_writel(pdata, DWCEQOS_DMA_CH_TXDESC_TAIL_PTR(qinx),
		       GET_TX_DESC_DMA_ADDR(qinx, last_index));

	DBGPR("<--pre_transmit\n");
}

/* This sequence is used to read data from device,
 * it checks whether data is good or bad and updates the errors appropriately
 */
static void device_read(struct dwc_eqos_prv_data *pdata, unsigned int qinx)
{
	struct dwc_eqos_rx_wrapper_descriptor *rx_desc_data =
		GET_RX_WRAPPER_DESC(qinx);
	struct s_rx_normal_desc *rx_normal_desc =
		GET_RX_DESC_PTR(qinx, rx_desc_data->cur_rx);
	unsigned int varown;
	unsigned int vares;
	struct dwc_eqos_rx_buffer *buffer =
		GET_RX_BUF_PTR(qinx, rx_desc_data->cur_rx);
	unsigned int varrs1v;
	unsigned int varippe;
	unsigned int varipcb;
	unsigned int variphe;
	struct s_rx_pkt_features *rx_pkt_features = &pdata->rx_pkt_features;
	unsigned int varrs0v;
	unsigned int varlt;
	unsigned int varrdes0;
	unsigned int varoe;
	struct s_rx_error_counters *rx_error_counters =
		&pdata->rx_error_counters;
	unsigned int varce;
	unsigned int varre;
	unsigned int varld;

	DBGPR("-->device_read: cur_rx = %d\n", rx_desc_data->cur_rx);

	/* check for data availability */
	RX_NORMAL_DESC_RDES3_OWN_MLF_RD(rx_normal_desc->rdes3, varown);
	if (varown == 0) {
		/* check whether it is good packet or bad packet */
		RX_NORMAL_DESC_RDES3_ES_MLF_RD(rx_normal_desc->rdes3, vares);
		RX_NORMAL_DESC_RDES3_LD_MLF_RD(rx_normal_desc->rdes3, varld);

		if ((vares == 0) && (varld == 1)) {
			/* get the packet length */
			RX_NORMAL_DESC_RDES3_FL_MLF_RD(rx_normal_desc->rdes3, buffer->len);
			RX_NORMAL_DESC_RDES3_RS1V_MLF_RD(rx_normal_desc->rdes3, varrs1v);
			if (varrs1v == 0x1) {
				/* check whether device has done csum correctly or not */
				RX_NORMAL_DESC_RDES1_IPPE_MLF_RD(rx_normal_desc->rdes1, varippe);
				RX_NORMAL_DESC_RDES1_IPCB_MLF_RD(rx_normal_desc->rdes1, varipcb);
				RX_NORMAL_DESC_RDES1_IPHE_MLF_RD(rx_normal_desc->rdes1, variphe);
				if ((varippe == 0) && (varipcb == 0) && (variphe == 0)) {
					/* IPC Checksum done */
					RX_PKT_FEATURES_PKT_ATTRIBUTES_CSUM_DONE_MLF_WR(
							rx_pkt_features->pkt_attributes, 0x1);
				}
			}
			if (IS_ENABLED(CONFIG_VLAN_8021Q)) {
				RX_NORMAL_DESC_RDES3_RS0V_MLF_RD(rx_normal_desc->rdes3,
								 varrs0v);
				if (varrs0v == 0x1) {
					/*  device received frame with VLAN Tag or double VLAN Tag ? */
					RX_NORMAL_DESC_RDES3_LT_MLF_RD(rx_normal_desc->rdes3, varlt);
					if ((varlt == 0x4) || (varlt == 0x5)) {
						RX_PKT_FEATURES_PKT_ATTRIBUTES_VLAN_PKT_MLF_WR(
						rx_pkt_features->pkt_attributes, 0x1);
						/* get the VLAN Tag */
						varrdes0 = rx_normal_desc->rdes0;
						RX_PKT_FEATURES_VLAN_TAG_VT_MLF_WR(rx_pkt_features->vlan_tag,
										   (varrdes0 & 0xffff));
					}
				}
			}
		} else {
#ifdef DWC_EQOS_ENABLE_RX_DESC_DUMP
			dump_rx_desc(qinx, rx_normal_desc, rx_desc_data->cur_rx);
#endif
			/* not a good packet, hence check for appropriate errors. */
			RX_NORMAL_DESC_RDES3_OE_MLF_RD(rx_normal_desc->rdes3, varoe);
			if (varoe == 1)
				RX_ERROR_COUNTERS_RX_ERRORS_OVERRUN_ERROR_MLF_WR(rx_error_counters->rx_errors, 1);
			RX_NORMAL_DESC_RDES3_CE_MLF_RD(rx_normal_desc->rdes3, varce);
			if (varce == 1)
				RX_ERROR_COUNTERS_RX_ERRORS_CRC_ERROR_MLF_WR(rx_error_counters->rx_errors, 1);
			RX_NORMAL_DESC_RDES3_RE_MLF_RD(rx_normal_desc->rdes3, varre);
			if (varre == 1)
				RX_ERROR_COUNTERS_RX_ERRORS_FRAME_ERROR_MLF_WR(rx_error_counters->rx_errors, 1);
			RX_NORMAL_DESC_RDES3_LD_MLF_RD(rx_normal_desc->rdes3, varld);
			if (varre == 0)
				RX_ERROR_COUNTERS_RX_ERRORS_OVERRUN_ERROR_MLF_WR(rx_error_counters->rx_errors, 1);
		}
	}

	DBGPR("<--device_read: cur_rx = %d\n", rx_desc_data->cur_rx);
}

static void update_rx_tail_ptr(struct dwc_eqos_prv_data *pdata,
			       unsigned int qinx, unsigned int dma_addr)
{
	dwceqos_writel(pdata, DWCEQOS_DMA_CH_RXDESC_TAIL_PTR(qinx), dma_addr);
}

/* This sequence is used to check whether CTXT bit is
 * set or not returns 1 if CTXT is set else returns zero
 */
static int get_tx_descriptor_ctxt(t_tx_normal_desc *txdesc)
{
	unsigned long varctxt;

	/* check TDES3.CTXT bit */
	TX_NORMAL_DESC_TDES3_CTXT_MLF_RD(txdesc->tdes3, varctxt);
	if (varctxt == 1)
		return 1;
	else
		return 0;
}

/* This sequence is used to check whether LD bit is set or not
 * returns 1 if LD is set else returns zero
 */
static int get_tx_descriptor_last(t_tx_normal_desc *txdesc)
{
	unsigned long varld;

	/* check TDES3.LD bit */
	TX_NORMAL_DESC_TDES3_LD_MLF_RD(txdesc->tdes3, varld);
	if (varld == 1)
		return 1;
	else
		return 0;
}

/* Exit routine
 * Exit function that unregisters the device, deallocates buffers,
 * unbinds the driver from controlling the device etc.
 */

static int dwc_eqos_yexit(struct dwc_eqos_prv_data *pdata)
{
	u32 regval;
	unsigned long retrycount = 1000;
	unsigned long vy_count;
	volatile unsigned long vardma_bmr;

	DBGPR("-->dwc_eqos_yexit\n");

	/*issue a software reset */
	regval = dwceqos_readl(pdata, DWCEQOS_DMA_MODE);
	DBGPR("Done reading reg\n");
	regval |= DWCEQOS_DMA_MODE_SWR;
	dwceqos_writel(pdata, DWCEQOS_DMA_MODE, regval);
	DBGPR("Done writing reg\n");
	/*DELAY IMPLEMENTATION USING udelay() */
	udelay(10);

	/*Poll Until Poll Condition */
	vy_count = 0;
	DBGPR("one");
	while (1) {
		if (vy_count > retrycount) {
			pr_alert("ERROR: eth%d timedout on MAC SW reset", pdata->if_idx);
			return -1;
		}
		vy_count++;
		mdelay(1);
		DBGPR("two");
		vardma_bmr = dwceqos_readl(pdata, DWCEQOS_DMA_MODE);
		if (GET_VALUE(vardma_bmr, DMA_BMR_SWR_LPOS, DMA_BMR_SWR_HPOS) == 0)
			break;
	}

	DBGPR("<--dwc_eqos_yexit\n");

	return 0;
}

/* This API will calculate per queue FIFO size.
 * fifo_size - total fifo size in h/w register
 * queue_count - total queue count
 * returns - fifo size per queue.
 */
static unsigned long calculate_per_queue_fifo(unsigned long fifo_size, unsigned char queue_count)
{
	unsigned long q_fifo_size = 0;	/* calculated fifo size per queue */
	unsigned long p_fifo = EDWC_EQOS_256; /* per queue fifo size programmable value */

	/* calculate Tx/Rx fifo share per queue */
	switch (fifo_size) {
	case 0:
		q_fifo_size = FIFO_SIZE_B(128);
		break;
	case 1:
		q_fifo_size = FIFO_SIZE_B(256);
		break;
	case 2:
		q_fifo_size = FIFO_SIZE_B(512);
		break;
	case 3:
		q_fifo_size = FIFO_SIZE_KB(1);
		break;
	case 4:
		q_fifo_size = FIFO_SIZE_KB(2);
		break;
	case 5:
		q_fifo_size = FIFO_SIZE_KB(4);
		break;
	case 6:
		q_fifo_size = FIFO_SIZE_KB(8);
		break;
	case 7:
		q_fifo_size = FIFO_SIZE_KB(16);
		break;
	case 8:
		q_fifo_size = FIFO_SIZE_KB(32);
		break;
	case 9:
		q_fifo_size = FIFO_SIZE_KB(64);
		break;
	case 10:
		q_fifo_size = FIFO_SIZE_KB(128);
		break;
	case 11:
		q_fifo_size = FIFO_SIZE_KB(256);
		break;
	}

	q_fifo_size = q_fifo_size / queue_count;

	if (q_fifo_size >= FIFO_SIZE_KB(32))
		p_fifo = EDWC_EQOS_32K;
	else if (q_fifo_size >= FIFO_SIZE_KB(16))
		p_fifo = EDWC_EQOS_16K;
	else if (q_fifo_size >= FIFO_SIZE_KB(8))
		p_fifo = EDWC_EQOS_8K;
	else if (q_fifo_size >= FIFO_SIZE_KB(4))
		p_fifo = EDWC_EQOS_4K;
	else if (q_fifo_size >= FIFO_SIZE_KB(2))
		p_fifo = EDWC_EQOS_2K;
	else if (q_fifo_size >= FIFO_SIZE_KB(1))
		p_fifo = EDWC_EQOS_1K;
	else if (q_fifo_size >= FIFO_SIZE_B(512))
		p_fifo = EDWC_EQOS_512;
	else if (q_fifo_size >= FIFO_SIZE_B(256))
		p_fifo = EDWC_EQOS_256;

	return p_fifo;
}

static int configure_mtl_queue(unsigned int qinx, struct dwc_eqos_prv_data *pdata)
{
	struct dwc_eqos_tx_queue *queue_data = &pdata->tx_queue[qinx];
	unsigned long retrycount = 1000;
	unsigned long vy_count;
	volatile unsigned long varmtl_qtomr;
	unsigned int p_rx_fifo = EDWC_EQOS_256, p_tx_fifo = EDWC_EQOS_256;
	u32 regval;

	DBGPR("-->configure_mtl_queue\n");

	/*Flush Tx Queue */
	regval = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_OP_MODE(qinx));
	regval &= ~(DWCEQOS_MTL_TQOM_FTQ);
	regval |= DWCEQOS_MTL_TQOM_FTQ;
	dwceqos_writel(pdata, DWCEQOS_MTL_TQ_OP_MODE(qinx), regval);

	/*Poll Until Poll Condition */
	vy_count = 0;
	while (1) {
		if (vy_count > retrycount)
			return -1;
		vy_count++;
		mdelay(1);
		varmtl_qtomr = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_OP_MODE(qinx));
		if (GET_VALUE(varmtl_qtomr, MTL_QTOMR_FTQ_LPOS, MTL_QTOMR_FTQ_HPOS)
				== 0)
			break;
	}

	/*Enable Store and Forward mode for TX */
	regval = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_OP_MODE(qinx));
	regval &= ~(DWCEQOS_MTL_TQOM_TSF);
	regval |= DWCEQOS_MTL_TQOM_TSF;
	dwceqos_writel(pdata, DWCEQOS_MTL_TQ_OP_MODE(qinx), regval);
	/* Program Tx operating mode */
	regval = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_OP_MODE(qinx));
	regval &= ~(DWCEQOS_MTL_TQOM_QEN(0x3));
	regval |= DWCEQOS_MTL_TQOM_QEN(queue_data->q_op_mode);
	dwceqos_writel(pdata, DWCEQOS_MTL_TQ_OP_MODE(qinx), regval);
	/* Transmit Queue weight */
	regval = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_QW(qinx));
	regval &= ~(DWCEQOS_MTL_TQQW_ISCQW(0x1fffff));
	regval |= DWCEQOS_MTL_TQQW_ISCQW(0x10 + qinx);
	dwceqos_writel(pdata, DWCEQOS_MTL_TQ_QW(qinx), regval);

	regval = dwceqos_readl(pdata, DWCEQOS_MTL_RQ_OP_MODE(qinx));
	regval &= ~(DWCEQOS_MTL_RQOM_FEP);
	regval |= DWCEQOS_MTL_RQOM_FEP;
	dwceqos_writel(pdata, DWCEQOS_MTL_RQ_OP_MODE(qinx), regval);

	/* Configure for Jumbo frame in MTL */
	if (pdata->ndev->mtu > DWC_EQOS_ETH_FRAME_LEN) {
		/* Disable RX Store and Forward mode */
		regval = dwceqos_readl(pdata, DWCEQOS_MTL_RQ_OP_MODE(qinx));
		regval &= ~(DWCEQOS_MTL_RQOM_RSF);
		dwceqos_writel(pdata, DWCEQOS_MTL_RQ_OP_MODE(qinx), regval);
		pr_info("RX is configured in threshold mode and threshold = 64Byte\n");
	}

	p_rx_fifo = calculate_per_queue_fifo(pdata->hw_feat.rx_fifo_size, pdata->rx_queue_cnt);
	p_tx_fifo = calculate_per_queue_fifo(pdata->hw_feat.tx_fifo_size, DWC_EQOS_TX_QUEUE_CNT);

	/* Transmit/Receive queue fifo size programmed */
	regval = dwceqos_readl(pdata, DWCEQOS_MTL_RQ_OP_MODE(qinx));
	regval &= ~(DWCEQOS_MTL_RQOM_RQS(0x3ff));
	regval |= DWCEQOS_MTL_RQOM_RQS(p_rx_fifo);
	dwceqos_writel(pdata, DWCEQOS_MTL_RQ_OP_MODE(qinx), regval);
	regval = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_OP_MODE(qinx));
	regval &= ~(DWCEQOS_MTL_TQOM_TQS(0x3ff));
	regval |= DWCEQOS_MTL_TQOM_TQS(p_tx_fifo);
	dwceqos_writel(pdata, DWCEQOS_MTL_TQ_OP_MODE(qinx), regval);
	pr_info("Queue%d Tx fifo size %d, Rx fifo size %d\n",
		 qinx, ((p_tx_fifo + 1) * 256), ((p_rx_fifo + 1) * 256));

	/* flow control will be used only if
	 * each channel gets 8KB or more fifo
	 */
	if (p_rx_fifo >= EDWC_EQOS_4K) {
		/* Enable Rx FLOW CTRL in MTL and MAC
		 * Programming is valid only if Rx fifo size is greater than
		 *  or equal to 8k
		 */
		if ((pdata->flow_ctrl & DWC_EQOS_FLOW_CTRL_TX) ==
		    DWC_EQOS_FLOW_CTRL_TX) {
			regval = dwceqos_readl(pdata, DWCEQOS_MTL_RQ_OP_MODE(qinx));
			regval &= ~(DWCEQOS_MTL_RQOM_EHFC);
			regval |= DWCEQOS_MTL_RQOM_EHFC;
			dwceqos_writel(pdata, DWCEQOS_MTL_RQ_OP_MODE(qinx), regval);

			/* Set Threshold for Activating Flow Contol space for min 2 frames
			 * ie, (1500 * 1) = 1500 bytes
			 *
			 * Set Threshold for Deactivating Flow Contol for space of
			 * min 1 frame (frame size 1500bytes) in receive fifo
			 */
			if (p_rx_fifo == EDWC_EQOS_4K) {
				/* This violates the above formula because of FIFO size limit
				 * therefore overflow may occur inspite of this
				 */
				regval = dwceqos_readl(pdata, DWCEQOS_MTL_RQ_OP_MODE(qinx));
				regval &= ~(DWCEQOS_MTL_RQOM_RFD(0x3f));
				regval |= DWCEQOS_MTL_RQOM_RFD(0x3); //Full - 3K
				regval &= ~(DWCEQOS_MTL_RQOM_RFA(0x3f));
				regval |= DWCEQOS_MTL_RQOM_RFA(0x1); //Full - 1.5K
				dwceqos_writel(pdata, DWCEQOS_MTL_RQ_OP_MODE(qinx), regval);
			} else if (p_rx_fifo == EDWC_EQOS_8K) {
				regval = dwceqos_readl(pdata, DWCEQOS_MTL_RQ_OP_MODE(qinx));
				regval &= ~(DWCEQOS_MTL_RQOM_RFD(0x3f));
				regval |= DWCEQOS_MTL_RQOM_RFD(0x6); //Full - 4K
				regval &= ~(DWCEQOS_MTL_RQOM_RFA(0x3f));
				regval |= DWCEQOS_MTL_RQOM_RFA(0xA); //Full - 6K
				dwceqos_writel(pdata, DWCEQOS_MTL_RQ_OP_MODE(qinx), regval);
			} else if (p_rx_fifo == EDWC_EQOS_16K) {
				regval = dwceqos_readl(pdata, DWCEQOS_MTL_RQ_OP_MODE(qinx));
				regval &= ~(DWCEQOS_MTL_RQOM_RFD(0x3f));
				regval |= DWCEQOS_MTL_RQOM_RFD(0x6); //Full - 4K
				regval &= ~(DWCEQOS_MTL_RQOM_RFA(0x3f));
				regval |= DWCEQOS_MTL_RQOM_RFA(0x12); //Full - 10K
				dwceqos_writel(pdata, DWCEQOS_MTL_RQ_OP_MODE(qinx), regval);
			} else if (p_rx_fifo == EDWC_EQOS_32K) {
				regval = dwceqos_readl(pdata, DWCEQOS_MTL_RQ_OP_MODE(qinx));
				regval &= ~(DWCEQOS_MTL_RQOM_RFD(0x3f));
				regval |= DWCEQOS_MTL_RQOM_RFD(0x6); //Full - 4K
				regval &= ~(DWCEQOS_MTL_RQOM_RFA(0x3f));
				regval |= DWCEQOS_MTL_RQOM_RFA(0x1E); //Full - 16K
				dwceqos_writel(pdata, DWCEQOS_MTL_RQ_OP_MODE(qinx), regval);
			}

		}
	}

	DBGPR("<--configure_mtl_queue\n");

	return 0;
}

static int configure_dma_channel(unsigned int qinx,
				 struct dwc_eqos_prv_data *pdata)
{
	u32 regval;

	struct dwc_eqos_rx_wrapper_descriptor *rx_desc_data =
		GET_RX_WRAPPER_DESC(qinx);

	DBGPR("-->configure_dma_channel\n");

	/*Enable OSF mode */
	config_osf_mode(pdata, qinx, 1);

	/*Select Rx Buffer size = 2048bytes */
	regval = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RX_CTRL(qinx));
	regval &= ~(DWCEQOS_DMA_CH_RC_RBSZ(0x3fff));
	switch (pdata->rx_buffer_len) {
	case 16384:
		regval |= DWCEQOS_DMA_CH_RC_RBSZ(16384);
		break;
	case 8192:
		regval |= DWCEQOS_DMA_CH_RC_RBSZ(8192);
		break;
	case 4096:
		regval |= DWCEQOS_DMA_CH_RC_RBSZ(4096);
		break;
	default:		/* default is 2K */
		regval |= DWCEQOS_DMA_CH_RC_RBSZ(2048);
		break;
	}
	dwceqos_writel(pdata, DWCEQOS_DMA_CH_RX_CTRL(qinx), regval);
	/* program RX watchdog timer */
	if (rx_desc_data->use_riwt) {
		config_rx_watchdog_timer(pdata, qinx, rx_desc_data->rx_riwt);
	} else {
		dwceqos_writel(pdata, DWCEQOS_DMA_CH_RX_INTR_WD_TIMER(qinx), 0);
	}
	pr_info("%s Rx watchdog timer\n",
		 (rx_desc_data->use_riwt ? "Enabled" : "Disabled"));

	enable_dma_interrupts(pdata, qinx);
	/* set PBLx8 */
	config_pblx8(pdata, qinx, 1);
	/* set TX PBL = 256 */
	config_tx_pbl_val(pdata, qinx, 32);
	/* set RX PBL = 256 */
	config_rx_pbl_val(pdata, qinx, 32);

	/* To get Best Performance */
	regval = dwceqos_readl(pdata, DWCEQOS_DMA_SYSBUS_MODE);
	regval |= DWCEQOS_DMA_SBM_BLEN16;
	regval |= DWCEQOS_DMA_SBM_BLEN8;
	regval |= DWCEQOS_DMA_SBM_BLEN4;
	regval &= ~(DWCEQOS_DMA_SBM_RD_OSR_LMT(0xf));
	regval |= DWCEQOS_DMA_SBM_RD_OSR_LMT(0x2);
	/* Disable enhanced address mode to be able to use both
	* buf1 and buf2 pointer in TDES0. This is required for
	* supporting jumbo frame xmit without needing to use
	* multiple chained tx descriptors.
	*/
	regval &= ~(DWCEQOS_DMA_SBM_EAME);
	dwceqos_writel(pdata, DWCEQOS_DMA_SYSBUS_MODE, regval);

	/* program split header mode */
	regval = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CTRL(qinx));
	if (pdata->rx_split_hdr) {
		regval &= ~(DWCEQOS_DMA_CH_CTRL_SPH);
		regval |= DWCEQOS_DMA_CH_CTRL_SPH;
	} else {
		regval &= ~(DWCEQOS_DMA_CH_CTRL_SPH);
	}
	dwceqos_writel(pdata, DWCEQOS_DMA_CH_CTRL(qinx), regval);
	pr_info("%s Rx Split header mode\n",
		 (pdata->rx_split_hdr ? "Enabled" : "Disabled"));

	/* start TX DMA */
	start_dma_tx(pdata, qinx);

	/* start RX DMA */
	start_dma_rx(pdata, qinx);

	DBGPR("<--configure_dma_channel\n");

	return 0;
}

/* This sequence is used to enable MAC interrupts
 */
static int enable_mac_interrupts(struct dwc_eqos_prv_data *pdata)
{
	unsigned long varmac_imr;

	/* Enable following interrupts */
	/* RGSMIIIM - RGMII/SMII interrupt Enable */
	/* PCSLCHGIM -  PCS Link Status Interrupt Enable */
	/* PCSANCIM - PCS AN Completion Interrupt Enable */
	/* PMTIM - PMT Interrupt Enable */
	/* LPIIM - LPI Interrupt Enable */
	varmac_imr = dwceqos_readl(pdata, DWCEQOS_MAC_INTR_EN);

	/*
	 * Handle the RGMII/SMII interrupt only if we are talking
	 * to an external PHY.
	 */
	if (pdata->hw_feat.sma_sel)
		varmac_imr |= DWCEQOS_MAC_INTR_EN_RGSMIIIE;
	else
		varmac_imr &= ~DWCEQOS_MAC_INTR_EN_RGSMIIIE;

	varmac_imr |= DWCEQOS_MAC_INTR_EN_PCSLCHGIE;
	varmac_imr |= DWCEQOS_MAC_INTR_EN_PCSANCIE;
	varmac_imr |= DWCEQOS_MAC_INTR_EN_PMTIE;
	varmac_imr |= DWCEQOS_MAC_INTR_EN_LPIIE;
	varmac_imr |= DWCEQOS_MAC_INTR_EN_TXSTSIE;
	varmac_imr |= DWCEQOS_MAC_INTR_EN_RXSTSIE;
	dwceqos_writel(pdata, DWCEQOS_MAC_INTR_EN, varmac_imr);
	return 0;
}

static int configure_mac(struct dwc_eqos_prv_data *pdata)
{
	unsigned int qinx;
	u32 regval;

	DBGPR("-->configure_mac\n");

	for (qinx = 0; qinx < pdata->rx_queue_cnt; qinx++) {
		regval = dwceqos_readl(pdata, DWCEQOS_MAC_RQ_CTRL0);
		regval &= ~(DWCEQOS_MAC_RQ_CTRL0_RQEN(qinx, 0x3));
		regval |= DWCEQOS_MAC_RQ_CTRL0_RQEN(qinx, 0x2);
		dwceqos_writel(pdata, DWCEQOS_MAC_RQ_CTRL0, regval);
	}

	/* Set Tx flow control parameters */
	for (qinx = 0; qinx < DWC_EQOS_TX_QUEUE_CNT; qinx++) {
		/* set Pause Time */
		regval = dwceqos_readl(pdata, DWCEQOS_MAC_Q_TX_FLOW_CTRL(qinx));
		regval &= ~(DWCEQOS_MAX_TX_CTRL_PT(0xffff));
		regval |= DWCEQOS_MAX_TX_CTRL_PT(0xffff);
		dwceqos_writel(pdata, DWCEQOS_MAC_Q_TX_FLOW_CTRL(qinx), regval);
		/* Assign priority for RX flow control */
		/* Assign priority for TX flow control */

		if (qinx < 4) {
			regval = dwceqos_readl(pdata, DWCEQOS_MAC_TQPM0);
			regval &= ~(DWCEQOS_MAC_TQPM0_PSTQ(qinx, 0xff));
			regval |= DWCEQOS_MAC_TQPM0_PSTQ(qinx, qinx);
			dwceqos_writel(pdata, DWCEQOS_MAC_TQPM0, regval);
			regval = dwceqos_readl(pdata, DWCEQOS_MAC_RQ_CTRL2);
			regval &= ~(DWCEQOS_MAC_RQ_CTRL2_PSRQ(qinx, 0xff));
			regval |= DWCEQOS_MAC_RQ_CTRL2_PSRQ(qinx, 0x1 << qinx);
			dwceqos_writel(pdata, DWCEQOS_MAC_RQ_CTRL2, regval);
		} else {
			regval = dwceqos_readl(pdata, DWCEQOS_MAC_TQPM1);
			regval &= ~(DWCEQOS_MAC_TQPM1_PSTQ(qinx - 4, 0xff));
			regval |= DWCEQOS_MAC_TQPM1_PSTQ(qinx - 4, qinx);
			dwceqos_writel(pdata, DWCEQOS_MAC_TQPM1, regval);
			regval = dwceqos_readl(pdata, DWCEQOS_MAC_RQ_CTRL3);
			regval &= ~(DWCEQOS_MAC_RQ_CTRL3_PSRQ(qinx - 4, 0xff));
			regval |= DWCEQOS_MAC_RQ_CTRL3_PSRQ(qinx - 4, 0x1 << qinx);
			dwceqos_writel(pdata, DWCEQOS_MAC_RQ_CTRL3, regval);
		}

		if ((pdata->flow_ctrl & DWC_EQOS_FLOW_CTRL_TX) == DWC_EQOS_FLOW_CTRL_TX)
			enable_tx_flow_ctrl(pdata, qinx);
		else
			disable_tx_flow_ctrl(pdata, qinx);
	}

	/* Set Rx flow control parameters */
	if ((pdata->flow_ctrl & DWC_EQOS_FLOW_CTRL_RX) == DWC_EQOS_FLOW_CTRL_RX)
		enable_rx_flow_ctrl(pdata);
	else
		disable_rx_flow_ctrl(pdata);

	/* Configure for Jumbo frame in MAC */
	if (pdata->ndev->mtu > DWC_EQOS_ETH_FRAME_LEN) {
		if (pdata->ndev->mtu < DWC_EQOS_MAX_GPSL) {
			regval = dwceqos_readl(pdata, DWCEQOS_MAC_CFG);
			regval |= DWCEQOS_MAC_CFG_JE;
			regval &= ~(DWCEQOS_MAC_CFG_WD);
			regval &= ~(DWCEQOS_MAC_CFG_GPSLCE);
			regval &= ~(DWCEQOS_MAC_CFG_JD);
			dwceqos_writel(pdata, DWCEQOS_MAC_CFG, regval);
		} else {
			regval = dwceqos_readl(pdata, DWCEQOS_MAC_CFG);
			regval &= ~(DWCEQOS_MAC_CFG_JE);
			regval |= DWCEQOS_MAC_CFG_WD;
			regval |= DWCEQOS_MAC_CFG_GPSLCE;
			dwceqos_writel(pdata, DWCEQOS_MAC_CFG, regval);
			regval = dwceqos_readl(pdata, DWCEQOS_MAC_EXT_CFG);
			regval &= ~(DWCEQOS_MAC_ECFG_GPSL(0x3fff));
			regval |= DWCEQOS_MAC_ECFG_GPSL(DWC_EQOS_MAX_SUPPORTED_MTU);
			dwceqos_writel(pdata, DWCEQOS_MAC_EXT_CFG, regval);
			regval = dwceqos_readl(pdata, DWCEQOS_MAC_CFG);
			regval |= DWCEQOS_MAC_CFG_JD;
			dwceqos_writel(pdata, DWCEQOS_MAC_CFG, regval);
			pr_info("Configured Gaint Packet Size Limit to %d\n",
				 DWC_EQOS_MAX_SUPPORTED_MTU);
		}
		pr_info("Enabled JUMBO pkt\n");
	} else {
		regval = dwceqos_readl(pdata, DWCEQOS_MAC_CFG);
		regval &= ~(DWCEQOS_MAC_CFG_JE);
		regval &= ~(DWCEQOS_MAC_CFG_WD);
		regval &= ~(DWCEQOS_MAC_CFG_GPSLCE);
		regval &= ~(DWCEQOS_MAC_CFG_JD);
		dwceqos_writel(pdata, DWCEQOS_MAC_CFG, regval);
		pr_info("Disabled JUMBO pkt\n");
	}

	/* update the MAC address */
	dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(0), ((pdata->ndev->dev_addr[5] << 8) |
				(pdata->ndev->dev_addr[4])));
	dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(0), ((pdata->ndev->dev_addr[3] << 24) |
				(pdata->ndev->dev_addr[2] << 16) |
				(pdata->ndev->dev_addr[1] << 8) |
				(pdata->ndev->dev_addr[0])));

	/*Enable MAC Transmit process */
	/*Enable MAC Receive process */
	/*Enable padding - disabled */
	/*Enable CRC stripping - disabled */
	regval = dwceqos_readl(pdata, DWCEQOS_MAC_CFG);
	regval |= (DWCEQOS_MAC_CFG_RE | DWCEQOS_MAC_CFG_TE |
		   DWCEQOS_MAC_CFG_ACS | DWCEQOS_MAC_CFG_CST);

	dwceqos_writel(pdata, DWCEQOS_MAC_CFG, regval);

	if (pdata->hw_feat.rx_coe_sel &&
	    ((pdata->dev_state & NETIF_F_RXCSUM) == NETIF_F_RXCSUM))
		enable_rx_csum(pdata->ndev);

	if (IS_ENABLED(CONFIG_VLAN_8021Q)) {
		configure_mac_for_vlan_pkt(pdata);
		if (pdata->hw_feat.vlan_hash_en)
			config_vlan_filtering(pdata->ndev, 1, 1, 0);
	}

	if (pdata->hw_feat.mmc_sel) {
		/* disable all MMC intterrupt as MMC are managed in SW and
		 * registers are cleared on each READ eventually
		 */
		disable_mmc_interrupts(pdata);
		config_mmc_counters(pdata);
	}

	enable_mac_interrupts(pdata);

	DBGPR("<--configure_mac\n");

	return 0;
}

/* Initialises device registers.
 * This function initialises device registers.
 */
static int dwc_eqos_yinit(struct dwc_eqos_prv_data *pdata)
{
	unsigned int qinx;

	DBGPR("-->dwc_eqos_yinit\n");

	/* reset mmc counters */
	dwceqos_writel(pdata, DWCEQOS_MMC_CTRL, 0x1);

	for (qinx = 0; qinx < DWC_EQOS_TX_QUEUE_CNT; qinx++)
		configure_mtl_queue(qinx, pdata);
	//Mapping MTL Rx queue and DMA Rx channel.
	dwceqos_writel(pdata, DWCEQOS_MTL_RQ_DMA_MAP0, 0x3020100);
	dwceqos_writel(pdata, DWCEQOS_MTL_RQ_DMA_MAP1, 0x7060504);

	configure_mac(pdata);

	/* Setting INCRx */
	dwceqos_writel(pdata, DWCEQOS_DMA_SYSBUS_MODE, 0x0);
	for (qinx = 0; qinx < DWC_EQOS_TX_QUEUE_CNT; qinx++)
		configure_dma_channel(qinx, pdata);

	DBGPR("<--dwc_eqos_yinit\n");

	return 0;
}

/* API to initialize the function pointers.
 *
 * This function is called in probe to initialize all the
 * function pointers which are used in other functions to capture
 * the different device features.
 *
 * hw_if - pointer to hw_if_struct structure.
 */
void dwc_eqos_init_function_ptrs_dev(struct hw_if_struct *hw_if)
{
	DBGPR("-->dwc_eqos_init_function_ptrs_dev\n");

	hw_if->tx_complete = tx_complete;
	hw_if->tx_window_error = NULL;
	hw_if->tx_aborted_error = tx_aborted_error;
	hw_if->tx_carrier_lost_error = tx_carrier_lost_error;
	hw_if->tx_fifo_underrun = tx_fifo_underrun;
	hw_if->tx_get_collision_count = NULL;
	hw_if->tx_handle_aborted_error = NULL;
	hw_if->tx_update_fifo_threshold = NULL;
	hw_if->tx_config_threshold = NULL;

	hw_if->set_promiscuous_mode = set_promiscuous_mode;
	hw_if->set_all_multicast_mode = set_all_multicast_mode;
	hw_if->set_multicast_list_mode = set_multicast_list_mode;
	hw_if->set_unicast_mode = set_unicast_mode;

	hw_if->enable_rx_csum = enable_rx_csum;
	hw_if->disable_rx_csum = disable_rx_csum;
	hw_if->get_rx_csum_status = get_rx_csum_status;

	hw_if->write_phy_regs = write_phy_regs;
	hw_if->read_phy_regs = read_phy_regs;
	hw_if->set_full_duplex = set_full_duplex;
	hw_if->set_half_duplex = set_half_duplex;
	hw_if->set_mii_speed_100 = set_mii_speed_100;
	hw_if->set_mii_speed_10 = set_mii_speed_10;
	hw_if->set_gmii_speed = set_gmii_speed;
	/* for PMT */
	hw_if->start_dma_rx = start_dma_rx;
	hw_if->stop_dma_rx = stop_dma_rx;
	hw_if->start_dma_tx = start_dma_tx;
	hw_if->stop_dma_tx = stop_dma_tx;
	hw_if->start_mac_tx_rx = start_mac_tx_rx;
	hw_if->stop_mac_tx_rx = stop_mac_tx_rx;

	hw_if->pre_xmit = pre_transmit;
	hw_if->dev_read = device_read;
	hw_if->init = dwc_eqos_yinit;
	hw_if->exit = dwc_eqos_yexit;
	/* Descriptor related Sequences have to be initialized here */
	hw_if->tx_desc_init = tx_descriptor_init;
	hw_if->rx_desc_init = rx_descriptor_init;
	hw_if->rx_desc_reset = rx_descriptor_reset;
	hw_if->tx_desc_reset = tx_descriptor_reset;
	hw_if->get_tx_desc_ls = get_tx_descriptor_last;
	hw_if->get_tx_desc_ctxt = get_tx_descriptor_ctxt;
	hw_if->update_rx_tail_ptr = update_rx_tail_ptr;

	/* for FLOW ctrl */
	hw_if->enable_rx_flow_ctrl = enable_rx_flow_ctrl;
	hw_if->disable_rx_flow_ctrl = disable_rx_flow_ctrl;
	hw_if->enable_tx_flow_ctrl = enable_tx_flow_ctrl;
	hw_if->disable_tx_flow_ctrl = disable_tx_flow_ctrl;

	/* for TX vlan control */
	hw_if->enable_vlan_reg_control = configure_reg_vlan_control;
	hw_if->enable_vlan_desc_control = configure_desc_vlan_control;

	/* for rx vlan stripping */
	hw_if->config_rx_outer_vlan_stripping =
		config_rx_outer_vlan_stripping;
	hw_if->config_rx_inner_vlan_stripping =
		config_rx_inner_vlan_stripping;

	/* for sa(source address) insert/replace */
	hw_if->configure_mac_addr0_reg = configure_mac_addr0_reg;
	hw_if->configure_mac_addr1_reg = configure_mac_addr1_reg;
	hw_if->configure_sa_via_reg = configure_sa_via_reg;

	/* for RX watchdog timer */
	hw_if->config_rx_watchdog = config_rx_watchdog_timer;

	/* for RX and TX threshold config */
	hw_if->config_rx_threshold = config_rx_threshold;
	hw_if->config_tx_threshold = config_tx_threshold;

	/* for RX and TX Store and Forward Mode config */
	hw_if->config_rsf_mode = config_rsf_mode;
	hw_if->config_tsf_mode = config_tsf_mode;

	/* for TX DMA Operating on Second Frame config */
	hw_if->config_osf_mode = config_osf_mode;

	/* for INCR/INCRX config */
	hw_if->config_incr_incrx_mode = config_incr_incrx_mode;
	/* for AXI PBL config */
	hw_if->config_axi_pbl_val = config_axi_pbl_val;
	/* for AXI WORL config */
	hw_if->config_axi_worl_val = config_axi_worl_val;
	/* for AXI RORL config */
	hw_if->config_axi_rorl_val = config_axi_rorl_val;

	/* for RX and TX PBL config */
	hw_if->config_rx_pbl_val = config_rx_pbl_val;
	hw_if->get_rx_pbl_val = get_rx_pbl_val;
	hw_if->config_tx_pbl_val = config_tx_pbl_val;
	hw_if->get_tx_pbl_val = get_tx_pbl_val;
	hw_if->config_pblx8 = config_pblx8;

	hw_if->disable_rx_interrupt = disable_rx_interrupt;
	hw_if->enable_rx_interrupt = enable_rx_interrupt;
	hw_if->disable_tx_interrupt = disable_tx_interrupt;
	hw_if->enable_tx_interrupt = enable_tx_interrupt;

	/* for handling MMC */
	hw_if->disable_mmc_interrupts = disable_mmc_interrupts;
	hw_if->config_mmc_counters = config_mmc_counters;

	hw_if->set_dcb_algorithm = set_dcb_algorithm;
	hw_if->set_dcb_queue_weight = set_dcb_queue_weight;

	hw_if->set_tx_queue_operating_mode = set_tx_queue_operating_mode;
	hw_if->set_avb_algorithm = set_avb_algorithm;
	hw_if->config_credit_control = config_credit_control;
	hw_if->config_send_slope = config_send_slope;
	hw_if->config_idle_slope = config_idle_slope;
	hw_if->config_high_credit = config_high_credit;
	hw_if->config_low_credit = config_low_credit;
	hw_if->config_slot_num_check = config_slot_num_check;
	hw_if->config_advance_slot_num_check = config_advance_slot_num_check;

	/* for hw time stamping */
	hw_if->config_hw_time_stamping = config_hw_time_stamping;
	hw_if->config_sub_second_increment = config_sub_second_increment;
	hw_if->init_systime = init_systime;
	hw_if->config_addend = config_addend;
	hw_if->adjust_systime = adjust_systime;
	hw_if->get_systime = get_systime;
	hw_if->get_tx_tstamp_status = get_tx_tstamp_status;
	hw_if->get_tx_tstamp = get_tx_tstamp;
	hw_if->get_tx_tstamp_status_via_reg = get_tx_tstamp_status_via_reg;
	hw_if->get_tx_tstamp_via_reg = get_tx_tstamp_via_reg;
	hw_if->rx_tstamp_available = rx_tstamp_available;
	hw_if->get_rx_tstamp_status = get_rx_tstamp_status;
	hw_if->get_rx_tstamp = get_rx_tstamp;
	hw_if->drop_tx_status_enabled = drop_tx_status_enabled;

	/* for l3 and l4 layer filtering */
	hw_if->config_l2_da_perfect_inverse_match = config_l2_da_perfect_inverse_match;
	hw_if->update_mac_addr32_127_low_high_reg = update_mac_addr32_127_low_high_reg;
	hw_if->update_mac_addr1_31_low_high_reg = update_mac_addr1_31_low_high_reg;
	hw_if->update_hash_table_reg = update_hash_table_reg;
	hw_if->config_mac_pkt_filter_reg = config_mac_pkt_filter_reg;
	hw_if->config_l3_l4_filter_enable = config_l3_l4_filter_enable;
	hw_if->config_l3_filters = config_l3_filters;
	hw_if->update_ip4_addr0 = update_ip4_addr0;
	hw_if->update_ip4_addr1 = update_ip4_addr1;
	hw_if->update_ip6_addr = update_ip6_addr;
	hw_if->config_l4_filters = config_l4_filters;
	hw_if->update_l4_sa_port_no = update_l4_sa_port_no;
	hw_if->update_l4_da_port_no = update_l4_da_port_no;

	/* for VLAN filtering */
	hw_if->get_vlan_hash_table_reg = get_vlan_hash_table_reg;
	hw_if->update_vlan_hash_table_reg = update_vlan_hash_table_reg;
	hw_if->update_vlan_id = update_vlan_id;
	hw_if->config_vlan_filtering = config_vlan_filtering;
	hw_if->config_mac_for_vlan_pkt = configure_mac_for_vlan_pkt;
	hw_if->get_vlan_tag_comparison = get_vlan_tag_comparison;

	/* for MAC loopback */
	hw_if->config_mac_loopback_mode = config_mac_loopback_mode;

	/* for PFC */
	hw_if->config_pfc = config_pfc;

	/* for MAC Double VLAN Processing config */
	hw_if->config_tx_outer_vlan = config_tx_outer_vlan;
	hw_if->config_tx_inner_vlan = config_tx_inner_vlan;
	hw_if->config_svlan = config_svlan;
	hw_if->config_dvlan = config_dvlan;
	hw_if->config_rx_outer_vlan_stripping = config_rx_outer_vlan_stripping;
	hw_if->config_rx_inner_vlan_stripping = config_rx_inner_vlan_stripping;

	/* for PTP Offloading */
	hw_if->config_ptpoffload_engine = config_ptpoffload_engine;

	DBGPR("<--dwc_eqos_init_function_ptrs_dev\n");
}
