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

#ifndef _DAL_CLIENT_H_
#define _DAL_CLIENT_H_

#include <linux/dal.h>
#include <linux/printk.h>
#include <linux/errno.h>
#include <security/keystore_api_common.h>

#define DAL_KEYSTORE_GCM_IV_SIZE 12
#define DAL_KEYSTORE_GCM_AUTH_SIZE 16
#define DAL_KEYSTORE_MAX_WRAP_KEY_LEN 49

typedef enum {
	DAL_KSM_UNINITIALIZED = 0,
	DAL_KSM_INIT,
	DAL_KSM_SESSION_CREATED,
	DAL_KSM_SESSION_CLOSED,
	DAL_KSM_DEINIT
} dal_state;

typedef enum {
	DAL_KEYSTORE_INIT = 1,
	DAL_KEYSTORE_REGISTER = 2,
	DAL_KEYSTORE_UNREGISTER = 9,
	DAL_KEYSTORE_GET_KEYSIZE = 17,
	DAL_KEYSTORE_WRAP_KEYSIZE = 3,
	DAL_KEYSTORE_WRAP_KEY = 4,
	DAL_KEYSTORE_GENERATE_KEY = 18,
	DAL_KEYSTORE_LOAD_KEY = 5,
	DAL_KEYSTORE_UNLOAD_KEY = 6,
	DAL_KEYSTORE_ENCRYPT = 7,
	DAL_KEYSTORE_DECRYPT = 8
} dal_commands;

struct dal_hdr_t {
	dal_state state;
	u64 session_handle;
	u8 *acp_pkg;
	size_t acp_pkg_len;
};

struct dal_key_info {
	enum keystore_key_spec keyspec;
	uint32_t key_size;
	uint32_t wrap_key_size;
};

/**
 * Reads the DAL Keystore conf file from user space and sets
 * dal_keystore_flag variable.
 * The value of dal_keystore_flag decides whether kernel keystore
 * or DAL keystore is used.
 *
 * Return: 0 if OK or negative error code (see errno.h)
 */
int read_dal_keystore_conf(const char *filename);

/**
 * Return value indicates whether the a DAL session has
 * been created using DAL KDI API.
 * DAL session is created only once.
 */
int is_dal_session_created(void);

/**
 * Reads the DAL Keystore applet from user space.
 */
int read_keystore_applet(const char *filename, u8 **buf, size_t *len);

/**
 * Creats DAL session using DAL KDI API.
 * DAL session is created only once.
 */
int create_session(void);

/**
 * Installs the DAL Keystore applet using DAL KDI API.
 */
int install_applet(void);


/**
 * Sends and receives data from kernel to DAL using DAL KDI API.
 */
int send_and_receive(size_t command_id, const u8 *input, size_t input_len,
		u8 **output, size_t *output_len, size_t *response_code);

/**
 * Closes DAL session using DAL KDI API.
 */
int close_session(void);
#endif /* _DAL_CLIENT_H_ */


