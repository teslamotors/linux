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
#include <linux/crypto.h>
#include <linux/err.h>
#include <linux/scatterlist.h>
#include "kdf_x963.h"

#include "utils.h"
#include "keystore_debug.h"

/* Parameters of hash() */
#if defined(X963_SHA1)
uint64_t hashmaxlen = -1; /* max input size */
uint32_t hashlen = 20; /* output size */
#elif defined(X963_SHA256)
uint64_t hashmaxlen = -1; /* max input size (actually 2^128-1) */
uint32_t hashlen = 32;/* output size */
#elif defined(USE_CRYPTOAPI)
uint64_t hashmaxlen = -1; /* max input size */
uint32_t hashlen = 20; /* output size */
#else
#pragma message("Warning: Unknown hash function!")
#endif

int kdf_x963(uint8_t *key, uint32_t keylen, uint8_t *rand, uint32_t randlen,
	     uint8_t *shared, uint32_t sharedlen)
{
#if defined(X963_SHA1)
	sha1_context ctx;
#elif defined(X963_SHA256)
	struct sha256_state_struct ctx;
#elif defined(USE_CRYPTOAPI)
	struct scatterlist sg[3];
	struct crypto_hash *tfm;
	struct hash_desc desc;
#endif
	uint32_t cntr;
	int extrabytes;

	/* simplified length checks to avoid overflows & bignum */
	uint32_t MAXINT = -1;

	if ((randlen + sharedlen + 4) >= hashmaxlen)
		return -1;

	if (keylen >= MAXINT)
		return -1;

#if defined(USE_CRYPTOAPI)
	tfm = crypto_alloc_hash("sha1",
				0 /* CRYPTO_ALG_INTERNAL */,
				/* CRYPTO_ALG_INTERNAL |*/  CRYPTO_ALG_ASYNC);
	if (IS_ERR(tfm)) {
		ks_err("ecc: tfm allocation failed\n");
		return 0;
	}
	desc.tfm = tfm;
	desc.flags = 0;
#endif

	for (cntr = 1; cntr <= keylen / hashlen; cntr++) {
		uint8_t cstring[sizeof(uint32_t)];

		native2string(cstring, &cntr, 1);
#if defined(X963_SHA1)
		sha1_starts(&ctx);
		sha1_update(&ctx, rand, randlen);
		sha1_update(&ctx, cstring, sizeof(uint32_t));
		sha1_update(&ctx, shared, sharedlen);
		sha1_finish(&ctx, key + (cntr - 1) * hashlen);
#elif defined(X963_SHA256)
		sha256_init(&ctx);
		sha256_update(&ctx, rand, randlen);
		sha256_update(&ctx, cstring, sizeof(uint32_t));
		sha256_update(&ctx, shared, sharedlen);
		sha256_final(key+(cntr-1)*hashlen, &ctx);
#elif defined(USE_CRYPTOAPI)
		crypto_hash_init(&desc);
		sg_init_table(sg, 3);
		sg_set_buf(sg+0, rand, randlen);
		sg_set_buf(sg+1, cstring, sizeof(uint32_t));
		sg_set_buf(sg+2, shared, sharedlen);

		crypto_hash_update(&desc, sg+0, randlen);
		crypto_hash_update(&desc, sg+1, sizeof(uint32_t));
		crypto_hash_update(&desc, sg+2, sharedlen);
		crypto_hash_final(&desc, key+(cntr-1)*hashlen);

#else
#pragma message("Warning: Unknown hash function!")
#endif
	}

	extrabytes = keylen % hashlen;
	if (extrabytes) {
		int i;
		uint8_t tmp[hashlen];
		uint8_t cstring[sizeof(uint32_t)];

		native2string(cstring, &cntr, 1);
#if defined(X963_SHA1)
		sha1_starts(&ctx);
		sha1_update(&ctx, rand, randlen);
		sha1_update(&ctx, cstring, sizeof(uint32_t));
		sha1_update(&ctx, shared, sharedlen);
		sha1_finish(&ctx, tmp);
#elif defined(X963_SHA256)
		sha256_init(&ctx);
		sha256_update(&ctx, rand, randlen);
		sha256_update(&ctx, cstring, sizeof(uint32_t));
		sha256_update(&ctx, shared, sharedlen);
		sha256_final(tmp, &ctx);
#elif defined(USE_CRYPTOAPI)
		crypto_hash_init(&desc);
		sg_init_table(sg, 3);
		sg_set_buf(sg+0, rand, randlen);
		sg_set_buf(sg+1, cstring, sizeof(uint32_t));
		sg_set_buf(sg+2, shared, sharedlen);

		crypto_hash_update(&desc, sg+0, randlen);
		crypto_hash_update(&desc, sg+1, sizeof(uint32_t));
		crypto_hash_update(&desc, sg+2, sharedlen);
		crypto_hash_final(&desc, tmp);
#else
#pragma message("Warning: Unknown hash function!")
#endif
		/* fill missing bytes of key[] */
		for (i = 0; i < extrabytes; i++)
			key[(cntr - 1) * hashlen + i] = tmp[i];
	}

#if defined(USE_CRYPTOAPI)
	crypto_free_hash(tfm);
#endif


	return 0;
}
