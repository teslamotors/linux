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

#include "gk20a/gk20a.h"
#include "gp10b/fifo_gp10b.h"
#include "fifo_gp106.h"
#include "hw_ccsr_gp106.h"
#include "hw_fifo_gp106.h"

static u32 gp106_fifo_get_num_fifos(struct gk20a *g)
{
	return ccsr_channel__size_1_v();
}

void gp106_init_fifo(struct gpu_ops *gops)
{
	gp10b_init_fifo(gops);
	gops->fifo.get_num_fifos = gp106_fifo_get_num_fifos;
	gops->fifo.eng_runlist_base_size = fifo_eng_runlist_base__size_1_v;
}
