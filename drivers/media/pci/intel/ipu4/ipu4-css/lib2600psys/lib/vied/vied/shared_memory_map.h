/*
* Support for Intel Camera Imaging ISP subsystem.
 * Copyright (c) 2010 - 2018, Intel Corporation.
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
#ifndef _SHARED_MEMORY_MAP_H
#define _SHARED_MEMORY_MAP_H

#include <vied/vied_subsystem_access_types.h>
#include <vied/vied_memory_access_types.h>
#include <vied/shared_memory_access.h>

typedef void (*shared_memory_invalidate_mmu_tlb)(void);
typedef void (*shared_memory_set_page_table_base_address)(vied_physical_address_t);

typedef void (*shared_memory_invalidate_mmu_tlb_ssid)(vied_subsystem_t id);
typedef void (*shared_memory_set_page_table_base_address_ssid)(vied_subsystem_t id, vied_physical_address_t);

/**
 * \brief Initialize the CSS virtual address system and MMU. The subsystem id will NOT be taken into account.
*/
int shared_memory_map_initialize(vied_subsystem_t id, vied_memory_t idm, size_t mmu_ps, size_t mmu_pnrs, vied_physical_address_t ddr_addr, shared_memory_invalidate_mmu_tlb inv_tlb, shared_memory_set_page_table_base_address sbt);

/**
 * \brief Initialize the CSS virtual address system and MMU. The subsystem id will be taken into account.
*/
int shared_memory_map_initialize_ssid(vied_subsystem_t id, vied_memory_t idm, size_t mmu_ps, size_t mmu_pnrs, vied_physical_address_t ddr_addr, shared_memory_invalidate_mmu_tlb_ssid inv_tlb, shared_memory_set_page_table_base_address_ssid sbt);

/**
 * \brief De-initialize the CSS virtual address system and MMU.
*/
void shared_memory_map_uninitialize(vied_subsystem_t id, vied_memory_t idm);

/**
 * \brief Convert a host virtual address to a CSS virtual address and update the MMU.
*/
vied_virtual_address_t shared_memory_map(vied_subsystem_t id, vied_memory_t idm, host_virtual_address_t addr);

/**
 * \brief Free a CSS virtual address and update the MMU.
*/
void shared_memory_unmap(vied_subsystem_t id, vied_memory_t idm, vied_virtual_address_t addr);


#endif /* _SHARED_MEMORY_MAP_H */
