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

#ifndef _CLKVFPOINT_H_
#define _CLKVFPOINT_H_
#include "ctrl/ctrlclk.h"
#include "ctrl/ctrlboardobj.h"
#include "pmuif/gpmuifclk.h"
#include "boardobj/boardobjgrp_e32.h"
#include "boardobj/boardobjgrpmask.h"

u32 clk_vf_point_sw_setup(struct gk20a *g);
u32 clk_vf_point_pmu_setup(struct gk20a *g);
u32 clk_vf_point_cache(struct gk20a *g);

struct clk_vf_points {
	struct boardobjgrp_e255 super;
};

struct clk_vf_point {
	struct boardobj super;
	u8  vfe_equ_idx;
	u8  volt_rail_idx;
	struct ctrl_clk_vf_pair pair;
};

struct clk_vf_point_volt {
	struct clk_vf_point super;
	u32 source_voltage_uv;
	int freq_delta_khz;
};

struct clk_vf_point_freq {
	struct clk_vf_point super;
	int volt_delta_uv;
};

#define CLK_CLK_VF_POINT_GET(pclk, idx)                                        \
	((struct clk_vf_point *)BOARDOBJGRP_OBJ_GET_BY_IDX(                    \
		&pclk->clk_vf_pointobjs.super.super, (u8)(idx)))

#define clkvfpointpairget(pvfpoint)                                            \
	(&((pvfpoint)->pair))

#define clkvfpointfreqmhzget(pgpu, pvfpoint)                                   \
	CTRL_CLK_VF_PAIR_FREQ_MHZ_GET(clkvfpointpairget(pvfpoint))

#define clkvfpointfreqdeltamhzGet(pgpu, pvfPoint)                              \
	((BOARDOBJ_GET_TYPE(pvfpoint) == CTRL_CLK_CLK_VF_POINT_TYPE_VOLT) ?    \
	(((struct clk_vf_point_volt *)(pvfpoint))->freq_delta_khz / 1000) : 0)

#define clkvfpointfreqmhzset(pgpu, pvfpoint, _freqmhz)                         \
	CTRL_CLK_VF_PAIR_FREQ_MHZ_SET(clkvfpointpairget(pvfpoint), _freqmhz)

#define clkvfpointvoltageuvset(pgpu, pvfpoint, _voltageuv)                     \
	CTRL_CLK_VF_PAIR_VOLTAGE_UV_SET(clkvfpointpairget(pvfpoint),           \
	_voltageuv)

#define clkvfpointvoltageuvget(pgpu, pvfpoint)                          \
	CTRL_CLK_VF_PAIR_VOLTAGE_UV_GET(clkvfpointpairget(pvfpoint))	\

struct clk_vf_point *construct_clk_vf_point(struct gk20a *g, void *pargs);

#endif
