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
#ifndef _PERF_H_
#define _PERF_H_

#include "vfe_equ.h"
#include "vfe_var.h"
#include "pstate/pstate.h"
#include "gk20a/gk20a.h"
#include "volt/volt.h"
#include "lpwr/lpwr.h"

#define CTRL_PERF_VFE_VAR_TYPE_INVALID                               0x00
#define CTRL_PERF_VFE_VAR_TYPE_DERIVED                               0x01
#define CTRL_PERF_VFE_VAR_TYPE_DERIVED_PRODUCT                       0x02
#define CTRL_PERF_VFE_VAR_TYPE_DERIVED_SUM                           0x03
#define CTRL_PERF_VFE_VAR_TYPE_SINGLE                                0x04
#define CTRL_PERF_VFE_VAR_TYPE_SINGLE_FREQUENCY                      0x05
#define CTRL_PERF_VFE_VAR_TYPE_SINGLE_SENSED                         0x06
#define CTRL_PERF_VFE_VAR_TYPE_SINGLE_SENSED_FUSE                    0x07
#define CTRL_PERF_VFE_VAR_TYPE_SINGLE_SENSED_TEMP                    0x08
#define CTRL_PERF_VFE_VAR_TYPE_SINGLE_VOLTAGE                        0x09

#define CTRL_PERF_VFE_VAR_SINGLE_OVERRIDE_TYPE_NONE                  0x00
#define CTRL_PERF_VFE_VAR_SINGLE_OVERRIDE_TYPE_VALUE                 0x01
#define CTRL_PERF_VFE_VAR_SINGLE_OVERRIDE_TYPE_OFFSET                0x02
#define CTRL_PERF_VFE_VAR_SINGLE_OVERRIDE_TYPE_SCALE                 0x03

#define CTRL_PERF_VFE_EQU_TYPE_INVALID                               0x00
#define CTRL_PERF_VFE_EQU_TYPE_COMPARE                               0x01
#define CTRL_PERF_VFE_EQU_TYPE_MINMAX                                0x02
#define CTRL_PERF_VFE_EQU_TYPE_QUADRATIC                             0x03

#define CTRL_PERF_VFE_EQU_OUTPUT_TYPE_UNITLESS                       0x00
#define CTRL_PERF_VFE_EQU_OUTPUT_TYPE_FREQ_MHZ                       0x01
#define CTRL_PERF_VFE_EQU_OUTPUT_TYPE_VOLT_UV                        0x02
#define CTRL_PERF_VFE_EQU_OUTPUT_TYPE_VF_GAIN                        0x03
#define CTRL_PERF_VFE_EQU_OUTPUT_TYPE_VOLT_DELTA_UV                  0x04

#define CTRL_PERF_VFE_EQU_QUADRATIC_COEFF_COUNT                      0x03

#define CTRL_PERF_VFE_EQU_COMPARE_FUNCTION_EQUAL                     0x00
#define CTRL_PERF_VFE_EQU_COMPARE_FUNCTION_GREATER_EQ                0x01
#define CTRL_PERF_VFE_EQU_COMPARE_FUNCTION_GREATER                   0x02

struct perf_pmupstate {
	struct vfe_vars vfe_varobjs;
	struct vfe_equs vfe_equobjs;
	struct pstates pstatesobjs;
	struct obj_volt volt;
	struct obj_lwpr lpwr;
};

u32 perf_pmu_vfe_load(struct gk20a *g);

#endif
