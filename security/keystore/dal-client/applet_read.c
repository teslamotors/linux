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

#define APPLET_MAX_LEN 30000

static int read_applet_data(struct file *file, loff_t offset,
				char *addr, unsigned long count)
{
	mm_segment_t old_fs;
	char __user *buf = (char __user *)addr;
	ssize_t ret;

	if (!(file->f_mode & FMODE_READ))
		return -EBADF;

	old_fs = get_fs();
	set_fs(get_ds());
	ret = __vfs_read(file, buf, count, &offset);
	set_fs(old_fs);

	return ret;
}

static int read_applet_file(struct file *file, u8 **out)
{
	loff_t i_size = 0, offset = 0;
	char *file_buf = 0;
	u8 *temp = NULL;

	int rc = 0, read = 0, count = 0;

	*out = NULL;

	i_size = i_size_read(file_inode(file));
	ks_debug(KBUILD_MODNAME ": %s  applet size = %lld\n", __func__, i_size);

	if (i_size == 0)
		goto out;

	if (i_size > APPLET_MAX_LEN) {
		ks_err(KBUILD_MODNAME ": %s applet size exceeds maximum\n",
					__func__);
		goto out;
	}

	file_buf = kzalloc(PAGE_SIZE, GFP_KERNEL);
	if (!file_buf) {
		ks_err(KBUILD_MODNAME ": %s could not allocate buffer for the applet\n",
					__func__);
		return -ENOMEM;
	}

	temp = kzalloc(i_size, GFP_KERNEL);
	if (!temp) {
		ks_err(KBUILD_MODNAME ": %s could not allocate buffer for the applet\n",
					__func__);
		return -ENOMEM;
	}

	if (!(file->f_mode & FMODE_READ)) {
		file->f_mode |= FMODE_READ;
		read = 1;
	}

	while (offset < i_size) {
		int file_buf_len;

		file_buf_len = read_applet_data(file, offset,
			file_buf, PAGE_SIZE);
		if (file_buf_len < 0) {
			rc = file_buf_len;
			break;
		}
		if (file_buf_len == 0)
			break;
		memcpy(temp + offset, file_buf, file_buf_len);
		offset += file_buf_len;
		count++;
	}

	if (read)
		file->f_mode &= ~FMODE_READ;
	kfree(file_buf);

	if (rc) {
		kfree(temp);
		return -EFAULT;
	}
	*out = temp;
	return i_size;
out:
	return -EFAULT;
}

static int lock_read_applet(struct file *file, u8 **out)
{
	struct inode *inode = file_inode(file);
	int ret = 0;

	inode_lock_shared(inode);
	ret =  read_applet_file(file, out);
	inode_unlock_shared(inode);
	return ret;
}

int read_keystore_applet(const char *filename, u8 **buf, size_t *len)
{
	struct file *file = 0;
	int ret = 0;

	file = filp_open(filename,  O_RDONLY, 0);
	if (IS_ERR(file)) {
		ks_err(KBUILD_MODNAME ": %s could not open applet file\n",
					__func__);
		return -EBADF;
	}

	ks_debug(KBUILD_MODNAME ": %s applet file opened\n", __func__);
	ret = lock_read_applet(file, buf);
	if (ret <= 0) {
		ks_err(KBUILD_MODNAME ": %s could not read applet file\n",
					__func__);
		return -EBADF;
	}
	*len = ret;
	filp_close(file, NULL);
	return 0;
}
