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
#include "clk_fll.h"
#include "clk_domain.h"
#include "include/bios.h"
#include "boardobj/boardobjgrp.h"
#include "boardobj/boardobjgrp_e32.h"
#include "pmuif/gpmuifboardobj.h"
#include "pmuif/gpmuifclk.h"
#include "gm206/bios_gm206.h"
#include "ctrl/ctrlclk.h"
#include "ctrl/ctrlvolt.h"
#include "gk20a/pmu_gk20a.h"

static struct clk_domain *construct_clk_domain(struct gk20a *g, void *pargs);

static u32 devinit_get_clocks_table(struct gk20a *g,
	struct clk_domains *pdomainobjs);

static u32 clk_domain_pmudatainit_super(struct gk20a *g, struct boardobj
	*board_obj_ptr,	struct nv_pmu_boardobj *ppmudata);

const struct vbios_clocks_table_1x_hal_clock_entry vbiosclktbl1xhalentry[] = {
	{ clkwhich_gpc2clk,    true,  },
	{ clkwhich_xbar2clk,   true,  },
	{ clkwhich_mclk,       false, },
	{ clkwhich_sys2clk,    true,  },
	{ clkwhich_hub2clk,    false, },
	{ clkwhich_nvdclk,     false, },
	{ clkwhich_pwrclk,     false, },
	{ clkwhich_dispclk,    false, },
	{ clkwhich_pciegenclk, false, }
};

static u32 clktranslatehalmumsettoapinumset(u32 clkhaldomains)
{
	u32   clkapidomains = 0;

	if (clkhaldomains & BIT(clkwhich_gpc2clk))
		clkapidomains |= CTRL_CLK_DOMAIN_GPC2CLK;
	if (clkhaldomains & BIT(clkwhich_xbar2clk))
		clkapidomains |= CTRL_CLK_DOMAIN_XBAR2CLK;
	if (clkhaldomains & BIT(clkwhich_sys2clk))
		clkapidomains |= CTRL_CLK_DOMAIN_SYS2CLK;
	if (clkhaldomains & BIT(clkwhich_hub2clk))
		clkapidomains |= CTRL_CLK_DOMAIN_HUB2CLK;
	if (clkhaldomains & BIT(clkwhich_pwrclk))
		clkapidomains |= CTRL_CLK_DOMAIN_PWRCLK;
	if (clkhaldomains & BIT(clkwhich_pciegenclk))
		clkapidomains |= CTRL_CLK_DOMAIN_PCIEGENCLK;
	if (clkhaldomains & BIT(clkwhich_mclk))
		clkapidomains |= CTRL_CLK_DOMAIN_MCLK;
	if (clkhaldomains & BIT(clkwhich_nvdclk))
		clkapidomains |= CTRL_CLK_DOMAIN_NVDCLK;
	if (clkhaldomains & BIT(clkwhich_dispclk))
		clkapidomains |= CTRL_CLK_DOMAIN_DISPCLK;

	return clkapidomains;
}

static u32 _clk_domains_pmudatainit_3x(struct gk20a *g,
				       struct boardobjgrp *pboardobjgrp,
				       struct nv_pmu_boardobjgrp_super *pboardobjgrppmu)
{
	struct nv_pmu_clk_clk_domain_boardobjgrp_set_header *pset =
		(struct nv_pmu_clk_clk_domain_boardobjgrp_set_header *)
		pboardobjgrppmu;
	struct clk_domains *pdomains = (struct clk_domains *)pboardobjgrp;
	u32 status = 0;

	status = boardobjgrp_pmudatainit_e32(g, pboardobjgrp, pboardobjgrppmu);
	if (status) {
		gk20a_err(dev_from_gk20a(g),
			  "error updating pmu boardobjgrp for clk domain 0x%x",
			  status);
		goto done;
	}

	pset->vbios_domains = pdomains->vbios_domains;
	pset->cntr_sampling_periodms = pdomains->cntr_sampling_periodms;
	pset->b_override_o_v_o_c = false;
	pset->b_debug_mode = false;
	pset->b_enforce_vf_monotonicity = pdomains->b_enforce_vf_monotonicity;
	pset->b_enforce_vf_smoothening = pdomains->b_enforce_vf_smoothening;
	pset->volt_rails_max = 2;
	status = boardobjgrpmask_export(
				&pdomains->master_domains_mask.super,
				pdomains->master_domains_mask.super.bitcount,
				&pset->master_domains_mask.super);

	memcpy(&pset->deltas, &pdomains->deltas,
		(sizeof(struct ctrl_clk_clk_delta)));

done:
	return status;
}

static u32 _clk_domains_pmudata_instget(struct gk20a *g,
					struct nv_pmu_boardobjgrp *pmuboardobjgrp,
					struct nv_pmu_boardobj **ppboardobjpmudata,
					u8 idx)
{
	struct nv_pmu_clk_clk_domain_boardobj_grp_set  *pgrp_set =
		(struct nv_pmu_clk_clk_domain_boardobj_grp_set *)
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

u32 clk_domain_sw_setup(struct gk20a *g)
{
	u32 status;
	struct boardobjgrp *pboardobjgrp = NULL;
	struct clk_domains *pclkdomainobjs;
	struct clk_domain *pdomain;
	struct clk_domain_3x_master *pdomain_master;
	struct clk_domain_3x_slave *pdomain_slave;
	u8 i;

	gk20a_dbg_info("");

	status = boardobjgrpconstruct_e32(&g->clk_pmu.clk_domainobjs.super);
	if (status) {
		gk20a_err(dev_from_gk20a(g),
			  "error creating boardobjgrp for clk domain, status - 0x%x",
			  status);
		goto done;
	}

	pboardobjgrp = &g->clk_pmu.clk_domainobjs.super.super;
	pclkdomainobjs = &(g->clk_pmu.clk_domainobjs);

	BOARDOBJGRP_PMU_CONSTRUCT(pboardobjgrp, CLK, CLK_DOMAIN);

	status = BOARDOBJGRP_PMU_CMD_GRP_SET_CONSTRUCT(g, pboardobjgrp,
			clk, CLK, clk_domain, CLK_DOMAIN);
	if (status) {
		gk20a_err(dev_from_gk20a(g),
			  "error constructing PMU_BOARDOBJ_CMD_GRP_SET interface - 0x%x",
			  status);
		goto done;
	}

	pboardobjgrp->pmudatainit  = _clk_domains_pmudatainit_3x;
	pboardobjgrp->pmudatainstget  = _clk_domains_pmudata_instget;

	/* Initialize mask to zero.*/
	boardobjgrpmask_e32_init(&pclkdomainobjs->prog_domains_mask, NULL);
	boardobjgrpmask_e32_init(&pclkdomainobjs->master_domains_mask, NULL);
	pclkdomainobjs->b_enforce_vf_monotonicity = true;
	pclkdomainobjs->b_enforce_vf_smoothening = true;

	memset(&pclkdomainobjs->ordered_noise_aware_list, 0,
		sizeof(pclkdomainobjs->ordered_noise_aware_list));

	memset(&pclkdomainobjs->ordered_noise_unaware_list, 0,
		sizeof(pclkdomainobjs->ordered_noise_unaware_list));

	memset(&pclkdomainobjs->deltas, 0,
		sizeof(struct ctrl_clk_clk_delta));

	status = devinit_get_clocks_table(g, pclkdomainobjs);
	if (status)
		goto done;

	BOARDOBJGRP_FOR_EACH(&(pclkdomainobjs->super.super),
			     struct clk_domain *, pdomain, i) {
		pdomain_master = NULL;
		if (pdomain->super.implements(g, &pdomain->super,
				CTRL_CLK_CLK_DOMAIN_TYPE_3X_PROG)) {
			status = boardobjgrpmask_bitset(
				&pclkdomainobjs->prog_domains_mask.super, i);
			if (status)
				goto done;
		}

		if (pdomain->super.implements(g, &pdomain->super,
				CTRL_CLK_CLK_DOMAIN_TYPE_3X_MASTER)) {
			status = boardobjgrpmask_bitset(
				&pclkdomainobjs->master_domains_mask.super, i);
			if (status)
				goto done;
		}

		if (pdomain->super.implements(g, &pdomain->super,
				CTRL_CLK_CLK_DOMAIN_TYPE_3X_SLAVE)) {
				pdomain_slave =
					(struct clk_domain_3x_slave *)pdomain;
				pdomain_master =
					(struct clk_domain_3x_master *)
					(CLK_CLK_DOMAIN_GET((&g->clk_pmu),
					pdomain_slave->master_idx));
			pdomain_master->slave_idxs_mask |= BIT(i);
		}

	}

done:
	gk20a_dbg_info(" done status %x", status);
	return status;
}

u32 clk_domain_pmu_setup(struct gk20a *g)
{
	u32 status;
	struct boardobjgrp *pboardobjgrp = NULL;

	gk20a_dbg_info("");

	pboardobjgrp = &g->clk_pmu.clk_domainobjs.super.super;

	if (!pboardobjgrp->bconstructed)
		return -EINVAL;

	status = pboardobjgrp->pmuinithandle(g, pboardobjgrp);

	gk20a_dbg_info("Done");
	return status;
}

static u32 devinit_get_clocks_table(struct gk20a *g,
				    struct clk_domains *pclkdomainobjs)
{
	u32 status = 0;
	u8 *clocks_table_ptr = NULL;
	struct vbios_clocks_table_1x_header clocks_table_header = { 0 };
	struct vbios_clocks_table_1x_entry clocks_table_entry = { 0 };
	u8 *clocks_tbl_entry_ptr = NULL;
	u32 index = 0;
	struct clk_domain *pclkdomain_dev;
	union {
		struct boardobj boardobj;
		struct clk_domain clk_domain;
		struct clk_domain_3x v3x;
		struct clk_domain_3x_fixed v3x_fixed;
		struct clk_domain_3x_prog v3x_prog;
		struct clk_domain_3x_master v3x_master;
		struct clk_domain_3x_slave v3x_slave;
	} clk_domain_data;

	gk20a_dbg_info("");

	if (g->ops.bios.get_perf_table_ptrs) {
		clocks_table_ptr = (u8 *)g->ops.bios.get_perf_table_ptrs(g,
				g->bios.clock_token, CLOCKS_TABLE);
		if (clocks_table_ptr == NULL) {
			status = -EINVAL;
			goto done;
		}
	}

	memcpy(&clocks_table_header, clocks_table_ptr,
			VBIOS_CLOCKS_TABLE_1X_HEADER_SIZE_07);
	if (clocks_table_header.header_size <
			VBIOS_CLOCKS_TABLE_1X_HEADER_SIZE_07) {
		status = -EINVAL;
		goto done;
	}

	if (clocks_table_header.entry_size <
	    VBIOS_CLOCKS_TABLE_1X_ENTRY_SIZE_09) {
		status = -EINVAL;
		goto done;
	}

	pclkdomainobjs->cntr_sampling_periodms =
		(u16)clocks_table_header.cntr_sampling_periodms;

	/* Read table entries*/
	clocks_tbl_entry_ptr = clocks_table_ptr +
		VBIOS_CLOCKS_TABLE_1X_HEADER_SIZE_07;
	for (index = 0; index < clocks_table_header.entry_count; index++) {
		memcpy(&clocks_table_entry, clocks_tbl_entry_ptr,
				clocks_table_header.entry_size);
		clk_domain_data.clk_domain.domain =
				vbiosclktbl1xhalentry[index].domain;
		clk_domain_data.clk_domain.api_domain =
				clktranslatehalmumsettoapinumset(
					BIT(clk_domain_data.clk_domain.domain));
		clk_domain_data.v3x.b_noise_aware_capable =
			vbiosclktbl1xhalentry[index].b_noise_aware_capable;

		switch (BIOS_GET_FIELD(clocks_table_entry.flags0,
				NV_VBIOS_CLOCKS_TABLE_1X_ENTRY_FLAGS0_USAGE)) {
		case  NV_VBIOS_CLOCKS_TABLE_1X_ENTRY_FLAGS0_USAGE_FIXED:
			clk_domain_data.boardobj.type =
				CTRL_CLK_CLK_DOMAIN_TYPE_3X_FIXED;
			clk_domain_data.v3x_fixed.freq_mhz = (u16)BIOS_GET_FIELD(
				clocks_table_entry.param1,
				NV_VBIOS_CLOCKS_TABLE_1X_ENTRY_PARAM1_FIXED_FREQUENCY_MHZ);
			break;

		case  NV_VBIOS_CLOCKS_TABLE_1X_ENTRY_FLAGS0_USAGE_MASTER:
			clk_domain_data.boardobj.type =
				CTRL_CLK_CLK_DOMAIN_TYPE_3X_MASTER;
			clk_domain_data.v3x_prog.clk_prog_idx_first =
				(u8)(BIOS_GET_FIELD(clocks_table_entry.param0,
				     NV_VBIOS_CLOCKS_TABLE_1X_ENTRY_PARAM0_PROG_CLK_PROG_IDX_FIRST));
			clk_domain_data.v3x_prog.clk_prog_idx_last =
				(u8)(BIOS_GET_FIELD(clocks_table_entry.param0,
				     NV_VBIOS_CLOCKS_TABLE_1X_ENTRY_PARAM0_PROG_CLK_PROG_IDX_LAST));
			clk_domain_data.v3x_prog.noise_unaware_ordering_index =
				(u8)(BIOS_GET_FIELD(clocks_table_entry.param2,
				     NV_VBIOS_CLOCKS_TABLE_1X_ENTRY_PARAM2_PROG_NOISE_UNAWARE_ORDERING_IDX));

			if (clk_domain_data.v3x.b_noise_aware_capable) {
				clk_domain_data.v3x_prog.noise_aware_ordering_index =
					(u8)(BIOS_GET_FIELD(clocks_table_entry.param2,
					     NV_VBIOS_CLOCKS_TABLE_1X_ENTRY_PARAM2_PROG_NOISE_AWARE_ORDERING_IDX));
				clk_domain_data.v3x_prog.b_force_noise_unaware_ordering =
					(u8)(BIOS_GET_FIELD(clocks_table_entry.param2,
					     NV_VBIOS_CLOCKS_TABLE_1X_ENTRY_PARAM2_PROG_FORCE_NOISE_UNAWARE_ORDERING));
			} else {
				clk_domain_data.v3x_prog.noise_aware_ordering_index =
					CTRL_CLK_CLK_DOMAIN_3X_PROG_ORDERING_INDEX_INVALID;
				clk_domain_data.v3x_prog.b_force_noise_unaware_ordering = false;
			}
			clk_domain_data.v3x_prog.factory_offset_khz = 0;

			clk_domain_data.v3x_prog.freq_delta_min_mhz =
				(u16)(BIOS_GET_FIELD(clocks_table_entry.param1,
				      NV_VBIOS_CLOCKS_TABLE_1X_ENTRY_PARAM1_MASTER_FREQ_OC_DELTA_MIN_MHZ));

			clk_domain_data.v3x_prog.freq_delta_max_mhz =
				(u16)(BIOS_GET_FIELD(clocks_table_entry.param1,
				      NV_VBIOS_CLOCKS_TABLE_1X_ENTRY_PARAM1_MASTER_FREQ_OC_DELTA_MAX_MHZ));
			break;

		case  NV_VBIOS_CLOCKS_TABLE_1X_ENTRY_FLAGS0_USAGE_SLAVE:
			clk_domain_data.boardobj.type =
				CTRL_CLK_CLK_DOMAIN_TYPE_3X_SLAVE;
			clk_domain_data.v3x_prog.clk_prog_idx_first =
				(u8)(BIOS_GET_FIELD(clocks_table_entry.param0,
				     NV_VBIOS_CLOCKS_TABLE_1X_ENTRY_PARAM0_PROG_CLK_PROG_IDX_FIRST));
			clk_domain_data.v3x_prog.clk_prog_idx_last =
				(u8)(BIOS_GET_FIELD(clocks_table_entry.param0,
				     NV_VBIOS_CLOCKS_TABLE_1X_ENTRY_PARAM0_PROG_CLK_PROG_IDX_LAST));
			clk_domain_data.v3x_prog.noise_unaware_ordering_index =
				(u8)(BIOS_GET_FIELD(clocks_table_entry.param2,
				     NV_VBIOS_CLOCKS_TABLE_1X_ENTRY_PARAM2_PROG_NOISE_UNAWARE_ORDERING_IDX));

			if (clk_domain_data.v3x.b_noise_aware_capable) {
				clk_domain_data.v3x_prog.noise_aware_ordering_index =
					(u8)(BIOS_GET_FIELD(clocks_table_entry.param2,
					     NV_VBIOS_CLOCKS_TABLE_1X_ENTRY_PARAM2_PROG_NOISE_AWARE_ORDERING_IDX));
				clk_domain_data.v3x_prog.b_force_noise_unaware_ordering =
					(u8)(BIOS_GET_FIELD(clocks_table_entry.param2,
					     NV_VBIOS_CLOCKS_TABLE_1X_ENTRY_PARAM2_PROG_FORCE_NOISE_UNAWARE_ORDERING));
			} else {
				clk_domain_data.v3x_prog.noise_aware_ordering_index =
					CTRL_CLK_CLK_DOMAIN_3X_PROG_ORDERING_INDEX_INVALID;
				clk_domain_data.v3x_prog.b_force_noise_unaware_ordering = false;
			}
			clk_domain_data.v3x_prog.factory_offset_khz = 0;
			clk_domain_data.v3x_prog.freq_delta_min_mhz = 0;
			clk_domain_data.v3x_prog.freq_delta_max_mhz = 0;
			clk_domain_data.v3x_slave.master_idx =
				(u8)(BIOS_GET_FIELD(clocks_table_entry.param1,
				     NV_VBIOS_CLOCKS_TABLE_1X_ENTRY_PARAM1_SLAVE_MASTER_DOMAIN));
			break;

		default:
			gk20a_err(dev_from_gk20a(g),
				  "error reading clock domain entry %d", index);
			status = -EINVAL;
			goto done;

		}
		pclkdomain_dev = construct_clk_domain(g,
				(void *)&clk_domain_data);
		if (pclkdomain_dev == NULL) {
			gk20a_err(dev_from_gk20a(g),
				  "unable to construct clock domain boardobj for %d",
				  index);
			status = -EINVAL;
			goto done;
		}
		status = boardobjgrp_objinsert(&pclkdomainobjs->super.super,
				(struct boardobj *)pclkdomain_dev, index);
		if (status) {
			gk20a_err(dev_from_gk20a(g),
			"unable to insert clock domain boardobj for %d", index);
			status = -EINVAL;
			goto done;
		}
		clocks_tbl_entry_ptr += clocks_table_header.entry_size;
	}

done:
	gk20a_dbg_info(" done status %x", status);
	return status;
}

static u32 clkdomainclkproglink_not_supported(struct gk20a *g,
					      struct clk_pmupstate *pclk,
					      struct clk_domain *pdomain)
{
	gk20a_dbg_info("");
	return -EINVAL;
}

static int clkdomainvfsearch_stub(
	struct gk20a *g,
	struct clk_pmupstate *pclk,
	struct clk_domain *pdomain,
	u16 *clkmhz,
	u32 *voltuv,
	u8 rail)

{
	gk20a_dbg_info("");
	return -EINVAL;
}

static u32 clkdomaingetfpoints_stub(
	struct gk20a *g,
	struct clk_pmupstate *pclk,
	struct clk_domain *pdomain,
	u32 *pfpointscount,
	u16 *pfreqpointsinmhz,
	u8 rail)
{
	gk20a_dbg_info("");
	return -EINVAL;
}


static u32 clk_domain_construct_super(struct gk20a *g,
				      struct boardobj **ppboardobj,
				      u16 size, void *pargs)
{
	struct clk_domain *pdomain;
	struct clk_domain *ptmpdomain = (struct clk_domain *)pargs;
	u32 status = 0;

	status = boardobj_construct_super(g, ppboardobj,
		size, pargs);

	if (status)
		return -EINVAL;

	pdomain = (struct clk_domain *)*ppboardobj;

	pdomain->super.pmudatainit =
			clk_domain_pmudatainit_super;

	pdomain->clkdomainclkproglink =
			clkdomainclkproglink_not_supported;

	pdomain->clkdomainclkvfsearch =
			clkdomainvfsearch_stub;

	pdomain->clkdomainclkgetfpoints =
			clkdomaingetfpoints_stub;

	pdomain->api_domain = ptmpdomain->api_domain;
	pdomain->domain = ptmpdomain->domain;
	pdomain->perf_domain_grp_idx =
		ptmpdomain->perf_domain_grp_idx;

	return status;
}

static u32 _clk_domain_pmudatainit_3x(struct gk20a *g,
				      struct boardobj *board_obj_ptr,
				      struct nv_pmu_boardobj *ppmudata)
{
	u32 status = 0;
	struct clk_domain_3x *pclk_domain_3x;
	struct nv_pmu_clk_clk_domain_3x_boardobj_set *pset;

	gk20a_dbg_info("");

	status = clk_domain_pmudatainit_super(g, board_obj_ptr, ppmudata);
	if (status != 0)
		return status;

	pclk_domain_3x = (struct clk_domain_3x *)board_obj_ptr;

	pset = (struct nv_pmu_clk_clk_domain_3x_boardobj_set *)ppmudata;

	pset->b_noise_aware_capable = pclk_domain_3x->b_noise_aware_capable;

	return status;
}

static u32 clk_domain_construct_3x(struct gk20a *g,
				   struct boardobj **ppboardobj,
				   u16 size, void *pargs)
{
	struct boardobj *ptmpobj = (struct boardobj *)pargs;
	struct clk_domain_3x *pdomain;
	struct clk_domain_3x *ptmpdomain =
			(struct clk_domain_3x *)pargs;
	u32 status = 0;

	ptmpobj->type_mask = BIT(CTRL_CLK_CLK_DOMAIN_TYPE_3X);
	status = clk_domain_construct_super(g, ppboardobj,
					size, pargs);
	if (status)
		return -EINVAL;

	pdomain = (struct clk_domain_3x *)*ppboardobj;

	pdomain->super.super.pmudatainit =
			_clk_domain_pmudatainit_3x;

	pdomain->b_noise_aware_capable = ptmpdomain->b_noise_aware_capable;

	return status;
}

static u32 clkdomainclkproglink_3x_prog(struct gk20a *g,
					struct clk_pmupstate *pclk,
					struct clk_domain *pdomain)
{
	u32 status = 0;
	struct clk_domain_3x_prog *p3xprog =
		(struct clk_domain_3x_prog *)pdomain;
	struct clk_prog *pprog = NULL;
	u8 i;

	gk20a_dbg_info("");

	for (i = p3xprog->clk_prog_idx_first;
	     i <= p3xprog->clk_prog_idx_last;
	     i++) {
		pprog = CLK_CLK_PROG_GET(pclk, i);
		if (pprog == NULL)
			status = -EINVAL;
	}
	return status;
}

static int clkdomaingetslaveclk(struct gk20a *g,
				struct clk_pmupstate *pclk,
				struct clk_domain *pdomain,
				u16 *pclkmhz,
				u16 masterclkmhz)
{
	int status = 0;
	struct clk_prog *pprog = NULL;
	struct clk_prog_1x_master *pprog1xmaster = NULL;
	u8 slaveidx;
	struct clk_domain_3x_master *p3xmaster;

	gk20a_dbg_info("");

	if (pclkmhz == NULL)
		return -EINVAL;

	if (masterclkmhz == 0)
		return -EINVAL;

	slaveidx = BOARDOBJ_GET_IDX(pdomain);
	p3xmaster = (struct clk_domain_3x_master *)
			CLK_CLK_DOMAIN_GET(pclk,
			((struct clk_domain_3x_slave *)
				pdomain)->master_idx);
	pprog = CLK_CLK_PROG_GET(pclk, p3xmaster->super.clk_prog_idx_first);
	pprog1xmaster = (struct clk_prog_1x_master *)pprog;

	status = pprog1xmaster->getslaveclk(g, pclk, pprog1xmaster,
			slaveidx, pclkmhz, masterclkmhz);
	return status;
}

static int clkdomainvfsearch(struct gk20a *g,
				struct clk_pmupstate *pclk,
				struct clk_domain *pdomain,
				u16 *pclkmhz,
				u32 *pvoltuv,
				u8 rail)
{
	int status = 0;
	struct clk_domain_3x_master *p3xmaster  =
		(struct clk_domain_3x_master *)pdomain;
	struct clk_prog *pprog = NULL;
	struct clk_prog_1x_master *pprog1xmaster = NULL;
	u8 i;
	u8 *pslaveidx = NULL;
	u8 slaveidx;
	u16 clkmhz;
	u32 voltuv;
	u16 bestclkmhz;
	u32 bestvoltuv;

	gk20a_dbg_info("");

	if ((pclkmhz == NULL) || (pvoltuv == NULL))
		return -EINVAL;

	if ((*pclkmhz != 0) && (*pvoltuv != 0))
		return -EINVAL;

	bestclkmhz = *pclkmhz;
	bestvoltuv = *pvoltuv;

	if (pdomain->super.implements(g, &pdomain->super,
			CTRL_CLK_CLK_DOMAIN_TYPE_3X_SLAVE)) {
		slaveidx = BOARDOBJ_GET_IDX(pdomain);
		pslaveidx = &slaveidx;
		p3xmaster = (struct clk_domain_3x_master *)
				CLK_CLK_DOMAIN_GET(pclk,
				((struct clk_domain_3x_slave *)
					pdomain)->master_idx);
	}
	/* Iterate over the set of CLK_PROGs pointed at by this domain.*/
	for (i = p3xmaster->super.clk_prog_idx_first;
	     i <= p3xmaster->super.clk_prog_idx_last;
	     i++) {
		clkmhz = *pclkmhz;
		voltuv = *pvoltuv;
		pprog = CLK_CLK_PROG_GET(pclk, i);

		/* MASTER CLK_DOMAINs must point to MASTER CLK_PROGs.*/
		if (!pprog->super.implements(g, &pprog->super,
				CTRL_CLK_CLK_PROG_TYPE_1X_MASTER)) {
			status = -EINVAL;
			goto done;
		}

		pprog1xmaster = (struct clk_prog_1x_master *)pprog;
		status = pprog1xmaster->vflookup(g, pclk, pprog1xmaster,
				pslaveidx, &clkmhz, &voltuv, rail);
		/* if look up has found the V or F value matching to other
		 exit */
		if (status == 0) {
			if (*pclkmhz == 0) {
				bestclkmhz = clkmhz;
			} else {
				bestvoltuv = voltuv;
				break;
			}
		}
	}
	/* clk and volt sent as zero to print vf table */
	if ((*pclkmhz == 0) && (*pvoltuv == 0)) {
		status = 0;
		goto done;
	}
	/* atleast one search found a matching value? */
	if ((bestvoltuv != 0) && (bestclkmhz != 0)) {
		*pclkmhz = bestclkmhz;
		*pvoltuv = bestvoltuv;
		status = 0;
		goto done;
	}
done:
	gk20a_dbg_info("done status %x", status);
	return status;
}

static u32 clkdomaingetfpoints
(
	struct gk20a *g,
	struct clk_pmupstate *pclk,
	struct clk_domain *pdomain,
	u32 *pfpointscount,
	u16 *pfreqpointsinmhz,
	u8 rail
)
{
	u32 status = 0;
	struct clk_domain_3x_master *p3xmaster  =
		(struct clk_domain_3x_master *)pdomain;
	struct clk_prog *pprog = NULL;
	struct clk_prog_1x_master *pprog1xmaster = NULL;
	u32 fpointscount = 0;
	u32 remainingcount;
	u32 totalcount;
	u16 *freqpointsdata;
	u8 i;

	gk20a_dbg_info("");

	if (pfpointscount == NULL)
		return -EINVAL;

	if ((pfreqpointsinmhz == NULL) && (*pfpointscount != 0))
		return -EINVAL;

	if (pdomain->super.implements(g, &pdomain->super,
			CTRL_CLK_CLK_DOMAIN_TYPE_3X_SLAVE))
		return -EINVAL;

	freqpointsdata = pfreqpointsinmhz;
	totalcount = 0;
	fpointscount = *pfpointscount;
	remainingcount = fpointscount;
	/* Iterate over the set of CLK_PROGs pointed at by this domain.*/
	for (i = p3xmaster->super.clk_prog_idx_first;
	     i <= p3xmaster->super.clk_prog_idx_last;
	     i++) {
		pprog = CLK_CLK_PROG_GET(pclk, i);
		pprog1xmaster = (struct clk_prog_1x_master *)pprog;
		status = pprog1xmaster->getfpoints(g, pclk, pprog1xmaster,
				&fpointscount, &freqpointsdata, rail);
		if (status) {
			*pfpointscount = 0;
			goto done;
		}
		totalcount += fpointscount;
		if (*pfpointscount) {
			remainingcount -= fpointscount;
			fpointscount = remainingcount;
		} else
			fpointscount = 0;

	}

	*pfpointscount = totalcount;
done:
	gk20a_dbg_info("done status %x", status);
	return status;
}

static u32 _clk_domain_pmudatainit_3x_prog(struct gk20a *g,
					   struct boardobj *board_obj_ptr,
					   struct nv_pmu_boardobj *ppmudata)
{
	u32 status = 0;
	struct clk_domain_3x_prog *pclk_domain_3x_prog;
	struct nv_pmu_clk_clk_domain_3x_prog_boardobj_set *pset;
	struct clk_domains *pdomains = &(g->clk_pmu.clk_domainobjs);

	gk20a_dbg_info("");

	status = _clk_domain_pmudatainit_3x(g, board_obj_ptr, ppmudata);
	if (status != 0)
		return status;

	pclk_domain_3x_prog = (struct clk_domain_3x_prog *)board_obj_ptr;

	pset = (struct nv_pmu_clk_clk_domain_3x_prog_boardobj_set *)
		ppmudata;

	pset->clk_prog_idx_first = pclk_domain_3x_prog->clk_prog_idx_first;
	pset->clk_prog_idx_last = pclk_domain_3x_prog->clk_prog_idx_last;
	pset->noise_unaware_ordering_index =
		pclk_domain_3x_prog->noise_unaware_ordering_index;
	pset->noise_aware_ordering_index =
		pclk_domain_3x_prog->noise_aware_ordering_index;
	pset->b_force_noise_unaware_ordering =
		pclk_domain_3x_prog->b_force_noise_unaware_ordering;
	pset->factory_offset_khz = pclk_domain_3x_prog->factory_offset_khz;
	pset->freq_delta_min_mhz = pclk_domain_3x_prog->freq_delta_min_mhz;
	pset->freq_delta_max_mhz = pclk_domain_3x_prog->freq_delta_max_mhz;
	memcpy(&pset->deltas, &pdomains->deltas,
		(sizeof(struct ctrl_clk_clk_delta)));

	return status;
}

static u32 clk_domain_construct_3x_prog(struct gk20a *g,
					struct boardobj **ppboardobj,
					u16 size, void *pargs)
{
	struct boardobj *ptmpobj = (struct boardobj *)pargs;
	struct clk_domain_3x_prog *pdomain;
	struct clk_domain_3x_prog *ptmpdomain =
			(struct clk_domain_3x_prog *)pargs;
	u32 status = 0;

	ptmpobj->type_mask |= BIT(CTRL_CLK_CLK_DOMAIN_TYPE_3X_PROG);
	status = clk_domain_construct_3x(g, ppboardobj, size, pargs);
	if (status)
		return -EINVAL;

	pdomain = (struct clk_domain_3x_prog *)*ppboardobj;

	pdomain->super.super.super.pmudatainit =
			_clk_domain_pmudatainit_3x_prog;

	pdomain->super.super.clkdomainclkproglink =
				clkdomainclkproglink_3x_prog;

	pdomain->super.super.clkdomainclkvfsearch =
				clkdomainvfsearch;

	pdomain->super.super.clkdomainclkgetfpoints =
				clkdomaingetfpoints;

	pdomain->clk_prog_idx_first = ptmpdomain->clk_prog_idx_first;
	pdomain->clk_prog_idx_last = ptmpdomain->clk_prog_idx_last;
	pdomain->noise_unaware_ordering_index =
		ptmpdomain->noise_unaware_ordering_index;
	pdomain->noise_aware_ordering_index =
		ptmpdomain->noise_aware_ordering_index;
	pdomain->b_force_noise_unaware_ordering =
		ptmpdomain->b_force_noise_unaware_ordering;
	pdomain->factory_offset_khz = ptmpdomain->factory_offset_khz;
	pdomain->freq_delta_min_mhz = ptmpdomain->freq_delta_min_mhz;
	pdomain->freq_delta_max_mhz = ptmpdomain->freq_delta_max_mhz;

	return status;
}

static u32 _clk_domain_pmudatainit_3x_slave(struct gk20a *g,
					    struct boardobj *board_obj_ptr,
					    struct nv_pmu_boardobj *ppmudata)
{
	u32 status = 0;
	struct clk_domain_3x_slave *pclk_domain_3x_slave;
	struct nv_pmu_clk_clk_domain_3x_slave_boardobj_set *pset;

	gk20a_dbg_info("");

	status = _clk_domain_pmudatainit_3x_prog(g, board_obj_ptr, ppmudata);
	if (status != 0)
		return status;

	pclk_domain_3x_slave = (struct clk_domain_3x_slave *)board_obj_ptr;

	pset = (struct nv_pmu_clk_clk_domain_3x_slave_boardobj_set *)
		ppmudata;

	pset->master_idx = pclk_domain_3x_slave->master_idx;

	return status;
}

static u32 clk_domain_construct_3x_slave(struct gk20a *g,
					 struct boardobj **ppboardobj,
					 u16 size, void *pargs)
{
	struct boardobj *ptmpobj = (struct boardobj *)pargs;
	struct clk_domain_3x_slave *pdomain;
	struct clk_domain_3x_slave *ptmpdomain =
			(struct clk_domain_3x_slave *)pargs;
	u32 status = 0;

	if (BOARDOBJ_GET_TYPE(pargs) != CTRL_CLK_CLK_DOMAIN_TYPE_3X_SLAVE)
		return -EINVAL;

	ptmpobj->type_mask |= BIT(CTRL_CLK_CLK_DOMAIN_TYPE_3X_SLAVE);
	status = clk_domain_construct_3x_prog(g, ppboardobj, size, pargs);
	if (status)
		return -EINVAL;

	pdomain = (struct clk_domain_3x_slave *)*ppboardobj;

	pdomain->super.super.super.super.pmudatainit =
			_clk_domain_pmudatainit_3x_slave;

	pdomain->master_idx = ptmpdomain->master_idx;

	pdomain->clkdomainclkgetslaveclk =
				clkdomaingetslaveclk;

	return status;
}

static u32 clkdomainclkproglink_3x_master(struct gk20a *g,
					  struct clk_pmupstate *pclk,
					  struct clk_domain *pdomain)
{
	u32 status = 0;
	struct clk_domain_3x_master *p3xmaster  =
		(struct clk_domain_3x_master *)pdomain;
	struct clk_prog *pprog = NULL;
	struct clk_prog_1x_master *pprog1xmaster = NULL;
	u16 freq_max_last_mhz = 0;
	u8 i;

	gk20a_dbg_info("");

	status = clkdomainclkproglink_3x_prog(g, pclk, pdomain);
	if (status)
		goto done;

	/* Iterate over the set of CLK_PROGs pointed at by this domain.*/
	for (i = p3xmaster->super.clk_prog_idx_first;
	     i <= p3xmaster->super.clk_prog_idx_last;
	     i++) {
		pprog = CLK_CLK_PROG_GET(pclk, i);

		/* MASTER CLK_DOMAINs must point to MASTER CLK_PROGs.*/
		if (!pprog->super.implements(g, &pprog->super,
				CTRL_CLK_CLK_PROG_TYPE_1X_MASTER)) {
			status = -EINVAL;
			goto done;
		}

		pprog1xmaster = (struct clk_prog_1x_master *)pprog;
		status = pprog1xmaster->vfflatten(g, pclk, pprog1xmaster,
			BOARDOBJ_GET_IDX(p3xmaster), &freq_max_last_mhz);
		if (status)
			goto done;
	}
done:
	gk20a_dbg_info("done status %x", status);
	return status;
}

static u32 _clk_domain_pmudatainit_3x_master(struct gk20a *g,
					     struct boardobj *board_obj_ptr,
					     struct nv_pmu_boardobj *ppmudata)
{
	u32 status = 0;
	struct clk_domain_3x_master *pclk_domain_3x_master;
	struct nv_pmu_clk_clk_domain_3x_master_boardobj_set *pset;

	gk20a_dbg_info("");

	status = _clk_domain_pmudatainit_3x_prog(g, board_obj_ptr, ppmudata);
	if (status != 0)
		return status;

	pclk_domain_3x_master = (struct clk_domain_3x_master *)board_obj_ptr;

	pset = (struct nv_pmu_clk_clk_domain_3x_master_boardobj_set *)
		ppmudata;

	pset->slave_idxs_mask = pclk_domain_3x_master->slave_idxs_mask;

	return status;
}

static u32 clk_domain_construct_3x_master(struct gk20a *g,
					  struct boardobj **ppboardobj,
					  u16 size, void *pargs)
{
	struct boardobj *ptmpobj = (struct boardobj *)pargs;
	struct clk_domain_3x_master *pdomain;
	u32 status = 0;

	if (BOARDOBJ_GET_TYPE(pargs) != CTRL_CLK_CLK_DOMAIN_TYPE_3X_MASTER)
		return -EINVAL;

	ptmpobj->type_mask |= BIT(CTRL_CLK_CLK_DOMAIN_TYPE_3X_MASTER);
	status = clk_domain_construct_3x_prog(g, ppboardobj, size, pargs);
	if (status)
		return -EINVAL;

	pdomain = (struct clk_domain_3x_master *)*ppboardobj;

	pdomain->super.super.super.super.pmudatainit =
			_clk_domain_pmudatainit_3x_master;
	pdomain->super.super.super.clkdomainclkproglink =
				clkdomainclkproglink_3x_master;

	pdomain->slave_idxs_mask = 0;

	return status;
}

static u32 clkdomainclkproglink_fixed(struct gk20a *g,
				      struct clk_pmupstate *pclk,
				      struct clk_domain *pdomain)
{
	gk20a_dbg_info("");
	return 0;
}

static u32 _clk_domain_pmudatainit_3x_fixed(struct gk20a *g,
					    struct boardobj *board_obj_ptr,
					    struct nv_pmu_boardobj *ppmudata)
{
	u32 status = 0;
	struct clk_domain_3x_fixed *pclk_domain_3x_fixed;
	struct nv_pmu_clk_clk_domain_3x_fixed_boardobj_set *pset;

	gk20a_dbg_info("");

	status = _clk_domain_pmudatainit_3x(g, board_obj_ptr, ppmudata);
	if (status != 0)
		return status;

	pclk_domain_3x_fixed = (struct clk_domain_3x_fixed *)board_obj_ptr;

	pset = (struct nv_pmu_clk_clk_domain_3x_fixed_boardobj_set *)
		ppmudata;

	pset->freq_mhz = pclk_domain_3x_fixed->freq_mhz;

	return status;
}

static u32 clk_domain_construct_3x_fixed(struct gk20a *g,
					 struct boardobj **ppboardobj,
					 u16 size, void *pargs)
{
	struct boardobj *ptmpobj = (struct boardobj *)pargs;
	struct clk_domain_3x_fixed *pdomain;
	struct clk_domain_3x_fixed *ptmpdomain =
			(struct clk_domain_3x_fixed *)pargs;
	u32 status = 0;

	if (BOARDOBJ_GET_TYPE(pargs) != CTRL_CLK_CLK_DOMAIN_TYPE_3X_FIXED)
		return -EINVAL;

	ptmpobj->type_mask |= BIT(CTRL_CLK_CLK_DOMAIN_TYPE_3X_FIXED);
	status = clk_domain_construct_3x(g, ppboardobj, size, pargs);
	if (status)
		return -EINVAL;

	pdomain = (struct clk_domain_3x_fixed *)*ppboardobj;

	pdomain->super.super.super.pmudatainit =
			_clk_domain_pmudatainit_3x_fixed;

	pdomain->super.super.clkdomainclkproglink =
			clkdomainclkproglink_fixed;

	pdomain->freq_mhz = ptmpdomain->freq_mhz;

	return status;
}

static struct clk_domain *construct_clk_domain(struct gk20a *g, void *pargs)
{
	struct boardobj *board_obj_ptr = NULL;
	u32 status;

	gk20a_dbg_info(" %d", BOARDOBJ_GET_TYPE(pargs));
	switch (BOARDOBJ_GET_TYPE(pargs)) {
	case CTRL_CLK_CLK_DOMAIN_TYPE_3X_FIXED:
		status = clk_domain_construct_3x_fixed(g, &board_obj_ptr,
			sizeof(struct clk_domain_3x_fixed), pargs);
		break;

	case CTRL_CLK_CLK_DOMAIN_TYPE_3X_MASTER:
		status = clk_domain_construct_3x_master(g, &board_obj_ptr,
			sizeof(struct clk_domain_3x_master), pargs);
		break;

	case CTRL_CLK_CLK_DOMAIN_TYPE_3X_SLAVE:
		status = clk_domain_construct_3x_slave(g, &board_obj_ptr,
			sizeof(struct clk_domain_3x_slave), pargs);
		break;

	default:
		return NULL;
	}

	if (status)
		return NULL;

	gk20a_dbg_info(" Done");

	return (struct clk_domain *)board_obj_ptr;
}

static u32 clk_domain_pmudatainit_super(struct gk20a *g,
					struct boardobj *board_obj_ptr,
					struct nv_pmu_boardobj *ppmudata)
{
	u32 status = 0;
	struct clk_domain *pclk_domain;
	struct nv_pmu_clk_clk_domain_boardobj_set *pset;

	gk20a_dbg_info("");

	status = boardobj_pmudatainit_super(g, board_obj_ptr, ppmudata);
	if (status != 0)
		return status;

	pclk_domain = (struct clk_domain *)board_obj_ptr;

	pset = (struct nv_pmu_clk_clk_domain_boardobj_set *)ppmudata;

	pset->domain = pclk_domain->domain;
	pset->api_domain = pclk_domain->api_domain;
	pset->perf_domain_grp_idx = pclk_domain->perf_domain_grp_idx;

	return status;
}

u32 clk_domain_clk_prog_link(struct gk20a *g, struct clk_pmupstate *pclk)
{
	u32 status = 0;
	struct clk_domain *pdomain;
	u8 i;

	/* Iterate over all CLK_DOMAINs and flatten their VF curves.*/
	BOARDOBJGRP_FOR_EACH(&(pclk->clk_domainobjs.super.super),
			struct clk_domain *, pdomain, i) {
		status = pdomain->clkdomainclkproglink(g, pclk, pdomain);
		if (status) {
			gk20a_err(dev_from_gk20a(g),
				  "error flattening VF for CLK DOMAIN - 0x%x",
				  pdomain->domain);
			goto done;
		}
	}

done:
	return status;
}
