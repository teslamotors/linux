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
#include <linux/delay.h>

#include "gk20a/gk20a.h"
#include "gp10b/fb_gp10b.h"
#include "hw_fb_gp106.h"

#define HW_SCRUB_TIMEOUT_DEFAULT	100 /* usec */
#define HW_SCRUB_TIMEOUT_MAX		2000000 /* usec */

static void gp106_fb_reset(struct gk20a *g)
{
	int retries = HW_SCRUB_TIMEOUT_MAX / HW_SCRUB_TIMEOUT_DEFAULT;
	/* wait for memory to be accessible */
	do {
		u32 w = gk20a_readl(g, fb_niso_scrub_status_r());
		if (fb_niso_scrub_status_flag_v(w)) {
			gk20a_dbg_fn("done");
			break;
		}
		udelay(HW_SCRUB_TIMEOUT_DEFAULT);
	} while (--retries);
}

void gp106_init_fb(struct gpu_ops *gops)
{
	gp10b_init_fb(gops);

	gops->fb.init_fs_state = NULL;
	gops->fb.reset = gp106_fb_reset;
}
