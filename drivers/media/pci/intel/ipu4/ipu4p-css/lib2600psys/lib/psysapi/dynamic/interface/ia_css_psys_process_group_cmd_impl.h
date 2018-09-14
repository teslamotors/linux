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

#ifndef __IA_CSS_PSYS_PROCESS_GROUP_CMD_IMPL_H
#define __IA_CSS_PSYS_PROCESS_GROUP_CMD_IMPL_H

#include "type_support.h"
#include "ia_css_psys_process_group.h"
#include "ia_css_rbm_manifest_types.h"

#define N_UINT64_IN_PROCESS_GROUP_STRUCT	2
#define N_UINT32_IN_PROCESS_GROUP_STRUCT	5
#define N_UINT16_IN_PROCESS_GROUP_STRUCT	5
#define N_UINT8_IN_PROCESS_GROUP_STRUCT		7
#define N_PADDING_UINT8_IN_PROCESS_GROUP_STRUCT	3

#define SIZE_OF_PROCESS_GROUP_STRUCT_BITS \
	(IA_CSS_RBM_BITS \
	+ N_UINT64_IN_PROCESS_GROUP_STRUCT * IA_CSS_UINT64_T_BITS \
	+ N_UINT32_IN_PROCESS_GROUP_STRUCT * IA_CSS_UINT32_T_BITS \
	+ IA_CSS_PROGRAM_GROUP_ID_BITS \
	+ IA_CSS_PROCESS_GROUP_STATE_BITS \
	+ VIED_VADDRESS_BITS \
	+ VIED_NCI_RESOURCE_BITMAP_BITS \
	+ N_UINT16_IN_PROCESS_GROUP_STRUCT * IA_CSS_UINT16_T_BITS \
	+ N_UINT8_IN_PROCESS_GROUP_STRUCT * IA_CSS_UINT8_T_BITS \
	+ N_PADDING_UINT8_IN_PROCESS_GROUP_STRUCT * IA_CSS_UINT8_T_BITS)

struct ia_css_process_group_s {
	/**< User (callback) token / user context reference,
	 * zero is an error value
	 */
	uint64_t token;
	/**< private token / context reference, zero is an error value */
	uint64_t private_token;
	/**< PG routing bitmap used to set connection between programs >*/
	ia_css_rbm_t routing_bitmap;
	/**< Size of this structure */
	uint32_t size;
	/**< The timestamp when PG load starts */
	uint32_t pg_load_start_ts;
	/**< PG load time in cycles */
	uint32_t pg_load_cycles;
	/**< PG init time in cycles */
	uint32_t pg_init_cycles;
	/**< PG processing time in cycles */
	uint32_t pg_processing_cycles;
	/**< Referral ID to program group FW */
	ia_css_program_group_ID_t ID;
	/**< State of the process group FSM */
	ia_css_process_group_state_t state;
	/**< Virtual address of process group in IPU */
	vied_vaddress_t ipu_virtual_address;
	/**< Bitmap of the compute resources used by the process group  */
	vied_nci_resource_bitmap_t resource_bitmap;
	/**< Number of fragments offered on each terminal */
	uint16_t fragment_count;
	/**< Current fragment of processing */
	uint16_t fragment_state;
	/**< Watermark to control fragment processing */
	uint16_t fragment_limit;
	/**< Array[process_count] of process addresses in this process group */
	uint16_t processes_offset;
	/**< Array[terminal_count] of terminal addresses on this process group */
	uint16_t terminals_offset;
	/**< Parameter dependent number of processes in this process group */
	uint8_t process_count;
	/**< Parameter dependent number of terminals on this process group */
	uint8_t terminal_count;
	/**< Parameter dependent number of independent subgraphs in
	 * this process group
	 */
	uint8_t subgraph_count;
	/**< Process group protocol version */
	uint8_t protocol_version;
	/**< Dedicated base queue id used for enqueueing payload buffer sets */
	uint8_t base_queue_id;
	/**< Number of dedicated queues used */
	uint8_t num_queues;
	/**< Mask the send_pg_done IRQ */
	uint8_t mask_irq;
	/**< Padding for 64bit alignment */
	uint8_t padding[N_PADDING_UINT8_IN_PROCESS_GROUP_STRUCT];
};

/*! Callback after process group is created. Implementations can provide
 * suitable actions needed when process group is created.

 @param	process_group[in]			process group object
 @param	program_group_manifest[in]		program group manifest
 @param	program_group_param[in]			program group parameters

 @return 0 on success and non-zero on failure
 */
extern int ia_css_process_group_on_create(
	ia_css_process_group_t			*process_group,
	const ia_css_program_group_manifest_t	*program_group_manifest,
	const ia_css_program_group_param_t	*program_group_param);

/*! Callback before process group is about to be destoyed. Any implementation
 * specific cleanups can be done here.

 @param	process_group[in]				process group object

 @return 0 on success and non-zero on failure
 */
extern int ia_css_process_group_on_destroy(
	ia_css_process_group_t					*process_group);

/*
 * Command processor
 */

/*! Execute a command locally or send it to be processed remotely

 @param	process_group[in]		process group object
 @param	cmd[in]					command

 @return < 0 on error
 */
extern int ia_css_process_group_exec_cmd(
	ia_css_process_group_t				*process_group,
	const ia_css_process_group_cmd_t		cmd);


/*! Enqueue a buffer set corresponding to a persistent program group by
 * sending a command to subsystem.

 @param	process_group[in]		process group object
 @param	buffer_set[in]			buffer set
 @param	queue_offset[in]		offset to be used from the queue id
					specified in the process group object
					(0 for first buffer set for frame, 1
					for late binding)

 @return < 0 on error
 */
extern int ia_css_enqueue_buffer_set(
	ia_css_process_group_t				*process_group,
	ia_css_buffer_set_t				*buffer_set,
	unsigned int					queue_offset);

/*! Enqueue a parameter buffer set corresponding to a persistent program
 *  group by sending a command to subsystem.

 @param	process_group[in]		process group object
 @param	buffer_set[in]			parameter buffer set

 @return < 0 on error
 */
extern int ia_css_enqueue_param_buffer_set(
	ia_css_process_group_t				*process_group,
	ia_css_buffer_set_t				*buffer_set);

/*! Need to store the 'secure' mode for each PG for FW test app only
 *
 * @param	process_group[in]		process group object
 * @param	secure[in]			parameter buffer set
 *
 * @return < 0 on error
 */
extern int ia_css_process_group_store(
	ia_css_process_group_t				*process_group,
	bool						secure);


#endif /* __IA_CSS_PSYS_PROCESS_GROUP_CMD_IMPL_H */
