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

#ifndef __IA_CSS_PSYS_TRANSPORT_DEP_H
#define __IA_CSS_PSYS_TRANSPORT_DEP_H

/*
 * The ID's of the Psys specific queues.
 */
typedef enum ia_css_psys_cmd_queues {
	/**< The in-order queue for scheduled process groups */
	IA_CSS_PSYS_CMD_QUEUE_COMMAND_ID = 0,
	/**< The in-order queue for commands changing psys or
	 * process group state
	 */
	IA_CSS_PSYS_CMD_QUEUE_DEVICE_ID,
	/**< An in-order queue for dedicated PPG commands */
	IA_CSS_PSYS_CMD_QUEUE_PPG0_COMMAND_ID,
	/**< An in-order queue for dedicated PPG commands */
	IA_CSS_PSYS_CMD_QUEUE_PPG1_COMMAND_ID,
	IA_CSS_N_PSYS_CMD_QUEUE_ID
} ia_css_psys_cmd_queue_ID_t;

#endif /* __IA_CSS_PSYS_TRANSPORT_DEP_H */
