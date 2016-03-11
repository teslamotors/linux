/*
 *
 * Intel Keystore Linux driver
 * Copyright (c) 2013, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */
#ifndef _KEYSTORE_IOCTL_H_
#define _KEYSTORE_IOCTL_H_

#include <linux/fs.h>

/**
 * keystore_ioctl() - the ioctl function
 *
 * @file: pointer to file structure
 * @cmd: command to execute
 * @arg: user space pointer to arguments structure
 *
 * @returns 0 on success, <0 on error
 */
long keystore_ioctl(struct file *file, unsigned int cmd, unsigned long arg);

#endif
