// SPDX-License-Identifier: GPL-2.0
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

#include "cbc_link_layer.h"
#include "cbc_mux_multiplexer.h"

static struct cbc_mux_channel_configuration *cbc_mux_configuration;

void cbc_mux_multiplexer_setup(struct cbc_mux_channel_configuration *config)
{
	cbc_mux_configuration = config;
}

/*
 * cbc_mux_multiplexer_process_rx_buffer - Processes a received buffer.
 * @cbc_buffer: CBC buffer to process.
 *
 * If the frame is valid, and the channel is valid, the contents of the CBC
 * buffer are passed to the channel's receive function.
 */
void cbc_mux_multiplexer_process_rx_buffer(struct cbc_buffer *cbc_buffer)
{
	u8 mux_idx = 0U;
	struct cbc_mux_channel_configuration *config;

	config = cbc_mux_configuration;
	if (!cbc_buffer || cbc_buffer->frame_length < CBC_HEADER_SIZE)
		return;

	mux_idx = (u8) ((cbc_buffer->data[2] >> CBC_MULTIPLEXER_SHIFT)
				& CBC_MULTIPLEXER_WIDTH_MASK) & 0xFFU;

	if (config) {
		struct cbc_mux_channel *channel;

		channel = &config->cbc_mux_channel_list[mux_idx];

		if (channel) {
			if (channel->buffer_receive) {
				channel->buffer_receive(channel->data,
						cbc_buffer);
			} else if (channel->data_receive) {
				channel->data_receive(channel->data,
					cbc_buffer->payload_length,
					&cbc_buffer->data[CBC_HEADER_SIZE]);
				cbc_buffer_release(cbc_buffer);
			}
		}

		/* Send to debug device */
		channel = &config->cbc_mux_channel_list[CBC_CHANNEL_DEBUG_IN];
		if (channel && channel->buffer_receive)
			channel->buffer_receive(channel->data, cbc_buffer);
	}
}

/*
 * cbc_mux_multiplexer_transmit_buffer - Send a buffer.
 * @channel_idx: Channel identifier.
 * @buffer: CBC buffer to transmit.
 *
 * Assembles CBC buffer for transmission.
 *
 * Return: CBC Error.
 */
enum cbc_error cbc_mux_multiplexer_transmit_buffer(
			enum cbc_channel_enumeration channel_idx,
			struct cbc_buffer *cbc_buffer)
{
	enum cbc_error result = CBC_OK;
	struct cbc_mux_channel *channel;
	struct cbc_mux_channel_configuration *config;

	config = cbc_mux_configuration;

	/*
	 * Transmit will release the buffer, make sure a reference is held.
	 * until it is enqueued in the debug device.
	 */
	cbc_buffer_increment_ref(cbc_buffer);

	if (config) {
		cbc_link_layer_assemble_buffer_for_transmission(
			(u8) channel_idx,
			config->cbc_mux_channel_list[channel_idx].priority,
			cbc_buffer);

		/* Send to debug device */
		channel = &config->cbc_mux_channel_list[CBC_CHANNEL_DEBUG_OUT];
		if (channel && channel->buffer_receive)
			channel->buffer_receive(channel->data, cbc_buffer);
	}

	/* Send to debug device */
	channel = &config->cbc_mux_channel_list[CBC_CHANNEL_DEBUG_OUT];
	if (channel && channel->buffer_receive)
		channel->buffer_receive(channel->data, cbc_buffer);

	cbc_buffer_release(cbc_buffer);

	return result;
}

/*
 * cbc_mux_multiplexer_set_priority - Set priority for specified channel.
 *						   multiplexer.
 * @channel_num: Channel ID.
 * @new_priority: UNew priority value.
 */

void cbc_mux_multiplexer_set_priority(u32 channel_num, u8 new_priority)
{
	struct cbc_mux_channel_configuration *config;

	config = cbc_mux_configuration;
	if (config && (channel_num < CBC_CHANNEL_MAX_NUMBER))
		config->cbc_mux_channel_list[channel_num].priority =
								new_priority;
}

/*
 * cbc_mux_multiplexer_get_priority - Get priority for specified channel.
 * @channel_num: Channel ID.
 *
 * Return: Priority for this channel.
 */
u8 cbc_mux_multiplexer_get_priority(u32 channel_num)
{
	struct cbc_mux_channel_configuration *config;

	config = cbc_mux_configuration;
	if (config && (channel_num < CBC_CHANNEL_MAX_NUMBER))
		return config->cbc_mux_channel_list[channel_num].priority;
	return 0;
}
