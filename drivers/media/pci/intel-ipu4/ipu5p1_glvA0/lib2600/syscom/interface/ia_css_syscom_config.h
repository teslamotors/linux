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

#ifndef __IA_CSS_SYSCOM_CONFIG_H__
#define __IA_CSS_SYSCOM_CONFIG_H__

#include <vied/shared_memory_access.h>

/* syscom size struct, output of ia_css_syscom_size, input for (external) allocation */
struct ia_css_syscom_size {
	unsigned int cpu;  /* Size of host buffer */
	unsigned int shm;  /* Size of shared config buffer        (host to cell) */
	unsigned int ibuf; /* Size of shared input queue buffers  (host to cell) */
	unsigned int obuf; /* Size of shared output queue buffers (cell to host) */
};

/* syscom buffer struct, output of (external) allocation, input for ia_css_syscom_open */
struct ia_css_syscom_buf {
	char *cpu; /* host buffer */

	host_virtual_address_t shm_host; /* shared memory buffer host address */
	vied_virtual_address_t shm_cell; /* shared memory buffer cell address */

	host_virtual_address_t ibuf_host; /* input queue shared buffer host address */
	vied_virtual_address_t ibuf_cell; /* input queue shared buffer cell address */

	host_virtual_address_t obuf_host; /* output queue shared buffer host address */
	vied_virtual_address_t obuf_cell; /* output queue shared buffer cell address */
};

struct ia_css_syscom_queue_config {
	unsigned int queue_size; /* tokens per queue */
	unsigned int token_size; /* bytes per token */
};

/**
  * Parameter struct for ia_css_syscom_open
  */
struct ia_css_syscom_config {
	/* This member in no longer used in syscom.
	   It is kept to not break any driver builds, and will be removed when
	   all assignments have been removed from driver code */
	/* address of firmware in DDR/IMR */
	unsigned long long host_firmware_address;

	/* address of firmware in DDR, seen from SPC */
	unsigned int vied_firmware_address;

	unsigned int ssid;
	unsigned int mmid;

	unsigned int num_input_queues;
	unsigned int num_output_queues;
	struct ia_css_syscom_queue_config *input;
	struct ia_css_syscom_queue_config *output;

	unsigned int regs_addr;
	unsigned int dmem_addr;

	/* firmware-specific configuration data */
	void *specific_addr;
	unsigned int specific_size;
};

#endif /*__IA_CSS_SYSCOM_CONFIG_H__*/

