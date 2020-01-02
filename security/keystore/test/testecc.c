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

#include "keystore_ecc.h"
#include "keystore_rand.h"
#include "keystore_debug.h"
#include "keystore_seed.h"

int keystore_test_ecc(void)
{
	int res = 0;
	uint8_t random_int[SEC_SEED_SIZE];
	struct ias_keystore_ecc_keypair key_pair;
	struct keystore_ecc_signature ecc_signature;

	/*TESTING Random number generation */
	res = keystore_get_rdrand(random_int, SEC_SEED_SIZE);
	if (res != 0) {
		ks_err(KBUILD_MODNAME ": Random number generator test error %d\n",
			       res);
		goto err1;
	}

	keystore_hexdump("Random Number", random_int, SEC_SEED_SIZE);

	/*TESTING ECC Generation, ECC Signing and ECC verification */
	res = keystore_ecc_gen_keys(random_int, SEC_SEED_SIZE, &key_pair);

	if (res != 0) {
		ks_err(KBUILD_MODNAME ": Generate ECC keys test error %d\n",
			       res);
		goto err1;
	}

	keystore_hexdump("ECC public key", &key_pair.public_key,
			 sizeof(key_pair.public_key));
	keystore_hexdump("ECC private key", key_pair.private_key,
			 sizeof(key_pair.private_key));
	res = keystore_ecc_sign(key_pair.private_key,
				&key_pair.private_key,
				sizeof(key_pair.private_key),
				&ecc_signature);
	if (res) {
		ks_err(KBUILD_MODNAME ": Veryfied ECC signature test error %d\n",
		       res);
		goto err1;
	}

	res = keystore_ecc_verify(
		&key_pair.public_key,
		key_pair.private_key,
		sizeof(key_pair.private_key),
		&ecc_signature);

	if (res != 0) {
		ks_err(KBUILD_MODNAME ": Veryfied ECC signature test error %d\n",
		       res);
	}

err1:
	return res;
}


/* end of file */
