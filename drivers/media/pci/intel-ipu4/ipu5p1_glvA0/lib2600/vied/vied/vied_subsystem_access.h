/*
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
#ifndef _HRT_VIED_SUBSYSTEM_ACCESS_H
#define _HRT_VIED_SUBSYSTEM_ACCESS_H

#include <type_support.h>
#include "vied_config.h"
#include "vied_subsystem_access_types.h"

#if !defined(CFG_VIED_SUBSYSTEM_ACCESS_INLINE_IMPL) && \
    !defined(CFG_VIED_SUBSYSTEM_ACCESS_LIB_IMPL)
#error Implementation selection macro for vied subsystem access not defined
#endif

#if defined(CFG_VIED_SUBSYSTEM_ACCESS_INLINE_IMPL)
#ifndef __HIVECC
#error "Inline implementation of subsystem access not supported for host"
#endif
#define _VIED_SUBSYSTEM_ACCESS_INLINE static __inline
#include "vied_subsystem_access_impl.h"
#else
#define _VIED_SUBSYSTEM_ACCESS_INLINE
#endif

_VIED_SUBSYSTEM_ACCESS_INLINE
void vied_subsystem_store_8 (vied_subsystem_t dev,
                             vied_subsystem_address_t addr, uint8_t  data);

_VIED_SUBSYSTEM_ACCESS_INLINE
void vied_subsystem_store_16(vied_subsystem_t dev,
                             vied_subsystem_address_t addr, uint16_t data);

_VIED_SUBSYSTEM_ACCESS_INLINE
void vied_subsystem_store_32(vied_subsystem_t dev,
                             vied_subsystem_address_t addr, uint32_t data);

_VIED_SUBSYSTEM_ACCESS_INLINE
void vied_subsystem_store(vied_subsystem_t dev,
                          vied_subsystem_address_t addr,
                          const void *data, unsigned int size);

_VIED_SUBSYSTEM_ACCESS_INLINE
uint8_t  vied_subsystem_load_8 (vied_subsystem_t dev,
                                vied_subsystem_address_t addr);

_VIED_SUBSYSTEM_ACCESS_INLINE
uint16_t vied_subsystem_load_16(vied_subsystem_t dev,
                                vied_subsystem_address_t addr);

_VIED_SUBSYSTEM_ACCESS_INLINE
uint32_t vied_subsystem_load_32(vied_subsystem_t dev,
                                vied_subsystem_address_t addr);

_VIED_SUBSYSTEM_ACCESS_INLINE
void vied_subsystem_load(vied_subsystem_t dev,
                         vied_subsystem_address_t addr,
                         void *data, unsigned int size);

#endif /* _HRT_VIED_SUBSYSTEM_ACCESS_H */
