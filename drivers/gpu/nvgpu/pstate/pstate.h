/*
 * general p state infrastructure
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
#ifndef __PSTATE_H__
#define __PSTATE_H__

#include "gk20a/gk20a.h"
#include "clk/clk.h"

#define CTRL_PERF_PSTATE_TYPE_3X	0x3

#define CTRL_PERF_PSTATE_P0		0
#define CTRL_PERF_PSTATE_P5		5
#define CTRL_PERF_PSTATE_P8		8

#define CLK_SET_INFO_MAX_SIZE		(32)

struct clk_set_info {
	enum nv_pmu_clk_clkwhich clkwhich;
	u32 nominal_mhz;
	u32 min_mhz;
	u32 max_mhz;
};

struct clk_set_info_list {
	u32 num_info;
	struct clk_set_info clksetinfo[CLK_SET_INFO_MAX_SIZE];
};

struct pstate {
	struct boardobj super;
	u32 num;
	u8 lpwr_entry_idx;
	struct clk_set_info_list clklist;
};

struct pstates {
	struct boardobjgrp_e32 super;
	u32  num_levels;
	wait_queue_head_t pstate_notifier_wq;
	u32 is_pstate_switch_on;
	struct mutex pstate_mutex; /* protect is_pstate_switch_on */
};

int gk20a_init_pstate_support(struct gk20a *g);
int gk20a_init_pstate_pmu_support(struct gk20a *g);

struct clk_set_info *pstate_get_clk_set_info(struct gk20a *g, u32 pstate_num,
		enum nv_pmu_clk_clkwhich clkwhich);
struct pstate *pstate_find(struct gk20a *g, u32 num);

#endif /* __PSTATE_H__ */
