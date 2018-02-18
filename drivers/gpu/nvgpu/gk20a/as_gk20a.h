/*
 * GK20A Address Spaces
 *
 * Copyright (c) 2011-2017, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef AS_GK20A_H
#define AS_GK20A_H

#include <linux/atomic.h>
#include <linux/cdev.h>
#include <linux/fs.h>

struct gk20a_as;
struct gk20a_as_share;
struct vm_gk20a;

struct gk20a_as_share {
	struct gk20a_as *as;
	int id;
	struct vm_gk20a *vm;
};

struct gk20a_as {
	int last_share_id; /* dummy allocator for now */
	struct cdev cdev;
	struct device *node;
};

int gk20a_as_release_share(struct gk20a_as_share *as_share);

/* struct file_operations driver interface */
int gk20a_as_dev_open(struct inode *inode, struct file *filp);
int gk20a_as_dev_release(struct inode *inode, struct file *filp);
long gk20a_as_dev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);

/* if big_page_size == 0, the default big page size is used */
int gk20a_as_alloc_share(struct gk20a_as *as, u32 big_page_size,
			 u32 flags, struct gk20a_as_share **out);

#endif
