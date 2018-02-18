/*
 *  * escore-list.h  --  escore list APIs
 *   *
 *    * This program is free software; you can redistribute it and/or modify
 *     * it under the terms of the GNU General Public License version 2 as
 *      * published by the Free Software Foundation.
 *       */

#ifndef _ESCORE_LIST_H
#define _ESCORE_LIST_H
struct msg_list_entry {
	struct list_head list;
	char *msg;
	int msg_len;
};
int escore_queue_msg_to_list(struct escore_priv *escore,
				char *msg, int msg_len);
int escore_flush_msg_list(struct escore_priv *escore);
int escore_write_msg_list(struct escore_priv *escore);

#define ESCORE_NR_MAX_MSG	200
#endif
