/*
 * GK20A memory interface
 *
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/types.h>

#include "gk20a/gk20a.h"
#include "gk20a/mc_gk20a.h"
#include "mc_gm20b.h"

void gm20b_init_mc(struct gpu_ops *gops)
{
	gops->mc.intr_enable = mc_gk20a_intr_enable;
	gops->mc.intr_unit_config = mc_gk20a_intr_unit_config;
	gops->mc.isr_stall = mc_gk20a_isr_stall;
	gops->mc.isr_nonstall = mc_gk20a_isr_nonstall;
	gops->mc.isr_thread_stall = mc_gk20a_intr_thread_stall;
	gops->mc.isr_thread_nonstall = mc_gk20a_intr_thread_nonstall;
}
