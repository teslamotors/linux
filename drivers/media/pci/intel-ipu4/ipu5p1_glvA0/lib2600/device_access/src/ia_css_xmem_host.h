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

#ifndef _IA_CSS_XMEM_HOST_H_
#define _IA_CSS_XMEM_HOST_H_

#include "ia_css_xmem.h"
#include <vied/shared_memory_access.h>
#include "assert_support.h"
#include <type_support.h>

STORAGE_CLASS_INLINE uint8_t
ia_css_xmem_load_8(unsigned int mmid, ia_css_xmem_address_t address)
{
	return shared_memory_load_8(mmid, address);
}

STORAGE_CLASS_INLINE uint16_t
ia_css_xmem_load_16(unsigned int mmid, ia_css_xmem_address_t address)
{
	/* Address has to be half-word aligned */
	assert(0 == (uintptr_t) address % 2);
	return shared_memory_load_16(mmid, address);
}

STORAGE_CLASS_INLINE uint32_t
ia_css_xmem_load_32(unsigned int mmid, ia_css_xmem_address_t address)
{
	/* Address has to be word aligned */
	assert(0 == (uintptr_t) address % 4);
	return shared_memory_load_32(mmid, address);
}

STORAGE_CLASS_INLINE void
ia_css_xmem_load(unsigned int mmid, ia_css_xmem_address_t address, void *data, unsigned int size)
{
	shared_memory_load(mmid, address, data, size);
}

STORAGE_CLASS_INLINE void
ia_css_xmem_store_8(unsigned int mmid, ia_css_xmem_address_t address, uint8_t value)
{
	shared_memory_store_8(mmid, address, value);
}

STORAGE_CLASS_INLINE void
ia_css_xmem_store_16(unsigned int mmid, ia_css_xmem_address_t address, uint16_t value)
{
	/* Address has to be half-word aligned */
	assert(0 == (uintptr_t) address % 2);
	shared_memory_store_16(mmid, address, value);
}

STORAGE_CLASS_INLINE void
ia_css_xmem_store_32(unsigned int mmid, ia_css_xmem_address_t address, uint32_t value)
{
	/* Address has to be word aligned */
	assert(0 == (uintptr_t) address % 4);
	shared_memory_store_32(mmid, address, value);
}

STORAGE_CLASS_INLINE void
ia_css_xmem_store(unsigned int mmid, ia_css_xmem_address_t address, const void *data, unsigned int bytes)
{
	shared_memory_store(mmid, address, data, bytes);
}

#endif /* _IA_CSS_XMEM_HOST_H_ */
