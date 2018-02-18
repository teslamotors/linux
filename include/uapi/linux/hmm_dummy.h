/*
 * Copyright 2013 Red Hat Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Authors: Jérôme Glisse <jglisse@redhat.com>
 */
/* This is a dummy driver made to exercice the HMM (hardware memory management)
 * API of the kernel. It allow an userspace program to map its whole address
 * space through the hmm dummy driver file.
 */
#ifndef _UAPI_LINUX_HMM_DUMMY_H
#define _UAPI_LINUX_HMM_DUMMY_H

#include <linux/types.h>
#include <linux/ioctl.h>
#include <linux/irqnr.h>

/* Expose the address space of the calling process through hmm dummy dev file */
#define HMM_DUMMY_EXPOSE_MM	_IO('R', 0x00)

#endif /* _UAPI_LINUX_RANDOM_H */
