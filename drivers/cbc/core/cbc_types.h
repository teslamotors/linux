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
#ifndef CBC_TYPES_H_
#define CBC_TYPES_H_

#include <linux/types.h>


/* -----------------------------------------------------------------
   - Global Definitions                                            -
   ----------------------------------------------------------------- */
/*! \brief value of start of frame byte in link layer */
#define CBC_SOF                          0x05U
/*! \brief fill byte allowed between CBC frames */
#define CBC_INTER_FRAME_FILL_BYTE        0xFFU

/*! \brief width bit mask for priority in mux layer */
#define CBC_PRIORITY_WIDTH_MASK          0x07U
/*! \brief width bit mask for multiplexer in mux layer */
#define CBC_MULTIPLEXER_WIDTH_MASK       0x1FU
/*! \brief shift multiplexer in mux layer */
#define CBC_MULTIPLEXER_SHIFT               3U

/*! \brief width bit mask for sequence counter in link layer */
#define CBC_SEQUENCE_COUNTER_WIDTH_MASK  0x03U
/*! \brief width bit mask for frame length link layer */
#define CBC_FRAME_LENGTH_WIDTH_MASK      0x1FU
/*! \brief maximum possible frame size that can be specified
 * with the \a IAS_CBC_FRAME_LENGTH_WIDTH_MASK */
#define CBC_MAX_POSSIBLE_FRAME_SIZE ((CBC_FRAME_LENGTH_WIDTH_MASK + 2U) * 4U)
/*! \brief shift the frame length in link layer */
#define CBC_FRAME_LENGTH_SHIFT              2U

#define CBC_HEADER_SIZE 3U
#define CBC_RAWHEADER_SIZE 3U
#define CBC_CHECKSUM_SIZE 1U

/*! \brief Maximum size of a CBC frame. This includes the
 * IAS_CBC_MAX_SERVICE_FRAME_SIZE, 4 bytes of CBC protocol
 * overhead and up to 28 additional bytes for padding to
 * 32 byte granularity.
 */
#define CBC_MAX_TOTAL_FRAME_SIZE           96U

/* CM section */
/*! \brief enables or disables the CBC load monitor */
#define CBC_LOAD_MONITOR_ENABLE                               1U


/* -----------------------------------------------------------------
   - Global Macros                                                 -
   ----------------------------------------------------------------- */

/* -----------------------------------------------------------------
   - Global Types                                                  -
   ----------------------------------------------------------------- */


/*! \brief enumeration, which holds the supported individual channels */
enum cbc_channel_enumeration {
	e_cbc_channel_pmt               = 0U,
	e_cbc_channel_lifecycle         = 1U,
	e_cbc_channel_signals           = 2U,
	e_cbc_channel_early_signals     = 3U,
	e_cbc_channel_diagnosis         = 4U,
	e_cbc_channel_dlt               = 5U,
	e_cbc_channel_linda             = 6U,
	e_cbc_channel_oem_raw_channel_0 = 7U,
	e_cbc_channel_oem_raw_channel_1 = 8U,
	e_cbc_channel_oem_raw_channel_2 = 9U,
	e_cbc_channel_oem_raw_channel_3 = 10U,
	e_cbc_channel_oem_raw_channel_4 = 11U,
	e_cbc_channel_oem_raw_channel_5 = 12U,
	e_cbc_channel_oem_raw_channel_6 = 13U,
	e_cbc_channel_oem_raw_channel_7 = 14U,
	e_cbc_channel_oem_raw_channel_8 = 15U,
	e_cbc_channel_oem_raw_channel_9 = 16U,
	e_cbc_channel_oem_raw_channel_10 = 17U,
	e_cbc_channel_oem_raw_channel_11 = 18U,
	e_cbc_channel_debug_out         = 19U,
	e_cbc_channel_debug_in          = 20U,
	e_cbc_channel_max_number        = 21U
};



#if (CBC_LOAD_MONITOR_ENABLE == 1U)

/*!
 * \brief Enumeration of the possible values of the CBC frame queue load levels.
 */
enum cbc_frame_queue_utilization {
	e_cbc_frame_queue_utilization_low    = 0U,
	e_cbc_frame_queue_utilization_medium = 1U,
	e_cbc_frame_queue_utilization_high   = 2U,
};

#endif


/*! \brief enumeration, which holds the available errors */
enum cbc_error {
	e_cbc_error_ok                     = 0U,
	e_cbc_error_queue_uninitialized    = 1U,
	e_cbc_error_queue_full             = 2U,
	e_cbc_error_queue_empty            = 3U,
	e_cbc_error_parameter_incorrect    = 4U,
	e_cbc_error_null_pointer_supplied  = 5U,
	e_cbc_error_checksum_mismatch      = 6U,
	e_cbc_error_unknown_channel        = 7U,
	e_cbc_error_out_of_queue_memory    = 8U,
	e_cbc_error_no_data_in_queue_memory = 9U,
	e_cbc_error_not_processed          = 10U,
	e_cbc_error_tp_frame_not_supported = 11U,
	e_cbc_error_tp_frame_not_expected  = 12U,
	e_cbc_error_busy_try_again         = 13U,
	e_cbc_error_tec                    = 14U,
	e_cbc_error_unknown_peripheral_id  = 15U,
	e_cbc_error_hw_no_write_access     = 16U,
	e_cbc_error_hw_no_read_access      = 17U,
	e_cbc_error_not_implemented        = 18U,
	e_cbc_error_general_error          = 19U,
	e_cbc_error_udp_get_adr            = 20U,
	e_cbc_error_udp_open_socket        = 21U,
	e_cbc_error_udp_connection_refused = 22U,
	e_cbc_error_udp_close_invalid_id   = 23U,
	e_cbc_error_udp_close_err          = 24U,
	e_cbc_error_dtc_list_empty         = 25U,
	e_cbc_error_incorrect_version      = 26U,
	e_cbc_error_power_supply_error     = 27U,
	e_cbc_error_parameter_invalid      = 28U,
	e_cbc_error_not_initialized        = 29U,
	e_cbc_error_number_of_errors       = 30U,
	e_cbc_error_customer_implementation_missing = 31U
};


/* user configurable section - start */
/*! \brief defines maximum number of CBC frames transmitted in each
 * cyclic call of the CBC core. Needs to be configured by integrator
 * based on time requirements and UART capabilities.
 */
#  define CBC_MAX_FRAME_TRANSMISSION_NUMBER                       50U
/*! \brief defines if UART load monitor API is available (tx)*/
#  define CBC_LOAD_MONITOR_ENABLE_UART_TRANSMISSION_MONITOR    1U
/*! \brief defines if UART load monitor API is available (rx)*/
#  define CBC_LOAD_MONITOR_ENABLE_UART_RECEPTION_MONITOR       1U
/*! \brief defines if CBC queue frame utilization API is available */
#  define CBC_LOAD_MONITOR_ENABLE_FRAME_QUEUE_UTILIZATION      1U
/*! \brief defines the time in ms, which is taken for average values */
#  define CBC_LOAD_TIME_GRANULARITY                          100U
/*! \brief UART bitrate of CBC link, used for calculation only */
#  define CBC_LOAD_UART_BITRATE                         4000000UL

/* common_types */

/*! \brief enumeration, which holds all
 * possible SVC raw channel data transport */
enum cbc_service_raw_channel_svc {
	e_cbc_raw_channel_use_transport_protocol = 1U,
	e_cbc_raw_channel_direct_transport       = 2U
}
;

#endif /* CBC_TYPES_H_ */
