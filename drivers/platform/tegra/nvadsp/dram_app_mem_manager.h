/*
 * Header file for dram app memory manager
 *
 * Copyright (c) 2014-2015, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __TEGRA_NVADSP_DRAM_APP_MEM_MANAGER_H
#define __TEGRA_NVADSP_DRAM_APP_MEM_MANAGER_H

#include "mem_manager.h"

int dram_app_mem_init(unsigned long, unsigned long);
void dram_app_mem_exit(void);

void *dram_app_mem_request(const char *name, size_t size);
bool dram_app_mem_release(void *handle);

unsigned long dram_app_mem_get_address(void *handle);
void dram_app_mem_print(void);

#endif /* __TEGRA_NVADSP_DRAM_APP_MEM_MANAGER_H */
