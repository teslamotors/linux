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

#ifndef _CLKVIN_H_
#define _CLKVIN_H_

#include "boardobj/boardobj.h"
#include "boardobj/boardobjgrp.h"
#include "clk.h"

struct vin_device;
struct clk_pmupstate;

struct avfsvinobjs {
	struct boardobjgrp_e32 super;
	u8 calibration_rev_vbios;
	u8 calibration_rev_fused;
	bool vin_is_disable_allowed;
};
typedef u32 vin_device_state_load(struct gk20a *g,
			struct clk_pmupstate *clk, struct vin_device *pdev);

struct vin_device {
	struct boardobj super;
	u8 id;
	u8 volt_domain;
	u8 volt_domain_vbios;
	u32 slope;
	u32 intercept;
	u32 flls_shared_mask;

	vin_device_state_load  *state_load;
};

/* get vin device object from descriptor table index*/
#define CLK_GET_VIN_DEVICE(pvinobjs, dev_index)                               \
	((struct vin_device *)BOARDOBJGRP_OBJ_GET_BY_IDX(                       \
	((struct boardobjgrp *)&(pvinobjs->super.super)), (dev_index)))

boardobj_construct construct_vindevice;
boardobj_pmudatainit vindeviceinit_pmudata_super;

u32 clk_vin_sw_setup(struct gk20a *g);
u32 clk_vin_pmu_setup(struct gk20a *g);

#endif
