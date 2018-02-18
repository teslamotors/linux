/*
 * aram_managerc
 *
 * ARAM manager
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

#include "aram_manager.h"

static void *aram_handle;

static LIST_HEAD(aram_alloc_list);
static LIST_HEAD(aram_free_list);

void aram_print(void)
{
	mem_print(aram_handle);
}
EXPORT_SYMBOL(aram_print);

void *aram_request(const char *name, size_t size)
{
	return mem_request(aram_handle, name, size);
}
EXPORT_SYMBOL(aram_request);

bool aram_release(void *handle)
{
	return mem_release(aram_handle, handle);
}
EXPORT_SYMBOL(aram_release);

unsigned long aram_get_address(void *handle)
{
	return mem_get_address(handle);
}
EXPORT_SYMBOL(aram_get_address);

#ifdef CONFIG_DEBUG_FS
static struct dentry *aram_dump_debugfs_file;

static int aram_dump(struct seq_file *s, void *data)
{
	mem_dump(aram_handle, s);
	return 0;
}

static int aram_dump_open(struct inode *inode, struct file *file)
{
	return single_open(file, aram_dump, inode->i_private);
}

static const struct file_operations aram_dump_fops = {
	.open		= aram_dump_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};
#endif

int aram_init(unsigned long addr, unsigned long size)
{
	aram_handle = create_mem_manager("ARAM", addr, size);
	if (IS_ERR(aram_handle)) {
		pr_err("ERROR: failed to create aram memory_manager");
		return PTR_ERR(aram_handle);
	}

#ifdef CONFIG_DEBUG_FS
	aram_dump_debugfs_file = debugfs_create_file("aram_dump",
		S_IRUSR, NULL, NULL, &aram_dump_fops);
	if (!aram_dump_debugfs_file) {
		pr_err("ERROR: failed to create aram_dump debugfs");
		destroy_mem_manager(aram_handle);
		return -ENOMEM;
	}
#endif
	return 0;
}
EXPORT_SYMBOL(aram_init);

void aram_exit(void)
{
#ifdef CONFIG_DEBUG_FS
	debugfs_remove(aram_dump_debugfs_file);
#endif
	destroy_mem_manager(aram_handle);
}
EXPORT_SYMBOL(aram_exit);

