/*
 * general thermal device structures & definitions
 *
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
#ifndef _THRMCHANNEL_H_
#define _THRMCHANNEL_H_

#include "boardobj/boardobj.h"
#include "boardobj/boardobjgrp.h"
#include "ctrl/ctrltherm.h"

struct therm_channel {
	struct boardobj super;
	s16 scaling;
	s16 offset;
	s32 temp_min;
	s32 temp_max;
};

struct therm_channels {
	struct boardobjgrp_e32 super;
};

struct therm_channel_device {
	struct therm_channel super;
	u8 therm_dev_idx;
	u8 therm_dev_prov_idx;
};

u32 therm_channel_sw_setup(struct gk20a *g);

#endif
