/*
* Support for Intel Camera Imaging ISP subsystem.
* Copyright (c) 2010 - 2017, Intel Corporation.
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

#ifndef __IA_CSS_PSYS_PROCESS_H
#define __IA_CSS_PSYS_PROCESS_H

/*! \file */

/** @file ia_css_psys_process.h
 *
 * Define the methods on the process object that are not part of
 * a single interface
 */

#include <ia_css_psys_process_types.h>
#include <ia_css_psys_dynamic_storage_class.h>

#include <vied_nci_psys_system_global.h>

#include <type_support.h>					/* uint8_t */

/*
 * Creation
 */
#include <ia_css_psys_process.hsys.user.h>

/*
 * Internal resources
 */
#include <ia_css_psys_process.hsys.kernel.h>

/*
 * Process manager
 */
#include <ia_css_psys_process.psys.h>

/*
 * Command processor
 */

/*! Execute a command locally or send it to be processed remotely

 @param	process[in]	process object
 @param	cmd[in]		command

 @return < 0 on invalid argument(s) or process state
 */
extern int ia_css_process_cmd(
	ia_css_process_t *process,
	const ia_css_process_cmd_t cmd);

/*! Get the internal memory offset of the process object

 @param	process[in]	process object
 @param	mem_id[in]	memory id

 @return internal memory offset,
	IA_CSS_PROCESS_INVALID_OFFSET on invalid argument(s)
*/
IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_H
vied_nci_resource_size_t ia_css_process_get_int_mem_offset(
	const ia_css_process_t *process,
	const vied_nci_mem_type_ID_t mem_id);


/*! Get the external memory offset of the process object

 @param	process[in]	process object
 @param	mem_id[in]	memory id

 @return external memory offset,
	IA_CSS_PROCESS_INVALID_OFFSET on invalid argument(s)
*/
IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_H
vied_nci_resource_size_t ia_css_process_get_ext_mem_offset(
	const ia_css_process_t *process,
	const vied_nci_mem_type_ID_t mem_type_id);


/*! Get the stored size of the process object

 @param	process[in]	process object

 @return size, 0 on invalid argument
 */
IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_H
size_t ia_css_process_get_size(const ia_css_process_t *process);

/*! Get the (pointer to) the process group parent of the process object

 @param	process[in]	process object

 @return the pointer to the parent, NULL on invalid argument
 */
IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_H
ia_css_process_group_t *ia_css_process_get_parent(
	const ia_css_process_t *process);

/*! Set the (pointer to) the process group parent of the process object

 @param	process[in]	process object
 @param	parent[in]	(pointer to the) process group parent object

 @return < 0 on invalid argument(s)
 */
extern int ia_css_process_set_parent(
	ia_css_process_t *process,
	ia_css_process_group_t *parent);

/*! Get the unique ID of program used by the process object

 @param	process[in]	process object

 @return ID, 0 on invalid argument
 */
IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_H
ia_css_program_ID_t ia_css_process_get_program_ID(
	const ia_css_process_t *process);

/*! Get the state of the process object

 @param	process[in]	process object

 @return state, limit value (IA_CSS_N_PROCESS_STATES) on invalid argument
 */
IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_H
ia_css_process_state_t ia_css_process_get_state(
	const ia_css_process_t *process);

/*! Set the state of the process object

 @param	process[in]	process object
 @param	state[in]	state of the process

 @return < 0 on invalid argument
 */
IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_H
int ia_css_process_set_state(
	ia_css_process_t *process,
	ia_css_process_state_t state);

/*! Get the assigned cell of the the process object

 @param	process[in]	process object

 @return cell ID, limit value (VIED_NCI_N_CELL_ID) on invalid argument
 */
IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_H
vied_nci_cell_ID_t ia_css_process_get_cell(
	const ia_css_process_t *process);

/*! Get the number of cells the process object depends on

 @param	process[in]	process object

 @return number of cells
 */
IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_H
uint8_t ia_css_process_get_cell_dependency_count(
	const ia_css_process_t *process);

/*! Get the number of terminals the process object depends on

 @param	process[in]	process object

 @return number of terminals
 */
IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_H
uint8_t ia_css_process_get_terminal_dependency_count(
	const ia_css_process_t *process);

/*! Set n-th cell dependency of a process object

 @param	process[in]	Process object
 @param	dep_index[in]	dep index
 @param	id[in]		dep id

 @return < 0 on invalid process argument
 */
extern int ia_css_process_set_cell_dependency(
	const ia_css_process_t *process,
	const unsigned int dep_index,
	const vied_nci_resource_id_t id);

/*! Get n-th cell dependency of a process object

 @param	process[in]	Process object
 @param	cell_num[in]	n-th cell

 @return n-th cell dependency,
	IA_CSS_PROCESS_INVALID_DEPENDENCY on invalid argument(s)
*/
IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_H
vied_nci_resource_id_t ia_css_process_get_cell_dependency(
	const ia_css_process_t *process,
	const unsigned int cell_num);

/*! Set n-th terminal dependency of a process object

 @param	process[in]	Process object
 @param	dep_index[in]	dep index
 @param	id[in]		dep id

 @return < 0 on on invalid argument(s)
 */
extern int ia_css_process_set_terminal_dependency(
	const ia_css_process_t *process,
	const unsigned int dep_index,
	const vied_nci_resource_id_t id);

/*! Get n-th terminal dependency of a process object

 @param	process[in]		Process object
 @param	terminal_num[in]	n-th cell

 @return n-th terminal dependency,
	IA_CSS_PROCESS_INVALID_DEPENDENCY on invalid argument(s)
*/
IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_H
uint8_t ia_css_process_get_terminal_dependency(
	const ia_css_process_t *process,
	const unsigned int terminal_num);

/*! Get the kernel bitmap of the the process object

 @param	process[in]	process object

 @return process kernel bitmap
 */
IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_H
ia_css_kernel_bitmap_t ia_css_process_get_kernel_bitmap(
	const ia_css_process_t *process);

/*! Get the device channel id-n resource allocation offset of the process object

 @param	process[in]	process object
 @param	dev_chn_id[in]	channel id

 @return resource offset, IA_CSS_PROCESS_INVALID_OFFSET on invalid argument(s)
 */
IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_H
vied_nci_resource_size_t ia_css_process_get_dev_chn(
	const ia_css_process_t *process,
	const vied_nci_dev_chn_ID_t dev_chn_id);

/*! Get the ext mem type-n resource id of the the process object

 @param	process[in]	process object
 @param	mem_type[in]	mem type

 @return resource offset, IA_CSS_PROCESS_INVALID_OFFSET on invalid argument(s)
 */
IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_H
vied_nci_mem_ID_t ia_css_process_get_ext_mem_id(
	const ia_css_process_t *process,
	const vied_nci_mem_type_ID_t mem_type);


/*! Sets the device channel id-n resource allocation offset of
 * the process object

 @param	process[in]	process object
 @param	dev_chn_id[in]	channel id
 @param offset[in]	resource offset

 @return < 0 on invalid argument(s) or process state
 */
int ia_css_process_set_dev_chn(
	ia_css_process_t *process,
	const vied_nci_dev_chn_ID_t dev_chn_id,
	const vied_nci_resource_size_t offset);

/*! Boolean test if the process object type is valid

 @param	process[in]	process object
 @param	p_manifest[in]	program manifest

 @return true if the process object is correct, false on error
 */
IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_H
bool ia_css_is_process_valid(
	const ia_css_process_t *process,
	const ia_css_program_manifest_t *p_manifest);

#ifdef __IA_CSS_PSYS_DYNAMIC_INLINE__
#include "ia_css_psys_process_impl.h"
#endif /* __IA_CSS_PSYS_DYNAMIC_INLINE__ */

#endif /* __IA_CSS_PSYS_PROCESS_H */
