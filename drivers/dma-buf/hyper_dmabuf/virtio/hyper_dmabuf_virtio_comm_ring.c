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
 * Authors:
 *    Mateusz Polrola <mateuszx.potrola@intel.com>
 *
 */

#include <linux/errno.h>
#include <linux/slab.h>
#include "hyper_dmabuf_virtio_comm_ring.h"

int virtio_comm_ring_init(struct virtio_comm_ring *ring,
			  int entry_size,
			  int num_entries)
{
	ring->data = kcalloc(num_entries, entry_size, GFP_KERNEL);

	if (!ring->data)
		return -ENOMEM;

	ring->head = 0;
	ring->tail = 0;
	ring->used = 0;
	ring->num_entries = num_entries;
	ring->entry_size = entry_size;

	return 0;
}

void virtio_comm_ring_free(struct virtio_comm_ring *ring)
{
	kfree(ring->data);
	ring->data = NULL;
}

bool virtio_comm_ring_full(struct virtio_comm_ring *ring)
{
	if (ring->used == ring->num_entries)
		return true;

	return false;
}

void *virtio_comm_ring_push(struct virtio_comm_ring *ring)
{
	int old_head;

	if (virtio_comm_ring_full(ring))
		return NULL;

	old_head = ring->head;

	ring->head++;
	ring->head %= ring->num_entries;
	ring->used++;

	return ring->data + (ring->entry_size * old_head);
}

void *virtio_comm_ring_pop(struct virtio_comm_ring *ring)
{
	int old_tail = ring->tail;

	ring->tail++;
	ring->tail %= ring->num_entries;
	ring->used--;

	return ring->data + (ring->entry_size * old_tail);
}
