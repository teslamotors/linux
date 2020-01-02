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
 */

#ifndef CBC_CORE_MOD_H_
#define CBC_CORE_MOD_H_

#include <linux/tty.h>
#include <linux/cdev.h>
#include <linux/ioctl.h>
#include <linux/cbc-core.h>
#include <linux/mutex.h>

#include "dev/cbc_memory.h"

#define CBC_IOCTL_MAGIC 0xf4

struct cbc_struct {
	int magic;
	struct tty_struct *tty;
	struct class *cbc_class;
	struct cbc_memory_pool *memory_pool;
	/* mutex to avoid unloading while accessing the driver */
	struct mutex ldisc_mutex;
};



#endif /* CBC_CORE_MOD_H_ */
