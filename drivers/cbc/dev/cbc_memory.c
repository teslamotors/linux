/*
 * carrier boards communicatons core.
 * demultiplexes the cbc protocol.
 *
 * Copryright (C) 2016 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */
/**
 * @file
 *
 */

#include "dev/cbc_memory.h"

#include <linux/slab.h>

int cbc_memory_pool_try_free(struct cbc_memory_pool *pool)
{
	u32 i = 0;
	u8 allfree = 1;
	int tmp;

	if (pool) {
		for (; i < (pool->num_blocks) ; i++) {
			tmp = atomic_read(&pool->pool[i].refcount);
			if (tmp > 0) {
				pr_err("Buffer %i was not free. (%i refs)\n", i, tmp);
				allfree = 0;
			}
		}

		if (allfree)
			kfree(pool);
		else
			return -1;
	}
	return 0;
}


struct cbc_memory_pool *cbc_memory_pool_create(const u16 num_blocks)
{
	size_t size;
	struct cbc_memory_pool *new_pool;
	u32 i = 0;

	size = sizeof(struct cbc_memory_pool) + (sizeof(struct cbc_buffer) * (num_blocks - 1));

	new_pool = kmalloc(size, GFP_KERNEL);
	new_pool->num_blocks = num_blocks;
	mutex_init(&new_pool->lock);

	for (; i < num_blocks ; i++)
		atomic_set(&new_pool->pool[i].refcount, 0);

	return new_pool;
}


struct cbc_buffer *cbc_memory_pool_get_buffer(struct cbc_memory_pool *pool)
{
	u32 i = 0;
	struct cbc_buffer *buffer = 0;
	int tmp;

	if (pool) {
		mutex_lock(&pool->lock);

		for (; i < pool->num_blocks; i++) {
			tmp = atomic_read(&pool->pool[i].refcount);
			if (tmp == 0) {
				atomic_inc(&pool->pool[i].refcount);
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
	if (buffer) {
		int tmp;

		tmp = atomic_read(&buffer->refcount);
		if (tmp == 0)
			pr_err("cbc_buffer_release, error: buffer already free\n");

		tmp = atomic_dec_return(&buffer->refcount);

		if (tmp == 0)
			memset(buffer->data, 0xCD, CBC_BUFFER_SIZE);
	}
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

	if (cbc_buffer_queue_full(queue)) {
		pr_err("cbc buffer queue full\n");
		return 0;
	}

	queue->queue[queue->write & CBC_QUEUE_BM] = buffer;
	queue->write++;
	return 1;
}


struct cbc_buffer *cbc_buffer_queue_dequeue(struct cbc_buffer_queue *queue)
{
	struct cbc_buffer *buffer = 0;

	if (!queue)
		return buffer;

	if (cbc_buffer_queue_empty(queue)) {
		pr_err("cbc buffer queue: dequeue while empty.\n");
		return buffer;
	}

	buffer = queue->queue[queue->read & CBC_QUEUE_BM];
	queue->queue[queue->read & CBC_QUEUE_BM] = 0;
	queue->read++;

	return buffer;
}


int cbc_buffer_queue_full(struct cbc_buffer_queue *queue)
{
	if (queue)
		return queue->read + CBC_QUEUE_LENGTH == queue->write;
	return 0;
}


int cbc_buffer_queue_empty(struct cbc_buffer_queue *queue)
{
	if (queue)
		return queue->read == queue->write;
	return 0;
}


int cbc_buffer_queue_size(struct cbc_buffer_queue *queue)
{
	if (queue)
		return queue->write - queue->read;
	return 0;
}


/* EOF */
