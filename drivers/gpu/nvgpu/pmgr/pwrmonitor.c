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
#include "pwrdev.h"
#include "include/bios.h"
#include "boardobj/boardobjgrp.h"
#include "boardobj/boardobjgrp_e32.h"
#include "pmuif/gpmuifboardobj.h"
#include "pmuif/gpmuifpmgr.h"
#include "gm206/bios_gm206.h"
#include "gk20a/pmu_gk20a.h"

static u32 _pwr_channel_pmudata_instget(struct gk20a *g,
			struct nv_pmu_boardobjgrp *pmuboardobjgrp,
			struct nv_pmu_boardobj **ppboardobjpmudata,
			u8 idx)
{
	struct nv_pmu_pmgr_pwr_channel_desc *ppmgrchannel =
		(struct nv_pmu_pmgr_pwr_channel_desc *)pmuboardobjgrp;

	gk20a_dbg_info("");

	/*check whether pmuboardobjgrp has a valid boardobj in index*/
	if (((u32)BIT(idx) &
		ppmgrchannel->hdr.data.super.obj_mask.super.data[0]) == 0)
		return -EINVAL;

	*ppboardobjpmudata = (struct nv_pmu_boardobj *)
		&ppmgrchannel->channels[idx].data.board_obj;

	/* handle Global/common data here as we need index */
	ppmgrchannel->channels[idx].data.pwr_channel.ch_idx = idx;

	gk20a_dbg_info(" Done");

	return 0;
}

static u32 _pwr_channel_rels_pmudata_instget(struct gk20a *g,
			struct nv_pmu_boardobjgrp *pmuboardobjgrp,
			struct nv_pmu_boardobj **ppboardobjpmudata,
			u8 idx)
{
	struct nv_pmu_pmgr_pwr_chrelationship_desc *ppmgrchrels =
		(struct nv_pmu_pmgr_pwr_chrelationship_desc *)pmuboardobjgrp;

	gk20a_dbg_info("");

	/*check whether pmuboardobjgrp has a valid boardobj in index*/
	if (((u32)BIT(idx) &
		ppmgrchrels->hdr.data.super.obj_mask.super.data[0]) == 0)
		return -EINVAL;

	*ppboardobjpmudata = (struct nv_pmu_boardobj *)
		&ppmgrchrels->ch_rels[idx].data.board_obj;

	gk20a_dbg_info(" Done");

	return 0;
}

static u32 _pwr_channel_state_init(struct gk20a *g)
{
	u8 indx = 0;
	struct pwr_channel *pchannel;
	u32 objmask =
		g->pmgr_pmu.pmgr_monitorobjs.pwr_channels.super.objmask;

	/* Initialize each PWR_CHANNEL's dependent channel mask */
	BOARDOBJGRP_FOR_EACH_INDEX_IN_MASK(32, indx, objmask) {
		pchannel = PMGR_PWR_MONITOR_GET_PWR_CHANNEL(g, indx);
		if (pchannel == NULL) {
			gk20a_err(dev_from_gk20a(g),
				"PMGR_PWR_MONITOR_GET_PWR_CHANNEL-failed %d", indx);
			return -EINVAL;
		}
		pchannel->dependent_ch_mask =0;
	}
	BOARDOBJGRP_FOR_EACH_INDEX_IN_MASK_END

	return 0;
}

static bool _pwr_channel_implements(struct pwr_channel *pchannel,
			u8 type)
{
	return (type == BOARDOBJ_GET_TYPE(pchannel));
}

static u32 _pwr_domains_pmudatainit_sensor(struct gk20a *g,
					struct boardobj *board_obj_ptr,
					struct nv_pmu_boardobj *ppmudata)
{
	struct nv_pmu_pmgr_pwr_channel_sensor *pmu_sensor_data;
	struct pwr_channel_sensor *sensor;
	u32 status = 0;

	status = boardobj_pmudatainit_super(g, board_obj_ptr, ppmudata);
	if (status) {
		gk20a_err(dev_from_gk20a(g),
			  "error updating pmu boardobjgrp for pwr sensor 0x%x",
			  status);
		goto done;
	}

	sensor = (struct pwr_channel_sensor *)board_obj_ptr;
	pmu_sensor_data = (struct nv_pmu_pmgr_pwr_channel_sensor *) ppmudata;

	pmu_sensor_data->super.pwr_rail = sensor->super.pwr_rail;
	pmu_sensor_data->super.volt_fixedu_v = sensor->super.volt_fixed_uv;
	pmu_sensor_data->super.pwr_corr_slope = sensor->super.pwr_corr_slope;
	pmu_sensor_data->super.pwr_corr_offsetm_w = sensor->super.pwr_corr_offset_mw;
	pmu_sensor_data->super.curr_corr_slope = sensor->super.curr_corr_slope;
	pmu_sensor_data->super.curr_corr_offsetm_a = sensor->super.curr_corr_offset_ma;
	pmu_sensor_data->super.dependent_ch_mask = sensor->super.dependent_ch_mask;
	pmu_sensor_data->super.ch_idx = 0;

	pmu_sensor_data->pwr_dev_idx = sensor->pwr_dev_idx;
	pmu_sensor_data->pwr_dev_prov_idx = sensor->pwr_dev_prov_idx;

done:
	return status;
}

static struct boardobj *construct_pwr_topology(struct gk20a *g,
				void *pargs, u16 pargs_size, u8 type)
{
	struct boardobj *board_obj_ptr = NULL;
	u32 status;
	struct pwr_channel_sensor *pwrchannel;
	struct pwr_channel_sensor *sensor = (struct pwr_channel_sensor*)pargs;

	status = boardobj_construct_super(g, &board_obj_ptr,
		pargs_size, pargs);
	if (status)
		return NULL;

	pwrchannel = (struct pwr_channel_sensor*)board_obj_ptr;

	/* Set Super class interfaces */
	board_obj_ptr->pmudatainit = _pwr_domains_pmudatainit_sensor;

	pwrchannel->super.pwr_rail = sensor->super.pwr_rail;
	pwrchannel->super.volt_fixed_uv = sensor->super.volt_fixed_uv;
	pwrchannel->super.pwr_corr_slope = sensor->super.pwr_corr_slope;
	pwrchannel->super.pwr_corr_offset_mw = sensor->super.pwr_corr_offset_mw;
	pwrchannel->super.curr_corr_slope = sensor->super.curr_corr_slope;
	pwrchannel->super.curr_corr_offset_ma = sensor->super.curr_corr_offset_ma;
	pwrchannel->super.dependent_ch_mask = 0;

	pwrchannel->pwr_dev_idx = sensor->pwr_dev_idx;
	pwrchannel->pwr_dev_prov_idx = sensor->pwr_dev_prov_idx;

	gk20a_dbg_info(" Done");

	return board_obj_ptr;
}

static u32 devinit_get_pwr_topology_table(struct gk20a *g,
				struct pmgr_pwr_monitor *ppwrmonitorobjs)
{
	u32 status = 0;
	u8 *pwr_topology_table_ptr = NULL;
	u8 *curr_pwr_topology_table_ptr = NULL;
	struct boardobj *boardobj;
	struct pwr_topology_2x_header pwr_topology_table_header = { 0 };
	struct pwr_topology_2x_entry pwr_topology_table_entry = { 0 };
	u32 index;
	u32 obj_index = 0;
	u16 pwr_topology_size;
	union {
		struct boardobj boardobj;
		struct pwr_channel pwrchannel;
		struct pwr_channel_sensor sensor;
	} pwr_topology_data;

	gk20a_dbg_info("");

	if (g->ops.bios.get_perf_table_ptrs != NULL) {
		pwr_topology_table_ptr = (u8 *)g->ops.bios.get_perf_table_ptrs(g,
				g->bios.perf_token, POWER_TOPOLOGY_TABLE);
		if (pwr_topology_table_ptr == NULL) {
			status = -EINVAL;
			goto done;
		}
	}

	memcpy(&pwr_topology_table_header, pwr_topology_table_ptr,
		VBIOS_POWER_TOPOLOGY_2X_HEADER_SIZE_06);

	if (pwr_topology_table_header.version !=
			VBIOS_POWER_TOPOLOGY_VERSION_2X) {
		status = -EINVAL;
		goto done;
	}

	g->pmgr_pmu.pmgr_monitorobjs.b_is_topology_tbl_ver_1x = false;

	if (pwr_topology_table_header.header_size <
			VBIOS_POWER_TOPOLOGY_2X_HEADER_SIZE_06) {
		status = -EINVAL;
		goto done;
	}

	if (pwr_topology_table_header.table_entry_size !=
			VBIOS_POWER_TOPOLOGY_2X_ENTRY_SIZE_16) {
		status = -EINVAL;
		goto done;
	}

	curr_pwr_topology_table_ptr = (pwr_topology_table_ptr +
		VBIOS_POWER_TOPOLOGY_2X_HEADER_SIZE_06);

	for (index = 0; index < pwr_topology_table_header.num_table_entries;
		index++) {
		u8 class_type;

		curr_pwr_topology_table_ptr += (pwr_topology_table_header.table_entry_size * index);

		pwr_topology_table_entry.flags0 = *curr_pwr_topology_table_ptr;
		pwr_topology_table_entry.pwr_rail = *(curr_pwr_topology_table_ptr + 1);

		memcpy(&pwr_topology_table_entry.param0,
			(curr_pwr_topology_table_ptr + 2),
			(VBIOS_POWER_TOPOLOGY_2X_ENTRY_SIZE_16 - 2));

		class_type = (u8)BIOS_GET_FIELD(
			pwr_topology_table_entry.flags0,
			NV_VBIOS_POWER_TOPOLOGY_2X_ENTRY_FLAGS0_CLASS);

		if (class_type == NV_VBIOS_POWER_TOPOLOGY_2X_ENTRY_FLAGS0_CLASS_SENSOR) {
			pwr_topology_data.sensor.pwr_dev_idx = (u8)BIOS_GET_FIELD(
				pwr_topology_table_entry.param1,
				NV_VBIOS_POWER_TOPOLOGY_2X_ENTRY_PARAM1_SENSOR_INDEX);
			pwr_topology_data.sensor.pwr_dev_prov_idx = (u8)BIOS_GET_FIELD(
				pwr_topology_table_entry.param1,
				NV_VBIOS_POWER_TOPOLOGY_2X_ENTRY_PARAM1_SENSOR_PROVIDER_INDEX);

			pwr_topology_size = sizeof(struct pwr_channel_sensor);
		} else
			continue;

		/* Initialize data for the parent class */
		pwr_topology_data.boardobj.type = CTRL_PMGR_PWR_CHANNEL_TYPE_SENSOR;
		pwr_topology_data.pwrchannel.pwr_rail = (u8)pwr_topology_table_entry.pwr_rail;
		pwr_topology_data.pwrchannel.volt_fixed_uv = pwr_topology_table_entry.param0;
		pwr_topology_data.pwrchannel.pwr_corr_slope = (1 << 12);
		pwr_topology_data.pwrchannel.pwr_corr_offset_mw = 0;
		pwr_topology_data.pwrchannel.curr_corr_slope  =
			(u32)pwr_topology_table_entry.curr_corr_slope;
		pwr_topology_data.pwrchannel.curr_corr_offset_ma =
			(s32)pwr_topology_table_entry.curr_corr_offset;

		boardobj = construct_pwr_topology(g, &pwr_topology_data,
					pwr_topology_size, pwr_topology_data.boardobj.type);

		if (!boardobj) {
			gk20a_err(dev_from_gk20a(g),
				"unable to create pwr topology for %d type %d",
				index, pwr_topology_data.boardobj.type);
			status = -EINVAL;
			goto done;
		}

		status = boardobjgrp_objinsert(&ppwrmonitorobjs->pwr_channels.super,
				boardobj, obj_index);

		if (status) {
			gk20a_err(dev_from_gk20a(g),
				"unable to insert pwr topology boardobj for %d", index);
			status = -EINVAL;
			goto done;
		}

		++obj_index;
	}

done:
	gk20a_dbg_info(" done status %x", status);
	return status;
}

u32 pmgr_monitor_sw_setup(struct gk20a *g)
{
	u32 status;
	struct boardobjgrp *pboardobjgrp = NULL;
	struct pwr_channel *pchannel;
	struct pmgr_pwr_monitor *ppwrmonitorobjs;
	u8 indx = 0;

	/* Construct the Super Class and override the Interfaces */
	status = boardobjgrpconstruct_e32(
		&g->pmgr_pmu.pmgr_monitorobjs.pwr_channels);
	if (status) {
		gk20a_err(dev_from_gk20a(g),
			"error creating boardobjgrp for pmgr channel, status - 0x%x",
			status);
		goto done;
	}

	pboardobjgrp = &(g->pmgr_pmu.pmgr_monitorobjs.pwr_channels.super);

	/* Override the Interfaces */
	pboardobjgrp->pmudatainstget = _pwr_channel_pmudata_instget;

	/* Construct the Super Class and override the Interfaces */
	status = boardobjgrpconstruct_e32(
			&g->pmgr_pmu.pmgr_monitorobjs.pwr_ch_rels);
	if (status) {
		gk20a_err(dev_from_gk20a(g),
			"error creating boardobjgrp for pmgr channel relationship, status - 0x%x",
			status);
		goto done;
	}

	pboardobjgrp = &(g->pmgr_pmu.pmgr_monitorobjs.pwr_ch_rels.super);

	/* Override the Interfaces */
	pboardobjgrp->pmudatainstget = _pwr_channel_rels_pmudata_instget;

	/* Initialize the Total GPU Power Channel Mask to 0 */
	g->pmgr_pmu.pmgr_monitorobjs.pmu_data.channels.hdr.data.total_gpu_power_channel_mask = 0;
	g->pmgr_pmu.pmgr_monitorobjs.total_gpu_channel_idx =
			CTRL_PMGR_PWR_CHANNEL_INDEX_INVALID;

	/* Supported topology table version 1.0 */
	g->pmgr_pmu.pmgr_monitorobjs.b_is_topology_tbl_ver_1x = true;

	ppwrmonitorobjs = &(g->pmgr_pmu.pmgr_monitorobjs);

	status = devinit_get_pwr_topology_table(g, ppwrmonitorobjs);
	if (status)
		goto done;

	status = _pwr_channel_state_init(g);
	if (status)
		goto done;

	/* Initialise physicalChannelMask */
	g->pmgr_pmu.pmgr_monitorobjs.physical_channel_mask = 0;

	pboardobjgrp = &g->pmgr_pmu.pmgr_monitorobjs.pwr_channels.super;

	BOARDOBJGRP_FOR_EACH(pboardobjgrp, struct pwr_channel *, pchannel, indx) {
		if (_pwr_channel_implements(pchannel,
				CTRL_PMGR_PWR_CHANNEL_TYPE_SENSOR)) {
			g->pmgr_pmu.pmgr_monitorobjs.physical_channel_mask  |= BIT(indx);
		}
	}

done:
	gk20a_dbg_info(" done status %x", status);
	return status;
}
