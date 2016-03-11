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

#ifndef _KEYSTORE_CONTEXT_SAFE_H_
#define _KEYSTORE_CONTEXT_SAFE_H_

#include <linux/types.h>
#include <linux/list.h>

#include "keystore_context.h"

/**
 * DOC:  Introduction
 *
 * The keystore_context_safe functions provide thread safe access
 * to the keystore client context.
 *
 * These functions are used to register clients with a given
 * client key, and store associated unwrapped application keys.
 *
 * Clients should call into this interface, rather than the
 * lower-level keystore_context functions.
 */

/**
 * struct keystore_backup_data - The keystore backup structure
 *
 * @client_key: The derived client key.
 * @client_id: The derived client ID
 *
 * The backup struct can be used to transfer wrapped keys between
 * different client application and between keystores with different
 * SEED values.
 *
 * The backup struct and corresponding client key is only exposed
 * outside of keystore in an encrypted form.
 */
struct keystore_backup_data {
	uint8_t client_key[KEYSTORE_CLIENT_KEY_SIZE];   /* ClientKey */
	uint8_t client_id[KEYSTORE_MAX_CLIENT_ID_SIZE]; /* Client ID */
};


/**
 * ctx_add_client() - Register a new client
 *
 * @client_ticket: The client ticket to register against. Expected to be an
 *          array of size %KEYSTORE_CLIENT_TICKET_SIZE.
 * @client_key: The client key to be used for wrapping application
 *              keys. Must be an array of size %KEYSTORE_CLIENT_KEY_SIZE.
 * @client_id: A unique ID for the client, must be an array of
 *             %KEYSTORE_MAX_CLIENT_ID_SIZE bytes.
 *
 * Adds a new client to the keystore context struct in a thread-safe
 * way.
 *
 * Returns: 0 on success or negative errno.
 *
 */
int ctx_add_client(const uint8_t *client_ticket, const uint8_t *client_key,
		   const uint8_t *client_id);

/**
 * ctx_remove_client() - Remove a client
 *
 * @client_ticket: The client ticket to register against. Expected to be an
 *          array of size %KEYSTORE_CLIENT_TICKET_SIZE.
 *
 * Removes a client from the keystore context and deletes the associated
 * client ID and client key.
 *
 * Returns: 0 on success or negative errno.
 *
 */
int ctx_remove_client(const uint8_t *client_ticket);


/**
 * ctx_add_app_key() - Add an unwrapped application key to context
 *
 * @client_ticket: The client ticket to register against. Expected to be an
 *          array of size %KEYSTORE_CLIENT_TICKET_SIZE.
 * @key_spec: The Key Spec for this key.
 * @app_key: A pointer to the application key to be stored.
 * @app_key_size: Size of @app_key in bytes.
 * @slot_id: Output the number of slot where the key is stored.
 *
 * Adds the given application key to a slot associated to the @client_ticket.
 *
 * Returns: 0 on success or negative errno.
 *
 */
int ctx_add_app_key(const uint8_t *client_ticket,
		    enum keystore_key_spec key_spec,
		    const uint8_t *app_key,
		    unsigned int app_key_size,
		    unsigned int *slot_id);


/**
 * ctx_get_client_key() - Get the client key for the given ticket.
 *
 * @client_ticket: The client ticket to register against. Expected to be an
 *          array of size %KEYSTORE_CLIENT_TICKET_SIZE.
 * @client_key: The client key to be used for wrapping application
 *              keys. Must be an array of size %KEYSTORE_CLIENT_KEY_SIZE.
 * @client_id: A unique ID for the client, must be an array of
 *             %KEYSTORE_MAX_CLIENT_ID_SIZE bytes.
 *
 * Copies the client key and client ID associated with the @client_ticket
 * into the buffers pointed to by @client_key and @client_id respectively.
 *
 * The caller is responsible for allocating arrays with the correct length
 * and ensuring the data are zeroed after use.
 *
 * Returns: 0 on success or negative errno.
 *
 */
int ctx_get_client_key(const uint8_t *client_ticket,
		       uint8_t *client_key, uint8_t *client_id);

/**
 * ctx_get_app_key() - Get the application key for the given ticket.
 *
 * @client_ticket: The client ticket to register against. Expected to be an
 *          array of size %KEYSTORE_CLIENT_TICKET_SIZE.
 * @key_spec: The Key Spec for this key.
 * @slot_id: The number of the slot where the key is stored.
 * @app_key: A pointer to the application key to be stored.
 * @app_key_size: Size of @app_key in bytes.
 *
 * Copies the app key with the @client_ticket in slot @slot_id
 * into the buffer pointed to by @app_key.
 *
 * The size of the buffer should be passed an input in @app_key_size.
 * If the buffer is large enough, the function will succeed and change
 * the value of @app_key_size to the size of the application key.
 * To find ou the length of the application key, the function should be
 * called setting @app_key to NULL. Alternatively, the caller can
 * statically declare an array of %KEYSTORE_MAX_APPKEY_SIZE bytes.
 *
 * The caller is responsible for allocating arrays with the correct length
 * and ensuring the data are zeroed after use.
 *
 * Returns: 0 on success or negative errno.
 *
 */
int ctx_get_app_key(const uint8_t *client_ticket, unsigned int slot_id,
		    enum keystore_key_spec *key_spec,
		    uint8_t *app_key, unsigned int *app_key_size);

/**
 * ctx_remove_app_key() - Remove an app key and free the slot.
 *
 * @client_ticket: The client ticket to register against. Expected to be an
 *          array of size %KEYSTORE_CLIENT_TICKET_SIZE.
 * @slot_id: The number of the slot where the key is stored.
 *
 * Returns: 0 on success or negative errno.
 */
int ctx_remove_app_key(const uint8_t *client_ticket, unsigned int slot_id);

/**
 * ctx_take_backup() - Copy the client key and ID into backup
 *
 * @client_ticket: The client ticket to register against. Expected to be an
 *          array of size %KEYSTORE_CLIENT_TICKET_SIZE.
 * @backup: The backup structure output.
 *
 * Returns: 0 on success or negative errno.
 */
int ctx_take_backup(const uint8_t *client_ticket,
		    struct keystore_backup_data *backup);

/**
 * ctx_free() - Remove all context information
 *
 * Removes all slots and client tickets in the keystore
 * context, and frees all memory associated to it.
 *
 * Returns: 0 on success or negative errno.
 */
int ctx_free(void);

#endif /* _KEYSTORE_CONTEXT_H_ */
