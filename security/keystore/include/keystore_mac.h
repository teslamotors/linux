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

#ifndef _KEYSTORE_MAC_H_
#define _KEYSTORE_MAC_H_

#include <linux/types.h>

#define SHA256_HMAC_SIZE	                32

/**
 * keystore_calc_mac() - Calculate SHA-256 HMAC or AES-128 CMAC.
 *
 * @alg_name: Algorithm name ("hmac(sha256)" or "cmac(aes)").
 * @key: Pointer to the key.
 * @klen: Key size in bytes.
 * @data_in: Pointer to the data block.
 * @dlen: Data block size in bytes.
 * @hash_out: Pointer to the output buffer.
 * @outlen: Output buffer size in bytes.
 *
 * Returns: 0 if OK or negative error code (see errno).
 */
int keystore_calc_mac(const char *alg_name, const char *key, size_t klen,
		      const char *data_in, size_t dlen,
		      char *hash_out, size_t outlen);

/**
 * keystore_sha256_block() - Calculate SHA-256 digest of a data block.
 *
 * @data: The data block pointer.
 * @size: The size of data in bytes.
 * @result: The result digest pointer.
 * @result_size: The result digest block size.
 *
 * Returns: 0 if OK or negative error code.
 */
int keystore_sha256_block(const void *data, unsigned int size,
			  void *result, unsigned int result_size);

#endif /* _KEYSTORE_MAC_H_ */
