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
 * This file contains the cbc load monitor implementation.
 */

/* -----------------------------------------------------------------
   - Include Section                                               -
   ----------------------------------------------------------------- */

#include "cbc_load_monitor.h"
#include "cbc_core_public.h"
#include "dev/cbc_memory.h"
#include <linux/jiffies.h>

#if (CBC_LOAD_MONITOR_ENABLE == 1U)

/* -----------------------------------------------------------------
   - Local defines                                                 -
   ----------------------------------------------------------------- */


/* -----------------------------------------------------------------
   - Local types                                                   -
   ----------------------------------------------------------------- */


/* -----------------------------------------------------------------
   - Local data                                                    -
   ----------------------------------------------------------------- */


/* -----------------------------------------------------------------
   - Local forward declarations                                    -
   ----------------------------------------------------------------- */


/* -----------------------------------------------------------------
   - Local function implementations                                -
   ----------------------------------------------------------------- */
static struct cbc_timestamp timeout_start_time;
static struct cbc_timespan const timeout_ms = {1000};

#if (CBC_LOAD_MONITOR_ENABLE_FRAME_QUEUE_UTILIZATION == 1U)
static enum cbc_frame_queue_utilization tx_frame_average_queue_utilization;
static enum cbc_frame_queue_utilization tx_frame_max_queue_utilization;
static void (*cbc_load_queue_utilization_cb)(enum cbc_frame_queue_utilization tx_average_utilization
		, enum cbc_frame_queue_utilization tx_max_utilization);
static void (*cbc_load_queue_overload_cb)(void);
#endif /* (CBC_LOAD_MONITOR_ENABLE_FRAME_QUEUE_UTILIZATION == 1U) */

#if (CBC_LOAD_MONITOR_ENABLE_UART_TRANSMISSION_MONITOR == 1U)
static u32 number_of_bytes_transmitted_on_uart;
static u8  uart_tx_average_load;
static u8  uart_tx_max_load;
static void (*cbc_load_tx_uart_cb)(u8 tx_average_load, u8 tx_max_load);
#endif /* (CBC_LOAD_MONITOR_ENABLE_UART_TRANSMISSION_MONITOR == 1U) */

#if (CBC_LOAD_MONITOR_ENABLE_UART_RECEPTION_MONITOR == 1U)
static u32 number_of_bytes_received_on_uart;
static u8  uart_rx_average_load;
static u8  uart_rx_max_load;
static void (*cbc_load_rx_uart_cb)(u8 rx_average_load, u8 rx_max_load);
static u32 checksum_errors_on_receive;
static u32 sequence_counter_errors_on_receive;
#endif /* (CBC_LOAD_MONITOR_ENABLE_UART_RECEPTION_MONITOR == 1U) */

/* -----------------------------------------------------------------
   - Global functions                                              -
   ----------------------------------------------------------------- */


struct cbc_timestamp cbc_timestamp_get_current_time(void)
{
	struct cbc_timestamp result;

	result.abstime_ms = jiffies_to_msecs(jiffies);
	return result;
}


struct cbc_timespan cbc_timestamp_sub_timestamp(const struct cbc_timestamp a, const struct cbc_timestamp b)
{
	struct cbc_timespan result;

	result.timespan_ms = a.abstime_ms - b.abstime_ms;
	return result;
}


/*!
 * \brief Initializes the CBC load monitor
 *
 */
void cbc_load_monitor_init(void)
{
	timeout_start_time = cbc_timestamp_get_current_time();

#if (CBC_LOAD_MONITOR_ENABLE_FRAME_QUEUE_UTILIZATION == 1U)
	tx_frame_average_queue_utilization = e_cbc_frame_queue_utilization_low;
	tx_frame_max_queue_utilization     = e_cbc_frame_queue_utilization_low;
#endif /* (CBC_LOAD_MONITOR_ENABLE_FRAME_QUEUE_UTILIZATION == 1U) */

#if (CBC_LOAD_MONITOR_ENABLE_UART_TRANSMISSION_MONITOR == 1U)
	number_of_bytes_transmitted_on_uart = 0UL;
	uart_tx_average_load                = 0U;
	uart_tx_max_load                    = 0U;
#endif /* (CBC_LOAD_MONITOR_ENABLE_UART_TRANSMISSION_MONITOR == 1U) */

#if (CBC_LOAD_MONITOR_ENABLE_UART_RECEPTION_MONITOR == 1U)
	number_of_bytes_received_on_uart   = 0UL;
	uart_rx_average_load               = 0U;
	uart_rx_max_load                   = 0U;
	checksum_errors_on_receive         = 0UL;
	sequence_counter_errors_on_receive = 0UL;
#endif /* (CBC_LOAD_MONITOR_ENABLE_UART_RECEPTION_MONITOR == 1U) */
} /* cbc_load_monitor_init() */

/*!
 * \brief Initializes the CBC load monitor
 *
 */
enum cbc_error cbc_load_monitor_set_callbacks(
#if (CBC_LOAD_MONITOR_ENABLE_FRAME_QUEUE_UTILIZATION == 1U)
	void (*on_cbc_load_queue_utilization)(enum cbc_frame_queue_utilization tx_average_utilization
			, enum cbc_frame_queue_utilization tx_max_utilization),
	void (*on_cbc_load_queue_overload)(void)
# if ((CBC_LOAD_MONITOR_ENABLE_UART_TRANSMISSION_MONITOR == 1U) \
|| (CBC_LOAD_MONITOR_ENABLE_UART_RECEPTION_MONITOR == 1U))
	,
# endif
#endif
#if (CBC_LOAD_MONITOR_ENABLE_UART_TRANSMISSION_MONITOR == 1U)
	void (*on_cbc_load_tx_uart)(u8 tx_average_load, u8 tx_max_load)
# if (CBC_LOAD_MONITOR_ENABLE_UART_RECEPTION_MONITOR == 1U)
	,
# endif
#endif

#if (CBC_LOAD_MONITOR_ENABLE_UART_RECEPTION_MONITOR == 1U)
	void (*on_cbc_load_rx_uart)(u8 rx_average_load, u8 rx_max_load)
#endif
)
{
	enum cbc_error cbc_error_code = e_cbc_error_ok;
#if (CBC_LOAD_MONITOR_ENABLE_FRAME_QUEUE_UTILIZATION == 1U)
	if (on_cbc_load_queue_utilization != NULL) {
		cbc_load_queue_utilization_cb = on_cbc_load_queue_utilization;
	} else {
		cbc_load_queue_utilization_cb = NULL;
		cbc_error_code = e_cbc_error_null_pointer_supplied;
	} /* else */

	if (on_cbc_load_queue_overload != NULL)	{
		cbc_load_queue_overload_cb = on_cbc_load_queue_overload;
	} else {
		cbc_load_queue_overload_cb = NULL;
		cbc_error_code = e_cbc_error_null_pointer_supplied;
	} /* else */
#endif /* (CBC_LOAD_MONITOR_ENABLE_FRAME_QUEUE_UTILIZATION == 1U) */

#if (CBC_LOAD_MONITOR_ENABLE_UART_TRANSMISSION_MONITOR == 1U)
	if (on_cbc_load_tx_uart != NULL) {
		cbc_load_tx_uart_cb = on_cbc_load_tx_uart;
	} else {
		cbc_load_tx_uart_cb = NULL;
		cbc_error_code = e_cbc_error_null_pointer_supplied;
	} /* else */
#endif /* (CBC_LOAD_MONITOR_ENABLE_UART_TRANSMISSION_MONITOR == 1U) */

#if (CBC_LOAD_MONITOR_ENABLE_UART_RECEPTION_MONITOR == 1U)
	if (on_cbc_load_rx_uart != NULL) {
		cbc_load_rx_uart_cb = on_cbc_load_rx_uart;
	} else {
		cbc_load_rx_uart_cb = NULL;
		cbc_error_code = e_cbc_error_null_pointer_supplied;
	} /* else */
#endif /* (CBC_LOAD_MONITOR_ENABLE_UART_RECEPTION_MONITOR == 1U) */

	return cbc_error_code;
} /* cbc_load_monitor_set_callbacks() */

/*!
 * \brief Cyclic call of the cbc load monitor.
 *
 * Shall be called every CBC_TIMER_TICK
 *
 */
void cbc_load_monitor_cyclic(void)
{
	u8 processed_load_monitor = 0U;
	struct cbc_timestamp current_time = cbc_timestamp_get_current_time();
	struct cbc_timespan delta = cbc_timestamp_sub_timestamp(current_time, timeout_start_time);
#ifdef __linux__
	if (delta.timespan_ms >= timeout_ms.timespan_ms) {
		timeout_start_time = current_time;
		processed_load_monitor = 1U;
	} /* if */
#else /* __linux__ - do not modify this comment */
	if ((timeout_start_time.monotonic_abstime_ms.low - current_time.monotonic_abstime_ms.low)
		>= timeout_ms.timespan_ms.low) {
		timeout_start_time = current_time;
		processed_load_monitor = 1U;
	} /* if */
#endif /* __linux__ - do not modify this comment */

	if (processed_load_monitor) {
#if (CBC_LOAD_MONITOR_ENABLE_UART_TRANSMISSION_MONITOR == 1U)
		uart_tx_average_load = (u8) (1000U
							 * ((100U * number_of_bytes_transmitted_on_uart)
								/ CBC_LOAD_UART_BITRATE_BYTES_PER_GRANULARITY)
									 / delta.timespan_ms);

		if (uart_tx_average_load > 100U)
			uart_tx_average_load = 100U;

		if (uart_tx_average_load > uart_tx_max_load)
			uart_tx_max_load = uart_tx_average_load;
		number_of_bytes_transmitted_on_uart = 0UL;
		if (cbc_load_tx_uart_cb != NULL)
			(*cbc_load_tx_uart_cb)(uart_tx_average_load, uart_tx_max_load);
#endif /* (CBC_LOAD_MONITOR_ENABLE_UART_TRANSMISSION_MONITOR == 1U) */

#if (CBC_LOAD_MONITOR_ENABLE_UART_RECEPTION_MONITOR == 1U)
		uart_rx_average_load = (u8) (1000U
									 * ((100U * number_of_bytes_received_on_uart)
								/ CBC_LOAD_UART_BITRATE_BYTES_PER_GRANULARITY)
							 / delta.timespan_ms);

		if (uart_rx_average_load > 100U)
			uart_rx_average_load = 100U;

		if (uart_rx_average_load > uart_rx_max_load)
			uart_rx_max_load = uart_rx_average_load;
		number_of_bytes_received_on_uart = 0UL;
		if (cbc_load_rx_uart_cb != NULL)
			(*cbc_load_rx_uart_cb)(uart_rx_average_load, uart_rx_max_load);
#endif /* (CBC_LOAD_MONITOR_ENABLE_UART_RECEPTION_MONITOR == 1U) */
#if (CBC_LOAD_MONITOR_ENABLE_FRAME_QUEUE_UTILIZATION == 1U)
		if (cbc_load_queue_utilization_cb != NULL)
			(*cbc_load_queue_utilization_cb)(tx_frame_average_queue_utilization
					 , cbc_load_monitor_get_tx_frame_max_queue_utilization());
#endif /* (CBC_LOAD_MONITOR_ENABLE_FRAME_QUEUE_UTILIZATION == 1U) */
	} /* if */
} /* cbc_load_monitor_cyclic() */


#if (CBC_LOAD_MONITOR_ENABLE_FRAME_QUEUE_UTILIZATION == 1U)
/*!
 * \brief Returns the current average utilization of the CBC UART transmit frame queue
 *
 * \return #cbc_frame_queue_utilization
 */
enum cbc_frame_queue_utilization cbc_load_monitor_get_tx_frame_average_queue_utilization(void)
{
	return tx_frame_average_queue_utilization;
} /* cbc_load_monitor_get_tx_frame_average_queue_utilization() */


/*!
 * \brief Returns the max utilization of the CBC UART transmit frame queue.
 *
 * \return #cbc_frame_queue_utilization
 */
enum cbc_frame_queue_utilization cbc_load_monitor_get_tx_frame_max_queue_utilization(void)
{
	return tx_frame_max_queue_utilization;
} /* cbc_load_monitor_get_tx_frame_max_queue_utilization() */

/*!
 * \brief Reset the maximum utilization of the CBC UART transmit frame queue to low.
 */
void cbc_load_monitor_reset_tx_frame_max_queue_utilization(void)
{
	tx_frame_max_queue_utilization = e_cbc_frame_queue_utilization_low;
} /* cbc_load_monitor_reset_tx_frame_max_queue_utilization */

/*!
 * \brief Sets the overload condition of the frame queue in the load monitor and calls the callback
 *
 */
void cbc_load_monitor_set_overload_condition(void)
{
	if (cbc_load_queue_overload_cb != NULL)
		(*cbc_load_queue_overload_cb)();
} /* cbc_load_monitor_set_overload_condition() */

#endif /* (CBC_LOAD_MONITOR_ENABLE_FRAME_QUEUE_UTILIZATION == 1U) */


#if (CBC_LOAD_MONITOR_ENABLE_UART_TRANSMISSION_MONITOR == 1U)
/*!
 * \brief Returns current average uart send rate
 *
 * \return value in %
 */
u8 cbc_load_monitor_get_current_average_uart_send_rate(void)
{
	return uart_tx_average_load;
} /* cbc_load_monitor_get_current_average_uart_send_rate() */


/*!
 * \brief Returns maximum UART send rate.
 *
 * \return value in %
 */
u8 cbc_load_monitor_get_maximum_uart_send_rate(void)
{
	return uart_tx_max_load;
} /* cbc_load_monitor_get_maximum_uart_send_rate() */

/*!
 * \brief Reset the maximum UART send rate.
 */
void cbc_load_monitor_reset_maximum_uart_send_rate(void)
{
	uart_tx_max_load = 0U;
} /* cbc_load_monitor_reset_maximum_uart_send_rate */

/*!
 * \brief Called from CBC link layer to count bytes transmitted on UART
 *
 * \param [in] bytes_transmitted - number of bytes, which were sent successfully on UART
 */
void cbc_load_monitor_transmitted_bytes_on_uart(u16 bytes_transmitted)
{
	number_of_bytes_transmitted_on_uart += bytes_transmitted;
} /* cbc_load_monitor_transmitted_bytes_on_uart() */

#endif /* (CBC_LOAD_MONITOR_ENABLE_UART_TRANSMISSION_MONITOR == 1U) */


#if (CBC_LOAD_MONITOR_ENABLE_UART_RECEPTION_MONITOR == 1U)
/*!
 * \brief Returns maximum uart receive rate and resets the value after reading it
 *
 * \return value in %
 */
u8 cbc_load_monitor_get_current_average_uart_receive_rate(void)
{
	return uart_rx_average_load;
} /* cbc_load_monitor_get_current_average_uart_receive_rate() */

/*!
 * \brief Returns maximum uart receive rate.
 *
 * \return value in %
 */
u8 cbc_load_monitor_get_maximum_uart_receive_rate(void)
{
	return uart_rx_max_load;
} /* cbc_load_monitor_get_maximum_uart_receive_rate() */

/*!
 * \brief Reset the maximum UART receive rate.
 */
void cbc_load_monitor_reset_maximum_uart_receive_rate(void)
{
	uart_rx_max_load = 0U;
}

/*!
 * \brief Called from CBC link layer to count bytes received on UART
 *
 * \param [in] bytes_received - number of bytes received from UART
 */
void cbc_load_monitor_received_bytes_on_uart(u16 bytes_received)
{
	number_of_bytes_received_on_uart += bytes_received;
} /* cbc_load_monitor_received_bytes_on_uart() */

/*!
 * \brief Signal a checksum error in the received CBC stream.
 */
void cbc_load_monitor_checksum_error_on_receive(void)
{
	++checksum_errors_on_receive;
} /* cbc_load_monitor_checksum_error_on_receive */

/*!
 * \brief Get the number of checksum errors in the received CBC stream.
 *
 * \return The number of checksum errors in the received CBC stream.
 */
u32 cbc_load_monitor_get_checksum_errors_on_receive(void)
{
	return checksum_errors_on_receive;
} /* cbc_load_monitor_get_checksum_errors_on_receive */

/*!
 * \brief Reset the number of checksum errors in the received CBC stream.
 */
void cbc_load_monitor_reset_checksum_errors_on_receive(void)
{
	checksum_errors_on_receive = 0UL;
} /* cbc_load_monitor_reset_checksum_errors_on_receive */

/*!
 * \brief Signal a sequence counter error in the received CBC stream.
 */
void cbc_load_monitor_sequence_counter_error_on_receive(void)
{
	++sequence_counter_errors_on_receive;
} /* cbc_load_monitor_sequence_counter_error_on_receive */

/*!
 * \brief Get the number of sequence counter errors in the received CBC stream.
 *
 * \return The number of sequence counter errors in the received CBC stream.
 */
u32 cbc_load_monitor_get_sequence_counter_errors_on_receive(void)
{
	return sequence_counter_errors_on_receive;
} /* cbc_load_monitor_get_sequence_counter_errors_on_receive */

/*!
 * \brief Reset the number of sequence counter errors in the received CBC stream.
 */
void cbc_load_monitor_reset_sequence_counter_errors_on_receive(void)
{
	sequence_counter_errors_on_receive = 0UL;
} /* cbc_load_monitor_reset_sequence_counter_errors_on_receive */

#endif /* (CBC_LOAD_MONITOR_ENABLE_UART_RECEPTION_MONITOR == 1U) */


#if (CBC_LOAD_MONITOR_ENABLE_FRAME_QUEUE_UTILIZATION == 1U)

/*!
 * \brief Sets the number of free queue elements in the CBC load monitor, called from CBC link layer
 *
 * \param [in] free_elements - number of free frames
 */
void cbc_load_monitor_set_free_queue_elements(u16 free_frames)
{
	if (free_frames < (CBC_QUEUE_LENGTH / 3U))
		tx_frame_average_queue_utilization = e_cbc_frame_queue_utilization_high;
	else if ((free_frames >= (CBC_QUEUE_LENGTH / 3U)) && (free_frames < ((2U * CBC_QUEUE_LENGTH) / 3U)))
		tx_frame_average_queue_utilization = e_cbc_frame_queue_utilization_medium;
	else
		tx_frame_average_queue_utilization = e_cbc_frame_queue_utilization_low;

	if (tx_frame_average_queue_utilization > tx_frame_max_queue_utilization)
		tx_frame_max_queue_utilization = tx_frame_average_queue_utilization;
} /* cbc_load_monitor_set_free_queue_elements() */
#endif /* (CBC_LOAD_MONITOR_ENABLE_FRAME_QUEUE_UTILIZATION == 1U) */

#endif /* (CBC_LOAD_MONITOR_ENABLE == 1U) */


/* -----------------------------------------------------------------
   - end of file                                                   -
   ----------------------------------------------------------------- */
