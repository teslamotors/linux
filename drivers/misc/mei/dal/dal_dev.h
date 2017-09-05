/******************************************************************************
 * Intel mei_dal Linux driver
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2016-2017 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * The full GNU General Public License is included in this distribution
 * in the file called LICENSE.GPL.
 *
 * Contact Information:
 *	Intel Corporation.
 *	linux-mei@linux.intel.com
 *	http://www.intel.com
 *
 * BSD LICENSE
 *
 * Copyright(c) 2016-2017 Intel Corporation. All rights reserved.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name Intel Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/
#ifndef _DAL_KDI_H_
#define _DAL_KDI_H_

#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/kfifo.h>

#define DAL_MAX_BUFFER_SIZE     4096
#define DAL_BUFFERS_PER_CLIENT    10

#define DAL_CLIENTS_PER_DEVICE     2

extern struct class *dal_class;

/**
 * enum dal_intf - dal interface type
 *
 * @DAL_INTF_KDI: (kdi) kernel space interface
 * @DAL_INTF_CDEV: char device interface
 */
enum dal_intf {
	DAL_INTF_KDI,
	DAL_INTF_CDEV,
};

/**
 * enum dal_dev_type - devices that are exposed to userspace
 *
 * @DAL_MEI_DEVICE_IVM: IVM - Intel/Issuer Virtual Machine
 * @DAL_MEI_DEVICE_SDM: SDM - Security Domain Manager
 * @DAL_MEI_DEVICE_RTM: RTM - Run Time Manager (Launcher)
 *
 * @DAL_MEI_DEVICE_MAX: max dal device type
 */
enum dal_dev_type {
	DAL_MEI_DEVICE_IVM,
	DAL_MEI_DEVICE_SDM,
	DAL_MEI_DEVICE_RTM,

	DAL_MEI_DEVICE_MAX
};

/**
 * struct dal_client - host client
 *
 * @ddev: dal parent device
 * @wrlink: link in the writers list
 * @read_queue: queue of received messages from DAL FW
 * @write_buffer: buffer to send to DAL FW
 * @intf: client interface - user space or kernel space
 *
 * @seq: the sequence number of the last message sent (in kernel space API only)
 *       When a message is received from DAL FW, we use this sequence number
 *       to decide which client should get the message. If the sequence
 *       number of the message is equals to the kernel space sequence number,
 *       the kernel space client should get the message.
 *       Otherwise the user space client will get it.
 * @expected_msg_size_from_fw: the expected msg size from DALFW
 * @expected_msg_size_to_fw: the expected msg size that will be sent to DAL FW
 * @bytes_rcvd_from_fw: number of bytes that were received from DAL FW
 * @bytes_sent_to_fw: number of bytes that were sent to DAL FW
 */
struct dal_client {
	struct dal_device *ddev;
	struct list_head wrlink;
	struct kfifo read_queue;
	char write_buffer[DAL_MAX_BUFFER_SIZE];
	enum dal_intf intf;

	u64 seq;
	u32 expected_msg_size_from_fw;
	u32 expected_msg_size_to_fw;
	u32 bytes_rcvd_from_fw;
	u32 bytes_sent_to_fw;
};

/**
 * struct dal_bh_msg - msg received from DAL FW.
 *
 * @len: message length
 * @msg: message buffer
 */
struct dal_bh_msg {
	size_t len;
	char *msg;
};

/**
 * struct dal_device - DAL private device struct.
 *     each DAL device has a context (i.e IVM, SDM, RTM)
 *
 * @dev: device on a bus
 * @cdev: character device
 * @status: dal device status
 *
 * @context_lock: big device lock
 * @write_lock: lock over write list
 * @wq: dal clients wait queue. When client wants to send or receive message,
 *      he waits in this queue until he is ready
 * @writers: write pending list
 * @clients: clients on this device (userspace and kernel space)
 * @bh_fw_msg: message which was received from DAL FW
 * @current_read_client: current reading client (which receives message from
 *                       DAL FW)
 *
 * @cldev: the MEI CL device which corresponds to a single DAL FW HECI client
 *
 * @is_device_removed: device removed flag
 *
 * @device_id: DAL device type
 */
struct dal_device {
	struct device dev;
	struct cdev cdev;
#define DAL_DEV_OPENED 0
	unsigned long status;

	struct mutex context_lock; /* device lock */
	struct mutex write_lock; /* write lock */
	wait_queue_head_t wq;
	struct list_head writers;
	struct dal_client *clients[DAL_CLIENTS_PER_DEVICE];
	struct dal_bh_msg bh_fw_msg;
	struct dal_client *current_read_client;

	struct mei_cl_device *cldev;

	bool is_device_removed;

	int device_id;
};

#define to_dal_device(d) container_of(d, struct dal_device, dev)

ssize_t dal_write(struct dal_client *dc, size_t count, u64 seq);
int dal_wait_for_read(struct dal_client *dc);

struct device *dal_find_dev(enum dal_dev_type device_id);

void dal_dc_print(struct device *dev, struct dal_client *dc);
int dal_dc_setup(struct dal_device *ddev, enum dal_intf intf);
void dal_dc_destroy(struct dal_device *ddev, enum dal_intf intf);

#endif /* _DAL_KDI_H_ */
