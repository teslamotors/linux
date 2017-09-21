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
 * This file is a modified copy of security/keystore/include/keystore_context.h
 */

#ifndef _DAL_CONTEXT_H_
#define _DAL_CONTEXT_H_

#include <linux/types.h>
#include <linux/list.h>

#include <security/keystore_api_common.h>

#define DAL_KEYSTORE_MAX_WRAPKEY_SIZE      49
#define DAL_KEYSTORE_CLIENTS_MAX          256
#define DAL_KEYSTORE_SLOTS_MAX            256

/**
 * DOC:  Introduction
 *
 * The dal_keystore_context functions provide access
 * to the keystore client context. (Not thread safe!)
 *
 * These functions are used to register clients with a given
 * client key, and store associated unwrapped application keys.
 *
 * Clients should not call into this interface, they should
 * rather user the higher-level thread-safe dal_context_safe functions.
 *
 * A client can register a number of different simultaneous sessions
 * with keystore, each with a separate client ticket. The client ticket
 * is simply a unique random array used as a client handle. Each entry in
 * the context stores this client ticket, together with a client key
 * and ID. A client (with the same client ID and key) can register
 * multiple times with different client tickets.
 *
 * For each client ticket, the context contains a number of slots
 * which can be used to load and cache unwrapped application keys.
 * These keys are used for encryption and decryption of application data.
 *
 */

/**
 * struct dal_keystore_slot - The keystore slot structure.
 *
 * @list: Kernel list head for linked list.
 * @wrapped_key: The application key.
 * @wrapped_key_size: Size of the application key.
 * @slot_id: Number identifying the slot.
 */
struct dal_keystore_slot {
	struct list_head list;       /* kernel's list structure */

	enum keystore_key_spec key_spec;
	uint8_t wrapped_key[DAL_KEYSTORE_MAX_WRAPKEY_SIZE]; /* Wrapped key*/
	unsigned int wrapped_key_size;   /* AppKey size in bytes */
	unsigned int slot_id;                 /* SlotID */
};

/**
 * struct dal_keystore_ctx - The keystore context structure.
 *
 * @list: Kernel list head for linked list.
 * @client_ticket: The client ticket to register against.
 * @client_key: The derived client key.
 * @client_id: The derived client ID
 * @slots: List of slots associated to this ticket.
 */
struct dal_keystore_ctx {
	struct list_head list;                               /* kernel list */

	uint8_t client_ticket[KEYSTORE_CLIENT_TICKET_SIZE];  /* ClientTicket */
	uint8_t client_id[KEYSTORE_MAX_CLIENT_ID_SIZE];      /* Client ID */
	enum keystore_seed_type seed_type;                   /* Seed Type */

	struct list_head slots;                              /* list of slots */
};


/**
 * dal_keystore_free_contexts() - Free all contexts.
 */
void dal_keystore_free_contexts(void);

/**
 * dal_keystore_allocate_context() - Allocate a context structure.
 *
 * Returns: Context structure pointer or NULL if out of memory.
 */
struct dal_keystore_ctx *dal_keystore_allocate_context(void);

/**
 * Free the context, searching by context structure.
 *
 * @ctx: Pointer to the context structure.
 *
 * Returns: 0 if OK or negative error code (see errno).
 */
int dal_keystore_free_context_ctx(struct dal_keystore_ctx *ctx);

/**
 * Free the context, searching by ClientTicket.
 *
 * @client_ticket: The client ticket (KEYSTORE_CLIENT_TICKET_SIZE bytes).
 *
 * Returns: 0 if OK or negative error code (see errno).
 */
int dal_keystore_free_context_ticket(const void *client_ticket);

/**
 * Find the context, searching by ClientTicket.
 *
 * @client_ticket: The client ticket (KEYSTORE_CLIENT_TICKET_SIZE bytes).
 *
 * Returns: Context structure pointer or NULL if not found.
 */
struct dal_keystore_ctx *dal_keystore_find_context_ticket(const void *client_ticket);

/**
 * Allocate a slot structure in the context.
 *
 * @ctx: Pointer to the context structure.
 *
 * Returns: Slot structure pointer or NULL if out of memory.
 */
struct dal_keystore_slot *dal_keystore_allocate_slot(struct dal_keystore_ctx *ctx);

/**
 * Free the slot, searching by slot ID.
 *
 * @ctx: Pointer to the context structure.
 * @slot_id: The slot ID.
 *
 * Returns: 0 if OK or negative error code (see errno).
 */
int dal_keystore_free_slot_id(struct dal_keystore_ctx *ctx, int slot_id);

/**
 * Find the slot, searching by slot ID.
 *
 * @ctx: Pointer to the context structure.
 * @slot_id: The slot ID.
 *
 * Returns: Slot structure pointer or NULL if not found.
 */
struct dal_keystore_slot *dal_keystore_find_slot_id(struct dal_keystore_ctx *ctx,
					    int slot_id);

/**
 * dal_keystore_dump_ctx() - Dump the context contents.
 *
 * @ctx: Pointer to the context structure.
 */
void dal_keystore_dump_ctx(const struct dal_keystore_ctx *ctx);

#endif /* _DAL_CONTEXT_H_ */
