/*
* Support for Intel Camera Imaging ISP subsystem.
 * Copyright (c) 2010 - 2016, Intel Corporation.
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

#ifndef __PSYS_SYSTEM_GLOBAL_H_INCLUDED__
#define __PSYS_SYSTEM_GLOBAL_H_INCLUDED__

#include <type_support.h>
#include "ia_css_base_types.h"
#include "vied_nci_psys_resource_model.h"

/*
 * Key system types
 */
/* Subsystem internal physical address */
#define VIED_ADDRESS_BITS 32

/* typedef uint32_t vied_address_t; */

/* Subsystem internal virtual address */

/* Subsystem internal data bus */
#define VIED_DATA_BITS 32
typedef uint32_t vied_data_t;

#define VIED_NULL ((vied_vaddress_t)0)

extern vied_nci_resource_bitmap_t vied_nci_bit_mask(
	const unsigned					index);

extern vied_nci_resource_bitmap_t vied_nci_bitmap_set(
	const vied_nci_resource_bitmap_t		bitmap,
	const vied_nci_resource_bitmap_t		bit_mask);

extern vied_nci_resource_bitmap_t vied_nci_bitmap_clear(
	const vied_nci_resource_bitmap_t		bitmap,
	const vied_nci_resource_bitmap_t		bit_mask);

extern bool vied_nci_is_bitmap_empty(
	const vied_nci_resource_bitmap_t		bitmap);

extern bool vied_nci_is_bitmap_set(
	const vied_nci_resource_bitmap_t		bitmap,
	const vied_nci_resource_bitmap_t		bit_mask);

extern bool vied_nci_is_bitmap_clear(
	const vied_nci_resource_bitmap_t		bitmap,
	const vied_nci_resource_bitmap_t		bit_mask);

extern int vied_nci_bitmap_compute_weight(
	const vied_nci_resource_bitmap_t		bitmap);

extern vied_nci_resource_bitmap_t vied_nci_bitmap_union(
	const vied_nci_resource_bitmap_t		bitmap0,
	const vied_nci_resource_bitmap_t		bitmap1);

extern vied_nci_resource_bitmap_t vied_nci_bitmap_intersection(
	const vied_nci_resource_bitmap_t		bitmap0,
	const vied_nci_resource_bitmap_t		bitmap1);

extern vied_nci_resource_bitmap_t vied_nci_bitmap_xor(
	const vied_nci_resource_bitmap_t		bitmap0,
	const vied_nci_resource_bitmap_t		bitmap1);

extern vied_nci_resource_bitmap_t vied_nci_bitmap_set_unique(
	const vied_nci_resource_bitmap_t		bitmap,
	const vied_nci_resource_bitmap_t		bit_mask);

extern vied_nci_resource_bitmap_t vied_nci_bit_mask_set_unique(
	const vied_nci_resource_bitmap_t		bitmap,
	const unsigned					index);

extern vied_nci_resource_bitmap_t vied_nci_cell_bit_mask(
	const vied_nci_cell_ID_t		cell_id);

extern vied_nci_resource_bitmap_t vied_nci_barrier_bit_mask(
	const vied_nci_barrier_ID_t		barrier_id);

extern vied_nci_cell_type_ID_t vied_nci_cell_get_type(
	const vied_nci_cell_ID_t		cell_id);

extern vied_nci_mem_type_ID_t vied_nci_mem_get_type(
	const vied_nci_mem_ID_t			mem_id);

extern uint16_t vied_nci_mem_get_size(
	const vied_nci_mem_ID_t			mem_id);

extern uint16_t vied_nci_dev_chn_get_size(
	const vied_nci_dev_chn_ID_t		dev_chn_id);

extern bool vied_nci_is_cell_of_type(
	const vied_nci_cell_ID_t		cell_id,
	const vied_nci_cell_type_ID_t	cell_type_id);

extern bool vied_nci_is_mem_of_type(
	const vied_nci_mem_ID_t			mem_id,
	const vied_nci_mem_type_ID_t	mem_type_id);

extern bool vied_nci_is_cell_mem_of_type(
	const vied_nci_cell_ID_t		cell_id,
	const uint16_t					mem_index,
	const vied_nci_mem_type_ID_t	mem_type_id);

extern bool vied_nci_has_cell_mem_of_id(
	const vied_nci_cell_ID_t		cell_id,
	const vied_nci_mem_ID_t			mem_id);

extern uint16_t vied_nci_cell_get_mem_count(
	const vied_nci_cell_ID_t		cell_id);

extern vied_nci_mem_type_ID_t vied_nci_cell_get_mem_type(
	const vied_nci_cell_ID_t		cell_id,
	const uint16_t					mem_index);

extern vied_nci_mem_ID_t vied_nci_cell_get_mem(
	const vied_nci_cell_ID_t		cell_id,
	const uint16_t					mem_index);

extern vied_nci_mem_type_ID_t vied_nci_cell_type_get_mem_type(
	const vied_nci_cell_type_ID_t	cell_type_id,
	const uint16_t					mem_index);

#endif /* __PSYS_SYSTEM_GLOBAL_H_INCLUDED__ */
