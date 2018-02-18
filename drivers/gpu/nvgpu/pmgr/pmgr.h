/*
 * general power device structures & definitions
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
#ifndef _PMGR_H_
#define _PMGR_H_

#include "pwrdev.h"
#include "pwrmonitor.h"
#include "pwrpolicy.h"

struct pmgr_pmupstate {
	struct pwr_devices pmgr_deviceobjs;
	struct pmgr_pwr_monitor pmgr_monitorobjs;
	struct pmgr_pwr_policy pmgr_policyobjs;
};

u32 pmgr_domain_sw_setup(struct gk20a *g);
u32 pmgr_domain_pmu_setup(struct gk20a *g);
int pmgr_pwr_devices_get_current(struct gk20a *g, u32 *val);
int pmgr_pwr_devices_get_voltage(struct gk20a *g, u32 *val);
int pmgr_pwr_devices_get_power(struct gk20a *g, u32 *val);

#endif
