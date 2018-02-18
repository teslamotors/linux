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

#ifndef __SEC2_H_
#define __SEC2_H_

int sec2_clear_halt_interrupt_status(struct gk20a *g, unsigned int timeout);
int sec2_wait_for_halt(struct gk20a *g, unsigned int timeout);
void sec2_copy_to_dmem(struct pmu_gk20a *pmu,
		u32 dst, u8 *src, u32 size, u8 port);
void sec2_dump_falcon_stats(struct pmu_gk20a *pmu);
int bl_bootstrap_sec2(struct pmu_gk20a *pmu,
	void *desc, u32 bl_sz);
void sec_enable_irq(struct pmu_gk20a *pmu, bool enable);
void init_pmu_setup_hw1(struct gk20a *g);
int init_sec2_setup_hw1(struct gk20a *g,
		void *desc, u32 bl_sz);

#endif /*__SEC2_H_*/
