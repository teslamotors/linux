/*
 * Copyright (c) 2015-2016 NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __TEGRA_VIRT_ALT_IVC_COMMON_H__
#define __TEGRA_VIRT_ALT_IVC_COMMON_H__

#include <linux/types.h>

enum ivc_audio_err_t {
	NVAUDIO_ERR_OK,
	NVAUDIO_ERR_SERVER_STATE,
	NVAUDIO_ERR_ARGS,
	NVAUDIO_ERR_REQ,
	NVAUDIO_ERR_UNSUPPORTED_REQ
};

enum rx_state_t {
	RX_INIT,
	RX_PENDING,
	RX_AVAIL,
	RX_DONE,
};

enum nvaudio_ivc_cmd_t {
	NVAUDIO_DMAIF_SET_RXCIF,
	NVAUDIO_DMAIF_SET_TXCIF,
	NVAUDIO_START_PLAYBACK,
	NVAUDIO_STOP_PLAYBACK,
	NVAUDIO_START_CAPTURE,
	NVAUDIO_STOP_CAPTURE,
	NVAUDIO_T124_DAM_SET_IN_SRATE,
	NVAUDIO_T124_DAM_GET_IN_SRATE,
	NVAUDIO_T124_DAM_SET_OUT_SRATE,
	NVAUDIO_T124_DAM_GET_OUT_SRATE,
	NVAUDIO_T124_DAM_CHANNEL_SET_GAIN,
	NVAUDIO_T124_DAM_CHANNEL_GET_GAIN,
	NVAUDIO_XBAR_SET_ROUTE,
	NVAUDIO_XBAR_GET_ROUTE,
	NVAUDIO_AMIXER_SET_RX_GAIN,
	NVAUDIO_AMIXER_SET_TX_ADDER_CONFIG,
	NVAUDIO_AMIXER_SET_ENABLE,
	NVAUDIO_AMIXER_GET_TX_ADDER_CONFIG,
	NVAUDIO_AMIXER_GET_ENABLE,
	NVAUDIO_CMD_MAX,
};

struct nvaudio_ivc_t124_dam_info {
	int32_t		id;
	uint32_t	in_srate;
	uint32_t	out_srate;
	uint32_t	channel_reg;
	uint32_t	gain;
};

struct nvaudio_ivc_t210_amixer_info {
	int32_t		id;
	uint32_t	rx_idx;
	uint32_t	gain;
	uint32_t	adder_idx;
	uint32_t	adder_rx_idx;
	uint32_t	adder_rx_idx_enable;
	uint32_t	enable;
};

struct nvaudio_ivc_xbar_link {
	uint32_t	rx_reg;
	uint32_t	tx_value;
	uint32_t	tx_idx;
	uint32_t	bit_pos;
};

struct nvaudio_ivc_dmaif_info {
	int32_t	id;
	int32_t	value;
};

struct nvaudio_ivc_msg {
	int32_t			channel_id;
	enum nvaudio_ivc_cmd_t	cmd;
	union {
		struct nvaudio_ivc_dmaif_info		dmaif_info;
		struct nvaudio_ivc_t124_dam_info	dam_info;
		struct nvaudio_ivc_t210_amixer_info	amixer_info;
		struct nvaudio_ivc_xbar_link		xbar_info;
	} params;
	bool			ack_required;
	int32_t			err;
};

#endif
