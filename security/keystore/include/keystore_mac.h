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

/**
 * Calculate SHA-256 HMAC or AES-128 CMAC.
 *
 * @param alg_name Algorithm name ("hmac(sha256)" or "cmac(aes)").
 * @param key Pointer to the key.
 * @param klen Key size in bytes.
 * @param data_in Pointer to the data block.
 * @param dlen Data block size in bytes.
 * @param hash_out Pointer to the output buffer.
 * @param outlen Output buffer size in bytes.
 *
 * @return 0 if OK or negative error code (see errno).
 */
int keystore_calc_mac(const char *alg_name, const char *key, size_t klen,
		      const char *data_in, size_t dlen,
		      char *hash_out, size_t outlen);

/**
 * Calculate SHA-256 digest of a data block.
 *
 * @param data The data block pointer.
 * @param size The size of data in bytes.
 * @param result The result digest pointer.
 * @param result_size The result digest block size.
 *
 * @return 0 if OK or negative error code.
 */
int keystore_sha256_block(const void *data, unsigned int size,
			  void *result, unsigned int result_size);

#endif /* _KEYSTORE_MAC_H_ */
