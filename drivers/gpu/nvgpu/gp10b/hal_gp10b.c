/*
 * GP10B Tegra HAL interface
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

#include <linux/types.h>
#include <linux/printk.h>

#include <linux/types.h>

#include "gk20a/gk20a.h"

#include "gp10b/gr_gp10b.h"
#include "gp10b/fecs_trace_gp10b.h"
#include "gp10b/mc_gp10b.h"
#include "gp10b/ltc_gp10b.h"
#include "gp10b/mm_gp10b.h"
#include "gp10b/ce_gp10b.h"
#include "gp10b/fb_gp10b.h"
#include "gp10b/pmu_gp10b.h"
#include "gp10b/gr_ctx_gp10b.h"
#include "gp10b/fifo_gp10b.h"
#include "gp10b/gp10b_gating_reglist.h"
#include "gp10b/regops_gp10b.h"
#include "gp10b/cde_gp10b.h"
#include "gp10b/therm_gp10b.h"

#include "gm20b/gr_gm20b.h"
#include "gm20b/fifo_gm20b.h"
#include "gm20b/pmu_gm20b.h"
#include "gm20b/clk_gm20b.h"
#include <linux/tegra-fuse.h>

#include "gp10b.h"
#include "hw_proj_gp10b.h"

#define FUSE_OPT_PRIV_SEC_EN_0 0x264
#define PRIV_SECURITY_ENABLED 0x01

static struct gpu_ops gp10b_ops = {
	.clock_gating = {
		.slcg_bus_load_gating_prod =
			gp10b_slcg_bus_load_gating_prod,
		.slcg_ce2_load_gating_prod =
			gp10b_slcg_ce2_load_gating_prod,
		.slcg_chiplet_load_gating_prod =
			gp10b_slcg_chiplet_load_gating_prod,
		.slcg_ctxsw_firmware_load_gating_prod =
			gp10b_slcg_ctxsw_firmware_load_gating_prod,
		.slcg_fb_load_gating_prod =
			gp10b_slcg_fb_load_gating_prod,
		.slcg_fifo_load_gating_prod =
			gp10b_slcg_fifo_load_gating_prod,
		.slcg_gr_load_gating_prod =
			gr_gp10b_slcg_gr_load_gating_prod,
		.slcg_ltc_load_gating_prod =
			ltc_gp10b_slcg_ltc_load_gating_prod,
		.slcg_perf_load_gating_prod =
			gp10b_slcg_perf_load_gating_prod,
		.slcg_priring_load_gating_prod =
			gp10b_slcg_priring_load_gating_prod,
		.slcg_pmu_load_gating_prod =
			gp10b_slcg_pmu_load_gating_prod,
		.slcg_therm_load_gating_prod =
			gp10b_slcg_therm_load_gating_prod,
		.slcg_xbar_load_gating_prod =
			gp10b_slcg_xbar_load_gating_prod,
		.blcg_bus_load_gating_prod =
			gp10b_blcg_bus_load_gating_prod,
		.blcg_ce_load_gating_prod =
			gp10b_blcg_ce_load_gating_prod,
		.blcg_ctxsw_firmware_load_gating_prod =
			gp10b_blcg_ctxsw_firmware_load_gating_prod,
		.blcg_fb_load_gating_prod =
			gp10b_blcg_fb_load_gating_prod,
		.blcg_fifo_load_gating_prod =
			gp10b_blcg_fifo_load_gating_prod,
		.blcg_gr_load_gating_prod =
			gp10b_blcg_gr_load_gating_prod,
		.blcg_ltc_load_gating_prod =
			gp10b_blcg_ltc_load_gating_prod,
		.blcg_pwr_csb_load_gating_prod =
			gp10b_blcg_pwr_csb_load_gating_prod,
		.blcg_pmu_load_gating_prod =
			gp10b_blcg_pmu_load_gating_prod,
		.blcg_xbar_load_gating_prod =
			gp10b_blcg_xbar_load_gating_prod,
		.pg_gr_load_gating_prod =
			gr_gp10b_pg_gr_load_gating_prod,
	}
};

static int gp10b_get_litter_value(struct gk20a *g,
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
		ret = proj_fbpa_base_v();
		break;
	case GPU_LIT_FBPA_SHARED_BASE:
		ret = proj_fbpa_shared_base_v();
		break;
	default:
		gk20a_err(dev_from_gk20a(g), "Missing definition %d", value);
		BUG();
		break;
	}

	return ret;
}

int gp10b_init_hal(struct gk20a *g)
{
	struct gpu_ops *gops = &g->ops;
	struct nvgpu_gpu_characteristics *c = &g->gpu_characteristics;

	*gops = gp10b_ops;
	gops->pmupstate = false;
#ifdef CONFIG_TEGRA_ACR
	if (tegra_platform_is_linsim()) {
		gops->privsecurity = 0;
		gops->securegpccs = 0;
	} else {
		if (tegra_fuse_readl(FUSE_OPT_PRIV_SEC_EN_0) &
				PRIV_SECURITY_ENABLED) {
			gops->privsecurity = 1;
			gops->securegpccs =1;
		} else {
			gk20a_dbg_info("priv security is disabled in HW");
			gops->privsecurity = 0;
			gops->securegpccs = 0;
		}
	}
#else
	if (tegra_platform_is_linsim()) {
		gk20a_dbg_info("running ASIM with PRIV security disabled");
		gops->privsecurity = 0;
		gops->securegpccs = 0;
	} else {
		if (tegra_fuse_readl(FUSE_OPT_PRIV_SEC_EN_0) &
				PRIV_SECURITY_ENABLED) {
			gk20a_dbg_info("priv security is not supported but enabled");
			gops->privsecurity = 1;
			gops->securegpccs =1;
			return -EPERM;
		} else {
			gops->privsecurity = 0;
			gops->securegpccs = 0;
		}
	}
#endif

	gp10b_init_mc(gops);
	gp10b_init_gr(gops);
	gp10b_init_fecs_trace_ops(gops);
	gp10b_init_ltc(gops);
	gp10b_init_fb(gops);
	gp10b_init_fifo(gops);
	gp10b_init_ce(gops);
	gp10b_init_gr_ctx(gops);
	gp10b_init_mm(gops);
	gp10b_init_pmu_ops(gops);
	gk20a_init_debug_ops(gops);
	gp10b_init_regops(gops);
	gp10b_init_cde_ops(gops);
	gp10b_init_therm_ops(gops);
	gk20a_init_tsg_ops(gops);
	gops->name = "gp10b";
	gops->chip_init_gpu_characteristics = gp10b_init_gpu_characteristics;
	gops->get_litter_value = gp10b_get_litter_value;
	gops->read_ptimer = gk20a_read_ptimer;

	c->twod_class = FERMI_TWOD_A;
	c->threed_class = PASCAL_A;
	c->compute_class = PASCAL_COMPUTE_A;
	c->gpfifo_class = PASCAL_CHANNEL_GPFIFO_A;
	c->inline_to_memory_class = KEPLER_INLINE_TO_MEMORY_B;
	c->dma_copy_class = PASCAL_DMA_COPY_A;

	return 0;
}
