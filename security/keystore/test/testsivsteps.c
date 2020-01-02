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
extern int aes_siv_xorend(const void *a_ptr, unsigned int a_size,
			   const void *b_ptr, unsigned int b_size,
			   void *output);
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

static const uint8_t cmac_zero[] = {0x0e, 0x04, 0xdf, 0xaf,
				    0xc1, 0xef, 0xbf, 0x04,
				    0x01, 0x40, 0x58, 0x28,
				    0x59, 0xbf, 0x07, 0x3a};

static const uint8_t double1[] = {0x1c, 0x09, 0xbf, 0x5f,
				  0x83, 0xdf, 0x7e, 0x08,
				  0x02, 0x80, 0xb0, 0x50,
				  0xb3, 0x7e, 0x0e, 0x74};

static const uint8_t cmac_ad[] = {0xf1, 0xf9, 0x22, 0xb7,
				  0xf5, 0x19, 0x3c, 0xe6,
				  0x4f, 0xf8, 0x0c, 0xb4,
				  0x7d, 0x93, 0xf2, 0x3b};

static const uint8_t xor1[] = {0xed, 0xf0, 0x9d, 0xe8, 0x76, 0xc6, 0x42, 0xee,
			       0x4d, 0x78, 0xbc, 0xe4, 0xce, 0xed, 0xfc, 0x4f};

static const uint8_t double2[] = {0xdb, 0xe1, 0x3b, 0xd0,
				  0xed, 0x8c, 0x85, 0xdc,
				  0x9a, 0xf1, 0x79, 0xc9,
				  0x9d, 0xdb, 0xf8, 0x19};

static const uint8_t pad[] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
			      0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0x80, 0x00};

static const uint8_t xor2[] = {0xca, 0xc3, 0x08, 0x94, 0xb8, 0xea, 0xf2, 0x54,
			       0x03, 0x5b, 0xc2, 0x05, 0x40, 0x35, 0x78, 0x19};

static const uint8_t cmac_final[] = {0x85, 0x63, 0x2d, 0x07,
				     0xc6, 0xe8, 0xf3, 0x7f,
				     0x95, 0x0a, 0xcd, 0x32,
				     0x0a, 0x2e, 0xcc, 0x93};

static const uint8_t ctr_iv[] = {0x85, 0x63, 0x2d, 0x07,
				 0xc6, 0xe8, 0xf3, 0x7f,
				 0x15, 0x0a, 0xcd, 0x32,
				 0x0a, 0x2e, 0xcc, 0x93};

static const uint8_t ctr_out[] = {0x40, 0xc0, 0x2b, 0x96,
				  0x90, 0xc4, 0xdc, 0x04,
				  0xda, 0xef, 0x7f, 0x6a,
				  0xfe, 0x5c};

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

	ks_info(KBUILD_MODNAME ": keystore_test_aes_siv_steps\n");
	memset(output, 0, output_size);

	/* CMAC(zero) */
	res = keystore_calc_mac("cmac(aes)", key, half_key_size, zeros,
			sizeof(zeros), (char *) output, output_size);
	if (res) {
		ks_info(KBUILD_MODNAME
": keystore_test_aes_siv_steps: keystore_calc_mac returned %d\n", res);
		return res;
	}

	keystore_hexdump("KEY       ", key, sizeof(key));
	keystore_hexdump("AD        ", ad, sizeof(ad));
	keystore_hexdump("PLAINTEXT ", plaintext, sizeof(plaintext));
	keystore_hexdump("ZEROS     ", zeros, sizeof(zeros));

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

	res = aes_siv_s2v(key, half_key_size,
			  strings, ARRAY_SIZE(strings),
			  output, output_size);
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
		       ": keystore_test_aes_siv_steps: keystore_aes_ctr_crypt returned %d\n",
		       res);
		return res;
	}

	if (show_and_compare("CTR RESULT", output, "EXPECT    ", ctr_out,
				sizeof(ctr_out))) {
		return -1;
	}

	ks_info(KBUILD_MODNAME ":   OK!\n");

	return 0;
}

/* From RFC 5297 (A.2 Nonce-Based Authenticated Encryption Example) */
/* Although nonce-based encryption is not exposed to the user, this */
/* test case is included to test the xorend function.               */

static const uint8_t key_2[] = {0x7f, 0x7e, 0x7d, 0x7c, 0x7b, 0x7a, 0x79, 0x78,
				0x77, 0x76, 0x75, 0x74, 0x73, 0x72, 0x71, 0x70,
				0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
				0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f};

static const uint8_t ad1_2[]   = {0x00, 0x11, 0x22, 0x33,
				  0x44, 0x55, 0x66, 0x77,
				  0x88, 0x99, 0xaa, 0xbb,
				  0xcc, 0xdd, 0xee, 0xff,
				  0xde, 0xad, 0xda, 0xda,
				  0xde, 0xad, 0xda, 0xda,
				  0xff, 0xee, 0xdd, 0xcc,
				  0xbb, 0xaa, 0x99, 0x88,
				  0x77, 0x66, 0x55, 0x44,
				  0x33, 0x22, 0x11, 0x00};

static const uint8_t ad2_2[]   = {0x10, 0x20, 0x30, 0x40,
				  0x50, 0x60, 0x70, 0x80,
				  0x90, 0xa0};

static const uint8_t nonce_2[] = {0x09, 0xf9, 0x11, 0x02,
				  0x9d, 0x74, 0xe3, 0x5b,
				  0xd8, 0x41, 0x56, 0xc5,
				  0x63, 0x56, 0x88, 0xc0};

static const uint8_t plaintext_2[] = {0x74, 0x68, 0x69, 0x73,
				      0x20, 0x69, 0x73, 0x20,
				      0x73, 0x6f, 0x6d, 0x65,
				      0x20, 0x70, 0x6c, 0x61,
				      0x69, 0x6e, 0x74, 0x65,
				      0x78, 0x74, 0x20, 0x74,
				      0x6f, 0x20, 0x65, 0x6e,
				      0x63, 0x72, 0x79, 0x70,
				      0x74, 0x20, 0x75, 0x73,
				      0x69, 0x6e, 0x67, 0x20,
				      0x53, 0x49, 0x56, 0x2d,
				      0x41, 0x45, 0x53};

static const uint8_t xor_2[] = { 0x16, 0x59, 0x2c, 0x17,
				 0x72, 0x9a, 0x5a, 0x72,
				 0x55, 0x67, 0x63, 0x61,
				 0x68, 0xb4, 0x83, 0x76};

static const uint8_t xorend_2[] = {0x74, 0x68, 0x69, 0x73,
				   0x20, 0x69, 0x73, 0x20,
				   0x73, 0x6f, 0x6d, 0x65,
				   0x20, 0x70, 0x6c, 0x61,
				   0x69, 0x6e, 0x74, 0x65,
				   0x78, 0x74, 0x20, 0x74,
				   0x6f, 0x20, 0x65, 0x6e,
				   0x63, 0x72, 0x79, 0x66,
				   0x2d, 0x0c, 0x62, 0x01,
				   0xf3, 0x34, 0x15, 0x75,
				   0x34, 0x2a, 0x37, 0x45,
				   0xf5, 0xc6, 0x25};

static const uint8_t cmac_final_2[] = {0x7b, 0xdb, 0x6e, 0x3b,
				       0x43, 0x26, 0x67, 0xeb,
				       0x06, 0xf4, 0xd1, 0x4b,
				       0xff, 0x2f, 0xbd, 0x0f};

static const uint8_t ctr_iv_2[] = {0x7b, 0xdb, 0x6e, 0x3b,
				   0x43, 0x26, 0x67, 0xeb,
				   0x06, 0xf4, 0xd1, 0x4b,
				   0x7f, 0x2f, 0xbd, 0x0f};

static const uint8_t ctr_out_2[] = {0xcb, 0x90, 0x0f, 0x2f,
				    0xdd, 0xbe, 0x40, 0x43,
				    0x26, 0x60, 0x19, 0x65,
				    0xc8, 0x89, 0xbf, 0x17,
				    0xdb, 0xa7, 0x7c, 0xeb,
				    0x09, 0x4f, 0xa6, 0x63,
				    0xb7, 0xa3, 0xf7, 0x48,
				    0xba, 0x8a, 0xf8, 0x29,
				    0xea, 0x64, 0xad, 0x54,
				    0x4a, 0x27, 0x2e, 0x9c,
				    0x48, 0x5b, 0x62, 0xa3,
				    0xfd, 0x5c, 0x0d};

int keystore_test_aes_siv_nonce(void)
{
	const unsigned int half_key_size = sizeof(key_2) / 2;
	const unsigned int output_size = sizeof(plaintext_2);
	uint32_t output[output_size / sizeof(uint32_t)];
	struct aes_siv_string strings[4];
	int res = 0;

	ks_info(KBUILD_MODNAME ": keystore_test_aes_siv_nonce\n");
	memset(output, 0, output_size);

	keystore_hexdump("KEY       ", key_2, sizeof(key_2));
	keystore_hexdump("AD1       ", ad1_2, sizeof(ad1_2));
	keystore_hexdump("AD2       ", ad2_2, sizeof(ad2_2));
	keystore_hexdump("PLAINTEXT ", plaintext_2, sizeof(plaintext_2));

	/* Just check the xorend function here */
	aes_siv_xorend(plaintext_2, sizeof(plaintext_2),
		       xor_2, sizeof(xor_2),
		       output);
	if (show_and_compare("XOREND    ", output,
			     "EXPECT    ", xorend_2,
			     output_size)) {
		return -1;
	}
	/* now try the same steps but combined in aes_siv_s2v() call */
	memset(output, 0, output_size);
	strings[0].data = ad1_2;
	strings[0].size = sizeof(ad1_2);
	strings[1].data = ad2_2;
	strings[1].size = sizeof(ad2_2);
	strings[2].data = nonce_2;
	strings[2].size = sizeof(nonce_2);
	strings[3].data = plaintext_2;
	strings[3].size = sizeof(plaintext_2);

	res = aes_siv_s2v(key_2, half_key_size,
			  strings, ARRAY_SIZE(strings),
			  output, output_size);
	if (res) {
		ks_info(KBUILD_MODNAME
":keystore_test_aes_siv_nonce: aes_siv_s2v returned %d\n", res);
		return res;
	}

	if (show_and_compare("S2V       ", output,
			     "EXPECT    ", cmac_final_2,
			     sizeof(cmac_final_2))) {
		return -1;
	}

	/* Q = V bitand (1^64 || 0^1 || 1^31 || 0^1 || 1^31) */
	output[2] &= ~(1 << 7);
	output[3] &= ~(1 << 7);

	if (show_and_compare("CTR IV    ", output, "EXPECT    ", ctr_iv_2,
			     sizeof(ctr_iv_2))) {
		return -1;
	}

	/* encrypt data using AES-CTR */
	res = keystore_aes_ctr_crypt(1, key_2 + half_key_size, half_key_size,
				     (const char *) output, AES_BLOCK_SIZE,
				     plaintext_2,
				     (char *)output, sizeof(plaintext_2));
	if (res) {
		ks_err(KBUILD_MODNAME
		       ": keystore_test_aes_siv_nonce: keystore_aes_ctr_crypt returned %d\n",
		       res);
		return res;
	}

	if (show_and_compare("CTR RESULT", output, "EXPECT    ", ctr_out_2,
				sizeof(ctr_out_2))) {
		return -1;
	}

	ks_info(KBUILD_MODNAME ":   OK!\n");

	return 0;
}

/* end of file */
