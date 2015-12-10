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

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <crypto/sha.h>

#include <security/keystore_api_common.h>
#include "keystore_constants.h"
#include "keystore_ecc.h"
#include "keystore_mac.h"
#include "keystore_debug.h"

#include "ecies.h"
#include "utils.h"

/**
 * Generate ECC private and public key from a seed
 *
 * @param public_key public ECC key.
 * @param private_key private ECC key.
 * @param seed_in a pointer to the seed used to derive the keys
 *
 * @return 0 OK or negative error code (see errno), ENODATA if SEED cannot
 *         be used to generate private key or EKEYREJECTED if public key
 *         cannot pass verification.
 */
int keystore_ecc_gen_keys(const uint8_t *seed_in, size_t seed_in_size,
			  struct ias_keystore_ecc_keypair *key_pair)
{
	int res = 0;
	int i, j;

	uint32_t seed[KEYSTORE_ECC_DIGITS];
	uint8_t hashed_seed[seed_in_size * 2];
	int limit = KEYSTORE_ECC_DIGITS > seed_in_size / 2 ?
		seed_in_size / 2 : KEYSTORE_ECC_DIGITS;

	FUNC_BEGIN;

	if (!key_pair || !seed_in)
		return -EFAULT;

	memset(seed, 0, sizeof(seed));
	memset(hashed_seed, 0, sizeof(hashed_seed));

	res = keystore_calc_mac("hmac(sha256)",
				seed_in, seed_in_size,
				seed_in, seed_in_size,
				hashed_seed, seed_in_size);
	if (res != 0) {
		ks_err(KBUILD_MODNAME ": %s: keystore_hmac_sha256 error %d\n",
				__func__, res);
		return res;
	}

	memcpy(hashed_seed + seed_in_size, hashed_seed, seed_in_size);
	/*rewrite seed to new buffer and padding unused space */
	for (i = 0, j = 0; i < limit; i++, j += 4) {
		seed[i] = (hashed_seed[j] << 24 |
				hashed_seed[j+1] << 16 |
				hashed_seed[j+2] << 8 |
				hashed_seed[j+3]);
	}

	keystore_hexdump("hashed_seed", hashed_seed, sizeof(hashed_seed));

	res = ecc_make_key((EccPoint *)&key_pair->public_key,
			   key_pair->private_key, seed);
	if (res == 1) {
		res = ecc_valid_public_key((EccPoint *)&key_pair->public_key);
		if (res != 1) {
			ks_err(KBUILD_MODNAME ": %s: Error Failed to validate public ECC key\n",
					__func__);
			res = -EKEYREJECTED;
		}
	} else {
		ks_err(KBUILD_MODNAME ": %s: Error This SEED is in proper to generate private ECC key:\n",
		       __func__);
		keystore_hexdump("seed", seed, sizeof(seed));
		res = -ENODATA;
	}

	if (res == 1) /* ECC lib returns 1 as success */
		res = 0;

	FUNC_RES(res);
	return res;
}
