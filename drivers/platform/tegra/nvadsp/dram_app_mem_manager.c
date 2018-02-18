/*
 * dram_app_mem_manager.c
 *
 * dram app memory manager for allocating memory for text,bss and data
 *
 * Copyright (C) 2014-2016, NVIDIA Corporation. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define pr_fmt(fmt) "%s : %d, " fmt, __func__, __LINE__

#include <linux/debugfs.h>
#include <linux/kernel.h>

#include "dram_app_mem_manager.h"

#define  ALIGN_TO_ADSP_PAGE(x)	ALIGN(x, 4096)

static void *dram_app_mem_handle;

static LIST_HEAD(dram_app_mem_alloc_list);
static LIST_HEAD(dram_app_mem_free_list);

void dram_app_mem_print(void)
{
	mem_print(dram_app_mem_handle);
}
EXPORT_SYMBOL(dram_app_mem_print);

void *dram_app_mem_request(const char *name, size_t size)
{
	return mem_request(dram_app_mem_handle, name, ALIGN_TO_ADSP_PAGE(size));
}
EXPORT_SYMBOL(dram_app_mem_request);

bool dram_app_mem_release(void *handle)
{
	return mem_release(dram_app_mem_handle, handle);
}
EXPORT_SYMBOL(dram_app_mem_release);

unsigned long dram_app_mem_get_address(void *handle)
{
	return mem_get_address(handle);
}
EXPORT_SYMBOL(dram_app_mem_get_address);

#ifdef CONFIG_DEBUG_FS
static struct dentry *dram_app_mem_dump_debugfs_file;

static int dram_app_mem_dump(struct seq_file *s, void *data)
{
	mem_dump(dram_app_mem_handle, s);
	return 0;
}

static int dram_app_mem_dump_open(struct inode *inode, struct file *file)
{
	return single_open(file, dram_app_mem_dump, inode->i_private);
}

static const struct file_operations dram_app_mem_dump_fops = {
	.open		= dram_app_mem_dump_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};
#endif

int dram_app_mem_init(unsigned long start, unsigned long size)
{
	dram_app_mem_handle =
		create_mem_manager("DRAM_APP_MANAGER", start, size);
	if (IS_ERR(dram_app_mem_handle)) {
		pr_err("ERROR: failed to create aram memory_manager");
		return PTR_ERR(dram_app_mem_handle);
	}

#ifdef CONFIG_DEBUG_FS
	dram_app_mem_dump_debugfs_file =
		debugfs_create_file("dram_app_mem_dump",
				S_IRUSR, NULL, NULL, &dram_app_mem_dump_fops);
	if (!dram_app_mem_dump_debugfs_file) {
		pr_err("ERROR: failed to create dram_app_mem_dump debugfs");
		destroy_mem_manager(dram_app_mem_handle);
		return -ENOMEM;
	}
#endif
	return 0;
}
EXPORT_SYMBOL(dram_app_mem_init);

void dram_app_mem_exit(void)
{
#ifdef CONFIG_DEBUG_FS
	debugfs_remove(dram_app_mem_dump_debugfs_file);
#endif
	destroy_mem_manager(dram_app_mem_handle);
}
EXPORT_SYMBOL(dram_app_mem_exit);

