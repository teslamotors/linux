// SPDX-License-Identifier: GPL-2.0
/*
 * CBC line discipline kernel module.
 * Handles Carrier Board Communications (CBC) protocol.
 *
 * Copyright (C) 2018 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/slab.h>

#include "cbc_memory.h"

bool cbc_memory_pool_try_free(struct cbc_memory_pool *pool)
{
	u32 i = 0;
	bool allfree = true;
	int tmp;

	if (!pool)
		return 0;

	mutex_lock(&pool->lock);

	for (i = 0; i < (pool->num_blocks); i++) {
		tmp = atomic_read(&pool->pool[i].refcount);
		if (tmp > 0) {
			pr_err("Buffer %i was not freed. (%i refs)\n",
								i, tmp);
			allfree = false;
		}
	}

	mutex_unlock(&pool->lock);

	if (allfree)
		kfree(pool);

	return allfree;
}

struct cbc_memory_pool *cbc_memory_pool_create(const u16 num_blocks)
{
	size_t size;
	struct cbc_memory_pool *new_pool;
	u32 i = 0;

	/* Check we have a valid queue length before we go any further. */
	BUILD_BUG_ON(CBC_QUEUE_LENGTH & (CBC_QUEUE_LENGTH - 1));

	size = sizeof(struct cbc_memory_pool)
		+ (sizeof(struct cbc_buffer) * num_blocks);

	new_pool = kmalloc(size, GFP_KERNEL);
	new_pool->num_blocks = num_blocks;
	mutex_init(&new_pool->lock);

	for (i = 0; i < num_blocks; i++)
		atomic_set(&new_pool->pool[i].refcount, 0);

	return new_pool;
}

struct cbc_buffer *cbc_memory_pool_get_buffer(struct cbc_memory_pool *pool)
{
	u32 i = 0;
	struct cbc_buffer *buffer = NULL;
	int tmp;

	if (pool) {
		mutex_lock(&pool->lock);

		for (; i < pool->num_blocks; i++) {
			tmp = atomic_read(&pool->pool[i].refcount);
			if (tmp == 0) {
				atomic_inc_and_test(&pool->pool[i].refcount);
				buffer = &pool->pool[i];
				buffer->payload_length = 0;
				buffer->frame_length = 0;
				break;
			}
		}
		mutex_unlock(&pool->lock);
	}
	return buffer;
}

void cbc_buffer_release(struct cbc_buffer *buffer)
{
	int tmp;

	if (!buffer)
		return;

	atomic_read(&buffer->refcount);

	tmp = atomic_dec_return(&buffer->refcount);
	if (tmp == 0)
		memset(buffer->data, 0xCD, CBC_BUFFER_SIZE);

}

void cbc_buffer_increment_ref(struct cbc_buffer *buffer)
{
	if (buffer)
		atomic_inc(&buffer->refcount);
}

void cbc_buffer_queue_init(struct cbc_buffer_queue *queue)
{
	queue->write = 0;
	queue->read = 0;
}

int cbc_buffer_queue_enqueue(struct cbc_buffer_queue *queue,
				struct cbc_buffer *buffer)
{
	if (!queue || !buffer)
		return 0;

	if (queue->read + CBC_QUEUE_LENGTH == queue->write) {
		pr_err("cbc buffer queue full\n");
		return 0;
	}

	queue->queue[queue->write & CBC_QUEUE_BM] = buffer;
	queue->write++;
	return 1;
}

struct cbc_buffer *cbc_buffer_queue_dequeue(struct cbc_buffer_queue *queue)
{
	struct cbc_buffer *buffer = NULL;

	if (!queue)
		return buffer;

	if (queue->read == queue->write) {
		pr_err("cbc buffer queue: dequeue while empty.\n");
		return buffer;
	}

	buffer = queue->queue[queue->read & CBC_QUEUE_BM];
	queue->queue[queue->read & CBC_QUEUE_BM] = NULL;
	queue->read++;

	return buffer;
}

