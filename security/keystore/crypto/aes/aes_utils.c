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

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>

#include "aes_utils.h"

/**
 * Allocate a buffer and optionally initialize with contents of a data block.
 *
 * @param sg The pointer to scatter-gather structure.
 * @param data The pointer to the initializing data (NULL if no initialization needed).
 * @param buflen The size of the buffer to allocate in bytes.
 * @param datalen The size of initialization data (not used if data = NULL).
 */
void *alloc_and_init_sg(struct scatterlist *sg, const void *data,
		size_t buflen, size_t datalen)
{
	void *ptr = kzalloc(buflen, GFP_KERNEL);

	if (ptr) {
		if (data)
			memcpy(ptr, data, datalen);

		sg_init_one(sg, ptr, buflen);
	}
	return ptr;
}

/* end of file */
