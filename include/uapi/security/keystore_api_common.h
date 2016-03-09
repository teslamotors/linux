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

/**
 * DOC: Introduction
 *
 * Common constants and structures common to both user- and kernel-space
 * clients are listed here.
 *
 */

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

/**
 * KEYSTORE_MAJOR - "/dev/keystore" char device major number
 */
#define KEYSTORE_MAJOR               40

/**
 * KEYSTORE_CLIENT_TICKET_SIZE - client_ticket size in bytes
 */
#define KEYSTORE_CLIENT_TICKET_SIZE   8

/**
 * KEYSTORE_MAX_IV_SIZE - Maximum size of the Initialization Vector
 */
#define KEYSTORE_MAX_IV_SIZE         16

/**
 * RSA_SIGNATURE_BYTE_SIZE - Expected RSA signature size.
 *
 * Used when verifying the signature of backup keys
 */
#define RSA_SIGNATURE_BYTE_SIZE	    256

/**
 * KEYSTORE_BACKUP_SIZE - Size of Encrypted backup
 *
 * The backup operation returns an ECC encrypted blob.
 * The total size of this blob is given below.
 */
#define KEYSTORE_BACKUP_SIZE        249

/**
 * KEYSTORE_ECC_DIGITS - Number of ECC digits used to calculate ECC key sizes
 */
#define KEYSTORE_ECC_DIGITS          17

/**
 * REENCRYPTED_BACKUP_SIZE - Size of the backup after
 * re-encryption with the migration key
 */
#define REENCRYPTED_BACKUP_SIZE       89

/**
 * KEYSTORE_MKEY_NONCE_SIZE - Size of the nonce used to create
 * the migration key
 */
#define KEYSTORE_MKEY_NONCE_SIZE      32

/**
 * KEYSTORE_MKEY_SIZE - Size of the migration key
 */
#define KEYSTORE_MKEY_SIZE            32

/**
 * KEYSTORE_EMKEY_SIZE - Size of the migration key encrypted
 * with an ECC public key
 */
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

/**
 * struct keystore_ecc_signature - ECDSA signature
 * @r: r component of the signature
 * @s: s component of the signature
 *
 * Holds and Eliptic Curve Digital Signature Algorithm signature.
 * The meaning of the (@r, @s) components can be found in FIPS-186-4.
 */
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
 * @ALGOSPEC_AES_CCM: AES_CCM Algorithm (128/256 bit depending on key length)
 * @ALGOSPEC_AES_GCM: AES_GCM Algorithm (128/256 bit depending on key length)
 */
enum keystore_algo_spec {
	ALGOSPEC_INVALID = 0,
	ALGOSPEC_AES_CCM = 1,
	ALGOSPEC_AES_GCM = 2
};

#endif /* _KEYSTORE_API_COMMON_H_ */
