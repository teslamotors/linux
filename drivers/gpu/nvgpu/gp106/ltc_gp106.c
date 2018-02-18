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

#include <linux/types.h>

#include "gk20a/gk20a.h"
#include "gm20b/ltc_gm20b.h"
#include "gp10b/ltc_gp10b.h"

void gp106_init_ltc(struct gpu_ops *gops)
{
	gp10b_init_ltc(gops);

	/* dGPU does not need the LTC hack */
	gops->ltc.cbc_fix_config = NULL;
	gops->ltc.init_cbc = NULL;
	gops->ltc.init_fs_state = gm20b_ltc_init_fs_state;
}
