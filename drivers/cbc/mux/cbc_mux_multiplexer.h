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
 * This is cbc core multiplexer memory header file
 */


/* -----------------------------------------------------------------
   - Include Protection                                            -
   ----------------------------------------------------------------- */
#ifndef CBC_MUX_MULTIPLEXER_H_
#define CBC_MUX_MULTIPLEXER_H_

#include "cbc_types.h"
#include "dev/cbc_memory.h"

struct cbc_mux_channel {
	void (*buffer_receive)(void *data, struct cbc_buffer *cbc_buffer);
	void (*data_receive)(void *data, const u16 length, const u8 *const buffer);
	void *data;
	u8 priority;
};


/*
 * configuration struct. This has to be provided
 * from outside.
 *
 * recommended values for the priorities:
 *    - e_cbc_channel_pmt: 6
 *    - e_cbc_channel_system_control: 6
 *    - e_cbc_channel_process_data: 3
 *    - e_cbc_channel_diagnostics: 3
 *    - e_cbc_channel_sw_transfer: 2
 *    - e_cbc_channel_debug: 6
 *    - e_cbc_channel_linda: 6
 *    - default: 3
 *
 */
struct cbc_mux_channel_configuration {
	struct cbc_mux_channel cbc_mux_channel_list[e_cbc_channel_max_number];
	u32 flags;
};


/* interface for configuration */
void cbc_mux_multiplexer_setup(struct cbc_mux_channel_configuration *config);


/* interface for the link layer */
void cbc_mux_multiplexer_process_rx_buffer(struct cbc_buffer *cbc_buffer);


enum cbc_error cbc_mux_multiplexer_transmit_buffer(
					enum cbc_channel_enumeration channel
				  , struct cbc_buffer *cbc_buffer);


/* not protected by a mutex */
void cbc_mux_multiplexer_set_flagfield(u32 new_value);

/* not protected by a mutex */
u32 cbc_mux_multiplexer_get_flagfield(void);

void cbc_mux_multiplexer_set_priority(u32 channel_num, u8 new_priority);

u8 cbc_mux_multiplexer_get_priority(u32 channel_num);


#endif /*CBC_MUX_MULTIPLEXER_H_ */

/* -----------------------------------------------------------------
   - end of file                                                   -
   ----------------------------------------------------------------- */
