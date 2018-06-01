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

#ifndef CBC_LINK_LAYER_H_
#define CBC_LINK_LAYER_H_

#include "cbc_types.h"
#include "cbc_memory.h"

void cbc_link_layer_init(struct cbc_memory_pool *memory);

enum cbc_error cbc_link_layer_set_frame_granularity(u8 granularity);

enum cbc_error cbc_link_layer_tx_handler(void);

void cbc_link_layer_rx_handler(void);

u8 cbc_core_on_receive_cbc_serial_data(u8 length, const u8 *rx_buf);

enum cbc_error cbc_link_layer_assemble_frame_for_transmission(u8 mux,
				u8 priority, u8 service_frame_length,
				u8 const * const raw_buffer);

enum cbc_error cbc_link_layer_assemble_buffer_for_transmission(u8 mux,
			u8 priority, struct cbc_buffer *buffer);

#endif /*CBC_LINK_LAYER_H_ */

