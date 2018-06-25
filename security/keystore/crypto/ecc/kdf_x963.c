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

/******************************************************************************
 * ANSI X9.63 Key Derivation Function
 *
 * Quick'n'Dirty implementation supporting SHA1 and SHA256
 *
 * © Steffen Schulz, Intel Corporation, 2014
 ******************************************************************************/
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>

#include <crypto/hash.h>
#include <linux/err.h>
#include <linux/scatterlist.h>
#include "kdf_x963.h"

#include "utils.h"
#include "keystore_debug.h"

uint64_t hashmaxlen = -1; /* max input size */
uint32_t hashlen = 20; /* output size */

int kdf_x963(uint8_t *key, uint32_t keylen, uint8_t *rand, uint32_t randlen,
	     uint8_t *shared, uint32_t sharedlen)
{
	struct crypto_shash *alg;
	struct shash_desc *sdesc;
	int size;
	uint32_t cntr;
	int extrabytes;

	/* simplified length checks to avoid overflows & bignum */
	uint32_t MAXINT = -1;

	if (!key || !rand || (!shared && sharedlen))
		return -1;

	if ((randlen + sharedlen + 4) >= hashmaxlen)
		return -1;

	if (keylen >= MAXINT)
		return -1;

	alg = crypto_alloc_shash("sha1", CRYPTO_ALG_TYPE_SHASH, 0);
	if (IS_ERR(alg)) {
		ks_err("ecc: alg allocation failed\n");
		return 0;
	}

	size = sizeof(struct shash_desc) + crypto_shash_descsize(alg);
	sdesc = kmalloc(size, GFP_KERNEL);
	if (!sdesc) {
		crypto_free_shash(alg);
		ks_err("ecc: memory allocation error\n");
		return 0;
	}

	sdesc->tfm = alg;
	sdesc->flags = 0x0;

	for (cntr = 1; cntr <= keylen / hashlen; cntr++) {
		uint8_t cstring[sizeof(uint32_t)];

		native2string(cstring, &cntr, 1);

		crypto_shash_init(sdesc);
		crypto_shash_update(sdesc, rand, randlen);
		crypto_shash_update(sdesc, cstring, sizeof(uint32_t));
		crypto_shash_update(sdesc, shared, sharedlen);
		crypto_shash_final(sdesc, key+(cntr-1)*hashlen);
	}

	extrabytes = keylen % hashlen;
	if (extrabytes) {
		int i;
		uint8_t tmp[hashlen];
		uint8_t cstring[sizeof(uint32_t)];

		native2string(cstring, &cntr, 1);

		crypto_shash_update(sdesc, rand, randlen);
		crypto_shash_update(sdesc, cstring, sizeof(uint32_t));
		crypto_shash_update(sdesc, shared, sharedlen);
		crypto_shash_final(sdesc, tmp);

		/* fill missing bytes of key[] */
		for (i = 0; i < extrabytes; i++)
			key[(cntr - 1) * hashlen + i] = tmp[i];
	}

	crypto_free_shash(alg);
	kfree(sdesc);

	return 0;
}
