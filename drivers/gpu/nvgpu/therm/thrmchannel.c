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
#include "thrmchannel.h"
#include "include/bios.h"
#include "boardobj/boardobjgrp.h"
#include "boardobj/boardobjgrp_e32.h"
#include "pmuif/gpmuifboardobj.h"
#include "pmuif/gpmuifthermsensor.h"
#include "gm206/bios_gm206.h"
#include "gk20a/pmu_gk20a.h"

static u32 _therm_channel_pmudatainit_device(struct gk20a *g,
			struct boardobj *board_obj_ptr,
			struct nv_pmu_boardobj *ppmudata)
{
	u32 status = 0;
	struct therm_channel *pchannel;
	struct therm_channel_device *ptherm_channel;
	struct nv_pmu_therm_therm_channel_device_boardobj_set *pset;

	status = boardobj_pmudatainit_super(g, board_obj_ptr, ppmudata);
	if (status) {
		gk20a_err(dev_from_gk20a(g),
			"error updating pmu boardobjgrp for therm channel 0x%x",
			status);
		status = -ENOMEM;
		goto done;
	}

	pchannel = (struct therm_channel *)board_obj_ptr;
	pset = (struct nv_pmu_therm_therm_channel_device_boardobj_set *)ppmudata;
	ptherm_channel = (struct therm_channel_device *)board_obj_ptr;

	pset->super.scaling = pchannel->scaling;
	pset->super.offset = pchannel->offset;
	pset->super.temp_min = pchannel->temp_min;
	pset->super.temp_max = pchannel->temp_max;

	pset->therm_dev_idx = ptherm_channel->therm_dev_idx;
	pset->therm_dev_prov_idx = ptherm_channel->therm_dev_prov_idx;

done:
	return status;
}
static struct boardobj *construct_channel_device(struct gk20a *g,
			void *pargs, u16 pargs_size, u8 type)
{
	struct boardobj *board_obj_ptr = NULL;
	struct therm_channel *pchannel;
	struct therm_channel_device *pchannel_device;
	u32 status;
	struct therm_channel_device *therm_device = (struct therm_channel_device*)pargs;

	status = boardobj_construct_super(g, &board_obj_ptr,
		pargs_size, pargs);
	if (status)
		return NULL;

	/* Set Super class interfaces */
	board_obj_ptr->pmudatainit = _therm_channel_pmudatainit_device;

	pchannel = (struct therm_channel *)board_obj_ptr;
	pchannel_device = (struct therm_channel_device *)board_obj_ptr;

	g->ops.therm.get_internal_sensor_limits(&pchannel->temp_max,
		&pchannel->temp_min);
	pchannel->scaling = (1 << 8);
	pchannel->offset = 0;

	pchannel_device->therm_dev_idx = therm_device->therm_dev_idx;
	pchannel_device->therm_dev_prov_idx = therm_device->therm_dev_prov_idx;

	gk20a_dbg_info(" Done");

	return board_obj_ptr;
}

static u32 _therm_channel_pmudata_instget(struct gk20a *g,
			struct nv_pmu_boardobjgrp *pmuboardobjgrp,
			struct nv_pmu_boardobj **ppboardobjpmudata,
			u8 idx)
{
	struct nv_pmu_therm_therm_channel_boardobj_grp_set *pgrp_set =
		(struct nv_pmu_therm_therm_channel_boardobj_grp_set *)
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

static u32 devinit_get_therm_channel_table(struct gk20a *g,
				struct therm_channels *pthermchannelobjs)
{
	u32 status = 0;
	u8 *therm_channel_table_ptr = NULL;
	u8 *curr_therm_channel_table_ptr = NULL;
	struct boardobj *boardobj;
	struct therm_channel_1x_header therm_channel_table_header = { 0 };
	struct therm_channel_1x_entry *therm_channel_table_entry = NULL;
	u32 index;
	u32 obj_index = 0;
	u16 therm_channel_size = 0;
	union {
		struct boardobj boardobj;
		struct therm_channel therm_channel;
		struct therm_channel_device device;
	} therm_channel_data;

	gk20a_dbg_info("");

	if (g->ops.bios.get_perf_table_ptrs) {
		therm_channel_table_ptr = (u8 *)g->ops.bios.get_perf_table_ptrs(g,
				g->bios.perf_token, THERMAL_CHANNEL_TABLE);
		if (therm_channel_table_ptr == NULL) {
			status = -EINVAL;
			goto done;
		}
	}

	memcpy(&therm_channel_table_header, therm_channel_table_ptr,
		VBIOS_THERM_CHANNEL_1X_HEADER_SIZE_09);

	if (therm_channel_table_header.version !=
			VBIOS_THERM_CHANNEL_VERSION_1X) {
		status = -EINVAL;
		goto done;
	}

	if (therm_channel_table_header.header_size <
			VBIOS_THERM_CHANNEL_1X_HEADER_SIZE_09) {
		status = -EINVAL;
		goto done;
	}

	curr_therm_channel_table_ptr = (therm_channel_table_ptr +
		VBIOS_THERM_CHANNEL_1X_HEADER_SIZE_09);

	for (index = 0; index < therm_channel_table_header.num_table_entries;
		index++) {
		therm_channel_table_entry = (struct therm_channel_1x_entry *)
			(curr_therm_channel_table_ptr +
				(therm_channel_table_header.table_entry_size * index));

		if (therm_channel_table_entry->class_id !=
				NV_VBIOS_THERM_CHANNEL_1X_ENTRY_CLASS_DEVICE) {
			continue;
		}

		therm_channel_data.device.therm_dev_idx = therm_channel_table_entry->param0;
		therm_channel_data.device.therm_dev_prov_idx = therm_channel_table_entry->param1;

		therm_channel_size = sizeof(struct therm_channel_device);
		therm_channel_data.boardobj.type = CTRL_THERMAL_THERM_CHANNEL_CLASS_DEVICE;

		boardobj = construct_channel_device(g, &therm_channel_data,
					therm_channel_size, therm_channel_data.boardobj.type);

		if (!boardobj) {
			gk20a_err(dev_from_gk20a(g),
				"unable to create thermal device for %d type %d",
				index, therm_channel_data.boardobj.type);
			status = -EINVAL;
			goto done;
		}

		status = boardobjgrp_objinsert(&pthermchannelobjs->super.super,
				boardobj, obj_index);

		if (status) {
			gk20a_err(dev_from_gk20a(g),
			"unable to insert thermal device boardobj for %d", index);
			status = -EINVAL;
			goto done;
		}

		++obj_index;
	}

done:
	gk20a_dbg_info(" done status %x", status);
	return status;
}

u32 therm_channel_sw_setup(struct gk20a *g)
{
	u32 status;
	struct boardobjgrp *pboardobjgrp = NULL;
	struct therm_channels *pthermchannelobjs;

	/* Construct the Super Class and override the Interfaces */
	status = boardobjgrpconstruct_e32(&g->therm_pmu.therm_channelobjs.super);
	if (status) {
		gk20a_err(dev_from_gk20a(g),
			  "error creating boardobjgrp for therm devices, status - 0x%x",
			  status);
		goto done;
	}

	pboardobjgrp = &g->therm_pmu.therm_channelobjs.super.super;
	pthermchannelobjs = &(g->therm_pmu.therm_channelobjs);

	/* Override the Interfaces */
	pboardobjgrp->pmudatainstget = _therm_channel_pmudata_instget;

	status = devinit_get_therm_channel_table(g, pthermchannelobjs);
	if (status)
		goto done;

	BOARDOBJGRP_PMU_CONSTRUCT(pboardobjgrp, THERM, THERM_CHANNEL);

	status = BOARDOBJGRP_PMU_CMD_GRP_SET_CONSTRUCT(g, pboardobjgrp,
			therm, THERM, therm_channel, THERM_CHANNEL);
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
