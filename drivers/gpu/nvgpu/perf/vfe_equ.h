/*
 * general perf structures & definitions
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
#ifndef _VFE_EQU_H_
#define _VFE_EQU_H_

#include "boardobj/boardobjgrp.h"
#include "perf/vfe_var.h"
#include "pmuif/gpmuifperf.h"
#include "pmuif/gpmuifperfvfe.h"

u32 vfe_equ_sw_setup(struct gk20a *g);
u32 vfe_equ_pmu_setup(struct gk20a *g);

#define VFE_EQU_GET(_pperf, _idx)                                              \
	((struct vfe_equ *)BOARDOBJGRP_OBJ_GET_BY_IDX(                         \
		&((_pperf)->vfe.equs.super.super), (_idx)))

#define VFE_EQU_IDX_IS_VALID(_pperf, _idx)                                     \
	boardobjgrp_idxisvalid(&((_pperf)->vfe.equs.super.super), (_idx))

#define VFE_EQU_OUTPUT_TYPE_IS_VALID(_pperf, _idx, _outputtype)                \
	(VFE_EQU_IDX_IS_VALID((_pperf), (_idx)) &&                             \
	((_outputtype) != CTRL_PERF_VFE_EQU_OUTPUT_TYPE_UNITLESS) &&           \
	((VFE_EQU_GET((_pperf), (_idx))->outputtype == (_outputtype)) ||       \
	(VFE_EQU_GET((_pperf), (_idx))->outputtype ==                          \
	CTRL_PERF_VFE_EQU_OUTPUT_TYPE_UNITLESS)))

struct vfe_equ {
	struct boardobj super;
	u8 var_idx;
	u8 equ_idx_next;
	u8 output_type;
	u32 out_range_min;
	u32 out_range_max;

	bool b_is_dynamic_valid;
	bool b_is_dynamic;
};

struct vfe_equs {
	struct boardobjgrp_e255 super;
};

struct vfe_equ_compare {
	struct vfe_equ super;
	u8 func_id;
	u8 equ_idx_true;
	u8 equ_idx_false;
	u32 criteria;
};

struct vfe_equ_minmax {
	struct vfe_equ super;
	bool b_max;
	u8 equ_idx0;
	u8 equ_idx1;
};

struct vfe_equ_quadratic {
	struct vfe_equ super;
	u32   coeffs[CTRL_PERF_VFE_EQU_QUADRATIC_COEFF_COUNT];
};

#endif
