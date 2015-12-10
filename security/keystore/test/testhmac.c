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
#include "keystore_constants.h"

/**
 * Test the SHA-256 HMAC.
 *
 * @return 0 if OK or negative error code (see errno).
 */
int keystore_test_hmac(void)
{
	static const char *key = "passphrase";
	static const char *data = "22";
	static const char *result =
		"\xb3\x55\x01\x79\xf6\xde\x6e\x89\x1b\xe2\xf7\x51\xca\xab\xf7\x82"
		"\x45\x8b\x9d\x49\xb0\xc0\xdb\x28\xe3\x85\xb3\xa3\x32\x26\x70\x56";
	unsigned char hmac[SHA256_HMAC_SIZE];
	int res;

#ifdef DEBUG_TRACE
	ks_info(KBUILD_MODNAME ": keystore_test_hmac\n");
#endif

	memset(hmac, 0, sizeof(hmac));

	res = keystore_calc_mac("hmac(sha256)", key, strlen(key), data,
			strlen(data), hmac, sizeof(hmac));
	if (res) {
		ks_info(KBUILD_MODNAME ": keystore_test_hmac: keystore_calc_mac returned %d\n",
			res);
		return res;
	}

#ifdef DEBUG_TRACE
	keystore_hexdump("KEY   ", key, strlen(key));
	keystore_hexdump("INPUT ", data, strlen(data));
	keystore_hexdump("HMAC  ", hmac, sizeof(hmac));
	keystore_hexdump("EXPECT", result, sizeof(hmac));
#endif

	res = memcmp(hmac, result, sizeof(hmac));

#ifdef DEBUG_TRACE
	ks_info(KBUILD_MODNAME ":   %s!\n", res ? "ERROR" : "OK");
#endif

	return res ? -1 : 0;
}

/* end of file */
