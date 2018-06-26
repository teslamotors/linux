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

#ifndef CBC_DEVICE_H_
#define CBC_DEVICE_H_

#include <linux/cdev.h>
#include <linux/circ_buf.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/wait.h>


#include "cbc_types.h"
#include "cbc_memory.h"

enum cbc_device_type {
	CBC_DEVICE_TYPE_DEFAULT,
	CBC_DEVICE_TYPE_RAW,
	CBC_DEVICE_TYPE_HIDDEN,
	CBC_DEVICE_TYPE_DEBUG
};

/*
 * struct cbc_device_data -Data for a single channel e.g. /dev/cbc-pmt.
 * @device_name: Device name.
 * @device_type: CBC device type.
 * See c:type 'enum cbc_device_type <cbc_device_type>'.
 * @device: Pointer to device struct.
 * @open_files_head: Linked list of open files for this device.
 *
 * Configuration for a given CBC device. It will be stored in the device private
 * data.
 */
struct cbc_device_data {
	char *device_name;
	enum cbc_device_type device_type;
	struct device *device;
	struct list_head open_files_head;
};

/*
 * struct cbc_file_data - Data for a CBC device file .
 * @queue: CBC buffer queue for this device.
 * @wq_read: wait_queue_head_t fused for waking device on events.
 * @cbc_device: Device data for this device.
 * @list: list_head.
 *
 */
struct cbc_file_data {
	struct cbc_buffer_queue queue;
	wait_queue_head_t wq_read;
	struct cbc_device_data *cbc_device;
	struct list_head list;
};

/*
 * cbc_device_init - Initialise CBC device data
 * @cd: pointer to CBC device data.
 *
 * Initialises device's list_head.
 */
void cbc_device_init(struct cbc_device_data *cd);

/*
 * cbc_file_init - Initialise CBC file data
 * @file: pointer to CBC file data.
 *
 * Initialises device file's CBC queue, wait_queue_head_t and list_head.
 */
void cbc_file_init(struct cbc_file_data *file);

/*
 * cbc_file_enqueue -Add CBC buffer to queue.
 * @fd: CBC device file data.
 * buffer: Pointer to CBC buffer.
 * Increases reference count on buffer.
 */
void cbc_file_enqueue(struct cbc_file_data *fd, struct cbc_buffer *buffer);

/*
 * cbc_file_dequeue - Remove buffer from head of queue.
 * @fd: CBC device file data.
 *
 * Does not decrease reference count.
 */
struct cbc_buffer *cbc_file_dequeue(struct cbc_file_data *fd);

/*
 * cbc_file_queue_empty - Is CBC queue empty?
 */
int cbc_file_queue_empty(struct cbc_file_data *fd);

#endif /* CBC_DEVICE_H_ */
