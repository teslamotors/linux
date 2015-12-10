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
#include "keystore_constants.h"

/**
 * The keystore backup structure.
 */
struct keystore_backup_data {
	uint8_t client_key[KEYSTORE_CLIENT_KEY_SIZE];   /* ClientKey */
	uint8_t client_id[KEYSTORE_MAX_CLIENT_ID_SIZE]; /* Client ID */
};

int ctx_add_client(const uint8_t *ticket, const uint8_t *client_key,
		   const uint8_t *client_id);

int ctx_remove_client(const uint8_t *client_ticket);

int ctx_add_app_key(const void *client_ticket,
		    const uint8_t *app_key,
		    unsigned int app_key_size,
		    unsigned int *slot_id);

int ctx_get_client_key(const void *client_ticket,
		       uint8_t *client_key, uint8_t *client_id);

int ctx_get_app_key(const void *client_ticket, unsigned int slot_id,
		    uint8_t *app_key, unsigned int *app_key_size);

int ctx_remove_app_key(const void *client_ticket, unsigned int slot_id);

int ctx_take_backup(const void *client_ticket,
		    struct keystore_backup_data *backup);

int ctx_free(void);

#endif /* _KEYSTORE_CONTEXT_H_ */
