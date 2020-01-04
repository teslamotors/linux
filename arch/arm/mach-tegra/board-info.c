/*
 * arch/arm/mach-tegra/board-info.c
 *
 * File to contain changes for sku id, serial id, chip id, etc.
 *
 * Copyright (c) 2013, NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include "board.h"
#include <mach/board_id.h>

#define SKUINFO_BUF_SIZE 19 /* skuinfo is 18 character long xxx-xxxxx-xxxx-xxx so buffer size is set to 18+1 */
#define SKUVER_BUF_SIZE 3   /* skuver is 2 character long XX so buffer size is set to 2+1 */

static char skuinfo_buffer[SKUINFO_BUF_SIZE];
static char skuver_buffer[SKUVER_BUF_SIZE];
static struct tegra_vcm_board_info vcm_board_info;

static int __init parse_vcm_skuinfo(char *line)
{
	char *traverser;
	int delimiter_index;

	if (line == NULL) {
		printk(KERN_WARNING"WARNING: Invalid VCM SKU info\n");
		return 0;
	}
	traverser = line;

	vcm_board_info.valid = 1;
	delimiter_index = strcspn(traverser, "-");
	memcpy(vcm_board_info.bom,
			traverser, delimiter_index);
	traverser += delimiter_index + 1;

	delimiter_index = strcspn(traverser, "-");
	memcpy(vcm_board_info.project,
			traverser, delimiter_index);
	traverser += delimiter_index + 1;

	delimiter_index = strcspn(traverser, "-");
	memcpy(vcm_board_info.sku,
			traverser, delimiter_index);
	traverser += delimiter_index + 1;

	delimiter_index = strcspn(traverser, "-");
	memcpy(vcm_board_info.revision,
			traverser, delimiter_index);
	traverser += delimiter_index + 1;
	return 0;
}

static int __init sku_info_setup(char *line)
{
	memcpy(skuinfo_buffer, line, SKUINFO_BUF_SIZE);
	skuinfo_buffer[SKUINFO_BUF_SIZE - 1] = '\0';
	parse_vcm_skuinfo(skuinfo_buffer);
	return 1;
}

__setup("nvsku=", sku_info_setup);

static int read_skuinfo_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%s\n", skuinfo_buffer);
	return 0;
}

static int read_skuinfo_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, read_skuinfo_proc_show, NULL);
}

static const struct file_operations read_skuinfo_proc_fops = {
	.open = read_skuinfo_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

int __init tegra_skuinfo_proc(void)
{
	if (!proc_create("skuinfo", 0, NULL, &read_skuinfo_proc_fops)) {
		printk(KERN_ERR "Can't create proc entry for skuinfo!\n");
		return -1;
	}
	return 0;
}

static int __init sku_ver_setup(char *line)
{
	memcpy(skuver_buffer, line, SKUVER_BUF_SIZE);
	return 1;
}

__setup("SkuVer=", sku_ver_setup);

static int read_skuver_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%s\n", skuver_buffer);
	return 0;
}

static int read_skuver_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, read_skuver_proc_show, NULL);
}

static const struct file_operations read_skuver_proc_fops = {
	.open = read_skuver_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

int __init tegra_skuver_proc(void)
{
	if (!proc_create("skuver", 0, NULL, &read_skuver_proc_fops)) {
		printk(KERN_ERR "Can't create proc entry for skuver!\n");
		return -1;
	}
	return 0;
}

bool tegra_is_vcm_board_of_type(const char *bom, const char *project,
			const char *sku, const char *revision)
{
	bool b = true;

	if (vcm_board_info.valid != 0) {
		b = (bom == NULL ||
				!strncmp(vcm_board_info.bom,
					bom, MAX_BUFFER)) &&
			(project == NULL ||
			 !strncmp(vcm_board_info.project,
				 project, MAX_BUFFER)) &&
			(sku == NULL ||
			 !strncmp(vcm_board_info.sku,
				 sku, MAX_BUFFER)) &&
			(revision == NULL ||
			 !strncmp(vcm_board_info.revision,
				 revision, MAX_BUFFER));

		if (b)
			return true;
	}
	return false;
}

u32 tegra_get_vcm_sku_rev(void)
{
	u32 sku_rev = 0;
	sku_rev = simple_strtol(vcm_board_info.revision, NULL, 0);
	return sku_rev;
}
