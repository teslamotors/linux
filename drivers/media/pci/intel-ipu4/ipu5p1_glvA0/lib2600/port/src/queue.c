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

#include "queue.h"

#include "regmem_access.h"
#include "port_env_struct.h"

unsigned int sys_queue_buf_size(unsigned int size, unsigned int token_size)
{
	return (size + 1) * token_size;
}

void
sys_queue_init(struct sys_queue *q, unsigned int size, unsigned int token_size, struct sys_queue_res *res)
{
	unsigned int buf_size;

	q->size         = size + 1;
	q->token_size   = token_size;
	buf_size = sys_queue_buf_size(size, token_size);

	/* acquire the shared buffer space */
	q->host_address = res->host_address;
	res->host_address += buf_size;
	q->vied_address	= res->vied_address;
	res->vied_address += buf_size;

	/* acquire the shared read and writer pointers */
	q->wr_reg = res->reg;
	res->reg++;
	q->rd_reg = res->reg;
	res->reg++;

}
