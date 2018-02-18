/** @file moal_debug.c
  *
  * @brief This file contains functions for debug proc file.
  *
  * Copyright (C) 2008-2013, Marvell International Ltd.
  *
  * This software file (the "File") is distributed by Marvell International
  * Ltd. under the terms of the GNU General Public License Version 2, June 1991
  * (the "License").  You may use, redistribute and/or modify this File in
  * accordance with the terms and conditions of the License, a copy of which
  * is available by writing to the Free Software Foundation, Inc.,
  * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA or on the
  * worldwide web at http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt.
  *
  * THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE
  * ARE EXPRESSLY DISCLAIMED.  The License provides additional details about
  * this warranty disclaimer.
  *
  */

/********************************************************
Change log:
    11/03/2008: initial version
********************************************************/

#include	"moal_main.h"

/********************************************************
		Global Variables
********************************************************/
/** MLAN debug info */
extern mlan_debug_info info;

/********************************************************
		Local Variables
********************************************************/
#ifdef CONFIG_PROC_FS

/** Get info item size */
#define item_size(n) (sizeof(info.n))
/** Get info item address */
#define item_addr(n) ((t_ptr) &(info.n))

/** Get moal_private member size */
#define item_priv_size(n) (sizeof((moal_private *)0)->n)
/** Get moal_private member address */
#define item_priv_addr(n) ((t_ptr) &((moal_private *)0)->n)

/** Get moal_handle member size */
#define item_handle_size(n) (sizeof((moal_handle *)0)->n)
/** Get moal_handle member address */
#define item_handle_addr(n) ((t_ptr) &((moal_handle *)0)->n)

#ifdef STA_SUPPORT
static struct debug_data items[] = {
#ifdef DEBUG_LEVEL1
	{"drvdbg", sizeof(drvdbg), (t_ptr) & drvdbg}
	,
#endif
	{"wmm_ac_vo", item_size(wmm_ac_vo), item_addr(wmm_ac_vo)}
	,
	{"wmm_ac_vi", item_size(wmm_ac_vi), item_addr(wmm_ac_vi)}
	,
	{"wmm_ac_be", item_size(wmm_ac_be), item_addr(wmm_ac_be)}
	,
	{"wmm_ac_bk", item_size(wmm_ac_bk), item_addr(wmm_ac_bk)}
	,
	{"max_tx_buf_size", item_size(max_tx_buf_size),
	 item_addr(max_tx_buf_size)}
	,
	{"tx_buf_size", item_size(tx_buf_size), item_addr(tx_buf_size)}
	,
	{"curr_tx_buf_size", item_size(curr_tx_buf_size),
	 item_addr(curr_tx_buf_size)}
	,
	{"ps_mode", item_size(ps_mode), item_addr(ps_mode)}
	,
	{"ps_state", item_size(ps_state), item_addr(ps_state)}
	,
	{"is_deep_sleep", item_size(is_deep_sleep), item_addr(is_deep_sleep)}
	,
	{"wakeup_dev_req", item_size(pm_wakeup_card_req),
	 item_addr(pm_wakeup_card_req)}
	,
	{"wakeup_tries", item_size(pm_wakeup_fw_try),
	 item_addr(pm_wakeup_fw_try)}
	,
	{"hs_configured", item_size(is_hs_configured),
	 item_addr(is_hs_configured)}
	,
	{"hs_activated", item_size(hs_activated), item_addr(hs_activated)}
	,
	{"rx_pkts_queued", item_size(rx_pkts_queued), item_addr(rx_pkts_queued)}
	,
	{"tx_pkts_queued", item_size(tx_pkts_queued), item_addr(tx_pkts_queued)}
	,
	{"pps_uapsd_mode", item_size(pps_uapsd_mode), item_addr(pps_uapsd_mode)}
	,
	{"sleep_pd", item_size(sleep_pd), item_addr(sleep_pd)}
	,
	{"qos_cfg", item_size(qos_cfg), item_addr(qos_cfg)}
	,
	{"tx_lock_flag", item_size(tx_lock_flag), item_addr(tx_lock_flag)}
	,
	{"port_open", item_size(port_open), item_addr(port_open)}
	,
	{"bypass_pkt_count", item_size(bypass_pkt_count),
	 item_addr(bypass_pkt_count)}
	,
	{"scan_processing", item_size(scan_processing),
	 item_addr(scan_processing)}
	,
	{"num_tx_timeout", item_size(num_tx_timeout), item_addr(num_tx_timeout)}
	,
	{"num_cmd_timeout", item_size(num_cmd_timeout),
	 item_addr(num_cmd_timeout)}
	,
	{"timeout_cmd_id", item_size(timeout_cmd_id), item_addr(timeout_cmd_id)}
	,
	{"timeout_cmd_act", item_size(timeout_cmd_act),
	 item_addr(timeout_cmd_act)}
	,
	{"last_cmd_id", item_size(last_cmd_id), item_addr(last_cmd_id)}
	,
	{"last_cmd_act", item_size(last_cmd_act), item_addr(last_cmd_act)}
	,
	{"last_cmd_index", item_size(last_cmd_index), item_addr(last_cmd_index)}
	,
	{"last_cmd_resp_id", item_size(last_cmd_resp_id),
	 item_addr(last_cmd_resp_id)}
	,
	{"last_cmd_resp_index", item_size(last_cmd_resp_index),
	 item_addr(last_cmd_resp_index)}
	,
	{"last_event", item_size(last_event), item_addr(last_event)}
	,
	{"last_event_index", item_size(last_event_index),
	 item_addr(last_event_index)}
	,
	{"num_no_cmd_node", item_size(num_no_cmd_node),
	 item_addr(num_no_cmd_node)}
	,
	{"num_cmd_h2c_fail", item_size(num_cmd_host_to_card_failure),
	 item_addr(num_cmd_host_to_card_failure)}
	,
	{"num_cmd_sleep_cfm_fail",
	 item_size(num_cmd_sleep_cfm_host_to_card_failure),
	 item_addr(num_cmd_sleep_cfm_host_to_card_failure)}
	,
	{"num_tx_h2c_fail", item_size(num_tx_host_to_card_failure),
	 item_addr(num_tx_host_to_card_failure)}
	,
	{"num_cmdevt_c2h_fail", item_size(num_cmdevt_card_to_host_failure),
	 item_addr(num_cmdevt_card_to_host_failure)}
	,
	{"num_rx_c2h_fail", item_size(num_rx_card_to_host_failure),
	 item_addr(num_rx_card_to_host_failure)}
	,
	{"num_int_read_fail", item_size(num_int_read_failure),
	 item_addr(num_int_read_failure)}
	,
	{"last_int_status", item_size(last_int_status),
	 item_addr(last_int_status)}
	,
#ifdef SDIO_MULTI_PORT_TX_AGGR
	{"mpa_sent_last_pkt", item_size(mpa_sent_last_pkt),
	 item_addr(mpa_sent_last_pkt)}
	,
	{"mpa_sent_no_ports", item_size(mpa_sent_no_ports),
	 item_addr(mpa_sent_no_ports)}
	,
#endif
	{"num_evt_deauth", item_size(num_event_deauth),
	 item_addr(num_event_deauth)}
	,
	{"num_evt_disassoc", item_size(num_event_disassoc),
	 item_addr(num_event_disassoc)}
	,
	{"num_evt_link_lost", item_size(num_event_link_lost),
	 item_addr(num_event_link_lost)}
	,
	{"num_cmd_deauth", item_size(num_cmd_deauth), item_addr(num_cmd_deauth)}
	,
	{"num_cmd_assoc_ok", item_size(num_cmd_assoc_success),
	 item_addr(num_cmd_assoc_success)}
	,
	{"num_cmd_assoc_fail", item_size(num_cmd_assoc_failure),
	 item_addr(num_cmd_assoc_failure)}
	,
	{"cmd_sent", item_size(cmd_sent), item_addr(cmd_sent)}
	,
	{"data_sent", item_size(data_sent), item_addr(data_sent)}
	,
	{"mp_rd_bitmap", item_size(mp_rd_bitmap), item_addr(mp_rd_bitmap)}
	,
	{"curr_rd_port", item_size(curr_rd_port), item_addr(curr_rd_port)}
	,
	{"mp_wr_bitmap", item_size(mp_wr_bitmap), item_addr(mp_wr_bitmap)}
	,
	{"curr_wr_port", item_size(curr_wr_port), item_addr(curr_wr_port)}
	,
	{"cmd_resp_received", item_size(cmd_resp_received),
	 item_addr(cmd_resp_received)}
	,
	{"event_received", item_size(event_received), item_addr(event_received)}
	,

	{"ioctl_pending", item_handle_size(ioctl_pending),
	 item_handle_addr(ioctl_pending)}
	,
	{"tx_pending", item_handle_size(tx_pending),
	 item_handle_addr(tx_pending)}
	,
	{"rx_pending", item_handle_size(rx_pending),
	 item_handle_addr(rx_pending)}
	,
	{"lock_count", item_handle_size(lock_count),
	 item_handle_addr(lock_count)}
	,
	{"malloc_count", item_handle_size(malloc_count),
	 item_handle_addr(malloc_count)}
	,
	{"vmalloc_count", item_handle_size(vmalloc_count),
	 item_handle_addr(vmalloc_count)}
	,
	{"mbufalloc_count", item_handle_size(mbufalloc_count),
	 item_handle_addr(mbufalloc_count)}
	,
	{"main_state", item_handle_size(main_state),
	 item_handle_addr(main_state)}
	,
	{"driver_state", item_handle_size(driver_state),
	 item_handle_addr(driver_state)}
	,
#ifdef SDIO_MMC_DEBUG
	{"sdiocmd53w", item_handle_size(cmd53w), item_handle_addr(cmd53w)}
	,
	{"sdiocmd53r", item_handle_size(cmd53r), item_handle_addr(cmd53r)}
	,
#endif
#if defined(SDIO_SUSPEND_RESUME)
	{"hs_skip_count", item_handle_size(hs_skip_count),
	 item_handle_addr(hs_skip_count)}
	,
	{"hs_force_count", item_handle_size(hs_force_count),
	 item_handle_addr(hs_force_count)}
	,
#endif
};

#endif

#ifdef UAP_SUPPORT
static struct debug_data uap_items[] = {
#ifdef DEBUG_LEVEL1
	{"drvdbg", sizeof(drvdbg), (t_ptr) & drvdbg}
	,
#endif
	{"wmm_ac_vo", item_size(wmm_ac_vo), item_addr(wmm_ac_vo)}
	,
	{"wmm_ac_vi", item_size(wmm_ac_vi), item_addr(wmm_ac_vi)}
	,
	{"wmm_ac_be", item_size(wmm_ac_be), item_addr(wmm_ac_be)}
	,
	{"wmm_ac_bk", item_size(wmm_ac_bk), item_addr(wmm_ac_bk)}
	,
	{"max_tx_buf_size", item_size(max_tx_buf_size),
	 item_addr(max_tx_buf_size)}
	,
	{"tx_buf_size", item_size(tx_buf_size), item_addr(tx_buf_size)}
	,
	{"curr_tx_buf_size", item_size(curr_tx_buf_size),
	 item_addr(curr_tx_buf_size)}
	,
	{"ps_mode", item_size(ps_mode), item_addr(ps_mode)}
	,
	{"ps_state", item_size(ps_state), item_addr(ps_state)}
	,
	{"wakeup_dev_req", item_size(pm_wakeup_card_req),
	 item_addr(pm_wakeup_card_req)}
	,
	{"wakeup_tries", item_size(pm_wakeup_fw_try),
	 item_addr(pm_wakeup_fw_try)}
	,
	{"hs_configured", item_size(is_hs_configured),
	 item_addr(is_hs_configured)}
	,
	{"hs_activated", item_size(hs_activated), item_addr(hs_activated)}
	,
	{"rx_pkts_queued", item_size(rx_pkts_queued), item_addr(rx_pkts_queued)}
	,
	{"tx_pkts_queued", item_size(tx_pkts_queued), item_addr(tx_pkts_queued)}
	,
	{"bypass_pkt_count", item_size(bypass_pkt_count),
	 item_addr(bypass_pkt_count)}
	,
	{"num_bridge_pkts", item_size(num_bridge_pkts),
	 item_addr(num_bridge_pkts)}
	,
	{"num_drop_pkts", item_size(num_drop_pkts), item_addr(num_drop_pkts)}
	,
	{"num_tx_timeout", item_size(num_tx_timeout), item_addr(num_tx_timeout)}
	,
	{"num_cmd_timeout", item_size(num_cmd_timeout),
	 item_addr(num_cmd_timeout)}
	,
	{"timeout_cmd_id", item_size(timeout_cmd_id), item_addr(timeout_cmd_id)}
	,
	{"timeout_cmd_act", item_size(timeout_cmd_act),
	 item_addr(timeout_cmd_act)}
	,
	{"last_cmd_id", item_size(last_cmd_id), item_addr(last_cmd_id)}
	,
	{"last_cmd_act", item_size(last_cmd_act), item_addr(last_cmd_act)}
	,
	{"last_cmd_index", item_size(last_cmd_index), item_addr(last_cmd_index)}
	,
	{"last_cmd_resp_id", item_size(last_cmd_resp_id),
	 item_addr(last_cmd_resp_id)}
	,
	{"last_cmd_resp_index", item_size(last_cmd_resp_index),
	 item_addr(last_cmd_resp_index)}
	,
	{"last_event", item_size(last_event), item_addr(last_event)}
	,
	{"last_event_index", item_size(last_event_index),
	 item_addr(last_event_index)}
	,
	{"num_no_cmd_node", item_size(num_no_cmd_node),
	 item_addr(num_no_cmd_node)}
	,
	{"num_cmd_h2c_fail", item_size(num_cmd_host_to_card_failure),
	 item_addr(num_cmd_host_to_card_failure)}
	,
	{"num_cmd_sleep_cfm_fail",
	 item_size(num_cmd_sleep_cfm_host_to_card_failure),
	 item_addr(num_cmd_sleep_cfm_host_to_card_failure)}
	,
	{"num_tx_h2c_fail", item_size(num_tx_host_to_card_failure),
	 item_addr(num_tx_host_to_card_failure)}
	,
	{"num_cmdevt_c2h_fail", item_size(num_cmdevt_card_to_host_failure),
	 item_addr(num_cmdevt_card_to_host_failure)}
	,
	{"num_rx_c2h_fail", item_size(num_rx_card_to_host_failure),
	 item_addr(num_rx_card_to_host_failure)}
	,
	{"num_int_read_fail", item_size(num_int_read_failure),
	 item_addr(num_int_read_failure)}
	,
	{"last_int_status", item_size(last_int_status),
	 item_addr(last_int_status)}
	,
#ifdef SDIO_MULTI_PORT_TX_AGGR
	{"mpa_sent_last_pkt", item_size(mpa_sent_last_pkt),
	 item_addr(mpa_sent_last_pkt)}
	,
	{"mpa_sent_no_ports", item_size(mpa_sent_no_ports),
	 item_addr(mpa_sent_no_ports)}
	,
#endif
	{"cmd_sent", item_size(cmd_sent), item_addr(cmd_sent)}
	,
	{"data_sent", item_size(data_sent), item_addr(data_sent)}
	,
	{"mp_rd_bitmap", item_size(mp_rd_bitmap), item_addr(mp_rd_bitmap)}
	,
	{"curr_rd_port", item_size(curr_rd_port), item_addr(curr_rd_port)}
	,
	{"mp_wr_bitmap", item_size(mp_wr_bitmap), item_addr(mp_wr_bitmap)}
	,
	{"curr_wr_port", item_size(curr_wr_port), item_addr(curr_wr_port)}
	,
	{"cmd_resp_received", item_size(cmd_resp_received),
	 item_addr(cmd_resp_received)}
	,
	{"event_received", item_size(event_received), item_addr(event_received)}
	,

	{"ioctl_pending", item_handle_size(ioctl_pending),
	 item_handle_addr(ioctl_pending)}
	,
	{"tx_pending", item_handle_size(tx_pending),
	 item_handle_addr(tx_pending)}
	,
	{"rx_pending", item_handle_size(rx_pending),
	 item_handle_addr(rx_pending)}
	,
	{"lock_count", item_handle_size(lock_count),
	 item_handle_addr(lock_count)}
	,
	{"malloc_count", item_handle_size(malloc_count),
	 item_handle_addr(malloc_count)}
	,
	{"vmalloc_count", item_handle_size(vmalloc_count),
	 item_handle_addr(vmalloc_count)}
	,
	{"mbufalloc_count", item_handle_size(mbufalloc_count),
	 item_handle_addr(mbufalloc_count)}
	,
	{"main_state", item_handle_size(main_state),
	 item_handle_addr(main_state)}
	,
	{"driver_state", item_handle_size(driver_state),
	 item_handle_addr(driver_state)}
	,
#ifdef SDIO_MMC_DEBUG
	{"sdiocmd53w", item_handle_size(cmd53w), item_handle_addr(cmd53w)}
	,
	{"sdiocmd53r", item_handle_size(cmd53r), item_handle_addr(cmd53r)}
	,
#endif
#if defined(SDIO_SUSPEND_RESUME)
	{"hs_skip_count", item_handle_size(hs_skip_count),
	 item_handle_addr(hs_skip_count)}
	,
	{"hs_force_count", item_handle_size(hs_force_count),
	 item_handle_addr(hs_force_count)}
	,
#endif
};
#endif /* UAP_SUPPORT */

/********************************************************
		Local Functions
********************************************************/
/**
 *  @brief Proc read function
 *
 *  @param sfp 	   pointer to seq_file structure
 *  @param data
 *
 *  @return 	   Number of output data or MLAN_STATUS_FAILURE
 */
static int
woal_debug_read(struct seq_file *sfp, void *data)
{
	int val = 0;
	unsigned int i;
#ifdef SDIO_MULTI_PORT_TX_AGGR
	unsigned int j;
#endif
	struct debug_data_priv *items_priv =
		(struct debug_data_priv *)sfp->private;
	struct debug_data *d = items_priv->items;
	moal_private *priv = items_priv->priv;

	ENTER();

	if (priv == NULL) {
		LEAVE();
		return -EFAULT;
	}

	if (MODULE_GET == 0) {
		LEAVE();
		return -EFAULT;
	}

	priv->phandle->driver_state = woal_check_driver_status(priv->phandle);
	/* Get debug information */
	if (woal_get_debug_info(priv, MOAL_PROC_WAIT, &info))
		goto exit;

	for (i = 0; i < (unsigned int)items_priv->num_of_items; i++) {
		if (d[i].size == 1)
			val = *((t_u8 *) d[i].addr);
		else if (d[i].size == 2)
			val = *((t_u16 *) d[i].addr);
		else if (d[i].size == 4)
			val = *((t_ptr *) d[i].addr);
		else {
			unsigned int j;
			seq_printf(sfp, "%s=", d[i].name);
			for (j = 0; j < d[i].size; j += 2) {
				val = *(t_u16 *) (d[i].addr + j);
				seq_printf(sfp, "0x%x ", val);
			}
			seq_printf(sfp, "\n");
			continue;
		}
		if (strstr(d[i].name, "id")
		    || strstr(d[i].name, "bitmap")
			)
			seq_printf(sfp, "%s=0x%x\n", d[i].name, val);
		else
			seq_printf(sfp, "%s=%d\n", d[i].name, val);
	}
#ifdef SDIO_MULTI_PORT_TX_AGGR
	seq_printf(sfp, "last_recv_wr_bitmap=0x%x last_mp_index=%d\n",
		   info.last_recv_wr_bitmap, info.last_mp_index);
	for (i = 0; i < SDIO_MP_DBG_NUM; i++) {
		seq_printf(sfp,
			   "mp_wr_bitmap: 0x%x mp_wr_ports=0x%x len=%d curr_wr_port=0x%x\n",
			   info.last_mp_wr_bitmap[i], info.last_mp_wr_ports[i],
			   info.last_mp_wr_len[i], info.last_curr_wr_port[i]);
		for (j = 0; j < SDIO_MP_AGGR_DEF_PKT_LIMIT; j++) {
			seq_printf(sfp, "0x%02x ",
				   info.last_mp_wr_info[i *
							SDIO_MP_AGGR_DEF_PKT_LIMIT
							+ j]);
		}
		seq_printf(sfp, "\n");
	}
	seq_printf(sfp, "SDIO MPA Tx: ");
	for (i = 0; i < SDIO_MP_AGGR_DEF_PKT_LIMIT; i++)
		seq_printf(sfp, "%d ", info.mpa_tx_count[i]);
	seq_printf(sfp, "\n");
#endif
#ifdef SDIO_MULTI_PORT_RX_AGGR
	seq_printf(sfp, "SDIO MPA Rx: ");
	for (i = 0; i < SDIO_MP_AGGR_DEF_PKT_LIMIT; i++)
		seq_printf(sfp, "%d ", info.mpa_rx_count[i]);
	seq_printf(sfp, "\n");
#endif
	seq_printf(sfp, "tcp_ack_drop_cnt=%d\n", priv->tcp_ack_drop_cnt);
	seq_printf(sfp, "tcp_ack_cnt=%d\n", priv->tcp_ack_cnt);
#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 29)
	for (i = 0; i < 4; i++)
		seq_printf(sfp, "wmm_tx_pending[%d]:%d\n", i,
			   atomic_read(&priv->wmm_tx_pending[i]));
#endif
	if (info.tx_tbl_num) {
		seq_printf(sfp, "Tx BA stream table:\n");
		for (i = 0; i < info.tx_tbl_num; i++) {
			seq_printf(sfp,
				   "tid = %d, ra = %02x:%02x:%02x:%02x:%02x:%02x amsdu=%d\n",
				   (int)info.tx_tbl[i].tid,
				   info.tx_tbl[i].ra[0], info.tx_tbl[i].ra[1],
				   info.tx_tbl[i].ra[2], info.tx_tbl[i].ra[3],
				   info.tx_tbl[i].ra[4], info.tx_tbl[i].ra[5],
				   (int)info.tx_tbl[i].amsdu);
		}
	}
	if (info.rx_tbl_num) {
		seq_printf(sfp, "Rx reorder table:\n");
		for (i = 0; i < info.rx_tbl_num; i++) {
			unsigned int j;

			seq_printf(sfp,
				   "tid = %d, ta =  %02x:%02x:%02x:%02x:%02x:%02x, start_win = %d, "
				   "win_size = %d, amsdu=%d\n",
				   (int)info.rx_tbl[i].tid,
				   info.rx_tbl[i].ta[0], info.rx_tbl[i].ta[1],
				   info.rx_tbl[i].ta[2], info.rx_tbl[i].ta[3],
				   info.rx_tbl[i].ta[4], info.rx_tbl[i].ta[5],
				   (int)info.rx_tbl[i].start_win,
				   (int)info.rx_tbl[i].win_size,
				   (int)info.rx_tbl[i].amsdu);
			seq_printf(sfp, "buffer: ");
			for (j = 0; j < info.rx_tbl[i].win_size; j++) {
				if (info.rx_tbl[i].buffer[j] == MTRUE)
					seq_printf(sfp, "1 ");
				else
					seq_printf(sfp, "0 ");
			}
			seq_printf(sfp, "\n");
		}
	}
	if (info.tdls_peer_list) {
		for (i = 0; i < info.tdls_peer_num; i++) {
			unsigned int j;
			seq_printf(sfp,
				   "tdls peer: %02x:%02x:%02x:%02x:%02x:%02x snr=%d nf=%d\n",
				   info.tdls_peer_list[i].mac_addr[0],
				   info.tdls_peer_list[i].mac_addr[1],
				   info.tdls_peer_list[i].mac_addr[2],
				   info.tdls_peer_list[i].mac_addr[3],
				   info.tdls_peer_list[i].mac_addr[4],
				   info.tdls_peer_list[i].mac_addr[5],
				   info.tdls_peer_list[i].snr,
				   -info.tdls_peer_list[i].nf);
			seq_printf(sfp, "htcap: ");
			for (j = 0; j < sizeof(IEEEtypes_HTCap_t); j++)
				seq_printf(sfp, "%02x ",
					   info.tdls_peer_list[i].ht_cap[j]);
			seq_printf(sfp, "\nExtcap: ");
			for (j = 0; j < sizeof(IEEEtypes_ExtCap_t); j++)
				seq_printf(sfp, "%02x ",
					   info.tdls_peer_list[i].ext_cap[j]);
			seq_printf(sfp, "\n");
			seq_printf(sfp, "vhtcap: ");
			for (j = 0; j < sizeof(IEEEtypes_VHTCap_t); j++)
				seq_printf(sfp, "%02x ",
					   info.tdls_peer_list[i].vht_cap[j]);
			seq_printf(sfp, "\n");
		}
	}
exit:
	MODULE_PUT;
	LEAVE();
	return 0;
}

/**
 *  @brief Proc write function
 *
 *  @param f	   file pointer
 *  @param buf     pointer to data buffer
 *  @param count   data number to write
 *  @param off     Offset
 *
 *  @return 	   number of data
 */
static ssize_t
woal_debug_write(struct file *f, const char __user * buf, size_t count,
		 loff_t * off)
{
	int r, i;
	char *pdata;
	char *p;
	char *p0;
	char *p1;
	char *p2;
	struct seq_file *sfp = f->private_data;
	struct debug_data_priv *items_priv =
		(struct debug_data_priv *)sfp->private;
	struct debug_data *d = items_priv->items;
	moal_private *priv = items_priv->priv;
#ifdef DEBUG_LEVEL1
	t_u32 last_drvdbg = drvdbg;
#endif

	ENTER();

	if (MODULE_GET == 0) {
		LEAVE();
		return MLAN_STATUS_FAILURE;
	}

	pdata = (char *)kmalloc(count + 1, GFP_KERNEL);
	if (pdata == NULL) {
		MODULE_PUT;
		LEAVE();
		return 0;
	}
	memset(pdata, 0, count + 1);

	if (copy_from_user(pdata, buf, count)) {
		PRINTM(MERROR, "Copy from user failed\n");
		kfree(pdata);
		MODULE_PUT;
		LEAVE();
		return 0;
	}

	if (woal_get_debug_info(priv, MOAL_PROC_WAIT, &info)) {
		kfree(pdata);
		MODULE_PUT;
		LEAVE();
		return 0;
	}

	p0 = pdata;
	for (i = 0; i < items_priv->num_of_items; i++) {
		do {
			p = strstr(p0, d[i].name);
			if (p == NULL)
				break;
			p1 = strchr(p, '\n');
			if (p1 == NULL)
				break;
			p0 = p1++;
			p2 = strchr(p, '=');
			if (!p2)
				break;
			p2++;
			r = woal_string_to_number(p2);
			if (d[i].size == 1)
				*((t_u8 *) d[i].addr) = (t_u8) r;
			else if (d[i].size == 2)
				*((t_u16 *) d[i].addr) = (t_u16) r;
			else if (d[i].size == 4)
				*((t_ptr *) d[i].addr) = (t_ptr) r;
			break;
		} while (MTRUE);
	}
	kfree(pdata);

#ifdef DEBUG_LEVEL1
	if (last_drvdbg != drvdbg) {
		woal_set_drvdbg(priv, drvdbg);

	}
#endif
#if 0
	/* Set debug information */
	if (woal_set_debug_info(priv, MOAL_PROC_WAIT, &info)) {
		MODULE_PUT;
		LEAVE();
		return 0;
	}
#endif
	MODULE_PUT;
	LEAVE();
	return count;
}

static int
woal_debug_proc_open(struct inode *inode, struct file *file)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0)
	return single_open(file, woal_debug_read, PDE_DATA(inode));
#else
	return single_open(file, woal_debug_read, PDE(inode)->data);
#endif
}

static const struct file_operations debug_proc_fops = {
	.owner = THIS_MODULE,
	.open = woal_debug_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.write = woal_debug_write,
};

/********************************************************
		Global Functions
********************************************************/
/**
 *  @brief Create debug proc file
 *
 *  @param priv	   A pointer to a moal_private structure
 *
 *  @return 	   N/A
 */
void
woal_debug_entry(moal_private * priv)
{
	struct proc_dir_entry *r;
	int i;
	int handle_items;

	ENTER();

	if (priv->proc_entry == NULL) {
		LEAVE();
		return;
	}
#ifdef STA_SUPPORT
	if (GET_BSS_ROLE(priv) == MLAN_BSS_ROLE_STA) {
		priv->items_priv.items =
			(struct debug_data *)kmalloc(sizeof(items), GFP_KERNEL);
		if (!priv->items_priv.items) {
			PRINTM(MERROR,
			       "Failed to allocate memory for debug data\n");
			LEAVE();
			return;
		}
		memcpy(priv->items_priv.items, items, sizeof(items));
		priv->items_priv.num_of_items = ARRAY_SIZE(items);
	}
#endif
#ifdef UAP_SUPPORT
	if (GET_BSS_ROLE(priv) == MLAN_BSS_ROLE_UAP) {
		priv->items_priv.items =
			(struct debug_data *)kmalloc(sizeof(uap_items),
						     GFP_KERNEL);
		if (!priv->items_priv.items) {
			PRINTM(MERROR,
			       "Failed to allocate memory for debug data\n");
			LEAVE();
			return;
		}
		memcpy(priv->items_priv.items, uap_items, sizeof(uap_items));
		priv->items_priv.num_of_items = ARRAY_SIZE(uap_items);
	}
#endif

	priv->items_priv.priv = priv;
	handle_items = 9;
#ifdef SDIO_MMC_DEBUG
	handle_items += 2;
#endif
#if defined(SDIO_SUSPEND_RESUME)
	handle_items += 2;
#endif
	for (i = 1; i <= handle_items; i++)
		priv->items_priv.items[priv->items_priv.num_of_items -
				       i].addr += (t_ptr) (priv->phandle);

	/* Create proc entry */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 26)
	r = proc_create_data("debug", 0644, priv->proc_entry, &debug_proc_fops,
			     &priv->items_priv);
	if (r == NULL)
#else
	r = create_proc_entry("debug", 0644, priv->proc_entry);
	if (r) {
		r->data = &priv->items_priv;
		r->proc_fops = &debug_proc_fops;
	} else
#endif
	{
		PRINTM(MMSG, "Fail to create proc debug entry\n");
		LEAVE();
		return;
	}

	LEAVE();
}

/**
 *  @brief Remove proc file
 *
 *  @param priv	 A pointer to a moal_private structure
 *
 *  @return 	 N/A
 */
void
woal_debug_remove(moal_private * priv)
{
	ENTER();

	kfree(priv->items_priv.items);
	/* Remove proc entry */
	remove_proc_entry("debug", priv->proc_entry);

	LEAVE();
}
#endif
