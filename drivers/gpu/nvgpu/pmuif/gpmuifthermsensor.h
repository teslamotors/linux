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

#ifndef _GPMUIFTHERMSENSOR_H_
#define _GPMUIFTHERMSENSOR_H_

#include "gk20a/gk20a.h"
#include "gk20a/pmu_gk20a.h"
#include "ctrl/ctrltherm.h"
#include "pmuif/gpmuifboardobj.h"
#include "gk20a/pmu_common.h"

#define NV_PMU_THERM_BOARDOBJGRP_CLASS_ID_THERM_DEVICE      0x00
#define NV_PMU_THERM_BOARDOBJGRP_CLASS_ID_THERM_CHANNEL     0x01

#define NV_PMU_THERM_CMD_ID_BOARDOBJ_GRP_SET                         0x0000000B
#define NV_PMU_THERM_MSG_ID_BOARDOBJ_GRP_SET                        0x00000008

struct nv_pmu_therm_therm_device_boardobjgrp_set_header {
	struct nv_pmu_boardobjgrp_e32 super;
};

struct nv_pmu_therm_therm_device_boardobj_set {
	struct nv_pmu_boardobj super;
};

struct nv_pmu_therm_therm_device_i2c_boardobj_set {
	struct nv_pmu_therm_therm_device_boardobj_set super;
	u8 i2c_dev_idx;
};

union nv_pmu_therm_therm_device_boardobj_set_union {
	struct nv_pmu_boardobj board_obj;
	struct nv_pmu_therm_therm_device_boardobj_set therm_device;
	struct nv_pmu_therm_therm_device_i2c_boardobj_set i2c;
};

NV_PMU_BOARDOBJ_GRP_SET_MAKE_E32(therm, therm_device);

struct nv_pmu_therm_therm_channel_boardobjgrp_set_header {
	struct nv_pmu_boardobjgrp_e32 super;
};

struct nv_pmu_therm_therm_channel_boardobj_set {
	struct nv_pmu_boardobj super;
	s16 scaling;
	s16 offset;
	s32 temp_min;
	s32 temp_max;
};

struct nv_pmu_therm_therm_channel_device_boardobj_set {
	struct nv_pmu_therm_therm_channel_boardobj_set super;
	u8 therm_dev_idx;
	u8 therm_dev_prov_idx;
};

union nv_pmu_therm_therm_channel_boardobj_set_union {
	struct nv_pmu_boardobj board_obj;
	struct nv_pmu_therm_therm_channel_boardobj_set therm_channel;
	struct nv_pmu_therm_therm_channel_device_boardobj_set device;
};

NV_PMU_BOARDOBJ_GRP_SET_MAKE_E32(therm, therm_channel);

#endif

