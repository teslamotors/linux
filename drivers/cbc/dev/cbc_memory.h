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
 * The file cbc memory provides a simple memory pool for cbc packets.
 */

#ifndef CBC_MEMORY_H_
#define CBC_MEMORY_H_

#include <linux/kernel.h>
#include <linux/atomic.h>
#include <linux/mutex.h>

#include "cbc_types.h"

#define CBC_BUFFER_SIZE CBC_MAX_TOTAL_FRAME_SIZE

struct cbc_buffer {
	u16 frame_length; /* length over all incl. headers and checksum. */
	u16 payload_length; /* length of payload without fill-bytes. including raw-header if present */
	atomic_t refcount;
	u8 data[CBC_BUFFER_SIZE];
};


/*
 * Pool for cbc_buffer. The cbc_memory_pool
 * holds a number of cbc_buffer with reference counting.
 */
struct cbc_memory_pool {
	u16 num_blocks; /* in number of cbc_buffer */
	struct mutex lock;
	struct cbc_buffer pool[0];
};


/* has to be a power of 2 */
#define CBC_QUEUE_LENGTH 16
#define CBC_QUEUE_BM (CBC_QUEUE_LENGTH - 1)

#if (CBC_QUEUE_LENGTH & (CBC_QUEUE_LENGTH - 1))
#error CBC_QUEUE_LENGTH is not a power of 2!
#endif


/*
 * circular queue for cbc_buffer pointers. Reference count
 * handling is not done by this queue.
 */
struct cbc_buffer_queue {
	struct cbc_buffer *queue[CBC_QUEUE_LENGTH];
	u8 write;
	u8 read;
};


/*
 * use kmalloc to create a new cbc_memory_pool with given number of cbc_buffers.
 */
struct cbc_memory_pool *cbc_memory_pool_create(const u16 num_blocks);

/*
 * frees the pool if no buffer is in use.
 * Make sure no new buffers are requested while calling this.
 */
int cbc_memory_pool_try_free(struct cbc_memory_pool *pool);


/*
 * Returns a free buffer or null.
 */
struct cbc_buffer *cbc_memory_pool_get_buffer(struct cbc_memory_pool *pool);


/*
 * decreases the reference count. A reference count of 0 marks this buffer as free.
 */
void cbc_buffer_release(struct cbc_buffer *buffer);


/*
 * Increases the reference count.
 */
void cbc_buffer_increment_ref(struct cbc_buffer *buffer);


/*
 * Initializes a cbc_buffer_queue.
 */
void cbc_buffer_queue_init(struct cbc_buffer_queue *queue);


/*
 * Enqueues a buffer into the queue. If the queue is full,
 * the buffer will not be enqueued without any error.
 * does not do reference count handling.
 */
int cbc_buffer_queue_enqueue(struct cbc_buffer_queue *queue,
						 struct cbc_buffer *buffer);


/*
 * Dequeues a buffer. If queue is empty, null is returned.
 * Does not do reference count handling
 */
struct cbc_buffer *cbc_buffer_queue_dequeue(struct cbc_buffer_queue *queue);


/*
 * Check whether queue is full. Returns 1 if queue is full.
 */
int cbc_buffer_queue_full(struct cbc_buffer_queue *queue);


/*
 * Returns 1 if queue is empty.
 */
int cbc_buffer_queue_empty(struct cbc_buffer_queue *queue);


/*
 * Returns the number of enqueued cbc_buffers.
 */
int cbc_buffer_queue_size(struct cbc_buffer_queue *queue);


#endif /* CBC_DEVICE_H_ */
