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

#include <linux/device.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/tty.h>
#include <linux/uaccess.h>

#include "cbc_core.h"
#include "cbc_types.h"
#include "cbc_device_manager.h"
#include "cbc_link_layer.h"
#include "cbc_mux_multiplexer.h"

MODULE_AUTHOR("Gerhard Bocksch.  gerhard.bocksch@intel.com");
MODULE_DESCRIPTION("CBC serial protocol demultiplexer");
MODULE_LICENSE("GPL");

static int granularity = 4;
module_param(granularity, int, 0644);
MODULE_PARM_DESC(granularity,
		"The granularity of the CBC messages. default: 4");

MODULE_ALIAS_LDISC(N_CBCCORE);

/* Marker to mark the cbc_core as valid. */
#define CBC_MAGIC 0xAFFEAFFE

static struct cbc_struct cbc_core = { .magic = CBC_MAGIC };

/*
 * cbc_core_configure_channel - Set the priority and receive
 *                              callback for a CBC channel.
 * @channel_idx: Channel identifier (see cbc_channel_enumeration)
 * @priority: Priority for channel.
 * @data: Data associated with channel.
 * @receive: Receive callback associated with channel.
 */
void cbc_core_configure_channel(u32 const channel_idx, const u8 priority,
				void *data,
				void (*receive)(void *data, const u16 length,
				const u8 * const buffer))
{
	cbc_mux_configure_data_channel(channel_idx, priority, data, receive);
}
EXPORT_SYMBOL_GPL(cbc_core_configure_channel);

/*
 * cbc_core_send_data - Send to a CBC channel.
 * @channel_idx: Channel identifier (see cbc_channel_enumeration)
 * @priority: Priority for channel.
 * @data: Data associated with channel.
 * @receive: Receive function associated with channel.
 */
void cbc_core_send_data(const u32 channel_idx, const u16 length,
						const u8 * const buffer)
{
	cbc_manager_transmit_data(channel_idx, length, buffer);
}
EXPORT_SYMBOL_GPL(cbc_core_send_data);

/*
 * cbc_ldisc_open - Open the CBC connection to the IOC.
 * @tty: Handle to tty.
 *
 * This function is called by the tty module when the
 * line discipline is requested. It allocates the memory pool and creates the
 * CBC devices.
 * Called in process context serialized from other ldisc calls.
 *
 * Return: 0 on success error otherwise.
 */
static int cbc_ldisc_open(struct tty_struct *tty)
{
	struct cbc_struct *cbc;
	int err;

	pr_debug("cbc-ldisc open.\n");
	mutex_lock(&cbc_core.ldisc_mutex);

	if (WARN_ON(tty->ops->write == NULL)) {
		pr_err("cbc-ldisc open write not supported.\n");
		err = -EOPNOTSUPP;
		goto err_exit;
	}

	cbc = tty->disc_data;

	err = -EEXIST;
	/* First make sure we're not already connected. */
	if (WARN_ON(cbc && cbc->magic == CBC_MAGIC)) {
		pr_err(
		"cbc-ldisc CBC line discipline already open or CBC magic wrong.\n");
		goto err_exit;
	}

	cbc = &cbc_core;

	cbc->tty = tty;
	tty->disc_data = cbc;
	tty->receive_room = 65536;

	/* Create class cbc-core. This will appear as /sys/class/cbc* */
	cbc->cbc_class = class_create(THIS_MODULE, "cbc");

	if (WARN_ON(!cbc->cbc_class)) {
		pr_err("cbc-ldisc open could not create cbc class.\n");
		goto err_exit;
	}

	/* Create memory pool */
	cbc->memory_pool = cbc_memory_pool_create(
			CBC_QUEUE_LENGTH * CBC_CHANNEL_MAX_NUMBER);
	if (WARN_ON(!cbc->memory_pool)) {
		pr_err("failed to create memory pool.\n");
		goto err_exit;
	}

	/* Initialise on every open, so we start with sequence-counters at 0.*/
	cbc_link_layer_init(cbc->memory_pool);

	/* Register devices here. */
	err = cbc_register_devices(cbc->cbc_class, cbc->memory_pool);
	if (err != 0) {
		pr_err("register devices failed\n");
		goto err_exit;
	}

	cbc_link_layer_set_frame_granularity(granularity);

	/* tty layer expects 0 on success */
	mutex_unlock(&cbc_core.ldisc_mutex);
	return 0;

err_exit: mutex_unlock(&cbc_core.ldisc_mutex);
	return err;
}

/*
 * cbc_ldisc_close - Close down the CBC communication.
 * @tty: Handle to tty
 *
 * Unregister CBC devices and destroy CBC class.
 */
static void cbc_ldisc_close(struct tty_struct *tty)
{
	struct cbc_struct *cbc;

	mutex_lock(&cbc_core.ldisc_mutex);

	cbc = (struct cbc_struct *) tty->disc_data;
	pr_debug("cbc-ldisc close\n");

	if (cbc && cbc->magic == CBC_MAGIC && cbc->tty == tty) {
		cbc_unregister_devices(cbc->cbc_class);
		class_destroy(cbc->cbc_class);
		if (WARN_ON(!cbc_memory_pool_try_free(cbc->memory_pool)))
			pr_err("could not free memory pool.\n");
		tty->disc_data = NULL;
		cbc->tty = NULL;
	} else {
		WARN_ON(1);
		pr_err("ldisc close with wrong CBC magic.\n");
	}

	mutex_unlock(&cbc_core.ldisc_mutex);
}

/*
 * Close line discipline on ldisc hangup.
 * @tty: Handle to tty
 */
static int cbc_ldisc_hangup(struct tty_struct *tty)
{
	cbc_ldisc_close(tty);
	return 0;
}

/*
 * cbc_ldisc_receive_buf - .receive call for a line discipline.
 * @tty: Handle to tty
 * @cp:  Received data buffer.
 * @fp:  Not used
 * @count: Amount of data received.
 *
 * Attempts to read a single CBC frame at a time. Cycles round
 * until all data has been processed.
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
		do {
			u8 chunksize = 254;

			if ((count - accepted_bytes_sum) < 255)
				chunksize = count - accepted_bytes_sum;
			accepted_bytes = cbc_core_on_receive_cbc_serial_data(
					chunksize, cp + accepted_bytes_sum);
			accepted_bytes_sum += accepted_bytes;
			cbc_link_layer_rx_handler();
		} while (accepted_bytes_sum != count);
	}
	mutex_unlock(&cbc_core.ldisc_mutex);
}

/*
 * Called from ldisc when write is possible. Not used.
 */
static void cbc_ldisc_write_wakeup(struct tty_struct *tty)
{
	(void) tty;
}

/* Called to send messages from channel specific device to the IOC */
enum cbc_error target_specific_send_cbc_uart_data(u16 length,
						const u8 *raw_buffer)
{
	mutex_lock(&cbc_core.ldisc_mutex);
	if (cbc_core.tty) {
		set_bit(TTY_DO_WRITE_WAKEUP, &cbc_core.tty->flags);

		cbc_core.tty->ops->write(cbc_core.tty, raw_buffer, length);
	}
	mutex_unlock(&cbc_core.ldisc_mutex);

	return CBC_OK;
}

static struct tty_ldisc_ops cbc_ldisc = {
	.owner = THIS_MODULE,
	.name = "cbc-ldisc",
	.magic = TTY_LDISC_MAGIC,
	.open = cbc_ldisc_open,
	.close = cbc_ldisc_close,
	.hangup = cbc_ldisc_hangup,
	.receive_buf = cbc_ldisc_receive_buf,
	.write_wakeup = cbc_ldisc_write_wakeup
};

/*
 * cbc_init - Module init call.
 * Registers this module as TTY line discipline.
 */
static int __init cbc_init(void)
{
	int status;

	pr_debug("cbc-ldisc init\n");
	mutex_init(&cbc_core.ldisc_mutex);
	mutex_lock(&cbc_core.ldisc_mutex);

	cbc_kmod_devices_init();

	/* Fill in line discipline, and register it */
	status = tty_register_ldisc(N_CBCCORE, &cbc_ldisc);
	if (status)
		pr_err("cbc-ldisc: can't register line discipline\n");

	mutex_unlock(&cbc_core.ldisc_mutex);
	return 0;
}

/*
 * cbc_exit - Module exit call.
 *
 * De-registers itself as line discipline.
 */
static void __exit cbc_exit(void)
{
	int i;

	mutex_lock(&cbc_core.ldisc_mutex);

	pr_debug("cbc-ldisc Exit\n");

	i = tty_unregister_ldisc(N_CBCCORE);
	if (i)
		pr_err("cbc-ldisc: can't unregister ldisc (err %d).\n", i);

	mutex_unlock(&cbc_core.ldisc_mutex);
}

module_init(cbc_init);
module_exit(cbc_exit);
