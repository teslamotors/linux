/**
* Support for Intel Camera Imaging ISP subsystem.
 * Copyright (c) 2010 - 2018, Intel Corporation.
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

#ifndef __IA_CSS_CELL_REGS_H
#define __IA_CSS_CELL_REGS_H

#include "storage_class.h"
#include "ipu_device_cell_type_properties.h"
#include "ia_css_cmem.h"

STORAGE_CLASS_INLINE void
ia_css_cell_regs_set_stat_ctrl(unsigned int ssid, unsigned int regs_addr,
				unsigned int value)
{
	ia_css_cmem_store_32(ssid,
		regs_addr + IPU_DEVICE_CELL_STAT_CTRL_REG_ADDRESS, value);
}

STORAGE_CLASS_INLINE unsigned int
ia_css_cell_regs_get_stat_ctrl(unsigned int ssid, unsigned int regs_addr)
{
	return ia_css_cmem_load_32(ssid,
		regs_addr + IPU_DEVICE_CELL_STAT_CTRL_REG_ADDRESS);
}

STORAGE_CLASS_INLINE void
ia_css_cell_icache_invalidate(unsigned int ssid, unsigned int regs_addr)
{
	ia_css_cell_regs_set_stat_ctrl(ssid, regs_addr,
		1u << IPU_DEVICE_CELL_STAT_CTRL_INVALIDATE_ICACHE_BIT);
}

STORAGE_CLASS_INLINE void
ia_css_cell_regs_set_start_pc(unsigned int ssid, unsigned int regs_addr,
				unsigned int pc)
{
	ia_css_cmem_store_32(ssid,
		regs_addr + IPU_DEVICE_CELL_START_PC_REG_ADDRESS, pc);
}

STORAGE_CLASS_INLINE void
ia_css_cell_regs_set_icache_base_address(unsigned int ssid,
					unsigned int regs_addr,
					unsigned int value)
{
	ia_css_cmem_store_32(ssid,
		regs_addr + IPU_DEVICE_CELL_ICACHE_BASE_REG_ADDRESS, value);
}

STORAGE_CLASS_INLINE void
ia_css_cell_regs_set_icache_info_bits(unsigned int ssid,
					unsigned int regs_addr,
					unsigned int value)
{
	ia_css_cmem_store_32(ssid,
		regs_addr + IPU_DEVICE_CELL_ICACHE_INFO_BITS_REG_ADDRESS,
		value);
}

STORAGE_CLASS_INLINE void
ia_css_cell_regs_icache_invalidate(unsigned int ssid, unsigned int regs_addr)
{
	ia_css_cell_regs_set_stat_ctrl(ssid, regs_addr,
		1u << IPU_DEVICE_CELL_STAT_CTRL_INVALIDATE_ICACHE_BIT);
}

#endif /* __IA_CSS_CELL_REGS_H */
