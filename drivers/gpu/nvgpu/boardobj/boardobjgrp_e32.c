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
#include "boardobj.h"
#include "boardobjgrp.h"
#include "boardobjgrp_e32.h"
#include "ctrl/ctrlboardobj.h"
#include "pmuif/gpmuifboardobj.h"
#include "boardobjgrpmask.h"


u32  boardobjgrpconstruct_e32(struct boardobjgrp_e32 *pboardobjgrp_e32)
{
	u32 status;
	u8  objslots;

	gk20a_dbg_info("");
	objslots = 32;

	status = boardobjgrpmask_e32_init(&pboardobjgrp_e32->mask, NULL);
	if (status)
		goto boardobjgrpconstruct_e32_exit;

	pboardobjgrp_e32->super.type      = CTRL_BOARDOBJGRP_TYPE_E32;
	pboardobjgrp_e32->super.ppobjects = pboardobjgrp_e32->objects;
	pboardobjgrp_e32->super.objslots  = objslots;
	pboardobjgrp_e32->super.mask     = &(pboardobjgrp_e32->mask.super);

	status = boardobjgrp_construct_super(&pboardobjgrp_e32->super);
	if (status)
		goto boardobjgrpconstruct_e32_exit;

	pboardobjgrp_e32->super.destruct = boardobjgrpdestruct_e32;

	pboardobjgrp_e32->super.pmuhdrdatainit = boardobjgrp_pmuhdrdatainit_e32;

boardobjgrpconstruct_e32_exit:
	return status;
}

u32 boardobjgrpdestruct_e32(struct boardobjgrp *pboardobjgrp)
{
	u32 status = 0;

	gk20a_dbg_info("");

	pboardobjgrp->mask     = NULL;
	pboardobjgrp->objslots  = 0;
	pboardobjgrp->ppobjects = NULL;

	return status;
}

u32 boardobjgrp_pmuhdrdatainit_e32(struct gk20a *g,
		struct boardobjgrp *pboardobjgrp,
		struct nv_pmu_boardobjgrp_super *pboardobjgrppmu,
		struct boardobjgrpmask *mask)
{
	struct nv_pmu_boardobjgrp_e32 *pgrpe32 =
		(struct nv_pmu_boardobjgrp_e32 *)pboardobjgrppmu;
	u32 status;

	gk20a_dbg_info("");

	if (pboardobjgrp == NULL)
		return -EINVAL;

	if (pboardobjgrppmu == NULL)
		return -EINVAL;
	status = boardobjgrpmask_export(mask,
				mask->bitcount,
				&pgrpe32->obj_mask.super);
	if (status) {
		gk20a_err(dev_from_gk20a(g), "e32 init:failed export grpmask");
		return status;
	}

	return boardobjgrp_pmuhdrdatainit_super(g,
			pboardobjgrp, pboardobjgrppmu, mask);
}
