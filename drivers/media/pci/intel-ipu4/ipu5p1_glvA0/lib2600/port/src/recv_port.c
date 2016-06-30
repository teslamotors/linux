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

/* implemented: */
#include "recv_port.h"

/* used: */
#include "queue.h"
#include "queue_struct.h"
#include "recv_port_struct.h"
#include "port_env_struct.h"
#include "regmem_access.h"
#include "buffer_access.h"
#include "storage_class.h"
#include "math_support.h"
#ifndef __VIED_CELL
#include "cpu_mem_support.h"
#endif

void
recv_port_open(struct recv_port *p, const struct sys_queue *q, const struct port_env *env)
{
	p->mmid = env->mmid;
	p->ssid = env->ssid;
	p->mem_addr = env->mem_addr;

	p->size   = q->size;
	p->token_size = q->token_size;
	p->wr_reg = q->wr_reg;
	p->rd_reg = q->rd_reg;

#ifdef __VIED_CELL
	p->buffer = q->vied_address;
#else
	p->buffer = q->host_address;
#endif
}

STORAGE_CLASS_INLINE unsigned int
recv_port_index(const struct recv_port *p, unsigned int i)
{
	unsigned int rd = regmem_load_32(p->mem_addr, p->rd_reg, p->ssid);

	return OP_std_modadd(rd, i, p->size);
}

unsigned int
recv_port_available(const struct recv_port *p)
{
	int wr = (int)regmem_load_32(p->mem_addr, p->wr_reg, p->ssid);
	int rd = (int)regmem_load_32(p->mem_addr, p->rd_reg, p->ssid);

	return OP_std_modadd(wr, -rd, p->size);
}

STORAGE_CLASS_INLINE void
recv_port_copy(const struct recv_port *p, unsigned int i, void *data)
{
	unsigned int rd   = recv_port_index(p, i);
	unsigned int token_size = p->token_size;
	buffer_address addr = p->buffer + (rd * token_size);
#ifndef __VIED_CELL
	ia_css_cpu_mem_cache_invalidate((void *)HOST_ADDRESS(p->buffer), token_size*p->size);
#endif
	buffer_load(addr, data, token_size, p->mmid);
}

STORAGE_CLASS_INLINE void
recv_port_release(const struct recv_port *p, unsigned int i)
{
	unsigned int rd = recv_port_index(p, i);

	regmem_store_32(p->mem_addr, p->rd_reg, rd, p->ssid);
}

unsigned int
recv_port_transfer(const struct recv_port *p, void *data)
{
	if (!recv_port_available(p))
		return 0;
	recv_port_copy(p, 0, data);
	recv_port_release(p, 1);
	return 1;
}
