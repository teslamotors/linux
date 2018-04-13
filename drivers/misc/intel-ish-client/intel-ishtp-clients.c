/*
 * ISHTP clients driver
 *
 * Copyright (c) 2016, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 */

#include <linux/capability.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/intel-ishtp-clients.h>
#include <linux/sched/signal.h>
#include <linux/ioctl.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/uuid.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>

#include "ishtp-dev.h"
#include "client.h"

/*
 * ISH client misc driver structure
 */
struct ishtp_cl_miscdev {
	struct miscdevice cl_miscdev;
	struct ishtp_cl_device *cl_device;
	struct ishtp_cl *cl;

	/* Wait queue for waiting ISHFW event/message */
	wait_queue_head_t read_wait;
	/* Read buffer, will point to available ISHFW message */
	struct ishtp_cl_rb *read_rb;

	/*
	 * cl member can be freed/changed by ISHFW reset and release() calling,
	 * so must pay attention to protect cl while try to access it. This
	 * mutex is used to protect cl member.
	 */
	struct mutex cl_mutex;

	struct work_struct reset_work;
};

/* ISH client GUIDs */
/* SMHI client UUID: bb579a2e-cc54-4450-b1-d0-5e-75-20-dc-ad-25 */
static const uuid_le ishtp_smhi_guid =
			UUID_LE(0xbb579a2e, 0xcc54, 0x4450,
				0xb1, 0xd0, 0x5e, 0x75, 0x20, 0xdc, 0xad, 0x25);

/* Trace log client UUID: c1cc78b9-b693-4e54-91-91-51-69-cb-02-7c-25 */
static const uuid_le ishtp_trace_guid =
			UUID_LE(0xc1cc78b9, 0xb693, 0x4e54,
				0x91, 0x91, 0x51, 0x69, 0xcb, 0x02, 0x7c, 0x25);

/* Trace config client UUID: 1f050626-d505-4e94-b1-89-53-5d-7d-e1-9c-f2 */
static const uuid_le ishtp_traceconfig_guid =
			UUID_LE(0x1f050626, 0xd505, 0x4e94,
				0xb1, 0x89, 0x53, 0x5d, 0x7d, 0xe1, 0x9c, 0xf2);

/* ISHFW loader client UUID: c804d06a-55bd-4ea7-ad-ed-1e-31-22-8c-76-dc */
static const uuid_le ishtp_loader_guid =
			UUID_LE(0xc804d06a, 0x55bd, 0x4ea7,
				0xad, 0xed, 0x1e, 0x31, 0x22, 0x8c, 0x76, 0xdc);

static int ishtp_cl_open(struct inode *inode, struct file *file)
{
	struct miscdevice *misc = file->private_data;
	struct ishtp_cl *cl;
	struct ishtp_device *dev;
	struct ishtp_cl_miscdev *ishtp_cl_misc;
	int err = 0;

	/* Non-blocking semantics are not supported */
	if (file->f_flags & O_NONBLOCK)
		return	-EINVAL;

	ishtp_cl_misc = container_of(misc,
				struct ishtp_cl_miscdev, cl_miscdev);
	if (!ishtp_cl_misc || !ishtp_cl_misc->cl_device)
		return -ENODEV;

	dev = ishtp_cl_misc->cl_device->ishtp_dev;
	if (!dev)
		return -ENODEV;

	mutex_lock(&ishtp_cl_misc->cl_mutex);

	/*
	 * Every client only supports one opened instance
	 * at the sametime.
	 */
	if (ishtp_cl_misc->cl) {
		err = -EBUSY;
		goto out_unlock;
	}

	cl = ishtp_cl_allocate(dev);
	if (!cl) {
		err = -ENOMEM;
		goto out_free;
	}

	if (dev->dev_state != ISHTP_DEV_ENABLED) {
		err = -ENODEV;
		goto out_free;
	}

	err = ishtp_cl_link(cl, ISHTP_HOST_CLIENT_ID_ANY);
	if (err)
		goto out_free;

	ishtp_cl_misc->cl = cl;

	file->private_data = ishtp_cl_misc;

	mutex_unlock(&ishtp_cl_misc->cl_mutex);

	return nonseekable_open(inode, file);

out_free:
	kfree(cl);
out_unlock:
	mutex_unlock(&ishtp_cl_misc->cl_mutex);
	return err;
}

#define WAIT_FOR_SEND_SLICE_MS		500
#define WAIT_FOR_SEND_COUNT		10

static int ishtp_cl_release(struct inode *inode, struct file *file)
{
	struct ishtp_cl_miscdev *ishtp_cl_misc = file->private_data;
	struct ishtp_cl *cl;
	struct ishtp_cl_rb *rb;
	struct ishtp_device *dev;
	int try = WAIT_FOR_SEND_COUNT;
	int rets;

	mutex_lock(&ishtp_cl_misc->cl_mutex);

	/* Wake up from waiting if anyone wait on it */
	if (waitqueue_active(&ishtp_cl_misc->read_wait))
		wake_up_interruptible(&ishtp_cl_misc->read_wait);

	cl = ishtp_cl_misc->cl;
	dev = cl->dev;

	/*
	 * May happen if device sent FW reset or was intentionally
	 * halted by host SW. The client is then invalid.
	 */
	if ((dev->dev_state == ISHTP_DEV_ENABLED) &&
			(cl->state == ISHTP_CL_CONNECTED)) {
		/*
		 * Check and wait 5s for message in tx_list to be sent.
		 */
		do {
			if (!ishtp_cl_tx_empty(cl))
				msleep_interruptible(WAIT_FOR_SEND_SLICE_MS);
			else
				break;
		} while (--try);

		cl->state = ISHTP_CL_DISCONNECTING;
		rets = ishtp_cl_disconnect(cl);
	}

	ishtp_cl_unlink(cl);
	ishtp_cl_flush_queues(cl);
	/* Disband and free all Tx and Rx client-level rings */
	ishtp_cl_free(cl);

	ishtp_cl_misc->cl = NULL;

	rb = ishtp_cl_misc->read_rb;
	if (rb) {
		ishtp_cl_io_rb_recycle(rb);
		ishtp_cl_misc->read_rb = NULL;
	}

	file->private_data = NULL;

	mutex_unlock(&ishtp_cl_misc->cl_mutex);

	return rets;
}

static ssize_t ishtp_cl_read(struct file *file, char __user *ubuf,
			size_t length, loff_t *offset)
{
	struct ishtp_cl_miscdev *ishtp_cl_misc = file->private_data;
	struct ishtp_cl *cl;
	struct ishtp_cl_rb *rb = NULL;
	struct ishtp_device *dev;
	int rets = 0;

	/* Non-blocking semantics are not supported */
	if (file->f_flags & O_NONBLOCK)
		return -EINVAL;

	mutex_lock(&ishtp_cl_misc->cl_mutex);

	cl = ishtp_cl_misc->cl;

	/*
	 * ISHFW reset or parallel release() calling will cause cl be freed.
	 * So must make sure cl is still available.
	 */
	if (WARN_ON(!cl || !cl->dev)) {
		rets = -ENODEV;
		goto out;
	}

	dev = cl->dev;
	if (dev->dev_state != ISHTP_DEV_ENABLED) {
		rets = -ENODEV;
		goto out;
	}

	if (ishtp_cl_misc->read_rb)
		goto get_rb;

	rb = ishtp_cl_rx_get_rb(cl);
	if (rb != NULL)
		goto copy_buffer;

	/*
	 * Release mutex for other operation can be processed parallelly
	 * during waiting.
	 */
	mutex_unlock(&ishtp_cl_misc->cl_mutex);

	if (wait_event_interruptible(ishtp_cl_misc->read_wait,
			ishtp_cl_misc->read_rb != NULL)) {
		dev_err(&ishtp_cl_misc->cl_device->dev,
			"Wake up not successful;"
			"signal pending = %d signal = %08lX\n",
			signal_pending(current),
			current->pending.signal.sig[0]);
		return -ERESTARTSYS;
	}

	mutex_lock(&ishtp_cl_misc->cl_mutex);

	/*
	 * waitqueue can be woken up in many cases, so must check
	 * if dev and cl is still available.
	 */
	if (dev->dev_state != ISHTP_DEV_ENABLED) {
		rets = -ENODEV;
		goto out;
	}

	cl = ishtp_cl_misc->cl;
	if (!cl) {
		rets = -ENODEV;
		goto out;
	}

	if (cl->state == ISHTP_CL_INITIALIZING ||
		cl->state == ISHTP_CL_DISCONNECTED ||
		cl->state == ISHTP_CL_DISCONNECTING) {
		rets = -EBUSY;
		goto out;
	}

get_rb:
	rb = ishtp_cl_misc->read_rb;
	if (!rb) {
		rets = -ENODEV;
		goto out;
	}

copy_buffer:
	/* Now copy the data to user space */
	if (length == 0 || ubuf == NULL || *offset > rb->buf_idx) {
		rets = -EMSGSIZE;
		goto out;
	}

	/*
	 * length is being truncated, however buf_idx may
	 * point beyond that.
	 */
	length = min_t(size_t, length, rb->buf_idx - *offset);

	if (copy_to_user(ubuf, rb->buffer.data + *offset, length)) {
		rets = -EFAULT;
		goto out;
	}

	rets = length;
	*offset += length;
	if ((unsigned long)*offset < rb->buf_idx)
		goto out;

	ishtp_cl_io_rb_recycle(rb);
	ishtp_cl_misc->read_rb = NULL;
	*offset = 0;

out:
	mutex_unlock(&ishtp_cl_misc->cl_mutex);
	return rets;
}

static ssize_t ishtp_cl_write(struct file *file, const char __user *ubuf,
	size_t length, loff_t *offset)
{
	struct ishtp_cl_miscdev *ishtp_cl_misc = file->private_data;
	struct ishtp_cl *cl;
	void *write_buf = NULL;
	struct ishtp_device *dev;
	int rets;

	/* Non-blocking semantics are not supported */
	if (file->f_flags & O_NONBLOCK) {
		rets = -EINVAL;
		goto out_unlock;
	}

	mutex_lock(&ishtp_cl_misc->cl_mutex);

	cl = ishtp_cl_misc->cl;

	/*
	 * ISHFW reset or parallel release() calling will cause cl be freed.
	 * So must make sure cl is still available.
	 */
	if (WARN_ON(!cl || !cl->dev)) {
		rets = -ENODEV;
		goto out_unlock;
	}

	dev = cl->dev;

	if (dev->dev_state != ISHTP_DEV_ENABLED) {
		rets = -ENODEV;
		goto out_unlock;
	}

	if (cl->state != ISHTP_CL_CONNECTED) {
		dev_err(&ishtp_cl_misc->cl_device->dev,
			"host client = %d isn't connected to fw client = %d\n",
			cl->host_client_id, cl->fw_client_id);
		rets = -ENODEV;
		goto out_unlock;
	}

	if (length <= 0) {
		rets = -EMSGSIZE;
		goto out_unlock;
	}

	if (length > cl->device->fw_client->props.max_msg_length) {
		rets = -EMSGSIZE;
		goto out_unlock;
	}

	write_buf = kmalloc(length, GFP_KERNEL);
	if (!write_buf) {
		rets = -ENOMEM;
		goto out_unlock;
	}

	rets = copy_from_user(write_buf, ubuf, length);
	if (rets)
		goto out_free;

	rets = ishtp_cl_send(cl, write_buf, length);
	if (!rets)
		rets = length;
	else
		rets = -EIO;

out_free:
	kfree(write_buf);
out_unlock:
	mutex_unlock(&ishtp_cl_misc->cl_mutex);

	return rets;
}

static int ishtp_cl_ioctl_connect_client(struct file *file,
	struct ishtp_connect_client_data *data)
{
	struct ishtp_cl_miscdev *ishtp_cl_misc = file->private_data;
	struct ishtp_device *dev;
	struct ishtp_client *client;
	struct ishtp_cl_device *cl_device;
	struct ishtp_cl *cl = ishtp_cl_misc->cl;
	struct ishtp_fw_client *fw_client;

	if (WARN_ON(!cl || !cl->dev))
		return -ENODEV;

	dev = cl->dev;

	if (dev->dev_state != ISHTP_DEV_ENABLED)
		return -ENODEV;

	if (cl->state != ISHTP_CL_INITIALIZING &&
			cl->state != ISHTP_CL_DISCONNECTED)
		return -EBUSY;

	cl_device = ishtp_cl_misc->cl_device;

	if (uuid_le_cmp(data->in_client_uuid,
			cl_device->fw_client->props.protocol_name) != 0) {
		dev_err(&ishtp_cl_misc->cl_device->dev,
			"Required uuid don't match current client uuid\n");
		return -EFAULT;
	}

	/* Find the fw client we're trying to connect to */
	fw_client = ishtp_fw_cl_get_client(dev, &data->in_client_uuid);
	if (fw_client == NULL) {
		dev_err(&ishtp_cl_misc->cl_device->dev,
			"Don't find current client uuid\n");
		return -ENOENT;
	}

	cl->fw_client_id = fw_client->client_id;
	cl->state = ISHTP_CL_CONNECTING;

	/* Prepare the output buffer */
	client = &data->out_client_properties;
	client->max_msg_length = fw_client->props.max_msg_length;
	client->protocol_version = fw_client->props.protocol_version;

	return ishtp_cl_connect(cl);
}

static long ishtp_cl_ioctl(struct file *file, unsigned int cmd,
			unsigned long data)
{
	struct ishtp_cl_miscdev *ishtp_cl_misc = file->private_data;
	struct ishtp_cl *cl;
	struct ishtp_device *dev;
	struct ishtp_connect_client_data *connect_data = NULL;
	char fw_stat_buf[20];
	unsigned int ring_size;
	int rets = 0;

	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;

	mutex_lock(&ishtp_cl_misc->cl_mutex);

	cl = ishtp_cl_misc->cl;
	/*
	 * ISHFW reset or parallel release() calling will cause cl be freed.
	 * So must make sure cl is still available.
	 */
	if (WARN_ON(!cl || !cl->dev)) {
		mutex_unlock(&ishtp_cl_misc->cl_mutex);
		return -ENODEV;
	}

	dev = cl->dev;

	switch (cmd) {
	case IOCTL_ISH_HW_RESET:
		rets = ish_hw_reset(dev);
		break;

	case IOCTL_ISHTP_SET_RX_FIFO_SIZE:
		ring_size = data;

		if (ring_size > CL_MAX_RX_RING_SIZE) {
			rets = -EINVAL;
			break;
		}

		if (cl->state != ISHTP_CL_INITIALIZING) {
			rets = -EBUSY;
			break;
		}

		cl->rx_ring_size = ring_size;
		break;

	case IOCTL_ISHTP_SET_TX_FIFO_SIZE:
		ring_size = data;

		if (ring_size > CL_MAX_TX_RING_SIZE) {
			rets = -EINVAL;
			break;
		}

		if (cl->state != ISHTP_CL_INITIALIZING) {
			rets = -EBUSY;
			break;
		}

		cl->tx_ring_size = ring_size;
		break;

	case IOCTL_ISH_GET_FW_STATUS:
		if (!data) {
			rets = -ENOMEM;
			break;
		}

		scnprintf(fw_stat_buf, sizeof(fw_stat_buf),
			"%08X\n", dev->ops->get_fw_status(dev));

		if (copy_to_user((char __user *)data, fw_stat_buf,
				strlen(fw_stat_buf))) {
			rets = -EFAULT;
			break;
		}

		rets = strlen(fw_stat_buf);
		break;

	case IOCTL_ISHTP_CONNECT_CLIENT:
		if (dev->dev_state != ISHTP_DEV_ENABLED) {
			rets = -ENODEV;
			break;
		}

		connect_data = kzalloc(sizeof(struct ishtp_connect_client_data),
								GFP_KERNEL);
		if (!connect_data) {
			rets = -ENOMEM;
			break;
		}

		if (copy_from_user(connect_data, (char __user *)data,
				sizeof(struct ishtp_connect_client_data))) {
			rets = -EFAULT;
			break;
		}

		rets = ishtp_cl_ioctl_connect_client(file, connect_data);
		if (rets)
			break;

		/* If all is ok, copying the data back to user. */
		if (copy_to_user((char __user *)data, connect_data,
				sizeof(struct ishtp_connect_client_data))) {
			rets = -EFAULT;
			break;
		}

		break;

	default:
		rets = -EINVAL;
		break;
	}

	mutex_unlock(&ishtp_cl_misc->cl_mutex);

	kfree(connect_data);

	return rets;
}

/*
 * File operations structure will be used for ishtp client misc device.
 */
static const struct file_operations ishtp_cl_fops = {
	.owner = THIS_MODULE,
	.read = ishtp_cl_read,
	.unlocked_ioctl = ishtp_cl_ioctl,
	.open = ishtp_cl_open,
	.release = ishtp_cl_release,
	.write = ishtp_cl_write,
	.llseek = no_llseek
};

/**
 * ishtp_cl_event_cb() - ISHTP client driver event callback
 * @cl_device:		ISHTP client device instance
 *
 * This function gets called on related event recevied from ISHFW.
 * It will remove event buffer exists on in_process list to related
 * client device and wait up client driver to process.
 */
static void ishtp_cl_event_cb(struct ishtp_cl_device *cl_device)
{
	struct ishtp_cl_miscdev *ishtp_cl_misc;
	struct ishtp_cl *cl;
	struct ishtp_cl_rb *rb;

	ishtp_cl_misc = ishtp_get_drvdata(cl_device);
	if (!ishtp_cl_misc)
		return;

	mutex_lock(&ishtp_cl_misc->cl_mutex);

	/*
	 * If this waitqueue is active, cl_mutex is locked by read(), it's safe
	 * to access ishtp_cl_misc and cl.
	 */
	if (waitqueue_active(&ishtp_cl_misc->read_wait)) {

		/*
		 * If already has read_rb, wake up waitqueue directly.
		 */
		if (ishtp_cl_misc->read_rb) {
			mutex_unlock(&ishtp_cl_misc->cl_mutex);
			wake_up_interruptible(&ishtp_cl_misc->read_wait);
			return;
		}

		cl = ishtp_cl_misc->cl;

		rb = ishtp_cl_rx_get_rb(cl);
		if (rb)
			ishtp_cl_misc->read_rb = rb;

		wake_up_interruptible(&ishtp_cl_misc->read_wait);
	}

	mutex_unlock(&ishtp_cl_misc->cl_mutex);
}

/**
 * ishtp_cl_reset_handler() - ISHTP client driver reset work handler
 * @work:		work struct
 *
 * This function gets called on reset workqueue scheduled when ISHFW
 * reset happen. It will disconnect and remove current ishtp_cl, then
 * create a new ishtp_cl and re-connect again.
 */
static void ishtp_cl_reset_handler(struct work_struct *work)
{
	struct ishtp_cl_miscdev *ishtp_cl_misc;
	struct ishtp_device *dev;
	struct ishtp_cl_device *cl_device;
	struct ishtp_cl *cl;
	struct ishtp_fw_client *fw_client;
	int err = 0;

	ishtp_cl_misc = container_of(work,
			struct ishtp_cl_miscdev, reset_work);

	dev = ishtp_cl_misc->cl_device->ishtp_dev;
	if (!dev) {
		dev_err(&ishtp_cl_misc->cl_device->dev,
			"This cl_device not link to ishtp_dev\n");
		return;
	}

	cl_device = ishtp_cl_misc->cl_device;

	mutex_lock(&ishtp_cl_misc->cl_mutex);

	/* Wake up from waiting if anyone wait on it */
	if (waitqueue_active(&ishtp_cl_misc->read_wait))
		wake_up_interruptible(&ishtp_cl_misc->read_wait);

	cl = ishtp_cl_misc->cl;
	if (cl) {
		ishtp_cl_unlink(cl);
		ishtp_cl_flush_queues(cl);
		ishtp_cl_free(cl);

		cl = NULL;

		cl = ishtp_cl_allocate(dev);
		if (!cl) {
			dev_err(&ishtp_cl_misc->cl_device->dev,
				"Allocate ishtp_cl failed\n");
			err = -ENOMEM;
			goto out_unlock;
		}

		if (dev->dev_state != ISHTP_DEV_ENABLED) {
			dev_err(&ishtp_cl_misc->cl_device->dev,
				"Ishtp dev isn't enabled\n");
			err = -ENODEV;
			goto out_free;
		}

		err = ishtp_cl_link(cl, ISHTP_HOST_CLIENT_ID_ANY);
		if (err) {
			dev_err(&ishtp_cl_misc->cl_device->dev,
				"Can not link to ishtp\n");
			goto out_free;
		}

		fw_client = ishtp_fw_cl_get_client(dev,
				&cl_device->fw_client->props.protocol_name);
		if (fw_client == NULL) {
			dev_err(&ishtp_cl_misc->cl_device->dev,
				"Don't find related fw client\n");
			err = -ENOENT;
			goto out_free;
		}

		cl->fw_client_id = fw_client->client_id;
		cl->state = ISHTP_CL_CONNECTING;

		err = ishtp_cl_connect(cl);
		if (err) {
			dev_err(&ishtp_cl_misc->cl_device->dev,
				"Connect to fw failed\n");
			goto out_free;
		}

		ishtp_cl_misc->cl = cl;
	}

	/* After reset, must register event callback again */
	ishtp_register_event_cb(cl_device, ishtp_cl_event_cb);

out_free:
	if (err) {
		ishtp_cl_free(cl);
		ishtp_cl_misc->cl = NULL;

		dev_err(&ishtp_cl_misc->cl_device->dev, "Reset failed\n");
	}

out_unlock:
	mutex_unlock(&ishtp_cl_misc->cl_mutex);
}

/**
 * ishtp_cl_probe() - ISHTP client driver probe
 * @cl_device:		ISHTP client device instance
 *
 * This function gets called on device create on ISHTP bus
 *
 * Return: 0 on success, non zero on error
 */
static int ishtp_cl_probe(struct ishtp_cl_device *cl_device)
{
	struct ishtp_cl_miscdev *ishtp_cl_misc;
	int ret;

	if (!cl_device)
		return -ENODEV;

	ishtp_cl_misc = kzalloc(sizeof(struct ishtp_cl_miscdev),
				GFP_KERNEL);
	if (!ishtp_cl_misc)
		return -ENOMEM;

	if (uuid_le_cmp(ishtp_smhi_guid,
			cl_device->fw_client->props.protocol_name) == 0)
		ishtp_cl_misc->cl_miscdev.name = "ish-smhi";
	else if (uuid_le_cmp(ishtp_trace_guid,
			cl_device->fw_client->props.protocol_name) == 0)
		ishtp_cl_misc->cl_miscdev.name = "ish-trace";
	else if (uuid_le_cmp(ishtp_traceconfig_guid,
			cl_device->fw_client->props.protocol_name) == 0)
		ishtp_cl_misc->cl_miscdev.name = "ish-tracec";
	else if (uuid_le_cmp(ishtp_loader_guid,
			cl_device->fw_client->props.protocol_name) == 0)
		ishtp_cl_misc->cl_miscdev.name = "ish-loader";
	else {
		dev_err(&cl_device->dev, "Not supported client\n");
		ret = -ENODEV;
		goto release_mem;
	}

	ishtp_cl_misc->cl_miscdev.parent = &cl_device->dev;
	ishtp_cl_misc->cl_miscdev.fops = &ishtp_cl_fops;
	ishtp_cl_misc->cl_miscdev.minor = MISC_DYNAMIC_MINOR,

	ret = misc_register(&ishtp_cl_misc->cl_miscdev);
	if (ret) {
		dev_err(&cl_device->dev, "misc device register failed\n");
		goto release_mem;
	}

	ishtp_cl_misc->cl_device = cl_device;

	init_waitqueue_head(&ishtp_cl_misc->read_wait);

	ishtp_set_drvdata(cl_device, (void *)ishtp_cl_misc);

	/* Register event callback */
	ishtp_register_event_cb(cl_device, ishtp_cl_event_cb);

	ishtp_get_device(cl_device);

	mutex_init(&ishtp_cl_misc->cl_mutex);

	INIT_WORK(&ishtp_cl_misc->reset_work, ishtp_cl_reset_handler);

	return 0;

release_mem:
	kfree(ishtp_cl_misc);

	return ret;
}

/**
 * ishtp_cl_remove() - ISHTP client driver remove
 * @cl_device:		ISHTP client device instance
 *
 * This function gets called on device remove on ISHTP bus
 *
 * Return: 0
 */
static int ishtp_cl_remove(struct ishtp_cl_device *cl_device)
{
	struct ishtp_cl_miscdev *ishtp_cl_misc;
	struct ishtp_cl *cl;

	ishtp_cl_misc = ishtp_get_drvdata(cl_device);
	if (!ishtp_cl_misc)
		return -ENODEV;

	if (ishtp_cl_misc->cl_miscdev.parent == NULL)
		return -ENODEV;

	/* Wake up from waiting if anyone wait on it */
	if (waitqueue_active(&ishtp_cl_misc->read_wait))
		wake_up_interruptible(&ishtp_cl_misc->read_wait);

	mutex_lock(&ishtp_cl_misc->cl_mutex);

	cl = ishtp_cl_misc->cl;
	if (cl) {
		cl->state = ISHTP_CL_DISCONNECTING;
		ishtp_cl_disconnect(cl);
		ishtp_cl_unlink(cl);
		ishtp_cl_flush_queues(cl);
		ishtp_cl_free(cl);
		ishtp_cl_misc->cl = NULL;
	}

	mutex_unlock(&ishtp_cl_misc->cl_mutex);

	mutex_destroy(&ishtp_cl_misc->cl_mutex);

	misc_deregister(&ishtp_cl_misc->cl_miscdev);

	dev_set_drvdata(&cl_device->dev, NULL);
	ishtp_put_device(cl_device);

	kfree(ishtp_cl_misc);

	return 0;
}

/**
 * ishtp_cl_reset() - ISHTP client driver reset
 * @cl_device:		ISHTP client device instance
 *
 * This function gets called on device reset on ISHTP bus.
 * If client is connected, needs to disconnect client and
 * reconnect again.
 *
 * Return: 0
 */
static int ishtp_cl_reset(struct ishtp_cl_device *cl_device)
{
	struct ishtp_cl_miscdev *ishtp_cl_misc;

	ishtp_cl_misc = ishtp_get_drvdata(cl_device);
	if (!ishtp_cl_misc) {
		dev_err(&cl_device->dev, "Client driver not ready yet\n");
		return -ENODEV;
	}

	schedule_work(&ishtp_cl_misc->reset_work);

	return 0;
}

static struct ishtp_cl_driver ishtp_cl_driver = {
	.name = "ishtp-client",
	.probe = ishtp_cl_probe,
	.remove = ishtp_cl_remove,
	.reset = ishtp_cl_reset,
};

static int __init ishtp_client_init(void)
{
	int ret;

	/* Register ISHTP client device driver with ISHTP Bus */
	ret = ishtp_cl_driver_register(&ishtp_cl_driver);

	return ret;
}

static void __exit ishtp_client_exit(void)
{
	ishtp_cl_driver_unregister(&ishtp_cl_driver);
}

late_initcall(ishtp_client_init);
module_exit(ishtp_client_exit);

MODULE_DESCRIPTION("ISH ISHTP client driver");
MODULE_AUTHOR("Even Xu <even.xu@intel.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("ishtp:*");
