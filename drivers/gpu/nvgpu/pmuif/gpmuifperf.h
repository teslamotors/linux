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
#ifndef _GPMUIFPERF_H_
#define _GPMUIFPERF_H_

#include "gpmuifvolt.h"
#include "gpmuifperfvfe.h"

/*
* Enumeration of BOARDOBJGRP class IDs within OBJPERF.  Used as "classId"
* argument for communications between Kernel and PMU via the various generic
* BOARDOBJGRP interfaces.
*/
#define NV_PMU_PERF_BOARDOBJGRP_CLASS_ID_VFE_VAR                            0x00
#define NV_PMU_PERF_BOARDOBJGRP_CLASS_ID_VFE_EQU                            0x01

#define NV_PMU_PERF_CMD_ID_RPC                                   (0x00000002)
#define NV_PMU_PERF_CMD_ID_BOARDOBJ_GRP_SET                      (0x00000003)
#define NV_PMU_PERF_CMD_ID_BOARDOBJ_GRP_GET_STATUS               (0x00000004)

struct nv_pmu_perf_cmd_set_object {
	u8 cmd_type;
	u8 pad[2];
	u8 object_type;
	struct nv_pmu_allocation object;
};

#define NV_PMU_PERF_SET_OBJECT_ALLOC_OFFSET                            \
	(offsetof(struct nv_pmu_perf_cmd_set_object, object))

/* RPC IDs */
#define NV_PMU_PERF_RPC_ID_VFE_LOAD                                 (0x00000001)

/*!
* Command requesting execution of the perf RPC.
*/
struct nv_pmu_perf_cmd_rpc {
	u8 cmd_type;
	u8 pad[3];
	struct nv_pmu_allocation request;
};

#define NV_PMU_PERF_CMD_RPC_ALLOC_OFFSET       \
	offsetof(struct nv_pmu_perf_cmd_rpc, request)

/*!
* Simply a union of all specific PERF commands. Forms the general packet
* exchanged between the Kernel and PMU when sending and receiving PERF commands
* (respectively).
*/
struct nv_pmu_perf_cmd {
	union {
	u8 cmd_type;
	struct nv_pmu_perf_cmd_set_object set_object;
	struct nv_pmu_boardobj_cmd_grp grp_set;
	struct nv_pmu_boardobj_cmd_grp grp_get_status;
	};
};

/*!
* Defines the data structure used to invoke PMU perf RPCs. Same structure is
* used to return the result of the RPC execution.
*/
struct nv_pmu_perf_rpc {
	u8 function;
	bool b_supported;
	bool b_success;
	flcn_status flcn_status;
	union {
		struct nv_pmu_perf_rpc_vfe_equ_eval vfe_equ_eval;
		struct nv_pmu_perf_rpc_vfe_load vfe_load;
	} params;
};


/* PERF Message-type Definitions */
#define NV_PMU_PERF_MSG_ID_RPC                                      (0x00000003)
#define NV_PMU_PERF_MSG_ID_BOARDOBJ_GRP_SET                         (0x00000004)
#define NV_PMU_PERF_MSG_ID_BOARDOBJ_GRP_GET_STATUS                  (0x00000006)
#define NV_PMU_PERF_MSG_ID_VFE_CALLBACK                             (0x00000005)

/*!
* Message carrying the result of the perf RPC execution.
*/
struct nv_pmu_perf_msg_rpc {
	u8 msg_type;
	u8 rsvd[3];
	struct nv_pmu_allocation response;
};

#define NV_PMU_PERF_MSG_RPC_ALLOC_OFFSET       \
	(offsetof(struct nv_pmu_perf_msg_rpc, response))

/*!
* Simply a union of all specific PERF messages. Forms the general packet
* exchanged between the Kernel and PMU when sending and receiving PERF messages
* (respectively).
*/
struct nv_pmu_perf_msg {
	union {
		u8 msg_type;
		struct nv_pmu_perf_msg_rpc rpc;
		struct nv_pmu_boardobj_msg_grp grp_set;
	};
};

#endif  /* _GPMUIFPERF_H_*/
