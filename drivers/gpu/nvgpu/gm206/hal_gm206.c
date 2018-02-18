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

#include <linux/types.h>
#include <linux/printk.h>

#include <linux/types.h>

#include "gk20a/gk20a.h"

#include "gm20b/mc_gm20b.h"
#include "gm20b/ltc_gm20b.h"
#include "gm20b/mm_gm20b.h"
#include "gm206/mm_gm206.h"
#include "ce_gm206.h"
#include "gm20b/fb_gm20b.h"
#include "gm20b/pmu_gm20b.h"
#include "gm20b/gr_gm20b.h"
#include "gm206/pmu_gm206.h"
#include "gm206/acr_gm206.h"
#include "gm20b/gr_ctx_gm20b.h"
#include "gm20b/gm20b_gating_reglist.h"
#include "gm20b/regops_gm20b.h"
#include "gm20b/cde_gm20b.h"
#include "gm20b/therm_gm20b.h"
#include "gm20b/clk_gm20b.h"
#include "gm20b/debug_gm20b.h"

#include "fifo_gm206.h"
#include "bios_gm206.h"
#include "gr_gm206.h"
#include "hw_proj_gm206.h"

static struct gpu_ops gm206_ops = {
	.clock_gating = {
		.slcg_bus_load_gating_prod =
			gm20b_slcg_bus_load_gating_prod,
		.slcg_ce2_load_gating_prod =
			gm20b_slcg_ce2_load_gating_prod,
		.slcg_chiplet_load_gating_prod =
			gm20b_slcg_chiplet_load_gating_prod,
		.slcg_ctxsw_firmware_load_gating_prod =
			gm20b_slcg_ctxsw_firmware_load_gating_prod,
		.slcg_fb_load_gating_prod =
			gm20b_slcg_fb_load_gating_prod,
		.slcg_fifo_load_gating_prod =
			gm20b_slcg_fifo_load_gating_prod,
		.slcg_gr_load_gating_prod =
			gr_gm20b_slcg_gr_load_gating_prod,
		.slcg_ltc_load_gating_prod =
			ltc_gm20b_slcg_ltc_load_gating_prod,
		.slcg_perf_load_gating_prod =
			gm20b_slcg_perf_load_gating_prod,
		.slcg_priring_load_gating_prod =
			gm20b_slcg_priring_load_gating_prod,
		.slcg_pmu_load_gating_prod =
			gm20b_slcg_pmu_load_gating_prod,
		.slcg_therm_load_gating_prod =
			gm20b_slcg_therm_load_gating_prod,
		.slcg_xbar_load_gating_prod =
			gm20b_slcg_xbar_load_gating_prod,
		.blcg_bus_load_gating_prod =
			gm20b_blcg_bus_load_gating_prod,
		.blcg_ctxsw_firmware_load_gating_prod =
			gm20b_blcg_ctxsw_firmware_load_gating_prod,
		.blcg_fb_load_gating_prod =
			gm20b_blcg_fb_load_gating_prod,
		.blcg_fifo_load_gating_prod =
			gm20b_blcg_fifo_load_gating_prod,
		.blcg_gr_load_gating_prod =
			gm20b_blcg_gr_load_gating_prod,
		.blcg_ltc_load_gating_prod =
			gm20b_blcg_ltc_load_gating_prod,
		.blcg_pwr_csb_load_gating_prod =
			gm20b_blcg_pwr_csb_load_gating_prod,
		.blcg_pmu_load_gating_prod =
			gm20b_blcg_pmu_load_gating_prod,
		.blcg_xbar_load_gating_prod =
			gm20b_blcg_xbar_load_gating_prod,
		.pg_gr_load_gating_prod =
			gr_gm20b_pg_gr_load_gating_prod,
	}
};

static int gm206_get_litter_value(struct gk20a *g,
		enum nvgpu_litter_value value)
{
	int ret = -EINVAL;

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
	case GPU_LIT_PPC_IN_GPC_STRIDE:
		ret = proj_ppc_in_gpc_stride_v();
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
	default:
		BUG();
		break;
	}

	return ret;
}

int gm206_init_hal(struct gk20a *g)
{
	struct gpu_ops *gops = &g->ops;
	struct nvgpu_gpu_characteristics *c = &g->gpu_characteristics;
	u32 ver = g->gpu_characteristics.arch + g->gpu_characteristics.impl;

	*gops = gm206_ops;

	gops->privsecurity = 1;
	gops->securegpccs = 1;
	gops->pmupstate = false;
	gm20b_init_mc(gops);
	gm20b_init_ltc(gops);
	gm206_init_gr(gops);
	gm20b_init_ltc(gops);
	gm20b_init_fb(gops);
	g->ops.fb.set_use_full_comp_tag_line = NULL;
	gm206_init_fifo(gops);
	gm206_init_ce(gops);
	gm20b_init_gr_ctx(gops);
	gm206_init_mm(gops);
	gm206_init_pmu_ops(gops);
	gm20b_init_clk_ops(gops);
	gm20b_init_regops(gops);
	gm20b_init_debug_ops(gops);
	gm20b_init_cde_ops(gops);
	gm20b_init_therm_ops(gops);
	gk20a_init_tsg_ops(gops);
	gm206_init_bios(gops);
	switch(ver){
	case GK20A_GPUID_GM206:
		gops->name = "gm206";
		break;
	case GK20A_GPUID_GM204:
		gops->name = "gm204";
		break;
	default:
		gk20a_err(g->dev, "no support for %x", ver);
		BUG();
	}
	gops->chip_init_gpu_characteristics = gk20a_init_gpu_characteristics;
	gops->get_litter_value = gm206_get_litter_value;
	gops->gr_ctx.use_dma_for_fw_bootstrap = true;

	c->twod_class = FERMI_TWOD_A;
	c->threed_class = MAXWELL_B;
	c->compute_class = MAXWELL_COMPUTE_B;
	c->gpfifo_class = MAXWELL_CHANNEL_GPFIFO_A;
	c->inline_to_memory_class = KEPLER_INLINE_TO_MEMORY_B;
	c->dma_copy_class = MAXWELL_DMA_COPY_A;

	return 0;
}
