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

void *alloc_and_init_sg(struct scatterlist *sg, const void *data,
			size_t buflen, size_t datalen)
{
	void *ptr = NULL;

	if (!sg || (data && (datalen > buflen)))
		return NULL;

	ptr = kzalloc(buflen, GFP_KERNEL);
	if (!ptr)
		return NULL;

	if (data)
		memcpy(ptr, data, datalen);

	sg_init_one(sg, ptr, buflen);

	return ptr;
}

/* end of file */
