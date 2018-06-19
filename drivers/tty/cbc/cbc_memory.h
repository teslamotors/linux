/* SPDX-License-Identifier: GPL-2.0 */
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

#ifndef CBC_MEMORY_H_
#define CBC_MEMORY_H_

#include <linux/kernel.h>
#include <linux/atomic.h>
#include <linux/mutex.h>

#include "cbc_types.h"

#define CBC_BUFFER_SIZE CBC_MAX_TOTAL_FRAME_SIZE

/*
 * struct cbc_buffer - Represents a single CBC frame buffer.
 * @frame_length: Total length  including headers and checksum.
 * @payload_length: Length of payload without fill-bytes, including raw-
 *			header if present.
 * @refcount: Reference count incremented/decremented when queuing
 *	de-queueing the buffer.
 * @data: Contents of buffer.
 */
struct cbc_buffer {
	u16 frame_length;
	u16 payload_length;
	atomic_t refcount;
	u8 data[CBC_BUFFER_SIZE];
};

/*
 * struct cbc_memory_pool - Memory pool for cbc_buffer.
 * @num_blocks: Number of blocks allocated (CBC queue length * maximum number of
 *		channels.
 * lock:	Mutex to lock memory operations.
 * pool:	The actual pool of CBC buffers.
 *
 * The cbc_memory_pool holds a number of cbc_buffers with reference counting.
 */
struct cbc_memory_pool {
	u16 num_blocks;
	struct mutex lock;
	struct cbc_buffer pool[0];
};

/* CBC queue length has to be a power of 2 */
#define CBC_QUEUE_LENGTH 16
#define CBC_QUEUE_BM (CBC_QUEUE_LENGTH - 1)

/*
 * cbc_buffer_queue - Circular buffer for cbc_buffer pointers.
 * @queue:	The queue of CBC buffer pointers.
 * @write:	Head of queue.
 * @read:	Tail of queue.
 *
 * Reference count handling is not done by this queue.
 */
struct cbc_buffer_queue {
	struct cbc_buffer *queue[CBC_QUEUE_LENGTH];
	u8 write;
	u8 read;
};

/* cbc_memory_pool_create - Create memory pool of CBC buffers.
 * @num_blocks: Size of memory pool to create (based on queue size and number
 *			maximum of channels).
 *
 * Use kmalloc to create a new cbc_memory_pool with given number of cbc_buffers.
 */
struct cbc_memory_pool *cbc_memory_pool_create(const u16 num_blocks);

/*
 * cbc_memory_pool_try_free - Frees the pool if no buffer is in use.
 * @pool: Pointer to memory pool.
 *
 * Ensure no new buffers are requested while calling this.
 *
 * Return: True if pool has been freed, false if not.
 */
bool cbc_memory_pool_try_free(struct cbc_memory_pool *pool);

/*
 * cbc_memory_pool_get_buffer - Returns a free CBC buffer if available.
 * @pool: Pointer to memory pool.
 *
 * Return: Pointer to buffer is one is available, null otherwise.
 */
struct cbc_buffer *cbc_memory_pool_get_buffer(struct cbc_memory_pool *pool);

/*
 * cbc_buffer_release - Release CBC buffer (if not is use elsewhere).
 * @buffer: Buffer to release.
 *
 * Decreases the reference count. A reference count of 0 marks this buffer as
 * free.
 */
void cbc_buffer_release(struct cbc_buffer *buffer);

/*
 * cbc_buffer_increment_ref - Increases the reference count for a CBC buffer.
 * @buffer: Buffer to increment ref count for.
 */
void cbc_buffer_increment_ref(struct cbc_buffer *buffer);

/*
 * cbc_buffer_queue_init - Initializes a cbc_buffer_queue.
 * @queue: CBC buffer queue to initialise.
 *
 * Initialises head and tail.
 */
void cbc_buffer_queue_init(struct cbc_buffer_queue *queue);

/*
 * cbc_buffer_queue_enqueue - Add CBC buffer to a queue.
 * @queue:	CBC buffer queue.
 * @buffer:	Buffer to add.
 *
 * Enqueues a buffer into the queue. If the queue is full,
 * the buffer will not be enqueued without any error.
 * does not do reference count handling.
 */
int cbc_buffer_queue_enqueue(struct cbc_buffer_queue *queue,
				struct cbc_buffer *buffer);

/*
 * cbc_buffer_queue_dequeue - Remove buffer from queue if not in use
 *					  elsewhere.
 * @queue:	CBC buffer queue
 * @buffer:	Buffer to dequeue
 *
 * Dequeues a buffer. If queue is empty, null is returned.
 * Does not do reference count handling
 */
struct cbc_buffer *cbc_buffer_queue_dequeue(struct cbc_buffer_queue *queue);

#endif /* CBC_DEVICE_H_ */
