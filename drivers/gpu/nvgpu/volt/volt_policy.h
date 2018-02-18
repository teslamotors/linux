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

#ifndef _VOLT_POLICY_H_
#define _VOLT_POLICY_H_

#define VOLT_POLICY_INDEX_IS_VALID(pvolt, policy_idx)	\
		(boardobjgrp_idxisvalid(	\
		&((pvolt)->volt_policy_metadata.volt_policies.super), \
		(policy_idx)))

/*!
 * extends boardobj providing attributes common to all voltage_policies.
 */
struct voltage_policy {
	struct boardobj super;
};

struct voltage_policy_metadata {
	u8 perf_core_vf_seq_policy_idx;
	struct boardobjgrp_e32 volt_policies;
};

/*!
 * extends voltage_policy providing attributes
 * common to all voltage_policy_split_rail.
 */
struct voltage_policy_split_rail {
	struct voltage_policy super;
	u8 rail_idx_master;
	u8 rail_idx_slave;
	u8 delta_min_vfe_equ_idx;
	u8 delta_max_vfe_equ_idx;
	s32 offset_delta_min_uv;
	s32 offset_delta_max_uv;
};

struct voltage_policy_split_rail_single_step {
	struct voltage_policy_split_rail super;
};

struct voltage_policy_split_rail_multi_step {
	struct voltage_policy_split_rail super;
	u16 inter_switch_delay_us;
};

struct voltage_policy_single_rail {
	struct voltage_policy  super;
	u8 rail_idx;
};

u32 volt_policy_sw_setup(struct gk20a *g);
u32 volt_policy_pmu_setup(struct gk20a *g);
#endif
