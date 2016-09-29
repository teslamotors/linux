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

#ifndef __VIED_NCI_ISA_ACB_TYPES_H__
#define __VIED_NCI_ISA_ACB_TYPES_H__

#include "type_support.h"
#include "storage_class.h"

#define IPU_DEVICE_ACB_GA_ACB_CMD_FIFO_TAIL_ADDR (0x00000)
#define IPU_DEVICE_ACB_GA_ACB_BASE_CTRL_ADDR (0x00004)
#define IPU_DEVICE_ACB_GA_ACB_INPUT_FRAME_SIZE_ADDR (0x00008)
#define IPU_DEVICE_ACB_GA_ACB_SCALE_ADDR (0x0000c)

#define IPU_DEVICE_ACB_ACK_CMD_ADDR (0x0000)
#define IPU_DEVICE_ACB_ACK_ADDR_ADDR (0x0004)

#define IPU_DEVICE_ACB_INIT_CMD_ID (0x0010)
#define IPU_DEVICE_ACB_PROC_N_LINES_CMD_ID (0x0001)

typedef enum {
	VIED_NCI_ACB_PORT_ISP = 0,
	VIED_NCI_ACB_PORT_ACC = 1,
	VIED_NCI_ACB_PORT_INVALID = 0xFF
} vied_nci_acb_port_t;

typedef struct {
	uint32_t ctrl_id;
} vied_nci_acb_init_cmd_t;

typedef __register struct {
	uint32_t ctrl_id;
	uint32_t config_set;
	uint32_t num_lines;
} vied_nci_acb_process_cmd_t;

typedef struct {
	/* 0 = ISP, 1 = Acc */
	vied_nci_acb_port_t in_select;
	/* 0 = ISP, 1 = Acc */
	vied_nci_acb_port_t out_select;
	/* When set, Ack will be sent only when Eof arrives */
	uint32_t ignore_line_num;
	/* Fork adapter to enable streaming to both output
	 * (next acb out and isp out)
	 */
	uint32_t fork_acb_output;
} vied_nci_acb_route_t;

typedef __register struct {
	/* ACB_FRAME_SIZE */
	/* For the FRAGMENT to process, just in ISYS frame == fragment */
	uint32_t frame_width;
	/* For the FRAGMENT to process, just in ISYS frame == fragment */
	uint32_t frame_height;
	/* ACB_ACB_SCALE */
	uint32_t scale_mult;
	uint32_t scale_nf;
} vied_nci_acb_control_config_t;

typedef __register struct {
	vied_nci_acb_control_config_t ctrl;
	uint32_t event_queue_address;
	uint32_t ack_pid;
	uint32_t ack_sid;
	uint32_t ack_msg;
} vied_nci_acb_config_t;


#endif /*__VIED_NCI_ISA_ACB_TYPES_H__*/
