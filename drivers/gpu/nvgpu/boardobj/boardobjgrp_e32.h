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

#ifndef _BOARDOBJGRP_E32_H_
#define _BOARDOBJGRP_E32_H_

#include <linux/nvgpu.h>
#include "gk20a/gk20a.h"
#include "gk20a/pmu_gk20a.h"
#include "ctrl/ctrlboardobj.h"
#include "boardobj.h"
#include "boardobjgrp.h"
#include "boardobjgrpmask.h"
#include "boardobj/boardobjgrp.h"

/*
 * boardobjgrp_e32 is @ref BOARDOBJGRP child class allowing storage of up to 32
 * @ref BOARDOBJ object pointers with single static 32-bit mask denoting valid
 * object pointers.
 */
struct boardobjgrp_e32 {
	/*
	* BOARDOBJGRP super-class. Must be first element of the structure.
	*/
	struct boardobjgrp super;
	/*
	* Statically allocated array of PBOARDOBJ-s
	*/
	struct boardobj  *objects[CTRL_BOARDOBJGRP_E32_MAX_OBJECTS];

	/*
	* Statically allocated mask strcuture referenced by super::pMask.
	*/
	struct boardobjgrpmask_e32 mask;
};

/*
 * Wrapper to the _SUPER implementation.  Provided for the child classes which
 * implement this interface.
 */
#define boardobjgrp_pmudatainit_e32(g, pboardpbjgrp, pboardobjgrppmu) \
		boardobjgrp_pmudatainit_super(g, pboardpbjgrp, pboardobjgrppmu)

/* Constructor and destructor */
u32 boardobjgrpconstruct_e32(struct boardobjgrp_e32 *pboardobjgrp);
boardobjgrp_destruct boardobjgrpdestruct_e32;
boardobjgrp_pmuhdrdatainit  boardobjgrp_pmuhdrdatainit_e32;

#endif
