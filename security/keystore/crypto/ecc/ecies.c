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
 * Hybrid Key Transport via Elliptic Curve Integrated Encryption Scheme (ECIES)
 *
 * Minimal implementation based on SECG SEC1 ECIES v2
 * © Steffen Schulz, Intel Corporation, 2014
 ******************************************************************************/
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>

#include "keystore_mac.h"
#include "keystore_debug.h"
#include "ecies.h"
#include "kdf_x963.h"
#include "utils.h"
#include "keystore_rand.h"

int ecies_encrypt(ecc_cipher_t *cipher, EccPoint *pubKey,
		  const uint8_t *msg, uint32_t msglen)
{
	EccPoint R;
	uint32_t k[NUM_ECC_DIGITS];
	uint32_t z[NUM_ECC_DIGITS];
	uint32_t NUM_ECC_BYTES = NUM_ECC_DIGITS * sizeof(uint32_t);

#ifdef USE_CRYPTOAPI
	int32_t ret = 0;
#else
	HmacState_t ctx = hmac_factory(HASH_SHA256);

	if (!ctx)
		return -1;
#endif

	/* sanity check provided inputs */
	if (!cipher || !pubKey || !msg || !msglen ||
			cipher->dhkeylen < 2 * NUM_ECC_BYTES + 1
			/*|| cipher->maclen < ctx->tag_size*/)
		return -1;

	/* Generate random ECDH keypair and session key z */
	keystore_get_rdrand((uint8_t *) k, NUM_ECC_BYTES);
	if (!ecc_make_key(&R, k, k)) /* R=k^2? Is this correct? */
		return -2;

	if (!ecdh_shared_secret(z, pubKey, k))
		return -2;

	{
		/* Generate encryption and mac keys k1,k2 */
		uint32_t keylen = msglen; /* XOR encryption */
#ifdef USE_CRYPTOAPI
		uint32_t maclen = cipher->maclen; /* HMAC-SHA2-256 */
#else
		uint32_t maclen = ctx->key_size; /* HMAC-SHA2-256 */
#endif
		uint8_t k1[keylen + maclen];
		uint8_t *k2 = k1 + keylen;
		uint32_t i;

		/* Convert z and R to octet strings (R needs extra encoding) */
		uint8_t Z[NUM_ECC_BYTES];
		uint8_t *shared1 = cipher->dhkey; /* octet-string of `R' */

		native2string(Z, z, NUM_ECC_DIGITS);

		shared1[0] = 0x04; /* = no point compression */
		native2string(shared1 + 1, R.x, NUM_ECC_DIGITS);
		native2string(shared1 + 1 + NUM_ECC_BYTES, R.y, NUM_ECC_DIGITS);

#ifdef ECIES_USE_R
		kdf_x963(k1, keylen+maclen,
				Z, NUM_ECC_BYTES,
				shared1, 2*NUM_ECC_BYTES+1);
#else
		kdf_x963(k1, keylen + maclen, Z, NUM_ECC_BYTES, 0, 0);
#endif

		/* store curve type and used length */
		cipher->curve = ECC_CURVE;
		cipher->dhkeylen = 2 * NUM_ECC_BYTES + 1;

		/*
		 * Encrypt-then-MAC
		 *
		 * This part can implement many algorithms
		 */

		/* XOR encryption of msg */
		if (cipher->textlen < msglen || msglen != keylen)
			return -1;
		for (i = 0; i < keylen; i++)
			cipher->text[i] = k1[i] ^ msg[i];
		cipher->textlen = keylen;

		/* HMAC of cipher->text */
#ifdef USE_CRYPTOAPI
		ret = keystore_calc_mac("hmac(sha256)",
					(const char *)k2,
					(size_t)maclen,
					(const char *)cipher->text,
					(size_t)cipher->textlen,
					(char *)cipher->mac,
					(size_t)maclen);
		if (ret != 0) {
			ks_err(KBUILD_MODNAME "Failed - hmac(sha256), ret: %d\n",
			       ret);
			return -1;
		}
#else
		hmac_init(ctx, k2, maclen);
		hmac_update(ctx, cipher->text, cipher->textlen);
		hmac_final(cipher->mac, ctx);
		cipher->maclen = ctx->tag_size;
#endif
	}

	return 0;
}

int ecies_decrypt(uint8_t *msg, uint32_t *msglen,
		  const uint32_t *prvkey, ecc_cipher_t *cipher)
{
	EccPoint R;
	uint32_t z[NUM_ECC_DIGITS];
	uint32_t NUM_ECC_BYTES = NUM_ECC_DIGITS * sizeof(uint32_t);
#ifdef USE_CRYPTOAPI
	int32_t ret = 0;
#else
	HmacState_t ctx = hmac_factory(HASH_SHA256);

	if (!ctx)
		return -1;
#endif

	/* sanity checks and validate we're using the same curve and params */
	if (!msg || !prvkey || !cipher ||
	    (cipher->dhkeylen < 2 * NUM_ECC_BYTES + 1) ||
	    /*(cipher->maclen < ctx->tag_size) ||*/
	    (cipher->curve != ECC_CURVE) || (cipher->dhkey[0] != 4))
		return -1;

	/* recover eph. pubkey */
	string2native(R.x, cipher->dhkey + 1, NUM_ECC_DIGITS);
	string2native(R.y, cipher->dhkey + 1 + NUM_ECC_BYTES, NUM_ECC_DIGITS);

	/* NOTE: Function assumes curve parameter a=-3, as in NIST curves */
	if (!ecc_valid_public_key(&R)) /* TODO this should also check n*Q=0? */
		return -2;

	/* compute shared secret */
	if (!ecdh_shared_secret(z, &R, prvkey))
		return -2;

	{
		/* Generate encryption and mac keys k1,k2 */
		uint32_t keylen = cipher->textlen; /* XOR encryption */
#ifdef USE_CRYPTOAPI
		uint32_t maclen = cipher->maclen; /* HMAC-SHA2-256 */
		uint8_t tag[cipher->maclen];
#else
		uint32_t maclen = ctx->key_size; /* HMAC-SHA2-256 */
		uint8_t tag[ctx->tag_size];
#endif
		uint8_t k1[keylen + maclen];
		uint8_t *k2 = k1 + keylen;
		uint32_t i;

		/* Need secret z and sharedInfo R as octet strings */
		uint8_t Z[NUM_ECC_BYTES];

		native2string(Z, z, NUM_ECC_DIGITS);

#ifdef ECIES_USE_R
		kdf_x963(k1, keylen+maclen,
				Z, NUM_ECC_BYTES,
				cipher->dhkey, cipher->dhkeylen);
#else
		kdf_x963(k1, keylen + maclen, Z, NUM_ECC_BYTES, 0, 0);
#endif

		/*
		 * Validate MAC, then decrypt
		 *
		 * This part can implement many algorithms
		 */

		/* recompute and compare ciphertext HMAC */
#ifdef USE_CRYPTOAPI
		ret = keystore_calc_mac("hmac(sha256)",
					(const char *)k2, (size_t)
					maclen,
					(const char *)cipher->text,
					cipher->textlen,
					(char *)tag,
					cipher->maclen);
		if (ret != 0) {
			ks_err(KBUILD_MODNAME "Failed - hmac(sha256), ret: %d\n",
					ret);
			return -1;
		}
#else
		hmac_init(ctx, k2, maclen);
		hmac_update(ctx, cipher->text, cipher->textlen);
		hmac_final(tag, ctx);
#endif

		for (i = 0; i < cipher->maclen; i++)
			if (tag[i] != cipher->mac[i])
				return -3; /* bad MAC! */

		/* XOR encryption of msg */
		if (cipher->textlen > *msglen || cipher->textlen != keylen)
			return -1;
		for (i = 0; i < keylen; i++)
			msg[i] = k1[i] ^ cipher->text[i];
		*msglen = keylen;
	}

	return 0;
}
