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

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/sched.h>

#include <linux/mei_cl_bus.h>

#include "bh_external.h"
#include "bh_cmd_defs.h"
#include "bh_errcode.h"
#include "dal_dev.h"
#include "dal_cdev.h"

/*
 * this class contains the 3 mei_cl_device, ivm, sdm, rtm.
 * it is initialized during dal_probe and is used by the kernel space kdi
 * to send/recv data to/from mei.
 *
 * this class must be initialized before the kernel space kdi uses it.
 */
struct class *dal_class;

/**
 * dal_dc_print - print client data for debug purpose
 *
 * @dev: device structure
 * @dc: dal client
 */
void dal_dc_print(struct device *dev, struct dal_client *dc)
{
	if (!dc) {
		dev_dbg(dev, "dc is null\n");
		return;
	}

	dev_dbg(dev, "dc: intf = %d. expected to send: %d, sent: %d. expected to receive: %d, received: %d\n",
		dc->intf,
		dc->expected_msg_size_to_fw,
		dc->bytes_sent_to_fw,
		dc->expected_msg_size_from_fw,
		dc->bytes_rcvd_from_fw);
}

/**
 * dal_dc_update_read_state - update client read state
 *
 * @dc : dal client
 * @len: received message length
 *
 * Locking: called under "ddev->context_lock" lock
 */
static void dal_dc_update_read_state(struct dal_client *dc, ssize_t len)
{
	struct dal_device *ddev = dc->ddev;

	/* check BH msg magic, if it exists this is the header */
	if (bh_msg_is_response(ddev->bh_fw_msg.msg, len)) {
		struct bh_response_header *hdr =
			(struct bh_response_header *)dc->ddev->bh_fw_msg.msg;

		dc->expected_msg_size_from_fw = hdr->h.length;
		dev_dbg(&ddev->dev, "expected_msg_size_from_fw = %d bytes read = %zd\n",
			dc->expected_msg_size_from_fw, len);

		/* clear data from the past. */
		dc->bytes_rcvd_from_fw = 0;
	}

	/* update number of bytes rcvd */
	dc->bytes_rcvd_from_fw += len;
}

/**
 * dal_get_client_by_squence_number - find the client interface which
 *                                    the received message is sent to
 *
 * @ddev : dal device
 *
 * Return: kernel space interface or user space interface
 */
static enum dal_intf dal_get_client_by_squence_number(struct dal_device *ddev)
{
	struct bh_response_header *head;

	if (!ddev->clients[DAL_INTF_KDI])
		return DAL_INTF_CDEV;

	head = (struct bh_response_header *)ddev->bh_fw_msg.msg;

	dev_dbg(&ddev->dev, "msg seq = %llu\n", head->seq);

	if (head->seq == ddev->clients[DAL_INTF_KDI]->seq)
		return DAL_INTF_KDI;

	return DAL_INTF_CDEV;
}

/**
 * dal_recv_cb - callback to receive message from DAL FW over mei
 *
 * @cldev : mei client device
 */
static void dal_recv_cb(struct mei_cl_device *cldev)
{
	struct dal_device *ddev;
	struct dal_client *dc;
	enum dal_intf intf;
	ssize_t len;
	ssize_t ret;
	bool is_unexpected_msg = false;

	ddev = mei_cldev_get_drvdata(cldev);

	/*
	 * read the msg from MEI
	 */
	len = mei_cldev_recv(cldev, ddev->bh_fw_msg.msg, DAL_MAX_BUFFER_SIZE);
	if (len < 0) {
		dev_err(&cldev->dev, "recv failed %zd\n", len);
		return;
	}

	/*
	 * lock to prevent read from MEI while writing to MEI and to
	 * deal with just one msg at the same time
	 */
	mutex_lock(&ddev->context_lock);

	/* save msg len */
	ddev->bh_fw_msg.len = len;

	/* set to which interface the msg should be sent */
	if (bh_msg_is_response(ddev->bh_fw_msg.msg, len)) {
		intf = dal_get_client_by_squence_number(ddev);
		dev_dbg(&ddev->dev, "recv_cb(): Client set by sequence number\n");
		dc = ddev->clients[intf];
	} else if (!ddev->current_read_client) {
		intf = DAL_INTF_CDEV;
		dev_dbg(&ddev->dev, "recv_cb(): EXTRA msg received - curr == NULL\n");
		dc = ddev->clients[intf];
		is_unexpected_msg = true;
	} else {
		dc = ddev->current_read_client;
		dev_dbg(&ddev->dev, "recv_cb(): FRAGMENT msg received - curr != NULL\n");
	}

	/* save the current read client */
	ddev->current_read_client = dc;
	dev_dbg(&cldev->dev, "read client type %d data from mei client seq =  %llu\n",
		dc->intf, dc->seq);

	/*
	 * save new msg in queue,
	 * if the queue is full all new messages will be thrown
	 */
	ret = kfifo_in(&dc->read_queue, &ddev->bh_fw_msg.len, sizeof(len));
	ret += kfifo_in(&dc->read_queue, ddev->bh_fw_msg.msg, len);
	if (ret < len + sizeof(len))
		dev_dbg(&ddev->dev, "queue is full - MSG THROWN\n");

	dal_dc_update_read_state(dc, len);

	/*
	 * To clear current client we check if the whole msg received
	 * for the current client
	 */
	if (is_unexpected_msg ||
	    dc->bytes_rcvd_from_fw == dc->expected_msg_size_from_fw) {
		dev_dbg(&ddev->dev, "recv_cb(): setting CURRENT_READER to NULL\n");
		ddev->current_read_client = NULL;
	}
	/* wake up all clients waiting for read or write */
	wake_up_interruptible(&ddev->wq);

	mutex_unlock(&ddev->context_lock);
	dev_dbg(&cldev->dev, "recv_cb(): unlock\n");
}

/**
 * dal_mei_enable - enable mei cldev
 *
 * @ddev: dal device
 *
 * Return: 0 on success
 *         <0 on failure
 */
static int dal_mei_enable(struct dal_device *ddev)
{
	int ret;

	ret = mei_cldev_enable(ddev->cldev);
	if (ret < 0) {
		dev_err(&ddev->cldev->dev, "mei_cl_enable_device() failed with ret = %d\n",
			ret);
		return ret;
	}

	/* save pointer to the context in the device */
	mei_cldev_set_drvdata(ddev->cldev, ddev);

	/* register to mei bus callbacks */
	ret = mei_cldev_register_rx_cb(ddev->cldev, dal_recv_cb);
	if (ret) {
		dev_err(&ddev->cldev->dev, "mei_cl_register_event_cb() failed ret = %d\n",
			ret);
		mei_cldev_disable(ddev->cldev);
	}

	return ret;
}

/**
 * dal_wait_for_write - wait until the dal client is the first writer
 *			in writers queue
 *
 * @ddev: dal device
 * @dc: dal client
 *
 * Return: 0 on success
 *         -ERESTARTSYS when wait was interrupted
 *         -ENODEV when the device was removed
 */
static int dal_wait_for_write(struct dal_device *ddev, struct dal_client *dc)
{
	if (wait_event_interruptible(ddev->wq,
				     list_first_entry(&ddev->writers,
						      struct dal_client,
						      wrlink) == dc ||
				     ddev->is_device_removed)) {
		return -ERESTARTSYS;
	}

	/* if the device was removed indicate that to the caller */
	if (ddev->is_device_removed)
		return -ENODEV;

	return 0;
}

/**
 * dal_send_error_access_denied - put 'access denied' message
 *        into the client read queue. In-band error message.
 *
 * @dc: dal client
 *
 * Return: 0 on success
 *         -ENOMEM when client read queue is full
 *
 * Locking: called under "ddev->write_lock" lock
 */
static int dal_send_error_access_denied(struct dal_client *dc)
{
	struct dal_device *ddev = dc->ddev;
	struct bh_response_header res;
	size_t len;
	int ret;

	mutex_lock(&ddev->context_lock);

	bh_prep_access_denied_response(dc->write_buffer, &res);
	len = sizeof(res);

	ret = kfifo_in(&dc->read_queue, &len, sizeof(len));
	if (ret < sizeof(len)) {
		ret = -ENOMEM;
		goto out;
	}

	ret = kfifo_in(&dc->read_queue, &res, len);
	if (ret < len) {
		ret = -ENOMEM;
		goto out;
	}
	ret = 0;

out:
	mutex_unlock(&ddev->context_lock);
	return ret;
}

/**
 * dal_validate_access - validate that the access is permitted.
 *
 * in case of open session command, validate that the client has the permissions
 * to open session to the requested ta
 *
 * @hdr: command header
 * @count: message size
 * @ctx: context (not used)
 *
 * Return: 0 when command is permitted
 *         -EINVAL when message is invalid
 *         -EPERM when access is not permitted
 *
 * Locking: called under "ddev->write_lock" lock
 */
static int dal_validate_access(const struct bh_command_header *hdr,
			       size_t count, void *ctx)
{
	struct dal_client *dc = ctx;
	struct dal_device *ddev = dc->ddev;
	const uuid_t *ta_id;

	if (!bh_msg_is_cmd_open_session(hdr))
		return 0;

	ta_id = bh_open_session_ta_id(hdr, count);
	if (!ta_id)
		return -EINVAL;

	return dal_access_policy_allowed(ddev, ta_id, dc);
}

/**
 * dal_is_kdi_msg - check if sequence is in kernel space sequence range
 *
 * Each interface (kernel space and user space) has different range of
 * sequence number. This function checks if given number is in kernel space
 * sequence range
 *
 * @hdr: command header
 *
 * Return: true when seq fits kernel space intf
 *         false when seq fits user space intf
 */
static bool dal_is_kdi_msg(const struct bh_command_header *hdr)
{
	return hdr->seq >= MSG_SEQ_START_NUMBER;
}

/**
 * dal_validate_seq - validate that message sequence fits client interface,
 *                    prevent user space client to use kernel space sequence
 *
 * @hdr: command header
 * @count: message size
 * @ctx: context - dal client
 *
 * Return: 0 when sequence match
 *         -EPERM when user space client uses kernel space sequence
 *
 * Locking: called under "ddev->write_lock" lock
 */
static int dal_validate_seq(const struct bh_command_header *hdr,
			    size_t count, void *ctx)
{
	struct dal_client *dc = ctx;

	if (dc->intf != DAL_INTF_KDI && dal_is_kdi_msg(hdr))
		return -EPERM;

	return 0;
}

/*
 * dal_write_filter_tbl - filter functions to validate that the message
 *     is being sent is valid, and the user client
 *     has the permissions to send it
 */
static const bh_filter_func dal_write_filter_tbl[] = {
	dal_validate_access,
	dal_validate_seq,
	NULL,
};

/**
 * dal_write - write message to DAL FW over mei
 *
 * @dc: dal client
 * @count: message size
 * @seq: message sequence (if client is kernel space client)
 *
 * Return: >=0 data length on success
 *         <0 on failure
 */
ssize_t dal_write(struct dal_client *dc, size_t count, u64 seq)
{
	struct dal_device *ddev = dc->ddev;
	struct device *dev;
	ssize_t wr;
	ssize_t ret;
	enum dal_intf intf = dc->intf;

	dev = &ddev->dev;

	dev_dbg(dev, "client interface %d\n", intf);
	dal_dc_print(dev, dc);

	/* lock for adding new client that want to write to fifo */
	mutex_lock(&ddev->write_lock);
	/* update client on latest msg seq number*/
	dc->seq = seq;
	dev_dbg(dev, "current_write_client seq = %llu\n", dc->seq);

	/* put dc in the writers queue if not already */
	if (list_first_entry_or_null(&ddev->writers,
				     struct dal_client, wrlink) != dc) {
		/* adding client to write queue - this is the first fragment */
		const struct bh_command_header *hdr;

		hdr = bh_msg_cmd_hdr(dc->write_buffer, count);
		if (!hdr) {
			dev_dbg(dev, "expected cmd hdr at first fragment\n");
			ret = -EINVAL;
			goto out;
		}
		ret = bh_filter_hdr(hdr, count, dc, dal_write_filter_tbl);
		if (ret == -EPERM) {
			ret = dal_send_error_access_denied(dc);
			ret = ret ? ret : count;
		}
		if (ret)
			goto out;

		dc->bytes_sent_to_fw = 0;
		dc->expected_msg_size_to_fw = hdr->h.length;

		list_add_tail(&dc->wrlink, &ddev->writers);
	}

	/* wait for current writer to finish his write session */
	mutex_unlock(&ddev->write_lock);
	ret = dal_wait_for_write(ddev, dc);
	mutex_lock(&ddev->write_lock);
	if (ret < 0)
		goto out;

	dev_dbg(dev, "before mei_cldev_send - client type %d\n", intf);

	/* send msg via MEI */
	wr = mei_cldev_send(ddev->cldev, dc->write_buffer, count);
	if (wr != count) {
		/* ENODEV can be issued upon internal reset */
		if (wr != -ENODEV) {
			dev_err(dev, "mei_cl_send() failed, write_bytes != count (%zd != %zu)\n",
				wr, count);
			ret = -EFAULT;
			goto out;
		}
		/* if DAL FW client is disconnected, try to reconnect */
		dev_dbg(dev, "try to reconnect to DAL FW cl\n");
		ret = mei_cldev_disable(ddev->cldev);
		if (ret < 0) {
			dev_err(&ddev->cldev->dev, "failed to disable mei cl [%zd]\n",
				ret);
			goto out;
		}
		ret = dal_mei_enable(ddev);
		if (ret < 0)
			dev_err(&ddev->cldev->dev, "failed to reconnect to DAL FW client [%zd]\n",
				ret);
		else
			ret = -EAGAIN;

		goto out;
	}

	dev_dbg(dev, "wrote %zu bytes to fw - client type %d\n", wr, intf);

	/* update client byte sent */
	dc->bytes_sent_to_fw += count;
	ret = wr;

	if (dc->bytes_sent_to_fw != dc->expected_msg_size_to_fw) {
		dev_dbg(dev, "expecting to write more data to DAL FW - client type %d\n",
			intf);
		goto write_more;
	}
out:
	/* remove current dc from the queue */
	list_del_init(&dc->wrlink);
	if (list_empty(&ddev->writers))
		wake_up_interruptible(&ddev->wq);

write_more:
	mutex_unlock(&ddev->write_lock);
	return ret;
}

/**
 * dal_wait_for_read - wait until the client (dc) will have data
 *                     in his read queue
 *
 * @dc: dal client
 *
 * Return: 0 on success
 *         -ENODEV when the device was removed
 */
int dal_wait_for_read(struct dal_client *dc)
{
	struct dal_device *ddev = dc->ddev;
	struct device *dev = &ddev->dev;

	dal_dc_print(dev, dc);

	dev_dbg(dev, "before wait_for_data_to_read() - client type %d kfifo status %d\n",
		dc->intf, kfifo_is_empty(&dc->read_queue));

	/* wait until there is data in the read_queue */
	wait_event_interruptible(ddev->wq, !kfifo_is_empty(&dc->read_queue) ||
				 ddev->is_device_removed);

	dev_dbg(dev, "after wait_for_data_to_read() - client type %d\n",
		dc->intf);

	/* FIXME: use reference counter */
	if (ddev->is_device_removed) {
		dev_dbg(dev, "woke up, device was removed\n");
		return -ENODEV;
	}

	return 0;
}

/**
 * dal_dc_destroy - destroy dal client
 *
 * @ddev: dal device
 * @intf: device interface
 *
 * Locking: called under "ddev->context_lock" lock
 */
void dal_dc_destroy(struct dal_device *ddev, enum dal_intf intf)
{
	struct dal_client *dc;

	dc = ddev->clients[intf];
	if (!dc)
		return;

	kfifo_free(&dc->read_queue);
	kfree(dc);
	ddev->clients[intf] = NULL;
}

/**
 * dal_dc_setup - initialize dal client
 *
 * @ddev: dal device
 * @intf: device interface
 *
 * Return: 0 on success
 *         -EINVAL when client is already initialized
 *         -ENOMEM on memory allocation failure
 */
int dal_dc_setup(struct dal_device *ddev, enum dal_intf intf)
{
	int ret;
	struct dal_client *dc;
	size_t readq_sz;

	if (ddev->clients[intf]) {
		dev_err(&ddev->dev, "client already set\n");
		return -EINVAL;
	}

	dc = kzalloc(sizeof(*dc), GFP_KERNEL);
	if (!dc)
		return  -ENOMEM;

	/* each buffer contains data and length */
	readq_sz = (DAL_MAX_BUFFER_SIZE + sizeof(ddev->bh_fw_msg.len)) *
		   DAL_BUFFERS_PER_CLIENT;
	ret = kfifo_alloc(&dc->read_queue, readq_sz, GFP_KERNEL);
	if (ret) {
		kfree(dc);
		return ret;
	}

	dc->intf = intf;
	dc->ddev = ddev;
	INIT_LIST_HEAD(&dc->wrlink);
	ddev->clients[intf] = dc;
	return 0;
}

/**
 * dal_dev_match - match function to find dal device
 *
 * Used to get dal device from dal_class by device id
 *
 * @dev: device structure
 * @data: the device id
 *
 * Return: 1 on match
 *         0 on mismatch
 */
static int dal_dev_match(struct device *dev, const void *data)
{
	struct dal_device *ddev;
	const enum dal_dev_type *device_id =
			(enum dal_dev_type *)data;

	ddev = container_of(dev, struct dal_device, dev);

	return ddev->device_id == *device_id;
}

/**
 * dal_find_dev - get dal device from dal_class by device id
 *
 * @device_id: device id
 *
 * Return: pointer to the requested device
 *         NULL if the device wasn't found
 */
struct device *dal_find_dev(enum dal_dev_type device_id)
{
	return class_find_device(dal_class, NULL, &device_id, dal_dev_match);
}

/**
 * dal_remove - dal remove callback in mei_cl_driver
 *
 * @cldev: mei client device
 *
 * Return: 0
 */
static int dal_remove(struct mei_cl_device *cldev)
{
	struct dal_device *ddev = mei_cldev_get_drvdata(cldev);

	if (!ddev)
		return 0;

	dal_dev_del(ddev);

	ddev->is_device_removed = true;
	/* make sure the above is set */
	smp_mb();
	/* wakeup write waiters so we can unload */
	if (waitqueue_active(&ddev->wq))
		wake_up_interruptible(&ddev->wq);

	device_del(&ddev->dev);

	mei_cldev_set_drvdata(cldev, NULL);

	mei_cldev_disable(cldev);

	put_device(&ddev->dev);

	return 0;
}
/**
 * dal_device_release - dal release callback in dev structure
 *
 * @dev: device structure
 */
static void dal_device_release(struct device *dev)
{
	struct dal_device *ddev = to_dal_device(dev);

	dal_access_list_free(ddev);
	kfree(ddev->bh_fw_msg.msg);
	kfree(ddev);
}

/**
 * dal_probe - dal probe callback in mei_cl_driver
 *
 * @cldev: mei client device
 * @id: mei client device id
 *
 * Return: 0 on success
 *         <0 on failure
 */
static int dal_probe(struct mei_cl_device *cldev,
		     const struct mei_cl_device_id *id)
{
	struct dal_device *ddev;
	struct device *pdev = &cldev->dev;
	int ret;

	ddev = kzalloc(sizeof(*ddev), GFP_KERNEL);
	if (!ddev)
		return -ENOMEM;

	/* initialize the mutex and wait queue */
	mutex_init(&ddev->context_lock);
	mutex_init(&ddev->write_lock);
	init_waitqueue_head(&ddev->wq);
	INIT_LIST_HEAD(&ddev->writers);
	ddev->cldev = cldev;
	ddev->device_id = id->driver_info;

	ddev->dev.parent = pdev;
	ddev->dev.class  = dal_class;
	ddev->dev.release = dal_device_release;
	dev_set_name(&ddev->dev, "dal%d", ddev->device_id);

	dal_dev_setup(ddev);

	ret = device_register(&ddev->dev);
	if (ret) {
		dev_err(pdev, "unable to register device\n");
		goto err;
	}

	ddev->bh_fw_msg.msg = kzalloc(DAL_MAX_BUFFER_SIZE, GFP_KERNEL);
	if (!ddev->bh_fw_msg.msg) {
		ret = -ENOMEM;
		goto err;
	}

	ret = dal_access_list_init(ddev);
	if (ret)
		goto err;

	ret = dal_mei_enable(ddev);
	if (ret < 0)
		goto err;

	ret = dal_dev_add(ddev);
	if (ret)
		goto err;

	return 0;

err:
	dal_remove(cldev);

	return ret;
}

/* DAL FW HECI client GUIDs */
#define IVM_UUID UUID_LE(0x3c4852d6, 0xd47b, 0x4f46, \
			 0xb0, 0x5e, 0xb5, 0xed, 0xc1, 0xaa, 0x44, 0x0e)
#define SDM_UUID UUID_LE(0xdba4d603, 0xd7ed, 0x4931, \
			 0x88, 0x23, 0x17, 0xad, 0x58, 0x57, 0x05, 0xd5)
#define RTM_UUID UUID_LE(0x5565a099, 0x7fe2, 0x45c1, \
			 0xa2, 0x2b, 0xd7, 0xe9, 0xdf, 0xea, 0x9a, 0x2e)

#define DAL_DEV_ID(__uuid, __device_type) \
	{.uuid = __uuid,                  \
	 .version = MEI_CL_VERSION_ANY,   \
	 .driver_info = __device_type}

/*
 * dal_device_id - ids of dal FW devices,
 * for all 3 dal FW clients (IVM, SDM and RTM)
 */
static const struct mei_cl_device_id dal_device_id[] = {
	DAL_DEV_ID(IVM_UUID, DAL_MEI_DEVICE_IVM),
	DAL_DEV_ID(SDM_UUID, DAL_MEI_DEVICE_SDM),
	DAL_DEV_ID(RTM_UUID, DAL_MEI_DEVICE_RTM),
	/* required last entry */
	{ }
};
MODULE_DEVICE_TABLE(mei, dal_device_id);

static struct mei_cl_driver dal_driver = {
	.id_table = dal_device_id,
	.name = KBUILD_MODNAME,

	.probe  = dal_probe,
	.remove = dal_remove,
};

/**
 * mei_dal_exit - module exit function
 */
static void __exit mei_dal_exit(void)
{
	mei_cldev_driver_unregister(&dal_driver);

	dal_dev_exit();

	dal_kdi_exit();

	class_destroy(dal_class);
}

/**
 * mei_dal_init - module init function
 *
 * Return: 0 on success
 *         <0 on failure
 */
static int __init mei_dal_init(void)
{
	int ret;

	dal_class = class_create(THIS_MODULE, "dal");
	if (IS_ERR(dal_class)) {
		pr_err("couldn't create class\n");
		return PTR_ERR(dal_class);
	}

	ret = dal_dev_init();
	if (ret < 0)
		goto err_class;

	ret = dal_kdi_init();
	if (ret)
		goto err_dev;

	ret = mei_cldev_driver_register(&dal_driver);
	if (ret < 0) {
		pr_err("mei_cl_driver_register failed with status = %d\n", ret);
		goto err;
	}

	return 0;

err:
	dal_kdi_exit();
err_dev:
	dal_dev_exit();
err_class:
	class_destroy(dal_class);
	return ret;
}

module_init(mei_dal_init);
module_exit(mei_dal_exit);

MODULE_AUTHOR("Intel Corporation");
MODULE_DESCRIPTION("Intel(R) MEI Dynamic Application Loader (DAL)");
MODULE_LICENSE("GPL v2");
