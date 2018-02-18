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

#include "gk20a/gk20a.h"
#include "gm206/bios_gm206.h"
#include "bios_gp106.h"
#include "hw_gc6_gp106.h"

static void gp106_init_xmemsel_zm_nv_reg_array(struct gk20a *g, bool *condition,
	u32 reg, u32 stride, u32 count, u32 data_table_offset)
{
	u8 i;
	u32 data, strap, index;

	if (*condition) {

		strap = gk20a_readl(g, gc6_sci_strap_r()) & 0xf;

		index = g->bios.mem_strap_xlat_tbl_ptr ?
			gm206_bios_read_u8(g, g->bios.mem_strap_xlat_tbl_ptr +
				strap) : strap;

		for (i = 0; i < count; i++) {
			data = gm206_bios_read_u32(g, data_table_offset + ((i *
				g->bios.mem_strap_data_count + index) *
				sizeof(u32)));
			gk20a_writel(g, reg, data);
			reg += stride;
		}
	}
}

static void gp106_init_condition(struct gk20a *g, bool *condition,
	u32 condition_id)
{
	struct condition_entry entry;

	entry.cond_addr = gm206_bios_read_u32(g, g->bios.condition_table_ptr +
		sizeof(entry)*condition_id);
	entry.cond_mask = gm206_bios_read_u32(g, g->bios.condition_table_ptr +
		sizeof(entry)*condition_id + 4);
	entry.cond_compare = gm206_bios_read_u32(g, g->bios.condition_table_ptr +
		sizeof(entry)*condition_id + 8);

	if ((gk20a_readl(g, entry.cond_addr) & entry.cond_mask)
		!= entry.cond_compare) {
		*condition = false;
	}
}

static int gp106_execute_script(struct gk20a *g, u32 offset)
{
	u8 opcode;
	u32 ip;
	u32 operand[8];
	bool condition, end;
	int status = 0;

	ip = offset;
	condition = true;
	end = false;

	while (!end) {

		opcode = gm206_bios_read_u8(g, ip++);

		switch (opcode) {

		case INIT_XMEMSEL_ZM_NV_REG_ARRAY:
			operand[0] = gm206_bios_read_u32(g, ip);
			operand[1] = gm206_bios_read_u8(g, ip+4);
			operand[2] = gm206_bios_read_u8(g, ip+5);
			ip += 6;

			gp106_init_xmemsel_zm_nv_reg_array(g, &condition,
				operand[0], operand[1], operand[2], ip);
			ip += operand[2] * sizeof(u32) *
				g->bios.mem_strap_data_count;
			break;

		case INIT_CONDITION:
			operand[0] = gm206_bios_read_u8(g, ip);
			ip++;

			gp106_init_condition(g, &condition, operand[0]);
			break;

		case INIT_RESUME:
			condition = true;
			break;

		case INIT_DONE:
			end = true;
			break;

		default:
			gk20a_err(dev_from_gk20a(g), "opcode: 0x%02x", opcode);
			end = true;
			status = -EINVAL;
			break;
		}
	}

	return status;
}

void gp106_init_bios(struct gpu_ops *gops)
{
	gm206_init_bios(gops);
	gops->bios.execute_script = gp106_execute_script;
}
