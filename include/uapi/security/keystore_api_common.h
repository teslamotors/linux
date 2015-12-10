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

#ifndef _KEYSTORE_API_COMMON_H_
#define _KEYSTORE_API_COMMON_H_

#include <linux/types.h>

/* Version numbers of the Keystore API
 * Follows the Apache versioning scheme
 *
 * Major versions represent large scale changes in the API.
 * Minor changes return API compatibility with older minor versions.
 * Patch changes are forwards and backwards compatible.
 *
 * Ensure that version numbers are updated if changes are made to
 * the API!
 */
#define KEYSTORE_VERSION_MAJOR 2
#define KEYSTORE_VERSION_MINOR 0
#define KEYSTORE_VERSION_PATCH 0

/* "/dev/keystore" char device major number */
#define KEYSTORE_MAJOR               40

/* client_ticket size in bytes for most API calls */
#define KEYSTORE_CLIENT_TICKET_SIZE   8

/* Maximum size of the Initialization vection for encrypt/decrypt */
#define KEYSTORE_MAX_IV_SIZE         16

/* Expected RSA signature size
 * Used when verifying the signature of backup keys
 */
#define RSA_SIGNATURE_BYTE_SIZE	    256

/* Size of Encrypted backup
 *
 * The backup operation returns an ECC encrypted blob.
 * The total size of this blob is given below.
 */
#define KEYSTORE_BACKUP_SIZE        249

/* Number of ECC digits used to calculate ECC key sizes */
#define KEYSTORE_ECC_DIGITS          17

/* ECC Public key size for backup operations */
#define KEYSTORE_ECC_PUB_KEY_SIZE   (sizeof(struct ias_keystore_ecc_public_key))

/* ECC Private key size used on the host for migration */
#define KEYSTORE_ECC_PRIV_KEY_SIZE  (sizeof(__u32) * KEYSTORE_ECC_DIGITS)

/* Size of the backup after re-encryption with the migration key */
#define REENCRYPTED_BACKUP_SIZE       89  /* size of re-encrypted backup data */

/* Size of the nonce used to create the migration key */
#define KEYSTORE_MKEY_NONCE_SIZE      32

/* Size of the migration key */
#define KEYSTORE_MKEY_SIZE            32

/* Size of the migration key encrypted with an ECC public key */
#define KEYSTORE_EMKEY_SIZE           217

/**
 * struct keystore_ecc_public_key_t - ECC Public Key Holder
 * @x: The x co-ordinate of the ECC key
 * @y: The y co-ordinate of the ECC key
 *
 * Represents an ECC public key
 */
struct keystore_ecc_public_key {
	__u32 x[KEYSTORE_ECC_DIGITS];
	__u32 y[KEYSTORE_ECC_DIGITS];
};

/**
 * struct ias_keystore_ecc_keypair - Public / private ECC key pair
 * @private_key: The ECC private key.
 * @public_key:  The ECC public key.
 */
struct ias_keystore_ecc_keypair {
	__u32 private_key[KEYSTORE_ECC_DIGITS];
	struct keystore_ecc_public_key public_key;
};

struct keystore_ecc_signature {
	__u32 r[KEYSTORE_ECC_DIGITS];
	__u32 s[KEYSTORE_ECC_DIGITS];
};

/**
 * enum keystore_seed_type - User/device seed type
 * @SEED_TYPE_DEVICE: The keys should be associated to the device.
 *                    SEED will only change if the device SEED is
 *                    compromised.
 * @SEED_TYPE_USER:   The keys should be associated to the user. The
 *                    SEED can be changed by the user if requested.
 */
enum keystore_seed_type {
	SEED_TYPE_DEVICE = 0,
	SEED_TYPE_USER   = 1
};

/**
 * enum keystore_key_spec - The key specification
 * @KEYSPEC_INVALID: Invalid keyspec
 * @KEYSPEC_LENGTH_128: 128-bit raw key (for AES)
 * @KEYSPEC_LENGTH_256: 256-bit raw key (for AES)
 */
enum keystore_key_spec {
	KEYSPEC_INVALID = 0,
	KEYSPEC_LENGTH_128,
	KEYSPEC_LENGTH_256
};

/**
 * enum keystore_algo_spec - The encryption algorithm specification
 * @ALGOSPEC_INVALID: Invalid Algospec
 * @ALGOSPEC_AES: AES Algorithm (128/256 bit depending on key length)
 */
enum keystore_algo_spec {
	ALGOSPEC_INVALID = 0,
	ALGOSPEC_AES
};

#endif /* _KEYSTORE_API_COMMON_H_ */
