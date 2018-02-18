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
#include "clk_prog.h"
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

static struct clk_prog *construct_clk_prog(struct gk20a *g, void *pargs);
static u32 devinit_get_clk_prog_table(struct gk20a *g,
	struct clk_progs *pprogobjs);
static vf_flatten vfflatten_prog_1x_master;
static vf_lookup vflookup_prog_1x_master;
static get_fpoints getfpoints_prog_1x_master;
static get_slaveclk getslaveclk_prog_1x_master;

static u32 _clk_progs_pmudatainit(struct gk20a *g,
				  struct boardobjgrp *pboardobjgrp,
				  struct nv_pmu_boardobjgrp_super *pboardobjgrppmu)
{
	struct nv_pmu_clk_clk_prog_boardobjgrp_set_header *pset =
		(struct nv_pmu_clk_clk_prog_boardobjgrp_set_header *)
		pboardobjgrppmu;
	struct clk_progs *pprogs = (struct clk_progs *)pboardobjgrp;
	u32 status = 0;

	status = boardobjgrp_pmudatainit_e32(g, pboardobjgrp, pboardobjgrppmu);
	if (status) {
		gk20a_err(dev_from_gk20a(g),
			  "error updating pmu boardobjgrp for clk prog 0x%x",
			  status);
		goto done;
	}
	pset->slave_entry_count = pprogs->slave_entry_count;
	pset->vf_entry_count = pprogs->vf_entry_count;

done:
	return status;
}

static u32 _clk_progs_pmudata_instget(struct gk20a *g,
				      struct nv_pmu_boardobjgrp *pmuboardobjgrp,
				      struct nv_pmu_boardobj **ppboardobjpmudata,
				      u8 idx)
{
	struct nv_pmu_clk_clk_prog_boardobj_grp_set  *pgrp_set =
		(struct nv_pmu_clk_clk_prog_boardobj_grp_set *)pmuboardobjgrp;

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

u32 clk_prog_sw_setup(struct gk20a *g)
{
	u32 status;
	struct boardobjgrp *pboardobjgrp = NULL;
	struct clk_progs *pclkprogobjs;

	gk20a_dbg_info("");

	status = boardobjgrpconstruct_e255(&g->clk_pmu.clk_progobjs.super);
	if (status) {
		gk20a_err(dev_from_gk20a(g),
			  "error creating boardobjgrp for clk prog, status - 0x%x",
			  status);
		goto done;
	}

	pboardobjgrp = &g->clk_pmu.clk_progobjs.super.super;
	pclkprogobjs = &(g->clk_pmu.clk_progobjs);

	BOARDOBJGRP_PMU_CONSTRUCT(pboardobjgrp, CLK, CLK_PROG);

	status = BOARDOBJGRP_PMU_CMD_GRP_SET_CONSTRUCT(g, pboardobjgrp,
			clk, CLK, clk_prog, CLK_PROG);
	if (status) {
		gk20a_err(dev_from_gk20a(g),
			"error constructing PMU_BOARDOBJ_CMD_GRP_SET interface - 0x%x",
			status);
		goto done;
	}

	pboardobjgrp->pmudatainit = _clk_progs_pmudatainit;
	pboardobjgrp->pmudatainstget  = _clk_progs_pmudata_instget;

	status = devinit_get_clk_prog_table(g, pclkprogobjs);
	if (status)
		goto done;

	status = clk_domain_clk_prog_link(g, &g->clk_pmu);
	if (status) {
		gk20a_err(dev_from_gk20a(g),
			  "error constructing VF point board objects");
		goto done;
	}


done:
	gk20a_dbg_info(" done status %x", status);
	return status;
}

u32 clk_prog_pmu_setup(struct gk20a *g)
{
	u32 status;
	struct boardobjgrp *pboardobjgrp = NULL;

	gk20a_dbg_info("");

	pboardobjgrp = &g->clk_pmu.clk_progobjs.super.super;

	if (!pboardobjgrp->bconstructed)
		return -EINVAL;

	status = pboardobjgrp->pmuinithandle(g, pboardobjgrp);

	gk20a_dbg_info("Done");
	return status;
}

static u32 devinit_get_clk_prog_table(struct gk20a *g,
				      struct clk_progs *pclkprogobjs)
{
	u32 status = 0;
	u8 *clkprogs_tbl_ptr = NULL;
	struct vbios_clock_programming_table_1x_header header = { 0 };
	struct vbios_clock_programming_table_1x_entry prog = { 0 };
	struct vbios_clock_programming_table_1x_slave_entry slaveprog = { 0 };
	struct vbios_clock_programming_table_1x_vf_entry vfprog = { 0 };
	u8 *entry = NULL;
	u8 *slaveentry = NULL;
	u8 *vfentry = NULL;
	u32 i, j = 0;
	struct clk_prog *pprog;
	u8 prog_type;
	u32 szfmt = VBIOS_CLOCK_PROGRAMMING_TABLE_1X_ENTRY_SIZE_0D;
	u32 hszfmt = VBIOS_CLOCK_PROGRAMMING_TABLE_1X_HEADER_SIZE_08;
	u32 slaveszfmt = VBIOS_CLOCK_PROGRAMMING_TABLE_1X_SLAVE_ENTRY_SIZE_03;
	u32 vfszfmt = VBIOS_CLOCK_PROGRAMMING_TABLE_1X_VF_ENTRY_SIZE_02;
	struct ctrl_clk_clk_prog_1x_master_vf_entry
		vfentries[CTRL_CLK_CLK_PROG_1X_MASTER_VF_ENTRY_MAX_ENTRIES];
	struct ctrl_clk_clk_prog_1x_master_ratio_slave_entry
		ratioslaveentries[CTRL_CLK_PROG_1X_MASTER_MAX_SLAVE_ENTRIES];
	struct ctrl_clk_clk_prog_1x_master_table_slave_entry
		tableslaveentries[CTRL_CLK_PROG_1X_MASTER_MAX_SLAVE_ENTRIES];
	union {
		struct boardobj board_obj;
		struct clk_prog clkprog;
		struct clk_prog_1x v1x;
		struct clk_prog_1x_master v1x_master;
		struct clk_prog_1x_master_ratio v1x_master_ratio;
		struct clk_prog_1x_master_table v1x_master_table;
	} prog_data;

	gk20a_dbg_info("");

	if (g->ops.bios.get_perf_table_ptrs) {
		clkprogs_tbl_ptr = (u8 *)g->ops.bios.get_perf_table_ptrs(g,
				g->bios.clock_token, CLOCK_PROGRAMMING_TABLE);
		if (clkprogs_tbl_ptr == NULL) {
			status = -EINVAL;
			goto done;
		}
	}

	memcpy(&header, clkprogs_tbl_ptr, hszfmt);
	if (header.header_size < hszfmt) {
		status = -EINVAL;
		goto done;
	}
	hszfmt = header.header_size;

	if (header.entry_size <= VBIOS_CLOCK_PROGRAMMING_TABLE_1X_ENTRY_SIZE_05)
		szfmt = header.entry_size;
	else if (header.entry_size <= VBIOS_CLOCK_PROGRAMMING_TABLE_1X_ENTRY_SIZE_0D)
		szfmt = header.entry_size;
	else {
		status = -EINVAL;
		goto done;
	}

	if (header.vf_entry_size < vfszfmt) {
		status = -EINVAL;
		goto done;
	}
	vfszfmt = header.vf_entry_size;
	if (header.slave_entry_size < slaveszfmt) {
		status = -EINVAL;
		goto done;
	}
	slaveszfmt = header.slave_entry_size;
	if (header.vf_entry_count > CTRL_CLK_CLK_DELTA_MAX_VOLT_RAILS) {
		status = -EINVAL;
		goto done;
	}

	pclkprogobjs->slave_entry_count = header.slave_entry_count;
	pclkprogobjs->vf_entry_count = header.vf_entry_count;

	for (i = 0; i < header.entry_count; i++) {
		memset(&prog_data, 0x0, (u32)sizeof(prog_data));

		/* Read table entries*/
		entry = clkprogs_tbl_ptr + hszfmt +
			(i * (szfmt + (header.slave_entry_count * slaveszfmt) +
			(header.vf_entry_count * vfszfmt)));

		memcpy(&prog, entry, szfmt);
		memset(vfentries, 0xFF,
			sizeof(struct ctrl_clk_clk_prog_1x_master_vf_entry) *
			CTRL_CLK_CLK_PROG_1X_MASTER_VF_ENTRY_MAX_ENTRIES);
		memset(ratioslaveentries, 0xFF,
			sizeof(struct ctrl_clk_clk_prog_1x_master_ratio_slave_entry) *
			CTRL_CLK_PROG_1X_MASTER_MAX_SLAVE_ENTRIES);
		memset(tableslaveentries, 0xFF,
			sizeof(struct ctrl_clk_clk_prog_1x_master_table_slave_entry) *
			CTRL_CLK_PROG_1X_MASTER_MAX_SLAVE_ENTRIES);
		prog_type = (u8)BIOS_GET_FIELD(prog.flags0,
					       NV_VBIOS_CLOCK_PROGRAMMING_TABLE_1X_ENTRY_FLAGS0_SOURCE);

		switch (prog_type) {
		case NV_VBIOS_CLOCK_PROGRAMMING_TABLE_1X_ENTRY_FLAGS0_SOURCE_PLL:
			prog_data.v1x.source = CTRL_CLK_PROG_1X_SOURCE_PLL;
			prog_data.v1x.source_data.pll.pll_idx =
				(u8)BIOS_GET_FIELD(prog.param0,
					NV_VBIOS_CLOCK_PROGRAMMING_TABLE_1X_ENTRY_PARAM0_PLL_PLL_INDEX);
			prog_data.v1x.source_data.pll.freq_step_size_mhz =
				(u8)BIOS_GET_FIELD(prog.param1,
					NV_VBIOS_CLOCK_PROGRAMMING_TABLE_1X_ENTRY_PARAM1_PLL_FREQ_STEP_SIZE);
			break;

		case NV_VBIOS_CLOCK_PROGRAMMING_TABLE_1X_ENTRY_FLAGS0_SOURCE_ONE_SOURCE:
			prog_data.v1x.source = CTRL_CLK_PROG_1X_SOURCE_ONE_SOURCE;
			break;

		case NV_VBIOS_CLOCK_PROGRAMMING_TABLE_1X_ENTRY_FLAGS0_SOURCE_FLL:
			prog_data.v1x.source = CTRL_CLK_PROG_1X_SOURCE_FLL;
			break;

		default:
			gk20a_err(dev_from_gk20a(g),
				  "invalid source %d", prog_type);
			status = -EINVAL;
			goto done;
		}

		prog_data.v1x.freq_max_mhz = (u16)prog.freq_max_mhz;

		prog_type = (u8)BIOS_GET_FIELD(prog.flags0,
			NV_VBIOS_CLOCK_PROGRAMMING_TABLE_1X_ENTRY_FLAGS0_TYPE);

		vfentry = entry + szfmt +
			header.slave_entry_count * slaveszfmt;
		slaveentry = entry + szfmt;
		switch (prog_type) {
		case NV_VBIOS_CLOCK_PROGRAMMING_TABLE_1X_ENTRY_FLAGS0_TYPE_MASTER_RATIO:
		case NV_VBIOS_CLOCK_PROGRAMMING_TABLE_1X_ENTRY_FLAGS0_TYPE_MASTER_TABLE:
			prog_data.v1x_master.b_o_c_o_v_enabled = false;
			for (j = 0; j < header.vf_entry_count; j++) {
				memcpy(&vfprog, vfentry, vfszfmt);

				vfentries[j].vfe_idx = (u8)vfprog.vfe_idx;
				if (CTRL_CLK_PROG_1X_SOURCE_FLL ==
					prog_data.v1x.source) {
					vfentries[j].gain_vfe_idx = (u8)BIOS_GET_FIELD(
						vfprog.param0,
						NV_VBIOS_CLOCK_PROGRAMMING_TABLE_1X_VF_ENTRY_PARAM0_FLL_GAIN_VFE_IDX);
				} else {
					vfentries[j].gain_vfe_idx = CTRL_BOARDOBJ_IDX_INVALID;
				}
				vfentry += vfszfmt;
			}

			prog_data.v1x_master.p_vf_entries = vfentries;

			for (j = 0; j < header.slave_entry_count; j++) {
				memcpy(&slaveprog, slaveentry, slaveszfmt);

				switch (prog_type) {
				case NV_VBIOS_CLOCK_PROGRAMMING_TABLE_1X_ENTRY_FLAGS0_TYPE_MASTER_RATIO:
					ratioslaveentries[j].clk_dom_idx =
						(u8)slaveprog.clk_dom_idx;
					ratioslaveentries[j].ratio = (u8)
					BIOS_GET_FIELD(slaveprog.param0,
					NV_VBIOS_CLOCK_PROGRAMMING_TABLE_1X_SLAVE_ENTRY_PARAM0_MASTER_RATIO_RATIO);
					break;

				case NV_VBIOS_CLOCK_PROGRAMMING_TABLE_1X_ENTRY_FLAGS0_TYPE_MASTER_TABLE:
					tableslaveentries[j].clk_dom_idx =
						(u8)slaveprog.clk_dom_idx;
					tableslaveentries[j].freq_mhz =
						(u16)BIOS_GET_FIELD(slaveprog.param0,
							NV_VBIOS_CLOCK_PROGRAMMING_TABLE_1X_SLAVE_ENTRY_PARAM0_MASTER_TABLE_FREQ);
					break;
				}
				slaveentry += slaveszfmt;
			}

			switch (prog_type) {
			case NV_VBIOS_CLOCK_PROGRAMMING_TABLE_1X_ENTRY_FLAGS0_TYPE_MASTER_RATIO:
				prog_data.board_obj.type = CTRL_CLK_CLK_PROG_TYPE_1X_MASTER_RATIO;
				prog_data.v1x_master_ratio.p_slave_entries =
					ratioslaveentries;
				break;

			case NV_VBIOS_CLOCK_PROGRAMMING_TABLE_1X_ENTRY_FLAGS0_TYPE_MASTER_TABLE:
				prog_data.board_obj.type = CTRL_CLK_CLK_PROG_TYPE_1X_MASTER_TABLE;

				prog_data.v1x_master_table.p_slave_entries =
					tableslaveentries;
				break;

			}
			break;

		case NV_VBIOS_CLOCK_PROGRAMMING_TABLE_1X_ENTRY_FLAGS0_TYPE_SLAVE:
			prog_data.board_obj.type = CTRL_CLK_CLK_PROG_TYPE_1X;
			break;


		default:
			gk20a_err(dev_from_gk20a(g),
				  "source issue %d", prog_type);
				  status = -EINVAL;
			goto done;
		}

		pprog = construct_clk_prog(g, (void *)&prog_data);
		if (pprog == NULL) {
			gk20a_err(dev_from_gk20a(g),
			"error constructing clk_prog boardobj %d", i);
			status = -EINVAL;
			goto done;
		}

		status = boardobjgrp_objinsert(&pclkprogobjs->super.super,
			(struct boardobj *)pprog, i);
		if (status) {
			gk20a_err(dev_from_gk20a(g),
				  "error adding clk_prog boardobj %d", i);
			status = -EINVAL;
			goto done;
		}
	}
done:
	gk20a_dbg_info(" done status %x", status);
	return status;
}

static u32 _clk_prog_pmudatainit_super(struct gk20a *g,
				       struct boardobj *board_obj_ptr,
				       struct nv_pmu_boardobj *ppmudata)
{
	u32 status = 0;

	gk20a_dbg_info("");

	status = boardobj_pmudatainit_super(g, board_obj_ptr, ppmudata);
	return status;
}

static u32 _clk_prog_pmudatainit_1x(struct gk20a *g,
				    struct boardobj *board_obj_ptr,
				    struct nv_pmu_boardobj *ppmudata)
{
	u32 status = 0;
	struct clk_prog_1x *pclk_prog_1x;
	struct nv_pmu_clk_clk_prog_1x_boardobj_set *pset;

	gk20a_dbg_info("");

	status = _clk_prog_pmudatainit_super(g, board_obj_ptr, ppmudata);
	if (status != 0)
		return status;

	pclk_prog_1x = (struct clk_prog_1x *)board_obj_ptr;

	pset = (struct nv_pmu_clk_clk_prog_1x_boardobj_set *)
		ppmudata;

	pset->source = pclk_prog_1x->source;
	pset->freq_max_mhz = pclk_prog_1x->freq_max_mhz;
	pset->source_data = pclk_prog_1x->source_data;

	return status;
}

static u32 _clk_prog_pmudatainit_1x_master(struct gk20a *g,
					   struct boardobj *board_obj_ptr,
					   struct nv_pmu_boardobj *ppmudata)
{
	u32 status = 0;
	struct clk_prog_1x_master *pclk_prog_1x_master;
	struct nv_pmu_clk_clk_prog_1x_master_boardobj_set *pset;
	u32 vfsize = sizeof(struct ctrl_clk_clk_prog_1x_master_vf_entry) *
		g->clk_pmu.clk_progobjs.vf_entry_count;

	gk20a_dbg_info("");

	status = _clk_prog_pmudatainit_1x(g, board_obj_ptr, ppmudata);

	pclk_prog_1x_master =
		(struct clk_prog_1x_master *)board_obj_ptr;

	pset = (struct nv_pmu_clk_clk_prog_1x_master_boardobj_set *)
		ppmudata;

	memcpy(pset->vf_entries, pclk_prog_1x_master->p_vf_entries, vfsize);

	pset->b_o_c_o_v_enabled = pclk_prog_1x_master->b_o_c_o_v_enabled;
	pset->source_data = pclk_prog_1x_master->source_data;

	memcpy(&pset->deltas, &pclk_prog_1x_master->deltas,
		(u32) sizeof(struct ctrl_clk_clk_delta));

	return status;
}

static u32 _clk_prog_pmudatainit_1x_master_ratio(struct gk20a *g,
						 struct boardobj *board_obj_ptr,
						 struct nv_pmu_boardobj *ppmudata)
{
	u32 status = 0;
	struct clk_prog_1x_master_ratio *pclk_prog_1x_master_ratio;
	struct nv_pmu_clk_clk_prog_1x_master_ratio_boardobj_set *pset;
	u32 slavesize = sizeof(struct ctrl_clk_clk_prog_1x_master_ratio_slave_entry) *
		g->clk_pmu.clk_progobjs.slave_entry_count;

	gk20a_dbg_info("");

	status = _clk_prog_pmudatainit_1x_master(g, board_obj_ptr, ppmudata);
	if (status != 0)
		return status;

	pclk_prog_1x_master_ratio =
		(struct clk_prog_1x_master_ratio *)board_obj_ptr;

	pset = (struct nv_pmu_clk_clk_prog_1x_master_ratio_boardobj_set *)
		ppmudata;

	memcpy(pset->slave_entries,
		pclk_prog_1x_master_ratio->p_slave_entries, slavesize);

	return status;
}

static u32 _clk_prog_pmudatainit_1x_master_table(struct gk20a *g,
						 struct boardobj *board_obj_ptr,
						 struct nv_pmu_boardobj *ppmudata)
{
	u32 status = 0;
	struct clk_prog_1x_master_table *pclk_prog_1x_master_table;
	struct nv_pmu_clk_clk_prog_1x_master_table_boardobj_set *pset;
	u32 slavesize = sizeof(struct ctrl_clk_clk_prog_1x_master_ratio_slave_entry) *
		g->clk_pmu.clk_progobjs.slave_entry_count;

	gk20a_dbg_info("");

	status = _clk_prog_pmudatainit_1x_master(g, board_obj_ptr, ppmudata);
	if (status != 0)
		return status;

	pclk_prog_1x_master_table =
		(struct clk_prog_1x_master_table *)board_obj_ptr;

	pset = (struct nv_pmu_clk_clk_prog_1x_master_table_boardobj_set *)
		ppmudata;
	memcpy(pset->slave_entries,
		pclk_prog_1x_master_table->p_slave_entries, slavesize);

	return status;
}

static u32 _clk_prog_1x_master_rail_construct_vf_point(struct gk20a *g,
						       struct clk_pmupstate *pclk,
						       struct clk_prog_1x_master *p1xmaster,
						       struct ctrl_clk_clk_prog_1x_master_vf_entry *p_vf_rail,
						       struct clk_vf_point *p_vf_point_tmp,
						       u8 *p_vf_point_idx)
{
	struct clk_vf_point *p_vf_point;
	u32 status;

	gk20a_dbg_info("");

	p_vf_point = construct_clk_vf_point(g, (void *)p_vf_point_tmp);
	if (p_vf_point == NULL) {
		status = -ENOMEM;
		goto done;
	}
	status = pclk->clk_vf_pointobjs.super.super.objinsert(
				&pclk->clk_vf_pointobjs.super.super,
				&p_vf_point->super,
				*p_vf_point_idx);
	if (status)
		goto done;

	p_vf_rail->vf_point_idx_last = (*p_vf_point_idx)++;

done:
	gk20a_dbg_info("done status %x", status);
	return status;
}

static u32 clk_prog_construct_super(struct gk20a *g,
				    struct boardobj **ppboardobj,
				    u16 size, void *pargs)
{
	struct clk_prog *pclkprog;
	u32 status = 0;

	status = boardobj_construct_super(g, ppboardobj,
		size, pargs);
	if (status)
		return -EINVAL;

	pclkprog = (struct clk_prog *)*ppboardobj;

	pclkprog->super.pmudatainit =
			_clk_prog_pmudatainit_super;
	return status;
}


static u32 clk_prog_construct_1x(struct gk20a *g,
				 struct boardobj **ppboardobj,
				 u16 size, void *pargs)
{
	struct boardobj *ptmpobj = (struct boardobj *)pargs;
	struct clk_prog_1x *pclkprog;
	struct clk_prog_1x *ptmpprog =
			(struct clk_prog_1x *)pargs;
	u32 status = 0;

	gk20a_dbg_info(" ");
	ptmpobj->type_mask |= BIT(CTRL_CLK_CLK_PROG_TYPE_1X);
	status = clk_prog_construct_super(g, ppboardobj, size, pargs);
	if (status)
		return -EINVAL;

	pclkprog = (struct clk_prog_1x *)*ppboardobj;

	pclkprog->super.super.pmudatainit =
			_clk_prog_pmudatainit_1x;

	pclkprog->source = ptmpprog->source;
	pclkprog->freq_max_mhz = ptmpprog->freq_max_mhz;
	pclkprog->source_data = ptmpprog->source_data;

	return status;
}

static u32 clk_prog_construct_1x_master(struct gk20a *g,
					struct boardobj **ppboardobj,
					u16 size, void *pargs)
{
	struct boardobj *ptmpobj = (struct boardobj *)pargs;
	struct clk_prog_1x_master *pclkprog;
	struct clk_prog_1x_master *ptmpprog =
			(struct clk_prog_1x_master *)pargs;
	u32 status = 0;
	u32 vfsize = sizeof(struct ctrl_clk_clk_prog_1x_master_vf_entry) *
		g->clk_pmu.clk_progobjs.vf_entry_count;
	u8 railidx;

	gk20a_dbg_info(" type - %x", BOARDOBJ_GET_TYPE(pargs));

	ptmpobj->type_mask |= BIT(CTRL_CLK_CLK_PROG_TYPE_1X_MASTER);
	status = clk_prog_construct_1x(g, ppboardobj, size, pargs);
	if (status)
		return -EINVAL;

	pclkprog = (struct clk_prog_1x_master *)*ppboardobj;

	pclkprog->super.super.super.pmudatainit =
			_clk_prog_pmudatainit_1x_master;

	pclkprog->vfflatten =
			vfflatten_prog_1x_master;

	pclkprog->vflookup =
			vflookup_prog_1x_master;

	pclkprog->getfpoints =
			getfpoints_prog_1x_master;

	pclkprog->getslaveclk =
			getslaveclk_prog_1x_master;

	pclkprog->p_vf_entries = (struct ctrl_clk_clk_prog_1x_master_vf_entry *)
		kzalloc(vfsize, GFP_KERNEL);

	memcpy(pclkprog->p_vf_entries, ptmpprog->p_vf_entries, vfsize);

	pclkprog->b_o_c_o_v_enabled = ptmpprog->b_o_c_o_v_enabled;

	for (railidx = 0;
	     railidx < g->clk_pmu.clk_progobjs.vf_entry_count;
	     railidx++) {
		pclkprog->p_vf_entries[railidx].vf_point_idx_first =
			CTRL_CLK_CLK_VF_POINT_IDX_INVALID;
		pclkprog->p_vf_entries[railidx].vf_point_idx_last =
			CTRL_CLK_CLK_VF_POINT_IDX_INVALID;
	}

	return status;
}

static u32 clk_prog_construct_1x_master_ratio(struct gk20a *g,
					      struct boardobj **ppboardobj,
					      u16 size, void *pargs)
{
	struct boardobj *ptmpobj = (struct boardobj *)pargs;
	struct clk_prog_1x_master_ratio *pclkprog;
	struct clk_prog_1x_master_ratio *ptmpprog =
			(struct clk_prog_1x_master_ratio *)pargs;
	u32 status = 0;
	u32 slavesize = sizeof(struct ctrl_clk_clk_prog_1x_master_ratio_slave_entry) *
		g->clk_pmu.clk_progobjs.slave_entry_count;

	if (BOARDOBJ_GET_TYPE(pargs) != CTRL_CLK_CLK_PROG_TYPE_1X_MASTER_RATIO)
		return -EINVAL;

	ptmpobj->type_mask |= BIT(CTRL_CLK_CLK_PROG_TYPE_1X_MASTER_RATIO);
	status = clk_prog_construct_1x_master(g, ppboardobj, size, pargs);
	if (status)
		return -EINVAL;

	pclkprog = (struct clk_prog_1x_master_ratio *)*ppboardobj;

	pclkprog->super.super.super.super.pmudatainit =
			_clk_prog_pmudatainit_1x_master_ratio;

	pclkprog->p_slave_entries =
		(struct ctrl_clk_clk_prog_1x_master_ratio_slave_entry *)
		kzalloc(slavesize, GFP_KERNEL);
	if (!pclkprog->p_slave_entries)
		return -ENOMEM;

	memset(pclkprog->p_slave_entries, CTRL_CLK_CLK_DOMAIN_INDEX_INVALID,
		slavesize);

	memcpy(pclkprog->p_slave_entries, ptmpprog->p_slave_entries, slavesize);

	return status;
}

static u32 clk_prog_construct_1x_master_table(struct gk20a *g,
					      struct boardobj **ppboardobj,
					      u16 size, void *pargs)
{
	struct boardobj *ptmpobj = (struct boardobj *)pargs;
	struct clk_prog_1x_master_table *pclkprog;
	struct clk_prog_1x_master_table *ptmpprog =
			(struct clk_prog_1x_master_table *)pargs;
	u32 status = 0;
	u32 slavesize = sizeof(struct ctrl_clk_clk_prog_1x_master_ratio_slave_entry) *
		g->clk_pmu.clk_progobjs.slave_entry_count;

	gk20a_dbg_info("type - %x", BOARDOBJ_GET_TYPE(pargs));

	if (BOARDOBJ_GET_TYPE(pargs) != CTRL_CLK_CLK_PROG_TYPE_1X_MASTER_TABLE)
		return -EINVAL;

	ptmpobj->type_mask |= BIT(CTRL_CLK_CLK_PROG_TYPE_1X_MASTER_TABLE);
	status = clk_prog_construct_1x_master(g, ppboardobj, size, pargs);
	if (status)
		return -EINVAL;

	pclkprog = (struct clk_prog_1x_master_table *)*ppboardobj;

	pclkprog->super.super.super.super.pmudatainit =
			_clk_prog_pmudatainit_1x_master_table;

	pclkprog->p_slave_entries =
		(struct ctrl_clk_clk_prog_1x_master_table_slave_entry *)
		kzalloc(slavesize, GFP_KERNEL);
	if (!pclkprog->p_slave_entries)
		return -ENOMEM;

	memset(pclkprog->p_slave_entries, CTRL_CLK_CLK_DOMAIN_INDEX_INVALID,
		slavesize);

	memcpy(pclkprog->p_slave_entries, ptmpprog->p_slave_entries, slavesize);

	return status;
}

static struct clk_prog *construct_clk_prog(struct gk20a *g, void *pargs)
{
	struct boardobj *board_obj_ptr = NULL;
	u32 status;

	gk20a_dbg_info(" type - %x", BOARDOBJ_GET_TYPE(pargs));
	switch (BOARDOBJ_GET_TYPE(pargs)) {
	case CTRL_CLK_CLK_PROG_TYPE_1X:
		status = clk_prog_construct_1x(g, &board_obj_ptr,
			sizeof(struct clk_prog_1x), pargs);
		break;

	case CTRL_CLK_CLK_PROG_TYPE_1X_MASTER_TABLE:
		status = clk_prog_construct_1x_master_table(g, &board_obj_ptr,
			sizeof(struct clk_prog_1x_master_table), pargs);
		break;

	case CTRL_CLK_CLK_PROG_TYPE_1X_MASTER_RATIO:
		status = clk_prog_construct_1x_master_ratio(g, &board_obj_ptr,
			sizeof(struct clk_prog_1x_master_ratio), pargs);
		break;

	default:
		return NULL;
	}

	if (status)
		return NULL;

	gk20a_dbg_info(" Done");

	return (struct clk_prog *)board_obj_ptr;
}

static u32 vfflatten_prog_1x_master(struct gk20a *g,
				    struct clk_pmupstate *pclk,
				    struct clk_prog_1x_master *p1xmaster,
				    u8 clk_domain_idx, u16 *pfreqmaxlastmhz)
{
	struct ctrl_clk_clk_prog_1x_master_vf_entry *p_vf_rail;
	union {
		struct boardobj board_obj;
		struct clk_vf_point vf_point;
		struct clk_vf_point_freq freq;
		struct clk_vf_point_volt volt;
	} vf_point_data;
	u32 status = 0;
	u8 step_count;
	u8 freq_step_size_mhz = 0;
	u8 vf_point_idx;
	u8 vf_rail_idx;

	gk20a_dbg_info("");
	memset(&vf_point_data, 0x0, sizeof(vf_point_data));

	vf_point_idx = BOARDOBJGRP_NEXT_EMPTY_IDX(
			&pclk->clk_vf_pointobjs.super.super);

	for (vf_rail_idx = 0;
	     vf_rail_idx < pclk->clk_progobjs.vf_entry_count;
	     vf_rail_idx++) {
		u32 voltage_min_uv;
		u32 voltage_step_size_uv;
		u8  i;

		p_vf_rail = &p1xmaster->p_vf_entries[vf_rail_idx];
		if (p_vf_rail->vfe_idx == CTRL_BOARDOBJ_IDX_INVALID)
			continue;

		p_vf_rail->vf_point_idx_first = vf_point_idx;

		vf_point_data.vf_point.vfe_equ_idx = p_vf_rail->vfe_idx;
		vf_point_data.vf_point.volt_rail_idx = vf_rail_idx;

		step_count = 0;

		switch (p1xmaster->super.source) {
		case CTRL_CLK_PROG_1X_SOURCE_PLL:
			freq_step_size_mhz =
				p1xmaster->super.source_data.pll.freq_step_size_mhz;
			step_count = (freq_step_size_mhz == 0) ? 0 :
				(p1xmaster->super.freq_max_mhz - *pfreqmaxlastmhz - 1) /
							freq_step_size_mhz;
			/* Intentional fall-through.*/

		case CTRL_CLK_PROG_1X_SOURCE_ONE_SOURCE:
			vf_point_data.board_obj.type =
				CTRL_CLK_CLK_VF_POINT_TYPE_FREQ;
			do {
				clkvfpointfreqmhzset(g, &vf_point_data.vf_point,
					p1xmaster->super.freq_max_mhz -
					  step_count * freq_step_size_mhz);

				status = _clk_prog_1x_master_rail_construct_vf_point(g, pclk,
					p1xmaster, p_vf_rail,
					&vf_point_data.vf_point, &vf_point_idx);
				if (status)
					goto done;
			} while (step_count-- > 0);
			break;

		case CTRL_CLK_PROG_1X_SOURCE_FLL:
			voltage_min_uv = CLK_FLL_LUT_MIN_VOLTAGE_UV(pclk);
			voltage_step_size_uv = CLK_FLL_LUT_STEP_SIZE_UV(pclk);
			step_count = CLK_FLL_LUT_VF_NUM_ENTRIES(pclk);

			/* FLL sources use a voltage-based VF_POINT.*/
			vf_point_data.board_obj.type =
				CTRL_CLK_CLK_VF_POINT_TYPE_VOLT;
			for (i = 0; i < step_count; i++) {
				vf_point_data.volt.source_voltage_uv =
					voltage_min_uv + i * voltage_step_size_uv;

				status = _clk_prog_1x_master_rail_construct_vf_point(g, pclk,
					p1xmaster, p_vf_rail,
					&vf_point_data.vf_point, &vf_point_idx);
				if (status)
					goto done;
			}
			break;
		}
	}

	*pfreqmaxlastmhz = p1xmaster->super.freq_max_mhz;

done:
	gk20a_dbg_info("done status %x", status);
	return status;
}

static u32 vflookup_prog_1x_master
(
	struct gk20a *g,
	struct clk_pmupstate *pclk,
	struct clk_prog_1x_master *p1xmaster,
	u8 *slave_clk_domain,
	u16 *pclkmhz,
	u32 *pvoltuv,
	u8 rail
)
{
	int j;
	struct ctrl_clk_clk_prog_1x_master_vf_entry
		*pvfentry;
	struct clk_vf_point *pvfpoint;
	struct clk_progs *pclkprogobjs;
	struct clk_prog_1x_master_ratio *p1xmasterratio;
	u16 clkmhz;
	u32 voltuv;
	u8 slaveentrycount;
	int i;
	struct ctrl_clk_clk_prog_1x_master_ratio_slave_entry *pslaveents;

	if ((*pclkmhz != 0) && (*pvoltuv != 0))
		return -EINVAL;

	pclkprogobjs = &(pclk->clk_progobjs);

	slaveentrycount = pclkprogobjs->slave_entry_count;

	if (pclkprogobjs->vf_entry_count >
		CTRL_CLK_CLK_PROG_1X_MASTER_VF_ENTRY_MAX_ENTRIES)
		return -EINVAL;

	if (rail >= pclkprogobjs->vf_entry_count)
		return -EINVAL;

	pvfentry =  p1xmaster->p_vf_entries;

	pvfentry = (struct ctrl_clk_clk_prog_1x_master_vf_entry *)(
			(u8 *)pvfentry +
			(sizeof(struct ctrl_clk_clk_prog_1x_master_vf_entry) *
			rail));

	clkmhz = *pclkmhz;
	voltuv = *pvoltuv;

	/*if domain is slave domain and freq is input
		then derive master clk */
	if ((slave_clk_domain != NULL) && (*pclkmhz != 0)) {
		if (p1xmaster->super.super.super.implements(g,
			&p1xmaster->super.super.super,
			CTRL_CLK_CLK_PROG_TYPE_1X_MASTER_RATIO)) {

			p1xmasterratio =
			(struct clk_prog_1x_master_ratio *)p1xmaster;
			pslaveents = p1xmasterratio->p_slave_entries;
			for (i = 0; i < slaveentrycount;  i++) {
				if (pslaveents->clk_dom_idx ==
					*slave_clk_domain)
					break;
				pslaveents++;
			}
			if (i == slaveentrycount)
				return -EINVAL;
			clkmhz = (clkmhz * 100)/pslaveents->ratio;
		} else {
			/* only support ratio for now */
			return -EINVAL;
		}
	}

	/* if both volt and clks are zero simply print*/
	if ((*pvoltuv == 0) && (*pclkmhz == 0)) {
		for (j = pvfentry->vf_point_idx_first;
			j <= pvfentry->vf_point_idx_last; j++) {
			pvfpoint = CLK_CLK_VF_POINT_GET(pclk, j);
			gk20a_err(dev_from_gk20a(g), "v %x c %x",
				clkvfpointvoltageuvget(g, pvfpoint),
				clkvfpointfreqmhzget(g, pvfpoint));
		}
		return -EINVAL;
	}
	/* start looking up f for v for v for f */
	/* looking for volt? */
	if (*pvoltuv == 0) {
		pvfpoint = CLK_CLK_VF_POINT_GET(pclk,
				pvfentry->vf_point_idx_last);
		/* above range? */
		if (clkmhz > clkvfpointfreqmhzget(g, pvfpoint))
			return -EINVAL;

		for (j = pvfentry->vf_point_idx_last;
			j >= pvfentry->vf_point_idx_first; j--) {
			pvfpoint = CLK_CLK_VF_POINT_GET(pclk, j);
			if (clkmhz <= clkvfpointfreqmhzget(g, pvfpoint))
				voltuv = clkvfpointvoltageuvget(g, pvfpoint);
			else
				break;
		}
	} else {	/* looking for clk? */

		pvfpoint = CLK_CLK_VF_POINT_GET(pclk,
				pvfentry->vf_point_idx_first);
		/* below range? */
		if (voltuv < clkvfpointvoltageuvget(g, pvfpoint))
			return -EINVAL;

		for (j = pvfentry->vf_point_idx_first;
			j <= pvfentry->vf_point_idx_last; j++) {
			pvfpoint = CLK_CLK_VF_POINT_GET(pclk, j);
			if (voltuv >= clkvfpointvoltageuvget(g, pvfpoint))
				clkmhz = clkvfpointfreqmhzget(g, pvfpoint);
			else
				break;
		}
	}

	/*if domain is slave domain and freq was looked up
		then derive slave clk */
	if ((slave_clk_domain != NULL) && (*pclkmhz == 0)) {
		if (p1xmaster->super.super.super.implements(g,
			&p1xmaster->super.super.super,
			CTRL_CLK_CLK_PROG_TYPE_1X_MASTER_RATIO)) {

			p1xmasterratio =
			(struct clk_prog_1x_master_ratio *)p1xmaster;
			pslaveents = p1xmasterratio->p_slave_entries;
			for (i = 0; i < slaveentrycount;  i++) {
				if (pslaveents->clk_dom_idx ==
					*slave_clk_domain)
					break;
				pslaveents++;
			}
			if (i == slaveentrycount)
				return -EINVAL;
			clkmhz = (clkmhz * pslaveents->ratio)/100;
		} else {
			/* only support ratio for now */
			return -EINVAL;
		}
	}
	*pclkmhz = clkmhz;
	*pvoltuv = voltuv;
	if ((clkmhz == 0) || (voltuv == 0))
		return -EINVAL;
	return 0;
}

static u32 getfpoints_prog_1x_master
(
	struct gk20a *g,
	struct clk_pmupstate *pclk,
	struct clk_prog_1x_master *p1xmaster,
	u32 *pfpointscount,
	u16 **ppfreqpointsinmhz,
	u8 rail
)
{

	struct ctrl_clk_clk_prog_1x_master_vf_entry
		*pvfentry;
	struct clk_vf_point *pvfpoint;
	struct clk_progs *pclkprogobjs;
	u8 j;
	u32 fpointscount = 0;

	if (pfpointscount == NULL)
		return -EINVAL;

	pclkprogobjs = &(pclk->clk_progobjs);

	if (pclkprogobjs->vf_entry_count >
		CTRL_CLK_CLK_PROG_1X_MASTER_VF_ENTRY_MAX_ENTRIES)
		return -EINVAL;

	if (rail >= pclkprogobjs->vf_entry_count)
		return -EINVAL;

	pvfentry =  p1xmaster->p_vf_entries;

	pvfentry = (struct ctrl_clk_clk_prog_1x_master_vf_entry *)(
			(u8 *)pvfentry +
			(sizeof(struct ctrl_clk_clk_prog_1x_master_vf_entry) *
			(rail+1)));

	fpointscount = pvfentry->vf_point_idx_last -
		pvfentry->vf_point_idx_first + 1;

	/* if pointer for freq data is NULL simply return count */
	if (*ppfreqpointsinmhz == NULL)
		goto done;

	if (fpointscount > *pfpointscount)
		return -ENOMEM;
	for (j = pvfentry->vf_point_idx_first;
		j <= pvfentry->vf_point_idx_last; j++) {
		pvfpoint = CLK_CLK_VF_POINT_GET(pclk, j);
		**ppfreqpointsinmhz = clkvfpointfreqmhzget(g, pvfpoint);
		(*ppfreqpointsinmhz)++;
	}
done:
	*pfpointscount = fpointscount;
	return 0;
}

static int getslaveclk_prog_1x_master(struct gk20a *g,
				struct clk_pmupstate *pclk,
				struct clk_prog_1x_master *p1xmaster,
				u8 slave_clk_domain,
				u16 *pclkmhz,
				u16 masterclkmhz
)
{
	struct clk_progs *pclkprogobjs;
	struct clk_prog_1x_master_ratio *p1xmasterratio;
	u8 slaveentrycount;
	u8 i;
	struct ctrl_clk_clk_prog_1x_master_ratio_slave_entry *pslaveents;

	if (pclkmhz == NULL)
		return -EINVAL;

	if (masterclkmhz == 0)
		return -EINVAL;

	*pclkmhz = 0;
	pclkprogobjs = &(pclk->clk_progobjs);

	slaveentrycount = pclkprogobjs->slave_entry_count;

	if (p1xmaster->super.super.super.implements(g,
		&p1xmaster->super.super.super,
		CTRL_CLK_CLK_PROG_TYPE_1X_MASTER_RATIO)) {
		p1xmasterratio =
		(struct clk_prog_1x_master_ratio *)p1xmaster;
		pslaveents = p1xmasterratio->p_slave_entries;
		for (i = 0; i < slaveentrycount;  i++) {
			if (pslaveents->clk_dom_idx ==
				slave_clk_domain)
				break;
			pslaveents++;
		}
		if (i == slaveentrycount)
			return -EINVAL;
		*pclkmhz = (masterclkmhz * pslaveents->ratio)/100;
	} else {
		/* only support ratio for now */
		return -EINVAL;
	}
	return 0;
}
