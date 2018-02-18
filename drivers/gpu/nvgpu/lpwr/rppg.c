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
#include "gk20a/pmu_gk20a.h"
#include "gp106/pmu_gp106.h"
#include "gk20a/pmu_api.h"
#include "gm206/bios_gm206.h"
#include "pstate/pstate.h"
#include "include/bios.h"
#include "pmuif/gpmuif_pg_rppg.h"

static void pmu_handle_rppg_init_msg(struct gk20a *g, struct pmu_msg *msg,
	void *param, u32 handle, u32 status)
{

	u8 ctrlId = NV_PMU_RPPG_CTRL_ID_MAX;
	u32 *success = param;

	if (status == 0) {
		switch (msg->msg.pg.rppg_msg.cmn.msg_id) {
		case NV_PMU_RPPG_MSG_ID_INIT_CTRL_ACK:
			ctrlId = msg->msg.pg.rppg_msg.init_ctrl_ack.ctrl_id;
			*success = 1;
			gp106_dbg_pmu("RPPG is acknowledged from PMU %x",
				msg->msg.pg.msg_type);
		break;
		}
	}

	gp106_dbg_pmu("RPPG is acknowledged from PMU %x",
				msg->msg.pg.msg_type);
}

static u32 rppg_send_cmd(struct gk20a *g, struct nv_pmu_rppg_cmd *prppg_cmd)
{
	struct pmu_cmd cmd;
	u32 seq;
	u32 status = 0;
	u32 success = 0;

	memset(&cmd, 0, sizeof(struct pmu_cmd));
	cmd.hdr.unit_id = PMU_UNIT_PG;
	cmd.hdr.size   = PMU_CMD_HDR_SIZE +
			sizeof(struct nv_pmu_rppg_cmd);

	cmd.cmd.pg.rppg_cmd.cmn.cmd_type = PMU_PMU_PG_CMD_ID_RPPG;
	cmd.cmd.pg.rppg_cmd.cmn.cmd_id   = prppg_cmd->cmn.cmd_id;

	switch (prppg_cmd->cmn.cmd_id) {
	case NV_PMU_RPPG_CMD_ID_INIT:
		break;
	case NV_PMU_RPPG_CMD_ID_INIT_CTRL:
		cmd.cmd.pg.rppg_cmd.init_ctrl.ctrl_id =
			prppg_cmd->init_ctrl.ctrl_id;
		cmd.cmd.pg.rppg_cmd.init_ctrl.domain_id =
			prppg_cmd->init_ctrl.domain_id;
		break;
	case NV_PMU_RPPG_CMD_ID_STATS_RESET:
		cmd.cmd.pg.rppg_cmd.stats_reset.ctrl_id =
			prppg_cmd->stats_reset.ctrl_id;
		break;
	default:
		gk20a_err(dev_from_gk20a(g), "Inivalid RPPG command %d",
			prppg_cmd->cmn.cmd_id);
		return -1;
	}

	status = gk20a_pmu_cmd_post(g, &cmd, NULL, NULL, PMU_COMMAND_QUEUE_HPQ,
			pmu_handle_rppg_init_msg, &success, &seq, ~0);
	if (status) {
		gk20a_err(dev_from_gk20a(g), "Unable to submit parameter command %d",
			prppg_cmd->cmn.cmd_id);
		goto exit;
	}

	if (prppg_cmd->cmn.cmd_id == NV_PMU_RPPG_CMD_ID_INIT_CTRL) {
		pmu_wait_message_cond(&g->pmu, gk20a_get_gr_idle_timeout(g),
			&success, 1);
		if (success == 0) {
			status = -EINVAL;
			gk20a_err(dev_from_gk20a(g), "Ack for the parameter command %x",
				prppg_cmd->cmn.cmd_id);
		}
	}

exit:
	return status;
}

static u32 rppg_init(struct gk20a *g)
{
	struct nv_pmu_rppg_cmd rppg_cmd;

	rppg_cmd.init.cmd_id = NV_PMU_RPPG_CMD_ID_INIT;

	return rppg_send_cmd(g, &rppg_cmd);
}

static u32 rppg_ctrl_init(struct gk20a *g, u8 ctrl_id)
{
	struct nv_pmu_rppg_cmd rppg_cmd;

	rppg_cmd.init_ctrl.cmd_id  = NV_PMU_RPPG_CMD_ID_INIT_CTRL;
	rppg_cmd.init_ctrl.ctrl_id = ctrl_id;

	switch (ctrl_id) {
	case NV_PMU_RPPG_CTRL_ID_GR:
	case NV_PMU_RPPG_CTRL_ID_MS:
		rppg_cmd.init_ctrl.domain_id = NV_PMU_RPPG_DOMAIN_ID_GFX;
		break;
	}

	return rppg_send_cmd(g, &rppg_cmd);
}

u32 init_rppg(struct gk20a *g)
{
	u32 status;

	status = rppg_init(g);
	if (status != 0) {
		gk20a_err(dev_from_gk20a(g),
			"Failed to initialize RPPG in PMU: 0x%08x", status);
		return status;
	}


	status = rppg_ctrl_init(g, NV_PMU_RPPG_CTRL_ID_GR);
	if (status != 0) {
		gk20a_err(dev_from_gk20a(g),
			"Failed to initialize RPPG_CTRL: GR in PMU: 0x%08x",
			status);
		return status;
	}

	status = rppg_ctrl_init(g, NV_PMU_RPPG_CTRL_ID_MS);
	if (status != 0) {
		gk20a_err(dev_from_gk20a(g),
			"Failed to initialize RPPG_CTRL: MS in PMU: 0x%08x",
			status);
		return status;
	}

	return status;
}


