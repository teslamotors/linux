/*
 * Register bitfield descriptions for Pondicherry2 memory controller.
 *
 * Copyright (c) 2016, Intel Corporation.
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

#ifndef _PND2_REGS_H
#define _PND2_REGS_H

struct b_cr_touud_lo_0_0_0_pci {
	u32	lock : 1;
	u32	reserved_1 : 19;
	u32	touud : 12;
};
#define b_cr_touud_lo_0_0_0_pci_port 0x4c
#define b_cr_touud_lo_0_0_0_pci_offset 0xa8
#define b_cr_touud_lo_0_0_0_pci_r_opcode 0x04

struct b_cr_touud_hi_0_0_0_pci {
	u32	touud : 7;
	u32	reserved_0 : 25;
};
#define b_cr_touud_hi_0_0_0_pci_port 0x4c
#define b_cr_touud_hi_0_0_0_pci_offset 0xac
#define b_cr_touud_hi_0_0_0_pci_r_opcode 0x04

struct b_cr_tolud_0_0_0_pci {
	u32	lock : 1;
	u32	reserved_0 : 19;
	u32	tolud : 12;
};
#define b_cr_tolud_0_0_0_pci_port 0x4c
#define b_cr_tolud_0_0_0_pci_offset 0xbc
#define b_cr_tolud_0_0_0_pci_r_opcode 0x04

struct b_cr_slice_channel_hash {
	u64	slice_1_disabled : 1;
	u64	hvm_mode : 1;
	u64	interleave_mode : 2;
	u64	slice_0_mem_disabled : 1;
	u64	reserved_0 : 1;
	u64	slice_hash_mask : 14;
	u64	reserved_1 : 11;
	u64	enable_pmi_dual_data_mode : 1;
	u64	ch_1_disabled : 1;
	u64	reserved_2 : 1;
	u64	sym_slice0_channel_enabled : 2;
	u64	sym_slice1_channel_enabled : 2;
	u64	ch_hash_mask : 14;
	u64	reserved_3 : 11;
	u64	lock : 1;
};
#define b_cr_slice_channel_hash_port 0x4c
#define b_cr_slice_channel_hash_offset 0x4c58
#define b_cr_slice_channel_hash_r_opcode 0x06

struct b_cr_mot_out_base_0_0_0_mchbar {
	u32	reserved_0 : 14;
	u32	mot_out_base : 15;
	u32	reserved_1 : 1;
	u32	tr_en : 1;
	u32	imr_en : 1;
};
#define b_cr_mot_out_base_0_0_0_mchbar_port 0x4c
#define b_cr_mot_out_base_0_0_0_mchbar_offset 0x6af0
#define b_cr_mot_out_base_0_0_0_mchbar_r_opcode 0x00

struct b_cr_mot_out_mask_0_0_0_mchbar {
	u32	reserved_0 : 14;
	u32	mot_out_mask : 15;
	u32	reserved_1 : 1;
	u32	ia_iwb_en : 1;
	u32	gt_iwb_en : 1;
};
#define b_cr_mot_out_mask_0_0_0_mchbar_port 0x4c
#define b_cr_mot_out_mask_0_0_0_mchbar_offset 0x6af4
#define b_cr_mot_out_mask_0_0_0_mchbar_r_opcode 0x00

struct b_cr_asym_mem_region0_0_0_0_mchbar {
	u32	pad : 4;
	u32	slice0_asym_base : 11;
	u32	pad_18_15 : 4;
	u32	slice0_asym_limit : 11;
	u32	slice0_asym_channel_select : 1;
	u32	slice0_asym_enable : 1;
};
#define b_cr_asym_mem_region0_0_0_0_mchbar_port 0x4c
#define b_cr_asym_mem_region0_0_0_0_mchbar_offset 0x6e40
#define b_cr_asym_mem_region0_0_0_0_mchbar_r_opcode 0x00

struct b_cr_asym_mem_region1_0_0_0_mchbar {
	u32	pad : 4;
	u32	slice1_asym_base : 11;
	u32	pad_18_15 : 4;
	u32	slice1_asym_limit : 11;
	u32	slice1_asym_channel_select : 1;
	u32	slice1_asym_enable : 1;
};
#define b_cr_asym_mem_region1_0_0_0_mchbar_port 0x4c
#define b_cr_asym_mem_region1_0_0_0_mchbar_offset 0x6e44
#define b_cr_asym_mem_region1_0_0_0_mchbar_r_opcode 0x00

struct b_cr_asym_2way_mem_region_0_0_0_mchbar {
	u32	pad : 2;
	u32	asym_2way_intlv_mode : 2;
	u32	asym_2way_base : 11;
	u32	pad_16_15 : 2;
	u32	asym_2way_limit : 11;
	u32	pad_30_28 : 3;
	u32	asym_2way_interleave_enable : 1;
};
#define b_cr_asym_2way_mem_region_0_0_0_mchbar_port 0x4c
#define b_cr_asym_2way_mem_region_0_0_0_mchbar_offset 0x6e50
#define b_cr_asym_2way_mem_region_0_0_0_mchbar_r_opcode 0x00

struct d_cr_drp0 {
	u32	rken0 : 1;
	u32	rken1 : 1;
	u32	ddmen : 1;
	u32	rsvd3 : 1;
	u32	dwid : 2;
	u32	dden : 3;
	u32	rsvd13_9 : 5;
	u32	rsien : 1;
	u32	bahen : 1;
	u32	rsvd18_16 : 3;
	u32	caswizzle : 2;
	u32	eccen : 1;
	u32	dramtype : 3;
	u32	blmode : 3;
	u32	addrdec : 2;
	u32	dramdevice_pr : 2;
};
#define d_cr_drp0_port0 0x10
#define d_cr_drp0_port1 0x11
#define d_cr_drp0_port2 0x18
#define d_cr_drp0_port3 0x19
#define d_cr_drp0_offset 0x1400
#define d_cr_drp0_r_opcode 0x00

#endif /* _PND2_REGS_H */
