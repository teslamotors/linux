/*
 * GP106 Graphics Context
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

#include "gk20a/gk20a.h"
#include "gr_ctx_gp106.h"
#include "nvgpu_gpuid_t18x.h"

static int gr_gp106_get_netlist_name(struct gk20a *g, int index, char *name)
{
	u32 ver = g->gpu_characteristics.arch + g->gpu_characteristics.impl;

	switch (ver) {
		case NVGPU_GPUID_GP104:
			sprintf(name, "%s/%s", "gp104",
					GP104_NETLIST_IMAGE_FW_NAME);
			break;
		case NVGPU_GPUID_GP106:
			sprintf(name, "%s/%s", "gp106",
					GP106_NETLIST_IMAGE_FW_NAME);
			break;
		default:
			gk20a_err(g->dev, "no support for GPUID %x", ver);
	}

	return 0;
}

static bool gr_gp106_is_firmware_defined(void)
{
	return true;
}

void gp106_init_gr_ctx(struct gpu_ops *gops)
{
	gops->gr_ctx.get_netlist_name = gr_gp106_get_netlist_name;
	gops->gr_ctx.is_fw_defined = gr_gp106_is_firmware_defined;
	gops->gr_ctx.use_dma_for_fw_bootstrap = false;
}
