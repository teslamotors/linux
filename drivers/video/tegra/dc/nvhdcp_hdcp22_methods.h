/*
 * drivers/video/tegra/dc/nvhdcp_hdcp22_methods.h
 *
 * Copyright (c) 2014, NVIDIA CORPORATION, All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/* for 0x50 HDCP 2.2 support */
#define HDCP_22_SUPPORTED               (1 << 2)
#define HDCP_22_REPEATER                (0x0100)
#define HDCP_STATUS_READY               (0x200)

int tsec_hdcp_readcaps(struct hdcp_context_t *hdcp_context);
int tsec_hdcp_init(struct hdcp_context_t *hdcp_context);
int tsec_hdcp_create_session(struct hdcp_context_t *hdcp_context);
int tsec_hdcp_verify_cert(struct hdcp_context_t *hdcp_context);
int tsec_hdcp_generate_ekm(struct hdcp_context_t *hdcp_context);
int tsec_hdcp_verify_hprime(struct hdcp_context_t *hdcp_context);
int tsec_hdcp_encrypt_pairing_info(struct hdcp_context_t *hdcp_context);
int tsec_hdcp_generate_lc_init(struct hdcp_context_t *hdcp_context);
int tsec_hdcp_verify_lprime(struct hdcp_context_t *hdcp_context);
int tsec_hdcp_ske_init(struct hdcp_context_t *hdcp_context);
int tsec_hdcp_verify_vprime(struct hdcp_context_t *hdcp_context);
int tsec_hdcp_exchange_info(struct hdcp_context_t *hdcp_context,
		u32 method_flag,
		u8 *version,
		u16 *caps);
int tsec_hdcp_update_rrx(struct hdcp_context_t *hdcp_context);
int tsec_hdcp_rptr_stream_ready(struct hdcp_context_t *hdcp_context);
int tsec_hdcp_session_ctrl(struct hdcp_context_t *hdcp_context, int flag);
int tsec_hdcp_revocation_check(struct hdcp_context_t *hdcp_context);
int tsec_hdcp_rptr_stream_manage(struct hdcp_context_t *hdcp_context);
