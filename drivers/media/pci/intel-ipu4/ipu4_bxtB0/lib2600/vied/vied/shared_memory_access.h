/*
* Support for Intel Camera Imaging ISP subsystem.
 * Copyright (c) 2010 - 2016, Intel Corporation.
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
#ifndef _SHARED_MEMORY_ACCESS_H
#define _SHARED_MEMORY_ACCESS_H

#include <type_support.h>
#include <type_support.h>
#include <vied/vied_memory_access_types.h>

typedef enum {
  sm_esuccess,
  sm_enomem,
  sm_ezeroalloc,
  sm_ebadvaddr,
  sm_einternalerror,
  sm_ecorruption,
  sm_enocontiguousmem,
  sm_enolocmem,
  sm_emultiplefree,
} shared_memory_error;

/**
 * \brief Virtual address of (DDR) shared memory space as seen from the VIED subsystem
 */
#if  defined(C_RUN) || defined(crun_hostlib)
typedef char* vied_virtual_address_t;
#else
typedef uint32_t vied_virtual_address_t;
#endif

/**
 * \brief Virtual address of (DDR) shared memory space as seen from the host
 */
#if  defined(C_RUN) || defined(crun_hostlib)
typedef char* host_virtual_address_t;
#else
typedef unsigned long long host_virtual_address_t;
#endif

/**
 * \brief List of physical addresses of (DDR) shared memory space. This is used to represent a list of physical pages.
 */
typedef struct shared_memory_physical_page_list_s *shared_memory_physical_page_list;
typedef struct shared_memory_physical_page_list_s
{
  shared_memory_physical_page_list next;
  vied_physical_address_t           address;
}shared_memory_physical_page_list_s;


/**
 * \brief Initialize the shared memory interface administration on the host.
 * \param idm: id of ddr memory
 * \param host_ddr_addr: physical address of memory as seen from host
 * \param memory_size: size of ddr memory in bytes
 * \param ps: size of page in bytes (for instance 4096)
 */
int shared_memory_allocation_initialize(vied_memory_t idm, vied_physical_address_t host_ddr_addr, size_t memory_size, size_t ps);

/**
 * \brief De-initialize the shared memory interface administration on the host.
 *
 */
void shared_memory_allocation_uninitialize(vied_memory_t idm);

/**
 * \brief Allocate (DDR) shared memory space and return a host virtual address. Returns NULL when insufficient memory available
 */
host_virtual_address_t shared_memory_alloc(vied_memory_t idm, size_t bytes);

/**
 * \brief Free (DDR) shared memory space.
*/
void shared_memory_free(vied_memory_t idm, host_virtual_address_t addr);

/**
 * \brief Translate a virtual host.address to a physical address.
*/
vied_physical_address_t shared_memory_virtual_host_to_physical_address (vied_memory_t idm, host_virtual_address_t addr);

/**
 * \brief Return the allocated physical pages for a virtual host.address.
*/
shared_memory_physical_page_list shared_memory_virtual_host_to_physical_pages (vied_memory_t idm, host_virtual_address_t addr);

/**
 * \brief Destroy a shared_memory_physical_page_list.
*/
void shared_memory_physical_pages_list_destroy (shared_memory_physical_page_list ppl);

/**
 * \brief Store a byte into (DDR) shared memory space using a host virtual address
 */
void shared_memory_store_8 (vied_memory_t idm, host_virtual_address_t addr, uint8_t  data);

/**
 * \brief Store a 16-bit word into (DDR) shared memory space using a host virtual address
 */
void shared_memory_store_16(vied_memory_t idm, host_virtual_address_t addr, uint16_t data);

/**
 * \brief Store a 32-bit word into (DDR) shared memory space using a host virtual address
 */
void shared_memory_store_32(vied_memory_t idm, host_virtual_address_t addr, uint32_t data);

/**
 * \brief Store a number of bytes into (DDR) shared memory space using a host virtual address
 */
void shared_memory_store(vied_memory_t idm, host_virtual_address_t addr, const void *data, size_t bytes);

/**
 * \brief Set a number of bytes of (DDR) shared memory space to 0 using a host virtual address
 */
void shared_memory_zero(vied_memory_t idm, host_virtual_address_t addr, size_t bytes);

/**
 * \brief Load a byte from (DDR) shared memory space using a host virtual address
 */
uint8_t shared_memory_load_8 (vied_memory_t idm, host_virtual_address_t addr);

/**
 * \brief Load a 16-bit word from (DDR) shared memory space using a host virtual address
 */
uint16_t shared_memory_load_16(vied_memory_t idm, host_virtual_address_t addr);

/**
 * \brief Load a 32-bit word from (DDR) shared memory space using a host virtual address
 */
uint32_t shared_memory_load_32(vied_memory_t idm, host_virtual_address_t addr);

/**
 * \brief Load a number of bytes from (DDR) shared memory space using a host virtual address
 */
void shared_memory_load(vied_memory_t idm, host_virtual_address_t addr, void *data, size_t bytes);

#endif /* _SHARED_MEMORY_ACCESS_H */
