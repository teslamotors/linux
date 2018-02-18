/*
 * drivers/platform/tegra/tegra_bootloader_debug.c
 *
 * Copyright (C) 2014-2015 NVIDIA Corporation. All rights reserved.
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/spinlock.h>
#include <linux/io.h>
#include <linux/seq_file.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/memblock.h>
#include <asm/page.h>
#include "tegra_bootloader_debug.h"

static const char *module_name = "tegra_bootloader_debug";
static const char *dir_name = "tegra_bootloader";
static const char *gr_file_mb1 = "gr_mb1";
static const char *gr_file_mb2 = "gr_mb2";
static const char *gr_file_cpu_bl = "gr_cpu_bl";
static const char *profiler = "profiler";

struct gr_address_value {
	unsigned int gr_address;
	unsigned int gr_value;
};

struct gr_header {
	uint32_t mb1_offset;
	uint32_t mb1_size;
	uint32_t mb2_offset;
	uint32_t mb2_size;
	uint32_t cpu_bl_offset;
	uint32_t cpu_bl_size;
};

enum gr_stage {
	enum_gr_mb1,
	enum_gr_mb2,
	enum_gr_cpu_bl,
};

const uint32_t gr_mb1 = enum_gr_mb1;
const uint32_t gr_mb2 = enum_gr_mb2;
const uint32_t gr_cpu_bl = enum_gr_cpu_bl;

static int dbg_golden_register_show(struct seq_file *s, void *unused);
static int profiler_show(struct seq_file *s, void *unused);
static int profiler_open(struct inode *inode, struct file *file);
static int dbg_golden_register_open_mb1(struct inode *inode, struct file *file);
static int dbg_golden_register_open_mb2(struct inode *inode, struct file *file);
static int dbg_golden_register_open_cpu_bl(struct inode *inode, struct file *file);
static struct dentry *bl_debug_node;
static struct dentry *bl_debug_verify_reg_node;
static struct dentry *bl_debug_profiler;
static void *tegra_bl_mapped_prof_start;
static void *tegra_bl_mapped_debug_data_start;

static const struct file_operations debug_gr_fops_mb1 = {
	.open           = dbg_golden_register_open_mb1,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = single_release,
};


static const struct file_operations debug_gr_fops_mb2 = {
	.open           = dbg_golden_register_open_mb2,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = single_release,
};


static const struct file_operations debug_gr_fops_cpu_bl = {
	.open           = dbg_golden_register_open_cpu_bl,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = single_release,
};


static const struct file_operations profiler_fops = {
	.open           = profiler_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = single_release,
};

#define MAX_PROFILE_STRLEN	55

struct profiler_record {
	char str[MAX_PROFILE_STRLEN + 1];
	uint64_t timestamp;
} __attribute__((packed));

static int profiler_show(struct seq_file *s, void *unused)
{
	struct profiler_record *profiler_data;
	int count = 0;
	int i = 0;
	int prof_data_section_valid = 0;

	if (!tegra_bl_mapped_prof_start || !tegra_bl_prof_size) {
		seq_printf(s, "Error mapping profiling data\n");
		return 0;
	}

	profiler_data = (struct profiler_record *)tegra_bl_mapped_prof_start;
	count = tegra_bl_prof_size / sizeof(struct profiler_record);
	i = -1;
	while (count--) {
		i++;
		if (!profiler_data[i].timestamp) {
			if (prof_data_section_valid) {
				seq_puts(s, "\n");
				prof_data_section_valid = 0;
			}
			continue;
		}
		seq_printf(s, "%-54s\t%16lld", profiler_data[i].str, profiler_data[i].timestamp);
		if (i > 0 && profiler_data[i - 1].timestamp) {
			seq_printf(s, "\t%16lld",
				profiler_data[i].timestamp -  profiler_data[i - 1].timestamp);
		}
		seq_puts(s, "\n");
		prof_data_section_valid = 1;
	}

	return 0;
}

static int profiler_open(struct inode *inode, struct file *file)
{
	return single_open(file, profiler_show, &inode->i_private);
}

static int dbg_golden_register_show(struct seq_file *s, void *unused)
{
	struct gr_header *golden_reg_header = (struct gr_header *)tegra_bl_mapped_debug_data_start;
	struct gr_address_value *gr_memory_dump;
	unsigned int gr_entries = 0;
	int i;

	switch (*(int *)(s->private)){
		case enum_gr_mb1:
			gr_entries = golden_reg_header->mb1_size / sizeof(struct gr_address_value);
			gr_memory_dump = (struct gr_address_value *)(golden_reg_header->mb1_offset +
					tegra_bl_mapped_debug_data_start + sizeof(struct gr_header));
			break;
		case enum_gr_mb2:
			gr_entries = golden_reg_header->mb2_size / sizeof(struct gr_address_value);
			gr_memory_dump = (struct gr_address_value *)(golden_reg_header->mb2_offset +
					tegra_bl_mapped_debug_data_start + sizeof(struct gr_header));
			break;
		case enum_gr_cpu_bl:
			gr_entries = golden_reg_header->cpu_bl_size / sizeof(struct gr_address_value);
			gr_memory_dump = (struct gr_address_value *)(golden_reg_header->cpu_bl_offset +
					tegra_bl_mapped_debug_data_start + sizeof(struct gr_header));
			break;
		default:
			seq_printf(s, "Eiiiirror mapping bootloader debug data%x \n",*(int *)(s->private));
			return 0;
	}
	if (!gr_entries || !tegra_bl_mapped_debug_data_start) {
		seq_printf(s, "Error mapping bootloader debug data\n");
		return 0;
	}

	for (i = 0; i < gr_entries; i++) {
		seq_printf(s, "{Address 0x%08x}, {Value 0x%08x}\n",
			gr_memory_dump->gr_address, gr_memory_dump->gr_value);

		gr_memory_dump++;
	}

	return 0;
}

static int dbg_golden_register_open_mb1(__attribute((unused))struct inode *inode, struct file *file)
{
	return single_open(file, dbg_golden_register_show, (void *)&gr_mb1);
}

static int dbg_golden_register_open_mb2( __attribute((unused))struct inode *inode, struct file *file)
{
	return single_open(file, dbg_golden_register_show, (void *)&gr_mb2);
}

static int dbg_golden_register_open_cpu_bl( __attribute((unused))struct inode *inode, struct file *file)
{
	return single_open(file, dbg_golden_register_show, (void *)&gr_cpu_bl);
}
static int __init tegra_bootloader_debuginit(void)
{
	void __iomem *ptr_bl_prof_start = NULL;
	void __iomem *ptr_bl_debug_data_start = NULL;

	bl_debug_node = debugfs_create_dir(dir_name, NULL);

	if (IS_ERR_OR_NULL(bl_debug_node)) {
		pr_err("%s: failed to create debugfs entries: %ld\n",
			module_name, PTR_ERR(bl_debug_node));
		goto out_err;
	}

	pr_info("%s: created %s directory\n", module_name, dir_name);

	bl_debug_verify_reg_node = debugfs_create_file(gr_file_mb1, S_IRUGO,
				bl_debug_node, NULL, &debug_gr_fops_mb1);

	if (IS_ERR_OR_NULL(bl_debug_verify_reg_node)) {
		pr_err("%s: failed to create debugfs entries: %ld\n",
			module_name, PTR_ERR(bl_debug_verify_reg_node));
		goto out_err;
	}

	bl_debug_verify_reg_node = debugfs_create_file(gr_file_mb2, S_IRUGO,
				bl_debug_node, NULL, &debug_gr_fops_mb2);

	if (IS_ERR_OR_NULL(bl_debug_verify_reg_node)) {
		pr_err("%s: failed to create debugfs entries: %ld\n",
			module_name, PTR_ERR(bl_debug_verify_reg_node));
		goto out_err;
	}

	bl_debug_verify_reg_node = debugfs_create_file(gr_file_cpu_bl, S_IRUGO,
				bl_debug_node, NULL, &debug_gr_fops_cpu_bl);

	if (IS_ERR_OR_NULL(bl_debug_verify_reg_node)) {
		pr_err("%s: failed to create debugfs entries: %ld\n",
			module_name, PTR_ERR(bl_debug_verify_reg_node));
		goto out_err;
	}
	bl_debug_profiler = debugfs_create_file(profiler, S_IRUGO,
				bl_debug_node, NULL, &profiler_fops);
	if (IS_ERR_OR_NULL(bl_debug_profiler)) {
		pr_err("%s: failed to create debugfs entries: %ld\n",
			module_name, PTR_ERR(bl_debug_profiler));
		goto out_err;
	}

	tegra_bl_mapped_prof_start = phys_to_virt(tegra_bl_prof_start);
	if (!pfn_valid(__phys_to_pfn(tegra_bl_prof_start))) {
		ptr_bl_prof_start = ioremap(tegra_bl_prof_start, tegra_bl_prof_size);

		WARN_ON(!ptr_bl_prof_start);
		if (!ptr_bl_prof_start) {
			pr_err("%s: Failed to map tegra_bl_prof_start%08x\n",
					__func__, (unsigned int)tegra_bl_prof_start);
			goto out_err;
		}

		tegra_bl_mapped_prof_start = ptr_bl_prof_start;
	}

	tegra_bl_mapped_debug_data_start = phys_to_virt(tegra_bl_debug_data_start);
	if (!pfn_valid(__phys_to_pfn(tegra_bl_debug_data_start))) {
		ptr_bl_debug_data_start = ioremap(tegra_bl_debug_data_start,
				tegra_bl_prof_size);

		WARN_ON(!ptr_bl_debug_data_start);
		if (!ptr_bl_debug_data_start) {
			pr_err("%s: Failed to map tegra_bl_debug_data_start%08x\n",
					__func__, (unsigned int)tegra_bl_debug_data_start);
			goto out_err;
		}

		tegra_bl_mapped_debug_data_start = ptr_bl_debug_data_start;
	}

	return 0;

out_err:
	if (!IS_ERR_OR_NULL(bl_debug_node))
		debugfs_remove_recursive(bl_debug_node);

	if (ptr_bl_prof_start)
		iounmap(ptr_bl_prof_start);

	if (ptr_bl_debug_data_start)
		iounmap(ptr_bl_debug_data_start);

	return -ENODEV;
}

static int __init tegra_bl_debuginit_module_init(void)
{
	return tegra_bootloader_debuginit();
}

static void __exit tegra_bl_debuginit_module_exit(void)
{
	if (!IS_ERR_OR_NULL(bl_debug_node))
		debugfs_remove_recursive(bl_debug_node);

	if (tegra_bl_mapped_prof_start)
		iounmap(tegra_bl_mapped_prof_start);

	if (tegra_bl_mapped_debug_data_start)
		iounmap(tegra_bl_mapped_debug_data_start);
}

module_init(tegra_bl_debuginit_module_init);
module_exit(tegra_bl_debuginit_module_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Driver to enumerate bootloader's debug data");
MODULE_AUTHOR("Mohit Dhingra <mdhingra@nvidia.com>");
