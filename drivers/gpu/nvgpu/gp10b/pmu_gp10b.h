/*
 * GP10B PMU
 *
 * Copyright (c) 2015-2016, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __PMU_GP10B_H_
#define __PMU_GP10B_H_

void gp10b_init_pmu_ops(struct gpu_ops *gops);
int gp10b_load_falcon_ucode(struct gk20a *g, u32 falconidmask);
int gp10b_pg_gr_init(struct gk20a *g, u32 pg_engine_id);
void gp10b_write_dmatrfbase(struct gk20a *g, u32 addr);

#endif /*__PMU_GP10B_H_*/
