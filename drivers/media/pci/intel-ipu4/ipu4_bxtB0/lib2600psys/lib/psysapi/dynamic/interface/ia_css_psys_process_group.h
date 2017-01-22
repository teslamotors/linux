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

#ifndef __IA_CSS_PSYS_PROCESS_GROUP_H
#define __IA_CSS_PSYS_PROCESS_GROUP_H

/*! \file */

/** @file ia_css_psys_process_group.h
 *
 * Define the methods on the process object that are not part of
 * a single interface
 */

#include <ia_css_psys_process_types.h>
#include <ia_css_psys_dynamic_storage_class.h>

#include <type_support.h>					/* uint8_t */

/*
 * Creation
 */
#include <ia_css_psys_process_group.hsys.user.h>

/*
 * Registration of user contexts / callback info
 * External resources
 * Sequencing resources
 */
#include <ia_css_psys_process_group.hsys.kernel.h>

/*
 * Dispatcher
 */
#include <ia_css_psys_process_group.psys.h>

/*
 * Access to sub-structure handles / fields
 */

#include "ia_css_terminal.h"

/*! Get the number of fragments on the process group

 @param	process_group[in]		process group object

 Note: Future change is to have a fragment count per
 independent subgraph

 @return the fragment count, 0 on error
 */
IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_H
uint16_t ia_css_process_group_get_fragment_count(
	const ia_css_process_group_t		*process_group);


/*! Get the fragment state on the process group

 @param	 process_group[in]		process group object
 @param	 fragment_state[in]		current fragment of processing

 @return -1 on error
 */
IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_H
int ia_css_process_group_get_fragment_state(
	const ia_css_process_group_t		*process_group,
	uint16_t				*fragment_state);

/*! Set the fragment state on the process group

 @param	process_group[in]		process group object
 @param	fragment_state[in]		current fragment of processing

 @return -1 on error
  */
IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_H
int ia_css_process_group_set_fragment_state(
	ia_css_process_group_t			*process_group,
	uint16_t				fragment_state);

/*! Get the number of processes on the process group

 @param	process_group[in]		process group object

 @return the process count, 0 on error
 */
IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_H
uint8_t ia_css_process_group_get_process_count(
	const ia_css_process_group_t		*process_group);

/*! Get the number of terminals on the process group

 @param	process_group[in]		process group object

 Note: Future change is to have a terminal count per
 independent subgraph

 @return the terminal count, 0 on error
 */
IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_H
uint8_t ia_css_process_group_get_terminal_count(
	const ia_css_process_group_t		*process_group);

/*! Get the PG load start timestamp

 @param	process_group[in]		process group object

 @return PG load start timestamp, 0 on error
 */
IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_H
uint32_t ia_css_process_group_get_pg_load_start_ts(
	const ia_css_process_group_t			*process_group);

/*! Get the PG load time in cycles

 @param	process_group[in]		process group object

 @return PG load time in cycles, 0 on error
 */
IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_H
uint32_t ia_css_process_group_get_pg_load_cycles(
	const ia_css_process_group_t			*process_group);

/*! Get the PG init time in cycles

 @param	process_group[in]		process group object

 @return PG init time in cycles, 0 on error
 */
IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_H
uint32_t ia_css_process_group_get_pg_init_cycles(
	const ia_css_process_group_t			*process_group);

/*! Get the PG processing time in cycles

 @param	process_group[in]		process group object

 @return PG processing time in cycles, 0 on error
 */
IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_H
uint32_t ia_css_process_group_get_pg_processing_cycles(
	const ia_css_process_group_t			*process_group);


/*! Get the (pointer to) the indexed terminal of the process group object

 @param	process_group[in]		process group object
 @param	terminal_index[in]		index of the terminal

 @return the pointer to the terminal, NULL on error
 */
IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_H
ia_css_terminal_t *ia_css_process_group_get_terminal(
	const ia_css_process_group_t		*process_group,
	const unsigned int			terminal_index);

/*! Get the (pointer to) the indexed process of the process group object

 @param	process_group[in]		process group object
 @param	process_index[in]		index of the process

 @return the pointer to the process, NULL on error
 */
IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_H
ia_css_process_t *ia_css_process_group_get_process(
	const ia_css_process_group_t		*process_group,
	const unsigned int			process_index);

/*! Get the stored size of the process group object

 @param	process_group[in]				process group object

 @return size, 0 on error
 */
IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_H
size_t ia_css_process_group_get_size(
	const ia_css_process_group_t		*process_group);

/*! Get the state of the the process group object

 @param	process_group[in]		process group object

 @return state, limit value on error
 */
IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_H
ia_css_process_group_state_t ia_css_process_group_get_state(
	const ia_css_process_group_t		*process_group);

/*! Get the unique ID of program group used by the process group object

 @param	process_group[in]		process group object

 @return ID, 0 on error
 */
IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_H
ia_css_program_group_ID_t ia_css_process_group_get_program_group_ID(
	const ia_css_process_group_t		*process_group);

/*! Get the resource bitmap of the process group

 @param	process_group[in]		process group object

 @return the reource bitmap
 */
IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_H
vied_nci_resource_bitmap_t ia_css_process_group_get_resource_bitmap(
	const ia_css_process_group_t		*process_group);

/*! Set the resource bitmap of the process group

 @param	process_group[in]		process group object
 @param	resource_bitmap[in]		the resource bitmap

 @return < 0 on error
 */
IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_H
int ia_css_process_group_set_resource_bitmap(
	ia_css_process_group_t			*process_group,
	const vied_nci_resource_bitmap_t	resource_bitmap);

/*! Get IPU virtual address of process group

 @param	 process_group[in]		process group object
 @param	 ipu_vaddress[in/out]	process group ipu virtual address

 @return -1 on error
 */
IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_H
int ia_css_process_group_get_ipu_vaddress(
	const ia_css_process_group_t		*process_group,
	vied_vaddress_t			*ipu_vaddress);

/*! Set IPU virtual address of process group

 @param	process_group[in]		process group object
 @param	ipu_vaddress[in]		process group ipu address

 @return -1 on error
 */
IA_CSS_PSYS_DYNAMIC_STORAGE_CLASS_H
int ia_css_process_group_set_ipu_vaddress(
	ia_css_process_group_t			*process_group,
	vied_vaddress_t			ipu_vaddress);

#ifdef __IA_CSS_PSYS_DYNAMIC_INLINE__
#include "ia_css_psys_process_group_impl.h"
#endif /* __IA_CSS_PSYS_DYNAMIC_INLINE__ */

#endif /* __IA_CSS_PSYS_PROCESS_GROUP_H */
