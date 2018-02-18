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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "m_ttcan.h"

#define TTCAN_MAX_LIST_MEMBERS 128

inline int is_msg_list_full(struct ttcan_controller *ttcan,
		enum ttcan_rx_type rxtype)
{
	return ttcan->list_status & rxtype & 0xFF;
}

int add_msg_controller_list(struct ttcan_controller *ttcan,
			    struct ttcanfd_frame *ttcanfd,
			    struct list_head *rx_q,
			    enum ttcan_rx_type rxtype)
{

	struct ttcan_rx_msg_list *msg_list;

	msg_list = (struct ttcan_rx_msg_list *)
		kzalloc(sizeof(struct ttcan_rx_msg_list), GFP_KERNEL);
	if (msg_list == NULL) {
		pr_err("%s: memory allocation failed\n", __func__);
		return -ENOMEM;
	}

	INIT_LIST_HEAD(&msg_list->recv_list);

	memcpy(&msg_list->msg, ttcanfd, sizeof(struct ttcanfd_frame));

	spin_lock(&ttcan->lock);

	if (is_msg_list_full(ttcan, rxtype)) {
		pr_err("%s: mesg list is full\n", __func__);
		kfree(msg_list);
		spin_unlock(&ttcan->lock);
		return -ENOMEM;
	}

	list_add_tail(&msg_list->recv_list, rx_q);

	switch (rxtype) {
	case BUFFER:
		if (++ttcan->rxb_mem >= TTCAN_MAX_LIST_MEMBERS)
			ttcan->list_status |= BUFFER;
		break;
	case FIFO_0:
		if (++ttcan->rxq0_mem >= TTCAN_MAX_LIST_MEMBERS)
			ttcan->list_status |= FIFO_0;
		break;
	case FIFO_1:
		if (++ttcan->rxq1_mem >= TTCAN_MAX_LIST_MEMBERS)
			ttcan->list_status |= FIFO_1;
	default:
		break;
	}
	spin_unlock(&ttcan->lock);

	return 0;
}

int add_event_controller_list(struct ttcan_controller *ttcan,
			    struct mttcan_tx_evt_element *txevt,
			    struct list_head *evt_q)
{

	struct ttcan_txevt_msg_list *evt_list;

	evt_list =
	    (struct ttcan_txevt_msg_list *)
	    kzalloc(sizeof(struct ttcan_txevt_msg_list), GFP_KERNEL);
	if (evt_list == NULL) {
		pr_err("%s: memory allocation failed\n", __func__);
		return -ENOMEM;
	}

	INIT_LIST_HEAD(&evt_list->txevt_list);

	memcpy(&evt_list->txevt, txevt, sizeof(struct mttcan_tx_evt_element));

	spin_lock(&ttcan->lock);

	if (is_msg_list_full(ttcan, TX_EVT)) {
		pr_err("%s: mesg list is full\n", __func__);
		kfree(evt_list);
		spin_unlock(&ttcan->lock);
		return -1;
	}

	list_add_tail(&evt_list->txevt_list, evt_q);

	if (++ttcan->evt_mem >= TTCAN_MAX_LIST_MEMBERS)
		ttcan->list_status |= TX_EVT;

	spin_unlock(&ttcan->lock);

	return 0;
}
