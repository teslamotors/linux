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

#ifndef _KEYSTORE_CONTEXT_H_
#define _KEYSTORE_CONTEXT_H_

#include <linux/types.h>
#include <linux/list.h>

#include <security/keystore_api_common.h>

#define KEYSTORE_MAX_APPKEY_SIZE      208
#define KEYSTORE_CLIENT_KEY_SIZE       32
#define KEYSTORE_MAX_CLIENT_ID_SIZE    32
#define KEYSTORE_CLIENTS_MAX          256
#define KEYSTORE_SLOTS_MAX            256

/**
 * DOC:  Introduction
 *
 * The keystore_context functions provide access
 * to the keystore client context. (Not thread safe!)
 *
 * These functions are used to register clients with a given
 * client key, and store associated unwrapped application keys.
 *
 * Clients should not call into this interface, they should
 * rather user the higher-level thread-safe keystore_context_safe functions.
 *
 * A client can register a number of different simultaneous sessions
 * with keystore, each with a separate client ticket. The client ticket
 * is simply a unique random array used as a client handle. Each entry in
 * the context stores this client ticket, together with a client key
 * and ID. A client (with the same client ID and key) can register
 * multiple times with diferent client tickets.
 *
 * For each client ticket, the context contains a number of slots
 * which can be used to load and cache unwrapped application keys.
 * These keys are used for encryption and decryption of application data.
 *
 */

/**
 * struct keystore_slot - The keystore slot structure.
 *
 * @list: Kernel list head for linked list.
 * @app_key: The application key.
 * @app_key_size: Size of the application key.
 * @slot_id: Number identifying the slot.
 */
struct keystore_slot {
	struct list_head list;       /* kernel's list structure */

	enum keystore_key_spec key_spec;
	uint8_t app_key[KEYSTORE_MAX_APPKEY_SIZE]; /* AppKey */
	unsigned int app_key_size;   /* AppKey size in bytes */
	unsigned int slot_id;                 /* SlotID */
};

/**
 * struct keystore_ctx - The keystore context structure.
 *
 * @list: Kernel list head for linked list.
 * @client_ticket: The client ticket to register against.
 * @client_key: The derived client key.
 * @client_id: The derived client ID
 * @slots: List of slots assoicated to this ticket.
 */
struct keystore_ctx {
	struct list_head list;                               /* kernel list */

	uint8_t client_ticket[KEYSTORE_CLIENT_TICKET_SIZE];  /* ClientTicket */
	uint8_t client_key[KEYSTORE_CLIENT_KEY_SIZE];        /* ClientKey */
	char client_id[KEYSTORE_MAX_CLIENT_ID_SIZE];         /* Client ID */

	struct list_head slots;                              /* list of slots */
};


/**
 * keystore_free_contexts() - Free all contexts.
 */
void keystore_free_contexts(void);

/**
 * keystore_allocate_context() - Allocate a context structure.
 *
 * Returns: Context structure pointer or NULL if out of memory.
 */
struct keystore_ctx *keystore_allocate_context(void);

/**
 * Free the context, searching by context structure.
 *
 * @ctx: Pointer to the context structure.
 *
 * Returns: 0 if OK or negative error code (see errno).
 */
int keystore_free_context_ctx(struct keystore_ctx *ctx);

/**
 * Free the context, searching by ClientTicket.
 *
 * @client_ticket: The client ticket (KEYSTORE_CLIENT_TICKET_SIZE bytes).
 *
 * Returns: 0 if OK or negative error code (see errno).
 */
int keystore_free_context_ticket(const void *client_ticket);

/**
 * Find the context, searching by ClientTicket.
 *
 * @client_ticket: The client ticket (KEYSTORE_CLIENT_TICKET_SIZE bytes).
 *
 * Returns: Context structure pointer or NULL if not found.
 */
struct keystore_ctx *keystore_find_context_ticket(const void *client_ticket);

/**
 * Allocate a slot structure in the context.
 *
 * @ctx: Pointer to the context structure.
 *
 * Returns: Slot structure pointer or NULL if out of memory.
 */
struct keystore_slot *keystore_allocate_slot(struct keystore_ctx *ctx);

/**
 * Free the slot, searching by slot ID.
 *
 * @ctx: Pointer to the context structure.
 * @slot_id: The slot ID.
 *
 * Returns: 0 if OK or negative error code (see errno).
 */
int keystore_free_slot_id(struct keystore_ctx *ctx, int slot_id);

/**
 * Find the slot, searching by slot ID.
 *
 * @ctx: Pointer to the context structure.
 * @slot_id: The slot ID.
 *
 * Returns: Slot structure pointer or NULL if not found.
 */
struct keystore_slot *keystore_find_slot_id(struct keystore_ctx *ctx,
					    int slot_id);

/**
 * keystore_dump_ctx() - Dump the context contents.
 *
 * @ctx: Pointer to the context structure.
 */
void keystore_dump_ctx(const struct keystore_ctx *ctx);

#endif /* _KEYSTORE_CONTEXT_H_ */
