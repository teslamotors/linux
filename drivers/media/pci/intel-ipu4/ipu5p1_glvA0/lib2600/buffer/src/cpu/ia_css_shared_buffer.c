/**
* INTEL CONFIDENTIAL
*
* Copyright (C) 2013 - 2015 Intel Corporation.
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


#include "ia_css_buffer.h"
#include "vied/shared_memory_access.h"
#include "vied/shared_memory_map.h"
#include "cpu_mem_support.h"
#include "assert_support.h"

#include "ia_css_shared_buffer_cpu.h"


ia_css_shared_buffer
ia_css_shared_buffer_alloc(vied_subsystem_t sid, vied_memory_t mid, unsigned int size)
{
	ia_css_shared_buffer b;

	/* allocate buffer container */
	b = ia_css_cpu_mem_alloc(sizeof(*b));
	if (b == NULL) {
		return NULL;
	}

	b->mem = shared_memory_alloc(mid, size);
	if (b->mem == 0) {
		ia_css_cpu_mem_free(b);
		return NULL;
	}

#ifndef HRT_HW
	/* initialize the buffer to avoid warnings when copying */
	shared_memory_zero(mid, b->mem, size);

	/* in simulation, we need to allocate a shadow host buffer */
	b->cpu_address = ia_css_cpu_mem_alloc_page_aligned(size);
	if (b->cpu_address == NULL) {
		shared_memory_free(mid, b->mem);
		ia_css_cpu_mem_free(b);
		return NULL;
	}
#else
	/* on hw / real platform we can use the pointer from shared memory alloc */
	b->cpu_address = (void *)HOST_ADDRESS(b->mem);
#endif

	b->css_address = shared_memory_map(sid, mid, b->mem);

	b->size	= size;
	b->state = buffer_unmapped;

	return b;
}


void
ia_css_shared_buffer_free(vied_subsystem_t sid, vied_memory_t mid, ia_css_shared_buffer b)
{
	assert(b->state == buffer_unmapped);

#ifndef HRT_HW
	/* only free if we actually allocated it separately */
	ia_css_cpu_mem_free(b->cpu_address);
#endif
	shared_memory_unmap(sid, mid, b->css_address);
	shared_memory_free(mid, b->mem);
	ia_css_cpu_mem_free(b);
}


ia_css_shared_buffer_cpu_address
ia_css_shared_buffer_cpu_map(ia_css_shared_buffer b)
{
	if (b == NULL)
		return NULL;

	/* map shared buffer to CPU address space */

	assert(b->state == buffer_unmapped);
	b->state = buffer_cpu;

	return b->cpu_address;
}


void
ia_css_shared_buffer_cpu_unmap(ia_css_shared_buffer b)
{
	if (b == NULL)
		return;

	/* unmap shared buffer from CPU address space */

	assert(b->state == buffer_cpu);
	b->state = buffer_unmapped;
}


ia_css_shared_buffer_css_address
ia_css_shared_buffer_css_map(ia_css_shared_buffer b)
{
	if (b == NULL)
		return 0;

	/* map shared buffer to CSS address space */

	assert(b->state == buffer_unmapped);
	b->state = buffer_css;

	return (ia_css_shared_buffer_css_address)b->css_address;
}


void
ia_css_shared_buffer_css_unmap(ia_css_shared_buffer b)
{
	if (b == NULL)
		return;

	/* unmap shared buffer from CSS address space */

	assert(b->state == buffer_css);
	b->state = buffer_unmapped;
}


void
ia_css_shared_buffer_css_update(vied_memory_t mid, ia_css_shared_buffer b)
{
	/* flush the buffer to CSS after it was modified by the CPU */

#ifndef HRT_HW
	/* copy data from CPU address space to CSS address space */
	shared_memory_store(mid, b->mem, b->cpu_address, b->size);
#else
	(void)mid;
#endif
	/* flush cache to ddr */
	ia_css_cpu_mem_cache_flush(b->cpu_address, b->size);
}


void
ia_css_shared_buffer_cpu_update(vied_memory_t mid, ia_css_shared_buffer b)
{
	if (b == NULL)
		return;

	/* flush the buffer to the CPU after it has been modified by CSS */

#ifndef HRT_HW
	/* copy data from CSS address space to CPU address space */
	shared_memory_load(mid, b->mem, b->cpu_address, b->size);
#else
	(void)mid;
#endif
	/* flush cache to ddr */
	ia_css_cpu_mem_cache_invalidate(b->cpu_address, b->size);
}

