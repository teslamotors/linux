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
#include "clk_vin.h"
#include "include/bios.h"
#include "boardobj/boardobjgrp.h"
#include "boardobj/boardobjgrp_e32.h"
#include "pmuif/gpmuifboardobj.h"
#include "pmuif/gpmuifclk.h"
#include "gm206/bios_gm206.h"
#include "ctrl/ctrlvolt.h"
#include "gk20a/pmu_gk20a.h"
#include "gp106/hw_fuse_gp106.h"

static u32 devinit_get_vin_device_table(struct gk20a *g,
		struct avfsvinobjs *pvinobjs);

static struct vin_device *construct_vin_device(struct gk20a *g, void *pargs);

static u32 vin_device_init_pmudata_super(struct gk20a *g,
				  struct boardobj *board_obj_ptr,
				  struct nv_pmu_boardobj *ppmudata);

static u32 read_vin_cal_fuse_rev(struct gk20a *g)
{
	return fuse_vin_cal_fuse_rev_v(
		gk20a_readl(g, fuse_vin_cal_fuse_rev_r()));
}

static u32 read_vin_cal_slope_intercept_fuse(struct gk20a *g,
					     u32 vin_id, u32 *slope,
					     u32 *intercept)
{
	u32 data = 0;
	u32 interceptdata = 0;
	u32 slopedata = 0;
	u32 gpc0data;
	u32 gpc0slopedata;
	u32 gpc0interceptdata;

	/* read gpc0 irrespective of vin id */
	gpc0data = gk20a_readl(g, fuse_vin_cal_gpc0_r());
	if (gpc0data == 0xFFFFFFFF)
		return -EINVAL;

	switch (vin_id) {
	case CTRL_CLK_VIN_ID_GPC0:
		break;

	case CTRL_CLK_VIN_ID_GPC1:
		data = gk20a_readl(g, fuse_vin_cal_gpc1_delta_r());
		break;

	case CTRL_CLK_VIN_ID_GPC2:
		data = gk20a_readl(g, fuse_vin_cal_gpc2_delta_r());
		break;

	case CTRL_CLK_VIN_ID_GPC3:
		data = gk20a_readl(g, fuse_vin_cal_gpc3_delta_r());
		break;

	case CTRL_CLK_VIN_ID_GPC4:
		data = gk20a_readl(g, fuse_vin_cal_gpc4_delta_r());
		break;

	case CTRL_CLK_VIN_ID_GPC5:
		data = gk20a_readl(g, fuse_vin_cal_gpc5_delta_r());
		break;

	case CTRL_CLK_VIN_ID_SYS:
	case CTRL_CLK_VIN_ID_XBAR:
	case CTRL_CLK_VIN_ID_LTC:
		data = gk20a_readl(g, fuse_vin_cal_shared_delta_r());
		break;

	case CTRL_CLK_VIN_ID_SRAM:
		data = gk20a_readl(g, fuse_vin_cal_sram_delta_r());
		break;

	default:
		return -EINVAL;
	}
	if (data == 0xFFFFFFFF)
		return -EINVAL;

	gpc0interceptdata = fuse_vin_cal_gpc0_icpt_data_v(gpc0data) * 1000;
	gpc0interceptdata = gpc0interceptdata >>
		fuse_vin_cal_gpc0_icpt_frac_size_v();

	switch (vin_id) {
	case CTRL_CLK_VIN_ID_GPC0:
		break;

	case CTRL_CLK_VIN_ID_GPC1:
	case CTRL_CLK_VIN_ID_GPC2:
	case CTRL_CLK_VIN_ID_GPC3:
	case CTRL_CLK_VIN_ID_GPC4:
	case CTRL_CLK_VIN_ID_GPC5:
	case CTRL_CLK_VIN_ID_SYS:
	case CTRL_CLK_VIN_ID_XBAR:
	case CTRL_CLK_VIN_ID_LTC:
		interceptdata =
			(fuse_vin_cal_gpc1_icpt_data_v(data)) * 1000;
		interceptdata = interceptdata >>
			fuse_vin_cal_gpc1_icpt_frac_size_v();
		break;

	case CTRL_CLK_VIN_ID_SRAM:
		interceptdata =
			(fuse_vin_cal_sram_icpt_data_v(data)) * 1000;
		interceptdata = interceptdata >>
			fuse_vin_cal_sram_icpt_frac_size_v();
		break;

	default:
		return -EINVAL;
	}

	if (data & fuse_vin_cal_gpc1_icpt_sign_f())
		*intercept = gpc0interceptdata - interceptdata;
	else
		*intercept = gpc0interceptdata + interceptdata;

	/* slope */
	gpc0slopedata = (fuse_vin_cal_gpc0_slope_data_v(gpc0data)) * 1000;
	gpc0slopedata = gpc0slopedata >>
		fuse_vin_cal_gpc0_slope_frac_size_v();

	switch (vin_id) {
	case CTRL_CLK_VIN_ID_GPC0:
		break;

	case CTRL_CLK_VIN_ID_GPC1:
	case CTRL_CLK_VIN_ID_GPC2:
	case CTRL_CLK_VIN_ID_GPC3:
	case CTRL_CLK_VIN_ID_GPC4:
	case CTRL_CLK_VIN_ID_GPC5:
	case CTRL_CLK_VIN_ID_SYS:
	case CTRL_CLK_VIN_ID_XBAR:
	case CTRL_CLK_VIN_ID_LTC:
	case CTRL_CLK_VIN_ID_SRAM:
		slopedata =
			(fuse_vin_cal_gpc1_slope_data_v(data)) * 1000;
		slopedata = slopedata >>
			fuse_vin_cal_gpc1_slope_frac_size_v();
		break;

	default:
		return -EINVAL;
	}

	if (data & fuse_vin_cal_gpc1_slope_sign_f())
		*slope = gpc0slopedata - slopedata;
	else
		*slope = gpc0slopedata + slopedata;
	return 0;
}

static u32 _clk_vin_devgrp_pmudatainit_super(struct gk20a *g,
					     struct boardobjgrp *pboardobjgrp,
					     struct nv_pmu_boardobjgrp_super *pboardobjgrppmu)
{
	struct nv_pmu_clk_clk_vin_device_boardobjgrp_set_header *pset =
		(struct nv_pmu_clk_clk_vin_device_boardobjgrp_set_header *)
		pboardobjgrppmu;
	struct avfsvinobjs *pvin_obbj = (struct avfsvinobjs *)pboardobjgrp;
	u32 status = 0;

	gk20a_dbg_info("");

	status = boardobjgrp_pmudatainit_e32(g, pboardobjgrp, pboardobjgrppmu);

	pset->b_vin_is_disable_allowed = pvin_obbj->vin_is_disable_allowed;

	gk20a_dbg_info(" Done");
	return status;
}

static u32 _clk_vin_devgrp_pmudata_instget(struct gk20a *g,
					   struct nv_pmu_boardobjgrp *pmuboardobjgrp,
					   struct nv_pmu_boardobj **ppboardobjpmudata,
					   u8 idx)
{
	struct nv_pmu_clk_clk_vin_device_boardobj_grp_set *pgrp_set =
		(struct nv_pmu_clk_clk_vin_device_boardobj_grp_set *)
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

static u32 _clk_vin_devgrp_pmustatus_instget(struct gk20a *g,
					     void *pboardobjgrppmu,
					     struct nv_pmu_boardobj_query **ppboardobjpmustatus,
					     u8 idx)
{
	struct nv_pmu_clk_clk_vin_device_boardobj_grp_get_status *pgrp_get_status =
		(struct nv_pmu_clk_clk_vin_device_boardobj_grp_get_status *)
		pboardobjgrppmu;

	/*check whether pmuboardobjgrp has a valid boardobj in index*/
	if (((u32)BIT(idx) &
		pgrp_get_status->hdr.data.super.obj_mask.super.data[0]) == 0)
		return -EINVAL;

	*ppboardobjpmustatus = (struct nv_pmu_boardobj_query *)
			&pgrp_get_status->objects[idx].data.board_obj;
	return 0;
}

u32 clk_vin_sw_setup(struct gk20a *g)
{
	u32 status;
	struct boardobjgrp *pboardobjgrp = NULL;
	u32 slope;
	u32 intercept;
	struct vin_device *pvindev;
	struct avfsvinobjs *pvinobjs;
	u8 i;

	gk20a_dbg_info("");

	status = boardobjgrpconstruct_e32(&g->clk_pmu.avfs_vinobjs.super);
	if (status) {
		gk20a_err(dev_from_gk20a(g),
			"error creating boardobjgrp for clk vin, statu - 0x%x",
			status);
		goto done;
	}

	pboardobjgrp = &g->clk_pmu.avfs_vinobjs.super.super;
	pvinobjs = &g->clk_pmu.avfs_vinobjs;

	BOARDOBJGRP_PMU_CONSTRUCT(pboardobjgrp, CLK, VIN_DEVICE);

	status = BOARDOBJGRP_PMU_CMD_GRP_SET_CONSTRUCT(g, pboardobjgrp,
			clk, CLK, clk_vin_device, CLK_VIN_DEVICE);
	if (status) {
		gk20a_err(dev_from_gk20a(g),
			"error constructing PMU_BOARDOBJ_CMD_GRP_SET interface - 0x%x",
			status);
		goto done;
	}

	pboardobjgrp->pmudatainit  = _clk_vin_devgrp_pmudatainit_super;
	pboardobjgrp->pmudatainstget  = _clk_vin_devgrp_pmudata_instget;
	pboardobjgrp->pmustatusinstget  = _clk_vin_devgrp_pmustatus_instget;

	status = devinit_get_vin_device_table(g, &g->clk_pmu.avfs_vinobjs);
	if (status)
		goto done;

	/*update vin calibration to fuse */
	if (pvinobjs->calibration_rev_vbios == read_vin_cal_fuse_rev(g)) {
		BOARDOBJGRP_FOR_EACH(&(pvinobjs->super.super),
				     struct vin_device *, pvindev, i) {
			slope = 0;
			intercept = 0;
			pvindev = CLK_GET_VIN_DEVICE(pvinobjs, i);
			status = read_vin_cal_slope_intercept_fuse(g,
					pvindev->id, &slope, &intercept);
			if (status) {
				gk20a_err(dev_from_gk20a(g),
				"err reading vin cal for id %x", pvindev->id);
				goto done;
			}
			if (slope != 0 && intercept != 0) {
				pvindev->slope = slope;
				pvindev->intercept = intercept;
			}
		}
	}
	status = BOARDOBJGRP_PMU_CMD_GRP_GET_STATUS_CONSTRUCT(g,
				&g->clk_pmu.avfs_vinobjs.super.super,
				clk, CLK, clk_vin_device, CLK_VIN_DEVICE);
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

u32 clk_vin_pmu_setup(struct gk20a *g)
{
	u32 status;
	struct boardobjgrp *pboardobjgrp = NULL;

	gk20a_dbg_info("");

	pboardobjgrp = &g->clk_pmu.avfs_vinobjs.super.super;

	if (!pboardobjgrp->bconstructed)
		return -EINVAL;

	status = pboardobjgrp->pmuinithandle(g, pboardobjgrp);

	gk20a_dbg_info("Done");
	return status;
}

static u32 devinit_get_vin_device_table(struct gk20a *g,
		struct avfsvinobjs *pvinobjs)
{
	u32 status = 0;
	u8 *vin_table_ptr = NULL;
	struct vin_descriptor_header_10 vin_desc_table_header = { 0 };
	struct vin_descriptor_entry_10 vin_desc_table_entry = { 0 };
	u8 *vin_tbl_entry_ptr = NULL;
	u32 index = 0;
	u32 slope, intercept;
	struct vin_device vin_dev_data;
	struct vin_device *pvin_dev;

	gk20a_dbg_info("");

	if (g->ops.bios.get_perf_table_ptrs) {
		vin_table_ptr = (u8 *)g->ops.bios.get_perf_table_ptrs(g,
				g->bios.clock_token, VIN_TABLE);
		if (vin_table_ptr == NULL) {
			status = -1;
			goto done;
		}
	}

	memcpy(&vin_desc_table_header, vin_table_ptr,
	       sizeof(struct vin_descriptor_header_10));

	pvinobjs->calibration_rev_vbios =
			BIOS_GET_FIELD(vin_desc_table_header.flags0,
				NV_VIN_DESC_FLAGS0_VIN_CAL_REVISION);
	pvinobjs->vin_is_disable_allowed =
			BIOS_GET_FIELD(vin_desc_table_header.flags0,
				NV_VIN_DESC_FLAGS0_DISABLE_CONTROL);

	/* VIN calibration slope: XX.YYY mV/code => XXYYY uV/code*/
	slope = ((BIOS_GET_FIELD(vin_desc_table_header.vin_cal,
			NV_VIN_DESC_VIN_CAL_SLOPE_INTEGER) * 1000)) +
		((BIOS_GET_FIELD(vin_desc_table_header.vin_cal,
			NV_VIN_DESC_VIN_CAL_SLOPE_FRACTION)));

	/* VIN calibration intercept: ZZZ.W mV => ZZZW00 uV */
	intercept = ((BIOS_GET_FIELD(vin_desc_table_header.vin_cal,
			NV_VIN_DESC_VIN_CAL_INTERCEPT_INTEGER) * 1000)) +
		    ((BIOS_GET_FIELD(vin_desc_table_header.vin_cal,
			NV_VIN_DESC_VIN_CAL_INTERCEPT_FRACTION) * 100));

	/* Read table entries*/
	vin_tbl_entry_ptr = vin_table_ptr + vin_desc_table_header.header_sizee;
	for (index = 0; index < vin_desc_table_header.entry_count; index++) {
		u32 vin_id;

		memcpy(&vin_desc_table_entry, vin_tbl_entry_ptr,
		       sizeof(struct vin_descriptor_entry_10));

		if (vin_desc_table_entry.vin_device_type == CTRL_CLK_VIN_TYPE_DISABLED)
			continue;

		vin_id = vin_desc_table_entry.vin_device_id;

		vin_dev_data.super.type =
			(u8)vin_desc_table_entry.vin_device_type;
		vin_dev_data.id = (u8)vin_desc_table_entry.vin_device_id;
		vin_dev_data.volt_domain_vbios =
			(u8)vin_desc_table_entry.volt_domain_vbios;
		vin_dev_data.slope = slope;
		vin_dev_data.intercept = intercept;

		vin_dev_data.flls_shared_mask = 0;

		pvin_dev = construct_vin_device(g, (void *)&vin_dev_data);

		status = boardobjgrp_objinsert(&pvinobjs->super.super,
				(struct boardobj *)pvin_dev, index);

		vin_tbl_entry_ptr += vin_desc_table_header.entry_size;
	}

done:
	gk20a_dbg_info(" done status %x", status);
	return status;
}

static struct vin_device *construct_vin_device(struct gk20a *g, void *pargs)
{
	struct boardobj *board_obj_ptr = NULL;
	struct vin_device *pvin_dev;
	struct vin_device *board_obj_vin_ptr = NULL;
	u32 status;

	gk20a_dbg_info("");
	status = boardobj_construct_super(g, &board_obj_ptr,
		sizeof(struct vin_device), pargs);
	if (status)
		return NULL;

	/*got vin board obj allocated now fill it into boardobj grp*/
	pvin_dev = (struct vin_device *)pargs;
	board_obj_vin_ptr = (struct vin_device *)board_obj_ptr;
	/* override super class interface */
	board_obj_ptr->pmudatainit = vin_device_init_pmudata_super;
	board_obj_vin_ptr->id = pvin_dev->id;
	board_obj_vin_ptr->volt_domain_vbios = pvin_dev->volt_domain_vbios;
	board_obj_vin_ptr->slope = pvin_dev->slope;
	board_obj_vin_ptr->intercept = pvin_dev->intercept;
	board_obj_vin_ptr->flls_shared_mask = pvin_dev->flls_shared_mask;
	board_obj_vin_ptr->volt_domain = CTRL_VOLT_DOMAIN_LOGIC;

	gk20a_dbg_info(" Done");

	return (struct vin_device *)board_obj_ptr;
}

static u32 vin_device_init_pmudata_super(struct gk20a *g,
					 struct boardobj *board_obj_ptr,
					 struct nv_pmu_boardobj *ppmudata)
{
	u32 status = 0;
	struct vin_device *pvin_dev;
	struct nv_pmu_clk_clk_vin_device_boardobj_set *perf_pmu_data;

	gk20a_dbg_info("");

	status = boardobj_pmudatainit_super(g, board_obj_ptr, ppmudata);
	if (status != 0)
		return status;

	pvin_dev = (struct vin_device *)board_obj_ptr;
	perf_pmu_data = (struct nv_pmu_clk_clk_vin_device_boardobj_set *)
		ppmudata;

	perf_pmu_data->id = pvin_dev->id;
	perf_pmu_data->intercept = pvin_dev->intercept;
	perf_pmu_data->volt_domain = pvin_dev->volt_domain;
	perf_pmu_data->slope = pvin_dev->slope;
	perf_pmu_data->flls_shared_mask = pvin_dev->flls_shared_mask;

	gk20a_dbg_info(" Done");

	return status;
}
