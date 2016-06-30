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
