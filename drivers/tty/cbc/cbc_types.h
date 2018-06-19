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

#ifndef CBC_TYPES_H_
#define CBC_TYPES_H_

#include <linux/types.h>


/* Start of frame indicator. */
#define CBC_SOF						0x05
/* Fill byte allowed between CBC frames */
#define CBC_INTER_FRAME_FILL_BYTE			0xFF

/*Width bit mask for priority in Mux. layer */
#define CBC_PRIORITY_WIDTH_MASK				GENMASK(2, 0)
/* Width bit mask for multiplexer in Mux layer */
#define CBC_MULTIPLEXER_WIDTH_MASK			GENMASK(4, 0)
/* Shift multiplexer in Mux layer */
#define CBC_MULTIPLEXER_SHIFT				0x03

/* Width bit mask for sequence counter in link layer */
#define CBC_SEQUENCE_COUNTER_WIDTH_MASK			GENMASK(1, 0)
/* Width bit mask for frame length link layer */
#define CBC_FRAME_LENGTH_WIDTH_MASK				GENMASK(4, 0)
/* Maximum possible frame size that can be specified
 * with the IAS_CBC_FRAME_LENGTH_WIDTH_MASK
 */
#define CBC_MAX_POSSIBLE_FRAME_SIZE ((CBC_FRAME_LENGTH_WIDTH_MASK + 2) * 4)
/* Frame shift length in link layer */
#define CBC_FRAME_LENGTH_SHIFT				0x02

#define CBC_HEADER_SIZE						0x03
#define CBC_RAWHEADER_SIZE					0x03
#define CBC_CHECKSUM_SIZE					1

/*
 * Maximum size of a CBC frame. This includes the
 * IAS_CBC_MAX_SERVICE_FRAME_SIZE, 4 bytes of CBC protocol
 * overhead and up to 28 additional bytes for padding to
 * 32 byte granularity.
 */
#define CBC_MAX_TOTAL_FRAME_SIZE			96

/* Enumeration of supported CBC channels */
enum cbc_channel_enumeration {
	CBC_CHANNEL_PMT =						0,
	CBC_CHANNEL_LIFECYCLE =					1,
	CBC_CHANNEL_SIGNALS =					2,
	CBC_CHANNEL_EARLY_SIGNALS =				3,
	CBC_CHANNEL_DIAGNOSIS =					4,
	CBC_CHANNEL_DLT =						5,
	CBC_CHANNEL_LINDA =						6,
	CBC_CHANNEL_OEM_RAW_CHANNEL_0 =			7,
	CBC_CHANNEL_OEM_RAW_CHANNEL_1 =			8,
	CBC_CHANNEL_OEM_RAW_CHANNEL_2 =			9,
	CBC_CHANNEL_OEM_RAW_CHANNEL_3 =			10,
	CBC_CHANNEL_OEM_RAW_CHANNEL_4 =			11,
	CBC_CHANNEL_OEM_RAW_CHANNEL_5 =			12,
	CBC_CHANNEL_OEM_RAW_CHANNEL_6 =			13,
	CBC_CHANNEL_OEM_RAW_CHANNEL_7 =			14,
	CBC_CHANNEL_OEM_RAW_CHANNEL_8 =			15,
	CBC_CHANNEL_OEM_RAW_CHANNEL_9 =			16,
	CBC_CHANNEL_OEM_RAW_CHANNEL_10 =		17,
	CBC_CHANNEL_OEM_RAW_CHANNEL_11 =		18,
	CBC_CHANNEL_DEBUG_OUT =					19,
	CBC_CHANNEL_DEBUG_IN =					20,
	CBC_CHANNEL_MAX_NUMBER =				21
};

/*
 * CBC load monitoring (transmit/receive throughput, errors etc.) can
 * be compiled in using the following define.
 */

/* Enumeration containing available errors */
enum cbc_error {
	CBC_OK =							0,
	CBC_ERROR_QUEUE_UNINITIALIZED =			1,
	CBC_ERROR_QUEUE_FULL =					2,
	CBC_ERROR_QUEUE_EMPTY =					3,
	CBC_ERROR_PARAMETER_INCORRECT =			4,
	CBC_ERROR_NULL_POINTER_SUPPLIED =		5,
	CBC_ERROR_CHECKSUM_MISMATCH =			6,
	CBC_ERROR_UNKNOWN_CHANNEL =				7,
	CBC_ERROR_OUT_OF_QUEUE_MEMORY =			8,
	CBC_ERROR_NO_DATA_IN_QUEUE_MEMORY =		9,
	CBC_ERROR_NOT_PROCESSED =				10,
	CBC_ERROR_TP_FRAME_NOT_SUPPORTED =		11,
	CBC_ERROR_TP_FRAME_NOT_EXPECTED =		12,
	CBC_ERROR_BUSY_TRY_AGAIN =				13,
	CBC_ERROR_TEC =							14,
	CBC_ERROR_UNKNOWN_PERIPHERAL_ID =		15,
	CBC_ERROR_HW_NO_WRITE_ACCESS =			16,
	CBC_ERROR_HW_NO_READ_ACCESS =			17,
	CBC_ERROR_NOT_IMPLEMENTED =				18,
	CBC_ERROR_GENERAL_ERROR =				19,
	CBC_ERROR_UDP_GET_ADR =					20,
	CBC_ERROR_UDP_OPEN_SOCKET =				21,
	CBC_ERROR_UDP_CONNECTION_REFUSED =		22,
	CBC_ERROR_UDP_CLOSE_INVALID_ID =		23,
	CBC_ERROR_UDP_CLOSE_ERR =				24,
	CBC_ERROR_DTC_LIST_EMPTY =				25,
	CBC_ERROR_INCORRECT_VERSION =			26,
	CBC_ERROR_POWER_SUPPLY_ERROR =			27,
	CBC_ERROR_PARAMETER_INVALID =			28,
	E_CBC_ERROR_NOT_INITIALIZED =			29,
	CBC_ERROR_NUMBER_OF_ERRORS =			30,
	CBC_ERROR_CUSTOMER_IMPLEMENTATION_MISSING =		31
};

/*
 * Maximum number of CBC frames transmitted in each cyclic call of the CBC
 * core.
 */
#	define CBC_MAX_FRAME_TRANSMISSION_NUMBER			50

/*
 * Enumeration indicating whether raw channel uses protocol or handles raw
 * data.
 */
enum cbc_service_raw_channel_svc {
	CBC_RAW_CHANNEL_USE_TRANSPORT_PROTOCOL =		1,
	CBC_RAW_CHANNEL_DIRECT_TRANSPORT =				2
}
;
#endif /* CBC_TYPES_H_ */
