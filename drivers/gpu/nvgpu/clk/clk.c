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

#include "gk20a/gk20a.h"
#include "clk.h"
#include "pmuif/gpmuifclk.h"
#include "pmuif/gpmuifvolt.h"
#include "ctrl/ctrlclk.h"
#include "ctrl/ctrlvolt.h"
#include "volt/volt.h"
#include "gk20a/pmu_gk20a.h"

#define BOOT_GPC2CLK_MHZ  2581
#define BOOT_MCLK_MHZ     3003

struct clkrpc_pmucmdhandler_params {
	struct nv_pmu_clk_rpc *prpccall;
	u32 success;
};

static void clkrpc_pmucmdhandler(struct gk20a *g, struct pmu_msg *msg,
				 void *param, u32 handle, u32 status)
{
	struct clkrpc_pmucmdhandler_params *phandlerparams =
		(struct clkrpc_pmucmdhandler_params *)param;

	gk20a_dbg_info("");

	if (msg->msg.clk.msg_type != NV_PMU_CLK_MSG_ID_RPC) {
		gk20a_err(dev_from_gk20a(g),
			  "unsupported msg for VFE LOAD RPC %x",
			  msg->msg.clk.msg_type);
		return;
	}

	if (phandlerparams->prpccall->b_supported)
		phandlerparams->success = 1;
}

int clk_pmu_freq_controller_load(struct gk20a *g, bool bload)
{
	struct pmu_cmd cmd;
	struct pmu_msg msg;
	struct pmu_payload payload = { {0} };
	u32 status;
	u32 seqdesc;
	struct nv_pmu_clk_rpc rpccall = {0};
	struct clkrpc_pmucmdhandler_params handler = {0};
	struct nv_pmu_clk_load *clkload;
	struct clk_freq_controllers *pclk_freq_controllers;
	struct ctrl_boardobjgrp_mask_e32 *load_mask;

	pclk_freq_controllers = &g->clk_pmu.clk_freq_controllers;
	rpccall.function = NV_PMU_CLK_RPC_ID_LOAD;
	clkload = &rpccall.params.clk_load;
	clkload->feature = NV_NV_PMU_CLK_LOAD_FEATURE_FREQ_CONTROLLER;
	clkload->action_mask = bload ?
			NV_NV_PMU_CLK_LOAD_ACTION_MASK_FREQ_CONTROLLER_CALLBACK_YES :
			NV_NV_PMU_CLK_LOAD_ACTION_MASK_FREQ_CONTROLLER_CALLBACK_NO;

	load_mask = &rpccall.params.clk_load.payload.freq_controllers.load_mask;

	status = boardobjgrpmask_export(
		&pclk_freq_controllers->freq_ctrl_load_mask.super,
		pclk_freq_controllers->freq_ctrl_load_mask.super.bitcount,
		&load_mask->super);

	cmd.hdr.unit_id = PMU_UNIT_CLK;
	cmd.hdr.size =  (u32)sizeof(struct nv_pmu_clk_cmd) +
			(u32)sizeof(struct pmu_hdr);

	cmd.cmd.clk.cmd_type = NV_PMU_CLK_CMD_ID_RPC;
	msg.hdr.size = sizeof(struct pmu_msg);

	payload.in.buf = (u8 *)&rpccall;
	payload.in.size = (u32)sizeof(struct nv_pmu_clk_rpc);
	payload.in.fb_size = PMU_CMD_SUBMIT_PAYLOAD_PARAMS_FB_SIZE_UNUSED;
	payload.in.offset = NV_PMU_CLK_CMD_RPC_ALLOC_OFFSET;

	payload.out.buf = (u8 *)&rpccall;
	payload.out.size = (u32)sizeof(struct nv_pmu_clk_rpc);
	payload.out.fb_size = PMU_CMD_SUBMIT_PAYLOAD_PARAMS_FB_SIZE_UNUSED;
	payload.out.offset = NV_PMU_CLK_MSG_RPC_ALLOC_OFFSET;

	handler.prpccall = &rpccall;
	handler.success = 0;
	status = gk20a_pmu_cmd_post(g, &cmd, NULL, &payload,
			PMU_COMMAND_QUEUE_LPQ,
			clkrpc_pmucmdhandler, (void *)&handler,
			&seqdesc, ~0);

	if (status) {
		gk20a_err(dev_from_gk20a(g),
			"unable to post clk RPC cmd %x",
			cmd.cmd.clk.cmd_type);
		goto done;
	}

	pmu_wait_message_cond(&g->pmu,
			gk20a_get_gr_idle_timeout(g),
			&handler.success, 1);

	if (handler.success == 0) {
		gk20a_err(dev_from_gk20a(g), "rpc call to load freq cntlr cal failed");
		status = -EINVAL;
	}

done:
	return status;
}

u32 clk_pmu_vin_load(struct gk20a *g)
{
	struct pmu_cmd cmd;
	struct pmu_msg msg;
	struct pmu_payload payload = { {0} };
	u32 status;
	u32 seqdesc;
	struct nv_pmu_clk_rpc rpccall = {0};
	struct clkrpc_pmucmdhandler_params handler = {0};
	struct nv_pmu_clk_load *clkload;

	rpccall.function = NV_PMU_CLK_RPC_ID_LOAD;
	clkload = &rpccall.params.clk_load;
	clkload->feature = NV_NV_PMU_CLK_LOAD_FEATURE_VIN;
	clkload->action_mask = NV_NV_PMU_CLK_LOAD_ACTION_MASK_VIN_HW_CAL_PROGRAM_YES << 4;

	cmd.hdr.unit_id = PMU_UNIT_CLK;
	cmd.hdr.size =  (u32)sizeof(struct nv_pmu_clk_cmd) +
			(u32)sizeof(struct pmu_hdr);

	cmd.cmd.clk.cmd_type = NV_PMU_CLK_CMD_ID_RPC;
	msg.hdr.size = sizeof(struct pmu_msg);

	payload.in.buf = (u8 *)&rpccall;
	payload.in.size = (u32)sizeof(struct nv_pmu_clk_rpc);
	payload.in.fb_size = PMU_CMD_SUBMIT_PAYLOAD_PARAMS_FB_SIZE_UNUSED;
	payload.in.offset = NV_PMU_CLK_CMD_RPC_ALLOC_OFFSET;

	payload.out.buf = (u8 *)&rpccall;
	payload.out.size = (u32)sizeof(struct nv_pmu_clk_rpc);
	payload.out.fb_size = PMU_CMD_SUBMIT_PAYLOAD_PARAMS_FB_SIZE_UNUSED;
	payload.out.offset = NV_PMU_CLK_MSG_RPC_ALLOC_OFFSET;

	handler.prpccall = &rpccall;
	handler.success = 0;
	status = gk20a_pmu_cmd_post(g, &cmd, NULL, &payload,
			PMU_COMMAND_QUEUE_LPQ,
			clkrpc_pmucmdhandler, (void *)&handler,
			&seqdesc, ~0);

	if (status) {
		gk20a_err(dev_from_gk20a(g),
			"unable to post clk RPC cmd %x",
			cmd.cmd.clk.cmd_type);
		goto done;
	}

	pmu_wait_message_cond(&g->pmu,
			gk20a_get_gr_idle_timeout(g),
			&handler.success, 1);

	if (handler.success == 0) {
		gk20a_err(dev_from_gk20a(g), "rpc call to load vin cal failed");
		status = -EINVAL;
	}

done:
	return status;
}

static u32 clk_pmu_vf_inject(struct gk20a *g, struct set_fll_clk *setfllclk)
{
	struct pmu_cmd cmd;
	struct pmu_msg msg;
	struct pmu_payload payload = { {0} };
	u32 status;
	u32 seqdesc;
	struct nv_pmu_clk_rpc rpccall = {0};
	struct clkrpc_pmucmdhandler_params handler = {0};
	struct nv_pmu_clk_vf_change_inject *vfchange;

	if ((setfllclk->gpc2clkmhz == 0) || (setfllclk->xbar2clkmhz == 0) ||
		(setfllclk->sys2clkmhz == 0) || (setfllclk->voltuv == 0))
		return -EINVAL;

	if ((setfllclk->target_regime_id_gpc > CTRL_CLK_FLL_REGIME_ID_FR) ||
		(setfllclk->target_regime_id_sys > CTRL_CLK_FLL_REGIME_ID_FR) ||
		(setfllclk->target_regime_id_xbar > CTRL_CLK_FLL_REGIME_ID_FR))
		return -EINVAL;

	rpccall.function = NV_PMU_CLK_RPC_ID_CLK_VF_CHANGE_INJECT;
	vfchange = &rpccall.params.clk_vf_change_inject;
	vfchange->flags = 0;
	vfchange->clk_list.num_domains = 3;
	vfchange->clk_list.clk_domains[0].clk_domain = CTRL_CLK_DOMAIN_GPC2CLK;
	vfchange->clk_list.clk_domains[0].clk_freq_khz =
					setfllclk->gpc2clkmhz * 1000;
	vfchange->clk_list.clk_domains[0].clk_flags = 0;
	vfchange->clk_list.clk_domains[0].current_regime_id =
		setfllclk->current_regime_id_gpc;
	vfchange->clk_list.clk_domains[0].target_regime_id =
		setfllclk->target_regime_id_gpc;
	vfchange->clk_list.clk_domains[1].clk_domain = CTRL_CLK_DOMAIN_XBAR2CLK;
	vfchange->clk_list.clk_domains[1].clk_freq_khz =
					setfllclk->xbar2clkmhz * 1000;
	vfchange->clk_list.clk_domains[1].clk_flags = 0;
	vfchange->clk_list.clk_domains[1].current_regime_id =
		setfllclk->current_regime_id_xbar;
	vfchange->clk_list.clk_domains[1].target_regime_id =
		setfllclk->target_regime_id_xbar;
	vfchange->clk_list.clk_domains[2].clk_domain = CTRL_CLK_DOMAIN_SYS2CLK;
	vfchange->clk_list.clk_domains[2].clk_freq_khz =
					setfllclk->sys2clkmhz * 1000;
	vfchange->clk_list.clk_domains[2].clk_flags = 0;
	vfchange->clk_list.clk_domains[2].current_regime_id =
		setfllclk->current_regime_id_sys;
	vfchange->clk_list.clk_domains[2].target_regime_id =
		setfllclk->target_regime_id_sys;
	vfchange->volt_list.num_rails = 1;
	vfchange->volt_list.rails[0].volt_domain = CTRL_VOLT_DOMAIN_LOGIC;
	vfchange->volt_list.rails[0].voltage_uv = setfllclk->voltuv;
	vfchange->volt_list.rails[0].voltage_min_noise_unaware_uv =
				setfllclk->voltuv;

	cmd.hdr.unit_id = PMU_UNIT_CLK;
	cmd.hdr.size =  (u32)sizeof(struct nv_pmu_clk_cmd) +
			(u32)sizeof(struct pmu_hdr);

	cmd.cmd.clk.cmd_type = NV_PMU_CLK_CMD_ID_RPC;
	msg.hdr.size = sizeof(struct pmu_msg);

	payload.in.buf = (u8 *)&rpccall;
	payload.in.size = (u32)sizeof(struct nv_pmu_clk_rpc);
	payload.in.fb_size = PMU_CMD_SUBMIT_PAYLOAD_PARAMS_FB_SIZE_UNUSED;
	payload.in.offset = NV_PMU_CLK_CMD_RPC_ALLOC_OFFSET;

	payload.out.buf = (u8 *)&rpccall;
	payload.out.size = (u32)sizeof(struct nv_pmu_clk_rpc);
	payload.out.fb_size = PMU_CMD_SUBMIT_PAYLOAD_PARAMS_FB_SIZE_UNUSED;
	payload.out.offset = NV_PMU_CLK_MSG_RPC_ALLOC_OFFSET;

	handler.prpccall = &rpccall;
	handler.success = 0;

	status = gk20a_pmu_cmd_post(g, &cmd, NULL, &payload,
			PMU_COMMAND_QUEUE_LPQ,
			clkrpc_pmucmdhandler, (void *)&handler,
			&seqdesc, ~0);

	if (status) {
		gk20a_err(dev_from_gk20a(g),
			  "unable to post clk RPC cmd %x",
			  cmd.cmd.clk.cmd_type);
		goto done;
	}

	pmu_wait_message_cond(&g->pmu,
			gk20a_get_gr_idle_timeout(g),
			&handler.success, 1);

	if (handler.success == 0) {
		gk20a_err(dev_from_gk20a(g), "rpc call to inject clock failed");
		status = -EINVAL;
	}
done:
	return status;
}

static u32 find_regime_id(struct gk20a *g, u32 domain, u16 clkmhz)
{
	struct fll_device *pflldev;
	u8 j;
	struct clk_pmupstate *pclk = &g->clk_pmu;

	BOARDOBJGRP_FOR_EACH(&(pclk->avfs_fllobjs.super.super),
		struct fll_device *, pflldev, j) {
		if (pflldev->clk_domain == domain) {
			if (pflldev->regime_desc.fixed_freq_regime_limit_mhz >=
							clkmhz)
				return CTRL_CLK_FLL_REGIME_ID_FFR;
			else
				return CTRL_CLK_FLL_REGIME_ID_FR;
		}
	}
	return CTRL_CLK_FLL_REGIME_ID_INVALID;
}

static int set_regime_id(struct gk20a *g, u32 domain, u32 regimeid)
{
	struct fll_device *pflldev;
	u8 j;
	struct clk_pmupstate *pclk = &g->clk_pmu;

	BOARDOBJGRP_FOR_EACH(&(pclk->avfs_fllobjs.super.super),
		struct fll_device *, pflldev, j) {
		if (pflldev->clk_domain == domain) {
			pflldev->regime_desc.regime_id = regimeid;
			return 0;
		}
	}
	return -EINVAL;
}

static int get_regime_id(struct gk20a *g, u32 domain, u32 *regimeid)
{
	struct fll_device *pflldev;
	u8 j;
	struct clk_pmupstate *pclk = &g->clk_pmu;

	BOARDOBJGRP_FOR_EACH(&(pclk->avfs_fllobjs.super.super),
		struct fll_device *, pflldev, j) {
		if (pflldev->clk_domain == domain) {
			*regimeid = pflldev->regime_desc.regime_id;
			return 0;
		}
	}
	return -EINVAL;
}

int clk_set_fll_clks(struct gk20a *g, struct set_fll_clk *setfllclk)
{
	int status = -EINVAL;

	/*set regime ids */
	status = get_regime_id(g, CTRL_CLK_DOMAIN_GPC2CLK,
			&setfllclk->current_regime_id_gpc);
	if (status)
		goto done;

	setfllclk->target_regime_id_gpc = find_regime_id(g,
			CTRL_CLK_DOMAIN_GPC2CLK, setfllclk->gpc2clkmhz);

	status = get_regime_id(g, CTRL_CLK_DOMAIN_SYS2CLK,
			&setfllclk->current_regime_id_sys);
	if (status)
		goto done;

	setfllclk->target_regime_id_sys = find_regime_id(g,
			CTRL_CLK_DOMAIN_SYS2CLK, setfllclk->sys2clkmhz);

	status = get_regime_id(g, CTRL_CLK_DOMAIN_XBAR2CLK,
			&setfllclk->current_regime_id_xbar);
	if (status)
		goto done;

	setfllclk->target_regime_id_xbar = find_regime_id(g,
			CTRL_CLK_DOMAIN_XBAR2CLK, setfllclk->xbar2clkmhz);

	status = clk_pmu_vf_inject(g, setfllclk);

	if (status)
		gk20a_err(dev_from_gk20a(g),
			"vf inject to change clk failed");

	/* save regime ids */
	status = set_regime_id(g, CTRL_CLK_DOMAIN_XBAR2CLK,
			setfllclk->target_regime_id_xbar);
	if (status)
		goto done;

	status = set_regime_id(g, CTRL_CLK_DOMAIN_GPC2CLK,
			setfllclk->target_regime_id_gpc);
	if (status)
		goto done;

	status = set_regime_id(g, CTRL_CLK_DOMAIN_SYS2CLK,
			setfllclk->target_regime_id_sys);
	if (status)
		goto done;
done:
	return status;
}

int clk_get_fll_clks(struct gk20a *g, struct set_fll_clk *setfllclk)
{
	int status = -EINVAL;
	struct clk_domain *pdomain;
	u8 i;
	struct clk_pmupstate *pclk = &g->clk_pmu;
	u16 clkmhz = 0;
	struct clk_domain_3x_master *p3xmaster;
	struct clk_domain_3x_slave *p3xslave;
	unsigned long slaveidxmask;

	if (setfllclk->gpc2clkmhz == 0)
		return -EINVAL;

	BOARDOBJGRP_FOR_EACH(&(pclk->clk_domainobjs.super.super),
			struct clk_domain *, pdomain, i) {

		if (pdomain->api_domain == CTRL_CLK_DOMAIN_GPC2CLK) {

			if (!pdomain->super.implements(g, &pdomain->super,
				CTRL_CLK_CLK_DOMAIN_TYPE_3X_MASTER)) {
				status = -EINVAL;
				goto done;
			}
			p3xmaster = (struct clk_domain_3x_master *)pdomain;
			slaveidxmask = p3xmaster->slave_idxs_mask;
			for_each_set_bit(i, &slaveidxmask, 32) {
				p3xslave = (struct clk_domain_3x_slave *)
						CLK_CLK_DOMAIN_GET(pclk, i);
				if ((p3xslave->super.super.super.api_domain !=
				     CTRL_CLK_DOMAIN_XBAR2CLK) &&
				    (p3xslave->super.super.super.api_domain !=
				     CTRL_CLK_DOMAIN_SYS2CLK))
					continue;
				clkmhz = 0;
				status = p3xslave->clkdomainclkgetslaveclk(g,
						pclk,
						(struct clk_domain *)p3xslave,
						&clkmhz,
						setfllclk->gpc2clkmhz);
				if (status) {
					status = -EINVAL;
					goto done;
				}
				if (p3xslave->super.super.super.api_domain ==
				     CTRL_CLK_DOMAIN_XBAR2CLK)
					setfllclk->xbar2clkmhz = clkmhz;
				if (p3xslave->super.super.super.api_domain ==
				     CTRL_CLK_DOMAIN_SYS2CLK)
					setfllclk->sys2clkmhz = clkmhz;
			}
		}
	}
done:
	return status;
}

u32 clk_domain_print_vf_table(struct gk20a *g, u32 clkapidomain)
{
	u32 status = -EINVAL;
	struct clk_domain *pdomain;
	u8 i;
	struct clk_pmupstate *pclk = &g->clk_pmu;
	u16 clkmhz = 0;
	u32 volt = 0;

	BOARDOBJGRP_FOR_EACH(&(pclk->clk_domainobjs.super.super),
			struct clk_domain *, pdomain, i) {
		if (pdomain->api_domain == clkapidomain) {
			status = pdomain->clkdomainclkvfsearch(g, pclk,
				pdomain, &clkmhz, &volt,
				CLK_PROG_VFE_ENTRY_LOGIC);
			status = pdomain->clkdomainclkvfsearch(g, pclk,
				pdomain, &clkmhz, &volt,
				CLK_PROG_VFE_ENTRY_SRAM);
		}
	}
	return status;
}

u32 clk_domain_get_f_or_v(
	struct gk20a *g,
	u32 clkapidomain,
	u16 *pclkmhz,
	u32 *pvoltuv,
	u8 railidx
)
{
	u32 status = -EINVAL;
	struct clk_domain *pdomain;
	u8 i;
	struct clk_pmupstate *pclk = &g->clk_pmu;
	u8 rail;

	if ((pclkmhz == NULL) || (pvoltuv == NULL))
		return -EINVAL;

	if (railidx == CTRL_VOLT_DOMAIN_LOGIC)
		rail = CLK_PROG_VFE_ENTRY_LOGIC;
	else if (railidx == CTRL_VOLT_DOMAIN_SRAM)
		rail = CLK_PROG_VFE_ENTRY_SRAM;
	else
		return -EINVAL;

	BOARDOBJGRP_FOR_EACH(&(pclk->clk_domainobjs.super.super),
			struct clk_domain *, pdomain, i) {
		if (pdomain->api_domain == clkapidomain) {
			status = pdomain->clkdomainclkvfsearch(g, pclk,
				pdomain, pclkmhz, pvoltuv, rail);
			return status;
		}
	}
	return status;
}

u32 clk_domain_get_f_points(
	struct gk20a *g,
	u32 clkapidomain,
	u32 *pfpointscount,
	u16 *pfreqpointsinmhz
)
{
	u32 status = -EINVAL;
	struct clk_domain *pdomain;
	u8 i;
	struct clk_pmupstate *pclk = &g->clk_pmu;

	if (pfpointscount == NULL)
		return -EINVAL;

	if ((pfreqpointsinmhz == NULL) && (*pfpointscount != 0))
		return -EINVAL;

	BOARDOBJGRP_FOR_EACH(&(pclk->clk_domainobjs.super.super),
			struct clk_domain *, pdomain, i) {
		if (pdomain->api_domain == clkapidomain) {
			status = pdomain->clkdomainclkgetfpoints(g, pclk,
				pdomain, pfpointscount,
				pfreqpointsinmhz,
				CLK_PROG_VFE_ENTRY_LOGIC);
			return status;
		}
	}
	return status;
}
