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
/*
 * This file contains the cbc core link layer implementation.
 */


/* -----------------------------------------------------------------
   - Include Section                                               -
   ----------------------------------------------------------------- */
#include "cbc_link_layer.h"
#include "cbc_core_public.h"
#include "cbc_link_checksum.h"

#include "mux/cbc_mux_multiplexer.h"
#include "dev/cbc_memory.h"

#include <linux/mutex.h>

#if (CBC_LOAD_MONITOR_ENABLE == 1U)
# include "cbc_load_monitor.h"
#endif

/* -----------------------------------------------------------------
   - Local defines                                                 -
   ----------------------------------------------------------------- */
#define CBC_MAX_RING_BUFFER_SIZE 256U

/* -----------------------------------------------------------------
   - Local types                                                   -
   ----------------------------------------------------------------- */
/*! \brief structure, which holds the queue control data */
struct cbc_queue_control {
	u8 next_element;
	u8 current_element;
};

/* -----------------------------------------------------------------
   - Local data                                                    -
   ----------------------------------------------------------------- */
/*static u8 rx_cvh_frame[CBC_MAX_POSSIBLE_FRAME_SIZE];*/
static struct cbc_memory_pool *memory_pool;
static u8 rx_cvh_ring_message[CBC_MAX_RING_BUFFER_SIZE];
static struct cbc_queue_control cvh_rx_queue_control;
static u8 number_of_bytes_expected = 0U;
static u8 number_of_bytes_skipped  = 0U;
static u8 ignore_all_skipped_bytes = 1U;
static u8 last_rx_frame_valid      = 1U;
static u8 rx_sequence_counter      = 0U;

/* tx data */
static struct cbc_buffer_queue tx_queue;
static u8 tx_sequence_counter; /*!< \brief tx sequence counter value for next tx frame */
static u8 cbc_frame_granularity;
static struct mutex transmit_frame_mutex;

/* -----------------------------------------------------------------
   - Local forward declarations                                    -
   ----------------------------------------------------------------- */
static void      cbc_link_layer_transmit_frame(void);
static void      cbc_link_release_rx_data(u8 bytes_to_free);

/*
 * used for outgoing frames. Calculates the total length of a frame
 * depending on its payload_length. Total length is stored in
 * cbc_buffer->frame_length
 */
static void calculate_total_frame_length(struct cbc_buffer *buffer);


/* -----------------------------------------------------------------
   - Local function implementations                                -
   ----------------------------------------------------------------- */

/*! \brief Release the number of bytes from the internal rx buffer
 * *\param [in] bytes_to_free - number of bytes to be freed
 */
static void cbc_link_release_rx_data(u8 bytes_to_free)
{
	cvh_rx_queue_control.current_element += bytes_to_free;
} /* cbc_link_release_rx_data() */


static void calculate_total_frame_length(struct cbc_buffer *buffer)
{
	u8 frame_length_in_bytes;

	if (!buffer)
		return;

	frame_length_in_bytes = buffer->payload_length + CBC_HEADER_SIZE + CBC_CHECKSUM_SIZE;

	/* adjust frame_length to granularity */
	if ((frame_length_in_bytes % cbc_frame_granularity) != 0U)
		frame_length_in_bytes += cbc_frame_granularity - (frame_length_in_bytes % cbc_frame_granularity);

	buffer->frame_length = frame_length_in_bytes;
}


/*! \brief Transmits a frame over UART
 *
 */
static void cbc_link_layer_transmit_frame(void)
{
	u8 total_length;
	u8 checksum = 0U;
	s32 mutex_lock_result = -1;
	u32 frame_transmission_counter = CBC_MAX_FRAME_TRANSMISSION_NUMBER;

	mutex_lock_result = mutex_lock_interruptible(&transmit_frame_mutex);
	if (mutex_lock_result == 0)	{
		struct cbc_buffer *buffer = 0;

		if (!cbc_buffer_queue_empty(&tx_queue))
			buffer = cbc_buffer_queue_dequeue(&tx_queue);

		while (buffer && frame_transmission_counter) {
			total_length = buffer->frame_length;
			frame_transmission_counter--;

			/* reset sequence counter bits first */
			buffer->data[1U] &= ~CBC_SEQUENCE_COUNTER_WIDTH_MASK;

			/* add sequence counter */
			buffer->data[1U] |= tx_sequence_counter;

			/* add checksum, substract 1, as the checksum field itself
			is cannot be included in calculation */
			cbc_checksum_calculate(total_length - 1U, buffer->data, &checksum);
			buffer->data[total_length - 1U] = checksum;

			/* try to send the frame */
			if (target_specific_send_cbc_uart_data(total_length, buffer->data) != e_cbc_error_ok) {
				/* not sent, release anyway */
				pr_err("cbc: Could not send paket.\n");

			} else {
				/* data was transmitted, so increase the sequence counter for next frame*/
				tx_sequence_counter = (tx_sequence_counter + 1U) & CBC_SEQUENCE_COUNTER_WIDTH_MASK;

#if ((CBC_LOAD_MONITOR_ENABLE_UART_TRANSMISSION_MONITOR == 1U) && (CBC_LOAD_MONITOR_ENABLE == 1U))
				cbc_load_monitor_transmitted_bytes_on_uart(total_length);
#endif /* (CBC_LOAD_MONITOR_ENABLE_UART_TRANSMISSION_MONITOR == 1U)*/
			} /* else */
			cbc_buffer_release(buffer);
			buffer = 0;

			if (!cbc_buffer_queue_empty(&tx_queue))
				buffer = cbc_buffer_queue_dequeue(&tx_queue);
		} /* while */

		mutex_unlock(&transmit_frame_mutex);
	} else {
		pr_err("Could not lock the transmit_frame_mutex\n");
	} /* else */
} /* cbc_link_layer_transmit_frame() */


/*! \brief Get UART rx data
 *
 * \param [in] mfs_channel - hardware serial channel
 * \param [out] out_buf- buffer pointer of data received
 * \return - number of bytes read
 */
static u8 cbc_link_layer_get_stored_serial_data(u8 *out_buf, u8 const max_length)
{
	u8 index8  = 0U;

	while (((cvh_rx_queue_control.current_element + index8) & 0xFFU) != cvh_rx_queue_control.next_element) {
		out_buf[index8] = rx_cvh_ring_message[(cvh_rx_queue_control.current_element + index8) & 0xFFU];
		index8++;
		/* avoid memory overflow of target array */
		if (index8 >= max_length)
			break;
	};
	return index8;
} /* cbc_link_layer_get_stored_serial_data */


/* -----------------------------------------------------------------
   - Global functions                                              -
   ----------------------------------------------------------------- */
/*!
 * \brief Set the CBC frame granularity.
 *
 * Supported values are 4, 8, 16 and 32 bytes.
 *
 * \param[in] granularity.
 * \return \c e_cbc_error_ok if a valid granularity has been specified, \c e_cbc_error_parameter_incorrect otherwise.
 */
enum cbc_error cbc_link_layer_set_frame_granularity(u8 granularity)
{
	if ((granularity == 4U) || (granularity == 8U) || (granularity == 16U) || (granularity == 32U)) {
		cbc_frame_granularity = granularity;
		return e_cbc_error_ok;
	} else {
		return e_cbc_error_parameter_incorrect;
	} /* else */
} /* cbc_link_layer_set_frame_granularity() */


/*! \brief Initialize link layer
 *
 * This function shall be called once during startup.
 * It shall be the first function of this file to be called.
 */
void cbc_link_layer_init(struct cbc_memory_pool *memory)
{
	cvh_rx_queue_control.next_element = 0U;
	cvh_rx_queue_control.current_element = 0U;

	memory_pool = memory;

	number_of_bytes_expected = 0U;
	number_of_bytes_skipped = 0U;
	ignore_all_skipped_bytes = 1U;
	last_rx_frame_valid = 1U;
	rx_sequence_counter = 0U;

	cbc_buffer_queue_init(&tx_queue);
	tx_sequence_counter = 0U;

	cbc_frame_granularity = 4U;

	mutex_init(&transmit_frame_mutex);
} /* cbc_link_layer_init() */


/* -----------------------------------------------------------------
   - Reception relevant functions are following                 -
   ----------------------------------------------------------------- */

/*! \brief This function is called on reception of serial data
 *
 * It extracts single CBC frames from the received data.
 * If incomplete frames are received, then it waits for more data.
 *
 * \param [in] length - number of bytes received
 * \param [in] rx_buf - data buffer pointer of received data
 *
 * \return number of bytes accepted
 */
u8 cbc_core_on_receive_cbc_serial_data(u8 length, const u8 *rx_buf)
{
	u8 number_of_bytes_accepted = 0U;
	u8 next_try_element = 0U;

	while (length != 0U) {
		next_try_element = (u8)((cvh_rx_queue_control.next_element + 1U) % CBC_MAX_RING_BUFFER_SIZE);
		if (next_try_element != cvh_rx_queue_control.current_element) {
			rx_cvh_ring_message[cvh_rx_queue_control.next_element] = *rx_buf;
			cvh_rx_queue_control.next_element = next_try_element;
			rx_buf++; /* next byte */
			length--;
			number_of_bytes_accepted++;
		} else {
			/* buffer is full, do not store additional bytes */
			return number_of_bytes_accepted;
		} /* else */
	} /* while */
	return number_of_bytes_accepted;
} /* cbc_core_on_receive_cbc_serial_data() */


/*! \brief Processes received serial data and parses for complete frames
 *
 */
void cbc_link_layer_rx_handler(void)
{
	u8 number_of_bytes_available = 0U;
	u8 service_layer_frame_length = 0U;
	u8 frame_length = 0U;
	u8 checksum = 0U;
	u8 expected_checksum = 0U;
	struct cbc_buffer *buffer;
	u8 *rx_cvh_frame;

	buffer = cbc_memory_pool_get_buffer(memory_pool);

	if (!buffer) {
		pr_err("out of memory\n");
		return;
	}

	/*number_of_bytes_available = cbc_link_layer_get_stored_serial_data(rx_cvh_frame, sizeof(rx_cvh_frame));*/
	number_of_bytes_available = cbc_link_layer_get_stored_serial_data(buffer->data, CBC_BUFFER_SIZE);
	rx_cvh_frame = buffer->data;

	/* wait for at least one frame (minimum size) */
	while ((number_of_bytes_available >= 8U)
		&& (number_of_bytes_available >= number_of_bytes_expected)) {
		/* check for start of frame */
		if (rx_cvh_frame[0] == CBC_SOF)	{
			/* log skipped bytes if necessary */
			if (number_of_bytes_skipped > 0U) {
				if (ignore_all_skipped_bytes == 0U)
					pr_err("Skipped %d bytes.\n", number_of_bytes_skipped);

				number_of_bytes_skipped = 0U;
				ignore_all_skipped_bytes = 1U;
			}

			service_layer_frame_length =
				(rx_cvh_frame[1] >> CBC_FRAME_LENGTH_SHIFT)
				& CBC_FRAME_LENGTH_WIDTH_MASK;

			frame_length = (service_layer_frame_length + 2U) * 4U;

			if (frame_length > CBC_MAX_TOTAL_FRAME_SIZE) {
				pr_err("cbc: Received frame has illegal length (%u bytes). Frame discared, try to realign.\n"
						, frame_length);
				cbc_link_release_rx_data(1U);
				number_of_bytes_skipped = 1U;
				last_rx_frame_valid = 0U;
			} else if (number_of_bytes_available >= frame_length) {
				/* ok */
				checksum     = rx_cvh_frame[frame_length - 1U];
				/* check checksum */
				if (cbc_checksum_check(frame_length - 1U
							, rx_cvh_frame
							, checksum
							, &expected_checksum) != e_cbc_error_ok) {
					pr_err("Received CBC frame contains an invalid checksum\n");
					pr_err("found 0x%x expected 0x%x. frame discarded (length: %i), try to realign.\n"
					, checksum, expected_checksum, frame_length);
					cbc_link_release_rx_data(1U);
					number_of_bytes_skipped = 1U;
					last_rx_frame_valid = 0U;

#if ((CBC_LOAD_MONITOR_ENABLE_UART_RECEPTION_MONITOR == 1U)\
&& (CBC_LOAD_MONITOR_ENABLE == 1U))
					cbc_load_monitor_checksum_error_on_receive();
#endif /* (CBC_LOAD_MONITOR_ENABLE_UART_RECEPTION_MONITOR == 1U) */
				} else {
					/* check the sequence counter */
					if ((rx_cvh_frame[1] & CBC_SEQUENCE_COUNTER_WIDTH_MASK)
							!= rx_sequence_counter)	{
						pr_err("Found unexpected Rx sequence counter %i, expected %i\n"
								, rx_cvh_frame[1] & 0x3U
								, rx_sequence_counter);
#if ((CBC_LOAD_MONITOR_ENABLE_UART_RECEPTION_MONITOR == 1U)\
&& (CBC_LOAD_MONITOR_ENABLE == 1U))
						cbc_load_monitor_sequence_counter_error_on_receive();
#endif /* (CBC_LOAD_MONITOR_ENABLE_UART_RECEPTION_MONITOR == 1U) */

						/* reset the sequence counter to the received value */
						rx_sequence_counter = rx_cvh_frame[1]
										& CBC_SEQUENCE_COUNTER_WIDTH_MASK;
					}

					/* inc seq counter */
					rx_sequence_counter++;
					rx_sequence_counter &= CBC_SEQUENCE_COUNTER_WIDTH_MASK;

					/* forward frame to mux layer */
					buffer->frame_length = frame_length;
					cbc_mux_multiplexer_process_rx_buffer(buffer);
					cbc_link_release_rx_data(frame_length);
#if ((CBC_LOAD_MONITOR_ENABLE_UART_RECEPTION_MONITOR == 1U)\
&& (CBC_LOAD_MONITOR_ENABLE == 1U))
					cbc_load_monitor_received_bytes_on_uart(frame_length);
#endif /* (CBC_LOAD_MONITOR_ENABLE_UART_RECEPTION_MONITOR == 1U) */

					last_rx_frame_valid = 1U;
				} /* else */
				number_of_bytes_expected = 0U;
			} else {
				/* wait for missing bytes to arrive, leave and try again */
				number_of_bytes_expected = frame_length;
			} /* else */
		} else {
			if (!(last_rx_frame_valid
				&& (rx_cvh_frame[0] == CBC_INTER_FRAME_FILL_BYTE)))
				ignore_all_skipped_bytes = 0U;

			cbc_link_release_rx_data(1U); /* no alignment found, skip current byte and try next one */
			number_of_bytes_expected = 0U;
			++number_of_bytes_skipped;
		} /* else */
/*
		number_of_bytes_available = cbc_link_layer_get_stored_serial_data(rx_cvh_frame
										, sizeof(rx_cvh_frame));*/

		cbc_buffer_release(buffer); /* process rx_buffer increases ref count, so always release here */
		buffer = cbc_memory_pool_get_buffer(memory_pool);

		if (!buffer) {
			pr_err("cbc: Out of memory.\n");
			rx_cvh_frame = 0;
			return;
		}
		number_of_bytes_available = cbc_link_layer_get_stored_serial_data(buffer->data, CBC_BUFFER_SIZE);
		rx_cvh_frame = buffer->data;
	} /* while */

	cbc_buffer_release(buffer); /* TODO: check whether data is available before fetching a buffer. */

} /* cbc_link_layer_rx_handler */


/* -----------------------------------------------------------------
   - Transmission relevant functions are following                 -
   ----------------------------------------------------------------- */

/*! \brief Triggers pending data transmission
 *
 * \return #cbc_error
 */
enum cbc_error cbc_link_layer_tx_handler(void)
{
	cbc_link_layer_transmit_frame();
#if ((CBC_LOAD_MONITOR_ENABLE == 1U)\
&& (CBC_LOAD_MONITOR_ENABLE_FRAME_QUEUE_UTILIZATION == 1U))
	cbc_load_monitor_set_free_queue_elements(CBC_QUEUE_LENGTH - cbc_buffer_queue_size(&tx_queue));
#endif
	return e_cbc_error_ok;
} /* cbc_link_layer_tx_handler() */


/*! \brief Adds a frame for transmission into the queue
 *
 * \param [in] mux - multiplexer value to be set
 * \param [in] priority - frame priority
 * \param [in] service_frame_length_in_bytes - frame length in bytes
 * \param [in] raw_buffer -pointer to service frame payload, will be copied to internal memory handler
 */
enum cbc_error
cbc_link_layer_assemble_buffer_for_transmission(u8 mux
										, u8 priority
										, struct cbc_buffer *buffer)
{
	u8 frame_length_in_bytes;
	enum cbc_error result = e_cbc_error_ok;
	u32 i;

	if (!buffer)
		return e_cbc_error_null_pointer_supplied;

	calculate_total_frame_length(buffer);
	frame_length_in_bytes = buffer->frame_length;

	/* fill in padding */
	for (i = buffer->payload_length + CBC_HEADER_SIZE; i < frame_length_in_bytes; i++)
		buffer->data[i] = 0xFF;

	/* fill in cbc header */
	buffer->data[0U] = CBC_SOF; /* set start of frame byte */
	buffer->data[1U] = ((((frame_length_in_bytes - 4U - 1U) / 4U)
					& CBC_FRAME_LENGTH_WIDTH_MASK) << CBC_FRAME_LENGTH_SHIFT);
	buffer->data[2U] = ((mux & CBC_MULTIPLEXER_WIDTH_MASK) << CBC_MULTIPLEXER_SHIFT)
					| (priority & CBC_PRIORITY_WIDTH_MASK);

	/* if transmission is done in a different thread, check for queue full first */
	cbc_buffer_queue_enqueue(&tx_queue, buffer);
	cbc_buffer_increment_ref(buffer);

	/* trigger transmission */
	cbc_link_layer_transmit_frame();

	return result;
} /* cbc_link_layer_assemble_frame_for_transmission() */


/* -----------------------------------------------------------------
   - end of file                                                   -
   ----------------------------------------------------------------- */
