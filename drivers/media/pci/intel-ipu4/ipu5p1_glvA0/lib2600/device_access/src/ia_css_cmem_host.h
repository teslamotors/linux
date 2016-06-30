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

#ifndef _IA_CSS_CMEM_HOST_H_
#define _IA_CSS_CMEM_HOST_H_

#include "ia_css_cmem.h"

#include <vied/vied_subsystem_access.h>
#include "assert_support.h"
#include "type_support.h"
#include "misc_support.h"

STORAGE_CLASS_INLINE uint32_t
ia_css_cmem_load_32(unsigned int ssid, ia_css_cmem_address_t address)
{
	/* Address has to be word aligned */
	assert(0 == address % 4);
	return vied_subsystem_load_32(ssid, address);
}

STORAGE_CLASS_INLINE uint32_t
ia_css_cond_cmem_load_32(bool cond, unsigned int ssid, ia_css_cmem_address_t address)
{
	/* Address has to be word aligned */
	assert(0 == address % 4);
	if (cond) {
		return vied_subsystem_load_32(ssid, address);
	} else {
		return 0;
	}
}

STORAGE_CLASS_INLINE void
ia_css_cmem_store_32(unsigned int ssid, ia_css_cmem_address_t address, uint32_t data)
{
	/* Address has to be word aligned */
	assert(0 == address % 4);
	vied_subsystem_store_32(ssid, address, data);
}

STORAGE_CLASS_INLINE void
ia_css_cond_cmem_store_32(bool cond, unsigned int ssid, ia_css_cmem_address_t address, uint32_t data)
{
	/* Address has to be word aligned */
	assert(0 == address % 4);
	if (cond)
		vied_subsystem_store_32(ssid, address, data);
}

STORAGE_CLASS_INLINE void
ia_css_cmem_load(unsigned int ssid, ia_css_cmem_address_t address, void *data, unsigned int size)
{
	vied_subsystem_load(ssid, address, data, size);
}

STORAGE_CLASS_INLINE void
ia_css_cmem_store(unsigned int ssid, ia_css_cmem_address_t address, const void *data, unsigned int size)
{
	uint32_t *data32 = (uint32_t *)data;
	uint32_t end = address + size;

	assert(size % 4 == 0);
	assert(address % 4 == 0);
	assert((long)data % 4 == 0);

	while (address != end) {
		ia_css_cmem_store_32(ssid, address, *data32);
		address += 4;
		data32 += 1;
	}
}

STORAGE_CLASS_INLINE void
ia_css_cmem_zero(unsigned int ssid, ia_css_cmem_address_t address, unsigned int size)
{
	uint32_t end = address + size;

	assert(size % 4 == 0);
	assert(address % 4 == 0);

	while (address != end) {
		ia_css_cmem_store_32(ssid, address, 0);
		address += 4;
	}
}

STORAGE_CLASS_INLINE ia_css_cmem_address_t
ia_css_cmem_get_cmem_addr_from_dmem(unsigned int base_addr, void *p)
{
	NOT_USED(base_addr);
	return (ia_css_cmem_address_t)(uintptr_t)p;
}

#endif
