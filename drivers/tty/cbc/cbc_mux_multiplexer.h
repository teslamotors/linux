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

#ifndef CBC_MUX_MULTIPLEXER_H_
#define CBC_MUX_MULTIPLEXER_H_

#include "cbc_types.h"
#include "cbc_memory.h"

struct cbc_mux_channel {
	void (*buffer_receive)(void *data, struct cbc_buffer *cbc_buffer);
	void (*data_receive)(void *data, const u16 length,
					const u8 * const buffer);
	void *data;
	u8 priority;
};

/*
 * Channel configuration struct.
 *
 * Priorities can be set via ioctl or in sysfs.
 * Recommended values for the priorities:
 *	   - CBC_CHANNEL_PMT: 6
 *	   - CBC_CHANNEL_SYSTEM_CONTROL: 6
 *	   - CBC_CHANNEL_PROCESS_DATA: 3
 *	   - CBC_CHANNEL_DIAGNOSTICS: 3
 *	   - CBC_CHANNEL_SW_TRANSFER: 2
 *	   - CBC_CHANNEL_DEBUG: 6
 *	   - CBC_CHANNEL_LINDA: 6
 *	   - default: 3
 *
 */
struct cbc_mux_channel_configuration {
	struct cbc_mux_channel cbc_mux_channel_list[CBC_CHANNEL_MAX_NUMBER];
};

/* Pass configuration to Multiplexer. */
void cbc_mux_multiplexer_setup(struct cbc_mux_channel_configuration *config);

/* Process buffer received over UARYT via link layer. */
void cbc_mux_multiplexer_process_rx_buffer(struct cbc_buffer *cbc_buffer);

enum cbc_error cbc_mux_multiplexer_transmit_buffer(
					enum cbc_channel_enumeration channel,
					struct cbc_buffer *cbc_buffer);

void cbc_mux_multiplexer_set_priority(u32 channel_num, u8 new_priority);

u8 cbc_mux_multiplexer_get_priority(u32 channel_num);

#endif /*CBC_MUX_MULTIPLEXER_H_ */

