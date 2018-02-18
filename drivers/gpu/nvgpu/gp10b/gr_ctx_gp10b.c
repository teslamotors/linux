/*
 * drivers/video/tegra/host/gp10b/gr_ctx_gp10b.c
 *
 * GM20B Graphics Context
 *
 * Copyright (c) 2015, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "gk20a/gk20a.h"
#include "gr_ctx_gp10b.h"

static int gr_gp10b_get_netlist_name(struct gk20a *g, int index, char *name)
{
	switch (index) {
#ifdef GP10B_NETLIST_IMAGE_FW_NAME
	case NETLIST_FINAL:
		sprintf(name, GP10B_NETLIST_IMAGE_FW_NAME);
		return 0;
#endif
#ifdef GK20A_NETLIST_IMAGE_A
	case NETLIST_SLOT_A:
		sprintf(name, GK20A_NETLIST_IMAGE_A);
		return 0;
#endif
#ifdef GK20A_NETLIST_IMAGE_B
	case NETLIST_SLOT_B:
		sprintf(name, GK20A_NETLIST_IMAGE_B);
		return 0;
#endif
#ifdef GK20A_NETLIST_IMAGE_C
	case NETLIST_SLOT_C:
		sprintf(name, GK20A_NETLIST_IMAGE_C);
		return 0;
#endif
#ifdef GK20A_NETLIST_IMAGE_D
	case NETLIST_SLOT_D:
		sprintf(name, GK20A_NETLIST_IMAGE_D);
		return 0;
#endif
	default:
		return -1;
	}

	return -1;
}

static bool gr_gp10b_is_firmware_defined(void)
{
#ifdef GP10B_NETLIST_IMAGE_FW_NAME
	return true;
#else
	return false;
#endif
}

void gp10b_init_gr_ctx(struct gpu_ops *gops) {
	gops->gr_ctx.get_netlist_name = gr_gp10b_get_netlist_name;
	gops->gr_ctx.is_fw_defined = gr_gp10b_is_firmware_defined;
	gops->gr_ctx.use_dma_for_fw_bootstrap = true;
}
