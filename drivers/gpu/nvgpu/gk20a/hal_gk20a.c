/*
 * drivers/video/tegra/host/gk20a/hal_gk20a.c
 *
 * GK20A Tegra HAL interface.
 *
 * Copyright (c) 2014-2016, NVIDIA CORPORATION.  All rights reserved.
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

#include "hal_gk20a.h"
#include "ltc_gk20a.h"
#include "fb_gk20a.h"
#include "gk20a.h"
#include "gk20a_gating_reglist.h"
#include "channel_gk20a.h"
#include "gr_ctx_gk20a.h"
#include "fecs_trace_gk20a.h"
#include "mm_gk20a.h"
#include "mc_gk20a.h"
#include "pmu_gk20a.h"
#include "clk_gk20a.h"
#include "regops_gk20a.h"
#include "therm_gk20a.h"
#include "hw_proj_gk20a.h"
#include "tsg_gk20a.h"

#define GK20A_FBPA_BASE        0x00110000
#define GK20A_FBPA_SHARED_BASE 0x0010F000

static struct gpu_ops gk20a_ops = {
	.clock_gating = {
		.slcg_gr_load_gating_prod =
			gr_gk20a_slcg_gr_load_gating_prod,
		.slcg_perf_load_gating_prod =
			gr_gk20a_slcg_perf_load_gating_prod,
		.slcg_ltc_load_gating_prod =
			ltc_gk20a_slcg_ltc_load_gating_prod,
		.blcg_gr_load_gating_prod =
			gr_gk20a_blcg_gr_load_gating_prod,
		.pg_gr_load_gating_prod =
			gr_gk20a_pg_gr_load_gating_prod,
		.slcg_therm_load_gating_prod =
			gr_gk20a_slcg_therm_load_gating_prod,
	},
};

static int gk20a_get_litter_value(struct gk20a *g,
		enum nvgpu_litter_value value)
{
	int ret = EINVAL;
	switch (value) {
	case GPU_LIT_NUM_GPCS:
		ret = proj_scal_litter_num_gpcs_v();
		break;
	case GPU_LIT_NUM_PES_PER_GPC:
		ret = proj_scal_litter_num_pes_per_gpc_v();
		break;
	case GPU_LIT_NUM_ZCULL_BANKS:
		ret = proj_scal_litter_num_zcull_banks_v();
		break;
	case GPU_LIT_NUM_TPC_PER_GPC:
		ret = proj_scal_litter_num_tpc_per_gpc_v();
		break;
	case GPU_LIT_NUM_FBPS:
		ret = proj_scal_litter_num_fbps_v();
		break;
	case GPU_LIT_GPC_BASE:
		ret = proj_gpc_base_v();
		break;
	case GPU_LIT_GPC_STRIDE:
		ret = proj_gpc_stride_v();
		break;
	case GPU_LIT_GPC_SHARED_BASE:
		ret = proj_gpc_shared_base_v();
		break;
	case GPU_LIT_TPC_IN_GPC_BASE:
		ret = proj_tpc_in_gpc_base_v();
		break;
	case GPU_LIT_TPC_IN_GPC_STRIDE:
		ret = proj_tpc_in_gpc_stride_v();
		break;
	case GPU_LIT_TPC_IN_GPC_SHARED_BASE:
		ret = proj_tpc_in_gpc_shared_base_v();
		break;
	case GPU_LIT_PPC_IN_GPC_BASE:
		ret = proj_ppc_in_gpc_base_v();
		break;
	case GPU_LIT_PPC_IN_GPC_STRIDE:
		ret = proj_ppc_in_gpc_stride_v();
		break;
	case GPU_LIT_PPC_IN_GPC_SHARED_BASE:
		ret = proj_ppc_in_gpc_shared_base_v();
		break;
	case GPU_LIT_ROP_BASE:
		ret = proj_rop_base_v();
		break;
	case GPU_LIT_ROP_STRIDE:
		ret = proj_rop_stride_v();
		break;
	case GPU_LIT_ROP_SHARED_BASE:
		ret = proj_rop_shared_base_v();
		break;
	case GPU_LIT_HOST_NUM_ENGINES:
		ret = proj_host_num_engines_v();
		break;
	case GPU_LIT_HOST_NUM_PBDMA:
		ret = proj_host_num_pbdma_v();
		break;
	case GPU_LIT_LTC_STRIDE:
		ret = proj_ltc_stride_v();
		break;
	case GPU_LIT_LTS_STRIDE:
		ret = proj_lts_stride_v();
		break;
	case GPU_LIT_NUM_FBPAS:
		ret = proj_scal_litter_num_fbpas_v();
		break;
	case GPU_LIT_FBPA_STRIDE:
		ret = proj_fbpa_stride_v();
		break;
	case GPU_LIT_FBPA_BASE:
		ret = GK20A_FBPA_BASE;
		break;
	case GPU_LIT_FBPA_SHARED_BASE:
		ret = GK20A_FBPA_SHARED_BASE;
		break;
	default:
		gk20a_err(dev_from_gk20a(g), "Missing definition %d", value);
		BUG();
		break;
	}

	return ret;
}

int gk20a_init_hal(struct gk20a *g)
{
	struct gpu_ops *gops = &g->ops;
	struct nvgpu_gpu_characteristics *c = &g->gpu_characteristics;

	*gops = gk20a_ops;
	gops->privsecurity = 0;
	gops->securegpccs = 0;
	gops->pmupstate = false;
	gk20a_init_mc(gops);
	gk20a_init_ltc(gops);
	gk20a_init_gr_ops(gops);
	gk20a_init_fecs_trace_ops(gops);
	gk20a_init_fb(gops);
	gk20a_init_fifo(gops);
	gk20a_init_ce2(gops);
	gk20a_init_gr_ctx(gops);
	gk20a_init_mm(gops);
	gk20a_init_pmu_ops(gops);
	gk20a_init_clk_ops(gops);
	gk20a_init_regops(gops);
	gk20a_init_debug_ops(gops);
	gk20a_init_therm_ops(gops);
	gk20a_init_tsg_ops(gops);
	gops->name = "gk20a";
	gops->chip_init_gpu_characteristics = gk20a_init_gpu_characteristics;
	gops->get_litter_value = gk20a_get_litter_value;
	gops->read_ptimer = gk20a_read_ptimer;

	c->twod_class = FERMI_TWOD_A;
	c->threed_class = KEPLER_C;
	c->compute_class = KEPLER_COMPUTE_A;
	c->gpfifo_class = KEPLER_CHANNEL_GPFIFO_C;
	c->inline_to_memory_class = KEPLER_INLINE_TO_MEMORY_A;
	c->dma_copy_class = KEPLER_DMA_COPY_A;

	return 0;
}
