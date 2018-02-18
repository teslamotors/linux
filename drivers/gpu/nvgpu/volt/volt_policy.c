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
#include "gk20a/pmu_gk20a.h"

#include "pmuif/gpmuifperfvfe.h"
#include "include/bios.h"
#include "volt.h"

static u32 volt_policy_pmu_data_init_super(struct gk20a *g,
	struct boardobj *pboardobj, struct nv_pmu_boardobj *ppmudata)
{
	return boardobj_pmudatainit_super(g, pboardobj, ppmudata);
}

static u32 construct_volt_policy(struct gk20a *g,
	struct boardobj  **ppboardobj, u16 size, void *pArgs)
{
	struct voltage_policy *pvolt_policy = NULL;
	u32 status = 0;

	status = boardobj_construct_super(g, ppboardobj, size, pArgs);
	if (status)
		return status;

	pvolt_policy = (struct voltage_policy *)*ppboardobj;

	pvolt_policy->super.pmudatainit = volt_policy_pmu_data_init_super;

	return status;
}

static u32 construct_volt_policy_split_rail(struct gk20a *g,
	struct boardobj **ppboardobj, u16 size, void *pArgs)
{
	struct voltage_policy_split_rail *ptmp_policy  =
			(struct voltage_policy_split_rail *)pArgs;
	struct voltage_policy_split_rail *pvolt_policy = NULL;
	u32 status = 0;

	status = construct_volt_policy(g, ppboardobj, size, pArgs);
	if (status)
		return status;

	pvolt_policy = (struct voltage_policy_split_rail *)*ppboardobj;

	pvolt_policy->rail_idx_master = ptmp_policy->rail_idx_master;
	pvolt_policy->rail_idx_slave = ptmp_policy->rail_idx_slave;
	pvolt_policy->delta_min_vfe_equ_idx =
			ptmp_policy->delta_min_vfe_equ_idx;
	pvolt_policy->delta_max_vfe_equ_idx =
			ptmp_policy->delta_max_vfe_equ_idx;

	return status;
}

u32 volt_policy_pmu_data_init_split_rail(struct gk20a *g,
	struct boardobj *pboardobj, struct nv_pmu_boardobj *ppmudata)
{
	u32 status = 0;
	struct voltage_policy_split_rail *ppolicy;
	struct nv_pmu_volt_volt_policy_splt_r_boardobj_set *pset;

	status = volt_policy_pmu_data_init_super(g, pboardobj, ppmudata);
	if (status)
		goto done;

	ppolicy = (struct voltage_policy_split_rail *)pboardobj;
	pset = (struct nv_pmu_volt_volt_policy_splt_r_boardobj_set *)
				ppmudata;

	pset->rail_idx_master = ppolicy->rail_idx_master;
	pset->rail_idx_slave = ppolicy->rail_idx_slave;
	pset->delta_min_vfe_equ_idx = ppolicy->delta_min_vfe_equ_idx;
	pset->delta_max_vfe_equ_idx = ppolicy->delta_max_vfe_equ_idx;
	pset->offset_delta_min_uv = ppolicy->offset_delta_min_uv;
	pset->offset_delta_max_uv = ppolicy->offset_delta_max_uv;

done:
	return status;
}

static u32 volt_construct_volt_policy_split_rail_single_step(struct gk20a *g,
	struct boardobj **ppboardobj, u16 size, void *pargs)
{
	struct boardobj *pboardobj   = NULL;
	struct voltage_policy_split_rail_single_step *p_volt_policy = NULL;
	u32 status = 0;

	status = construct_volt_policy_split_rail(g, ppboardobj, size, pargs);
	if (status)
		return status;

	pboardobj = (*ppboardobj);
	p_volt_policy = (struct voltage_policy_split_rail_single_step *)
						*ppboardobj;

	pboardobj->pmudatainit = volt_policy_pmu_data_init_split_rail;

	return status;
}

struct voltage_policy *volt_volt_policy_construct(struct gk20a *g, void *pargs)
{
	struct boardobj *pboard_obj = NULL;
	u32 status = 0;

	if (BOARDOBJ_GET_TYPE(pargs) ==
		CTRL_VOLT_POLICY_TYPE_SR_SINGLE_STEP) {
		status = volt_construct_volt_policy_split_rail_single_step(g,
			&pboard_obj,
			sizeof(struct voltage_policy_split_rail_single_step),
			pargs);
		if (status) {
			gk20a_err(dev_from_gk20a(g),
				"Could not allocate memory for voltage_policy");
				pboard_obj = NULL;
		}
	}

	return (struct voltage_policy *)pboard_obj;
}

static u8 volt_policy_type_convert(u8 vbios_type)
{
	switch (vbios_type) {
	case NV_VBIOS_VOLTAGE_POLICY_1X_ENTRY_TYPE_SINGLE_RAIL:
		return CTRL_VOLT_POLICY_TYPE_SINGLE_RAIL;

	case NV_VBIOS_VOLTAGE_POLICY_1X_ENTRY_TYPE_SR_MULTI_STEP:
		return CTRL_VOLT_POLICY_TYPE_SR_MULTI_STEP;

	case NV_VBIOS_VOLTAGE_POLICY_1X_ENTRY_TYPE_SR_SINGLE_STEP:
		return CTRL_VOLT_POLICY_TYPE_SR_SINGLE_STEP;
	}

	return CTRL_VOLT_POLICY_TYPE_INVALID;
}

static u32 volt_get_volt_policy_table(struct gk20a *g,
		struct voltage_policy_metadata *pvolt_policy_metadata)
{
	u32 status = 0;
	u8 *voltage_policy_table_ptr = NULL;
	struct voltage_policy *ppolicy = NULL;
	struct vbios_voltage_policy_table_1x_header header = { 0 };
	struct vbios_voltage_policy_table_1x_entry entry  = { 0 };
	u8 i;
	u8 policy_type = 0;
	u8 *entry_offset;
	union policy_type {
		struct boardobj		board_obj;
		struct voltage_policy	volt_policy;
		struct voltage_policy_split_rail	split_rail;
	} policy_type_data;

	if (g->ops.bios.get_perf_table_ptrs) {
		voltage_policy_table_ptr =
			(u8 *)g->ops.bios.get_perf_table_ptrs(g,
				g->bios.perf_token, VOLTAGE_POLICY_TABLE);
		if (voltage_policy_table_ptr == NULL) {
			status = -EINVAL;
			goto done;
		}
	} else {
		status = -EINVAL;
		goto done;
	}

	memcpy(&header, voltage_policy_table_ptr,
			sizeof(struct vbios_voltage_policy_table_1x_header));

	/* Set Voltage Policy Table Index for Perf Core VF Sequence client. */
	pvolt_policy_metadata->perf_core_vf_seq_policy_idx =
		(u8)header.perf_core_vf_seq_policy_idx;

	/* Read in the entries. */
	for (i = 0; i < header.num_table_entries; i++) {
		entry_offset = (voltage_policy_table_ptr + header.header_size +
						i * header.table_entry_size);

		memcpy(&entry, entry_offset,
			sizeof(struct vbios_voltage_policy_table_1x_entry));

		memset(&policy_type_data, 0x0, sizeof(policy_type_data));

		policy_type = volt_policy_type_convert((u8)entry.type);

		if (policy_type == CTRL_VOLT_POLICY_TYPE_SR_SINGLE_STEP) {
			policy_type_data.split_rail.rail_idx_master =
				(u8)BIOS_GET_FIELD(entry.param0,
				  NV_VBIOS_VPT_ENTRY_PARAM0_SR_VD_MASTER);

			policy_type_data.split_rail.rail_idx_slave =
				(u8)BIOS_GET_FIELD(entry.param0,
				  NV_VBIOS_VPT_ENTRY_PARAM0_SR_VD_SLAVE);

			policy_type_data.split_rail.delta_min_vfe_equ_idx =
				(u8)BIOS_GET_FIELD(entry.param0,
				  NV_VBIOS_VPT_ENTRY_PARAM0_SR_DELTA_SM_MIN);

			policy_type_data.split_rail.delta_max_vfe_equ_idx =
				(u8)BIOS_GET_FIELD(entry.param0,
				  NV_VBIOS_VPT_ENTRY_PARAM0_SR_DELTA_SM_MAX);
		}

		policy_type_data.board_obj.type = policy_type;

		ppolicy = volt_volt_policy_construct(g,
				(void *)&policy_type_data);
		if (ppolicy == NULL) {
			gk20a_err(dev_from_gk20a(g),
				"Failure to construct VOLT_POLICY object.");
			status = -EINVAL;
			goto done;
		}

		status = boardobjgrp_objinsert(
				&pvolt_policy_metadata->volt_policies.super,
				(struct boardobj *)ppolicy, i);
		if (status) {
			gk20a_err(dev_from_gk20a(g),
				"could not add volt_policy for entry %d into boardobjgrp ",
				i);
			goto done;
		}
	}

done:
	return status;
}
static u32 _volt_policy_devgrp_pmudata_instget(struct gk20a *g,
	struct nv_pmu_boardobjgrp *pmuboardobjgrp,
	struct nv_pmu_boardobj **ppboardobjpmudata, u8 idx)
{
	struct nv_pmu_volt_volt_policy_boardobj_grp_set *pgrp_set =
		(struct nv_pmu_volt_volt_policy_boardobj_grp_set *)
		pmuboardobjgrp;

	gk20a_dbg_info("");

	/*check whether pmuboardobjgrp has a valid boardobj in index*/
	if (((u32)BIT(idx) &
		pgrp_set->hdr.data.super.obj_mask.super.data[0]) == 0)
		return -EINVAL;

	*ppboardobjpmudata = (struct nv_pmu_boardobj *)
		&pgrp_set->objects[idx].data.board_obj;
	gk20a_dbg_info(" Done");
	return 0;
}

static u32 _volt_policy_devgrp_pmustatus_instget(struct gk20a *g,
	void *pboardobjgrppmu,
	struct nv_pmu_boardobj_query **ppboardobjpmustatus, u8 idx)
{
	struct nv_pmu_volt_volt_policy_boardobj_grp_get_status *p_get_status =
		(struct nv_pmu_volt_volt_policy_boardobj_grp_get_status *)
		pboardobjgrppmu;

	/*check whether pmuboardobjgrp has a valid boardobj in index*/
	if (((u32)BIT(idx) &
		p_get_status->hdr.data.super.obj_mask.super.data[0]) == 0)
		return -EINVAL;

	*ppboardobjpmustatus = (struct nv_pmu_boardobj_query *)
			&p_get_status->objects[idx].data.board_obj;
	return 0;
}

u32 volt_policy_pmu_setup(struct gk20a *g)
{
	u32 status;
	struct boardobjgrp *pboardobjgrp = NULL;

	gk20a_dbg_info("");

	pboardobjgrp =
		&g->perf_pmu.volt.volt_policy_metadata.volt_policies.super;

	if (!pboardobjgrp->bconstructed)
		return -EINVAL;

	status = pboardobjgrp->pmuinithandle(g, pboardobjgrp);

	gk20a_dbg_info("Done");
	return status;
}

u32 volt_policy_sw_setup(struct gk20a *g)
{
	u32 status = 0;
	struct boardobjgrp *pboardobjgrp = NULL;

	gk20a_dbg_info("");

	status = boardobjgrpconstruct_e32(
			&g->perf_pmu.volt.volt_policy_metadata.volt_policies);
	if (status) {
		gk20a_err(dev_from_gk20a(g),
			"error creating boardobjgrp for volt rail, status - 0x%x",
			status);
		goto done;
	}

	pboardobjgrp =
		&g->perf_pmu.volt.volt_policy_metadata.volt_policies.super;

	pboardobjgrp->pmudatainstget  = _volt_policy_devgrp_pmudata_instget;
	pboardobjgrp->pmustatusinstget  = _volt_policy_devgrp_pmustatus_instget;

	/* Obtain Voltage Rail Table from VBIOS */
	status = volt_get_volt_policy_table(g, &g->perf_pmu.volt.
			volt_policy_metadata);
	if (status)
		goto done;

	/* Populate data for the VOLT_RAIL PMU interface */
	BOARDOBJGRP_PMU_CONSTRUCT(pboardobjgrp, VOLT, VOLT_POLICY);

	status = BOARDOBJGRP_PMU_CMD_GRP_SET_CONSTRUCT(g, pboardobjgrp,
			volt, VOLT, volt_policy, VOLT_POLICY);
	if (status) {
		gk20a_err(dev_from_gk20a(g),
			"error constructing PMU_BOARDOBJ_CMD_GRP_SET interface - 0x%x",
			status);
		goto done;
	}

	status = BOARDOBJGRP_PMU_CMD_GRP_GET_STATUS_CONSTRUCT(g,
		&g->perf_pmu.volt.volt_policy_metadata.volt_policies.super,
			volt, VOLT, volt_policy, VOLT_POLICY);
	if (status) {
		gk20a_err(dev_from_gk20a(g),
			"error constructing PMU_BOARDOBJ_CMD_GRP_SET interface - 0x%x",
			status);
		goto done;
	}

done:
	gk20a_dbg_info(" done status %x", status);
	return status;
}
