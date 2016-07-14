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
 * Prints the mpi variable as hex bytes.
 *
 * @param a    - mpi variable.
 *
 */
void print_mpi(MPI a)
{
	int sign = 0;
	unsigned int nbytes = 0;
	unsigned char *buf = 0;

	buf = (unsigned char *)mpi_get_buffer(a, &nbytes, &sign);
	keystore_hexdump("", buf, nbytes);

	kfree(buf);
}

#ifdef DEBUG_APP_AUTH
/**
 * Prints the public key components in hex form.
 *
 * @param key    - public key to be printed.
 *
 */
void debug_public_key(struct public_key *key)
{
	ks_debug("DEBUG_APPAUTH: key->rsa.e\n");
	print_mpi(key->rsa.e);
	ks_debug("DEBUG_APPAUTH: key->rsa.n\n");
	print_mpi(key->rsa.n);
}

/**
 * Prints the character array.
 *
 * @param p      - string to be printed.
 * @param len    - length of the string.
 *
 */
void print_string(unsigned char *p, int len)
{
	int i = 0;

	for (i = 0; i < len; i++)
		ks_err("%d ", p[i]);
	ks_err("\n");
}

/**
 * Prints the key id in hex form.
 *
 * @param id    -  key id to be printed.
 *
 */
void print_kid(struct asymmetric_key_id *id)
{
	unsigned short len = 0;

	if (!id) {
		ks_err("DEBUG_APPAUTH: kid is null\n");
		return;
	}

	ks_debug("DEBUG_APPAUTH: kid len = %d\n", id->len);
	len = id->len;

	print_string((unsigned char *)(id->data), len);
}

/**
 * Prints the key id in hex form.
 *
 * @param id    -  key id to be printed.
 *
 */
void print_key_id(struct asymmetric_key_id *id)
{
	int asn_len = 0;

	if (((id->data)[0]) == 0x31) {
		asn_len = 2 + ((id->data)[1]);
		ks_debug("DEBUG_APPAUTH: asn_len = %d\n", asn_len);
		print_string((unsigned char *)((id->data) + asn_len),
							id->len - asn_len);
	}
}
#endif



/**
 * Provides the process file path.
 *
 * @param buf    -  The buffer to store the file path.
 * @param buflen -  The size of the buffer.
 *
 * @returns the address of the file path.
 */
static char *get_exe_path(char *buf, int buflen)
{
	struct file *exe_file = NULL;
	char *result = NULL;
	struct mm_struct *mm = NULL;

	mm = get_task_mm(current);
	if (!mm) {
#ifdef DEBUG_APP_AUTH
		ks_info(KBUILD_MODNAME ": %s error get_task_mm\n", __func__);
#endif
		goto out;
	}

	down_read(&mm->mmap_sem);
	exe_file = mm->exe_file;

	if (exe_file)
		path_get(&exe_file->f_path);

	up_read(&mm->mmap_sem);
	mmput(mm);
	if (exe_file) {
		result = d_path(&exe_file->f_path, buf, buflen);
		path_put(&exe_file->f_path);
	}
out:
	return result;
}

/**
 * Provides the process file path.
 * Performs error check on the retrieved path.
 *
 * @param buf    -  The buffer to store the file path.
 *
 * @returns the address of the file path if success or nul otherwise.
 */
char *get_exe_name(char **buf)
{
	char *f_path = NULL;

	/* alloc mem for pwd##app, use linux defined limits */
	*buf = kmalloc(PATH_MAX + NAME_MAX, GFP_KERNEL);
	if (NULL != *buf) {
		/* clear the buf */
		memset(*buf, 0, PATH_MAX + NAME_MAX);

		f_path = get_exe_path(*buf,
					(PATH_MAX + NAME_MAX));

		if (!f_path)
			return NULL;

		/* check if it was an error */
		if (f_path && IS_ERR(f_path)) {
#ifdef DEBUG_APP_AUTH
			/* error case, do not register */
			ks_err(KBUILD_MODNAME "get_exe_name() failed\n");
#endif
			return NULL;
		}
#ifdef DEBUG_APP_AUTH
		ks_debug(KBUILD_MODNAME "absolute path of exe: %s\n", f_path);
#endif
	} else
		return NULL;

	return f_path;
}
