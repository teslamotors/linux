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
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/string.h>

#include "keystore_debug.h"
#include "keystore_rand.h"
#include "keystore_mac.h"
#include "ecc.h"
#include "ecies.h"
#include "utils.h"


/**
 * Perform data sign
 *
 * @param key Private ECC key.
 * @param data Data from which hash sum will be counted.
 * @param data_size Data size.
 *
 * @return signature size if OK or negative error code (see errno).
 */
int keystore_ecc_sign(const void *key,
		      const void *data, unsigned int data_size,
		      struct keystore_ecc_signature *sig)
{
	int res;
	uint32_t p_hash[NUM_ECC_DIGITS];
	uint32_t p_random[NUM_ECC_DIGITS];

	if (!key || !data || !sig)
		return -EFAULT;

	memset(p_hash, 0, sizeof(p_hash));
	memset(p_random, 0, sizeof(p_random));

	/* calculate ECC key digest */
	res = keystore_sha256_block(data, data_size, p_hash,
				    sizeof(p_hash));
	if (res < 0) {
		ks_err(KBUILD_MODNAME ": keystore_sha256_block() error %d in %s\n",
		       res, __func__);
		return res;
	}

	keystore_hexdump("Sign: p_hash", p_hash, sizeof(p_hash));
	keystore_hexdump("Sign: Private Key", key,
			 NUM_ECC_DIGITS * sizeof(uint32_t));

	res = keystore_get_rdrand((uint8_t *)p_random,
				  sizeof(p_random));
	if (res != 0) {
		ks_err(KBUILD_MODNAME ": Random number generator error %d\n",
				res);
		return res;
	}

	/*hmm, ecc lib returns 0 on error! */
	res = ecdsa_sign(sig->r, sig->s, (uint32_t *)key, p_random, p_hash);
	if (res == 0) {
		ks_err(KBUILD_MODNAME ": ecdsa_sign error %d\n",
				res);
		return -EKEYREJECTED;
	}

	keystore_hexdump("Sign: r", sig->r, NUM_ECC_DIGITS * sizeof(uint32_t));
	keystore_hexdump("Sign: s", sig->s, NUM_ECC_DIGITS * sizeof(uint32_t));

	return 0;
}

/**
 * Perform data signature verification
 *
 * @param key Public ECC key.
 * @param data Data from which hash sum will be counted.
 * @param data_size Data size.
 * @param r,s Signature to be verified.
 *
 * @return 0 size if OK or negative error code (see errno).
 */

int keystore_ecc_verify(const struct keystore_ecc_public_key *key,
			const void *data, unsigned int data_size,
			const struct keystore_ecc_signature *sig)
{
	int res;
	uint32_t p_hash[NUM_ECC_DIGITS];

	if (!key || !data || !sig)
		return -EFAULT;

	memset(p_hash, 0, sizeof(p_hash));

	/* calculate ECC key digest */
	res = keystore_sha256_block(data, data_size, p_hash,
					sizeof(p_hash));
	if (res < 0) {
		ks_err(KBUILD_MODNAME ": keystore_sha256_block() error %d in %s\n",
		       res, __func__);
		return res;
	}

	keystore_hexdump("Verify: Public-key:", key, sizeof(EccPoint));
	keystore_hexdump("Verify: p_hash", p_hash, sizeof(p_hash));

	keystore_hexdump("Sig: r", sig->r, NUM_ECC_DIGITS * sizeof(uint32_t));
	keystore_hexdump("Sig: s", sig->s, NUM_ECC_DIGITS * sizeof(uint32_t));

	/*hmm, ecc lib returns 0 on error! */
	if ((ecdsa_verify((const EccPoint *)key,
			  p_hash, sig->r, sig->s)) == 0) {
		ks_err(KBUILD_MODNAME ": %s - ecdsa_verify error\n",
				__func__);
		return -EKEYREJECTED;
	}

	return 0;
}

