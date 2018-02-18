/*
 * "drivers/net/can/m_ttcan/m_ttcan_linux.c"
 *
 * Copyright (c) 2015-2017, NVIDIA CORPORATION. All rights reserved.
 *
 * References are taken from "Bosch C_CAN controller" at
 * "drivers/net/can/c_can/c_can.c"
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "hw/m_ttcan.h"
#include <linux/platform_device.h>

#define MTTCAN_POLL_TIME 50

static __init int mttcan_hw_init(struct mttcan_priv *priv)
{
	int err = 0;
	u32 ie = 0, ttie = 0, gfc_reg = 0;
	struct ttcan_controller *ttcan = priv->ttcan;
	ttcan_set_ok(ttcan);

	if (!priv->poll) {
		ie = 0x3BBFF7FF;
		ttie = 0x50C03;
	}
	err = ttcan_controller_init(ttcan, ie, ttie);
	if (err)
		return err;

	err = ttcan_mesg_ram_config(ttcan, (u32 *)priv->mram_param,
		(u32 *)priv->tx_conf, (u32 *)priv->rx_conf);
	if (err)
		return err;

	err = ttcan_set_config_change_enable(ttcan);
	if (err)
		return err;

	/* Accept unmatched in Rx FIFO0 and reject all remote frame */
	gfc_reg = (GFC_ANFS_RXFIFO_0 | GFC_ANFE_RXFIFO_0 | GFC_RRFS_REJECT |
	     GFC_RRFE_REJECT);

	priv->gfc_reg = gfc_reg;
	err = ttcan_set_gfc(ttcan, gfc_reg);
	if (err)
		return err;

	/* Reset XIDAM to default */
	priv->xidam_reg = DEF_MTTCAN_XIDAM;
	ttcan_set_xidam(ttcan, DEF_MTTCAN_XIDAM);

	/* Rx buffers set */
	ttcan_set_rx_buffers_elements(ttcan);

	ttcan_set_std_id_filter_addr(ttcan);
	ttcan_set_xtd_id_filter_addr(ttcan);

	ttcan_set_time_stamp_conf(ttcan, 9, TS_INTERNAL);
	ttcan_set_txevt_fifo_conf(ttcan);

	ttcan_set_tx_buffer_addr(ttcan);

	if (priv->tt_param[0]) {
		dev_info(priv->device, "TTCAN Enabled\n");
		ttcan_disable_auto_retransmission(ttcan, true);
		ttcan_set_trigger_mem_conf(ttcan);
		ttcan_set_tur_config(ttcan, 0x0800, 0x0000, 1);
	}

	if (ttcan->mram_cfg[MRAM_SIDF].num) {
		priv->std_shadow = devm_kzalloc(priv->device,
			(ttcan->mram_cfg[MRAM_SIDF].num * SIDF_ELEM_SIZE),
			GFP_KERNEL);
		if (!priv->std_shadow)
			return -ENOMEM;
		ttcan_prog_std_id_fltrs(ttcan, priv->std_shadow);
	}
	if (ttcan->mram_cfg[MRAM_XIDF].num) {
		priv->xtd_shadow = devm_kzalloc(priv->device,
			(ttcan->mram_cfg[MRAM_XIDF].num * XIDF_ELEM_SIZE),
			GFP_KERNEL);
		if (!priv->xtd_shadow)
			return -ENOMEM;
		ttcan_prog_xtd_id_fltrs(ttcan, priv->xtd_shadow);
	}
	if (ttcan->mram_cfg[MRAM_TMC].num) {
		priv->tmc_shadow = devm_kzalloc(priv->device,
			(ttcan->mram_cfg[MRAM_TMC].num * TRIG_ELEM_SIZE),
			GFP_KERNEL);
		if (!priv->tmc_shadow)
			return -ENOMEM;
		ttcan_prog_trigger_mem(ttcan, priv->tmc_shadow);
	}

	ttcan_print_version(ttcan);

	return err;
}

static inline void mttcan_hw_deinit(const struct mttcan_priv *priv)
{
	struct ttcan_controller *ttcan = priv->ttcan;
	ttcan_set_init(ttcan);
}

static __init int mttcan_hw_reinit(const struct mttcan_priv *priv)
{
	int err = 0;

	struct ttcan_controller *ttcan = priv->ttcan;

	ttcan_set_ok(ttcan);

	err = ttcan_set_config_change_enable(ttcan);
	if (err)
		return err;

	err = ttcan_set_gfc(ttcan, priv->gfc_reg);
	if (err)
		return err;

	/* Reset XIDAM to default */
	ttcan_set_xidam(ttcan, priv->xidam_reg);

	/* Rx buffers set */
	ttcan_set_rx_buffers_elements(ttcan);

	ttcan_set_std_id_filter_addr(ttcan);
	ttcan_set_xtd_id_filter_addr(ttcan);
	ttcan_set_time_stamp_conf(ttcan, 9, TS_INTERNAL);
	ttcan_set_txevt_fifo_conf(ttcan);

	ttcan_set_tx_buffer_addr(ttcan);

	if (priv->tt_param[0]) {
		dev_info(priv->device, "TTCAN Enabled\n");
		ttcan_disable_auto_retransmission(ttcan, true);
		ttcan_set_trigger_mem_conf(ttcan);
		ttcan_set_tur_config(ttcan, 0x0800, 0x0000, 1);
	}

	if (ttcan->mram_cfg[MRAM_SIDF].num)
		ttcan_prog_std_id_fltrs(ttcan, priv->std_shadow);
	if (ttcan->mram_cfg[MRAM_XIDF].num)
		ttcan_prog_xtd_id_fltrs(ttcan, priv->xtd_shadow);
	if (ttcan->mram_cfg[MRAM_TMC].num)
		ttcan_prog_trigger_mem(ttcan, priv->tmc_shadow);

	return err;
}

static const struct can_bittiming_const mttcan_normal_bittiming_const = {
	.name = KBUILD_MODNAME,
	.tseg1_min = 2,		/* Time segment 1 = prop_seg + phase_seg1 */
	.tseg1_max = 255,
	.tseg2_min = 0,		/* Time segment 2 = phase_seg2 */
	.tseg2_max = 127,
	.sjw_max = 127,
	.brp_min = 1,
	.brp_max = 511,
	.brp_inc = 1,
};

static const struct can_bittiming_const mttcan_data_bittiming_const = {
	.name = KBUILD_MODNAME,
	.tseg1_min = 1,		/* Time segment 1 = prop_seg + phase_seg1 */
	.tseg1_max = 31,
	.tseg2_min = 0,		/* Time segment 2 = phase_seg2 */
	.tseg2_max = 15,
	.sjw_max = 15,
	.brp_min = 1,
	.brp_max = 15,
	.brp_inc = 1,
};

static struct platform_device_id mttcan_id_table[] = {
	[0] = {
	       .name = "mttcan",
	       .driver_data = 0,
	       }, {
		   }
};

MODULE_DEVICE_TABLE(platform, mttcan_id_table);

static const struct of_device_id mttcan_of_table[] = {
	{ .compatible = "bosch,mttcan", .data = &mttcan_id_table[0]},
	{ /* sentinel */ },
};

MODULE_DEVICE_TABLE(of, mttcan_of_table);

static inline void mttcan_pm_runtime_enable(const struct mttcan_priv *priv)
{
	if (priv->device)
		pm_runtime_enable(priv->device);
}

static inline void mttcan_pm_runtime_disable(const struct mttcan_priv *priv)
{
	if (priv->device)
		pm_runtime_disable(priv->device);
}

static inline void mttcan_pm_runtime_get_sync(const struct mttcan_priv *priv)
{
	if (priv->device)
		pm_runtime_get_sync(priv->device);
}

static inline void mttcan_pm_runtime_put_sync(const struct mttcan_priv *priv)
{
	if (priv->device)
		pm_runtime_put_sync(priv->device);
}

static void mttcan_handle_lost_frame(struct net_device *dev, int fifo_num)
{
	struct mttcan_priv *priv = netdev_priv(dev);
	struct net_device_stats *stats = &dev->stats;
	u32 ack_ir;
	struct sk_buff *skb;
	struct can_frame *frame;

	if (fifo_num)
		ack_ir = MTT_IR_RF1L_MASK;
	else
		ack_ir = MTT_IR_RF0L_MASK;
	ttcan_ir_write(priv->ttcan, ack_ir);

	skb = alloc_can_err_skb(dev, &frame);
	if (unlikely(!skb))
		return;

	frame->can_id |= CAN_ERR_CRTL;
	frame->data[1] = CAN_ERR_CRTL_RX_OVERFLOW;
	stats->rx_errors++;
	stats->rx_over_errors++;
	netif_receive_skb(skb);
}

static int mttcan_do_receive(struct net_device *dev,
			     struct ttcanfd_frame *canfd)
{
	struct net_device_stats *stats = &dev->stats;
	struct sk_buff *skb;
	struct canfd_frame *frame;

	skb = alloc_canfd_skb(dev, &frame);
	if (!skb) {
		stats->rx_dropped++;
		return 0;
	}

	memcpy(frame, canfd, sizeof(struct canfd_frame));

	netif_receive_skb(skb);
	stats->rx_packets++;
	stats->rx_bytes += frame->len;

	return 1;
}

static int mttcan_read_rcv_list(struct net_device *dev,
				struct list_head *rcv,
				enum ttcan_rx_type rx_type,
				int rec_msgs, int quota)
{
	unsigned int pushed;
	unsigned long flags;
	struct mttcan_priv *priv = netdev_priv(dev);
	struct ttcan_rx_msg_list *rx;
	struct net_device_stats *stats = &dev->stats;
	struct list_head *cur, *next, rx_q;

	if (list_empty(rcv))
		return 0;

	INIT_LIST_HEAD(&rx_q);

	spin_lock_irqsave(&priv->ttcan->lock, flags);
	switch (rx_type) {
	case BUFFER:
		priv->ttcan->rxb_mem = 0;
		priv->ttcan->list_status &= ~(BUFFER & 0xFF);
		break;
	case FIFO_0:
		priv->ttcan->rxq0_mem = 0;
		priv->ttcan->list_status &= ~(FIFO_0 & 0xFF);
		break;
	case FIFO_1:
		priv->ttcan->rxq1_mem = 0;
		priv->ttcan->list_status &= ~(FIFO_1 & 0xFF);
	default:
		break;
	}
	list_splice_init(rcv, &rx_q);
	spin_unlock_irqrestore(&priv->ttcan->lock, flags);

	pushed = rec_msgs;
	list_for_each_safe(cur, next, &rx_q) {
		struct sk_buff *skb;
		struct canfd_frame *fd_frame;
		struct can_frame *frame;
		if (!quota--)
			break;
		list_del_init(cur);

		rx = list_entry(cur, struct ttcan_rx_msg_list, recv_list);
		if (rx->msg.flags & CAN_FD_FLAG) {
			skb = alloc_canfd_skb(dev, &fd_frame);
			if (!skb) {
				stats->rx_dropped += pushed;
				return 0;
			}
			memcpy(fd_frame, &rx->msg, sizeof(struct canfd_frame));
			stats->rx_bytes += fd_frame->len;
		} else {
			skb = alloc_can_skb(dev, &frame);
			if (!skb) {
				stats->rx_dropped += pushed;
				return 0;
			}
			frame->can_id =  rx->msg.can_id;
			frame->can_dlc = rx->msg.d_len;
			memcpy(frame->data, &rx->msg.data, frame->can_dlc);
			stats->rx_bytes += frame->can_dlc;
		}

		kfree(rx);
		netif_receive_skb(skb);
		stats->rx_packets++;
		pushed--;
	}
	return rec_msgs - pushed;
}

static int mttcan_state_change(struct net_device *dev,
			       enum can_state error_type)
{
	u32 ecr;
	struct mttcan_priv *priv = netdev_priv(dev);
	struct net_device_stats *stats = &dev->stats;
	struct can_frame *cf;
	struct sk_buff *skb;
	struct can_berr_counter bec;

	/* propagate the error condition to the CAN stack */
	skb = alloc_can_err_skb(dev, &cf);
	if (unlikely(!skb))
		return 0;

	ecr = ttcan_read_ecr(priv->ttcan);
	bec.rxerr = (ecr & MTT_ECR_REC_MASK) >> MTT_ECR_REC_SHIFT;
	bec.txerr = (ecr & MTT_ECR_TEC_MASK) >> MTT_ECR_TEC_SHIFT;

	switch (error_type) {
	case CAN_STATE_ERROR_WARNING:
		/* error warning state */
		priv->can.can_stats.error_warning++;
		priv->can.state = CAN_STATE_ERROR_WARNING;
		cf->can_id |= CAN_ERR_CRTL;
		cf->data[1] = (bec.txerr > bec.rxerr) ?
		    CAN_ERR_CRTL_TX_WARNING : CAN_ERR_CRTL_RX_WARNING;
		cf->data[6] = bec.txerr;
		cf->data[7] = bec.rxerr;

		break;
	case CAN_STATE_ERROR_PASSIVE:
		/* error passive state */
		priv->can.can_stats.error_passive++;
		priv->can.state = CAN_STATE_ERROR_PASSIVE;
		cf->can_id |= CAN_ERR_CRTL;
		if (ecr & MTT_ECR_RP_MASK)
			cf->data[1] |= CAN_ERR_CRTL_RX_PASSIVE;
		if (bec.txerr > 127)
			cf->data[1] |= CAN_ERR_CRTL_TX_PASSIVE;

		cf->data[6] = bec.txerr;
		cf->data[7] = bec.rxerr;
		break;
	case CAN_STATE_BUS_OFF:
		/* bus-off state */
		priv->can.state = CAN_STATE_BUS_OFF;
		cf->can_id |= CAN_ERR_BUSOFF;
		/*
		 * disable all interrupts in bus-off mode to ensure that
		 * the CPU is not hogged down
		 */
		ttcan_set_intrpts(priv->ttcan, 0);
		can_bus_off(dev);
		break;
	default:
		break;
	}
	netif_receive_skb(skb);
	stats->rx_packets++;
	stats->rx_bytes += cf->can_dlc;

	return 1;
}

static int mttcan_handle_bus_err(struct net_device *dev,
				 enum ttcan_lec_type lec_type)
{
	struct mttcan_priv *priv = netdev_priv(dev);
	struct net_device_stats *stats = &dev->stats;
	struct can_frame *cf;
	struct sk_buff *skb;

	if (lec_type == LEC_NO_CHANGE || lec_type == LEC_NO_ERROR)
		return 0;
	/* propagate the error condition to the CAN stack */
	skb = alloc_can_err_skb(dev, &cf);
	if (unlikely(!skb))
		return 0;

	/* common for all type of bus errors */
	priv->can.can_stats.bus_error++;
	stats->rx_errors++;
	cf->can_id |= CAN_ERR_PROT | CAN_ERR_BUSERROR;
	cf->data[2] |= CAN_ERR_PROT_UNSPEC;

	switch (lec_type) {
	case LEC_STUFF_ERROR:
		netdev_err(dev, "Stuff Error Detected\n");
		cf->data[2] |= CAN_ERR_PROT_STUFF;
		break;
	case LEC_FORM_ERROR:
		netdev_err(dev, "Format Error Detected\n");
		cf->data[2] |= CAN_ERR_PROT_FORM;
		break;
	case LEC_ACK_ERROR:
		if (printk_ratelimit())
			netdev_err(dev, "Acknowledgement Error Detected\n");
		cf->data[3] |= (CAN_ERR_PROT_LOC_ACK |
				CAN_ERR_PROT_LOC_ACK_DEL);
		break;
	case LEC_BIT1_ERROR:
		netdev_err(dev, "Bit1 Error Detected\n");
		cf->data[2] |= CAN_ERR_PROT_BIT1;
		break;
	case LEC_BIT0_ERROR:
		netdev_err(dev, "Bit0 Error Detected\n");
		cf->data[2] |= CAN_ERR_PROT_BIT0;
		break;
	case LEC_CRC_ERROR:
		netdev_err(dev, "CRC Error Detected\n");
		cf->data[3] |= (CAN_ERR_PROT_LOC_CRC_SEQ |
				CAN_ERR_PROT_LOC_CRC_DEL);
		break;
	default:
		break;
	}

	netif_receive_skb(skb);
	stats->rx_packets++;
	stats->rx_bytes += cf->can_dlc;
	return 1;
}

static u64 mttcan_ts_value(struct ttcan_controller *ttcan, unsigned short ts)
{
	return ((u64)ts * ttcan->ts_prescalar * 1000000 /
			ttcan->bt_config.nominal.bitrate) +
			ttcan->ts_counter;
}

static void mttcan_tx_event(struct net_device *dev)
{
	struct mttcan_priv *priv = netdev_priv(dev);
	struct ttcan_txevt_msg_list *evt;
	struct list_head *cur, *next, evt_q;
	struct mttcan_tx_evt_element txevt;
	u32 xtd, id;
	unsigned long flags;

	INIT_LIST_HEAD(&evt_q);

	spin_lock_irqsave(&priv->ttcan->lock, flags);

	if (list_empty(&priv->ttcan->tx_evt)) {
		spin_unlock_irqrestore(&priv->ttcan->lock, flags);
		return;
	}

	priv->ttcan->evt_mem = 0;
	priv->ttcan->list_status &= ~(TX_EVT & 0xFF);
	list_splice_init(&priv->ttcan->tx_evt, &evt_q);
	spin_unlock_irqrestore(&priv->ttcan->lock, flags);

	list_for_each_safe(cur, next, &evt_q) {
		list_del_init(cur);

		evt = list_entry(cur, struct ttcan_txevt_msg_list, txevt_list);
		memcpy(&txevt, &evt->txevt,
			sizeof(struct mttcan_tx_evt_element));
		kfree(evt);
		xtd = (txevt.f0 & MTT_TXEVT_ELE_F0_XTD_MASK) >>
			MTT_TXEVT_ELE_F0_XTD_SHIFT;
		id = (txevt.f0 & MTT_TXEVT_ELE_F0_ID_MASK) >>
			MTT_TXEVT_ELE_F0_ID_SHIFT;
		pr_debug("%s:TS %llu:(index %u) ID %x(%s %s %s) Evt_Type %02d\n",
			__func__, mttcan_ts_value(priv->ttcan, txevt.f1 &
			MTT_TXEVT_ELE_F1_TXTS_MASK), (txevt.f1 &
			MTT_TXEVT_ELE_F1_MM_MASK) >> MTT_TXEVT_ELE_F1_MM_SHIFT,
			xtd ? id : id >> 18, xtd ? "XTD" : "STD",
			txevt.f1 & MTT_TXEVT_ELE_F1_FDF_MASK ? "FD" : "NON-FD",
			txevt.f1 & MTT_TXEVT_ELE_F1_BRS_MASK ? "BRS" : "NOBRS",
			(txevt.f1 & MTT_TXEVT_ELE_F1_ET_MASK)
			>> MTT_TXEVT_ELE_F1_ET_SHIFT);
	}
}

static void mttcan_tx_complete(struct net_device *dev)
{
	u32 msg_no;

	struct mttcan_priv *priv = netdev_priv(dev);
	struct ttcan_controller *ttcan = priv->ttcan;
	struct net_device_stats *stats = &dev->stats;

	u32 completed_tx = ttcan_read_tx_complete_reg(ttcan);

	/* apply mask to consider only active CAN Tx transactions */
	completed_tx &= ttcan->tx_object;

	while (completed_tx) {
		msg_no = ffs(completed_tx) - 1;
		can_get_echo_skb(dev, msg_no);
		can_led_event(dev, CAN_LED_EVENT_TX);
		clear_bit(msg_no, &ttcan->tx_object);
		stats->tx_packets++;
		stats->tx_bytes += ttcan->tx_buf_dlc[msg_no];
		completed_tx &= ~(1U << msg_no);
	}

	if (netif_queue_stopped(dev))
		netif_wake_queue(dev);
}

static void mttcan_tx_cancelled(struct net_device *dev)
{
	u32 buff_bit, cancelled_reg, cancelled_msg, msg_no;
	struct mttcan_priv *priv = netdev_priv(dev);
	struct ttcan_controller *ttcan = priv->ttcan;
	struct net_device_stats *stats = &dev->stats;


	cancelled_reg = ttcan_read_tx_cancelled_reg(ttcan);

	/* cancelled_msg are newly cancelled message for current interrupt */
	cancelled_msg = (ttcan->tx_obj_cancelled ^ cancelled_reg) &
		~(ttcan->tx_obj_cancelled);
	ttcan->tx_obj_cancelled = cancelled_reg;

	if (cancelled_msg && netif_queue_stopped(dev))
		netif_wake_queue(dev);

	while (cancelled_msg) {
		msg_no = ffs(cancelled_msg) - 1;
		buff_bit = 1U << msg_no;
		if (ttcan->tx_object & buff_bit) {
			can_free_echo_skb(dev, msg_no);
			clear_bit(msg_no, &ttcan->tx_object);
			cancelled_msg &= ~(buff_bit);
			stats->tx_aborted_errors++;
		} else {
			pr_debug("%s TCF %x ttcan->tx_object %lx\n", __func__,
					cancelled_msg, ttcan->tx_object);
			break;
		}
	}
}

static int mttcan_poll_ir(struct napi_struct *napi, int quota)
{
	int work_done = 0;
	int rec_msgs = 0;
	struct net_device *dev = napi->dev;
	struct mttcan_priv *priv = netdev_priv(dev);
	u32 ir, ack, ttir, ttack, psr;
	ir = priv->irqstatus;
	ttir = priv->tt_irqstatus;

	netdev_dbg(dev, "IR %x\n", ir);
	if (!ir && !ttir)
		goto end;

	if (ir) {
		if (ir & MTTCAN_ERR_INTR) {
			psr = ttcan_read_psr(priv->ttcan);
			priv->ttcan->proto_state = psr;
			ack = ir & MTTCAN_ERR_INTR;
			ttcan_ir_write(priv->ttcan, ack);
			if ((ir & MTT_IR_EW_MASK) && (psr & MTT_PSR_EW_MASK)) {
				work_done += mttcan_state_change(dev,
					CAN_STATE_ERROR_WARNING);
				netdev_warn(dev,
					    "entered error warning state\n");
			}
			if ((ir & MTT_IR_EP_MASK) && (psr & MTT_PSR_EP_MASK)) {
				work_done += mttcan_state_change(dev,
					CAN_STATE_ERROR_PASSIVE);
				netdev_err(dev,
					    "entered error passive state\n");
			}
			if ((ir & MTT_IR_BO_MASK) && (psr & MTT_PSR_BO_MASK)) {
				work_done +=
				    mttcan_state_change(dev, CAN_STATE_BUS_OFF);
				netdev_err(dev, "entered bus off state\n");
			}
			if (((ir & MTT_IR_EP_MASK) && !(psr & MTT_PSR_EP_MASK))
				|| ((ir & MTT_IR_EW_MASK) &&
				!(psr & MTT_PSR_EW_MASK))) {
				if (ir & MTT_IR_EP_MASK)
					netdev_dbg(dev,
						"left error passive state\n");
				else
					netdev_dbg(dev,
						"left error warning state\n");

				priv->can.state = CAN_STATE_ERROR_ACTIVE;
			}

			/* Handle Bus error change */
			if (priv->can.ctrlmode & CAN_CTRLMODE_BERR_REPORTING) {
				if ((ir & MTT_IR_PED_MASK) ||
					(ir & MTT_IR_PEA_MASK)) {
					enum ttcan_lec_type lec;

					if (ir & MTT_IR_PEA_MASK)
						lec = (psr & MTT_PSR_LEC_MASK)
							>> MTT_PSR_LEC_SHIFT;
					else
						lec = (psr & MTT_PSR_DLEC_MASK)
							>> MTT_PSR_DLEC_SHIFT;
					work_done +=
					    mttcan_handle_bus_err(dev, lec);

					if (printk_ratelimit())
						netdev_err(dev,
							"IR %#x PSR %#x\n",
							ir, psr);
				}
			}
			if (ir & MTT_IR_WDI_MASK)
				netdev_warn(dev,
					"Message RAM watchdog not handled\n");
		}

		if (ir & MTT_IR_TOO_MASK) {
			ack = MTT_IR_TOO_MASK;
			ttcan_ir_write(priv->ttcan, ack);
			netdev_warn(dev, "Rx timeout not handled\n");
		}

		/* High Priority Message */
		if (ir & MTTCAN_RX_HP_INTR) {
			struct ttcanfd_frame ttcanfd;
			ack = MTT_IR_HPM_MASK;
			ttcan_ir_write(priv->ttcan, ack);
			if (ttcan_read_hp_mesgs(priv->ttcan, &ttcanfd))
				work_done += mttcan_do_receive(dev, &ttcanfd);
			pr_debug("%s: hp mesg received\n", __func__);
		}

		/* Handle dedicated buffer */
		if (ir & MTT_IR_DRX_MASK) {
			ack = MTT_IR_DRX_MASK;
			ttcan_ir_write(priv->ttcan, ack);
			rec_msgs = ttcan_read_rx_buffer(priv->ttcan);
			work_done +=
			    mttcan_read_rcv_list(dev, &priv->ttcan->rx_b,
						 BUFFER, rec_msgs,
						 quota - work_done);
			pr_debug("%s: buffer mesg received\n", __func__);

		}

		/* Handle RX Fifo interrupt */
		if (ir & MTTCAN_RX_FIFO_INTR) {
			if (ir & MTT_IR_RF1L_MASK) {
				ack = MTT_IR_RF1L_MASK;
				ttcan_ir_write(priv->ttcan, ack);
				mttcan_handle_lost_frame(dev, 1);
				work_done++;
			}
			if (ir & MTT_IR_RF0L_MASK) {
				ack = MTT_IR_RF0L_MASK;
				ttcan_ir_write(priv->ttcan, ack);
				mttcan_handle_lost_frame(dev, 0);
				work_done++;
			}

			if (ir & (MTT_IR_RF1F_MASK | MTT_IR_RF1W_MASK |
				MTT_IR_RF1N_MASK)) {
				ack = ir & (MTT_IR_RF1F_MASK |
					MTT_IR_RF1W_MASK |
					MTT_IR_RF1N_MASK);
				ttcan_ir_write(priv->ttcan, ack);

				rec_msgs = ttcan_read_rx_fifo1(priv->ttcan);
				work_done +=
				    mttcan_read_rcv_list(dev,
							 &priv->ttcan->rx_q1,
							 FIFO_1, rec_msgs,
							 quota - work_done);
				pr_debug("%s: msg received in Q1\n", __func__);
			}
			if (ir & (MTT_IR_RF0F_MASK | MTT_IR_RF0W_MASK |
				MTT_IR_RF0N_MASK)) {
				ack = ir & (MTT_IR_RF0F_MASK |
					MTT_IR_RF0W_MASK |
					MTT_IR_RF0N_MASK);
				ttcan_ir_write(priv->ttcan, ack);
				rec_msgs = ttcan_read_rx_fifo0(priv->ttcan);
				work_done +=
				    mttcan_read_rcv_list(dev,
							 &priv->ttcan->rx_q0,
							 FIFO_0, rec_msgs,
							 quota - work_done);
				pr_debug("%s: msg received in Q0\n", __func__);
			}

			if (ir & MTT_IR_RF0L_MASK) {
				if (printk_ratelimit())
					netdev_warn(dev,
						"%s: some msgs lost in Q0\n",
						__func__);
				ack = MTT_IR_RF0L_MASK;
				ttcan_ir_write(priv->ttcan, ack);
			}

			if (ir & MTT_IR_RF1L_MASK) {
				if (printk_ratelimit())
					netdev_warn(dev,
						"%s: some msgs lost in Q1\n",
						__func__);
				ack = MTT_IR_RF1L_MASK;
				ttcan_ir_write(priv->ttcan, ack);
			}
		}

		/* Handle Timer wrap around */
		if (ir & MTT_IR_TSW_MASK) {
			/* milliseconds */
			priv->ttcan->ts_counter += priv->ttcan->ts_prescalar *
				65536 * 1000000 /
				priv->ttcan->bt_config.nominal.bitrate;
			ack = MTT_IR_TSW_MASK;
			ttcan_ir_write(priv->ttcan, ack);
		}

		/* Handle Transmission cancellation finished
		* TCF interrupt is set when transmission cancelled is request
		* by TXBCR register but in case wherer DAR (one-shot) is set
		* the Tx buffers which transmission is not complete due to some
		* reason are not retransmitted and for those buffers
		* corresponding bit in TXBCF is set. Handle them to release
		* Tx queue lockup in software.
		*/
		if ((ir & MTT_IR_TCF_MASK) || (priv->can.ctrlmode &
			CAN_CTRLMODE_ONE_SHOT)) {
			if (ir & MTT_IR_TCF_MASK) {
				ack = MTT_IR_TCF_MASK;
				ttcan_ir_write(priv->ttcan, ack);
			}
			mttcan_tx_cancelled(dev);
		}

		if (ir & MTT_IR_TC_MASK) {
			ack = MTT_IR_TC_MASK;
			ttcan_ir_write(priv->ttcan, ack);
			mttcan_tx_complete(dev);
		}

		if (ir & MTT_IR_TFE_MASK) {
			/*
			 * netdev_info(dev, "Tx Fifo Empty %x\n", ir);
			 */
			ack = MTT_IR_TFE_MASK;
			ttcan_ir_write(priv->ttcan, ack);
		}

		/* Handle Tx Event */
		if (ir & MTTCAN_TX_EV_FIFO_INTR) {
			/* New Tx Event */
			if ((ir & MTT_IR_TEFN_MASK) ||
				(ir & MTT_IR_TEFW_MASK)) {
				ttcan_read_txevt_fifo(priv->ttcan);
				mttcan_tx_event(dev);
			}

			if ((ir & MTT_IR_TEFL_MASK) &&
				priv->ttcan->tx_config.evt_q_num)
				if (printk_ratelimit())
					netdev_warn(dev, "Tx event lost\n");

			ack = MTTCAN_TX_EV_FIFO_INTR;
			ttcan_ir_write(priv->ttcan, ack);
		}

	}

	if (ttir) {
		/* Handle CAN TT interrupts */
		unsigned int tt_err = 0;
		unsigned int ttost = 0;

		if (ttir & 0x7B100) {
			tt_err = 1;
			ttost = ttcan_get_ttost(priv->ttcan);
		}
		if (ttir & MTT_TTIR_CER_MASK)
			netdev_warn(dev, "TT Configuration Error\n");
		if (ttir & MTT_TTIR_AW_MASK)
			netdev_warn(dev, "TT Application wdt triggered\n");
		if (ttir & MTT_TTIR_WT_MASK)
			netdev_warn(dev, "TT Referrence Mesg missing\n");
		if (ttir & MTT_TTIR_IWT_MASK)
			netdev_warn(dev, "TT Initialization Watch Triggered\n");
		if (ttir & MTT_TTIR_SE2_MASK || ttir & MTT_TTIR_SE1_MASK)
			netdev_warn(dev, "TT Scheduling error SE%d\n",
				(ttir & MTT_TTIR_SE1_MASK) ? 1 : 2);
		if (ttir & MTT_TTIR_TXO_MASK)
			netdev_warn(dev, "TT Tx count overflow\n");
		if (ttir & MTT_TTIR_TXU_MASK)
			netdev_warn(dev, "TT Tx count underflow\n");
		if (ttir & MTT_TTIR_GTE_MASK)
			netdev_warn(dev, "TT Global timer error\n");
		if (ttir & MTT_TTIR_GTD_MASK)
			netdev_warn(dev, "TT Global time discontinuity\n");
		if (ttir & MTT_TTIR_GTW_MASK)
			netdev_info(dev, "TT Global time wrapped\n");
		if (ttir & MTT_TTIR_SWE_MASK)
			netdev_info(dev, "TT Stop watch event\n");
		if (ttir & MTT_TTIR_TTMI_MASK)
			netdev_warn(dev, "TT TMI event (int)\n");
		if (ttir & MTT_TTIR_RTMI_MASK)
			netdev_warn(dev, "TT Register TMI\n");
		if (ttir & MTT_TTIR_SOG_MASK)
			netdev_info(dev, "TT Start of Gap\n");
		if (ttir & MTT_TTIR_SMC_MASK)
			netdev_info(dev, "TT Start of Matrix Cycle\n");
		if (ttir & MTT_TTIR_SBC_MASK)
			netdev_info(dev, "TT Start of Basic Cycle\n");
		if (tt_err)
			netdev_err(dev, "TTOST 0x%x\n", ttost);
		ttack = 0xFFFFFFFF;
		ttcan_ttir_write(priv->ttcan, ttack);
	}
end:
	if (work_done < quota) {
		napi_complete(napi);
		ttcan_set_intrpts(priv->ttcan, 1);
	}

	return work_done;
}

static int mttcan_get_berr_counter(const struct net_device *dev,
				   struct can_berr_counter *bec)
{
	struct mttcan_priv *priv = netdev_priv(dev);
	u32 ecr;

	mttcan_pm_runtime_get_sync(priv);

	ecr = ttcan_read_ecr(priv->ttcan);
	bec->rxerr = (ecr & MTT_ECR_REC_MASK) >> MTT_ECR_REC_SHIFT;
	bec->txerr = (ecr & MTT_ECR_TEC_MASK) >> MTT_ECR_TEC_SHIFT;

	mttcan_pm_runtime_put_sync(priv);

	return 0;
}

static int mttcan_do_set_bittiming(struct net_device *dev)
{
	int err = 0;
	struct mttcan_priv *priv = netdev_priv(dev);
	const struct can_bittiming *bt = &priv->can.bittiming;
	const struct can_bittiming *dbt = &priv->can.data_bittiming;

	memcpy(&priv->ttcan->bt_config.nominal, bt,
		sizeof(struct can_bittiming));
	memcpy(&priv->ttcan->bt_config.data, dbt,
		sizeof(struct can_bittiming));

	if (priv->can.ctrlmode & CAN_CTRLMODE_FD)
		priv->ttcan->bt_config.fd_flags = CAN_FD_FLAG  | CAN_BRS_FLAG;
	else
		priv->ttcan->bt_config.fd_flags = 0;

	err = ttcan_set_bitrate(priv->ttcan);
	if (err) {
		netdev_err(priv->dev, "Unable to set bitrate\n");
		return err;
	}

	netdev_info(priv->dev, "Bitrate set\n");
	return 0;
}

static void mttcan_controller_config(struct net_device *dev)
{
	struct mttcan_priv *priv = netdev_priv(dev);

	/* set CCCR.INIT and then CCCR.CCE */
	ttcan_set_config_change_enable(priv->ttcan);

	pr_info("%s: ctrlmode %x\n", __func__, priv->can.ctrlmode);
	/* enable automatic retransmission */
	if ((priv->can.ctrlmode & CAN_CTRLMODE_ONE_SHOT) ||
		priv->tt_param[0])
		ttcan_disable_auto_retransmission(priv->ttcan, true);
	else
		ttcan_disable_auto_retransmission(priv->ttcan, false);

	if ((priv->can.ctrlmode & CAN_CTRLMODE_LOOPBACK) &&
	    (priv->can.ctrlmode & CAN_CTRLMODE_LISTENONLY)) {
		/* internal loopback mode : useful for self-test function */
		ttcan_set_bus_monitoring_mode(priv->ttcan, true);
		ttcan_set_loopback(priv->ttcan);

	} else if (priv->can.ctrlmode & CAN_CTRLMODE_LOOPBACK) {
		/* external loopback mode : useful for self-test function */
		ttcan_set_bus_monitoring_mode(priv->ttcan, false);
		ttcan_set_loopback(priv->ttcan);

	} else if (priv->can.ctrlmode & CAN_CTRLMODE_LISTENONLY) {
		/* silent mode : bus-monitoring mode */
		ttcan_set_bus_monitoring_mode(priv->ttcan, true);
	} else
		/* clear bus montor or external loopback mode */
		ttcan_set_normal_mode(priv->ttcan);

	/* set bit timing and start controller */
	mttcan_do_set_bittiming(dev);
}

static void mttcan_start(struct net_device *dev)
{
	struct mttcan_priv *priv = netdev_priv(dev);
	struct ttcan_controller *ttcan = priv->ttcan;
	u32 psr;

	mttcan_controller_config(dev);

	ttcan_clear_intr(ttcan);
	ttcan_clear_tt_intr(ttcan);

	ttcan_set_intrpts(priv->ttcan, 1);

	/* Set current state of CAN controller *
	 * It is assumed the controller is reset durning probing time
	 * It should be in sane state at first start but not guaranteed
	 */
	if (ttcan->proto_state)
		psr = ttcan->proto_state;
	else
		psr = ttcan_read_psr(ttcan);

	if (psr & MTT_PSR_BO_MASK) {
		/* Bus off */
		priv->can.state = CAN_STATE_BUS_OFF;
		ttcan_set_intrpts(ttcan, 0);
		can_bus_off(dev);
	} else if (psr & MTT_PSR_EP_MASK) {
		/* Error Passive */
		priv->can.state = CAN_STATE_ERROR_PASSIVE;
	} else if (psr & MTT_PSR_EW_MASK) {
		/* Error Warning */
		priv->can.state = CAN_STATE_ERROR_WARNING;
	} else {
		/* Error Active */
		priv->can.state = CAN_STATE_ERROR_ACTIVE;
	}

	/* start Tx/Rx and enable protected mode */
	if (!priv->tt_param[0])
		ttcan_reset_init(ttcan);

	if (priv->poll)
		schedule_delayed_work(&priv->can_work,
			msecs_to_jiffies(MTTCAN_POLL_TIME));
}

static void mttcan_stop(struct mttcan_priv *priv)
{
	ttcan_set_intrpts(priv->ttcan, 0);

	priv->can.state = CAN_STATE_STOPPED;

	ttcan_set_config_change_enable(priv->ttcan);
}

static int mttcan_set_mode(struct net_device *dev, enum can_mode mode)
{
	switch (mode) {
	case CAN_MODE_START:
		mttcan_start(dev);
		netif_wake_queue(dev);
		break;
	default:
		return -EOPNOTSUPP;
	}
	return 0;
}

static struct net_device *alloc_mttcan_dev(void)
{
	struct net_device *dev;
	struct mttcan_priv *priv;

	dev = alloc_candev(sizeof(struct mttcan_priv), MTT_CAN_TX_OBJ_NUM);
	if (!dev)
		return NULL;

	/* TODO:- check if we need to disable local loopback */
	dev->flags = (IFF_NOARP | IFF_ECHO);

	priv = netdev_priv(dev);

	priv->dev = dev;
	priv->can.bittiming_const = &mttcan_normal_bittiming_const;
	priv->can.data_bittiming_const = &mttcan_data_bittiming_const;
	priv->can.do_set_bittiming = mttcan_do_set_bittiming;
	priv->can.do_set_mode = mttcan_set_mode;
	priv->can.do_get_berr_counter = mttcan_get_berr_counter;
	priv->can.ctrlmode_supported = CAN_CTRLMODE_LOOPBACK |
	    CAN_CTRLMODE_LISTENONLY | CAN_CTRLMODE_FD |
	    CAN_CTRLMODE_BERR_REPORTING | CAN_CTRLMODE_ONE_SHOT;

	netif_napi_add(dev, &priv->napi, mttcan_poll_ir, MTT_CAN_NAPI_WEIGHT);

	return dev;
}

static irqreturn_t mttcan_isr(int irq, void *dev_id)
{
	struct net_device *dev = (struct net_device *)dev_id;
	struct mttcan_priv *priv = netdev_priv(dev);

	priv->irqstatus = ttcan_read_ir(priv->ttcan);
	priv->tt_irqstatus = ttcan_read_ttir(priv->ttcan);

	if (!priv->irqstatus && !priv->tt_irqstatus)
		return IRQ_NONE;

	/* If tt_stop > 0, then stop when TT interrupt count > tt_stop */
	if (priv->tt_param[1] && priv->tt_irqstatus)
		if (priv->tt_intrs++ > priv->tt_param[1])
			ttcan_set_config_change_enable(priv->ttcan);

	/* disable and clear all interrupts */
	ttcan_set_intrpts(priv->ttcan, 0);

	/* schedule the NAPI */
	napi_schedule(&priv->napi);

	return IRQ_HANDLED;
}

static void mttcan_work(struct work_struct *work)
{
	struct mttcan_priv *priv = container_of(to_delayed_work(work),
						struct mttcan_priv, can_work);

	priv->irqstatus = ttcan_read_ir(priv->ttcan);
	priv->tt_irqstatus = ttcan_read_ttir(priv->ttcan);

	if (priv->irqstatus || priv->tt_irqstatus) {
		/* disable and clear all interrupts */
		ttcan_set_intrpts(priv->ttcan, 0);

		/* schedule the NAPI */
		napi_schedule(&priv->napi);
	}
	schedule_delayed_work(&priv->can_work,
		msecs_to_jiffies(MTTCAN_POLL_TIME));
}

static int mttcan_power_up(struct mttcan_priv *priv)
{
	int level;
	mttcan_pm_runtime_get_sync(priv);

	if (gpio_is_valid(priv->gpio_can_stb.gpio)) {
		level = !priv->gpio_can_stb.active_low;
		gpio_direction_output(priv->gpio_can_stb.gpio, level);
	}

	if (gpio_is_valid(priv->gpio_can_en.gpio)) {
		level = !priv->gpio_can_en.active_low;
		gpio_direction_output(priv->gpio_can_en.gpio, level);
	}

	return ttcan_set_power(priv->ttcan, 1);
}

static int mttcan_power_down(struct net_device *dev)
{
	int level;
	struct mttcan_priv *priv = netdev_priv(dev);

	if (ttcan_set_power(priv->ttcan, 0))
		return -ETIMEDOUT;

	if (gpio_is_valid(priv->gpio_can_stb.gpio)) {
		level = priv->gpio_can_stb.active_low;
		gpio_direction_output(priv->gpio_can_stb.gpio, level);
	}

	if (gpio_is_valid(priv->gpio_can_en.gpio)) {
		level = priv->gpio_can_en.active_low;
		gpio_direction_output(priv->gpio_can_en.gpio, level);
	}

	mttcan_pm_runtime_put_sync(priv);

	return 0;
}

static int mttcan_open(struct net_device *dev)
{
	int err;
	struct mttcan_priv *priv = netdev_priv(dev);

	mttcan_pm_runtime_get_sync(priv);

	err = mttcan_power_up(priv);
	if (err) {
		netdev_err(dev, "unable to power on\n");
		goto exit_open_fail;
	}
	err = open_candev(dev);
	if (err) {
		netdev_err(dev, "failed to open can device\n");
		goto exit_open_fail;
	}

	err = devm_request_irq(priv->device, dev->irq, mttcan_isr, IRQF_SHARED,
			dev->name, dev);
	if (err < 0) {
		netdev_err(dev, "failed to request interrupt\n");
		goto fail;
	}

	napi_enable(&priv->napi);
	can_led_event(dev, CAN_LED_EVENT_OPEN);

	mttcan_start(dev);
	netif_start_queue(dev);

	return 0;

fail:
	close_candev(dev);
exit_open_fail:
	mttcan_pm_runtime_put_sync(priv);
	return err;
}

static int mttcan_close(struct net_device *dev)
{
	struct mttcan_priv *priv = netdev_priv(dev);
	netif_stop_queue(dev);
	napi_disable(&priv->napi);
	mttcan_power_down(dev);
	mttcan_stop(priv);
	devm_free_irq(priv->device, dev->irq, dev);
	close_candev(dev);
	mttcan_pm_runtime_put_sync(priv);

	can_led_event(dev, CAN_LED_EVENT_STOP);
	return 0;
}

static netdev_tx_t mttcan_start_xmit(struct sk_buff *skb,
				     struct net_device *dev)
{
	int msg_no = -1;
	struct mttcan_priv *priv = netdev_priv(dev);
	struct canfd_frame *frame = (struct canfd_frame *)skb->data;

	if (can_dropped_invalid_skb(dev, skb))
		return NETDEV_TX_OK;

	if (can_is_canfd_skb(skb))
		frame->flags |= CAN_FD_FLAG;

	if (ttcan_tx_buffers_full(priv->ttcan)) {
		netif_stop_queue(dev);
		smp_mb();
		return NETDEV_TX_BUSY;
	}

	/* Write Tx message to controller */
	msg_no = ttcan_tx_msg_buffer_write(priv->ttcan,
			(struct ttcanfd_frame *)frame);

	if (msg_no < 0)
		msg_no = ttcan_tx_fifo_queue_msg(priv->ttcan,
				(struct ttcanfd_frame *)frame);

	if (msg_no < 0) {
		if (printk_ratelimit())
			netdev_warn(dev, "No Tx space left\n");
		kfree_skb(skb);
		dev->stats.tx_dropped++;
		return NETDEV_TX_OK;
	}

	can_put_echo_skb(skb, dev, msg_no);

	/* Set go bit for non-TTCAN messages */
	if (!priv->tt_param[0])
		ttcan_tx_trigger_msg_transmit(priv->ttcan, msg_no);

	/* State management for Tx complete/cancel processing */
	if (test_and_set_bit(msg_no, &priv->ttcan->tx_object))
		netdev_err(dev, "Writing to occupied echo_skb buffer\n");
	clear_bit(msg_no, &priv->ttcan->tx_obj_cancelled);

	return NETDEV_TX_OK;
}

static int mttcan_change_mtu(struct net_device *dev, int new_mtu)
{
	if (dev->flags & IFF_UP)
		return -EBUSY;

	if (new_mtu != CANFD_MTU)
		dev->mtu = new_mtu;
	return 0;
}

static const struct net_device_ops mttcan_netdev_ops = {
	.ndo_open = mttcan_open,
	.ndo_stop = mttcan_close,
	.ndo_start_xmit = mttcan_start_xmit,
	.ndo_change_mtu = mttcan_change_mtu,
};

static int register_mttcan_dev(struct net_device *dev)
{
	int err;

	dev->netdev_ops = &mttcan_netdev_ops;
	err = register_candev(dev);
	if (!err)
		devm_can_led_init(dev);

	return err;
}

static int mttcan_prepare_clock(struct mttcan_priv *priv)
{
	int err;

	mttcan_pm_runtime_enable(priv);

	err = clk_prepare_enable(priv->hclk);
	if (err)
		return err;
	err = clk_prepare_enable(priv->cclk);
	if (err)
		clk_disable_unprepare(priv->hclk);

	return err;
}

static void mttcan_unprepare_clock(struct mttcan_priv *priv)
{
	clk_disable_unprepare(priv->hclk);
	clk_disable_unprepare(priv->cclk);
}


void unregister_mttcan_dev(struct net_device *dev)
{
	struct mttcan_priv *priv = netdev_priv(dev);
	unregister_candev(dev);
	mttcan_pm_runtime_disable(priv);
}

void free_mttcan_dev(struct net_device *dev)
{
	struct mttcan_priv *priv = netdev_priv(dev);
	netif_napi_del(&priv->napi);
	free_candev(dev);
}

static int set_can_clk_src_and_rate(struct mttcan_priv *priv, ulong rate)
{
	int ret  = 0;
	unsigned long new_rate = 0;
	struct clk *hclk, *cclk;
	struct clk *pclk;
	const char *pclk_name;
	/* get the appropriate clk */
	hclk = devm_clk_get(priv->device, "can_host");
	cclk = devm_clk_get(priv->device, "can");
	if (IS_ERR(hclk) || IS_ERR(cclk)) {
		dev_err(priv->device, "no clock defined\n");
		return -ENODEV;
	}

	ret = of_property_read_string(priv->device->of_node,
		"pll_source", &pclk_name);
	if (ret) {
		dev_warn(priv->device, "clk-source not changed\n");
		goto pclk_exit;
	}

	pclk = clk_get(priv->device, pclk_name);
	if (IS_ERR(pclk)) {
		dev_warn(priv->device, "clk-source cannot change\n");
		goto pclk_exit;
	}

	ret = clk_set_parent(cclk, pclk);
	if (ret) {
		dev_warn(priv->device, "unable to change parent clk\n");
		goto pclk_exit;
	}

	new_rate = clk_round_rate(cclk, rate);
	if (!new_rate)
		dev_warn(priv->device, "incorrect clock rate\n");

pclk_exit:
	ret = clk_set_rate(cclk, new_rate > 0 ? new_rate : rate);
	if (ret) {
		dev_warn(priv->device, "unable to set clock rate\n");
		return -EINVAL;
	}
	ret = clk_set_rate(hclk, new_rate > 0 ? new_rate : rate);
	if (ret) {
		dev_warn(priv->device, "unable to set clock rate\n");
		return -EINVAL;
	}
	priv->can.clock.freq = clk_get_rate(cclk);
	priv->cclk = cclk;
	priv->hclk = hclk;
	return 0;
}

static int mttcan_probe(struct platform_device *pdev)
{
	int ret = 0;
	int irq = 0;
	enum of_gpio_flags flags;
	void __iomem *regs = NULL, *xregs = NULL;
	void __iomem *mram_addr = NULL;
	struct net_device *dev;
	struct mttcan_priv *priv;
	struct resource *ext_res;
	struct reset_control *rstc;
	struct resource *mesg_ram, *ctrl_res;
	const struct of_device_id *match;
	struct device_node *np;


	match = of_match_device(mttcan_of_table, &pdev->dev);
	if (!match) {
		dev_err(&pdev->dev, "Failed to find matching dt id\n");
		dev_err(&pdev->dev, "probe failed\n");
		return -EINVAL;
	}

	np = pdev->dev.of_node;
	if (!np) {
		dev_err(&pdev->dev, "No valid device node, probe failed\n");
		return -EINVAL;
	}

	/* get the platform data */
	irq = platform_get_irq(pdev, 0);
	if (irq <= 0) {
		ret = -ENODEV;
		dev_err(&pdev->dev, "IRQ not defined\n");
		goto exit;
	}

	dev = alloc_mttcan_dev();
	if (!dev) {
		ret = -ENOMEM;
		dev_err(&pdev->dev, "CAN device allocation failed\n");
		goto exit;
	}

	priv = netdev_priv(dev);

	/* mem0 Controller Register Space
	 * mem1 Controller Extra Registers Space
	 * mem2 Controller Messege RAM Space
	 */
	ctrl_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	ext_res  = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	mesg_ram = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	if (!ctrl_res || !ext_res || !mesg_ram) {
		ret = -ENODEV;
		dev_err(&pdev->dev, "Resource allocation failed\n");
		goto exit_free_can;
	}
	rstc = devm_reset_control_get(&pdev->dev, "can");
	if (IS_ERR(rstc)) {
		dev_err(&pdev->dev, "Missing controller reset\n");
		goto exit_free_can;
	}
	reset_control_reset(rstc);

	regs = devm_ioremap_resource(&pdev->dev, ctrl_res);
	xregs = devm_ioremap_resource(&pdev->dev, ext_res);
	mram_addr = devm_ioremap_resource(&pdev->dev, mesg_ram);

	if (!mram_addr || !xregs || !regs) {
		dev_err(&pdev->dev, "failed to map can port\n");
		ret = -ENOMEM;
		goto exit;
	}

	/* allocate the mttcan device */

	dev->irq = irq;
	priv->device = &pdev->dev;

	if (set_can_clk_src_and_rate(priv, 40000000))
		goto exit_free_device;

	/* set device-tree properties */
	priv->gpio_can_en.gpio = of_get_named_gpio_flags(np,
					"gpio_can_en", 0, &flags);
	priv->gpio_can_en.active_low = flags & OF_GPIO_ACTIVE_LOW;
	priv->gpio_can_stb.gpio = of_get_named_gpio_flags(np,
					"gpio_can_stb", 0, &flags);
	priv->gpio_can_stb.active_low = flags & OF_GPIO_ACTIVE_LOW;
	priv->instance = of_alias_get_id(np, "mttcan");
	priv->poll = of_property_read_bool(np, "use-polling");
	of_property_read_u32_array(np, "tt-param", priv->tt_param, 2);
	if (of_property_read_u32_array(np, "tx-config",
		priv->tx_conf, TX_CONF_MAX)) {
		dev_err(priv->device, "tx-config missing\n");
		goto exit_free_device;
	}
	if (of_property_read_u32_array(np, "rx-config",
		priv->rx_conf, RX_CONF_MAX)) {
		dev_err(priv->device, "rx-config missing\n");
		goto exit_free_device;
	}
	if (of_property_read_u32_array(np, "mram-params",
			priv->mram_param, MTT_CAN_MAX_MRAM_ELEMS)) {
		dev_err(priv->device, "mram-param missing\n");
		goto exit_free_device;
	}

	if (gpio_is_valid(priv->gpio_can_stb.gpio)) {
		if (devm_gpio_request(priv->device, priv->gpio_can_stb.gpio,
			"gpio_can_stb") < 0) {
			dev_err(priv->device, "stb gpio request failed\n");
			goto exit_free_device;
		}
	}
	if (gpio_is_valid(priv->gpio_can_en.gpio)) {
		if (devm_gpio_request(priv->device, priv->gpio_can_en.gpio,
			"gpio_can_en") < 0) {
			dev_err(priv->device, "stb gpio request failed\n");
			goto exit_free_device;
		}
	}


	/* allocate controller struct memory and set fields */
	priv->ttcan =
	    devm_kzalloc(priv->device, sizeof(struct ttcan_controller),
			 GFP_KERNEL);
	if (!priv->ttcan) {
		dev_err(priv->device,
			"cannot allocate memory for ttcan_controller\n");
		goto exit_free_device;
	}
	memset(priv->ttcan, 0, sizeof(struct ttcan_controller));
	priv->ttcan->base = regs;
	priv->ttcan->xbase = xregs;
	priv->ttcan->mram_base = mesg_ram->start;
	priv->ttcan->id = priv->instance;
	priv->ttcan->mram_vbase = mram_addr;
	INIT_LIST_HEAD(&priv->ttcan->rx_q0);
	INIT_LIST_HEAD(&priv->ttcan->rx_q1);
	INIT_LIST_HEAD(&priv->ttcan->rx_b);
	INIT_LIST_HEAD(&priv->ttcan->tx_evt);

	platform_set_drvdata(pdev, dev);
	SET_NETDEV_DEV(dev, &pdev->dev);

	if (priv->poll) {
		dev_info(&pdev->dev, "Polling Mode enabled\n");
		INIT_DELAYED_WORK(&priv->can_work, mttcan_work);
	}

	ret = mttcan_prepare_clock(priv);
	if (ret)
		goto exit_free_device;

	ret = mttcan_hw_init(priv);
	if (ret)
		goto exit_free_device;

	ret = register_mttcan_dev(dev);
	if (ret) {
		dev_err(&pdev->dev, "registering %s failed (err=%d)\n",
			KBUILD_MODNAME, ret);
		goto exit_hw_deinit;
	}

	ret = mttcan_create_sys_files(&dev->dev);
	if (ret)
		goto exit_unreg_candev;

	dev_info(&dev->dev, "%s device registered (regs=%p, irq=%d)\n",
		 KBUILD_MODNAME, priv->ttcan->base, dev->irq);
	return 0;

exit_unreg_candev:
	unregister_mttcan_dev(dev);
exit_hw_deinit:
	mttcan_hw_deinit(priv);
	mttcan_unprepare_clock(priv);
exit_free_device:
	platform_set_drvdata(pdev, NULL);
exit_free_can:
	free_mttcan_dev(dev);
exit:
	dev_err(&pdev->dev, "probe failed\n");
	return ret;
}

static int mttcan_remove(struct platform_device *pdev)
{
	struct net_device *dev = platform_get_drvdata(pdev);
	struct mttcan_priv *priv = netdev_priv(dev);
	if (priv->poll)
		cancel_delayed_work_sync(&priv->can_work);

	dev_info(&dev->dev, "%s\n", __func__);

	mttcan_delete_sys_files(&dev->dev);
	unregister_mttcan_dev(dev);
	mttcan_unprepare_clock(priv);
	platform_set_drvdata(pdev, NULL);
	free_mttcan_dev(dev);

	return 0;
}

#ifdef CONFIG_PM
static int mttcan_suspend(struct platform_device *pdev, pm_message_t state)
{
	int ret;
	struct net_device *ndev = platform_get_drvdata(pdev);
	struct mttcan_priv *priv = netdev_priv(ndev);

	if (netif_running(ndev)) {
		netif_stop_queue(ndev);
		netif_device_detach(ndev);
	}
	ret = mttcan_power_down(ndev);
	if (ret) {
		netdev_err(ndev, "failed to enter power down mode\n");
		return ret;
	}

	if (ndev->flags & IFF_UP)
		mttcan_stop(priv);

	priv->can.state = CAN_STATE_SLEEPING;
	return 0;
}

static int mttcan_resume(struct platform_device *pdev)
{
	int ret;
	struct net_device *ndev = platform_get_drvdata(pdev);
	struct mttcan_priv *priv = netdev_priv(ndev);

	ret = mttcan_power_up(priv);
	if (ret)
		return ret;

	ret = mttcan_hw_reinit(priv);
	if (ret)
		return ret;

	if (ndev->flags & IFF_UP)
		mttcan_start(ndev);

	priv->can.state = CAN_STATE_ERROR_ACTIVE;
	if (netif_running(ndev)) {
		netif_device_attach(ndev);
		netif_start_queue(ndev);
	}
	return 0;
}
#endif

static struct platform_driver mttcan_plat_driver = {
	.driver = {
		   .name = KBUILD_MODNAME,
		   .owner = THIS_MODULE,
		   .of_match_table = of_match_ptr(mttcan_of_table),
		   },
	.probe = mttcan_probe,
	.remove = mttcan_remove,
#ifdef CONFIG_PM
	.suspend = mttcan_suspend,
	.resume = mttcan_resume,
#endif
	.id_table = mttcan_id_table,
};

module_platform_driver(mttcan_plat_driver);
MODULE_AUTHOR("Manoj Chourasia <mchourasia@nvidia.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Platform CAN bus driver for Bosch M_TTCAN controller");
