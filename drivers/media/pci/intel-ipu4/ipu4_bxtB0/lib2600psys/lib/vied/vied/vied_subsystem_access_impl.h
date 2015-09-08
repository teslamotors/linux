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
#ifndef VIED_SUBSYSTEM_ACCESS_IMPL_H
#define VIED_SUBSYSTEM_ACCESS_IMPL_H

/**
 * Legacy code
 * HRT subsystem access for cell
 * Will be replaced by DAI
 */

#include <hive/attributes.h>

#include "vied_subsystem_access.h"

#ifndef NOT_USED
#define NOT_USED(a) ((void)(a))
#endif

#ifdef HRT_MT_INT
#define VIED_SUBSYSTEM_MEMORY_PORT HRT_MT_INT
#else
#endif

#ifdef VIED_SUBSYSTEM_MEMORY_PORT
#define _VIED_SS_MEM MEM(VIED_SUBSYSTEM_MEMORY_PORT)
#else
#define _VIED_SS_MEM
#endif


typedef uint8_t   _VIED_SS_MEM *uint8_t_ptr;
typedef uint16_t  _VIED_SS_MEM *uint16_t_ptr;
typedef uint32_t  _VIED_SS_MEM *uint32_t_ptr;


#define store_8(address,data)  (*(uint8_t_ptr)(address ) = (data))
#define store_16(address,data) (*(uint16_t_ptr)(address) = (data))
#define store_32(address,data) (*(uint32_t_ptr)(address) = (data))
#define load_8(address)        (*(uint8_t_ptr)(address ))
#define load_16(address)       (*(uint16_t_ptr)(address))
#define load_32(address)       (*(uint32_t_ptr)(address))

#define vied_subsystem_port_width 4

_VIED_SUBSYSTEM_ACCESS_INLINE
void
vied_subsystem_store_8 (vied_subsystem_t dev, vied_subsystem_address_t addr, uint8_t  data)
{
  NOT_USED(dev);
  store_8 (addr, data);
}

_VIED_SUBSYSTEM_ACCESS_INLINE
void
vied_subsystem_store_16(vied_subsystem_t dev, vied_subsystem_address_t addr, uint16_t data)
{
  NOT_USED(dev);
  store_16(addr, data);
}

_VIED_SUBSYSTEM_ACCESS_INLINE
void
vied_subsystem_store_32(vied_subsystem_t dev, vied_subsystem_address_t addr, uint32_t data)
{
  NOT_USED(dev);
  store_32(addr, data);
}

_VIED_SUBSYSTEM_ACCESS_INLINE
void
vied_subsystem_store(vied_subsystem_t dev, vied_subsystem_address_t addr, const void *data, unsigned int size)
{
  uint8_t  *data_b;
  uint32_t *data_w;

  /* store first unaligned bytes */
  data_b = (uint8_t*)data;
  while (size && (addr & (vied_subsystem_port_width-1))) {
    vied_subsystem_store_8 (dev, addr, *data_b);
    addr++;
    data_b++;
    size--;
  }

  /* store full words */
  data_w = (uint32_t*)data_b;
  while (size/vied_subsystem_port_width) {
    vied_subsystem_store_32(dev, addr, *data_w);
    addr    += vied_subsystem_port_width;
    data_w++;
    size    -= vied_subsystem_port_width;
  }

  /* store last bytes, less than 'vied_subsystem_port_width' bytes */
  data_b = (uint8_t*)data_w;
  while (size) {
    vied_subsystem_store_8 (dev, addr, *data_b);
    addr++;
    data_b++;
    size--;
  }
}

_VIED_SUBSYSTEM_ACCESS_INLINE
uint8_t
vied_subsystem_load_8 (vied_subsystem_t dev, vied_subsystem_address_t addr)
{
  NOT_USED(dev);
  return load_8 (addr);
}

_VIED_SUBSYSTEM_ACCESS_INLINE
uint16_t
vied_subsystem_load_16(vied_subsystem_t dev, vied_subsystem_address_t addr)
{
  NOT_USED(dev);
  return load_16 (addr);
}

_VIED_SUBSYSTEM_ACCESS_INLINE
uint32_t
vied_subsystem_load_32(vied_subsystem_t dev, vied_subsystem_address_t addr)
{
  NOT_USED(dev);
  return load_32 (addr);
}

_VIED_SUBSYSTEM_ACCESS_INLINE
void
vied_subsystem_load(vied_subsystem_t dev, vied_subsystem_address_t addr, void *data, unsigned int size)
{
  uint8_t  *data_b;
  uint32_t *data_w;

  /* load first unaligned bytes */
  data_b = (uint8_t*)data;
  while (size && (addr & (vied_subsystem_port_width-1)))  {
    *data_b = vied_subsystem_load_8(dev, addr);
    addr++;
    data_b++;
    size--;
  }

  /* load full words */
  data_w = (uint32_t*)data_b;
  while (size/vied_subsystem_port_width) {
    *data_w = vied_subsystem_load_32(dev, addr);
    addr  += vied_subsystem_port_width;
    data_w++;
    size  -= vied_subsystem_port_width;
  }

  /* load last bytes, less than '_hrt_master_port_width' bytes */
  data_b = (uint8_t*)data_w;
  while (size){
    *data_b = vied_subsystem_load_8(dev, addr);
    addr++;
    data_b++;
    size--;
  }
}

#endif /* VIED_SUBSYSTEM_ACCESS_IMPL_H */
