/*
* Support for Intel Camera Imaging ISP subsystem.
* Copyright (c) 2010 - 2015, Intel Corporation.
* 
* This program is free software; you can redistribute it and/or modify it
* under the terms and conditions of the GNU General Public License,
* version 2, as published by the Free Software Foundation.
* 
* This program is distributed in the hope it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
* more details.
*/
#ifndef _VIED_DEVICE_ACCESS_H
#define _VIED_DEVICE_ACCESS_H

#include <type_support.h>
#include "vied_types.h"
#include "vied_subsystem_access_types.h"
#include "vied_device_access_types.h"

#if !defined(CFG_VIED_DEVICE_ACCESS_INLINE_IMPL) && \
    !defined(CFG_VIED_DEVICE_ACCESS_LIB_IMPL)
#error Implementation selection macro not defined
#endif

#if defined(CFG_VIED_DEVICE_ACCESS_INLINE_IMPL)
#define _VIED_DEVICE_ACCESS_INLINE static inline
#include "vied_device_access_impl.h"
#else
#define _VIED_DEVICE_ACCESS_INLINE
#endif

/* Device access handle */
_VIED_DEVICE_ACCESS_INLINE
const vied_device_t *vied_device_open(vied_subsystem_t tgt_ss, vied_device_id_t tgt_dev);

_VIED_DEVICE_ACCESS_INLINE
void vied_device_close(const vied_device_t *tgt_dev);

/* Device addressing */
_VIED_DEVICE_ACCESS_INLINE
const vied_device_route_t *vied_device_get_route(const vied_device_t *dev, vied_device_port_id_t  port);

/* Device register access */
_VIED_DEVICE_ACCESS_INLINE
void vied_device_reg_store_32(const vied_device_route_t *route,
                              vied_device_reg_id_t reg, uint32_t data);

_VIED_DEVICE_ACCESS_INLINE
uint32_t vied_device_reg_load_32(const vied_device_route_t *route, vied_device_reg_id_t reg);
/* Device register access */
_VIED_DEVICE_ACCESS_INLINE
void vied_device_reg_store_32_b(const vied_device_route_t *route,
                              vied_device_regbank_id_t bank, int bank_index,
                              vied_device_reg_id_t reg, uint32_t data);

_VIED_DEVICE_ACCESS_INLINE
uint32_t vied_device_reg_load_32_b(const vied_device_route_t *route,
                                 vied_device_regbank_id_t bank, int bank_index,
                                 vied_device_reg_id_t reg);

/* Device memory access */
_VIED_DEVICE_ACCESS_INLINE
void vied_device_mem_store_8(const vied_device_route_t *route,
                             vied_device_memory_id_t mem, vied_address_t mem_addr,
                             uint8_t data);

_VIED_DEVICE_ACCESS_INLINE
void vied_device_mem_store_16(const vied_device_route_t *route,
                              vied_device_memory_id_t mem, vied_address_t mem_addr,
                              uint16_t data);

_VIED_DEVICE_ACCESS_INLINE
void vied_device_mem_store_32(const vied_device_route_t *route,
                              vied_device_memory_id_t mem, vied_address_t mem_addr,
                              uint32_t data);

_VIED_DEVICE_ACCESS_INLINE
void vied_device_mem_store(const vied_device_route_t *route,
                           vied_device_memory_id_t mem, vied_address_t mem_addr,
                           const void *data, unsigned int size);

_VIED_DEVICE_ACCESS_INLINE
uint8_t vied_device_mem_load_8(const vied_device_route_t *route,
                               vied_device_memory_id_t mem, vied_address_t mem_addr);

_VIED_DEVICE_ACCESS_INLINE
uint16_t vied_device_mem_load_16(const vied_device_route_t *route,
                                 vied_device_memory_id_t mem, vied_address_t mem_addr);


_VIED_DEVICE_ACCESS_INLINE
uint32_t vied_device_mem_load_32(const vied_device_route_t *route,
                                 vied_device_memory_id_t mem, vied_address_t mem_addr);

_VIED_DEVICE_ACCESS_INLINE
void vied_device_mem_load(const vied_device_route_t *route,
                          vied_device_memory_id_t mem, vied_address_t mem_addr,
                          void *data, unsigned int size);

#if defined(CFG_VIED_DEVICE_ACCESS_INLINE_IMPL)
#undef _VIED_DEVICE_ACCESS_INLINE
#endif

#endif /* _VIED_DEVICE_ACCESS_H */
