/*
 * general thermal table structures & definitions
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
#ifndef _THRM_H_
#define _THRM_H_

#include "thrmdev.h"
#include "thrmchannel.h"

struct therm_pmupstate {
	struct therm_devices therm_deviceobjs;
	struct therm_channels therm_channelobjs;
};

u32 therm_domain_sw_setup(struct gk20a *g);
u32 therm_domain_pmu_setup(struct gk20a *g);

#endif
