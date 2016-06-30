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

/* provided interface */
#include "ia_css_buffer.h"

/* used interfaces */
#include "vied/shared_memory_access.h"
#include "vied/shared_memory_map.h"
#include "cpu_mem_support.h"

ia_css_buffer_t
ia_css_buffer_alloc(vied_subsystem_t sid, vied_memory_t mid, unsigned int size)
{
	ia_css_buffer_t b;

	b = ia_css_cpu_mem_alloc(sizeof(*b));
	if (b == NULL) {
		return NULL;
	}

	b->mem = shared_memory_alloc(mid, size);

	if (b->mem == 0) {
		ia_css_cpu_mem_free(b);
		return NULL;
	}

	b->css_address = shared_memory_map(sid, mid, b->mem);
	b->size	= size;
	return b;
}


void
ia_css_buffer_free(vied_subsystem_t sid, vied_memory_t mid, ia_css_buffer_t b)
{
	shared_memory_unmap(sid, mid, b->css_address);
	shared_memory_free(mid, b->mem);
	ia_css_cpu_mem_free(b);
}

