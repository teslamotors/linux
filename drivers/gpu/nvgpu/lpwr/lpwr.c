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
#include "perf/perf.h"
#include "lpwr.h"

static int get_lpwr_idx_table(struct gk20a *g)
{
	u32 *lpwr_idx_table_ptr;
	u8 *entry_addr;
	u32 idx;
	struct nvgpu_lpwr_bios_idx_data *pidx_data =
			&g->perf_pmu.lpwr.lwpr_bios_data.idx;
	struct nvgpu_bios_lpwr_idx_table_1x_header header = { 0 };
	struct nvgpu_bios_lpwr_idx_table_1x_entry entry = { 0 };

	if (g->ops.bios.get_perf_table_ptrs) {
		lpwr_idx_table_ptr = (u32 *)g->ops.bios.get_perf_table_ptrs(g,
			g->bios.perf_token, LOWPOWER_TABLE);
		if (lpwr_idx_table_ptr == NULL)
			return -EINVAL;
	} else
		 return -EINVAL;

	memcpy(&header, lpwr_idx_table_ptr,
		sizeof(struct nvgpu_bios_lpwr_idx_table_1x_header));

	if (header.entry_count >= LPWR_VBIOS_IDX_ENTRY_COUNT_MAX)
		return -EINVAL;

	pidx_data->base_sampling_period = (u16)header.base_sampling_period;

	/* Parse the LPWR Index Table entries.*/
	for (idx = 0; idx < header.entry_count; idx++) {
		entry_addr = (u8 *)lpwr_idx_table_ptr + header.header_size +
			(idx * header.entry_size);

		memcpy(&entry, entry_addr,
			sizeof(struct nvgpu_bios_lpwr_idx_table_1x_entry));

		pidx_data->entry[idx].pcie_idx = entry.pcie_idx;
		pidx_data->entry[idx].gr_idx = entry.gr_idx;
		pidx_data->entry[idx].ms_idx = entry.ms_idx;
		pidx_data->entry[idx].di_idx = entry.di_idx;
		pidx_data->entry[idx].gc6_idx = entry.gc6_idx;

	}

	return 0;
}

static int get_lpwr_gr_table(struct gk20a *g)
{
	u32 *lpwr_gr_table_ptr;
	u8 *entry_addr;
	u32 idx;
	struct nvgpu_lpwr_bios_gr_data *pgr_data =
			&g->perf_pmu.lpwr.lwpr_bios_data.gr;
	struct nvgpu_bios_lpwr_gr_table_1x_header header = { 0 };
	struct nvgpu_bios_lpwr_gr_table_1x_entry entry = { 0 };

	if (g->ops.bios.get_perf_table_ptrs) {
		lpwr_gr_table_ptr = (u32 *)g->ops.bios.get_perf_table_ptrs(g,
			g->bios.perf_token, LOWPOWER_GR_TABLE);
		if (lpwr_gr_table_ptr == NULL)
			return -EINVAL;
	} else
		 return -EINVAL;

	memcpy(&header, lpwr_gr_table_ptr,
		sizeof(struct nvgpu_bios_lpwr_gr_table_1x_header));

	/* Parse the LPWR Index Table entries.*/
	for (idx = 0; idx < header.entry_count; idx++) {
		entry_addr = (u8 *)lpwr_gr_table_ptr + header.header_size +
			(idx * header.entry_size);

		memcpy(&entry, entry_addr,
			sizeof(struct nvgpu_bios_lpwr_gr_table_1x_entry));

		if (BIOS_GET_FIELD(entry.feautre_mask,
			NV_VBIOS_LPWR_MS_FEATURE_MASK_MS)) {
			pgr_data->entry[idx].gr_enabled = true;

			pgr_data->entry[idx].feature_mask =
				NVGPU_PMU_GR_FEATURE_MASK_ALL;

			if (!BIOS_GET_FIELD(entry.feautre_mask,
				NV_VBIOS_LPWR_GR_FEATURE_MASK_GR_RPPG))
				pgr_data->entry[idx].feature_mask &=
					~NVGPU_PMU_GR_FEATURE_MASK_RPPG;
		}

	}

	return 0;
}

static int get_lpwr_ms_table(struct gk20a *g)
{
	u32 *lpwr_ms_table_ptr;
	u8 *entry_addr;
	u32 idx;
	struct nvgpu_lpwr_bios_ms_data *pms_data =
			&g->perf_pmu.lpwr.lwpr_bios_data.ms;
	struct nvgpu_bios_lpwr_ms_table_1x_header header = { 0 };
	struct nvgpu_bios_lpwr_ms_table_1x_entry entry = { 0 };

	if (g->ops.bios.get_perf_table_ptrs) {
		lpwr_ms_table_ptr = (u32 *)g->ops.bios.get_perf_table_ptrs(g,
			g->bios.perf_token, LOWPOWER_MS_TABLE);
		if (lpwr_ms_table_ptr == NULL)
			return -EINVAL;
	} else
		 return -EINVAL;

	memcpy(&header, lpwr_ms_table_ptr,
		sizeof(struct nvgpu_bios_lpwr_ms_table_1x_header));

	if (header.entry_count >= LPWR_VBIOS_MS_ENTRY_COUNT_MAX)
		return -EINVAL;

	pms_data->default_entry_idx = (u8)header.default_entry_idx;

	pms_data->idle_threshold_us = (u32)(header.idle_threshold_us * 10);

	/* Parse the LPWR MS Table entries.*/
	for (idx = 0; idx < header.entry_count; idx++) {
		entry_addr = (u8 *)lpwr_ms_table_ptr + header.header_size +
			(idx * header.entry_size);

		memcpy(&entry, entry_addr,
			sizeof(struct nvgpu_bios_lpwr_ms_table_1x_entry));

		if (BIOS_GET_FIELD(entry.feautre_mask,
			NV_VBIOS_LPWR_MS_FEATURE_MASK_MS)) {
			pms_data->entry[idx].ms_enabled = true;

			pms_data->entry[idx].feature_mask =
				NVGPU_PMU_MS_FEATURE_MASK_ALL;

			if (!BIOS_GET_FIELD(entry.feautre_mask,
				NV_VBIOS_LPWR_MS_FEATURE_MASK_MS_CLOCK_GATING))
				pms_data->entry[idx].feature_mask &=
					~NVGPU_PMU_MS_FEATURE_MASK_CLOCK_GATING;

			if (!BIOS_GET_FIELD(entry.feautre_mask,
				NV_VBIOS_LPWR_MS_FEATURE_MASK_MS_SWASR))
				pms_data->entry[idx].feature_mask &=
					~NVGPU_PMU_MS_FEATURE_MASK_SW_ASR;

			if (!BIOS_GET_FIELD(entry.feautre_mask,
				NV_VBIOS_LPWR_MS_FEATURE_MASK_MS_RPPG))
				pms_data->entry[idx].feature_mask &=
					~NVGPU_PMU_MS_FEATURE_MASK_RPPG;
		}

		pms_data->entry[idx].dynamic_current_logic =
				entry.dynamic_current_logic;

		pms_data->entry[idx].dynamic_current_sram =
				entry.dynamic_current_sram;
	}

	return 0;
}

u32 nvgpu_lpwr_pg_setup(struct gk20a *g)
{
	u32 err = 0;

	gk20a_dbg_fn("");

	err = get_lpwr_gr_table(g);
	if (err)
		return err;

	err = get_lpwr_ms_table(g);
	if (err)
		return err;

	err = get_lpwr_idx_table(g);

	return err;
}

static void nvgpu_pmu_handle_param_lpwr_msg(struct gk20a *g,
		struct pmu_msg *msg, void *param,
		u32 handle, u32 status)
{
	u32 *ack_status = param;

	gk20a_dbg_fn("");

	if (status != 0) {
		gk20a_err(dev_from_gk20a(g), "LWPR PARAM cmd aborted");
		return;
	}

	*ack_status = 1;

	gp106_dbg_pmu("lpwr-param is acknowledged from PMU %x",
			msg->msg.pg.msg_type);
}

int nvgpu_lwpr_mclk_change(struct gk20a *g, u32 pstate)
{
	struct pmu_cmd cmd;
	u32 seq, status = 0;
	u32 payload = NV_PMU_PG_PARAM_MCLK_CHANGE_MS_SWASR_ENABLED;
	struct clk_set_info *pstate_info;
	u32 ack_status = 0;

	gk20a_dbg_fn("");

	pstate_info = pstate_get_clk_set_info(g, pstate,
			clkwhich_mclk);
	if (!pstate_info)
		return -EINVAL;

	if (pstate_info->max_mhz >
			MAX_SWASR_MCLK_FREQ_WITHOUT_WR_TRAINING_MAXWELL_MHZ)
		payload |=
			NV_PMU_PG_PARAM_MCLK_CHANGE_GDDR5_WR_TRAINING_ENABLED;

	if (payload != g->perf_pmu.lpwr.mclk_change_cache) {
		g->perf_pmu.lpwr.mclk_change_cache = payload;

		cmd.hdr.unit_id = PMU_UNIT_PG;
		cmd.hdr.size = PMU_CMD_HDR_SIZE +
			sizeof(struct pmu_pg_cmd_mclk_change);
		cmd.cmd.pg.mclk_change.cmd_type =
			PMU_PG_CMD_ID_PG_PARAM;
		cmd.cmd.pg.mclk_change.cmd_id =
			PMU_PG_PARAM_CMD_MCLK_CHANGE;
		cmd.cmd.pg.mclk_change.data = payload;

		gp106_dbg_pmu("cmd post MS PMU_PG_PARAM_CMD_MCLK_CHANGE");
		status = gk20a_pmu_cmd_post(g, &cmd, NULL, NULL,
			PMU_COMMAND_QUEUE_HPQ,
			nvgpu_pmu_handle_param_lpwr_msg, &ack_status, &seq, ~0);

		pmu_wait_message_cond(&g->pmu, gk20a_get_gr_idle_timeout(g),
			&ack_status, 1);
		if (ack_status == 0) {
			status = -EINVAL;
			gk20a_err(dev_from_gk20a(g), "MCLK-CHANGE ACK failed");
		}
	}

	return status;
}

u32 nvgpu_lpwr_post_init(struct gk20a *g)
{
	struct pmu_cmd cmd;
	u32 seq;
	u32 status = 0;
	u32 ack_status = 0;

	memset(&cmd, 0, sizeof(struct pmu_cmd));
	cmd.hdr.unit_id = PMU_UNIT_PG;
	cmd.hdr.size   = PMU_CMD_HDR_SIZE +
		sizeof(struct pmu_pg_cmd_post_init_param);

	cmd.cmd.pg.post_init.cmd_type =
		PMU_PG_CMD_ID_PG_PARAM;
	cmd.cmd.pg.post_init.cmd_id =
		PMU_PG_PARAM_CMD_POST_INIT;

	gp106_dbg_pmu("cmd post post-init PMU_PG_PARAM_CMD_POST_INIT");
	status = gk20a_pmu_cmd_post(g, &cmd, NULL, NULL,
		PMU_COMMAND_QUEUE_LPQ,
		nvgpu_pmu_handle_param_lpwr_msg, &ack_status, &seq, ~0);

	pmu_wait_message_cond(&g->pmu, gk20a_get_gr_idle_timeout(g),
		&ack_status, 1);
	if (ack_status == 0) {
		status = -EINVAL;
		gk20a_err(dev_from_gk20a(g), "post-init ack failed");
	}

	return status;
}

u32 nvgpu_lpwr_is_mscg_supported(struct gk20a *g, u32 pstate_num)
{
	struct nvgpu_lpwr_bios_ms_data *pms_data =
			&g->perf_pmu.lpwr.lwpr_bios_data.ms;
	struct nvgpu_lpwr_bios_idx_data *pidx_data =
			&g->perf_pmu.lpwr.lwpr_bios_data.idx;
	struct pstate *pstate = pstate_find(g, pstate_num);
	u32 ms_idx;

	gk20a_dbg_fn("");

	if (!pstate)
		return 0;

	ms_idx = pidx_data->entry[pstate->lpwr_entry_idx].ms_idx;
	if (pms_data->entry[ms_idx].ms_enabled)
		return 1;
	else
		return 0;
}

u32 nvgpu_lpwr_is_rppg_supported(struct gk20a *g, u32 pstate_num)
{
	struct nvgpu_lpwr_bios_gr_data *pgr_data =
			&g->perf_pmu.lpwr.lwpr_bios_data.gr;
	struct nvgpu_lpwr_bios_idx_data *pidx_data =
			&g->perf_pmu.lpwr.lwpr_bios_data.idx;
	struct pstate *pstate = pstate_find(g, pstate_num);
	u32 idx;

	gk20a_dbg_fn("");

	if (!pstate)
		return 0;

	idx = pidx_data->entry[pstate->lpwr_entry_idx].gr_idx;
	if (pgr_data->entry[idx].gr_enabled)
		return 1;
	else
		return 0;
}


int nvgpu_lpwr_enable_pg(struct gk20a *g, bool pstate_lock)
{
	struct pmu_gk20a *pmu = &g->pmu;
	u32  status = 0;
	u32 is_mscg_supported = 0;
	u32 is_rppg_supported = 0;
	u32 present_pstate = 0;

	gk20a_dbg_fn("");

	if (pstate_lock)
		nvgpu_clk_arb_pstate_change_lock(g, true);
	mutex_lock(&pmu->pg_mutex);

	present_pstate = nvgpu_clk_arb_get_current_pstate(g);

	is_mscg_supported = nvgpu_lpwr_is_mscg_supported(g,
			present_pstate);
	if (is_mscg_supported && g->mscg_enabled) {
		if (!ACCESS_ONCE(pmu->mscg_stat)) {
			WRITE_ONCE(pmu->mscg_stat, PMU_MSCG_ENABLED);
			/* make status visible */
			smp_mb();
		}
	}

	is_rppg_supported = nvgpu_lpwr_is_rppg_supported(g,
			present_pstate);
	if (is_rppg_supported) {
		if (support_gk20a_pmu(g->dev) && g->elpg_enabled)
			status = gk20a_pmu_enable_elpg(g);
	}

	mutex_unlock(&pmu->pg_mutex);
	if (pstate_lock)
		nvgpu_clk_arb_pstate_change_lock(g, false);

	return status;
}

int nvgpu_lpwr_disable_pg(struct gk20a *g, bool pstate_lock)
{
	struct pmu_gk20a *pmu = &g->pmu;
	int status = 0;
	u32 is_mscg_supported = 0;
	u32 is_rppg_supported = 0;
	u32 present_pstate = 0;

	gk20a_dbg_fn("");

	if (pstate_lock)
		nvgpu_clk_arb_pstate_change_lock(g, true);
	mutex_lock(&pmu->pg_mutex);

	present_pstate = nvgpu_clk_arb_get_current_pstate(g);

	is_rppg_supported = nvgpu_lpwr_is_rppg_supported(g,
			present_pstate);
	if (is_rppg_supported) {
		if (support_gk20a_pmu(g->dev) && g->elpg_enabled) {
			status = gk20a_pmu_disable_elpg(g);
			if (status)
				goto exit_unlock;
		}
	}

	is_mscg_supported = nvgpu_lpwr_is_mscg_supported(g,
			present_pstate);
	if (is_mscg_supported && g->mscg_enabled) {
		if (ACCESS_ONCE(pmu->mscg_stat)) {
			WRITE_ONCE(pmu->mscg_stat, PMU_MSCG_DISABLED);
			/* make status visible */
			smp_mb();
		}
	}

exit_unlock:
	mutex_unlock(&pmu->pg_mutex);
	if (pstate_lock)
		nvgpu_clk_arb_pstate_change_lock(g, false);

	gk20a_dbg_fn("done");
	return status;
}
