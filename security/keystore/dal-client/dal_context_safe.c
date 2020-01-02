/*
 * This file is a modified copy of security/keystore/keystore_context_safe.c
 */

#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/dcache.h>

#include "dal_context.h"
#include "dal_context_safe.h"
#include "keystore_debug.h"

/* The mutex for contexts access synchronization. */
DEFINE_MUTEX(dal_ctx_mutex);

int dal_ctx_add_client(const uint8_t *client_ticket, enum keystore_seed_type seed_type,
		   const uint8_t *client_id)
{
	struct dal_keystore_ctx *ctx;
	int res = 0;

	if (!client_ticket || !client_id)
		return -EFAULT;

	mutex_lock(&dal_ctx_mutex);

	/* allocate new context */
	ctx = dal_keystore_allocate_context();
	if (!ctx) {
		res = -ENOMEM;
		goto unlock_mutex;
	}

	/* save ClientTicket and ClientKey */
	memcpy(ctx->client_ticket,
	       client_ticket, KEYSTORE_CLIENT_TICKET_SIZE);
	ctx->seed_type = seed_type;
	memcpy(ctx->client_id,
	       client_id, KEYSTORE_MAX_CLIENT_ID_SIZE);

unlock_mutex:
	mutex_unlock(&dal_ctx_mutex);
	return res;
}

int dal_ctx_remove_client(const uint8_t *client_ticket)
{
	int res = 0;

	if (!client_ticket)
		return -EFAULT;

	mutex_lock(&dal_ctx_mutex);
	res = dal_keystore_free_context_ticket(client_ticket);
	mutex_unlock(&dal_ctx_mutex);

	return res;
}

int dal_ctx_add_wrapped_key(const uint8_t *client_ticket,
		    enum keystore_key_spec key_spec,
		    const uint8_t *wrapped_key, unsigned int wrapped_key_size,
		    unsigned int *slot_id)
{
	int res = 0;
	struct dal_keystore_ctx *ctx;
	struct dal_keystore_slot *slot = NULL;

	if (!client_ticket || !wrapped_key || !slot_id)
		return -EFAULT;

	mutex_lock(&dal_ctx_mutex);
	ctx = dal_keystore_find_context_ticket(client_ticket);
	if (!ctx) {
		res = -EINVAL;
		goto unlock_mutex;
	}

	/* allocate new slot */
	slot = dal_keystore_allocate_slot(ctx);
	if (!slot) {
		res = -ENOMEM;
		goto unlock_mutex;
	}

	slot->key_spec = key_spec;
	slot->wrapped_key_size = wrapped_key_size;
	memcpy(slot->wrapped_key, wrapped_key, wrapped_key_size);

	*slot_id = slot->slot_id;

unlock_mutex:
	mutex_unlock(&dal_ctx_mutex);
	return res;
}

int dal_ctx_get_client_seed_type(const uint8_t *client_ticket,
		enum keystore_seed_type *seed_type, const uint8_t *client_id)
{
	struct dal_keystore_ctx *ctx = NULL;
	int res = 0;

	if (!client_ticket || !seed_type || !client_id)
		return -EFAULT;

	/* Cache the client key */
	mutex_lock(&dal_ctx_mutex);
	ctx = dal_keystore_find_context_ticket(client_ticket);
	if (!ctx) {
		res = -EINVAL;
		goto unlock_mutex;
	}

	if (memcmp(client_id, ctx->client_id, KEYSTORE_MAX_CLIENT_ID_SIZE))
	{
		res = -EINVAL;
		goto unlock_mutex;
	}

	/* cache context data (the seed type) before releasing the lock */
	*seed_type = ctx->seed_type;

unlock_mutex:
	mutex_unlock(&dal_ctx_mutex);
	return res;
}


int dal_ctx_get_wrapped_key(const uint8_t *client_ticket, unsigned int slot_id,
		    enum keystore_key_spec *key_spec,
		    uint8_t *wrapped_key, unsigned int *wrapped_key_size)
{
	struct dal_keystore_ctx *ctx = NULL;
	struct dal_keystore_slot *slot = NULL;
	int res = 0;

	if (!client_ticket || !key_spec || !wrapped_key_size)
		return -EFAULT;

	if (slot_id >= DAL_KEYSTORE_SLOTS_MAX)
		return -EINVAL;

	/* Get the context */
	mutex_lock(&dal_ctx_mutex);
	ctx = dal_keystore_find_context_ticket(client_ticket);
	if (!ctx) {
		ks_err(KBUILD_MODNAME ": %s: Cannot find context\n", __func__);
		res = -EINVAL;
		goto unlock_mutex;
	}

	/* Get the slot */
	slot = dal_keystore_find_slot_id(ctx, slot_id);

	if (!slot) {
		ks_err(KBUILD_MODNAME ": %s: Cannot find slot\n", __func__);
		res = -EBADSLT;
		goto unlock_mutex;
	}

	/* Copy the key spec to start with */
	*key_spec = slot->key_spec;

	/* If wrapped_key is null, assume the caller wants to know the size */
	if (!wrapped_key) {
		*wrapped_key_size = slot->wrapped_key_size;
		goto unlock_mutex;
	}

	/* Check buffer size */
	if (*wrapped_key_size < slot->wrapped_key_size) {
		ks_err(KBUILD_MODNAME ": %s wrapped_key_size too small\n",
		       __func__);
		res = -EINVAL;
		goto unlock_mutex;
	}

	memcpy(wrapped_key, slot->wrapped_key, slot->wrapped_key_size);
	*wrapped_key_size = slot->wrapped_key_size;

unlock_mutex:
	mutex_unlock(&dal_ctx_mutex);
	return res;
}

int dal_ctx_remove_wrapped_key(const uint8_t *client_ticket, unsigned int slot_id)
{
	struct dal_keystore_ctx *ctx = NULL;
	int res = 0;

	if (!client_ticket)
		return -EFAULT;

	if (slot_id >= DAL_KEYSTORE_SLOTS_MAX)
		return -EINVAL;

	mutex_lock(&dal_ctx_mutex);

	ctx = dal_keystore_find_context_ticket(client_ticket);
	if (!ctx) {
		ks_err(KBUILD_MODNAME ": %s: Cannot find context\n", __func__);
		res = -EINVAL;
		goto unlock_mutex;
	}

	res = dal_keystore_free_slot_id(ctx, slot_id);

unlock_mutex:
	mutex_unlock(&dal_ctx_mutex);
	return res;
}

int dal_ctx_free(void)
{
	/* free contexts */
	mutex_lock(&dal_ctx_mutex);
	dal_keystore_free_contexts();
	mutex_unlock(&dal_ctx_mutex);

	return 0;
}
