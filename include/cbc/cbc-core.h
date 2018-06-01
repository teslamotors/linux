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

#ifndef _UAPI_CBC_CORE_H_
#define _UAPI_CBC_CORE_H_

#include <linux/ioctl.h>

/* Get/set the priority of this channel */
#define CBC_PRIORITY_GET _IOR(CBC_IOCTL_MAGIC, 2, int)
#define CBC_PRIORITY_SET _IOW(CBC_IOCTL_MAGIC, 3, int)


#endif /* CBC_CORE_MOD_H_ */
