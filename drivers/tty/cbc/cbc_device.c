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

#include <linux/sched.h>

#include "cbc_device.h"


void cbc_device_init(struct cbc_device_data *cd)
{
	if (cd)
		INIT_LIST_HEAD(&cd->open_files_head);
}

void cbc_file_init(struct cbc_file_data *file)
{
	if (file) {
		cbc_buffer_queue_init(&file->queue);
		init_waitqueue_head(&file->wq_read);
		INIT_LIST_HEAD(&file->list);
	}
}

void cbc_file_enqueue(struct cbc_file_data *fd, struct cbc_buffer *buffer)
{
	if (fd) {
		if (cbc_buffer_queue_enqueue(&fd->queue, buffer)) {
			cbc_buffer_increment_ref(buffer);
			wake_up_interruptible(&fd->wq_read);
		}
	}
}

struct cbc_buffer *cbc_file_dequeue(struct cbc_file_data *fd)
{
	struct cbc_buffer *buffer = NULL;

	if (fd)
		buffer = cbc_buffer_queue_dequeue(&fd->queue);

	if (buffer && atomic_read(&buffer->refcount) == 0) {
		buffer = NULL;
		pr_err("cbc-core: De-queueing an already freed buffer\n");
	}

	return buffer;
}

int cbc_file_queue_empty(struct cbc_file_data *fd)
{
	if (fd)
		return (fd->queue.write == fd->queue.read);

	return 1;
}

