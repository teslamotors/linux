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
#include <linux/fdtable.h>
#include <linux/fs.h>
#include <linux/fs_struct.h>
#include <linux/dcache.h>
#include <linux/random.h>
#include <asm-generic/current.h>

#include <security/keystore_api_kernel.h>
#include "keystore_constants.h"
#include "keystore_context.h"
#include "keystore_crypto.h"
#include "keystore_dev.h"
#include "keystore_globals.h"
#include <security/ecc.h>
#include "keystore_keys_holder.h"
#include "keystore_helpers.h"

/**
 * This API will be deprecated in upcoming releases.
 *
 * Perform integrity/authorization checks on client, validating
 * the authenticity of the claimed ClientID.
 *
 * @client_id - ignored - needed for backward compatibility
 * @param client_ticket Output buffer for the client ticket (KEYSTORE_CLIENT_TICKET_SIZE bytes).
 *
 * @return 0 if OK or negative error code (see errno).
 */

/* NOTE: client_id from applicatons are not used anylonger, this is changed along with CR: 6376 */
int keystore_register_client(const char *client_id, void *client_ticket)
{
	int res = 0;

	FUNC_BEGIN;

	/* wrapper function for existing clients */
	res = keystore_register(SEED_TYPE_DEVICE, client_ticket);

	FUNC_RES(res);

	return res;
}

/**
 * Perform integrity/authorization checks on client, validating
 * the authenticity of the claimed ClientID based on application path.
 *
 * @param seed_type, this argument decides the client seed type, {device or user specific seed}
 * @param client_ticket Output buffer for the client ticket (KEYSTORE_CLIENT_TICKET_SIZE bytes).
 *
 * @return 0 if OK or negative error code (see errno).
 */

int keystore_register(keystore_seed_type_t seed_type, void *client_ticket)
{
	uint8_t client_key[KEYSTORE_CLIENT_KEY_SIZE];
	uint8_t clientid[KEYSTORE_MAX_CLIENT_ID_SIZE];
	uint8_t ticket[KEYSTORE_CLIENT_TICKET_SIZE];
	keystore_ctx_t *ctx = NULL;
	char *buf = NULL, *f_path = NULL;
	int res = 0;

	FUNC_BEGIN;

	/*
	 * Check for secure boot status.
	 *
	 * On secure boot disabled systems, /dev/keystore device is not created
	 * So userspace clients wont be functional. But kernel-space clients
	 * can still call api's so check for sb-status and act accordingly.
	 */
#ifdef KEYSTORE_HARDCODE_SEED
	pr_info(KBUILD_MODNAME ": Using hardcoded SEEDs for key generation\n");
#else
	if (!get_sb_stat()) {
		pr_err(KBUILD_MODNAME ": Cannot register with keystore - secure boot not enabled\n");
		return -EPERM;
	}
#endif

	if (!client_ticket || seed_type < 0 || seed_type >= MAX_SEED_TYPES) {
		res = -EFAULT;
	} else {
#if defined(KEYSTORE_DISABLE_DEVICE_SEED)
		if (seed_type == DEVICE_SEED)
			return -EINVAL;
#endif

#if defined(KEYSTORE_DISABLE_USER_SEED)
		if (seed_type == USER_SEED)
			return -EINVAL;
#endif
		/* alloc mem for pwd##app, use linux defined limits */
		buf = kmalloc(PATH_MAX + NAME_MAX, GFP_KERNEL);
		if (NULL != buf) {
			/* clear the buf */
			memset(buf, 0, PATH_MAX + NAME_MAX);

			f_path = get_current_process_path(buf,
					(PATH_MAX + NAME_MAX));

			pr_info(KBUILD_MODNAME ": %s KSM-Client ABS path: %s\n",
					__func__, f_path);

			/* check if it was an error */
			if (f_path && IS_ERR(f_path)) {
				/* error case, do not register */
				pr_err(KBUILD_MODNAME ": Cannot register with keystore - failed client auth\n");
				res = -EFAULT;
			} else {
				/*  f_path is NULL for PF_KTHREAD type */
				if (!f_path) {
					pr_info(KBUILD_MODNAME ": %s Kernel client - use default.\n", __func__);
					f_path = KERNEL_CLIENTS_ID;
				}

				/* calculate sha2 on f_path - this is new clientid! */
				keystore_sha256_block(f_path, strlen(f_path),
					clientid,
					KEYSTORE_MAX_CLIENT_ID_SIZE);
#ifdef DEBUG
#warning " remove critical debug print"
				keystore_hexdump("keystore_register_client - clientid from app path(hex)"
					, clientid,
					KEYSTORE_MAX_CLIENT_ID_SIZE);
#endif

				/* calculate KDF(key = SEED, data = ClientID) */
				res = keystore_calc_mac("hmac(sha256)",
					((unsigned char *)((char (*)[SEC_SEED_SIZE])sec_seed + seed_type)),
					SEC_SEED_SIZE,
					clientid, sizeof(clientid),
					client_key,
					sizeof(client_key));
#ifdef DEBUG
#warning " remove critical debug print"
				keystore_hexdump("keystore_register_client - SEED",
					((unsigned char *)((char (*)[SEC_SEED_SIZE])sec_seed + seed_type)),
					SEC_SEED_SIZE);
#endif
				if (res) {
					pr_err(KBUILD_MODNAME ": %s: keystore_hmac_sha256 error %d\n", __func__, res);
				} else {
					/* randomize a buffer */
					keystore_get_rdrand(ticket,
							sizeof(ticket));

					mutex_lock(&contexts_mutex);

					/* allocate new context */
					ctx = keystore_allocate_context();
					if (!ctx) {
						pr_err(KBUILD_MODNAME ": %s: Cannot allocate context\n", __func__);
						res = -ENOMEM;
					} else {
						/* save ClientTicket and ClientKey */
						memcpy(ctx->client_ticket,
							ticket,
							sizeof(ticket));
						memcpy(ctx->client_key,
							client_key,
							sizeof(client_key));
						/*set to zeros and do same in
						 * backup/restore function */
						memset(ctx->client_id, 0,
							sizeof(ctx->client_id));
						memcpy(ctx->client_id,
							clientid,
							sizeof(clientid));

						/* return ClientTicket to the caller */
						memcpy(client_ticket, ticket, sizeof(ticket));
					}
					mutex_unlock(&contexts_mutex);
#ifdef DEBUG
#warning " remove critical debug print"
			keystore_hexdump("keystore_register_client - client_key"
					, ctx->client_key, sizeof(client_key));
#endif
				}
			}
			kfree(buf);
		} else {
			pr_err(KBUILD_MODNAME ": %s error not sufficient memory\n", __func__);
			res = -ENOMEM;
		}

		/* clear local secure data */
		memset(client_key, 0, sizeof(client_key));
		memset(ticket, 0, sizeof(ticket));
	}

	FUNC_RES(res);

	return res;
}
EXPORT_SYMBOL(keystore_register_client);

/**
 * Remove all state associated with ClientTicket.
 *
 * @param client_ticket The client ticket (KEYSTORE_CLIENT_TICKET_SIZE bytes).
 *
 * @return 0 if OK or negative error code (see errno).
 */
int keystore_unregister_client(const void *client_ticket)
{
	int res = -EFAULT;

	FUNC_BEGIN;

	if (client_ticket) {
		mutex_lock(&contexts_mutex);
		res = keystore_free_context_ticket(client_ticket);
		mutex_unlock(&contexts_mutex);
		if (res)
			pr_err(KBUILD_MODNAME ": %s: Cannot find context\n", __func__);
	}

	FUNC_RES(res);

	return res;
}
EXPORT_SYMBOL(keystore_unregister_client);

/**
 * Generate new random NewKey and format it according to KeySpec.
 *
 * @param client_ticket The client ticket (KEYSTORE_CLIENT_TICKET_SIZE bytes).
 * @param keyspec The key specification.
 * @param wrapped_key Output buffer for the wrapped key (key size + KEYSTORE_WRAPPED_KEY_EXTRA bytes).
 *
 * @return Wrapped key size in bytes if OK or negative error code (see errno).
 */
int keystore_generate_key(const void *client_ticket, keystore_key_spec_t keyspec,
		void *wrapped_key)
{
	unsigned int newkey_size;
	keystore_ctx_t *ctx;
	int res = -EINVAL;

	FUNC_BEGIN;

	if (!client_ticket || !wrapped_key) {
		FUNC_RES(-EFAULT);
		return -EFAULT;
	}

	switch (keyspec) {
	case KEYSPEC_LENGTH_128:
		newkey_size = AES128_KEY_SIZE;
		break;
	case KEYSPEC_LENGTH_256:
		newkey_size = AES256_KEY_SIZE;
		break;
	default:
		FUNC_RES(-EINVAL);
		return -EINVAL;
	}

	mutex_lock(&contexts_mutex);
	ctx = keystore_find_context_ticket(client_ticket);
	if (!ctx) {
		mutex_unlock(&contexts_mutex);
		pr_err(KBUILD_MODNAME ": %s: Cannot find context\n",
				__func__);
		res = -EINVAL;
	} else {
		uint8_t client_key[KEYSTORE_CLIENT_KEY_SIZE];
		uint8_t newkey[1 + newkey_size]; /* starts with
						    1-byte key spec */

		/* cache context data (the ClientKey)
		 * before releasing the lock */
		memcpy(client_key, ctx->client_key, sizeof(client_key));
		mutex_unlock(&contexts_mutex);

		/* prepare NewKey */
		newkey[0] = (uint8_t) keyspec;
		keystore_get_rdrand((newkey + 1), (int)newkey_size);

#ifdef DEBUG
#warning " remove critical debug print"
		keystore_hexdump("keystore_generate_key - plain appkey",
				newkey, sizeof(newkey));
#endif

		/* wrappedKey = KENC(key:ClientKey, data:KeySpec||NewKey) */
		res = keystore_aes_siv_crypt(1, client_key,
				KEYSTORE_CLIENT_KEY_SIZE, newkey,
				sizeof(newkey),	NULL, 0,
				wrapped_key,
				newkey_size + KEYSTORE_WRAPPED_KEY_EXTRA);
		if (res) {
			pr_err(KBUILD_MODNAME ": %s: Cannot wrap new key\n", __func__);
		} else {
			/* OK */
			res = newkey_size + KEYSTORE_WRAPPED_KEY_EXTRA;
		}

		/* clear local copies of the keys */
		memset(client_key, 0, sizeof(client_key));
		memset(newkey, 0, sizeof(newkey));
	}

	FUNC_RES(res);

	return res;
}
EXPORT_SYMBOL(keystore_generate_key);

/**
 * Generate new fixed key (only for kernel space clients) and format it according to KeySpec.
 *
 * @param client_ticket The client ticket (KEYSTORE_CLIENT_TICKET_SIZE bytes).
 * @param keyspec The key specification.
 * @param fixed_key Output buffer for the fixed key (KEYSPEC_LENGTH_256+1 bytes, 1 byte is for storing the keyspec).
 * @param fixed_key_size - size of fixedkey memory, this should be KEYSPEC_LENGTH_XXX + 1 bytes
 *
 * @return 0 if OK or negative error code (see errno).
 */
int keystore_generate_fixedkey(const void *client_ticket,
		keystore_key_spec_t keyspec, void *fixed_key,
		unsigned int fixed_key_size)
{
	keystore_ctx_t *ctx = NULL;
	uint8_t *newkey = NULL;
	unsigned int newkey_size;
	int res = -EINVAL;

	FUNC_BEGIN;

	if (!client_ticket || !fixed_key) {
		FUNC_RES(-EFAULT);
		return -EFAULT;
	}

	mutex_lock(&contexts_mutex);
	ctx = keystore_find_context_ticket(client_ticket);
	if (!ctx) {
		pr_err(KBUILD_MODNAME ": %s: Cannot find context\n",
				__func__);
		res = -EINVAL;
	} else {

		switch (keyspec) {
		case KEYSPEC_LENGTH_256:
			newkey_size = AES256_KEY_SIZE;
			break;
		default:
			FUNC_RES(-EINVAL);
			mutex_unlock(&contexts_mutex);
			return -EINVAL;
		}

		/* starts with 1-byte key spec */
		newkey = kmalloc(KEYSTORE_KEYSPEC_SIZE + newkey_size,
				GFP_KERNEL);
		if (!newkey) {
			pr_err(KBUILD_MODNAME ": %s: not sufficent memory kmalloc failed\n", __func__);
			FUNC_RES(-EINVAL);
			mutex_unlock(&contexts_mutex);
			return -EINVAL;
		}

		if (fixed_key_size < newkey_size + KEYSTORE_KEYSPEC_SIZE) {
			pr_err(KBUILD_MODNAME ": %s: not sufficent memory for output key (1byte extra needed for keyspec?)\n", __func__);
			kfree(newkey);
			mutex_unlock(&contexts_mutex);
			FUNC_RES(-EINVAL);
			return -EINVAL;
		}

		/* prepare NewKey */
		newkey[0] = (uint8_t) keyspec;

		/* calculate KDF(key = SEED, data = ClientID) */
		res = keystore_calc_mac("hmac(sha256)", (const char *)sec_seed,
				SEC_SEED_SIZE, KERNEL_CLIENTS_ID,
				strlen(KERNEL_CLIENTS_ID),
				newkey+KEYSTORE_KEYSPEC_SIZE, newkey_size);
		if (res) {
			pr_err(KBUILD_MODNAME ": %s: Cannot create fixed kernel client keys\n", __func__);
			pr_err(KBUILD_MODNAME ": %s: keystore_hmac_sha256 error %d\n", __func__, res);
		} else {
			/* OK, copy the key to client */
			memcpy(fixed_key, newkey,
					KEYSTORE_KEYSPEC_SIZE + newkey_size);
#ifdef DEBUG
#warning " remove critical debug print"
			keystore_hexdump("keystore_generate_fixed_key - plain kernel-client-key",
					fixed_key, KEYSTORE_KEYSPEC_SIZE + newkey_size);
#endif
		}
		/* clear local copies of the keys */
		memset(newkey, 0, KEYSTORE_KEYSPEC_SIZE + newkey_size);
		kfree(newkey);

	}
	mutex_unlock(&contexts_mutex);

	FUNC_RES(res);

	return res;
}
EXPORT_SYMBOL(keystore_generate_fixedkey);


/**
 * Encrypt the application key and wrap according to KeySpec.
 *
 * @param client_ticket The client ticket (KEYSTORE_CLIENT_TICKET_SIZE bytes).
 * @param app_key The application key.
 * @param app_key_size The application key size in bytes.
 * @param keyspec The key specification.
 * @param wrapped_key Output buffer for the wrapped key (app_key_size + KEYSTORE_WRAPPED_KEY_EXTRA bytes).
 *
 * @return Wrapped key size in bytes if OK or negative error code (see errno).
 */
int keystore_wrap_key(const void *client_ticket, const void *app_key,
		unsigned int app_key_size, keystore_key_spec_t keyspec,
		void *wrapped_key)
{
	keystore_ctx_t *ctx;
	int res;

	FUNC_BEGIN;

	if (!client_ticket || !app_key || !wrapped_key) {
		FUNC_RES(-EFAULT);
		return -EFAULT;
	} else if (app_key_size > KEYSTORE_MAX_APPKEY_SIZE) {
		FUNC_RES(-EINVAL);
		return -EINVAL;
	}

	switch (keyspec) {
	case KEYSPEC_LENGTH_128:
		if (app_key_size != AES128_KEY_SIZE) {
			FUNC_RES(-EINVAL);
			return -EINVAL;
		}
		break;
	case KEYSPEC_LENGTH_256:
		if (app_key_size != AES256_KEY_SIZE) {
			FUNC_RES(-EINVAL);
			return -EINVAL;
		}
		break;
	default:
		FUNC_RES(-EINVAL);
		return -EINVAL;
	}

	mutex_lock(&contexts_mutex);
	ctx = keystore_find_context_ticket(client_ticket);
	if (!ctx) {
		mutex_unlock(&contexts_mutex);
		pr_err(KBUILD_MODNAME ": %s: Cannot find context\n",
				__func__);
		res = -EINVAL;
	} else {
		uint8_t client_key[KEYSTORE_CLIENT_KEY_SIZE];
		uint8_t key[KEYSTORE_KEYSPEC_SIZE + app_key_size]; /*
								     * starts with 1-byte key spec */

		/* cache context data (the ClientKey) before releasing the lock */
		memcpy(client_key, ctx->client_key, sizeof(client_key));
		mutex_unlock(&contexts_mutex);

		/* prepare key for wrapping */
		key[0] = (uint8_t) keyspec;
		memcpy(key + KEYSTORE_KEYSPEC_SIZE, app_key, app_key_size);

		/* wrappedKey = KENC(key:ClientKey, data:KeySpec||AppKey) */
		res = keystore_aes_siv_crypt(1, client_key,
				KEYSTORE_CLIENT_KEY_SIZE, key, sizeof(key),
				NULL, 0, wrapped_key,
				app_key_size + KEYSTORE_WRAPPED_KEY_EXTRA);
		if (res) {
			pr_err(KBUILD_MODNAME ": %s: Cannot wrap app key\n", __func__);
		} else {
			/* OK */
			res = app_key_size + KEYSTORE_WRAPPED_KEY_EXTRA;
		}

		/* clear local copies of the keys */
		memset(client_key, 0, sizeof(client_key));
		memset(key, 0, sizeof(key));
	}

	FUNC_RES(res);

	return res;
}
EXPORT_SYMBOL(keystore_wrap_key);

static int keystore_validate_keyspec(uint8_t *key)
{
	int res = 0;

	FUNC_BEGIN;

	/* validate key and check if the keyspec is supported */
	if (!key) {
		res = -EINVAL;
	} else {
		switch (key[0]) {
		case KEYSPEC_LENGTH_256:
		case KEYSPEC_LENGTH_128:
			break;
		default:
			res = -EINVAL;
		}
	}

	FUNC_RES(res);
	return res;
}

/**
 * Decrypt the application key and store in a slot.
 *
 * @param client_ticket The client ticket (KEYSTORE_CLIENT_TICKET_SIZE bytes).
 * @param wrapped_key The wrapped key.
 * @param wrapped_key_size The wrapped key size in bytes.
 *
 * @return Used slot ID if OK or negative error code (see errno).
 */
int keystore_load_key(const void *client_ticket, const void *wrapped_key, unsigned int wrapped_key_size)
{
	keystore_ctx_t *ctx;
	int res = 0;
	unsigned int app_key_size;
	uint8_t *key = NULL;
	keystore_slot_t *slot = NULL;

	FUNC_BEGIN;

	if (!client_ticket || !wrapped_key) {
		res = -EFAULT;
	} else if ((wrapped_key_size == AES256_KEY_SIZE + KEYSTORE_WRAPPED_KEY_EXTRA) ||
			(wrapped_key_size == AES128_KEY_SIZE + KEYSTORE_WRAPPED_KEY_EXTRA)) {
		mutex_lock(&contexts_mutex);

		ctx = keystore_find_context_ticket(client_ticket);
		if (!ctx) {
			pr_err(KBUILD_MODNAME ": %s: Cannot find context\n", __func__);
			res = -EINVAL;
		} else {
			app_key_size = wrapped_key_size -
				KEYSTORE_WRAPPED_KEY_EXTRA;

			key = kmalloc(KEYSTORE_KEYSPEC_SIZE +
					app_key_size, GFP_KERNEL);
			if (!key) {
				pr_err(KBUILD_MODNAME ": %s: not sufficent memory kmalloc failed\n", __func__);
				res = -EINVAL;
			} else {
				/* decrypt AppKey||KeySpec = KDEC(key:ClientKey, data:wrappedKey) */
				res = keystore_aes_siv_crypt(0, ctx->client_key,
					KEYSTORE_CLIENT_KEY_SIZE, wrapped_key,
					wrapped_key_size,
					NULL, 0, key,
					KEYSTORE_KEYSPEC_SIZE + app_key_size);
				if (res) {
					pr_err(KBUILD_MODNAME ": %s: Cannot unwrap app key\n",
							__func__);
				} else if (keystore_validate_keyspec(key) != 0) {
					/* validate AppKey matches KeySpec */
					pr_err(KBUILD_MODNAME ": %s: Unknown KeySpec\n",
							__func__);
					res = -EINVAL;
				} else {
					/* allocate new slot */
					slot = keystore_allocate_slot(ctx);

					if (!slot) {
						pr_err(KBUILD_MODNAME ": %s: Cannot allocate slot\n", __func__);
						res = -ENOMEM;
					} else {
						/* save AppKey */
						memcpy(slot->app_key,
							key +
							KEYSTORE_KEYSPEC_SIZE,
							app_key_size);
						slot->app_key_size = app_key_size;

						/* return slotID */
						res = slot->slot_id;
					}
				}
				kfree(key);
			}
		}

		mutex_unlock(&contexts_mutex);
	} else {
		pr_err(KBUILD_MODNAME ": %s: Invalid wrapped key size\n", __func__);
		res = -EINVAL;
	}

	FUNC_RES(res);

	return res;
}
EXPORT_SYMBOL(keystore_load_key);

/**
 * Load the kernel client key and store in a slot.
 *
 * @param client_ticket The client ticket (KEYSTORE_CLIENT_TICKET_SIZE bytes).
 * @param fixed_key The wrapped key.
 * @param fixed_key_size The wrapped key size in bytes.
 *
 * @return Used slot ID if OK or negative error code (see errno).
 */
int keystore_load_fixedkey(const void *client_ticket, const void *fixed_key, unsigned int fixed_key_size)
{
	keystore_ctx_t *ctx;
	int res;
	uint8_t *keyptr = (uint8_t *)fixed_key;
	keystore_slot_t *slot = NULL;

	if (!client_ticket || !fixed_key) {
		res = -EFAULT;
	} else {
		mutex_lock(&contexts_mutex);

		ctx = keystore_find_context_ticket(client_ticket);
		if (!ctx) {
			pr_err(KBUILD_MODNAME ": %s: Cannot find context\n", __func__);
			res = -EINVAL;
		} else {
			switch (keyptr[0]) {
			case KEYSPEC_LENGTH_256:
				if (fixed_key_size != AES256_KEY_SIZE +
						KEYSTORE_KEYSPEC_SIZE)	{
					pr_err(KBUILD_MODNAME ": %s: wrong keylength: %d\n", __func__, fixed_key_size);
					FUNC_RES(-EINVAL);
					return -EINVAL;
				}
				break;
			default:
				pr_err(KBUILD_MODNAME ": %s: invalid keyspec: %#x\n", __func__, keyptr[0]);
				FUNC_RES(-EINVAL);
				return -EINVAL;
			}

			/* allocate new slot */
			slot = keystore_allocate_slot(ctx);
			if (!slot) {
				pr_err(KBUILD_MODNAME ": %s: Cannot allocate slot\n", __func__);
				res = -ENOMEM;
			} else {
				/* save AppKey */
				memcpy(slot->app_key,
						keyptr + KEYSTORE_KEYSPEC_SIZE,
						(fixed_key_size -
						 KEYSTORE_KEYSPEC_SIZE));
				slot->app_key_size = (fixed_key_size -
						KEYSTORE_KEYSPEC_SIZE);

				/* return slotID */
				res = slot->slot_id;
			}
		}

		mutex_unlock(&contexts_mutex);
	}

	FUNC_RES(res);

	return res;
}
EXPORT_SYMBOL(keystore_load_fixedkey);




/**
 * Lookup key by slotID/ClientTicket and purge it from KSM memory.
 *
 * @param client_ticket The client ticket (KEYSTORE_CLIENT_TICKET_SIZE bytes).
 * @param slot_id The slot ID.
 *
 * @return 0 if OK or negative error code (see errno).
 */
int keystore_unload_key(const void *client_ticket, int slot_id)
{
	int res;

	FUNC_BEGIN;

	if (!client_ticket) {
		res = -EFAULT;
	} else if ((slot_id < 0) || (slot_id >= KEYSTORE_SLOTS_MAX)) {
		res = -EINVAL;
	} else {
		keystore_ctx_t *ctx;

		mutex_lock(&contexts_mutex);

		ctx = keystore_find_context_ticket(client_ticket);
		if (!ctx) {
			pr_err(KBUILD_MODNAME ": %s: Cannot find context\n", __func__);
			res = -EINVAL;
		} else {
			res = keystore_free_slot_id(ctx, slot_id);
			if (res)
				pr_err(KBUILD_MODNAME ": %s: Cannot find slot\n",
						__func__);
		}

		mutex_unlock(&contexts_mutex);
	}

	FUNC_RES(res);

	return res;
}
EXPORT_SYMBOL(keystore_unload_key);

/**
 * Encrypt plaintext using AppKey/IV according to AlgoSpec.
 *
 * @param client_ticket The client ticket (KEYSTORE_CLIENT_TICKET_SIZE bytes).
 * @param slot_id The slot ID.
 * @param algo_spec The algorithm specification.
 * @param iv Encryption initialization vector.
 * @param iv_size Initialization vector size in bytes.
 * @param input Input block of data to encrypt.
 * @param input_size Input block size in bytes.
 * @param output Pointer to the block for encrypted data.
 * @param output_size Output block size in bytes (at least iv_size + input_size + 9 bytes).
 *
 * @return Encrypted data size in bytes if OK or negative error code (see errno).
 */
int keystore_encrypt(const void *client_ticket, int slot_id,
		keystore_algo_spec_t algo_spec,	const void *iv,
		unsigned int iv_size, const void *input,
		unsigned int input_size, void *output, unsigned int output_size)
{
	int res;

	FUNC_BEGIN;

#ifdef DEBUG_TRACE
	pr_info(KBUILD_MODNAME
			": keystore_encrypt slot_id=%d algo_spec=%d iv_size=%u isize=%u osize=%u\n",
			slot_id, (int) algo_spec, iv_size, input_size,
			output_size);
#endif

	if (!client_ticket || !iv || !input || !output) {
		res = -EFAULT;
	} else if ((slot_id < 0) || (slot_id >= KEYSTORE_SLOTS_MAX) ||
			!input_size || (algo_spec != ALGOSPEC_AES) ||
			(iv_size != KEYSTORE_AES_IV_SIZE) ||
			(output_size < iv_size + 1 + input_size +
			 KEYSTORE_CCM_AUTH_SIZE)) {
		res = -EINVAL;
	} else {
		keystore_ctx_t *ctx;

		mutex_lock(&contexts_mutex);
		ctx = keystore_find_context_ticket(client_ticket);
		if (!ctx) {
			mutex_unlock(&contexts_mutex);
			pr_err(KBUILD_MODNAME ": %s: Cannot find context\n", __func__);
			res = -EINVAL;
		} else {
			keystore_slot_t *slot = keystore_find_slot_id(ctx,
					slot_id);
			if (!slot) {
				mutex_unlock(&contexts_mutex);
				pr_err(KBUILD_MODNAME ": %s: Cannot find slot\n", __func__);
				res = -EINVAL;
			} else {
				uint8_t app_key[slot->app_key_size];
				uint8_t *out;

				/* cache slot data (the AppKey) before releasing the lock */
				memcpy(app_key, slot->app_key,
						slot->app_key_size);
				mutex_unlock(&contexts_mutex);

				/* prepare block to be returned */
				/* AlgoSpec || IV || Cipher */
				out = (uint8_t *) output;
				*out++ = (uint8_t) algo_spec;
				memcpy(out, iv, iv_size);
				out += iv_size;

				/* Cipher = AENC(AppKey, AlgoSpec, IV, Plaintext) */
				res = keystore_aes_ccm_crypt(1, app_key,
						sizeof(app_key), iv, iv_size,
						input, input_size, NULL, 0,
						out, input_size +
						KEYSTORE_CCM_AUTH_SIZE);
				if (res) {
					pr_err(KBUILD_MODNAME
							": %s: Error %d in keystore_aes_ccm_crypt\n",
							__func__, res);
				} else {
					/* OK */
					res = 1 + iv_size + input_size +
						KEYSTORE_CCM_AUTH_SIZE;
				}

				/* clear local copy of AppKey */
				memset(app_key, 0, sizeof(app_key));
			}
		}
	}

	FUNC_RES(res);

	return res;
}
EXPORT_SYMBOL(keystore_encrypt);

/**
 * Decrypt cipher using AppKey and AlgoSpec/IV.
 *
 * @param client_ticket The client ticket (KEYSTORE_CLIENT_TICKET_SIZE bytes).
 * @param slot_id The slot ID.
 * @param input Input block of data to decrypt.
 * @param input_size Input block size in bytes.
 * @param output Pointer to the block for decrypted data.
 * @param output_size Output block size in bytes.
 *
 * @return Decrypted data size in bytes if OK or negative error code (see errno).
 */
int keystore_decrypt(const void *client_ticket, int slot_id, const void *input, unsigned int input_size,
		void *output, unsigned int output_size)
{
	int res;

	FUNC_BEGIN;

#ifdef DEBUG_TRACE
	pr_info(KBUILD_MODNAME
			": keystore_decrypt slot_id=%d input_size=%u output_size=%u\n",
			slot_id, input_size, output_size);
#endif

	if (!client_ticket || !input || !output) {
		res = -EFAULT;
	} else if ((slot_id < 0) || (slot_id >= KEYSTORE_SLOTS_MAX) ||
			(input_size < 1 + KEYSTORE_AES_IV_SIZE + 1 +
			 KEYSTORE_CCM_AUTH_SIZE) || (output_size < input_size -
				 1 - KEYSTORE_AES_IV_SIZE -
				 KEYSTORE_CCM_AUTH_SIZE)) {
		res = -EINVAL;
	} else {
		keystore_ctx_t *ctx;

		mutex_lock(&contexts_mutex);
		ctx = keystore_find_context_ticket(client_ticket);
		if (!ctx) {
			mutex_unlock(&contexts_mutex);
			pr_err(KBUILD_MODNAME ": %s: Cannot find context\n", __func__);
			res = -EINVAL;
		} else {
			keystore_slot_t *slot = keystore_find_slot_id(ctx,
					slot_id);
			if (!slot) {
				mutex_unlock(&contexts_mutex);
				pr_err(KBUILD_MODNAME ": %s: Cannot find slot\n", __func__);
				res = -EINVAL;
			} else {
				/* extract AlgoSpec, IV and Cipher from Ciphertext object */
				const uint8_t *in = (const uint8_t *) input;
				keystore_algo_spec_t algo_spec =
					(keystore_algo_spec_t) *in++;
				if (algo_spec != ALGOSPEC_AES) {
					mutex_unlock(&contexts_mutex);
					pr_err(KBUILD_MODNAME ": %s: Unknown algo spec %d\n",
							__func__,
							(int) algo_spec);
					res = -EINVAL;
				} else {
					const void *iv = (const void *) in;
					const unsigned int iv_size =
						KEYSTORE_AES_IV_SIZE;
					uint8_t app_key[slot->app_key_size];

					/* cache slot data (the AppKey) before releasing the lock */
					memcpy(app_key, slot->app_key,
							slot->app_key_size);
					mutex_unlock(&contexts_mutex);

					in += iv_size;
					output_size = input_size - 1 - iv_size -
						KEYSTORE_CCM_AUTH_SIZE;

					/* decrypt Cipher using AppKey and AlgoSpec/IV */
					/* Plaintext = ADEC(AppKey, AlgoSpec, IV, Cipher) */
					res = keystore_aes_ccm_crypt(0, app_key,
							sizeof(app_key), iv,
							iv_size, in,
							input_size - 1 - iv_size,
							NULL, 0,
							output, output_size);
					if (res) {
						pr_err(KBUILD_MODNAME ": %s: Error %d in keystore_aes_ccm_crypt\n",
								__func__, res);
					} else {
						/* OK */
						res = (int) output_size;
					}

					/* clear local copy of AppKey */
					memset(app_key, 0, sizeof(app_key));
				}
			}
		}
	}

	FUNC_RES(res);

	return res;
}
EXPORT_SYMBOL(keystore_decrypt);

/**
 * Retrieve the Keystore Public ECC key
 *
 * @param public_key The Keystore public ECC key
 *
 * @return 0 OK or negative error code (see errno),ENODATA if SEED cannot be used to generate private key or EKEYREJECTED if public key cannot pass verification.
 */
int keystore_get_ksm_key(EccPoint *public_key, uint32_t public_key_size)
{
	int res = 0;
	uint32_t private_key[NUM_ECC_DIGITS];

	FUNC_BEGIN;

	if (!public_key || public_key_size != sizeof(EccPoint))
		return -EINVAL;

	res = keystore_get_ksm_keypair(public_key, sizeof(EccPoint),
			private_key, sizeof(uint32_t) * NUM_ECC_DIGITS);

	memset(private_key, 0, sizeof(uint32_t) * NUM_ECC_DIGITS);

	FUNC_RES(res);

	return res;
}
EXPORT_SYMBOL(keystore_get_ksm_key);

/**
 * Validate OEM public ECC key and encrypt ClientKey with it.
 *
 * @param client_ticket The client ticket (KEYSTORE_CLIENT_TICKET_SIZE bytes).
 * @param OEMpub OEM public ECC key.
 * @param OEMpub_size OEM public ECC key size in bytes.
 * @param output Pointer to the block for encrypted data.
 * @param output_size Output block size in bytes (at least KEYSTORE_BACKUP_SIZE bytes).
 * @param output_signature signature of output buffer
 * @param output_signature_size size of signature
 *
 * @return Wrapped key size in bytes if OK or negative error code (see errno).
 */
int keystore_backup(const void *client_ticket,
		const void *OEMpub, unsigned int OEMpub_size,
		const void *signature, unsigned int signature_size,
		void *output, unsigned int output_size,
		void *output_signature, unsigned int output_signature_size)
{
	int res;
	ias_ecc_keypair_t ias_ecc_keypair;
	int backup_size = 0;

	EccPoint KSMpub;
	uint32_t KSMpriv[NUM_ECC_DIGITS];

	FUNC_BEGIN;

	if (!client_ticket || !OEMpub || !signature ||
			!output || !output_signature) {
		pr_err(KBUILD_MODNAME ": %s: Invalid ptr: client_ticket(%p) OEMpub(%p) KSMpub(%p) KSMpriv(%p) signature(%p) output(%p) output_signature(%p)\n",
				__func__, client_ticket, OEMpub, &KSMpub,
				KSMpriv, signature, output, output_signature);
		res = -EFAULT;
		FUNC_RES(res);
		return res;
	}

	/* Set up the OEM ECC keys to encrypt backup */
	ias_ecc_keypair_init(&ias_ecc_keypair);

	res = keystore_get_raw_ecc_keys(&ias_ecc_keypair, OEMpub,
			OEMpub_size);
	if (res != 0) {
		ias_ecc_keypair_free(&ias_ecc_keypair);
		FUNC_RES(res);
		return res;
	}

	if ((output_size < KEYSTORE_BACKUP_SIZE) ||
			(output_signature_size < ECC_SIGNATURE_SIZE)) {
		pr_err(KBUILD_MODNAME ": %s: Not sufficent memory for output - backup-output: %d(expected: > %d), backup-sig-output: %d(expected: > %d),\n",
				__func__, output_size, KEYSTORE_BACKUP_SIZE,
				output_signature_size, ECC_SIGNATURE_SIZE);
		res = -EINVAL;
		FUNC_RES(res);
		return res;
	}

	if (signature_size != RSA_SIGNATURE_BYTE_SIZE) {
		pr_err(KBUILD_MODNAME ": %s: signature_size: %d	(expected:%d)\n",
				__func__, signature_size,
				RSA_SIGNATURE_BYTE_SIZE);
		res = -EINVAL;
		FUNC_RES(res);
		return res;
	} else {
		/* Calculate the Keystore internal ECC keys */
		res = keystore_get_ksm_keypair(&KSMpub, sizeof(EccPoint),
			KSMpriv, sizeof(uint32_t) * NUM_ECC_DIGITS);

		if (!res) {
			/* verify public key signature */
			res = keystore_check_pub_key(ias_ecc_keypair.public_key,
				ias_ecc_keypair.public_key_size,
				&oem_rsa_public_key,
				signature, RSA_SIGNATURE_BYTE_SIZE);
		} else {
			pr_err(KBUILD_MODNAME ": Error retrieving KSM ECC keys.\n");
		}

		if (!res) {
			keystore_ctx_t *ctx;
			backup_data_t backup;

			memset(&backup, 0, sizeof(backup));

			mutex_lock(&contexts_mutex);
			ctx = keystore_find_context_ticket(client_ticket);

			if (!ctx) {
				mutex_unlock(&contexts_mutex);
				pr_err(KBUILD_MODNAME ": %s: Cannot find context\n", __func__);
				res = -EFAULT;
			} else {
				memcpy(backup.client_id, ctx->client_id,
						sizeof(backup.client_id));
				memcpy(backup.client_key, ctx->client_key,
						sizeof(backup.client_key));
#ifdef DEBUG
#warning " remove critical debug print"
				keystore_hexdump("backup - clientid",
						backup.client_id,
						sizeof(backup.client_id));
				keystore_hexdump("backup - client-key",
						backup.client_key,
						sizeof(backup.client_key));
#endif
				mutex_unlock(&contexts_mutex);

				res = keystore_ecc_encrypt
					(ias_ecc_keypair.public_key,
					 ias_ecc_keypair.public_key_size,
					 &backup, sizeof(backup),
					 output, output_size);
				if (res != KEYSTORE_BACKUP_SIZE) {
					pr_err(KBUILD_MODNAME ": %s: Error %d in keystore_ecc_encrypt\n",
							__func__, res);
				} else {
					backup_size = res;/*store size to return back to calling func */

					res = keystore_ecc_sign2(
							(uint32_t *)KSMpriv,
							output,
							output_size,
							output_signature,
							output_signature_size);
					if (res < 0) {
						pr_err(KBUILD_MODNAME ": %s: Error %d in keystore_ecc_sign\n",
								__func__,
								res);
					} else {
#ifdef DEBUG
						pr_err(KBUILD_MODNAME ": %s: Backup signature size: %d\n", __func__, res);
#endif
						res = keystore_ecc_verify2(
								&KSMpub,
								output,
								output_size,
								output_signature,
								output_signature +
								(NUM_ECC_DIGITS*sizeof(uint32_t)));
						if (res == 0) {
							pr_err(KBUILD_MODNAME ": %s: Backup-data signature verification - OK\n",
									__func__);
							res = backup_size;
						} else {
							pr_err(KBUILD_MODNAME ": %s: Backup-data signature verification - Failed\n",
									__func__);
						}
					}
				}
			}
			memset(&backup, 0, sizeof(backup));/* empty */
		} else {
			/* The RSA signature check failed */
#ifdef DEBUG
			pr_err(KBUILD_MODNAME ": %s: public key size %u\n", __func__, OEMpub_size);
			keystore_hexdump("Backup in pub", OEMpub, OEMpub_size);
			pr_err(KBUILD_MODNAME ": %s: public key size %u\n", __func__, ias_ecc_keypair.public_key_size);
			keystore_hexdump("Backup extracted key",
					ias_ecc_keypair.public_key,
					ias_ecc_keypair.public_key_size);
#endif
			res = -EKEYREJECTED;
		}
	}

	ias_ecc_keypair_free(&ias_ecc_keypair);
	FUNC_RES(res);

	return res;
}
EXPORT_SYMBOL(keystore_backup);

#if defined(KEYSTORE_TEST_MIGRATION)
/**
 * Generate a random ECC private and public key
 *
 * @param public_key public ECC key.
 * @param private_key private ECC key.
 *
 * @return 0 OK or negative error code (see errno),ENODATA if SEED cannot be used to generate private key or EKEYREJECTED if public key cannot pass verification.
 */
int keystore_gen_ecc_keys(EccPoint *public_key, uint32_t public_key_size,
		uint32_t *private_key, uint32_t private_key_size) {

	int res = 0;
	uint8_t random_seed[SEC_SEED_SIZE];

	FUNC_BEGIN;

	if (!public_key || !private_key ||
			public_key_size != sizeof(EccPoint) ||
			private_key_size != KEYSTORE_ECC_PRIV_KEY_SIZE) {
		return -EINVAL;
	}

	res = keystore_get_rdrand(random_seed, SEC_SEED_SIZE);

	if (res == 0) {
		res = keystore_ecc_gen_keys(public_key, public_key_size,
				private_key, private_key_size,
				random_seed, SEC_SEED_SIZE);
	} else {
#if defined(DEBUG)
	   pr_err(KBUILD_MODNAME ": %s: Error generating random seed.\n", __func__);
#endif
	}

	FUNC_RES(res);

	return res;
}
EXPORT_SYMBOL(keystore_gen_ecc_keys);
#endif

#if defined(KEYSTORE_TEST_MIGRATION)
/**
 * Decrypt data encrypted by backup function. Functionality wont be available if
 * proper flags during compilation will no be set
 * @param client_ticket The client ticket (KEYSTORE_CLIENT_TICKET_SIZE bytes).
 * @param OEMpriv private ECC key.
 * @param OEMpriv_size OEM rsa public key size.
 * @param KSMpub private ECC key.
 * @param KSMpub_size OEM rsa public key size.
 * @param data encrypted data
 * @param data_size encrypted data size
 * @param output decrypted data
 * @param output_size decrypted data size
 * @return 0 if OK or negative error code (see errno).
 */
int keystore_test_decrypt_backup(const void *client_ticket,
		void *OEMpriv, unsigned int OEMpriv_size,
		void *KSMpub, unsigned int KSMpub_size,
		void *data, unsigned int data_size,
		void *signature, unsigned int signature_size,
		void *output, unsigned int output_size)
{
	int res = 0;
	keystore_ctx_t *ctx;
	backup_data_t backup;
	backup_data_t backup_dec;
	ias_ecc_keypair_t ias_ecc_keypair;

	FUNC_BEGIN;

	if (!client_ticket || !OEMpriv || !KSMpub ||
			!data || !signature || !output)
		return -EFAULT;

	if (output_size != sizeof(backup) ||
			signature_size != ECC_SIGNATURE_SIZE ||
			KSMpub_size != ECC_PUB_KEY_SIZE)
		return -EINVAL;

	ias_ecc_keypair_init(&ias_ecc_keypair);

	res = keystore_get_raw_ecc_keys(&ias_ecc_keypair, OEMpriv,
			OEMpriv_size);
	if (res != 0) {
		ias_ecc_keypair_free(&ias_ecc_keypair);
		return res;
	}

	memset(&backup, 0, sizeof(backup));

	mutex_lock(&contexts_mutex);
	ctx = keystore_find_context_ticket(client_ticket);

	if (!ctx) {
		mutex_unlock(&contexts_mutex);
		pr_err(KBUILD_MODNAME ": %s: Cannot find context\n",
				__func__);
		res = -EINVAL;
	} else {
		memcpy(backup.client_id, ctx->client_id,
				sizeof(backup.client_id));
		memcpy(backup.client_key, ctx->client_key,
				sizeof(backup.client_key));
		mutex_unlock(&contexts_mutex);

		res = keystore_ecc_verify2((EccPoint *)KSMpub,
				data,
				data_size,
				signature,
				signature + (NUM_ECC_DIGITS*sizeof(uint32_t)));
		if (res != 0)
			pr_err(KBUILD_MODNAME ": %s: Error %d in keystore_ecc_verify_signature\n",
					__func__, res);
		else {
			pr_err(KBUILD_MODNAME ": %s: Signature verfication backup data - OK\n",
					__func__);

			res = keystore_ecc_decrypt(ias_ecc_keypair.private_key,
					ias_ecc_keypair.private_key_size,
					data, data_size,
					&backup_dec, sizeof(backup_dec));

			if (res < 0)
				pr_err(KBUILD_MODNAME ": %s: Error %d in keystore_ecc_decrypt\n",
						__func__, res);
			else {
				res = memcmp(&backup_dec, &backup,
						sizeof(backup_dec));
				if (res != 0) {
					pr_warn("keystore_test_decrypt_backup: decryption failed\n");

					keystore_hexdump("expected:",
							&backup,
							sizeof(backup));
					keystore_hexdump("output  :",
							&backup_dec,
							sizeof(backup_dec));
				} else {
					memcpy(output, &backup_dec,
							sizeof(backup_dec));
				}
			}
		}
	}
	memset(&backup, 0, sizeof(backup));
	memset(&backup_dec, 0, sizeof(backup_dec));

	ias_ecc_keypair_free(&ias_ecc_keypair);

	FUNC_RES(res);

	return res;
}
EXPORT_SYMBOL(keystore_test_decrypt_backup);
#endif

#if defined(KEYSTORE_TEST_MIGRATION)
/**
 * Verify ECC signature using public key
 * @param public_key public ECC key.
 * @param public_key_size public ECC key size.
 * @param data to verify its signature
 * @param data_size size of data to verify signature
 * @param sig signature
 * @param sig_size signature size
 * @return 0 if OK or negative error code (see errno).
 */
int keystore_test_verify_ecc_signature(const void *public_key,
		unsigned int public_key_size, const void *data,
		unsigned int data_size,	const void *sig, unsigned int sig_size)
{
	int res;
	ias_ecc_keypair_t ias_ecc_keypair;

	FUNC_BEGIN;

	if (!public_key || !data || !sig)
		return -EFAULT;

	ias_ecc_keypair_init(&ias_ecc_keypair);

	res = keystore_get_raw_ecc_keys(&ias_ecc_keypair,
			public_key, public_key_size);
	if (res != 0) {
		ias_ecc_keypair_free(&ias_ecc_keypair);
		return res;
	}

	res = keystore_ecc_verify2(ias_ecc_keypair.public_key, data,
			data_size, (void *) sig,
			((void *)sig) + (NUM_ECC_DIGITS*sizeof(uint32_t)));
	if (res != 0)
		pr_err(KBUILD_MODNAME ": %s: Error %d in keystore_ecc_verify_signature\n", __func__, res);
	ias_ecc_keypair_free(&ias_ecc_keypair);

	FUNC_RES(res);

	return res;
}
EXPORT_SYMBOL(keystore_test_verify_ecc_signature);
#endif

#if defined(KEYSTORE_TEST_MIGRATION)
/**
 * Decrypt backup and migration key and then encrypt backup data with migration key
 *
 * @param OEMpriv OEM private ECC key.
 * @param OEMpriv_size OEM private ECC key size.
 * @param backup backuped data.
 * @param backup_size backuped data size.
 * @param OEMpriv2 OEMprivate ECC key from new device.
 * @param OEMpriv2_size OEM private ECC key size.
 * @param mig_key migration key.
 * @param mig_key_size migration key size.
 * @param output re encrypted backup data
 * @param output_size re encrypted backup data size.
 * @param output_sig output signature
 * @param output_sig_size output signature size
 *
 * @return 0 OK or negative error code (see errno).
 */
int keystore_test_reencrypt_backup_with_mk(const void *OEMpriv,
		unsigned int OEMpriv_size, const void *backup,
		unsigned int backup_size, const void *OEMpriv2,
		unsigned int OEMpriv2_size, const void *mig_key,
		unsigned int mig_key_size, void *output,
		unsigned int output_size)
{
	int res = -EINVAL;
	ias_ecc_keypair_t ias_ecc_keypair, ias_ecc_keypair2;
	backup_data_t _backup, aes_backup;
	uint8_t mkey[KEYSTORE_MKEY_SIZE];
	uint8_t *p_reencrypted;
	uint8_t *p_decrypted;
	uint8_t *p_iv_dec;
	int cnt;
	uint8_t iv[KEYSTORE_MAX_IV_SIZE];
	int iv_size = sizeof(iv);

	FUNC_BEGIN;

	if (!OEMpriv || !backup || !OEMpriv2 || !mig_key || !output) {
		FUNC_RES(res);
		return -EINVAL;
	}

	if (sizeof(_backup) + 1 + iv_size + KEYSTORE_GCM_AUTH_SIZE >
			output_size) {
		pr_err(KBUILD_MODNAME ": %s: Error %zu, %u fix output size\n",
				__func__, sizeof(_backup) + 1 + iv_size +
				KEYSTORE_GCM_AUTH_SIZE, output_size);
		FUNC_RES(res);
		return -EFAULT;
	}

	memset(&_backup, 0, sizeof(_backup));
	memset(&aes_backup, 0, sizeof(aes_backup));
	memset(&mkey, 0, sizeof(mkey));
	memset(iv, 3, iv_size);

	ias_ecc_keypair_init(&ias_ecc_keypair);
	ias_ecc_keypair_init(&ias_ecc_keypair2);

	res = keystore_get_raw_ecc_keys(&ias_ecc_keypair, OEMpriv,
			OEMpriv_size);
	if (res != 0)
		goto end;

	res = keystore_get_raw_ecc_keys(&ias_ecc_keypair2,
			OEMpriv2,
			OEMpriv2_size);
	if (res != 0)
		goto end;

	res = keystore_ecc_decrypt(ias_ecc_keypair.private_key,
			ias_ecc_keypair.private_key_size,
			backup, backup_size,
			&_backup, sizeof(_backup));
	if (res < 0) {
		pr_err(KBUILD_MODNAME ": %s: Error %d, fail to decrypt backup\n", __func__, res);
		goto end;
	}

	res = keystore_ecc_decrypt(ias_ecc_keypair2.private_key,
			ias_ecc_keypair2.private_key_size,
			mig_key, mig_key_size,
			&mkey, sizeof(mkey));
	if (res < 0) {
		pr_err(KBUILD_MODNAME ": %s: Error %d, fail to decrypt migration key\n",
				__func__, res);
		goto end;
	}
	keystore_hexdump("Decrypted mkey (ECIES)", mkey, sizeof(mkey));

	p_reencrypted = output;
	p_decrypted = output;
	/* prepare block to be returned */
	/* AlgoSpec || IV || Cipher */
	*p_reencrypted++ = (uint8_t) ALGOSPEC_AES;/*and algo spec at 0 possition */
	memcpy(p_reencrypted, iv, iv_size); /*copy iv to 1...iv_size */
	p_reencrypted += KEYSTORE_AES_IV_SIZE;

	keystore_hexdump("migrate - IV", iv, KEYSTORE_MAX_IV_SIZE);

	/* Cipher = AENC(AppKey, AlgoSpec, IV, Plaintext) */
	res = keystore_aes_gcm_crypt(1, mkey, sizeof(mkey), iv,
			KEYSTORE_AES_IV_SIZE, (const char *) &_backup,
			sizeof(_backup), NULL, 0, p_reencrypted,
			sizeof(_backup) + KEYSTORE_GCM_AUTH_SIZE);
	if (res) {
		pr_err(KBUILD_MODNAME ": %s: Error %d in keystore_aes_gcm_crypt\n",
				__func__, res);
		res = -EFAULT;
		goto end;
	}

	cnt = 1 + iv_size + sizeof(_backup) + KEYSTORE_GCM_AUTH_SIZE;

	pr_err(KBUILD_MODNAME ": %s: GCM encrypted size: %d, IV size: %d\n",
			__func__, cnt, iv_size);
	keystore_hexdump("Backup-eMkey(AES-GCM)", p_reencrypted, output_size);
	/* decrypt Cipher using AppKey and AlgoSpec/IV */
	/* Plaintext = ADEC(AppKey, AlgoSpec, IV, Cipher) */

	p_iv_dec = ++p_decrypted;/*skip algo spec */
	p_decrypted += iv_size;/*skip iv */
	cnt =  cnt - 1 - iv_size;/*correct size */

	res = keystore_aes_gcm_crypt(0, mkey, sizeof(mkey), p_iv_dec, iv_size,
			p_decrypted, cnt, NULL, 0,
			(char *) &aes_backup, cnt - KEYSTORE_GCM_AUTH_SIZE);
	if (res) {
		pr_err(KBUILD_MODNAME ": %s: Error %d in keystore_aes_gcm_crypt\n", __func__, res);
		res = -EFAULT;
		goto end;
	}

	cnt -= KEYSTORE_GCM_AUTH_SIZE;

	pr_err(KBUILD_MODNAME ": %s: GCM decrypted size: %d\n",
			__func__, cnt);
	keystore_hexdump("Backup-dMkey (AES-GCM)", &aes_backup,
			sizeof(aes_backup));

	if (memcmp((const void *)&_backup, (const void *)&aes_backup,
				sizeof(_backup)) != 0) {
		pr_info(KBUILD_MODNAME ": %s: failed to decrypt data\n", __func__);
		keystore_hexdump("original backup:", &_backup, sizeof(_backup));
		keystore_hexdump("aes dec  backup:", &aes_backup,
				sizeof(aes_backup));
		res = -EFAULT;
	}

	goto end;

end:
	ias_ecc_keypair_free(&ias_ecc_keypair);
	ias_ecc_keypair_free(&ias_ecc_keypair2);

	FUNC_RES(res);

	return res;
}
EXPORT_SYMBOL(keystore_test_reencrypt_backup_with_mk);
#endif

/**
 * Get the absolute path of current process in the filesystem. This is used
 * for client authentication purpose.
 *
 * Always use the return variable from this function, input variable 'buf'
 * may not contain the start of the path. In case if kernel was executing
 * a kernel thread then this function return NULL.
 *
 * @param input buf place holder for updating the path
 * @param input buflen avaialbe space
 *
 * @return path of current process, or NULL if it was kernel thread.
 */

char *get_current_process_path(char *buf, int buflen)
{
	struct file *exe_file = NULL;
	char *result = NULL;
	struct mm_struct *mm = NULL;

	mm = get_task_mm(current);
	if (!mm) {
		pr_info(KBUILD_MODNAME ": %s error get_task_mm\n", __func__);
		goto out;
	}

	down_read(&mm->mmap_sem);
	exe_file = mm->exe_file;

	if (exe_file)
		path_get(&exe_file->f_path);

	up_read(&mm->mmap_sem);
	mmput(mm);
	if (exe_file) {
		result = d_path(&exe_file->f_path, buf, buflen);
		path_put(&exe_file->f_path);
	}
out:
	return result;
}

/* end of file */
