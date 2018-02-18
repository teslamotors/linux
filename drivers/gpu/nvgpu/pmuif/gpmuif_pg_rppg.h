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
#ifndef _GPMUIFRPPG_H_
#define _GPMUIFRPPG_H_

#define NV_PMU_RPPG_CTRL_ID_GR    (0x0000)
#define NV_PMU_RPPG_CTRL_ID_MS    (0x0001)
#define NV_PMU_RPPG_CTRL_ID_DI    (0x0002)
#define NV_PMU_RPPG_CTRL_ID_MAX   (0x0003)

#define NV_PMU_RPPG_CTRL_MASK_ENABLE_ALL  (BIT(NV_PMU_RPPG_CTRL_ID_GR) |\
	BIT(NV_PMU_RPPG_CTRL_ID_MS) |\
	BIT(NV_PMU_RPPG_CTRL_ID_DI))

#define NV_PMU_RPPG_CTRL_MASK_DISABLE_ALL 0

enum {
	NV_PMU_RPPG_DOMAIN_ID_GFX = 0x0,
	NV_PMU_RPPG_DOMAIN_ID_NON_GFX,
};

struct nv_pmu_rppg_ctrl_stats {
	u32 entry_count;
	u32 exit_count;
};

struct nv_pmu_rppg_cmd_common {
	u8 cmd_type;
	u8 cmd_id;
};

struct nv_pmu_rppg_cmd_init {
	u8 cmd_type;
	u8 cmd_id;
};

struct nv_pmu_rppg_cmd_init_ctrl {
	u8 cmd_type;
	u8 cmd_id;
	u8 ctrl_id;
	u8 domain_id;
};

struct nv_pmu_rppg_cmd_stats_reset {
	u8 cmd_type;
	u8 cmd_id;
	u8 ctrl_id;
};

struct nv_pmu_rppg_cmd {
	union {
		u8 cmd_type;
		struct nv_pmu_rppg_cmd_common cmn;
		struct nv_pmu_rppg_cmd_init init;
		struct nv_pmu_rppg_cmd_init_ctrl init_ctrl;
		struct nv_pmu_rppg_cmd_stats_reset stats_reset;
	};
};

enum {
	NV_PMU_RPPG_CMD_ID_INIT = 0x0,
	NV_PMU_RPPG_CMD_ID_INIT_CTRL,
	NV_PMU_RPPG_CMD_ID_STATS_RESET,
};


struct nv_pmu_rppg_msg_common {
	u8 msg_type;
	u8 msg_id;
};

struct nv_pmu_rppg_msg_init_ctrl_ack {
	u8 msg_type;
	u8 msg_id;
	u8 ctrl_id;
	u32 stats_dmem_offset;
};

struct nv_pmu_rppg_msg {
	union {
		u8 msg_type;
		struct nv_pmu_rppg_msg_common cmn;
		struct nv_pmu_rppg_msg_init_ctrl_ack init_ctrl_ack;
	};
};

enum {
	NV_PMU_RPPG_MSG_ID_INIT_CTRL_ACK = 0x0,
};

#endif
