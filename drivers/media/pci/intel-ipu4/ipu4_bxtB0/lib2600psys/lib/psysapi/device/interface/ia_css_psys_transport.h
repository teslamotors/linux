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

#ifndef __IA_CSS_PSYS_TRANSPORT_H_INCLUDED__
#define __IA_CSS_PSYS_TRANSPORT_H_INCLUDED__

#include <vied_nci_psys_system_global.h>	/* vied_vaddress_t */

#include <type_support.h>

/*
 * The ID's of the Psys specific queues.
 */
typedef enum ia_css_psys_cmd_queues {
	IA_CSS_PSYS_CMD_QUEUE_COMMAND_ID = 0,		/**< The in-order queue for scheduled proces groups */
	IA_CSS_PSYS_CMD_QUEUE_DEVICE_ID,		/**< The in-order queue for comamnds changing psys or process group state */
	IA_CSS_N_PSYS_CMD_QUEUE_ID
} ia_css_psys_cmd_queue_ID_t;

typedef enum ia_css_psys_event_queues {
	IA_CSS_PSYS_EVENT_QUEUE_MAIN_ID,			/**< The in-order queue for event returns */
	IA_CSS_N_PSYS_EVENT_QUEUE_ID
} ia_css_psys_event_queue_ID_t;

typedef enum ia_css_psys_event_types {
	IA_CSS_PSYS_EVENT_TYPE_CMD_COMPLETE,			/**< Command processed succussfully */
	IA_CSS_PSYS_EVENT_TYPE_FRAGMENT_COMPLETE,		/**< Fragment processed succussfully */
	IA_CSS_PSYS_EVENT_TYPE_ERROR				/**< Error */
} ia_css_psys_event_type_t;


#define IA_CSS_PSYS_CMD_BITS					64
struct ia_css_psys_cmd_s {
	uint16_t			command;				/**< The command issued to the process group */
	uint16_t			msg;					/**< Message field of the command */
	uint32_t			process_group;			/**< The process group reference */
};

#define IA_CSS_PSYS_EVENT_BITS					128
struct ia_css_psys_event_s {
	uint16_t			status;					/**< The (return) status of the command issued to the process group this event refers to */
	uint16_t			command;				/**< The command issued to the process group this event refers to */
	uint32_t			process_group;			/**< The process group reference */
	uint64_t			token;					/**< This token (size) must match the token registered in a process group */
};

struct ia_css_psys_buffer_s {
	void				*host_buffer;			/**< The in-order queue for scheduled proces groups */
	vied_vaddress_t		*isp_buffer;
};

#endif
