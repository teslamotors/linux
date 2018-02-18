/*
 * GP10B CDE
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

#include "gk20a/gk20a.h"
#include "cde_gp10b.h"

enum gp10b_programs {
	GP10B_PROG_HPASS              = 0,
	GP10B_PROG_HPASS_4K           = 1,
	GP10B_PROG_VPASS              = 2,
	GP10B_PROG_VPASS_4K           = 3,
	GP10B_PROG_HPASS_DEBUG        = 4,
	GP10B_PROG_HPASS_4K_DEBUG     = 5,
	GP10B_PROG_VPASS_DEBUG        = 6,
	GP10B_PROG_VPASS_4K_DEBUG     = 7,
	GP10B_PROG_PASSTHROUGH        = 8,
};

static void gp10b_cde_get_program_numbers(struct gk20a *g,
					  u32 block_height_log2,
					  int *hprog_out, int *vprog_out)
{
	int hprog, vprog;

	if (g->cde_app.shader_parameter == 1) {
		hprog = GP10B_PROG_PASSTHROUGH;
		vprog = GP10B_PROG_PASSTHROUGH;
	} else {
		hprog = GP10B_PROG_HPASS;
		vprog = GP10B_PROG_VPASS;
		if (g->cde_app.shader_parameter == 2) {
			hprog = GP10B_PROG_HPASS_DEBUG;
			vprog = GP10B_PROG_VPASS_DEBUG;
		}
		if (g->mm.bypass_smmu) {
			if (!g->mm.disable_bigpage) {
				gk20a_warn(g->dev,
					   "when bypass_smmu is 1, disable_bigpage must be 1 too");
			}
			hprog |= 1;
			vprog |= 1;
		}
	}

	*hprog_out = hprog;
	*vprog_out = vprog;
}

static bool gp10b_need_scatter_buffer(struct gk20a *g)
{
	return g->mm.bypass_smmu;
}

static u8 parity(u32 a)
{
	a ^= a>>16u;
	a ^= a>>8u;
	a ^= a>>4u;
	a &= 0xfu;
	return (0x6996u >> a) & 1u;
}

static int gp10b_populate_scatter_buffer(struct gk20a *g,
					 struct sg_table *sgt,
					 size_t surface_size,
					 void *scatter_buffer_ptr,
					 size_t scatter_buffer_size)
{
	/* map scatter buffer to CPU VA and fill it */
	const u32 page_size_log2 = 12;
	const u32 page_size = 1 << page_size_log2;
	const u32 page_size_shift = page_size_log2 - 7u;

	/* 0011 1111 1111 1111 1111 1110 0100 1000 */
	const u32 getSliceMaskGP10B = 0x3ffffe48;
	u8 *scatter_buffer = scatter_buffer_ptr;

	size_t i;
	struct scatterlist *sg = NULL;
	u8 d = 0;
	size_t page = 0;
	size_t pages_left;

	surface_size = round_up(surface_size, page_size);

	pages_left = surface_size >> page_size_log2;
	if ((pages_left >> 3) > scatter_buffer_size)
	    return -ENOMEM;

	for_each_sg(sgt->sgl, sg, sgt->nents, i) {
		unsigned int j;
		u64 surf_pa = sg_phys(sg);
		unsigned int n = (int)(sg->length >> page_size_log2);

		gk20a_dbg(gpu_dbg_cde, "surfPA=0x%llx + %d pages", surf_pa, n);

		for (j=0; j < n && pages_left > 0; j++, surf_pa += page_size) {
			u32 addr = (((u32)(surf_pa>>7)) & getSliceMaskGP10B) >> page_size_shift;
			u8 scatter_bit = parity(addr);
			u8 bit = page & 7;

			d |= scatter_bit << bit;
			if (bit == 7) {
				scatter_buffer[page >> 3] = d;
				d = 0;
			}

			++page;
			--pages_left;
		}

		if (pages_left == 0)
			break;
	}

	/* write the last byte in case the number of pages is not divisible by 8 */
	if ((page & 7) != 0)
		scatter_buffer[page >> 3] = d;

#if defined(GK20A_DEBUG)
	if (unlikely(gpu_dbg_cde & gk20a_dbg_mask)) {
		gk20a_dbg(gpu_dbg_cde, "scatterBuffer content:");
		for (i=0; i < page>>3; i++) {
			gk20a_dbg(gpu_dbg_cde, " %x", scatter_buffer[i]);
		}
	}
#endif
	return 0;
}

void gp10b_init_cde_ops(struct gpu_ops *gops)
{
	gops->cde.get_program_numbers = gp10b_cde_get_program_numbers;
	gops->cde.need_scatter_buffer = gp10b_need_scatter_buffer;
	gops->cde.populate_scatter_buffer = gp10b_populate_scatter_buffer;
}
