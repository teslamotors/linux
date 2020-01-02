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
 * This is the cbc core checksum header file of the CBC load monitor.
 */

/* -----------------------------------------------------------------
   - Include Protection                                            -
   ----------------------------------------------------------------- */
#ifndef _CBC_LOAD_MONITOR_H_
#define _CBC_LOAD_MONITOR_H_

#include "cbc_types.h"

#if (CBC_LOAD_MONITOR_ENABLE == 1U)

/* -----------------------------------------------------------------
   - Global Definitions                                            -
   ----------------------------------------------------------------- */
#define CBC_LOAD_UART_BITRATE_BYTES_PER_SECOND (CBC_LOAD_UART_BITRATE / 10U)
#define CBC_LOAD_UART_BITRATE_BYTES_PER_GRANULARITY (CBC_LOAD_UART_BITRATE_BYTES_PER_SECOND / CBC_LOAD_TIME_GRANULARITY)

/* -----------------------------------------------------------------
   - Global Macros                                                 -
   ----------------------------------------------------------------- */
/* define check for CBC load monitor */
#if ((CBC_LOAD_MONITOR_ENABLE_FRAME_QUEUE_UTILIZATION == 0U) \
&& (CBC_LOAD_MONITOR_ENABLE_UART_TRANSMISSION_MONITOR == 0U)\
&& (CBC_LOAD_MONITOR_ENABLE_UART_RECEPTION_MONITOR == 0U))
#error "CBC_LOAD_MONITOR_ENABLE is enabled, but all API features are disabled"
#endif /* (CBC_LOAD_MONITOR_ENABLE_FRAME_QUEUE_UTILIZATION == 1U) */


/* -----------------------------------------------------------------
   - Global Types                                                  -
   ----------------------------------------------------------------- */


/**
 * @brief struct that holds an absolute timestamp with millisecond resolution referred to the
 *    monotonic system clock
 *
 *  A timestamp is an unsigned type and can only hold positive values.
 */
struct cbc_timestamp {
	/**
	 * @brief the member holding the clock
	 */
	u32 abstime_ms;
};

struct cbc_timespan {
	s32 timespan_ms;
};

struct cbc_timestamp cbc_timestamp_get_current_time(void);
struct cbc_timespan cbc_timestamp_sub_timestamp(const struct cbc_timestamp a
					, const struct cbc_timestamp b);

/* -----------------------------------------------------------------
   - Global Prototypes                                             -
   ----------------------------------------------------------------- */

void                            cbc_load_monitor_init(void);
enum cbc_error                  cbc_load_monitor_set_callbacks(
#if (CBC_LOAD_MONITOR_ENABLE_FRAME_QUEUE_UTILIZATION == 1U)
	void (*on_cbc_load_queue_utilization)(
			enum cbc_frame_queue_utilization tx_average_utilization
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
);

void cbc_load_monitor_cyclic(void);

#if (CBC_LOAD_MONITOR_ENABLE_UART_TRANSMISSION_MONITOR == 1U)
void cbc_load_monitor_transmitted_bytes_on_uart(u16 bytes_transmitted);
#endif /* (CBC_LOAD_MONITOR_ENABLE_UART_TRANSMISSION_MONITOR == 1U) */

#if (CBC_LOAD_MONITOR_ENABLE_UART_RECEPTION_MONITOR == 1U)
void cbc_load_monitor_received_bytes_on_uart(u16 bytes_received);
void cbc_load_monitor_checksum_error_on_receive(void);
u32  cbc_load_monitor_get_checksum_errors_on_receive(void);
void cbc_load_monitor_reset_checksum_errors_on_receive(void);
void cbc_load_monitor_sequence_counter_error_on_receive(void);
u32  cbc_load_monitor_get_sequence_counter_errors_on_receive(void);
void cbc_load_monitor_reset_sequence_counter_errors_on_receive(void);
#endif /* (CBC_LOAD_MONITOR_ENABLE_UART_RECEPTION_MONITOR == 1U) */

#if (CBC_LOAD_MONITOR_ENABLE_FRAME_QUEUE_UTILIZATION == 1U)
void cbc_load_monitor_set_free_queue_elements(u16 free_elements);
void  cbc_load_monitor_set_overload_condition(void);
#endif /* (CBC_LOAD_MONITOR_ENABLE_FRAME_QUEUE_UTILIZATION == 1U) */


enum cbc_frame_queue_utilization
		cbc_load_monitor_get_tx_frame_max_queue_utilization(void);


#endif /* (CBC_LOAD_MONITOR_ENABLE == 1U) */

#endif /*_CBC_LOAD_MONITOR_H_ */

/* -----------------------------------------------------------------
   - end of file                                                   -
   ----------------------------------------------------------------- */
