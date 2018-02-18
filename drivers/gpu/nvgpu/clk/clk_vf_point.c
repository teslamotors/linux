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
#include "clk_vf_point.h"
#include "include/bios.h"
#include "boardobj/boardobjgrp.h"
#include "boardobj/boardobjgrp_e32.h"
#include "pmuif/gpmuifboardobj.h"
#include "pmuif/gpmuifclk.h"
#include "gm206/bios_gm206.h"
#include "ctrl/ctrlclk.h"
#include "ctrl/ctrlvolt.h"
#include "gk20a/pmu_gk20a.h"

static u32 _clk_vf_point_pmudatainit_super(struct gk20a *g, struct boardobj
	*board_obj_ptr,	struct nv_pmu_boardobj *ppmudata);

static u32 _clk_vf_points_pmudatainit(struct gk20a *g,
				      struct boardobjgrp *pboardobjgrp,
				      struct nv_pmu_boardobjgrp_super *pboardobjgrppmu)
{
	u32 status = 0;

	status = boardobjgrp_pmudatainit_e32(g, pboardobjgrp, pboardobjgrppmu);
	if (status) {
		gk20a_err(dev_from_gk20a(g),
			  "error updating pmu boardobjgrp for clk vfpoint 0x%x",
			  status);
		goto done;
	}

done:
	return status;
}

static u32 _clk_vf_points_pmudata_instget(struct gk20a *g,
					  struct nv_pmu_boardobjgrp *pmuboardobjgrp,
					  struct nv_pmu_boardobj **ppboardobjpmudata,
					  u8 idx)
{
	struct nv_pmu_clk_clk_vf_point_boardobj_grp_set  *pgrp_set =
		(struct nv_pmu_clk_clk_vf_point_boardobj_grp_set *)
		pmuboardobjgrp;

	gk20a_dbg_info("");

	/*check whether pmuboardobjgrp has a valid boardobj in index*/
	if (idx >= CTRL_BOARDOBJGRP_E255_MAX_OBJECTS)
		return -EINVAL;

	*ppboardobjpmudata = (struct nv_pmu_boardobj *)
		&pgrp_set->objects[idx].data.board_obj;
	gk20a_dbg_info(" Done");
	return 0;
}

static u32 _clk_vf_points_pmustatus_instget(struct gk20a *g,
					    void *pboardobjgrppmu,
					    struct nv_pmu_boardobj_query **ppboardobjpmustatus,
					    u8 idx)
{
	struct nv_pmu_clk_clk_vf_point_boardobj_grp_get_status *pgrp_get_status =
		(struct nv_pmu_clk_clk_vf_point_boardobj_grp_get_status *)
		pboardobjgrppmu;

	/*check whether pmuboardobjgrp has a valid boardobj in index*/
	if (idx >= CTRL_BOARDOBJGRP_E255_MAX_OBJECTS)
		return -EINVAL;

	*ppboardobjpmustatus = (struct nv_pmu_boardobj_query *)
			&pgrp_get_status->objects[idx].data.board_obj;
	return 0;
}

u32 clk_vf_point_sw_setup(struct gk20a *g)
{
	u32 status;
	struct boardobjgrp *pboardobjgrp = NULL;
	struct clk_vf_points *pclkvfpointobjs;

	gk20a_dbg_info("");

	status = boardobjgrpconstruct_e255(&g->clk_pmu.clk_vf_pointobjs.super);
	if (status) {
		gk20a_err(dev_from_gk20a(g),
		"error creating boardobjgrp for clk vfpoint, status - 0x%x",
		status);
		goto done;
	}

	pboardobjgrp = &g->clk_pmu.clk_vf_pointobjs.super.super;
	pclkvfpointobjs = &(g->clk_pmu.clk_vf_pointobjs);

	BOARDOBJGRP_PMU_CONSTRUCT(pboardobjgrp, CLK, CLK_VF_POINT);

	status = BOARDOBJGRP_PMU_CMD_GRP_SET_CONSTRUCT(g, pboardobjgrp,
			clk, CLK, clk_vf_point, CLK_VF_POINT);
	if (status) {
		gk20a_err(dev_from_gk20a(g),
			"error constructing PMU_BOARDOBJ_CMD_GRP_SET interface - 0x%x",
			status);
		goto done;
	}

	status = BOARDOBJGRP_PMU_CMD_GRP_GET_STATUS_CONSTRUCT(g,
				&g->clk_pmu.clk_vf_pointobjs.super.super,
				clk, CLK, clk_vf_point, CLK_VF_POINT);
	if (status) {
		gk20a_err(dev_from_gk20a(g),
			"error constructing PMU_BOARDOBJ_CMD_GRP_SET interface - 0x%x",
			status);
		goto done;
	}

	pboardobjgrp->pmudatainit = _clk_vf_points_pmudatainit;
	pboardobjgrp->pmudatainstget  = _clk_vf_points_pmudata_instget;
	pboardobjgrp->pmustatusinstget  = _clk_vf_points_pmustatus_instget;

done:
	gk20a_dbg_info(" done status %x", status);
	return status;
}

u32 clk_vf_point_pmu_setup(struct gk20a *g)
{
	u32 status;
	struct boardobjgrp *pboardobjgrp = NULL;

	gk20a_dbg_info("");

	pboardobjgrp = &g->clk_pmu.clk_vf_pointobjs.super.super;

	if (!pboardobjgrp->bconstructed)
		return -EINVAL;

	status = pboardobjgrp->pmuinithandle(g, pboardobjgrp);

	gk20a_dbg_info("Done");
	return status;
}

static u32 clk_vf_point_construct_super(struct gk20a *g,
					struct boardobj **ppboardobj,
					u16 size, void *pargs)
{
	struct clk_vf_point *pclkvfpoint;
	struct clk_vf_point *ptmpvfpoint =
			(struct clk_vf_point *)pargs;
	u32 status = 0;

	status = boardobj_construct_super(g, ppboardobj,
		size, pargs);
	if (status)
		return -EINVAL;

	pclkvfpoint = (struct clk_vf_point *)*ppboardobj;

	pclkvfpoint->super.pmudatainit =
			_clk_vf_point_pmudatainit_super;

	pclkvfpoint->vfe_equ_idx = ptmpvfpoint->vfe_equ_idx;
	pclkvfpoint->volt_rail_idx = ptmpvfpoint->volt_rail_idx;

	return status;
}

static u32 _clk_vf_point_pmudatainit_volt(struct gk20a *g,
					  struct boardobj *board_obj_ptr,
					  struct nv_pmu_boardobj *ppmudata)
{
	u32 status = 0;
	struct clk_vf_point_volt *pclk_vf_point_volt;
	struct nv_pmu_clk_clk_vf_point_volt_boardobj_set *pset;

	gk20a_dbg_info("");

	status = _clk_vf_point_pmudatainit_super(g, board_obj_ptr, ppmudata);
	if (status != 0)
		return status;

	pclk_vf_point_volt =
		(struct clk_vf_point_volt *)board_obj_ptr;

	pset = (struct nv_pmu_clk_clk_vf_point_volt_boardobj_set *)
		ppmudata;

	pset->source_voltage_uv = pclk_vf_point_volt->source_voltage_uv;
	pset->freq_delta_khz = pclk_vf_point_volt->freq_delta_khz;

	return status;
}

static u32 _clk_vf_point_pmudatainit_freq(struct gk20a *g,
					  struct boardobj *board_obj_ptr,
					  struct nv_pmu_boardobj *ppmudata)
{
	u32 status = 0;
	struct clk_vf_point_freq *pclk_vf_point_freq;
	struct nv_pmu_clk_clk_vf_point_freq_boardobj_set *pset;

	gk20a_dbg_info("");

	status = _clk_vf_point_pmudatainit_super(g, board_obj_ptr, ppmudata);
	if (status != 0)
		return status;

	pclk_vf_point_freq =
		(struct clk_vf_point_freq *)board_obj_ptr;

	pset = (struct nv_pmu_clk_clk_vf_point_freq_boardobj_set *)
		ppmudata;

	pset->freq_mhz =
		clkvfpointfreqmhzget(g, &pclk_vf_point_freq->super);

	pset->volt_delta_uv = pclk_vf_point_freq->volt_delta_uv;

	return status;
}

static u32 clk_vf_point_construct_volt(struct gk20a *g,
				       struct boardobj **ppboardobj,
				       u16 size, void *pargs)
{
	struct boardobj *ptmpobj = (struct boardobj *)pargs;
	struct clk_vf_point_volt *pclkvfpoint;
	struct clk_vf_point_volt *ptmpvfpoint =
			(struct clk_vf_point_volt *)pargs;
	u32 status = 0;

	if (BOARDOBJ_GET_TYPE(pargs) != CTRL_CLK_CLK_VF_POINT_TYPE_VOLT)
		return -EINVAL;

	ptmpobj->type_mask = BIT(CTRL_CLK_CLK_VF_POINT_TYPE_VOLT);
	status = clk_vf_point_construct_super(g, ppboardobj, size, pargs);
	if (status)
		return -EINVAL;

	pclkvfpoint = (struct clk_vf_point_volt *)*ppboardobj;

	pclkvfpoint->super.super.pmudatainit =
			_clk_vf_point_pmudatainit_volt;

	pclkvfpoint->source_voltage_uv = ptmpvfpoint->source_voltage_uv;

	return status;
}

static u32 clk_vf_point_construct_freq(struct gk20a *g,
				       struct boardobj **ppboardobj,
				       u16 size, void *pargs)
{
	struct boardobj *ptmpobj = (struct boardobj *)pargs;
	struct clk_vf_point_freq *pclkvfpoint;
	struct clk_vf_point_freq *ptmpvfpoint =
			(struct clk_vf_point_freq *)pargs;
	u32 status = 0;

	if (BOARDOBJ_GET_TYPE(pargs) != CTRL_CLK_CLK_VF_POINT_TYPE_FREQ)
		return -EINVAL;

	ptmpobj->type_mask = BIT(CTRL_CLK_CLK_VF_POINT_TYPE_FREQ);
	status = clk_vf_point_construct_super(g, ppboardobj, size, pargs);
	if (status)
		return -EINVAL;

	pclkvfpoint = (struct clk_vf_point_freq *)*ppboardobj;

	pclkvfpoint->super.super.pmudatainit =
			_clk_vf_point_pmudatainit_freq;

	clkvfpointfreqmhzset(g, &pclkvfpoint->super,
		clkvfpointfreqmhzget(g, &ptmpvfpoint->super));

	return status;
}

struct clk_vf_point *construct_clk_vf_point(struct gk20a *g, void *pargs)
{
	struct boardobj *board_obj_ptr = NULL;
	u32 status;

	gk20a_dbg_info("");
	switch (BOARDOBJ_GET_TYPE(pargs)) {
	case CTRL_CLK_CLK_VF_POINT_TYPE_FREQ:
		status = clk_vf_point_construct_freq(g, &board_obj_ptr,
			sizeof(struct clk_vf_point_freq), pargs);
		break;

	case CTRL_CLK_CLK_VF_POINT_TYPE_VOLT:
		status = clk_vf_point_construct_volt(g, &board_obj_ptr,
			sizeof(struct clk_vf_point_volt), pargs);
		break;

	default:
		return NULL;
	}

	if (status)
		return NULL;

	gk20a_dbg_info(" Done");

	return (struct clk_vf_point *)board_obj_ptr;
}

static u32 _clk_vf_point_pmudatainit_super(struct gk20a *g,
					   struct boardobj *board_obj_ptr,
					   struct nv_pmu_boardobj *ppmudata)
{
	u32 status = 0;
	struct clk_vf_point *pclk_vf_point;
	struct nv_pmu_clk_clk_vf_point_boardobj_set *pset;

	gk20a_dbg_info("");

	status = boardobj_pmudatainit_super(g, board_obj_ptr, ppmudata);
	if (status != 0)
		return status;

	pclk_vf_point =
		(struct clk_vf_point *)board_obj_ptr;

	pset = (struct nv_pmu_clk_clk_vf_point_boardobj_set *)
		ppmudata;


	pset->vfe_equ_idx = pclk_vf_point->vfe_equ_idx;
	pset->volt_rail_idx = pclk_vf_point->volt_rail_idx;
	return status;
}


static u32 clk_vf_point_update(struct gk20a *g,
				struct boardobj *board_obj_ptr,
				struct nv_pmu_boardobj *ppmudata)
{
	struct clk_vf_point *pclk_vf_point;
	struct nv_pmu_clk_clk_vf_point_boardobj_get_status *pstatus;

	gk20a_dbg_info("");


	pclk_vf_point =
		(struct clk_vf_point *)board_obj_ptr;

	pstatus = (struct nv_pmu_clk_clk_vf_point_boardobj_get_status *)
		ppmudata;

	if (pstatus->super.type != pclk_vf_point->super.type) {
		gk20a_err(dev_from_gk20a(g),
			"pmu data and boardobj type not matching");
		return -EINVAL;
	}
	/* now copy VF pair */
	memcpy(&pclk_vf_point->pair, &pstatus->pair,
		sizeof(struct ctrl_clk_vf_pair));
	return 0;
}

/*get latest vf point data from PMU */
u32 clk_vf_point_cache(struct gk20a *g)
{

	struct clk_vf_points *pclk_vf_points;
	struct boardobjgrp *pboardobjgrp;
	struct boardobjgrpmask *pboardobjgrpmask;
	struct nv_pmu_boardobjgrp_super *pboardobjgrppmu;
	struct boardobj *pboardobj = NULL;
	struct nv_pmu_boardobj_query *pboardobjpmustatus = NULL;
	u32 status;
	u8 index;

	gk20a_dbg_info("");
	pclk_vf_points = &g->clk_pmu.clk_vf_pointobjs;
	pboardobjgrp = &pclk_vf_points->super.super;
	pboardobjgrpmask = &pclk_vf_points->super.mask.super;

	status = pboardobjgrp->pmugetstatus(g, pboardobjgrp, pboardobjgrpmask);
	if (status) {
		gk20a_err(dev_from_gk20a(g), "err getting boardobjs from pmu");
		return status;
	}
	pboardobjgrppmu = pboardobjgrp->pmu.getstatus.buf;

	BOARDOBJGRP_FOR_EACH(pboardobjgrp, struct boardobj*, pboardobj, index) {
		status = pboardobjgrp->pmustatusinstget(g,
				(struct nv_pmu_boardobjgrp *)pboardobjgrppmu,
				&pboardobjpmustatus, index);
		if (status) {
			gk20a_err(dev_from_gk20a(g),
				"could not get status object instance");
			return status;
		}

		status = clk_vf_point_update(g, pboardobj,
			(struct nv_pmu_boardobj *)pboardobjpmustatus);
		if (status) {
			gk20a_err(dev_from_gk20a(g),
				"invalid data from pmu at %d", index);
			return status;
		}
	}

	return 0;
}
