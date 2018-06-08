/*
 * Copyright © 2017 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 */

#ifndef __HYPER_DMABUF_VIRTIO_COMM_RING_H__
#define __HYPER_DMABUF_VIRTIO_COMM_RING_H__

/* Generic ring buffer */
struct virtio_comm_ring {
	/* Buffer allocated for keeping ring entries */
	void *data;

	/* Index pointing to next free element in ring */
	int head;

	/* Index pointing to last released element in ring */
	int tail;

	/* Total number of elements that ring can contain */
	int num_entries;

	/* Size of single ring element in bytes */
	int entry_size;

	/* Number of currently used elements */
	int used;
};

/* Initializes given ring for keeping given a
 * number of entries of specific size */
int virtio_comm_ring_init(struct virtio_comm_ring *ring,
			  int entry_size,
			  int num_entries);

/* Frees buffer used for storing ring entries */
void virtio_comm_ring_free(struct virtio_comm_ring *ring);

/* Checks if ring is full */
bool virtio_comm_ring_full(struct virtio_comm_ring *ring);

/* Gets next free element from ring and marks it as used
 * or NULL if ring is full */
void *virtio_comm_ring_push(struct virtio_comm_ring *ring);

/* Pops oldest element from ring and marks it as free */
void *virtio_comm_ring_pop(struct virtio_comm_ring *ring);

#endif /* __HYPER_DMABUF_VIRTIO_COMM_RING_H__*/
