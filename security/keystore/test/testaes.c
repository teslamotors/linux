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

/**
 * Test the AES-128 encryption & decryption.
 *
 * @return 0 if OK or negative error code (see errno).
 */
static int keystore_test_aes128(void)
{
	/* From FIPS-197 */
	static const char *key = "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F";
	const unsigned int klen = 16;
	static const char *input = "\x00\x11\x22\x33\x44\x55\x66\x77\x88\x99\xAA\xBB\xCC\xDD\xEE\xFF";
	static const char *result = "\x69\xC4\xE0\xD8\x6A\x7B\x04\x30\xD8\xCD\xB7\x80\x70\xB4\xC5\x5A";
	const unsigned int len = 16;

	unsigned char output[len];
	unsigned char output2[len];
	int res;

	ks_info(KBUILD_MODNAME ": keystore_test_aes128\n");

	memset(output, 0, len);
	memset(output2, 0, len);

	res = keystore_aes_crypt(1, key, klen, input, output, len);
	if (res) {
		ks_info(KBUILD_MODNAME ": keystore_aes_crypt returned %d\n",
			res);
		return res;
	}

	keystore_hexdump("KEY    ", key, klen);
	keystore_hexdump("PLAIN  ", input, len);
	keystore_hexdump("ENCRYPT", output, len);
	keystore_hexdump("EXPECT ", result, len);

	res = memcmp(output, result, len);

	ks_info(KBUILD_MODNAME ":   %s!\n", res ? "ERROR" : "OK");

	if (res)
		return -1;

	res = keystore_aes_crypt(0, key, klen, output, output2, len);
	if (res) {
		ks_info(KBUILD_MODNAME ": keystore_aes_crypt returned %d\n",
			res);
		return res;
	}

	keystore_hexdump("DECRYPT", output2, len);
	keystore_hexdump("EXPECT ", input, len);

	res = memcmp(output2, input, len);

	ks_info(KBUILD_MODNAME ":   %s!\n", res ? "ERROR" : "OK");

	return res ? -1 : 0;
}

/**
 * Test the AES-256 encryption & decryption.
 *
 * @return 0 if OK or negative error code (see errno).
 */
static int keystore_test_aes256(void)
{
	/* From FIPS-197 */
	static const char *key = "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F";
	const unsigned int klen = 32;
	static const char *input = "\x00\x11\x22\x33\x44\x55\x66\x77\x88\x99\xAA\xBB\xCC\xDD\xEE\xFF";
	static const char *result = "\x8E\xA2\xB7\xCA\x51\x67\x45\xBF\xEA\xFC\x49\x90\x4B\x49\x60\x89";
	const unsigned int len = 16;

	unsigned char output[len];
	unsigned char output2[len];
	int res;

	ks_info(KBUILD_MODNAME ": keystore_test_aes256\n");

	memset(output, 0, len);
	memset(output2, 0, len);

	res = keystore_aes_crypt(1, key, klen, input, output, len);
	if (res) {
		ks_info(KBUILD_MODNAME ": keystore_aes_crypt returned %d\n", res);
		return res;
	}

	keystore_hexdump("KEY    ", key, klen);
	keystore_hexdump("PLAIN  ", input, len);
	keystore_hexdump("ENCRYPT", output, len);
	keystore_hexdump("EXPECT ", result, len);

	res = memcmp(output, result, len);

	ks_info(KBUILD_MODNAME ":   %s!\n", res ? "ERROR" : "OK");

	if (res)
		return -1;

	res = keystore_aes_crypt(0, key, klen, output, output2, len);
	if (res) {
		ks_info(KBUILD_MODNAME ": keystore_aes_crypt returned %d\n",
			res);
		return res;
	}

	keystore_hexdump("DECRYPT", output2, len);
	keystore_hexdump("EXPECT ", input, len);

	res = memcmp(output2, input, len);

	ks_info(KBUILD_MODNAME ":   %s!\n", res ? "ERROR" : "OK");

	return res ? -1 : 0;
}

/**
 * Test the AES CCM encryption & decryption.
 *
 * @return 0 if OK or negative error code (see errno).
 */
static int keystore_test_aes_ccm(void)
{
	/* From RFC 3610 */
	static const char *key = "\xc0\xc1\xc2\xc3\xc4\xc5\xc6\xc7\xc8\xc9\xca\xcb\xcc\xcd\xce\xcf";
	const unsigned int klen = 16;
	static const char *iv = "\x01\x00\x00\x00\x03\x02\x01\x00\xa0\xa1\xa2\xa3\xa4\xa5\x00\x00";
	const unsigned int ivlen = 16;
	static const char *assoc = "\x00\x01\x02\x03\x04\x05\x06\x07";
	const unsigned int alen = 8;
	static const char *input = "\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e";
	const unsigned int ilen = 23;
	static const char *result = "\x00\x01\x02\x03\x04\x05\x06\x07\x58\x8c\x97\x9a\x61\xc6\x63\xd2\xf0\x66\xd0\xc2\xc0\xf9\x89\x80\x6d\x5f\x6b\x61\xda\xc3\x84\x17\xe8\xd1\x2c\xfd\xf9\x26\xe0";
	const unsigned int rlen = 39;

	unsigned char input2[alen + ilen];
	unsigned char output[rlen];
	unsigned char output2[alen + ilen];
	int res;

	ks_info(KBUILD_MODNAME ": keystore_test_aes_ccm\n");

	/* Input buffer for encryption: assoc data || plaintext */
	memcpy(input2, assoc, alen);
	memcpy(input2 + alen, input, ilen);

	memset(output, 0, sizeof(output));
	memset(output2, 0, sizeof(output2));

	res = keystore_aes_ccm_crypt(1, key, klen, iv, ivlen, input2,
				     alen + ilen, alen, output, sizeof(output));
	if (res) {
		ks_info(KBUILD_MODNAME ": keystore_aes_ccm_crypt returned %d\n",
			res);
		return res;
	}

	keystore_hexdump("KEY    ", key, klen);
	keystore_hexdump("IV     ", iv, ivlen);
	keystore_hexdump("ASSOC  ", assoc, alen);
	keystore_hexdump("PLAIN  ", input, ilen);
	keystore_hexdump("ENCRYPT", output, sizeof(output));
	keystore_hexdump("EXPECT ", result, rlen);

	res = memcmp(output, result, rlen);

	ks_info(KBUILD_MODNAME ":   %s!\n", res ? "ERROR" : "OK");

	if (res)
		return -1;

	res = keystore_aes_ccm_crypt(0, key, klen, iv, ivlen, output,
			sizeof(output), alen, output2, sizeof(output2));
	if (res) {
		ks_info(KBUILD_MODNAME ": keystore_aes_ccm_crypt returned %d\n",
			res);
		return res;
	}

	keystore_hexdump("DECRYPT", output2, sizeof(output2));
	keystore_hexdump("EXPECT ", input, ilen);

	res = memcmp(output2, input, ilen);

	ks_info(KBUILD_MODNAME ":   %s!\n", res ? "ERROR" : "OK");

	return res ? -1 : 0;
}

/**
 * Test the AES GCM encryption & decryption.
 *
 * @return 0 if OK or negative error code (see errno).
 */
static int keystore_test_aes_gcm(void)
{
	/* From GCM-Spec Test case 3 */
	static const char *key = "\xfe\xff\xe9\x92\x86\x65\x73\x1c\x6d\x6a\x8f\x94\x67\x30\x83\x08";
	const unsigned int klen = 16;
	static const char *iv = "\xca\xfe\xba\xbe\xfa\xce\xdb\xad\xde\xca\xf8\x88";
	const unsigned int ivlen = 12;
	static const char *assoc;
	const unsigned int alen = 0;
	static const char *input = "\xd9\x31\x32\x25\xf8\x84\x06\xe5\xa5\x59\x09\xc5\xaf\xf5\x26\x9a\x86\xa7\xa9\x53\x15\x34\xf7\xda\x2e\x4c\x30\x3d\x8a\x31\x8a\x72\x1c\x3c\x0c\x95\x95\x68\x09\x53\x2f\xcf\x0e\x24\x49\xa6\xb5\x25\xb1\x6a\xed\xf5\xaa\x0d\xe6\x57\xba\x63\x7b\x39\x1a\xaf\xd2\x55";
	const unsigned int ilen = 64;
	static const char *result = "\x42\x83\x1e\xc2\x21\x77\x74\x24\x4b\x72\x21\xb7\x84\xd0\xd4\x9c\xe3\xaa\x21\x2f\x2c\x02\xa4\xe0\x35\xc1\x7e\x23\x29\xac\xa1\x2e\x21\xd5\x14\xb2\x54\x66\x93\x1c\x7d\x8f\x6a\x5a\xac\x84\xaa\x05\x1b\xa3\x0b\x39\x6a\x0a\xac\x97\x3d\x58\xe0\x91\x47\x3f\x59\x85";
	const unsigned int rlen = 64;
	unsigned char output[rlen + KEYSTORE_GCM_AUTH_SIZE];
	unsigned char output2[ilen];
	int res;

	ks_info(KBUILD_MODNAME ": keystore_test_aes_gcm\n");

	memset(output, 0, sizeof(output));
	memset(output2, 0, sizeof(output2));

	res = keystore_aes_gcm_crypt(1, key, klen, iv, ivlen, input, ilen,
				     alen, output, sizeof(output));
	if (res) {
		ks_info(KBUILD_MODNAME ": keystore_aes_gcm_crypt returned %d\n",
			res);
		return res;
	}

	keystore_hexdump("KEY    ", key, klen);
	keystore_hexdump("IV     ", iv, ivlen);
	keystore_hexdump("ASSOC  ", assoc, alen);
	keystore_hexdump("PLAIN  ", input, ilen);
	keystore_hexdump("ENCRYPT", output, sizeof(output));
	keystore_hexdump("EXPECT ", result, rlen);

	res = memcmp(output, result, rlen);

	ks_info(KBUILD_MODNAME ":   %s!\n", res ? "ERROR" : "OK");

	if (res)
		return -1;

	res = keystore_aes_gcm_crypt(0, key, klen, iv, ivlen, output,
			sizeof(output), alen, output2, sizeof(output2));
	if (res) {
		ks_info(KBUILD_MODNAME ": keystore_aes_gcm_crypt returned %d\n",
			res);
		return res;
	}

	keystore_hexdump("DECRYPT", output2, sizeof(output2));
	keystore_hexdump("EXPECT ", input, ilen);

	res = memcmp(output2, input, ilen);

	ks_info(KBUILD_MODNAME ":   %s!\n", res ? "ERROR" : "OK");

	return res ? -1 : 0;
}

/**
 * Test the AES CTR encryption & decryption.
 *
 * @return 0 if OK or negative error code (see errno).
 */
static int keystore_test_aes_ctr(void)
{
	/* From NIST Pub 800-38A */
	static const char *key = "\x2b\x7e\x15\x16\x28\xae\xd2\xa6\xab\xf7\x15\x88\x09\xcf\x4f\x3c";
	const unsigned int klen = 16;
	static const char *iv = "\xf0\xf1\xf2\xf3\xf4\xf5\xf6\xf7\xf8\xf9\xfa\xfb\xfc\xfd\xfe\xff";
	const unsigned int ivlen = 16;
	static const char *input =
		"\x6b\xc1\xbe\xe2\x2e\x40\x9f\x96\xe9\x3d\x7e\x11\x73\x93\x17\x2a"
		"\xae\x2d\x8a\x57\x1e\x03\xac\x9c\x9e\xb7\x6f\xac\x45\xaf\x8e\x51"
		"\x30\xc8\x1c\x46\xa3\x5c\xe4\x11\xe5\xfb\xc1\x19\x1a\x0a\x52\xef"
		"\xf6\x9f\x24\x45\xdf\x4f\x9b\x17\xad\x2b\x41\x7b\xe6\x6c\x37\x10";
	const unsigned int len = 64;
	static const char *result =
		"\x87\x4d\x61\x91\xb6\x20\xe3\x26\x1b\xef\x68\x64\x99\x0d\xb6\xce"
		"\x98\x06\xf6\x6b\x79\x70\xfd\xff\x86\x17\x18\x7b\xb9\xff\xfd\xff"
		"\x5a\xe4\xdf\x3e\xdb\xd5\xd3\x5e\x5b\x4f\x09\x02\x0d\xb0\x3e\xab"
		"\x1e\x03\x1d\xda\x2f\xbe\x03\xd1\x79\x21\x70\xa0\xf3\x00\x9c\xee";

	unsigned char output[len];
	unsigned char output2[len];
	int res;

	ks_info(KBUILD_MODNAME ": keystore_test_aes_ctr\n");

	memset(output, 0, sizeof(output));
	memset(output2, 0, sizeof(output2));

	res = keystore_aes_ctr_crypt(1, key, klen, iv, ivlen, input,
				     output, len);
	if (res) {
		ks_info(KBUILD_MODNAME ": keystore_aes_ctr_crypt returned %d\n",
			res);
		return res;
	}

	keystore_hexdump("KEY    ", key, klen);
	keystore_hexdump("IV     ", iv, ivlen);
	keystore_hexdump("PLAIN  ", input, len);
	keystore_hexdump("ENCRYPT", output, len);
	keystore_hexdump("EXPECT ", result, len);

	res = memcmp(output, result, len);

	ks_info(KBUILD_MODNAME ":   %s!\n", res ? "ERROR" : "OK");

	if (res)
		return -1;

	res = keystore_aes_ctr_crypt(0, key, klen, iv, ivlen, output, output2,
			len);
	if (res) {
		ks_info(KBUILD_MODNAME ": keystore_aes_ctr_crypt returned %d\n",
			res);
		return res;
	}

	keystore_hexdump("DECRYPT", output2, len);
	keystore_hexdump("EXPECT ", input, len);

	res = memcmp(output2, input, len);

	ks_info(KBUILD_MODNAME ":   %s!\n", res ? "ERROR" : "OK");

	return res ? -1 : 0;
}

/**
 * Test the AES encryption & decryption.
 *
 * @return 0 if OK or negative error code (see errno).
 */
int keystore_test_aes(void)
{
	int res = 0;

	res |= keystore_test_aes128();
	res |= keystore_test_aes256();
	res |= keystore_test_aes_ccm();
	res |= keystore_test_aes_gcm();
	res |= keystore_test_aes_ctr();

	return res;
}

/* end of file */
