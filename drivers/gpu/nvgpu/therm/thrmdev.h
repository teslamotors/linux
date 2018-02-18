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
#ifndef _THRMDEV_H_
#define _THRMDEV_H_

#include "boardobj/boardobj.h"
#include "boardobj/boardobjgrp.h"

struct therm_devices {
	struct boardobjgrp_e32 super;
};

struct therm_device {
	struct therm_devices super;
};

u32 therm_device_sw_setup(struct gk20a *g);

#endif
