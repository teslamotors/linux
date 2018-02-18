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

#ifndef _CLKPROG_H_
#define _CLKPROG_H_
#include "ctrl/ctrlclk.h"
#include "ctrl/ctrlboardobj.h"
#include "pmuif/gpmuifclk.h"
#include "boardobj/boardobjgrp_e32.h"
#include "boardobj/boardobjgrpmask.h"

u32 clk_prog_sw_setup(struct gk20a *g);
u32 clk_prog_pmu_setup(struct gk20a *g);
struct clk_prog_1x_master;

typedef u32 vf_flatten(struct gk20a *g, struct clk_pmupstate *pclk,
			struct clk_prog_1x_master *p1xmaster,
			u8 clk_domain_idx, u16 *pfreqmaxlastmhz);

typedef u32 vf_lookup(struct gk20a *g, struct clk_pmupstate *pclk,
			struct clk_prog_1x_master *p1xmaster,
			u8 *slave_clk_domain_idx, u16 *pclkmhz,
			u32 *pvoltuv, u8 rail);

typedef int get_slaveclk(struct gk20a *g, struct clk_pmupstate *pclk,
			struct clk_prog_1x_master *p1xmaster,
			u8 slave_clk_domain_idx, u16 *pclkmhz,
			u16 masterclkmhz);

typedef u32 get_fpoints(struct gk20a *g, struct clk_pmupstate *pclk,
			struct clk_prog_1x_master *p1xmaster,
			u32 *pfpointscount,
			u16 **ppfreqpointsinmhz, u8 rail);


struct clk_progs {
	struct boardobjgrp_e255 super;
	u8 slave_entry_count;
	u8 vf_entry_count;

};

struct clk_prog {
	struct boardobj super;
};

struct clk_prog_1x {
	struct clk_prog super;
	u8  source;
	u16 freq_max_mhz;
	union ctrl_clk_clk_prog_1x_source_data source_data;
};

struct clk_prog_1x_master {
	struct clk_prog_1x super;
	bool b_o_c_o_v_enabled;
	struct ctrl_clk_clk_prog_1x_master_vf_entry *p_vf_entries;
	struct ctrl_clk_clk_delta deltas;
	union ctrl_clk_clk_prog_1x_master_source_data source_data;
	vf_flatten *vfflatten;
	vf_lookup *vflookup;
	get_fpoints *getfpoints;
	get_slaveclk *getslaveclk;
};

struct clk_prog_1x_master_ratio {
	struct clk_prog_1x_master super;
	struct ctrl_clk_clk_prog_1x_master_ratio_slave_entry *p_slave_entries;
};

struct clk_prog_1x_master_table {
	struct clk_prog_1x_master super;
	struct ctrl_clk_clk_prog_1x_master_table_slave_entry *p_slave_entries;
};

#define CLK_CLK_PROG_GET(pclk, idx)                                            \
	((struct clk_prog *)BOARDOBJGRP_OBJ_GET_BY_IDX(			\
		&pclk->clk_progobjs.super.super, (u8)(idx)))

#endif
