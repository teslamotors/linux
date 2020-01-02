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

#ifndef _UAPI_CBC_CORE_H_
#define _UAPI_CBC_CORE_H_

#include <linux/ioctl.h>

/* get/set global flag field */
#define CBC_FLAG_GET _IOR(CBC_IOCTL_MAGIC, 0, int)
#define CBC_FLAG_SET _IOW(CBC_IOCTL_MAGIC, 1, int)

/* get/set the priority of this channel */
#define CBC_PRIORITY_GET _IOR(CBC_IOCTL_MAGIC, 2, int)
#define CBC_PRIORITY_SET _IOW(CBC_IOCTL_MAGIC, 3, int)


#endif /* CBC_CORE_MOD_H_ */
