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
#include "thrmdev.h"
#include "include/bios.h"
#include "boardobj/boardobjgrp.h"
#include "boardobj/boardobjgrp_e32.h"
#include "pmuif/gpmuifboardobj.h"
#include "pmuif/gpmuifthermsensor.h"
#include "gm206/bios_gm206.h"
#include "gk20a/pmu_gk20a.h"
#include "ctrl/ctrltherm.h"

static struct boardobj *construct_therm_device(struct gk20a *g,
			void *pargs, u16 pargs_size, u8 type)
{
	struct boardobj *board_obj_ptr = NULL;
	u32 status;

	status = boardobj_construct_super(g, &board_obj_ptr,
		pargs_size, pargs);
	if (status)
		return NULL;

	gk20a_dbg_info(" Done");

	return board_obj_ptr;
}

static u32 _therm_device_pmudata_instget(struct gk20a *g,
			struct nv_pmu_boardobjgrp *pmuboardobjgrp,
			struct nv_pmu_boardobj **ppboardobjpmudata,
			u8 idx)
{
	struct nv_pmu_therm_therm_device_boardobj_grp_set *pgrp_set =
		(struct nv_pmu_therm_therm_device_boardobj_grp_set *)
		pmuboardobjgrp;

	gk20a_dbg_info("");

	/*check whether pmuboardobjgrp has a valid boardobj in index*/
	if (((u32)BIT(idx) &
			pgrp_set->hdr.data.super.obj_mask.super.data[0]) == 0)
		return -EINVAL;

	*ppboardobjpmudata = (struct nv_pmu_boardobj *)
		&pgrp_set->objects[idx].data;

	gk20a_dbg_info(" Done");

	return 0;
}

static u32 devinit_get_therm_device_table(struct gk20a *g,
				struct therm_devices *pthermdeviceobjs)
{
	u32 status = 0;
	u8 *therm_device_table_ptr = NULL;
	u8 *curr_therm_device_table_ptr = NULL;
	struct boardobj *boardobj;
	struct therm_device_1x_header therm_device_table_header = { 0 };
	struct therm_device_1x_entry *therm_device_table_entry = NULL;
	u32 index;
	u32 obj_index = 0;
	u16 therm_device_size = 0;
	union {
		struct boardobj boardobj;
		struct therm_device therm_device;
	} therm_device_data;

	gk20a_dbg_info("");

	if (g->ops.bios.get_perf_table_ptrs) {
		therm_device_table_ptr = (u8 *)g->ops.bios.get_perf_table_ptrs(g,
				g->bios.perf_token, THERMAL_DEVICE_TABLE);
		if (therm_device_table_ptr == NULL) {
			status = -EINVAL;
			goto done;
		}
	}

	memcpy(&therm_device_table_header, therm_device_table_ptr,
		VBIOS_THERM_DEVICE_1X_HEADER_SIZE_04);

	if (therm_device_table_header.version !=
			VBIOS_THERM_DEVICE_VERSION_1X) {
		status = -EINVAL;
		goto done;
	}

	if (therm_device_table_header.header_size <
			VBIOS_THERM_DEVICE_1X_HEADER_SIZE_04) {
		status = -EINVAL;
		goto done;
	}

	curr_therm_device_table_ptr = (therm_device_table_ptr +
		VBIOS_THERM_DEVICE_1X_HEADER_SIZE_04);

	for (index = 0; index < therm_device_table_header.num_table_entries;
		index++) {
		therm_device_table_entry = (struct therm_device_1x_entry *)
			(curr_therm_device_table_ptr +
				(therm_device_table_header.table_entry_size * index));

		if (therm_device_table_entry->class_id !=
				NV_VBIOS_THERM_DEVICE_1X_ENTRY_CLASS_GPU) {
			continue;
		}

		therm_device_size = sizeof(struct therm_device);
		therm_device_data.boardobj.type = CTRL_THERMAL_THERM_DEVICE_CLASS_GPU;

		boardobj = construct_therm_device(g, &therm_device_data,
					therm_device_size, therm_device_data.boardobj.type);

		if (!boardobj) {
			gk20a_err(dev_from_gk20a(g),
				"unable to create thermal device for %d type %d",
				index, therm_device_data.boardobj.type);
			status = -EINVAL;
			goto done;
		}

		status = boardobjgrp_objinsert(&pthermdeviceobjs->super.super,
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

u32 therm_device_sw_setup(struct gk20a *g)
{
	u32 status;
	struct boardobjgrp *pboardobjgrp = NULL;
	struct therm_devices *pthermdeviceobjs;

	/* Construct the Super Class and override the Interfaces */
	status = boardobjgrpconstruct_e32(&g->therm_pmu.therm_deviceobjs.super);
	if (status) {
		gk20a_err(dev_from_gk20a(g),
			  "error creating boardobjgrp for therm devices, status - 0x%x",
			  status);
		goto done;
	}

	pboardobjgrp = &g->therm_pmu.therm_deviceobjs.super.super;
	pthermdeviceobjs = &(g->therm_pmu.therm_deviceobjs);

	/* Override the Interfaces */
	pboardobjgrp->pmudatainstget = _therm_device_pmudata_instget;

	status = devinit_get_therm_device_table(g, pthermdeviceobjs);
	if (status)
		goto done;

	BOARDOBJGRP_PMU_CONSTRUCT(pboardobjgrp, THERM, THERM_DEVICE);

	status = BOARDOBJGRP_PMU_CMD_GRP_SET_CONSTRUCT(g, pboardobjgrp,
			therm, THERM, therm_device, THERM_DEVICE);
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
