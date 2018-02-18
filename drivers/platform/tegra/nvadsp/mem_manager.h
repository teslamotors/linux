/*
 * Header file for memory manager
 *
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __TEGRA_NVADSP_MEM_MANAGER_H
#define __TEGRA_NVADSP_MEM_MANAGER_H

#include <linux/sizes.h>

#define NAME_SIZE SZ_16

struct mem_chunk {
	struct list_head node;
	char name[NAME_SIZE];
	unsigned long address;
	unsigned long size;
};

struct mem_manager_info {
	struct list_head *alloc_list;
	struct list_head *free_list;
	char name[NAME_SIZE];
	unsigned long start_address;
	unsigned long size;
	spinlock_t lock;
};

void *create_mem_manager(const char *name, unsigned long start_address,
	unsigned long size);
void destroy_mem_manager(void *mem_handle);

void *mem_request(void *mem_handle, const char *name, size_t size);
bool mem_release(void *mem_handle, void *handle);

unsigned long mem_get_address(void *handle);

void mem_print(void *mem_handle);
void mem_dump(void *mem_handle, struct seq_file *s);

#endif /* __TEGRA_NVADSP_MEM_MANAGER_H */
