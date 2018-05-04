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

#ifndef __IA_CSS_PSYS_PROGRAM_MANIFEST_HSYS_KERNEL_H
#define __IA_CSS_PSYS_PROGRAM_MANIFEST_HSYS_KERNEL_H

/*! \file */

/** @file ia_css_psys_program_manifest.hsys.kernel.h
 *
 * Define the methods on the program manifest object: Hsys kernel interface
 */

#include <ia_css_psys_manifest_types.h>

#include <vied_nci_psys_system_global.h>

#include <type_support.h>					/* uint8_t */

/*
 * Resources needs
 */

/*! Get the cell ID from the program manifest object

 @param	manifest[in]			program manifest object

 Note: If the cell ID is specified, the program this manifest belongs to
 must be mapped on that instance. If the cell ID is invalid (limit value)
 then the cell type ID must be specified instead

 @return cell ID, limit value if not specified
 */
extern vied_nci_cell_ID_t ia_css_program_manifest_get_cell_ID(
	const ia_css_program_manifest_t			*manifest);

/*! Get the cell type ID from the program manifest object

 @param	manifest[in]			program manifest object

 Note: If the cell type ID is specified, the program this manifest belongs
 to can be mapped on any instance of this clee type. If the cell type ID is
 invalid (limit value) then a specific cell ID must be specified instead

 @return cell ID, limit value if not specified
 */
extern vied_nci_cell_type_ID_t ia_css_program_manifest_get_cell_type_ID(
	const ia_css_program_manifest_t			*manifest);

/*! Get the memory resource (size) specification for a memory
 that belongs to the cell where the program will be mapped

 @param	manifest[in]			program manifest object
 @param	mem_type_id[in]			mem type ID

 @return 0 when not applicable
 */
extern vied_nci_resource_size_t ia_css_program_manifest_get_int_mem_size(
	const ia_css_program_manifest_t			*manifest,
	const vied_nci_mem_type_ID_t			mem_type_id);

/*! Get the memory resource (size) specification for a memory
 that does not belong to the cell where the program will be mapped

 @param	manifest[in]			program manifest object
 @param	mem_type_id[in]			mem type ID

 @return 0 when not applicable
 */
extern vied_nci_resource_size_t ia_css_program_manifest_get_ext_mem_size(
	const ia_css_program_manifest_t			*manifest,
	const vied_nci_mem_type_ID_t			mem_type_id);

/*! Get a device channel resource (size) specification

 @param	manifest[in]			program manifest object
 @param	dev_chn_id[in]			device channel ID

 @return 0 when not applicable
 */
extern vied_nci_resource_size_t ia_css_program_manifest_get_dev_chn_size(
	const ia_css_program_manifest_t			*manifest,
	const vied_nci_dev_chn_ID_t				dev_chn_id);

#endif /* __IA_CSS_PSYS_PROGRAM_MANIFEST_HSYS_KERNEL_H */
