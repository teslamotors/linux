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

#ifndef __BUFFER_H__
#define __BUFFER_H__

#include "type_support.h" /* workaround: needed because <vied/shared_memory_map.h> uses size_t */
#include "vied/shared_memory_map.h"

typedef enum {
	buffer_unmapped,	/* buffer is not accessible by cpu, nor css */
	buffer_write,		/* output buffer: css has write access */
				/* input  buffer: cpu has write access */
	buffer_read,		/* input  buffer: css has read access */
				/* output buffer: cpu has read access */
	buffer_cpu,		/* shared buffer: cpu has read and write access */
	buffer_css		/* shared buffer: css has read and write access */
} buffer_state;

struct ia_css_buffer_s {
	unsigned int		size;		/* number of bytes bytes allocated */
	host_virtual_address_t	mem;		/* allocated virtual memory object */
	vied_virtual_address_t	css_address;	/* virtual address to be used on css/firmware */
	void *cpu_address;	/* virtual address to be used on cpu/host */
	buffer_state		state;
};

typedef struct ia_css_buffer_s *ia_css_buffer_t;

ia_css_buffer_t	ia_css_buffer_alloc(vied_subsystem_t sid, vied_memory_t mid, unsigned int size);
void		ia_css_buffer_free(vied_subsystem_t sid, vied_memory_t mid, ia_css_buffer_t b);

#endif /*__BUFFER_H__*/
