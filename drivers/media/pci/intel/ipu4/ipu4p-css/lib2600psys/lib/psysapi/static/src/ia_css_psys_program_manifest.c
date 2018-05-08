/*
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


#include <ia_css_psys_program_manifest.h>
#include <ia_css_psys_program_group_manifest.h>
/* for ia_css_kernel_bitmap_t, ia_css_kernel_bitmap_print */
#include <ia_css_kernel_bitmap.h>

#include <vied_nci_psys_system_global.h>
#include "ia_css_psys_program_group_private.h"
#include "ia_css_psys_static_trace.h"

#include <error_support.h>
#include <misc_support.h>

size_t ia_css_sizeof_program_manifest(
	const uint8_t program_dependency_count,
	const uint8_t terminal_dependency_count)
{
	size_t	size = 0;

	IA_CSS_TRACE_0(PSYSAPI_STATIC, VERBOSE,
		"ia_css_sizeof_program_manifest(): enter:\n");

	size += sizeof(ia_css_program_manifest_t);
	size += program_dependency_count * sizeof(uint8_t);
	size += terminal_dependency_count * sizeof(uint8_t);
	size = ceil_mul(size, sizeof(uint64_t));

	return size;
}

bool ia_css_has_program_manifest_fixed_cell(
	const ia_css_program_manifest_t			*manifest)
{
	bool	has_fixed_cell = false;

	vied_nci_cell_ID_t		cell_id;
	vied_nci_cell_type_ID_t	cell_type_id;

	IA_CSS_TRACE_0(PSYSAPI_STATIC, VERBOSE,
		       "ia_css_has_program_manifest_fixed_cell(): enter:\n");

	verifexit(manifest != NULL);

	cell_id = ia_css_program_manifest_get_cell_ID(manifest);
	cell_type_id = ia_css_program_manifest_get_cell_type_ID(manifest);

	has_fixed_cell = ((cell_id != VIED_NCI_N_CELL_ID) &&
			  (cell_type_id == VIED_NCI_N_CELL_TYPE_ID));

EXIT:
	if (NULL == manifest) {
		IA_CSS_TRACE_0(PSYSAPI_STATIC, WARNING,
		   "ia_css_has_program_manifest_fixed_cell invalid argument\n");
	}
	return has_fixed_cell;
}

size_t ia_css_program_manifest_get_size(
	const ia_css_program_manifest_t			*manifest)
{
	size_t	size = 0;

	IA_CSS_TRACE_0(PSYSAPI_STATIC, VERBOSE,
		"ia_css_program_manifest_get_size(): enter:\n");

	if (manifest != NULL) {
		size = manifest->size;
	} else {
		IA_CSS_TRACE_0(PSYSAPI_STATIC, WARNING,
			"ia_css_program_manifest_get_size invalid argument\n");
	}

	return size;
}

ia_css_program_ID_t ia_css_program_manifest_get_program_ID(
	const ia_css_program_manifest_t			*manifest)
{
	ia_css_program_ID_t		program_id = IA_CSS_PROGRAM_INVALID_ID;

	IA_CSS_TRACE_0(PSYSAPI_STATIC, VERBOSE,
		"ia_css_program_manifest_get_program_ID(): enter:\n");

	if (manifest != NULL) {
		program_id = manifest->ID;
	} else {
		IA_CSS_TRACE_0(PSYSAPI_STATIC, WARNING,
		   "ia_css_program_manifest_get_program_ID invalid argument\n");
	}
	return program_id;
}

int ia_css_program_manifest_set_program_ID(
	ia_css_program_manifest_t			*manifest,
	ia_css_program_ID_t id)
{
	int ret = -1;

	IA_CSS_TRACE_0(PSYSAPI_STATIC, VERBOSE,
		"ia_css_program_manifest_set_program_ID(): enter:\n");

	if (manifest != NULL) {
		manifest->ID = id;
		ret = 0;
	} else {
		IA_CSS_TRACE_1(PSYSAPI_STATIC, ERROR,
		   "ia_css_program_manifest_set_program_ID failed (%i)\n", ret);
	}
	return ret;
}

ia_css_program_group_manifest_t *ia_css_program_manifest_get_parent(
	const ia_css_program_manifest_t			*manifest)
{
	ia_css_program_group_manifest_t	*parent = NULL;
	char *base;

	IA_CSS_TRACE_0(PSYSAPI_STATIC, VERBOSE,
		"ia_css_program_manifest_get_parent(): enter:\n");

	verifexit(manifest != NULL);

	base = (char *)((char *)manifest + manifest->parent_offset);

	parent = (ia_css_program_group_manifest_t *) (base);
EXIT:
	if (NULL == manifest) {
		IA_CSS_TRACE_0(PSYSAPI_STATIC, WARNING,
		       "ia_css_program_manifest_get_parent invalid argument\n");
	}
	return parent;
}

int ia_css_program_manifest_set_parent_offset(
	ia_css_program_manifest_t	*manifest,
	int32_t program_offset)
{
	int	retval = -1;

	IA_CSS_TRACE_0(PSYSAPI_STATIC, VERBOSE,
		"ia_css_program_manifest_set_parent_offset(): enter:\n");

	verifexit(manifest != NULL);

	/* parent is at negative offset away from current program offset*/
	manifest->parent_offset = -program_offset;

	retval = 0;
EXIT:
	if (retval != 0) {
		IA_CSS_TRACE_1(PSYSAPI_STATIC, ERROR,
			"ia_css_program_manifest_set_parent_offset failed (%i)\n",
			retval);
	}
	return retval;
}

ia_css_program_type_t ia_css_program_manifest_get_type(
	const ia_css_program_manifest_t			*manifest)
{
	ia_css_program_type_t	program_type = IA_CSS_N_PROGRAM_TYPES;

	IA_CSS_TRACE_0(PSYSAPI_STATIC, VERBOSE,
		"ia_css_program_manifest_get_type(): enter:\n");

	if (manifest != NULL) {
		program_type = manifest->program_type;
	} else {
		IA_CSS_TRACE_0(PSYSAPI_STATIC, WARNING,
			"ia_css_program_manifest_get_type invalid argument\n");
	}
	return program_type;
}

int ia_css_program_manifest_set_type(
	ia_css_program_manifest_t				*manifest,
	const ia_css_program_type_t				program_type)
{
	int	retval = -1;

	IA_CSS_TRACE_0(PSYSAPI_STATIC, VERBOSE,
		"ia_css_program_manifest_set_type(): enter:\n");

	if (manifest != NULL) {
		manifest->program_type = program_type;
		retval = 0;
	} else {
		IA_CSS_TRACE_1(PSYSAPI_STATIC, ERROR,
		      "ia_css_program_manifest_set_type failed (%i)\n", retval);
	}
	return retval;
}

ia_css_kernel_bitmap_t ia_css_program_manifest_get_kernel_bitmap(
	const ia_css_program_manifest_t			*manifest)
{
	ia_css_kernel_bitmap_t	kernel_bitmap = ia_css_kernel_bitmap_clear();

	IA_CSS_TRACE_0(PSYSAPI_STATIC, VERBOSE,
		"ia_css_program_manifest_get_kernel_bitmap(): enter:\n");

	if (manifest != NULL) {
		kernel_bitmap = manifest->kernel_bitmap;
	} else {
		IA_CSS_TRACE_0(PSYSAPI_STATIC, WARNING,
		"ia_css_program_manifest_get_kernel_bitmap invalid argument\n");
	}
	return kernel_bitmap;
}

int ia_css_program_manifest_set_kernel_bitmap(
	ia_css_program_manifest_t			*manifest,
	const ia_css_kernel_bitmap_t			kernel_bitmap)
{
	int	retval = -1;

	IA_CSS_TRACE_0(PSYSAPI_STATIC, VERBOSE,
		"ia_css_program_manifest_set_kernel_bitmap(): enter:\n");

	if (manifest != NULL) {
		manifest->kernel_bitmap = kernel_bitmap;
		retval = 0;
	} else {
		IA_CSS_TRACE_1(PSYSAPI_STATIC, ERROR,
			"ia_css_program_manifest_set_kernel_bitmap failed (%i)\n",
			retval);
	}
	return retval;
}

vied_nci_cell_ID_t ia_css_program_manifest_get_cell_ID(
	const ia_css_program_manifest_t			*manifest)
{
	vied_nci_cell_ID_t		cell_id = VIED_NCI_N_CELL_ID;
#if IA_CSS_PROCESS_MAX_CELLS > 1
	int i = 0;
#endif /* IA_CSS_PROCESS_MAX_CELLS > 1 */
	IA_CSS_TRACE_0(PSYSAPI_STATIC, VERBOSE,
		"ia_css_program_manifest_get_cell_ID(): enter:\n");

	verifexit(manifest != NULL);

#if IA_CSS_PROCESS_MAX_CELLS == 1
	cell_id = manifest->cell_id;
#else
	for (i = 1; i < IA_CSS_PROCESS_MAX_CELLS; i++) {
		assert(VIED_NCI_N_CELL_ID == manifest->cells[i]);
#ifdef __HIVECC
#pragma hivecc unroll
#endif
	}
	cell_id = manifest->cells[0];
#endif /* IA_CSS_PROCESS_MAX_CELLS == 1 */
EXIT:
	if (NULL == manifest) {
		IA_CSS_TRACE_0(PSYSAPI_STATIC, WARNING,
		      "ia_css_program_manifest_get_cell_ID invalid argument\n");
	}
	return cell_id;
}

int ia_css_program_manifest_set_cell_ID(
	ia_css_program_manifest_t			*manifest,
	const vied_nci_cell_ID_t			cell_id)
{
	int	retval = -1;
#if IA_CSS_PROCESS_MAX_CELLS > 1
	int i = 0;
#endif /* IA_CSS_PROCESS_MAX_CELLS > 1 */
	IA_CSS_TRACE_0(PSYSAPI_STATIC, VERBOSE,
		"ia_css_program_manifest_set_cell_ID(): enter:\n");
	if (manifest != NULL) {
#if IA_CSS_PROCESS_MAX_CELLS == 1
		manifest->cell_id = cell_id;
#else
		manifest->cells[0] = cell_id;
		for (i = 1; i < IA_CSS_PROCESS_MAX_CELLS; i++) {
			manifest->cells[i] = VIED_NCI_N_CELL_ID;
		}
#endif /* IA_CSS_PROCESS_MAX_CELLS == 1 */
		retval = 0;
	} else {
		IA_CSS_TRACE_1(PSYSAPI_STATIC, ERROR,
		   "ia_css_program_manifest_set_cell_ID failed (%i)\n", retval);
	}
	return retval;
}

vied_nci_cell_type_ID_t ia_css_program_manifest_get_cell_type_ID(
	const ia_css_program_manifest_t			*manifest)
{
	vied_nci_cell_type_ID_t	cell_type_id = VIED_NCI_N_CELL_TYPE_ID;

	IA_CSS_TRACE_0(PSYSAPI_STATIC, VERBOSE,
		"ia_css_program_manifest_get_cell_type_ID(): enter:\n");

	verifexit(manifest != NULL);

	cell_type_id = (vied_nci_cell_type_ID_t)(manifest->cell_type_id);
EXIT:
	if (NULL == manifest) {
		IA_CSS_TRACE_0(PSYSAPI_STATIC, WARNING,
		 "ia_css_program_manifest_get_cell_type_ID invalid argument\n");
	}
	return cell_type_id;
}

int ia_css_program_manifest_set_cell_type_ID(
	ia_css_program_manifest_t			*manifest,
	const vied_nci_cell_type_ID_t			cell_type_id)
{
	int	retval = -1;

	IA_CSS_TRACE_0(PSYSAPI_STATIC, VERBOSE,
		"ia_css_program_manifest_set_cell_type_ID(): enter:\n");
	if (manifest != NULL) {
		manifest->cell_type_id = cell_type_id;
		retval = 0;
	} else {
		IA_CSS_TRACE_1(PSYSAPI_STATIC, ERROR,
			"ia_css_program_manifest_set_cell_type_ID failed (%i)\n",
			retval);
	}
	return retval;
}

vied_nci_resource_size_t ia_css_program_manifest_get_int_mem_size(
	const ia_css_program_manifest_t			*manifest,
	const vied_nci_mem_type_ID_t			mem_type_id)
{
	vied_nci_resource_size_t	int_mem_size = 0;
	vied_nci_cell_type_ID_t	cell_type_id;
	int mem_index;

	IA_CSS_TRACE_0(PSYSAPI_STATIC, VERBOSE,
		"ia_css_program_manifest_get_int_mem_size(): enter:\n");

	verifexit(manifest != NULL);
	verifexit(mem_type_id < VIED_NCI_N_MEM_TYPE_ID);

	if (ia_css_has_program_manifest_fixed_cell(manifest)) {
		vied_nci_cell_ID_t cell_id =
				  ia_css_program_manifest_get_cell_ID(manifest);

		cell_type_id = vied_nci_cell_get_type(cell_id);
	} else {
		cell_type_id =
			ia_css_program_manifest_get_cell_type_ID(manifest);
	}

	/* loop over vied_nci_cell_mem_type to verify mem_type_id for a
	 * specific cell_type_id
	 */
	for (mem_index = 0; mem_index < VIED_NCI_N_MEM_TYPE_ID; mem_index++) {
		if ((int)mem_type_id ==
		    (int)vied_nci_cell_type_get_mem_type(
				cell_type_id, mem_index)) {
			int_mem_size = manifest->int_mem_size[mem_index];
		}
	}

EXIT:
	if (NULL == manifest || mem_type_id >= VIED_NCI_N_MEM_TYPE_ID) {
		IA_CSS_TRACE_0(PSYSAPI_STATIC, WARNING,
		 "ia_css_program_manifest_get_int_mem_size invalid argument\n");
	}
	return int_mem_size;
}

int ia_css_program_manifest_set_cells_bitmap(
	ia_css_program_manifest_t			*manifest,
	const vied_nci_resource_bitmap_t	bitmap)
{
	int retval = -1;
	int array_index = 0;
	int bit_index;

	IA_CSS_TRACE_0(PSYSAPI_STATIC, VERBOSE,
		       "ia_css_program_manifest_set_cells_bitmap(): enter:\n");

	if (manifest != NULL) {
		for (bit_index = 0; bit_index < VIED_NCI_N_CELL_ID; bit_index++) {
			if (vied_nci_is_bit_set_in_bitmap(bitmap, bit_index)) {
				verifexit(array_index < IA_CSS_PROCESS_MAX_CELLS);
#if IA_CSS_PROCESS_MAX_CELLS == 1
				manifest->cell_id = (vied_nci_cell_ID_t)bit_index;
#else
				manifest->cells[array_index] = (vied_nci_cell_ID_t)bit_index;
#endif /* IA_CSS_PROCESS_MAX_CELLS == 1 */
				array_index++;
			}
		}
		for (; array_index < IA_CSS_PROCESS_MAX_CELLS; array_index++) {
#if IA_CSS_PROCESS_MAX_CELLS == 1
			manifest->cell_id = VIED_NCI_N_CELL_ID;
#else
			manifest->cells[array_index] = VIED_NCI_N_CELL_ID;
#endif /* IA_CSS_PROCESS_MAX_CELLS */
		}
		retval = 0;
	} else {
		IA_CSS_TRACE_0(PSYSAPI_STATIC, WARNING,
		 "ia_css_program_manifest_set_cells_bitmap invalid argument\n");
	}
EXIT:
	return retval;
}

vied_nci_resource_bitmap_t ia_css_program_manifest_get_cells_bitmap(
	const ia_css_program_manifest_t			*manifest)
{
	vied_nci_resource_bitmap_t	bitmap = 0;
#if IA_CSS_PROCESS_MAX_CELLS > 1
	int i = 0;
#endif /* IA_CSS_PROCESS_MAX_CELLS > 1 */
	IA_CSS_TRACE_0(PSYSAPI_STATIC, VERBOSE,
		"ia_css_program_manifest_get_cells_bitmap(): enter:\n");

	verifexit(manifest != NULL);

#if IA_CSS_PROCESS_MAX_CELLS == 1
	bitmap = (1 << manifest->cell_id);
#else
	for (i = 0; i < IA_CSS_PROCESS_MAX_CELLS; i++) {
		if (VIED_NCI_N_CELL_ID != manifest->cells[i]) {
			bitmap |= (1 << manifest->cells[i]);
		}
#ifdef __HIVECC
#pragma hivecc unroll
#endif
	}
#endif /* IA_CSS_PROCESS_MAX_CELLS == 1 */
EXIT:
	if (NULL == manifest) {
		IA_CSS_TRACE_0(PSYSAPI_STATIC, WARNING,
		 "ia_css_program_manifest_get_cells_bitmap invalid argument\n");
	}
	return bitmap;
}

int ia_css_program_manifest_set_dfm_port_bitmap(
	ia_css_program_manifest_t            *manifest,
	const vied_nci_dev_dfm_id_t          dfm_type_id,
	const vied_nci_resource_bitmap_t     bitmap)
{
	int retval = -1;

	IA_CSS_TRACE_0(PSYSAPI_STATIC, VERBOSE,
		       "ia_css_program_manifest_set_dfm_port_bitmap(): enter:\n");

	verifexit(manifest != NULL);
#if (VIED_NCI_N_DEV_DFM_ID > 0)
	verifexit(dfm_type_id < VIED_NCI_N_DEV_DFM_ID);
	manifest->dfm_port_bitmap[dfm_type_id] = bitmap;
#else
	(void)bitmap;
	(void)dfm_type_id;
#endif
	retval = 0;

EXIT:
	if (retval != 0) {
		IA_CSS_TRACE_0(PSYSAPI_STATIC, WARNING,
				"ia_css_program_manifest_set_dfm_port_bitmap invalid argument\n");
	}
	return retval;
}

int ia_css_program_manifest_set_dfm_active_port_bitmap(
	ia_css_program_manifest_t           *manifest,
	const vied_nci_dev_dfm_id_t          dfm_type_id,
	const vied_nci_resource_bitmap_t     bitmap)
{
	int retval = -1;

	IA_CSS_TRACE_0(PSYSAPI_STATIC, VERBOSE,
			"ia_css_program_manifest_set_dfm_active_port_bitmap(): enter:\n");

	verifexit(manifest != NULL);
#if (VIED_NCI_N_DEV_DFM_ID > 0)
	verifexit(dfm_type_id < VIED_NCI_N_DEV_DFM_ID);
	manifest->dfm_active_port_bitmap[dfm_type_id] = bitmap;
#else
	(void)bitmap;
	(void)dfm_type_id;
#endif
	retval = 0;

EXIT:
	if (retval != 0) {
		IA_CSS_TRACE_0(PSYSAPI_STATIC, WARNING,
				"ia_css_program_manifest_set_dfm_active_port_bitmap invalid argument\n");
	}
	return retval;
}

int ia_css_program_manifest_set_is_dfm_relocatable(
	ia_css_program_manifest_t       *manifest,
	const vied_nci_dev_dfm_id_t      dfm_type_id,
	const uint8_t                    is_relocatable)
{
	int retval = -1;

	IA_CSS_TRACE_0(PSYSAPI_STATIC, VERBOSE,
			"ia_css_program_manifest_set_is_dfm_relocatable(): enter:\n");

	verifexit(manifest != NULL);
#if (VIED_NCI_N_DEV_DFM_ID > 0)
	verifexit(dfm_type_id < VIED_NCI_N_DEV_DFM_ID);
	manifest->is_dfm_relocatable[dfm_type_id] = is_relocatable;
#else
	(void)is_relocatable;
	(void)dfm_type_id;
#endif
	retval = 0;

	EXIT:
	if (retval != 0) {
		IA_CSS_TRACE_0(PSYSAPI_STATIC, WARNING,
				"ia_css_program_manifest_set_is_dfm_relocatable invalid argument\n");
	}

	return retval;
}

uint8_t ia_css_program_manifest_get_is_dfm_relocatable(
	const ia_css_program_manifest_t      *manifest,
	const vied_nci_dev_dfm_id_t    dfm_type_id)
{
	uint8_t	ret = 0;

	IA_CSS_TRACE_0(PSYSAPI_STATIC, VERBOSE,
		"ia_css_program_manifest_get_is_dfm_relocatable(): enter:\n");

	verifexit(manifest != NULL);
#if (VIED_NCI_N_DEV_DFM_ID > 0)
	verifexit(dfm_type_id < VIED_NCI_N_DEV_DFM_ID);
	ret = manifest->is_dfm_relocatable[dfm_type_id];
#else
	ret = 0;
	(void)dfm_type_id;
#endif
EXIT:
	if (NULL == manifest) {
		IA_CSS_TRACE_0(PSYSAPI_STATIC, WARNING,
		 "ia_css_program_manifest_get_is_dfm_relocatable invalid argument\n");
	}
	return ret;
}

vied_nci_resource_bitmap_t ia_css_program_manifest_get_dfm_port_bitmap(
	const ia_css_program_manifest_t  *manifest,
	const vied_nci_dev_dfm_id_t       dfm_type_id)
{
	vied_nci_resource_bitmap_t	bitmap = 0;

	IA_CSS_TRACE_0(PSYSAPI_STATIC, VERBOSE,
		"ia_css_program_manifest_get_dfm_port_bitmap(): enter:\n");

	verifexit(manifest != NULL);
#if (VIED_NCI_N_DEV_DFM_ID > 0)
	verifexit(dfm_type_id < VIED_NCI_N_DEV_DFM_ID);
	bitmap = manifest->dfm_port_bitmap[dfm_type_id];
#else
	bitmap = 0;
	(void)dfm_type_id;
#endif
EXIT:
	if (NULL == manifest) {
		IA_CSS_TRACE_0(PSYSAPI_STATIC, WARNING,
		 "ia_css_program_manifest_get_dfm_port_bitmap invalid argument\n");
	}
	return bitmap;
}

vied_nci_resource_bitmap_t ia_css_program_manifest_get_dfm_active_port_bitmap(
	const ia_css_program_manifest_t  *manifest,
	const vied_nci_dev_dfm_id_t      dfm_type_id)
{
	vied_nci_resource_bitmap_t	bitmap = 0;

	IA_CSS_TRACE_0(PSYSAPI_STATIC, VERBOSE,
		"ia_css_program_manifest_get_dfm_active_port_bitmap(): enter:\n");

	verifexit(manifest != NULL);
#if (VIED_NCI_N_DEV_DFM_ID > 0)
	verifexit(dfm_type_id < VIED_NCI_N_DEV_DFM_ID);
	bitmap = manifest->dfm_active_port_bitmap[dfm_type_id];
#else
	bitmap = 0;
	(void)dfm_type_id;
#endif
EXIT:
	if (NULL == manifest) {
		IA_CSS_TRACE_0(PSYSAPI_STATIC, WARNING,
		 "ia_css_program_manifest_get_dfm_active_port_bitmap invalid argument\n");
	}
	return bitmap;
}

int ia_css_program_manifest_set_int_mem_size(
	ia_css_program_manifest_t			*manifest,
	const vied_nci_mem_type_ID_t			mem_type_id,
	const vied_nci_resource_size_t			int_mem_size)
{
	int retval = -1;
	vied_nci_cell_type_ID_t	cell_type_id;
	int mem_index;

	IA_CSS_TRACE_0(PSYSAPI_STATIC, VERBOSE,
		"ia_css_program_manifest_set_int_mem_size(): enter:\n");

	if (ia_css_has_program_manifest_fixed_cell(manifest)) {
		vied_nci_cell_ID_t cell_id =
			ia_css_program_manifest_get_cell_ID(manifest);

		cell_type_id = vied_nci_cell_get_type(cell_id);
	} else {
		cell_type_id =
			ia_css_program_manifest_get_cell_type_ID(manifest);
	}

	if (manifest != NULL && mem_type_id < VIED_NCI_N_MEM_TYPE_ID) {
		/* loop over vied_nci_cell_mem_type to verify mem_type_id for
		* a specific cell_type_id
		*/
		for (mem_index = 0; mem_index < VIED_NCI_N_MEM_TYPE_ID;
		     mem_index++) {
			if ((int)mem_type_id ==
				(int)vied_nci_cell_type_get_mem_type(
					cell_type_id, mem_index)) {
				manifest->int_mem_size[mem_index] =
					int_mem_size;
				retval = 0;
			}
		}
	}
	if (retval != 0) {
		IA_CSS_TRACE_2(PSYSAPI_STATIC, ERROR,
			"ia_css_program_manifest_set_int_mem_size cell_type_id %d has no mem_type_id %d\n",
			(int)cell_type_id, (int)mem_type_id);
	}

	return retval;
}

vied_nci_resource_size_t ia_css_program_manifest_get_ext_mem_size(
	const ia_css_program_manifest_t			*manifest,
	const vied_nci_mem_type_ID_t			mem_type_id)
{
	vied_nci_resource_size_t	ext_mem_size = 0;

	IA_CSS_TRACE_0(PSYSAPI_STATIC, VERBOSE,
		       "ia_css_program_manifest_get_ext_mem_size(): enter:\n");

	verifexit(manifest != NULL);
	verifexit(mem_type_id < VIED_NCI_N_DATA_MEM_TYPE_ID);

	ext_mem_size = manifest->ext_mem_size[mem_type_id];
EXIT:
	if (NULL == manifest || mem_type_id >= VIED_NCI_N_DATA_MEM_TYPE_ID) {
		IA_CSS_TRACE_0(PSYSAPI_STATIC, WARNING,
		 "ia_css_program_manifest_get_ext_mem_size invalid argument\n");
	}
	return ext_mem_size;
}

vied_nci_resource_size_t ia_css_program_manifest_get_ext_mem_offset(
	const ia_css_program_manifest_t			*manifest,
	const vied_nci_mem_type_ID_t			mem_type_id)
{
	vied_nci_resource_size_t	ext_mem_offset = 0;

	IA_CSS_TRACE_0(PSYSAPI_STATIC, VERBOSE,
		       "ia_css_program_manifest_get_ext_mem_offset(): enter:\n");

	verifexit(manifest != NULL);
	verifexit(mem_type_id < VIED_NCI_N_DATA_MEM_TYPE_ID);

	ext_mem_offset = manifest->ext_mem_offset[mem_type_id];
EXIT:
	if (NULL == manifest || mem_type_id >= VIED_NCI_N_DATA_MEM_TYPE_ID) {
		IA_CSS_TRACE_0(PSYSAPI_STATIC, WARNING,
		 "ia_css_program_manifest_get_ext_mem_offset invalid argument\n");
	}
	return ext_mem_offset;
}

int ia_css_program_manifest_set_ext_mem_size(
	ia_css_program_manifest_t			*manifest,
	const vied_nci_mem_type_ID_t			mem_type_id,
	const vied_nci_resource_size_t			ext_mem_size)
{
	int retval = -1;

	IA_CSS_TRACE_0(PSYSAPI_STATIC, VERBOSE,
		       "ia_css_program_manifest_set_ext_mem_size(): enter:\n");

	if (manifest != NULL && mem_type_id < VIED_NCI_N_DATA_MEM_TYPE_ID) {
		manifest->ext_mem_size[mem_type_id] = ext_mem_size;
		retval = 0;
	} else {
		IA_CSS_TRACE_0(PSYSAPI_STATIC, WARNING,
		 "ia_css_program_manifest_set_ext_mem_size invalid argument\n");
	}

	return retval;
}

int ia_css_program_manifest_set_ext_mem_offset(
	ia_css_program_manifest_t			*manifest,
	const vied_nci_mem_type_ID_t			mem_type_id,
	const vied_nci_resource_size_t			ext_mem_offset)
{
	int retval = -1;

	IA_CSS_TRACE_0(PSYSAPI_STATIC, VERBOSE,
		       "ia_css_program_manifest_set_ext_mem_offset(): enter:\n");

	if (manifest != NULL && mem_type_id < VIED_NCI_N_DATA_MEM_TYPE_ID) {
		manifest->ext_mem_offset[mem_type_id] = ext_mem_offset;
		retval = 0;
	} else {
		IA_CSS_TRACE_0(PSYSAPI_STATIC, WARNING,
		 "ia_css_program_manifest_set_ext_mem_offset invalid argument\n");
	}

	return retval;
}

vied_nci_resource_size_t ia_css_program_manifest_get_dev_chn_size(
	const ia_css_program_manifest_t			*manifest,
	const vied_nci_dev_chn_ID_t				dev_chn_id)
{
	vied_nci_resource_size_t	dev_chn_size = 0;

	IA_CSS_TRACE_0(PSYSAPI_STATIC, VERBOSE,
		"ia_css_program_manifest_get_dev_chn_size(): enter:\n");

	verifexit(manifest != NULL);
	verifexit(dev_chn_id < VIED_NCI_N_DEV_CHN_ID);

	dev_chn_size = manifest->dev_chn_size[dev_chn_id];
EXIT:
	if (NULL == manifest || dev_chn_id >= VIED_NCI_N_DEV_CHN_ID) {
		IA_CSS_TRACE_0(PSYSAPI_STATIC, WARNING,
		 "ia_css_program_manifest_get_dev_chn_size invalid argument\n");
	}
	return dev_chn_size;
}

vied_nci_resource_size_t ia_css_program_manifest_get_dev_chn_offset(
	const ia_css_program_manifest_t			*manifest,
	const vied_nci_dev_chn_ID_t				dev_chn_id)
{
	vied_nci_resource_size_t	dev_chn_offset = 0;

	IA_CSS_TRACE_0(PSYSAPI_STATIC, VERBOSE,
		"ia_css_program_manifest_get_dev_chn_offset(): enter:\n");

	verifexit(manifest != NULL);
	verifexit(dev_chn_id < VIED_NCI_N_DEV_CHN_ID);

	dev_chn_offset = manifest->dev_chn_offset[dev_chn_id];
EXIT:
	if (NULL == manifest || dev_chn_id >= VIED_NCI_N_DEV_CHN_ID) {
		IA_CSS_TRACE_0(PSYSAPI_STATIC, WARNING,
		 "ia_css_program_manifest_get_dev_chn_offset invalid argument\n");
	}
	return dev_chn_offset;
}

int ia_css_program_manifest_set_dev_chn_size(
	ia_css_program_manifest_t			*manifest,
	const vied_nci_dev_chn_ID_t			dev_chn_id,
	const vied_nci_resource_size_t			dev_chn_size)
{
	int retval = -1;

	IA_CSS_TRACE_0(PSYSAPI_STATIC, VERBOSE,
		       "ia_css_program_manifest_set_dev_chn_size(): enter:\n");

	if (manifest != NULL && dev_chn_id < VIED_NCI_N_DEV_CHN_ID) {
		manifest->dev_chn_size[dev_chn_id] = dev_chn_size;
		retval = 0;
	} else {
		IA_CSS_TRACE_0(PSYSAPI_STATIC, WARNING,
		 "ia_css_program_manifest_set_dev_chn_size invalid argument\n");
	}

	return retval;
}

int ia_css_program_manifest_set_dev_chn_offset(
	ia_css_program_manifest_t			*manifest,
	const vied_nci_dev_chn_ID_t			dev_chn_id,
	const vied_nci_resource_size_t			dev_chn_offset)
{
	int retval = -1;

	IA_CSS_TRACE_0(PSYSAPI_STATIC, VERBOSE,
		       "ia_css_program_manifest_set_dev_chn_offset(): enter:\n");

	if (manifest != NULL && dev_chn_id < VIED_NCI_N_DEV_CHN_ID) {
		manifest->dev_chn_offset[dev_chn_id] = dev_chn_offset;
		retval = 0;
	} else {
		IA_CSS_TRACE_0(PSYSAPI_STATIC, WARNING,
		 "ia_css_program_manifest_set_dev_chn_offset invalid argument\n");
	}

	return retval;
}

uint8_t ia_css_program_manifest_get_program_dependency_count(
	const ia_css_program_manifest_t			*manifest)
{
	uint8_t	program_dependency_count = 0;

	IA_CSS_TRACE_0(PSYSAPI_STATIC, VERBOSE,
	    "ia_css_program_manifest_get_program_dependency_count(): enter:\n");

	if (manifest != NULL) {
		program_dependency_count = manifest->program_dependency_count;
	} else {
		IA_CSS_TRACE_0(PSYSAPI_STATIC, WARNING,
			"ia_css_program_manifest_get_program_dependency_count invalid argument\n");
	}
	return program_dependency_count;
}

uint8_t ia_css_program_manifest_get_program_dependency(
	const ia_css_program_manifest_t			*manifest,
	const unsigned int				index)
{
	uint8_t program_dependency = IA_CSS_PROGRAM_INVALID_DEPENDENCY;
	uint8_t *program_dep_ptr;
	uint8_t program_dependency_count;

	IA_CSS_TRACE_0(PSYSAPI_STATIC, VERBOSE,
		"ia_css_program_manifest_get_program_dependency(): enter:\n");

	program_dependency_count =
		ia_css_program_manifest_get_program_dependency_count(manifest);

	if (index < program_dependency_count) {
		program_dep_ptr =
			(uint8_t *)((uint8_t *)manifest +
				manifest->program_dependency_offset +
				index * sizeof(uint8_t));
		program_dependency = *program_dep_ptr;
	} else {
		IA_CSS_TRACE_0(PSYSAPI_STATIC, WARNING,
			"ia_css_program_manifest_get_program_dependency invalid argument\n");
	}
	return program_dependency;
}

int ia_css_program_manifest_set_program_dependency(
	ia_css_program_manifest_t	*manifest,
	const uint8_t			program_dependency,
	const unsigned int		index)
{
	int	retval = -1;
	uint8_t *program_dep_ptr;
	uint8_t	program_dependency_count;
	uint8_t	program_count;

	IA_CSS_TRACE_0(PSYSAPI_STATIC, VERBOSE,
		"ia_css_program_manifest_set_program_dependency(): enter:\n");

	program_dependency_count =
		ia_css_program_manifest_get_program_dependency_count(manifest);
	program_count =
		ia_css_program_group_manifest_get_program_count(
			ia_css_program_manifest_get_parent(manifest));

	if ((index < program_dependency_count) &&
	    (program_dependency < program_count)) {
		program_dep_ptr = (uint8_t *)((uint8_t *)manifest +
				  manifest->program_dependency_offset +
				  index*sizeof(uint8_t));
		 *program_dep_ptr = program_dependency;
		retval = 0;
	}

	if (retval != 0) {
		IA_CSS_TRACE_3(PSYSAPI_STATIC, ERROR,
			"ia_css_program_manifest_set_program_dependency(m, %d, %d) failed (%i)\n",
			program_dependency, index, retval);
	}
	return retval;
}

uint8_t ia_css_program_manifest_get_terminal_dependency_count(
	const ia_css_program_manifest_t			*manifest)
{
	uint8_t	terminal_dependency_count = 0;

	IA_CSS_TRACE_0(PSYSAPI_STATIC, VERBOSE,
		"ia_css_program_manifest_get_terminal_dependency_count(): enter:\n");

	if (manifest != NULL) {
		terminal_dependency_count = manifest->terminal_dependency_count;
	} else {
		IA_CSS_TRACE_0(PSYSAPI_STATIC, WARNING,
			"ia_css_program_manifest_get_terminal_dependency_count invalid argument\n");
	}
	return terminal_dependency_count;
}

uint8_t ia_css_program_manifest_get_terminal_dependency(
	const ia_css_program_manifest_t			*manifest,
	const unsigned int				index)
{
	uint8_t terminal_dependency = IA_CSS_PROGRAM_INVALID_DEPENDENCY;
	uint8_t *terminal_dep_ptr;
	uint8_t terminal_dependency_count =
		ia_css_program_manifest_get_terminal_dependency_count(manifest);

	IA_CSS_TRACE_0(PSYSAPI_STATIC, VERBOSE,
		"ia_css_program_manifest_get_terminal_dependency(): enter:\n");

	if (index < terminal_dependency_count) {
		terminal_dep_ptr = (uint8_t *)((uint8_t *)manifest +
				  manifest->terminal_dependency_offset + index);
		terminal_dependency = *terminal_dep_ptr;
	} else {
		IA_CSS_TRACE_0(PSYSAPI_STATIC, WARNING,
			"ia_css_program_manifest_get_terminal_dependency invalid argument\n");
	}
	return terminal_dependency;
}

int ia_css_program_manifest_set_terminal_dependency(
	ia_css_program_manifest_t			*manifest,
	const uint8_t					terminal_dependency,
	const unsigned int				index)
{
	int	retval = -1;
	uint8_t *terminal_dep_ptr;
	uint8_t terminal_dependency_count =
		ia_css_program_manifest_get_terminal_dependency_count(manifest);
	uint8_t terminal_count =
		ia_css_program_group_manifest_get_terminal_count(
			ia_css_program_manifest_get_parent(manifest));

	IA_CSS_TRACE_0(PSYSAPI_STATIC, VERBOSE,
		"ia_css_program_manifest_set_terminal_dependency(): enter:\n");

	if ((index < terminal_dependency_count) &&
			(terminal_dependency < terminal_count)) {
		terminal_dep_ptr = (uint8_t *)((uint8_t *)manifest +
				  manifest->terminal_dependency_offset + index);
		 *terminal_dep_ptr = terminal_dependency;
		retval = 0;
	}

	if (retval != 0) {
		IA_CSS_TRACE_1(PSYSAPI_STATIC, ERROR,
			"ia_css_program_manifest_set_terminal_dependency failed (%i)\n",
			retval);
	}
	return retval;
}

bool ia_css_is_program_manifest_subnode_program_type(
	const ia_css_program_manifest_t			*manifest)
{
	ia_css_program_type_t		program_type;

	IA_CSS_TRACE_0(PSYSAPI_STATIC, VERBOSE,
		"ia_css_is_program_manifest_subnode_program_type(): enter:\n");

	program_type = ia_css_program_manifest_get_type(manifest);
/* The error return is the limit value, so no need to check on the manifest
 * pointer
 */
	return (program_type == IA_CSS_PROGRAM_TYPE_PARALLEL_SUB) ||
			(program_type == IA_CSS_PROGRAM_TYPE_EXCLUSIVE_SUB) ||
			(program_type == IA_CSS_PROGRAM_TYPE_VIRTUAL_SUB);
}

bool ia_css_is_program_manifest_supernode_program_type(
	const ia_css_program_manifest_t			*manifest)
{
	ia_css_program_type_t		program_type;

	IA_CSS_TRACE_0(PSYSAPI_STATIC, VERBOSE,
	       "ia_css_is_program_manifest_supernode_program_type(): enter:\n");

	program_type = ia_css_program_manifest_get_type(manifest);

/* The error return is the limit value, so no need to check on the manifest
 * pointer
 */
	return (program_type == IA_CSS_PROGRAM_TYPE_PARALLEL_SUPER) ||
			(program_type == IA_CSS_PROGRAM_TYPE_EXCLUSIVE_SUPER) ||
			(program_type == IA_CSS_PROGRAM_TYPE_VIRTUAL_SUPER);
}

bool ia_css_is_program_manifest_singular_program_type(
	const ia_css_program_manifest_t			*manifest)
{
	ia_css_program_type_t		program_type;

	IA_CSS_TRACE_0(PSYSAPI_STATIC, VERBOSE,
		"ia_css_is_program_manifest_singular_program_type(): enter:\n");

	program_type = ia_css_program_manifest_get_type(manifest);

/* The error return is the limit value, so no need to check on the manifest
 * pointer
 */
	return (program_type == IA_CSS_PROGRAM_TYPE_SINGULAR);
}

void ia_css_program_manifest_init(
	ia_css_program_manifest_t	*blob,
	const uint8_t	program_dependency_count,
	const uint8_t	terminal_dependency_count)
{
	IA_CSS_TRACE_0(PSYSAPI_STATIC, INFO,
		"ia_css_program_manifest_init(): enter:\n");

	/*TODO: add assert*/
	if (!blob)
		return;

	blob->ID = 1;
	blob->program_dependency_count = program_dependency_count;
	blob->terminal_dependency_count = terminal_dependency_count;
	blob->program_dependency_offset = sizeof(ia_css_program_manifest_t);
	blob->terminal_dependency_offset = blob->program_dependency_offset +
		sizeof(uint8_t) * program_dependency_count;
	blob->size =
		(uint16_t)ia_css_sizeof_program_manifest(
				program_dependency_count,
				terminal_dependency_count);
}

/* We need to refactor those files in order to build in the firmware only
   what is needed, switches are put current to workaround compilation problems
   in the firmware (for example lack of uint64_t support)
   supported in the firmware
  */
#if !defined(__HIVECC)

#if defined(_MSC_VER)
/* WA for a visual studio compiler bug, refer to
 developercommunity.visualstudio.com/content/problem/209359/ice-with-fpfast-in-156-and-msvc-daily-1413263051-p.html
*/
#pragma optimize( "", off )
#endif

int ia_css_program_manifest_print(
	const ia_css_program_manifest_t *manifest,
	void *fid)
{
	int			retval = -1;
	int			i, mem_index, dev_chn_index;

	vied_nci_cell_type_ID_t	cell_type_id;
	uint8_t					program_dependency_count;
	uint8_t					terminal_dependency_count;
	ia_css_kernel_bitmap_t	bitmap;

	IA_CSS_TRACE_0(PSYSAPI_STATIC, INFO,
		       "ia_css_program_manifest_print(): enter:\n");

	verifexit(manifest != NULL);
	NOT_USED(fid);

	IA_CSS_TRACE_1(PSYSAPI_STATIC, INFO, "sizeof(manifest) = %d\n",
		(int)ia_css_program_manifest_get_size(manifest));
	IA_CSS_TRACE_1(PSYSAPI_STATIC, INFO, "program ID = %d\n",
		(int)ia_css_program_manifest_get_program_ID(manifest));

	bitmap = ia_css_program_manifest_get_kernel_bitmap(manifest);
	verifexit(ia_css_kernel_bitmap_print(bitmap, fid) == 0);

	if (ia_css_has_program_manifest_fixed_cell(manifest)) {
		vied_nci_cell_ID_t cell_id =
				  ia_css_program_manifest_get_cell_ID(manifest);

		cell_type_id = vied_nci_cell_get_type(cell_id);
		IA_CSS_TRACE_1(PSYSAPI_STATIC, INFO, "cell(program) = %d\n",
			(int)cell_id);
	} else {
		cell_type_id =
			ia_css_program_manifest_get_cell_type_ID(manifest);
	}
	IA_CSS_TRACE_1(PSYSAPI_STATIC, INFO, "cell type(program) = %d\n",
		(int)cell_type_id);

	for (mem_index = 0; mem_index < (int)VIED_NCI_N_MEM_TYPE_ID;
	     mem_index++) {
		IA_CSS_TRACE_1(PSYSAPI_STATIC, INFO,
			"\ttype(internal mem) type = %d\n",
		(int)vied_nci_cell_type_get_mem_type(cell_type_id, mem_index));
		IA_CSS_TRACE_1(PSYSAPI_STATIC, INFO,
			"\ttype(internal mem) size = %d\n",
			manifest->int_mem_size[mem_index]);
	}

	for (mem_index = 0; mem_index < (int)VIED_NCI_N_DATA_MEM_TYPE_ID;
	     mem_index++) {
		IA_CSS_TRACE_1(PSYSAPI_STATIC, INFO,
			"\ttype(external mem) type = %d\n",
			(int)(vied_nci_mem_type_ID_t)mem_index);
		IA_CSS_TRACE_1(PSYSAPI_STATIC, INFO,
			"\ttype(external mem) size = %d\n",
			manifest->ext_mem_size[mem_index]);
		IA_CSS_TRACE_1(PSYSAPI_STATIC, INFO,
			"\ttype(external mem) offset = %d\n",
			manifest->ext_mem_offset[mem_index]);
	}

	for (dev_chn_index = 0; dev_chn_index < (int)VIED_NCI_N_DEV_CHN_ID;
	     dev_chn_index++) {
		IA_CSS_TRACE_1(PSYSAPI_STATIC, INFO,
			"\ttype(device channel) type = %d\n",
			(int)dev_chn_index);
		IA_CSS_TRACE_1(PSYSAPI_STATIC, INFO,
			"\ttype(device channel) size = %d\n",
			manifest->dev_chn_size[dev_chn_index]);
		IA_CSS_TRACE_1(PSYSAPI_STATIC, INFO,
			"\ttype(device channel) offset = %d\n",
			manifest->dev_chn_offset[dev_chn_index]);
	}
#if HAS_DFM
	for (dev_chn_index = 0; dev_chn_index < (int)VIED_NCI_N_DEV_DFM_ID;
		dev_chn_index++) {
		IA_CSS_TRACE_1(PSYSAPI_STATIC, INFO,
			"\ttype(dfm port) type = %d\n",
			(int)dev_chn_index);
		IA_CSS_TRACE_1(PSYSAPI_STATIC, INFO,
			"\ttype(dfm port) port_bitmap = %d\n",
			manifest->dfm_port_bitmap[dev_chn_index]);
		IA_CSS_TRACE_1(PSYSAPI_STATIC, INFO,
			"\ttype(dfm port) active_port_bitmap = %d\n",
			manifest->dfm_active_port_bitmap[dev_chn_index]);

		IA_CSS_TRACE_1(PSYSAPI_STATIC, INFO,
			"\ttype(dfm port) is_dfm_relocatable = %d\n",
			manifest->is_dfm_relocatable[dev_chn_index]);
	}
#endif

#if IA_CSS_PROCESS_MAX_CELLS == 1
	IA_CSS_TRACE_1(PSYSAPI_STATIC, INFO,
			"\ttype(cells) bitmap = %d\n",
			manifest->cell_id);
#else
	for (i = 0; i < IA_CSS_PROCESS_MAX_CELLS; i++) {
		IA_CSS_TRACE_1(PSYSAPI_STATIC, INFO,
			"\ttype(cells) bitmap = %d\n",
			manifest->cells[i]);
	}
#endif /* IA_CSS_PROCESS_MAX_CELLS == 1 */
	program_dependency_count =
		ia_css_program_manifest_get_program_dependency_count(manifest);
	if (program_dependency_count == 0) {
		IA_CSS_TRACE_1(PSYSAPI_STATIC, INFO,
			"program_dependencies[%d] {};\n",
			program_dependency_count);
	} else {
		uint8_t prog_dep;

		IA_CSS_TRACE_1(PSYSAPI_STATIC, INFO,
			"program_dependencies[%d] {\n",
			program_dependency_count);
		for (i = 0; i < (int)program_dependency_count - 1; i++) {
			prog_dep =
			ia_css_program_manifest_get_program_dependency(
				manifest, i);
			IA_CSS_TRACE_1(PSYSAPI_STATIC, INFO,
				"\t %4d,\n", prog_dep);
		}
		prog_dep =
		ia_css_program_manifest_get_program_dependency(manifest, i);
		IA_CSS_TRACE_1(PSYSAPI_STATIC, INFO, "\t %4d }\n", prog_dep);
		(void)prog_dep;
	}

	terminal_dependency_count =
		ia_css_program_manifest_get_terminal_dependency_count(manifest);
	if (terminal_dependency_count == 0) {
		IA_CSS_TRACE_1(PSYSAPI_STATIC, INFO,
			"terminal_dependencies[%d] {};\n",
			terminal_dependency_count);
	} else {
		uint8_t term_dep;

		IA_CSS_TRACE_1(PSYSAPI_STATIC, INFO,
			"terminal_dependencies[%d] {\n",
			terminal_dependency_count);
		for (i = 0; i < (int)terminal_dependency_count - 1; i++) {
			term_dep =
			ia_css_program_manifest_get_terminal_dependency(
				manifest, i);
			IA_CSS_TRACE_1(PSYSAPI_STATIC, INFO,
				"\t %4d,\n", term_dep);
		}
		term_dep =
		   ia_css_program_manifest_get_terminal_dependency(manifest, i);
		IA_CSS_TRACE_1(PSYSAPI_STATIC, INFO, "\t %4d }\n", term_dep);
		(void)term_dep;
	}
	(void)cell_type_id;

	retval = 0;
EXIT:
	if (retval != 0) {
		IA_CSS_TRACE_1(PSYSAPI_STATIC, ERROR,
			"ia_css_program_manifest_print failed (%i)\n", retval);
	}
	return retval;
}

#if defined(_MSC_VER)
/* WA for a visual studio compiler bug */
#pragma optimize( "", off )
#endif

#endif

