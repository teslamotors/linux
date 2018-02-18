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
#ifndef _GPMUIFBIOS_H_
#define _GPMUIFBIOS_H_

struct nv_pmu_bios_vfield_register_segment_super {
	u8 type;
	u8 low_bit;
	u8 high_bit;
};

struct nv_pmu_bios_vfield_register_segment_reg {
	struct nv_pmu_bios_vfield_register_segment_super super;
	u32 addr;
};

struct nv_pmu_bios_vfield_register_segment_index_reg {
	struct nv_pmu_bios_vfield_register_segment_super super;
	u32 addr;
	u32 reg_index;
	u32 index;
};

union nv_pmu_bios_vfield_register_segment {
	struct nv_pmu_bios_vfield_register_segment_super super;
	struct nv_pmu_bios_vfield_register_segment_reg reg;
	struct nv_pmu_bios_vfield_register_segment_index_reg index_reg;
};


#endif /* _GPMUIFBIOS_H_*/
