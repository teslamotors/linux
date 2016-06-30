/**
* INTEL CONFIDENTIAL
*
* Copyright (C) 2013 - 2016 Intel Corporation.
* All Rights Reserved.
*
* The source code contained or described herein and all documents
* related to the source code ("Material") are owned by Intel Corporation
* or licensors. Title to the Material remains with Intel
* Corporation or its licensors. The Material contains trade
* secrets and proprietary and confidential information of Intel or its
* licensors. The Material is protected by worldwide copyright
* and trade secret laws and treaty provisions. No part of the Material may
* be used, copied, reproduced, modified, published, uploaded, posted,
* transmitted, distributed, or disclosed in any way without Intel's prior
* express written permission.
*
* No License under any patent, copyright, trade secret or other intellectual
* property right is granted to or conferred upon you by disclosure or
* delivery of the Materials, either expressly, by implication, inducement,
* estoppel or otherwise. Any license under such intellectual property rights
* must be express and approved by Intel in writing.
*/

#ifndef __IA_CSS_SYSCOM_CONTEXT_H__
#define __IA_CSS_SYSCOM_CONTEXT_H__

#include <vied/shared_memory_access.h>

#include "port_env_struct.h"

/* host context */
struct ia_css_syscom_context {
	vied_virtual_address_t	cell_firmware_addr;
	unsigned int		cell_regs_addr;
	unsigned int		cell_dmem_addr;

	struct port_env env;

	unsigned int num_input_queues;
	unsigned int num_output_queues;

	struct sys_queue *input_queue;  /* array of input queues (from host to SP) */
	struct sys_queue *output_queue; /* array of output queues (from SP to host) */

	struct send_port *send_port;
	struct recv_port *recv_port;

	unsigned int regmem_idx;
	unsigned int free_buf;

	host_virtual_address_t config_host_addr;
	host_virtual_address_t input_queue_host_addr;
	host_virtual_address_t output_queue_host_addr;
	host_virtual_address_t specific_host_addr;
	host_virtual_address_t ibuf_host_addr;
	host_virtual_address_t obuf_host_addr;

	vied_virtual_address_t config_vied_addr;
	vied_virtual_address_t input_queue_vied_addr;
	vied_virtual_address_t output_queue_vied_addr;
	vied_virtual_address_t specific_vied_addr;
	vied_virtual_address_t ibuf_vied_addr;
	vied_virtual_address_t obuf_vied_addr;
};

#endif /*__IA_CSS_SYSCOM_CONTEXT_H__*/
