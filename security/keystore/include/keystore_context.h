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
#include "keystore_constants.h"

#define KEYSTORE_MAX_APPKEY_SIZE       32
#define KEYSTORE_CLIENT_KEY_SIZE       32
#define KEYSTORE_MAX_CLIENT_ID_SIZE    32

/**
 * The keystore slot structure.
 */
struct keystore_slot {
	struct list_head list;       /* kernel's list structure */

	uint8_t app_key[KEYSTORE_MAX_APPKEY_SIZE]; /* AppKey */
	unsigned int app_key_size;   /* AppKey size in bytes */
	unsigned int slot_id;                 /* SlotID */
};

/**
 * The keystore context structure.
 */
struct keystore_ctx {
	struct list_head list;                               /* kernel list */

	uint8_t client_ticket[KEYSTORE_CLIENT_TICKET_SIZE];  /* ClientTicket */
	uint8_t client_key[KEYSTORE_CLIENT_KEY_SIZE];        /* ClientKey */
	char client_id[KEYSTORE_MAX_CLIENT_ID_SIZE];         /* Client ID */

	struct list_head slots;                              /* list of slots */
};


/**
 * Free all contexts.
 */
void keystore_free_contexts(void);

/**
 * Allocate a context structure.
 *
 * @return Context structure pointer or NULL if out of memory.
 */
struct keystore_ctx *keystore_allocate_context(void);

/**
 * Free the context, searching by context structure.
 *
 * @param ctx Pointer to the context structure.
 *
 * @return 0 if OK or negative error code (see errno).
 */
int keystore_free_context_ctx(struct keystore_ctx *ctx);

/**
 * Free the context, searching by ClientTicket.
 *
 * @param client_ticket The client ticket (KEYSTORE_CLIENT_TICKET_SIZE bytes).
 *
 * @return 0 if OK or negative error code (see errno).
 */
int keystore_free_context_ticket(const void *client_ticket);

/**
 * Find the context, searching by ClientTicket.
 *
 * @param client_ticket The client ticket (KEYSTORE_CLIENT_TICKET_SIZE bytes).
 *
 * @return Context structure pointer or NULL if not found.
 */
struct keystore_ctx *keystore_find_context_ticket(const void *client_ticket);

/**
 * Allocate a slot structure in the context.
 *
 * @param ctx Pointer to the context structure.
 *
 * @return Slot structure pointer or NULL if out of memory.
 */
struct keystore_slot *keystore_allocate_slot(struct keystore_ctx *ctx);

/**
 * Free the slot, searching by slot ID.
 *
 * @param ctx Pointer to the context structure.
 * @param slot_id The slot ID.
 *
 * @return 0 if OK or negative error code (see errno).
 */
int keystore_free_slot_id(struct keystore_ctx *ctx, int slot_id);

/**
 * Find the slot, searching by slot ID.
 *
 * @param ctx Pointer to the context structure.
 * @param slot_id The slot ID.
 *
 * @return Slot structure pointer or NULL if not found.
 */
struct keystore_slot *keystore_find_slot_id(struct keystore_ctx *ctx,
					    int slot_id);

/**
 * Dump the context contents.
 *
 * @param ctx Pointer to the context structure.
 */
void keystore_dump_ctx(const struct keystore_ctx *ctx);

#endif /* _KEYSTORE_CONTEXT_H_ */
