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

#ifndef CBC_CORE_H_
#define CBC_CORE_H_

#include <uapi/linux/cbc-core.h>
#include <linux/tty.h>
#include <linux/cdev.h>

void cbc_core_configure_channel(u32 const channel_idx, const u8 priority,
		void *data, void(*receive)(void *data, const u16 length,
			const u8 * const buffer));

/*
 * send any data, the cbc_core takes core of the length, when this is a raw channel.
 */
void cbc_core_send_data(u32 const channel_idx, const u16 length,
		const u8 * const buffer);

#endif /* CBC_CORE_MOD_H_ */
