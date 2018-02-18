/*
 * Copyright (C) 2016 NVIDIA CORPORATION.  All rights reserved.
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

#ifndef TEGRA_TSEC_H
#define TEGRA_TSEC_H

#include "drm.h"
#include "falcon.h"

#define NV_PSEC_THI_METHOD1	0x00000044	/* RW-4R */
#define NV_PSEC_THI_METHOD0	0x00000040	/* RW-4R */

#define HDCP_ALIGNMENT_256		256

#define HDCP_SCRATCH_BUFFER_SIZE	2304
#define HDCP_SCRATCH_BUFFER_SIZE_ALIGNED 2304

#define HDCP_DCP_KPUB_SIZE		388
#define HDCP_DCP_KPUB_SIZE_ALIGNED	512

#define HDCP_SRM_SIZE			512
#define HDCP_SRM_SIZE_ALIGNED		512

#define HDCP_CERT_SIZE			522
#define HDCP_CERT_SIZE_ALIGNED		768

#define HDCP_MTHD_RPLY_BUF_SIZE		256
#define HDCP_MTHD_BUF_SIZE		256

#define HDCP_RCVR_ID_LIST_SIZE		635
#define HDCP_CONTENT_BUF_SIZE		2048

/* Size in bytes */
#define HDCP_SIZE_RECV_ID_8	(40/8)
#define HDCP_SIZE_RTX_8		(64/8)
#define HDCP_SIZE_RTX_64	(HDCP_SIZE_RTX_8/8)
#define HDCP_SIZE_RRX_8		(64/8)
#define HDCP_SIZE_RRX_64	(HDCP_SIZE_RRX_8/8)
#define HDCP_SIZE_RN_8		(64/8)
#define HDCP_SIZE_RN_64		(HDCP_SIZE_RTX_8/8)
#define HDCP_SIZE_RIV_8		(64/8)
#define HDCP_SIZE_RIV_64	(HDCP_SIZE_RIV_8/8)
#define HDCP_SIZE_CERT_RX_8	(4176/8)
#define HDCP_SIZE_DCP_KPUB_8	(3072/8)
#define HDCP_SIZE_DCP_KPUB_64	(HDCP_SIZE_DCP_KPUB_8/8)
#define HDCP_SIZE_RX_KPUB_8	(1048/8)

/*each receiver ID is 5 bytes long * 128 max receivers */
#define HDCP_SIZE_MAX_RECV_ID_LIST_8	640
#define HDCP_SIZE_MAX_RECV_ID_LIST_64	(HDCP_SIZE_MAX_RECV_ID_LIST_8/8)
#define HDCP_SIZE_PVT_RX_KEY_8		(340)
#define HDCP_SIZE_TMR_INFO_8		(5)
#define HDCP_SIZE_RCV_INFO_8		(5)
#define HDCP_SIZE_KM_8			(16)
#define HDCP_SIZE_DKEY_8		(16)
#define HDCP_SIZE_KD_8			(32)
#define HDCP_SIZE_KH_8			(16)
#define HDCP_SIZE_KS_8			(16)
#define HDCP_SIZE_M_8			(128/8)
#define HDCP_SIZE_M_64			(HDCP_SIZE_M_8/8)
#define HDCP_SIZE_E_KM_8		(1024/8)
#define HDCP_SIZE_E_KM_64		(HDCP_SIZE_E_KM_8/8)
#define HDCP_SIZE_EKH_KM_8		(128/8)
#define HDCP_SIZE_EKH_KM_64		(HDCP_SIZE_EKH_KM_8/8)
#define HDCP_SIZE_E_KS_8		(128/8)
#define HDCP_SIZE_E_KS_64		(HDCP_SIZE_E_KS_8/8)

/* 96 bytes,	round up from 85 bytes */
#define HDCP_SIZE_EPAIR_8		96
#define HDCP_SIZE_EPAIR_64		(HDCP_SIZE_EPAIR_8/8)
#define HDCP_SIZE_EPAIR_SIGNATURE_8	(256/8)
#define HDCP_SIZE_EPAIR_SIGNATURE_64	(HDCP_SIZE_EPAIR_SIGNATURE_8/8)
#define HDCP_SIZE_HPRIME_8		(256/8)
#define HDCP_SIZE_HPRIME_64		(HDCP_SIZE_HPRIME_8/8)
#define HDCP_SIZE_LPRIME_8		(256/8)
#define HDCP_SIZE_LPRIME_64		(HDCP_SIZE_LPRIME_8/8)
#define HDCP_SIZE_MPRIME_8		(256/8)
#define HDCP_SIZE_MPRIME_64		(HDCP_SIZE_MPRIME_8/8)
#define HDCP_SIZE_VPRIME_2X_8		(256/8)
#define HDCP_SIZE_VPRIME_2X_64		(HDCP_SIZE_VPRIME_2X_8/8)
#define HDCP_SIZE_SPRIME_8		384
#define HDCP_SIZE_SPRIME_64		(HDCP_SIZE_SPRIME_8/8)
#define HDCP_SIZE_SEQ_NUM_V_8		3
#define HDCP_SIZE_SEQ_NUM_M_8		3
#define HDCP_SIZE_CONTENT_ID_8		2
#define HDCP_SIZE_CONTENT_TYPE_8	1
#define HDCP_SIZE_PES_HDR_8		(128/8)
#define HDCP_SIZE_PES_HDR_64		(HDCP_SIZE_PES_HDR_8/8)
#define HDCP_SIZE_CHIP_NAME		(8)
#define HDCP_VERIFY_VPRIME_MAX_ATTEMPTS 3

/* HDCP1X uses SHA1 for VPRIME which produces 160 bits of output */
#define HDCP_SIZE_VPRIME_1X_8		(160/8)
#define HDCP_SIZE_VPRIME_1X_32		(HDCP_SIZE_VPRIME_1X_8/4)
#define HDCP_SIZE_LPRIME_1X_8		(160/8)
#define HDCP_SIZE_LPRIME_1X_32		(HDCP_SIZE_LPRIME_1X_8/4)
#define HDCP_SIZE_QID_1X_8		(64/8)
#define HDCP_SIZE_QID_1X_64		(HDCP_SIZE_QID_1X_8/8)

#define MAX_DEVS			127
#define MAX_STREAMS			1

#define SET_APPLICATION_ID		(0x00000200)
#define SET_APPLICATION_ID_ID_HDCP	(0x00000001)
#define EXECUTE				(0x00000300)
#define HDCP_INIT			(0x00000500)
#define HDCP_CREATE_SESSION		(0x00000504)
#define HDCP_VERIFY_CERT_RX		(0x00000508)
#define HDCP_GENERATE_EKM		(0x0000050C)
#define HDCP_REVOCATION_CHECK		(0x00000510)
#define HDCP_VERIFY_HPRIME		(0x00000514)
#define HDCP_ENCRYPT_PAIRING_INFO	(0x00000518)
#define HDCP_DECRYPT_PAIRING_INFO	(0x0000051C)
#define HDCP_UPDATE_SESSION		(0x00000520)
#define HDCP_GENERATE_LC_INIT		(0x00000524)
#define HDCP_VERIFY_LPRIME		(0x00000528)
#define HDCP_GENERATE_SKE_INIT		(0x0000052C)
#define HDCP_VERIFY_VPRIME		(0x00000530)
#define HDCP_ENCRYPTION_RUN_CTRL	(0x00000534)
#define HDCP_SESSION_CTRL		(0x00000538)
#define HDCP_COMPUTE_SPRIME		(0x0000053C)
#define HDCP_GET_CERT_RX		(0x00000540)
#define HDCP_EXCHANGE_INFO		(0x00000544)
#define HDCP_DECRYPT_KM			(0x00000548)
#define HDCP_GET_HPRIME			(0x0000054C)
#define HDCP_GENERATE_EKH_KM		(0x00000550)
#define HDCP_VERIFY_RTT_CHALLENGE	(0x00000554)
#define HDCP_GET_LPRIME			(0x00000558)
#define HDCP_DECRYPT_KS			(0x0000055C)
#define HDCP_DECRYPT			(0x00000560)
#define HDCP_GET_RRX			(0x00000564)
#define HDCP_DECRYPT_REENCRYPT		(0x00000568)
#define HDCP_VALIDATE_SRM		(0x00000700)
#define HDCP_VALIDATE_STREAM		(0x00000704)
#define HDCP_TEST_SECURE_STATUS		(0x00000708)
#define HDCP_SET_DCP_KPUB		(0x0000070C)
#define HDCP_SET_RX_KPUB		(0x00000710)
#define HDCP_SET_CERT_RX		(0x00000714)
#define HDCP_SET_SCRATCH_BUFFER		(0x00000718)
#define HDCP_SET_SRM			(0x0000071C)
#define HDCP_SET_RECEIVER_ID_LIST	(0x00000720)
#define HDCP_SET_SPRIME			(0x00000724)
#define HDCP_SET_ENC_INPUT_BUFFER	(0x00000728)
#define HDCP_SET_ENC_OUTPUT_BUFFER	(0x0000072C)
#define HDCP_GET_RTT_CHALLENGE		(0x00000730)
#define HDCP_STREAM_MANAGE		(0x00000734)
#define HDCP_READ_CAPS			(0x00000738)
#define HDCP_ENCRYPT			(0x0000073C)

#define HDCP_MTHD_FLAGS_SB		0x00000001
#define HDCP_MTHD_FLAGS_DCP_KPUB	0x00000002
#define HDCP_MTHD_FLAGS_SRM		0x00000004
#define HDCP_MTHD_FLAGS_CERT		0x00000008
#define HDCP_MTHD_FLAGS_RECV_ID_LIST	0x00000010
#define HDCP_MTHD_FLAGS_INPUT_BUFFER	0x00000020
#define HDCP_MTHD_FLAGS_OUTPUT_BUFFER	0x00000040

struct nvhdcp_msg {
	/* AKE_INIT */
	u8	ake_init_msg_id;
	u64				rtx;
	u8	txcaps_version;
	u16				txcaps_capmask;
	/* SEND_CERT */
	u8	ake_send_cert_msg_id;
	u8	cert_rx[HDCP_CERT_SIZE];
	u64				rrx;
	u8	rxcaps_version;
	u16				rxcaps_capmask;
	/* NO_STORED_KM */
	u8	ake_no_stored_km_msg_id;
	u8	ekm[HDCP_SIZE_E_KM_8];
	/* STORED_KM */
	u8	ake_stored_km_msg_id;
	u8	eKhKm[HDCP_SIZE_EKH_KM_8];
	u8	m[HDCP_SIZE_M_8];
	/* SEND_HPRIME */
	u8	ake_send_hprime_msg_id;
	u8	hprime[HDCP_SIZE_HPRIME_8];
	/* SEND_PAIRING_INFO */
	u8	ake_send_pairing_info_msg_id;
	u8	ekhkm[HDCP_SIZE_EKH_KM_8];
	/* LC_INIT */
	u8	lc_init_msg_id;
	u64				rn;
	/* LC_SEND_LPRIME */
	u8	lc_send_lprime_msg_id;
	u8	lprime[HDCP_SIZE_LPRIME_8];
	/* SEND_EKS */
	u8	ske_send_eks_msg_id;
	u8	eks[HDCP_SIZE_E_KS_8];
	u64				riv;
	/* SEND_RECEIVERID_LIST */
	u8	send_receiverid_list_msg_id;
	u16				rxinfo;
	u8	seq_num[HDCP_SIZE_SEQ_NUM_V_8];
	u8	vprime[HDCP_SIZE_VPRIME_2X_8/2];
	u8	rcvr_id_list[HDCP_RCVR_ID_LIST_SIZE];
	/* REPEATER_AUTH_SEND_ACK  */
	u8	rptr_send_ack_msg_id;
	u8	v[HDCP_SIZE_VPRIME_2X_8/2];
	/* REPEATER_AUTH_STREAM_MANAGE */
	u8	rptr_auth_stream_manage_msg_id;
	u8	seq_num_m[HDCP_SIZE_SEQ_NUM_M_8];
	u16				k;
	u16				streamid_type[MAX_STREAMS];
	/* REPEATER_AUTH_STREAM_READY */
	u8	rptr_auth_stream_ready_msg_id;
	u8	mprime[HDCP_SIZE_MPRIME_8];
} __packed;

struct hdcp_context_t {
	u32		*cpuvaddr_scratch;
	dma_addr_t	dma_handle_scratch;
	u32		*cpuvaddr_scratch_aligned;
	dma_addr_t	dma_handle_scratch_aligned;
	u32		*cpuvaddr_dcp_kpub;
	dma_addr_t	dma_handle_dcp_kpub;
	u32		*cpuvaddr_dcp_kpub_aligned;
	dma_addr_t	dma_handle_dcp_kpub_aligned;
	u32		*cpuvaddr_srm;
	dma_addr_t	dma_handle_srm;
	u32		*cpuvaddr_cert;
	dma_addr_t	dma_handle_cert;
	u32		*cpuvaddr_cert_aligned;
	dma_addr_t	dma_handle_cert_aligned;
	u32		*cpuvaddr_mthd_buf;
	dma_addr_t	dma_handle_mthd_buf;
	u32		cpuvaddr_mthd_buf_aligned;
	dma_addr_t	dma_handle_mthd_buf_aligned;
	u32		*cpuvaddr_rcvr_id_list;
	dma_addr_t	dma_handle_rcvr_id_list;
	u32		*cpuvaddr_rcvr_id_list_aligned;
	dma_addr_t	dma_handle_rcvr_id_list_aligned;
	u32		*cpuvaddr_input_buf;
	dma_addr_t	dma_handle_input_buf;
	u32		*cpuvaddr_input_buf_aligned;
	dma_addr_t	dma_handle_input_buf_aligned;
	u32		*cpuvaddr_output_buf;
	dma_addr_t	dma_handle_output_buf;
	u32		*cpuvaddr_output_buf_aligned;
	dma_addr_t	dma_handle_output_buf_aligned;
	/* display type is 0 for HDMI and 1 for DP */
	u32		display_type;
	u32		session_id;

	struct nvhdcp_msg				 msg;
};

struct hdcp_context_priv_bo_t {
	struct tegra_bo *scratch;
	struct tegra_bo *dcp_kpub;
	struct tegra_bo *srm;
	struct tegra_bo *cert;
	struct tegra_bo *mthd_buf;
	struct tegra_bo *rcvr_id_list;
	struct tegra_bo *input_buf;
	struct tegra_bo *output_buf;
};

struct tsec_config {
	/* Firmware name */
	const char *ucode_name;
	const struct tegra_drm_client_ops *drm_client_ops;

	u32 class_id;
};

struct tsec {
	struct falcon falcon;
	bool booted;

	void __iomem *regs;
	struct tegra_drm_client client;
	struct host1x_channel *channel;
	struct iommu_domain *domain;
	struct device *dev;
	struct clk *clk;
	struct reset_control *rst;

	/* Platform configuration */
	const struct tsec_config *config;

	/* for firewall - this determines if method 1 should be regarded
	 * as an address register */
	bool method_data_is_addr_reg;

	struct drm_device *tsec_drm;
};

static inline struct tsec *to_tsec(struct tegra_drm_client *client)
{
	return container_of(client, struct tsec, client);
}

static void __maybe_unused tsec_writel(struct tsec *tsec, u32 value, unsigned int offset)
{
	writel(value, tsec->regs + offset);
}

int tsec_open_channel(struct tegra_drm_client *client,
			    struct tegra_drm_context *context);
void tsec_close_channel(struct tegra_drm_context *context);

#endif /* TEGRA_TSEC_H */
