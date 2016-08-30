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
#include "keystore_debug.h"

/* From RFC 5297 (A.1. Deterministic Authenticated Encryption Example) */
static const uint8_t key[] = {
	0xff, 0xfe, 0xfd, 0xfc, 0xfb, 0xfa, 0xf9, 0xf8,
	0xf7, 0xf6, 0xf5, 0xf4, 0xf3, 0xf2, 0xf1, 0xf0,
	0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
	0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
};

static const uint8_t ad[] = {
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
	0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27};

static const uint8_t plaintext[] = {
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
	0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee
};

static const uint8_t result[] = {
	0x85, 0x63, 0x2d, 0x07, 0xc6, 0xe8, 0xf3, 0x7f,
	0x95, 0x0a, 0xcd, 0x32, 0x0a, 0x2e, 0xcc, 0x93,
	0x40, 0xc0, 0x2b, 0x96, 0x90, 0xc4, 0xdc, 0x04,
	0xda, 0xef, 0x7f, 0x6a, 0xfe, 0x5c
};

/**
 * Test the AES_SIV.
 *
 * @return 0 if OK or negative error code (see errno).
 */
int keystore_test_aes_siv(void)
{
	uint8_t output[sizeof(result)];
	int res;

	ks_info(KBUILD_MODNAME ": keystore_test_aes_siv\n");

	memset(output, 0, sizeof(output));

	keystore_hexdump("KEY       ", key, sizeof(key));
	keystore_hexdump("AD        ", ad, sizeof(ad));
	keystore_hexdump("PLAINTEXT ", plaintext, sizeof(plaintext));

	/* test encryption */
	res = keystore_aes_siv_crypt(1, key, sizeof(key), plaintext,
			sizeof(plaintext), ad, sizeof(ad), output,
			sizeof(output));
	if (res) {
		ks_info(KBUILD_MODNAME ": keystore_test_aes_siv: keystore_aes_siv_crypt returned %d\n",
			res);
		return res;
	}

	if (show_and_compare("ENCRYPTED ", output, "EXPECT    ", result,
				sizeof(output)))
		return -1;

	memset(output, 0, sizeof(output));

	/* test decryption */
	res = keystore_aes_siv_crypt(0, key, sizeof(key), result, sizeof(result)
			, ad, sizeof(ad), output, sizeof(plaintext));
	if (res) {
		ks_info(KBUILD_MODNAME ": keystore_test_aes_siv: keystore_aes_siv_crypt returned %d\n",
			res);
		return res;
	}

	if (show_and_compare("DECRYPTED ", output, "EXPECT    ",
			     plaintext, sizeof(plaintext)))
		return -1;

	ks_info(KBUILD_MODNAME ":   OK!\n");

	return 0;
}

/* end of file */
