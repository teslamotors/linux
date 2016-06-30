/**
* INTEL CONFIDENTIAL
*
* Copyright (C) 2014 - 2016 Intel Corporation.
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

#ifndef __SYS_QUEUE_STRUCT_H__
#define __SYS_QUEUE_STRUCT_H__

/* queue description, shared between sender and receiver */

#include "type_support.h"

#ifdef __VIED_CELL
typedef struct {uint32_t v[2]; } host_buffer_address_t;
#else
typedef uint64_t		host_buffer_address_t;
#endif

typedef uint32_t		vied_buffer_address_t;


struct sys_queue {
	host_buffer_address_t host_address;
	vied_buffer_address_t vied_address;
	unsigned int size;
	unsigned int token_size;
	unsigned int wr_reg; /* reg no in subsystem's regmem */
	unsigned int rd_reg;
	unsigned int _align;
};

struct sys_queue_res {
	host_buffer_address_t host_address;
	vied_buffer_address_t vied_address;
	unsigned int reg;
};

#endif /*__QUEUE_STRUCT_H__*/
