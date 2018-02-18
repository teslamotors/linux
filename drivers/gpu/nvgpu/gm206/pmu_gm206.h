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

#ifndef __PMU_GM206_H_
#define __PMU_GM206_H_

void gm206_init_pmu_ops(struct gpu_ops *gops);
bool gm206_is_priv_load(u32 falcon_id);
bool gm206_is_lazy_bootstrap(u32 falcon_id);
int gm206_load_falcon_ucode(struct gk20a *g, u32 falconidmask);

#endif /*__PMU_GM206_H_*/
