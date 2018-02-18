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

#ifndef _BOARDOBJ_H_
#define _BOARDOBJ_H_

struct boardobj;

#include <linux/nvgpu.h>
#include "gk20a/gk20a.h"
#include "gk20a/pmu_gk20a.h"
#include "ctrl/ctrlboardobj.h"
#include "pmuif/gpmuifboardobj.h"

/*
* check whether the specified BOARDOBJ object implements the queried
* type/class enumeration.
*/
typedef bool boardobj_implements(struct gk20a *g, struct boardobj *pboardobj,
					u8 type);

/*
* Fills out the appropriate the nv_pmu_xxxx_device_desc_<xyz> driver->PMU
* description structure, describing this BOARDOBJ board device to the PMU.
*
*/
typedef u32 boardobj_pmudatainit(struct gk20a *g, struct boardobj *pboardobj,
				struct nv_pmu_boardobj *pmudata);

/*
* Constructor for the base Board Object. Called by each device-specific
* implementation of the BOARDOBJ interface to initialize the board object.
*/
typedef u32 boardobj_construct(struct gk20a *g, struct boardobj **pboardobj,
				u16 size, void *args);

/*
* Destructor for the base board object. Called by each device-Specific
* implementation of the BOARDOBJ interface to destroy the board object.
* This has to be explicitly set by each device that extends from the
* board object.
*/
typedef u32 boardobj_destruct(struct boardobj *pboardobj);

/*
* Base Class for all physical or logical device on the PCB.
* Contains fields common to all devices on the board. Specific types of
* devices may extend this object adding any details specific to that
* device or device-type.
*/

struct boardobj {
	u8 type; /*type of the device*/
	u8 idx;  /*index of boardobj within in its group*/
	u32 type_mask; /*mask of types this boardobjimplements*/
	boardobj_implements  *implements;
	boardobj_destruct    *destruct;
	/*
	* Access interface apis which will be overridden by the devices
	* that inherit from BOARDOBJ
	*/
	boardobj_pmudatainit *pmudatainit;
};

boardobj_construct   boardobj_construct_super;
boardobj_destruct    boardobj_destruct_super;
boardobj_implements  boardobj_implements_super;
boardobj_pmudatainit boardobj_pmudatainit_super;

#define BOARDOBJ_GET_TYPE(pobj) (((struct boardobj *)(pobj))->type)
#define BOARDOBJ_GET_IDX(pobj) (((struct boardobj *)(pobj))->idx)

#endif
