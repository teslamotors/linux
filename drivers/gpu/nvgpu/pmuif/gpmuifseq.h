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
#ifndef _GPMUIFSEQ_H_
#define _GPMUIFSEQ_H_

#include "gk20a/pmu_common.h"

#define PMU_UNIT_SEQ            (0x02)

/*!
* @file   gpmuifseq.h
* @brief  PMU Command/Message Interfaces - Sequencer
*/

/*!
* Defines the identifiers various high-level types of sequencer commands.
*
* _RUN_SCRIPT @ref NV_PMU_SEQ_CMD_RUN_SCRIPT
*/
enum {
	NV_PMU_SEQ_CMD_ID_RUN_SCRIPT = 0,
};

struct nv_pmu_seq_cmd_run_script {
	u8 cmd_type;
	u8 pad[3];
	struct pmu_allocation_v3 script_alloc;
	struct pmu_allocation_v3 reg_alloc;
};

#define NV_PMU_SEQ_CMD_ALLOC_OFFSET              4

#define NV_PMU_SEQ_MSG_ALLOC_OFFSET                                         \
	(NV_PMU_SEQ_CMD_ALLOC_OFFSET + NV_PMU_CMD_ALLOC_SIZE)

struct nv_pmu_seq_cmd {
	struct pmu_hdr hdr;
	union {
		u8 cmd_type;
		struct nv_pmu_seq_cmd_run_script run_script;
	};
};

enum {
	NV_PMU_SEQ_MSG_ID_RUN_SCRIPT = 0,
};

struct nv_pmu_seq_msg_run_script {
	u8 msg_type;
	u8 error_code;
	u16 error_pc;
	u32 timeout_stat;
};

struct nv_pmu_seq_msg {
	struct pmu_hdr hdr;
	union {
		u8 msg_type;
		struct nv_pmu_seq_msg_run_script run_script;
	};
};

#endif
