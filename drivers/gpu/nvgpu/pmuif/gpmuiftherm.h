/*
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

#ifndef _GPMUIFTHERM_H_
#define _GPMUIFTHERM_H_

#include "gk20a/pmu_common.h"

#define NV_PMU_THERM_CMD_ID_RPC                                      0x00000002
#define NV_PMU_THERM_MSG_ID_RPC                                      0x00000002
#define NV_PMU_THERM_RPC_ID_SLCT                                     0x00000000
#define NV_PMU_THERM_RPC_ID_SLCT_EVENT_TEMP_TH_SET                   0x00000006
#define NV_PMU_THERM_EVENT_THERMAL_1                                 0x00000004
#define NV_PMU_THERM_CMD_ID_HW_SLOWDOWN_NOTIFICATION                 0x00000001
#define NV_RM_PMU_THERM_HW_SLOWDOWN_NOTIFICATION_REQUEST_ENABLE      0x00000001
#define NV_PMU_THERM_MSG_ID_EVENT_HW_SLOWDOWN_NOTIFICATION           0x00000001

struct nv_pmu_therm_rpc_slct_event_temp_th_set {
	s32 temp_threshold;
	u8 event_id;
	flcn_status flcn_stat;
};

struct nv_pmu_therm_rpc_slct {
	u32 mask_enabled;
	flcn_status flcn_stat;
};

struct nv_pmu_therm_rpc {
	u8 function;
	bool b_supported;
	union {
		struct nv_pmu_therm_rpc_slct slct;
		struct nv_pmu_therm_rpc_slct_event_temp_th_set slct_event_temp_th_set;
	} params;
};

struct nv_pmu_therm_cmd_rpc {
	u8 cmd_type;
	u8 pad[3];
	struct nv_pmu_allocation request;
};

struct nv_pmu_therm_cmd_hw_slowdown_notification {
	u8 cmd_type;
	u8 request;
};

#define NV_PMU_THERM_CMD_RPC_ALLOC_OFFSET       \
	offsetof(struct nv_pmu_therm_cmd_rpc, request)

struct nv_pmu_therm_cmd {
	union {
		u8 cmd_type;
		struct nv_pmu_therm_cmd_rpc rpc;
		struct nv_pmu_therm_cmd_hw_slowdown_notification hw_slct_notification;
	};
};

struct nv_pmu_therm_msg_rpc {
	u8 msg_type;
	u8 rsvd[3];
	struct nv_pmu_allocation response;
};

struct nv_pmu_therm_msg_event_hw_slowdown_notification {
	u8 msg_type;
	u32 mask;
};

#define NV_PMU_THERM_MSG_RPC_ALLOC_OFFSET       \
	offsetof(struct nv_pmu_therm_msg_rpc, response)

struct nv_pmu_therm_msg {
	union {
		u8 msg_type;
		struct nv_pmu_therm_msg_rpc rpc;
		struct nv_pmu_therm_msg_event_hw_slowdown_notification hw_slct_msg;
	};
};

#endif

