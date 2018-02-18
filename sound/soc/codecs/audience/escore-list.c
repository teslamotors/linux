/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include "escore.h"
#include "escore-list.h"

int escore_queue_msg_to_list(struct escore_priv *escore,
				char *msg, int msg_len)
{
	struct msg_list_entry *entry;
	int rc = 0;

	pr_debug("%s()\n", __func__);
	if (!escore)
		escore = &escore_priv;
	entry = kzalloc(sizeof(struct msg_list_entry), GFP_KERNEL);
	entry->msg = kzalloc(msg_len * sizeof(char), GFP_KERNEL);
	INIT_LIST_HEAD(&entry->list);
	memcpy(entry->msg, msg, msg_len);
	entry->msg_len = msg_len;
	mutex_lock(&escore->msg_list_mutex);
	list_add_tail(&entry->list, &escore->msg_list);
	mutex_unlock(&escore->msg_list_mutex);

	return rc;
}
EXPORT_SYMBOL_GPL(escore_queue_msg_to_list);

/* This function must be called with access_lock acquired */
int escore_write_msg_list(struct escore_priv *escore)
{
	struct msg_list_entry *entry;
	int rc = 0, i;
	u32 api_word[ESCORE_NR_MAX_MSG] = {0};
	u32 resp;

	pr_debug("%s()\n", __func__);
	if (!escore)
		escore = &escore_priv;

	rc = escore_pm_get_sync();
	if (rc < 0) {
		pr_err("%s(): Failed to resume :%d\n", __func__, rc);
		return rc;
	}

	mutex_lock(&escore->msg_list_mutex);
	list_for_each_entry(entry, &escore->msg_list, list) {
		memcpy((char *)api_word, entry->msg, entry->msg_len);
		for (i = 0; i < entry->msg_len / 4; i++) {
			resp = 0;
			rc = escore_cmd_nopm(escore, api_word[i], &resp);
			if (rc < 0) {
				pr_err("%s(): Write Msg fail %d\n",
				       __func__, rc);
				goto exit;
			}
		}
	}
exit:
	mutex_unlock(&escore->msg_list_mutex);
	escore_pm_put_autosuspend();

	return rc;
}
EXPORT_SYMBOL_GPL(escore_write_msg_list);

int escore_flush_msg_list(struct escore_priv *escore)
{
	struct msg_list_entry *entry, *tmp;
	char *msg;

	pr_debug("%s()\n", __func__);

	if (!escore)
		escore = &escore_priv;
	mutex_lock(&escore->msg_list_mutex);
	list_for_each_entry_safe(entry, tmp, &escore->msg_list, list) {
		msg = entry->msg;
		list_del(&entry->list);
		kfree(entry->msg);
		kfree(entry);
	}
	mutex_unlock(&escore->msg_list_mutex);

	return 0;
}
EXPORT_SYMBOL_GPL(escore_flush_msg_list);
