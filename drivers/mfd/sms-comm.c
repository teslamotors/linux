#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/mutex.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_reserved_mem.h>
#include <linux/dma-mapping.h>
#include <asm/barrier.h>
#include <linux/mfd/sms-mbox.h>

#include "mailbox_service_comm.h"

enum DEVICE_MINORS
{
	SMSCAN_MINOR = 0,
	SMSGP_MINOR = 1,
	NUM_SMSCOMM_DEVICES = 2
};

#define CAN_QUEUE_MAGIC_VALUE 0x73514A72U	// TESLAA72 in leet
#define GP_QUEUE_MAGIC_VALUE 0x41543237U	// TA72 in ASCII

// The maximum number of CAN messages is determined by the size of the ring
// buffer in shared memory between the SMS and the A72 cluster on HW3.
#define MAX_NUM_CAN_SMS_MSGS 256U
#define SMSCAN_QUEUE_VERSION 0x2U

// The maximum number of messages is determined by the size of the ring buffer
// in shared memory between the SMS and the A72 cluster on HW3.
#define MAX_NUM_MSGS 256U
#define SMSGP_QUEUE_VERSION 0x1U

struct smscomm_device_params {
	const uint32_t queue_version;
	const uint32_t queue_size;
	const uint32_t minor;
	const size_t message_size;
	uint32_t mailbox_region;
	uint32_t magic_value;
};

struct smscomm_dev {
	char *name;
	struct sms_queue *read_queue;
	struct sms_queue *write_queue;
	dma_addr_t read_queue_handle;
	dma_addr_t write_queue_handle;
	struct mutex lock;
	struct cdev cdev;
	struct platform_device *pdev;
	uint8_t mode;
	uint64_t reserved_memory_size;
	struct device *device_node;
	const struct smscomm_device_params *params;
};

struct sms_queue_header {
	uint32_t magic_value;
	uint32_t version;
	uint32_t tail;
	uint32_t head;
	uint32_t queue_size;
	uint32_t message_size;
} __attribute__((__packed__));  // enable unaligned placement

struct sms_queue {
	struct sms_queue_header header;
	uint8_t message[];
} __attribute__((__packed__));  // enable unaligned placement;

#define MESSAGE_BUFFER_SIZE 128U

struct gp_message {
	uint32_t timestamp_ms;
	uint16_t msg_id;
	uint16_t reserved_0;
	uint8_t  msg_size;
	uint8_t  msg_version;
	uint8_t  msg_crc;
	uint8_t  reserved_1;
	uint8_t  msg_buffer[MESSAGE_BUFFER_SIZE];
} __attribute__((__packed__));

struct can_message {
	uint16_t message_id;
	uint8_t bus : 4;
	uint8_t length : 4;
	uint8_t reserved;
	uint8_t payload[8];
} __attribute__((__packed__));

struct can_message_with_timestamp {
	uint64_t timestamp_ns;
	struct can_message msg;
	uint8_t crc;
	uint8_t reserved0;
	uint16_t reserved1;
} __attribute__((__packed__));

static const struct smscomm_device_params smscan_params = {
	.queue_version = SMSCAN_QUEUE_VERSION,
	.queue_size = MAX_NUM_CAN_SMS_MSGS,
	.minor = SMSCAN_MINOR,
	.message_size = sizeof(struct can_message_with_timestamp),
	.mailbox_region = COMM_SERVICE_CAN_REGION_ID,
	.magic_value = CAN_QUEUE_MAGIC_VALUE
};

static const struct smscomm_device_params smsgp_params = {
	.queue_version = SMSGP_QUEUE_VERSION,
	.queue_size = MAX_NUM_MSGS,
	.minor = SMSGP_MINOR,
	.message_size = sizeof(struct gp_message),
	.mailbox_region = COMM_SERVICE_GP_REGION_ID,
	.magic_value = GP_QUEUE_MAGIC_VALUE
};

int smscomm_major;
struct class *smscomm_device_class;

struct smscomm_dev smscan_device = {
	.name = "smscan",
	.params = &smscan_params,
};

struct smscomm_dev smsgp_device = {
	.name = "smsgp",
	.params = &smsgp_params,
};

static const struct of_device_id smscomm_dt_match[] = {
	{ .compatible = "turbo,trav-smscan", .data = &smscan_device },
	{ .compatible = "turbo,trav-smsgp" , .data = &smsgp_device },
	{}
};

/**
 * get_reserved_memory_size - Get the size of the memory region reserved for a
 * particular device from the device tree.
 *
 * @pdev: the platform device whose device tree node is to be parsed.
 * @size: a pointer to return the size of the reserved memory.
 *
 * Return value is 0 if the memory region was successfully parsed and an error
 * value otherwise.
 */
static int get_reserved_memory_size(struct platform_device *pdev, uint64_t *size)
{
	struct device_node *memory;
	struct reserved_mem *res_mem;

	if (!size)
		return -EINVAL;

	memory = of_parse_phandle(pdev->dev.of_node, "memory-region", 0);
	if (!memory) {
		dev_err(&pdev->dev, "smscomm: Memory region not found.");
		return -ENOENT;
	}

	res_mem = of_reserved_mem_lookup(memory);
	if (!res_mem)
	{
		dev_err(&pdev->dev, "smscomm: Reserved memory not found.");
		return -ENODATA;
	}

	*size = res_mem->size;
	return 0;
}

/**
 * get_queue_current_size - Gets the number of elements currrently in the
 * queue.
 *
 * @head: The head of the queue.
 * @tail: The tail of the queue.
 * @max_size: The maximum size of the queue.
 *
 * Returns the number of elements currently in the queue.
 */
static size_t get_queue_current_size(uint32_t head, uint32_t tail,
		size_t max_size)
{
	size_t retval = 0;

	if (head <= tail)
		retval = (tail - head);
	else
		retval = (max_size - head + tail);

	return retval;
}

/**
 * get_queue_available_size - Gets the number of elements that can be added to
 * the queue.
 *
 * @head: The head of the queue.
 * @tail: The tail of the queue.
 * @max_size: The maximum size of the queue.
 *
 * Returns the number of elements that can be added to the queue.
 */
static size_t get_queue_available_size(uint32_t head, uint32_t tail,
		size_t max_size)
{
	size_t retval = 0;
	size_t current_size = 0;

	current_size = get_queue_current_size(head, tail, max_size);

	if (current_size < 0)
		return current_size;

	retval = (max_size - 1) - current_size;
	return retval;
}

static ssize_t read_from_queue(struct sms_queue *queue, char __user *buf,
			size_t buf_size)
{
	uint32_t head;
	uint32_t tail;
	size_t current_size;
	ssize_t retval;
	size_t num_bytes;
	size_t bytes_remaining;
	uint32_t message_size;

	if (queue == NULL)
		return -EINVAL;

	message_size = queue->header.message_size;
	if (buf_size % message_size != 0)
		return -EINVAL;

	head = queue->header.head;
	tail = READ_ONCE(queue->header.tail);
	if ((head >= queue->header.queue_size) ||
	    (tail >= queue->header.queue_size))
		return -EIO;

	current_size = get_queue_current_size(head, tail,
						queue->header.queue_size);
	current_size *= message_size;

	if (current_size == 0)
		return -EAGAIN;

	bytes_remaining = min(buf_size, current_size);
	retval = bytes_remaining;

	/*
	 * Copy from the head to the end of the array as long as the user mode
	 * buffer can accommodate the data.
	 */
	num_bytes = (queue->header.queue_size - head) * message_size;
	if (bytes_remaining < num_bytes)
		num_bytes = bytes_remaining;

	if (copy_to_user(buf, &queue->message[head * message_size], num_bytes))
		return -EFAULT;

	bytes_remaining -= num_bytes;
	if (bytes_remaining > 0) {
		/*
		 * Copy from the beginning of the array to the tail as long as
		 * the user mode buffer can accommodate the data.
		 */
		buf += num_bytes;
		num_bytes = tail * message_size;
		if (bytes_remaining < num_bytes)
			num_bytes = bytes_remaining;

		if (copy_to_user(buf, &queue->message[0], num_bytes))
			return -EFAULT;
	}

	head += retval / message_size;
	head %= queue->header.queue_size;

	/*
	 * Make sure that all the memory reads from the shared memory are
	 * completed before updating the head of the queue.
	 */
	rmb();
	WRITE_ONCE(queue->header.head, head);
	return retval;
}

static ssize_t write_to_queue(struct sms_queue *queue,
		const char __user *buf, size_t buf_size)
{
	uint32_t head;
	uint32_t tail;
	size_t available_size;
	ssize_t retval;
	size_t num_bytes;
	size_t bytes_remaining;
	uint32_t message_size;
	const char *bufptr;

	if (queue == NULL)
		return -EINVAL;

	message_size = queue->header.message_size;
	if (buf_size % message_size != 0) {
		pr_info("Buf Size: %zu  Msg Size: %u", buf_size, message_size);
		return -EINVAL;
	}

	head = READ_ONCE(queue->header.head);
	tail = queue->header.tail;
	available_size = get_queue_available_size(head, tail,
						  queue->header.queue_size);

	available_size *= message_size;
	if (available_size == 0)
		return -EAGAIN;

	bytes_remaining = min(buf_size, available_size);
	retval = bytes_remaining;

	/*
	 * Copy from the tail to the end of the array as long as there is data
	 * in the user mode buffer.
	 */
	bufptr = buf;
	num_bytes = (queue->header.queue_size - tail) * message_size;
	if (bytes_remaining < num_bytes)
		num_bytes = bytes_remaining;

	if (copy_from_user(&queue->message[tail * message_size], bufptr, num_bytes))
		return -EFAULT;

	bytes_remaining -= num_bytes;
	if (bytes_remaining > 0) {
		/*
		 * Copy from the beginning of the array to the head as long as
		 * the user mode buffer can accommodate the data.
		 */
		bufptr = buf + num_bytes;
		num_bytes = (head - 1) * message_size;
		if (bytes_remaining < num_bytes)
			num_bytes = bytes_remaining;

		if (copy_from_user(&queue->message[0], bufptr, num_bytes))
			return -EFAULT;
	}

	tail += retval / message_size;
	tail %= queue->header.queue_size;

	/*
	 * Make sure that all the memory writes to the shared memory are
	 * completed before updating the tail of the queue.
	 */
	wmb();
	WRITE_ONCE(queue->header.tail, tail);
	return retval;
}

static int smscomm_open(struct inode *inode, struct file *filp)
{
	struct smscomm_dev *dev;

	dev = container_of(inode->i_cdev, struct smscomm_dev, cdev);
	filp->private_data = dev;
	return 0;
}

static int smscomm_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static ssize_t smscomm_read(struct file *filp, char __user *buf, size_t count,
				loff_t *f_pos)
{
	struct smscomm_dev *dev = filp->private_data;
	ssize_t retval;
	struct sms_queue *queue = (struct sms_queue *)dev->read_queue;

	if (mutex_lock_interruptible(&dev->lock))
		return -ERESTARTSYS;

	retval = read_from_queue(queue, buf, count);
	mutex_unlock(&dev->lock);
	return retval;
}

static ssize_t smscomm_write(struct file *filp, const char __user *buf,
			size_t count, loff_t *f_pos)
{
	struct smscomm_dev *dev = filp->private_data;
	ssize_t retval = 0;
	struct sms_queue *queue = (struct sms_queue *)dev->write_queue;

	if (mutex_lock_interruptible(&dev->lock))
		return -ERESTARTSYS;

	retval = write_to_queue(queue, buf, count);
	mutex_unlock(&dev->lock);
	return retval;
}

const struct file_operations smscomm_fops = {
	.owner = THIS_MODULE,
	.read = smscomm_read,
	.write = smscomm_write,
	.open = smscomm_open,
	.release = smscomm_release,
};

static int smscomm_remove(struct platform_device *pdev)
{
	struct smscomm_dev *dev = platform_get_drvdata(pdev);

	if (!IS_ERR(dev->device_node))
		device_destroy(smscomm_device_class,
			MKDEV(smscomm_major, dev->params->minor));

	if (dev->write_queue != NULL) {
		dma_free_coherent(&pdev->dev, dev->reserved_memory_size,
			dev->write_queue,
			dev->write_queue_handle);
	}

	cdev_del(&dev->cdev);
	mutex_destroy(&dev->lock);
	of_reserved_mem_device_release(&pdev->dev);
	return 0;
}

static int smscomm_setup_cdev(struct smscomm_dev *smsdev)
{
	int err;
	int devno = MKDEV(smscomm_major, smsdev->params->minor);

	cdev_init(&smsdev->cdev, &smscomm_fops);
	smsdev->cdev.owner = THIS_MODULE;
	err = cdev_add(&smsdev->cdev, devno, 1);
	if (err) {
		dev_err(&(smsdev->pdev->dev),
			"smscomm: Error %d adding cdev.\n", err);
		return err;
	}

	return 0;
}

static int smscomm_probe(struct platform_device *pdev)
{
	int err;
	char *read_queue_addr;
	size_t buffer_size;
	struct smscomm_dev *smsdev;
	const struct of_device_id *of_id;

	of_id = of_match_node(smscomm_dt_match, pdev->dev.of_node);
	smsdev = (struct smscomm_dev *)of_id->data;
	smsdev->pdev = pdev;
	platform_set_drvdata(pdev, smsdev);
	err = get_reserved_memory_size(pdev, &smsdev->reserved_memory_size);
	if (err) {
		pr_err("smscomm: Unable to get reserved memory size.");
		return err;
	}

	err = of_reserved_mem_device_init(&pdev->dev);
	if (err) {
		pr_err("smscomm: Unable to reserve memory for %s\n", pdev->name);
		return err;
	}

	dma_set_coherent_mask(&pdev->dev, 0xFFFFFFFF);
	mutex_init(&smsdev->lock);
	err = smscomm_setup_cdev(smsdev);
	if (err)
		return err;

	/*
	 * Allocate memory for the read and write queues. The call to
	 * dma_zalloc_coherent will also initialize the head and tail pointers
	 * for queues by setting them to zero.
	 */
	smsdev->write_queue = dma_zalloc_coherent(&pdev->dev,
					(size_t)smsdev->reserved_memory_size,
					&smsdev->write_queue_handle,
					GFP_KERNEL);

	if (smsdev->write_queue == NULL) {
		pr_err("smscomm: Unable to allocate memory for queues\n");
		err = -ENOMEM;
		goto exit;
	}

	pr_info("smscomm: Write queue at DMA:0x%0llx, VA:0x%p\n",
		smsdev->write_queue_handle,
		smsdev->write_queue);

	buffer_size = smsdev->reserved_memory_size / 2;
	smsdev->write_queue->header.queue_size = smsdev->params->queue_size;
	read_queue_addr = (char *)smsdev->write_queue + buffer_size;
	smsdev->read_queue = (struct sms_queue *)read_queue_addr;
	read_queue_addr = (char *)smsdev->write_queue_handle +
				 buffer_size;

	smsdev->read_queue_handle = (dma_addr_t)read_queue_addr;

	pr_info("smscomm: Read queue at DMA:0x%0llx, VA:0x%p\n",
		smsdev->read_queue_handle,
		smsdev->read_queue);

	smsdev->read_queue->header.queue_size = smsdev->params->queue_size;
	smsdev->device_node = device_create(smscomm_device_class, NULL,
				MKDEV(smscomm_major, smsdev->params->minor),
				NULL, smsdev->name);

	if (IS_ERR(smsdev->device_node)) {
		err = PTR_ERR(smsdev->device_node);
		goto exit;
	}

	/*
	 * Add magic values to the queue structure so that SMS can do a sanity
	 * check that the addresses provided are correct
	 */
	smsdev->read_queue->header.magic_value = smsdev->params->magic_value;
	smsdev->write_queue->header.magic_value = smsdev->params->magic_value;

	smsdev->read_queue->header.version = smsdev->params->queue_version;
	smsdev->write_queue->header.version = smsdev->params->queue_version;

	smsdev->read_queue->header.message_size = smsdev->params->message_size;
	smsdev->write_queue->header.message_size = smsdev->params->message_size;

	err = sms_mbox_reset_sms_comm(smsdev->params->mailbox_region,
		smsdev->write_queue_handle, smsdev->reserved_memory_size);

	if (err) {
		pr_err("smscomm: Couldn't set ringbuffer address via mailbox\n");
		goto exit;
	}

	return 0;

exit:
	smscomm_remove(pdev);
	return err;
}

MODULE_DEVICE_TABLE(of, smscomm_dt_match);

static struct platform_driver smscomm_driver = {
	.driver = {
		.name = "smscomm",
		.of_match_table = smscomm_dt_match,
	},
	.probe = smscomm_probe,
	.remove = smscomm_remove,
};

static void smscomm_uninit(void)
{
	dev_t devno = MKDEV(smscomm_major, SMSCAN_MINOR);

	if (!IS_ERR(smscomm_device_class))
		class_destroy(smscomm_device_class);

	unregister_chrdev_region(devno, NUM_SMSCOMM_DEVICES);
}

static void smscomm_exit(void)
{
	platform_driver_unregister(&smscomm_driver);
	smscomm_uninit();
}

static int __init smscomm_init(void)
{
	int err;
	dev_t dev = 0;

	err = alloc_chrdev_region(&dev, SMSCAN_MINOR, NUM_SMSCOMM_DEVICES,
		"smscomm");

	smscomm_major = MAJOR(dev);
	if (err) {
		pr_err("smscomm: Can't get major for devices.\n");
		return err;
	}

	smscomm_device_class = class_create(THIS_MODULE, "smscomm");
	if (IS_ERR(smscomm_device_class)) {
		pr_err("smscomm: Failed to create device class smscomm\n");
		unregister_chrdev_region(dev, NUM_SMSCOMM_DEVICES);
		err = PTR_ERR(smscomm_device_class);
		return err;
	}

	err = platform_driver_register(&smscomm_driver);
	if (err) {
		pr_err("smscomm: Failed to register driver\n");
		smscomm_uninit();
		return err;
	}

	return 0;
}

module_init(smscomm_init);
module_exit(smscomm_exit);

MODULE_LICENSE("GPL");
