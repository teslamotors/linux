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

#include "keystore_mac.h"
#include "keystore_debug.h"

/**
 * Test the CMAC (AES-128).
 *
 * @return 0 if OK or negative error code (see errno).
 */
int keystore_test_cmac(void)
{
	/* From NIST Pub 800-38B, Example 3: Mlen = 320 */
	static const char *key = "\x2b\x7e\x15\x16\x28\xae\xd2\xa6\xab\xf7\x15\x88\x09\xcf\x4f\x3c";
	const unsigned int klen = 16;
	static const char *input = "\x6b\xc1\xbe\xe2\x2e\x40\x9f\x96\xe9\x3d\x7e\x11\x73\x93\x17\x2a"
		"\xae\x2d\x8a\x57\x1e\x03\xac\x9c\x9e\xb7\x6f\xac\x45\xaf\x8e\x51\x30\xc8\x1c\x46\xa3\x5c\xe4\x11";
	const unsigned int ilen = 40;
	static const char *result = "\xdf\xa6\x67\x47\xde\x9a\xe6\x30\x30\xca\x32\x61\x14\x97\xc8\x27";
	const unsigned int olen = 16;

	unsigned char output[olen];
	int res;

#ifdef DEBUG_TRACE
	ks_info(KBUILD_MODNAME ": keystore_test_cmac\n");
#endif

	memset(output, 0, sizeof(output));

	res = keystore_calc_mac("cmac(aes)", key, klen, input, ilen, output,
			olen);
	if (res) {
		ks_info(KBUILD_MODNAME ": keystore_test_cmac: keystore_calc_mac returned %d\n",
			res);
		return res;
	}

#ifdef DEBUG_TRACE
	keystore_hexdump("KEY   ", key, klen);
	keystore_hexdump("INPUT ", input, ilen);
	keystore_hexdump("CMAC  ", output, olen);
	keystore_hexdump("EXPECT", result, olen);
#endif

	res = memcmp(output, result, olen);

#ifdef DEBUG_TRACE
	ks_info(KBUILD_MODNAME ":   %s!\n", res ? "ERROR" : "OK");
#endif

	return res ? -1 : 0;
}

/* end of file */
