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
#include <linux/slab.h>

#include <security/keystore_api_kernel.h>
#include "keystore_context_safe.h"
#include "keystore_rand.h"
#include "keystore_operations.h"

#include "keystore_debug.h"

int keystore_generate_mkey(const struct keystore_ecc_public_key *key_enc_mkey,
			   const uint8_t *key_enc_mkey_sig,
			   uint8_t *mkey_enc,
			   struct keystore_ecc_signature *mkey_sig,
			   uint8_t *nonce)
{
	int res;
	uint8_t mkey[KEYSTORE_MKEY_SIZE] = {0};

	FUNC_BEGIN;

	if (!mkey_sig || !key_enc_mkey) {
		ks_err(KBUILD_MODNAME ": %s: [ptr] mkey_sig: %p, key_enc_mkey: %p\n",
		       __func__, mkey_sig, key_enc_mkey);
		res = -EFAULT;
		goto exit;
	}

	/* verify public key signature */
	res = verify_oem_signature(key_enc_mkey,
				   sizeof(struct keystore_ecc_public_key),
				   key_enc_mkey_sig, RSA_SIGNATURE_BYTE_SIZE);

	if (res) {
		ks_err(KBUILD_MODNAME ": %s: OEMpub2Sig verfifcation: Failed\n",
		       __func__);
		goto exit;
	}

	/* use RDRAND to get the nonce */
	res = keystore_get_rdrand(nonce, KEYSTORE_MKEY_NONCE_SIZE);
	if (res != 0) {
		ks_err(KBUILD_MODNAME ": Random number generator test error %d\n",
		       res);
		goto exit;
	}

	keystore_hexdump("migration - nonce", nonce,
			 KEYSTORE_MKEY_NONCE_SIZE);

	/* Generate Migration Key */
	res = generate_migration_key(nonce, mkey);
	if (res) {
		ks_err(KBUILD_MODNAME ": %s: generate_migration_key error %d\n",
		       __func__, res);
		goto exit;
	}

	res = encrypt_for_host(key_enc_mkey, mkey, KEYSTORE_MKEY_SIZE,
			       mkey_enc, KEYSTORE_EMKEY_SIZE);
	if (res) {
		ks_err(KBUILD_MODNAME
		       ": keystore_ecc_encrypt() error %d in %s\n",
		       res, __func__);
		goto zero_buf;
	}

	res = sign_for_host(mkey_enc, KEYSTORE_EMKEY_SIZE, mkey_sig);

	keystore_hexdump("migration key signature", mkey_sig,
			 sizeof(struct keystore_ecc_signature));

	ks_debug(KBUILD_MODNAME ": migration - ECC signature verification - OK\n");
	ks_debug(KBUILD_MODNAME ": migration - Signing MKEY with KSM2 ECC private - OK\n");

zero_buf:
	/* clear local secure data */
	memset(mkey, 0, KEYSTORE_MKEY_SIZE);
exit:
	FUNC_RES(res);
	return res;
}
EXPORT_SYMBOL(keystore_generate_mkey);

int keystore_rewrap_key(const uint8_t *client_ticket,
			const uint8_t *backup_mk_enc,
			const uint8_t *nonce,
			const uint8_t *wrapped_key,
			unsigned int wrapped_key_size,
			uint8_t *rewrapped_key)
{
	int res = 0;
	uint8_t mkey[KEYSTORE_MKEY_SIZE] = {0};

	struct keystore_backup_data plain_backup;

	unsigned int plain_app_key_size;
	uint8_t *plain_app_key; /* starts with 1-byte key spec */
	enum keystore_key_spec keyspec;

	uint8_t client_key[KEYSTORE_CLIENT_KEY_SIZE];
	uint8_t client_id[KEYSTORE_MAX_CLIENT_ID_SIZE];

	FUNC_BEGIN;

	/****************************************************************/
	/* 1. Check input values                                        */
	/****************************************************************/
	if (!client_ticket || !backup_mk_enc || !nonce ||
	    !wrapped_key || !rewrapped_key) {
		ks_err(KBUILD_MODNAME ": %s: NULL pointer: client_ticket: %p, backup_mk_enc: %p, nonce: %p, wrapped_key: %p, rewrapped_key: %p\n",
		       __func__, client_ticket, backup_mk_enc, nonce,
		       wrapped_key, rewrapped_key);
		res = -EFAULT;
		goto exit;
	}

	/****************************************************************/
	/* 2. Initilise values                                          */
	/****************************************************************/

	/* Setup sizes */
	res = unwrapped_key_size(wrapped_key_size, &plain_app_key_size);
	if (res)
		goto exit;

	plain_app_key = kmalloc(plain_app_key_size, GFP_KERNEL);
	if (!plain_app_key) {
		res = -ENOMEM;
		goto exit;
	}
	/* clear client info */
	memset(client_id, 0, sizeof(client_id));
	memset(client_key, 0, sizeof(client_key));
	memset(&plain_backup, 0, sizeof(plain_backup));

	/****************************************************************/
	/* 3. Get Context for this client                               */
	/****************************************************************/
	res = ctx_get_client_key(client_ticket, client_key, client_id);
	if (res) {
		ks_err(KBUILD_MODNAME ": %s: Cannot find context\n",
		       __func__);
		goto free_key;
	}

	/****************************************************************/
	/* 4. Re-generate migration key using the client key and nonce  */
	/****************************************************************/
	keystore_hexdump("rewrap - nonce",
			 nonce, KEYSTORE_MKEY_NONCE_SIZE);

	/* Generate Migration Key */
	res = generate_migration_key(nonce, mkey);
	if (res) {
		ks_err(KBUILD_MODNAME ": %s: generate_migration_key error %d\n",
		       __func__, res);
		goto zero_buf;
	}

	keystore_hexdump("rewrap- MKEY", mkey, KEYSTORE_MKEY_SIZE);

	/****************************************************************/
	/* 5. Use the migration key to decrypt the backup data          */
	/****************************************************************/

	res = unwrap_backup(mkey, backup_mk_enc, REENCRYPTED_BACKUP_SIZE,
			    &plain_backup, sizeof(plain_backup));
	if (res) {
		ks_err(KBUILD_MODNAME ": %s: Cannot unwrap backup\n", __func__);
		goto zero_buf;
	}

	/****************************************************************/
	/* 6. Unwrap the wrapped key using the client-key from backup   */
	/****************************************************************/
	res = unwrap_key(plain_backup.client_key,
			 wrapped_key, wrapped_key_size,
			 &keyspec, plain_app_key);

	if (res) {
		ks_err(KBUILD_MODNAME ": %s: Cannot unwrap appkey\n", __func__);
		goto zero_buf;
	}

	keystore_hexdump("rewrap - plain appkey",
			 plain_app_key,
			 plain_app_key_size);

	/****************************************************************/
	/* 7. Re-wrap bare key using the KSM2 client key (this client)  */
	/****************************************************************/
	res = wrap_key(client_key,
		       plain_app_key, plain_app_key_size,
		       keyspec, rewrapped_key);

	if (res) {
		ks_err(KBUILD_MODNAME ": %s: Cannot wrap new key\n", __func__);
		goto zero_buf;
	}

	ks_debug(KBUILD_MODNAME ": %s: rewrap appkey - done\n", __func__);

zero_buf:
	/* clear local copies of the keys */
	memset(client_key, 0, sizeof(client_key));
	memset(plain_app_key, 0, plain_app_key_size);
free_key:
	kfree(plain_app_key);
exit:
	FUNC_RES(res);
	return res;
}
EXPORT_SYMBOL(keystore_rewrap_key);

