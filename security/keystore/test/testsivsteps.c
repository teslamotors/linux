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
#include <linux/module.h>

#include "keystore_constants.h"

#include "keystore_aes.h"
#include "keystore_mac.h"
#include "keystore_debug.h"

struct aes_siv_string {
	const void *data;
	unsigned int size;
};

extern void aes_siv_dbl(void *ptr);
extern void aes_siv_pad(const void *x, unsigned int x_size,
			void *output, unsigned int output_size);
extern void aes_siv_xor(const void *a, const void *b, void *output);
extern int aes_siv_s2v(const void *key, unsigned int key_size,
		       const struct aes_siv_string *strings,
		       unsigned int num_strings,
		       void *output, unsigned int output_size);

/* From RFC 5297 (A.1. Deterministic Authenticated Encryption Example) */
static const uint8_t key[] = {0xff, 0xfe, 0xfd, 0xfc, 0xfb, 0xfa, 0xf9, 0xf8,
	0xf7, 0xf6, 0xf5, 0xf4, 0xf3, 0xf2, 0xf1, 0xf0,
	0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
	0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff};

static const uint8_t ad[] = {0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
	0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27};

static const uint8_t plaintext[] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
	0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee};

static const uint8_t zeros[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static const uint8_t cmac_zero[] = {0x0e, 0x04, 0xdf, 0xaf, 0xc1, 0xef, 0xbf,
	0x04, 0x01, 0x40, 0x58, 0x28, 0x59, 0xbf, 0x07, 0x3a};

static const uint8_t double1[] = {0x1c, 0x09, 0xbf, 0x5f, 0x83, 0xdf, 0x7e, 0x08
	, 0x02, 0x80, 0xb0, 0x50, 0xb3, 0x7e, 0x0e, 0x74};

static const uint8_t cmac_ad[] = {0xf1, 0xf9, 0x22, 0xb7, 0xf5, 0x19, 0x3c, 0xe6
	, 0x4f, 0xf8, 0x0c, 0xb4, 0x7d, 0x93, 0xf2, 0x3b};

static const uint8_t xor1[] = {0xed, 0xf0, 0x9d, 0xe8, 0x76, 0xc6, 0x42, 0xee,
	0x4d, 0x78, 0xbc, 0xe4, 0xce, 0xed, 0xfc, 0x4f};

static const uint8_t double2[] = {0xdb, 0xe1, 0x3b, 0xd0, 0xed, 0x8c, 0x85, 0xdc
	, 0x9a, 0xf1, 0x79, 0xc9, 0x9d, 0xdb, 0xf8, 0x19};

static const uint8_t pad[] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
	0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0x80, 0x00};

static const uint8_t xor2[] = {0xca, 0xc3, 0x08, 0x94, 0xb8, 0xea, 0xf2, 0x54,
	0x03, 0x5b, 0xc2, 0x05, 0x40, 0x35, 0x78, 0x19};

static const uint8_t cmac_final[] = {0x85, 0x63, 0x2d, 0x07, 0xc6, 0xe8, 0xf3,
	0x7f, 0x95, 0x0a, 0xcd, 0x32, 0x0a, 0x2e, 0xcc, 0x93};

static const uint8_t ctr_iv[] = {0x85, 0x63, 0x2d, 0x07, 0xc6, 0xe8, 0xf3, 0x7f,
	0x15, 0x0a, 0xcd, 0x32, 0x0a, 0x2e, 0xcc, 0x93};

static const uint8_t ctr_out[] = {0x40, 0xc0, 0x2b, 0x96, 0x90, 0xc4, 0xdc, 0x04
	, 0xda, 0xef, 0x7f, 0x6a, 0xfe, 0x5c};

/**
 * Test the AES_SIV helper functions.
 *
 * @return 0 if OK or negative error code (see errno).
 */
int keystore_test_aes_siv_steps(void)
{
	const unsigned int half_key_size = sizeof(key) / 2;
	const unsigned int output_size = AES_BLOCK_SIZE;
	uint32_t output[output_size / sizeof(uint32_t)];
	struct aes_siv_string strings[2];
	int res;

#ifdef DEBUG_TRACE
	ks_info(KBUILD_MODNAME ": keystore_test_aes_siv_steps\n");
#endif
	memset(output, 0, output_size);

	/* CMAC(zero) */
	res = keystore_calc_mac("cmac(aes)", key, half_key_size, zeros,
			sizeof(zeros), (char *) output, output_size);
	if (res) {
		ks_info(KBUILD_MODNAME
": keystore_test_aes_siv_steps: keystore_calc_mac returned %d\n", res);
		return res;
	}

#ifdef DEBUG_TRACE
	keystore_hexdump("KEY       ", key, sizeof(key));
	keystore_hexdump("AD        ", ad, sizeof(ad));
	keystore_hexdump("PLAINTEXT ", plaintext, sizeof(plaintext));
	keystore_hexdump("ZEROS     ", zeros, sizeof(zeros));
#endif

	if (show_and_compare("CMAC(zero)", output, "EXPECT    ", cmac_zero
				, output_size)) {
		return -1;
	}

	/* double() */
	aes_siv_dbl(output);
	if (show_and_compare("DOUBLE    ", output, "EXPECT    ", double1
				, output_size)) {
		return -1;
	}

	/* CMAC(ad) */
	res = keystore_calc_mac("cmac(aes)", key, half_key_size, ad, sizeof(ad),
			(char *) output, output_size);
	if (res) {
		ks_info(KBUILD_MODNAME
": keystore_test_aes_siv_steps: keystore_calc_mac returned %d\n", res);
		return res;
	}

	if (show_and_compare("CMAC(ad)  ", output, "EXPECT    ", cmac_ad,
				output_size)) {
		return -1;
	}

	/* xor */
	aes_siv_xor(double1, cmac_ad, output);
	if (show_and_compare("XOR       ", output, "EXPECT    ", xor1,
				output_size)) {
		return -1;
	}

	/* double() */
	aes_siv_dbl(output);
	if (show_and_compare("DOUBLE    ", output, "EXPECT    ", double2,
				output_size)) {
		return -1;
	}

	/* pad */
	aes_siv_pad(plaintext, sizeof(plaintext), output, output_size);
	if (show_and_compare("PAD       ", output, "EXPECT    ", pad,
				output_size)) {
		return -1;
	}

	/* xor */
	aes_siv_xor(double2, pad, output);
	if (show_and_compare("XOR       ", output, "EXPECT    ", xor2,
				output_size)) {
		return -1;
	}

	/* CMAC(final) */
	res = keystore_calc_mac("cmac(aes)", key, half_key_size, xor2,
			sizeof(xor2), (char *) output, output_size);
	if (res) {
		ks_info(KBUILD_MODNAME
": keystore_test_aes_siv_steps: keystore_calc_mac returned %d\n", res);
		return res;
	}

	if (show_and_compare("CMAC(finl)", output, "EXPECT    ", cmac_final,
				output_size)) {
		return -1;
	}

	/* now try the same steps but combined in aes_siv_s2v() call */
	memset(output, 0, output_size);
	strings[0].data = ad;
	strings[0].size = sizeof(ad);
	strings[1].data = plaintext;
	strings[1].size = sizeof(plaintext);

	res = aes_siv_s2v(key, half_key_size, strings, 2, output, output_size);
	if (res) {
		ks_info(KBUILD_MODNAME
": keystore_test_aes_siv_steps: aes_siv_s2v returned %d\n", res);
		return res;
	}

	if (show_and_compare("S2V       ", output, "EXPECT    ", cmac_final,
				output_size)) {
		return -1;
	}

	/* Q = V bitand (1^64 || 0^1 || 1^31 || 0^1 || 1^31) */
	output[2] &= ~(1 << 7);
	output[3] &= ~(1 << 7);

	if (show_and_compare("CTR IV    ", output, "EXPECT    ", ctr_iv,
				output_size)) {
		return -1;
	}

	/* encrypt data using AES-CTR */
	res = keystore_aes_ctr_crypt(1, key + half_key_size,
			half_key_size, (const char *) output, output_size,
			plaintext, (char *) output, sizeof(plaintext));
	if (res) {
		ks_err(KBUILD_MODNAME
": keystore_test_aes_siv_steps: keystore_aes_ctr_crypt returned %d\n", res);
		return res;
	}

	if (show_and_compare("CTR RESULT", output, "EXPECT    ", ctr_out,
				sizeof(ctr_out))) {
		return -1;
	}

#ifdef DEBUG_TRACE
	ks_info(KBUILD_MODNAME ":   OK!\n");
#endif

	return 0;
}

/* end of file */
