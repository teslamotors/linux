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
#include "dwc_eth_qos_ethtool.h"

struct dwc_eqos_stats {
	char stat_string[ETH_GSTRING_LEN];
	int sizeof_stat;
	int stat_offset;
};

/* HW extra status */
#define DWC_EQOS_EXTRA_STAT(m) \
	{#m, FIELD_SIZEOF(struct dwc_eqos_extra_stats, m), \
	offsetof(struct dwc_eqos_prv_data, xstats.m)}

static const struct dwc_eqos_stats dwc_eqos_gstrings_stats[] = {
	DWC_EQOS_EXTRA_STAT(q_re_alloc_rx_buf_failed[0]),
	DWC_EQOS_EXTRA_STAT(q_re_alloc_rx_buf_failed[1]),
	DWC_EQOS_EXTRA_STAT(q_re_alloc_rx_buf_failed[2]),
	DWC_EQOS_EXTRA_STAT(q_re_alloc_rx_buf_failed[3]),
	DWC_EQOS_EXTRA_STAT(q_re_alloc_rx_buf_failed[4]),
	DWC_EQOS_EXTRA_STAT(q_re_alloc_rx_buf_failed[5]),
	DWC_EQOS_EXTRA_STAT(q_re_alloc_rx_buf_failed[6]),
	DWC_EQOS_EXTRA_STAT(q_re_alloc_rx_buf_failed[7]),

	/* Tx/Rx IRQ error info */
	DWC_EQOS_EXTRA_STAT(tx_process_stopped_irq_n[0]),
	DWC_EQOS_EXTRA_STAT(tx_process_stopped_irq_n[1]),
	DWC_EQOS_EXTRA_STAT(tx_process_stopped_irq_n[2]),
	DWC_EQOS_EXTRA_STAT(tx_process_stopped_irq_n[3]),
	DWC_EQOS_EXTRA_STAT(tx_process_stopped_irq_n[4]),
	DWC_EQOS_EXTRA_STAT(tx_process_stopped_irq_n[5]),
	DWC_EQOS_EXTRA_STAT(tx_process_stopped_irq_n[6]),
	DWC_EQOS_EXTRA_STAT(tx_process_stopped_irq_n[7]),
	DWC_EQOS_EXTRA_STAT(rx_process_stopped_irq_n[0]),
	DWC_EQOS_EXTRA_STAT(rx_process_stopped_irq_n[1]),
	DWC_EQOS_EXTRA_STAT(rx_process_stopped_irq_n[2]),
	DWC_EQOS_EXTRA_STAT(rx_process_stopped_irq_n[3]),
	DWC_EQOS_EXTRA_STAT(rx_process_stopped_irq_n[4]),
	DWC_EQOS_EXTRA_STAT(rx_process_stopped_irq_n[5]),
	DWC_EQOS_EXTRA_STAT(rx_process_stopped_irq_n[6]),
	DWC_EQOS_EXTRA_STAT(rx_process_stopped_irq_n[7]),
	DWC_EQOS_EXTRA_STAT(tx_buf_unavailable_irq_n[0]),
	DWC_EQOS_EXTRA_STAT(tx_buf_unavailable_irq_n[1]),
	DWC_EQOS_EXTRA_STAT(tx_buf_unavailable_irq_n[2]),
	DWC_EQOS_EXTRA_STAT(tx_buf_unavailable_irq_n[3]),
	DWC_EQOS_EXTRA_STAT(tx_buf_unavailable_irq_n[4]),
	DWC_EQOS_EXTRA_STAT(tx_buf_unavailable_irq_n[5]),
	DWC_EQOS_EXTRA_STAT(tx_buf_unavailable_irq_n[6]),
	DWC_EQOS_EXTRA_STAT(tx_buf_unavailable_irq_n[7]),
	DWC_EQOS_EXTRA_STAT(rx_buf_unavailable_irq_n[0]),
	DWC_EQOS_EXTRA_STAT(rx_buf_unavailable_irq_n[1]),
	DWC_EQOS_EXTRA_STAT(rx_buf_unavailable_irq_n[2]),
	DWC_EQOS_EXTRA_STAT(rx_buf_unavailable_irq_n[3]),
	DWC_EQOS_EXTRA_STAT(rx_buf_unavailable_irq_n[4]),
	DWC_EQOS_EXTRA_STAT(rx_buf_unavailable_irq_n[5]),
	DWC_EQOS_EXTRA_STAT(rx_buf_unavailable_irq_n[6]),
	DWC_EQOS_EXTRA_STAT(rx_buf_unavailable_irq_n[7]),
	DWC_EQOS_EXTRA_STAT(rx_watchdog_irq_n),
	DWC_EQOS_EXTRA_STAT(fatal_bus_error_irq_n),
	/* Tx/Rx IRQ Events */
	DWC_EQOS_EXTRA_STAT(tx_normal_irq_n[0]),
	DWC_EQOS_EXTRA_STAT(tx_normal_irq_n[1]),
	DWC_EQOS_EXTRA_STAT(tx_normal_irq_n[2]),
	DWC_EQOS_EXTRA_STAT(tx_normal_irq_n[3]),
	DWC_EQOS_EXTRA_STAT(tx_normal_irq_n[4]),
	DWC_EQOS_EXTRA_STAT(tx_normal_irq_n[5]),
	DWC_EQOS_EXTRA_STAT(tx_normal_irq_n[6]),
	DWC_EQOS_EXTRA_STAT(tx_normal_irq_n[7]),
	DWC_EQOS_EXTRA_STAT(rx_normal_irq_n[0]),
	DWC_EQOS_EXTRA_STAT(rx_normal_irq_n[1]),
	DWC_EQOS_EXTRA_STAT(rx_normal_irq_n[2]),
	DWC_EQOS_EXTRA_STAT(rx_normal_irq_n[3]),
	DWC_EQOS_EXTRA_STAT(rx_normal_irq_n[4]),
	DWC_EQOS_EXTRA_STAT(rx_normal_irq_n[5]),
	DWC_EQOS_EXTRA_STAT(rx_normal_irq_n[6]),
	DWC_EQOS_EXTRA_STAT(rx_normal_irq_n[7]),
	DWC_EQOS_EXTRA_STAT(napi_poll_txq[0]),
	DWC_EQOS_EXTRA_STAT(napi_poll_txq[1]),
	DWC_EQOS_EXTRA_STAT(napi_poll_txq[2]),
	DWC_EQOS_EXTRA_STAT(napi_poll_txq[3]),
	DWC_EQOS_EXTRA_STAT(napi_poll_txq[4]),
	DWC_EQOS_EXTRA_STAT(napi_poll_txq[5]),
	DWC_EQOS_EXTRA_STAT(napi_poll_txq[6]),
	DWC_EQOS_EXTRA_STAT(napi_poll_txq[7]),
	DWC_EQOS_EXTRA_STAT(napi_poll_rxq[0]),
	DWC_EQOS_EXTRA_STAT(napi_poll_rxq[1]),
	DWC_EQOS_EXTRA_STAT(napi_poll_rxq[2]),
	DWC_EQOS_EXTRA_STAT(napi_poll_rxq[3]),
	DWC_EQOS_EXTRA_STAT(napi_poll_rxq[4]),
	DWC_EQOS_EXTRA_STAT(napi_poll_rxq[5]),
	DWC_EQOS_EXTRA_STAT(napi_poll_rxq[6]),
	DWC_EQOS_EXTRA_STAT(napi_poll_rxq[7]),
	DWC_EQOS_EXTRA_STAT(tx_clean_n[0]),
	DWC_EQOS_EXTRA_STAT(tx_clean_n[1]),
	DWC_EQOS_EXTRA_STAT(tx_clean_n[2]),
	DWC_EQOS_EXTRA_STAT(tx_clean_n[3]),
	DWC_EQOS_EXTRA_STAT(tx_clean_n[4]),
	DWC_EQOS_EXTRA_STAT(tx_clean_n[5]),
	DWC_EQOS_EXTRA_STAT(tx_clean_n[6]),
	DWC_EQOS_EXTRA_STAT(tx_clean_n[7]),
	/* Tx/Rx frames */
	DWC_EQOS_EXTRA_STAT(tx_pkt_n),
	DWC_EQOS_EXTRA_STAT(rx_pkt_n),
	DWC_EQOS_EXTRA_STAT(tx_vlan_pkt_n),
	DWC_EQOS_EXTRA_STAT(rx_vlan_pkt_n),
	DWC_EQOS_EXTRA_STAT(tx_timestamp_captured_n),
	DWC_EQOS_EXTRA_STAT(rx_timestamp_captured_n),
	DWC_EQOS_EXTRA_STAT(tx_tso_pkt_n),
	DWC_EQOS_EXTRA_STAT(rx_split_hdr_pkt_n),

	/* Tx/Rx frames per channels/queues */
	DWC_EQOS_EXTRA_STAT(q_tx_pkt_n[0]),
	DWC_EQOS_EXTRA_STAT(q_tx_pkt_n[1]),
	DWC_EQOS_EXTRA_STAT(q_tx_pkt_n[2]),
	DWC_EQOS_EXTRA_STAT(q_tx_pkt_n[3]),
	DWC_EQOS_EXTRA_STAT(q_tx_pkt_n[4]),
	DWC_EQOS_EXTRA_STAT(q_tx_pkt_n[5]),
	DWC_EQOS_EXTRA_STAT(q_tx_pkt_n[6]),
	DWC_EQOS_EXTRA_STAT(q_tx_pkt_n[7]),
	DWC_EQOS_EXTRA_STAT(q_rx_pkt_n[0]),
	DWC_EQOS_EXTRA_STAT(q_rx_pkt_n[1]),
	DWC_EQOS_EXTRA_STAT(q_rx_pkt_n[2]),
	DWC_EQOS_EXTRA_STAT(q_rx_pkt_n[3]),
	DWC_EQOS_EXTRA_STAT(q_rx_pkt_n[4]),
	DWC_EQOS_EXTRA_STAT(q_rx_pkt_n[5]),
	DWC_EQOS_EXTRA_STAT(q_rx_pkt_n[6]),
	DWC_EQOS_EXTRA_STAT(q_rx_pkt_n[7]),
};

#define DWC_EQOS_EXTRA_STAT_LEN ARRAY_SIZE(dwc_eqos_gstrings_stats)

/* HW MAC Management counters (if supported) */
#define DWC_EQOS_MMC_STAT(m)	\
	{ #m, FIELD_SIZEOF(struct dwc_eqos_mmc_counters, m),	\
	offsetof(struct dwc_eqos_prv_data, mmc.m)}

static const struct dwc_eqos_stats dwc_eqos_mmc[] = {
	/* MMC TX counters */
	DWC_EQOS_MMC_STAT(mmc_tx_octetcount_gb),
	DWC_EQOS_MMC_STAT(mmc_tx_framecount_gb),
	DWC_EQOS_MMC_STAT(mmc_tx_broadcastframe_g),
	DWC_EQOS_MMC_STAT(mmc_tx_multicastframe_g),
	DWC_EQOS_MMC_STAT(mmc_tx_64_octets_gb),
	DWC_EQOS_MMC_STAT(mmc_tx_65_to_127_octets_gb),
	DWC_EQOS_MMC_STAT(mmc_tx_128_to_255_octets_gb),
	DWC_EQOS_MMC_STAT(mmc_tx_256_to_511_octets_gb),
	DWC_EQOS_MMC_STAT(mmc_tx_512_to_1023_octets_gb),
	DWC_EQOS_MMC_STAT(mmc_tx_1024_to_max_octets_gb),
	DWC_EQOS_MMC_STAT(mmc_tx_unicast_gb),
	DWC_EQOS_MMC_STAT(mmc_tx_multicast_gb),
	DWC_EQOS_MMC_STAT(mmc_tx_broadcast_gb),
	DWC_EQOS_MMC_STAT(mmc_tx_underflow_error),
	DWC_EQOS_MMC_STAT(mmc_tx_singlecol_g),
	DWC_EQOS_MMC_STAT(mmc_tx_multicol_g),
	DWC_EQOS_MMC_STAT(mmc_tx_deferred),
	DWC_EQOS_MMC_STAT(mmc_tx_latecol),
	DWC_EQOS_MMC_STAT(mmc_tx_exesscol),
	DWC_EQOS_MMC_STAT(mmc_tx_carrier_error),
	DWC_EQOS_MMC_STAT(mmc_tx_octetcount_g),
	DWC_EQOS_MMC_STAT(mmc_tx_framecount_g),
	DWC_EQOS_MMC_STAT(mmc_tx_excessdef),
	DWC_EQOS_MMC_STAT(mmc_tx_pause_frame),
	DWC_EQOS_MMC_STAT(mmc_tx_vlan_frame_g),

	/* MMC RX counters */
	DWC_EQOS_MMC_STAT(mmc_rx_framecount_gb),
	DWC_EQOS_MMC_STAT(mmc_rx_octetcount_gb),
	DWC_EQOS_MMC_STAT(mmc_rx_octetcount_g),
	DWC_EQOS_MMC_STAT(mmc_rx_broadcastframe_g),
	DWC_EQOS_MMC_STAT(mmc_rx_multicastframe_g),
	DWC_EQOS_MMC_STAT(mmc_rx_crc_errror),
	DWC_EQOS_MMC_STAT(mmc_rx_align_error),
	DWC_EQOS_MMC_STAT(mmc_rx_run_error),
	DWC_EQOS_MMC_STAT(mmc_rx_jabber_error),
	DWC_EQOS_MMC_STAT(mmc_rx_undersize_g),
	DWC_EQOS_MMC_STAT(mmc_rx_oversize_g),
	DWC_EQOS_MMC_STAT(mmc_rx_64_octets_gb),
	DWC_EQOS_MMC_STAT(mmc_rx_65_to_127_octets_gb),
	DWC_EQOS_MMC_STAT(mmc_rx_128_to_255_octets_gb),
	DWC_EQOS_MMC_STAT(mmc_rx_256_to_511_octets_gb),
	DWC_EQOS_MMC_STAT(mmc_rx_512_to_1023_octets_gb),
	DWC_EQOS_MMC_STAT(mmc_rx_1024_to_max_octets_gb),
	DWC_EQOS_MMC_STAT(mmc_rx_unicast_g),
	DWC_EQOS_MMC_STAT(mmc_rx_length_error),
	DWC_EQOS_MMC_STAT(mmc_rx_outofrangetype),
	DWC_EQOS_MMC_STAT(mmc_rx_pause_frames),
	DWC_EQOS_MMC_STAT(mmc_rx_fifo_overflow),
	DWC_EQOS_MMC_STAT(mmc_rx_vlan_frames_gb),
	DWC_EQOS_MMC_STAT(mmc_rx_watchdog_error),

	/* IPC */
	DWC_EQOS_MMC_STAT(mmc_rx_ipc_intr_mask),
	DWC_EQOS_MMC_STAT(mmc_rx_ipc_intr),

	/* IPv4 */
	DWC_EQOS_MMC_STAT(mmc_rx_ipv4_gd),
	DWC_EQOS_MMC_STAT(mmc_rx_ipv4_hderr),
	DWC_EQOS_MMC_STAT(mmc_rx_ipv4_nopay),
	DWC_EQOS_MMC_STAT(mmc_rx_ipv4_frag),
	DWC_EQOS_MMC_STAT(mmc_rx_ipv4_udsbl),

	/* IPV6 */
	DWC_EQOS_MMC_STAT(mmc_rx_ipv6_gd_octets),
	DWC_EQOS_MMC_STAT(mmc_rx_ipv6_hderr_octets),
	DWC_EQOS_MMC_STAT(mmc_rx_ipv6_nopay_octets),

	/* Protocols */
	DWC_EQOS_MMC_STAT(mmc_rx_udp_gd),
	DWC_EQOS_MMC_STAT(mmc_rx_udp_err),
	DWC_EQOS_MMC_STAT(mmc_rx_tcp_gd),
	DWC_EQOS_MMC_STAT(mmc_rx_tcp_err),
	DWC_EQOS_MMC_STAT(mmc_rx_icmp_gd),
	DWC_EQOS_MMC_STAT(mmc_rx_icmp_err),

	/* IPv4 */
	DWC_EQOS_MMC_STAT(mmc_rx_ipv4_gd_octets),
	DWC_EQOS_MMC_STAT(mmc_rx_ipv4_hderr_octets),
	DWC_EQOS_MMC_STAT(mmc_rx_ipv4_nopay_octets),
	DWC_EQOS_MMC_STAT(mmc_rx_ipv4_frag_octets),
	DWC_EQOS_MMC_STAT(mmc_rx_ipv4_udsbl_octets),

	/* IPV6 */
	DWC_EQOS_MMC_STAT(mmc_rx_ipv6_gd),
	DWC_EQOS_MMC_STAT(mmc_rx_ipv6_hderr),
	DWC_EQOS_MMC_STAT(mmc_rx_ipv6_nopay),

	/* Protocols */
	DWC_EQOS_MMC_STAT(mmc_rx_udp_gd_octets),
	DWC_EQOS_MMC_STAT(mmc_rx_udp_err_octets),
	DWC_EQOS_MMC_STAT(mmc_rx_tcp_gd_octets),
	DWC_EQOS_MMC_STAT(mmc_rx_tcp_err_octets),
	DWC_EQOS_MMC_STAT(mmc_rx_icmp_gd_octets),
	DWC_EQOS_MMC_STAT(mmc_rx_icmp_err_octets),
};

#define DWC_EQOS_MMC_STATS_LEN ARRAY_SIZE(dwc_eqos_mmc)

static const struct ethtool_ops dwc_eqos_ethtool_ops = {
	.get_link = ethtool_op_get_link,
	.get_pauseparam = dwc_eqos_get_pauseparam,
	.set_pauseparam = dwc_eqos_set_pauseparam,
	.get_wol = dwc_eqos_get_wol,
	.set_wol = dwc_eqos_set_wol,
	.get_coalesce = dwc_eqos_get_coalesce,
	.set_coalesce = dwc_eqos_set_coalesce,
	.get_ethtool_stats = dwc_eqos_get_ethtool_stats,
	.get_strings = dwc_eqos_get_strings,
	.get_sset_count = dwc_eqos_get_sset_count,
	.get_link_ksettings = dwc_eqos_get_link_ksettings,
	.set_link_ksettings = dwc_eqos_set_link_ksettings,
	.get_ts_info = dwc_eth_qos_get_ts_info,
};

struct ethtool_ops *dwc_eqos_get_ethtool_ops(void)
{
	return (struct ethtool_ops *)&dwc_eqos_ethtool_ops;
}

/*!
 * \details This function is invoked by kernel when user request to get the
 * pause parameters through standard ethtool command.
 *
 * \param[in] dev – pointer to net device structure.
 * \param[in] Pause – pointer to ethtool_pauseparam structure.
 *
 * \return void
 */

static void dwc_eqos_get_pauseparam(struct net_device *dev,
				    struct ethtool_pauseparam *pause)
{
	struct dwc_eqos_prv_data *pdata = netdev_priv(dev);
	struct phy_device *phydev = pdata->phydev;

	DBGPR("-->dwc_eqos_get_pauseparam\n");

	pause->rx_pause = 0;
	pause->tx_pause = 0;
	pause->autoneg = pdata->phydev->autoneg;

	/* return if PHY doesn't support FLOW ctrl */
	if (!(phydev->supported & SUPPORTED_Pause) ||
	    !(phydev->supported & SUPPORTED_Asym_Pause))
		return;

	if ((pdata->flow_ctrl & DWC_EQOS_FLOW_CTRL_RX) ==
	    DWC_EQOS_FLOW_CTRL_RX)
		pause->rx_pause = 1;

	if ((pdata->flow_ctrl & DWC_EQOS_FLOW_CTRL_TX) ==
	    DWC_EQOS_FLOW_CTRL_TX)
		pause->tx_pause = 1;

	DBGPR("<--dwc_eqos_get_pauseparam\n");
}

/*!
 * \details This function is invoked by kernel when user request to set the
 * pause parameters through standard ethtool command.
 *
 * \param[in] dev – pointer to net device structure.
 * \param[in] pause – pointer to ethtool_pauseparam structure.
 *
 * \return int
 *
 * \retval zero on success and -ve number on failure.
 */

static int dwc_eqos_set_pauseparam(struct net_device *dev,
				   struct ethtool_pauseparam *pause)
{
	struct dwc_eqos_prv_data *pdata = netdev_priv(dev);
	struct phy_device *phydev = pdata->phydev;
	int new_pause = DWC_EQOS_FLOW_CTRL_OFF;
	int ret = 0;

	DBGPR("-->dwc_eqos_set_pauseparam: "\
	      "autoneg = %d tx_pause = %d rx_pause = %d\n",
	      pause->autoneg, pause->tx_pause, pause->rx_pause);

	/* return if PHY doesn't support FLOW ctrl */
	if (!(phydev->supported & SUPPORTED_Pause) ||
	    !(phydev->supported & SUPPORTED_Asym_Pause))
		return -EINVAL;

	if (pause->rx_pause)
		new_pause |= DWC_EQOS_FLOW_CTRL_RX;
	if (pause->tx_pause)
		new_pause |= DWC_EQOS_FLOW_CTRL_TX;

	if (new_pause == pdata->flow_ctrl && !pause->autoneg)
		return -EINVAL;

	pdata->flow_ctrl = new_pause;

	phydev->autoneg = pause->autoneg;

	if (phydev->autoneg) {
		if (netif_running(dev))
			ret = phy_start_aneg(phydev);
	} else {
		dwc_eqos_configure_flow_ctrl(pdata);
	}

	DBGPR("<--dwc_eqos_set_pauseparam\n");

	return ret;
}

void dwc_eqos_configure_flow_ctrl(struct dwc_eqos_prv_data *pdata)
{
	struct hw_if_struct *hw_if = &pdata->hw_if;
	unsigned int qinx;

	DBGPR("-->dwc_eqos_configure_flow_ctrl\n");

	if ((pdata->flow_ctrl & DWC_EQOS_FLOW_CTRL_RX) ==
	    DWC_EQOS_FLOW_CTRL_RX) {
		hw_if->enable_rx_flow_ctrl(pdata);
	} else {
		hw_if->disable_rx_flow_ctrl(pdata);
	}

	/* As ethtool does not provide queue level configuration
	 * Tx flow control is disabled/enabled for all transmit queues
	 */
	if ((pdata->flow_ctrl & DWC_EQOS_FLOW_CTRL_TX) ==
	    DWC_EQOS_FLOW_CTRL_TX) {
		for (qinx = 0; qinx < DWC_EQOS_TX_QUEUE_CNT; qinx++)
			hw_if->enable_tx_flow_ctrl(pdata, qinx);
	} else {
		for (qinx = 0; qinx < DWC_EQOS_TX_QUEUE_CNT; qinx++)
			hw_if->disable_tx_flow_ctrl(pdata, qinx);
	}

	pdata->oldflow_ctrl = pdata->flow_ctrl;

	DBGPR("<--dwc_eqos_configure_flow_ctrl\n");
}

/*!
 * \details This function is invoked by kernel when user request to get the
 * various device settings through standard ethtool command. This function
 * support to get the PHY related settings like link status, interface type,
 * auto-negotiation parameters and pause parameters etc.
 *
 * \param[in] dev – pointer to net device structure.
 * \param[in] cmd – pointer to ethtool_cmd structure.
 *
 * \return int
 *
 * \retval zero on success and -ve number on failure.
 */
#define SPEED_UNKNOWN -1
#define DUPLEX_UNKNOWN 0xff

static int dwc_eqos_get_link_ksettings(struct net_device *dev,
				       struct ethtool_link_ksettings *cmd)
{
	struct dwc_eqos_prv_data *pdata = netdev_priv(dev);

	DBGPR("-->dwc_eqos_getsettings\n");

	if (!pdata->phydev) {
		pr_alert("%s: PHY is not registered\n", dev->name);
		return -ENODEV;
	}

	if (!netif_running(dev)) {
		pr_alert("%s: interface is disabled: we cannot track "\
			 "link speed / duplex settings\n", dev->name);
		return -EBUSY;
	}

	cmd->base.transceiver = XCVR_EXTERNAL;

	spin_lock_irq(&pdata->lock);
	phy_ethtool_ksettings_get(pdata->phydev, cmd);
	spin_unlock_irq(&pdata->lock);

	DBGPR("<--dwc_eqos_getsettings\n");

	return 0;
}

/*!
 * \details This function is invoked by kernel when user request to set the
 * various device settings through standard ethtool command. This function
 * support to set the PHY related settings like link status, interface type,
 * auto-negotiation parameters and pause parameters etc.
 *
 * \param[in] dev – pointer to net device structure.
 * \param[in] cmd – pointer to ethtool_cmd structure.
 *
 * \return int
 *
 * \retval zero on success and -ve number on failure.
 */

static int dwc_eqos_set_link_ksettings(struct net_device *dev,
			const struct ethtool_link_ksettings *cmd)
{
	struct dwc_eqos_prv_data *pdata = netdev_priv(dev);
	struct ethtool_link_ksettings c = *cmd;
	int ret = 0;

	pr_alert("-->dwc_eqos_set_link_ksettings\n");

	ret = phy_ethtool_ksettings_set(pdata->phydev, &c);

	pr_alert("<--dwc_eqos_set_link_ksettings\n");

	return ret;
}

/*!
 * \details This function is invoked by kernel when user request to get report
 * whether wake-on-lan is enable or not.
 *
 * \param[in] dev – pointer to net device structure.
 * \param[in] wol – pointer to ethtool_wolinfo structure.
 *
 * \return void
 */

static void dwc_eqos_get_wol(struct net_device *dev,
			     struct ethtool_wolinfo *wol)
{
	struct dwc_eqos_prv_data *pdata = netdev_priv(dev);

	DBGPR("-->dwc_eqos_get_wol\n");

	wol->supported = 0;
	spin_lock_irq(&pdata->lock);
	if (device_can_wakeup(pdata->ndev->dev.parent)) {
		if (pdata->hw_feat.mgk_sel)
			wol->supported |= WAKE_MAGIC;
		if (pdata->hw_feat.rwk_sel)
			wol->supported |= WAKE_UCAST;
		wol->wolopts = pdata->wolopts;
	}
	spin_unlock_irq(&pdata->lock);

	DBGPR("<--dwc_eqos_get_wol\n");

	return;
}

/*!
 * \details This function is invoked by kernel when user request to set
 * pmt parameters for remote wakeup or magic wakeup
 *
 * \param[in] dev – pointer to net device structure.
 * \param[in] wol – pointer to ethtool_wolinfo structure.
 *
 * \return int
 *
 * \retval zero on success and -ve number on failure.
 */

static int dwc_eqos_set_wol(struct net_device *dev,
			    struct ethtool_wolinfo *wol)
{
	struct dwc_eqos_prv_data *pdata = netdev_priv(dev);
	u32 support = WAKE_MAGIC | WAKE_UCAST;
	int ret = 0;

	DBGPR("-->dwc_eqos_set_wol\n");

	/* By default almost all GMAC devices support the WoL via
	 * magic frame but we can disable it if the HW capability
	 * register shows no support for pmt_magic_frame.
	 */
	if (!pdata->hw_feat.mgk_sel)
		wol->wolopts &= ~WAKE_MAGIC;
	if (!pdata->hw_feat.rwk_sel)
		wol->wolopts &= ~WAKE_UCAST;

	if (!device_can_wakeup(pdata->ndev->dev.parent))
		return -EINVAL;

	if (wol->wolopts & ~support)
		return -EINVAL;

	if (wol->wolopts) {
		pr_alert("Wakeup enable\n");
		device_set_wakeup_enable(pdata->ndev->dev.parent, 1);
		enable_irq_wake(pdata->irq_number);
	} else {
		pr_alert("Wakeup disable\n");
		device_set_wakeup_enable(pdata->ndev->dev.parent, 0);
		disable_irq_wake(pdata->irq_number);
	}

	spin_lock_irq(&pdata->lock);
	pdata->wolopts = wol->wolopts;
	spin_unlock_irq(&pdata->lock);

	DBGPR("<--dwc_eqos_set_wol\n");

	return ret;
}

u32 dwc_eqos_usec2riwt(u32 usec, struct dwc_eqos_prv_data *pdata)
{
	u32 ret = 0;

	DBGPR("-->dwc_eqos_usec2riwt\n");

	/* Eg:
	 * System clock is 62.5MHz, each clock cycle would then be 16ns
	 * For value 0x1 in watchdog timer, device would wait for 256
	 * clock cycles,
	 * ie, (16ns x 256) => 4.096us (rounding off to 4us)
	 * So formula with above values is,
	 * ret = usec/4
	 */

	ret = (usec * (DWC_EQOS_SYSCLOCK / 1000000)) / 256;

	DBGPR("<--dwc_eqos_usec2riwt\n");

	return ret;
}

static u32 dwc_eqos_riwt2usec(u32 riwt, struct dwc_eqos_prv_data *pdata)
{
	u32 ret = 0;

	DBGPR("-->dwc_eqos_riwt2usec\n");

	/* using formula from 'dwc_eqos_usec2riwt' */
	ret = (riwt * 256) / (DWC_EQOS_SYSCLOCK / 1000000);

	DBGPR("<--dwc_eqos_riwt2usec\n");

	return ret;
}

/*!
 * \details This function is invoked by kernel when user request to get
 * interrupt coalescing parameters. As coalescing parameters are same
 * for all the channels, so this function will get coalescing
 * details from channel zero and return.
 *
 * \param[in] dev – pointer to net device structure.
 * \param[in] wol – pointer to ethtool_coalesce structure.
 *
 * \return int
 *
 * \retval 0
 */

static int dwc_eqos_get_coalesce(struct net_device *dev,
				 struct ethtool_coalesce *ec)
{
	struct dwc_eqos_prv_data *pdata = netdev_priv(dev);
	struct dwc_eqos_rx_wrapper_descriptor *rx_desc_data =
	    GET_RX_WRAPPER_DESC(0);

	DBGPR("-->dwc_eqos_get_coalesce\n");

	memset(ec, 0, sizeof(struct ethtool_coalesce));

	ec->rx_coalesce_usecs =
	    dwc_eqos_riwt2usec(rx_desc_data->rx_riwt, pdata);
	ec->rx_max_coalesced_frames = rx_desc_data->rx_coal_frames;

	DBGPR("<--dwc_eqos_get_coalesce\n");

	return 0;
}

/*!
 * \details This function is invoked by kernel when user request to set
 * interrupt coalescing parameters. This driver maintains same coalescing
 * parameters for all the channels, hence same changes will be applied to
 * all the channels.
 *
 * \param[in] dev – pointer to net device structure.
 * \param[in] wol – pointer to ethtool_coalesce structure.
 *
 * \return int
 *
 * \retval zero on success and -ve number on failure.
 */

static int dwc_eqos_set_coalesce(struct net_device *dev,
				 struct ethtool_coalesce *ec)
{
	struct dwc_eqos_prv_data *pdata = netdev_priv(dev);
	struct dwc_eqos_rx_wrapper_descriptor *rx_desc_data =
	    GET_RX_WRAPPER_DESC(0);
	struct hw_if_struct *hw_if = &pdata->hw_if;
	unsigned int rx_riwt, rx_usec, local_use_riwt, qinx;

	DBGPR("-->dwc_eqos_set_coalesce\n");

	/* Check for not supported parameters  */
	if ((ec->rx_coalesce_usecs_irq) ||
	    (ec->rx_max_coalesced_frames_irq) || (ec->tx_coalesce_usecs_irq) ||
	    (ec->use_adaptive_rx_coalesce) || (ec->use_adaptive_tx_coalesce) ||
	    (ec->pkt_rate_low) || (ec->rx_coalesce_usecs_low) ||
	    (ec->rx_max_coalesced_frames_low) || (ec->tx_coalesce_usecs_high) ||
	    (ec->tx_max_coalesced_frames_low) || (ec->pkt_rate_high) ||
	    (ec->tx_coalesce_usecs_low) || (ec->rx_coalesce_usecs_high) ||
	    (ec->rx_max_coalesced_frames_high) ||
	    (ec->tx_max_coalesced_frames_irq) ||
	    (ec->stats_block_coalesce_usecs) ||
	    (ec->tx_max_coalesced_frames_high) || (ec->rate_sample_interval) ||
	    (ec->tx_coalesce_usecs) || (ec->tx_max_coalesced_frames))
		return -EOPNOTSUPP;

	/* both rx_coalesce_usecs and rx_max_coalesced_frames should
	 * be > 0 in order for coalescing to be active.
	 */
	if ((ec->rx_coalesce_usecs <= 3) || (ec->rx_max_coalesced_frames <= 1))
		local_use_riwt = 0;
	else
		local_use_riwt = 1;

	pr_alert("RX COALESCING is %s\n",
		 (local_use_riwt ? "ENABLED" : "DISABLED"));

	rx_riwt = dwc_eqos_usec2riwt(ec->rx_coalesce_usecs, pdata);

	/* Check the bounds of values for RX */
	if (rx_riwt > DWC_EQOS_MAX_DMA_RIWT) {
		rx_usec = dwc_eqos_riwt2usec(DWC_EQOS_MAX_DMA_RIWT,
					     pdata);
		pr_alert("RX Coalesing is limited to %d usecs\n",
			 rx_usec);
		return -EINVAL;
	}
	if (ec->rx_max_coalesced_frames > RX_DESC_CNT) {
		pr_alert("RX Coalesing is limited to %d frames\n",
			 DWC_EQOS_RX_MAX_FRAMES);
		return -EINVAL;
	}
	if (rx_desc_data->rx_coal_frames != ec->rx_max_coalesced_frames &&
	    netif_running(dev)) {
		pr_alert("Coalesce frame parameter can be changed only if interface is down\n");
		return -EINVAL;
	}
	/* The selected parameters are applied to all the
	 * receive queues equally, so all the queue configurations
	 * are in sync
	 */
	for (qinx = 0; qinx < DWC_EQOS_RX_QUEUE_CNT; qinx++) {
		rx_desc_data = GET_RX_WRAPPER_DESC(qinx);
		rx_desc_data->use_riwt = local_use_riwt;
		rx_desc_data->rx_riwt = rx_riwt;
		rx_desc_data->rx_coal_frames = ec->rx_max_coalesced_frames;
		hw_if->config_rx_watchdog(pdata, qinx, rx_desc_data->rx_riwt);
	}

	DBGPR("<--dwc_eqos_set_coalesce\n");

	return 0;
}

/*!
 * \details This function is invoked by kernel when user
 * requests to get the extended statistics about the device.
 *
 * \param[in] dev – pointer to net device structure.
 * \param[in] data – pointer in which extended statistics
 *                   should be put.
 *
 * \return void
 */

static void dwc_eqos_get_ethtool_stats(struct net_device *dev,
				       struct ethtool_stats *dummy, u64 *data)
{
	struct dwc_eqos_prv_data *pdata = netdev_priv(dev);
	int i, j = 0;

	DBGPR("-->dwc_eqos_get_ethtool_stats\n");

	if (pdata->hw_feat.mmc_sel) {
		dwc_eqos_mmc_read(pdata, &pdata->mmc);

		for (i = 0; i < DWC_EQOS_MMC_STATS_LEN; i++) {
			char *p = (char *)pdata +
					dwc_eqos_mmc[i].stat_offset;

			data[j++] = (dwc_eqos_mmc[i].sizeof_stat ==
				sizeof(u64)) ? (*(u64 *)p) : (*(u32 *)p);
		}
	}

	for (i = 0; i < DWC_EQOS_EXTRA_STAT_LEN; i++) {
		char *p = (char *)pdata +
				dwc_eqos_gstrings_stats[i].stat_offset;
		data[j++] = (dwc_eqos_gstrings_stats[i].sizeof_stat ==
				sizeof(u64)) ? (*(u64 *)p) : (*(u32 *)p);
	}

	DBGPR("<--dwc_eqos_get_ethtool_stats\n");
}

/*!
 * \details This function returns a set of strings that describe
 * the requested objects.
 *
 * \param[in] dev – pointer to net device structure.
 * \param[in] data – pointer in which requested string should be put.
 *
 * \return void
 */

static void dwc_eqos_get_strings(struct net_device *dev,
				 u32 stringset, u8 *data)
{
	struct dwc_eqos_prv_data *pdata = netdev_priv(dev);
	int i;
	u8 *p = data;

	DBGPR("-->dwc_eqos_get_strings\n");

	switch (stringset) {
	case ETH_SS_STATS:
		if (pdata->hw_feat.mmc_sel) {
			for (i = 0; i < DWC_EQOS_MMC_STATS_LEN; i++) {
				memcpy(p, dwc_eqos_mmc[i].stat_string,
				       ETH_GSTRING_LEN);
				p += ETH_GSTRING_LEN;
			}
		}

		for (i = 0; i < DWC_EQOS_EXTRA_STAT_LEN; i++) {
			memcpy(p, dwc_eqos_gstrings_stats[i].stat_string,
			       ETH_GSTRING_LEN);
			p += ETH_GSTRING_LEN;
		}
		break;
	default:
		WARN_ON(1);
	}

	DBGPR("<--dwc_eqos_get_strings\n");
}

/*!
 * \details This function gets number of strings that @get_strings
 * will write.
 *
 * \param[in] dev – pointer to net device structure.
 *
 * \return int
 *
 * \retval +ve(>0) on success, 0 if that string is not
 * defined and -ve on failure.
 */

static int dwc_eqos_get_sset_count(struct net_device *dev, int sset)
{
	struct dwc_eqos_prv_data *pdata = netdev_priv(dev);
	int len = 0;

	DBGPR("-->dwc_eqos_get_sset_count\n");

	switch (sset) {
	case ETH_SS_STATS:
		if (pdata->hw_feat.mmc_sel)
			len = DWC_EQOS_MMC_STATS_LEN;
		len += DWC_EQOS_EXTRA_STAT_LEN;
		break;
	default:
		len = -EOPNOTSUPP;
	}

	DBGPR("<--dwc_eqos_get_sset_count\n");

	return len;
}

static int dwc_eth_qos_get_ts_info(struct net_device *dev,struct ethtool_ts_info *info)
{	struct dwc_eqos_prv_data *pdata = netdev_priv(dev);
	info->so_timestamping |= (SOF_TIMESTAMPING_TX_HARDWARE |
                                  SOF_TIMESTAMPING_RX_HARDWARE |
                                  SOF_TIMESTAMPING_RAW_HARDWARE);
	DBGPR("-->dwc_eth_qos_get_ts_info so_timestamp = %x\n",info->so_timestamping);
	info->phc_index = ptp_clock_index(pdata->ptp_clock);
	DBGPR("dwc_eth_qos_get_ts_info phc_index = %x\n",info->phc_index );

	info->tx_types = BIT(HWTSTAMP_TX_OFF) | BIT(HWTSTAMP_TX_ON);
	DBGPR(" tx_types  = %x",info->tx_types);
	info->rx_filters = (BIT(HWTSTAMP_FILTER_NONE) |
			   BIT(HWTSTAMP_FILTER_PTP_V1_L4_EVENT) |
			   BIT(HWTSTAMP_FILTER_PTP_V1_L4_SYNC) |
			   BIT(HWTSTAMP_FILTER_PTP_V1_L4_DELAY_REQ) |
			   BIT(HWTSTAMP_FILTER_PTP_V2_L4_EVENT) |
			   BIT(HWTSTAMP_FILTER_PTP_V2_L4_SYNC) |
			   BIT(HWTSTAMP_FILTER_PTP_V2_L4_DELAY_REQ) |
			   BIT(HWTSTAMP_FILTER_PTP_V2_EVENT) |
			   BIT(HWTSTAMP_FILTER_PTP_V2_SYNC) |
			   BIT(HWTSTAMP_FILTER_PTP_V2_DELAY_REQ) |
			   BIT(HWTSTAMP_FILTER_ALL));
	DBGPR(" <-- dwc_eth_qos_get_ts_info rx_filter  = %x",info->rx_filters);

	return 0;
}
