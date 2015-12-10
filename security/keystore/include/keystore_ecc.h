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

/**
 * Generate ECC private and public key from a seed
 *
 * @param public_key public ECC key.
 * @param private_key private ECC key.
 * @param seed_in a pointer to the seed used to derive the keys
 *
 * @return 0 OK or negative error code (see errno),
 * ENODATA if SEED cannot be used to generate private key or
 * EKEYREJECTED if public key cannot pass verification.
 */
int keystore_ecc_gen_keys(const uint8_t *seed_in, size_t seed_in_size,
			  struct ias_keystore_ecc_keypair *key_pair);

/**
 * Encrypt a block of data using ECC (ECIES).
 *
 * @param key Public key pointer (raw data).
 * @param key_size Public key size in bytes.
 * @param data Block of data to be encrypted.
 * @param data_size Size of data in bytes.
 * @param output Block of data for the result.
 * @param output_size Size of output data block.
 *
 * @return The number of bytes written into output block if OK
 * or negative error code (see errno).
 */
int keystore_ecc_encrypt(const struct keystore_ecc_public_key *key,
			 const void *data, unsigned int data_size,
			 void *output, unsigned int output_size);

/**
 * Decrypt a block of data using ECC (ECIES).
 *
 * @param key Private key pointer (raw data).
 * @param key_size Pricate key size in bytes.
 * @param data Block of data to be decrypted.
 * @param data_size Size of data in bytes.
 * @param output Block of data for the result.
 * @param output_size Size of output data block.
 *
 * @return The number of bytes written into output block if OK or negative error
 * code (see errno).
 */
int keystore_ecc_decrypt(const uint32_t *key,
			 const void *data, unsigned int data_size,
			 void *output, unsigned int output_size);

/**
 * Perform data sign using ecc Public key.
 *
 * @param key Public ECC key.
 * @param key_size Public ECC key size.
 * @param data Data from which hash sum will be counted.
 * @param data Data size.
 *
 * @return signature size if OK or negative error code (see errno).
 */
int keystore_ecc_sign(const void *key,
		      const void *data, unsigned int data_size,
		      struct keystore_ecc_signature *sig);

/**
 * Perform signature verification using ecc public key.
 *
 * @param signature Private ECC key.
 * @param signature_size Private ECC key size.
 * @param key public ECC key to decrypt hash sum from signature file.
 * @param key key size.
 * @param data Data from which hash sum will be counted.
 * @param data Data size.
 *
 * @return 0 if OK or negative error code (see errno).
 */
int keystore_ecc_verify(const struct keystore_ecc_public_key *key,
			const void *data, unsigned int data_size,
			const struct keystore_ecc_signature *sig);

#endif /* _KEYSTORE_ECC_H_ */
