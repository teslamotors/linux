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
 * Created on: Mar 20, 2014
 */

#include "cbc_device_manager.h"
#include "cbc_device.h"
#include "mux/cbc_mux_multiplexer.h"
#include "../cbc-core.h"
#include "link/cbc_kmod_load_monitoring.h"
#include "core/cbc_types.h"
#include "dev/cbc_memory.h"

#include <linux/device.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/list.h>


/* major number used for cbc core devices in /dev */
#ifndef CBC_CORE_MAJOR
#define CBC_CORE_MAJOR 42
#endif

/* minor start number.
 * This has to be 0 to allow a lookup of the device structs in an array.
 */
#define CBC_MINOR 0
/* Device-name/driver-name when registering the devices in the linux kernel.*/
#define DEVICE_NAME "cbc-core"

/* max number of open files per cbc channel */
#define MAX_OPEN_FILES 6

static void demuxed_receive(void *void_data,
			struct cbc_buffer *cbc_buffer);

/*
 * Prototypes
 */
static int cbc_device_open(struct inode *, struct file *);
static int cbc_device_release(struct inode *, struct file *);
static ssize_t cbc_device_read(struct file *, char *, size_t, loff_t *);
static ssize_t cbc_device_write(struct file *, const char *, size_t, loff_t *);
static unsigned int cbc_device_poll(struct file *file, poll_table *wait);
static long cbc_device_ioctl(struct file *, unsigned int, unsigned long);


const struct file_operations cbc_dev_file_operations = {
	.owner     = THIS_MODULE,
	.open      = cbc_device_open,
	.release   = cbc_device_release,
	.read      = cbc_device_read,
	.write     = cbc_device_write,
	.poll      = cbc_device_poll,
	.unlocked_ioctl = cbc_device_ioctl
};


struct cbc_device_manager {
	struct cdev cdev;
	struct cbc_device_data channels[e_cbc_channel_max_number];
	struct mutex send_lock;
	struct cbc_memory_pool *cbc_memory;
};


/* Currently, only one cbc per kernel supported.*/
struct cbc_device_manager cbc_device_mgr_configuration = {
	.channels[e_cbc_channel_pmt].device_type = e_cbc_device_type_hidden,
	.channels[e_cbc_channel_pmt].device_name = "cbc-pmt",

	.channels[e_cbc_channel_lifecycle].device_type = e_cbc_device_type_default,
	.channels[e_cbc_channel_lifecycle].device_name = "cbc-lifecycle",

	.channels[e_cbc_channel_signals].device_type = e_cbc_device_type_default,
	.channels[e_cbc_channel_signals].device_name = "cbc-signals",

	.channels[e_cbc_channel_early_signals].device_type = e_cbc_device_type_default,
	.channels[e_cbc_channel_early_signals].device_name = "cbc-early-signals",

	.channels[e_cbc_channel_diagnosis].device_type = e_cbc_device_type_default,
	.channels[e_cbc_channel_diagnosis].device_name = "cbc-diagnosis",

	.channels[e_cbc_channel_dlt].device_type = e_cbc_device_type_default,
	.channels[e_cbc_channel_dlt].device_name = "cbc-dlt",

	.channels[e_cbc_channel_linda].device_type = e_cbc_device_type_hidden,
	.channels[e_cbc_channel_linda].device_name = "cbc-linda",

	.channels[e_cbc_channel_oem_raw_channel_0].device_type = e_cbc_device_type_default,
	.channels[e_cbc_channel_oem_raw_channel_0].device_name = "cbc-raw0",

	.channels[e_cbc_channel_oem_raw_channel_1].device_type = e_cbc_device_type_default,
	.channels[e_cbc_channel_oem_raw_channel_1].device_name = "cbc-raw1",

	.channels[e_cbc_channel_oem_raw_channel_2].device_type = e_cbc_device_type_default,
	.channels[e_cbc_channel_oem_raw_channel_2].device_name = "cbc-raw2",

	.channels[e_cbc_channel_oem_raw_channel_3].device_type = e_cbc_device_type_default,
	.channels[e_cbc_channel_oem_raw_channel_3].device_name = "cbc-raw3",

	.channels[e_cbc_channel_oem_raw_channel_4].device_type = e_cbc_device_type_default,
	.channels[e_cbc_channel_oem_raw_channel_4].device_name = "cbc-raw4",

	.channels[e_cbc_channel_oem_raw_channel_5].device_type = e_cbc_device_type_default,
	.channels[e_cbc_channel_oem_raw_channel_5].device_name = "cbc-raw5",

	.channels[e_cbc_channel_oem_raw_channel_6].device_type = e_cbc_device_type_default,
	.channels[e_cbc_channel_oem_raw_channel_6].device_name = "cbc-raw6",

	.channels[e_cbc_channel_oem_raw_channel_7].device_type = e_cbc_device_type_default,
	.channels[e_cbc_channel_oem_raw_channel_7].device_name = "cbc-raw7",

	.channels[e_cbc_channel_oem_raw_channel_8].device_type = e_cbc_device_type_default,
	.channels[e_cbc_channel_oem_raw_channel_8].device_name = "cbc-raw8",

	.channels[e_cbc_channel_oem_raw_channel_9].device_type = e_cbc_device_type_default,
	.channels[e_cbc_channel_oem_raw_channel_9].device_name = "cbc-raw9",

	.channels[e_cbc_channel_oem_raw_channel_10].device_type = e_cbc_device_type_default,
	.channels[e_cbc_channel_oem_raw_channel_10].device_name = "cbc-raw10",

	.channels[e_cbc_channel_oem_raw_channel_11].device_type = e_cbc_device_type_default,
	.channels[e_cbc_channel_oem_raw_channel_11].device_name = "cbc-raw11",

	.channels[e_cbc_channel_debug_out].device_type = e_cbc_device_type_debug,
	.channels[e_cbc_channel_debug_out].device_name = "cbc-debug-out",

	.channels[e_cbc_channel_debug_in].device_type = e_cbc_device_type_debug,
	.channels[e_cbc_channel_debug_in].device_name = "cbc-debug-in",
};


struct cbc_mux_channel_configuration cbc_mux_config;



/*
 * device attribute priority. Every channel (entry in /dev) has a priority. This can be set/read
 * by a ioctl or in the sysfs.
 */
static ssize_t cbc_dev_attr_priority_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct cbc_device_data *chn_data =
		(struct cbc_device_data *)dev_get_drvdata(dev);
	int idx = chn_data - &cbc_device_mgr_configuration.channels[0];
	int prio = cbc_mux_multiplexer_get_priority(idx);

	pr_info("cbc read priority %i for channel: %i\n", prio, idx);
	return scnprintf(buf, PAGE_SIZE, "%i\n", prio);
}


static ssize_t cbc_dev_attr_priority_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t count)
{
	struct cbc_device_data *chn_data = (struct cbc_device_data *)dev_get_drvdata(dev);
	int idx = chn_data - &cbc_device_mgr_configuration.channels[0];
	u8 tmp = 0;
	int res = kstrtou8(buf, 0, &tmp);

	if ((res == 0) && (tmp < 8)) {
		pr_info("cbc write priority %i to channel %i\n", tmp, idx);
		cbc_mux_multiplexer_set_priority(idx, tmp);
	}
	return count;
}


static DEVICE_ATTR(priority,
				   0664,
				   cbc_dev_attr_priority_show,
				   cbc_dev_attr_priority_store);


/*
 * Entry point for showing cbc device "flagfield" class attribute.
 */
static ssize_t cbc_cls_attr_flagfield_show(struct class *cls,
		struct class_attribute *attr,
		char *buf)
{
	(void)cls;
	(void)attr;
	return scnprintf(buf,
					 PAGE_SIZE, "0x%x\n",
					 cbc_mux_multiplexer_get_flagfield());
}


static ssize_t cbc_cls_attr_flagfield_store(struct class *cls,
		struct class_attribute *attr,
		const char *buf,
		size_t count)
{
	u32 tmp = 0;
	int res = kstrtouint(buf, 0, &tmp);
	(void)cls;
	(void)attr;
	if (res == 0) {
		cbc_mux_multiplexer_set_flagfield(tmp);
		pr_info("cbc_attr_flagfield_store\n");
	}
	return count;
}


struct class_attribute cbc_cls_attr_flagfield =
	__ATTR(flagfield, 0664, cbc_cls_attr_flagfield_show,
		   cbc_cls_attr_flagfield_store);


static int cbc_device_open(struct inode *inode, struct file *file)
{
	struct cbc_device_data *device_data =
		&cbc_device_mgr_configuration.channels[MINOR(inode->i_rdev)];
	int ret = 0;
	u32 num_open_files = 0;
	struct cbc_file_data *file_data = kmalloc(sizeof(struct cbc_file_data), GFP_KERNEL);

	if (!device_data)
		ret = -EIO;

	if (!file_data)
		ret = -ENOMEM;

	if (ret == 0) {
		pr_info("cbc_core device_open: %d.%d %s\n",
				MAJOR(inode->i_rdev),
				MINOR(inode->i_rdev),
				device_data->device_name);

		if (MINOR(inode->i_rdev) >= e_cbc_channel_max_number) {
			pr_err("Invalid cbc channel number\n");
			ret = -ENODEV;
		}
	}

	if (ret == 0) {
		struct list_head *tmp;

		list_for_each(tmp, &device_data->open_files_head)
			num_open_files++;

		if (num_open_files > MAX_OPEN_FILES)
			ret = -EBUSY;
	}

	if (0 == ret) {
		cbc_file_init(file_data);
		file_data->cbc_device = device_data;
		list_add(&file_data->list, &device_data->open_files_head);
		file->private_data = file_data;
	} else {
		kfree(file_data);
	}


	return ret;
}


static int cbc_device_release(struct inode *inode, struct file *file)
{
	u32 dev_idx = MINOR(inode->i_rdev);
	struct cbc_file_data *file_data = file->private_data;

	if (file_data) {
		list_del(&file_data->list);

		pr_info("device_release: %d.%d %s\n",
				MAJOR(inode->i_rdev),
				dev_idx,
				file_data->cbc_device->device_name);

		while (!cbc_file_queue_empty(file_data))
			cbc_buffer_release(cbc_file_dequeue(file_data));

		kfree(file_data);
		file->private_data = 0;
	}
	return 0;
}


static ssize_t cbc_device_read(struct file *file,
							   char *user_buffer,
							   size_t length,
							   loff_t *offset)
{
	struct cbc_file_data *f = (struct cbc_file_data *)file->private_data;
	s32 ret = 0;

	if (!f)
		ret = -EIO;

	if (0 == ret) {
		while (cbc_file_queue_empty(f) && (0 == ret)) {
			if (file->f_flags & O_NONBLOCK) {
				ret = -EAGAIN;
				return ret;
			}
			ret = wait_event_interruptible(f->wq_read,
										   !(cbc_file_queue_empty(f)));
			if ((ret != 0) && (ret != -ERESTARTSYS)) {
				/* ERESTARTSYS happens when a file is polled while shutting down the ldisc. */
				/* This is not an error. */
				pr_err(" fifo_read: woke up with error %d\n", ret);
				ret = -EIO;
			}
		}
	}

	if (0 == ret) {
		if (!cbc_file_queue_empty(f)) {
			struct cbc_buffer *cbc_buffer;

			cbc_buffer = cbc_file_dequeue(f);

			if (cbc_buffer) {
				u32 offset = CBC_HEADER_SIZE;
				u16 data_length = cbc_buffer->payload_length;

				if (f->cbc_device->device_type == e_cbc_device_type_raw) {
					offset = CBC_HEADER_SIZE + CBC_RAWHEADER_SIZE;
					data_length = data_length - CBC_RAWHEADER_SIZE;
				} else if (f->cbc_device->device_type == e_cbc_device_type_debug) {
					offset = 0;
					data_length = cbc_buffer->frame_length;
				}

				if (data_length <= length) {
					ret = copy_to_user(user_buffer, &cbc_buffer->data[offset], data_length);
					if (ret == 0) {
						ret = data_length;
					} else {
						pr_err("device_read %u bytes copy to user failed\n",
								data_length);
					}
				} else {
					pr_err("device_read, buffer too small for %u bytes.\n",
							data_length);
					ret = -EINVAL;
				}
				cbc_buffer_release(cbc_buffer);
			} else {
				pr_err("dequeued a null-buffer\n");
			}

		} else {
			pr_err("queue empty after wake waitq\n");
		}

	}
	return ret;
}


static ssize_t cbc_device_write(struct file *file,
								const char *user_buffer,
								size_t length,
								loff_t *offset)
{
	int n = 0;
	struct cbc_file_data *file_data = (struct cbc_file_data *)file->private_data;
	struct cbc_device_data *chn_data = file_data->cbc_device;

	struct cbc_buffer *cbc_buffer = cbc_memory_pool_get_buffer(cbc_device_mgr_configuration.cbc_memory);
	int ret = 0;
	u32 payload_offset = CBC_HEADER_SIZE;
	u32 additional_header_size = 0;

	u32 tmp = (u32)length;

	if (!cbc_buffer) {
		pr_err("cbc out of memory\n");
		ret = -ENOMEM;
	}

	if (0 == ret) {
		if (chn_data == NULL) {
			pr_err("chan_data is NULL\n");
			ret = -EINVAL;
		}
	}

	if (0 == ret) {
		if (chn_data->device_type == e_cbc_device_type_raw) {
			payload_offset = CBC_HEADER_SIZE + CBC_RAWHEADER_SIZE;
			additional_header_size = CBC_RAWHEADER_SIZE;
		} else if (chn_data->device_type == e_cbc_device_type_debug) {
			ret = -EINVAL; /* debug channels do not support write */
		}
	}


	if (length + payload_offset > CBC_BUFFER_SIZE) {
		pr_err("device_write %u bytes not possible\n", tmp);
		ret = -EINVAL;
	}

	if (0 == ret) {
		if (user_buffer == NULL) {
			pr_err("device_write buffer is NULL\n");
			ret = -EINVAL;
		}
	}

	if (0 == ret) {
		if (0 == copy_from_user(&cbc_buffer->data[payload_offset], user_buffer, length)) {
			int idx = chn_data - &cbc_device_mgr_configuration.channels[0];

			n = length;
			cbc_buffer->payload_length = length + additional_header_size;
			cbc_manager_transmit_buffer(idx, cbc_buffer);
		}
	}

	cbc_buffer_release(cbc_buffer);

	if (0 == ret)
		ret = n;
	return ret;
}


void cbc_mux_configure_data_channel(u32 const channel_idx
		, const u8 priority
		, void *data
		, void(*receive)(void *data
				, const u16 length
				, const u8 * const buffer))
{
	if (channel_idx < e_cbc_channel_max_number) {
		cbc_mux_config.cbc_mux_channel_list[channel_idx].data = data;
		cbc_mux_config.cbc_mux_channel_list[channel_idx].priority = priority;
		cbc_mux_config.cbc_mux_channel_list[channel_idx].buffer_receive = 0;
		cbc_mux_config.cbc_mux_channel_list[channel_idx].data_receive = receive;
	}
}


int cbc_register_devices(struct class *cbc_class, struct cbc_memory_pool *memory)
{
	int ret = 0;
	int i;
	struct cbc_device_manager *cbc = &cbc_device_mgr_configuration;

	cbc->cbc_memory = memory;

	/* set up the devices after the line discipline is opened */
	ret = register_chrdev_region(MKDEV(CBC_CORE_MAJOR, CBC_MINOR),
								 e_cbc_channel_max_number, DEVICE_NAME);

	if (ret < 0)
		pr_err("cbc-core ldisc open register chrdev region failed\n");

	if (0 == ret) {
		cdev_init(&cbc->cdev, &cbc_dev_file_operations);
		cbc->cdev.owner = THIS_MODULE;
		cbc->cdev.ops = &cbc_dev_file_operations;
		ret = cdev_add(&cbc->cdev, MKDEV(CBC_CORE_MAJOR, CBC_MINOR),
					   e_cbc_channel_max_number);
		if (ret < 0) {
			unregister_chrdev_region(MKDEV(CBC_CORE_MAJOR, CBC_MINOR),
									 e_cbc_channel_max_number);
			pr_err("cbc-core ldisc open add cdev failed\n");
		}
	}

	if (0 == ret) {
		for (i = 0; i < e_cbc_channel_max_number; i++) {
			cbc_device_init(&cbc->channels[i]);

			if (cbc->channels[i].device_type != e_cbc_device_type_hidden) {
				/* Create the devices
				   These will appear in /sys/class/cbc and,
				   if udev is running, /dev */
				cbc->channels[i].device =
					device_create(cbc_class,
								  NULL,
								  MKDEV(CBC_CORE_MAJOR, i),
								  NULL,
								  cbc->channels[i].device_name,
								  i);

				/* Add the attribute */
				ret = device_create_file(cbc->channels[i].device, &dev_attr_priority);

				/* Set private data to point to the fifo */
				dev_set_drvdata(cbc->channels[i].device, &cbc->channels[i]);
			} else {
				cbc->channels[i].device = NULL;
			}
		}
	}

	if (0 == ret)
		ret = class_create_file(cbc_class, &cbc_cls_attr_flagfield);

	if (0 == ret)
		ret = cbc_kmod_load_monitoring_init(cbc_class);

	if (ret != 0)
		cbc_unregister_devices(cbc_class);

	return ret;
}


void cbc_unregister_devices(struct class *cbc_class)
{
	int i;
	struct cbc_device_manager *cbc = &cbc_device_mgr_configuration;

	/* remove the /dev devices */
	cdev_del(&cbc->cdev);

	for (i = 0; i < e_cbc_channel_max_number; i++)	{
		if (cbc->channels[i].device != NULL) {
			device_remove_file(cbc->channels[i].device, &dev_attr_priority);
			device_destroy(cbc_class, MKDEV(CBC_CORE_MAJOR, i));
		}
	}

	/* class destroy also destroys all class attribute files,
	because they are ref counted*/
	unregister_chrdev_region(MKDEV(CBC_CORE_MAJOR, CBC_MINOR),
							 e_cbc_channel_max_number);

	cbc->cbc_memory = NULL;
}


void cbc_manager_transmit_data(const u32 channel_idx,
						   const u16 length,
						   const u8 * const buffer)
{
	struct cbc_buffer *cbc_buffer =
			cbc_memory_pool_get_buffer(cbc_device_mgr_configuration.cbc_memory);
	struct cbc_device_data *chn_data;
	u32 offset = CBC_HEADER_SIZE;
	u32 copy_length = length;

	if (channel_idx >= e_cbc_channel_max_number) {
		pr_err("cbc_mux_transmit_data(): Invalid cbc channel idx.\n");
		return;
	}

	chn_data = &cbc_device_mgr_configuration.channels[channel_idx];

	if (!cbc_buffer)
		return;

	if (chn_data->device_type == e_cbc_device_type_raw)
		offset = CBC_HEADER_SIZE + CBC_RAWHEADER_SIZE;

	if (length + offset > CBC_MAX_TOTAL_FRAME_SIZE)
		copy_length = CBC_MAX_TOTAL_FRAME_SIZE - offset;

	memcpy(&cbc_buffer->data[offset], buffer, copy_length);

	cbc_manager_transmit_buffer(channel_idx, cbc_buffer);
	cbc_buffer_release(cbc_buffer);
}


void cbc_manager_transmit_buffer(const u32 channel_idx,
					struct cbc_buffer *buffer)
{
	if (channel_idx >= e_cbc_channel_max_number) {
		pr_err("cbc_mux_transmit_data(): Invalid cbc channel idx.\n");
	} else {
		enum cbc_error res = e_cbc_error_ok;
		struct cbc_device_data *chn_data =
					&cbc_device_mgr_configuration.channels[channel_idx];

		mutex_lock(&cbc_device_mgr_configuration.send_lock);

		if (chn_data->device_type == e_cbc_device_type_raw) {
			/* room for raw header is already reserved in buffer. */
			/* calculate raw header data length without raw header */
			u32 real_payload_size = buffer->payload_length - CBC_RAWHEADER_SIZE;

			buffer->data[CBC_HEADER_SIZE] = e_cbc_raw_channel_direct_transport;
			buffer->data[CBC_HEADER_SIZE + 1] = (u8) (real_payload_size & 0xFF);
			buffer->data[CBC_HEADER_SIZE + 2] = (u8) ((real_payload_size >> 8) & 0xFFU);
		}

		res = cbc_mux_multiplexer_transmit_buffer(channel_idx,
				buffer);
		mutex_unlock(&cbc_device_mgr_configuration.send_lock);
		if (e_cbc_error_ok != res)
			pr_err("Error transmitting frame %u.\n", res);
	}
	/* buffer is released in the calling cbc_device_write() */
}


static unsigned int cbc_device_poll(struct file *file, poll_table *wait)
{
	struct cbc_file_data *f = (struct cbc_file_data *)file->private_data;
	unsigned int mask = 0;

	poll_wait(file, &f->wq_read, wait);

	if (!cbc_file_queue_empty(f))
		mask |= (POLLIN | POLLRDNORM);
	if (!cbc_file_queue_full(f))
		mask |= (POLLOUT | POLLWRNORM);

	return mask;
}


long cbc_device_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int tmp;
	struct cbc_file_data *file_data = (struct cbc_file_data *)file->private_data;
	struct cbc_device_data *chn_data = file_data->cbc_device;

	int idx = chn_data - &cbc_device_mgr_configuration.channels[0];

	switch (cmd) {
	case CBC_FLAG_GET:
		tmp = cbc_mux_multiplexer_get_flagfield();
		if (copy_to_user((void __user *)arg, &tmp, sizeof(tmp)))
			return -EFAULT;
		return 0;

	case CBC_FLAG_SET:
		if (copy_from_user(&tmp, (void __user *)arg, sizeof(tmp)))
			return -EFAULT;
		cbc_mux_multiplexer_set_flagfield(tmp);
		return 0;

	case CBC_PRIORITY_GET:
		tmp = cbc_mux_multiplexer_get_priority(idx);
		if (copy_to_user((void __user *)arg, &tmp, sizeof(tmp)))
			return -EFAULT;
		return 0;

	case CBC_PRIORITY_SET:
		if (copy_from_user(&tmp, (void __user *)arg, sizeof(tmp)))
			return -EFAULT;
		cbc_mux_multiplexer_set_priority(idx, tmp);
		return 0;

	default:
		return -EINVAL;
	}

}


static u8 get_default_priority(enum cbc_channel_enumeration channel_id)
{
	u8 result = 1;

	switch (channel_id) {
	case e_cbc_channel_pmt:
	case e_cbc_channel_lifecycle:
	case e_cbc_channel_dlt:
	case e_cbc_channel_linda:
		result = 6;
		break;

	case e_cbc_channel_diagnosis:
		result = 2;
		break;

	default:
		result = 3;
		break;
	} /* switch */
	return result;
} /* get_default_priority() */


static void demuxed_receive(void *void_data,
		struct cbc_buffer *cbc_buffer)
{
	struct cbc_device_data *device_data = (struct cbc_device_data *)void_data;
	struct list_head *current_item;
	struct cbc_file_data *current_file_data;

	if (device_data && cbc_buffer
			&& cbc_buffer->frame_length > CBC_HEADER_SIZE + CBC_CHECKSUM_SIZE) {
		/* payload_length includes raw_header */
		u16 payload_length = cbc_buffer->frame_length - (CBC_HEADER_SIZE + CBC_CHECKSUM_SIZE);

		if (device_data->device_type == e_cbc_device_type_raw) {
			if (cbc_buffer->frame_length
				> (CBC_HEADER_SIZE + CBC_RAWHEADER_SIZE + CBC_CHECKSUM_SIZE)) {
				u16 raw_length;

				raw_length = cbc_buffer->data[4];
				raw_length |= (cbc_buffer->data[5] << 8);

				if (raw_length + CBC_RAWHEADER_SIZE  > payload_length) {
					pr_err("raw length (%i) is longer than payload length (%i)\n",
							raw_length,
							payload_length);
					/* payload_length already set to max value */
				} else {
					payload_length = raw_length + CBC_RAWHEADER_SIZE;
				}
			} else {
				pr_err("Frame to short for a raw frame\n");
			}
			cbc_buffer->payload_length = payload_length;
		} else if (device_data->device_type == e_cbc_device_type_default) {
			cbc_buffer->payload_length = payload_length;
		}
		/* else, do not touch payload_length in a debug-channel */

		/* enqueue */
		for (current_item = device_data->open_files_head.next
				; current_item != &device_data->open_files_head
				; current_item = current_item->next) {

			current_file_data = list_entry(current_item,
					struct cbc_file_data,
					list);
			/* file_enqueue increases ref */
			cbc_file_enqueue(current_file_data, cbc_buffer);
		}
	} else {
		pr_err("(<- IOC) dev_receive data is null\n");
	}
}


void cbc_kmod_devices_init(void)
{
	/*
	 * set up the multiplexer configuration /
	 * channellist and configure the multiplexer with this.
	 */
	u32 i = 0;

	for (; i < e_cbc_channel_max_number; i++) {
		cbc_mux_config.cbc_mux_channel_list[i].buffer_receive = demuxed_receive;
		cbc_mux_config.cbc_mux_channel_list[i].data_receive = 0;
		cbc_mux_config.cbc_mux_channel_list[i].data = &cbc_device_mgr_configuration.channels[i];
		cbc_mux_config.cbc_mux_channel_list[i].priority = get_default_priority(i);
	}
	cbc_mux_config.flags = 0;

	cbc_mux_multiplexer_setup(&cbc_mux_config);

	mutex_init(&cbc_device_mgr_configuration.send_lock);
}
