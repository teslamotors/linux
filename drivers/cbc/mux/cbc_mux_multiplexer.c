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
 * This file contains the cbc core multiplexer implementation.
 */


/* -----------------------------------------------------------------
   - Include Section                                               -
   ----------------------------------------------------------------- */

#include "cbc_mux_multiplexer.h"

#include "link/cbc_link_layer.h"


static struct cbc_mux_channel_configuration *cbc_mux_configuration;


void cbc_mux_multiplexer_setup(struct cbc_mux_channel_configuration *config)
{
	cbc_mux_configuration = config;
}


/*! \brief Processes a received multiplexer request
 *
 * \param [in] cbc_buffer - data
 */
void cbc_mux_multiplexer_process_rx_buffer(struct cbc_buffer *cbc_buffer)
{
	u8 mux_idx = 0U;

	if (!cbc_buffer || cbc_buffer->frame_length < CBC_HEADER_SIZE)
		return;

	mux_idx = (u8) ((cbc_buffer->data[2] >> CBC_MULTIPLEXER_SHIFT) & CBC_MULTIPLEXER_WIDTH_MASK) & 0xFFU;

	if (cbc_mux_configuration) {
		struct cbc_mux_channel *channel;

		channel = &cbc_mux_configuration->cbc_mux_channel_list[mux_idx];

		if (channel) {
			if (channel->buffer_receive) {
				channel->buffer_receive(channel->data, cbc_buffer);
			} else if (channel->data_receive) {
				channel->data_receive(channel->data
						, cbc_buffer->payload_length
						, &cbc_buffer->data[CBC_HEADER_SIZE]);
				cbc_buffer_release(cbc_buffer);
			}
		}

		/* send to debug device */
		channel = &cbc_mux_configuration->cbc_mux_channel_list[e_cbc_channel_debug_in];
		if (channel && channel->buffer_receive)
			channel->buffer_receive(channel->data, cbc_buffer);
	}
} /* cbc_mux_multiplexer_process_rx_frame() */


enum cbc_error cbc_mux_multiplexer_transmit_buffer(
					enum cbc_channel_enumeration channel_idx
				  , struct cbc_buffer *cbc_buffer)
{
	enum cbc_error result = e_cbc_error_ok;
	struct cbc_mux_channel *channel;

	 /* transmit would release the buffer, make sure a ref is hold
	  * until it is enqueued in the debug device
	  */
	cbc_buffer_increment_ref(cbc_buffer);

	if (cbc_mux_configuration)  {
		cbc_link_layer_assemble_buffer_for_transmission((u8)channel_idx
				, cbc_mux_configuration->cbc_mux_channel_list[channel_idx].priority
				, cbc_buffer);
	}

	/* send to debug device */
	channel = &cbc_mux_configuration->cbc_mux_channel_list[e_cbc_channel_debug_out];
	if (channel && channel->buffer_receive)
		channel->buffer_receive(channel->data, cbc_buffer);

	cbc_buffer_release(cbc_buffer);

	/* this function does not release the buffer. */
	return result;
}


void cbc_mux_multiplexer_set_flagfield(u32 new_value)
{
	if (cbc_mux_configuration)
		cbc_mux_configuration->flags = new_value;
}


u32 cbc_mux_multiplexer_get_flagfield(void)
{
	if (cbc_mux_configuration)
		return cbc_mux_configuration->flags;
	return 0;
}


void cbc_mux_multiplexer_set_priority(u32 channel_num, u8 new_priority)
{
	if (cbc_mux_configuration && (channel_num < e_cbc_channel_max_number))
		cbc_mux_configuration->cbc_mux_channel_list[channel_num].priority = new_priority;
}


u8 cbc_mux_multiplexer_get_priority(u32 channel_num)
{
	if (cbc_mux_configuration && (channel_num < e_cbc_channel_max_number))
		return cbc_mux_configuration->cbc_mux_channel_list[channel_num].priority;
	return 0;
}


/* -----------------------------------------------------------------
   - end of file                                                   -
   ----------------------------------------------------------------- */

