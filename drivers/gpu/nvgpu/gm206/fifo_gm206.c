/*
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

#include <linux/delay.h>
#include <linux/types.h>

#include "gk20a/gk20a.h"
#include "gm20b/fifo_gm20b.h"
#include "fifo_gm206.h"
#include "hw_ccsr_gm206.h"
#include "hw_fifo_gm206.h"

static u32 gm206_fifo_get_num_fifos(struct gk20a *g)
{
	return ccsr_channel__size_1_v();
}

void gm206_init_fifo(struct gpu_ops *gops)
{
	gm20b_init_fifo(gops);
	gops->fifo.get_num_fifos = gm206_fifo_get_num_fifos;
	gops->fifo.eng_runlist_base_size = fifo_eng_runlist_base__size_1_v;
}
