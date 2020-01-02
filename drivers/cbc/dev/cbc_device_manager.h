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
 * Created on: Mar 20, 2014
 *
 * part of the cbc kernel module. It connects the cbc multiplexer with the
 * char-devices in /dev.
 */

#ifndef CBC_DEVICE_MANAGER_H
#define CBC_DEVICE_MANAGER_H

#include "dev/cbc_memory.h"

#include <linux/types.h>
#include <linux/device.h>

void cbc_kmod_devices_init(void);

int cbc_register_devices(struct class *cbc_class, struct cbc_memory_pool *memory);

void cbc_unregister_devices(struct class *cbc_class);


/*
 * mux and devices are currently in the same file,
 * this may be separated some day.
 *
 * channels can only be configured after cbc_kmod_devices_init().
 * This will overwrite the settings for the devices.
 * The device will be created anyway, to allow the cbc_socket_server to
 * work without a handling for missing devices.
 */
void cbc_mux_configure_data_channel(u32 const channel_idx
		, const u8 priority
		, void *data
		, void(*receive)(void *data
				, const u16 length
				, const u8 * const buffer));

/* transmit data to IOC. This is the version provided as kernel symbol.*/
void cbc_manager_transmit_data(const u32 channel_idx,
						   const u16 length,
						   const u8 * const buffer);


/* transmit data to IOC. This is the internal version, without memcpy.*/
void cbc_manager_transmit_buffer(const u32 channel_idx,
					struct cbc_buffer *buffer);


#endif /* CBC_DEVICE_MANAGER_H */
