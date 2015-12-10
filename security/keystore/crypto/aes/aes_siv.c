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

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/string.h>

#include "keystore_constants.h"
#include "keystore_aes.h"
#include "keystore_mac.h"
#include "keystore_debug.h"

#define BYTES_TO_DWORDS(x)  (((x) + sizeof(uint32_t) - 1) / sizeof(uint32_t))

static const uint32_t s2v_zero[] = {0, 0, 0, 0};  /* LSB first */
static const uint32_t s2v_one[] = {1, 0, 0, 0};   /* LSB first */

struct aes_siv_string {
	const void *data;
	unsigned int size;
};

/**
 * Given a 128-bit binary string, shift the string left by one bit and XOR the
 * result with 0x00...87 if the bit shifted off was one. This is the dbl
 * function of RFC 5297.
 */
void aes_siv_dbl(void *ptr)
{
	uint8_t *p = (uint8_t *) ptr;
	uint8_t msb = p[0];
	unsigned int i;

	/* shift left AES_BLOCK_SIZE bytes, take care about carry bit */
	for (i = 0; i < AES_BLOCK_SIZE - 1; i++, p++)
		*p = (p[0] << 1) | (p[1] >> 7);
	*p <<= 1;

	/* if MS bit was shifted out, XOR LS byte with 0x87 */
	if (msb & 0x80)
		*p ^= 0x87;
}

/**
 * pad(X)
 * indicates padding of string X, len(X) < 128, out to 128 bits by
 * the concatenation of a single bit of 1 followed by as many 0 bits
 * as are necessary.
 */
void aes_siv_pad(const void *x, unsigned int x_size, void *output,
		unsigned int output_size)
{
	uint8_t *out = (uint8_t *) output;

	memcpy(out, x, x_size);
	if (x_size < output_size) {
		out[x_size] = 0x80;
		memset(out + x_size + 1, 0, output_size - x_size - 1);
	}
}

/**
 * A xorend B
 * where len(A) >= len(B), means xoring a string B onto the end of
 * string A -- i.e., leftmost(A, len(A)-len(B)) || (rightmost(A, len(B)) xor B).
 */
void aes_siv_xorend(const void *a_ptr, unsigned int a_size, const void *b_ptr,
	       unsigned int b_size, void *output)
{
	const uint8_t *a = (const uint8_t *) a_ptr;
	const uint8_t *b = (const uint8_t *) b_ptr;
	uint8_t *o = (uint8_t *) output;
	unsigned int diff = a_size - b_size;

	/* XOR b_size bytes starting from LSB */
	while (b_size--)
		*o++ = *a++ ^ *b++;

	/* copy the rest from A */
	memcpy(o, a, diff);
}

/**
 * A xor B
 * where len = 16 bytes
 */
void aes_siv_xor(const void *a, const void *b, void *output)
{
	const uint32_t *p1 = (const uint32_t *) a;
	const uint32_t *p2 = (const uint32_t *) b;
	uint32_t *o = (uint32_t *) output;

	o[0] = p1[0] ^ p2[0];
	o[1] = p1[1] ^ p2[1];
	o[2] = p1[2] ^ p2[2];
	o[3] = p1[3] ^ p2[3];
}

/**
 * Run the S2V "string to vector" function of RFC 5297 using the input key
 * and string vector.
 */
int aes_siv_s2v(const void *key, unsigned int key_size,
		const struct aes_siv_string *strings, unsigned int num_strings,
		void *output, unsigned int output_size)
{
	uint32_t cmac[BYTES_TO_DWORDS(AES_BLOCK_SIZE)];
	uint32_t d[BYTES_TO_DWORDS(AES_BLOCK_SIZE)];
	struct aes_siv_string last_string;
	unsigned int i;
	int res;

	if (!key || !key_size || !output || !output_size ||
			(!strings && (num_strings > 0)))
		return -EINVAL;

	if (!num_strings) {
		/* return V = AES-CMAC(K, <one>) */
		res = keystore_calc_mac("cmac(aes)", key, key_size,
				(const char *) s2v_one, sizeof(s2v_one),
				output, output_size);
		if (res)
			ks_err(KBUILD_MODNAME ": keystore_calc_mac returned %d\n",
			       res);
		return res;
	}

	/* D = AES-CMAC(K, <zero>) */
	res = keystore_calc_mac("cmac(aes)", key, key_size,
			(const char *) s2v_zero, sizeof(s2v_zero),
			(char *) d, sizeof(d));
	if (res) {
		ks_err(KBUILD_MODNAME ": keystore_calc_mac returned %d\n", res);
		return res;
	}

	/* Handle all strings but the last */
	for (i = 0; i < num_strings - 1; i++) {
		res = keystore_calc_mac("cmac(aes)", key, key_size,
				strings[i].data, strings[i].size,
				(char *) cmac, sizeof(cmac));
		if (res) {
			ks_err(KBUILD_MODNAME
					": keystore_calc_mac returned %d\n",
					res);
			return res;
		}

		/* D = dbl(D) ... */
		aes_siv_dbl(d);

		/* D = ... xor AES-CMAC(K, Si) */
		aes_siv_xor(d, cmac, d);
	}

	/* Handle the last string */
	last_string = strings[num_strings - 1];
	if (last_string.size >= AES_BLOCK_SIZE) {
		uint32_t t[BYTES_TO_DWORDS(last_string.size)];

		/* T = Sn ... */
		memcpy(t, last_string.data, last_string.size);

		/* T = ... xorend D */
		aes_siv_xor(t, d, t);

		/* return V = AES-CMAC(K, T) */
		res = keystore_calc_mac("cmac(aes)", key, key_size,
			       (const char *) t, last_string.size,
			       output, output_size);
	} else {
		uint32_t t[BYTES_TO_DWORDS(AES_BLOCK_SIZE)];

		/* T = dbl(D) ... */
		aes_siv_dbl(d);

		/* T = ... xor pad(Sn) */
		aes_siv_pad(last_string.data, last_string.size, t, sizeof(t));

		aes_siv_xor(t, d, t);

		/* return V = AES-CMAC(K, T) */
		res = keystore_calc_mac("cmac(aes)", key, key_size,
				(const char *) t, sizeof(t), output,
				output_size);
	}

	if (res)
		ks_err(KBUILD_MODNAME
				": keystore_calc_mac returned %d\n", res);
	return res;
}

/**
 * Encrypt a block of data using AES-SIV.
 *
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
int aes_siv_encrypt(const void *key, unsigned int key_size,
		const void *input, unsigned int isize,
		const void *associated_data, unsigned int asize,
		void *output, unsigned int osize)
{
	/* K1 = leftmost(K, len(K)/2) */
	const unsigned int k1_size = key_size / 2;
	const void *k1 = key;

	/* K2 = rightmost(K, len(K)/2) */
	const unsigned int k2_size = k1_size;
	const void *k2 = (const void *)(((const unsigned char *) key)
			+ k1_size);

	/* place for AES-CTR output */
	void *ctr_output = (void *)(((unsigned char *) output)
			+ AES_BLOCK_SIZE);

	unsigned char q[AES_BLOCK_SIZE];
	struct aes_siv_string strings[2];
	unsigned int strings_used;
	int res;

	if (!key || !input || !isize || (!associated_data && (asize > 0)) ||
			!output || (osize != isize + AES_BLOCK_SIZE))
		return -EINVAL;

	/* accept key sizes: 256, 384, 512-bit */
	if ((key_size != 32) && (key_size != 48) && (key_size != 64))
		return -EINVAL;

	/* prepare strings table for S2V */
	if (asize > 0) {
		strings[0].data = associated_data;
		strings[0].size = asize;
		strings[1].data = input;
		strings[1].size = isize;
		strings_used = 2;
	} else {
		strings[0].data = input;
		strings[0].size = isize;
		strings_used = 1;
	}

	/* V = S2V(K1, AD1, ..., ADn, P) */
	res = aes_siv_s2v(k1, k1_size, strings, strings_used, output,
			AES_BLOCK_SIZE);
	if (res)
		return res;

	/* The 31st and 63rd bit (where the rightmost bit is the 0th) of the */
	/* counter are zeroed out just prior to being used by CTR for */
	/* optimization purposes */

	/* Q = V bitand (1^64 || 0^1 || 1^31 || 0^1 || 1^31) */
	memcpy(q, output, AES_BLOCK_SIZE);
	q[AES_BLOCK_SIZE - 4] &= 0x7f;
	q[AES_BLOCK_SIZE - 8] &= 0x7f;

	/* encrypt data using AES-CTR */
	res = keystore_aes_ctr_crypt(1, k2, k2_size, q, AES_BLOCK_SIZE,
			input, ctr_output, isize);
	if (res) {
		ks_err(KBUILD_MODNAME ": keystore_aes_ctr_crypt returned %d\n",
				res);
		return res;
	}

	return 0;
}

/**
 * Decrypt a block of data using AES-SIV.
 *
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
int aes_siv_decrypt(const void *key, unsigned int key_size,
		const void *input, unsigned int isize,
		const void *associated_data, unsigned int asize,
		void *output, unsigned int osize)
{
	/* K1 = leftmost(K, len(K)/2) */
	const unsigned int k1_size = key_size / 2;
	const void *k1 = key;

	/* K2 = rightmost(K, len(K)/2) */
	const unsigned int k2_size = k1_size;
	const void *k2 = (const void *)(((const unsigned char *) key) +
			k1_size);

	/* C = rightmost(Z, len(Z)-128) */
	const void *c = (const void *)(((const unsigned char *) input) +
			AES_BLOCK_SIZE);

	unsigned char q[AES_BLOCK_SIZE];
	struct aes_siv_string strings[2];
	unsigned int strings_used;
	int res;

	if (!key || !input || (!associated_data && (asize > 0)) ||
		!output || !osize || (isize != osize + AES_BLOCK_SIZE))
		return -EINVAL;

	/* accept key sizes: 256, 384, 512-bit */
	if ((key_size != 32) && (key_size != 48) && (key_size != 64))
		return -EINVAL;

	/* The 31st and 63rd bit (where the rightmost bit is the 0th) of the */
	/* counter are zeroed out just prior to being used by CTR for */
	/* optimization purposes */

	/* Q = V bitand (1^64 || 0^1 || 1^31 || 0^1 || 1^31) */
	memcpy(q, input, AES_BLOCK_SIZE);
	q[AES_BLOCK_SIZE - 4] &= 0x7f;
	q[AES_BLOCK_SIZE - 8] &= 0x7f;

	/* decrypt data using AES-CTR */
	res = keystore_aes_ctr_crypt(0, k2, k2_size, q, AES_BLOCK_SIZE,
			c, output, osize);
	if (res) {
		ks_err(KBUILD_MODNAME
				": keystore_aes_ctr_crypt returned %d\n", res);
		return res;
	}

	/* prepare strings table for S2V */
	if (asize > 0) {
		strings[0].data = associated_data;
		strings[0].size = asize;
		strings[1].data = output;
		strings[1].size = osize;
		strings_used = 2;
	} else {
		strings[0].data = output;
		strings[0].size = osize;
		strings_used = 1;
	}

	/* T = S2V(K1, AD1, ..., ADn, P) */
	res = aes_siv_s2v(k1, k1_size, strings, strings_used, q,
			AES_BLOCK_SIZE);
	if (res)
		return res;

	/* if T = V then */
	/*   return P */
	/* else */
	/*   return FAIL */
	/* fi */
	return memcmp(q, input, AES_BLOCK_SIZE) ? -EKEYREJECTED : 0;
}

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
		const void *associated_data, unsigned int asize, void *output,
		unsigned int osize)
{
#ifdef DEBUG_TRACE
	ks_info(KBUILD_MODNAME ": keystore_aes_siv_crypt enc=%d klen=%u ilen=%u alen=%u olen=%u\n",
			enc, key_size, isize, asize, osize);
#endif

	return enc ?
		aes_siv_encrypt(key, key_size, input, isize, associated_data,
				asize, output, osize) :
		aes_siv_decrypt(key, key_size, input, isize, associated_data,
				asize, output, osize);
}

/* end of file */
