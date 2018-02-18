/*
 * Copyright (c) 2014-2015, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __SMMU_LIB_HV_H__
#define __SMMU_LIB_HV_H__

#ifdef CONFIG_TEGRA_HV_MANAGER
#include <linux/tegra-ivc.h>

extern int libsmmu_get_num_asids(int comm_chan_id, unsigned int *num_asids);
extern int libsmmu_get_dma_window(int comm_chan_id, u64 *base, size_t *size);
extern int libsmmu_get_swgids(int comm_chan_id, u64 *swgids);
extern int libsmmu_attach_hwgrp(int comm_chan_id, u32 asid, u32 hwgrp);
extern int libsmmu_detach_hwgrp(int comm_chan_id, u32 hwgrp);
extern int libsmmu_map_large_page(int comm_chan_id, u32 asid, u64 iova, u64 pa,
								int attr);
extern int libsmmu_map_page(int comm_chan_id, u32 asid, u64 iova, int count,
								int attr);
extern int libsmmu_unmap(int comm_chan_id, u32 asid, u64 iova, u64 bytes);
extern int libsmmu_iova_to_phys(int comm_chan_id, u32 asid, u64 iova, u64 *ipa);
extern int libsmmu_connect(int comm_chan_id);
extern int libsmmu_debug_op(int comm_chan_id, u32 op, u64 op_data_in,
							u64 *op_data_out);
extern int libsmmu_get_mempool_params(void **base, int *size);
extern int tegra_hv_smmu_comm_chan_alloc(void);
extern void tegra_hv_smmu_comm_chan_free(int chan_id);
extern int tegra_hv_smmu_comm_init(struct device *dev);

#define MAX_COMM_CHANS  130
#define COMM_CHAN_UNASSIGNED	MAX_COMM_CHANS
enum smmu_msg_t {
	SMMU_INFO,
	ATTACH,
	DETACH,
	MAP_PAGE,
	MAP_LARGE_PAGE,
	UNMAP_PAGE,
	IPA,
	CONNECT,
	DISCONNECT,
	DEBUG_OP,
	INVALID,
};

enum ivc_smmu_err {
	SMMU_ERR_OK,
	SMMU_ERR_SERVER_STATE,
	SMMU_ERR_PERMISSION,
	SMMU_ERR_ARGS,
	SMMU_ERR_REQ,
	SMMU_ERR_UNSUPPORTED_REQ
};

enum rx_state_t {
	RX_INIT,
	RX_PENDING,
	RX_AVAIL,
	RX_DONE,
};

struct smmu_sg_ent {
	uint64_t ipa;
	uint64_t count;
};

struct drv_ctxt {
	unsigned int asid;
	unsigned int hwgrp;
	u64 iova;
	u64 ipa;
	int count;
	int attr;
};

struct smmu_resource {
	uint64_t start;
	uint64_t end;
};

struct smmu_info {
	struct smmu_resource as_pool;
	struct smmu_resource window;
	uint64_t swgids;
};


struct smmu_ivc_msg {
	unsigned int s_marker;
	enum smmu_msg_t msg;
	int comm_chan_id;
	struct drv_ctxt dctxt;
	struct smmu_info sinfo;
	unsigned int err;
	unsigned int e_marker;
};

struct tegra_hv_smmu_comm_dev;

/* channel is virtual abstraction over a single ivc queue
 * each channel hold messagethat are independent of other channels
 * we allocate one channel per asid as these can operate in parallel
 */
struct tegra_hv_smmu_comm_chan {
	struct tegra_hv_ivc_cookie *ivck;
	int id;
	wait_queue_head_t wait;
	int timeout;
	struct smmu_ivc_msg rx_msg;
	enum rx_state_t rx_state;
	struct tegra_hv_smmu_comm_dev *hv_comm_dev;
	spinlock_t lock;
};

struct tegra_hv_smmu_comm_dev {
	struct tegra_hv_ivc_cookie *ivck;
	struct tegra_hv_ivm_cookie *ivm;
	void *virt_ivm_base;
	struct device *dev;
	spinlock_t ivck_rx_lock;
	spinlock_t ivck_tx_lock;
	spinlock_t lock;
	struct tegra_hv_smmu_comm_chan *hv_comm_chan[MAX_COMM_CHANS];
};
#endif
#endif
