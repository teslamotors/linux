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
#include <asm/setup.h>

static unsigned int board_serial_low;
static unsigned int board_serial_high;

static int read_serialinfo_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%013llu\n", (((unsigned long long int)board_serial_high) << 32) | board_serial_low);
	return 0;
}

static int read_serialinfo_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, read_serialinfo_proc_show, NULL);
}

static const struct file_operations read_serialinfo_proc_fops = {
	.open = read_serialinfo_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

int __init board_serial_proc(void)
{
	board_serial_low = system_serial_low;
	board_serial_high = system_serial_high;
	if (!proc_create("board_serial", 0, NULL, &read_serialinfo_proc_fops)) {
		printk(KERN_ERR "Can't create proc entry for board_serial!\n");
		return -1;
	}
	return 0;
}
