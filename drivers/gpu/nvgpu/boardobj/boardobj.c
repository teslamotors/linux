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
#include <linux/firmware.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include "boardobj.h"
#include "ctrl/ctrlboardobj.h"
#include "pmuif/gpmuifboardobj.h"


u32 boardobj_construct_super(struct gk20a *g, struct boardobj **ppboardobj,
				u16 size, void *args)
{
	struct boardobj  *pboardobj = NULL;
	struct boardobj  *devtmp = (struct boardobj *)args;

	gk20a_dbg_info(" ");

	if (devtmp == NULL)
		return -EINVAL;

	if (*ppboardobj == NULL) {
		*ppboardobj = kzalloc(size, GFP_KERNEL);
		if (ppboardobj == NULL)
			return -ENOMEM;
	}

	pboardobj = *ppboardobj;
	pboardobj->type = devtmp->type;
	pboardobj->idx = CTRL_BOARDOBJ_IDX_INVALID;
	pboardobj->type_mask   = BIT(pboardobj->type) | devtmp->type_mask;

	pboardobj->implements  = boardobj_implements_super;
	pboardobj->destruct    = boardobj_destruct_super;
	pboardobj->pmudatainit = boardobj_pmudatainit_super;

	return 0;
}

u32 boardobj_destruct_super(struct boardobj *pboardobj)
{
	gk20a_dbg_info("");
	if (pboardobj == NULL)
		return -EINVAL;
	kfree(pboardobj);
	return 0;
}

bool boardobj_implements_super(struct gk20a *g, struct boardobj *pboardobj,
	u8 type)
{
	gk20a_dbg_info("");

	return (0 != (pboardobj->type_mask & BIT(type)));
}

u32 boardobj_pmudatainit_super(struct gk20a *g, struct boardobj *pboardobj,
				struct nv_pmu_boardobj *pmudata)
{
	gk20a_dbg_info("");
	if (pboardobj == NULL)
		return -EINVAL;
	if (pmudata == NULL)
		return -EINVAL;
	pmudata->type = pboardobj->type;
	gk20a_dbg_info(" Done");
	return 0;
}
