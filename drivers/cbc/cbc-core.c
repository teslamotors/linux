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


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/errno.h>
#include <linux/major.h>
#include <linux/device.h>

#include "cbc_types.h"
#include "mux/cbc_mux_multiplexer.h"

#include "link/cbc_link_layer.h"
#include "link/cbc_load_monitor.h"

#include "cbc-core.h"
#include "dev/cbc_device_manager.h"

MODULE_AUTHOR("Gerhard Bocksch.  gerhard.bocksch@intel.com");
MODULE_DESCRIPTION("cbc serial protocol demultiplexer");
MODULE_LICENSE("GPL v2");

static int granularity = 4;
module_param(granularity, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(granularity, "The granularity of the cbc messages. default: 4");

static int debug;
module_param(debug, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(debug, "Debug level of this module. default: 0");


#ifndef N_CBCCORE
#define N_CBCCORE 32 /* load with ldattach 32 /dev/...  or cbcattach */
#endif
MODULE_ALIAS_LDISC(N_CBCCORE);


/* marker to mark the cbc_core as valid. */
#define CBC_MAGIC 0xAFFEAFFE


struct cbc_struct cbc_core = {
	.magic = CBC_MAGIC
};

/*
 * other kernel modules can use the exported symbols
 * to connect to a cbc-channel
 */

/*
 * set the priority and receive callback for a cbc channel.
 */
EXPORT_SYMBOL_GPL(cbc_core_configure_channel);
void cbc_core_configure_channel(u32 const channel_idx
		, const u8 priority
		, void *data
		, void(*receive)(void *data
				, const u16 length
				, const u8 * const buffer))
{
	cbc_mux_configure_data_channel(channel_idx, priority, data, receive);
}


/*
 * send to a cbc channel.
 */
EXPORT_SYMBOL_GPL(cbc_core_send_data);
void cbc_core_send_data(const u32 channel_idx
						, const u16 length
						, const u8 * const buffer)
{
	cbc_manager_transmit_data(channel_idx, length, buffer);
}


/*
 * debug flagfield
 */
static ssize_t cbc_ldisc_attr_debug_show(struct class *cls,
		struct class_attribute *attr, char *buf)
{
	pr_debug("cbc read debug: %i\n", debug);
	return scnprintf(buf, PAGE_SIZE, "%i\n", debug);
}


static ssize_t cbc_ldisc_attr_debug_store(struct class *cls,
		struct class_attribute *attr, const char *buf, size_t count)
{
	u32 new_value = 0;
	int res = kstrtou32(buf, 10, &new_value);

	if (res == 0) {
		pr_debug("cbc store debug: %i\n", new_value);
		debug = new_value;
	}

	return count;
}


struct class_attribute cbc_ldisc_attr_debug =
			__ATTR(cbcdebugfield
					, 0664
					, cbc_ldisc_attr_debug_show
					, cbc_ldisc_attr_debug_store);



/********           tty line stuff  ************/


/*
 * Open the cbc connection to the IOC.
 * Create devices.
 * This function is called by the TTY module when the
 * line discipline is called for.
 *
 * Called in process context serialized from other ldisc calls.
 */
static int cbc_ldisc_open(struct tty_struct *tty)
{
	struct cbc_struct *cbc;
	int err;

	pr_info("cbc-ldisc open\n");
	mutex_lock(&cbc_core.ldisc_mutex);

	if (tty->ops->write == NULL) {
		pr_err("cbc-ldisc open write not supported\n");
		err =  -EOPNOTSUPP;
		goto err_exit;
	}

	cbc = tty->disc_data;

	err = -EEXIST;
	/* First make sure we're not already connected. */
	if (cbc && cbc->magic == CBC_MAGIC) {
		pr_err("cbc-ldisc ldisc open cbc or magic wrong\n");
		goto err_exit;
	}

	cbc = &cbc_core;

	cbc->tty = tty;
	tty->disc_data = cbc;

	tty->receive_room = 65536;

	/* Create class cbc-core. This will appear as /sys/class/cbc */
	cbc->cbc_class = class_create(THIS_MODULE, "cbc");

	if (!cbc->cbc_class) {
		pr_err("cbc-ldisc open could not create cbc class\n");
		goto err_exit;
	}

	/* create memory pool */
	cbc->memory_pool = cbc_memory_pool_create(CBC_QUEUE_LENGTH * e_cbc_channel_max_number);

	/* init on every open, to start with seq-counter 0 */
	cbc_link_layer_init(cbc->memory_pool);

	/* register devices here */
	err = cbc_register_devices(cbc->cbc_class, cbc->memory_pool);
	if (err != 0) {
		pr_err("register devices failed\n");
		goto err_exit;
	}

	cbc_link_layer_set_frame_granularity(granularity);

	if (0 == err)
		err = class_create_file(cbc->cbc_class, &cbc_ldisc_attr_debug);

	if (err != 0) {
		pr_err("create cbc_debug failed\n");
		goto err_exit;
	}

	/* TTY layer expects 0 on success */
	mutex_unlock(&cbc_core.ldisc_mutex);
	return 0;

err_exit:

	/* Count references from TTY module */
	mutex_unlock(&cbc_core.ldisc_mutex);
	return err;
}


/*
 * Close down the cbc communication.
 */
static void cbc_ldisc_close(struct tty_struct *tty)
{
	struct cbc_struct *cbc;

	mutex_lock(&cbc_core.ldisc_mutex);
	cbc = (struct cbc_struct *) tty->disc_data;

	pr_info("cbc-ldisc close\n");

	if (cbc && cbc->magic == CBC_MAGIC && cbc->tty == tty) {
		cbc_unregister_devices(cbc->cbc_class);
		class_destroy(cbc->cbc_class);

		if (0 != cbc_memory_pool_try_free(cbc->memory_pool))
			pr_err("could not free memory pool\n");

		tty->disc_data = NULL;
		cbc->tty = NULL;
	} else
		pr_err("ldisc close with wrong cbc/magic\n");

	mutex_unlock(&cbc_core.ldisc_mutex);
}


/*
 * close ldisc on ldisc hangup
 */
static int cbc_ldisc_hangup(struct tty_struct *tty)
{
	cbc_ldisc_close(tty);
	return 0;
}


/*
 * receive call for a line discipline
 */
static void cbc_ldisc_receive_buf(struct tty_struct *tty,
						const unsigned char *cp, char *fp, int count)
{
	struct cbc_struct *cbc;
	u8 accepted_bytes; /* per cbc_serial_receive call */
	unsigned int accepted_bytes_sum = 0;

	mutex_lock(&cbc_core.ldisc_mutex);
	cbc = (struct cbc_struct *) tty->disc_data;

	if (cbc && (cbc->magic == CBC_MAGIC) && (cbc->tty == tty)) {
		if (debug & 0x01) {
			u32 i;

			pr_debug(" ");
			pr_cont("(IOC -> CM) %i bytes:", count);
			for (i = 0; i < count; i++)
				pr_cont(" 0x%X ", cp[i]);
			pr_cont("\n");
		}

		do {
			u8 chunksize = 254;

			if ((count - accepted_bytes_sum) < 255)
				chunksize = count - accepted_bytes_sum;
			accepted_bytes = cbc_core_on_receive_cbc_serial_data(chunksize, cp + accepted_bytes_sum);
			accepted_bytes_sum += accepted_bytes;
			cbc_link_layer_rx_handler();
			cbc_load_monitor_cyclic();
		} while (accepted_bytes_sum != count);
	}
	mutex_unlock(&cbc_core.ldisc_mutex);
}


/*
 * called from ldisc when write is possible. Not used.
 */
static void cbc_ldisc_write_wakeup(struct tty_struct *tty)
{
	(void)tty;
}


/*
 * handles ioctls.
 *
 * The cbc_core module provides a flagfield which can be
 * set and read via ioctls.
 */
static int cbc_ldisc_ioctl(struct tty_struct *tty
						   , struct file *file
						   , unsigned int cmd
						   , unsigned long arg)
{
	struct cbc_struct *cbc;
	int tmp;
	int result = 0;

	mutex_lock(&cbc_core.ldisc_mutex);
	cbc = (struct cbc_struct *) tty->disc_data;

	if (!cbc || (cbc->magic != CBC_MAGIC) || (cbc->tty != tty))
		result = -EFAULT;

	if (result == 0) {
		switch (cmd) {
		case CBC_FLAG_GET:
			tmp = cbc_mux_multiplexer_get_flagfield();
			if (copy_to_user((void __user *)arg, &tmp, sizeof(tmp)))
				result = -EFAULT;
			else
				result = 0;
			break;

		case CBC_FLAG_SET:
			if (copy_from_user(&tmp, (void __user *)arg, sizeof(tmp)))
				result = -EFAULT;
			else {
				cbc_mux_multiplexer_set_flagfield(tmp);
				result = 0;
			}
			break;

		default:
			result = tty_mode_ioctl(tty, file, cmd, arg);
			break;
		}
	}

	mutex_unlock(&cbc_core.ldisc_mutex);
	return result;
}


/* called to send messages from channel specific device to the IOC */
enum cbc_error  target_specific_send_cbc_uart_data(u16 length
												 , const u8 *raw_buffer)
{
	u32 i;

	mutex_lock(&cbc_core.ldisc_mutex);
	if (cbc_core.tty) {
		set_bit(TTY_DO_WRITE_WAKEUP, &cbc_core.tty->flags);

		if (debug & 0x02) {
			pr_debug(" ");
			pr_cont("(CM -> IOC) %i bytes:", length);
			for (i = 0; i < length; i++)
				pr_cont(" 0x%X ", raw_buffer[i]);
			pr_cont("\n");
		}
		cbc_core.tty->ops->write(cbc_core.tty, raw_buffer, length);
		cbc_load_monitor_cyclic();
	}
	mutex_unlock(&cbc_core.ldisc_mutex);

	return e_cbc_error_ok;
}


static struct tty_ldisc_ops cbc_ldisc = {
	.owner    = THIS_MODULE,
	.name   = "cbc-ldisc",
	.magic    = TTY_LDISC_MAGIC,
	.open   = cbc_ldisc_open,
	.close    = cbc_ldisc_close,
	.hangup   = cbc_ldisc_hangup,
	.ioctl    = cbc_ldisc_ioctl,
	.receive_buf  = cbc_ldisc_receive_buf,
	.write_wakeup = cbc_ldisc_write_wakeup
};


/*
 * module init call.
 * registers this module as tty line discipline
 */
static int __init cbc_init(void)
{
	int ret = 0;
	int status;

	pr_info("cbc-ldisc init\n");
	mutex_init(&cbc_core.ldisc_mutex);
	mutex_lock(&cbc_core.ldisc_mutex);

	cbc_kmod_devices_init();

	/* Fill in  line discipline, and register it */
	status = tty_register_ldisc(N_CBCCORE, &cbc_ldisc);
	if (status)
		pr_err("cbc-ldisc: can't register line discipline\n");

	mutex_unlock(&cbc_core.ldisc_mutex);
	return ret;
}


/*
 * module exit call. De-registers itself as line discipline
 */
static void __exit cbc_exit(void)
{
	int i;

	mutex_lock(&cbc_core.ldisc_mutex);

	pr_info("cbc-ldisc Goodbye\n");

	i = tty_unregister_ldisc(N_CBCCORE);
	if (i)
		pr_err("cbc-ldisc: can't unregister ldisc (err %d)\n", i);
	mutex_unlock(&cbc_core.ldisc_mutex);
}

module_init(cbc_init);
module_exit(cbc_exit);
