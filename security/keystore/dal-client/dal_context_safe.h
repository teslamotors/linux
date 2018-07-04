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

/*
 * This file is a modified copy of security/keystore/include/keystore_context_safe.h
 */

#ifndef _DAL_CONTEXT_SAFE_H_
#define _DAL_CONTEXT_SAFE_H_

#include <linux/types.h>
#include <linux/list.h>

#include "dal_context.h"

/**
 * DOC:  Introduction
 *
 * The dal_context_safe functions provide thread safe access
 * to the keystore client context.
 *
 * These functions are used to register clients with a
 * seed type, and store associated wrapped application keys.
 *
 * Clients should call into this interface, rather than the
 * lower-level dal_context functions.
 */

/**
 * dal_ctx_add_client() - Register a new client
 *
 * @client_ticket: The client ticket to register against. Expected to be an
 *          array of size %KEYSTORE_CLIENT_TICKET_SIZE.
 * @seed_type: The seed type used by client to register with DAL KS.
 *
 * @client_id: A unique ID for the client, must be an array of
 *             %KEYSTORE_MAX_CLIENT_ID_SIZE bytes.
 *
 * Adds a new client to the keystore context struct in a thread-safe
 * way.
 *
 * Returns: 0 on success or negative errno.
 *
 */
int dal_ctx_add_client(const uint8_t *client_ticket, enum keystore_seed_type seed_type,
		   const uint8_t *client_id);

/**
 * dal_ctx_remove_client() - Remove a client
 *
 * @client_ticket: The client ticket to register against. Expected to be an
 *          array of size %KEYSTORE_CLIENT_TICKET_SIZE.
 *
 * Removes a client from the DAL keystore context and deletes the associated
 * client ID and wrapped key.
 *
 * Returns: 0 on success or negative errno.
 *
 */
int dal_ctx_remove_client(const uint8_t *client_ticket);

/**
 * dal_ctx_add_wrapped_key() - Add a wrapped application key to context
 *
 * @client_ticket: The client ticket to register against. Expected to be an
 *          array of size %KEYSTORE_CLIENT_TICKET_SIZE.
 * @key_spec: The Key Spec for this key.
 * @wrapped_key: A pointer to the application key to be stored.
 * @wrapped_key_size: Size of @wrapped_key in bytes.
 * @slot_id: Output the number of slot where the key is stored.
 *
 * Adds the given wrapped key to a slot associated to the @client_ticket.
 *
 * Returns: 0 on success or negative errno.
 *
 */
int dal_ctx_add_wrapped_key(const uint8_t *client_ticket,
		    enum keystore_key_spec key_spec,
		    const uint8_t *wrapped_key,
		    unsigned int wrapped_key_size,
		    unsigned int *slot_id);

/**
 * dal_ctx_get_client_seed_type() - Get the client seed type for the given ticket.
 *
 * @client_ticket: The client ticket to register against. Expected to be an
 *          array of size %KEYSTORE_CLIENT_TICKET_SIZE.
 * @seed_type: The client seed type used for registering with DAL Keystore.
 *              Must be a pointer to %enum keystore_seed_type.
 * @client_id: A unique ID for the client, must be an array of
 *             %KEYSTORE_MAX_CLIENT_ID_SIZE bytes.
 *
 * Copies the seed type and client ID associated with the @client_ticket
 * into the buffers pointed to by @seed_type and @client_id respectively.
 *
 * The caller is responsible for allocating arrays with the correct length
 * and ensuring the data are zeroed after use.
 *
 * Returns: 0 on success or negative errno.
 *
 */
int dal_ctx_get_client_seed_type(const uint8_t *client_ticket,
		       enum keystore_seed_type *seed_type, const uint8_t *client_id);

/**
 * dal_ctx_get_wrapped_key() - Get a wrapped application key for the given ticket.
 *
 * @client_ticket: The client ticket to register against. Expected to be an
 *          array of size %KEYSTORE_CLIENT_TICKET_SIZE.
 * @key_spec: The Key Spec for this key.
 * @slot_id: The number of the slot where the key is stored.
 * @wrapped_key: A pointer to the wrapped application key to be stored.
 * @wrapped_key_size: Size of @wrapped_key in bytes.
 *
 * Copies the wrapped key with the @client_ticket in slot @slot_id
 * into the buffer pointed to by @wrapped_key.
 *
 * The size of the buffer should be passed an input in @wrapped_key_size.
 * If the buffer is large enough, the function will succeed and change
 * the value of @wrapped_key_size to the size of the wrapped key.
 * To find out the length of the wrapped key, the function should be
 * called setting @wrapped_key to NULL. Alternatively, the caller can
 * statically declare an array of %KEYSTORE_MAX_APPKEY_SIZE bytes.
 *
 * The caller is responsible for allocating arrays with the correct length
 * and ensuring the data are zeroed after use.
 *
 * Returns: 0 on success or negative errno.
 *
 */
int dal_ctx_get_wrapped_key(const uint8_t *client_ticket, unsigned int slot_id,
		    enum keystore_key_spec *key_spec,
		    uint8_t *wrapped_key, unsigned int *wrapped_key_size);

/**
 * dal_ctx_remove_wrapped_key() - Remove a wrapped key and free the slot.
 *
 * @client_ticket: The client ticket to register against. Expected to be an
 *          array of size %KEYSTORE_CLIENT_TICKET_SIZE.
 * @slot_id: The number of the slot where the key is stored.
 *
 * Returns: 0 on success or negative errno.
 */
int dal_ctx_remove_wrapped_key(const uint8_t *client_ticket, unsigned int slot_id);

/**
 * dal_ctx_free() - Remove all context information
 *
 * Removes all slots and client tickets in the keystore
 * context, and frees all memory associated to it.
 *
 * Returns: 0 on success or negative errno.
 */
int dal_ctx_free(void);

#endif /* _DAL_CONTEXT_SAFE_H_ */
