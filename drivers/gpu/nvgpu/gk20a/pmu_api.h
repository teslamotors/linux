/*
 * Copyright (c) 2011-2016, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __PMU_API_H__
#define __PMU_API_H__

#include "pmu_common.h"
#include "pmuif/gpmuif_pg_rppg.h"

/* PMU Command/Message Interfaces for Adaptive Power */
/* Macro to get Histogram index */
#define PMU_AP_HISTOGRAM(idx)		(idx)
#define PMU_AP_HISTOGRAM_CONT		(4)

/* Total number of histogram bins */
#define PMU_AP_CFG_HISTOGRAM_BIN_N	(16)

/* Mapping between Idle counters and histograms */
#define PMU_AP_IDLE_MASK_HIST_IDX_0		(2)
#define PMU_AP_IDLE_MASK_HIST_IDX_1		(3)
#define PMU_AP_IDLE_MASK_HIST_IDX_2		(5)
#define PMU_AP_IDLE_MASK_HIST_IDX_3		(6)


/* Mapping between AP_CTRLs and Histograms */
#define PMU_AP_HISTOGRAM_IDX_GRAPHICS	(PMU_AP_HISTOGRAM(1))

/* Mapping between AP_CTRLs and Idle counters */
#define PMU_AP_IDLE_MASK_GRAPHICS	(PMU_AP_IDLE_MASK_HIST_IDX_1)

/* Adaptive Power Controls (AP_CTRL) */
enum {
	PMU_AP_CTRL_ID_GRAPHICS = 0x0,
	PMU_AP_CTRL_ID_MAX,
};

/* AP_CTRL Statistics */
struct pmu_ap_ctrl_stat {
	/*
	 * Represents whether AP is active or not
	 */
	u8	b_active;

	/* Idle filter represented by histogram bin index */
	u8	idle_filter_x;
	u8	rsvd[2];

	/* Total predicted power saving cycles. */
	s32	power_saving_h_cycles;

	/* Counts how many times AP gave us -ve power benefits. */
	u32	bad_decision_count;

	/*
	 * Number of times ap structure needs to skip AP iterations
	 * KICK_CTRL from kernel updates this parameter.
	 */
	u32	skip_count;
	u8	bin[PMU_AP_CFG_HISTOGRAM_BIN_N];
};

/* Parameters initialized by INITn APCTRL command */
struct pmu_ap_ctrl_init_params {
	/* Minimum idle filter value in Us */
	u32	min_idle_filter_us;

	/*
	 * Minimum Targeted Saving in Us. AP will update idle thresholds only
	 * if power saving achieved by updating idle thresholds is greater than
	 * Minimum targeted saving.
	 */
	u32	min_target_saving_us;

	/* Minimum targeted residency of power feature in Us */
	u32	power_break_even_us;

	/*
	 * Maximum number of allowed power feature cycles per sample.
	 *
	 * We are allowing at max "pgPerSampleMax" cycles in one iteration of AP
	 * AKA pgPerSampleMax in original algorithm.
	 */
	u32	cycles_per_sample_max;
};

/* AP Commands/Message structures */

/*
 * Structure for Generic AP Commands
 */
struct pmu_ap_cmd_common {
	u8	cmd_type;
	u16	cmd_id;
};

/*
 * Structure for INIT AP command
 */
struct pmu_ap_cmd_init {
	u8	cmd_type;
	u16	cmd_id;
	u8	rsvd;
	u32	pg_sampling_period_us;
};

/*
 * Structure for Enable/Disable ApCtrl Commands
 */
struct pmu_ap_cmd_enable_ctrl {
	u8	cmd_type;
	u16	cmd_id;

	u8	ctrl_id;
};

struct pmu_ap_cmd_disable_ctrl {
	u8	cmd_type;
	u16	cmd_id;

	u8	ctrl_id;
};

/*
 * Structure for INIT command
 */
struct pmu_ap_cmd_init_ctrl {
	u8				cmd_type;
	u16				cmd_id;
	u8				ctrl_id;
	struct pmu_ap_ctrl_init_params	params;
};

struct pmu_ap_cmd_init_and_enable_ctrl {
	u8				cmd_type;
	u16				cmd_id;
	u8				ctrl_id;
	struct pmu_ap_ctrl_init_params	params;
};

/*
 * Structure for KICK_CTRL command
 */
struct pmu_ap_cmd_kick_ctrl {
	u8	cmd_type;
	u16	cmd_id;
	u8	ctrl_id;

	u32	skip_count;
};

/*
 * Structure for PARAM command
 */
struct pmu_ap_cmd_param {
	u8	cmd_type;
	u16	cmd_id;
	u8	ctrl_id;

	u32	data;
};

/*
 * Defines for AP commands
 */
enum {
	PMU_AP_CMD_ID_INIT = 0x0,
	PMU_AP_CMD_ID_INIT_AND_ENABLE_CTRL,
	PMU_AP_CMD_ID_ENABLE_CTRL,
	PMU_AP_CMD_ID_DISABLE_CTRL,
	PMU_AP_CMD_ID_KICK_CTRL,
};

/*
 * AP Command
 */
union pmu_ap_cmd {
	u8					cmd_type;
	struct pmu_ap_cmd_common		cmn;
	struct pmu_ap_cmd_init			init;
	struct pmu_ap_cmd_init_and_enable_ctrl	init_and_enable_ctrl;
	struct pmu_ap_cmd_enable_ctrl		enable_ctrl;
	struct pmu_ap_cmd_disable_ctrl		disable_ctrl;
	struct pmu_ap_cmd_kick_ctrl		kick_ctrl;
};

/*
 * Structure for generic AP Message
 */
struct pmu_ap_msg_common {
	u8	msg_type;
	u16	msg_id;
};

/*
 * Structure for INIT_ACK Message
 */
struct pmu_ap_msg_init_ack {
	u8	msg_type;
	u16	msg_id;
	u8	ctrl_id;
	u32	stats_dmem_offset;
};

/*
 * Defines for AP messages
 */
enum {
	PMU_AP_MSG_ID_INIT_ACK = 0x0,
};

/*
 * AP Message
 */
union pmu_ap_msg {
	u8				msg_type;
	struct pmu_ap_msg_common	cmn;
	struct pmu_ap_msg_init_ack	init_ack;
};

/* Default Sampling Period of AELPG */
#define APCTRL_SAMPLING_PERIOD_PG_DEFAULT_US                    (1000000)

/* Default values of APCTRL parameters */
#define APCTRL_MINIMUM_IDLE_FILTER_DEFAULT_US                   (100)
#define APCTRL_MINIMUM_TARGET_SAVING_DEFAULT_US                 (10000)
#define APCTRL_POWER_BREAKEVEN_DEFAULT_US                       (2000)
#define APCTRL_CYCLES_PER_SAMPLE_MAX_DEFAULT                    (200)

/*
 * Disable reason for Adaptive Power Controller
 */
enum {
	APCTRL_DISABLE_REASON_RM_UNLOAD,
	APCTRL_DISABLE_REASON_RMCTRL,
};

/*
 * Adaptive Power Controller
 */
struct ap_ctrl {
	u32			stats_dmem_offset;
	u32			disable_reason_mask;
	struct pmu_ap_ctrl_stat	stat_cache;
	u8			b_ready;
};

/*
 * Adaptive Power structure
 *
 * ap structure provides generic infrastructure to make any power feature
 * adaptive.
 */
struct pmu_ap {
	u32			supported_mask;
	struct ap_ctrl		ap_ctrl[PMU_AP_CTRL_ID_MAX];
};
/*---------------------------------------------------------*/

/*perfmon task defines*/
enum pmu_perfmon_cmd_start_fields {
	COUNTER_ALLOC
};

enum {
	PMU_PERFMON_CMD_ID_START = 0,
	PMU_PERFMON_CMD_ID_STOP  = 1,
	PMU_PERFMON_CMD_ID_INIT  = 2
};

struct pmu_perfmon_cmd_start_v3 {
	u8 cmd_type;
	u8 group_id;
	u8 state_id;
	u8 flags;
	struct pmu_allocation_v3 counter_alloc;
};

struct pmu_perfmon_cmd_start_v2 {
	u8 cmd_type;
	u8 group_id;
	u8 state_id;
	u8 flags;
	struct pmu_allocation_v2 counter_alloc;
};

struct pmu_perfmon_cmd_start_v1 {
	u8 cmd_type;
	u8 group_id;
	u8 state_id;
	u8 flags;
	struct pmu_allocation_v1 counter_alloc;
};

struct pmu_perfmon_cmd_start_v0 {
	u8 cmd_type;
	u8 group_id;
	u8 state_id;
	u8 flags;
	struct pmu_allocation_v0 counter_alloc;
};

struct pmu_perfmon_cmd_stop {
	u8 cmd_type;
};

struct pmu_perfmon_cmd_init_v3 {
	u8 cmd_type;
	u8 to_decrease_count;
	u8 base_counter_id;
	u32 sample_period_us;
	struct pmu_allocation_v3 counter_alloc;
	u8 num_counters;
	u8 samples_in_moving_avg;
	u16 sample_buffer;
};

struct pmu_perfmon_cmd_init_v2 {
	u8 cmd_type;
	u8 to_decrease_count;
	u8 base_counter_id;
	u32 sample_period_us;
	struct pmu_allocation_v2 counter_alloc;
	u8 num_counters;
	u8 samples_in_moving_avg;
	u16 sample_buffer;
};

struct pmu_perfmon_cmd_init_v1 {
	u8 cmd_type;
	u8 to_decrease_count;
	u8 base_counter_id;
	u32 sample_period_us;
	struct pmu_allocation_v1 counter_alloc;
	u8 num_counters;
	u8 samples_in_moving_avg;
	u16 sample_buffer;
};

struct pmu_perfmon_cmd_init_v0 {
	u8 cmd_type;
	u8 to_decrease_count;
	u8 base_counter_id;
	u32 sample_period_us;
	struct pmu_allocation_v0 counter_alloc;
	u8 num_counters;
	u8 samples_in_moving_avg;
	u16 sample_buffer;
};

struct pmu_perfmon_cmd {
	union {
		u8 cmd_type;
		struct pmu_perfmon_cmd_start_v0 start_v0;
		struct pmu_perfmon_cmd_start_v1 start_v1;
		struct pmu_perfmon_cmd_start_v2 start_v2;
		struct pmu_perfmon_cmd_start_v3 start_v3;
		struct pmu_perfmon_cmd_stop stop;
		struct pmu_perfmon_cmd_init_v0 init_v0;
		struct pmu_perfmon_cmd_init_v1 init_v1;
		struct pmu_perfmon_cmd_init_v2 init_v2;
		struct pmu_perfmon_cmd_init_v3 init_v3;
	};
};

struct pmu_zbc_cmd {
	u8 cmd_type;
	u8 pad;
	u16 entry_mask;
};

/* PERFMON MSG */
enum {
	PMU_PERFMON_MSG_ID_INCREASE_EVENT = 0,
	PMU_PERFMON_MSG_ID_DECREASE_EVENT = 1,
	PMU_PERFMON_MSG_ID_INIT_EVENT     = 2,
	PMU_PERFMON_MSG_ID_ACK            = 3
};

struct pmu_perfmon_msg_generic {
	u8 msg_type;
	u8 state_id;
	u8 group_id;
	u8 data;
};

struct pmu_perfmon_msg {
	union {
		u8 msg_type;
		struct pmu_perfmon_msg_generic gen;
	};
};
/*---------------------------------------------------------*/
/*ELPG/PG defines*/
enum {
	PMU_PG_ELPG_MSG_INIT_ACK,
	PMU_PG_ELPG_MSG_DISALLOW_ACK,
	PMU_PG_ELPG_MSG_ALLOW_ACK,
	PMU_PG_ELPG_MSG_FREEZE_ACK,
	PMU_PG_ELPG_MSG_FREEZE_ABORT,
	PMU_PG_ELPG_MSG_UNFREEZE_ACK,
};

struct pmu_pg_msg_elpg_msg {
	u8 msg_type;
	u8 engine_id;
	u16 msg;
};

enum {
	PMU_PG_STAT_MSG_RESP_DMEM_OFFSET = 0,
};

struct pmu_pg_msg_stat {
	u8 msg_type;
	u8 engine_id;
	u16 sub_msg_id;
	u32 data;
};

enum {
	PMU_PG_MSG_ENG_BUF_LOADED,
	PMU_PG_MSG_ENG_BUF_UNLOADED,
	PMU_PG_MSG_ENG_BUF_FAILED,
};

struct pmu_pg_msg_eng_buf_stat {
	u8 msg_type;
	u8 engine_id;
	u8 buf_idx;
	u8 status;
};

struct pmu_pg_msg {
	union {
		u8 msg_type;
		struct pmu_pg_msg_elpg_msg elpg_msg;
		struct pmu_pg_msg_stat stat;
		struct pmu_pg_msg_eng_buf_stat eng_buf_stat;
		/* TBD: other pg messages */
		union pmu_ap_msg ap_msg;
		struct nv_pmu_rppg_msg rppg_msg;
	};
};

enum {
	PMU_PG_ELPG_CMD_INIT,
	PMU_PG_ELPG_CMD_DISALLOW,
	PMU_PG_ELPG_CMD_ALLOW,
	PMU_PG_ELPG_CMD_FREEZE,
	PMU_PG_ELPG_CMD_UNFREEZE,
};

enum {
	PMU_PG_CMD_ID_ELPG_CMD = 0,
	PMU_PG_CMD_ID_ENG_BUF_LOAD,
	PMU_PG_CMD_ID_ENG_BUF_UNLOAD,
	PMU_PG_CMD_ID_PG_STAT,
	PMU_PG_CMD_ID_PG_LOG_INIT,
	PMU_PG_CMD_ID_PG_LOG_FLUSH,
	PMU_PG_CMD_ID_PG_PARAM,
	PMU_PG_CMD_ID_ELPG_INIT,
	PMU_PG_CMD_ID_ELPG_POLL_CTXSAVE,
	PMU_PG_CMD_ID_ELPG_ABORT_POLL,
	PMU_PG_CMD_ID_ELPG_PWR_UP,
	PMU_PG_CMD_ID_ELPG_DISALLOW,
	PMU_PG_CMD_ID_ELPG_ALLOW,
	PMU_PG_CMD_ID_AP,
	RM_PMU_PG_CMD_ID_PSI,
	RM_PMU_PG_CMD_ID_CG,
	PMU_PG_CMD_ID_ZBC_TABLE_UPDATE,
	PMU_PG_CMD_ID_PWR_RAIL_GATE_DISABLE = 0x20,
	PMU_PG_CMD_ID_PWR_RAIL_GATE_ENABLE,
	PMU_PG_CMD_ID_PWR_RAIL_SMU_MSG_DISABLE,
	PMU_PMU_PG_CMD_ID_RPPG = 0x24,
};

struct pmu_pg_cmd_elpg_cmd {
	u8 cmd_type;
	u8 engine_id;
	u16 cmd;
};

struct pmu_pg_cmd_eng_buf_load_v0 {
	u8 cmd_type;
	u8 engine_id;
	u8 buf_idx;
	u8 pad;
	u16 buf_size;
	u32 dma_base;
	u8 dma_offset;
	u8 dma_idx;
};

struct pmu_pg_cmd_eng_buf_load_v1 {
	u8 cmd_type;
	u8 engine_id;
	u8 buf_idx;
	u8 pad;
	struct flcn_mem_desc {
		struct falc_u64 dma_addr;
		u16 dma_size;
		u8 dma_idx;
	} dma_desc;
};

struct pmu_pg_cmd_eng_buf_load_v2 {
	u8 cmd_type;
	u8 engine_id;
	u8 buf_idx;
	u8 pad;
	struct flcn_mem_desc_v0 dma_desc;
};

enum {
	PMU_PG_STAT_CMD_ALLOC_DMEM = 0,
};

#define PMU_PG_PARAM_CMD_GR_INIT_PARAM  0x0
#define PMU_PG_PARAM_CMD_MS_INIT_PARAM  0x01
#define PMU_PG_PARAM_CMD_MCLK_CHANGE  0x04
#define PMU_PG_PARAM_CMD_POST_INIT  0x06

#define PMU_PG_FEATURE_GR_SDIV_SLOWDOWN_ENABLED	(1 << 0)
#define PMU_PG_FEATURE_GR_POWER_GATING_ENABLED	(1 << 2)
#define PMU_PG_FEATURE_GR_RPPG_ENABLED		(1 << 3)

#define NVGPU_PMU_GR_FEATURE_MASK_RPPG	(1 << 3)
#define NVGPU_PMU_GR_FEATURE_MASK_ALL	\
	(	\
		NVGPU_PMU_GR_FEATURE_MASK_RPPG   \
	)

#define NVGPU_PMU_MS_FEATURE_MASK_CLOCK_GATING  (1 << 0)
#define NVGPU_PMU_MS_FEATURE_MASK_SW_ASR        (1 << 1)
#define NVGPU_PMU_MS_FEATURE_MASK_RPPG          (1 << 8)
#define NVGPU_PMU_MS_FEATURE_MASK_FB_TRAINING   (1 << 5)

#define NVGPU_PMU_MS_FEATURE_MASK_ALL	\
	(	\
		NVGPU_PMU_MS_FEATURE_MASK_CLOCK_GATING  |\
		NVGPU_PMU_MS_FEATURE_MASK_SW_ASR        |\
		NVGPU_PMU_MS_FEATURE_MASK_RPPG          |\
		NVGPU_PMU_MS_FEATURE_MASK_FB_TRAINING   \
	)

#define PG_REQUEST_TYPE_GLOBAL 0x0
#define PG_REQUEST_TYPE_PSTATE 0x1

struct pmu_pg_cmd_gr_init_param {
	u8 cmd_type;
	u16 sub_cmd_id;
	u8 featuremask;
};

struct pmu_pg_cmd_ms_init_param {
	u8 cmd_type;
	u16 cmd_id;
	u8 psi;
	u8 idle_flipped_test_enabled;
	u16 psiSettleTimeUs;
	u8 rsvd[2];
	u32 support_mask;
	u32 abort_timeout_us;
};

struct pmu_pg_cmd_mclk_change {
	u8 cmd_type;
	u16 cmd_id;
	u8 rsvd;
	u32 data;
};

#define PG_VOLT_RAIL_IDX_MAX 2

struct pmu_pg_volt_rail {
	u8    volt_rail_idx;
	u8    sleep_volt_dev_idx;
	u8    sleep_vfe_idx;
	u32   sleep_voltage_uv;
	u32   therm_vid0_cache;
	u32   therm_vid1_cache;
};

struct pmu_pg_cmd_post_init_param {
	u8 cmd_type;
	u16 cmd_id;
	struct pmu_pg_volt_rail pg_volt_rail[PG_VOLT_RAIL_IDX_MAX];
};

struct pmu_pg_cmd_stat {
	u8 cmd_type;
	u8 engine_id;
	u16 sub_cmd_id;
	u32 data;
};

struct pmu_pg_cmd {
	union {
		u8 cmd_type;
		struct pmu_pg_cmd_elpg_cmd elpg_cmd;
		struct pmu_pg_cmd_eng_buf_load_v0 eng_buf_load_v0;
		struct pmu_pg_cmd_eng_buf_load_v1 eng_buf_load_v1;
		struct pmu_pg_cmd_eng_buf_load_v2 eng_buf_load_v2;
		struct pmu_pg_cmd_stat stat;
		struct pmu_pg_cmd_gr_init_param gr_init_param;
		struct pmu_pg_cmd_ms_init_param ms_init_param;
		struct pmu_pg_cmd_mclk_change mclk_change;
		struct pmu_pg_cmd_post_init_param post_init;
		/* TBD: other pg commands */
		union pmu_ap_cmd ap_cmd;
		struct nv_pmu_rppg_cmd rppg_cmd;
	};
};

/*---------------------------------------------------------*/
/* ACR Commands/Message structures */

enum {
	PMU_ACR_CMD_ID_INIT_WPR_REGION = 0x0,
	PMU_ACR_CMD_ID_BOOTSTRAP_FALCON,
	PMU_ACR_CMD_ID_RESERVED,
	PMU_ACR_CMD_ID_BOOTSTRAP_MULTIPLE_FALCONS,
};

/*
 * Initializes the WPR region details
 */
struct pmu_acr_cmd_init_wpr_details {
	u8  cmd_type;
	u32 regionid;
	u32 wproffset;

};

/*
 * falcon ID to bootstrap
 */
struct pmu_acr_cmd_bootstrap_falcon {
	u8 cmd_type;
	u32 flags;
	u32 falconid;
};

/*
 * falcon ID to bootstrap
 */
struct pmu_acr_cmd_bootstrap_multiple_falcons {
	u8 cmd_type;
	u32 flags;
	u32 falconidmask;
	u32 usevamask;
	struct falc_u64 wprvirtualbase;
};

#define PMU_ACR_CMD_BOOTSTRAP_FALCON_FLAGS_RESET_NO  1
#define PMU_ACR_CMD_BOOTSTRAP_FALCON_FLAGS_RESET_YES 0


struct pmu_acr_cmd {
	union {
		u8 cmd_type;
		struct pmu_acr_cmd_bootstrap_falcon bootstrap_falcon;
		struct pmu_acr_cmd_init_wpr_details init_wpr;
		struct pmu_acr_cmd_bootstrap_multiple_falcons boot_falcons;
	};
};

/* acr messages */

/*
 * returns the WPR region init information
 */
#define PMU_ACR_MSG_ID_INIT_WPR_REGION   0

/*
 * Returns the Bootstrapped falcon ID to RM
 */
#define PMU_ACR_MSG_ID_BOOTSTRAP_FALCON  1

/*
 * Returns the WPR init status
 */
#define PMU_ACR_SUCCESS                  0
#define PMU_ACR_ERROR                    1

/*
 * PMU notifies about bootstrap status of falcon
 */
struct pmu_acr_msg_bootstrap_falcon {
	u8 msg_type;
	union {
		u32 errorcode;
		u32 falconid;
	};
};

struct pmu_acr_msg {
	union {
		u8 msg_type;
		struct pmu_acr_msg_bootstrap_falcon acrmsg;
	};
};
/*---------------------------------------------------------*/
/* FECS mem override command*/

#define PMU_LRF_TEX_LTC_DRAM_CMD_ID_EN_DIS   0

/*!
 * Enable/Disable FECS error feature
 */
struct pmu_cmd_lrf_tex_ltc_dram_en_dis {
	/*Command type must be first*/
	u8  cmd_type;
	/*unit bitmask*/
	u8  en_dis_mask;
};

struct pmu_lrf_tex_ltc_dram_cmd {
	union {
		u8 cmd_type;
		struct pmu_cmd_lrf_tex_ltc_dram_en_dis en_dis;
	};
};

/*  FECS mem override messages*/
#define PMU_LRF_TEX_LTC_DRAM_MSG_ID_EN_DIS    0

struct pmu_msg_lrf_tex_ltc_dram_en_dis {
	/*!
	 * Must be at start
	 */
	u8 msg_type;
	u8 en_fail_mask;
	u8 dis_fail_mask;
	u32 pmu_status;
};

struct pmu_lrf_tex_ltc_dram_msg {
	union {
		u8 msg_type;
		struct pmu_msg_lrf_tex_ltc_dram_en_dis en_dis;
	};
};

#endif /*__PMU_API_H__*/
