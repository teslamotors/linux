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
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include <security/keystore_api_kernel.h>
#include <security/keystore_api_user.h>

#include "keystore_debug.h"
#include "keystore_tests.h"

/* Universal operation data union */
union keystore_ops_union {
	struct ias_keystore_version version;
	struct ias_keystore_register register_client_type;
	struct ias_keystore_unregister unregister_client;
	struct ias_keystore_wrapped_key_size wrapped_key_size;
	struct ias_keystore_generate_key generate_key;
	struct ias_keystore_wrap_key wrap_key;
	struct ias_keystore_load_key load_key;
	struct ias_keystore_unload_key unload_key;
	struct ias_keystore_crypto_size crypto_size;
	struct ias_keystore_encrypt_decrypt encrypt_decrypt;
	struct ias_keystore_sign_verify sign_verify;
	struct ias_keystore_get_public_key pub_key;
	struct ias_keystore_backup backup;
	struct keystore_ecc_public_key ecc_pub_key;
	struct ias_keystore_ecc_keypair ecc_keys;
	struct ias_keystore_gen_mkey gen_mkey;
	struct ias_keystore_rewrap rewrap;
	struct ias_keystore_verify_signature verify_signature;
	struct ias_keystore_migrate migrate;
	struct ias_keystore_unwrapped_key_size unwrapped_keysize;
	struct ias_keystore_unwrap_with_backup unwrap;
};

static unsigned int is_cmd_supported(unsigned int cmd)
{
	switch (cmd) {
	case KEYSTORE_IOC_VERSION:
	case KEYSTORE_IOC_REGISTER:
	case KEYSTORE_IOC_UNREGISTER:
	case KEYSTORE_IOC_WRAPPED_KEYSIZE:
	case KEYSTORE_IOC_GENERATE_KEY:
	case KEYSTORE_IOC_WRAP_KEY:
	case KEYSTORE_IOC_LOAD_KEY:
	case KEYSTORE_IOC_UNLOAD_KEY:
	case KEYSTORE_IOC_ENCRYPT_SIZE:
	case KEYSTORE_IOC_ENCRYPT:
	case KEYSTORE_IOC_DECRYPT_SIZE:
	case KEYSTORE_IOC_DECRYPT:
	case KEYSTORE_IOC_SIGN_VERIFY_SIZE:
	case KEYSTORE_IOC_SIGN:
	case KEYSTORE_IOC_VERIFY:
	case KEYSTORE_IOC_PUBKEY:
	case KEYSTORE_IOC_BACKUP:
	case KEYSTORE_IOC_GET_KSM_KEY:
	case KEYSTORE_IOC_GEN_MKEY:
	case KEYSTORE_IOC_REWRAP:
#if defined(CONFIG_KEYSTORE_TEST_MIGRATION)
	case KEYSTORE_IOC_GEN_ECC_KEYS:
	case KEYSTORE_IOC_TEST_VERIFY_ECC_SIGNATURE:
	case KEYSTORE_IOC_MIGRATE:
	case KEYSTORE_IOC_UNWRAPPED_KEYSIZE:
	case KEYSTORE_IOC_UNWRAP_WITH_BACKUP:
#endif
#if defined(CONFIG_KEYSTORE_TESTMODE)
	case KEYSTORE_IOC_RUN_TESTS:
#endif
		return 1;
	default:
		return 0;
	}
}

#if defined(CONFIG_KEYSTORE_DEBUG)
static unsigned char *getcmdstr(unsigned int cmd)
{
	switch (cmd) {
	case KEYSTORE_IOC_VERSION:
		return "KEYSTORE_IOC_VERSION";
	case KEYSTORE_IOC_REGISTER:
		return "KEYSTORE_IOC_REGISTER";
	case KEYSTORE_IOC_UNREGISTER:
		return "KEYSTORE_IOC_UNREGISTER";
	case KEYSTORE_IOC_WRAPPED_KEYSIZE:
		return "KEYSTORE_IOC_WRAPPED_KEYSIZE";
	case KEYSTORE_IOC_GENERATE_KEY:
		return "KEYSTORE_IOC_GENERATE_KEY";
	case KEYSTORE_IOC_WRAP_KEY:
		return "KEYSTORE_IOC_WRAP_KEY";
	case KEYSTORE_IOC_LOAD_KEY:
		return "KEYSTORE_IOC_LOAD_KEY";
	case KEYSTORE_IOC_UNLOAD_KEY:
		return "KEYSTORE_IOC_UNLOAD_KEY";
	case KEYSTORE_IOC_ENCRYPT_SIZE:
		return "KEYSTORE_IOC_ENCRYPT_SIZE";
	case KEYSTORE_IOC_ENCRYPT:
		return "KEYSTORE_IOC_ENCRYPT";
	case KEYSTORE_IOC_DECRYPT_SIZE:
		return "KEYSTORE_IOC_DECRYPT_SIZE";
	case KEYSTORE_IOC_DECRYPT:
		return "KEYSTORE_IOC_DECRYPT";
	case KEYSTORE_IOC_SIGN_VERIFY_SIZE:
		return "KEYSTORE_IOC_SIGN_VERIFY_SIZE";
	case KEYSTORE_IOC_SIGN:
		return "KEYSTORE_IOC_SIGN";
	case KEYSTORE_IOC_VERIFY:
		return "KEYSTORE_IOC_VERIFY";
	case KEYSTORE_IOC_PUBKEY:
		return "KEYSTORE_IOC_PUBKEY";
	case KEYSTORE_IOC_BACKUP:
		return "KEYSTORE_IOC_BACKUP";
	case  KEYSTORE_IOC_GET_KSM_KEY:
		return "KEYSTORE_IOC_GET_KSM_KEY";
	case KEYSTORE_IOC_GEN_MKEY:
		return "KEYSTORE_IOC_GEN_MKEY";
	case KEYSTORE_IOC_REWRAP:
		return "KEYSTORE_IOC_REWRAP";
	case KEYSTORE_IOC_GEN_ECC_KEYS:
		return "KEYSTORE_IOC_GEN_ECC_KEYS";
	case KEYSTORE_IOC_TEST_VERIFY_ECC_SIGNATURE:
		return "KEYSTORE_IOC_TEST_VERIFY_ECC_SIGNATURE";
	case KEYSTORE_IOC_MIGRATE:
		return "KEYSTORE_IOC_MIGRATE";
	case KEYSTORE_IOC_UNWRAPPED_KEYSIZE:
		return "KEYSTORE_IOC_UNWRAPPED_KEYSIZE";
	case KEYSTORE_IOC_UNWRAP_WITH_BACKUP:
		return "KEYSTORE_IOC_UNWRAP_WITH_BACKUP";
	case KEYSTORE_IOC_RUN_TESTS:
		return "KEYSTORE_IOC_RUN_TESTS";
	default:
		return "not-supported";
	}
}
#endif

static int version_op(struct ias_keystore_version *user_data)
{
	if (!user_data)
		return -EFAULT;

	user_data->major = KEYSTORE_VERSION_MAJOR;
	user_data->minor = KEYSTORE_VERSION_MINOR;
	user_data->patch = KEYSTORE_VERSION_PATCH;

	return 0;
}

static int register_op(struct ias_keystore_register *user_data)
{
	if (!user_data)
		return -EFAULT;
	return keystore_register(user_data->seed_type,
		 user_data->client_ticket);
}

static int unregister_op(struct ias_keystore_unregister *user_data)
{
	if (!user_data)
		return -EFAULT;
	return keystore_unregister(user_data->client_ticket);
}

static int wrapped_keysize_op(struct ias_keystore_wrapped_key_size *user_data)
{
	if (!user_data)
		return -EFAULT;
	return keystore_wrapped_key_size(user_data->key_spec,
					 &user_data->key_size,
					 &user_data->unwrapped_key_size);
}

static int generate_key_op(struct ias_keystore_generate_key *user_data)
{
	int res = 0;
	unsigned int wrapped_key_size = 0;
	uint8_t *wrapped_key = NULL;

	if (!user_data)
		return -EFAULT;
	res = keystore_wrapped_key_size(user_data->key_spec,
					&wrapped_key_size, NULL);
	if (res)
		return -EINVAL;

	wrapped_key = kmalloc(wrapped_key_size, GFP_KERNEL);
	if (!wrapped_key)
		return -ENOMEM;
	res = keystore_generate_key(user_data->client_ticket,
				    user_data->key_spec,
				    wrapped_key);
	if (res)
		goto free_buf;

	res = copy_to_user(user_data->wrapped_key, wrapped_key,
			   wrapped_key_size);

free_buf:
	kzfree(wrapped_key);
	return res;
}

static int wrap_key_op(struct ias_keystore_wrap_key *user_data)
{
	int res = 0;
	uint8_t *wrapped_key = NULL;
	uint8_t *app_key = NULL;
	unsigned int wrapped_key_size;
	unsigned int unwrapped_key_size;

	if (!user_data)
		return -EFAULT;
	res = keystore_wrapped_key_size(user_data->key_spec,
					&wrapped_key_size, &unwrapped_key_size);
	if (res)
		return -EINVAL;

	wrapped_key = kmalloc(wrapped_key_size, GFP_KERNEL);
	app_key = kmalloc(user_data->app_key_size, GFP_KERNEL);
	if (!wrapped_key || !app_key) {
		res = -ENOMEM;
		goto free_buf;
	}

	res = copy_from_user(app_key, user_data->app_key,
			     user_data->app_key_size);

		res = keystore_wrap_key(user_data->client_ticket,
				app_key,
				user_data->app_key_size,
				user_data->key_spec,
				wrapped_key);
	if (res)
		goto free_buf;

	res = copy_to_user(user_data->wrapped_key, wrapped_key,
			   wrapped_key_size);

free_buf:
	kzfree(wrapped_key);
	kzfree(app_key);
	return res;
}

static int load_key_op(struct ias_keystore_load_key *user_data)
{
	int res = 0;
	uint8_t *wrapped_key = NULL;

	if (!user_data)
		return -EFAULT;

	wrapped_key = kmalloc(user_data->wrapped_key_size, GFP_KERNEL);
	if (!wrapped_key)
		return -ENOMEM;

	res = copy_from_user(wrapped_key,
			     user_data->wrapped_key,
			     user_data->wrapped_key_size);
	if (res) {
		res = -EINVAL;
		goto free_buf;
	}

		res = keystore_load_key(user_data->client_ticket,
				wrapped_key,
				user_data->wrapped_key_size,
				&user_data->slot_id);

	if (res == -EAGAIN) {
		res = copy_to_user(user_data->wrapped_key, wrapped_key,
				user_data->wrapped_key_size);
		if (res == 0)
			res = -EAGAIN;
	}

free_buf:
	kzfree(wrapped_key);
	return res;
}

static int unload_key_op(struct ias_keystore_unload_key *user_data)
{
	int res = 0;

	if (!user_data)
		return -EFAULT;
	res = keystore_unload_key(user_data->client_ticket,
				user_data->slot_id);

	return res;
}

static int encrypt_size_op(struct ias_keystore_crypto_size *user_data)
{
	if (!user_data)
		return -EFAULT;
	return keystore_encrypt_size(user_data->algospec,
		user_data->input_size, &user_data->output_size);
}

static int encrypt_op(struct ias_keystore_encrypt_decrypt *user_data)
{
	int res;
	unsigned int output_size = 0;
	uint8_t *iv = NULL;
	uint8_t *input = NULL;
	uint8_t *output = NULL;

	if (!user_data || !user_data->input || !user_data->output)
		return -EFAULT;

	if (!user_data->input_size)
		return -EINVAL;
	res = keystore_encrypt_size(user_data->algospec,
		user_data->input_size, &output_size);
	if (res)
		return res;

	input = kmalloc(user_data->input_size, GFP_KERNEL);
	output = kmalloc(output_size, GFP_KERNEL);
	if (!input || !output) {
		res = -ENOMEM;
		goto free_buf;
	}

	res = copy_from_user(input, user_data->input, user_data->input_size);
	if (res)
		goto free_buf;

	if (user_data->iv && user_data->iv_size > 0) {
		iv = kmalloc(user_data->iv_size, GFP_KERNEL);
		if (!iv) {
			res = -ENOMEM;
			goto free_buf;
		}

		res = copy_from_user(iv, user_data->iv, user_data->iv_size);
		if (res)
			goto free_buf;
	}

		res = keystore_encrypt(user_data->client_ticket,
				user_data->slot_id,
				user_data->algospec, iv,
				user_data->iv_size,
				input, user_data->input_size,
				output);

	if (res)
		goto free_buf;

	res = copy_to_user(user_data->output, output, output_size);

free_buf:
	kzfree(input);
	kzfree(output);
	kzfree(iv);
	return res;
}

static int decrypt_size_op(struct ias_keystore_crypto_size *user_data)
{
	if (!user_data)
		return -EFAULT;
	return keystore_decrypt_size(user_data->algospec,
			user_data->input_size, &user_data->output_size);
}

static int decrypt_op(struct ias_keystore_encrypt_decrypt *user_data)
{
	int res;
	unsigned int output_size = 0;
	uint8_t *iv = NULL;
	uint8_t *input = NULL;
	uint8_t *output = NULL;

	if (!user_data || !user_data->input || !user_data->output)
		return -EFAULT;

	if (!user_data->input_size)
		return -EINVAL;
	res = keystore_decrypt_size(user_data->algospec,
			user_data->input_size, &output_size);
	if (res)
		return res;

	input = kmalloc(user_data->input_size, GFP_KERNEL);
	output = kmalloc(output_size, GFP_KERNEL);
	if (!input || !output) {
		res = -ENOMEM;
		goto free_buf;
	}

	res = copy_from_user(input, user_data->input, user_data->input_size);
	if (res)
		goto free_buf;

	if (user_data->iv && user_data->iv_size > 0) {
		iv = kmalloc(user_data->iv_size, GFP_KERNEL);
		if (!iv) {
			res = -ENOMEM;
			goto free_buf;
		}

		res = copy_from_user(iv, user_data->iv, user_data->iv_size);
		if (res)
			goto free_buf;
	}

		res = keystore_decrypt(user_data->client_ticket,
			user_data->slot_id, user_data->algospec, iv,
			user_data->iv_size,
			input, user_data->input_size, output);

	if (res)
		goto free_buf;

	res = copy_to_user(user_data->output, output, output_size);

free_buf:
	kzfree(input);
	kzfree(output);
	kzfree(iv);
	return res;
}

static int sign_verify_size_op(struct ias_keystore_crypto_size *user_data)
{
	if (!user_data)
		return -EFAULT;

	return keystore_sign_verify_size(user_data->algospec,
				     &user_data->output_size);
}

static int sign_op(struct ias_keystore_sign_verify *user_data)
{
	int res;
	unsigned int signature_size = 0;
	uint8_t *input = NULL;
	uint8_t *signature = NULL;

	if (!user_data)
		return -EFAULT;

	res = keystore_sign_verify_size(user_data->algospec,
				    &signature_size);
	if (res)
		return res;

	input = kmalloc(user_data->input_size, GFP_KERNEL);
	signature = kmalloc(signature_size, GFP_KERNEL);
	if (!input || !signature) {
		res = -ENOMEM;
		goto free_buf;
	}

	res = copy_from_user(input, user_data->input, user_data->input_size);
	if (res)
		goto free_buf;

	res = keystore_sign(user_data->client_ticket, user_data->slot_id,
				user_data->algospec,
				input, user_data->input_size,
				signature);

	if (res)
		goto free_buf;

	res = copy_to_user(user_data->signature, signature, signature_size);

free_buf:
	kzfree(input);
	kzfree(signature);
	return res;
}

static int verify_op(struct ias_keystore_sign_verify *user_data)
{
	int res;
	unsigned int signature_size = 0;
	uint8_t *input = NULL;
	uint8_t *signature = NULL;

	if (!user_data)
		return -EFAULT;

	res = keystore_sign_verify_size(user_data->algospec,
				    &signature_size);
	if (res)
		return res;

	input = kmalloc(user_data->input_size, GFP_KERNEL);
	signature = kmalloc(signature_size, GFP_KERNEL);
	if (!input || !signature) {
		res = -ENOMEM;
		goto free_buf;
	}

	res = copy_from_user(input, user_data->input, user_data->input_size);
	if (res)
		goto free_buf;

	res = copy_from_user(signature, user_data->signature, signature_size);
	if (res)
		goto free_buf;

	res = keystore_verify(user_data->client_ticket, user_data->slot_id,
			       user_data->algospec,
			       input, user_data->input_size,
			       signature);
free_buf:
	kzfree(input);
	kzfree(signature);
	return res;
}

static int pubkey_op(struct ias_keystore_get_public_key *user_data)
{
	int res;
	uint8_t *unwrapped_key = NULL;
	unsigned int wrapped_size, unwrapped_size;

	if (!user_data)
		return -EFAULT;

	/* Call once to extract the key_spec of the key in slot */
	res = keystore_get_public_key(user_data->client_ticket,
				      user_data->slot_id,
				      &user_data->key_spec,
				      NULL);

	/* Return on error or if the unwrapped key is NULL:     */
	/* A NULL unwrapped_key indicates the user only wants   */
	/* to extract the key_spec.                             */
	if (res || !user_data->unwrapped_key)
		return res;

	res = keystore_wrapped_key_size(user_data->key_spec,
					&wrapped_size, &unwrapped_size);
	if (res)
		return res;

	unwrapped_key = kmalloc(unwrapped_size, GFP_KERNEL);
	if (!unwrapped_key)
		return -ENOMEM;

	res = keystore_get_public_key(user_data->client_ticket,
				      user_data->slot_id,
				      &user_data->key_spec,
				      unwrapped_key);
	if (res)
		goto free_buf;

	res = copy_to_user(user_data->unwrapped_key, unwrapped_key,
			   unwrapped_size);

free_buf:
	kzfree(unwrapped_key);
	return res;
}

static int backup_op(struct ias_keystore_backup *user_data)
{
	int res = 0;

	if (!user_data)
		return -EFAULT;

	res = keystore_backup(user_data->client_ticket,
			      &user_data->key_enc_backup,
			      user_data->key_enc_backup_sig,
			      user_data->backup_enc,
			      &user_data->backup_enc_sig);

	return res;
}

static int gen_mkey_op(struct ias_keystore_gen_mkey *user_data)
{
	int res = 0;

	if (!user_data)
		return -EFAULT;

	res = keystore_generate_mkey(&user_data->key_enc_mkey,
				     user_data->key_enc_mkey_sig,
				     user_data->mkey_enc,
				     &user_data->mkey_sig,
				     user_data->nonce);
	return res;
}

static int get_ksm_key_op(struct keystore_ecc_public_key *user_data)
{
	if (!user_data)
		return -EFAULT;

	return keystore_get_ksm_key(user_data);
}

static int keystore_rewrap_op(struct ias_keystore_rewrap *user_data)
{
	int res = 0;
	uint8_t *wrapped_key = NULL;
	uint8_t *rewrapped_key = NULL;

	if (!user_data)
		return -EFAULT;

	wrapped_key = kmalloc(user_data->wrapped_key_size, GFP_KERNEL);
	rewrapped_key = kmalloc(user_data->wrapped_key_size, GFP_KERNEL);
	if (!wrapped_key || !rewrapped_key) {
		res = -ENOMEM;
		goto free_buf;
	}

	res = copy_from_user(wrapped_key, user_data->wrapped_key,
			     user_data->wrapped_key_size);
	if (res)
		goto free_buf;

	res = keystore_rewrap_key(user_data->client_ticket,
				  user_data->backup_mk_enc,
				  user_data->nonce,
				  wrapped_key,
				  user_data->wrapped_key_size,
				  rewrapped_key);
	if (res)
		goto free_buf;

	res = copy_to_user(user_data->rewrapped_key, rewrapped_key,
			   user_data->wrapped_key_size);

free_buf:
	kzfree(wrapped_key);
	kzfree(rewrapped_key);
	return res;
}

#if defined(CONFIG_KEYSTORE_TEST_MIGRATION)
static int gen_ecc_keys_op(struct ias_keystore_ecc_keypair *data)
{
	if (!data)
		return -EFAULT;

	return keystore_gen_ecc_keys(data);
}

static int keystore_test_verify_ecc_signature_op(
	struct ias_keystore_verify_signature *user_data)
{
	int res = 0;
	uint8_t *data = NULL;

	if (!user_data)
		return -EFAULT;

	data = kmalloc(user_data->data_size, GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	res = copy_from_user(data, user_data->data, user_data->data_size);
	if (res)
		goto free_buf;

	res = keystore_test_verify_ecc_signature(&user_data->public_key,
						 data,
						 user_data->data_size,
						 &user_data->sig);
free_buf:
	kzfree(data);
	return res;
}

static int unwrapped_keysize_op(struct ias_keystore_unwrapped_key_size
				*user_data)
{
	int res = 0;

	if (!user_data)
		return -EFAULT;

	res = keystore_unwrapped_key_size(user_data->wrapped_size,
					  &user_data->unwrapped_size);

	return res;
}

static int unwrap_op(struct ias_keystore_unwrap_with_backup *user_data)
{
	int res = 0;
	uint8_t *wrapped_key = NULL;
	uint8_t *unwrapped_key = NULL;
	unsigned int unwrapped_key_size;

	if (!user_data)
		return -EFAULT;

	res = keystore_unwrapped_key_size(user_data->wrapped_key_size,
					  &unwrapped_key_size);
	if (res)
		return -EINVAL;

	wrapped_key = kmalloc(user_data->wrapped_key_size, GFP_KERNEL);
	unwrapped_key = kmalloc(unwrapped_key_size, GFP_KERNEL);
	if (!wrapped_key || !unwrapped_key) {
		res = -ENOMEM;
		goto free_buf;
	}

	res = copy_from_user(wrapped_key, user_data->wrapped_key,
			     user_data->wrapped_key_size);
	if (res)
		goto free_buf;

	res = keystore_unwrap_with_backup(user_data->key_enc_backup,
					  user_data->backup_enc,
					  wrapped_key,
					  user_data->wrapped_key_size,
					  &user_data->keyspec,
					  unwrapped_key);
	if (res)
		goto free_buf;

	res = copy_to_user(user_data->unwrapped_key, unwrapped_key,
			   unwrapped_key_size);

free_buf:
	kzfree(wrapped_key);
	kzfree(unwrapped_key);
	return res;
}

static int migrate_op(struct ias_keystore_migrate *user_data)
{
	int res = 0;

	if (!user_data)
		return -EFAULT;

	res = keystore_migrate(user_data->key_enc_backup,
			       user_data->backup_enc,
			       user_data->key_enc_mkey,
			       user_data->mkey_enc,
			       user_data->backup_mk_enc);

	return res;
}
#endif

/**
 * keystore_ioctl - the ioctl function
 *
 * @param file pointer to file structure
 * @param cmd command to execute
 * @param arg user space pointer to arguments structure
 *
 * @returns 0 on success, <0 on error
 */
long keystore_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	union keystore_ops_union op;
	unsigned int size;
	int res = 0;

	FUNC_BEGIN;

	ks_debug(KBUILD_MODNAME ": %s - cmd = 0x%x - %s\n",
		 __func__, cmd, getcmdstr(cmd));

	/* copy data structure from user space to system */
	size = _IOC_SIZE(cmd);

	if (!is_cmd_supported(cmd) ||
	    (size > sizeof(union keystore_ops_union))) {
		ks_err(KBUILD_MODNAME ": %s - cmd=%u unknown, size:%d, size(keystore_ops_union): %zu\n",
		       __func__, cmd, size, sizeof(union keystore_ops_union));
		return -ENOIOCTLCMD;
	}
	if (!access_ok(VERIFY_READ, (void *)arg, size)) {
		ks_err(KBUILD_MODNAME ": %s - cmd=%u no read access\n",
		       __func__, cmd);
		return -EFAULT;
	}
	if ((_IOC_DIR(cmd) & _IOC_READ) &&
	    (!access_ok(VERIFY_WRITE, (void *)arg, size))) {
		ks_err(KBUILD_MODNAME ": %s - cmd=%u no write access\n",
		       __func__, cmd);
		return -EFAULT;
	}
	if (copy_from_user(&op, (void *)arg, size)) {
		ks_err(KBUILD_MODNAME ": %s - cannot copy data\n",
		       __func__);
		return -EFAULT;
	}

	switch (cmd) {
	case KEYSTORE_IOC_VERSION:
		res = version_op(&op.version);
	case KEYSTORE_IOC_REGISTER:
		res = register_op(&op.register_client_type);
		break;

	case KEYSTORE_IOC_UNREGISTER:
		res = unregister_op(&op.unregister_client);
		break;

	case KEYSTORE_IOC_WRAPPED_KEYSIZE:
		res = wrapped_keysize_op(&op.wrapped_key_size);
		break;

	case KEYSTORE_IOC_GENERATE_KEY:
		res = generate_key_op(&op.generate_key);
		break;

	case KEYSTORE_IOC_WRAP_KEY:
		res = wrap_key_op(&op.wrap_key);
		break;

	case KEYSTORE_IOC_LOAD_KEY:
		res = load_key_op(&op.load_key);
		break;

	case KEYSTORE_IOC_UNLOAD_KEY:
		res = unload_key_op(&op.unload_key);
		break;

	case KEYSTORE_IOC_ENCRYPT_SIZE:
		res = encrypt_size_op(&op.crypto_size);
		break;

	case KEYSTORE_IOC_ENCRYPT:
		res = encrypt_op(&op.encrypt_decrypt);
		break;

	case KEYSTORE_IOC_DECRYPT_SIZE:
		res = decrypt_size_op(&op.crypto_size);
		break;

	case KEYSTORE_IOC_DECRYPT:
		res = decrypt_op(&op.encrypt_decrypt);
		break;

	case KEYSTORE_IOC_SIGN_VERIFY_SIZE:
		res = sign_verify_size_op(&op.crypto_size);
		break;

	case KEYSTORE_IOC_SIGN:
		res = sign_op(&op.sign_verify);
		break;

	case KEYSTORE_IOC_VERIFY:
		res = verify_op(&op.sign_verify);
		break;

	case KEYSTORE_IOC_PUBKEY:
		res = pubkey_op(&op.pub_key);
		break;

	case KEYSTORE_IOC_BACKUP:
		res = backup_op(&op.backup);
		break;

	case KEYSTORE_IOC_GET_KSM_KEY:
		res = get_ksm_key_op(&op.ecc_pub_key);
		break;

	case KEYSTORE_IOC_GEN_MKEY:
		res = gen_mkey_op(&op.gen_mkey);
		break;

	case KEYSTORE_IOC_REWRAP:
		res = keystore_rewrap_op(&op.rewrap);
		break;

	case KEYSTORE_IOC_RUN_TESTS:
		res = keystore_run_tests();
		break;

#if defined(CONFIG_KEYSTORE_TEST_MIGRATION)
	case KEYSTORE_IOC_GEN_ECC_KEYS:
		res = gen_ecc_keys_op(&op.ecc_keys);
		break;

	case KEYSTORE_IOC_TEST_VERIFY_ECC_SIGNATURE:
		res = keystore_test_verify_ecc_signature_op(
			&op.verify_signature);
		break;

	case KEYSTORE_IOC_UNWRAPPED_KEYSIZE:
		res = unwrapped_keysize_op(&op.unwrapped_keysize);
		break;

	case KEYSTORE_IOC_UNWRAP_WITH_BACKUP:
		res = unwrap_op(&op.unwrap);
		break;

	case KEYSTORE_IOC_MIGRATE:
		res = migrate_op(&op.migrate);
		break;
#endif
	default:
		res = -ENOIOCTLCMD;
		ks_err(KBUILD_MODNAME ": %s - cmd=%u not known\n",
		       __func__, cmd);
		break;
	}

	/* check if OK and we need to return some data in memory block */
	if ((res >= 0) && (_IOC_DIR(cmd) & _IOC_READ)) {
		/* then copy data from system to user space */
		if (copy_to_user((void *)arg, &op, size)) {
			ks_err(KBUILD_MODNAME ": %s - cannot copy data\n",
			       __func__);
			res = -EFAULT;
		}
	}

	memset(&op, 0, sizeof(op));
	FUNC_RES(res);
	return res;
}

/* end of file */
