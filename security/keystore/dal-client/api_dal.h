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
#ifndef _DAL_KEYSTORE_API_H_
#define _DAL_KEYSTORE_API_H_

/**
 * KEYSTORE_DAL_MAX_IV_SIZE - Maximum size of the Initialization Vector for the DAL backend
 */
#define KEYSTORE_DAL_MAX_IV_SIZE    12

#include "dal_context.h"

void set_dal_keystore_conf(void);

/**
 * Register a client with DAL Keystore. The client is identified using the
 * path of the executable of the calling user process.
 *
 * A random ticket is generated and returned to the caller.
 *
 * Return: 0 if OK or negative error code (see errno.h)
 */
int dal_keystore_register(enum keystore_seed_type seed_type,
			uint8_t *client_ticket);

/**
 * unregister a client which has registered earlier.
 *
 * Return: 0 if OK or negative error code (see errno.h)
 */
int dal_keystore_unregister(const uint8_t *client_ticket);

/**
 * Provides the wrapped key size and key size corresponding to the keyspec.
 *
 * Return: 0 if OK or negative error code (see errno.h)
 */
int dal_keystore_wrapped_key_size(enum keystore_key_spec keyspec,
			unsigned int *size,
			unsigned int *unwrapped_size);

/**
 * Returns the wrapped key size corresponding to the keyspec.
 *
 * Return: negative error code in case of error (see errno.h)
 */
int dal_get_wrapped_key_size(enum keystore_key_spec keyspec);

/**
 * Wraps an application key using DAL Keystore.
 * application key size and key spec are inputs.
 *
 * Return: 0 if OK or negative error code (see errno.h)
 */
int dal_keystore_wrap_key(const uint8_t *client_ticket,
			const uint8_t *app_key, unsigned int app_key_size,
			enum keystore_key_spec keyspec, uint8_t *wrapped_key);

/**
 * Generates an application key inside DAL Keystore and returns it
 * wrapped.
 * key spec is input.
 *
 * Return: 0 if OK or negative error code (see errno.h)
 */
int dal_keystore_generate_key(const uint8_t *client_ticket,
			const enum keystore_key_spec keyspec,
			uint8_t *wrapped_key);

/**
 * Loads a wrapped key in DAL Keystore.
 * id of the loaded key slot is passed back to the caller.
 *
 * Return: 0 if OK or negative error code (see errno.h)
 */
int dal_keystore_load_key(const uint8_t *client_ticket,
			uint8_t *wrapped_key,
			unsigned int wrapped_key_size, unsigned int *slot_id);

/**
 * Unloads a previously loaded key in DAL Keystore.
 * id of the loaded key slot is input.
 *
 * Return: 0 if OK or negative error code (see errno.h)
 */
int dal_keystore_unload_key(const uint8_t *client_ticket, unsigned int slot_id);

/**
 * Provides the output size corresponding to the input size for encryption.
 * algo spec and input size are inputs.
 * output size is passed back to the caller.
 * Return: 0 if OK or negative error code (see errno.h)
 */
int dal_keystore_encrypt_size(enum keystore_algo_spec algo_spec,
			unsigned int input_size, unsigned int *output_size);

/**
 * Encrypts application data using DAL Keystore.
 * slot id, algo spec, initialization vector, input data are
 * inputs.
 * encrypted data is passed back to the caller.
 *
 * Return: 0 if OK or negative error code (see errno.h)
 */
int dal_keystore_encrypt(const uint8_t *client_ticket, int slot_id,
			enum keystore_algo_spec algo_spec,
			const uint8_t *iv, unsigned int iv_size,
			const uint8_t *input, unsigned int input_size,
			uint8_t *output);

/**
 * Provides the output size corresponding to the input size for decryption.
 * algo spec and input size are inputs.
 * output size is passed back to the caller.
 * Return: 0 if OK or negative error code (see errno.h)
 */
int dal_keystore_decrypt_size(enum keystore_algo_spec algo_spec,
			unsigned int input_size, unsigned int *output_size);

/**
 * Decrypts application data using DAL Keystore.
 * slot id, algo spec, initialization vector, encrypted data are
 * inputs.
 * decrypted data is passed back to the caller.
 *
 * Return: 0 if OK or negative error code (see errno.h)
 */
int dal_keystore_decrypt(const uint8_t *client_ticket, int slot_id,
			enum keystore_algo_spec algo_spec,
			const uint8_t *iv, unsigned int iv_size,
			const uint8_t *input, unsigned int input_size,
			uint8_t *output);
#endif /* _DAL_KEYSTORE_API_H_ */
