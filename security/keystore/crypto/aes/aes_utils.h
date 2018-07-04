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

/**
 * alloc_and_init_sg() - Allocate a buffer and optionally initialize.
 *
 * @sg: The pointer to scatter-gather structure.
 * @data: The pointer to the initializing data
 *        (NULL if no initialization needed).
 * @buflen: The size of the buffer to allocate in bytes.
 * @datalen: The size of initialization data (not used if data = NULL).
 *
 * Allocate a new buffer of length @buflen.
 * If the @data is not NULL and @datalen < @buflen, copy @data into the buffer.
 * Initialise scatter-gather structure with the new buffer.
 *
 * Returns: A pointer to the newly allocated buffer if successful. Will return
 * NULL if the memory allocation fails, if NULL input pointers are provided, or
 * if @data is non-NULL and @datalen > @buflen.
 */
void *alloc_and_init_sg(struct scatterlist *sg, const void *data, size_t buflen,
			size_t datalen);

#endif /* _KEYSTORE_HELPERS_H_ */
