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
#include "include/bios.h"
#include "boardobj/boardobjgrp.h"
#include "boardobj/boardobjgrp_e32.h"
#include "pmuif/gpmuifboardobj.h"
#include "gm206/bios_gm206.h"
#include "ctrl/ctrlvolt.h"
#include "ctrl/ctrlperf.h"
#include "gk20a/pmu_gk20a.h"

#include "pmuif/gpmuifperfvfe.h"
#include "pmuif/gpmuifvolt.h"
#include "include/bios.h"
#include "volt.h"

#define RAIL_COUNT 2

struct volt_rpc_pmucmdhandler_params {
	struct nv_pmu_volt_rpc *prpc_call;
	u32 success;
};

static void volt_rpc_pmucmdhandler(struct gk20a *g, struct pmu_msg *msg,
				  void *param, u32 handle, u32 status)
{
	struct volt_rpc_pmucmdhandler_params *phandlerparams =
		(struct volt_rpc_pmucmdhandler_params *)param;

	gk20a_dbg_info("");

	if (msg->msg.volt.msg_type != NV_PMU_VOLT_MSG_ID_RPC) {
		gk20a_err(dev_from_gk20a(g), "unsupported msg for VOLT RPC %x",
			msg->msg.volt.msg_type);
		return;
	}

	if (phandlerparams->prpc_call->b_supported)
		phandlerparams->success = 1;
}


static u32 volt_pmu_rpc_execute(struct gk20a *g,
	struct nv_pmu_volt_rpc *prpc_call)
{
	struct pmu_cmd cmd = { { 0 } };
	struct pmu_msg msg = { { 0 } };
	struct pmu_payload payload = { { 0 } };
	u32 status = 0;
	u32 seqdesc;
	struct volt_rpc_pmucmdhandler_params handler = {0};

	cmd.hdr.unit_id = PMU_UNIT_VOLT;
	cmd.hdr.size = (u32)sizeof(struct nv_pmu_volt_cmd) +
					(u32)sizeof(struct pmu_hdr);
	cmd.cmd.volt.cmd_type = NV_PMU_VOLT_CMD_ID_RPC;
	msg.hdr.size = sizeof(struct pmu_msg);

	payload.in.buf = (u8 *)prpc_call;
	payload.in.size = (u32)sizeof(struct nv_pmu_volt_rpc);
	payload.in.fb_size = PMU_CMD_SUBMIT_PAYLOAD_PARAMS_FB_SIZE_UNUSED;
	payload.in.offset = NV_PMU_VOLT_CMD_RPC_ALLOC_OFFSET;

	payload.out.buf = (u8 *)prpc_call;
	payload.out.size = (u32)sizeof(struct nv_pmu_volt_rpc);
	payload.out.fb_size = PMU_CMD_SUBMIT_PAYLOAD_PARAMS_FB_SIZE_UNUSED;
	payload.out.offset = NV_PMU_VOLT_MSG_RPC_ALLOC_OFFSET;

	handler.prpc_call = prpc_call;
	handler.success = 0;

	status = gk20a_pmu_cmd_post(g, &cmd, NULL, &payload,
			PMU_COMMAND_QUEUE_LPQ,
			volt_rpc_pmucmdhandler, (void *)&handler,
			&seqdesc, ~0);
	if (status) {
		gk20a_err(dev_from_gk20a(g), "unable to post volt RPC cmd %x",
			cmd.cmd.volt.cmd_type);
		goto volt_pmu_rpc_execute;
	}

	pmu_wait_message_cond(&g->pmu,
			gk20a_get_gr_idle_timeout(g),
			&handler.success, 1);

	if (handler.success == 0) {
		status = -EINVAL;
		gk20a_err(dev_from_gk20a(g), "rpc call to volt failed");
	}

volt_pmu_rpc_execute:
	return status;
}

u32 volt_pmu_send_load_cmd_to_pmu(struct gk20a *g)
{
	struct nv_pmu_volt_rpc rpc_call = { 0 };
	u32 status = 0;

	rpc_call.function = NV_PMU_VOLT_RPC_ID_LOAD;

	status =  volt_pmu_rpc_execute(g, &rpc_call);
	if (status)
		gk20a_err(dev_from_gk20a(g),
			"Error while executing LOAD RPC: status = 0x%08x.",
			status);

	return status;
}

static u32 volt_rail_get_voltage(struct gk20a *g,
	u8 volt_domain, u32 *pvoltage_uv)
{
	struct nv_pmu_volt_rpc rpc_call = { 0 };
	u32 status  = 0;
	u8 rail_idx;

	rail_idx = volt_rail_volt_domain_convert_to_idx(g, volt_domain);
	if ((rail_idx == CTRL_VOLT_RAIL_INDEX_INVALID) ||
		(!VOLT_RAIL_INDEX_IS_VALID(&g->perf_pmu.volt, rail_idx))) {
		gk20a_err(dev_from_gk20a(g),
			"failed: volt_domain = %d, voltage rail table = %d.",
			volt_domain, rail_idx);
		return -EINVAL;
	}

	/* Set RPC parameters. */
	rpc_call.function = NV_PMU_VOLT_RPC_ID_VOLT_RAIL_GET_VOLTAGE;
	rpc_call.params.volt_rail_get_voltage.rail_idx = rail_idx;

	/* Execute the voltage get request via PMU RPC. */
	status = volt_pmu_rpc_execute(g, &rpc_call);
	if (status) {
		gk20a_err(dev_from_gk20a(g),
			"Error while executing volt_rail_get_voltage rpc");
		return status;
	}

	/* Copy out the current voltage. */
	*pvoltage_uv = rpc_call.params.volt_rail_get_voltage.voltage_uv;

	return status;
}


static u32 volt_policy_set_voltage(struct gk20a *g, u8 client_id,
		struct ctrl_perf_volt_rail_list *prail_list)
{
	struct nv_pmu_volt_rpc rpc_call = { 0 };
	struct obj_volt *pvolt = &g->perf_pmu.volt;
	u32 status = 0;
	u8 policy_idx = CTRL_VOLT_POLICY_INDEX_INVALID;
	u8 i = 0;

	/* Sanity check input rail list. */
	for (i = 0; i < prail_list->num_rails; i++) {
		if ((prail_list->rails[i].volt_domain ==
				CTRL_VOLT_DOMAIN_INVALID) ||
			(prail_list->rails[i].voltage_uv ==
				NV_PMU_VOLT_VALUE_0V_IN_UV)) {
			gk20a_err(dev_from_gk20a(g), "Invalid voltage domain or target ");
			gk20a_err(dev_from_gk20a(g), " client_id = %d, listEntry = %d ",
					client_id, i);
			gk20a_err(dev_from_gk20a(g),
				"volt_domain = %d, voltage_uv = %d uV.",
				prail_list->rails[i].volt_domain,
				prail_list->rails[i].voltage_uv);
			status = -EINVAL;
			goto exit;
		}
	}

	/* Convert the client ID to index. */
	if (client_id == CTRL_VOLT_POLICY_CLIENT_PERF_CORE_VF_SEQ)
		policy_idx =
			pvolt->volt_policy_metadata.perf_core_vf_seq_policy_idx;
	else {
		status = -EINVAL;
		goto exit;
	}

	/* Set RPC parameters. */
	rpc_call.function = NV_PMU_VOLT_RPC_ID_VOLT_POLICY_SET_VOLTAGE;
	rpc_call.params.volt_policy_voltage_data.policy_idx = policy_idx;
	memcpy(&rpc_call.params.volt_policy_voltage_data.rail_list, prail_list,
		(sizeof(struct ctrl_perf_volt_rail_list)));

	/* Execute the voltage change request via PMU RPC. */
	status = volt_pmu_rpc_execute(g, &rpc_call);
	if (status)
		gk20a_err(dev_from_gk20a(g),
			"Error while executing VOLT_POLICY_SET_VOLTAGE RPC");

exit:
	return status;
}

u32 volt_set_voltage(struct gk20a *g, u32 logic_voltage_uv, u32 sram_voltage_uv)
{
	u32 status = 0;
	struct ctrl_perf_volt_rail_list rail_list = { 0 };

	rail_list.num_rails = RAIL_COUNT;
	rail_list.rails[0].volt_domain = CTRL_VOLT_DOMAIN_LOGIC;
	rail_list.rails[0].voltage_uv = logic_voltage_uv;
	rail_list.rails[0].voltage_min_noise_unaware_uv = logic_voltage_uv;
	rail_list.rails[1].volt_domain = CTRL_VOLT_DOMAIN_SRAM;
	rail_list.rails[1].voltage_uv = sram_voltage_uv;
	rail_list.rails[1].voltage_min_noise_unaware_uv = sram_voltage_uv;

	status = volt_policy_set_voltage(g,
		CTRL_VOLT_POLICY_CLIENT_PERF_CORE_VF_SEQ, &rail_list);

	return status;

}

u32 volt_get_voltage(struct gk20a *g, u32 volt_domain, u32 *voltage_uv)
{
	return volt_rail_get_voltage(g, volt_domain, voltage_uv);
}

static int volt_policy_set_noiseaware_vmin(struct gk20a *g,
		struct ctrl_volt_volt_rail_list *prail_list)
{
	struct nv_pmu_volt_rpc rpc_call = { 0 };
	u32 status = 0;

	/* Set RPC parameters. */
	rpc_call.function = NV_PMU_VOLT_RPC_ID_VOLT_RAIL_SET_NOISE_UNAWARE_VMIN;
	rpc_call.params.volt_rail_set_noise_unaware_vmin.num_rails =
			prail_list->num_rails;
	memcpy(&rpc_call.params.volt_rail_set_noise_unaware_vmin.rail_list,
		prail_list, (sizeof(struct ctrl_volt_volt_rail_list)));

	/* Execute the voltage change request via PMU RPC. */
	status = volt_pmu_rpc_execute(g, &rpc_call);
	if (status) {
		gk20a_err(dev_from_gk20a(g),
			"Error while executing VOLT_POLICY_SET_VOLTAGE RPC");
		return -EINVAL;
	}

	return 0;
}

int volt_set_noiseaware_vmin(struct gk20a *g, u32 logic_voltage_uv,
	u32 sram_voltage_uv)
{
	int status = 0;
	struct ctrl_volt_volt_rail_list rail_list = { 0 };

	rail_list.num_rails = RAIL_COUNT;
	rail_list.rails[0].rail_idx = 0;
	rail_list.rails[0].voltage_uv = logic_voltage_uv;
	rail_list.rails[1].rail_idx = 1;
	rail_list.rails[1].voltage_uv = sram_voltage_uv;

	status = volt_policy_set_noiseaware_vmin(g, &rail_list);

	return status;

}

