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

#ifndef CBC_DEVICE_MANAGER_H
#define CBC_DEVICE_MANAGER_H

#include "cbc_memory.h"

#include <linux/types.h>
#include <linux/device.h>

void cbc_kmod_devices_init(void);

int cbc_register_devices(struct class *cbc_class,
					struct cbc_memory_pool *memory);

void cbc_unregister_devices(struct class *cbc_class);

/*
 * cbc_mux_configure_data_channel - Configure channels
 * @channel_idx: Channel identifier (see cbc_channel_enumeration)
 * @priority: Priority for this channel
 * @data: Channel data
 * @receive: Receive data function associated with this channel.
 * Channels can only be configured after cbc_kmod_devices_init().
 * This will overwrite the settings for the devices.
 * The device will be created anyway, to allow the cbc_socket_server to
 * work without a requirement for handling missing devices.
 */
void cbc_mux_configure_data_channel(u32 const channel_idx, const u8 priority,
				void *data,
				void (*receive)(void *data, const u16 length,
				const u8 * const buffer));

/*
 * cbc_manager_transmit_data - Transmit data to IOC.
 * @channel_idx: Channel identifier (see cbc_channel_enumeration)
 * @length: Length of data
 * @buffer: The data
 *
 * This is the version provided as a kernel symbol.
 */
void cbc_manager_transmit_data(const u32 channel_idx, const u16 length,
						const u8 * const buffer);

/*
 * cbc_manager_transmit_data - Transmit data to IOC.
 * @channel_idx: Channel identifier (see cbc_channel_enumeration)
 * @buffer: The data
 *
 * This is the internal version, without memcpy.
 */
void cbc_manager_transmit_buffer(const u32 channel_idx,
	struct cbc_buffer *buffer);

#endif /* CBC_DEVICE_MANAGER_H */
