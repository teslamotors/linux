/**
* INTEL CONFIDENTIAL
*
* Copyright (C) 2015 - 2016 Intel Corporation.
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

#ifndef _IA_CSS_XMEM_CMEM_IMPL_H_
#define _IA_CSS_XMEM_CMEM_IMPL_H_

#include "ia_css_xmem_cmem.h"

#include "ia_css_cmem.h"
#include "ia_css_xmem.h"

/* Copy data from xmem to cmem, e.g., from a program in DDR to a cell's DMEM */
/* This may also be implemented using DMA */

STORAGE_CLASS_INLINE void
ia_css_xmem_to_cmem_copy(
	unsigned int mmid,
	unsigned int ssid,
	ia_css_xmem_address_t src,
	ia_css_cmem_address_t dst,
	unsigned int size)
{
	/* copy from ddr to subsystem, e.g., cell dmem */
	ia_css_cmem_address_t end = dst + size;

	assert(size % 4 == 0);
	assert((uintptr_t) dst % 4 == 0);
	assert((uintptr_t) src % 4 == 0);

	while (dst != end) {
		uint32_t data;

		data = ia_css_xmem_load_32(mmid, src);
		ia_css_cmem_store_32(ssid, dst, data);
		dst += 4;
		src += 4;
	}
}

/* Copy data from cmem to xmem */

STORAGE_CLASS_INLINE void
ia_css_cmem_to_xmem_copy(
	unsigned int mmid,
	unsigned int ssid,
	ia_css_cmem_address_t src,
	ia_css_xmem_address_t dst,
	unsigned int size)
{
	/* copy from ddr to subsystem, e.g., cell dmem */
	ia_css_xmem_address_t end = dst + size;

	assert(size % 4 == 0);
	assert((uintptr_t) dst % 4 == 0);
	assert((uintptr_t) src % 4 == 0);

	while (dst != end) {
		uint32_t data;

		data = ia_css_cmem_load_32(mmid, src);
		ia_css_xmem_store_32(ssid, dst, data);
		dst += 4;
		src += 4;
	}
}


#endif /* _IA_CSS_XMEM_CMEM_IMPLH_ */
