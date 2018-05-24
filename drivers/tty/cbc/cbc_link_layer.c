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

#include <linux/circ_buf.h>
#include <linux/mutex.h>

#include "cbc_core_public.h"
#include "cbc_link_checksum.h"
#include "cbc_link_layer.h"
#include "cbc_memory.h"
#include "cbc_mux_multiplexer.h"

#define CBC_MAX_RING_BUFFER_SIZE 256

/*
 * struct cbc_queue_control - Structure holding the queue control data.
 * @next_element: Next element in received data buffer.
 * @current_element: Current element in received data buffer.
 *
 * Handles the current (and next) position in the received data circular buffer.
 */
struct cbc_queue_control {
	u8 next_element;
	u8 current_element;
};

static struct cbc_memory_pool *memory_pool;

static u8 rx_cvh_ring_message[CBC_MAX_RING_BUFFER_SIZE];
static struct cbc_queue_control cvh_rx_queue_control;
static u8 number_of_bytes_expected;
static u8 number_of_bytes_skipped;
static u8 ignore_all_skipped_bytes =			1;
static u8 last_rx_frame_valid =					1;
static u8 rx_sequence_counter;

/* Transmitted data queue. */
static struct cbc_buffer_queue tx_queue;
static u8 tx_sequence_counter; /* Sequence counter value for next tx frame */
static u8 cbc_frame_granularity;
static struct mutex transmit_frame_mutex;

static void cbc_link_layer_transmit_frame(void);
static void cbc_link_release_rx_data(u8 bytes_to_free);

/*
 * calculate_total_frame_length - Calculates the total length of a frame.
 * @buffer: Pointer to CBC buffer.
 *
 * Used for outgoing frames. Calculates the total length of a frame
 * depending on its payload_length. Total length is stored in
 * cbc_buffer->frame_length.
 */
static void calculate_total_frame_length(struct cbc_buffer *buffer);

/*
 * cbc_link_release_rx_data - Release the specified number of bytes from the
 *			      internal rx buffer.
 * @bytes_to_free: Number of bytes to be released.
 */
static void cbc_link_release_rx_data(u8 bytes_to_free)
{
	cvh_rx_queue_control.current_element += bytes_to_free;
}

static void calculate_total_frame_length(struct cbc_buffer *buffer)
{
	u8 frame_length_in_bytes;

	if (!buffer)
		return;

	frame_length_in_bytes = buffer->payload_length + CBC_HEADER_SIZE +
							CBC_CHECKSUM_SIZE;

	/* Adjust frame_length to granularity */
	if ((frame_length_in_bytes % cbc_frame_granularity) != 0)
		frame_length_in_bytes += cbc_frame_granularity -
					(frame_length_in_bytes %
					cbc_frame_granularity);

	buffer->frame_length = frame_length_in_bytes;
}

/*
 * cbc_link_layer_transmit_frame - Transmits a frame over a UART.
 */
static void cbc_link_layer_transmit_frame(void)
{
	u8 total_len;
	u8 checksum = 0U;
	s32 mutex_lock_result = -1;
	u32 frame_transmission_counter = CBC_MAX_FRAME_TRANSMISSION_NUMBER;
	struct cbc_buffer *buffer = NULL;

	mutex_lock_result = mutex_lock_interruptible(&transmit_frame_mutex);
	if (mutex_lock_result != 0) {
		pr_err("cbc-core: Could not lock the transmit_frame_mutex\n");
		return;
	}

	/* If queue is not empty. */
	if (tx_queue.read != tx_queue.write)
		buffer = cbc_buffer_queue_dequeue(&tx_queue);

	while (buffer && frame_transmission_counter) {
		total_len = buffer->frame_length;
		frame_transmission_counter--;

		/* Reset sequence counter bits first */
		buffer->data[1U] &= ~CBC_SEQUENCE_COUNTER_WIDTH_MASK;

		/* Qdd sequence counter */
		buffer->data[1U] |= tx_sequence_counter;

		/*
		 * Qdd checksum, subtract 1, as the checksum field
		 * itself cannot be included in calculation
		 */
		cbc_checksum_calculate(total_len - 1U, buffer->data,
						&checksum);
		buffer->data[total_len - 1U] = checksum;

		/* Try to send the frame */
		if (target_specific_send_cbc_uart_data(total_len,
				buffer->data) != CBC_OK) {
			/* Not sent, release anyway */
			pr_debug("cbc-core: Could not send packet.\n");

		} else {
			/*
			 * Data was transmitted, so increase the
			 * sequence counter for next frame
			 */
			tx_sequence_counter = (tx_sequence_counter + 1U)
				& CBC_SEQUENCE_COUNTER_WIDTH_MASK;
		} /* else */
		cbc_buffer_release(buffer);
		buffer = NULL;

		/* If queue is not empty. */
		if (tx_queue.read != tx_queue.write)
			buffer = cbc_buffer_queue_dequeue(&tx_queue);
	}

	mutex_unlock(&transmit_frame_mutex);
}

/*
 * cbc_link_layer_get_stored_serial_data - Get stored serial data.
 * @out_buf:Pointer to buffer populated with serial data.
 * @max_length: Maximum amount of data to be retrieved.
 *
 * Populates out_buf and returns number of bytes read. Stop reading if the
 * maximum length is reached.
 *
 * Return: The amount of data read.
 */
static u8 cbc_link_layer_get_stored_serial_data(u8 *out_buf,
						u8 const max_length)
{
	u8 index8 = 0;
	u8 curr = cvh_rx_queue_control.current_element;
	u8 next = cvh_rx_queue_control.next_element;

	while (((curr + index8) & 0xFFU) != next) {
		out_buf[index8] = rx_cvh_ring_message[(curr + index8) &	0xFFU];
		index8++;
		/* Avoid memory overflow of target array */
		if (index8 >= max_length)
			break;
	}
	return index8;
}

/*
 * cbc_link_layer_set_frame_granularity - Set the CBC frame granularity.
 * @granularity: Supported values are 4, 8, 16 and 32 bytes.
 *
 * Return: CBC error, OK or incorrect parameter (if invalid granularity
 * supplied).
 */
enum cbc_error cbc_link_layer_set_frame_granularity(u8 granularity)
{
	if ((granularity == 4) || (granularity == 8) || (granularity == 16) ||
							(granularity == 32U)) {
		cbc_frame_granularity = granularity;
		return CBC_OK;
	} else {
		return CBC_ERROR_PARAMETER_INCORRECT;
	} /* else */
}

/*
 * cbc_link_layer_init - Initialize link layer.
 *
 * This function shall be called once during startup.
 * It shall be the first function of this file to be called.
 */
void cbc_link_layer_init(struct cbc_memory_pool *memory)
{
	cvh_rx_queue_control.next_element =		0;
	cvh_rx_queue_control.current_element =	0;

	memory_pool = memory;

	number_of_bytes_expected =		0;
	number_of_bytes_skipped =		0;
	ignore_all_skipped_bytes =		1;
	last_rx_frame_valid =			1;
	rx_sequence_counter =			0;

	cbc_buffer_queue_init(&tx_queue);
	tx_sequence_counter =			0;

	cbc_frame_granularity =			4;

	mutex_init(&transmit_frame_mutex);
}

/*
 * cbc_core_on_receive_cbc_serial_data - Called on reception of data on UART.
 * @length: Maximum size of data to retrieve in one go.
 * @rx_buf: Pointer to buffer to populate.
 *
 * This function is called on reception of serial data. It extracts single CBC
 * frames from the received data. If incomplete frames are received, it waits
 * for more data. Buffers are added to a circular buffer rx_cvh_ring_message.
 *
 * Return number of bytes retrieved.
 */
u8 cbc_core_on_receive_cbc_serial_data(u8 length, const u8 *rx_buf)
{
	u8 number_of_bytes_accepted =	0;
	u8 next_try_element =		0;

	while (length != 0) {
		next_try_element =
				(u8) ((cvh_rx_queue_control.next_element + 1U)
						% CBC_MAX_RING_BUFFER_SIZE);
		if (next_try_element != cvh_rx_queue_control.current_element) {
			rx_cvh_ring_message[cvh_rx_queue_control.next_element] =
								*rx_buf;

			cvh_rx_queue_control.next_element = next_try_element;
			rx_buf++; /* next byte */
			length--;
			number_of_bytes_accepted++;
		} else {
			/* Buffer is full, do not store additional bytes */
			return number_of_bytes_accepted;
		} /* else */
	} /* while */
	return number_of_bytes_accepted;
}

static void _cbc_link_layer_checksum(u8 *rx_cvh_frame, u8 frame_length)
{
	u8 expected_checksum = 0U;
	u8 checksum = 0U;

	checksum = rx_cvh_frame[frame_length - 1U];
	/* Check checksum is valid. */
	if (cbc_checksum_check(frame_length - 1U,
				rx_cvh_frame, checksum,
				&expected_checksum)
				!= CBC_OK) {
		pr_err("cbc-core: Received CBC frame contains an invalid checksum\n");
		pr_err("cbc-core: found 0x%x expected 0x%x. frame discarded (length: %i), try to realign.\n",
				checksum,
				expected_checksum,
				frame_length);
		cbc_link_release_rx_data(1U);
		number_of_bytes_skipped = 1;
		last_rx_frame_valid = 0;
	} else {
		/* check the sequence counter */
		if ((rx_cvh_frame[1]
			& CBC_SEQUENCE_COUNTER_WIDTH_MASK)
			!= rx_sequence_counter) {
			pr_err("cbc-core: Found unexpected Rx sequence counter %i, expected %i\n",
				rx_cvh_frame[1]
				& 0x3,
				rx_sequence_counter);

			/*
			 * Reset the sequence counter
			 * to the received value.
			 *
			 */
			rx_sequence_counter =
			rx_cvh_frame[1]
			& CBC_SEQUENCE_COUNTER_WIDTH_MASK;
		}
	}

}

/*
 * cbc_link_layer_rx_handler - Process data received on UART.
 *
 * Processes received serial data and parses for complete frames
 */
void cbc_link_layer_rx_handler(void)
{
	u8 bytes_avail = 0U;
	u8 service_layer_frame_length = 0U;
	u8 frame_length = 0U;

	struct cbc_buffer *buffer;
	u8 *rx_cvh_frame;

	buffer = cbc_memory_pool_get_buffer(memory_pool);

	if (!buffer) {
		pr_err("cbc-core: Out of memory.\n");
		return;
	}

	bytes_avail = cbc_link_layer_get_stored_serial_data(
					buffer->data, CBC_BUFFER_SIZE);
	rx_cvh_frame = buffer->data;

	/* Wait for at least one frame (minimum size) */
	while ((bytes_avail >= 8U) &&
				(bytes_avail >=	number_of_bytes_expected)) {
		/* Check for start of frame */
		if (rx_cvh_frame[0] == CBC_SOF) {
			/* Log skipped bytes if necessary */
			if (number_of_bytes_skipped > 0U) {
				if (ignore_all_skipped_bytes == 0U)
					pr_err("Skipped %d bytes.\n",
						number_of_bytes_skipped);

				number_of_bytes_skipped = 0U;
				ignore_all_skipped_bytes = 1U;
			}

			service_layer_frame_length = (rx_cvh_frame[1] >>
						CBC_FRAME_LENGTH_SHIFT) &
						CBC_FRAME_LENGTH_WIDTH_MASK;

			frame_length = (service_layer_frame_length + 2U) * 4U;

			if (frame_length > CBC_MAX_TOTAL_FRAME_SIZE) {
				pr_err("cbc: Received frame has illegal length (%u bytes).Frame discarded, try to realign.\n",
					frame_length);
				cbc_link_release_rx_data(1U);
				number_of_bytes_skipped = 1U;
				last_rx_frame_valid = 0U;
			} else if (bytes_avail >= frame_length) {
				/* ok */
				_cbc_link_layer_checksum(rx_cvh_frame,
							 frame_length);

				/* Increment seq. counter. */
				rx_sequence_counter++;
				rx_sequence_counter &=
				CBC_SEQUENCE_COUNTER_WIDTH_MASK;

				/* Forward frame to Mux. layer. */
				buffer->frame_length = frame_length;
				cbc_mux_multiplexer_process_rx_buffer(
							buffer);
				cbc_link_release_rx_data(frame_length);

					last_rx_frame_valid = 1;
				 /* else */
				number_of_bytes_expected = 0;
			} else {
				/*
				 * Wait for missing bytes to arrive,
				 * leave and try again.
				 */
				number_of_bytes_expected = frame_length;
			} /* else */
		} else {
			if (!(last_rx_frame_valid && (rx_cvh_frame[0] ==
						CBC_INTER_FRAME_FILL_BYTE)))
				ignore_all_skipped_bytes = 0;

			/*
			 * No alignment found,
			 * skip current byte and try next one.
			 */
			cbc_link_release_rx_data(1U);
			number_of_bytes_expected = 0;
			++number_of_bytes_skipped;
		} /* else */

		/* Process rx_buffer increases ref count,
		 * so always release here.
		 */
		cbc_buffer_release(buffer);
		buffer = cbc_memory_pool_get_buffer(memory_pool);

		if (!buffer) {
			pr_err("cbc-core: Out of memory.\n");
			rx_cvh_frame = NULL;
			return;
		}
		bytes_avail =
				cbc_link_layer_get_stored_serial_data(
				buffer->data, CBC_BUFFER_SIZE);
		rx_cvh_frame = buffer->data;
	} /* while */

	cbc_buffer_release(buffer);

}

/*
 * cbc_link_layer_tx_handler - Triggers pending data transmission.
 */
enum cbc_error cbc_link_layer_tx_handler(void)
{
	cbc_link_layer_transmit_frame();

	return CBC_OK;
}

/*
 * cbc_link_layer_assemble_buffer_for_transmission - Add frame to queue for
 *						     transmission.
 * @mux: CBC channel frame is associated with.
 * @priority: Priority for frame.
 * @buffer: Frame data.
 *
 * Generate a CBC frame from supplied data.
 * Fills in CBC header details (adds start of frame identifier, frame length
 * and channel. Also adds padding. Buffer is added to queue and transmission
 * is triggered.
 *
 * Return CBC error code (CBC_OK if frame assembled successfully).
 */
enum cbc_error cbc_link_layer_assemble_buffer_for_transmission(u8 mux,
				u8 priority, struct cbc_buffer *buffer)
{
	u8 frame_length_in_bytes;
	enum cbc_error result = CBC_OK;
	u32 i;

	if (!buffer)
		return CBC_ERROR_NULL_POINTER_SUPPLIED;

	calculate_total_frame_length(buffer);
	frame_length_in_bytes = buffer->frame_length;

	/* Fill in padding */
	for (i = buffer->payload_length + CBC_HEADER_SIZE;
				i < frame_length_in_bytes; i++)
		buffer->data[i] = 0xFF;

	/* Fill in cbc header. */
	buffer->data[0] = CBC_SOF; /* set start of frame byte */
	buffer->data[1] = ((((frame_length_in_bytes - 4U - 1U) / 4U) &
					CBC_FRAME_LENGTH_WIDTH_MASK) <<
					CBC_FRAME_LENGTH_SHIFT);
	buffer->data[2] = ((mux & CBC_MULTIPLEXER_WIDTH_MASK) <<
					CBC_MULTIPLEXER_SHIFT) |
					(priority & CBC_PRIORITY_WIDTH_MASK);

	/*
	 * If transmission is done in a different thread,
	 * check for queue full first.
	 */
	cbc_buffer_queue_enqueue(&tx_queue, buffer);
	cbc_buffer_increment_ref(buffer);

	/* Trigger transmission */
	cbc_link_layer_transmit_frame();

	return result;
}
