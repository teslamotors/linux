/**
* INTEL CONFIDENTIAL
*
* Copyright (C) 2015 - 2015 Intel Corporation.
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

#ifndef _IA_CSS_CMEM_H_
#define _IA_CSS_CMEM_H_

#include "type_support.h"
#include "storage_class.h"

#ifdef __VIED_CELL
typedef unsigned int ia_css_cmem_address_t;
#else
#include <vied/vied_subsystem_access.h>
typedef vied_subsystem_address_t ia_css_cmem_address_t;
#endif

STORAGE_CLASS_INLINE uint32_t
ia_css_cmem_load_32(unsigned int ssid, ia_css_cmem_address_t address);

STORAGE_CLASS_INLINE void
ia_css_cmem_store_32(unsigned int ssid, ia_css_cmem_address_t address, uint32_t value);

STORAGE_CLASS_INLINE void
ia_css_cmem_load(unsigned int ssid, ia_css_cmem_address_t address, void *data, unsigned int size);

STORAGE_CLASS_INLINE void
ia_css_cmem_store(unsigned int ssid, ia_css_cmem_address_t address, const void *data, unsigned int size);

STORAGE_CLASS_INLINE void
ia_css_cmem_zero(unsigned int ssid, ia_css_cmem_address_t address, unsigned int size);

STORAGE_CLASS_INLINE ia_css_cmem_address_t
ia_css_cmem_get_cmem_addr_from_dmem(unsigned int base_addr, void *p);

/* Include inline implementation */

#ifdef __VIED_CELL
#include "ia_css_cmem_cell.h"
#else
#include "ia_css_cmem_host.h"
#endif

#endif
