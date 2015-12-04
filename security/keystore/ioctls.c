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
#include "keystore_constants.h"
#include "keystore_dev.h"

/*FIXME: review this it might be required under DEBUG/SELF_TESTS, however this is required for functionality  */
/*#if defined(DEBUG) || defined(SELF_TESTS) */
#include "keystore_globals.h"
/*#endif */

/* Universal operation data union */
typedef union {
	ias_keystore_register_t register_client_type;
	ias_keystore_register_client_t register_client;
	ias_keystore_unregister_client_t unregister_client;
	ias_keystore_generate_key_t generate_key;
	ias_keystore_wrap_key_t wrap_key;
	ias_keystore_load_key_t load_key;
	ias_keystore_unload_key_t unload_key;
	ias_keystore_encrypt_t encrypt;
	ias_keystore_decrypt_t decrypt;
	ias_keystore_backup_t backup;
	ias_keystore_ecc_keys_t ecc_keys;
	ias_keystore_gen_mkey_t gen_mkey;
	ias_keystore_rewrap_t rewrap;
#if defined(KEYSTORE_TEST_MIGRATION)
	ias_keystore_test_backup_t test_backup;
	ias_keystore_verify_signature_t verify_signature;
	ias_keystore_test_reencrypt_backup_with_mk_t keystore_test_reencrypt_backup_with_mk;
#endif
} keystore_ops_union;

static unsigned int is_cmd_supported(unsigned int cmd)
{
	switch (cmd) {
	case KEYSTORE_IOC_REGISTER:
	case KEYSTORE_IOC_REGISTER_CLIENT:
	case KEYSTORE_IOC_UNREGISTER_CLIENT:
	case KEYSTORE_IOC_GENERATE_KEY:
	case KEYSTORE_IOC_WRAP_KEY:
	case KEYSTORE_IOC_LOAD_KEY:
	case KEYSTORE_IOC_UNLOAD_KEY:
	case KEYSTORE_IOC_ENCRYPT:
	case KEYSTORE_IOC_DECRYPT:
	case KEYSTORE_IOC_BACKUP:
	case KEYSTORE_IOC_GET_KSM_KEY:
	case KEYSTORE_IOC_GEN_MKEY:
	case KEYSTORE_IOC_REWRAP:
#if defined(KEYSTORE_TEST_MIGRATION)
	case KEYSTORE_IOC_GEN_ECC_KEYS:
	case KEYSTORE_IOC_TEST_BACKUP_DECRYPT:
	case KEYSTORE_IOC_TEST_VERIFY_ECC_SIGNATURE:
	case KEYSTORE_IOC_TEST_REENCRYPT_BACKUP_WITH_MK:
#endif
		return 1;

	default:
		return 0;
	}
}

static unsigned char *getcmdstr(unsigned int cmd)
{
	switch (cmd) {
	case KEYSTORE_IOC_REGISTER:
		return "KEYSTORE_IOC_REGISTER";
	case KEYSTORE_IOC_REGISTER_CLIENT:
		return "KEYSTORE_IOC_REGISTER_CLIENT";
	case KEYSTORE_IOC_UNREGISTER_CLIENT:
		return "KEYSTORE_IOC_REGISTER_CLIENT";
	case KEYSTORE_IOC_GENERATE_KEY:
		return "KEYSTORE_IOC_GENERATE_KEY";
	case KEYSTORE_IOC_WRAP_KEY:
		return "KEYSTORE_IOC_WRAP_KEY";
	case KEYSTORE_IOC_LOAD_KEY:
		return "KEYSTORE_IOC_LOAD_KEY";
	case KEYSTORE_IOC_UNLOAD_KEY:
		return "KEYSTORE_IOC_UNLOAD_KEY";
	case KEYSTORE_IOC_ENCRYPT:
		return "KEYSTORE_IOC_ENCRYPT";
	case KEYSTORE_IOC_DECRYPT:
		return "KEYSTORE_IOC_DECRYPT";
	case KEYSTORE_IOC_BACKUP:
		return "KEYSTORE_IOC_BACKUP";
	case  KEYSTORE_IOC_GET_KSM_KEY:
		return "KEYSTORE_IOC_GET_KSM_KEY";
	case KEYSTORE_IOC_GEN_MKEY:
		return "KEYSTORE_IOC_GEN_MKEY";
	case KEYSTORE_IOC_REWRAP:
		return "KEYSTORE_IOC_REWRAP";
#if defined(KEYSTORE_TEST_MIGRATION)
	case KEYSTORE_IOC_GEN_ECC_KEYS:
		return "KEYSTORE_IOC_GEN_ECC_KEYS";
	case KEYSTORE_IOC_TEST_BACKUP_DECRYPT:
		return "KEYSTORE_IOC_TEST_BACKUP_DECRYPT";
	case KEYSTORE_IOC_TEST_VERIFY_ECC_SIGNATURE:
		return "KEYSTORE_IOC_TEST_VERIFY_ECC_SIGNATURE";
	case KEYSTORE_IOC_TEST_REENCRYPT_BACKUP_WITH_MK:
		return "KEYSTORE_IOC_TEST_REENCRYPT_BACKUP_WITH_MK";
#endif
	default:
		return "not-supported";
	}
}
static int encrypt_op(ias_keystore_encrypt_t *data)
{
	void *input, *output;
	int res;

	if (!data) {
		pr_err(KBUILD_MODNAME ": %s - data NULL\n",
				__func__);
		return -EINVAL;
	}

	if (!data->input || !data->input_size) {
		pr_err(KBUILD_MODNAME ": %s - input NULL\n",
				__func__);
		return -EINVAL;
	}

	if (!data->output || !data->output_size) {
		pr_err(KBUILD_MODNAME ": %s - input(output) NULL\n",
				__func__);
		return -EINVAL;
	}

	input = kmalloc(data->input_size, GFP_KERNEL);
	output = kmalloc(data->output_size, GFP_KERNEL);

	if (!input || !output) {
		pr_err(KBUILD_MODNAME ": %s - out of memory\n",
				__func__);
		res = -ENOMEM;
	} else {
		if (copy_from_user(input, data->input, data->input_size)) {
			pr_err(KBUILD_MODNAME ": %s - cannot read data\n", __func__);
			res = -EFAULT;
		} else {
			res = keystore_encrypt(data->client_ticket,
					data->slot_id,
					(keystore_algo_spec_t) data->algospec,
					data->iv, data->iv_size, input,
					data->input_size, output,
					data->output_size);
			if (res > 0) {
				if (copy_to_user(data->output, output, res)) {
					pr_err(KBUILD_MODNAME ": %s - cannot write data\n", __func__);
					res = -EFAULT;
				}
			}
		}
	}

	kzfree(input);
	kzfree(output);

	return res;
}

static int decrypt_op(ias_keystore_decrypt_t *data)
{
	void *input, *output;
	int res;

	if (!data) {
		pr_err(KBUILD_MODNAME ": %s - data NULL\n",
				__func__);
		return -EINVAL;
	}

	if (!data->input || !data->input_size) {
		pr_err(KBUILD_MODNAME ": %s - input NULL\n",
				__func__);
		return -EINVAL;
	}

	if (!data->output || !data->output_size) {
		pr_err(KBUILD_MODNAME ": %s - input(output) NULL\n",
				__func__);
		return -EINVAL;
	}

	input = kmalloc(data->input_size, GFP_KERNEL);
	output = kmalloc(data->output_size, GFP_KERNEL);

	if (!input || !output) {
		pr_err(KBUILD_MODNAME ": %s - out of memory\n",
				__func__);
		res = -ENOMEM;
	} else {
		if (copy_from_user(input, data->input, data->input_size)) {
			pr_err(KBUILD_MODNAME ": %s - cannot read data\n", __func__);
			res = -EFAULT;
		} else {
			res = keystore_decrypt(data->client_ticket,
					data->slot_id, input, data->input_size,
					output, data->output_size);
			if (res > 0) {
				if (copy_to_user(data->output, output, res)) {
					pr_err(KBUILD_MODNAME ": %s - cannot write data\n", __func__);
					res = -EFAULT;
				}
			}
		}
	}

	kzfree(input);
	kzfree(output);

	return res;
}

static int backup_op(ias_keystore_backup_t *data)
{
	void *OEMpub, *sig, *output, *output_sig;
	int res = 0;

	if ((data->output_size < KEYSTORE_BACKUP_SIZE) ||
			(data->output_sig_size < ECC_SIGNATURE_SIZE) ||
			(data->OEMpub_size > sizeof(EccPoint))) {
		return -EINVAL;
	}

	OEMpub = kmalloc(data->OEMpub_size, GFP_KERNEL);
	sig = kmalloc(data->sig_size, GFP_KERNEL);
	output = kmalloc(data->output_size, GFP_KERNEL);
	output_sig = kmalloc(data->output_sig_size, GFP_KERNEL);

	if (!OEMpub || !sig || !output || !output_sig) {
		pr_err(KBUILD_MODNAME ": %s - out of memory\n",
				__func__);
		res = -ENOMEM;
	} else {

		if (res != 0 ||
			copy_from_user(sig, data->sig, data->sig_size) ||
			copy_from_user(OEMpub, data->OEMpub, data->OEMpub_size)) {
			pr_err(KBUILD_MODNAME ": %s - cannot read key\n", __func__);
			res = -EFAULT;
		} else {
			res = keystore_backup(data->client_ticket,
					OEMpub, data->OEMpub_size,
					sig, data->sig_size,
					output, data->output_size,
					output_sig, data->output_sig_size);
			if (res > 0 && res == KEYSTORE_BACKUP_SIZE) {
				if (copy_to_user(data->output, output,
							KEYSTORE_BACKUP_SIZE) ||
						copy_to_user(data->output_sig,
							output_sig,
							ECC_SIGNATURE_SIZE)) {
					pr_err(KBUILD_MODNAME ": %s - cannot write data\n", __func__);
					res = -EFAULT;
				}
			} else {
				if (res > 0) {
					pr_err(KBUILD_MODNAME ": %s - wrong backup size: %d, expected: %d\n",
				__func__, res, KEYSTORE_BACKUP_SIZE);
					res = -EFAULT;
				}
			}
		}
	}
	kzfree(OEMpub);
	kzfree(sig);
	kzfree(output);
	kzfree(output_sig);

	return res;
}

static int gen_mkey_op(ias_keystore_gen_mkey_t *data)
{
	void *key, *sig, *output, *output_sig, *output_nonce;
	int res;

	if ((data->output_size < KEYSTORE_MKEY_SIZE) ||
			(data->output_sig_size < ECC_MKEY_SIGNATURE_SIZE) ||
			(data->output_nonce_size < KEYSTORE_MKEY_NONCE_SIZE)) {
		pr_err(KBUILD_MODNAME ": %s wrong size, mkeysize:%d, mkey_sig_sz: %d, nonce_size: %d\n",
		__func__, data->output_size, data->output_sig_size,
		data->output_nonce_size);
		res = -EINVAL;
	} else {
		key = kmalloc(data->key_size, GFP_KERNEL);
		sig = kmalloc(data->sig_size, GFP_KERNEL);
		output = kmalloc(data->output_size, GFP_KERNEL);
		output_sig = kmalloc(data->output_sig_size, GFP_KERNEL);
		output_nonce = kmalloc(data->output_nonce_size, GFP_KERNEL);

		if (!key || !sig || !output || !output_sig || !output_nonce) {
			pr_err(KBUILD_MODNAME ": %s - out of memory\n", __func__);
			res = -ENOMEM;
		} else{
			if (copy_from_user(key, data->key, data->key_size) ||
			copy_from_user(sig, data->sig, data->sig_size)) {
				pr_err(KBUILD_MODNAME ": %s - cannot read key\n",
						__func__);
				res = -EFAULT;
			} else {
				res = keystore_generate_mkey(key,
						data->key_size,
						sig, data->sig_size,
						output, data->output_size,
						output_sig, data->output_sig_size,
						output_nonce, data->output_nonce_size);
#ifdef DEBUG
				keystore_hexdump("migration - MKEY(ioctl)",
						output, data->output_size);
				keystore_hexdump("migration - MKEY-Signed(ioctl)", output_sig, data->output_sig_size);
				keystore_hexdump("migration - MKEY-NONCE(ioctl)"
						, output_nonce,
						data->output_nonce_size);
#endif
				if (res == 0) {
					if (copy_to_user(data->output, output,
							KEYSTORE_EMKEY_SIZE) ||
						copy_to_user(data->output_sig,
							output_sig,
							SIGNED_MKEY_SIZE) ||
						copy_to_user(data->output_nonce,
							output_nonce,
							KEYSTORE_MKEY_NONCE_SIZE
							)) {
						pr_err(KBUILD_MODNAME ": %s - cannot write data\n", __func__);
						res = -EFAULT;
					}
					pr_err(KBUILD_MODNAME ": %s - MK generation - Done\n", __func__);
				}
			}
		}
		kzfree(key);
		kzfree(sig);
		kzfree(output);
		kzfree(output_sig);
		kzfree(output_nonce);
	}

	return res;
}


static int get_ksm_key_op(ias_keystore_ecc_keys_t *data)
{
	void *pub_key;
	int res = 0;

	if (!data) {
		res = -EINVAL;
		return res;
	}

	pub_key = kmalloc(data->public_key_size, GFP_KERNEL);

	if (!pub_key) {
		pr_err(KBUILD_MODNAME ": %s - KEYSTORE_IOC_GET_KSM_KEY out of memory\n", __func__);
		res = -ENOMEM;
	} else {
		res = keystore_get_ksm_key(pub_key, data->public_key_size);
		if (res == 0) {
			if (copy_to_user(data->public_key, pub_key,
						data->public_key_size)) {
				pr_err(KBUILD_MODNAME ": %s - KEYSTORE_IOC_GET_KSM_KEY cannot read key\n", __func__);
				res = -EFAULT;
			}
		}
	}

	kzfree(pub_key);

	return res;
}

static int keystore_rewrap_op(ias_keystore_rewrap_t *data)
{
	int res;
	void *backup, *nonce, *wrapped_key, *rewrapped_key;

	if ((data->nonce_size < KEYSTORE_MKEY_NONCE_SIZE) ||
		(data->wrapped_key_size > KEYSTORE_MAX_WRAPPED_KEY_SIZE)) {
		pr_err(KBUILD_MODNAME ": %s: error - nonce_size : %d, wrapped_key_size: %d\n",
		 __func__, data->nonce_size, data->wrapped_key_size);
		res = -EINVAL;
	} else {
		nonce = kmalloc(data->nonce_size, GFP_KERNEL);
		backup = kmalloc(data->backup_size, GFP_KERNEL);
		wrapped_key = kmalloc(data->wrapped_key_size, GFP_KERNEL);
		rewrapped_key = kmalloc(data->rewrapped_key_size, GFP_KERNEL);

		if (!nonce || !backup || !wrapped_key || !rewrapped_key) {
			pr_err(KBUILD_MODNAME ": %s out of memory\n",
					__func__);
			res = -ENOMEM;
		} else {
			if (copy_from_user(nonce, data->nonce,
						data->nonce_size) ||
				copy_from_user(backup, data->backup,
					data->backup_size) ||
				copy_from_user(wrapped_key, data->wrapped_key,
					data->wrapped_key_size)) {
				pr_err(KBUILD_MODNAME ": %s failed copy_from_user\n", __func__);
				res = -EFAULT;
			} else {
				res = keystore_rewrap_key(data->client_ticket,
						backup, data->backup_size,
						nonce, data->nonce_size,
						wrapped_key,
						data->wrapped_key_size,
						rewrapped_key,
						data->rewrapped_key_size);

				keystore_hexdump("rewrap wrapped_key- (from ioctl)", wrapped_key, data->wrapped_key_size);
				keystore_hexdump("rewrap rewrapped_key - (from ioctl)", rewrapped_key, data->rewrapped_key_size);

				if (res > 0) /* if rewrapping is ok, api returns rewrapped no of bytes */ {
					pr_err(KBUILD_MODNAME ": %s rewrapping - Done\n", __func__);
					if (copy_to_user(data->rewrapped_key,
								rewrapped_key,
								res)) {
						pr_err(KBUILD_MODNAME ": %s cannot write data\n", __func__);
						res = -EFAULT;
					}
				}
			}
		}

		kzfree(backup);
		kzfree(nonce);
		kzfree(wrapped_key);
		kzfree(rewrapped_key);
	}

	return res;
}


#if defined(KEYSTORE_TEST_MIGRATION)
static int gen_ecc_keys_op(ias_keystore_ecc_keys_t *data)
{
	void *priv_key, *pub_key;
	int res;

	if (!data) {
		res = -EINVAL;
		return res;
	}

	priv_key = kmalloc(data->private_key_size, GFP_KERNEL);
	pub_key = kmalloc(data->public_key_size, GFP_KERNEL);

	if (!priv_key || !pub_key) {
		pr_err(KBUILD_MODNAME ": %s - KEYSTORE_IOC_GEN_ECC_KEYS out of memory\n", __func__);
		res = -ENOMEM;
	} else {
		res = keystore_gen_ecc_keys(pub_key,
			data->public_key_size,
			priv_key,
			data->private_key_size);
		if (res == 0) {
			if (copy_to_user(data->private_key, priv_key,
						data->private_key_size)) {
				pr_err(KBUILD_MODNAME ": %s - KEYSTORE_IOC_GEN_ECC_KEYS cannot read key\n", __func__);
				res = -EFAULT;
			}

			if (copy_to_user(data->public_key, pub_key,
						data->public_key_size)) {
				pr_err(KBUILD_MODNAME ": %s - KEYSTORE_IOC_GEN_ECC_KEYS cannot read key\n", __func__);
				res = -EFAULT;
			}
		}
	}

	kzfree(priv_key);
	kzfree(pub_key);

	return res;
}
#endif


#if defined(KEYSTORE_TEST_MIGRATION)
static int keystore_test_decrypt_backup_op(ias_keystore_test_backup_t *data)
{
	int res;
	void *key, *pubkey, *enc_data, *output, *signature;

	if (!data) {
		res = -EINVAL;
		return res;
	}

	key = kmalloc(data->key_size, GFP_KERNEL);
	pubkey = kmalloc(data->pubkey_size, GFP_KERNEL);
	enc_data = kmalloc(data->data_size, GFP_KERNEL);
	signature = kmalloc(data->signature_size, GFP_KERNEL);
	output = kmalloc(data->output_size, GFP_KERNEL);
	if (!key || !pubkey || !enc_data || !signature || !output) {
		pr_err(KBUILD_MODNAME ": %s - KEYSTORE_IOC_TEST_BACKUP_DECRYPT out of memory\n", __func__);
		res = -ENOMEM;
	} else {
		if (copy_from_user(key, data->key, data->key_size) ||
				copy_from_user(enc_data, data->data,
					data->data_size) ||
				copy_from_user(signature, data->signature,
					data->signature_size) ||
				copy_from_user(pubkey, data->pubkey,
					data->pubkey_size)) {
			pr_err(KBUILD_MODNAME ": %s - KEYSTORE_IOC_TEST_BACKUP_DECRYPT cannot read key\n", __func__);
			res = -EFAULT;
		} else {
			res = keystore_test_decrypt_backup(
				data->client_ticket,
				key, data->key_size,
				pubkey,
				data->pubkey_size,
				enc_data,
				data->data_size,
				signature,
				data->signature_size,
				output,
				data->output_size);
			if (res == 0) {
				if (copy_to_user(data->output, output,
							data->output_size)) {
					pr_err(KBUILD_MODNAME ": %s - KEYSTORE_IOC_TEST_BACKUP_DECRYPT cannot read output\n", __func__);
					res = -EFAULT;
				}
			} else {
				pr_err(KBUILD_MODNAME ": %s - KEYSTORE_IOC_TEST_BACKUP_DECRYPT failed (%d)\n", __func__, res);
			}
		}
	}
	kzfree(key);
	kzfree(enc_data);
	kzfree(output);

	return res;
}
#endif

#if defined(KEYSTORE_TEST_MIGRATION)
int keystore_test_verify_ecc_signature_op(ias_keystore_verify_signature_t *data)
{
	int res;
	void *pub_key, *mdata, *sig;

	pub_key = kmalloc(data->key_size, GFP_KERNEL);
	mdata = kmalloc(data->data_size, GFP_KERNEL);
	sig = kmalloc(data->sig_size, GFP_KERNEL);

	if (!pub_key || !mdata || !sig) {
		pr_err(KBUILD_MODNAME ": %s - KEYSTORE_IOC_TEST_VERIFY_ECC_SIGNATURE out of memory\n", __func__);
		res = -ENOMEM;
	} else {
		if (copy_from_user(pub_key, data->key, data->key_size) ||
				copy_from_user(mdata, data->data,
					data->data_size) ||
				copy_from_user(sig, data->sig, data->sig_size)) {
			pr_err(KBUILD_MODNAME ": %s - KEYSTORE_IOC_TEST_VERIFY_ECC_SIGNATURE cannot read key\n", __func__);
			res = -EFAULT;
		} else {
			res = keystore_test_verify_ecc_signature(pub_key,
					data->key_size, mdata, data->data_size,
					sig, data->sig_size);
		}
	}
	kzfree(pub_key);
	kzfree(mdata);
	kzfree(sig);

	return res;
}
#endif

#if defined(KEYSTORE_TEST_MIGRATION)
/*FIXME: rename this to proper migration operation i.e - migrate() */
int keystore_test_reencrypt_backup_with_mk_op(
		ias_keystore_test_reencrypt_backup_with_mk_t *data)
{
	int res;
	void *OEMpriv, *backup, *OEMpriv2, *mig_key, *output;

	OEMpriv = kmalloc(data->OEMpriv_size, GFP_KERNEL);
	backup = kmalloc(data->backup_size, GFP_KERNEL);
	OEMpriv2 = kmalloc(data->OEMpriv2_size, GFP_KERNEL);
	mig_key = kmalloc(data->mig_key_size, GFP_KERNEL);
	output = kmalloc(data->output_size, GFP_KERNEL);

	if (!OEMpriv || !backup || !OEMpriv2 || !mig_key || !output) {
		pr_err(KBUILD_MODNAME ": %s - KEYSTORE_IOC_TEST_REENCRYPT_BACKUP_WITH_MK out of memory\n", __func__);
		res = -ENOMEM;
	} else {
		if (copy_from_user(OEMpriv, data->OEMpriv, data->OEMpriv_size) ||
			copy_from_user(backup, data->backup, data->backup_size)
			|| copy_from_user(OEMpriv2, data->OEMpriv2,
				data->OEMpriv2_size) ||
			copy_from_user(mig_key, data->mig_key,
				data->mig_key_size)) {
			pr_err(KBUILD_MODNAME ": %s - KEYSTORE_IOC_TEST_REENCRYPT_BACKUP_WITH_MK cannot copy data from user space\n", __func__);
			res = -EFAULT;
		} else {
			res = keystore_test_reencrypt_backup_with_mk(OEMpriv,
					data->OEMpriv_size,
					backup, data->backup_size,
					OEMpriv2, data->OEMpriv2_size,
					mig_key, data->mig_key_size,
					output, data->output_size);
			if (res == 0) {
				if (copy_to_user(data->output, output,
							data->output_size)) {
					pr_err(KBUILD_MODNAME ": %s - KEYSTORE_IOC_TEST_REENCRYPT_BACKUP_WITH_MK cannot copy data to user space\n", __func__);
					res = -EFAULT;
				}
			}
		}
	}

	kzfree(OEMpriv);
	kzfree(backup);
	kzfree(OEMpriv2);
	kzfree(mig_key);
	kzfree(output);

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
	keystore_ops_union op;
	unsigned int size;
	int res;

	FUNC_BEGIN;

	/*FIXME: move this later under DEBUG flag, now required this for functionality  */
	/*#ifdef DEBUG_TRACE */
	pr_info(KBUILD_MODNAME ": %s - cmd = 0x%x - %s\n",
			__func__, cmd, getcmdstr(cmd));
	/*#endif */

	/* copy data structure from user space to system */
	size = _IOC_SIZE(cmd);
	if (!is_cmd_supported(cmd) || (size > sizeof(keystore_ops_union))) {
		pr_err(KBUILD_MODNAME ": %s - cmd=%u unknown, size:%d, size(keystore_ops_union): %zu\n",
		__func__, cmd, size, sizeof(keystore_ops_union));
		res = -ENOIOCTLCMD;
	} else if (!access_ok(VERIFY_READ, (void *) arg, size)) {
		pr_err(KBUILD_MODNAME ": %s - cmd=%u no read access\n",
			__func__, cmd);
		res = -EFAULT;
	} else if ((_IOC_DIR(cmd) & _IOC_READ) && (!access_ok(VERIFY_WRITE,
					(void *) arg, size))) {
		pr_err(KBUILD_MODNAME ": %s - cmd=%u no write access\n", __func__, cmd);
		res = -EFAULT;
	} else if (copy_from_user(&op, (void *) arg, size)) {
		pr_err(KBUILD_MODNAME ": %s - cannot copy data\n",
				__func__);
		res = -EFAULT;
	} else {
		switch (cmd) {
		case KEYSTORE_IOC_REGISTER:
			res = keystore_register(
					op.register_client_type.seed_type,
					op.register_client_type.client_ticket);
			break;

		case KEYSTORE_IOC_REGISTER_CLIENT: /* will be deprecated as part of CR: 6376 */
			res = keystore_register_client(
					op.register_client.client_id,
					op.register_client.client_ticket);
			break;

		case KEYSTORE_IOC_UNREGISTER_CLIENT:
			res = keystore_unregister_client(
					op.unregister_client.client_ticket);
			break;

		case KEYSTORE_IOC_GENERATE_KEY:
			res = keystore_generate_key(
				op.generate_key.client_ticket,
				(keystore_key_spec_t) op.generate_key.key_spec,
				op.generate_key.wrapped_key);
			break;

		case KEYSTORE_IOC_WRAP_KEY:
			res = keystore_wrap_key(op.wrap_key.client_ticket,
				op.wrap_key.app_key, op.wrap_key.app_key_size,
				(keystore_key_spec_t) op.wrap_key.key_spec,
				op.wrap_key.wrapped_key);
			break;

		case KEYSTORE_IOC_LOAD_KEY:
			res = keystore_load_key(op.load_key.client_ticket,
				     op.load_key.wrapped_key,
				     op.load_key.wrapped_key_size);
			break;

		case KEYSTORE_IOC_UNLOAD_KEY:
			res = keystore_unload_key(op.unload_key.client_ticket,
					op.unload_key.slot_id);
			break;

		case KEYSTORE_IOC_ENCRYPT:
			res = encrypt_op(&op.encrypt);
			break;

		case KEYSTORE_IOC_DECRYPT:
			res = decrypt_op(&op.decrypt);
			break;

		case KEYSTORE_IOC_BACKUP:
			res = backup_op(&op.backup);
			break;

		case KEYSTORE_IOC_GET_KSM_KEY:
			res = get_ksm_key_op(&op.ecc_keys);
			break;

		case KEYSTORE_IOC_GEN_MKEY:
			res = gen_mkey_op(&op.gen_mkey);
			break;

		case KEYSTORE_IOC_REWRAP:
			res = keystore_rewrap_op(&op.rewrap);
			break;

#if defined(KEYSTORE_TEST_MIGRATION)
		case KEYSTORE_IOC_GEN_ECC_KEYS:
			res = gen_ecc_keys_op(&op.ecc_keys);
			break;

		case KEYSTORE_IOC_TEST_BACKUP_DECRYPT:
			res = keystore_test_decrypt_backup_op(&op.test_backup);
			break;

		case KEYSTORE_IOC_TEST_VERIFY_ECC_SIGNATURE:
			res = keystore_test_verify_ecc_signature_op(
					&op.verify_signature);
			break;

		case KEYSTORE_IOC_TEST_REENCRYPT_BACKUP_WITH_MK:
			res = keystore_test_reencrypt_backup_with_mk_op(
				&op.keystore_test_reencrypt_backup_with_mk);
			break;
#endif

		default:
			res = -ENOIOCTLCMD;
			pr_err(KBUILD_MODNAME ": %s - cmd=%u not known\n", __func__, cmd);
			break;
		}

		/* check if OK and we need to return some data in memory block */
		if ((res >= 0) && (_IOC_DIR(cmd) & _IOC_READ)) {
			/* then copy data from system to user space */
			if (copy_to_user((void *) arg, &op, size)) {
				pr_err(KBUILD_MODNAME ": %s - cannot copy data\n", __func__);
				res = -EFAULT;
			}
		}
	}

	memset(&op, 0, sizeof(op));

	FUNC_RES(res);

	return res;
}

/* end of file */
