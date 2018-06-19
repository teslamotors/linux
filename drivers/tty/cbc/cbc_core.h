/* SPDX-License-Identifier: GPL-2.0 */
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

#ifndef CBC_CORE_MOD_H_
#define CBC_CORE_MOD_H_

#include <linux/cbc/cbc-core.h>
#include <linux/cdev.h>
#include <linux/ioctl.h>
#include <linux/mutex.h>
#include <linux/tty.h>

#include "cbc_memory.h"

#define CBC_IOCTL_MAGIC 0xf4

/*
 * struct cbc_struct -
 *
 * @magic: Marker to mark the cbc_core as valid.
 * @tty: tty associated with the CBC driver.
 * @class: CBC device class
 * @memory_pool: Memory pool of CBC buffer allocated for used by CBC driver.
 * @ldisc_mutex: Mutex to avoid unloading while accessing the driver
 */
struct cbc_struct {
	int magic;
	struct tty_struct *tty;
	struct class *cbc_class;
	struct cbc_memory_pool *memory_pool;
	struct mutex ldisc_mutex;
};

#endif /* CBC_CORE_MOD_H_ */
