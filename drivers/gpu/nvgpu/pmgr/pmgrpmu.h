/*
 * general power device control structures & definitions
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
#ifndef _PMGRPMU_H_
#define _PMGRPMU_H_

#include "gk20a/gk20a.h"
#include "pwrdev.h"
#include "pwrmonitor.h"

u32 pmgr_send_pmgr_tables_to_pmu(struct gk20a *g);

u32 pmgr_pmu_pwr_devices_query_blocking(
		struct gk20a *g,
		u32 pwr_dev_mask,
		struct nv_pmu_pmgr_pwr_devices_query_payload *ppayload);

#endif
