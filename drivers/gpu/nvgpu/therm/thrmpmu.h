/*
 * general thermal pmu control structures & definitions
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
#ifndef _THRMPMU_H_
#define _THRMPMU_H_

u32 therm_send_pmgr_tables_to_pmu(struct gk20a *g);

u32 therm_configure_therm_alert(struct gk20a *g);

#endif
