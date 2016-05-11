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
#include <security/sb.h>

#include "keystore_debug.h"
#include "keystore_client.h"
#include "keystore_context_safe.h"
#include "keystore_operations.h"

#include "keystore_rand.h"
#ifdef CONFIG_APPLICATION_AUTH
#include "appauth/manifest_verify.h"
#endif

int keystore_register(enum keystore_seed_type seed_type, uint8_t *client_ticket)
{
	uint8_t client_key[KEYSTORE_CLIENT_KEY_SIZE];
	uint8_t client_id[KEYSTORE_MAX_CLIENT_ID_SIZE];
	int res = 0;

	FUNC_BEGIN;

#ifdef CONFIG_APPLICATION_AUTH
	ks_debug(KBUILD_MODNAME ": calling verify_manifest\n");
	verify_manifest_file("/opt/ias/bin/manifest", 0, 0);
#endif

	/*
	 * Check for secure boot status.
	 *
	 * On secure boot disabled systems, /dev/keystore device is not created
	 * So userspace clients wont be functional. But kernel-space clients
	 * can still call api's so check for sb-status and act accordingly.
	 */
#ifdef CONFIG_KEYSTORE_SECURE_BOOT_IGNORE
	ks_warn(KBUILD_MODNAME ": Ignoring secure boot status!\n");
#else
	if (!get_sb_stat()) {
		ks_err(KBUILD_MODNAME ": Cannot register with keystore - secure boot not enabled\n");
		res = -EPERM;
		goto exit;
	}
#endif

	if (!client_ticket) {
		res = -EFAULT;
		goto exit;
	}

	/* Calculate the Client ID */
	res = keystore_calc_clientid(client_id, sizeof(client_id));
	if (res) {
		ks_err(KBUILD_MODNAME ": %s Error calculating client ID: %d\n",
		       __func__, res);
		goto exit;
	}

	/* Derive the client key */
	res = keystore_calc_clientkey(seed_type,
				      client_id, sizeof(client_id),
				      client_key, sizeof(client_key));
	if (res) {
		ks_err(KBUILD_MODNAME ": %s Error deriving the client key %d\n",
		       __func__, res);
		goto zero_key;
	}

	/* randomize a buffer */
	keystore_get_rdrand(client_ticket, KEYSTORE_CLIENT_TICKET_SIZE);

	/* Add the new client to the context */
	res = ctx_add_client(client_ticket, client_key, client_id);
	if (res) {
		ks_err(KBUILD_MODNAME ": %s: Cannot allocate context\n",
		       __func__);
		goto zero_key;
	}

zero_key:
	/* clear local secure data */
	/* Note client ID is not sensitive as it is just hash of the path */
	memset(client_key, 0, sizeof(client_key));
exit:
	FUNC_RES(res);
	return res;
}
EXPORT_SYMBOL(keystore_register);

int keystore_unregister(const uint8_t *client_ticket)
{
	int res = 0;

	FUNC_BEGIN;

	res = ctx_remove_client(client_ticket);

	if (res)
		ks_err(KBUILD_MODNAME ": %s: Cannot find context\n",
		       __func__);

	FUNC_RES(res);
	return res;
}
EXPORT_SYMBOL(keystore_unregister);

int keystore_wrapped_key_size(enum keystore_key_spec keyspec,
			      unsigned int *size,
			      unsigned int *unwrapped_size)
{
	int res = 0;

	FUNC_BEGIN;
	res = keyspec_to_wrapped_keysize(keyspec, size);
	if (res)
		goto exit;

	/* Also copy the unwrapped size if not null */
	if (unwrapped_size)
		res = unwrapped_key_size(*size, unwrapped_size);

exit:
	FUNC_RES(res);
	return res;
}
EXPORT_SYMBOL(keystore_wrapped_key_size);

int keystore_generate_key(const uint8_t *client_ticket,
			  const enum keystore_key_spec keyspec,
			  uint8_t *wrapped_key)
{
	uint8_t client_key[KEYSTORE_CLIENT_KEY_SIZE];
	uint8_t *new_key = NULL;
	unsigned int new_key_size = 0;
	int res = 0;

	FUNC_BEGIN;

	/* Cache the client key */
	res = ctx_get_client_key(client_ticket, client_key, NULL);
	if (res) {
		ks_err(KBUILD_MODNAME ": %s: Cannot find context\n",
		       __func__);
		goto key_clear;
	}

	/* Generate a key */
	new_key = generate_new_key(keyspec, &new_key_size);
	if (res) {
		ks_err(KBUILD_MODNAME ": %s: Error generating client key\n",
		       __func__);
		return -ENOMEM;
	}

	/* Wrap the key */
	res = wrap_key(client_key, new_key, new_key_size, keyspec, wrapped_key);
	if (res) {
		ks_err(KBUILD_MODNAME ": %s: Cannot wrap new key\n", __func__);
		goto key_free;
	}

key_free:
	kzfree(new_key);
key_clear:
	/* clear local copies of the keys */
	memset(client_key, 0, sizeof(client_key));
	FUNC_RES(res);
	return res;
}
EXPORT_SYMBOL(keystore_generate_key);

int keystore_wrap_key(const uint8_t *client_ticket,
		      const uint8_t *app_key, unsigned int app_key_size,
		      enum keystore_key_spec keyspec, uint8_t *wrapped_key)
{
	uint8_t client_key[KEYSTORE_CLIENT_KEY_SIZE];
	int res = 0;

	FUNC_BEGIN;

	/* Cache the client key */
	res = ctx_get_client_key(client_ticket, client_key, NULL);
	if (res) {
		ks_err(KBUILD_MODNAME ": %s: Cannot find context\n",
		       __func__);
		goto exit;
	}

	res = wrap_key(client_key, app_key, app_key_size, keyspec, wrapped_key);
	if (res) {
		ks_err(KBUILD_MODNAME ": %s: Cannot wrap new key\n", __func__);
		goto key_clear;
	}

key_clear:
	/* clear local copies of the keys */
	memset(client_key, 0, sizeof(client_key));
exit:
	FUNC_RES(res);
	return res;
}
EXPORT_SYMBOL(keystore_wrap_key);

int keystore_load_key(const uint8_t *client_ticket, const uint8_t *wrapped_key,
		      unsigned int wrapped_key_size, unsigned int *slot_id)
{
	uint8_t client_key[KEYSTORE_CLIENT_KEY_SIZE];
	uint8_t app_key[KEYSTORE_MAX_APPKEY_SIZE];
	unsigned int app_key_size = 0;
	int res = 0;
	enum keystore_key_spec keyspec = KEYSPEC_INVALID;

	FUNC_BEGIN;

	if (!slot_id)
		return -EFAULT;

	res = unwrapped_key_size(wrapped_key_size, &app_key_size);
	if (res)
		goto exit;

	if (app_key_size > KEYSTORE_MAX_APPKEY_SIZE) {
		ks_err(KBUILD_MODNAME ": %s: app_key_size %u, expected < %u",
		       __func__, app_key_size, KEYSTORE_MAX_APPKEY_SIZE);
		res = -EINVAL;
		goto exit;
	}

	res = ctx_get_client_key(client_ticket, client_key, NULL);
	if (res) {
		ks_err(KBUILD_MODNAME ": %s: Cannot find context\n",
		       __func__);
		goto exit;
	}

	res = unwrap_key(client_key, wrapped_key, wrapped_key_size,
			 &keyspec, app_key);
	if (res) {
		ks_err(KBUILD_MODNAME ": %s: Unwrap key failed\n", __func__);
		goto key_clear;
	}

	res = ctx_add_app_key(client_ticket, keyspec, app_key, app_key_size,
			      slot_id);
	if (res) {
		ks_err(KBUILD_MODNAME ": %s: Insert key to slot failed\n",
		       __func__);
		goto key_clear;
	}

key_clear:
	/* clear local copies of the keys */
	memset(client_key, 0, sizeof(client_key));
exit:
	FUNC_RES(res);
	return res;
}
EXPORT_SYMBOL(keystore_load_key);

int keystore_unload_key(const uint8_t *client_ticket, unsigned int slot_id)
{
	int res = 0;

	FUNC_BEGIN;

	res = ctx_remove_app_key(client_ticket, slot_id);

	FUNC_RES(res);
	return res;
}
EXPORT_SYMBOL(keystore_unload_key);

int keystore_encrypt_size(enum keystore_algo_spec algo_spec,
			  unsigned int input_size, unsigned int *output_size)
{
	int res = 0;

	FUNC_BEGIN;
	res = encrypt_output_size(algo_spec, input_size, output_size);

	FUNC_RES(res);
	return res;
}
EXPORT_SYMBOL(keystore_encrypt_size);

int keystore_encrypt(const uint8_t *client_ticket, int slot_id,
		     enum keystore_algo_spec algo_spec,
		     const uint8_t *iv, unsigned int iv_size,
		     const uint8_t *input, unsigned int input_size,
		     uint8_t *output)
{
	uint8_t app_key[KEYSTORE_MAX_APPKEY_SIZE];
	unsigned int app_key_size = sizeof(app_key);
	enum keystore_key_spec key_spec = KEYSPEC_INVALID;
	int res = 0;

	FUNC_BEGIN;

	ks_debug(KBUILD_MODNAME
		 ": keystore_encrypt slot_id=%d algo_spec=%d iv_size=%u isize=%u\n",
		 slot_id, (int)algo_spec, iv_size, input_size);

	/* Get the application key */
	res = ctx_get_app_key(client_ticket, slot_id, &key_spec,
			      app_key, &app_key_size);
	if (res) {
		ks_err(KBUILD_MODNAME ": %s: Error %d getting app key.\n",
		       __func__, res);
		goto exit;
	}

	/* Encrypt */
	res = do_encrypt(algo_spec, app_key, app_key_size, iv, iv_size,
			 input, input_size, output);
	if (res) {
		ks_err(KBUILD_MODNAME ": %s: Error %d in do_encrypt\n",
		       __func__, res);
		goto zero_buf;
	}

zero_buf:
	/* clear local copy of AppKey */
	memset(app_key, 0, sizeof(app_key));
exit:
	FUNC_RES(res);
	return res;
}
EXPORT_SYMBOL(keystore_encrypt);

int keystore_decrypt_size(enum keystore_algo_spec algo_spec,
			  unsigned int input_size, unsigned int *output_size)
{
	return decrypt_output_size(algo_spec, input_size, output_size);
}
EXPORT_SYMBOL(keystore_decrypt_size);

int keystore_decrypt(const uint8_t *client_ticket, int slot_id,
		     enum keystore_algo_spec algo_spec,
		     const uint8_t *iv, unsigned int iv_size,
		     const uint8_t *input, unsigned int input_size,
		     uint8_t *output)
{
	uint8_t app_key[KEYSTORE_MAX_APPKEY_SIZE];
	unsigned int app_key_size = sizeof(app_key);
	enum keystore_key_spec key_spec = KEYSPEC_INVALID;
	int res = 0;

	FUNC_BEGIN;

	/* Get the application key */
	res = ctx_get_app_key(client_ticket, slot_id, &key_spec,
			      app_key, &app_key_size);
	if (res) {
		ks_err(KBUILD_MODNAME ": %s: Error %d getting app key.\n",
		       __func__, res);
		goto exit;
	}

	res = do_decrypt(algo_spec, app_key, app_key_size, iv, iv_size,
			 input, input_size, output);
	if (res) {
		ks_err(KBUILD_MODNAME ": %s: Error %d in do_decrypt\n",
		       __func__, res);
		goto zero_buf;
	}

zero_buf:
	/* clear local copy of AppKey */
	memset(app_key, 0, sizeof(app_key));
exit:
	FUNC_RES(res);
	return res;
}
EXPORT_SYMBOL(keystore_decrypt);

int keystore_sign_verify_size(enum keystore_algo_spec algo_spec,
			      unsigned int *signature_size)
{
	int res = 0;

	FUNC_BEGIN;
	res = signature_input_output_size(algo_spec, signature_size);

	FUNC_RES(res);
	return res;
}
EXPORT_SYMBOL(keystore_sign_verify_size);

int keystore_sign(const uint8_t *client_ticket, int slot_id,
		  enum keystore_algo_spec algo_spec,
		  const uint8_t *input, unsigned int input_size,
		  uint8_t *signature)
{
	uint8_t app_key[KEYSTORE_MAX_APPKEY_SIZE];
	unsigned int app_key_size = sizeof(app_key);
	enum keystore_key_spec key_spec = KEYSPEC_INVALID;
	int res = 0;

	FUNC_BEGIN;

	ks_debug(KBUILD_MODNAME
		 ": keystore_sign slot_id=%d algo_spec=%d isize=%u\n",
		 slot_id, (int)algo_spec, input_size);

	/* Get the application key */
	res = ctx_get_app_key(client_ticket, slot_id, &key_spec,
			      app_key, &app_key_size);
	if (res) {
		ks_err(KBUILD_MODNAME ": %s: Error %d getting app key.\n",
		       __func__, res);
		goto exit;
	}

	/* Sign */
	res = do_sign(algo_spec, app_key, app_key_size,
			 input, input_size, signature);
	if (res) {
		ks_err(KBUILD_MODNAME ": %s: Error %d in do_sign\n",
		       __func__, res);
		goto zero_buf;
	}

zero_buf:
	/* clear local copy of AppKey */
	memset(app_key, 0, sizeof(app_key));
exit:
	FUNC_RES(res);
	return res;
}
EXPORT_SYMBOL(keystore_sign);

int keystore_verify(const uint8_t *client_ticket, int slot_id,
		    enum keystore_algo_spec algo_spec,
		    const uint8_t *input, unsigned int input_size,
		    uint8_t *signature)
{
	uint8_t app_key[KEYSTORE_MAX_APPKEY_SIZE];
	unsigned int app_key_size = sizeof(app_key);
	enum keystore_key_spec key_spec = KEYSPEC_INVALID;
	int res = 0;

	FUNC_BEGIN;

	ks_debug(KBUILD_MODNAME
		 ": keystore_verify slot_id=%d algo_spec=%d isize=%u\n",
		 slot_id, (int)algo_spec, input_size);

	/* Get the application key */
	res = ctx_get_app_key(client_ticket, slot_id, &key_spec,
			      app_key, &app_key_size);
	if (res) {
		ks_err(KBUILD_MODNAME ": %s: Error %d getting app key.\n",
		       __func__, res);
		goto exit;
	}

	/* Verify */
	res = do_verify(algo_spec, app_key, app_key_size,
			input, input_size, signature);
	if (res) {
		ks_err(KBUILD_MODNAME ": %s: Error %d in do_verify\n",
		       __func__, res);
		goto zero_buf;
	}

zero_buf:
	/* clear local copy of AppKey */
	memset(app_key, 0, sizeof(app_key));
exit:
	FUNC_RES(res);
	return res;
}
EXPORT_SYMBOL(keystore_verify);

int keystore_get_public_key(const uint8_t *client_ticket, int slot_id,
			    enum keystore_key_spec *key_spec,
			    uint8_t *unwrapped_key)
{
	uint8_t app_key[KEYSTORE_MAX_APPKEY_SIZE];
	unsigned int app_key_size = sizeof(app_key);

	int res = 0;

	FUNC_BEGIN;

	ks_debug(KBUILD_MODNAME
		 ": keystore_get_public_key slot_id=%d\n", slot_id);

	if (!key_spec) {
		res = -EFAULT;
		goto exit;
	}

	/* Get the application key */
	res = ctx_get_app_key(client_ticket, slot_id, key_spec,
			      app_key, &app_key_size);
	if (res) {
		ks_err(KBUILD_MODNAME ": %s: Error %d getting app key.\n",
		       __func__, res);
		goto exit;
	}

	/* A null unwrapped_key: assume the caller just wants the key_spec */
	if (!unwrapped_key) {
		ks_debug(KBUILD_MODNAME ": Key spec: %u\n", *key_spec);
		res = 0;
		goto zero_buf;
	}

	res = get_public_key(*key_spec, app_key, app_key_size, unwrapped_key);
	if (res) {
		ks_err(KBUILD_MODNAME ": %s: Error %d in get_public_key\n",
		       __func__, res);
	}

zero_buf:
	/* clear local copy of AppKey */
	memset(app_key, 0, sizeof(app_key));
exit:
	FUNC_RES(res);
	return res;
}
EXPORT_SYMBOL(keystore_get_public_key);

int keystore_get_ksm_key(struct keystore_ecc_public_key *public_key)
{
	int res = 0;
	struct ias_keystore_ecc_keypair key_pair;

	FUNC_BEGIN;

	if (!public_key)
		return -EFAULT;

	res = keystore_get_ksm_keypair(&key_pair);

	memset(key_pair.private_key, 0, sizeof(key_pair.private_key));
	memcpy(public_key, &key_pair.public_key,
	       sizeof(struct keystore_ecc_public_key));

	FUNC_RES(res);
	return res;
}
EXPORT_SYMBOL(keystore_get_ksm_key);

int keystore_backup(const uint8_t *client_ticket,
		    const struct keystore_ecc_public_key *key_enc_backup,
		    const uint8_t *key_enc_backup_sig,
		    uint8_t *backup_enc,
		    struct keystore_ecc_signature *backup_enc_sig)
{
	int res = 0;
	int backup_size = 0;

	struct keystore_backup_data backup;

	FUNC_BEGIN;

	res = verify_oem_signature(key_enc_backup,
				   sizeof(struct keystore_ecc_public_key),
				   key_enc_backup_sig, RSA_SIGNATURE_BYTE_SIZE);
	if (res)
		goto exit;

	res = ctx_take_backup(client_ticket, &backup);
	if (res) {
		ks_err(KBUILD_MODNAME ": Error taking backup.\n");
		goto zero_backup;
	}

	keystore_hexdump("backup - clientid",
			 backup.client_id, sizeof(backup.client_id));
	keystore_hexdump("backup - client-key",
			 backup.client_key, sizeof(backup.client_key));

	/* Encrypt the backup with the backup ECC key */
	res = encrypt_for_host(key_enc_backup, &backup, sizeof(backup),
			       backup_enc, KEYSTORE_BACKUP_SIZE);
	if (res) {
		ks_err(KBUILD_MODNAME ": %s: Error %d in keystore_ecc_encrypt\n",
		       __func__, res);
		goto zero_backup;
	}

	keystore_hexdump("Encrypted backup", backup_enc_sig,
			 KEYSTORE_BACKUP_SIZE);

	backup_size = res; /* store size to return back to calling func */

	res = sign_for_host(backup_enc, KEYSTORE_BACKUP_SIZE, backup_enc_sig);
	if (res) {
		ks_err(KBUILD_MODNAME ": %s: Error %d in keystore_ecc_encrypt\n",
		       __func__, res);
	}

	keystore_hexdump("Encrypted backup signature", backup_enc_sig,
			 sizeof(struct keystore_ecc_signature));

zero_backup:
	memset(&backup, 0, sizeof(backup));/* empty */
exit:
	FUNC_RES(res);
	return res;
}
EXPORT_SYMBOL(keystore_backup);

/* end of file */
