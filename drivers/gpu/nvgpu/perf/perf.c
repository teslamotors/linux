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
#include "perf.h"
#include "pmuif/gpmuifperf.h"
#include "pmuif/gpmuifperfvfe.h"
#include "gk20a/pmu_gk20a.h"
#include "clk/clk_arb.h"

struct perfrpc_pmucmdhandler_params {
	struct nv_pmu_perf_rpc *prpccall;
	u32 success;
};

static void perfrpc_pmucmdhandler(struct gk20a *g, struct pmu_msg *msg,
				  void *param, u32 handle, u32 status)
{
	struct perfrpc_pmucmdhandler_params *phandlerparams =
		(struct perfrpc_pmucmdhandler_params *)param;

	gk20a_dbg_info("");

	if (msg->msg.perf.msg_type != NV_PMU_PERF_MSG_ID_RPC) {
		gk20a_err(dev_from_gk20a(g),
		"unsupported msg for VFE LOAD RPC %x",
		msg->msg.perf.msg_type);
		return;
	}

	if (phandlerparams->prpccall->b_supported)
		phandlerparams->success = 1;
}

static int pmu_handle_perf_event(struct gk20a *g, void *pmu_msg)
{
	struct nv_pmu_perf_msg *msg = (struct nv_pmu_perf_msg *)pmu_msg;

	gk20a_dbg_fn("");
	switch (msg->msg_type) {
	case NV_PMU_PERF_MSG_ID_VFE_CALLBACK:
		nvgpu_clk_arb_schedule_vf_table_update(g);
		break;
	default:
		WARN_ON(1);
		break;
	}
	return 0;
}

u32 perf_pmu_vfe_load(struct gk20a *g)
{
	struct pmu_cmd cmd;
	struct pmu_msg msg;
	struct pmu_payload payload = { {0} };
	u32 status;
	u32 seqdesc;
	struct nv_pmu_perf_rpc rpccall = {0};
	struct perfrpc_pmucmdhandler_params handler = {0};

	/*register call back for future VFE updates*/
	g->ops.perf.handle_pmu_perf_event = pmu_handle_perf_event;

	rpccall.function = NV_PMU_PERF_RPC_ID_VFE_LOAD;
	rpccall.params.vfe_load.b_load = true;
	cmd.hdr.unit_id = PMU_UNIT_PERF;
	cmd.hdr.size = (u32)sizeof(struct nv_pmu_perf_cmd) +
		       (u32)sizeof(struct pmu_hdr);

	cmd.cmd.perf.cmd_type = NV_PMU_PERF_CMD_ID_RPC;
	msg.hdr.size = sizeof(struct pmu_msg);

	payload.in.buf = (u8 *)&rpccall;
	payload.in.size = (u32)sizeof(struct nv_pmu_perf_rpc);
	payload.in.fb_size = PMU_CMD_SUBMIT_PAYLOAD_PARAMS_FB_SIZE_UNUSED;
	payload.in.offset = NV_PMU_PERF_CMD_RPC_ALLOC_OFFSET;

	payload.out.buf = (u8 *)&rpccall;
	payload.out.size = (u32)sizeof(struct nv_pmu_perf_rpc);
	payload.out.fb_size = PMU_CMD_SUBMIT_PAYLOAD_PARAMS_FB_SIZE_UNUSED;
	payload.out.offset = NV_PMU_PERF_MSG_RPC_ALLOC_OFFSET;

	handler.prpccall = &rpccall;
	handler.success = 0;

	status = gk20a_pmu_cmd_post(g, &cmd, NULL, &payload,
			PMU_COMMAND_QUEUE_LPQ,
			perfrpc_pmucmdhandler, (void *)&handler,
			&seqdesc, ~0);

	if (status) {
		gk20a_err(dev_from_gk20a(g),
			  "unable to post perf RPC cmd %x",
			  cmd.cmd.perf.cmd_type);
		goto done;
	}

	pmu_wait_message_cond(&g->pmu,
			gk20a_get_gr_idle_timeout(g),
			&handler.success, 1);

	if (handler.success == 0) {
		status = -EINVAL;
		gk20a_err(dev_from_gk20a(g), "rpc call to load VFE failed");
	}
done:
	return status;
}
