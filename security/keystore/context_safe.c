#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/dcache.h>

#include "keystore_context.h"
#include "keystore_context_safe.h"
#include "keystore_debug.h"

/* The mutex for contexts access synchronization. */
DEFINE_MUTEX(contexts_mutex);

int ctx_add_client(const uint8_t *client_ticket, const uint8_t *client_key,
		   const uint8_t *client_id, enum keystore_seed_type seed_type)

{
	struct keystore_ctx *ctx;
	int res = 0;

	if (!client_ticket || !client_key || !client_id)
		return -EFAULT;

	mutex_lock(&contexts_mutex);

	/* allocate new context */
	ctx = keystore_allocate_context();
	if (!ctx) {
		res = -ENOMEM;
		goto unlock_mutex;
	}

	/* save ClientTicket and ClientKey */
	memcpy(ctx->client_ticket,
	       client_ticket, KEYSTORE_CLIENT_TICKET_SIZE);
	memcpy(ctx->client_key,
	       client_key, KEYSTORE_CLIENT_KEY_SIZE);
	memcpy(ctx->client_id,
	       client_id, KEYSTORE_MAX_CLIENT_ID_SIZE);
	ctx->seed_type = seed_type;

unlock_mutex:
	mutex_unlock(&contexts_mutex);
	return res;
}

int ctx_remove_client(const uint8_t *client_ticket)
{
	int res = 0;

	if (!client_ticket)
		return -EFAULT;

	mutex_lock(&contexts_mutex);
	res = keystore_free_context_ticket(client_ticket);
	mutex_unlock(&contexts_mutex);

	return res;
}

int ctx_add_app_key(const uint8_t *client_ticket,
		    enum keystore_key_spec key_spec,
		    const uint8_t *app_key, unsigned int app_key_size,
		    unsigned int *slot_id)
{
	int res = 0;
	struct keystore_ctx *ctx;
	struct keystore_slot *slot = NULL;

	if (!client_ticket || !app_key || !slot_id)
		return -EFAULT;

	mutex_lock(&contexts_mutex);
	ctx = keystore_find_context_ticket(client_ticket);
	if (!ctx) {
		res = -EINVAL;
		goto unlock_mutex;
	}

	/* allocate new slot */
	slot = keystore_allocate_slot(ctx);
	if (!slot) {
		res = -ENOMEM;
		goto unlock_mutex;
	}

	slot->key_spec = key_spec;
	slot->app_key_size = app_key_size;
	memcpy(slot->app_key, app_key, app_key_size);

	*slot_id = slot->slot_id;

unlock_mutex:
	mutex_unlock(&contexts_mutex);
	return res;
}

int ctx_get_client_key(const uint8_t *client_ticket, uint8_t *client_key,
			uint8_t *client_id, enum keystore_seed_type *seed_type)
{
	struct keystore_ctx *ctx = NULL;
	int res = 0;

	if (!client_ticket || !client_key)
		return -EFAULT;

	/* Cache the client key */
	mutex_lock(&contexts_mutex);
	ctx = keystore_find_context_ticket(client_ticket);
	if (!ctx) {
		res = -EINVAL;
		goto unlock_mutex;
	}

	/* cache context data (the ClientKey) before releasing the lock */
	memcpy(client_key, ctx->client_key, KEYSTORE_CLIENT_KEY_SIZE);

	if (client_id)
		memcpy(client_id, ctx->client_id,
		       KEYSTORE_MAX_CLIENT_ID_SIZE);
	if (seed_type)
		*seed_type = ctx->seed_type;

unlock_mutex:
	mutex_unlock(&contexts_mutex);
	return res;
}


int ctx_get_app_key(const uint8_t *client_ticket, unsigned int slot_id,
		    enum keystore_key_spec *key_spec,
		    uint8_t *app_key, unsigned int *app_key_size)
{
	struct keystore_ctx *ctx = NULL;
	struct keystore_slot *slot = NULL;
	int res = 0;

	if (!client_ticket || !key_spec || !app_key_size)
		return -EFAULT;

	if (slot_id >= KEYSTORE_SLOTS_MAX)
		return -EINVAL;

	/* Get the context */
	mutex_lock(&contexts_mutex);
	ctx = keystore_find_context_ticket(client_ticket);
	if (!ctx) {
		ks_err(KBUILD_MODNAME ": %s: Cannot find context\n", __func__);
		res = -EINVAL;
		goto unlock_mutex;
	}

	/* Get the slot */
	slot = keystore_find_slot_id(ctx, slot_id);

	if (!slot) {
		ks_err(KBUILD_MODNAME ": %s: Cannot find slot\n", __func__);
		res = -EBADSLT;
		goto unlock_mutex;
	}

	/* Copy the key spec to start with */
	*key_spec = slot->key_spec;

	/* If app_key is null, assume the caller wants to know the size */
	if (!app_key) {
		*app_key_size = slot->app_key_size;
		goto unlock_mutex;
	}

	/* Check buffer size */
	if (*app_key_size < slot->app_key_size) {
		ks_err(KBUILD_MODNAME ": %s app_key_size too small\n",
		       __func__);
		res = -EINVAL;
		goto unlock_mutex;
	}

	memcpy(app_key, slot->app_key, slot->app_key_size);
	*app_key_size = slot->app_key_size;

unlock_mutex:
	mutex_unlock(&contexts_mutex);
	return res;
}

int ctx_remove_app_key(const uint8_t *client_ticket, unsigned int slot_id)
{
	struct keystore_ctx *ctx = NULL;
	int res = 0;

	if (!client_ticket)
		return -EFAULT;

	if (slot_id >= KEYSTORE_SLOTS_MAX)
		return -EINVAL;

	mutex_lock(&contexts_mutex);

	ctx = keystore_find_context_ticket(client_ticket);
	if (!ctx) {
		ks_err(KBUILD_MODNAME ": %s: Cannot find context\n", __func__);
		res = -EINVAL;
		goto unlock_mutex;
	}

	res = keystore_free_slot_id(ctx, slot_id);

unlock_mutex:
	mutex_unlock(&contexts_mutex);
	return res;
}

int ctx_take_backup(const uint8_t *client_ticket,
		    struct keystore_backup_data *backup)
{
	struct keystore_ctx *ctx = NULL;
	int res = 0;

	if (!client_ticket || !backup)
		return -EFAULT;

	/* Get the context */
	mutex_lock(&contexts_mutex);

	ctx = keystore_find_context_ticket(client_ticket);
	if (!ctx) {
		ks_err(KBUILD_MODNAME ": %s: Cannot find context\n", __func__);
		res = -EINVAL;
		goto unlock_mutex;
	}

	/* Copy the context into the backup struct */
	memset(backup, 0, sizeof(struct keystore_backup_data));
	memcpy(backup->client_id, ctx->client_id, sizeof(backup->client_id));
	memcpy(backup->client_key, ctx->client_key, sizeof(backup->client_key));

unlock_mutex:
	mutex_unlock(&contexts_mutex);
	return res;
}


int ctx_free(void)
{
	/* free contexts */
	mutex_lock(&contexts_mutex);
	keystore_free_contexts();
	mutex_unlock(&contexts_mutex);

	return 0;
}
