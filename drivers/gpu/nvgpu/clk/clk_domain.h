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

#ifndef _CLKDOMAIN_H_
#define _CLKDOMAIN_H_

#include "ctrl/ctrlclk.h"
#include "ctrl/ctrlboardobj.h"
#include "pmuif/gpmuifclk.h"
#include "boardobj/boardobjgrp_e32.h"
#include "boardobj/boardobjgrpmask.h"

struct clk_domains;
struct clk_domain;

/*data and function definition to talk to driver*/
u32 clk_domain_sw_setup(struct gk20a *g);
u32 clk_domain_pmu_setup(struct gk20a *g);

typedef u32 clkproglink(struct gk20a *g, struct clk_pmupstate *pclk,
			struct clk_domain *pdomain);

typedef int clkvfsearch(struct gk20a *g, struct clk_pmupstate *pclk,
			struct clk_domain *pdomain, u16 *clkmhz,
			u32 *voltuv, u8 rail);

typedef int clkgetslaveclk(struct gk20a *g, struct clk_pmupstate *pclk,
			struct clk_domain *pdomain, u16 *clkmhz,
			u16 masterclkmhz);

typedef u32 clkgetfpoints(struct gk20a *g, struct clk_pmupstate *pclk,
			struct clk_domain *pdomain, u32 *pfpointscount,
			  u16 *pfreqpointsinmhz, u8 rail);

struct clk_domains {
	struct boardobjgrp_e32 super;
	u8 n_num_entries;
	u8 version;
	bool b_enforce_vf_monotonicity;
	bool b_enforce_vf_smoothening;
	u32 vbios_domains;
	struct boardobjgrpmask_e32 prog_domains_mask;
	struct boardobjgrpmask_e32 master_domains_mask;
	u16 cntr_sampling_periodms;
	struct ctrl_clk_clk_delta  deltas;

	struct clk_domain *ordered_noise_aware_list[CTRL_BOARDOBJ_MAX_BOARD_OBJECTS];

	struct clk_domain *ordered_noise_unaware_list[CTRL_BOARDOBJ_MAX_BOARD_OBJECTS];
};

struct clk_domain {
	struct boardobj super;
	u32 api_domain;
	u32 part_mask;
	u8 domain;
	u8 perf_domain_index;
	u8 perf_domain_grp_idx;
	u8 ratio_domain;
	u8 usage;
	clkproglink *clkdomainclkproglink;
	clkvfsearch *clkdomainclkvfsearch;
	clkgetfpoints *clkdomainclkgetfpoints;
};

struct clk_domain_3x {
	struct clk_domain super;
	bool b_noise_aware_capable;
};

struct clk_domain_3x_fixed {
	struct clk_domain_3x super;
	u16  freq_mhz;
};

struct clk_domain_3x_prog {
	struct clk_domain_3x super;
	u8  clk_prog_idx_first;
	u8  clk_prog_idx_last;
	u8 noise_unaware_ordering_index;
	u8 noise_aware_ordering_index;
	bool b_force_noise_unaware_ordering;
	int factory_offset_khz;
	short freq_delta_min_mhz;
	short freq_delta_max_mhz;
	struct ctrl_clk_clk_delta deltas;
};

struct clk_domain_3x_master {
	struct clk_domain_3x_prog super;
	u32 slave_idxs_mask;
};

struct clk_domain_3x_slave {
	struct clk_domain_3x_prog super;
	u8 master_idx;
	clkgetslaveclk *clkdomainclkgetslaveclk;
};

u32 clk_domain_clk_prog_link(struct gk20a *g, struct clk_pmupstate *pclk);

#define CLK_CLK_DOMAIN_GET(pclk, idx)                                   \
	((struct clk_domain *)BOARDOBJGRP_OBJ_GET_BY_IDX(		\
		&pclk->clk_domainobjs.super.super, (u8)(idx)))

#endif
