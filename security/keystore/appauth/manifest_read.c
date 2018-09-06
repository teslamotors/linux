/*
 *
 * Application Authentication
 * Copyright (c) 2016, Intel Corporation.
 * Written by Vinod Kumar P M (p.m.vinod@intel.com)
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

#include "app_auth.h"

/**
 * Reads the manifest file and copy the content to the manifest_buf.
 *
 * @param file         - pointer to the file structure for the manifest file.
 * @param manifest_buf - contain the entire manifest file content.
 *
 * @return 0 if success or error code.
 */
static int appauth_read_buf(struct file *file, char **manifest_buf)
{
	loff_t i_size, offset = 0;
	int read = 0;
	int manifest_len = 0;

	i_size = i_size_read(file_inode(file));
	ks_debug("DEBUG_APPAUTH: file size = %lld\n", i_size);

	if (i_size == 0)
		goto out;

	if (i_size >= MANIFEST_MAX_LEN) {
		manifest_len = -1;
		goto out;
	}

	*manifest_buf = kzalloc(PAGE_SIZE, GFP_KERNEL);
	if (!(*manifest_buf))
		return -ENOMEM;

	if (!(file->f_mode & FMODE_READ)) {
		file->f_mode |= FMODE_READ;
		read = 1;
	}

	manifest_len = appauth_kernel_read(file, offset,
					*manifest_buf, PAGE_SIZE);
	ks_debug("DEBUG_APPAUTH: manifest_len = %d\n", manifest_len);
	if (read)
		file->f_mode &= ~FMODE_READ;
out:
	return manifest_len;
}

/**
 * Frees the manifest_buf content.
 *
 * @param manifest_buf - contain the entire manifest file content.
 *
 */
void appauth_free_buf(char **manifest_buf)
{
	kfree(*manifest_buf);
	*manifest_buf = NULL;
}

/**
 * Reads the manifest file and copy the content to the manifest_buf.
 * Locks the inode mutex for the file read.
 *
 * @param file         - pointer to the file structure for the manifest file.
 * @param manifest_buf - contain the entire manifest file content.
 *
 * @return 0 if success or error code (see enum APP_AUTH_ERROR).
 */
int read_file(struct file *file, char **manifest_buf)
{
	struct inode *inode = file_inode(file);
	int manifest_len = 0;

	ks_debug("DEBUG_APPAUTH: calling mutex_lock\n");
	mutex_lock(&(file->f_pos_lock));
	manifest_len = appauth_read_buf(file, manifest_buf);
	mutex_unlock(&(file->f_pos_lock));
	return manifest_len;
}

/**
 * Reads the manifest file and copy the content to the manifest_buf.
 * Opens the file for reading.
 *
 * @param filename     - absolute path of the manifest file.
 * @param manifest_buf - contain the entire manifest file content.
 * @param manifest_len - manifest file length.
 *
 * @return 0 if success or error code.
 */
int read_manifest(const char *filename, char **manifest_buf, int *manifest_len)
{
	struct file *file = 0;

	file = filp_open(filename,  O_RDONLY, 0);
	if (IS_ERR(file)) {
		ks_err("DEBUG_APPAUTH: filp_open failed\n");
		return -EBADF;
	}
	ks_debug("DEBUG_APPAUTH: filp_open succeeded\n");
	*manifest_len = read_file(file, manifest_buf);
	filp_close(file, NULL);
	if (*manifest_len < 0) {
		ks_err("DEBUG_APPAUTH: manifest read_file failed (manifest file too big)\n");
		return -EILSEQ;
	}
	return 0;
}
