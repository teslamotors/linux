/*
 * general clock structures & definitions
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
#ifndef _CLK_H_
#define _CLK_H_

#include "clk_vin.h"
#include "clk_fll.h"
#include "clk_domain.h"
#include "clk_prog.h"
#include "clk_vf_point.h"
#include "clk_mclk.h"
#include "clk_freq_controller.h"
#include "gk20a/gk20a.h"

#define NV_PERF_DOMAIN_4X_CLOCK_DOMAIN_SKIP 0x10
#define NV_PERF_DOMAIN_4X_CLOCK_DOMAIN_MASK 0x1F
#define NV_PERF_DOMAIN_4X_CLOCK_DOMAIN_SHIFT 0

/* clock related defines for GPUs supporting clock control from pmu*/
struct clk_pmupstate {
	struct avfsvinobjs avfs_vinobjs;
	struct avfsfllobjs avfs_fllobjs;
	struct clk_domains clk_domainobjs;
	struct clk_progs clk_progobjs;
	struct clk_vf_points clk_vf_pointobjs;
	struct clk_mclk_state clk_mclk;
	struct clk_freq_controllers clk_freq_controllers;
};

struct clockentry {
		u8 vbios_clk_domain;
		u8 clk_which;
		u8 perf_index;
		u32 api_clk_domain;
};

struct set_fll_clk {
		u32 voltuv;
		u16 gpc2clkmhz;
		u32 current_regime_id_gpc;
		u32 target_regime_id_gpc;
		u16 sys2clkmhz;
		u32 current_regime_id_sys;
		u32 target_regime_id_sys;
		u16 xbar2clkmhz;
		u32 current_regime_id_xbar;
		u32 target_regime_id_xbar;
};

#define NV_PERF_HEADER_4X_CLOCKS_DOMAINS_MAX_NUMCLKS         9

struct vbios_clock_domain {
	u8 clock_type;
	u8 num_domains;
	struct clockentry clock_entry[NV_PERF_HEADER_4X_CLOCKS_DOMAINS_MAX_NUMCLKS];
};

struct vbios_clocks_table_1x_hal_clock_entry {
	enum nv_pmu_clk_clkwhich domain;
	bool b_noise_aware_capable;
};

#define NV_PERF_HEADER_4X_CLOCKS_DOMAINS_4_GPC2CLK           0
#define NV_PERF_HEADER_4X_CLOCKS_DOMAINS_4_XBAR2CLK          1
#define NV_PERF_HEADER_4X_CLOCKS_DOMAINS_4_DRAMCLK           2
#define NV_PERF_HEADER_4X_CLOCKS_DOMAINS_4_SYS2CLK           3
#define NV_PERF_HEADER_4X_CLOCKS_DOMAINS_4_HUB2CLK           4
#define NV_PERF_HEADER_4X_CLOCKS_DOMAINS_4_MSDCLK            5
#define NV_PERF_HEADER_4X_CLOCKS_DOMAINS_4_PWRCLK            6
#define NV_PERF_HEADER_4X_CLOCKS_DOMAINS_4_DISPCLK           7
#define NV_PERF_HEADER_4X_CLOCKS_DOMAINS_4_NUMCLKS           8

#define PERF_CLK_MCLK           0
#define PERF_CLK_DISPCLK        1
#define PERF_CLK_GPC2CLK        2
#define PERF_CLK_HOSTCLK        3
#define PERF_CLK_LTC2CLK        4
#define PERF_CLK_SYS2CLK        5
#define PERF_CLK_HUB2CLK        6
#define PERF_CLK_LEGCLK         7
#define PERF_CLK_MSDCLK         8
#define PERF_CLK_XCLK           9
#define PERF_CLK_PWRCLK         10
#define PERF_CLK_XBAR2CLK       11
#define PERF_CLK_PCIEGENCLK     12
#define PERF_CLK_NUM            13

u32 clk_pmu_vin_load(struct gk20a *g);
u32 clk_domain_print_vf_table(struct gk20a *g, u32 clkapidomain);
u32 clk_domain_get_f_or_v(
	struct gk20a *g,
	u32 clkapidomain,
	u16 *pclkmhz,
	u32 *pvoltuv,
	u8 railidx
);
u32 clk_domain_get_f_points(
	struct gk20a *g,
	u32 clkapidomain,
	u32 *fpointscount,
	u16 *freqpointsinmhz
);
int clk_get_fll_clks(struct gk20a *g, struct set_fll_clk *fllclk);
int clk_set_fll_clks(struct gk20a *g, struct set_fll_clk *fllclk);
int clk_pmu_freq_controller_load(struct gk20a *g, bool bload);
#endif
