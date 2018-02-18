/*
 * drivers/misc/tegra-profiler/dwarf.h
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
 */

#ifndef __QUADD_DWARF_H
#define __QUADD_DWARF_H

#define DW_CFA_advance_loc		0x40
#define DW_CFA_offset			0x80
#define DW_CFA_restore			0xc0
#define DW_CFA_nop			0x00
#define DW_CFA_set_loc			0x01
#define DW_CFA_advance_loc1		0x02
#define DW_CFA_advance_loc2		0x03
#define DW_CFA_advance_loc4		0x04
#define DW_CFA_offset_extended		0x05
#define DW_CFA_restore_extended		0x06
#define DW_CFA_undefined		0x07
#define DW_CFA_same_value		0x08
#define DW_CFA_register			0x09
#define DW_CFA_remember_state		0x0a
#define DW_CFA_restore_state		0x0b
#define DW_CFA_def_cfa			0x0c
#define DW_CFA_def_cfa_register		0x0d
#define DW_CFA_def_cfa_offset		0x0e

/* DWARF 3.  */
#define DW_CFA_def_cfa_expression	0x0f
#define DW_CFA_expression		0x10
#define DW_CFA_offset_extended_sf	0x11
#define DW_CFA_def_cfa_sf		0x12
#define DW_CFA_def_cfa_offset_sf	0x13
#define DW_CFA_val_offset		0x14
#define DW_CFA_val_offset_sf		0x15
#define DW_CFA_val_expression		0x16

#define DW_CFA_lo_user			0x1c
#define DW_CFA_hi_user			0x3f

/* GNU extensions.  */
#define DW_CFA_GNU_window_save		0x2d
#define DW_CFA_GNU_args_size		0x2e
#define DW_CFA_GNU_negative_offset_extended	0x2f

/* For use with GNU frame unwind information. */

#define DW_EH_PE_absptr		0x00
#define DW_EH_PE_omit		0xff

#define DW_EH_PE_uleb128	0x01
#define DW_EH_PE_udata2		0x02
#define DW_EH_PE_udata4		0x03
#define DW_EH_PE_udata8		0x04
#define DW_EH_PE_sleb128	0x09
#define DW_EH_PE_sdata2		0x0A
#define DW_EH_PE_sdata4		0x0B
#define DW_EH_PE_sdata8		0x0C
#define DW_EH_PE_signed		0x08

#define DW_EH_PE_pcrel		0x10
#define DW_EH_PE_textrel	0x20
#define DW_EH_PE_datarel	0x30
#define DW_EH_PE_funcrel	0x40
#define DW_EH_PE_aligned	0x50

#define DW_EH_PE_indirect	0x80

#define DW_CIE_ID	0xffffffff
#define DW64_CIE_ID	0xffffffffffffffffULL

#define DW_CIE_VERSION	1

#endif  /* __QUADD_DWARF_H */
