/*
 * arch/arm/mach-tegra/board-info.c
 *
 * File to contain changes for sku id, serial id, chip id, etc.
 *
 * Copyright (c) 2010, NVIDIA Corporation.
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

#define PRODINFO_BUF_SIZE 19 /* prodinfo is 18 character long xxx-xxxxx-xxxx-xxx so buffer size is set to 18+1 */
#define PRODVER_BUF_SIZE 3   /* prodver is 2 character long XX so buffer size is set to 2+1 */

static char prodinfo_buffer[PRODINFO_BUF_SIZE];
static char prodver_buffer[PRODVER_BUF_SIZE];

static int __init prod_info_setup(char *line)
{
	memcpy(prodinfo_buffer, line, PRODINFO_BUF_SIZE);
	return 1;
}

__setup("ProdInfo=", prod_info_setup);

static int read_prodinfo_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%s\n", prodinfo_buffer);
	return 0;
}

static int read_prodinfo_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, read_prodinfo_proc_show, NULL);
}

static const struct file_operations read_prodinfo_proc_fops = {
	.open = read_prodinfo_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

int __init tegra_prodinfo_proc(void)
{
	if (!proc_create("prodinfo", 0, NULL, &read_prodinfo_proc_fops)) {
		printk(KERN_ERR "Can't create proc entry for prodinfo!\n");
		return -1;
	}
	return 0;
}

static int __init prod_ver_setup(char *line)
{
	memcpy(prodver_buffer, line, PRODVER_BUF_SIZE);
	return 1;
}

__setup("ProdVer=", prod_ver_setup);

static int read_prodver_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%s\n", prodver_buffer);
	return 0;
}

static int read_prodver_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, read_prodver_proc_show, NULL);
}

static const struct file_operations read_prodver_proc_fops = {
	.open = read_prodver_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

int __init tegra_prodver_proc(void)
{
	if (!proc_create("prodver", 0, NULL, &read_prodver_proc_fops)) {
		printk(KERN_ERR "Can't create proc entry for prodver!\n");
		return -1;
	}
	return 0;
}






