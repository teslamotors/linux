/*
 * Header file for aram manager
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

#ifndef __TEGRA_NVADSP_ARAM_MANAGER_H
#define __TEGRA_NVADSP_ARAM_MANAGER_H

#include "mem_manager.h"

int aram_init(unsigned long addr, unsigned long size);
void aram_exit(void);

void *aram_request(const char *name, size_t size);
bool aram_release(void *handle);

unsigned long aram_get_address(void *handle);
void aram_print(void);

#endif /* __TEGRA_NVADSP_ARAM_MANAGER_H */
