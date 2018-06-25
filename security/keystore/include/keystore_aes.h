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

#ifndef _KEYSTORE_AES_H_
#define _KEYSTORE_AES_H_

#include <linux/types.h>

#define AES_BLOCK_SIZE				16
#define AES128_KEY_SIZE				16
#define AES256_KEY_SIZE				32
#define KEYSTORE_CCM_AUTH_SIZE			8
#define KEYSTORE_GCM_AUTH_SIZE			8
#define KEYSTORE_AES_IV_SIZE			AES_BLOCK_SIZE


/**
 * keystore_aes_crypt() - Encrypt or decrypt a block of data using AES ECB.
 *
 * @enc: Mode: 1 - encrypt, 0 - decrypt.
 * @key: Pointer to the key.
 * @klen: Key size in bytes.
 * @data_in: Pointer to the input data block.
 * @data_out: Pointer to the output buffer.
 * @len: Input/output data size in bytes.
 *
 * Returns: 0 if OK or negative error code (see errno).
 */
int keystore_aes_crypt(int enc, const char *key, size_t klen,
		       const char *data_in, char *data_out, size_t len);

/**
 * keystore_aes_ccm_crypt() - Encrypt or decrypt a block of data using AES CCM.
 *
 * @enc: Mode: 1 - encrypt, 0 - decrypt.
 * @key: Pointer to the key.
 * @klen: Key size in bytes.
 * @iv: Pointer to the Initialization Vector.
 * @ivlen: Initialization Vector size in bytes.
 * @data_in: Pointer to the input data block.
 * @ilen: Input data block size in bytes.
 * @assoc_in: Pointer to the associated data block.
 * @alen: Associated data block size in bytes.
 * @data_out: Pointer to the output buffer.
 * @outlen: Output buffer size in bytes.
 *
 * See aead_request_set_crypt() for input and output data formatting.
 *
 * Returns: 0 if OK or negative error code (see errno).
 */
int keystore_aes_ccm_crypt(int enc, const char *key, size_t klen,
		const char *iv, size_t ivlen,
		const char *data_in, size_t ilen,
		size_t alen,
		char *data_out, size_t outlen);

/**
 * keystore_aes_gcm_crypt() - Encrypt or decrypt a block of data using AES GCM.
 *
 * @enc: Mode: 1 - encrypt, 0 - decrypt.
 * @key: Pointer to the key.
 * @klen: Key size in bytes.
 * @iv: Pointer to the Initialization Vector.
 * @ivlen: Initialization Vector size in bytes.
 * @data_in: Pointer to the input data block.
 * @ilen: Input data block size in bytes.
 * @assoc_in: Pointer to the associated data block.
 * @alen: Associated data block size in bytes.
 * @data_out: Pointer to the output buffer.
 * @outlen: Output buffer size in bytes.
 *
 * Returns: if OK or negative error code (see errno).
 */
int keystore_aes_gcm_crypt(int enc, const char *key, size_t klen,
		const char *iv, size_t ivlen,
		const char *data_in, size_t ilen,
		size_t alen,
		char *data_out, size_t outlen);

/**
 * keystore_aes_ctr_crypt() - Encrypt or decrypt a block of data using AES CTR.
 *
 * @enc: Mode: 1 - encrypt, 0 - decrypt.
 * @key: Pointer to the key.
 * @klen: Key size in bytes.
 * @iv: Pointer to the Initialization Vector.
 * @ivlen: Initialization Vector size in bytes.
 * @data_in: Pointer to the input data block.
 * @data_out: Pointer to the output buffer.
 * @len: Input/output data size in bytes.
 *
 * Returns: 0 if OK or negative error code (see errno).
 */
int keystore_aes_ctr_crypt(int enc, const char *key, size_t klen,
		const char *iv, size_t ivlen,
		const char *data_in, char *data_out, size_t len);

/**
 * keystore_aes_siv_crypt() - Decrypt a block of data using AES-SIV.
 *
 * @enc: Mode: 1 - encrypt, 0 - decrypt.
 * @key: Pointer to the key.
 * @key_size: Key size in bytes.
 * @input: Pointer to the input data block.
 * @isize: Input data block size in bytes.
 * @associated_data: Pointer to the associated data block.
 * @asize: Associated data block size in bytes.
 * @output: Pointer to the output buffer.
 * @osize: Output buffer size in bytes.
 *
 * Returns: 0 if OK or negative error code (see errno).
 */
int keystore_aes_siv_crypt(int enc, const void *key, unsigned int key_size,
			   const void *input, unsigned int isize,
			   const void *associated_data, unsigned int asize,
			   void *output, unsigned int osize);

#endif /* _KEYSTORE_CRYPTO_H_ */
