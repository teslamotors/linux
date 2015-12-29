/**
* Support for Intel Camera Imaging ISP subsystem.
 * Copyright (c) 2010 - 2015, Intel Corporation.
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


#include <vied_nci_psys_system_global.h>

#include <type_support.h>
#include <assert_support.h>

#include "ia_css_psys_sim_trace.h"

static const vied_nci_cell_type_ID_t vied_nci_cell_type[VIED_NCI_N_CELL_ID] = {
	VIED_NCI_SP_CTRL_TYPE_ID,
	VIED_NCI_SP_SERVER_TYPE_ID,
	VIED_NCI_SP_SERVER_TYPE_ID,
	VIED_NCI_SP_FLOAT_TYPE_ID,
	VIED_NCI_VP_TYPE_ID,
	VIED_NCI_VP_TYPE_ID,
	VIED_NCI_VP_TYPE_ID,
	VIED_NCI_VP_TYPE_ID,
	VIED_NCI_ACC_ISA_TYPE_ID,
	VIED_NCI_ACC_PSA_TYPE_ID,
	VIED_NCI_ACC_PSA_TYPE_ID,
	VIED_NCI_ACC_PSA_TYPE_ID,
	VIED_NCI_ACC_PSA_TYPE_ID,
	VIED_NCI_ACC_PSA_TYPE_ID,
	VIED_NCI_ACC_PSA_TYPE_ID,
	VIED_NCI_ACC_OSA_TYPE_ID,
	VIED_NCI_GDC_TYPE_ID,
	VIED_NCI_GDC_TYPE_ID};

static const vied_nci_mem_type_ID_t vied_nci_mem_type[VIED_NCI_N_MEM_ID] = {
	VIED_NCI_VMEM_TYPE_ID,
	VIED_NCI_VMEM_TYPE_ID,
	VIED_NCI_VMEM_TYPE_ID,
	VIED_NCI_VMEM_TYPE_ID,
	VIED_NCI_GMEM_TYPE_ID, /* VMEM4 is GMEM according to vied_nci_cell_mem */
	VIED_NCI_BAMEM_TYPE_ID,
	VIED_NCI_BAMEM_TYPE_ID,
	VIED_NCI_BAMEM_TYPE_ID,
	VIED_NCI_BAMEM_TYPE_ID,
	VIED_NCI_DMEM_TYPE_ID,
	VIED_NCI_DMEM_TYPE_ID,
	VIED_NCI_DMEM_TYPE_ID,
	VIED_NCI_DMEM_TYPE_ID,
	VIED_NCI_DMEM_TYPE_ID,
	VIED_NCI_DMEM_TYPE_ID,
	VIED_NCI_DMEM_TYPE_ID,
	VIED_NCI_DMEM_TYPE_ID,
	VIED_NCI_PMEM_TYPE_ID,
	VIED_NCI_PMEM_TYPE_ID,
	VIED_NCI_PMEM_TYPE_ID,
	VIED_NCI_PMEM_TYPE_ID};

static uint16_t vied_nci_N_cell_mem[VIED_NCI_N_CELL_TYPE_ID] = {
	VIED_NCI_N_SP_CTRL_MEM,
	VIED_NCI_N_SP_SERVER_MEM,
	VIED_NCI_N_SP_FLOAT_MEM,
	VIED_NCI_N_VP_MEM,
	VIED_NCI_N_ACC_PSA_MEM,
	VIED_NCI_N_ACC_ISA_MEM,
	VIED_NCI_N_ACC_OSA_MEM};

static const vied_nci_mem_type_ID_t vied_nci_cell_mem_type[VIED_NCI_N_CELL_TYPE_ID][VIED_NCI_N_MEM_TYPE_ID] = {
	{VIED_NCI_N_MEM_TYPE_ID, VIED_NCI_DMEM_TYPE_ID, VIED_NCI_N_MEM_TYPE_ID, VIED_NCI_N_MEM_TYPE_ID, VIED_NCI_N_MEM_TYPE_ID},
	{VIED_NCI_N_MEM_TYPE_ID, VIED_NCI_DMEM_TYPE_ID, VIED_NCI_N_MEM_TYPE_ID, VIED_NCI_N_MEM_TYPE_ID, VIED_NCI_N_MEM_TYPE_ID},
	{VIED_NCI_N_MEM_TYPE_ID, VIED_NCI_DMEM_TYPE_ID, VIED_NCI_N_MEM_TYPE_ID, VIED_NCI_N_MEM_TYPE_ID, VIED_NCI_N_MEM_TYPE_ID},
	{VIED_NCI_GMEM_TYPE_ID, VIED_NCI_DMEM_TYPE_ID, VIED_NCI_VMEM_TYPE_ID, VIED_NCI_BAMEM_TYPE_ID, VIED_NCI_PMEM_TYPE_ID},
	{VIED_NCI_N_MEM_TYPE_ID, VIED_NCI_N_MEM_TYPE_ID, VIED_NCI_N_MEM_TYPE_ID, VIED_NCI_N_MEM_TYPE_ID, VIED_NCI_N_MEM_TYPE_ID},
	{VIED_NCI_N_MEM_TYPE_ID, VIED_NCI_N_MEM_TYPE_ID, VIED_NCI_N_MEM_TYPE_ID, VIED_NCI_N_MEM_TYPE_ID, VIED_NCI_N_MEM_TYPE_ID},
	{VIED_NCI_N_MEM_TYPE_ID, VIED_NCI_N_MEM_TYPE_ID, VIED_NCI_N_MEM_TYPE_ID, VIED_NCI_N_MEM_TYPE_ID, VIED_NCI_N_MEM_TYPE_ID},
	{VIED_NCI_N_MEM_TYPE_ID, VIED_NCI_N_MEM_TYPE_ID, VIED_NCI_N_MEM_TYPE_ID, VIED_NCI_N_MEM_TYPE_ID, VIED_NCI_N_MEM_TYPE_ID}};

static const vied_nci_mem_ID_t vied_nci_cell_mem[VIED_NCI_N_CELL_ID][VIED_NCI_N_MEM_TYPE_ID] = {
	{VIED_NCI_N_MEM_ID, VIED_NCI_DMEM0_ID, VIED_NCI_N_MEM_ID, VIED_NCI_N_MEM_ID, VIED_NCI_N_MEM_ID},
	{VIED_NCI_N_MEM_ID, VIED_NCI_DMEM1_ID, VIED_NCI_N_MEM_ID, VIED_NCI_N_MEM_ID, VIED_NCI_N_MEM_ID},
	{VIED_NCI_N_MEM_ID, VIED_NCI_DMEM2_ID, VIED_NCI_N_MEM_ID, VIED_NCI_N_MEM_ID, VIED_NCI_N_MEM_ID},
	{VIED_NCI_N_MEM_ID, VIED_NCI_DMEM3_ID, VIED_NCI_N_MEM_ID, VIED_NCI_N_MEM_ID, VIED_NCI_N_MEM_ID},
	{VIED_NCI_VMEM4_ID, VIED_NCI_DMEM4_ID, VIED_NCI_VMEM0_ID, VIED_NCI_BAMEM0_ID, VIED_NCI_PMEM0_ID},
	{VIED_NCI_VMEM4_ID, VIED_NCI_DMEM5_ID, VIED_NCI_VMEM1_ID, VIED_NCI_BAMEM1_ID, VIED_NCI_PMEM1_ID},
	{VIED_NCI_VMEM4_ID, VIED_NCI_DMEM6_ID, VIED_NCI_VMEM2_ID, VIED_NCI_BAMEM2_ID, VIED_NCI_PMEM2_ID},
	{VIED_NCI_VMEM4_ID, VIED_NCI_DMEM7_ID, VIED_NCI_VMEM3_ID, VIED_NCI_BAMEM3_ID, VIED_NCI_PMEM3_ID},
	{VIED_NCI_N_MEM_ID, VIED_NCI_N_MEM_ID, VIED_NCI_N_MEM_ID, VIED_NCI_N_MEM_ID, VIED_NCI_N_MEM_ID},
	{VIED_NCI_N_MEM_ID, VIED_NCI_N_MEM_ID, VIED_NCI_N_MEM_ID, VIED_NCI_N_MEM_ID, VIED_NCI_N_MEM_ID},
	{VIED_NCI_N_MEM_ID, VIED_NCI_N_MEM_ID, VIED_NCI_N_MEM_ID, VIED_NCI_N_MEM_ID, VIED_NCI_N_MEM_ID},
	{VIED_NCI_N_MEM_ID, VIED_NCI_N_MEM_ID, VIED_NCI_N_MEM_ID, VIED_NCI_N_MEM_ID, VIED_NCI_N_MEM_ID},
	{VIED_NCI_N_MEM_ID, VIED_NCI_N_MEM_ID, VIED_NCI_N_MEM_ID, VIED_NCI_N_MEM_ID, VIED_NCI_N_MEM_ID},
	{VIED_NCI_N_MEM_ID, VIED_NCI_N_MEM_ID, VIED_NCI_N_MEM_ID, VIED_NCI_N_MEM_ID, VIED_NCI_N_MEM_ID},
	{VIED_NCI_N_MEM_ID, VIED_NCI_N_MEM_ID, VIED_NCI_N_MEM_ID, VIED_NCI_N_MEM_ID, VIED_NCI_N_MEM_ID},
	{VIED_NCI_N_MEM_ID, VIED_NCI_N_MEM_ID, VIED_NCI_N_MEM_ID, VIED_NCI_N_MEM_ID, VIED_NCI_N_MEM_ID},
	{VIED_NCI_N_MEM_ID, VIED_NCI_N_MEM_ID, VIED_NCI_N_MEM_ID, VIED_NCI_N_MEM_ID, VIED_NCI_N_MEM_ID},
	{VIED_NCI_N_MEM_ID, VIED_NCI_N_MEM_ID, VIED_NCI_N_MEM_ID, VIED_NCI_N_MEM_ID, VIED_NCI_N_MEM_ID}};

/* This static global is not used anywhere. The HIVE compiler
 * seems more aggressive and flags the unused warning which
 * under strict flag is treated as error. Th host side compile
 * e.g. GNU compiler doesn't flag this as error.
 */
#if !defined(__HIVECC)
static const uint16_t vied_nci_mem_element_size[VIED_NCI_N_MEM_TYPE_ID] = {
	512, 32, 512, 512, 512};
#endif

static const uint16_t vied_nci_mem_size[VIED_NCI_N_MEM_ID] = {
	VIED_NCI_VMEM0_SIZE,
	VIED_NCI_VMEM1_SIZE,
	VIED_NCI_VMEM2_SIZE,
	VIED_NCI_VMEM3_SIZE,
	VIED_NCI_VMEM4_SIZE,
	VIED_NCI_BAMEM0_SIZE,
	VIED_NCI_BAMEM1_SIZE,
	VIED_NCI_BAMEM2_SIZE,
	VIED_NCI_BAMEM3_SIZE,
	VIED_NCI_DMEM0_SIZE,
	VIED_NCI_DMEM1_SIZE,
	VIED_NCI_DMEM2_SIZE,
	VIED_NCI_DMEM3_SIZE,
	VIED_NCI_DMEM4_SIZE,
	VIED_NCI_DMEM5_SIZE,
	VIED_NCI_DMEM6_SIZE,
	VIED_NCI_DMEM7_SIZE,
	VIED_NCI_PMEM0_SIZE,
	VIED_NCI_PMEM1_SIZE,
	VIED_NCI_PMEM2_SIZE,
	VIED_NCI_PMEM3_SIZE};

static const uint16_t vied_nci_dev_chn_size[VIED_NCI_N_DEV_CHN_ID] = {
	256, 256, 256, 256, 256, 256, 256, 256};

/* Use vied_bits instead, however for test purposes we uses explicit type checking */
vied_nci_resource_bitmap_t vied_nci_bit_mask(
	const unsigned int						index)
{
	vied_nci_resource_bitmap_t	bit_mask = 0;

	IA_CSS_TRACE_0(PSYSAPI_SIM, VERBOSE, "vied_nci_bit_mask(): enter: \n");

	if (index < VIED_NCI_RESOURCE_BITMAP_BITS) {
		bit_mask = (vied_nci_resource_bitmap_t)1 << index;
	}
	return bit_mask;
}

vied_nci_resource_bitmap_t vied_nci_bitmap_set(
	const vied_nci_resource_bitmap_t		bitmap,
	const vied_nci_resource_bitmap_t		bit_mask)
{

	IA_CSS_TRACE_0(PSYSAPI_SIM, VERBOSE, "vied_nci_bitmap_set(): enter: \n");

/*
	assert(vied_nci_is_bitmap_one_hot(bit_mask));
*/
	return bitmap | bit_mask;
}

vied_nci_resource_bitmap_t vied_nci_bitmap_clear(
	const vied_nci_resource_bitmap_t		bitmap,
	const vied_nci_resource_bitmap_t		bit_mask)
{

	IA_CSS_TRACE_0(PSYSAPI_SIM, VERBOSE, "vied_nci_bitmap_clear(): enter: \n");

/*
	assert(vied_nci_is_bitmap_one_hot(bit_mask));
*/
	return bitmap & (~bit_mask);
}

vied_nci_resource_bitmap_t vied_nci_bitmap_set_unique(
	const vied_nci_resource_bitmap_t		bitmap,
	const vied_nci_resource_bitmap_t		bit_mask)
{
	vied_nci_resource_bitmap_t	ret = 0;

	IA_CSS_TRACE_0(PSYSAPI_SIM, VERBOSE, "vied_nci_bitmap_set_unique(): enter: \n");

	if ((bitmap & bit_mask) == 0) {
		ret = bitmap | bit_mask;
	}
	return ret;
}

vied_nci_resource_bitmap_t vied_nci_bit_mask_set_unique(
	const vied_nci_resource_bitmap_t		bitmap,
	const unsigned int						index)
{
	vied_nci_resource_bitmap_t	ret = 0;
	vied_nci_resource_bitmap_t	bit_mask;

	IA_CSS_TRACE_0(PSYSAPI_SIM, VERBOSE, "vied_nci_bit_mask_set_unique(): enter: \n");

	bit_mask = vied_nci_bit_mask(index);

	if (((bitmap & bit_mask) == 0) && (bit_mask != 0)) {
		ret = bitmap | bit_mask;
	}
	return ret;
}

bool vied_nci_is_bitmap_empty(
	const vied_nci_resource_bitmap_t		bitmap)
{

	IA_CSS_TRACE_0(PSYSAPI_SIM, VERBOSE, "vied_nci_is_bitmap_empty(): enter: \n");

	return (bitmap == 0);
}

bool vied_nci_is_bitmap_set(
	const vied_nci_resource_bitmap_t		bitmap,
	const vied_nci_resource_bitmap_t		bit_mask)
{

	IA_CSS_TRACE_0(PSYSAPI_SIM, VERBOSE, "vied_nci_is_bitmap_set(): enter: \n");

/*
	assert(vied_nci_is_bitmap_one_hot(bit_mask));
*/
	return !vied_nci_is_bitmap_clear(bitmap, bit_mask);
}

bool vied_nci_is_bitmap_clear(
	const vied_nci_resource_bitmap_t		bitmap,
	const vied_nci_resource_bitmap_t		bit_mask)
{

	IA_CSS_TRACE_0(PSYSAPI_SIM, VERBOSE, "vied_nci_is_bitmap_clear(): enter: \n");

/*
	assert(vied_nci_is_bitmap_one_hot(bit_mask));
*/
	return ((bitmap & bit_mask) == 0);
}

int vied_nci_bitmap_compute_weight(
	const vied_nci_resource_bitmap_t		bitmap)
{
	vied_nci_resource_bitmap_t	loc_bitmap = bitmap;
	int	weight = 0;
	int	i;

	IA_CSS_TRACE_0(PSYSAPI_SIM, VERBOSE, "vied_nci_bitmap_compute_weight(): enter: \n");

	/* Do not need the iterator "i" */
	for (i = 0; (i < VIED_NCI_RESOURCE_BITMAP_BITS) && (loc_bitmap != 0); i++) {
		weight += loc_bitmap & 0x01;
		loc_bitmap >>= 1;
	}

	return weight;
}

vied_nci_resource_bitmap_t vied_nci_bitmap_union(
        const vied_nci_resource_bitmap_t	bitmap0,
        const vied_nci_resource_bitmap_t	bitmap1)
{
        IA_CSS_TRACE_0(PSYSAPI_SIM, VERBOSE, "vied_nci_bitmap_union(): enter: \n");
        return (bitmap0 | bitmap1);
}

vied_nci_resource_bitmap_t vied_nci_bitmap_intersection(
        const vied_nci_resource_bitmap_t		bitmap0,
        const vied_nci_resource_bitmap_t		bitmap1)
{
        IA_CSS_TRACE_0(PSYSAPI_SIM, VERBOSE, "ia_css_kernel_bitmap_intersection(): enter: \n");
        return (bitmap0 & bitmap1);
}

vied_nci_resource_bitmap_t vied_nci_bitmap_xor(
        const vied_nci_resource_bitmap_t		bitmap0,
        const vied_nci_resource_bitmap_t		bitmap1)
{
        IA_CSS_TRACE_0(PSYSAPI_SIM, VERBOSE, "vied_nci_bitmap_xor(): enter: \n");
        return (bitmap0 ^ bitmap1);
}

vied_nci_resource_bitmap_t vied_nci_cell_bit_mask(
	const vied_nci_cell_ID_t		cell_id)
{
	vied_nci_resource_bitmap_t	bit_mask = 0;

	IA_CSS_TRACE_0(PSYSAPI_SIM, VERBOSE, "vied_nci_cell_bit_mask(): enter: \n");

	if ((cell_id < VIED_NCI_N_CELL_ID) && (cell_id < VIED_NCI_RESOURCE_BITMAP_BITS)) {
		bit_mask = (vied_nci_resource_bitmap_t)1 << cell_id;
	}
	return bit_mask;
}

vied_nci_resource_bitmap_t vied_nci_barrier_bit_mask(
	const vied_nci_barrier_ID_t		barrier_id)
{
	vied_nci_resource_bitmap_t	bit_mask = 0;

	IA_CSS_TRACE_0(PSYSAPI_SIM, VERBOSE, "vied_nci_barrier_bit_mask(): enter: \n");

	if ((barrier_id < VIED_NCI_N_BARRIER_ID) && ((barrier_id + VIED_NCI_N_CELL_ID) < VIED_NCI_RESOURCE_BITMAP_BITS)) {
		bit_mask = (vied_nci_resource_bitmap_t)1 << (barrier_id + VIED_NCI_N_CELL_ID);
	}
	return bit_mask;
}

vied_nci_cell_type_ID_t vied_nci_cell_get_type(
	const vied_nci_cell_ID_t		cell_id)
{
	vied_nci_cell_type_ID_t	cell_type = VIED_NCI_N_CELL_TYPE_ID;

	IA_CSS_TRACE_0(PSYSAPI_SIM, VERBOSE, "vied_nci_cell_get_type(): enter: \n");

	if (cell_id < VIED_NCI_N_CELL_ID) {
		cell_type = vied_nci_cell_type[cell_id];
	} else {
		IA_CSS_TRACE_0(PSYSAPI_SIM, WARNING, "vied_nci_cell_get_type(): invalid argument\n");
	}

	return cell_type;
}

vied_nci_mem_type_ID_t vied_nci_mem_get_type(
	const vied_nci_mem_ID_t			mem_id)
{
	vied_nci_mem_type_ID_t	mem_type = VIED_NCI_N_MEM_TYPE_ID;

	IA_CSS_TRACE_0(PSYSAPI_SIM, VERBOSE, "vied_nci_mem_get_type(): enter: \n");

	if (mem_id < VIED_NCI_N_MEM_ID) {
		mem_type = vied_nci_mem_type[mem_id];
	} else {
		IA_CSS_TRACE_0(PSYSAPI_SIM, WARNING, "vied_nci_mem_get_type(): invalid argument\n");
	}

	return mem_type;
}

uint16_t vied_nci_mem_get_size(
	const vied_nci_mem_ID_t			mem_id)
{
	uint16_t	mem_size = 0;

	IA_CSS_TRACE_0(PSYSAPI_SIM, VERBOSE, "vied_nci_mem_get_size(): enter: \n");

	if (mem_id < VIED_NCI_N_MEM_ID) {
		mem_size = vied_nci_mem_size[mem_id];
	} else {
		IA_CSS_TRACE_0(PSYSAPI_SIM, WARNING, "vied_nci_mem_get_size(): invalid argument\n");
	}

	return mem_size;
}

uint16_t vied_nci_dev_chn_get_size(
	const vied_nci_dev_chn_ID_t		dev_chn_id)
{
	uint16_t	dev_chn_size = 0;

	IA_CSS_TRACE_0(PSYSAPI_SIM, VERBOSE, "vied_nci_dev_chn_get_size(): enter: \n");

	if (dev_chn_id < VIED_NCI_N_DEV_CHN_ID) {
		dev_chn_size = vied_nci_dev_chn_size[dev_chn_id];
	} else {
		IA_CSS_TRACE_0(PSYSAPI_SIM, WARNING, "vied_nci_dev_chn_get_size(): invalid argument\n");
	}

	return dev_chn_size;
}

bool vied_nci_is_cell_of_type(
	const vied_nci_cell_ID_t		cell_id,
	const vied_nci_cell_type_ID_t	cell_type_id)
{
	IA_CSS_TRACE_0(PSYSAPI_SIM, VERBOSE, "vied_nci_is_cell_of_type(): enter: \n");

	return ((vied_nci_cell_get_type(cell_id) == cell_type_id) && (cell_type_id != VIED_NCI_N_CELL_TYPE_ID));
}

bool vied_nci_is_mem_of_type(
	const vied_nci_mem_ID_t			mem_id,
	const vied_nci_mem_type_ID_t	mem_type_id)
{
	IA_CSS_TRACE_0(PSYSAPI_SIM, VERBOSE, "vied_nci_is_mem_of_type(): enter: \n");

	return ((vied_nci_mem_get_type(mem_id) == mem_type_id) && (mem_type_id != VIED_NCI_N_MEM_TYPE_ID));
}

bool vied_nci_is_cell_mem_of_type(
	const vied_nci_cell_ID_t		cell_id,
	const uint16_t					mem_index,
	const vied_nci_mem_type_ID_t	mem_type_id)
{
	IA_CSS_TRACE_0(PSYSAPI_SIM, VERBOSE, "vied_nci_is_cell_mem_of_type(): enter: \n");

	return ((vied_nci_cell_get_mem_type(cell_id, mem_index) == mem_type_id) && (mem_type_id != VIED_NCI_N_MEM_TYPE_ID));
}

bool vied_nci_has_cell_mem_of_id(
	const vied_nci_cell_ID_t		cell_id,
	const vied_nci_mem_ID_t			mem_id)
{
	uint16_t		mem_index;

	IA_CSS_TRACE_0(PSYSAPI_SIM, VERBOSE, "vied_nci_has_cell_mem_of_id(): enter: \n");

	for (mem_index = 0; mem_index < VIED_NCI_N_MEM_TYPE_ID; mem_index++) {
		if ((vied_nci_cell_get_mem(cell_id, mem_index) == mem_id) && (mem_id != VIED_NCI_N_MEM_ID)) {
			break;
		}
	}

	return (mem_index < VIED_NCI_N_MEM_TYPE_ID);
}

uint16_t vied_nci_cell_get_mem_count(
	const vied_nci_cell_ID_t		cell_id)
{
	uint16_t	mem_count = 0;
	vied_nci_cell_type_ID_t	cell_type;

	IA_CSS_TRACE_0(PSYSAPI_SIM, VERBOSE, "vied_nci_cell_get_mem_count(): enter: \n");

	cell_type = vied_nci_cell_get_type(cell_id);

	if (cell_type < VIED_NCI_N_CELL_TYPE_ID) {
		mem_count = vied_nci_N_cell_mem[cell_type];
	}

	return mem_count;
}

vied_nci_mem_type_ID_t vied_nci_cell_get_mem_type(
	const vied_nci_cell_ID_t		cell_id,
	const uint16_t					mem_index)
{
	vied_nci_mem_type_ID_t	mem_type = VIED_NCI_N_MEM_TYPE_ID;

	IA_CSS_TRACE_0(PSYSAPI_SIM, VERBOSE, "vied_nci_cell_get_mem_type(): enter: \n");

	if ((cell_id < VIED_NCI_N_CELL_ID) && (mem_index < VIED_NCI_N_MEM_TYPE_ID)) {
		mem_type = vied_nci_cell_mem_type[vied_nci_cell_get_type(cell_id)][mem_index];
	}

	return mem_type;
}

vied_nci_mem_ID_t vied_nci_cell_get_mem(
	const vied_nci_cell_ID_t		cell_id,
	const uint16_t					mem_index)
{
	vied_nci_mem_ID_t	mem_id = VIED_NCI_N_MEM_ID;

	IA_CSS_TRACE_0(PSYSAPI_SIM, VERBOSE, "vied_nci_cell_get_mem(): enter: \n");

	if ((cell_id < VIED_NCI_N_CELL_ID) && (mem_index < VIED_NCI_N_MEM_TYPE_ID)) {
		mem_id = vied_nci_cell_mem[cell_id][mem_index];
	}

	return mem_id;
}

vied_nci_mem_type_ID_t vied_nci_cell_type_get_mem_type(
	const vied_nci_cell_type_ID_t	cell_type_id,
	const uint16_t					mem_index)
{
	vied_nci_mem_type_ID_t	mem_type = VIED_NCI_N_MEM_TYPE_ID;

	IA_CSS_TRACE_0(PSYSAPI_SIM, VERBOSE, "vied_nci_cell_type_get_mem_type(): enter: \n");

	if ((cell_type_id < VIED_NCI_N_CELL_TYPE_ID) && (mem_index < VIED_NCI_N_MEM_TYPE_ID)) {
		mem_type = vied_nci_cell_mem_type[cell_type_id][mem_index];
	}

	return mem_type;
}

