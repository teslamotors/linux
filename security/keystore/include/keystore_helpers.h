/*
 *
 * Intel Keystore Linux driver
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

#ifndef _KEYSTORE_HELPERS_H_
#define _KEYSTORE_HELPERS_H_

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/scatterlist.h>

#define DBGLINE pr_info(KBUILD_MODNAME " %s:%u\n", __func__, __LINE__)

/**
 * Allocate a buffer and optionally initialize with contents of a data block.
 *
 * @param sg The pointer to scatter-gather structure.
 * @param data The pointer to the initializing data (NULL if no initialization needed).
 * @param buflen The size of the buffer to allocate in bytes.
 * @param datalen The size of initialization data (not used if data = NULL).
 */
void *alloc_and_init_sg(struct scatterlist *sg, const void *data, size_t buflen,
		size_t datalen);

/**
 * Compare two blocks of data. Show the first block. Show error message and the second block if different.
 *
 * @param name1 First data block name.
 * @param ptr1 First data block pointer.
 * @param name2 Second data block name.
 * @param ptr2 Second data block pointer.
 * @param size Size of data blocks in bytes.
 */
int show_and_compare(const char *name1, const void *ptr1, const char *name2, const void *ptr2, unsigned int size);

/**
 * Get the absolute path of current process in the filesystem. This is used
 * for client authentication purpose.
 *
 * Always use the return variable from this function, input variable 'buf'
 * may not contain the start of the path. In case if kernel was executing
 * a kernel thread then this function return NULL.
 *
 * @param input buf place holder for updating the path
 * @param input buflen avaialbe space
 *
 * @return path of current process, or NULL if it was kernel thread.
 */

char *get_current_process_path(char *buf, int buflen);



#endif /* _KEYSTORE_HELPERS_H_ */
