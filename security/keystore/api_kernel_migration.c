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
#include "keystore_constants.h"
#include "keystore_context_safe.h"
#include "keystore_rand.h"
#include "keystore_operations.h"

#include "keystore_debug.h"

int keystore_generate_mkey(const struct keystore_ecc_public_key *oem_pub,
			   const uint8_t *oem_pub_sig,
			   uint8_t *emkey,
			   struct keystore_ecc_signature *mkey_sig,
			   uint8_t *mkey_nonce)
{
	int res;
	uint8_t mkey[KEYSTORE_MKEY_SIZE] = {0};

	FUNC_BEGIN;

	if (!mkey_sig || !mkey_nonce) {
		ks_err(KBUILD_MODNAME ": %s: [ptr] oem_pub: %p, emkey: %p, mkey_sig: %p, mkey_nonce: %p\n",
		       __func__, oem_pub, emkey, mkey_sig, mkey_nonce);
		res = -EFAULT;
		goto exit;
	}

	/* verify public key signature */
	res = verify_oem_signature(oem_pub,
				   sizeof(struct keystore_ecc_public_key),
				   oem_pub_sig, RSA_SIGNATURE_BYTE_SIZE);

	if (res) {
		ks_err(KBUILD_MODNAME ": %s: OEMpub2Sig verfifcation: Failed\n",
		       __func__);
		goto exit;
	}

	/* use RDRAND to get the nonce */
	res = keystore_get_rdrand(mkey_nonce, KEYSTORE_MKEY_NONCE_SIZE);
	if (res != 0) {
		ks_err(KBUILD_MODNAME ": Random number generator test error %d\n",
		       res);
		goto exit;
	}

	keystore_hexdump("migration - nonce", mkey_nonce,
			 KEYSTORE_MKEY_NONCE_SIZE);

	/* Generate Migration Key */
	res = generate_migration_key(mkey_nonce, mkey);
	if (res) {
		ks_err(KBUILD_MODNAME ": %s: generate_migration_key error %d\n",
		       __func__, res);
		goto exit;
	}

	res = encrypt_for_host(oem_pub, mkey, KEYSTORE_MKEY_SIZE,
			       emkey, KEYSTORE_EMKEY_SIZE);
	if (res < 1) {
		ks_err(KBUILD_MODNAME
		       ": keystore_ecc_encrypt() error %d in %s\n",
		       res, __func__);
		goto zero_buf;
	}

	res = sign_for_host(emkey, KEYSTORE_EMKEY_SIZE, mkey_sig);

#ifdef DEBUG
	ks_info(KBUILD_MODNAME ": migration - ECC signature verification - OK\n");
	ks_info(KBUILD_MODNAME ": migration - Signing MKEY with KSM2 ECC private - OK\n");
#endif

zero_buf:
	/* clear local secure data */
	memset(mkey, 0, KEYSTORE_MKEY_SIZE);
exit:
	FUNC_RES(res);
	return res;
}
EXPORT_SYMBOL(keystore_generate_mkey);

int keystore_rewrap_key(const uint8_t *client_ticket,
			const uint8_t *backup_emkey,
			const uint8_t *mkey_nonce,
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
	if (!client_ticket || !backup_emkey || !mkey_nonce ||
	    !wrapped_key || !rewrapped_key) {
		ks_err(KBUILD_MODNAME ": %s: NULL pointer: client_ticket: %p, backup_emkey: %p, mkey_nonce: %p, wrapped_key: %p, rewrapped_key: %p\n",
		       __func__, client_ticket, backup_emkey, mkey_nonce,
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
			 mkey_nonce, KEYSTORE_MKEY_NONCE_SIZE);

	/* Generate Migration Key */
	res = generate_migration_key(mkey_nonce, mkey);
	if (res) {
		ks_err(KBUILD_MODNAME ": %s: generate_migration_key error %d\n",
		       __func__, res);
		goto zero_buf;
	}

	keystore_hexdump("rewrap- MKEY", mkey, KEYSTORE_MKEY_SIZE);

	/****************************************************************/
	/* 5. Use the migration key to decrypt the backup data          */
	/****************************************************************/

	res = unwrap_backup(mkey, backup_emkey, REENCRYPTED_BACKUP_SIZE,
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

