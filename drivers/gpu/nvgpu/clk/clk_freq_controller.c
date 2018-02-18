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
#include "clk_freq_controller.h"
#include "include/bios.h"
#include "boardobj/boardobjgrp.h"
#include "boardobj/boardobjgrp_e32.h"
#include "pmuif/gpmuifboardobj.h"
#include "pmuif/gpmuifclk.h"
#include "gm206/bios_gm206.h"
#include "ctrl/ctrlclk.h"
#include "ctrl/ctrlvolt.h"
#include "gk20a/pmu_gk20a.h"

static u32 clk_freq_controller_pmudatainit_super(struct gk20a *g,
	struct boardobj *board_obj_ptr,
	struct nv_pmu_boardobj *ppmudata)
{
	struct nv_pmu_clk_clk_freq_controller_boardobj_set *pfreq_cntlr_set;
	struct clk_freq_controller *pfreq_cntlr;
	u32 status = 0;

	status = boardobj_pmudatainit_super(g, board_obj_ptr, ppmudata);
	if (status)
		return status;

	pfreq_cntlr_set =
		(struct nv_pmu_clk_clk_freq_controller_boardobj_set *)ppmudata;
	pfreq_cntlr    = (struct clk_freq_controller *)board_obj_ptr;

	pfreq_cntlr_set->controller_id = pfreq_cntlr->controller_id;
	pfreq_cntlr_set->clk_domain = pfreq_cntlr->clk_domain;
	pfreq_cntlr_set->parts_freq_mode = pfreq_cntlr->parts_freq_mode;
	pfreq_cntlr_set->bdisable = pfreq_cntlr->bdisable;
	pfreq_cntlr_set->freq_cap_noise_unaware_vmin_above =
		pfreq_cntlr->freq_cap_noise_unaware_vmin_above;
	pfreq_cntlr_set->freq_cap_noise_unaware_vmin_below =
		pfreq_cntlr->freq_cap_noise_unaware_vmin_below;
	pfreq_cntlr_set->freq_hyst_pos_mhz = pfreq_cntlr->freq_hyst_pos_mhz;
	pfreq_cntlr_set->freq_hyst_neg_mhz = pfreq_cntlr->freq_hyst_neg_mhz;

	return status;
}

static u32 clk_freq_controller_pmudatainit_pi(struct gk20a *g,
	struct boardobj *board_obj_ptr,
	struct nv_pmu_boardobj *ppmudata)
{
	struct nv_pmu_clk_clk_freq_controller_pi_boardobj_set
		*pfreq_cntlr_pi_set;
	struct clk_freq_controller_pi *pfreq_cntlr_pi;
	u32 status = 0;

	status = clk_freq_controller_pmudatainit_super(g,
		board_obj_ptr, ppmudata);
	if (status)
		return -1;

	pfreq_cntlr_pi_set =
		(struct nv_pmu_clk_clk_freq_controller_pi_boardobj_set *)
		ppmudata;
	pfreq_cntlr_pi = (struct clk_freq_controller_pi *)board_obj_ptr;

	pfreq_cntlr_pi_set->prop_gain = pfreq_cntlr_pi->prop_gain;
	pfreq_cntlr_pi_set->integ_gain = pfreq_cntlr_pi->integ_gain;
	pfreq_cntlr_pi_set->integ_decay = pfreq_cntlr_pi->integ_decay;
	pfreq_cntlr_pi_set->volt_delta_min = pfreq_cntlr_pi->volt_delta_min;
	pfreq_cntlr_pi_set->volt_delta_max = pfreq_cntlr_pi->volt_delta_max;
	pfreq_cntlr_pi_set->slowdown_pct_min = pfreq_cntlr_pi->slowdown_pct_min;
	pfreq_cntlr_pi_set->bpoison = pfreq_cntlr_pi->bpoison;

	return status;
}

static u32 clk_freq_controller_construct_super(struct gk20a *g,
	struct boardobj **ppboardobj,
	u16 size, void *pargs)
{
	struct clk_freq_controller *pfreq_cntlr = NULL;
	struct clk_freq_controller *pfreq_cntlr_tmp = NULL;
	u32 status = 0;

	status = boardobj_construct_super(g, ppboardobj, size, pargs);
	if (status)
		return -EINVAL;

	pfreq_cntlr_tmp = (struct clk_freq_controller *)pargs;
	pfreq_cntlr = (struct clk_freq_controller *)*ppboardobj;

	pfreq_cntlr->super.pmudatainit = clk_freq_controller_pmudatainit_super;

	pfreq_cntlr->controller_id   = pfreq_cntlr_tmp->controller_id;
	pfreq_cntlr->clk_domain      = pfreq_cntlr_tmp->clk_domain;
	pfreq_cntlr->parts_freq_mode  = pfreq_cntlr_tmp->parts_freq_mode;
	pfreq_cntlr->freq_cap_noise_unaware_vmin_above  =
		pfreq_cntlr_tmp->freq_cap_noise_unaware_vmin_above;
	pfreq_cntlr->freq_cap_noise_unaware_vmin_below  =
		pfreq_cntlr_tmp->freq_cap_noise_unaware_vmin_below;
	pfreq_cntlr->freq_hyst_pos_mhz = pfreq_cntlr_tmp->freq_hyst_pos_mhz;
	pfreq_cntlr->freq_hyst_neg_mhz = pfreq_cntlr_tmp->freq_hyst_neg_mhz;

	return status;
}

static u32 clk_freq_controller_construct_pi(struct gk20a *g,
	struct boardobj **ppboardobj,
	u16 size, void *pargs)
{
	struct clk_freq_controller_pi *pfreq_cntlr_pi = NULL;
	struct clk_freq_controller_pi *pfreq_cntlr_pi_tmp = NULL;
	u32 status = 0;

	status = clk_freq_controller_construct_super(g, ppboardobj,
			size, pargs);
	if (status)
		return -EINVAL;

	pfreq_cntlr_pi = (struct clk_freq_controller_pi *)*ppboardobj;
	pfreq_cntlr_pi_tmp = (struct clk_freq_controller_pi *)pargs;

	pfreq_cntlr_pi->super.super.pmudatainit =
		clk_freq_controller_pmudatainit_pi;

	pfreq_cntlr_pi->prop_gain = pfreq_cntlr_pi_tmp->prop_gain;
	pfreq_cntlr_pi->integ_gain = pfreq_cntlr_pi_tmp->integ_gain;
	pfreq_cntlr_pi->integ_decay = pfreq_cntlr_pi_tmp->integ_decay;
	pfreq_cntlr_pi->volt_delta_min = pfreq_cntlr_pi_tmp->volt_delta_min;
	pfreq_cntlr_pi->volt_delta_max = pfreq_cntlr_pi_tmp->volt_delta_max;
	pfreq_cntlr_pi->slowdown_pct_min = pfreq_cntlr_pi_tmp->slowdown_pct_min;
	pfreq_cntlr_pi->bpoison = pfreq_cntlr_pi_tmp->bpoison;

	return status;
}

struct clk_freq_controller *clk_clk_freq_controller_construct(struct gk20a *g,
	void *pargs)
{
	struct boardobj *board_obj_ptr = NULL;
	u32 status = 0;

	if (BOARDOBJ_GET_TYPE(pargs) != CTRL_CLK_CLK_FREQ_CONTROLLER_TYPE_PI)
		return NULL;

	status = clk_freq_controller_construct_pi(g, &board_obj_ptr,
				sizeof(struct clk_freq_controller_pi), pargs);
	if (status)
		return NULL;

	return (struct clk_freq_controller *)board_obj_ptr;
}


static u32 clk_get_freq_controller_table(struct gk20a *g,
		struct clk_freq_controllers *pclk_freq_controllers)
{
	u32 status = 0;
	u8 *pfreq_controller_table_ptr = NULL;
	struct vbios_fct_1x_header header = { 0 };
	struct vbios_fct_1x_entry entry  = { 0 };
	u8 entry_idx;
	u8 *entry_offset;
	u32 freq_controller_id;
	struct clk_freq_controller *pclk_freq_cntr = NULL;
	struct clk_freq_controller *ptmp_freq_cntr = NULL;
	struct clk_freq_controller_pi *ptmp_freq_cntr_pi = NULL;
	struct clk_domain *pclk_domain;

	struct freq_controller_data_type {
		union {
			struct boardobj board_obj;
			struct clk_freq_controller freq_controller;
			struct clk_freq_controller_pi  freq_controller_pi;
		};
	} freq_controller_data;

	if (g->ops.bios.get_perf_table_ptrs) {
		pfreq_controller_table_ptr =
			(u8 *)g->ops.bios.get_perf_table_ptrs(g,
				g->bios.clock_token,
				FREQUENCY_CONTROLLER_TABLE);
		if (pfreq_controller_table_ptr == NULL) {
			status = -EINVAL;
			goto done;
		}
	} else {
		status = -EINVAL;
		goto done;
	}

	memcpy(&header, pfreq_controller_table_ptr,
			sizeof(struct vbios_fct_1x_header));

	pclk_freq_controllers->sampling_period_ms = header.sampling_period_ms;
	pclk_freq_controllers->volt_policy_idx = 0;

	/* Read in the entries. */
	for (entry_idx = 0; entry_idx < header.entry_count; entry_idx++) {
		entry_offset = (pfreq_controller_table_ptr +
			header.header_size + (entry_idx * header.entry_size));

		memset(&freq_controller_data, 0x0,
				sizeof(struct freq_controller_data_type));
		ptmp_freq_cntr = &freq_controller_data.freq_controller;
		ptmp_freq_cntr_pi = &freq_controller_data.freq_controller_pi;

		memcpy(&entry, entry_offset,
			sizeof(struct vbios_fct_1x_entry));

		if (!BIOS_GET_FIELD(entry.flags0,
				NV_VBIOS_FCT_1X_ENTRY_FLAGS0_TYPE))
			continue;

		freq_controller_data.board_obj.type = (u8)BIOS_GET_FIELD(
			entry.flags0, NV_VBIOS_FCT_1X_ENTRY_FLAGS0_TYPE);

		ptmp_freq_cntr->controller_id =
			(u8)BIOS_GET_FIELD(entry.param0,
				NV_VBIOS_FCT_1X_ENTRY_PARAM0_ID);

		freq_controller_id = ptmp_freq_cntr->controller_id;

		pclk_domain = CLK_CLK_DOMAIN_GET((&g->clk_pmu),
				(u32)entry.clk_domain_idx);
		freq_controller_data.freq_controller.clk_domain =
			pclk_domain->api_domain;

		ptmp_freq_cntr->parts_freq_mode =
			(u8)BIOS_GET_FIELD(entry.param0,
				NV_VBIOS_FCT_1X_ENTRY_PARAM0_FREQ_MODE);

		/* Populate PI specific data */
		ptmp_freq_cntr_pi->slowdown_pct_min =
			(u8)BIOS_GET_FIELD(entry.param1,
				NV_VBIOS_FCT_1X_ENTRY_PARAM1_SLOWDOWN_PCT_MIN);

		ptmp_freq_cntr_pi->bpoison =
			BIOS_GET_FIELD(entry.param1,
				NV_VBIOS_FCT_1X_ENTRY_PARAM1_POISON);

		ptmp_freq_cntr_pi->prop_gain =
			(s32)BIOS_GET_FIELD(entry.param2,
				NV_VBIOS_FCT_1X_ENTRY_PARAM2_PROP_GAIN);

		ptmp_freq_cntr_pi->integ_gain =
			(s32)BIOS_GET_FIELD(entry.param3,
				NV_VBIOS_FCT_1X_ENTRY_PARAM3_INTEG_GAIN);

		ptmp_freq_cntr_pi->integ_decay =
			(s32)BIOS_GET_FIELD(entry.param4,
				NV_VBIOS_FCT_1X_ENTRY_PARAM4_INTEG_DECAY);

		ptmp_freq_cntr_pi->volt_delta_min =
		(s32)BIOS_GET_FIELD(entry.param5,
			NV_VBIOS_FCT_1X_ENTRY_PARAM5_VOLT_DELTA_MIN);

		ptmp_freq_cntr_pi->volt_delta_max =
		(s32)BIOS_GET_FIELD(entry.param6,
			NV_VBIOS_FCT_1X_ENTRY_PARAM6_VOLT_DELTA_MAX);

		ptmp_freq_cntr->freq_cap_noise_unaware_vmin_above =
			(s16)BIOS_GET_FIELD(entry.param7,
				NV_VBIOS_FCT_1X_ENTRY_PARAM7_FREQ_CAP_VF);

		ptmp_freq_cntr->freq_cap_noise_unaware_vmin_below =
		(s16)BIOS_GET_FIELD(entry.param7,
			NV_VBIOS_FCT_1X_ENTRY_PARAM7_FREQ_CAP_VMIN);

		ptmp_freq_cntr->freq_hyst_pos_mhz =
			(s16)BIOS_GET_FIELD(entry.param8,
				NV_VBIOS_FCT_1X_ENTRY_PARAM8_FREQ_HYST_POS);
		ptmp_freq_cntr->freq_hyst_neg_mhz =
			(s16)BIOS_GET_FIELD(entry.param8,
				NV_VBIOS_FCT_1X_ENTRY_PARAM8_FREQ_HYST_NEG);

		if (ptmp_freq_cntr_pi->volt_delta_max <
			ptmp_freq_cntr_pi->volt_delta_min)
			goto done;

		pclk_freq_cntr = clk_clk_freq_controller_construct(g,
						(void *)&freq_controller_data);

		if (pclk_freq_cntr == NULL) {
			gk20a_err(dev_from_gk20a(g),
				"unable to construct clock freq cntlr boardobj for %d",
				entry_idx);
			status = -EINVAL;
			goto done;
		}

		status = boardobjgrp_objinsert(
				&pclk_freq_controllers->super.super,
				(struct boardobj *)pclk_freq_cntr, entry_idx);
		if (status) {
			gk20a_err(dev_from_gk20a(g),
			"unable to insert clock freq cntlr boardobj for");
			status = -EINVAL;
			goto done;
		}

	}

done:
	return status;
}

u32 clk_freq_controller_pmu_setup(struct gk20a *g)
{
	u32 status;
	struct boardobjgrp *pboardobjgrp = NULL;

	gk20a_dbg_info("");

	pboardobjgrp = &g->clk_pmu.clk_freq_controllers.super.super;

	if (!pboardobjgrp->bconstructed)
		return -EINVAL;

	status = pboardobjgrp->pmuinithandle(g, pboardobjgrp);

	gk20a_dbg_info("Done");
	return status;
}

static u32 _clk_freq_controller_devgrp_pmudata_instget(struct gk20a *g,
					   struct nv_pmu_boardobjgrp *pmuboardobjgrp,
					   struct nv_pmu_boardobj **ppboardobjpmudata,
					   u8 idx)
{
	struct nv_pmu_clk_clk_freq_controller_boardobj_grp_set *pgrp_set =
		(struct nv_pmu_clk_clk_freq_controller_boardobj_grp_set *)
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

static u32 _clk_freq_controllers_pmudatainit(struct gk20a *g,
				 struct boardobjgrp *pboardobjgrp,
				 struct nv_pmu_boardobjgrp_super *pboardobjgrppmu)
{
	struct nv_pmu_clk_clk_freq_controller_boardobjgrp_set_header *pset =
		(struct nv_pmu_clk_clk_freq_controller_boardobjgrp_set_header *)
		pboardobjgrppmu;
	struct clk_freq_controllers *pcntrs =
		(struct clk_freq_controllers *)pboardobjgrp;
	u32 status = 0;

	status = boardobjgrp_pmudatainit_e32(g, pboardobjgrp, pboardobjgrppmu);
	if (status) {
		gk20a_err(dev_from_gk20a(g),
			"error updating pmu boardobjgrp for clk freq ctrs 0x%x",
			 status);
		goto done;
	}
	pset->sampling_period_ms = pcntrs->sampling_period_ms;
	pset->volt_policy_idx = pcntrs->volt_policy_idx;

done:
	return status;
}

u32 clk_freq_controller_sw_setup(struct gk20a *g)
{
	u32 status = 0;
	struct boardobjgrp *pboardobjgrp = NULL;
	struct clk_freq_controllers *pclk_freq_controllers;
	struct avfsfllobjs *pfllobjs = &(g->clk_pmu.avfs_fllobjs);
	struct fll_device *pfll;
	struct clk_freq_controller *pclkfreqctrl;
	u8 i;
	u8 j;

	gk20a_dbg_info("");

	pclk_freq_controllers = &g->clk_pmu.clk_freq_controllers;
	status = boardobjgrpconstruct_e32(&pclk_freq_controllers->super);
	if (status) {
		gk20a_err(dev_from_gk20a(g),
			"error creating boardobjgrp for clk FCT, status - 0x%x",
			status);
		goto done;
	}

	pboardobjgrp = &g->clk_pmu.clk_freq_controllers.super.super;

	pboardobjgrp->pmudatainit  = _clk_freq_controllers_pmudatainit;
	pboardobjgrp->pmudatainstget  = _clk_freq_controller_devgrp_pmudata_instget;
	pboardobjgrp->pmustatusinstget  = NULL;

	/* Initialize mask to zero.*/
	boardobjgrpmask_e32_init(&pclk_freq_controllers->freq_ctrl_load_mask,
		NULL);

	BOARDOBJGRP_PMU_CONSTRUCT(pboardobjgrp, CLK, CLK_FREQ_CONTROLLER);

	status = BOARDOBJGRP_PMU_CMD_GRP_SET_CONSTRUCT(g, pboardobjgrp,
			clk, CLK, clk_freq_controller, CLK_FREQ_CONTROLLER);
	if (status) {
		gk20a_err(dev_from_gk20a(g),
			  "error constructing PMU_BOARDOBJ_CMD_GRP_SET interface - 0x%x",
			  status);
		goto done;
	}

	status = clk_get_freq_controller_table(g, pclk_freq_controllers);
	if (status) {
		gk20a_err(dev_from_gk20a(g),
			"error reading freq controller table - 0x%x",
			status);
		goto done;
	}

	BOARDOBJGRP_FOR_EACH(&(pclk_freq_controllers->super.super),
			     struct clk_freq_controller *, pclkfreqctrl, i) {
		pfll = NULL;
		j = 0;
		BOARDOBJGRP_FOR_EACH(&(pfllobjs->super.super),
			     struct fll_device *, pfll, j) {
			if (pclkfreqctrl->controller_id == pfll->id) {
				pfll->freq_ctrl_idx = i;
				break;
			}
		}
		boardobjgrpmask_bitset(&pclk_freq_controllers->
			freq_ctrl_load_mask.super, i);
	}
done:
		gk20a_dbg_info(" done status %x", status);
		return status;
}
