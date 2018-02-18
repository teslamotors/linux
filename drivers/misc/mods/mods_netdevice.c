/*
 * mods_net.c - This file is part of NVIDIA MODS kernel driver.
 *
 * Copyright (c) 2015, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA MODS kernel driver is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * NVIDIA MODS kernel driver is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NVIDIA MODS kernel driver.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include "mods.h"
#include "mods_internal.h"
#include <linux/netdevice.h>

int esc_mods_net_force_link(struct file *pfile, struct MODS_NET_DEVICE_NAME *p)
{
	struct net_device *ndev;

	if (!p || (strnlen(p->device_name, MAX_NET_DEVICE_NAME_LENGTH) == 0)
	       || (!memchr(p->device_name, '\0', MAX_NET_DEVICE_NAME_LENGTH))) {
			mods_error_printk("invalid device name\n");
			return -EINVAL;
	}

	for_each_netdev(&init_net, ndev)
		if (!strcmp(ndev->name, p->device_name)) {
			netif_carrier_on(ndev);
			mods_info_printk("carrier forced on: %s\n",
					 p->device_name);
			return OK;
		}

	mods_error_printk("failed to find network device %s\n",
			  p->device_name);
	return -EINVAL;
}
