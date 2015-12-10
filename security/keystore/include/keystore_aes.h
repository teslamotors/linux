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

/**
 * Encrypt or decrypt a block of data using AES ECB.
 *
 * @param enc Mode: 1 - encrypt, 0 - decrypt.
 * @param key Pointer to the key.
 * @param klen Key size in bytes.
 * @param data_in Pointer to the input data block.
 * @param data_out Pointer to the output buffer.
 * @param len Input/output data size in bytes.
 *
 * @return 0 if OK or negative error code (see errno).
 */
int keystore_aes_crypt(int enc, const char *key, size_t klen,
		const char *data_in, char *data_out, size_t len);

/**
 * Encrypt or decrypt a block of data using AES CCM.
 *
 * @param enc Mode: 1 - encrypt, 0 - decrypt.
 * @param key Pointer to the key.
 * @param klen Key size in bytes.
 * @param iv Pointer to the Initialization Vector.
 * @param ivlen Initialization Vector size in bytes.
 * @param data_in Pointer to the input data block.
 * @param ilen Input data block size in bytes.
 * @param assoc_in Pointer to the associated data block.
 * @param alen Associated data block size in bytes.
 * @param data_out Pointer to the output buffer.
 * @param outlen Output buffer size in bytes.
 *
 * @return 0 if OK or negative error code (see errno).
 */
int keystore_aes_ccm_crypt(int enc, const char *key, size_t klen,
		const char *iv, size_t ivlen,
		const char *data_in, size_t ilen, const char *assoc_in,
		size_t alen,
		char *data_out, size_t outlen);

/**
 ** Encrypt or decrypt a block of data using AES GCM.
 **
 ** @param enc Mode: 1 - encrypt, 0 - decrypt.
 ** @param key Pointer to the key.
 ** @param klen Key size in bytes.
 ** @param iv Pointer to the Initialization Vector.
 ** @param ivlen Initialization Vector size in bytes.
 ** @param data_in Pointer to the input data block.
 ** @param ilen Input data block size in bytes.
 ** @param assoc_in Pointer to the associated data block.
 ** @param alen Associated data block size in bytes.
 ** @param data_out Pointer to the output buffer.
 ** @param outlen Output buffer size in bytes.
 **
 ** @return 0 if OK or negative error code (see errno).
 **/
int keystore_aes_gcm_crypt(int enc, const char *key, size_t klen,
		const char *iv, size_t ivlen,
		const char *data_in, size_t ilen, const char *assoc_in,
		size_t alen,
		char *data_out, size_t outlen);

/**
 * Encrypt or decrypt a block of data using AES CTR.
 *
 * @param enc Mode: 1 - encrypt, 0 - decrypt.
 * @param key Pointer to the key.
 * @param klen Key size in bytes.
 * @param iv Pointer to the Initialization Vector.
 * @param ivlen Initialization Vector size in bytes.
 * @param data_in Pointer to the input data block.
 * @param data_out Pointer to the output buffer.
 * @param len Input/output data size in bytes.
 *
 * @return 0 if OK or negative error code (see errno).
 */
int keystore_aes_ctr_crypt(int enc, const char *key, size_t klen,
		const char *iv, size_t ivlen,
		const char *data_in, char *data_out, size_t len);

/**
 * Decrypt a block of data using AES-SIV.
 *
 * @param enc Mode: 1 - encrypt, 0 - decrypt.
 * @param key Pointer to the key.
 * @param key_size Key size in bytes.
 * @param input Pointer to the input data block.
 * @param isize Input data block size in bytes.
 * @param associated_data Pointer to the associated data block.
 * @param asize Associated data block size in bytes.
 * @param output Pointer to the output buffer.
 * @param osize Output buffer size in bytes.
 *
 * @return 0 if OK or negative error code (see errno).
 */
int keystore_aes_siv_crypt(int enc, const void *key, unsigned int key_size,
			   const void *input, unsigned int isize,
			   const void *associated_data, unsigned int asize,
			   void *output, unsigned int osize);

#endif /* _KEYSTORE_CRYPTO_H_ */
