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

#include <linux/delay.h>	/* for udelay */
#include "gk20a/gk20a.h"
#include "gk20a/pmu_gk20a.h"
#include "gk20a/pmu_gk20a.h"
#include "gm20b/acr_gm20b.h"
#include "gm206/acr_gm206.h"
#include "gm20b/pmu_gm20b.h"
#include "gm206/pmu_gm206.h"
#include "hw_gr_gm206.h"
#include "hw_pwr_gm206.h"


#define gm206_dbg_pmu(fmt, arg...) \
	gk20a_dbg(gpu_dbg_pmu, fmt, ##arg)

bool gm206_is_lazy_bootstrap(u32 falcon_id)
{
	bool enable_status = false;

	switch (falcon_id) {
	case LSF_FALCON_ID_FECS:
		enable_status = true;
		break;
	case LSF_FALCON_ID_GPCCS:
		enable_status = true;
		break;
	default:
		break;
	}

	return enable_status;
}

bool gm206_is_priv_load(u32 falcon_id)
{
	bool enable_status = false;

	switch (falcon_id) {
	case LSF_FALCON_ID_FECS:
		enable_status = true;
		break;
	case LSF_FALCON_ID_GPCCS:
		enable_status = true;
		break;
	default:
		break;
	}

	return enable_status;
}

static void gm206_pmu_load_multiple_falcons(struct gk20a *g, u32 falconidmask,
					 u32 flags)
{
	struct pmu_gk20a *pmu = &g->pmu;
	struct pmu_cmd cmd;
	u32 seq;

	gk20a_dbg_fn("");

	gm206_dbg_pmu("wprinit status = %x\n", g->ops.pmu.lspmuwprinitdone);
	if (g->ops.pmu.lspmuwprinitdone) {
		/* send message to load FECS falcon */
		memset(&cmd, 0, sizeof(struct pmu_cmd));
		cmd.hdr.unit_id = PMU_UNIT_ACR;
		cmd.hdr.size = PMU_CMD_HDR_SIZE +
		  sizeof(struct pmu_acr_cmd_bootstrap_multiple_falcons);
		cmd.cmd.acr.boot_falcons.cmd_type =
		  PMU_ACR_CMD_ID_BOOTSTRAP_MULTIPLE_FALCONS;
		cmd.cmd.acr.boot_falcons.flags = flags;
		cmd.cmd.acr.boot_falcons.falconidmask =
				falconidmask;
		cmd.cmd.acr.boot_falcons.usevamask = 0;
		cmd.cmd.acr.boot_falcons.wprvirtualbase.lo = 0;
		cmd.cmd.acr.boot_falcons.wprvirtualbase.hi = 0;

		gm206_dbg_pmu("PMU_ACR_CMD_ID_BOOTSTRAP_MULTIPLE_FALCONS:%x\n",
				falconidmask);
		gk20a_pmu_cmd_post(g, &cmd, NULL, NULL, PMU_COMMAND_QUEUE_HPQ,
				pmu_handle_fecs_boot_acr_msg, pmu, &seq, ~0);
	}

	gk20a_dbg_fn("done");
}

int gm206_load_falcon_ucode(struct gk20a *g, u32 falconidmask)
{
	u32 flags = PMU_ACR_CMD_BOOTSTRAP_FALCON_FLAGS_RESET_YES;

	/* GM20B PMU supports loading FECS and GPCCS only */
	if (falconidmask == 0)
		return -EINVAL;
	if (falconidmask & ~((1 << LSF_FALCON_ID_FECS) |
				(1 << LSF_FALCON_ID_GPCCS)))
				return -EINVAL;
	g->ops.pmu.lsfloadedfalconid = 0;
	/* check whether pmu is ready to bootstrap lsf if not wait for it */
	if (!g->ops.pmu.lspmuwprinitdone) {
		pmu_wait_message_cond(&g->pmu,
				gk20a_get_gr_idle_timeout(g),
				&g->ops.pmu.lspmuwprinitdone, 1);
		/* check again if it still not ready indicate an error */
		if (!g->ops.pmu.lspmuwprinitdone) {
			gk20a_err(dev_from_gk20a(g),
				"PMU not ready to load LSF");
			return -ETIMEDOUT;
		}
	}
	/* load falcon(s) */
	gm206_pmu_load_multiple_falcons(g, falconidmask, flags);
	pmu_wait_message_cond(&g->pmu,
			gk20a_get_gr_idle_timeout(g),
			&g->ops.pmu.lsfloadedfalconid, falconidmask);
	if (g->ops.pmu.lsfloadedfalconid != falconidmask)
		return -ETIMEDOUT;
	return 0;
}


void gm206_init_pmu_ops(struct gpu_ops *gops)
{
	if (gops->privsecurity) {
		gm206_init_secure_pmu(gops);
		gops->pmu.init_wpr_region = gm20b_pmu_init_acr;
		gops->pmu.load_lsfalcon_ucode = gm206_load_falcon_ucode;
		gops->pmu.is_lazy_bootstrap = gm206_is_lazy_bootstrap;
		gops->pmu.is_priv_load = gm206_is_priv_load;
	} else {
		gk20a_init_pmu_ops(gops);
		gops->pmu.pmu_setup_hw_and_bootstrap =
			gm20b_init_nspmu_setup_hw1;
		gops->pmu.load_lsfalcon_ucode = NULL;
		gops->pmu.init_wpr_region = NULL;
	}
	gops->pmu.pmu_setup_elpg = NULL;
	gops->pmu.lspmuwprinitdone = 0;
	gops->pmu.fecsbootstrapdone = false;
	gops->pmu.write_dmatrfbase = gm20b_write_dmatrfbase;
	gops->pmu.pmu_elpg_statistics = NULL;
	gops->pmu.pmu_pg_init_param = NULL;
	gops->pmu.pmu_pg_supported_engines_list = NULL;
	gops->pmu.pmu_pg_engines_feature_list = NULL;
	gops->pmu.pmu_pg_param_post_init = NULL;
	gops->pmu.send_lrf_tex_ltc_dram_overide_en_dis_cmd = NULL;
	gops->pmu.dump_secure_fuses = NULL;
	gops->pmu.reset = gk20a_pmu_reset;
}

