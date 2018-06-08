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

#ifndef VIED_NCI_ACB_ROUTE_TYPE_H_
#define VIED_NCI_ACB_ROUTE_TYPE_H_

#include "type_support.h"

typedef enum {
	NCI_ACB_PORT_ISP = 0,
	NCI_ACB_PORT_ACC = 1,
	NCI_ACB_PORT_INVALID = 0xFF
} nci_acb_port_t;

typedef struct {
	/* 0 = ISP, 1 = Acc */
	nci_acb_port_t in_select;
	/* 0 = ISP, 1 = Acc */
	nci_acb_port_t out_select;
	/* When set, Ack will be sent only when Eof arrives */
	uint32_t ignore_line_num;
	/* Fork adapter to enable streaming to both output
	 * (next acb out and isp out)
	 */
	uint32_t fork_acb_output;
} nci_acb_route_t;

#endif /* VIED_NCI_ACB_ROUTE_TYPE_H_ */
