/*
 * carrier boards communicatons core.
 * demultiplexes the cbc protocol.
 *
 * Copryright (C) 2014 Intel Corporation
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
 * A cbc device is the struct used inside the kernel module
 * to hold the device struct and the read queue of one device in /dev
 * which represents one mux channel from cbc.
 */

#ifndef CBC_DEVICE_H_
#define CBC_DEVICE_H_

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/wait.h>
#include <linux/list.h>

#include "cbc_types.h"
#include "dev/cbc_memory.h"

enum cbc_device_type {
	e_cbc_device_type_default,
	e_cbc_device_type_raw,
	e_cbc_device_type_hidden,
	e_cbc_device_type_debug
};


/* data for 1 channel like /dev/cbc-pmt */
/* contains:
 * configuration like device name and if it is a raw channel
 * open files queues
 *
 * it will be stored in the device-privatedata
 */
struct cbc_device_data {
	char *device_name;
	enum cbc_device_type device_type;

	struct device *device;

	struct list_head open_files_head;
};


/*
 * data for one open file associated to one channel.
 *
 */
struct cbc_file_data {
	struct cbc_buffer_queue queue;

	wait_queue_head_t wq_read;
	struct cbc_device_data *cbc_device;
	struct list_head list;
};


void cbc_device_init(struct cbc_device_data *cd);


void cbc_file_init(struct cbc_file_data *file);


/* increases ref count on buffer */
void cbc_file_enqueue(struct cbc_file_data *cd,
						 struct cbc_buffer *buffer);


/* does not decrease ref count */
struct cbc_buffer *cbc_file_dequeue(struct cbc_file_data *fd);


int cbc_file_queue_full(struct cbc_file_data *fd);


int cbc_file_queue_empty(struct cbc_file_data *fd);


#endif /* CBC_DEVICE_H_ */
