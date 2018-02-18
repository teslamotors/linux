/*
 * general power channel structures & definitions
 *
 * Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef _PWRMONITOR_H_
#define _PWRMONITOR_H_

#include "boardobj/boardobjgrp.h"
#include "boardobj/boardobj.h"
#include "pmuif/gpmuifpmgr.h"
#include "ctrl/ctrlpmgr.h"

struct pwr_channel {
	struct boardobj super;
	u8 pwr_rail;
	u32 volt_fixed_uv;
	u32 pwr_corr_slope;
	s32 pwr_corr_offset_mw;
	u32 curr_corr_slope;
	s32 curr_corr_offset_ma;
	u32 dependent_ch_mask;
};

struct pwr_chrelationship {
	struct boardobj super;
	u8 chIdx;
};

struct pwr_channel_sensor {
	struct pwr_channel super;
	u8 pwr_dev_idx;
	u8 pwr_dev_prov_idx;
};

struct pmgr_pwr_monitor {
	bool b_is_topology_tbl_ver_1x;
	struct boardobjgrp_e32 pwr_channels;
	struct boardobjgrp_e32 pwr_ch_rels;
	u8 total_gpu_channel_idx;
	u32 physical_channel_mask;
	struct nv_pmu_pmgr_pwr_monitor_pack pmu_data;
};

#define PMGR_PWR_MONITOR_GET_PWR_CHANNEL(g, channel_idx)                    \
	((struct pwr_channel *)BOARDOBJGRP_OBJ_GET_BY_IDX(                                 \
		&(g->pmgr_pmu.pmgr_monitorobjs.pwr_channels.super), (channel_idx)))

u32 pmgr_monitor_sw_setup(struct gk20a *g);

#endif
