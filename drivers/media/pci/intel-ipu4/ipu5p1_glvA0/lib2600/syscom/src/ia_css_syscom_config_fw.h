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

#ifndef __IA_CSS_SYSCOM_CONFIG_FW_H__
#define __IA_CSS_SYSCOM_CONFIG_FW_H__

#include "type_support.h"

enum {
	SYSCOM_STATE_UNINIT	= 0x57A7E000,	/* Program load or explicit host setting should init to this */
	SYSCOM_STATE_READY	= 0x57A7E001,	/* SP Syscom sets this when it is ready for use */
	SYSCOM_STATE_INACTIVE	= 0x57A7E002	/* SP Syscom sets this when no more syscom accesses will happen */
};

enum {
	SYSCOM_COMMAND_UNINIT	= 0x57A7F000,	/* Program load or explicit host setting should init to this */
	SYSCOM_COMMAND_INACTIVE = 0x57A7F001	/* Host Syscom requests syscom to become inactive */
};

/* firmware config: data that sent from the host to SP via DDR */
/* Cell copies data into a context */

struct ia_css_syscom_config_fw {
	unsigned int firmware_address;

	unsigned int num_input_queues;
	unsigned int num_output_queues;
	unsigned int input_queue; /* hmm_ptr / struct queue* */
	unsigned int output_queue; /* hmm_ptr / struct queue* */

	unsigned int specific_addr; /* vied virtual address */
	unsigned int specific_size;
};

#endif /*__IA_CSS_SYSCOM_CONFIG_FW_H__*/
