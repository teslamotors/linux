/*
 * general p state infrastructure
 *
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
#include "clk/clk.h"
#include "perf/perf.h"
#include "include/bios.h"
#include "pstate/pstate.h"
#include "pmgr/pmgr.h"
#include "therm/thrm.h"

static int pstate_sw_setup(struct gk20a *g);

/*sw setup for pstate components*/
int gk20a_init_pstate_support(struct gk20a *g)
{
	u32 err;

	gk20a_dbg_fn("");

	err = volt_rail_sw_setup(g);
	if (err)
		return err;

	err = volt_dev_sw_setup(g);
	if (err)
		return err;

	err = volt_policy_sw_setup(g);
	if (err)
		return err;

	err = clk_vin_sw_setup(g);
	if (err)
		return err;

	err = clk_fll_sw_setup(g);
	if (err)
		return err;

	err = therm_domain_sw_setup(g);
	if (err)
		return err;

	err = vfe_var_sw_setup(g);
	if (err)
		return err;

	err = vfe_equ_sw_setup(g);
	if (err)
		return err;

	err = clk_domain_sw_setup(g);
	if (err)
		return err;

	err = clk_vf_point_sw_setup(g);
	if (err)
		return err;

	err = clk_prog_sw_setup(g);
	if (err)
		return err;

	err = pstate_sw_setup(g);
	if (err)
		return err;

	err = pmgr_domain_sw_setup(g);
	if (err)
		return err;

	err = clk_freq_controller_sw_setup(g);
	if (err)
		return err;

	err = nvgpu_lpwr_pg_setup(g);

	return err;
}

/*sw setup for pstate components*/
int gk20a_init_pstate_pmu_support(struct gk20a *g)
{
	u32 err;

	gk20a_dbg_fn("");

	err = volt_rail_pmu_setup(g);
	if (err)
		return err;

	err = volt_dev_pmu_setup(g);
	if (err)
		return err;

	err = volt_policy_pmu_setup(g);
	if (err)
		return err;

	err = volt_pmu_send_load_cmd_to_pmu(g);
	if (err) {
		gk20a_err(dev_from_gk20a(g),
			"Failed to send VOLT LOAD CMD to PMU: status = 0x%08x.",
			err);
		return err;
	}

	err = therm_domain_pmu_setup(g);
	if (err)
		return err;

	err = vfe_var_pmu_setup(g);
	if (err)
		return err;

	err = vfe_equ_pmu_setup(g);
	if (err)
		return err;

	err = clk_domain_pmu_setup(g);
	if (err)
		return err;

	err = clk_prog_pmu_setup(g);
	if (err)
		return err;

	err = clk_vin_pmu_setup(g);
	if (err)
		return err;

	err = clk_fll_pmu_setup(g);
	if (err)
		return err;

	err = clk_vf_point_pmu_setup(g);
	if (err)
		return err;

	err = clk_freq_controller_pmu_setup(g);
	if (err)
		return err;

	err = clk_pmu_vin_load(g);
	if (err)
		return err;

	err = perf_pmu_vfe_load(g);
	if (err)
		return err;

	err = pmgr_domain_pmu_setup(g);
	return err;
}

int pstate_construct_super(struct gk20a *g, struct boardobj **ppboardobj,
				u16 size, void *args)
{
	struct pstate *ptmppstate = (struct pstate *)args;
	struct pstate *pstate;
	int err;

	err = boardobj_construct_super(g, ppboardobj, size, args);
	if (err)
		return err;

	pstate = (struct pstate *)*ppboardobj;

	pstate->num = ptmppstate->num;
	pstate->clklist = ptmppstate->clklist;
	pstate->lpwr_entry_idx = ptmppstate->lpwr_entry_idx;

	return 0;
}

int pstate_construct_3x(struct gk20a *g, struct boardobj **ppboardobj,
				u16 size, void *args)
{
	struct boardobj  *ptmpobj = (struct boardobj *)args;

	ptmpobj->type_mask |= BIT(CTRL_PERF_PSTATE_TYPE_3X);
	return pstate_construct_super(g, ppboardobj, size, args);
}

struct pstate *pstate_construct(struct gk20a *g, void *args)
{
	struct pstate *pstate = NULL;
	struct pstate *tmp = (struct pstate *)args;

	if ((tmp->super.type != CTRL_PERF_PSTATE_TYPE_3X) ||
	    (pstate_construct_3x(g, (struct boardobj **)&pstate,
			    sizeof(struct pstate), args)))
		gk20a_err(dev_from_gk20a(g),
			"error constructing pstate num=%u", tmp->num);

	return pstate;
}

int pstate_insert(struct gk20a *g, struct pstate *pstate, int index)
{
	struct pstates *pstates = &(g->perf_pmu.pstatesobjs);
	int err;

	err = boardobjgrp_objinsert(&pstates->super.super,
			(struct boardobj *)pstate, index);
	if (err) {
		gk20a_err(dev_from_gk20a(g),
			  "error adding pstate boardobj %d", index);
		return err;
	}

	pstates->num_levels++;

	return err;
}

static int parse_pstate_entry_5x(struct gk20a *g,
		struct vbios_pstate_header_5x *hdr,
		struct vbios_pstate_entry_5x *entry,
		struct pstate *pstate)
{
	u8 *p = (u8 *)entry;
	u32 clkidx;

	p += hdr->base_entry_size;

	memset(pstate, 0, sizeof(struct pstate));
	pstate->super.type = CTRL_PERF_PSTATE_TYPE_3X;
	pstate->num = 0x0F - entry->pstate_level;
	pstate->clklist.num_info = hdr->clock_entry_count;
	pstate->lpwr_entry_idx = entry->lpwr_entry_idx;

	gk20a_dbg_info("pstate P%u", pstate->num);

	for (clkidx = 0; clkidx < hdr->clock_entry_count; clkidx++) {
		struct clk_set_info *pclksetinfo;
		struct vbios_pstate_entry_clock_5x *clk_entry;
		struct clk_domain *clk_domain;

		clk_domain = (struct clk_domain *)BOARDOBJGRP_OBJ_GET_BY_IDX(
			    &g->clk_pmu.clk_domainobjs.super.super, clkidx);

		pclksetinfo = &pstate->clklist.clksetinfo[clkidx];
		clk_entry = (struct vbios_pstate_entry_clock_5x *)p;

		pclksetinfo->clkwhich = clk_domain->domain;
		pclksetinfo->nominal_mhz =
			BIOS_GET_FIELD(clk_entry->param0,
				VBIOS_PSTATE_5X_CLOCK_PROG_PARAM0_NOM_FREQ_MHZ);
		pclksetinfo->min_mhz =
			BIOS_GET_FIELD(clk_entry->param1,
				VBIOS_PSTATE_5X_CLOCK_PROG_PARAM1_MIN_FREQ_MHZ);
		pclksetinfo->max_mhz =
			BIOS_GET_FIELD(clk_entry->param1,
				VBIOS_PSTATE_5X_CLOCK_PROG_PARAM1_MAX_FREQ_MHZ);

		gk20a_dbg_info(
			"clk_domain=%u nominal_mhz=%u min_mhz=%u max_mhz=%u",
			pclksetinfo->clkwhich, pclksetinfo->nominal_mhz,
			pclksetinfo->min_mhz, pclksetinfo->max_mhz);

		p += hdr->clock_entry_size;
	}

	return 0;
}

static int parse_pstate_table_5x(struct gk20a *g,
		struct vbios_pstate_header_5x *hdr)
{
	struct pstate _pstate, *pstate = &_pstate;
	struct vbios_pstate_entry_5x *entry;
	u32 entry_size;
	u8 i;
	u8 *p = (u8 *)hdr;
	int err = 0;

	if ((hdr->header_size != VBIOS_PSTATE_HEADER_5X_SIZE_10) ||
		(hdr->base_entry_count == 0) ||
		((hdr->base_entry_size != VBIOS_PSTATE_BASE_ENTRY_5X_SIZE_2) &&
		 (hdr->base_entry_size != VBIOS_PSTATE_BASE_ENTRY_5X_SIZE_3)) ||
		(hdr->clock_entry_size != VBIOS_PSTATE_CLOCK_ENTRY_5X_SIZE_6) ||
		(hdr->clock_entry_count > CLK_SET_INFO_MAX_SIZE))
		return -EINVAL;

	p += hdr->header_size;

	entry_size = hdr->base_entry_size +
			hdr->clock_entry_count * hdr->clock_entry_size;

	for (i = 0; i < hdr->base_entry_count; i++, p += entry_size) {
		entry = (struct vbios_pstate_entry_5x *)p;

		if (entry->pstate_level == VBIOS_PERFLEVEL_SKIP_ENTRY)
			continue;

		err = parse_pstate_entry_5x(g, hdr, entry, pstate);
		if (err)
			goto done;

		pstate = pstate_construct(g, pstate);
		if (!pstate)
			goto done;

		err = pstate_insert(g, pstate, i);
		if (err)
			goto done;
	}

done:
	return err;
}

static int pstate_sw_setup(struct gk20a *g)
{
	struct vbios_pstate_header_5x *hdr = NULL;
	int err = 0;

	gk20a_dbg_fn("");

	init_waitqueue_head(&g->perf_pmu.pstatesobjs.pstate_notifier_wq);
	mutex_init(&g->perf_pmu.pstatesobjs.pstate_mutex);

	err = boardobjgrpconstruct_e32(&g->perf_pmu.pstatesobjs.super);
	if (err) {
		gk20a_err(dev_from_gk20a(g),
			  "error creating boardobjgrp for pstates, err=%d",
			  err);
		goto done;
	}

	if (g->ops.bios.get_perf_table_ptrs) {
		hdr = (struct vbios_pstate_header_5x *)
				g->ops.bios.get_perf_table_ptrs(g,
				g->bios.perf_token, PERFORMANCE_TABLE);
		if (!hdr) {
			gk20a_err(dev_from_gk20a(g),
					"performance table not found");
			err = -EINVAL;
			goto done;
		}
	}

	if (hdr->version != VBIOS_PSTATE_TABLE_VERSION_5X) {
		gk20a_err(dev_from_gk20a(g),
				"unknown/unsupported clocks table version=0x%02x",
				hdr->version);
		err = -EINVAL;
		goto done;
	}

	err = parse_pstate_table_5x(g, hdr);
done:
	return err;
}

struct pstate *pstate_find(struct gk20a *g, u32 num)
{
	struct pstates *pstates = &(g->perf_pmu.pstatesobjs);
	struct pstate *pstate;
	u8 i;

	gk20a_dbg_info("pstates = %p", pstates);

	BOARDOBJGRP_FOR_EACH(&pstates->super.super,
			struct pstate *, pstate, i) {
		gk20a_dbg_info("pstate=%p num=%u (looking for num=%u)",
				pstate, pstate->num, num);
		if (pstate->num == num)
			return pstate;
	}
	return NULL;
}

struct clk_set_info *pstate_get_clk_set_info(struct gk20a *g,
		u32 pstate_num, enum nv_pmu_clk_clkwhich clkwhich)
{
	struct pstate *pstate = pstate_find(g, pstate_num);
	struct clk_set_info *info;
	u32 clkidx;

	gk20a_dbg_info("pstate = %p", pstate);

	if (!pstate)
		return NULL;

	for (clkidx = 0; clkidx < pstate->clklist.num_info; clkidx++) {
		info = &pstate->clklist.clksetinfo[clkidx];
		if (info->clkwhich == clkwhich)
			return info;
	}
	return NULL;
}
