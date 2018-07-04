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

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/dcache.h>
#include <linux/random.h>

#include <security/keystore_api_kernel.h>
#include "keystore_client.h"
#include "keystore_context_safe.h"
#include "keystore_rand.h"
#include "keystore_ecc.h"
#include "keystore_seed.h"
#include "keystore_operations.h"
#include "keystore_debug.h"

int keystore_gen_ecc_keys(struct ias_keystore_ecc_keypair *key_pair)
{
	int res = 0;
	uint8_t random_seed[SEC_SEED_SIZE];

	FUNC_BEGIN;

	if (!key_pair)
		return -EFAULT;

	res = keystore_get_rdrand(random_seed, SEC_SEED_SIZE);

	if (res == 0) {
		res = keystore_ecc_gen_keys(random_seed, SEC_SEED_SIZE,
					    key_pair);
	} else {
	   ks_err(KBUILD_MODNAME ": %s: Error generating random seed.\n",
		  __func__);
	}

	FUNC_RES(res);
	return res;
}
EXPORT_SYMBOL(keystore_gen_ecc_keys);

int keystore_test_verify_ecc_signature(const struct keystore_ecc_public_key
				       *public_key, const uint8_t *data,
				       unsigned int data_size,
				       const struct keystore_ecc_signature
				       *output_sig)
{
	int res = 0;

	FUNC_BEGIN;

	if (!public_key || !data || !output_sig) {
		res = -EFAULT;
		goto exit;
	}

	res = keystore_ecc_verify(public_key, data, data_size, output_sig);

	if (res != 0)
		ks_err(KBUILD_MODNAME ": %s: Error %d in keystore_ecc_verify_signature\n",
		       __func__, res);

exit:
	FUNC_RES(res);
	return res;
}
EXPORT_SYMBOL(keystore_test_verify_ecc_signature);

int keystore_unwrapped_key_size(unsigned int wrapped_size,
				unsigned int *unwrapped_size)
{
	return unwrapped_key_size(wrapped_size, unwrapped_size);
}
EXPORT_SYMBOL(keystore_unwrapped_key_size);

int keystore_unwrap_with_backup(const uint32_t *key_enc_backup,
				const uint8_t *backup_enc,
				const uint8_t *wrapped_key,
				unsigned int wrapped_key_size,
				enum keystore_key_spec *keyspec,
				uint8_t *unwrapped_key)
{
	int res = 0;
	struct keystore_backup_data backup;

	FUNC_BEGIN;

	/* Check inputs */
	if (!key_enc_backup || !backup_enc ||
	    !wrapped_key || !unwrapped_key || !keyspec) {
		res = -EFAULT;
		goto exit;
	}

	res = decrypt_from_target(key_enc_backup,
				  backup_enc, KEYSTORE_BACKUP_SIZE,
				  &backup, sizeof(backup));

	keystore_hexdump("Decrypted backup", &backup, sizeof(backup));

	if (res < 0) {
		ks_warn(KBUILD_MODNAME ": %s: Error %d, fail to decrypt backup\n",
			__func__, res);
		goto exit;
	}

	res = unwrap_key(backup.client_key,
			 wrapped_key, wrapped_key_size,
			 keyspec, unwrapped_key);

	if (res) {
		ks_err(KBUILD_MODNAME ": %s: Cannot unwrap appkey\n", __func__);
		goto exit;
	}
exit:
	FUNC_RES(res);
	return res;
}
EXPORT_SYMBOL(keystore_unwrap_with_backup);

int keystore_migrate(const uint32_t *key_enc_backup,
		     const uint8_t *backup_enc,
		     const uint32_t *key_enc_mkey,
		     const uint8_t *mkey_enc,
		     uint8_t *backup_mk_enc)
{
	int res = 0;
	struct keystore_backup_data backup, aes_backup;
	uint8_t mkey[KEYSTORE_MKEY_SIZE];

	FUNC_BEGIN;

	/* Check inputs */
	if (!key_enc_backup || !backup_enc ||
	    !key_enc_mkey || !mkey_enc ||
	    !backup_mk_enc) {
		res = -EFAULT;
		goto exit;
	}

	/* Prepare data strutures */
	memset(&backup, 0, sizeof(backup));
	memset(&aes_backup, 0, sizeof(aes_backup));
	memset(&mkey, 0, sizeof(mkey));

	res = decrypt_from_target(key_enc_backup,
				  backup_enc, KEYSTORE_BACKUP_SIZE,
				  &backup, sizeof(backup));

	keystore_hexdump("Decrypted backup", &backup, sizeof(backup));

	if (res < 0) {
		ks_warn(KBUILD_MODNAME ": %s: Error %d, fail to decrypt backup\n",
			__func__, res);
		goto exit;
	}

	res = decrypt_from_target(key_enc_mkey, mkey_enc, KEYSTORE_EMKEY_SIZE,
				  mkey, sizeof(mkey));

	keystore_hexdump("Decrypted mkey (ECIES)", mkey, sizeof(mkey));

	if (res < 0) {
		ks_warn(KBUILD_MODNAME ": %s: Error %d, fail to decrypt migration key\n",
			__func__, res);
		goto exit;
	}

	res = wrap_backup(mkey, &backup, sizeof(backup),
			  backup_mk_enc, REENCRYPTED_BACKUP_SIZE);

	keystore_hexdump("Output", backup_mk_enc, REENCRYPTED_BACKUP_SIZE);

	if (res) {
		ks_warn(KBUILD_MODNAME ": %s: Error %d in wrap_backup\n",
			__func__, res);
		goto exit;
	}

	res = unwrap_backup(mkey, backup_mk_enc, REENCRYPTED_BACKUP_SIZE,
			    &aes_backup, sizeof(aes_backup));

	keystore_hexdump("original backup:", &backup, sizeof(backup));
	keystore_hexdump("aes dec  backup:", &aes_backup, sizeof(aes_backup));

	if (res) {
		ks_warn(KBUILD_MODNAME ": %s: failed to decrypt data\n",
			__func__);
		goto exit;
	}

	if (res || memcmp((const uint8_t *)&backup,
			  (const uint8_t *)&aes_backup,
			  sizeof(backup)) != 0) {
		ks_warn(KBUILD_MODNAME ": %s: Inconsistency in re-encrypted backup data!\n",
			__func__);
		res = -EBADE;
	}

exit:
	FUNC_RES(res);
	return res;
}
EXPORT_SYMBOL(keystore_migrate);

