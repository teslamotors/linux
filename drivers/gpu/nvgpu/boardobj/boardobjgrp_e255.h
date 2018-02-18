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

#ifndef _BOARDOBJGRP_E255_H_
#define _BOARDOBJGRP_E255_H_

#include <linux/nvgpu.h>
#include "gk20a/gk20a.h"
#include "gk20a/pmu_gk20a.h"
#include "ctrl/ctrlboardobj.h"
#include "boardobj.h"
#include "boardobjgrpmask.h"
#include "boardobj/boardobjgrp.h"
#include "boardobjgrp_e255.h"

/*
 * boardobjgrp_e255 is @ref BOARDOBJGRP child class allowing storage of up
 * to 255 @ref BOARDOBJ object pointers with single static 255-bit mask denoting
 * valid object pointers.
 */
struct boardobjgrp_e255 {
	struct boardobjgrp  super;
	struct boardobj *objects[CTRL_BOARDOBJGRP_E255_MAX_OBJECTS];
	struct boardobjgrpmask_e255  mask;
};

#define boardobjgrp_pmudatainit_e255(g, pboardpbjgrp, pboardobjgrppmu) \
		boardobjgrp_pmudatainit_super(g, pboardpbjgrp, pboardobjgrppmu)

/* Constructor and destructor */
u32 boardobjgrpconstruct_e255(struct boardobjgrp_e255 *pboardobjgrp);
boardobjgrp_destruct boardobjgrpdestruct_e255;
boardobjgrp_pmuhdrdatainit  boardobjgrp_pmuhdrdatainit_e255;

#endif
