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

#ifndef _VFE_VAR_H_
#define _VFE_VAR_H_

#include "boardobj/boardobjgrp.h"
#include "pmuif/gpmuifperf.h"
#include "pmuif/gpmuifperfvfe.h"

u32 vfe_var_sw_setup(struct gk20a *g);
u32 vfe_var_pmu_setup(struct gk20a *g);

#define VFE_VAR_GET(_pperf, _idx)                                              \
	((struct vfe_var)BOARDOBJGRP_OBJ_GET_BY_IDX(                           \
	&((_pperf)->vfe.vars.super.super), (_idx)))

#define VFE_VAR_IDX_IS_VALID(_pperf, _idx)                                     \
	boardobjgrp_idxisvalid(&((_pperf)->vfe.vars.super.super), (_idx))

struct vfe_var {
	struct boardobj super;
	u32 out_range_min;
	u32 out_range_max;
	bool b_is_dynamic_valid;
	bool b_is_dynamic;
};

struct vfe_vars {
	struct boardobjgrp_e32 super;
	u8 polling_periodms;
};

struct vfe_var_derived {
	struct vfe_var super;
};

struct vfe_var_derived_product {
	struct vfe_var_derived super;
	u8 var_idx0;
	u8 var_idx1;
};

struct vfe_var_derived_sum {
	struct vfe_var_derived super;
	u8 var_idx0;
	u8 var_idx1;
};

struct vfe_var_single {
	struct vfe_var super;
	u8 override_type;
	u32 override_value;
};

struct vfe_var_single_frequency {
	struct vfe_var_single  super;
};

struct vfe_var_single_voltage {
	struct vfe_var_single super;
};

struct vfe_var_single_sensed {
	struct vfe_var_single super;
};

struct vfe_var_single_sensed_fuse {
	struct vfe_var_single_sensed super;
	struct nv_pmu_vfe_var_single_sensed_fuse_override_info	override_info;
	struct nv_pmu_vfe_var_single_sensed_fuse_vfield_info vfield_info;
	struct nv_pmu_vfe_var_single_sensed_fuse_ver_vfield_info vfield_ver_info;
	u32 fuse_value_integer;
	u32 fuse_value_hw_integer;
	u8 fuse_version;
	bool b_version_check_done;
};

struct vfe_var_single_sensed_temp {
	struct vfe_var_single_sensed super;
	u8 therm_channel_index;
	int temp_hysteresis_positive;
	int temp_hysteresis_negative;
	int temp_default;
};

#endif
