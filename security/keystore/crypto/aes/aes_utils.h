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

#endif /* _KEYSTORE_HELPERS_H_ */
