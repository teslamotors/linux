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

#ifndef _KEYSTORE_ECC_H_
#define _KEYSTORE_ECC_H_

#include <security/keystore_api_common.h>

/* ECC Public key size for backup operations */
#define KEYSTORE_ECC_PUB_KEY_SIZE   (sizeof(struct ias_keystore_ecc_public_key))

/* ECC Private key size used on the host for migration */
#define KEYSTORE_ECC_PRIV_KEY_SIZE  (sizeof(__u32) * KEYSTORE_ECC_DIGITS)

/**
 * keystore_ecc_gen_keys() - Generate ECC private and public key from a seed
 *
 * @seed_in: Pointer to the random seed used to derive the keys.
 * @seed_in_size: Size of the seed @seed_in.
 * @key_pair: Output generated ECC key pair.
 *
 * Returns: 0 OK or negative error code (see errno),
 * ENODATA if SEED cannot be used to generate private key or
 * EKEYREJECTED if public key cannot pass verification.
 */
int keystore_ecc_gen_keys(const uint8_t *seed_in, size_t seed_in_size,
			  struct ias_keystore_ecc_keypair *key_pair);

/**
 * keystore_ecc_encrypt() - Encrypt a block of data using ECC (ECIES).
 *
 * @key: Public key pointer (raw data).
 * @data: Block of data to be encrypted.
 * @data_size: Size of data in bytes.
 * @output: Block of data for the result.
 * @output_size: Size of output data block.
 *
 * Returns: The number of bytes written into output block if OK
 * or negative error code (see errno).
 */
int keystore_ecc_encrypt(const struct keystore_ecc_public_key *key,
			 const void *data, unsigned int data_size,
			 void *output, unsigned int output_size);

/**
 * keystore_ecc_decrypt() - Decrypt a block of data using ECC (ECIES).
 *
 * @key: Private key pointer (raw data).
 * @data: Block of data to be decrypted.
 * @data_size: Size of data in bytes.
 * @output: Block of data for the result.
 * @output_size: Size of output data block.
 *
 * Returns: The number of bytes written into output block if OK or negative error
 * code (see errno).
 */
int keystore_ecc_decrypt(const uint32_t *key,
			 const void *data, unsigned int data_size,
			 void *output, unsigned int output_size);

/**
 * keystore_ecc_sign() - Perform data sign using ecc Public key.
 *
 * @key: Private ECC key.
 * @data: Data from which hash sum will be counted.
 * @data_size: Size of @data.
 * @sig: The ECC signature of @data.
 *
 * Returns: signature size if OK or negative error code (see errno).
 */
int keystore_ecc_sign(const uint32_t *key,
		      const void *data, unsigned int data_size,
		      struct keystore_ecc_signature *sig);

/**
 *  keystore_ecc_verify() - Perform signature verification using ecc public key.
 *
 * @key: public ECC key to decrypt hash sum from signature file.
 * @data: Data from which hash sum will be counted.
 * @data_size: Data size.
 * @sig: ECC signature to be verfied.
 *
 * Returns: if OK or negative error code (see errno).
 */
int keystore_ecc_verify(const struct keystore_ecc_public_key *key,
			const void *data, unsigned int data_size,
			const struct keystore_ecc_signature *sig);

#endif /* _KEYSTORE_ECC_H_ */
