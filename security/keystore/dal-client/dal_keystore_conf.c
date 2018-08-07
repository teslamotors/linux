/*
 *
 * Intel DAL Keystore Linux driver
 * Copyright (c) 2013, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#include <linux/file.h>
#include <linux/fs.h>
#include <linux/crypto.h>
#include <linux/key-type.h>
#include <linux/mpi.h>
#include <linux/kernel.h>
#include <linux/string.h>

#include "keystore_debug.h"

#define DAL_KEYSTORE_CONF_MAX_LEN 10

static int read_conf_value(struct file *file)
{
	mm_segment_t old_fs;
	char conf_value = 0;
	char __user *buf = (char __user *)&conf_value;
	loff_t offset = 0;

	if (!(file->f_mode & FMODE_READ))
		return -EBADF;

	old_fs = get_fs();
	set_fs(get_ds());
	__vfs_read(file, buf, 1, &offset);
	set_fs(old_fs);
	if (conf_value == '1')
		return 1;
	else
		return 0;
}

static int read_conf_file(struct file *file)
{
	loff_t i_size = 0;
	int ret = 0, read = 0;

	i_size = i_size_read(file_inode(file));
	if ((i_size == 0) || (i_size > DAL_KEYSTORE_CONF_MAX_LEN))
		return -EBADF;

	if (!(file->f_mode & FMODE_READ)) {
		file->f_mode |= FMODE_READ;
		read = 1;
	}
	ret = read_conf_value(file);
	if (read)
		file->f_mode &= ~FMODE_READ;
	return ret;
}

int read_dal_keystore_conf(const char *filename)
{
	struct file *file = 0;
	int ret = 0;
	struct inode *inode = NULL;

	file = filp_open(filename,  O_RDONLY, 0);
	if (IS_ERR(file)) {
		ks_err(KBUILD_MODNAME ": %s file open failed: %s\n",
				__func__, filename);
		return -EBADF;
	}
	ks_err(KBUILD_MODNAME ": %s file open succeeded: %s\n",
				__func__, filename);

	inode = file_inode(file);
	inode_lock_shared(inode);
	ret = read_conf_file(file);
	inode_unlock_shared(inode);

	filp_close(file, NULL);
	return ret;
}
