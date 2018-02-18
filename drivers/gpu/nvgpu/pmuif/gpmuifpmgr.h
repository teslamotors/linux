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

#ifndef _GPMUIFPMGR_H_
#define _GPMUIFPMGR_H_

#include "gk20a/gk20a.h"
#include "gk20a/pmu_gk20a.h"
#include "ctrl/ctrlpmgr.h"
#include "pmuif/gpmuifboardobj.h"
#include "gk20a/pmu_common.h"

struct nv_pmu_pmgr_i2c_device_desc {
	struct nv_pmu_boardobj super;
	u8 dcb_index;
	u16 i2c_address;
	u32 i2c_flags;
	u8 i2c_port;
};

#define NV_PMU_PMGR_I2C_DEVICE_DESC_TABLE_MAX_DEVICES                      (32)

struct nv_pmu_pmgr_i2c_device_desc_table {
	u32 dev_mask;
	struct nv_pmu_pmgr_i2c_device_desc
	devices[NV_PMU_PMGR_I2C_DEVICE_DESC_TABLE_MAX_DEVICES];
};

struct nv_pmu_pmgr_pwr_device_desc {
	struct nv_pmu_boardobj super;
	u32 power_corr_factor;
};

#define NV_PMU_PMGR_PWR_DEVICE_INA3221_CH_NUM                              0x03

struct nv_pmu_pmgr_pwr_device_desc_ina3221 {
	struct nv_pmu_pmgr_pwr_device_desc super;
	u8 i2c_dev_idx;
	struct ctrl_pmgr_pwr_device_info_rshunt
		r_shuntm_ohm[NV_PMU_PMGR_PWR_DEVICE_INA3221_CH_NUM];
	u16 configuration;
	u16 mask_enable;
	u32 event_mask;
	u16 curr_correct_m;
	s16 curr_correct_b;
};

union nv_pmu_pmgr_pwr_device_desc_union {
	struct nv_pmu_boardobj board_obj;
	struct nv_pmu_pmgr_pwr_device_desc pwr_dev;
	struct nv_pmu_pmgr_pwr_device_desc_ina3221 ina3221;
};

struct nv_pmu_pmgr_pwr_device_ba_info {
	bool b_initialized_and_used;
};

struct nv_pmu_pmgr_pwr_device_desc_table_header {
	struct nv_pmu_boardobjgrp_e32 super;
	struct nv_pmu_pmgr_pwr_device_ba_info ba_info;
};

NV_PMU_MAKE_ALIGNED_STRUCT(nv_pmu_pmgr_pwr_device_desc_table_header,
		sizeof(struct nv_pmu_pmgr_pwr_device_desc_table_header));
NV_PMU_MAKE_ALIGNED_UNION(nv_pmu_pmgr_pwr_device_desc_union,
		sizeof(union nv_pmu_pmgr_pwr_device_desc_union));

struct nv_pmu_pmgr_pwr_device_desc_table {
	union nv_pmu_pmgr_pwr_device_desc_table_header_aligned hdr;
	union nv_pmu_pmgr_pwr_device_desc_union_aligned
	devices[CTRL_PMGR_PWR_DEVICES_MAX_DEVICES];
};

union nv_pmu_pmgr_pwr_device_dmem_size {
	union nv_pmu_pmgr_pwr_device_desc_table_header_aligned pwr_device_hdr;
	union nv_pmu_pmgr_pwr_device_desc_union_aligned pwr_device;
};

struct nv_pmu_pmgr_pwr_channel {
	struct nv_pmu_boardobj super;
	u8 pwr_rail;
	u8 ch_idx;
	u32 volt_fixedu_v;
	u32 pwr_corr_slope;
	s32 pwr_corr_offsetm_w;
	u32 curr_corr_slope;
	s32 curr_corr_offsetm_a;
	u32 dependent_ch_mask;
};

#define NV_PMU_PMGR_PWR_CHANNEL_MAX_CHANNELS                                  16

#define NV_PMU_PMGR_PWR_CHANNEL_MAX_CHRELATIONSHIPS                           16

struct nv_pmu_pmgr_pwr_channel_sensor {
	struct nv_pmu_pmgr_pwr_channel super;
	u8 pwr_dev_idx;
	u8 pwr_dev_prov_idx;
};

struct nv_pmu_pmgr_pwr_channel_pmu_compactible {
	u8 pmu_compactible_data[56];
};

union nv_pmu_pmgr_pwr_channel_union {
	struct nv_pmu_boardobj board_obj;
	struct nv_pmu_pmgr_pwr_channel pwr_channel;
	struct nv_pmu_pmgr_pwr_channel_sensor sensor;
	struct nv_pmu_pmgr_pwr_channel_pmu_compactible pmu_pwr_channel;
};

#define NV_PMU_PMGR_PWR_MONITOR_TYPE_NO_POLLING                             0x02

struct nv_pmu_pmgr_pwr_monitor_pstate {
	u32 hw_channel_mask;
};

union nv_pmu_pmgr_pwr_monitor_type_specific {
	struct nv_pmu_pmgr_pwr_monitor_pstate pstate;
};

struct nv_pmu_pmgr_pwr_chrelationship_pmu_compactible {
	u8 pmu_compactible_data[28];
};

union nv_pmu_pmgr_pwr_chrelationship_union {
	struct nv_pmu_boardobj board_obj;
	struct nv_pmu_pmgr_pwr_chrelationship_pmu_compactible pmu_pwr_chrelationship;
};

struct nv_pmu_pmgr_pwr_channel_header {
	struct nv_pmu_boardobjgrp_e32 super;
	u8 type;
	union nv_pmu_pmgr_pwr_monitor_type_specific type_specific;
	u8 sample_count;
	u16 sampling_periodms;
	u16 sampling_period_low_powerms;
	u32 total_gpu_power_channel_mask;
	u32 physical_channel_mask;
};

struct nv_pmu_pmgr_pwr_chrelationship_header {
	struct nv_pmu_boardobjgrp_e32 super;
};

NV_PMU_MAKE_ALIGNED_STRUCT(nv_pmu_pmgr_pwr_channel_header,
		sizeof(struct nv_pmu_pmgr_pwr_channel_header));
NV_PMU_MAKE_ALIGNED_STRUCT(nv_pmu_pmgr_pwr_chrelationship_header,
		sizeof(struct nv_pmu_pmgr_pwr_chrelationship_header));
NV_PMU_MAKE_ALIGNED_UNION(nv_pmu_pmgr_pwr_chrelationship_union,
		sizeof(union nv_pmu_pmgr_pwr_chrelationship_union));
NV_PMU_MAKE_ALIGNED_UNION(nv_pmu_pmgr_pwr_channel_union,
		sizeof(union nv_pmu_pmgr_pwr_channel_union));

struct nv_pmu_pmgr_pwr_channel_desc {
	union nv_pmu_pmgr_pwr_channel_header_aligned hdr;
	union nv_pmu_pmgr_pwr_channel_union_aligned
	channels[NV_PMU_PMGR_PWR_CHANNEL_MAX_CHANNELS];
};

struct nv_pmu_pmgr_pwr_chrelationship_desc {
	union nv_pmu_pmgr_pwr_chrelationship_header_aligned hdr;
	union nv_pmu_pmgr_pwr_chrelationship_union_aligned
	ch_rels[NV_PMU_PMGR_PWR_CHANNEL_MAX_CHRELATIONSHIPS];
};

union nv_pmu_pmgr_pwr_monitor_dmem_size {
	union nv_pmu_pmgr_pwr_channel_header_aligned channel_hdr;
	union nv_pmu_pmgr_pwr_channel_union_aligned channel;
	union nv_pmu_pmgr_pwr_chrelationship_header_aligned ch_rels_hdr;
	union nv_pmu_pmgr_pwr_chrelationship_union_aligned ch_rels;
};

struct nv_pmu_pmgr_pwr_monitor_pack {
	struct nv_pmu_pmgr_pwr_channel_desc channels;
	struct nv_pmu_pmgr_pwr_chrelationship_desc ch_rels;
};

#define NV_PMU_PMGR_PWR_POLICY_MAX_POLICIES                                   32

#define NV_PMU_PMGR_PWR_POLICY_MAX_POLICY_RELATIONSHIPS                       32

struct nv_pmu_pmgr_pwr_policy {
	struct nv_pmu_boardobj super;
	u8 ch_idx;
	u8 num_limit_inputs;
	u8 limit_unit;
	u8 sample_mult;
	u32 limit_curr;
	u32 limit_min;
	u32 limit_max;
	struct ctrl_pmgr_pwr_policy_info_integral integral;
	enum ctrl_pmgr_pwr_policy_filter_type filter_type;
	union ctrl_pmgr_pwr_policy_filter_param filter_param;
};

struct nv_pmu_pmgr_pwr_policy_hw_threshold {
	struct nv_pmu_pmgr_pwr_policy super;
	u8 threshold_idx;
	u8 low_threshold_idx;
	bool b_use_low_threshold;
	u16 low_threshold_value;
};

struct nv_pmu_pmgr_pwr_policy_sw_threshold {
	struct nv_pmu_pmgr_pwr_policy super;
	u8 threshold_idx;
	u8 low_threshold_idx;
	bool b_use_low_threshold;
	u16 low_threshold_value;
	u8 event_id;
};

struct nv_pmu_pmgr_pwr_policy_pmu_compactible {
	u8 pmu_compactible_data[68];
};

union nv_pmu_pmgr_pwr_policy_union {
	struct nv_pmu_boardobj board_obj;
	struct nv_pmu_pmgr_pwr_policy pwr_policy;
	struct nv_pmu_pmgr_pwr_policy_hw_threshold hw_threshold;
	struct nv_pmu_pmgr_pwr_policy_sw_threshold sw_threshold;
	struct nv_pmu_pmgr_pwr_policy_pmu_compactible pmu_pwr_policy;
};

struct nv_pmu_pmgr_pwr_policy_relationship_pmu_compactible {
	u8 pmu_compactible_data[24];
};

union nv_pmu_pmgr_pwr_policy_relationship_union {
	struct nv_pmu_boardobj board_obj;
	struct nv_pmu_pmgr_pwr_policy_relationship_pmu_compactible pmu_pwr_relationship;
};

struct nv_pmu_pmgr_pwr_violation_pmu_compactible {
	u8 pmu_compactible_data[16];
};

union nv_pmu_pmgr_pwr_violation_union {
	struct nv_pmu_boardobj board_obj;
	struct nv_pmu_pmgr_pwr_violation_pmu_compactible violation;
};

#define NV_PMU_PMGR_PWR_POLICY_DESC_TABLE_VERSION_3X                    0x30

NV_PMU_MAKE_ALIGNED_UNION(nv_pmu_pmgr_pwr_policy_union,
		sizeof(union nv_pmu_pmgr_pwr_policy_union));
NV_PMU_MAKE_ALIGNED_UNION(nv_pmu_pmgr_pwr_policy_relationship_union,
		sizeof(union nv_pmu_pmgr_pwr_policy_relationship_union));

#define NV_PMU_PERF_DOMAIN_GROUP_MAX_GROUPS 2

struct nv_pmu_perf_domain_group_limits
{
	u32 values[NV_PMU_PERF_DOMAIN_GROUP_MAX_GROUPS];
} ;

#define NV_PMU_PMGR_RESERVED_PWR_POLICY_MASK_COUNT                    0x6

struct nv_pmu_pmgr_pwr_policy_desc_header {
	struct nv_pmu_boardobjgrp_e32 super;
	u8 version;
	bool b_enabled;
	u8 low_sampling_mult;
	u8 semantic_policy_tbl[CTRL_PMGR_PWR_POLICY_IDX_NUM_INDEXES];
	u16 base_sample_period;
	u16 min_client_sample_period;
	u32 reserved_pmu_policy_mask[NV_PMU_PMGR_RESERVED_PWR_POLICY_MASK_COUNT];
	struct nv_pmu_perf_domain_group_limits global_ceiling;
};

NV_PMU_MAKE_ALIGNED_STRUCT(nv_pmu_pmgr_pwr_policy_desc_header ,
		sizeof(struct nv_pmu_pmgr_pwr_policy_desc_header ));

struct nv_pmu_pmgr_pwr_policyrel_desc_header {
	struct nv_pmu_boardobjgrp_e32 super;
};

NV_PMU_MAKE_ALIGNED_STRUCT(nv_pmu_pmgr_pwr_policyrel_desc_header,
		sizeof(struct nv_pmu_pmgr_pwr_policyrel_desc_header));

struct nv_pmu_pmgr_pwr_violation_desc_header {
	struct nv_pmu_boardobjgrp_e32 super;
};

NV_PMU_MAKE_ALIGNED_STRUCT(nv_pmu_pmgr_pwr_violation_desc_header,
		sizeof(struct nv_pmu_pmgr_pwr_violation_desc_header));
NV_PMU_MAKE_ALIGNED_UNION(nv_pmu_pmgr_pwr_violation_union,
		sizeof(union nv_pmu_pmgr_pwr_violation_union));

struct nv_pmu_pmgr_pwr_policy_desc {
	union nv_pmu_pmgr_pwr_policy_desc_header_aligned hdr;
	union nv_pmu_pmgr_pwr_policy_union_aligned
	policies[NV_PMU_PMGR_PWR_POLICY_MAX_POLICIES];
};

struct nv_pmu_pmgr_pwr_policyrel_desc {
	union nv_pmu_pmgr_pwr_policyrel_desc_header_aligned hdr;
	union nv_pmu_pmgr_pwr_policy_relationship_union_aligned
	policy_rels[NV_PMU_PMGR_PWR_POLICY_MAX_POLICY_RELATIONSHIPS];
};

struct nv_pmu_pmgr_pwr_violation_desc {
	union nv_pmu_pmgr_pwr_violation_desc_header_aligned hdr;
	union nv_pmu_pmgr_pwr_violation_union_aligned
	violations[CTRL_PMGR_PWR_VIOLATION_MAX];
};

union nv_pmu_pmgr_pwr_policy_dmem_size {
	union nv_pmu_pmgr_pwr_policy_desc_header_aligned policy_hdr;
	union nv_pmu_pmgr_pwr_policy_union_aligned policy;
	union nv_pmu_pmgr_pwr_policyrel_desc_header_aligned policy_rels_hdr;
	union nv_pmu_pmgr_pwr_policy_relationship_union_aligned policy_rels;
	union nv_pmu_pmgr_pwr_violation_desc_header_aligned violation_hdr;
	union nv_pmu_pmgr_pwr_violation_union_aligned violation;
};

struct nv_pmu_pmgr_pwr_policy_pack {
	struct nv_pmu_pmgr_pwr_policy_desc policies;
	struct nv_pmu_pmgr_pwr_policyrel_desc policy_rels;
	struct nv_pmu_pmgr_pwr_violation_desc violations;
};

#define NV_PMU_PMGR_CMD_ID_SET_OBJECT                              (0x00000000)

#define NV_PMU_PMGR_MSG_ID_QUERY                                   (0x00000002)

#define NV_PMU_PMGR_CMD_ID_PWR_DEVICES_QUERY                       (0x00000001)

#define NV_PMU_PMGR_CMD_ID_LOAD                                    (0x00000006)

#define NV_PMU_PMGR_CMD_ID_UNLOAD                                  (0x00000007)

struct nv_pmu_pmgr_cmd_set_object {
	u8 cmd_type;
	u8 pad[2];
	u8 object_type;
	struct nv_pmu_allocation object;
};

#define NV_PMU_PMGR_SET_OBJECT_ALLOC_OFFSET                              (0x04)

#define NV_PMU_PMGR_OBJECT_I2C_DEVICE_DESC_TABLE                   (0x00000000)

#define NV_PMU_PMGR_OBJECT_PWR_DEVICE_DESC_TABLE                   (0x00000001)

#define NV_PMU_PMGR_OBJECT_PWR_MONITOR                             (0x00000002)

#define NV_PMU_PMGR_OBJECT_PWR_POLICY                              (0x00000005)

struct nv_pmu_pmgr_pwr_devices_query_payload {
	struct {
		u32 powerm_w;
		u32 voltageu_v;
		u32 currentm_a;
	} devices[CTRL_PMGR_PWR_DEVICES_MAX_DEVICES];
};

struct nv_pmu_pmgr_cmd_pwr_devices_query {
	u8 cmd_type;
	u8 pad[3];
	u32 dev_mask;
	struct nv_pmu_allocation payload;
};

#define NV_PMU_PMGR_PWR_DEVICES_QUERY_ALLOC_OFFSET                        (0x08)

struct nv_pmu_pmgr_cmd_load {
	u8 cmd_type;
};

struct nv_pmu_pmgr_cmd_unload {
	u8 cmd_type;
};

struct nv_pmu_pmgr_cmd {
	union {
		u8 cmd_type;
		struct nv_pmu_pmgr_cmd_set_object set_object;
		struct nv_pmu_pmgr_cmd_pwr_devices_query pwr_dev_query;
		struct nv_pmu_pmgr_cmd_load load;
		struct nv_pmu_pmgr_cmd_unload unload;
	};
};

#define NV_PMU_PMGR_MSG_ID_SET_OBJECT                              (0x00000000)

#define NV_PMU_PMGR_MSG_ID_LOAD                                    (0x00000004)

#define NV_PMU_PMGR_MSG_ID_UNLOAD                                  (0x00000005)

struct nv_pmu_pmgr_msg_set_object {
	u8 msg_type;
	bool b_success;
	flcn_status flcnstatus;
	u8 object_type;
};

struct nv_pmu_pmgr_msg_query {
	u8 msg_type;
	bool b_success;
	flcn_status flcnstatus;
	u8 cmd_type;
};

struct nv_pmu_pmgr_msg_load {
	u8 msg_type;
	bool b_success;
	flcn_status flcnstatus;
};

struct nv_pmu_pmgr_msg_unload {
	u8 msg_type;
};

struct nv_pmu_pmgr_msg {
	union {
		u8 msg_type;
		struct nv_pmu_pmgr_msg_set_object set_object;
		struct nv_pmu_pmgr_msg_query query;
		struct nv_pmu_pmgr_msg_load load;
		struct nv_pmu_pmgr_msg_unload unload;
	};
};

#endif
