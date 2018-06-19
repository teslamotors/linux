// SPDX-License-Identifier: GPL-2.0
/*
 * CBC line discipline kernel module.
 * Handles Carrier Board Communications (CBC) protocol.
 *
 * Copyright (C) 2018 Intel Corporation
 */

#include <linux/device.h>
#include <linux/list.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/slab.h>

#include "cbc_core.h"
#include "cbc_device.h"
#include "cbc_device_manager.h"
#include "cbc_memory.h"
#include "cbc_mux_multiplexer.h"
#include "cbc_types.h"

static int major; /* Default to dynamic major */
module_param(major, int, 0);
MODULE_PARM_DESC(major, "Major device number");

/*
 * Minor start number.
 * This has to be 0 to allow a lookup of the device structs in an array.
 */
#define CBC_MINOR 0
/* Device-name/driver-name when registering the devices in the linux kernel.*/
#define DEVICE_NAME "cbc-core"

/* Max number of open files per cbc channel */
#define MAX_OPEN_FILES 6

static void demuxed_receive(void *void_data, struct cbc_buffer *cbc_buffer);

static int cbc_device_open(struct inode *, struct file *);
static int cbc_device_release(struct inode *, struct file *);
static ssize_t cbc_device_read(struct file *, char __user *, size_t,
							  loff_t *);
static ssize_t cbc_device_write(struct file *, const char __user *, size_t,
								loff_t *);
static unsigned int cbc_device_poll(struct file *file, poll_table *wait);
static long cbc_device_ioctl(struct file *, unsigned int, unsigned long);

static const struct file_operations cbc_dev_file_operations = {
	.owner = THIS_MODULE,
	.open = cbc_device_open,
	.release = cbc_device_release,
	.read = cbc_device_read,
	.write = cbc_device_write,
	.poll = cbc_device_poll,
	.unlocked_ioctl = cbc_device_ioctl
};

struct cbc_device_manager {
	struct cdev cdev;
	struct cbc_device_data channels[CBC_CHANNEL_MAX_NUMBER];
	struct mutex send_lock;
	struct cbc_memory_pool *cbc_memory;
};

/* Currently, only one CBC per kernel supported.*/
static struct cbc_device_manager cbc_device_mgr_configuration = {
	.channels[CBC_CHANNEL_PMT].device_type =
				CBC_DEVICE_TYPE_HIDDEN,
	.channels[CBC_CHANNEL_PMT].device_name =
				"cbc-pmt",

	.channels[CBC_CHANNEL_LIFECYCLE].device_type =
				CBC_DEVICE_TYPE_DEFAULT,
	.channels[CBC_CHANNEL_LIFECYCLE].device_name =
				"cbc-lifecycle",

	.channels[CBC_CHANNEL_SIGNALS].device_type =
				CBC_DEVICE_TYPE_DEFAULT,
	.channels[CBC_CHANNEL_SIGNALS].device_name =
				"cbc-signals",

	.channels[CBC_CHANNEL_EARLY_SIGNALS].device_type =
				CBC_DEVICE_TYPE_DEFAULT,
	.channels[CBC_CHANNEL_EARLY_SIGNALS].device_name =
				"cbc-early-signals",

	.channels[CBC_CHANNEL_DIAGNOSIS].device_type =
				CBC_DEVICE_TYPE_DEFAULT,
	.channels[CBC_CHANNEL_DIAGNOSIS].device_name =
				"cbc-diagnosis",

	.channels[CBC_CHANNEL_DLT].device_type =
				CBC_DEVICE_TYPE_DEFAULT,
	.channels[CBC_CHANNEL_DLT].device_name =
				"cbc-dlt",

	.channels[CBC_CHANNEL_LINDA].device_type =
				CBC_DEVICE_TYPE_HIDDEN,
	.channels[CBC_CHANNEL_LINDA].device_name =
				"cbc-linda",

	.channels[CBC_CHANNEL_OEM_RAW_CHANNEL_0].device_type =
				CBC_DEVICE_TYPE_DEFAULT,
	.channels[CBC_CHANNEL_OEM_RAW_CHANNEL_0].device_name =
				"cbc-raw0",

	.channels[CBC_CHANNEL_OEM_RAW_CHANNEL_1].device_type =
				CBC_DEVICE_TYPE_DEFAULT,
	.channels[CBC_CHANNEL_OEM_RAW_CHANNEL_1].device_name =
				"cbc-raw1",

	.channels[CBC_CHANNEL_OEM_RAW_CHANNEL_2].device_type =
				CBC_DEVICE_TYPE_DEFAULT,
	.channels[CBC_CHANNEL_OEM_RAW_CHANNEL_2].device_name =
				"cbc-raw2",

	.channels[CBC_CHANNEL_OEM_RAW_CHANNEL_3].device_type =
				CBC_DEVICE_TYPE_DEFAULT,
	.channels[CBC_CHANNEL_OEM_RAW_CHANNEL_3].device_name =
				"cbc-raw3",

	.channels[CBC_CHANNEL_OEM_RAW_CHANNEL_4].device_type =
				CBC_DEVICE_TYPE_DEFAULT,
	.channels[CBC_CHANNEL_OEM_RAW_CHANNEL_4].device_name =
				"cbc-raw4",

	.channels[CBC_CHANNEL_OEM_RAW_CHANNEL_5].device_type =
				CBC_DEVICE_TYPE_DEFAULT,
	.channels[CBC_CHANNEL_OEM_RAW_CHANNEL_5].device_name =
				"cbc-raw5",

	.channels[CBC_CHANNEL_OEM_RAW_CHANNEL_6].device_type =
				CBC_DEVICE_TYPE_DEFAULT,
	.channels[CBC_CHANNEL_OEM_RAW_CHANNEL_6].device_name =
				"cbc-raw6",

	.channels[CBC_CHANNEL_OEM_RAW_CHANNEL_7].device_type =
				CBC_DEVICE_TYPE_DEFAULT,
	.channels[CBC_CHANNEL_OEM_RAW_CHANNEL_7].device_name =
				"cbc-raw7",

	.channels[CBC_CHANNEL_OEM_RAW_CHANNEL_8].device_type =
				CBC_DEVICE_TYPE_DEFAULT,
	.channels[CBC_CHANNEL_OEM_RAW_CHANNEL_8].device_name =
				"cbc-raw8",

	.channels[CBC_CHANNEL_OEM_RAW_CHANNEL_9].device_type =
				CBC_DEVICE_TYPE_DEFAULT,
	.channels[CBC_CHANNEL_OEM_RAW_CHANNEL_9].device_name =
				"cbc-raw9",

	.channels[CBC_CHANNEL_OEM_RAW_CHANNEL_10].device_type =
				CBC_DEVICE_TYPE_DEFAULT,
	.channels[CBC_CHANNEL_OEM_RAW_CHANNEL_10].device_name =
				"cbc-raw10",

	.channels[CBC_CHANNEL_OEM_RAW_CHANNEL_11].device_type =
				CBC_DEVICE_TYPE_DEFAULT,
	.channels[CBC_CHANNEL_OEM_RAW_CHANNEL_11].device_name =
				"cbc-raw11",

	.channels[CBC_CHANNEL_DEBUG_OUT].device_type =
				CBC_DEVICE_TYPE_DEBUG,
	.channels[CBC_CHANNEL_DEBUG_OUT].device_name =
				"cbc-debug-out",

	.channels[CBC_CHANNEL_DEBUG_IN].device_type =
				CBC_DEVICE_TYPE_DEBUG,
	.channels[CBC_CHANNEL_DEBUG_IN].device_name =
				"cbc-debug-in",
};

static struct cbc_mux_channel_configuration cbc_mux_config;

/*
 * priority_show - Retrieve device attribute priority.
 * @dev: device (i.e /dev/cbc*)
 * @attr: Priority attribute
 * @buf:  Buffer to write to.
 *
 * Every channel (entry in /dev) has a priority.
 * This can be set/read by a ioctl or in the sysfs.
 */
static ssize_t priority_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct cbc_device_data *chn_data =
			(struct cbc_device_data *) dev_get_drvdata(dev);
	int idx = chn_data - &cbc_device_mgr_configuration.channels[0];
	int prio = cbc_mux_multiplexer_get_priority(idx);

	pr_debug("cbc-core: read priority %i for channel: %i\n", prio, idx);
	return scnprintf(buf, PAGE_SIZE, "%i\n", prio);
}

/*
 * priority_store - Store device attribute priority.
 * @dev: device (i.e /dev/cbc*)
 * @attr: Priority attribute
 * @buf:  Buffer to write to.
 *
 * Every channel (entry in /dev) has a priority.
 * This can be set/read by a ioctl or in the sysfs.
 */
static ssize_t priority_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct cbc_device_data *chn_data =
		(struct cbc_device_data *) dev_get_drvdata(dev);
	int idx = chn_data - &cbc_device_mgr_configuration.channels[0];
	u8 tmp = 0;
	int res = kstrtou8(buf, 0, &tmp);

	if ((res == 0) && (tmp < 8)) {
		pr_debug("cbc-core: write priority %i to channel %i\n", tmp,
									idx);
		cbc_mux_multiplexer_set_priority(idx, tmp);
	}
	return count;
}
static DEVICE_ATTR_RW(priority);

/*
 * cbc_device_open - Open CBC char device. Implementation of .open.
 * @inode:Pointer to inode object for this device.
 * @file: Pointer to file object for this device.
 *
 * Return 0 if device opened successfully, Linux error code otherwise.
 */
static int cbc_device_open(struct inode *inode, struct file *file)
{
	struct cbc_device_data *device_data =
			&cbc_device_mgr_configuration.channels[MINOR(
					inode->i_rdev)];
	int ret = 0;
	u32 num_open_files = 0;
	struct cbc_file_data *file_data = kmalloc(sizeof(struct cbc_file_data),
								GFP_KERNEL);

	if (!device_data)
		ret = -EIO;

	if (!file_data)
		ret = -ENOMEM;

	if (ret == 0) {
		pr_debug("cbc_core: device_open: %d.%d %s\n",
				MAJOR(inode->i_rdev), MINOR(inode->i_rdev),
				device_data->device_name);

		if (MINOR(inode->i_rdev) >= CBC_CHANNEL_MAX_NUMBER) {
			pr_err("cbc-core: invalid cbc channel number.\n");
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

	if (ret == 0) {
		cbc_file_init(file_data);
		file_data->cbc_device = device_data;
		list_add(&file_data->list, &device_data->open_files_head);
		file->private_data = file_data;
	} else {
		kfree(file_data);
	}

	return ret;
}

/*
 * cbc_device_release - Release char device. Implementation of .release
 * @inode:Pointer to inode object for this device.
 * @file: Pointer to file object for this device.
 */
static int cbc_device_release(struct inode *inode, struct file *file)
{
	u32 dev_idx = MINOR(inode->i_rdev);
	struct cbc_file_data *file_data = file->private_data;

	if (file_data) {
		list_del(&file_data->list);

		pr_debug("cbc-core: device_release: %d.%d %s\n",
			MAJOR(inode->i_rdev), dev_idx,
			file_data->cbc_device->device_name);

		while (!cbc_file_queue_empty(file_data))
			cbc_buffer_release(cbc_file_dequeue(file_data));

		kfree(file_data);
		file->private_data = NULL;
	}
	return 0;
}

/*
 * cbc_device_read - CBC device read. Implementation of .read.
 * @file: Pointer to file object for this device.
 * @user_buffer: Pointer to buffer containing data to be read.
 * @length:Length of buffer.
 * @offset: Offset into buffer.
 */
static ssize_t cbc_device_read(struct file *file, char __user *user_buffer,
						size_t length, loff_t *offset)
{
	struct cbc_file_data *f = (struct cbc_file_data *) file->private_data;
	s32 ret = 0;

	if (!f)
		ret = -EIO;

	if (ret == 0) {
		while (cbc_file_queue_empty(f) && (ret == 0)) {
			if (file->f_flags & O_NONBLOCK) {
				ret = -EAGAIN;
				return ret;
			}
			ret = wait_event_interruptible(f->wq_read,
					!(cbc_file_queue_empty(f)));
			if ((ret != 0) && (ret != -ERESTARTSYS)) {
				/*
				 * ERESTARTSYS happens when a file is polled
				 * while shutting down the ldisc.
				 * This is not an error.
				 */
				pr_err("cbc-core:  fifo_read: woke up with error %d.\n",
					ret);
				ret = -EIO;
			}
		}
	}

	if (ret == 0) {
		if (!cbc_file_queue_empty(f)) {
			struct cbc_buffer *cbc_buffer;

			cbc_buffer = cbc_file_dequeue(f);

			if (cbc_buffer) {
				u32 offset = CBC_HEADER_SIZE;
				u16 data_length = cbc_buffer->payload_length;

				if (f->cbc_device->device_type ==
						CBC_DEVICE_TYPE_RAW) {
					offset = CBC_HEADER_SIZE +
						CBC_RAWHEADER_SIZE;
					data_length = data_length -
						CBC_RAWHEADER_SIZE;
				} else if (f->cbc_device->device_type ==
						CBC_DEVICE_TYPE_DEBUG) {
					offset = 0;
					data_length = cbc_buffer->frame_length;
				}

				if (data_length <= length) {
					ret =  copy_to_user(
						(void __user *) user_buffer,
						&cbc_buffer->data[offset],
						data_length);
					if (ret == 0) {
						ret = data_length;
					} else {
						pr_err(
						"cbc-core: device_read %u bytes copy to user failed.\n",
						data_length);
					}
				} else {
					pr_err(
						"cbc-core: device_read, buffer too small for %u bytes.\n",
						data_length);
					ret = -EINVAL;
				}
				cbc_buffer_release(cbc_buffer);
			} else {
				pr_err("cbc-core: dequeued a null-buffer.\n");
			}

		} else {
			pr_err("cbc-core: queue empty after response to wait.\n");
		}

	}
	return ret;
}

/*
 * cbc_device_write - Write data to char device. Implementation of .write.
 * @file: Pointer to file object for this device.
 * @user_buffer: Pointer to buffer containing data to be read.
 * @length:Length of buffer.
 * @offset: Offset into buffer.
 */
static ssize_t cbc_device_write(struct file *file,
		const char __user *user_buffer, size_t length, loff_t *offset)
{
	int n = 0;
	struct cbc_file_data *file_data =
			(struct cbc_file_data *) file->private_data;
	struct cbc_device_data *chn_data = file_data->cbc_device;

	struct cbc_buffer *cbc_buffer = cbc_memory_pool_get_buffer(
				cbc_device_mgr_configuration.cbc_memory);
	int ret = 0;
	u32 payload_offset = CBC_HEADER_SIZE;
	u32 additional_header_size = 0;

	u32 tmp = (u32) length;

	if (!cbc_buffer) {
		pr_err("cbc-core: Out of memory.\n");
		ret = -ENOMEM;
	}

	if (ret == 0) {
		if (chn_data == NULL) {
			pr_err("cbc-core: Channel data is NULL.\n");
			ret = -EINVAL;
		}
	}

	if (ret == 0) {
		if (chn_data->device_type == CBC_DEVICE_TYPE_RAW) {
			payload_offset = CBC_HEADER_SIZE + CBC_RAWHEADER_SIZE;
			additional_header_size = CBC_RAWHEADER_SIZE;
		} else if (chn_data->device_type == CBC_DEVICE_TYPE_DEBUG) {
			ret = -EINVAL; /* debug channels do not support write */
		}
	}

	if (length + payload_offset > CBC_BUFFER_SIZE) {
		pr_err(
		"cbc-core: Device_write %u bytes not possible,maximum buffer size exceeded.\n",
		 tmp);
		ret = -EINVAL;
	}

	if (ret == 0) {
		if (user_buffer == NULL) {
			pr_err("cbc-core: Device_write buffer is NULL.\n");
			ret = -EINVAL;
		}
	}

	if (ret == 0) {
		if (copy_from_user(&cbc_buffer->data[payload_offset],
				(void __user *) user_buffer, length) == 0) {
			int idx = chn_data -
				&cbc_device_mgr_configuration.channels[0];

			n = length;
			cbc_buffer->payload_length = length +
						additional_header_size;
			cbc_manager_transmit_buffer(idx, cbc_buffer);
		}
	}

	cbc_buffer_release(cbc_buffer);

	if (ret == 0)
		ret = n;
	return ret;
}

/*
 * cbc_mux_configure_data_channel - Configure the specified channel.
 * @channel_idx: Channel identifier.
 * @data: Data associated with this channel.
 * @receive: Data receive function for this channel.
 *
 * Other kernel modules may wish to use the CBC line discipline.
 * They can potentially define their own configurations for the CBC channels.
 */
void cbc_mux_configure_data_channel(u32 const channel_idx, const u8 priority,
				void *data,
				void (*receive)(void *data, const u16 length,
				const u8 * const buffer))
{
	if (channel_idx < CBC_CHANNEL_MAX_NUMBER) {
		struct cbc_mux_channel *list =
			&cbc_mux_config.cbc_mux_channel_list[channel_idx];
		list->data = data;
		list->priority = priority;
		list->buffer_receive = NULL;
		list->data_receive = receive;
	}
}


/*
 * cbc_register_devices - Register the CBC channels as Linux character devices.
 * @cbc_class: CBC device class.
 * @memory: CBC memory pool allocated for CBC buffers.
 *
 * Return: 0 on success or Linux error code.
 */
int cbc_register_devices(struct class *cbc_class,
			struct cbc_memory_pool *memory)
{
	int ret = 0;
	int i;
	dev_t devid;

	struct cbc_device_manager *cbc = &cbc_device_mgr_configuration;

	cbc->cbc_memory = memory;

	/* Set up the devices after the line discipline is opened. */
	if (major) {
		devid = MKDEV(major, 0);
		ret = register_chrdev_region(devid, CBC_CHANNEL_MAX_NUMBER,
		DEVICE_NAME);
	} else {
		ret = alloc_chrdev_region(&devid, 0, CBC_CHANNEL_MAX_NUMBER,
		DEVICE_NAME);
		major = MAJOR(devid);
	}

	if (ret < 0)
		pr_err("cbc-core: ldisc open register chrdev region failed.\n");

	if (ret == 0) {
		cdev_init(&cbc->cdev, &cbc_dev_file_operations);
		cbc->cdev.owner = THIS_MODULE;
		cbc->cdev.ops = &cbc_dev_file_operations;
		ret = cdev_add(&cbc->cdev, MKDEV(major, CBC_MINOR),
						CBC_CHANNEL_MAX_NUMBER);
		if (ret < 0) {
			unregister_chrdev_region(MKDEV(major, CBC_MINOR),
						CBC_CHANNEL_MAX_NUMBER);
			pr_err("cbc-core: ldisc open add cdev failed\n");
		}
	}

	if (ret == 0) {
		for (i = 0; i < CBC_CHANNEL_MAX_NUMBER; i++) {
			cbc_device_init(&cbc->channels[i]);

			if (cbc->channels[i].device_type !=
					CBC_DEVICE_TYPE_HIDDEN) {
				/*
				 * Create the devices.
				 * These will appear in /sys/class/cbc and
				 * if udev is running, /dev
				 */
				cbc->channels[i].device = device_create(
					cbc_class, NULL,
					MKDEV(major, i), NULL,
					cbc->channels[i].device_name,
					i);

				/* Add the attribute */
				ret = device_create_file(
					cbc->channels[i].device,
					&dev_attr_priority);

				/* Set private data to point to the fifo */
				dev_set_drvdata(cbc->channels[i].device,
							&cbc->channels[i]);
			} else {
				cbc->channels[i].device = NULL;
			}
		}
	}

	if (ret != 0)
		cbc_unregister_devices(cbc_class);

	return ret;
}

/*
 * cbc_unregister_devices - Remove CBC devices.
 * @cbc_class: CBC Device class.
 *
 * Remove CBC device files and unregisters chrdev region.
 */
void cbc_unregister_devices(struct class *cbc_class)
{
	int i;
	struct cbc_device_manager *cbc = &cbc_device_mgr_configuration;

	/* Remove the /dev/cbc* devices */
	cdev_del(&cbc->cdev);

	for (i = 0; i < CBC_CHANNEL_MAX_NUMBER; i++) {
		if (cbc->channels[i].device != NULL) {
			device_remove_file(cbc->channels[i].device,
						&dev_attr_priority);
			device_destroy(cbc_class, MKDEV(major, i));
		}
	}

	/*
	 * Also destroys all class attribute files,
	 * because they are ref. counted.
	 */
	unregister_chrdev_region(MKDEV(major, CBC_MINOR),
					CBC_CHANNEL_MAX_NUMBER);

	cbc->cbc_memory = NULL;
}

/*
 * cbc_manager_transmit_data - Transmit data on specified channel.
 * @channel_idx: Channel identifier.
 * @length: Length of data.
 * @buffer: Pointer to data buffer.
 *
 * If a CBC buffer is available from the memory pool, and the channel
 * is valid, the supplied data is copied into a CBC buffer and transmitted.
 */
void cbc_manager_transmit_data(const u32 channel_idx, const u16 length,
						const u8 * const buffer)
{
	struct cbc_buffer *cbc_buffer = cbc_memory_pool_get_buffer(
				cbc_device_mgr_configuration.cbc_memory);
	struct cbc_device_data *chn_data;
	u32 offset = CBC_HEADER_SIZE;
	u32 copy_length = length;

	if (channel_idx >= CBC_CHANNEL_MAX_NUMBER) {
		pr_err("cbc_mux_transmit_data(): Invalid cbc channel idx.\n");
		return;
	}

	chn_data = &cbc_device_mgr_configuration.channels[channel_idx];

	if (!cbc_buffer)
		return;

	if (chn_data->device_type == CBC_DEVICE_TYPE_RAW)
		offset = CBC_HEADER_SIZE + CBC_RAWHEADER_SIZE;

	if (length + offset > CBC_MAX_TOTAL_FRAME_SIZE)
		copy_length = CBC_MAX_TOTAL_FRAME_SIZE - offset;

	memcpy(&cbc_buffer->data[offset], buffer, copy_length);

	cbc_manager_transmit_buffer(channel_idx, cbc_buffer);
	cbc_buffer_release(cbc_buffer);
}

/*
 * cbc_manager_transmit_buffer - Transmits CBC buffer on specified channel.
 * @channel_idx: Channel identifier.
 * @buffer: CBC buffer to transmit.
 */
void cbc_manager_transmit_buffer(const u32 channel_idx,
				struct cbc_buffer *buffer)
{
	if (channel_idx >= CBC_CHANNEL_MAX_NUMBER) {
		pr_err("cbc_mux_transmit_data(): Invalid cbc channel idx.\n");
	} else {
		enum cbc_error res = CBC_OK;
		struct cbc_device_data *chn_data =
			&cbc_device_mgr_configuration.channels[channel_idx];

		mutex_lock(&cbc_device_mgr_configuration.send_lock);

		if (chn_data->device_type == CBC_DEVICE_TYPE_RAW) {
			/*
			 * Room for raw header is already reserved in buffer.
			 * Calculate raw header data length without raw
			 * header.
			 */
			u32 real_payload_size = buffer->payload_length -
							CBC_RAWHEADER_SIZE;

			buffer->data[CBC_HEADER_SIZE] =
					CBC_RAW_CHANNEL_DIRECT_TRANSPORT;
			buffer->data[CBC_HEADER_SIZE + 1] =
					(u8) (real_payload_size & 0xFF);
			buffer->data[CBC_HEADER_SIZE + 2] =
					(u8) ((real_payload_size >> 8) & 0xFFU);
		}

		res = cbc_mux_multiplexer_transmit_buffer(channel_idx, buffer);
		mutex_unlock(&cbc_device_mgr_configuration.send_lock);
		if (res != CBC_OK)
			pr_err("Error transmitting frame %u.\n", res);
	}
	/* Buffer is released in the calling cbc_device_write() */
}

/*
 * cbc_device_poll - Set up polling based on current status of queue.
 * @file: Handle to cbc_device_data.
 * @wait: Pointer to poll_table.
 *
 * Return: Updated poll mask.
 */
static unsigned int cbc_device_poll(struct file *file, poll_table *wait)
{
	struct cbc_file_data *f = (struct cbc_file_data *) file->private_data;
	unsigned int mask = 0;

	poll_wait(file, &f->wq_read, wait);

	if (!cbc_file_queue_empty(f))
		mask |= (POLLIN | POLLRDNORM);

	if (!(f->queue.read + CBC_QUEUE_LENGTH == f->queue.write))
		mask |= (POLLOUT | POLLWRNORM);

	return mask;
}

/*
 * cbc_device_ioctl - Handle CBC channel ioctl call.
 * @file: Handle to cbc_device_data.
 * @cmd: ioctl command.
 * @arg: argument associated with the command.
 *
 * The flag field and priority can get get/set for a CBC device (channel).
 *
 * Return: 0 if command successfully handled, Linux error otherwise.
 */
static long cbc_device_ioctl(struct file *file, unsigned int cmd,
						unsigned long arg)
{
	int tmp;
	struct cbc_file_data *file_data =
			(struct cbc_file_data *) file->private_data;
	struct cbc_device_data *chn_data = file_data->cbc_device;

	int idx = chn_data - &cbc_device_mgr_configuration.channels[0];

	switch (cmd) {
	case CBC_PRIORITY_GET:
		tmp = cbc_mux_multiplexer_get_priority(idx);
		if (copy_to_user((void __user *) arg, &tmp, sizeof(tmp)))
			return -EFAULT;
		return 0;

	case CBC_PRIORITY_SET:
		if (copy_from_user(&tmp, (void __user *) arg, sizeof(tmp)))
			return -EFAULT;
		cbc_mux_multiplexer_set_priority(idx, tmp);
		return 0;

	default:
		return -EINVAL;
	}

}

/*
 * get_default_priority - Get default priority for specified channel.
 * @channel_id: channel identifier.
 *
 * Return: Priority for specified channel.
 */
static u8 get_default_priority(enum cbc_channel_enumeration channel_id)
{
	u8 result = 1;

	switch (channel_id) {
	case CBC_CHANNEL_PMT:
	case CBC_CHANNEL_LIFECYCLE:
	case CBC_CHANNEL_DLT:
	case CBC_CHANNEL_LINDA:
		result = 6;
		break;

	case CBC_CHANNEL_DIAGNOSIS:
		result = 2;
		break;

	default:
		result = 3;
		break;
	}
	return result;
}

/*
 * demuxed_receive - Handle a CBC buffer received over UART.
 * @void_data: CBC device data.
 * @cbc_buffer: CBC buffer received over UART.
 *
 * Checks if there is valid data. Determines the frame type and
 * adds to buffer queue.
 */
static void demuxed_receive(void *void_data, struct cbc_buffer *cbc_buffer)
{
	struct cbc_device_data *device_data =
			(struct cbc_device_data *) void_data;
	struct list_head *current_item;
	struct cbc_file_data *current_file_data;

	if (device_data && cbc_buffer
			&& cbc_buffer->frame_length >
			CBC_HEADER_SIZE + CBC_CHECKSUM_SIZE) {
		/* Payload_length includes raw_header */
		u16 payload_length = cbc_buffer->frame_length -
				(CBC_HEADER_SIZE + CBC_CHECKSUM_SIZE);

		if (device_data->device_type == CBC_DEVICE_TYPE_RAW) {
			if (cbc_buffer->frame_length >
				(CBC_HEADER_SIZE + CBC_RAWHEADER_SIZE +
						CBC_CHECKSUM_SIZE)) {
				u16 raw_length;

				raw_length = cbc_buffer->data[4];
				raw_length |= (cbc_buffer->data[5] << 8);

				if (raw_length + CBC_RAWHEADER_SIZE >
							payload_length) {
					pr_err(
					"raw length (%i) is longer than payload length (%i)\n",
					raw_length, payload_length);
					/* Payload_length already set
					 * to max value
					 */
				} else {
					payload_length = raw_length +
							CBC_RAWHEADER_SIZE;
				}
			} else {
				pr_err("cbc-core: Frame to short for a raw frame\n");
			}
			cbc_buffer->payload_length = payload_length;
		} else if (device_data->device_type ==
						CBC_DEVICE_TYPE_DEFAULT) {
			cbc_buffer->payload_length = payload_length;
		}
		/* else, do not touch payload_length in a debug-channel */

		/* Enqueue */
		for (current_item = device_data->open_files_head.next
		; current_item != &device_data->open_files_head; current_item =
							current_item->next) {

			current_file_data = list_entry(current_item,
					struct cbc_file_data, list);
			/* File_enqueue increases ref. count. */
			cbc_file_enqueue(current_file_data, cbc_buffer);
		}
	} else {
		pr_err("cbc-core: (<- IOC) dev_receive data is null\n");
	}
}

/*
 * cbc_kmod_devices_init - Configure CBC multiplexer.
 *
 * Configures multiplexer channel list and configures the multiplexer using
 * this list. Initialises mutex lock for multiplexer.
 */
void cbc_kmod_devices_init(void)
{
	/*
	 * Set up the multiplexer channel list and use it to configure the
	 * multiplexer.
	 */
	u32 i = 0;

	for (; i < CBC_CHANNEL_MAX_NUMBER; i++) {
		cbc_mux_config.cbc_mux_channel_list[i].buffer_receive =
				demuxed_receive;
		cbc_mux_config.cbc_mux_channel_list[i].data_receive = NULL;
		cbc_mux_config.cbc_mux_channel_list[i].data =
				&cbc_device_mgr_configuration.channels[i];
		cbc_mux_config.cbc_mux_channel_list[i].priority =
				get_default_priority(i);
	}

	cbc_mux_multiplexer_setup(&cbc_mux_config);
	mutex_init(&cbc_device_mgr_configuration.send_lock);
}
