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

#ifndef __IA_CSS_CELL_H
#define __IA_CSS_CELL_H

#include "storage_class.h"
#include "type_support.h"

STORAGE_CLASS_INLINE unsigned int
ia_css_cell_get_stat_ctrl(unsigned int ssid, unsigned int cell_id);

STORAGE_CLASS_INLINE void
ia_css_cell_set_stat_ctrl(unsigned int ssid, unsigned int cell_id,
			  unsigned int value);

STORAGE_CLASS_INLINE void
ia_css_cell_set_start_pc(unsigned int ssid, unsigned int cell_id,
			 unsigned int pc);

STORAGE_CLASS_INLINE void
ia_css_cell_set_icache_base_address(unsigned int ssid, unsigned int cell_id,
				    unsigned int value);

#if 0 /* To be implemented after completing cell device properties */
STORAGE_CLASS_INLINE void
ia_css_cell_set_icache_info_bits(unsigned int ssid, unsigned int cell_id,
				 unsigned int value);

STORAGE_CLASS_INLINE unsigned int
ia_css_cell_get_debug_pc(unsigned int ssid, unsigned int cell_id);

STORAGE_CLASS_INLINE unsigned int
ia_css_cell_get_stall_bits(unsigned int ssid, unsigned int cell_id);
#endif

/* configure master ports */

STORAGE_CLASS_INLINE void
ia_css_cell_set_master_base_address(unsigned int ssid, unsigned int cell_id,
	unsigned int master, unsigned int value);

STORAGE_CLASS_INLINE void
ia_css_cell_set_master_segment_base_address(unsigned int ssid,
		unsigned int cell_id,
		unsigned int master, unsigned int segment, unsigned int value);

STORAGE_CLASS_INLINE void
ia_css_cell_set_master_info_bits(unsigned int ssid, unsigned int cell_id,
	unsigned int master, unsigned int value);

STORAGE_CLASS_INLINE void
ia_css_cell_set_master_segment_info_bits(unsigned int ssid,
		unsigned int cell_id,
		unsigned int master, unsigned int segment, unsigned int value);

STORAGE_CLASS_INLINE void
ia_css_cell_set_master_info_override_bits(unsigned int ssid, unsigned int cell,
	unsigned int master, unsigned int value);

STORAGE_CLASS_INLINE void
ia_css_cell_set_master_segment_info_override_bits(unsigned int ssid,
		unsigned int cell,
		unsigned int master, unsigned int segment, unsigned int value);

/* Access memories */

STORAGE_CLASS_INLINE void
ia_css_cell_mem_store_32(unsigned int ssid, unsigned int cell_id,
	unsigned int mem_id, unsigned int addr, unsigned int value);

STORAGE_CLASS_INLINE unsigned int
ia_css_cell_mem_load_32(unsigned int ssid, unsigned int cell_id,
	unsigned int mem_id, unsigned int addr);

/***********************************************************************/

STORAGE_CLASS_INLINE unsigned int
ia_css_cell_is_ready(unsigned int ssid, unsigned int cell_id);

STORAGE_CLASS_INLINE void
ia_css_cell_set_start_bit(unsigned int ssid, unsigned int cell_id);

STORAGE_CLASS_INLINE void
ia_css_cell_set_run_bit(unsigned int ssid, unsigned int cell_id,
			unsigned int value);

STORAGE_CLASS_INLINE void
ia_css_cell_start(unsigned int ssid, unsigned int cell_id);

STORAGE_CLASS_INLINE void
ia_css_cell_start_prefetch(unsigned int ssid, unsigned int cell_id,
			   bool prefetch);

STORAGE_CLASS_INLINE void
ia_css_cell_wait(unsigned int ssid, unsigned int cell_id);

/* include inline implementation */
#include "ia_css_cell_impl.h"

#endif /* __IA_CSS_CELL_H */
