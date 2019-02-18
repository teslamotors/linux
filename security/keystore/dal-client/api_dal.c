/*
 *
 * Intel DAL Keystore Linux driver
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
#include <linux/slab.h>

#include <security/keystore_api_kernel.h>

#include "keystore_debug.h"
#include "keystore_client.h"
#include "dal_context_safe.h"
#include "keystore_operations.h"
#include "keystore_rand.h"

#include "../appauth/manifest_verify.h"
#include "dal_client.h"

#define KEYSPEC_DAL_WRAPPED_KEY (165)
#define EXCEPTION_SESSION_NOT_PRESENT (-155)
#define EXCEPTION_KEY_REWRAPPED (-171)

struct dal_key_info key_info = {0};

static int dal_keystore_init(void);
static int dal_keystore_close(void);
static void init_key_cache(void);
static int dal_keystore_install_and_init(void);
static int handle_command_response(int res, int response_code, int *retry,
		const uint8_t *client_ticket, const uint8_t *client_id);
static int pack_int_to_buf(unsigned int i, unsigned char *buf);

/*
 * The value of dal_keystore_flag decides whether kernel keystore
 * or DAL keystore is used
 */
int dal_keystore_flag;

void set_dal_keystore_conf(void)
{
	if (read_dal_keystore_conf(CONFIG_DAL_KEYSTORE_CONF_PATH) == 1) {
		dal_keystore_flag = 1;
		ks_debug(KBUILD_MODNAME ": %s Using DAL Keystore\n", __func__);
	} else
		dal_keystore_flag = 0;
}

static int dal_keystore_init(void)
{
	size_t output_len = 0, response_code = 0;
	int commandId = DAL_KEYSTORE_INIT;
	uint8_t *out_buf = NULL;

	int res = send_and_receive(commandId, NULL, 0,
		&out_buf, &output_len, &response_code);

	if (res) {
		ks_err(KBUILD_MODNAME ": %s Error in send_and_receive: command id = %d %d\n",
		__func__, commandId, res);
		kzfree(out_buf);
		return -EFAULT;
	}
	init_key_cache();
	kzfree(out_buf);

	return 0;
}

static int dal_keystore_close(void)
{
	close_session();

	return 0;
}

static int dal_calc_clientid(u8 *client_id, const unsigned int client_id_size)
{
	/* Calculate the Client ID */
#ifdef CONFIG_APPLICATION_AUTH
	return keystore_calc_clientid(client_id, client_id_size,
				      MANIFEST_CACHE_TTL,
				      MANIFEST_DEFAULT_CAPS);
#else
	return keystore_calc_clientid(client_id, client_id_size);
#endif

}

static int dal_keystore_install_and_init(void)
{
	int res = 0;
	if (install_applet()) {
		res = -EFAULT;
		ks_err("install_applet() failed\n");
		goto ret;
	}
	if (dal_keystore_init()) {
		res = -EFAULT;
		ks_err("dal_keystore_init() failed\n");
		goto ret;
	}
	ks_debug("dal_keystore_init: success\n");

ret:
	return res;
}

static int dal_keystore_register_client(enum keystore_seed_type seed_type,
				const uint8_t *client_ticket, const uint8_t *client_id)
{
	int res = 0;
	size_t output_len = 0, response_code = 0;
	int commandId = DAL_KEYSTORE_REGISTER;
	uint8_t input[KEYSTORE_MAX_CLIENT_ID_SIZE
				  + KEYSTORE_CLIENT_TICKET_SIZE + 2];
	uint8_t *out_buf = NULL;
	size_t index = 0;

	FUNC_BEGIN;

	if (!client_ticket || !client_id) {
		res = -EFAULT;
		goto err;
	}

	memcpy(input, client_id, sizeof(client_id));
	index += sizeof(client_id);
	memcpy(input + index, client_ticket, KEYSTORE_CLIENT_TICKET_SIZE);
	index += KEYSTORE_CLIENT_TICKET_SIZE;
	pack_int_to_buf(seed_type, input + index);
	index += 2;

cmd_retry:
	res = send_and_receive(commandId, input, index,
		&out_buf, &output_len, &response_code);

	int retry = 0;
	res = handle_command_response(res, response_code, &retry, NULL, NULL);
	if (res) {
		ks_info(KBUILD_MODNAME ": %s Error in send_and_receive: command id = %d %d %d\n",
					 __func__, commandId, res, response_code);

		kzfree(out_buf);
		goto close_session;
	}

	kzfree(out_buf);

	FUNC_RES(res);
	return res;
close_session:
	dal_keystore_close();
err:
	return res;
}

static int restore_client_session(const uint8_t *client_ticket,
		const uint8_t *client_id)
{
	enum keystore_seed_type seed_type;
	int res = 0;

	res = dal_ctx_get_client_seed_type(client_ticket, &seed_type, client_id);

	if (res) {
		ks_info(KBUILD_MODNAME ": %s: Get client seed type failed\n",
			   __func__);
		return res;
	}

	res = dal_keystore_register_client(seed_type, client_ticket, client_id);

	if (res)
		ks_info(KBUILD_MODNAME ": %s: Client re-register failed\n",
			   __func__);

	return res;
}

static int handle_command_response(int res, int response_code, int *retry,
		const uint8_t *client_ticket, const uint8_t *client_id)
{
	if (res == DAL_KDI_STATUS_INTERNAL_ERROR) {
		*retry = 1;
		res = dal_keystore_install_and_init();
		ks_info(KBUILD_MODNAME ": %s install/init status: %d\n",
				__func__, res);
		return res;
	} else if (res == 0 && response_code == EXCEPTION_SESSION_NOT_PRESENT
				&& client_ticket && client_id) {
		*retry = 1;
		res = restore_client_session(client_ticket, client_id);
		ks_info(KBUILD_MODNAME ": %s restore session status: %d\n",
				__func__, res);
		return res;
	} else if (res == 0 &&
			response_code == EXCEPTION_KEY_REWRAPPED) {
		*retry = 0;
		ks_info(KBUILD_MODNAME ": %s command exception: %d\n",
				__func__, response_code);
		return -EAGAIN;
	} else if (res == 0 && response_code < 0) {
		*retry = 0;
		ks_info(KBUILD_MODNAME ": %s command exception: %d\n",
				__func__, response_code);
		return response_code;
	}

	/* falls thru */
	*retry = 0;
	ks_info(KBUILD_MODNAME ": %s passing thru status: %d %d\n",
			__func__, res, response_code);
	return res;
}


int dal_keystore_register(enum keystore_seed_type seed_type,
				uint8_t *client_ticket)
{
	uint8_t client_id[KEYSTORE_MAX_CLIENT_ID_SIZE];
	uint8_t input[KEYSTORE_MAX_CLIENT_ID_SIZE
				  + KEYSTORE_CLIENT_TICKET_SIZE + 2];
	int res = 0;
	size_t output_len = 0, response_code = 0;
	int commandId = DAL_KEYSTORE_REGISTER;
	uint8_t *out_buf = NULL;
	size_t index = 0;

	FUNC_BEGIN;

	if (!client_ticket) {
		res = -EFAULT;
		goto err;
	}

	if (is_dal_session_created()) {
		res = dal_keystore_install_and_init();
		if (res) {
			ks_err(KBUILD_MODNAME ": %s Applet install and init failed %d\n",
				 __func__, res);
			goto close_session;
		}
	}

	res = dal_calc_clientid(client_id, sizeof(client_id));
	if (res) {
		ks_err(KBUILD_MODNAME ": %s Error calculating client ID: %d\n",
		       __func__, res);
		goto close_session;
	}

	/* randomize a buffer */
	keystore_get_rdrand(client_ticket, KEYSTORE_CLIENT_TICKET_SIZE);

	memcpy(input, client_id, sizeof(client_id));
	index += sizeof(client_id);
	memcpy(input + index, client_ticket, KEYSTORE_CLIENT_TICKET_SIZE);
	index += KEYSTORE_CLIENT_TICKET_SIZE;
	pack_int_to_buf(seed_type, input + index);
	index += 2;

cmd_retry:
	res = send_and_receive(commandId, input, index,
		&out_buf, &output_len, &response_code);

	int retry = 0;
	res = handle_command_response(res, response_code, &retry, NULL, NULL);

	if (!res && retry) {
		response_code = 0;
		/* It's very unlikely that we slept between applet initialization
		 * and client registrastion, but just in case... */
		goto cmd_retry;
	}

	if (res) {
		ks_err(KBUILD_MODNAME ": %s Error in send_and_receive: command id = %d %d\n",
			 __func__, commandId, res);
		kzfree(out_buf);
		goto close_session;
	}

	/* Add the new client to the context */
	res = dal_ctx_add_client(client_ticket, seed_type, client_id);
	if (res) {
		ks_err(KBUILD_MODNAME ": %s: Cannot allocate context\n",
			   __func__);
		kzfree(out_buf);
		goto close_session;
	}

	kzfree(out_buf);

	FUNC_RES(res);
	return res;
close_session:
	dal_keystore_close();
err:
	return res;
}
EXPORT_SYMBOL(dal_keystore_register);

int dal_keystore_unregister(const uint8_t *client_ticket)
{
	uint8_t input[KEYSTORE_MAX_CLIENT_ID_SIZE + KEYSTORE_CLIENT_TICKET_SIZE];
	int res = 0;
	size_t output_len = 0, response_code = 0;
	int commandId = DAL_KEYSTORE_UNREGISTER;
	uint8_t *out_buf = NULL;

	FUNC_BEGIN;
	res = dal_calc_clientid(input, KEYSTORE_MAX_CLIENT_ID_SIZE);

	if (res) {
		ks_err(KBUILD_MODNAME ": %s Error calculating client ID: %d\n",
			__func__, res);
		return res;
	}

	memcpy(input + KEYSTORE_MAX_CLIENT_ID_SIZE,
			client_ticket, KEYSTORE_CLIENT_TICKET_SIZE);

	res = send_and_receive(commandId, input, sizeof(input),
		&out_buf, &output_len, &response_code);

	int retry = 0;
	res = handle_command_response(res, response_code, &retry, NULL, NULL);

	if (!res && retry) {
		response_code = 0;
		/* DAL Keystore session is reset, just reinitialized the applet */
	}

	if (res) {
		ks_err(KBUILD_MODNAME ": %s Error in send_and_receive: command id = %d %d\n",
				__func__, commandId, res);
	}

	res = dal_ctx_remove_client(client_ticket);

	if (res)
		ks_err(KBUILD_MODNAME ": %s: Cannot find context\n",
				__func__);

	kzfree(out_buf);
	FUNC_RES(res);
	return res;
}
EXPORT_SYMBOL(dal_keystore_unregister);

static int pack_int_to_buf(unsigned int i, unsigned char *buf)
{
	/* integers are represented by 2 bytes currently */
	if (i >= 0x10000)
		return -EFAULT;
	buf[0] = i & 0xFF;
	buf[1] = (i >> 8) & 0xFF;

	return 0;
}

static void init_key_cache(void)
{
	key_info.keyspec = KEYSPEC_INVALID;
	key_info.key_size = 0;
	key_info.wrap_key_size = 0;
}

static int get_cached_wrapped_key_size(void)
{
	if (key_info.keyspec != KEYSPEC_INVALID) {
		if (key_info.wrap_key_size != 0)
			return key_info.wrap_key_size;
	}
	return -EFAULT;
}

static int get_cached_key_size(void)
{
	if (key_info.keyspec != KEYSPEC_INVALID) {
		if (key_info.key_size != 0)
			return key_info.key_size;
	}
	return -EFAULT;
}

static void cache_wrapped_key_size(enum keystore_key_spec keyspec,
				uint32_t wrap_key_size)
{
	key_info.keyspec = keyspec;
	key_info.wrap_key_size = wrap_key_size;
}

static void cache_key_size(enum keystore_key_spec keyspec, uint32_t key_size)
{
	key_info.keyspec = keyspec;
	key_info.key_size = key_size;
}

int dal_keystore_wrapped_key_size(enum keystore_key_spec keyspec,
			      unsigned int *size,
			      unsigned int *unwrapped_size)
{
	uint8_t client_id[KEYSTORE_MAX_CLIENT_ID_SIZE];
	int res = 0;
	size_t output_len = 0, response_code = 0;
	unsigned char buf[2];
	int commandId = 0;
	int key_len = 0;
	uint8_t *out_buf = NULL;

	FUNC_BEGIN;
	res = dal_calc_clientid(client_id, sizeof(client_id));

	if (res) {
		ks_err(KBUILD_MODNAME ": %s Error calculating client ID: %d\n",
			__func__, res);
		return res;
	}

	pack_int_to_buf(keyspec, buf);
	commandId = DAL_KEYSTORE_GET_KEYSIZE;

get_keysize_retry:
	res = send_and_receive(commandId, buf, 2,
		&out_buf, &output_len, &response_code);
	int retry = 0;
	res = handle_command_response(res, response_code, &retry, NULL, client_id);

	if (!res && retry) {
		response_code = 0;
		goto get_keysize_retry;
	}

	if (res) {
		ks_err(KBUILD_MODNAME ": %s Error in send_and_receive: command id = %d %d\n",
			__func__, commandId, res);
		kzfree(out_buf);
		return res;
	}
	key_len = response_code;

	cache_key_size(keyspec, key_len);
	response_code = 0;

	commandId = DAL_KEYSTORE_WRAP_KEYSIZE;

get_wrap_keysize_retry:
	res = send_and_receive(commandId, buf, 2,
		&out_buf, &output_len, &response_code);
	retry = 0;
	res = handle_command_response(res, response_code, &retry, NULL, client_id);

	if (!res && retry) {
		response_code = 0;
		goto get_wrap_keysize_retry;
	}

	if (res) {
		ks_err(KBUILD_MODNAME ": %s Error in send_and_receive: command id = %d %d\n",
			__func__, commandId, res);
		kzfree(out_buf);
		return res;
	}

	if (unwrapped_size)
		*unwrapped_size = key_len;
	if (size)
		*size = response_code;

	cache_wrapped_key_size(keyspec, response_code);

	kzfree(out_buf);

	FUNC_RES(res);
	return res;
}
EXPORT_SYMBOL(dal_keystore_wrapped_key_size);

static int dal_get_wrapped_key_size(enum keystore_key_spec keyspec)
{
	int res = 0;
	size_t output_len = 0, response_code = 0;
	unsigned char buf[2];
	int commandId = DAL_KEYSTORE_WRAP_KEYSIZE;
	uint8_t *out_buf = NULL;

	pack_int_to_buf(keyspec, buf);

cmd_retry:
	res = send_and_receive(commandId, buf, 2,
		&out_buf, &output_len, &response_code);
	int retry = 0;
	res = handle_command_response(res, response_code, &retry, NULL, NULL);

	if (!res && retry) {
		response_code = 0;
		goto cmd_retry;
	}

	if (res) {
		ks_err(KBUILD_MODNAME ": %s Error in send_and_receive: command id = %d %d\n",
			__func__, commandId, res);
		kzfree(out_buf);
		return res;
	}
	cache_wrapped_key_size(keyspec, response_code);

	kzfree(out_buf);

	FUNC_RES(res);
	return response_code;
}

int dal_keystore_wrap_key(const uint8_t *client_ticket,
		      const uint8_t *app_key, unsigned int app_key_size,
		      enum keystore_key_spec keyspec, uint8_t *wrapped_key)
{
	uint8_t client_id[KEYSTORE_MAX_CLIENT_ID_SIZE];
	int res = 0;
	size_t response_code = 0;
	uint8_t input[KEYSTORE_MAX_CLIENT_ID_SIZE + KEYSTORE_CLIENT_TICKET_SIZE
				+ app_key_size + 2];
	int commandId = DAL_KEYSTORE_WRAP_KEY;
	size_t output_len = 0;
	uint8_t *out_buf = NULL;
	size_t index = 0;

	FUNC_BEGIN;

	res = dal_calc_clientid(client_id, KEYSTORE_MAX_CLIENT_ID_SIZE);

	if (res) {
		ks_err(KBUILD_MODNAME ": %s Error calculating client ID: %d\n",
			__func__, res);
		return res;
	}

	memcpy(input, client_id, sizeof(client_id));
	index += sizeof(client_id);
	memcpy(input + index, client_ticket, KEYSTORE_CLIENT_TICKET_SIZE);
	index += KEYSTORE_CLIENT_TICKET_SIZE;
	pack_int_to_buf(keyspec, input + index);
	index += 2;
	memcpy(input + index, app_key, app_key_size);
	index += app_key_size;

cmd_retry:
	output_len = get_cached_wrapped_key_size();

	res = send_and_receive(commandId, input, sizeof(input),
		&out_buf, &output_len, &response_code);
	int retry = 0;
	res = handle_command_response(res, response_code, &retry,
			client_ticket, client_id);

	if (!res && retry) {
		response_code = 0;
		goto cmd_retry;
	}

	if (res) {
		ks_err(KBUILD_MODNAME ": %s Error in send_and_receive: command id = %d %d %d\n",
			__func__, commandId, res, response_code);

		goto exit;
	}
	memcpy(wrapped_key, out_buf, output_len);

exit:
	if (out_buf)
		kzfree(out_buf);

	FUNC_RES(res);
	return res;
}
EXPORT_SYMBOL(dal_keystore_wrap_key);

int dal_keystore_generate_key(const uint8_t *client_ticket,
			const enum keystore_key_spec keyspec,
			uint8_t *wrapped_key)
{
	uint8_t client_id[KEYSTORE_MAX_CLIENT_ID_SIZE];
	int res = 0;
	size_t response_code = 0;
	uint8_t input[KEYSTORE_MAX_CLIENT_ID_SIZE + KEYSTORE_CLIENT_TICKET_SIZE + 2];
	int commandId = DAL_KEYSTORE_GENERATE_KEY;
	size_t output_len = 0;
	uint8_t *out_buf = NULL;
	size_t index = 0;

	FUNC_BEGIN;

	res = dal_calc_clientid(client_id, sizeof(client_id));

	if (res) {
		ks_err(KBUILD_MODNAME ": %s Error calculating client ID: %d\n",
			__func__, res);
		return res;
	}

	memcpy(input, client_id, sizeof(client_id));
	index += sizeof(client_id);
	memcpy(input + index, client_ticket, KEYSTORE_CLIENT_TICKET_SIZE);
	index += KEYSTORE_CLIENT_TICKET_SIZE;
	pack_int_to_buf(keyspec, input + index);
	index += 2;

cmd_retry:
	output_len = dal_get_wrapped_key_size(keyspec);

	res = send_and_receive(commandId, input, index,
		&out_buf, &output_len, &response_code);
	int retry = 0;
	res = handle_command_response(res, response_code, &retry,
			client_ticket, client_id);

	if (!res && retry) {
		response_code = 0;
		goto cmd_retry;
	}

	if (res) {
		ks_err(KBUILD_MODNAME ": %s Error in send_and_receive: command id = %d %d\n",
			__func__, commandId, res);
		kzfree(out_buf);
		return res;
	}

	memcpy(wrapped_key, out_buf, output_len);
	kzfree(out_buf);

	FUNC_RES(res);
	return res;
}
EXPORT_SYMBOL(dal_keystore_generate_key);

int dal_keystore_load_key(const uint8_t *client_ticket,
			uint8_t *wrapped_key,
			unsigned int wrapped_key_size, unsigned int *slot_id)
{
	int res = 0;
	uint8_t client_id[KEYSTORE_MAX_CLIENT_ID_SIZE];
	size_t output_len = 0;
	int commandId = DAL_KEYSTORE_LOAD_KEY;
	size_t response_code = 0;
	uint8_t *out_buf = NULL;

	FUNC_BEGIN;

	if (!slot_id)
		return -EFAULT;

	if (wrapped_key_size > DAL_KEYSTORE_MAX_WRAP_KEY_LEN) {
		ks_err(KBUILD_MODNAME ": %s: wrapped_key_size %u, expected < %u",
				__func__, wrapped_key_size, DAL_KEYSTORE_MAX_WRAP_KEY_LEN);
		return -EINVAL;
	}

	uint8_t input[KEYSTORE_MAX_CLIENT_ID_SIZE
				  + KEYSTORE_CLIENT_TICKET_SIZE + wrapped_key_size];

	res = dal_calc_clientid(client_id, KEYSTORE_MAX_CLIENT_ID_SIZE);

	if (res) {
		ks_err(KBUILD_MODNAME ": %s Error calculating client ID: %d\n",
				__func__, res);
		return res;
	}

	size_t index = 0;
	memcpy(input, client_id, sizeof(client_id));
	index += sizeof(client_id);
	memcpy(input + index, client_ticket, KEYSTORE_CLIENT_TICKET_SIZE);
	index += KEYSTORE_CLIENT_TICKET_SIZE;
	memcpy(input + index, wrapped_key, wrapped_key_size);
	index += wrapped_key_size;

cmd_retry:
	output_len = wrapped_key_size;
	res = send_and_receive(commandId, input, index,
			&out_buf, &output_len, &response_code);

	int retry = 0;
	res = handle_command_response(res, response_code, &retry,
			client_ticket, client_id);

	if (!res && retry) {
		response_code = 0;
		goto cmd_retry;
	}

	if (res) {
		ks_err(KBUILD_MODNAME ": %s Error in send_and_receive: command id = %d %d\n",
				__func__, commandId, res);

		if (res == -EAGAIN && output_len && out_buf) {
			/* The key was re-wrapped with current SEED
			 * so client has to update it. */
			memcpy(wrapped_key, out_buf, output_len);
		}

		kzfree(out_buf);
		return res;
	}

	res = dal_ctx_add_wrapped_key(client_ticket, KEYSPEC_DAL_WRAPPED_KEY,
			wrapped_key, wrapped_key_size, slot_id);
	if (res) {
		ks_err(KBUILD_MODNAME ": %s: Insert wrapped key to slot failed\n",
				__func__);
	}

	FUNC_RES(res);
	return res;
}
EXPORT_SYMBOL(dal_keystore_load_key);

int dal_keystore_unload_key(const uint8_t *client_ticket, unsigned int slot_id)
{
	int res = 0;

	FUNC_BEGIN;

	res = dal_ctx_remove_wrapped_key(client_ticket, slot_id);

	FUNC_RES(res);
	return res;
}
EXPORT_SYMBOL(dal_keystore_unload_key);

static int dal_encrypt_output_size(enum keystore_algo_spec algo_spec,
			unsigned int input_size,
			unsigned int *output_size)
{
	if (!output_size)
		return -EFAULT;

	*output_size = 0;

	switch (algo_spec) {
	case ALGOSPEC_AES_GCM:
		*output_size = input_size + DAL_KEYSTORE_GCM_AUTH_SIZE;
		break;
	default:
		ks_err(KBUILD_MODNAME ": Unknown or invalid algo_spec: %u\n",
		algo_spec);
		return -EINVAL;
	}

	return 0;
}

int dal_keystore_encrypt_size(enum keystore_algo_spec algo_spec,
			  unsigned int input_size, unsigned int *output_size)
{
	int res = 0;

	FUNC_BEGIN;
	res = dal_encrypt_output_size(algo_spec, input_size, output_size);

	FUNC_RES(res);
	return res;
}
EXPORT_SYMBOL(dal_keystore_encrypt_size);


int dal_keystore_encrypt(const uint8_t *client_ticket, int slot_id,
			enum keystore_algo_spec algo_spec,
			const uint8_t *iv, unsigned int iv_size,
			const uint8_t *input, unsigned int input_size,
			uint8_t *output)
{
	int res = 0;
	uint8_t client_id[KEYSTORE_MAX_CLIENT_ID_SIZE];
	uint8_t wrapped_key[DAL_KEYSTORE_MAX_WRAP_KEY_LEN];
	unsigned int wrapped_key_size = DAL_KEYSTORE_MAX_WRAP_KEY_LEN;
	enum keystore_key_spec key_spec = KEYSPEC_INVALID;
	int commandId = DAL_KEYSTORE_ENCRYPT;
	const size_t in_size = (KEYSTORE_MAX_CLIENT_ID_SIZE
			+ KEYSTORE_CLIENT_TICKET_SIZE + DAL_KEYSTORE_MAX_WRAP_KEY_LEN
			+ DAL_KEYSTORE_GCM_IV_SIZE + input_size + 8);
	size_t index = 0;
	size_t output_len = 0;
	size_t response_code = 0;
	uint8_t *in = NULL;
	uint8_t *out_buf = NULL;

	FUNC_BEGIN;

	in = kmalloc(in_size, GFP_KERNEL);
	if (!in) {
		res = -ENOMEM;
		goto exit;
	}

	ks_debug(KBUILD_MODNAME
		": keystore_encrypt slot_id=%d algo_spec=%d iv_size=%u isize=%u\n",
		slot_id, (int)algo_spec, iv_size, input_size);

	if (!iv || iv_size < DAL_KEYSTORE_GCM_IV_SIZE || iv_size > KEYSTORE_DAL_MAX_IV_SIZE) {
		ks_err(KBUILD_MODNAME ": Incorrect input values to %s\n",
				       __func__);
		return -EINVAL;
	}

	iv_size = DAL_KEYSTORE_GCM_IV_SIZE;

	res = dal_calc_clientid(client_id, sizeof(client_id));

	if (res) {
		ks_err(KBUILD_MODNAME ": %s Error calculating client ID: %d\n",
			__func__, res);
		goto exit;
	}

	memcpy(in, client_id, sizeof(client_id));
	index += sizeof(client_id);
	memcpy(in + index, client_ticket, KEYSTORE_CLIENT_TICKET_SIZE);
	index += KEYSTORE_CLIENT_TICKET_SIZE;

	/* Get the wrapped application key */
	res = dal_ctx_get_wrapped_key(client_ticket, slot_id, &key_spec,
			wrapped_key, &wrapped_key_size);

	if (res) {
		ks_err(KBUILD_MODNAME ": %s: Error %d getting wrapped key.\n",
			   __func__, res);
		goto exit;
	}

	if (key_spec != KEYSPEC_DAL_WRAPPED_KEY ||
		wrapped_key_size > DAL_KEYSTORE_MAX_WRAP_KEY_LEN) {
			ks_err(KBUILD_MODNAME ": %s: Invalid key retrived from wrapped key cache.\n",
				   __func__, res);
		goto exit;
	}

	res = pack_int_to_buf(wrapped_key_size, (uint8_t *)(in + index));
	index += 2;
	memcpy(in + index, wrapped_key, wrapped_key_size);
	index += wrapped_key_size;
	res = pack_int_to_buf(iv_size, (uint8_t *)(in + index));
	index += 2;
	memcpy(in + index, iv, iv_size);
	index += iv_size;
	res = pack_int_to_buf(input_size, (uint8_t *)(in + index));
	index += 2;
	memcpy(in + index, input, input_size);
	index += input_size;

cmd_retry:
	res = dal_keystore_encrypt_size(algo_spec, input_size,
				(unsigned int *)&output_len);
	if (res) {
		res = -ENOMEM;
		goto exit;
	}

	res = send_and_receive(commandId, in, index,
		&out_buf, &output_len, &response_code);

	int retry = 0;
	res = handle_command_response(res, response_code, &retry,
				client_ticket, client_id);

	if (!res && retry) {
		response_code = 0;
		goto cmd_retry;
	}

	if (res) {
		ks_err(KBUILD_MODNAME ": %s Error in send_and_receive: command id = %d %d\n",
			__func__, commandId, res);
		goto exit;
	}
	memcpy(output, out_buf, output_len);

exit:
	if (in)
		kzfree(in);
	if (out_buf)
		kzfree(out_buf);

	FUNC_RES(res);
	return res;
}
EXPORT_SYMBOL(dal_keystore_encrypt);

static int dal_decrypt_output_size(enum keystore_algo_spec algo_spec,
			unsigned int input_size,
			unsigned int *output_size)
{
	if (!output_size)
		return -EFAULT;

	*output_size = 0;

	switch (algo_spec) {
	case ALGOSPEC_AES_GCM:
		if (input_size < DAL_KEYSTORE_GCM_AUTH_SIZE)
			return -EINVAL;
		*output_size = input_size - DAL_KEYSTORE_GCM_AUTH_SIZE;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

int dal_keystore_decrypt_size(enum keystore_algo_spec algo_spec,
		unsigned int input_size, unsigned int *output_size)
{
	return dal_decrypt_output_size(algo_spec, input_size, output_size);
}
EXPORT_SYMBOL(dal_keystore_decrypt_size);

int dal_keystore_decrypt(const uint8_t *client_ticket, int slot_id,
			enum keystore_algo_spec algo_spec,
			const uint8_t *iv, unsigned int iv_size,
			const uint8_t *input, unsigned int input_size,
			uint8_t *output)
{
	int res = 0;
	uint8_t client_id[KEYSTORE_MAX_CLIENT_ID_SIZE];
	uint8_t wrapped_key[DAL_KEYSTORE_MAX_WRAP_KEY_LEN];
	unsigned int wrapped_key_size = DAL_KEYSTORE_MAX_WRAP_KEY_LEN;
	enum keystore_key_spec key_spec = KEYSPEC_INVALID;
	int commandId = DAL_KEYSTORE_DECRYPT;
	const size_t in_size = (KEYSTORE_MAX_CLIENT_ID_SIZE
			+ KEYSTORE_CLIENT_TICKET_SIZE + DAL_KEYSTORE_MAX_WRAP_KEY_LEN
			+ DAL_KEYSTORE_GCM_IV_SIZE + input_size + 8);
	size_t index = 0;
	size_t output_len = 0;
	size_t response_code = 0;
	uint8_t *in = NULL;
	uint8_t *out_buf = NULL;

	FUNC_BEGIN;

	in = kmalloc(in_size, GFP_KERNEL);
	if (!in) {
		res = -ENOMEM;
		goto exit;
	}

	ks_debug(KBUILD_MODNAME
		": keystore_decrypt slot_id=%d algo_spec=%d iv_size=%u isize=%u\n",
		slot_id, (int)algo_spec, iv_size, input_size);

	if (!iv || iv_size < DAL_KEYSTORE_GCM_IV_SIZE || iv_size > KEYSTORE_DAL_MAX_IV_SIZE) {
		ks_err(KBUILD_MODNAME ": Incorrect input values to %s\n",
				       __func__);
		return -EINVAL;
	}

	iv_size = DAL_KEYSTORE_GCM_IV_SIZE;

	res = dal_calc_clientid(client_id, sizeof(client_id));

	if (res) {
		ks_err(KBUILD_MODNAME ": %s Error calculating client ID: %d\n",
			__func__, res);
		goto exit;
	}

	memcpy(in, client_id, sizeof(client_id));
	index += sizeof(client_id);
	memcpy(in + index, client_ticket, KEYSTORE_CLIENT_TICKET_SIZE);
	index += KEYSTORE_CLIENT_TICKET_SIZE;

	/* Get the wrapped application key */
	res = dal_ctx_get_wrapped_key(client_ticket, slot_id, &key_spec,
			wrapped_key, &wrapped_key_size);

	if (res) {
		ks_err(KBUILD_MODNAME ": %s: Error %d getting wrapped key.\n",
			   __func__, res);
		goto exit;
	}

	if (key_spec != KEYSPEC_DAL_WRAPPED_KEY ||
		wrapped_key_size > DAL_KEYSTORE_MAX_WRAP_KEY_LEN) {
			ks_err(KBUILD_MODNAME ": %s: Invalid key retrived from wrapped key cache.\n",
				   __func__, res);
		goto exit;
	}

	res = pack_int_to_buf(wrapped_key_size, (uint8_t *)(in + index));
	index += 2;
	memcpy(in + index, wrapped_key, wrapped_key_size);
	index += wrapped_key_size;
	res = pack_int_to_buf(iv_size, (uint8_t *)(in + index));
	index += 2;
	memcpy(in + index, iv, iv_size);
	index += iv_size;
	res = pack_int_to_buf(input_size, (uint8_t *)(in + index));
	index += 2;
	memcpy(in + index, input, input_size);
	index += input_size;

cmd_retry:
	res = dal_keystore_decrypt_size(algo_spec, input_size,
		(unsigned int *)&output_len);
	if (res)
		return res;

	res = send_and_receive(commandId, in, index,
		&out_buf, &output_len, &response_code);

	int retry = 0;
	res = handle_command_response(res, response_code, &retry,
				client_ticket, client_id);

	if (!res && retry) {
		response_code = 0;
		goto cmd_retry;
	}

	if (res) {
		ks_err(KBUILD_MODNAME ": %s Error in send_and_receive: command id = %d %d\n",
		__func__, commandId, res);
		goto exit;
	}

	memcpy(output, out_buf, output_len);

exit:
	if (in)
		kzfree(in);
	if (out_buf)
		kzfree(out_buf);

	FUNC_RES(res);
	return res;
}
EXPORT_SYMBOL(dal_keystore_decrypt);
/* end of file */
